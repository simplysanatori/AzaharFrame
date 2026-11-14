// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiumenus: Angel's Aim's Send Report (the Mods tab's segment). The payload of a bug
// report or an idea sent to the developer on the player's tap: what the player typed, the app's
// version, the game and which Angel's Aim settings are on. Nothing personal: no user name, no
// folders (the typed text goes through the same scrubber as Report a Problem's), no log.

#pragma once

#include <string>

#include "core/frame_crash_report.h"
#include "core/frame_crash_send.h"

namespace Core::AngelsReport {

enum class Type { Bug, Idea };

/// "bug" or "idea" (the tag `type`).
const char* TypeKey(Type type);

/// What the report says of the app, the game and the settings.
struct Info {
    std::string app_version; ///< "1.1.1-524 ..." (Common::g_build_fullname)
    std::string release;     ///< "" ReleaseName()
    u64 title_id = 0;
    std::string title;        ///< the game's name
    std::string region;       ///< "Europe", "" unknown
    std::string game_version; ///< "1.0", "" unknown
    bool mod_installed = false; ///< a C-Stick & Gyro mod
    int aiming = 0;             ///< stored: 0 Off, 1 Gyro Aim, 2 Direct Aim
    bool weapon_in_hand = false;
    bool laser_sight = false;
    bool recoil = false;
    int recoil_charged = 0, recoil_rapid = 0; ///< percent
    int weapon_size = 0;                      ///< 0 Game Size, 1 Hand Size
    int weapon_scale = 100;                   ///< percent
    int weapon_opacity = 100, projectile_opacity = 100; ///< AzaharFrame kiuopacity: percent
    int flash_size = 100; ///< AzaharFrame kiuweap2: Muzzle Flash Size, percent
    int gyro_sensitivity = 0, turn_zone = 0, turn_speed = 0;
    bool immersive = false;
};

/// The event to send: title "Angel's Aim Bug: <first line>" or "Angel's Aim Idea: <first line>",
/// tags area=angels-aim and type=bug|idea (and mode, title_id, game_region), extras with the
/// version, the game and the settings; the text (scrubbed, at most 2000 characters) as comments.
CrashReport::Feedback BuildPayload(Type type, const std::string& text, const Info& info,
                                   const CrashReport::Scrubber& scrub);

} // namespace Core::AngelsReport
