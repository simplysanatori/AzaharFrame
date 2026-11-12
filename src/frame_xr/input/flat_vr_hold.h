// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>

// AzaharFrame flat
namespace FrameXr::Input {

/// Menu held in Flat (VrUi::FlatFrame), on top of the mapper's 1 s hold that opens the Flat menu
/// in a game (and leaves Fullscreen). A press held 3 s in all asks "Switch to VR?" (in the game
/// list, in a game, in the Flat menu); it never switches by itself. In the Flat menu, a press that
/// started there and is let go after 1 s, before 3 s, closes the Flat menu: the old "Menu held
/// 1 s again" close, now on release so it cannot clash with the 3 s hold. A bumper or View with
/// Menu (the VR / flat chord being formed) cancels the press, and so does a press that starts
/// while the question shows. Counts only presses that start after Menu was seen up (not one held
/// over from VR). Not thread-safe.
class FlatVrHold {
public:
    enum class Event { None, AskVr, CloseFlatMenu };
    static constexpr int64_t kAskNs = 3'000'000'000;   ///< from the press, Fullscreen or not
    static constexpr int64_t kCloseNs = 1'000'000'000; ///< the Flat menu's own close

    Event Update(bool menu, bool chord_part, bool in_flat_menu, bool asking, int64_t now_ns) {
        if (!menu) {
            Event e = Event::None;
            if (down_ && !blocked_ && !fired_ && started_in_menu_ && in_flat_menu &&
                now_ns - since_ >= kCloseNs) {
                e = Event::CloseFlatMenu;
            }
            down_ = false;
            armed_ = true;
            return e;
        }
        if (!armed_) {
            return Event::None;
        }
        if (!down_) {
            down_ = true;
            since_ = now_ns;
            fired_ = false;
            blocked_ = asking;
            started_in_menu_ = in_flat_menu;
        }
        if (chord_part) {
            blocked_ = true;
        }
        if (!blocked_ && !fired_ && now_ns - since_ >= kAskNs) {
            fired_ = true;
            return Event::AskVr;
        }
        return Event::None;
    }

    /// The press now (or last) began in the Flat menu. When it began in a game, the mapper's
    /// 1 s hold opened the Flat menu on the way (and left Fullscreen), so No goes back.
    bool StartedInFlatMenu() const {
        return started_in_menu_;
    }

    void Reset() {
        *this = {};
    }

private:
    bool armed_ = false;
    bool down_ = false;
    bool blocked_ = false;
    bool fired_ = false;
    bool started_in_menu_ = false;
    int64_t since_ = 0;
};

} // namespace FrameXr::Input
