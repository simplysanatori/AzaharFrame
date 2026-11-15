// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fixa: the settings the in-game menu writes to a game's own file, in one list.
//
// The Qt side saves a per-game change by constructing the game's QtConfig, which reads that file
// back into the settings first; every value the menu just set is kept across that read and put
// back before the save (citra_qt.cpp FrameMenuRequest). The keeps were one variable each, and two
// of them (Primary Screen as Panel and Motion Clarity's In Immersive Modes) were made but never
// put back, so their per-game changes reverted a few milliseconds later. KeepMenuKeys keeps every
// setting of ForEachMenuKey and puts them back when it goes out of scope, so a key added here is
// kept, and one can't be kept without being put back. src/tests/frame_xr/per_game_keep.cpp
// checks every per-game row's key is in the list and survives a write and a read of the file.

#pragma once

#include <functional>
#include <vector>
#include "common/settings.h"

namespace FrameXr::PerGame {

/// Calls `f` with every setting the in-game menu may write to a game's own file: the per-game
/// rows' (Menu::PerGameSetting, per_game.cpp WithSetting; both 3D Depth keys and the Render
/// Margin keys) and the ones written beside them (World Scale's saved state, the measured game
/// FOV, the Controls preset's marker).
template <typename F>
void ForEachMenuKey(F&& f) {
    auto& v = Settings::values;
    // Immersive and the screens
    f(v.vr_hide_bottom_screen);
    f(v.vr_top_aspect);
    f(v.vr_immersive_mode);
    f(v.vr_immersive_positional_game_scaler);
    f(v.vr_immersive_world_scale); // AzaharFrame natural
    f(v.vr_immersive_world_set);
    f(v.vr_immersive_eye_per_far);
    f(v.vr_immersive_depth);
    f(v.vr_immersive_follow_screen);
    f(v.vr_immersive_game_fov);
    f(v.vr_immersive_match_camera_mode); // AzaharFrame depth, fovonce
    f(v.vr_immersive_fov_measured);
    f(v.vr_immersive_wider_view_level);
    f(v.vr_immersive_render_margin_x); // AzaharFrame fovaxis
    f(v.vr_immersive_render_margin_y);
    f(v.vr_immersive_scale_x); // AzaharFrame kiu
    f(v.vr_immersive_scale_y);
    f(v.vr_immersive_scale_mode); // AzaharFrame fovscale
    f(v.vr_motion_clarity);       // AzaharFrame flash
    f(v.vr_motion_clarity_immersive); // AzaharFrame projdef
    f(v.vr_screens_in_projection);
    // Graphics, General
    f(v.factor_3d); // AzaharFrame depth3d
    f(v.custom_textures); // AzaharFrame texui
    f(v.preload_textures);
    f(v.resolution_factor); // AzaharFrame menuux
    f(v.texture_filter);
    f(v.async_shader_compilation);
    f(v.image_copy_accuracy); // AzaharFrame accxfer
    f(v.fast_forward_audio);  // AzaharFrame ffaudio
    f(v.autosave_enabled);    // AzaharFrame autosave
    f(v.autosave_every);
    f(v.autosave_keep);
    // Controls (AzaharFrame reticle)
    f(v.vr_touchpad_touch);
    f(v.vr_bottom_screen_reticle);
    f(v.vr_reticle_at_edge);
    f(v.vr_reticle_speed);
    f(v.vr_reticle_center_until_pressed); // AzaharFrame fixes7
    f(v.vr_reticle_stick_moves);
    f(v.vr_reticle_edge_cstick); // AzaharFrame kiu
    f(v.vr_motion_controls);
    f(v.vr_gyro_aim); // AzaharFrame kiugyro
    f(v.vr_gyro_sensitivity);
    f(v.vr_light_gun_turn_zone);
    f(v.vr_light_gun_turn_speed);
    f(v.vr_light_gun_recoil);
    f(v.vr_light_gun_weapon); // AzaharFrame kiuguns
    f(v.vr_light_gun_laser);
    f(v.vr_light_gun_flash); // AzaharFrame kiuflash
    f(v.vr_light_gun_projectile);
    f(v.vr_light_gun_projectile_size);
    f(v.vr_light_gun_projectile_speed);
    f(v.vr_light_gun_recoil_charged); // AzaharFrame kiufix
    f(v.vr_light_gun_recoil_rapid);
    f(v.vr_light_gun_weapon_size);
    f(v.vr_light_gun_weapon_scale);
    f(v.vr_light_gun_turn_zone_from); // AzaharFrame kiuaim3
    f(v.vr_light_gun_lock);
    f(v.vr_light_gun_grip);
    f(v.vr_light_gun_weapon_opacity); // AzaharFrame kiuopacity
    f(v.vr_light_gun_projectile_opacity);
    f(v.vr_light_gun_muzzle_flash_size); // AzaharFrame kiuweap2
    f(v.vr_controls_preset_done);
}

/// Every setting of ForEachMenuKey as it is now (whether it follows the global value, and the
/// game's own value), put back by Restore or, at the latest, when this goes out of scope.
class KeepMenuKeys {
public:
    KeepMenuKeys() {
        ForEachMenuKey([this](auto& s) { Keep(s); });
    }
    ~KeepMenuKeys() {
        Restore();
    }
    KeepMenuKeys(const KeepMenuKeys&) = delete;
    KeepMenuKeys& operator=(const KeepMenuKeys&) = delete;

    /// Puts every kept setting back (once; later calls do nothing).
    void Restore() {
        for (const auto& restore : restores) {
            restore();
        }
        restores.clear();
    }

private:
    template <typename T, bool R>
    void Keep(Settings::SwitchableSetting<T, R>& s) {
        restores.push_back([&s, global = s.UsingGlobal(), value = s.GetValue()] {
            s.SetGlobal(global);
            if (!global) {
                s.SetValue(value);
            }
        });
    }
    std::vector<std::function<void()>> restores;
};

} // namespace FrameXr::PerGame
