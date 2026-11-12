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
    /// Settings::values.vr_immersive_mode (3 = Full (6DoF): Other Games)
    uint32_t mode;
    /// Settings::values.vr_si_mode_register_offset (the view matrix register)
    int32_t register_offset;
    /// Settings::values.vr_immersive_wider_view: the game's scenery holds up past its own view.
    bool wider_view = false;
    /// AzaharFrame depth: Settings::values.vr_immersive_positional_game_scaler (0 = 1x, 1 = 10x,
    /// 2 = 100x). In mode 3 it sets the eye distance and the head movement in the game's units
    /// (CitraVR: 10^value x 3D Depth / 10 x 6.5 cm), so it is the game's world scale.
    int32_t depth_multiplier = 0;
    /// AzaharFrame depth: Settings::values.vr_immersive_game_fov, the game camera's vertical
    /// angle in degrees, measured from the game's projection (0 = keep the setting).
    int32_t game_fov = 0;
    /// AzaharFrame depth: Settings::values.vr_immersive_eye_indicator ("87,2"; null = Auto).
    const char* eye_indicator = nullptr;
    /// AzaharFrame depth: Settings::values.vr_immersive_squeeze (Narrow Screen, 0 to 3).
    uint32_t squeeze = 0;
};

/// The preset for the game with this title id (any region), or null.
const ImmersivePreset* FindImmersivePreset(uint64_t title_id);

/// Every title id with a preset, for tests and documentation.
struct ImmersivePresetTitle {
    uint64_t title_id;
    const ImmersivePreset* preset;
};
const ImmersivePresetTitle* ImmersivePresetTitles(std::size_t& count);

} // namespace FrameXr
