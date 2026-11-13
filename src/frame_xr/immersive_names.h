// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu: the immersive settings' names and tooltips, in one place, so a name changes in
// one edit: the VR menu, the toasts, the help diagram and Azahar's window all take them from here.
// Header only (the menu library and Qt use it without the rest of frame_xr).

#pragma once

#include <cmath>
#include <string>

namespace FrameXr::Immersive::Names {

/// Settings::values.vr_immersive_wider_view_level (was "Wider View").
inline constexpr const char* kRenderMargin = "Render Margin";
inline constexpr const char* kRenderMarginTip =
    "Renders extra picture around the 3DS screen's edges so more of the game shows. Off shows "
    "exactly the game's own picture.";
/// Settings::values.vr_immersive_scale_x and _y (were Narrow Screen and Narrow Height).
inline constexpr const char* kHorizontalScale = "Horizontal Scale";
inline constexpr const char* kHorizontalScaleTip =
    "Makes the immersive screen wider or narrower. Narrower brings the side HUD into view.";
inline constexpr const char* kVerticalScale = "Vertical Scale";
inline constexpr const char* kVerticalScaleTip = "Makes the immersive screen taller or shorter.";

/// AzaharFrame integ8: Settings::values.factor_3d. In Full (6DoF): Ocarina of Time, Majora's Mask
/// (mode 2) the game is shown at most the stereo cap's limit (immersive.cpp UpdateStereoCap).
inline constexpr const char* kDepth3d = "3D Depth";
inline constexpr const char* kDepth3dCapNote =
    "In Full (6DoF), it's limited so faraway scenery never goes past infinity.";
/// The VR menu's tooltip (the menu is on this game's values while one runs).
inline constexpr const char* kDepth3dTip =
    "How deep the 3D looks in this game. In Full (6DoF), it's limited so faraway scenery never "
    "goes past infinity.";
/// Azahar's window (Graphics, Enhancements: Depth), global or a game's properties.
inline constexpr const char* kDepth3dWindowTip =
    "How deep the 3D looks. In Full (6DoF), it's limited so faraway scenery never goes past "
    "infinity.";

/// AzaharFrame integ8: the toast when 3D Depth changes (the VR menu's slider or a shortcut) in
/// mode 2 and the cap applies: "3D Depth: 30% (Limited to 8%)", in whole percents. `limit` is
/// the cap's limit in slider units (below 0: none known), `depth_scale` the share of 3D Depth the
/// game gets (1 in the Full modes). Empty when the cap does not apply at `depth`, also when the
/// two round to the same percent.
inline std::string DepthLimitedToast(unsigned depth, float limit, float depth_scale = 1.0f) {
    if (depth == 0 || !(limit >= 0.0f) || !std::isfinite(limit) || !(depth_scale > 0.0f)) {
        return {};
    }
    const unsigned shown = depth > 100 ? 100 : depth;
    const float limit_pct = 100.0f * limit / depth_scale;
    const long rounded = std::lround(limit_pct);
    if (limit_pct >= static_cast<float>(shown) || rounded >= static_cast<long>(depth)) {
        return {};
    }
    return std::string(kDepth3d) + ": " + std::to_string(depth) + "% (Limited to " +
           std::to_string(rounded) + "%)";
}

} // namespace FrameXr::Immersive::Names
