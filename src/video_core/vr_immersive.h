// Copyright 2026 CitraFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// CitraFrame: the hand-off for CitraVR's immersive mode between the frame_xr thread and the
// emulation thread.
//
// CitraVR called RasterizerInterface::SetVRData from its OpenXR thread while the emulation
// thread was drawing with the same fields (a data race). Here the XR thread only publishes a
// snapshot (a seqlock of atomics, one writer); the emulation thread takes the newest one once
// per guest frame (GPU::VBlankCallback, right after SwapBuffers) and calls SetVRData itself,
// so the rasterizer is only ever touched from the thread that draws.
//
// Header only: video_core does not link frame_xr; frame_xr links video_core and publishes.
// Nothing is published without an XR session, so a flat run behaves as upstream.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>

#include "common/common_types.h"
#include "common/settings.h"
#include "video_core/rasterizer_interface.h"

namespace VideoCore::Immersive {

/// The arguments of RasterizerInterface::SetVRData. The defaults are CitraVR's "off" values
/// (vr_main.cpp DisableImmersiveMode): mode 1 with factor 1 changes nothing.
struct VRData {
    s32 mode = 1;
    float factor = 1.0f;
    s32 uoffset = -1;
    float game_pos_scaler = 0.0f;
    std::array<float, 16> inv_view{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

/// One writer (the XR thread), any number of readers. A reader never blocks the writer; a
/// read that overlaps a write is retried a few times and otherwise skipped (the next guest
/// frame takes the next snapshot).
class Mailbox {
public:
    void Publish(const VRData& data) noexcept {
        const u32 s = seq.load(std::memory_order_relaxed);
        seq.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        mode.store(data.mode, std::memory_order_relaxed);
        factor.store(data.factor, std::memory_order_relaxed);
        uoffset.store(data.uoffset, std::memory_order_relaxed);
        game_pos_scaler.store(data.game_pos_scaler, std::memory_order_relaxed);
        for (std::size_t i = 0; i < 16; i++) {
            inv_view[i].store(data.inv_view[i], std::memory_order_relaxed);
        }
        seq.store(s + 2, std::memory_order_release);
    }

    /// False: nothing published yet, or a write was in progress every try.
    /// `serial` is a number that changes with every publish.
    bool Read(VRData& out, u32& serial) const noexcept {
        for (int attempt = 0; attempt < 4; attempt++) {
            const u32 s0 = seq.load(std::memory_order_acquire);
            if (s0 == 0) {
                return false;
            }
            if (s0 & 1) {
                continue;
            }
            VRData d;
            d.mode = mode.load(std::memory_order_relaxed);
            d.factor = factor.load(std::memory_order_relaxed);
            d.uoffset = uoffset.load(std::memory_order_relaxed);
            d.game_pos_scaler = game_pos_scaler.load(std::memory_order_relaxed);
            for (std::size_t i = 0; i < 16; i++) {
                d.inv_view[i] = inv_view[i].load(std::memory_order_relaxed);
            }
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq.load(std::memory_order_relaxed) == s0) {
                out = d;
                serial = s0;
                return true;
            }
        }
        return false;
    }

private:
    std::atomic<u32> seq{0};
    std::atomic<s32> mode{1};
    std::atomic<float> factor{1.0f};
    std::atomic<s32> uoffset{-1};
    std::atomic<float> game_pos_scaler{0.0f};
    std::array<std::atomic<float>, 16> inv_view{};
};

inline Mailbox g_mailbox;

/// True while frame_xr's immersive module is installed (an XR session exists).
inline std::atomic<bool> g_xr_active{false};

/// XR thread: the newest VR data for the emulation thread.
inline void Publish(const VRData& data) {
    g_mailbox.Publish(data);
}

/// Emulation thread, once per guest frame: hands the newest snapshot to the rasterizer (again
/// for a new rasterizer, as every game boot makes one).
inline void ApplyPending(RasterizerInterface* rasterizer) {
    static u32 last_serial = 0;
    static const RasterizerInterface* last_rasterizer = nullptr;
    if (!rasterizer) {
        return;
    }
    VRData d;
    u32 serial = 0;
    if (!g_mailbox.Read(d, serial)) {
        return;
    }
    if (serial == last_serial && rasterizer == last_rasterizer) {
        return;
    }
    last_serial = serial;
    last_rasterizer = rasterizer;
    rasterizer->SetVRData(d.mode, d.factor, d.uoffset, d.game_pos_scaler, d.inv_view.data());
}

/// CitraVR (vr_main.cpp:402) renders two resolution steps higher while immersive mode is set,
/// as the top screen covers much more of the view. Capped at 8x (SteamVR on the Frame scales
/// down layers wider than 3458 px, see renderer_vulkan_xr.cpp), never below the base.
inline u32 AdjustResolution(u32 base) {
    constexpr u32 kSteps = 2;
    constexpr u32 kMaxScale = 8;
    if (!g_xr_active.load(std::memory_order_relaxed) ||
        Settings::values.vr_immersive_mode.GetValue() == 0) {
        return base;
    }
    return std::min(base + kSteps, std::max(base, kMaxScale));
}

} // namespace VideoCore::Immersive
