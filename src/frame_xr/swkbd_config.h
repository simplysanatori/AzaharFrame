// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: what the game asked of its keyboard (Frontend::KeyboardConfig) as the VR
// keyboard's Spec (menu/swkbd_model.h): the buttons and their labels as Azahar's own dialog
// (citra_qt/applets/swkbd.cpp) makes them, and the applet's own checks
// (Frontend::SoftwareKeyboard::ValidateFilters and ValidateInput) with a message for each.

#pragma once

#include <string>

#include "core/frontend/applets/swkbd.h"
#include "frame_xr/menu/swkbd_model.h"

namespace FrameXr {

/// The VR keyboard for `config`. The checks call `keyboard` (which must outlive the Spec).
Swkbd::Spec MakeSwkbdSpec(const Frontend::SoftwareKeyboard& keyboard,
                          const Frontend::KeyboardConfig& config);

/// What the keyboard says under the field for an error ("" for None).
std::string SwkbdErrorText(Frontend::ValidationError error, const Frontend::KeyboardConfig& config);

} // namespace FrameXr
