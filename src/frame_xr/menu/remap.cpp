// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/menu/remap.h"

#include <cmath>

#include "frame_xr/input/key_names.h"
#include "frame_xr/input/mouse.h"

namespace FrameXr::Menu {

using Input::Control;

bool Remap::Editable(int button) {
    return (button >= DsButton::A && button <= DsButton::Select) || button == DsButton::ZL ||
           button == DsButton::ZR || button == DsButton::Home;
}

bool Remap::Listenable(int control) {
    switch (static_cast<Control>(control)) {
    case Control::A:
    case Control::B:
    case Control::X:
    case Control::Y:
    case Control::DpadUp:
    case Control::DpadDown:
    case Control::DpadLeft:
    case Control::DpadRight:
    case Control::BumperL:
    case Control::BumperR:
    case Control::Menu:
    case Control::View:
        return true;
    default:
        return false;
    }
}

bool Remap::Listenable(int control, RemapSource from) {
    if (from == RemapSource::Frame) {
        return Listenable(control);
    }
    if (from == RemapSource::Mouse) { // AzaharFrame padsui: any of its seven inputs
        return control >= 0 && control < Input::kNumMouseInputs;
    }
    if (from == RemapSource::Keyboard) { // AzaharFrame keys: any key with a name
        return Input::BindableKey(control);
    }
    // A gamepad's controls are Input::Controls too (lane pads: SDL's A B X Y, the D-pad, the
    // shoulders, the triggers, Start as Menu, Back as View, the stick clicks, the paddles as the
    // grips); its triggers do not click the beam, and its Guide and touchpad never reach here.
    return control >= 0 && control < Input::kNumControls;
}

bool Remap::HasTarget(RemapSource from, int target) {
    switch (from) {
    case RemapSource::Mouse:
        return IsExtra(target);
    case RemapSource::Keyboard:
        return target == DsButton::OpenMenu || target == DsButton::Modifier;
    default:
        return false;
    }
}

uint32_t RemapButtonsOn(const RemapTable& table, int control) {
    uint32_t mask = 0;
    for (int i = 0; i < kRemapButtons; i++) {
        if (table[static_cast<std::size_t>(i)] == control) {
            mask |= 1u << i;
        }
    }
    return mask;
}

void Remap::Begin(const RemapTable& from, const RemapTable& reset_to, RemapSource from_source,
                  const RemapSticks& from_sticks, const RemapSticks& sticks_reset_to,
                  const RemapExtras& from_extras, const RemapExtras& extras_reset_to,
                  const RemapKeys& from_keys, const RemapKeys& keys_reset_to) {
    table = from;
    keys = from_keys;
    key_defaults = keys_reset_to;
    extras = from_extras;
    extra_defaults = extras_reset_to;
    defaults = reset_to;
    source = from_source;
    sticks = from_sticks;
    stick_defaults = sticks_reset_to;
    view.editing = true;
    view.listening = -1;
    view.step = 0;
    view.countdown = 0;
    view.rev++;
    listened = 0.0f;
}

Remap::Event Remap::Choose(int target) {
    if (!view.editing) {
        return Event::None;
    }
    const bool extra = HasTarget(source, target);
    if ((!Editable(target) && !IsStick(target) && !extra) || target == view.listening) {
        return Cancel();
    }
    if (view.listening >= 0) {
        Cancel(); // AzaharFrame keys: a keyboard stick half done goes back first
    }
    before_table = table;
    before_extras = extras;
    before_keys = keys;
    key_swapped = false;
    view.listening = target;
    view.step = 0;
    view.countdown = 0;
    up_push = {};
    view.rev++;
    listened = 0.0f;
    since_choose = 0.0f;
    return Event::Listening;
}

Remap::Event Remap::Press(int control) {
    const bool key_stick = source == RemapSource::Keyboard && IsStick(view.listening);
    if (!view.editing || view.listening < 0 || (IsStick(view.listening) && !key_stick)) {
        return Event::None;
    }
    if (!Listenable(control, source) || IgnoresPress(control)) {
        return Event::None; // a button that cannot be used: keep listening
    }
    if (key_stick) {
        return PressStickKey(control);
    }
    // The table's 3DS buttons, then the mouse's Touch Screen and Open Menu (the keyboard's Open
    // Menu and modifier, and its sticks' keys), as one list.
    const auto slot = [this](int target) -> int8_t& {
        if (target == DsButton::Modifier) {
            return keys[kModifierSlot];
        }
        return IsExtra(target) ? extras[static_cast<std::size_t>(target - DsButton::Touch)]
                               : table[static_cast<std::size_t>(target)];
    };
    const int target = view.listening;
    view.listening = -1;
    view.step = 0;
    view.countdown = 0;
    view.rev++;
    const int8_t old = slot(target);
    if (old == control) {
        return Event::Cancelled;
    }
    // A 3DS button that had the control takes the listening one's old control (or none).
    bool swapped = false;
    const auto take_old = [&](int8_t& other) {
        if (other == control) {
            other = old;
            swapped = swapped || old >= 0;
        }
    };
    for (int i = 0; i < kRemapButtons; i++) {
        if (i != target) {
            take_old(table[static_cast<std::size_t>(i)]);
        }
    }
    for (int t : {static_cast<int>(DsButton::Touch), static_cast<int>(DsButton::OpenMenu),
                  static_cast<int>(DsButton::Modifier)}) {
        if (t != target && HasTarget(source, t)) {
            take_old(slot(t));
        }
    }
    if (source == RemapSource::Keyboard) {
        for (std::size_t i = 0; i < kModifierSlot; i++) {
            take_old(keys[i]);
        }
    }
    slot(target) = static_cast<int8_t>(control);
    return swapped ? Event::Swapped : Event::Moved;
}

// AzaharFrame keys: one of the four keys of a keyboard's 3DS stick (Press).
Remap::Event Remap::PressStickKey(int control) {
    const std::size_t at = (view.listening == DsButton::CirclePad ? 0 : 4) +
                           static_cast<std::size_t>(view.step);
    int8_t& mine = keys[at];
    const int8_t old = mine;
    if (old != control) {
        const auto take_old = [&](int8_t& other) {
            if (&other != &mine && other == control) {
                other = old;
                key_swapped = key_swapped || old >= 0;
            }
        };
        for (int8_t& b : table) {
            take_old(b);
        }
        take_old(extras[static_cast<std::size_t>(DsButton::OpenMenu - DsButton::Touch)]);
        for (int8_t& k : keys) {
            take_old(k);
        }
        mine = static_cast<int8_t>(control);
    }
    view.rev++;
    view.countdown = 0;
    listened = 0.0f;
    if (view.step < 3) {
        view.step++;
        return Event::Next;
    }
    view.listening = -1;
    view.step = 0;
    if (table == before_table && extras == before_extras && keys == before_keys) {
        return Event::Cancelled;
    }
    return key_swapped ? Event::Swapped : Event::Moved;
}

Remap::Event Remap::Stick(int stick, Input::Vec2f value) {
    if (!view.editing || !IsStick(view.listening) || source == RemapSource::Keyboard) {
        return Event::None;
    }
    const Input::StickPush push = Input::DetectStickPush(stick, value);
    if (push.stick < 0) {
        return Event::None;
    }
    if (view.step == 0) {
        up_push = push;
        view.step = 1;
        view.countdown = 0;
        view.rev++;
        listened = 0.0f;
        return Event::Next;
    }
    const Input::StickRoute route = Input::RouteFromPushes(up_push, push);
    if (route.stick < 0) {
        return Event::None; // still the first push, or the other stick: keep listening
    }
    const std::size_t target = view.listening == DsButton::CirclePad ? 0 : 1;
    const std::size_t other = 1 - target;
    view.listening = -1;
    view.step = 0;
    view.countdown = 0;
    view.rev++;
    const int8_t old = sticks[target];
    const int8_t now = Input::EncodeStickRoute(route);
    if (old == now) {
        return Event::Cancelled;
    }
    bool swapped = false;
    if (Input::DecodeStickRoute(sticks[other]).stick == route.stick) {
        sticks[other] = old;
        swapped = old >= 0;
    }
    sticks[target] = now;
    return swapped ? Event::Swapped : Event::Moved;
}

Remap::Event Remap::Cancel() {
    if (!view.editing || view.listening < 0) {
        return Event::None;
    }
    if (source == RemapSource::Keyboard && IsStick(view.listening)) {
        // AzaharFrame keys: a stick's keys go back as they were (a half-done four).
        table = before_table;
        extras = before_extras;
        keys = before_keys;
    }
    view.listening = -1;
    view.step = 0;
    view.countdown = 0;
    view.rev++;
    return Event::Cancelled;
}

Remap::Event Remap::Reset() {
    if (!view.editing) {
        return Event::None;
    }
    table = defaults;
    sticks = stick_defaults;
    extras = extra_defaults;
    keys = key_defaults; // AzaharFrame keys
    view.listening = -1;
    view.step = 0;
    view.countdown = 0;
    view.rev++;
    return Event::Reset;
}

Remap::Event Remap::Done() {
    if (!view.editing) {
        return Event::None;
    }
    view.editing = false;
    view.listening = -1;
    view.step = 0;
    view.countdown = 0;
    view.rev++;
    return Event::Done;
}

Remap::Event Remap::Tick(float seconds) {
    if (!view.editing || view.listening < 0) {
        return Event::None;
    }
    listened += seconds;
    since_choose += seconds;
    if (listened >= kListenSeconds) {
        return Cancel(); // the old binding stays
    }
    // AzaharFrame input2: 3, 2, 1 once nothing came for kQuietSeconds.
    const int countdown =
        listened >= kQuietSeconds ? static_cast<int>(std::ceil(kListenSeconds - listened)) : 0;
    if (countdown != view.countdown) {
        view.countdown = countdown;
        view.rev++;
    }
    return Event::None;
}

void Remap::SetPointerSelect(int control, bool pointer_mode) {
    select_control = control;
    select_pointer_mode = control >= 0 && pointer_mode;
}

bool Remap::IgnoresPress(int control) const {
    return control >= 0 && control == select_control &&
           (select_pointer_mode || since_choose < kSelectGuardSeconds);
}

} // namespace FrameXr::Menu
