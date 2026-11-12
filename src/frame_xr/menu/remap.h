// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame remap: Edit Controls on the menu's Controls tab. This is the state machine only (no
// OpenXR, Qt or settings), so it is unit tested: which control of the device being edited drives
// each 3DS button, which 3DS button is listening, and what a pressed button does to the table. The
// host (menu_panel.cpp) turns the device's mapping into a RemapTable, feeds the presses and writes
// the result back.
//
// The table says, for each 3DS button (Settings::NativeButton), the control of the device that
// drives it, or -1: an Input::Control, for the Frame controllers and a gamepad alike (lane pads
// gives a gamepad's controls as Input::Controls: SDL's A is Control::A...). AzaharFrame padsui: the 3DS button is the target, the device's button the source;
// giving a 3DS button a control another 3DS button had swaps the two.

#pragma once

#include <array>
#include <cstdint>

#include "frame_xr/input/controls.h"

namespace FrameXr::Menu {

constexpr int kRemapButtons = 18; ///< Settings::NativeButton::NumButtons
using RemapTable = std::array<int8_t, kRemapButtons>;

/// AzaharFrame padsui: the 3DS buttons by Settings::NativeButton index (the menu has no settings
/// dependency; menu_panel.cpp checks they match).
namespace DsButton {
enum : int { A, B, X, Y, Up, Down, Left, Right, L, R, Start, Select, ZL = 14, ZR = 15, Home = 16 };
} // namespace DsButton

/// AzaharFrame padsui: whose buttons the table holds, for which presses can take a 3DS button.
enum class RemapSource : uint8_t { Frame, Pad };

/// What the drawing needs of the state.
struct RemapView {
    bool editing = false;
    int listening = -1; ///< the 3DS button (Settings::NativeButton) waiting for a press, or -1
    uint32_t rev = 0;   ///< counts every change, so the menu redraws
    bool operator==(const RemapView&) const = default;
};

class Remap {
public:
    enum class Event {
        None,      ///< nothing happened
        Listening, ///< a 3DS button now waits for a press
        Moved,     ///< the 3DS button took a control no other 3DS button had
        Swapped,   ///< the 3DS button took another's control, which took its old one
        Cancelled, ///< listening ended with no change
        Reset,     ///< back to the default profile
        Done,      ///< edit mode ended
    };

    /// Seconds a chip listens before it gives up.
    static constexpr float kListenSeconds = 10.0f;

    /// A 3DS button Edit Controls can give a control: A, B, X, Y, the D-pad, L, R, ZL, ZR,
    /// Start, Select and Home.
    static bool Editable(int button);
    /// A Frame control a pressed button can be: the 3DS's buttons' usual ones but the triggers (a
    /// trigger is also how the beam clicks), and B, which cancels (the host leaves B out). Grips
    /// and stick clicks keep their own jobs.
    static bool Listenable(int control);
    /// The same for `source`: for a gamepad, any Input::Control (lane pads gives a gamepad's
    /// controls as Input::Controls; its Guide button is SteamOS's and its touchpad Touchpad Touch's).
    static bool Listenable(int control, RemapSource source);

    /// Edit mode starts from `table` of `source`; `defaults` is what Reset to Default restores.
    void Begin(const RemapTable& table, const RemapTable& defaults,
               RemapSource source = RemapSource::Frame);
    RemapSource Source() const {
        return source;
    }
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

    /// A 3DS button was clicked: it listens (the same one again, or one that cannot change,
    /// stops listening).
    Event Choose(int button);
    /// A button of the device was pressed while a 3DS button listens: the 3DS button takes it,
    /// and a 3DS button that had it takes the listening one's old control. Pressing the control
    /// it already has ends listening (Cancelled); one that cannot be used keeps it (None).
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
    RemapSource source = RemapSource::Frame;
    float listened = 0.0f;
};

/// The 3DS buttons on `control` in `table` (a bitmask by button index).
uint32_t RemapButtonsOn(const RemapTable& table, int control);

} // namespace FrameXr::Menu
