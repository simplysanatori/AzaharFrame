// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame reticle: Controls tab presets: games that start with a Controls value of their own
// (Per-Game) when the player has not set one. Kept apart from the immersive presets
// (presets.h); applied the same way, in citra_qt BootGame: only where this game's config has no
// value of its own and the global one is still the default, so a value the player set (Off
// too) is left alone.

#pragma once

#include <cstdint>
#include <iterator>

namespace FrameXr {

struct ControlsPreset {
    const char* name;
    /// Settings::values.vr_bottom_screen_reticle (0 = none: keep the setting).
    uint32_t bottom_screen_reticle = 0;
    /// AzaharFrame fixes7: Settings::values.vr_reticle_stick_moves on (false: keep the setting).
    bool stick_moves_reticle = false;
};

/// Kid Icarus: Uprising aims with the stylus on the bottom screen while the left hand moves
/// and fires: the head aims the reticle (Headset) and either trigger touches.
/// AzaharFrame fixes7: and the right stick moves the reticle too (Stick Moves Reticle): with the
/// C-Stick & Gyro patch the stick aims only while the screen is not touched ("Touchscreen input
/// retains priority", its INSTALLATION.md), so the stick and the head add up into the touch.
inline constexpr ControlsPreset kKidIcarusUprisingControls{
    .name = "Kid Icarus: Uprising", .bottom_screen_reticle = 3, .stick_moves_reticle = true};

struct ControlsPresetTitle {
    uint64_t title_id;
    const ControlsPreset* preset;
};

/// Title ids as presets.cpp has them for the game: Japan, the USA and Europe.
inline constexpr ControlsPresetTitle kControlsPresetTitles[] = {
    {0x0004000000030100, &kKidIcarusUprisingControls},
    {0x0004000000030200, &kKidIcarusUprisingControls},
    {0x0004000000030300, &kKidIcarusUprisingControls},
};

/// The Controls preset for the game with this title id (any region), or null.
inline const ControlsPreset* FindControlsPreset(uint64_t title_id) {
    for (const auto& entry : kControlsPresetTitles) {
        if (entry.title_id == title_id) {
            return entry.preset;
        }
    }
    return nullptr;
}

} // namespace FrameXr
