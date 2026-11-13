// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu: the immersive settings' names and tooltips, in one place, so a name changes in
// one edit: the VR menu, the toasts, the help diagram and Azahar's window all take them from here.
// Header only (the menu library and Qt use it without the rest of frame_xr).

#pragma once

namespace FrameXr::Immersive::Names {

/// Settings::values.vr_immersive_wider_view_level (was "Wider View").
inline constexpr const char* kRenderMargin = "Render Margin";
inline constexpr const char* kRenderMarginTip =
    "Renders extra picture around the 3DS screen's edges so more of the game shows. Off shows "
    "exactly the game's own picture.";

} // namespace FrameXr::Immersive::Names
