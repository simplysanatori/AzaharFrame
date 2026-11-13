// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu: the immersive settings' names and tooltips, in one place, so a name changes in
// one edit: the VR menu, the toasts, the help diagram and Azahar's window all take them from here.
// Header only (the menu library and Qt use it without the rest of frame_xr).

#pragma once

#include <algorithm> // AzaharFrame natural: World Scale's text
#include <array>
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

// AzaharFrame natural: 3D Depth in the Full modes, World Scale and Follow Screen Size. Material 3
// Title Case for names and choices, sentence case for tooltips.

/// The 3D Depth row (VR menu, Graphics): the 3DS slider in Normal and Wide, a share of the
/// natural 3D in the Full modes (vr_immersive_depth).
inline constexpr const char* kDepth3d = "3D Depth";
inline constexpr const char* kDepth3dTip =
    "How deep the 3D looks. In the Full modes, 100% makes faraway scenery look truly far away.";
/// Azahar's window, VR group: the Full modes' 3D Depth.
inline constexpr const char* kImmersiveDepth = "Immersive 3D Depth";
inline constexpr const char* kImmersiveDepthTip =
    "How deep the 3D looks in the Full modes. At 100%, faraway scenery looks truly far away.";

/// Settings::values.vr_immersive_world_scale (was the Depth Factor Multiplier).
inline constexpr const char* kWorldScale = "World Scale";
/// Its value at 100 %, and that on the VR menu's slider (as Internal Resolution's "Auto").
inline constexpr const char* kWorldScaleAutomatic = "Automatic";
inline constexpr const char* kWorldScaleAutoShort = "Auto";
inline constexpr const char* kWorldScaleTip =
    "How big the game world feels. Automatic matches the game's own 3D. Grip and push the stick "
    "to change it while you play.";
inline constexpr const char* kWorldScaleWindowTip =
    "How big the game world feels in the Full modes. Automatic (100%) matches the game's own 3D. "
    "In VR, grip and push the stick forward or back to change it.";

/// World Scale's range, percent (100: Automatic), and the slider's stops: the R20 series from
/// 10 to 400 (6 a doubling, each 12 % more than the last).
inline constexpr float kWorldScaleAuto = 100.0f;
inline constexpr float kWorldScaleMin = 10.0f;
inline constexpr float kWorldScaleMax = 400.0f;
inline constexpr std::array<float, 33> kWorldScaleStops{
    10,  11,  12,  14,  16,  18,  20,  22,  25,  28,  32,  36,  40,  45,  50,  56,  63,
    71,  80,  90,  100, 112, 125, 140, 160, 180, 200, 224, 250, 280, 315, 355, 400};
/// Within the range (Automatic for anything not finite).
inline float ClampWorldScale(float percent) {
    return std::isfinite(percent) ? std::clamp(percent, kWorldScaleMin, kWorldScaleMax)
                                  : kWorldScaleAuto;
}
/// The stop nearest `percent` on the log scale.
inline int WorldScaleStop(float percent) {
    const float p = ClampWorldScale(percent);
    int best = 0;
    for (int i = 1; i < static_cast<int>(kWorldScaleStops.size()); ++i) {
        if (std::abs(std::log(kWorldScaleStops[static_cast<std::size_t>(i)] / p)) <
            std::abs(std::log(kWorldScaleStops[static_cast<std::size_t>(best)] / p))) {
            best = i;
        }
    }
    return best;
}
/// "Automatic" at 100 % (as rounded), else "45%".
inline std::string WorldScaleText(float percent) {
    const long rounded = std::lround(ClampWorldScale(percent));
    return rounded == static_cast<long>(kWorldScaleAuto) ? std::string(kWorldScaleAutomatic)
                                                          : std::to_string(rounded) + "%";
}
/// The VR menu slider's value: "Auto" at 100 % (its value column is narrow), else "45%".
inline std::string WorldScaleShortText(float percent) {
    const std::string t = WorldScaleText(percent);
    return t == kWorldScaleAutomatic ? std::string(kWorldScaleAutoShort) : t;
}
/// "World Scale: 45%", "World Scale: Automatic" (the grip's toast and the menu's).
inline std::string WorldScaleToast(float percent) {
    return std::string(kWorldScale) + ": " + WorldScaleText(percent);
}

/// Settings::values.vr_immersive_follow_screen.
inline constexpr const char* kFollowScreen = "Follow Screen Size";
inline constexpr const char* kFollowScreenTip =
    "When you resize the screen, the world grows or shrinks with it.";
inline constexpr const char* kFollowScreenWindowTip =
    "When you resize the screen in the Full modes, the world grows or shrinks with it. Off keeps "
    "the world's size.";
/// The toast after resizing the screen with Follow Screen Size on.
inline constexpr const char* kWorldScaleFollowsToast = "World Scale Follows Screen Size";

/// AzaharFrame integ8: Azahar's window (Graphics, Enhancements: Depth, Settings::values.factor_3d),
/// global or a game's properties. AzaharFrame natural: the Full modes use Immersive 3D Depth.
inline constexpr const char* kDepth3dWindowTip =
    "How deep the 3D looks. The Full modes use Immersive 3D Depth.";

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
