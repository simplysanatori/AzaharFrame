// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame reticle: Controls tab presets: games that start with a Controls value of their own
// (Per-Game) when the player has not set one. Kept apart from the immersive presets
// (presets.h); applied in citra_qt BootGame, after the game's file is read (ApplyControlsPreset).
// AzaharFrame kiu: starting values only, never a lock: a value is given only where this game's
// file has none and the global one is still the default, and only once per game: the values
// given (or the game already had) are marked in the game's file (vr_controls_preset_done), so
// whatever the player chooses afterwards stays, Use Global Value too.

#pragma once

#include <cstdint>
#include <iterator>
#include <vector>

#include "core/mods/library.h"

namespace FrameXr {

struct ControlsPreset {
    const char* name;
    /// Settings::values.vr_bottom_screen_reticle (0 = none: keep the setting).
    uint32_t bottom_screen_reticle = 0;
    /// AzaharFrame kiu: Settings::values.vr_motion_controls (-1 = none: keep the setting).
    int motion_controls = -1;
    /// AzaharFrame kiu: Settings::values.vr_reticle_edge_cstick on (C-Stick at Edge; false:
    /// keep the setting).
    bool edge_cstick = false;
    /// AzaharFrame kiuedge: edge_cstick is given only when a C-Stick mod for the game is installed
    /// (ApplyControlsPreset's `cstick_mod_installed`); without one the setting is left alone.
    bool edge_cstick_needs_mod = false;
};

/// AzaharFrame kiu: the bits of vr_controls_preset_done: a preset value already decided for the
/// game (given, or the game had its own), so it is never given again.
enum ControlsPresetField : uint32_t {
    kPresetReticle = 1u << 0,
    kPresetMotionControls = 1u << 1,
    kPresetEdgeCStick = 1u << 2,
};

/// Kid Icarus: Uprising aims with the stylus on the bottom screen while the left hand moves
/// and fires: the head aims the reticle (Headset) and either trigger touches.
/// AzaharFrame kiu: for the C-Stick & Gyro mod, the way people play it: its C-Stick aim turns the
/// camera at the screen's edge, so a touch held at the edge pushes the C-Stick there (C-Stick
/// at Edge on). Right Stick Moves Touch stays off (the global default): with the mod the right
/// stick aims as the C-Stick already, and the mod gives the touch priority over it ("Touchscreen
/// input retains priority", its INSTALLATION.md). Motion Controls Off: with them on, the mod's
/// gyro aim moved the reticle while the head or the controller moved. The player can turn any of
/// them on or off, for this game or everywhere.
inline constexpr ControlsPreset kKidIcarusUprisingControls{.name = "Kid Icarus: Uprising",
                                                           .bottom_screen_reticle = 3,
                                                           .motion_controls = 0,
                                                           .edge_cstick = true,
                                                           .edge_cstick_needs_mod = true};

struct ControlsPresetTitle {
    uint64_t title_id;
    const ControlsPreset* preset;
};

/// Title ids as presets.cpp has them for the game: Japan (0004000000030000, as the C-Stick & Gyro
/// mod lists it, AzaharFrame kiu), then the three of the compatibility list.
inline constexpr ControlsPresetTitle kControlsPresetTitles[] = {
    {0x0004000000030000, &kKidIcarusUprisingControls},
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

/// AzaharFrame kiuedge: catalogue ids of the Kid Icarus: Uprising C-Stick mods (C-Stick & Gyro and
/// its widescreen builds; hosiniku's kiu-cstick-aim, whose entry another lane adds).
inline constexpr const char* kCStickModIds[] = {
    "kiu-cstick-gyro",
    "kiu-cstick-gyro-widescreen-16-9",
    "kiu-cstick-gyro-ultrawide-21-9",
    "kiu-cstick-aim",
};

/// AzaharFrame kiuedge: whether one of these (a game's mod list, Mods::Library::ListLoaded) is a
/// turned-on C-Stick mod: a catalogue id in kCStickModIds; or, for a mod put in by hand or from a
/// zip, a full code replacement (code.bin, as C-Stick & Gyro installs) or a name with "cstick" /
/// "c-stick" / "c_stick" in it (item name, id or file path). Mods in the library but off do not count.
bool CStickModInstalled(const std::vector<Mods::ModInfo>& mods);
/// The same for the mods folder of this title (Mods::Default()).
bool CStickModInstalled(uint64_t title_id);

/// AzaharFrame kiu: gives the running game the preset's values, with the game's file read into
/// the settings: each one only where the game has no value of its own, the global value is still
/// the default, and vr_controls_preset_done does not have its bit. A value given, or one the game
/// already had, sets its bit (in the game's value of vr_controls_preset_done, saved with the
/// game's file); a global value the player changed sets none (the preset may give it later, once
/// the global value is the default again). Returns the bits given this time.
/// AzaharFrame kiuedge: a value with `edge_cstick_needs_mod` is given only when
/// `cstick_mod_installed`; without one it is left alone and not marked, so installing the mod
/// later lets the preset give it then (still only where the player has no value of their own).
uint32_t ApplyControlsPreset(const ControlsPreset& preset, bool cstick_mod_installed);

} // namespace FrameXr
