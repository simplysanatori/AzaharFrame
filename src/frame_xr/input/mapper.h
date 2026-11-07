// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include "frame_xr/input/click_gesture.h"
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
    /// The game hides the bottom screen on its own (per-game "Hide Bottom Screen", immersive
    /// mode), so Left Bumper + Right Bumper + View shows or hides it: the mapper reports
    /// `toggle_bottom` and keeps that View press from reaching the game. Off: the chord is
    /// nothing special and the game sees L, R and Select as pressed.
    bool bottom_toggle_enabled = false;
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
    /// Menu held this long opens (or closes) the in-game menu; the game never sees that hold.
    int64_t menu_hold_ns = 1'000'000'000;
    /// A shorter Menu press is Start, sent when Menu is let go and held for this long.
    int64_t start_pulse_ns = 100'000'000;
    /// View pressed with both bumpers held waits this long for Menu (the VR / flat chord) before
    /// it is the bottom screen toggle or Select.
    int64_t chord_grace_ns = 150'000'000;
    /// After a Quick Save or Quick Load, the next one waits this long (a bumper that bounces).
    int64_t quick_cooldown_ns = 1'000'000'000;
    /// While dragging the top screen, stick Y beyond this moves it nearer or further (CitraVR 0.5).
    float depth_threshold = 0.5f;
    // A stick click that starts while its stick is pushed further than this is ignored (the stick
    // clicks by accident during free camera), so it never swaps, hides or resets the screens.
    float reset_click_max_deflection = 0.5f;
    /// A stick click held this long is the long action (reset the screens, Home); a shorter one
    /// acts when it is let go (swap the screens, hide or show the other one).
    int64_t stick_click_hold_ns = 1'000'000'000;
    /// Speed of the depth change at full deflection, metres per second. CitraVR moves 0.05 m per
    /// displayed frame, about 3.6 m/s at 72 Hz.
    float depth_speed_mps = 3.0f;
    /// CitraVR default: dragging the top screen never moves it sideways ("horizontal axis lock").
    bool horizontal_lock = true;
    /// A touch only moves when it changed by at least this many 3DS pixels.
    float touch_move_epsilon_px = 0.25f;
    /// How the Frame's face buttons reach the pad. On (the default, "By Position"): by where the
    /// button is, so Frame A (bottom) is the pad's B, Frame B (right) is A, Frame X (left) is Y
    /// and Frame Y (top) is X, as on a 3DS. Off ("By Label"): A is A, B is B, X is X, Y is Y.
    /// The pad's A, B, X and Y are always the 3DS-named ones that default_bindings.h binds to
    /// the 3DS A, B, X and Y.
    bool face_buttons_by_position = true;
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
    /// The trigger is down on the panel (a touch is in progress) or a grip holds it (a drag).
    bool pressed = false;
};

