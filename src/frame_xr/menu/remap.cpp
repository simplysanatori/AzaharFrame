// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/menu/remap.h"

namespace FrameXr::Menu {

using Input::Control;

bool Remap::Editable(int control) {
    switch (static_cast<Control>(control)) {
    case Control::A:
    case Control::B:
    case Control::X:
    case Control::Y:
    case Control::DpadUp:
    case Control::DpadDown:
    case Control::DpadLeft:
    case Control::DpadRight:
    case Control::TriggerL:
    case Control::TriggerR:
    case Control::BumperL:
    case Control::BumperR:
    case Control::Menu:
    case Control::View:
        return true;
    default:
        return false;
    }
}

bool Remap::Listenable(int control) {
    return Editable(control) && control != static_cast<int>(Control::TriggerL) &&
           control != static_cast<int>(Control::TriggerR);
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

void Remap::Begin(const RemapTable& from, const RemapTable& reset_to) {
    table = from;
    defaults = reset_to;
    view.editing = true;
    view.listening = -1;
    view.rev++;
    listened = 0.0f;
}

Remap::Event Remap::Choose(int control) {
    if (!view.editing) {
        return Event::None;
    }
    if (!Editable(control) || control == view.listening) {
        return Cancel();
    }
    view.listening = control;
    view.rev++;
    listened = 0.0f;
    return Event::Listening;
}

Remap::Event Remap::Press(int control) {
    if (!view.editing || view.listening < 0) {
        return Event::None;
    }
    if (!Listenable(control)) {
        return Event::None; // a button that cannot take a function: keep listening
    }
    const int from = view.listening;
    view.listening = -1;
    view.rev++;
    if (control == from) {
        return Event::Cancelled;
    }
    const uint32_t mine = RemapButtonsOn(table, from);
    const uint32_t theirs = RemapButtonsOn(table, control);
    if (mine == 0 && theirs == 0) {
        return Event::Cancelled;
    }
    for (int i = 0; i < kRemapButtons; i++) {
        auto& c = table[static_cast<std::size_t>(i)];
        if (c == from) {
            c = static_cast<int8_t>(control);
        } else if (c == control) {
            c = static_cast<int8_t>(from);
        }
    }
    return mine != 0 && theirs != 0 ? Event::Swapped : Event::Moved;
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
