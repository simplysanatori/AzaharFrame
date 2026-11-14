// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: a hook the HID service calls on the emulation thread at each pad update
// (about 234 times a second of the game's time, always, whatever the game enables), with the
// emulated clock. frame_xr reads a few values of a game's memory there (frame_xr/kiu_weapon.h):
// on the emulation thread, so never while the game writes them. Unset: nothing.

#pragma once

#include <atomic>
#include <cstdint>

namespace Common::FrameEmuPoll {

using Hook = void (*)(int64_t ticks);

inline std::atomic<Hook>& Slot() {
    static std::atomic<Hook> hook{nullptr};
    return hook;
}

inline void Set(Hook hook) {
    Slot().store(hook, std::memory_order_release);
}

inline void Run(int64_t ticks) {
    if (const Hook hook = Slot().load(std::memory_order_acquire)) {
        hook(ticks);
    }
}

} // namespace Common::FrameEmuPoll
