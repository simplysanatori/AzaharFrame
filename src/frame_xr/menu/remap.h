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
/// The mouse's own targets: the input that touches the bottom screen, the one that opens the
/// menu (RemapExtras).
enum : int { Touch = 34, OpenMenu = 35 };
/// AzaharFrame keys: the keyboard's modifier key (both sticks tilted halfway while held); Open
/// Menu is the keyboard's too.
enum : int { Modifier = 36 };
} // namespace DsButton

/// AzaharFrame padsui: Circle Pad, C-Stick -> an encoded Input::StickRoute (stick_route.h), or -1.
using RemapSticks = std::array<int8_t, 2>;
inline constexpr RemapSticks kDefaultSticks{0, 1}; ///< the left stick the Circle Pad, the right the C-Stick

/// AzaharFrame padsui: the mouse's Touch Screen and Open Menu inputs (MouseInputs), or -1.
using RemapExtras = std::array<int8_t, 2>;
inline constexpr RemapExtras kNoExtras{-1, -1};

/// AzaharFrame keys: the keyboard's keys for the sticks (the Circle Pad's up, down, left, right,
/// then the C-Stick's) and its modifier key, as key codes (key_names.h), -1 none.
using RemapKeys = std::array<int8_t, 9>;
inline constexpr RemapKeys kNoKeys{-1, -1, -1, -1, -1, -1, -1, -1, -1};
inline constexpr std::size_t kModifierSlot = 8;

/// AzaharFrame padsui: whose buttons the table holds, for which presses can take a 3DS button.
/// The mouse's table holds MouseInputs (Left Click...) and it has the extras. AzaharFrame keys:
/// the keyboard's holds key codes; its sticks are four keys each (RemapKeys), and of the extras
/// it has Open Menu, and the modifier.
enum class RemapSource : uint8_t { Frame, Pad, Mouse, Keyboard };

/// What the drawing needs of the state.
struct RemapView {
    bool editing = false;
    int listening = -1; ///< the 3DS button (Settings::NativeButton) waiting for a press, or -1
    /// A stick listening: 0 "Push the Stick Up", 1 "Push the Stick Right"; the keyboard's 0 to
    /// 3: the key for up, down, left, right.
    int step = 0;
    /// AzaharFrame input2: 3, 2, 1 in the last seconds before listening gives up, else 0.
    int countdown = 0;
    uint32_t rev = 0;   ///< counts every change, so the menu redraws
    bool operator==(const RemapView&) const = default;
};

class Remap {
public:
    enum class Event {
        None,      ///< nothing happened
        Listening, ///< a 3DS button now waits for a press
        Next,      ///< a stick took the push for up, and waits for the one for right (the
                   ///< keyboard: a key for one direction, and waits for the next)
        Moved,     ///< the 3DS button took a control no other 3DS button had
        Swapped,   ///< the 3DS button took another's control, which took its old one
        Cancelled, ///< listening ended with no change
        Reset,     ///< back to the default profile
        Done,      ///< edit mode ended
    };

    /// Seconds a chip listens before it gives up, keeping its binding. AzaharFrame input2: 3 s
    /// quietly, then the prompt counts 3, 2, 1 (the countdown shows only when no answer came for
    /// kQuietSeconds, so a quick press never sees it; a stick's first push starts it again).
    static constexpr float kQuietSeconds = 3.0f;
    static constexpr float kListenSeconds = 6.0f;
    /// AzaharFrame input2: the pointer's select (the mouse's touch input in Pointer mode, the
    /// trigger as the beam's select) is never taken for this long after a chip starts listening:
    /// the click that chose the chip is not its answer.
    static constexpr float kSelectGuardSeconds = 0.3f;

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
    /// Touch Screen and Open Menu (the mouse's).
    static bool IsExtra(int target) {
        return target == DsButton::Touch || target == DsButton::OpenMenu;
    }
    /// AzaharFrame keys: the targets a source has besides the 3DS buttons and sticks (the
    /// mouse: Touch Screen, Open Menu; the keyboard: Open Menu, the modifier).
    static bool HasTarget(RemapSource source, int target);

    /// Edit mode starts from `table` and `sticks` of `source`; the defaults are what Reset to
    /// Default restores.
    void Begin(const RemapTable& table, const RemapTable& defaults,
               RemapSource source = RemapSource::Frame, const RemapSticks& sticks = kDefaultSticks,
               const RemapSticks& stick_defaults = kDefaultSticks,
               const RemapExtras& extras = kNoExtras,
               const RemapExtras& extra_defaults = kNoExtras, const RemapKeys& keys = kNoKeys,
               const RemapKeys& key_defaults = kNoKeys);
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
    const RemapExtras& Extras() const {
        return extras;
    }
    const RemapKeys& Keys() const { ///< AzaharFrame keys
        return keys;
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
    /// and a 3DS button that had it takes the listening one's old control (the mouse's Touch
    /// Screen and Open Menu swap with the buttons alike). Pressing the control
    /// it already has ends listening (Cancelled); one that cannot be used keeps it (None).
    /// AzaharFrame keys: the keyboard's 3DS stick takes four keys in turn (Next after each of
    /// the first three, each swapping as a button does); the last one gives Moved or Swapped
    /// for the four together (Cancelled when nothing changed). Cancel or the time running out
    /// before the fourth puts all four back.
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
    /// AzaharFrame input2: the device's control that is the pointer's select now (a MouseInput
    /// for the mouse), or -1. A press of it is ignored (listening goes on) for
    /// kSelectGuardSeconds after a chip starts listening, and always while `pointer_mode` (the
    /// device being edited is the pointer: its select must keep working the menu, so it can
    /// never be bound away).
    void SetPointerSelect(int control, bool pointer_mode);
    /// The press is the pointer's select and is not taken now (Press would ignore it).
    bool IgnoresPress(int control) const;

private:
    Event PressStickKey(int control);

    RemapTable table{};
    RemapTable defaults{};
    RemapSticks sticks = kDefaultSticks;
    RemapSticks stick_defaults = kDefaultSticks;
    RemapExtras extras = kNoExtras;
    RemapExtras extra_defaults = kNoExtras;
    RemapKeys keys = kNoKeys; ///< AzaharFrame keys
    RemapKeys key_defaults = kNoKeys;
    /// AzaharFrame keys: the keyboard's stick listening: what to put back on Cancel, and
    /// whether a step swapped.
    RemapTable before_table{};
    RemapExtras before_extras = kNoExtras;
    RemapKeys before_keys = kNoKeys;
    bool key_swapped = false;
    Input::StickPush up_push;
    RemapView view;
    RemapSource source = RemapSource::Frame;
    float listened = 0.0f;
    float since_choose = 0.0f; ///< AzaharFrame input2: for kSelectGuardSeconds
    int select_control = -1;
    bool select_pointer_mode = false;
};

/// The 3DS buttons on `control` in `table` (a bitmask by button index).
uint32_t RemapButtonsOn(const RemapTable& table, int control);

} // namespace FrameXr::Menu
