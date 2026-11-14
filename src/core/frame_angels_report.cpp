// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiumenus: Angel's Aim's Send Report payload (frame_angels_report.h).

#include "core/frame_angels_report.h"

#include <fmt/format.h>

namespace Core::AngelsReport {

const char* TypeKey(Type type) {
    return type == Type::Idea ? "idea" : "bug";
}

namespace {
const char* AimingName(int aiming) {
    return aiming == 2 ? "Direct Aim" : aiming == 1 ? "Gyro Aim" : "Off";
}
const char* OnOff(bool on) {
    return on ? "On" : "Off";
}
std::string Clip(std::string s, std::size_t n) {
    if (s.size() > n) {
        std::size_t cut = n;
        while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) {
            cut--;
        }
        s.resize(cut);
    }
    return s;
}
} // namespace

CrashReport::Feedback BuildPayload(Type type, const std::string& text, const Info& info,
                                   const CrashReport::Scrubber& scrub) {
    CrashReport::Feedback f;
    f.release = info.release;
    f.comments = Clip(scrub.Apply(CrashReport::ScrubNetwork(text)), 2000);
    while (!f.comments.empty() &&
           (f.comments.back() == '\n' || f.comments.back() == ' ' || f.comments.back() == '\r')) {
        f.comments.pop_back();
    }
    const std::string line = CrashReport::ManualTitle(f.comments);
    const char* kind = type == Type::Idea ? "Idea" : "Bug";
    f.message = f.comments.empty() ? fmt::format("Angel's Aim {}", kind)
                                   : fmt::format("Angel's Aim {}: {}", kind, line);
    f.tags = {{"area", "angels-aim"}, {"type", TypeKey(type)}, {"mode", "vr"}};
    if (info.title_id != 0) {
        f.tags.emplace_back("title_id", fmt::format("{:016X}", info.title_id));
    }
    if (!info.region.empty()) {
        f.tags.emplace_back("game_region", info.region);
    }
    f.tags.emplace_back("cstick_gyro_mod", info.mod_installed ? "installed" : "not-installed");
    f.tags.emplace_back("aiming", AimingName(info.aiming));
    f.tags.emplace_back("immersive", OnOff(info.immersive));
    f.extra = {
        {"app_version", info.app_version},
        {"game", info.title},
        {"game_version", info.game_version.empty() ? "unknown" : info.game_version},
        {"settings",
         fmt::format("Aiming: {}\nWeapon in Hand: {}\nLaser Sight: {}\nRecoil: {}\n"
                     "Charged Shot: {} %\nRapid Fire: {} %\nWeapon Size: {}\nWeapon Scale: {} %\n"
                     "Gyro Sensitivity: {}\nTurn Zone: {}\nTurn Speed: {}\nImmersive Mode: {}",
                     AimingName(info.aiming), OnOff(info.weapon_in_hand), OnOff(info.laser_sight),
                     OnOff(info.recoil), info.recoil_charged, info.recoil_rapid,
                     info.weapon_size == 1 ? "Hand Size" : "Game Size", info.weapon_scale,
                     info.gyro_sensitivity, info.turn_zone, info.turn_speed,
                     OnOff(info.immersive))},
    };
    for (auto& [k, v] : f.extra) {
        v = scrub.Apply(v);
    }
    return f;
}

} // namespace Core::AngelsReport
