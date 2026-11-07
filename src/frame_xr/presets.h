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
