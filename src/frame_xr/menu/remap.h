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
#include "frame_xr/input/stick_route.h"

namespace FrameXr::Menu {

constexpr int kRemapButtons = 18; ///< Settings::NativeButton::NumButtons
using RemapTable = std::array<int8_t, kRemapButtons>;

/// AzaharFrame padsui: the 3DS buttons by Settings::NativeButton index (the menu has no settings
/// dependency; menu_panel.cpp checks they match).
namespace DsButton {
enum : int { A, B, X, Y, Up, Down, Left, Right, L, R, Start, Select, ZL = 14, ZR = 15, Home = 16 };
/// The two sticks, as Edit Controls' targets (after the buttons).
enum : int { CirclePad = 32, CStick = 33 };
} // namespace DsButton

/// AzaharFrame padsui: Circle Pad, C-Stick -> an encoded Input::StickRoute (stick_route.h), or -1.
using RemapSticks = std::array<int8_t, 2>;
inline constexpr RemapSticks kDefaultSticks{0, 1}; ///< the left stick the Circle Pad, the right the C-Stick

/// AzaharFrame padsui: whose buttons the table holds, for which presses can take a 3DS button.
enum class RemapSource : uint8_t { Frame, Pad };

/// What the drawing needs of the state.
struct RemapView {
    bool editing = false;
    int listening = -1; ///< the 3DS button (Settings::NativeButton) waiting for a press, or -1
    int step = 0;       ///< a stick listening: 0 "Push the Stick Up", 1 "Push the Stick Right"
    uint32_t rev = 0;   ///< counts every change, so the menu redraws
    bool operator==(const RemapView&) const = default;
};

class Remap {
public:
    enum class Event {
        None,      ///< nothing happened
        Listening, ///< a 3DS button now waits for a press
        Next,      ///< a stick took the push for up, and waits for the one for right
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

    /// Circle Pad and C-Stick.
    static bool IsStick(int target) {
        return target == DsButton::CirclePad || target == DsButton::CStick;
    }

    /// Edit mode starts from `table` and `sticks` of `source`; the defaults are what Reset to
    /// Default restores.
    void Begin(const RemapTable& table, const RemapTable& defaults,
               RemapSource source = RemapSource::Frame, const RemapSticks& sticks = kDefaultSticks,
               const RemapSticks& stick_defaults = kDefaultSticks);
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
    const RemapSticks& Sticks() const {
        return sticks;
    }
    /// A stick target listens.
    bool ListeningStick() const {
        return IsStick(view.listening);
    }
    const RemapView& View() const {
        return view;
    }

    /// A 3DS button or stick was clicked: it listens (the same one again, or one that cannot
    /// change, stops listening).
    Event Choose(int target);
    /// A button of the device was pressed while a 3DS button listens: the 3DS button takes it,
    /// and a 3DS button that had it takes the listening one's old control. Pressing the control
    /// it already has ends listening (Cancelled); one that cannot be used keeps it (None).
    Event Press(int control);
    /// The device's stick `stick` (Input::AnalogControl) is at `value` while a 3DS stick
    /// listens. A clear push along one axis is the first answer (up: Next), a push of the same
    /// stick along its other axis the second (right): the 3DS stick takes that stick with its
    /// axes as pushed, and the other 3DS stick, if it had that stick, takes this one's old route.
    /// The same route as before ends listening (Cancelled).
    Event Stick(int stick, Input::Vec2f value);
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
    RemapSticks sticks = kDefaultSticks;
    RemapSticks stick_defaults = kDefaultSticks;
    Input::StickPush up_push;
    RemapView view;
    RemapSource source = RemapSource::Frame;
    float listened = 0.0f;
};

/// The 3DS buttons on `control` in `table` (a bitmask by button index).
uint32_t RemapButtonsOn(const RemapTable& table, int control);

} // namespace FrameXr::Menu
