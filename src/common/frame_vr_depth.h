// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame vrsize: the 3D slider the games see is the 3D depth setting times this (0 to 1).
// frame_xr sets it while the normal top screen is made bigger than its normal size (grip + stick
// sideways): the stereo picture's left-right offsets grow with the screen in metres, so at 3
// times the size a distant object's two images would be further apart than the eyes and could
// not be fused. 1 / scale keeps them as far apart as at the normal size. The 3D setting itself
// is left alone (never saved).

#pragma once

#include <atomic>

namespace Settings {
inline std::atomic<float> frame_vr_depth_scale{1.0f};

// AzaharFrame stereo: in immersive mode 2 the game's own two eyes are shown on one big screen,
// where its far scenery can end up farther apart than the eyes (they would have to diverge: a
// double image). frame_xr (immersive.cpp, the stereo cap) sets the most the slider may be so it
// never does; below 0, no limit. Only the slider the game is shown: the 3D Depth setting itself,
// and the head-motion scale made from it, stay as they are.
inline std::atomic<float> frame_vr_stereo_cap{-1.0f};
// The slider the game was shown last (HID's pad update), for the emulation thread's measure of
// the game's far scenery per slider unit (video_core/vr_immersive.h).
inline std::atomic<float> frame_vr_slider_shown{0.0f};

// AzaharFrame natural: the slider mode 2 shows the game, set by frame_xr every frame (immersive.cpp
// UpdateNatural: 3D Depth x the natural limit); below 0 when not set (another mode, no game yet).
inline std::atomic<float> frame_vr_natural_slider{-1.0f};

/// The slider a game in VR is shown in modes 0 to 2: the 3D Depth (0 to 1) times the depth
/// scale, at most `cap` when that is 0 or more. AzaharFrame natural: `natural` instead when it is
/// 0 or more (mode 2).
inline float FrameVrSlider(float depth, float depth_scale, float cap, float natural = -1.0f) {
    if (natural >= 0.0f) {
        return natural < 1.0f ? natural : 1.0f;
    }
    const float s = depth * depth_scale;
    return cap >= 0.0f && s > cap ? cap : s;
}
} // namespace Settings
