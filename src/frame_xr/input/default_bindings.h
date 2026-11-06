// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/settings.h"
#include "frame_xr/input/controls.h"

// AzaharFrame
// The default VR input profile: which virtual gamepad control drives which 3DS button or stick.
// This is CitraVR's mapping with the Frame's real D-pad as the D-pad. The table is the single
// source of truth: the default profile strings, the tests and MAPPING.md all follow it.

namespace FrameXr::Input {

struct ButtonBinding {
    Control control;
    Settings::NativeButton::Values button;
};

struct AnalogBinding {
    AnalogControl control;
    Settings::NativeAnalog::Values analog;
};

/// Controls that are deliberately unbound by default: BumperL, BumperR (free for the user),
/// StickClickR (reset panel positions, handled by the mapper before the game sees it).
inline constexpr std::array<ButtonBinding, 15> kDefaultButtonBindings{{
    {Control::A, Settings::NativeButton::A},
    {Control::B, Settings::NativeButton::B},
    {Control::X, Settings::NativeButton::X},
    {Control::Y, Settings::NativeButton::Y},
    {Control::DpadUp, Settings::NativeButton::Up},
    {Control::DpadDown, Settings::NativeButton::Down},
    {Control::DpadLeft, Settings::NativeButton::Left},
    {Control::DpadRight, Settings::NativeButton::Right},
    {Control::GripL, Settings::NativeButton::L},
    {Control::GripR, Settings::NativeButton::R},
    {Control::TriggerL, Settings::NativeButton::ZL},
    {Control::TriggerR, Settings::NativeButton::ZR},
    {Control::Menu, Settings::NativeButton::Start},
    {Control::View, Settings::NativeButton::Select},
    {Control::StickClickL, Settings::NativeButton::Home},
}};

inline constexpr std::array<AnalogBinding, 2> kDefaultAnalogBindings{{
    {AnalogControl::StickL, Settings::NativeAnalog::CirclePad},
    {AnalogControl::StickR, Settings::NativeAnalog::CStick},
}};

/// Applies the default profile to a pad state, giving the 3DS side of a mapper result.
struct ThreeDsPadState {
    std::array<bool, Settings::NativeButton::NumButtons> buttons{};
    std::array<Vec2f, Settings::NativeAnalog::NumAnalogs> analogs{};
};

inline ThreeDsPadState ApplyDefaultBindings(const ControlState& controls) {
    ThreeDsPadState pad;
    for (const auto& binding : kDefaultButtonBindings) {
        if (controls.Get(binding.control)) {
            pad.buttons[binding.button] = true;
        }
    }
    for (const auto& binding : kDefaultAnalogBindings) {
        pad.analogs[binding.analog] = controls.GetStick(binding.control);
    }
    return pad;
}

} // namespace FrameXr::Input
