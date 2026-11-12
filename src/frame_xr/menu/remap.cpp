// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/menu/remap.h"

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
    // A gamepad's controls are Input::Controls too (lane pads: SDL's A B X Y, the D-pad, the
    // shoulders, the triggers, Start as Menu, Back as View, the stick clicks, the paddles as the
    // grips); its triggers do not click the beam, and its Guide and touchpad never reach here.
    return control >= 0 && control < Input::kNumControls;
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

void Remap::Begin(const RemapTable& from, const RemapTable& reset_to, RemapSource from_source) {
    table = from;
    defaults = reset_to;
    source = from_source;
    view.editing = true;
    view.listening = -1;
    view.rev++;
    listened = 0.0f;
}

Remap::Event Remap::Choose(int button) {
    if (!view.editing) {
        return Event::None;
    }
    if (!Editable(button) || button == view.listening) {
        return Cancel();
    }
    view.listening = button;
    view.rev++;
    listened = 0.0f;
    return Event::Listening;
}

Remap::Event Remap::Press(int control) {
    if (!view.editing || view.listening < 0) {
        return Event::None;
    }
    if (!Listenable(control, source)) {
        return Event::None; // a button that cannot be used: keep listening
    }
    const auto target = static_cast<std::size_t>(view.listening);
    view.listening = -1;
    view.rev++;
    const int8_t old = table[target];
    if (old == control) {
        return Event::Cancelled;
    }
    // A 3DS button that had the control takes the listening one's old control (or none).
    bool swapped = false;
    for (std::size_t i = 0; i < table.size(); i++) {
        if (i != target && table[i] == control) {
            table[i] = old;
            swapped = swapped || old >= 0;
        }
    }
    table[target] = static_cast<int8_t>(control);
    return swapped ? Event::Swapped : Event::Moved;
}

Remap::Event Remap::Cancel() {
    if (!view.editing || view.listening < 0) {
        return Event::None;
    }
    view.listening = -1;
    view.rev++;
    return Event::Cancelled;
}

Remap::Event Remap::Reset() {
    if (!view.editing) {
        return Event::None;
    }
    table = defaults;
    view.listening = -1;
    view.rev++;
    return Event::Reset;
}

Remap::Event Remap::Done() {
    if (!view.editing) {
        return Event::None;
    }
    view.editing = false;
    view.listening = -1;
    view.rev++;
    return Event::Done;
}

Remap::Event Remap::Tick(float seconds) {
    if (!view.editing || view.listening < 0) {
        return Event::None;
    }
    listened += seconds;
    return listened >= kListenSeconds ? Cancel() : Event::None;
}

} // namespace FrameXr::Menu
