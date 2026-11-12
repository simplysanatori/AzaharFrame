// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include "common/settings.h"

// AzaharFrame pads
namespace FrameXr::Input {

/// For each Settings::NativeButton, the device input that drives it, or -1. For the Frame
/// controllers an Input::Control, for a gamepad an Input::PadInput (gamepads.h). The same shape
/// as the remap lane's Menu::RemapTable.
using ButtonTable = std::array<int8_t, Settings::NativeButton::NumButtons>;
/// For each Settings::NativeAnalog (Circle Pad, C-Stick), the stick that drives it, or -1: an
/// Input::AnalogControl for the Frame, an Input::PadStick for a gamepad (both 0 left, 1 right).
/// AzaharFrame padsui: an encoded StickRoute (stick_route.h): the stick in bits 0-1 (so 0 and 1
/// are plain sticks, as before), its axes swapped or turned round in the bits above.
using AnalogTable = std::array<int8_t, Settings::NativeAnalog::NumAnalogs>;

/// Which Frame control drives each 3DS button and stick in the controls profile in use (the
/// profile's "frame_xr" bindings). A gamepad's 3DS buttons reach the game through them: each
/// one it presses presses the Frame control the profile binds to that button.
struct ProfileRoutes {
    ButtonTable buttons{};
    AnalogTable analogs{};
    bool operator==(const ProfileRoutes&) const = default;
};

} // namespace FrameXr::Input