struct PanelCommands {
    /// A screen is being grabbed (a grip held on it; either screen moves both, the bottom one
    /// follows the top). While true, apply
    /// SolveSphereDrag(top pose, top_drag_target, viewer, &bottom position, limits) each frame.
    bool top_drag_active = false;
    /// The point the top screen's centre should head toward (grab offset already removed; x is
    /// zeroed when horizontal_lock is on, which keeps the screen on the centre line like CitraVR).
    /// When the bottom screen was grabbed it is the top screen's centre moved by the same amount
    /// as the grabbed bottom point.
    Vec3f top_drag_target;
    /// Add to the top screen's position.z this frame (negative = further away). Zero when the
    /// stick is within the threshold. Only non-zero while top_drag_active.
    float top_depth_delta_z = 0.0f;
    /// Put both screens back to their default poses (right stick click held 1 s). Also the
    /// "recentre": the XR core places the defaults in front of the current head heading.
    bool reset_panels = false;
    /// Right stick click, tap: the other screen becomes the main one (ScreenView::Swap).
    bool swap_screens = false;
    /// Left stick click, tap: hide or show the screen that is not in focus.
    bool toggle_other_screen = false;
    /// Left Bumper + Right Bumper held and View pressed while `bottom_toggle_enabled`: show or
    /// hide the bottom screen. Fires once per press.
    bool toggle_bottom = false;
    /// View was held long enough: stop the game and return to the picker. Fires once per hold.
    bool stop_game = false;
    /// 0..1 while View is held, for an on-screen "hold to quit" bar; 0 otherwise.
    float stop_hold_progress = 0.0f;
    /// Menu was held long enough: open the in-game menu. Fires once per hold.
    bool menu_toggle = false;
    /// 0..1 while Menu is held; 0 otherwise.
    float menu_hold_progress = 0.0f;
    /// Left Bumper + Right Bumper + View + Menu all down: switch between VR and flat. Fires once
    /// per chord; the game sees no View or Menu of it, and L and R only until it fires.
    bool toggle_vr = false;
    /// View pressed first (no bumper down yet), then the Right Bumper: Quick Save (the 3DS save
    /// state slot of the Quick Save menu item). Fires once per bumper press; the game sees none
    /// of the three buttons. View pressed first is held back for chord_grace_ns, as the chord's.
    bool quick_save = false;
    /// The same with the Left Bumper: Quick Load.
    bool quick_load = false;
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
    /// Takes effect with the next Update.
    void SetFaceButtonsByPosition(bool by_position) {
        config_.face_buttons_by_position = by_position;
    }
    Hand preferred_hand() const {
        return preferred_hand_;
    }

private:
    /// What a pulled trigger does until it is released.
    enum class Gesture : int { None, Passthrough, Touch, Blocked };

    struct HandState {
        bool trigger_down = false;
        bool grip_down = false;
        Gesture gesture = Gesture::None;
        /// This hand's grip holds a screen (drag_panel_).
        bool dragging = false;
    };

    Vec2f ApplyStickDeadzone(Vec2f stick) const;

    MapperConfig config_;
    std::array<HandState, kNumHands> hands_{};
    std::array<bool, kNumHands> grip_was_down_{};
    Hand preferred_hand_ = Hand::Right;

    // View hold (stop game)
    bool view_down_ = false;
    int64_t view_down_since_ns_ = 0;
    bool stop_fired_ = false;
    // Menu: Start on a short press (when let go), held: the in-game menu.
    bool menu_down_ = false;
    int64_t menu_down_since_ns_ = 0;
    bool menu_fired_ = false;
    bool start_pulse_ = false;
    int64_t start_pulse_until_ns_ = 0;

    // Stick clicks: tap or hold (click_gesture.h). Left held 1 s is Home, sent as a short pulse.
    ClickGesture click_l_;
    ClickGesture click_r_;
    bool home_pulse_ = false;
    int64_t home_pulse_until_ns_ = 0;

    // Bumper chord (bottom screen toggle). `swallow_view_` keeps that View press from the game.
    bool swallow_view_ = false;
    bool view_prev_ = false;
    // View pressed with both bumpers down, held back from the game while it may still become
    // the VR / flat chord (chord_grace_ns).
    bool view_held_back_ = false;
    int64_t view_held_back_since_ns_ = 0;
    bool select_pulse_ = false; ///< a View tap shorter than the grace, sent late
    int64_t select_pulse_until_ns_ = 0;
    // Quick Save / Quick Load: View pressed with no bumper down, then one bumper. The View press
    // is held back for chord_grace_ns (a bumper may follow); `quick_armed_` lasts as long as View.
    bool quick_armed_ = false;
    bool view_quick_wait_ = false;
    int64_t view_quick_since_ns_ = 0;
    std::array<bool, kNumHands> bumper_prev_{};
    bool quick_done_ = false; ///< an action fired at least once; quick_last_ns_ is valid
    int64_t quick_last_ns_ = 0;
    // After the VR / flat chord: each of these stays away from the game until let go.
    bool swallow_menu_ = false;
    std::array<bool, kNumHands> swallow_bumper_{};

    // Touch in progress
    bool touch_active_ = false;
    Vec2f touch_last_px_;

    // Drag in progress (a grip held on a screen)
    bool drag_active_ = false;
    Panel drag_panel_ = Panel::None;
    Vec3f drag_grab_offset_;
    Vec3f drag_last_target_;

    int64_t last_time_ns_ = 0;
    bool have_last_time_ = false;
};

} // namespace FrameXr::Input
