// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include "frame_xr/input/geometry.h"

// AzaharFrame screenswap
// Which 3DS screen is in focus, and which are hidden. No OpenXR, Qt or emulator calls.
//
// The placement has two slots: the main one (where the top screen is normally, 2 m ahead, 1 m
// high) and the secondary one below it (where the bottom screen is normally, tilted back, 0.75
// times as big). Grips and the reset move the slots. Focus only decides which screen sits in
// which slot, so a screen moved in one focus is where it was moved in the other.
//
//   Top in focus (the start of every game): top in the main slot, bottom in the secondary one.
//   Bottom in focus: the bottom screen takes the main slot at the same height (so the same
//   pixels per metre) and the top screen takes the secondary slot at that slot's height.
//
// The screen in focus is always shown. The other one can be hidden:
//   - Top in focus: the bottom screen's visibility. In games where Immersive (immersive.cpp)
//     manages it (Hide Bottom Screen or an immersive mode, `managed` below) it is the same state
//     as the Left Bumper + Right Bumper + View chord, so both flip the one flag; in other games
//     it is a flag of its own, and the chord stays with the game.
//   - Bottom in focus: the top screen is hidden or shown, a flag that lasts until focus changes.
// Taking focus on a bottom screen that is hidden shows it (a "peek"); going back to the top hides
// it again. The chord with the bottom in focus hides the bottom screen and returns to the top.
namespace FrameXr::Input {

/// Where each 3DS screen is drawn and pointed at. `visible` is false for a hidden screen.
struct ScreenQuads {
    PanelQuad top;
    PanelQuad bottom;
};

class ScreenView {
public:
    /// What the caller must do outside this class.
    struct Effect {
        /// Call Immersive::ToggleBottom(): it keeps the other half of the managed state.
        bool toggle_immersive_bottom = false;
    };

    bool bottom_focus() const {
        return bottom_focus_;
    }
    bool top_hidden() const {
        return top_hidden_;
    }
    /// Whether the bottom screen is shown now (it always is in bottom focus).
    bool BottomShown(bool managed) const {
        return bottom_focus_ || (managed ? immersive_bottom_shown_ : !bottom_hidden_);
    }

    /// AzaharFrame move: the screen drawn as the top one (true) or the bottom one is in the main
    /// slot (Detach Screens moves one slot).
    bool InMainSlot(bool top_screen) const {
        return top_screen != bottom_focus_;
    }

    /// Right stick click, tap.
    Effect Swap(bool managed);
    /// Left stick click, tap: hides or shows the screen that is not in focus.
    Effect ToggleOther(bool managed);
    /// Left Bumper + Right Bumper + View (managed games only): shows or hides the bottom screen.
    Effect ToggleBottom(bool managed);
    /// A game ended (Immersive forgets its half at the same moment).
    void Reset() {
        *this = ScreenView{};
    }

    /// Where the screens go for the two slots. `main_slot` and `secondary_slot` carry the
    /// slots' visibility (immersive may hide them); focus and hiding are applied here.
    ScreenQuads Layout(const PanelQuad& main_slot, const PanelQuad& secondary_slot,
                       bool managed) const;

    /// The mapper reports a drag as a target for "its" top screen; with the bottom in focus that
    /// screen sits in the secondary slot, while the drag moves the main slot. Returns the target
    /// for the main slot (the same movement). `mapper_top` is the top quad Layout gave.
    Vec3f MainDragTarget(const Vec3f& mapper_target, const PanelQuad& mapper_top,
                         const PanelQuad& main_slot) const;

private:
    Effect SetBottomShown(bool managed, bool show);

    bool bottom_focus_ = false;
    bool top_hidden_ = false;    ///< bottom focus only
    bool bottom_hidden_ = false; ///< top focus, games Immersive does not manage
    /// Mirror of Immersive's "bottom screen brought back" flag (managed games).
    bool immersive_bottom_shown_ = false;
    /// The bottom screen was hidden when it took focus; hide it again when focus goes back.
    bool peek_ = false;
};

} // namespace FrameXr::Input
