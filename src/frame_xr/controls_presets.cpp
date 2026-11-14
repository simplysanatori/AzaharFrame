// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include <string>
#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/controls_presets.h"

// AzaharFrame kiu: Controls presets as starting values only (controls_presets.h).
namespace FrameXr {

namespace {
std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool NamesCStick(const std::string& text) {
    const std::string t = Lower(text);
    return t.find("cstick") != std::string::npos || t.find("c-stick") != std::string::npos ||
           t.find("c_stick") != std::string::npos;
}
} // namespace

bool CStickModInstalled(const std::vector<Mods::ModInfo>& mods) {
    for (const Mods::ModInfo& m : mods) {
        if (m.state != Mods::ModState::Active) {
            continue; // in the library only
        }
        for (const char* id : kCStickModIds) {
            if (m.id == id) {
                return true;
            }
        }
        if (m.managed && m.id.rfind(Mods::kZipPrefix, 0) != 0) {
            continue; // another catalogue mod: its id decides
        }
        if (NamesCStick(m.id) || NamesCStick(m.name)) {
            return true;
        }
        for (const std::string& f : m.files) {
            if (f == "code.bin" || f == "exefs/code.bin" || NamesCStick(f)) {
                return true;
            }
        }
    }
    return false;
}

bool CStickModInstalled(uint64_t title_id) {
    return CStickModInstalled(Mods::Default().ListLoaded(title_id));
}

void MirrorGyroAim() {
    auto& v = Settings::values;
    auto& done = v.vr_controls_preset_done;
    const uint32_t before = done.GetValue();
    uint32_t now = before & ~kGyroAimMirrorMask;
    if (!v.vr_gyro_aim.UsingGlobal()) {
        now |= ((std::min<uint32_t>(v.vr_gyro_aim.GetValue(), 2) + 1) << kGyroAimMirrorShift) &
               kGyroAimMirrorMask;
    }
    if (now != before) {
        done.SetGlobal(false);
        done.SetValue(now);
    }
}

bool RestoreGyroAim() {
    auto& v = Settings::values;
    const uint32_t mirror =
        (v.vr_controls_preset_done.GetValue() & kGyroAimMirrorMask) >> kGyroAimMirrorShift;
    if (mirror == 0 || !v.vr_gyro_aim.UsingGlobal()) {
        return false;
    }
    v.vr_gyro_aim.SetGlobal(false);
    v.vr_gyro_aim.SetValue(mirror - 1);
    LOG_INFO(Frontend, "xr: Gyro Aim {} for this game, from vr_controls_preset_done (its key was "
                       "dropped by an older build)",
             mirror - 1);
    return true;
}

namespace {
// AzaharFrame kiuguns: a game's own switch in two bits of vr_controls_preset_done (0: global).
void MirrorSwitch(Settings::SwitchableSetting<bool>& setting, uint32_t shift) {
    auto& done = Settings::values.vr_controls_preset_done;
    const uint32_t before = done.GetValue();
    uint32_t now = before & ~(3u << shift);
    if (!setting.UsingGlobal()) {
        now |= (setting.GetValue() ? 2u : 1u) << shift;
    }
    if (now != before) {
        done.SetGlobal(false);
        done.SetValue(now);
    }
}

bool RestoreSwitch(Settings::SwitchableSetting<bool>& setting, uint32_t shift, const char* name) {
    const uint32_t mirror = (Settings::values.vr_controls_preset_done.GetValue() >> shift) & 3u;
    if (mirror == 0 || mirror > 2 || !setting.UsingGlobal()) {
        return false;
    }
    setting.SetGlobal(false);
    setting.SetValue(mirror == 2);
    LOG_INFO(Frontend, "xr: {} {} for this game, from vr_controls_preset_done (its key was "
                       "dropped by an older build)",
             name, mirror == 2 ? "on" : "off");
    return true;
}
} // namespace

void MirrorWeaponInHand() { // AzaharFrame kiuguns
    MirrorSwitch(Settings::values.vr_light_gun_weapon, kWeaponMirrorShift);
    MirrorSwitch(Settings::values.vr_light_gun_laser, kLaserMirrorShift);
}

bool RestoreWeaponInHand() { // AzaharFrame kiuguns
    const bool weapon =
        RestoreSwitch(Settings::values.vr_light_gun_weapon, kWeaponMirrorShift, "Weapon in Hand");
    const bool laser =
        RestoreSwitch(Settings::values.vr_light_gun_laser, kLaserMirrorShift, "Laser Sight");
    return weapon || laser;
}

namespace {
// AzaharFrame kiufix: a game's own percent (`lo` to `lo + 100`, steps of 10) in four bits.
void MirrorPercent(Settings::SwitchableSetting<u32, true>& setting, uint32_t shift, uint32_t lo,
                   uint32_t& now) {
    now &= ~(0xFu << shift);
    if (!setting.UsingGlobal()) {
        const uint32_t v = std::clamp<uint32_t>(setting.GetValue(), lo, lo + 100);
        now |= (((v - lo + 5) / 10) + 1) << shift;
    }
}
bool RestorePercent(Settings::SwitchableSetting<u32, true>& setting, uint32_t shift, uint32_t lo,
                    const char* name) {
    const uint32_t mirror = (Settings::values.vr_controls_preset_done.GetValue() >> shift) & 0xFu;
    if (mirror == 0 || mirror > 11 || !setting.UsingGlobal()) {
        return false;
    }
    setting.SetGlobal(false);
    setting.SetValue(lo + (mirror - 1) * 10);
    LOG_INFO(Frontend, "xr: {} {} % for this game, from vr_controls_preset_done (its key was "
                       "dropped by an older build)",
             name, lo + (mirror - 1) * 10);
    return true;
}
} // namespace

void MirrorAimExtras() { // AzaharFrame kiufix
    auto& v = Settings::values;
    auto& done = v.vr_controls_preset_done;
    const uint32_t before = done.GetValue();
    uint32_t now = before;
    MirrorPercent(v.vr_light_gun_recoil_charged, kChargedMirrorShift, 0, now);
    MirrorPercent(v.vr_light_gun_recoil_rapid, kRapidMirrorShift, 0, now);
    now &= ~(3u << kWeaponSizeMirrorShift);
    if (!v.vr_light_gun_weapon_size.UsingGlobal()) {
        now |= (std::min<uint32_t>(v.vr_light_gun_weapon_size.GetValue(), 1) + 1)
               << kWeaponSizeMirrorShift;
    }
    MirrorPercent(v.vr_light_gun_weapon_scale, kWeaponScaleMirrorShift, 50, now);
    if (now != before) {
        done.SetGlobal(false);
        done.SetValue(now);
    }
}

bool RestoreAimExtras() { // AzaharFrame kiufix
    auto& v = Settings::values;
    bool any = RestorePercent(v.vr_light_gun_recoil_charged, kChargedMirrorShift, 0, "Charged Shot");
    any |= RestorePercent(v.vr_light_gun_recoil_rapid, kRapidMirrorShift, 0, "Rapid Fire");
    const uint32_t size =
        (v.vr_controls_preset_done.GetValue() >> kWeaponSizeMirrorShift) & 3u;
    if (size != 0 && size <= 2 && v.vr_light_gun_weapon_size.UsingGlobal()) {
        v.vr_light_gun_weapon_size.SetGlobal(false);
        v.vr_light_gun_weapon_size.SetValue(size - 1);
        LOG_INFO(Frontend, "xr: Weapon Size {} for this game, from vr_controls_preset_done", size - 1);
        any = true;
    }
    any |= RestorePercent(v.vr_light_gun_weapon_scale, kWeaponScaleMirrorShift, 50, "Weapon Scale");
    return any;
}

uint32_t ApplyControlsPreset(const ControlsPreset& preset, bool cstick_mod_installed) {
    auto& v = Settings::values;
    auto& done_setting = v.vr_controls_preset_done;
    const uint32_t done_before = done_setting.GetValue();
    uint32_t done = done_before;
    uint32_t given = 0;
    const auto offer = [&](auto& setting, auto value, uint32_t bit, const char* what) {
        if ((done & bit) != 0) {
            return; // decided before: the player's choice since then stays
        }
        if (!setting.UsingGlobal()) {
            done |= bit; // the game's own value (the player's, or given before) stays
            return;
        }
        if (setting.GetValue() != setting.GetDefault()) {
            return; // a global value the player chose
        }
        setting.SetGlobal(false);
        setting.SetValue(value);
        done |= bit;
        given |= bit;
        LOG_INFO(Frontend, "xr: {}: {} preset {}", preset.name, what, value);
    };
    if (preset.bottom_screen_reticle != 0) {
        offer(v.vr_bottom_screen_reticle, preset.bottom_screen_reticle, kPresetReticle,
              "Bottom Screen Reticle");
    }
    const int motion = cstick_mod_installed && preset.motion_controls_with_mod >= 0
                           ? preset.motion_controls_with_mod
                           : preset.motion_controls;
    if (motion >= 0) {
        offer(v.vr_motion_controls, static_cast<u32>(motion), kPresetMotionControls,
              "Motion Controls");
    }
    if (preset.edge_cstick && (cstick_mod_installed || !preset.edge_cstick_needs_mod)) {
        offer(v.vr_reticle_edge_cstick, true, kPresetEdgeCStick, "C-Stick at Edge");
    }
    if (preset.gyro_aim_with_mod != 0 && cstick_mod_installed) { // AzaharFrame kiugyro
        offer(v.vr_gyro_aim, preset.gyro_aim_with_mod, kPresetGyroAim, "Gyro Aim");
    }
    if (done != done_before) {
        done_setting.SetGlobal(false);
        done_setting.SetValue(done);
    }
    return given;
}

} // namespace FrameXr
