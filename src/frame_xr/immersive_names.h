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
#include <cstdint>
#include <string>

namespace FrameXr::Immersive::Names {

/// Settings::values.vr_immersive_wider_view_level (was "Wider View").
inline constexpr const char* kRenderMargin = "Render Margin";
/// AzaharFrame fovaxis: Render Margin per axis (Settings::values.vr_immersive_render_margin_x,
/// _y; render_margin.h): the VR menu's and Azahar's window's rows, which replace the one Render
/// Margin row (the toasts and logs keep kRenderMargin).
inline constexpr const char* kRenderMarginHorizontal = "Render Margin: Horizontal";
inline constexpr const char* kRenderMarginHorizontalTip =
    "With Stretch, draws more of the game left and right of the 3DS screen, and the screen grows "
    "with it. Off adds nothing.";
inline constexpr const char* kRenderMarginVertical = "Render Margin: Vertical";
inline constexpr const char* kRenderMarginVerticalTip =
    "With Stretch, draws more of the game above and below the 3DS screen, and the screen grows "
    "with it. Off adds nothing.";
/// AzaharFrame fovaxis: Render Margin applies with Stretch only (Layout::MarginFactor): the
/// note under both rows' labels (VR menu) and after their values (Azahar's window) while Scale
/// Mode is Field of View, and on the grip + D-pad toast then.
inline constexpr const char* kAppliesWithStretch = "Applies with Stretch";
/// Settings::values.vr_immersive_scale_x and _y (were Narrow Screen and Narrow Height).
/// AzaharFrame fovaxis: in Field of View a narrower screen crops; Stretch squeezes.
inline constexpr const char* kHorizontalScale = "Horizontal Scale";
inline constexpr const char* kHorizontalScaleTip =
    "Makes the screen wider or narrower. With Stretch, narrower brings the side HUD in.";
inline constexpr const char* kVerticalScale = "Vertical Scale";
inline constexpr const char* kVerticalScaleTip = "Makes the immersive screen taller or shorter.";
/// AzaharFrame fovaxis: the note under Horizontal or Vertical Scale's label (VR menu) and after
/// its value (Azahar's window) when the settings go past the caps against warping
/// (Layout::ScalesMayWarp).
inline constexpr const char* kMayWarp = "May Warp";
/// AzaharFrame fovscale: Settings::values.vr_immersive_scale_mode (0 Stretch, 1 Field of View):
/// how the two scales act. The VR menu's segmented row and Azahar's window.
inline constexpr const char* kScaleMode = "Scale Mode";
inline constexpr const char* kScaleModeTip = "How Horizontal and Vertical Scale change the picture.";
inline constexpr const char* kScaleStretch = "Stretch";
inline constexpr const char* kScaleStretchTip =
    "Stretches the whole picture, HUD included. The world looks wider or thinner.";
inline constexpr const char* kScaleFieldOfView = "Field of View";
inline constexpr const char* kScaleFieldOfViewTip =
    "Shows more or less of the game, cropping when smaller. The world keeps its shape.";
/// AzaharFrame fovaxis: Fill View (VR menu, under Scale Mode): sets the screen's size and both
/// scales once so the screen covers the view (Layout::FillView); the toast is Layout::FillToast.
inline constexpr const char* kFillView = "Fill View";
inline constexpr const char* kFillViewTip =
    "Sets the screen's size and both scales once to cover your view, as far as they reach. "
    "Adjust from there.";

// AzaharFrame natural: 3D Depth in the Full modes, World Scale and Follow Screen Size. Material 3
// Title Case for names and choices, sentence case for tooltips.

/// The 3D Depth row (VR menu, Graphics): the 3DS slider in Normal and Wide, a share of the
/// natural 3D in the Full modes (vr_immersive_depth).
inline constexpr const char* kDepth3d = "3D Depth";
inline constexpr const char* kDepth3dTip =
    "How deep the 3D looks. In the Full modes, 100% puts the farthest scenery at infinity.";
/// Azahar's window, VR group: the Full modes' 3D Depth.
inline constexpr const char* kImmersiveDepth = "Immersive 3D Depth";
inline constexpr const char* kImmersiveDepthTip =
    "How deep the 3D looks in the Full modes. At 100%, the farthest scenery is at infinity.";

/// Settings::values.vr_immersive_world_scale (was the Depth Factor Multiplier).
inline constexpr const char* kWorldScale = "World Scale";
inline constexpr const char* kWorldScaleTip =
    "How big the game world feels. Hold a grip and push the stick up or down to change it.";
inline constexpr const char* kWorldScaleWindowTip =
    "How big the game world feels in the Full modes. In VR, hold the immersive screen with a "
    "grip and push the stick up or down.";

/// World Scale's range, percent (100: the default), and the slider's stops: the R20 series from
/// 10 to 400 (6 a doubling, each 12 % more than the last).
inline constexpr float kWorldScaleAuto = 100.0f;
inline constexpr float kWorldScaleMin = 10.0f;
inline constexpr float kWorldScaleMax = 400.0f;
inline constexpr std::array<float, 33> kWorldScaleStops{
    10,  11,  12,  14,  16,  18,  20,  22,  25,  28,  32,  36,  40,  45,  50,  56,  63,
    71,  80,  90,  100, 112, 125, 140, 160, 180, 200, 224, 250, 280, 315, 355, 400};
/// Within the range (the default for anything not finite).
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
/// The grip's World Scale after a ratio of the stick's push: exact, only clamped (never snapped to
/// a slider stop; the menu's slider shows the nearest stop, the value stays as it is).
inline float WorldScaleAfterGrip(float before, float ratio) {
    return ClampWorldScale(ClampWorldScale(before) * ratio);
}
/// The whole percent the toast shows for `percent` (what WorldScaleText prints).
inline long WorldScaleWhole(float percent) {
    return std::lround(ClampWorldScale(percent));
}
/// How often the grip's toast may change (nanoseconds). Every change of text redraws the toast row
/// and uploads the whole toast atlas through a blocking copy on the frame thread (which shares the
/// GPU queue with the game): above about 100 % the whole percent changes every frame, 120 uploads a
/// second, which stalls the frames and the game while the grip is held. The value itself is
/// computed every frame; the toast shows it at this rate.
inline constexpr int64_t kWorldToastIntervalNs = 80'000'000;
/// When the grip's toast changes: at most every kWorldToastIntervalNs, and only to another whole
/// percent. A value that changed and then stopped is shown by the next call after the interval.
struct WorldScaleToastGate {
    long shown = -1;
    int64_t last_ns = 0;
    bool Due(long whole, int64_t now_ns, bool force = false) {
        if (whole == shown || (!force && shown >= 0 && now_ns - last_ns < kWorldToastIntervalNs)) {
            return false;
        }
        shown = whole;
        last_ns = now_ns;
        return true;
    }
    void Reset() {
        shown = -1;
        last_ns = 0;
    }
};
/// "45%", "100%" (as rounded).
inline std::string WorldScaleText(float percent) {
    return std::to_string(WorldScaleWhole(percent)) + "%";
}
/// "World Scale: 45%", "World Scale: 100%" (the grip's toast and the menu's).
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
/// AzaharFrame flash: Settings::values.vr_motion_clarity (frame_xr/motion_clarity.h).
inline constexpr const char* kMotionClarity = "Motion Clarity";
/// AzaharFrame projdef: experimental (the owner's A/B, 1.1.1-590); its tooltip says so.
inline constexpr const char* kMotionClarityTip =
    "Experimental. A clearer, more solid picture, but it may flicker or double in demanding scenes.";
inline constexpr const char* kMotionClarityWindowTip = kMotionClarityTip;
/// AzaharFrame projdef: the chip beside Motion Clarity's label in the VR menu.
inline constexpr const char* kExperimental = "Experimental";
/// AzaharFrame projdef: Settings::values.vr_motion_clarity_immersive, the row under Motion Clarity
/// (VR menu), its Configure box, its toast each time it is turned on.
inline constexpr const char* kMotionClarityImmersive = "In Immersive Modes";
inline constexpr const char* kMotionClarityImmersiveTip =
    "Experimental. May flicker, mostly in demanding games.";
inline constexpr const char* kMotionClarityImmersiveWindow = "Motion Clarity in Immersive Modes";
inline constexpr const char* kMotionClarityImmersiveToast =
    "Motion Clarity: Experimental \u00b7 May Flicker";
/// AzaharFrame projdef: Settings::values.vr_screens_in_projection, as its opposite: the Primary
/// screen drawn as a panel (a quad layer) in the Full modes. The VR menu's row (with the
/// Experimental chip), its tooltip, its note while Motion Clarity takes the panel, its toasts;
/// the Configure box uses the label and tooltip.
inline constexpr const char* kScreenAsPanel = "Primary Screen as Panel";
inline constexpr const char* kScreenAsPanelTip =
    "Experimental. Draws the Primary screen as a separate panel in the Full modes. It may double "
    "on fast head turns.";
inline constexpr const char* kScreenAsPanelNote = "Used by Motion Clarity";
inline constexpr const char* kScreenAsPanelOnToast = "Primary Screen as Panel: May Double";
inline constexpr const char* kScreenAsPanelOffToast = "Primary Screen as Panel: Off";

/// The toast after resizing the screen with Follow Screen Size on.
inline constexpr const char* kWorldScaleFollowsToast = "Follow Screen Size: On";

/// AzaharFrame integ8: Azahar's window (Graphics, Enhancements: Depth, Settings::values.factor_3d),
/// global or a game's properties. AzaharFrame natural: the Full modes use Immersive 3D Depth.
inline constexpr const char* kDepth3dWindowTip =
    "How deep the 3D looks. The Full immersive modes use Immersive 3D Depth instead.";

/// AzaharFrame natural: the toast when the 3D Factor shortcuts change the Full modes' 3D Depth:
/// "3D Depth: 45%".
inline std::string DepthToast(unsigned percent) {
    return std::string(kDepth3d) + ": " + std::to_string(percent > 100 ? 100 : percent) + "%";
}

} // namespace FrameXr::Immersive::Names
