// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include "frame_xr/input/gyro_aim.h" // AzaharFrame kiugyro
#include "frame_xr/input/mapper.h"
#include "frame_xr/input/motion.h"
#include "frame_xr/input/mouse.h"
#include "frame_xr/input/touchpad_touch.h"

// AzaharFrame
// Glue between the mapper and the emulator: runs the mapper on an InputFrame, writes the virtual
// pad that the "frame_xr" input engine reads, and sends touches to the touch sink. The panel
// commands and the cursor are returned for the frame loop to apply. XR thread only.
namespace FrameXr::Input {

/// AzaharFrame kiugyro: Gyro Aim this frame (controls.cpp): the mode (Settings::values.vr_gyro_aim,
/// Off outside a game or with the menu open) and whether the grip of its controller is held.
struct GyroAimFrame {
    GyroAimMode mode = GyroAimMode::Off;
    bool clutch = false;
    AimCalibration calibration; ///< AzaharFrame kiufix: Light Gun's target through it
};

class InputBridge {
public:
    explicit InputBridge(const MapperConfig& config = {});

    /// One display frame. `panels` is where the screens are now. The returned reference is valid
    /// until the next call. AzaharFrame pads: with `gamepads` (VR, a game running, the menu
    /// closed: the controllers' raw input of this frame) the other gamepads join the Frame's
    /// virtual pad (frame_xr/input/devices.h).
    /// AzaharFrame padsui: `mouse` (the hub's PollMouse of this frame) in Bottom Screen mode
    /// moves its cursor and touches; its buttons join with the gamepads'.
    /// AzaharFrame mouse: `reticle` (Bottom Screen Reticle) touches through the same pipeline.
    const MapperOutput& Process(const InputFrame& frame, const PanelGeometry& panels,
                                const InputFrame* gamepads = nullptr,
                                const MouseFrame* mouse = nullptr,
                                const ReticleSample* reticle = nullptr,
                                const GyroAimFrame* gyro_aim = nullptr);

    /// Releases every button, centres the sticks and lifts a touch in progress. Call when the
    /// game ends, the session loses focus, or the picker takes over.
    void Release();

    Mapper& mapper() {
        return mapper_;
    }

    /// AzaharFrame touchpad: the pad's or the mouse's dot of the last Process (never visible
    /// while the beam holds the touch, or with the bottom screen hidden).
    const TouchpadCursor& touchpad_cursor() const {
        return touchpad_cursor_;
    }

    /// AzaharFrame kiu: C-Stick at Edge's push added to the C-Stick in the last Process (zero
    /// when none).
    Vec2f edge_stick() const {
        return edge_stick_;
    }
    /// AzaharFrame kiugyro: fire (a trigger as ZL/ZR, pressing L too) while Gyro Aim was on, in
    /// the last Process: Recoil's shot.
    bool gyro_fire() const {
        return gyro_fire_;
    }
    /// AzaharFrame kiu2: C-Stick at Edge's hand-over state after the last Process.
    ReticleEdgeHandoff::State edge_state() const {
        return touchpad_.edge_state();
    }

private:
    Mapper mapper_;
    MotionTracker motion_;
    GyroAimTracker gyro_aim_; ///< AzaharFrame kiugyro
    bool light_gun_on_ = false;
    bool gyro_fire_ = false;
    TouchpadTouch touchpad_;
    TouchpadCursor touchpad_cursor_;
    Vec2f edge_stick_; ///< AzaharFrame kiu
    MapperOutput last_;
};

} // namespace FrameXr::Input
