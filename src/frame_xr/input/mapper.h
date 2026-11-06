// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include "frame_xr/input/controls.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/input_frame.h"

// AzaharFrame
// Pure controller logic: InputFrame + where the two screens are -> virtual gamepad, touch events,
// panel drag commands and the cursor. No OpenXR, Qt or emulator calls; call Update once per
// display frame from the XR thread. The mapping is described in MAPPING.md.
namespace FrameXr::Input {

/// Where the two screens are this frame, in the same (LOCAL) space as the aim poses.
struct PanelGeometry {
    PanelQuad top;
    PanelQuad bottom;
};

struct MapperConfig {
    /// A trigger is pressed above `press` and released below `release` (hysteresis).
    float trigger_press = 0.55f;
    float trigger_release = 0.40f;
    float grip_press = 0.60f;
    float grip_release = 0.40f;
    /// Radial deadzone of both sticks; the rest of the travel is rescaled to 0..1.
    float stick_deadzone = 0.12f;
    /// View held this long asks to stop the game (back to the picker).
    int64_t stop_hold_ns = 1'500'000'000;
    /// While dragging the top screen, stick Y beyond this moves it nearer or further (CitraVR 0.5).
    float depth_threshold = 0.5f;
    /// Speed of the depth change at full deflection, metres per second. CitraVR moves 0.05 m per
    /// displayed frame, about 3.6 m/s at 72 Hz.
    float depth_speed_mps = 3.0f;
    /// CitraVR default: dragging the top screen never moves it sideways ("horizontal axis lock").
    bool horizontal_lock = true;
    /// A touch only moves when it changed by at least this many 3DS pixels.
    float touch_move_epsilon_px = 0.25f;
};

enum class Panel : int { None = 0, Top = 1, Bottom = 2 };

enum class TouchEventType : int { Pressed, Moved, Released };

/// A touch on the 3DS bottom screen in its native pixels (0..320 x 0..240).
struct TouchEvent {
    TouchEventType type = TouchEventType::Pressed;
    float x = 0.0f;
    float y = 0.0f;
};

/// The pointer dot. The XR core draws it as a small quad at `position`, facing `orientation`, `scale_m`
/// wide (it grows slowly with distance so it stays visible on a far screen, as in CitraVR).
struct Cursor {
    bool visible = false;
    Hand hand = Hand::Right;
    Panel panel = Panel::None;
    Vec3f position;
    Quatf orientation;
    float scale_m = 0.01f;
    /// Pixel position on the bottom screen (3DS pixels, 320x240); 0 for the top screen.
    float px = 0.0f;
    float py = 0.0f;
    /// Position on the panel pointed at, 0..1 from the left / top edge, clamped.
    float u = 0.0f;
    float v = 0.0f;
    /// The trigger is down on the panel (a touch or a drag is in progress).
    bool pressed = false;
};

struct PanelCommands {
    /// The top screen is being grabbed (trigger held on it). While true, apply
    /// SolveSphereDrag(top pose, top_drag_target, viewer, &bottom position, limits) each frame.
    bool top_drag_active = false;
    /// The point the top screen's centre should head toward (grab offset already removed; x is
    /// zeroed when horizontal_lock is on, which keeps the screen on the centre line like CitraVR).
    Vec3f top_drag_target;
    /// Add to the top screen's position.z this frame (negative = further away). Zero when the
    /// stick is within the threshold. Only non-zero while top_drag_active.
    float top_depth_delta_z = 0.0f;
    /// Put both screens back to their default poses (right stick click). Also the "recentre":
    /// the XR core places the defaults in front of the current head heading.
    bool reset_panels = false;
    /// View was held long enough: stop the game and return to the picker. Fires once per hold.
    bool stop_game = false;
    /// 0..1 while View is held, for an on-screen "hold to quit" bar; 0 otherwise.
    float stop_hold_progress = 0.0f;
};

struct MapperOutput {
    ControlState controls;
    std::array<TouchEvent, 4> touch_events{};
    int touch_event_count = 0;
    Cursor cursor;
    PanelCommands commands;
};

class Mapper {
public:
    explicit Mapper(const MapperConfig& config = {});

    /// Processes one frame. `panels` is where the screens are right now (after last frame's drag
    /// and reset were applied).
    MapperOutput Update(const InputFrame& frame, const PanelGeometry& panels);

    /// Forgets all latched state (button edges, gestures). Returns true when a touch was in
    /// progress: the caller must send the touch release itself (Update will not). Call when the
    /// session loses focus or a game ends.
    bool Reset();

    const MapperConfig& config() const {
        return config_;
    }
    Hand preferred_hand() const {
        return preferred_hand_;
    }

private:
    enum class Gesture : int { None, Passthrough, Touch, Drag, Blocked };

    struct HandState {
        bool trigger_down = false;
        bool grip_down = false;
        Gesture gesture = Gesture::None;
    };

    Vec2f ApplyStickDeadzone(Vec2f stick) const;

    MapperConfig config_;
    std::array<HandState, kNumHands> hands_{};
    Hand preferred_hand_ = Hand::Right;

    // View hold (stop game)
    bool view_down_ = false;
    int64_t view_down_since_ns_ = 0;
    bool stop_fired_ = false;

    bool stick_click_r_prev_ = false;

    // Touch in progress
    bool touch_active_ = false;
    Vec2f touch_last_px_;

    // Drag in progress
    bool drag_active_ = false;
    Vec3f drag_grab_offset_;
    Vec3f drag_last_target_;

    int64_t last_time_ns_ = 0;
    bool have_last_time_ = false;
};

} // namespace FrameXr::Input
