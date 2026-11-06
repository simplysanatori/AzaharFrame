// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include "frame_xr/input/types.h"

// AzaharFrame
// The "frame_xr" input engine exposes the Steam Frame controllers as a virtual gamepad. These are
// the engine's inputs: the physical controls after the mapper has decided which of them are for
// the game (a trigger that is aiming at a screen is a touch or a drag, not ZL/ZR). The default
// input profile binds them to the 3DS buttons; the Qt Controls dialog can rebind any of them.

namespace FrameXr::Input {

enum class Control : int {
    A,
    B,
    X,
    Y,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    GripL,
    GripR,
    TriggerL,
    TriggerR,
    BumperL,
    BumperR,
    Menu,
    View,
    StickClickL,
    StickClickR,
    Count,
};

enum class AnalogControl : int {
    StickL,
    StickR,
    Count,
};

constexpr int kNumControls = static_cast<int>(Control::Count);
constexpr int kNumAnalogControls = static_cast<int>(AnalogControl::Count);

constexpr const char* ControlName(Control control) {
    switch (control) {
    case Control::A:
        return "A";
    case Control::B:
        return "B";
    case Control::X:
        return "X";
    case Control::Y:
        return "Y";
    case Control::DpadUp:
        return "D-Pad Up";
    case Control::DpadDown:
        return "D-Pad Down";
    case Control::DpadLeft:
        return "D-Pad Left";
    case Control::DpadRight:
        return "D-Pad Right";
    case Control::GripL:
        return "Left Grip";
    case Control::GripR:
        return "Right Grip";
    case Control::TriggerL:
        return "Left Trigger";
    case Control::TriggerR:
        return "Right Trigger";
    case Control::BumperL:
        return "Left Bumper";
    case Control::BumperR:
        return "Right Bumper";
    case Control::Menu:
        return "Menu";
    case Control::View:
        return "View";
    case Control::StickClickL:
        return "Left Stick Click";
    case Control::StickClickR:
        return "Right Stick Click";
    default:
        return "Unknown";
    }
}

constexpr const char* AnalogControlName(AnalogControl control) {
    switch (control) {
    case AnalogControl::StickL:
        return "Left Stick";
    case AnalogControl::StickR:
        return "Right Stick";
    default:
        return "Unknown";
    }
}

/// The state of the virtual gamepad.
struct ControlState {
    std::array<bool, kNumControls> buttons{};
    std::array<Vec2f, kNumAnalogControls> sticks{};

    bool Get(Control control) const {
        return buttons[static_cast<std::size_t>(control)];
    }
    void Set(Control control, bool pressed) {
        buttons[static_cast<std::size_t>(control)] = pressed;
    }
    Vec2f GetStick(AnalogControl control) const {
        return sticks[static_cast<std::size_t>(control)];
    }
    void SetStick(AnalogControl control, Vec2f value) {
        sticks[static_cast<std::size_t>(control)] = value;
    }
};

} // namespace FrameXr::Input
