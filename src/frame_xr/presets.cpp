// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <iterator>

#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/presets.h"

namespace FrameXr {

namespace {

// CitraVR's v0.4 notes: "Mario Kart 7 or Super Mario 3D Land: Super Immersive Mode Profile 2
// (our mode 3), Register Offset 90. Asphalt: Profile 2, Register Offset 8."
//
// AzaharFrame depth: the rest was measured in mock runs of each game's play (IMMERSIVE.md,
// "Tuned games"). Game FOV is the camera's vertical angle from its projection matrix: the
// screen is sized with it (AzaharFrame immersive2: a game with its own FOV keeps one screen size,
// the gain follows the measured angle), and it is the value before the camera is seen. The eye
// indicator is the one CitraVR's search finds, set so the 3D is there from the first frame. The
// Depth Factor Multiplier makes the eye distance (6.5 cm x 3D Depth / 10 x 10^value, in the
// game's units) about the game's own at full 3D, at a 3D Depth of 90. Nintendo's games keep the
// projection in registers 86 to 89, the view matrix in 90 to 93 and the eye in 87,2.
//
// AzaharFrame immersive2: every game starts from the owner's Majora's Mask 3D settings (true
// scale, Wider View 1.4x, Narrow Screen 10 %, 3 m; immersive_layout.h); a preset changes only
// what its game needs.
//
// AzaharFrame presets: the values of Mario Kart 7, Super Mario 3D Land, Kid Icarus: Uprising,
// Ocarina of Time 3D and Majora's Mask 3D are the owner's own tuned per-game settings (headset
// sessions of 2026-11-11 and -12), shipped as the defaults a player without values of their own
// starts from; Ocarina of Time 3D uses exactly Majora's Mask 3D's.
//
// Mario Kart 7: 52 degrees racing (40 in the menus), the 3DS's eyes 3.4 units apart: 10x.
// AzaharFrame immersive2: the owner found it stretched at the view-filling size (gain 0.4 to
// 0.5: a perspective picture magnified 2 to 2.5x in the middle is drawn 1.4 to 1.6 times taller
// than wide 35 degrees to the side) and with Narrow Screen 20 % (20 % thinner still); 10 % was
// "okay-ish". So no narrowing, and the screen 10 % smaller instead: the HUD sits where 10 % put
// it across, and the world keeps its shape (gain 1.11 both ways).
// AzaharFrame jump: those notes are older than the values below.
// 1.2.0 fix lane B (session 651, triage item 16): the owner's own settings of 2026-11-15, from
// the game's file (config/custom/0004000000030800.ini) and its last placement (log: "-5.6
// degrees from the view's middle, 3.00 m, size 2.05"): Internal Resolution 4x, Texture Filter
// Bicubic, Detect Game FOV Always, Scale Mode Field of View with Horizontal Scale +55 % and
// Vertical Scale +70 %, Immersive Game FOV 40, Render Margin 1.3x (Stretch's; kept), the
// screen at 2.05 and -5.6 degrees, the placement's distance 3.00 m with World Scale 10 % (the
// screen 0.30 m away in the Full modes, close to the 0.37 m before), and the Full modes' 3D
// Depth 85. (Before: Stretch, -30 % and -15 %, size 2.022 at 0.371 m, Detect Game FOV off,
// Internal Resolution 3x, Texture Filter ScaleForce.)
constexpr ImmersivePreset kMarioKart7{.name = "Mario Kart 7",
                                      .mode = 3,
                                      .register_offset = 90,
                                      .depth_multiplier = 1,
                                      .game_fov = 40,
                                      .eye_indicator = "87,2",
                                      .wider_level = 3,
                                      .scale_x = 55,
                                      .screen_size = 2.05f,
                                      .match_camera = 2, // Always
                                      .resolution = 4,
                                      .texture_filter = 2, // Bicubic
                                      .scale_y = 70,
                                      .own_placement = true,
                                      .place_pitch_deg = -5.6f,
                                      .place_distance = 3.0f,
                                      .scale_mode = 1, // Field of View
                                      .world_scale = 10.0f,
                                      .immersive_depth = 85};
// Super Mario 3D Land: 45 degrees on the title and in the levels; the 3DS's eyes 58 units
// apart in a level (the title camera is about 1500 units away): 100x. At 1x its 3D could not
// be seen. Mock runs at 1.4x showed black below the floor of its levels; the owner plays it at
// 1.4x, so the default stays. AzaharFrame fovaxis: Render Margin applies with Stretch only, so
// in Field of View (the default) Horizontal and Vertical Scale +40 % draw that 1.4x.
constexpr ImmersivePreset kSuperMario3DLand{.name = "Super Mario 3D Land",
                                            .mode = 3,
                                            .register_offset = 90,
                                            .depth_multiplier = 1,
                                            .game_fov = 45,
                                            .eye_indicator = "87,2",
                                            .scale_x = 40,
                                            .match_camera = 2,
                                            .factor_3d = 100,
                                            .resolution = 4,
                                            .scale_y = 40,
                                            .own_placement = true,
                                            .place_pitch_deg = -7.15f,
                                            .place_distance = 3.0f};
// Ocarina of Time 3D and Majora's Mask 3D: CitraVR's mode 2 (made for them; the view matrix is
// register 4). The picture is one flat screen, so the multiplier only moves the camera when
// you lean: Link is about 56 units tall, so 10x at a 3D Depth of 30 is life size, 10x at 50
// (the default) a little more. About 55 degrees in play (35 to 70 in cutscenes).
// AzaharFrame presets: Ocarina of Time 3D has exactly Majora's Mask 3D's values (the owner's
// request). Its measured angle swings between 30 and 55 degrees in play, so Match Game Camera is
// off and the 55 degrees hold; 3D Depth 30, the screen 9.21 degrees below the view's middle, 3 m
// away. AzaharFrame kiu: no Horizontal Scale (it had Narrow Screen 10 %; the owner wants the
// default, 0, here too; a value a player saved stays). AzaharFrame fovaxis: Render Margin
// applies with Stretch only, so the 1.4x the default margin gave in Field of View is now
// Horizontal and Vertical Scale +40 % (the same picture and screen).
constexpr ImmersivePreset kZelda3D{.name = "Ocarina of Time 3D / Majora's Mask 3D",
                                   .mode = 2,
                                   .register_offset = -1,
                                   .depth_multiplier = 1,
                                   .game_fov = 55,
                                   .scale_x = 40,
                                   .match_camera = 0,
                                   .factor_3d = 30,
                                   .scale_y = 40,
                                   .own_placement = true,
                                   .place_pitch_deg = -9.21f,
                                   .place_distance = 3.0f};
// AzaharFrame fovaxis: +40 % both ways, the 1.4x the default Render Margin gave in Field of View.
constexpr ImmersivePreset kAsphalt3D{
    .name = "Asphalt 3D", .mode = 3, .register_offset = 8, .scale_x = 40, .scale_y = 40};
// Kid Icarus: Uprising aims with a reticle the game draws on the screen (2D) and shoots along
// its own camera; in the Full modes the world is drawn from a camera turned with the head, which
// the game does not know about, so its shots fly off where the unturned camera looked while the
// reticle stays on the screen: they come apart whenever the head turns. Wide (mode 1) keeps
// them together but adds little. So it starts in the normal view (mode 0: Immersive Mode stays
// Off); the player can still turn it on from the VR menu or its properties.
// 1.2.0 RC2 (the owner): the owner's own settings of 2026-11-15, from the game's file
// (config/custom/0004000000030200.ini), as starting values: 3D Depth 100, Internal Resolution
// 4x, Texture Filter 2, Async Shader Compilation on, Motion Clarity in the Off and Wide modes on
// with Match Bottom Screen Brightness, and Show Controllers on for the game (the weapon in hand
// needs them; a global Off the player chose stays). Screen Shape Automatic and the Full modes'
// Motion Clarity off are the defaults; World Scale is measured, not preset. The Controls values
// are the Controls preset's (controls_presets.h).
constexpr ImmersivePreset kKidIcarusUprising{.name = "Kid Icarus: Uprising",
                                             .mode = 0,
                                             .register_offset = -1,
                                             .factor_3d = 100,
                                             .resolution = 4,
                                             .texture_filter = 2,
                                             .async_shaders = 1,
                                             .motion_clarity_normal = 1,
                                             .motion_clarity_match_bottom = 1,
                                             .show_controllers = 1};

// Title ids: Azahar's compatibility list (every release of the game it knows) and, for Super
// Mario 3D Land, src/common/hacks/hack_list.cpp. Other regions of Asphalt 3D are not confirmed.
constexpr ImmersivePresetTitle kTitles[] = {
    // Mario Kart 7
    {0x0004000000030600, &kMarioKart7},
    {0x0004000000030700, &kMarioKart7},
    {0x0004000000030800, &kMarioKart7},
    {0x0004000000030A00, &kMarioKart7},
    {0x000400000008B400, &kMarioKart7},
    {0x000400000008B500, &kMarioKart7},
    // Super Mario 3D Land
    {0x0004000000053F00, &kSuperMario3DLand}, // EUR
    {0x0004000000054000, &kSuperMario3DLand}, // USA
    {0x0004000000054100, &kSuperMario3DLand}, // JPN
    {0x0004000000089D00, &kSuperMario3DLand}, // KOR
    {0x0004000000089E00, &kSuperMario3DLand}, // CHN
    // The Legend of Zelda: Ocarina of Time 3D
    {0x0004000000033400, &kZelda3D},
    {0x0004000000033500, &kZelda3D},
    {0x0004000000033600, &kZelda3D},
    {0x000400000008F800, &kZelda3D},
    {0x000400000008F900, &kZelda3D},
    // The Legend of Zelda: Majora's Mask 3D
    {0x0004000000125500, &kZelda3D},
    {0x0004000000125600, &kZelda3D},
    {0x00040000000D6E00, &kZelda3D},
    // Asphalt 3D
    {0x0004000000034C00, &kAsphalt3D},
    // Kid Icarus: Uprising: Japan (compatibility list), the USA (the owner's copy) and Europe
    // (not in the compatibility list). AzaharFrame kiu: and 0004000000030000, Japan as the
    // C-Stick & Gyro mod lists it.
    {0x0004000000030000, &kKidIcarusUprising},
    {0x0004000000030100, &kKidIcarusUprising},
    {0x0004000000030200, &kKidIcarusUprising},
    {0x0004000000030300, &kKidIcarusUprising},
};

} // namespace

void ApplyPresetSettings(const ImmersivePreset& preset) {
    auto& v = Settings::values;
    auto& mode = v.vr_immersive_mode;
    auto& offset = v.vr_si_mode_register_offset;
    // Only where this game's config has no value of its own and the global one is still the
    // default (Off, Auto); a value the player set, even Off, is left alone. A mode 0 preset (Kid
    // Icarus: Uprising) keeps the default: the game starts in the normal view, and nothing is set
    // for the mode, so the player can still turn Immersive Mode on.
    if (preset.mode != 0 && mode.UsingGlobal() && mode.GetValue() == 0) {
        mode.SetGlobal(false);
        mode.SetValue(preset.mode);
        LOG_INFO(Frontend, "xr: {}: Immersive Mode preset {}", preset.name, preset.mode);
    }
    // The immersive values apply when the game runs in the preset's mode (any mode for a game
    // that starts in the normal view).
    const bool fits = preset.mode == 0 || mode.GetValue() == preset.mode;
    // AzaharFrame presets: a value of the preset's own, where this game has none and the global
    // one is the default.
    const auto fill = [&](auto& setting, auto value, const char* what) {
        if (setting.UsingGlobal() && setting.GetValue() == setting.GetDefault()) {
            setting.SetGlobal(false);
            setting.SetValue(value);
            LOG_INFO(Frontend, "xr: {}: {} preset applied", preset.name, what);
            return true;
        }
        return false;
    };
    if (preset.register_offset != -1 && fits && offset.UsingGlobal() &&
        offset.GetValue() == -1) {
        offset.SetGlobal(false);
        offset.SetValue(preset.register_offset);
        LOG_INFO(Frontend, "xr: {}: View Matrix Register preset {}", preset.name,
                 preset.register_offset);
    }
    if (preset.wider_level >= 0 && fits) {
        fill(v.vr_immersive_wider_view_level, static_cast<u32>(preset.wider_level),
             "Render Margin"); // AzaharFrame kiu: was Wider View
    }
    if (preset.match_camera >= 0 && fits) {
        fill(v.vr_immersive_match_camera_mode, static_cast<u32>(preset.match_camera),
             "Detect Game FOV"); // AzaharFrame fovonce: 0 Off, 1 Once, 2 Always
    }
    // AzaharFrame depth: the game's world scale (the eye distance in mode 3). AzaharFrame
    // natural: 1.2.0's multiplier, which World Scale's migration turns into the same world.
    if (preset.depth_multiplier != 0 && fits) {
        fill(v.vr_immersive_positional_game_scaler, static_cast<u32>(preset.depth_multiplier),
             "World Scale (1.2.0 multiplier)");
    }
    if (preset.game_fov != 0 && fits) {
        fill(v.vr_immersive_game_fov, preset.game_fov, "Immersive Game FOV");
    }
    auto& eye = v.vr_immersive_eye_indicator;
    if (preset.eye_indicator != nullptr && fits && eye.UsingGlobal() && eye.GetValue().empty()) {
        eye.SetGlobal(false);
        eye.SetValue(preset.eye_indicator);
        LOG_INFO(Frontend, "xr: {}: Eye Indicator Register preset {}", preset.name,
                 preset.eye_indicator);
    }
    // AzaharFrame fovaxis: Scale Mode with the preset's scales only (before they are filled):
    // a game with scales of its own keeps whatever mode it has.
    const auto unset = [](const auto& setting) {
        return setting.UsingGlobal() && setting.GetValue() == setting.GetDefault();
    };
    if (preset.scale_mode >= 0 && fits && unset(v.vr_immersive_scale_x) &&
        unset(v.vr_immersive_scale_y)) {
        fill(v.vr_immersive_scale_mode, static_cast<u32>(preset.scale_mode), "Scale Mode");
    }
    if (preset.scale_x != 0 && fits) { // AzaharFrame kiu: was Narrow Screen
        fill(v.vr_immersive_scale_x, static_cast<s32>(preset.scale_x), "Horizontal Scale");
    }
    if (preset.scale_y != 0 && fits) { // and Narrow Height
        fill(v.vr_immersive_scale_y, static_cast<s32>(preset.scale_y), "Vertical Scale");
    }
    if (preset.hide_bottom >= 0) {
        fill(v.vr_hide_bottom_screen, preset.hide_bottom != 0, "Hide Bottom Screen");
    }
    if (preset.top_aspect >= 0) {
        fill(v.vr_top_aspect, static_cast<u32>(preset.top_aspect), "Top Screen Aspect");
    }
    // Graphics and 3D.
    if (preset.factor_3d >= 0) {
        fill(v.factor_3d, static_cast<u32>(preset.factor_3d), "3D Depth");
    }
    if (preset.resolution >= 0) {
        fill(v.resolution_factor, static_cast<u32>(preset.resolution), "Internal Resolution");
    }
    if (preset.texture_filter >= 0) {
        fill(v.texture_filter, static_cast<Settings::TextureFilter>(preset.texture_filter),
             "Texture Filter");
    }
    // 1.2.0 RC2 (the owner): Async Shader Compilation, Motion Clarity in the Off and Wide modes,
    // Match Bottom Screen Brightness and Show Controllers, filled the same way.
    if (preset.async_shaders >= 0) {
        fill(v.async_shader_compilation, preset.async_shaders != 0, "Async Shader Compilation");
    }
    if (preset.motion_clarity_normal >= 0) {
        fill(v.vr_motion_clarity_normal, preset.motion_clarity_normal != 0,
             "Motion Clarity (Off and Wide)");
    }
    if (preset.motion_clarity_match_bottom >= 0) {
        fill(v.vr_motion_clarity_match_bottom, preset.motion_clarity_match_bottom != 0,
             "Match Bottom Screen Brightness");
    }
    if (preset.show_controllers >= 0) {
        fill(v.vr_show_controllers, preset.show_controllers != 0, "Show Controllers");
    }
    // 1.2.0 fix lane B: World Scale and the Full modes' 3D Depth. World Scale filled marks the
    // game as having its own (vr_immersive_world_set, as the grip and the migration write it),
    // so the 1.2.0 migration does not replace it.
    if (preset.world_scale > 0.0f && fits &&
        fill(v.vr_immersive_world_scale, std::clamp(preset.world_scale, 10.0f, 400.0f),
             "World Scale")) {
        v.vr_immersive_world_set.SetGlobal(false);
        v.vr_immersive_world_set.SetValue(true);
    }
    if (preset.immersive_depth >= 0 && fits) {
        fill(v.vr_immersive_depth, static_cast<u32>(preset.immersive_depth),
             "3D Depth (Full modes)");
    }
}

const ImmersivePreset* FindImmersivePreset(uint64_t title_id) {
    for (const auto& entry : kTitles) {
        if (entry.title_id == title_id) {
            return entry.preset;
        }
    }
    return nullptr;
}

const ImmersivePresetTitle* ImmersivePresetTitles(std::size_t& count) {
    count = std::size(kTitles);
    return kTitles;
}

} // namespace FrameXr
