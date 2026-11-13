// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

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
// AzaharFrame jump: those notes are older than the values below, which are the owner's own of
// 2026-11-12 (set in the in-game menu): Immersive Game FOV 40, Match Game Camera off, Narrow
// Screen 30 %, Narrow Height 15 %, size 2.02 at 0.37 m. Match Game Camera is set off here too,
// so a player on the defaults gets the gain the owner tuned (with it on, the gain would follow
// the 52-degree racing camera instead).
constexpr ImmersivePreset kMarioKart7{.name = "Mario Kart 7",
                                      .mode = 3,
                                      .register_offset = 90,
                                      .depth_multiplier = 1,
                                      .game_fov = 40,
                                      .eye_indicator = "87,2",
                                      .wider_level = 3,
                                      .narrow = 30,
                                      .screen_size = 2.022f,
                                      .match_camera = 0,
                                      .resolution = 3,
                                      .texture_filter = 3,
                                      .narrow_y = 15,
                                      .own_placement = true,
                                      .place_pitch_deg = -4.95f,
                                      .place_distance = 0.371f};
// Super Mario 3D Land: 45 degrees on the title and in the levels; the 3DS's eyes 58 units
// apart in a level (the title camera is about 1500 units away): 100x. At 1x its 3D could not
// be seen. Mock runs at 1.4x showed black below the floor of its levels; the owner plays it at
// 1.4x, so the default stays.
constexpr ImmersivePreset kSuperMario3DLand{.name = "Super Mario 3D Land",
                                            .mode = 3,
                                            .register_offset = 90,
                                            .depth_multiplier = 1,
                                            .game_fov = 45,
                                            .eye_indicator = "87,2",
                                            .match_camera = 2,
                                            .factor_3d = 100,
                                            .resolution = 4,
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
// away. AzaharFrame oot: no Narrow Screen of its own (it was the 10 % step), so the default (Off)
// applies and the world keeps its shape.
constexpr ImmersivePreset kZelda3D{.name = "Ocarina of Time 3D / Majora's Mask 3D",
                                   .mode = 2,
                                   .register_offset = -1,
                                   .depth_multiplier = 1,
                                   .game_fov = 55,
                                   .match_camera = 0,
                                   .factor_3d = 30,
                                   .own_placement = true,
                                   .place_pitch_deg = -9.21f,
                                   .place_distance = 3.0f};
constexpr ImmersivePreset kAsphalt3D{.name = "Asphalt 3D", .mode = 3, .register_offset = 8};
// Kid Icarus: Uprising aims with a reticle the game draws on the screen (2D) and shoots along
// its own camera; in the Full modes the world is drawn from a camera turned with the head, which
// the game does not know about, so its shots fly off where the unturned camera looked while the
// reticle stays on the screen: they come apart whenever the head turns. Wide (mode 1) keeps
// them together but adds little. So it starts in the normal view (mode 0: Immersive Mode stays
// Off); the player can still turn it on from the VR menu or its properties.
constexpr ImmersivePreset kKidIcarusUprising{.name = "Kid Icarus: Uprising",
                                             .mode = 0,
                                             .register_offset = -1,
                                             .resolution = 4,
                                             .texture_filter = 2};

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
    // (not in the compatibility list)
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
        }
    };
    if (preset.register_offset != -1 && fits && offset.UsingGlobal() &&
        offset.GetValue() == -1) {
        offset.SetGlobal(false);
        offset.SetValue(preset.register_offset);
        LOG_INFO(Frontend, "xr: {}: View Matrix Register preset {}", preset.name,
                 preset.register_offset);
    }
    if (preset.wider_level >= 0 && fits) {
        fill(v.vr_immersive_wider_view_level, static_cast<u32>(preset.wider_level), "Wider View");
    }
    if (preset.match_camera >= 0 && fits) {
        fill(v.vr_immersive_match_camera_mode, static_cast<u32>(preset.match_camera),
             "Match Game Camera"); // AzaharFrame fovonce: 0 Off, 1 Set Once, 2 Always
    }
    // AzaharFrame depth: the game's world scale (the eye distance in mode 3).
    if (preset.depth_multiplier != 0 && fits) {
        fill(v.vr_immersive_positional_game_scaler, static_cast<u32>(preset.depth_multiplier),
             "Depth Factor Multiplier");
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
    if (preset.narrow >= 0 && fits) {
        fill(v.vr_immersive_squeeze, static_cast<u32>(preset.narrow), "Narrow Screen");
    }
    if (preset.narrow_y >= 0 && fits) {
        fill(v.vr_immersive_squeeze_y, static_cast<u32>(preset.narrow_y), "Narrow Height");
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
