// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Immersive mode presets: games whose Full (6DoF) setup is known (CitraVR's playtest notes). When
// such a game starts and its per-game config has no value of its own, and the global value is
// still the default, these are used (citra_qt BootGame; docs/USER_GUIDE.md).

#pragma once

#include <cstddef>
#include <cstdint>

namespace FrameXr {

struct ImmersivePreset {
    const char* name;
    /// Settings::values.vr_immersive_mode (3 = Full (6DoF): Other Games; 0 = the game starts in
    /// the normal view, and the player can still turn Immersive Mode on)
    uint32_t mode;
    /// Settings::values.vr_si_mode_register_offset (the view matrix register)
    int32_t register_offset;
    /// AzaharFrame depth: Settings::values.vr_immersive_positional_game_scaler (0 = 1x, 1 = 10x,
    /// 2 = 100x). In mode 3 it sets the eye distance and the head movement in the game's units
    /// (CitraVR: 10^value x 3D Depth / 10 x 6.5 cm), so it is the game's world scale.
    int32_t depth_multiplier = 0;
    /// AzaharFrame depth: Settings::values.vr_immersive_game_fov, the game camera's vertical
    /// angle in degrees, measured from the game's projection (0 = keep the setting).
    int32_t game_fov = 0;
    /// AzaharFrame depth: Settings::values.vr_immersive_eye_indicator ("87,2"; null = Auto).
    const char* eye_indicator = nullptr;
    /// AzaharFrame immersive2: Settings::values.vr_immersive_wider_view_level (0 Off to 4 = 1.4x;
    /// -1 = the default, 1.4x).
    int32_t wider_level = -1;
    /// AzaharFrame kiu: Settings::values.vr_immersive_scale_x, Horizontal Scale in percent
    /// (-100 to 100; 0 = keep: the default). Was Narrow Screen: Narrow Screen 30 % is -30.
    int32_t scale_x = 0;
    /// AzaharFrame immersive: the immersive screen's default size, times the true-scale default
    /// (0 = 1; immersive_layout.h). A placement the player saved wins.
    float screen_size = 0.0f;
    /// AzaharFrame immersive2, fovonce: Settings::values.vr_immersive_match_camera_mode, 0 Off,
    /// 1 Set Once, 2 Always (-1 = the default, Always).
    int32_t match_camera = -1;
    /// AzaharFrame presets: Settings::values.factor_3d (3D Depth, 0 to 100; -1 = keep).
    int32_t factor_3d = -1;
    /// AzaharFrame presets: Settings::values.resolution_factor (Internal Resolution; 0 = Auto is
    /// the default, -1 = keep).
    int32_t resolution = -1;
    /// AzaharFrame presets: Settings::values.texture_filter (Settings::TextureFilter; -1 = keep).
    int32_t texture_filter = -1;
    /// AzaharFrame kiu: Settings::values.vr_immersive_scale_y, Vertical Scale in percent (0 =
    /// keep). Was Narrow Height.
    int32_t scale_y = 0;
    /// AzaharFrame presets: Settings::values.vr_hide_bottom_screen (-1 = keep).
    int32_t hide_bottom = -1;
    /// AzaharFrame presets: Settings::values.vr_top_aspect (-1 = keep).
    int32_t top_aspect = -1;
    /// AzaharFrame presets: the head-locked screen's default pitch (degrees from the view's
    /// middle) and distance (metres), when `own_placement`. A placement the player saved wins.
    bool own_placement = false;
    float place_pitch_deg = 0.0f;
    float place_distance = 0.0f;
};

/// The preset for the game with this title id (any region), or null.
const ImmersivePreset* FindImmersivePreset(uint64_t title_id);

/// Applies a preset to Settings::values for the game that is starting: each value only where
/// this game's config has no value of its own and the global one is still the default (a value
/// the player set, even Off, is left alone). Called by citra_qt BootGame after the per-game
/// config is loaded.
void ApplyPresetSettings(const ImmersivePreset& preset);

/// Every title id with a preset, for tests and documentation.
struct ImmersivePresetTitle {
    uint64_t title_id;
    const ImmersivePreset* preset;
};
const ImmersivePresetTitle* ImmersivePresetTitles(std::size_t& count);

} // namespace FrameXr
