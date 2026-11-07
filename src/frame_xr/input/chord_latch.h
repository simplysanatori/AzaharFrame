// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include "frame_xr/input/input_frame.h"

// AzaharFrame
namespace FrameXr::Input {

/// After a VR / flat switch: the four buttons of the switch chord (both bumpers, View, Menu)
/// read as up until all four have been seen up, no earlier than `cooldown_ns` after the first
/// frame of input that follows Arm(). Then the chord can fire again.
///
/// Why both: the buttons are still held when the new mode starts, and its input may first
/// report them up for a moment (Steam Input's gamepad, newly opened by SDL after the OpenXR
/// session ended, starts at zero and then reports the held buttons as fresh presses). A latch
/// that waited for the release alone was cleared by that first frame and fired the chord again.
///
/// The cooldown counts from the first Filter() after Arm(), in that frame's clock, so Arm() may
/// be called in one mode and Filter() in the next (OpenXR display time in VR, the steady clock
/// in flat). Not thread-safe: one thread (the XR thread) calls both.
class ChordLatch {
public:
    static constexpr int64_t kDefaultCooldownNs = 1'000'000'000;

    explicit ChordLatch(int64_t cooldown_ns = kDefaultCooldownNs) : cooldown_ns_{cooldown_ns} {}

    /// A switch was asked for, or a mode began.
    void Arm() {
        latched_ = true;
        started_ = false;
    }

    /// One frame of input (in.time_ns is its time): while latched, the four buttons are cleared
    /// in place. Returns true while latched.
    bool Filter(InputFrame& in) {
        if (!latched_) {
            return false;
        }
        if (!started_) {
            started_ = true;
            since_ns_ = in.time_ns;
        }
        const bool any = in.hand[0].bumper || in.hand[1].bumper || in.view || in.menu;
        if (!any && in.time_ns - since_ns_ >= cooldown_ns_) {
            latched_ = false;
            return false;
        }
        in.hand[0].bumper = false;
        in.hand[1].bumper = false;
        in.view = false;
        in.menu = false;
        return true;
    }

    bool latched() const {
        return latched_;
    }

private:
    int64_t cooldown_ns_;
    bool latched_ = false;
    bool started_ = false;
    int64_t since_ns_ = 0;
};

} // namespace FrameXr::Input
