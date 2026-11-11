// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame remap: Edit Controls on the menu's Controls tab. This is the state machine only (no
// OpenXR, Qt or settings), so it is unit tested: which Frame control each 3DS button is on, which
// chip is listening, and what a pressed Frame button does to the table. The host (menu_panel.cpp)
// turns the controls profile into a RemapTable, feeds the presses and writes the result back.
//
// The table says, for each 3DS button, the Frame control (Input::Control) that drives it, or -1
// (not driven by a Frame control: unbound, or bound to another device, which is left alone).
// Moving a 3DS function from one control to another swaps the two controls' buttons.

#pragma once

#include <array>
#include <cstdint>

#include "frame_xr/input/controls.h"

namespace FrameXr::Menu {

constexpr int kRemapButtons = 18; ///< Settings::NativeButton::NumButtons
using RemapTable = std::array<int8_t, kRemapButtons>;

/// What the drawing needs of the state.
struct RemapView {
    bool editing = false;
    int listening = -1; ///< the Input::Control whose chip waits for a press, or -1
    uint32_t rev = 0;   ///< counts every change, so the menu redraws
    bool operator==(const RemapView&) const = default;
};

class Remap {
public:
    enum class Event {
        None,      ///< nothing happened
        Listening, ///< a chip now waits for a button
        Moved,     ///< the 3DS buttons moved to a control that had none
        Swapped,   ///< two controls swapped their 3DS buttons
        Cancelled, ///< listening ended with no change
        Reset,     ///< back to the default profile
        Done,      ///< edit mode ended
    };

    /// Seconds a chip listens before it gives up.
    static constexpr float kListenSeconds = 10.0f;

    /// A control whose 3DS buttons can move (the gestures cannot).
    static bool Editable(int control);
    /// A control a pressed button can move a function to: the Editable ones but the triggers (a
    /// trigger is also how the beam clicks), and B, which cancels (the host leaves B out).
    static bool Listenable(int control);

    /// Edit mode starts from `table`; `defaults` is what Reset to Default restores.
    void Begin(const RemapTable& table, const RemapTable& defaults);
    bool Editing() const {
        return view.editing;
    }
    int Listening() const {
        return view.listening;
    }
    const RemapTable& Table() const {
        return table;
    }
    const RemapView& View() const {
        return view;
    }

    /// A chip was clicked: it listens (the same chip again, or a gesture's, stops listening).
    Event Choose(int control);
    /// A Frame button was pressed while a chip listens: its 3DS buttons and the pressed control's
    /// swap. Pressing the listening chip's own button, or one that cannot take a function,
    /// leaves listening as it was (None) or ends it (Cancelled).
    Event Press(int control);
    Event Cancel();
    /// Reset to Default: the table is the defaults; listening ends.
    Event Reset();
    /// Done: edit mode ends.
    Event Done();
    /// Time passes while listening; after kListenSeconds it cancels.
    Event Tick(float seconds);

private:
    RemapTable table{};
    RemapTable defaults{};
    RemapView view;
    float listened = 0.0f;
};

/// The 3DS buttons on `control` in `table` (a bitmask by button index).
uint32_t RemapButtonsOn(const RemapTable& table, int control);

} // namespace FrameXr::Menu
