// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: who holds the XR screen images between the emu thread and the handoff thread
// (vk_xr_target.cpp). No Vulkan and no OpenXR here, and no lock: the caller holds its mutex
// around every call. Unit tested in src/tests/frame_xr/xr_handoff.cpp.
//
// OpenXR lets an app write a swapchain image only between xrWaitSwapchainImage and
// xrReleaseSwapchainImage, one waited image per swapchain at a time, and SteamVR shows an image
// as soon as it is released, finished or not. So an image can only be waited for once the GPU
// finished the one before it. The emu thread never does that wait:
//
//   - The handoff thread waits for an image of each of the three swapchains ahead of time
//     ("armed"). A frame that finds them armed is drawn straight into them (Direct); the handoff
//     thread releases them once the GPU reached the frame's tick, then arms the next ones.
//   - A frame that finds them still in use (the GPU has not finished the frame before) is drawn
//     into one of two spill slots of the renderer's own (Spill). Once armed again, the handoff
//     thread copies the newest spilled frame into the swapchain images and releases them when
//     the copy finished. A spilled frame not yet taken is replaced by the next one (newest wins).
//
// The three screens of a frame always take the same path, so both eyes and the bottom screen
// shown together come from the same emulated frame, and frames are shown in order: a Direct
// frame is only drawn when no spilled frame waits for its copy.

#pragma once

#include <array>
#include <cstddef>

#include "common/common_types.h"

namespace Vulkan {

class XrHandoff {
public:
    static constexpr std::size_t kSlots = 2;

    enum class Path : u8 {
        None,   ///< no XR output this frame
        Direct, ///< into the armed swapchain images
        Spill,  ///< into spill slot `slot`
    };
    struct Frame {
        Path path = Path::None;
        std::size_t slot = 0;
        bool replaced = false; ///< the slot held a spilled frame not yet shown: it is dropped
    };

    enum class JobKind : u8 {
        None,
        ReleaseDirect, ///< wait for `tick`, release the three images
        Arm,           ///< acquire and wait for an image of each swapchain not armed yet
        CopySpill,     ///< wait for `tick`, copy slot `slot` into the armed images, release them
    };
    struct Job {
        JobKind kind = JobKind::None;
        u64 tick = 0;
        std::size_t slot = 0;
    };

    // ---- Emu thread ----

    /// Where this frame is drawn. Never waits. `spill_ok`: spill slots exist.
    Frame BeginFrame(bool spill_ok) {
        want_arm = true; // an arm that failed is tried again once a frame
        if (armed && !direct_writing && !direct_pending && !AnySlot(Slot::Writing) &&
            !AnySlot(Slot::Queued) && !AnySlot(Slot::Copying)) {
            armed = false;
            direct_writing = true;
            return {Path::Direct, 0, false};
        }
        if (!spill_ok) {
            return {};
        }
        for (std::size_t i = 0; i < kSlots; i++) {
            if (slots[i] == Slot::Queued) {
                slots[i] = Slot::Writing;
                return {Path::Spill, i, true};
            }
        }
        for (std::size_t i = 0; i < kSlots; i++) {
            if (slots[i] == Slot::Free) {
                slots[i] = Slot::Writing;
                return {Path::Spill, i, false};
            }
        }
        return {}; // not reached: at most one slot is being copied
    }

    /// The frame from BeginFrame was submitted; its last submission signals `tick`.
    void EndFrame(const Frame& frame, u64 tick) {
        if (frame.path == Path::Direct) {
            direct_writing = false;
            direct_pending = true;
            direct_tick = tick;
        } else if (frame.path == Path::Spill) {
            slots[frame.slot] = Slot::Queued;
            slot_tick[frame.slot] = tick;
        }
    }

    // ---- Handoff thread ----

    /// The next job, marked as running; None: nothing to do now.
    Job NextJob() {
        if (busy) {
            return {};
        }
        if (direct_pending) {
            busy = true;
            return {JobKind::ReleaseDirect, direct_tick, 0};
        }
        if (paused) {
            return {};
        }
        if (!armed) {
            if (!want_arm || direct_writing) {
                return {}; // (the emu thread draws into the waited images)
            }
            busy = true;
            return {JobKind::Arm, 0, 0};
        }
        for (std::size_t i = 0; i < kSlots; i++) {
            if (slots[i] == Slot::Queued) {
                slots[i] = Slot::Copying;
                armed = false; // the copy uses the armed images
                busy = true;
                return {JobKind::CopySpill, slot_tick[i], i};
            }
        }
        return {};
    }

    void ReleaseDone() {
        direct_pending = false;
        busy = false;
        want_arm = true;
    }
    /// `all`: every swapchain has an image waited for now. Not all: tried again next frame.
    void ArmDone(bool all) {
        armed = all;
        want_arm = all;
        busy = false;
    }
    void CopyDone(std::size_t slot) {
        slots[slot] = Slot::Free;
        busy = false;
        want_arm = true;
    }

    // ---- Control (emu thread) ----

    /// Paused: no arming and no copy is started (releases still run). For swapchain changes.
    void SetPaused(bool p) {
        paused = p;
        if (!p) {
            want_arm = true;
        }
    }
    /// A spilled frame not taken yet is dropped. True when there was one.
    bool DropQueued() {
        bool dropped = false;
        for (auto& slot : slots) {
            if (slot == Slot::Queued) {
                slot = Slot::Free;
                dropped = true;
            }
        }
        return dropped;
    }
    /// Nothing in flight: no job running, no release due, no slot in use.
    bool Idle() const {
        return !busy && !direct_pending && !direct_writing && !AnySlot(Slot::Writing) &&
               !AnySlot(Slot::Queued) && !AnySlot(Slot::Copying);
    }
    /// Some armed image is gone (its swapchain was made again): arm again.
    void Unarm() {
        armed = false;
        want_arm = true;
    }

    bool Armed() const {
        return armed;
    }
    bool Paused() const {
        return paused;
    }

private:
    enum class Slot : u8 { Free, Writing, Queued, Copying };

    bool AnySlot(Slot state) const {
        for (const Slot slot : slots) {
            if (slot == state) {
                return true;
            }
        }
        return false;
    }

    std::array<Slot, kSlots> slots{};
    std::array<u64, kSlots> slot_tick{};
    bool armed = false;          ///< each swapchain has a waited image nobody draws into
    bool direct_writing = false; ///< the emu thread draws into the armed images
    bool direct_pending = false; ///< drawn, to be released at direct_tick
    u64 direct_tick = 0;
    bool busy = false;    ///< the handoff thread runs a job
    bool want_arm = true; ///< false after an arm that did not get every image
    bool paused = false;
};

} // namespace Vulkan
