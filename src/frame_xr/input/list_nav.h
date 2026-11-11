// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstdint>

// AzaharFrame gamelist
// The game list with the controller, in Azahar's window before a game starts (VR and Flat): the
// D-pad or the left stick up and down move the highlighted game, repeating while held, and A
// pressed twice quickly starts it. One A is still the left click where the beam points (VR).
// ListNav is the timing (XR thread); ListNavAllowed is the rule for when the window takes it
// (GUI thread, citra_qt/frame_mirror.cpp, which checks it again for every step it is sent).
namespace FrameXr::Input {

struct ListNavTiming {
    /// A second press of A this soon after the first starts the game (Qt's double-click
    /// interval, so a double tap with the beam on a game is also Qt's double click).
    int64_t double_tap_ns = 400'000'000;
    /// A direction held: the first step at once, the second after this, then every repeat_ns.
    int64_t repeat_delay_ns = 400'000'000;
    int64_t repeat_ns = 110'000'000;
    /// The left stick is a direction past stick_on and lets go below stick_off.
    float stick_on = 0.6f;
    float stick_off = 0.4f;
};

class ListNav {
public:
    struct Result {
        int step = 0;          ///< -1 one game up, +1 one game down, 0 nothing
        bool activate = false; ///< A pressed twice quickly: start the highlighted game
    };

    explicit ListNav(const ListNavTiming& timing = {}) : t(timing) {}

    /// Once per frame. `enabled` false (a game runs, a dialog or menu is open): nothing comes
    /// out, and a direction or A held as it becomes true does nothing until let go. `stick_y`:
    /// the left stick, positive up (OpenXR).
    Result Update(bool enabled, bool dpad_up, bool dpad_down, float stick_y, bool a,
                  int64_t now_ns) {
        Result r;
        // The stick's direction, with hysteresis.
        if (stick_dir == 0) {
            stick_dir = stick_y >= t.stick_on ? -1 : stick_y <= -t.stick_on ? 1 : 0;
        } else if (stick_dir * -stick_y < t.stick_off) {
            stick_dir = 0;
        }
        int dir = dpad_up == dpad_down ? 0 : dpad_up ? -1 : 1;
        if (dir == 0) {
            dir = stick_dir;
        }
        const bool a_edge = a && !prev_a;
        prev_a = a;
        if (!enabled) {
            held_dir = dir; // held from before: nothing until let go
            blocked = true;
            last_tap_ns = kNone;
            return r;
        }
        if (dir != held_dir) {
            held_dir = dir;
            blocked = false;
            if (dir != 0) {
                r.step = dir;
                next_repeat_ns = now_ns + t.repeat_delay_ns;
            }
        } else if (dir != 0 && !blocked && now_ns >= next_repeat_ns) {
            r.step = dir;
            // Never a burst after a long frame.
            next_repeat_ns = std::max(next_repeat_ns + t.repeat_ns, now_ns + t.repeat_ns / 2);
        }
        if (a_edge) {
            if (last_tap_ns != kNone && now_ns - last_tap_ns <= t.double_tap_ns) {
                r.activate = true;
                last_tap_ns = kNone; // a third tap starts a new pair
            } else {
                last_tap_ns = now_ns;
            }
        }
        return r;
    }

    void Reset() {
        *this = ListNav{t};
    }

private:
    static constexpr int64_t kNone = INT64_MIN;
    ListNavTiming t;
    int stick_dir = 0;
    int held_dir = 0;
    bool blocked = true; ///< held_dir was held when enabled turned true
    bool prev_a = true;  ///< an A held when the window appears is no press
    int64_t next_repeat_ns = 0;
    int64_t last_tap_ns = kNone;
};

/// What the window looks like (GUI thread, frame_mirror.cpp).
struct ListNavState {
    bool game_running = false;  ///< a game runs (also paused, with a menu over it)
    bool list_visible = false;  ///< the game list (not the "add a folder" page) shows
    bool popup_open = false;    ///< a menu or dropdown is open
    bool dialog_open = false;   ///< a dialog or another window of Azahar's is shown
    bool main_active = false;   ///< Azahar's main window has the keys
    bool none_active = false;   ///< no window of Azahar's has the keys
    bool vr_showing = false;    ///< VR (the mirrored window) rather than Flat
};

/// The D-pad, the left stick and A's double tap drive the game list only here: no game, the
/// list shown, nothing over it, and the main window has the keys. In VR the window is in the
/// headset and may show no window as active (gamescope); in Flat other apps share the screen,
/// so it must be Azahar's.
inline bool ListNavAllowed(const ListNavState& s) {
    if (s.game_running || !s.list_visible || s.popup_open || s.dialog_open) {
        return false;
    }
    return s.main_active || (s.vr_showing && s.none_active);
}

} // namespace FrameXr::Input
