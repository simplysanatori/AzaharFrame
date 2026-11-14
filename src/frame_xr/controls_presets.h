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
    /// AzaharFrame kiugyro: with a C-Stick mod installed, Motion Controls starts at this instead
    /// of `motion_controls` (-1: the same).
    int motion_controls_with_mod = -1;
    /// AzaharFrame kiugyro: with a C-Stick mod installed, Settings::values.vr_gyro_aim (0: keep
    /// the setting).
    uint32_t gyro_aim_with_mod = 0;
};

/// AzaharFrame kiu: the bits of vr_controls_preset_done: a preset value already decided for the
/// game (given, or the game had its own), so it is never given again.
enum ControlsPresetField : uint32_t {
    kPresetReticle = 1u << 0,
    kPresetMotionControls = 1u << 1,
    kPresetEdgeCStick = 1u << 2,
    kPresetGyroAim = 1u << 3, ///< AzaharFrame kiugyro
};

/// AzaharFrame kiugyro: bits 8 and 9 of vr_controls_preset_done mirror the game's own Gyro Aim
/// (its value + 1; 0: the game uses the global value). Builds before Gyro Aim keep that key but
/// drop vr_gyro_aim when they save the game's file, so going back to an older build and forward
/// again would lose the choice (and the preset, already marked, would not give it again); the
/// mirror brings it back. Old builds never read these bits.
inline constexpr uint32_t kGyroAimMirrorShift = 8;
inline constexpr uint32_t kGyroAimMirrorMask = 3u << kGyroAimMirrorShift;
/// Before a game's file is written: the mirror from the game's Gyro Aim (Settings::values).
void MirrorGyroAim();
/// After a game's file is read: a Gyro Aim the file lost (it has no vr_gyro_aim but the mirror
/// has a value) is given back as the game's own. Returns whether it was.
bool RestoreGyroAim();

/// AzaharFrame kiuguns: bits 10 and 11 mirror the game's own Weapon in Hand
/// (vr_light_gun_weapon) the same way: 0 the game uses the global value, 1 off, 2 on. Older
/// builds drop the key when they save the game's file; the mirror brings the choice back.
inline constexpr uint32_t kWeaponMirrorShift = 10;
inline constexpr uint32_t kWeaponMirrorMask = 3u << kWeaponMirrorShift;
/// Bits 12 and 13: Laser Sight (vr_light_gun_laser), the same way. Both are mirrored and
/// restored by these two.
inline constexpr uint32_t kLaserMirrorShift = 12;
inline constexpr uint32_t kLaserMirrorMask = 3u << kLaserMirrorShift;
/// AzaharFrame kiuflash: bits 28 and 29: Muzzle Flash (vr_light_gun_flash), the same way, by the
/// same two (14 to 27 are Angel's Aim's recoil strengths and weapon size, lane/kiufix).
inline constexpr uint32_t kFlashMirrorShift = 28;
inline constexpr uint32_t kFlashMirrorMask = 3u << kFlashMirrorShift;
void MirrorWeaponInHand();
bool RestoreWeaponInHand();

/// AzaharFrame kiufix: the same for Recoil's strengths and Weapon Size, four values per field
/// (0: the game uses the global value): bits 14 to 17 Charged Shot (its percent / 10 + 1, 0 to
/// 100 %), 18 to 21 Rapid Fire (the same), 22 and 23 Weapon Size (value + 1), 24 to 27 Weapon
/// Scale ((percent - 50) / 10 + 1, 50 to 150 %). A percent between the 10 % steps is mirrored
/// to the nearest step. Old builds never read these bits.
inline constexpr uint32_t kChargedMirrorShift = 14;
inline constexpr uint32_t kRapidMirrorShift = 18;
inline constexpr uint32_t kWeaponSizeMirrorShift = 22;
inline constexpr uint32_t kWeaponScaleMirrorShift = 24;
inline constexpr uint32_t kAimExtrasMirrorMask = 0x3FFFu << kChargedMirrorShift;
void MirrorAimExtras();
bool RestoreAimExtras();

/// Kid Icarus: Uprising aims with the stylus on the bottom screen while the left hand moves
/// and fires: the head aims the reticle (Headset) and either trigger touches.
/// AzaharFrame kiugyro: with Darkshade's C-Stick & Gyro mod installed, the reticle on the top
/// screen goes where the right controller points (Gyro Aim: Light Gun, through the mod's own
/// gyro; Motion Controls Right Controller), and the trigger fires. C-Stick at Edge (AzaharFrame
/// kiu) is no longer turned on: the headset check of 2026-11-14 found its turn poor; it stays a
/// switch of its own. Without the mod: Motion Controls Off, as before (with them on, the game's
/// reticle moved with the head or the controller). Right Stick Moves Touch stays off. The player
/// can change any of them, for this game or everywhere.
inline constexpr ControlsPreset kKidIcarusUprisingControls{.name = "Kid Icarus: Uprising",
                                                           .bottom_screen_reticle = 3,
                                                           .motion_controls = 0,
                                                           .edge_cstick = false,
                                                           .edge_cstick_needs_mod = true,
                                                           .motion_controls_with_mod = 1,
                                                           .gyro_aim_with_mod = 2};

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
/// AzaharFrame kiugyro: the same for Gyro Aim; Motion Controls takes `motion_controls_with_mod`
/// when the mod is installed at the time it is given.
uint32_t ApplyControlsPreset(const ControlsPreset& preset, bool cstick_mod_installed);

} // namespace FrameXr
