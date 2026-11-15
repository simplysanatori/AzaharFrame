// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include "frame_xr/input/types.h"

// AzaharFrame
namespace FrameXr::Input {

/// One hand of the Steam Frame controllers, as read by Actions::Sync.
struct HandInput {
    /// The controller is tracked and its aim pose is valid this frame.
    bool active = false;
    /// Aim pose in the base (LOCAL) space. The pointing ray is the pose's -Z axis.
    Posef aim;
    /// AzaharFrame ctrlglitch: what SteamVR said of `aim` (pose_guard.h TrackBits), and its pose
    /// for the next display time (`ahead_valid`), where the models' image will be on view.
    uint8_t track = 0;
    bool ahead_valid = false;
    Posef aim_ahead;
    /// Index trigger, 0..1.
    float trigger = 0.0f;
    /// Squeeze (grip), 0..1.
    float grip = 0.0f;
    bool bumper = false;
    Vec2f stick;
    bool stick_click = false;
    /// AzaharFrame padsui: the mouse's ray stands in for this hand (Mouse Mode: Pointer): no
    /// controller to draw, no beam, its trigger only clicks (mouse_pointer.h).
    bool mouse = false;
    /// AzaharFrame polish: this controller drives the Bottom Screen Reticle this frame
    /// (ReticleDrivingHand, reticle.h): no beam, no pointer dot and no stats grab of its own.
    /// Only in a game with the menu closed; everywhere else it is a normal pointer.
    bool reticle = false;
    /// AzaharFrame kiuaim3: Direct Aim keeps this controller's reticle on the top screen (Keep
    /// Reticle on Screen, the aim locked: gyro_aim.h LockedTargetFor): its beam touches nothing
    /// and points at nothing on the bottom screen (its trigger is ZL/ZR, which fires). Its grip
    /// frees the aim (Grip Frees Aim), and then it points and touches as usual.
    bool aim_locked = false;
};

/// Everything the controllers report in one display frame. A and B and X and Y and Menu are on the
/// right controller; the D-pad and View are on the left one.
struct InputFrame {
    /// Predicted display time in nanoseconds (XrTime). Only differences are used.
    int64_t time_ns = 0;
    std::array<HandInput, kNumHands> hand;
    /// The head (view space) pose in the base (LOCAL) space, when it is located this frame. Only
    /// the Headset choice of Motion Controls reads it.
    bool head_active = false;
    Posef head;
    bool a = false;
    bool b = false;
    bool x = false;
    bool y = false;
    bool menu = false;
    bool view = false;
    bool dpad_up = false;
    bool dpad_down = false;
    bool dpad_left = false;
    bool dpad_right = false;
};

} // namespace FrameXr::Input
