// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/menu/edit_devices.h"

#include "frame_xr/input/controls.h"

namespace FrameXr::Menu {

namespace {

using Input::Control;

std::string FrameName(Control c, bool by_position) {
    // By position the Frame's A drives the virtual pad's B (mapper.cpp), and so on.
    if (by_position) {
        switch (c) {
        case Control::A:
            return "B";
        case Control::B:
            return "A";
        case Control::X:
            return "Y";
        case Control::Y:
            return "X";
        default:
            break;
        }
    }
    return Input::ControlName(c);
}

std::string PadName(ButtonNames n, Control c) {
    const bool ps = n == ButtonNames::DualShock || n == ButtonNames::DualSense;
    const bool nin = n == ButtonNames::Nintendo || n == ButtonNames::NintendoPos;
    const bool xbox = n == ButtonNames::Xbox || n == ButtonNames::Steam;
    switch (c) {
    case Control::A:
        return ps ? "Cross" : n == ButtonNames::NintendoPos ? "B" : "A";
    case Control::B:
        return ps ? "Circle" : n == ButtonNames::NintendoPos ? "A" : "B";
    case Control::X:
        return ps ? "Square" : n == ButtonNames::NintendoPos ? "Y" : "X";
    case Control::Y:
        return ps ? "Triangle" : n == ButtonNames::NintendoPos ? "X" : "Y";
    case Control::BumperL:
        return ps ? "L1" : nin ? "L" : xbox ? "LB" : "Left Bumper";
    case Control::BumperR:
        return ps ? "R1" : nin ? "R" : xbox ? "RB" : "Right Bumper";
    case Control::TriggerL:
        return ps ? "L2" : nin ? "ZL" : xbox ? "LT" : "Left Trigger";
    case Control::TriggerR:
        return ps ? "R2" : nin ? "ZR" : xbox ? "RT" : "Right Trigger";
    case Control::Menu:
        return ps ? "Options" : nin ? "Plus" : xbox ? "Menu" : "Start";
    case Control::View:
        return n == ButtonNames::DualSense ? "Create"
               : ps                        ? "Share"
               : nin                       ? "Minus"
               : xbox                      ? "View"
                                           : "Back";
    case Control::StickClickL:
        return ps ? "L3" : "Left Stick Click";
    case Control::StickClickR:
        return ps ? "R3" : "Right Stick Click";
    case Control::GripL: // lane pads: the left back paddles
        return n == ButtonNames::Steam ? "Left Grip" : "Left Paddle";
    case Control::GripR:
        return n == ButtonNames::Steam ? "Right Grip" : "Right Paddle";
    default:
        return Input::ControlName(c); // the D-pad
    }
}

} // namespace

ButtonNames NamesFor(int pad_type, bool steam_input) {
    switch (pad_type) {
    case 1: // Xbox 360
    case 2: // Xbox One, Series
        return ButtonNames::Xbox;
    case 3: // PS3
    case 4: // PS4
        return ButtonNames::DualShock;
    case 7: // PS5
        return ButtonNames::DualSense;
    case 5:  // Switch Pro
    case 11: // Joy-Con
    case 12:
    case 13:
        return steam_input ? ButtonNames::NintendoPos : ButtonNames::Nintendo;
    case 100: // Steam
        return ButtonNames::Steam;
    default:
        return ButtonNames::Generic;
    }
}

std::string SourceName(ButtonNames names, int control, bool face_by_position) {
    if (control < 0 || control >= Input::kNumControls) {
        return {};
    }
    const auto c = static_cast<Control>(control);
    return names == ButtonNames::Frame ? FrameName(c, face_by_position) : PadName(names, c);
}

std::string StickRouteName(int8_t route) {
    const Input::StickRoute r = Input::DecodeStickRoute(route);
    if (r.stick < 0 || r.stick >= Input::kNumAnalogControls) {
        return {};
    }
    const std::string name = Input::AnalogControlName(static_cast<Input::AnalogControl>(r.stick));
    return r.Plain() ? name : name + " Flipped";
}

RemapTable PositionalPadTable() {
    RemapTable t;
    t.fill(-1);
    const auto set = [&t](int button, Control c) {
        t[static_cast<std::size_t>(button)] = static_cast<int8_t>(c);
    };
    set(DsButton::A, Control::B);
    set(DsButton::B, Control::A);
    set(DsButton::X, Control::Y);
    set(DsButton::Y, Control::X);
    set(DsButton::Up, Control::DpadUp);
    set(DsButton::Down, Control::DpadDown);
    set(DsButton::Left, Control::DpadLeft);
    set(DsButton::Right, Control::DpadRight);
    set(DsButton::L, Control::BumperL);
    set(DsButton::R, Control::BumperR);
    set(DsButton::Start, Control::Menu);
    set(DsButton::Select, Control::View);
    set(DsButton::ZL, Control::TriggerL);
    set(DsButton::ZR, Control::TriggerR);
    return t;
}

} // namespace FrameXr::Menu
