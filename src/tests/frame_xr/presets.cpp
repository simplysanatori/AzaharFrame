// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <set>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/presets.h"

// AzaharFrame
using namespace FrameXr;

TEST_CASE("FrameXr presets: the known games and their registers", "[frame_xr]") {
    // Every release of the game gets the same preset.
    for (const uint64_t id : {0x0004000000030800ull, 0x0004000000030700ull, 0x0004000000030600ull}) {
        const auto* preset = FindImmersivePreset(id);
        REQUIRE(preset != nullptr);
        REQUIRE(preset->mode == 3);
        REQUIRE(preset->register_offset == 90);
        REQUIRE(preset->wider_level == 3); // AzaharFrame presets: the owner's 1.3x
    }
    for (const uint64_t id : {0x0004000000054000ull, 0x0004000000053F00ull, 0x0004000000054100ull,
                              0x0004000000089D00ull, 0x0004000000089E00ull}) {
        const auto* preset = FindImmersivePreset(id);
        REQUIRE(preset != nullptr);
        REQUIRE(preset->mode == 3);
        REQUIRE(preset->register_offset == 90);
        REQUIRE(preset->wider_level == -1); // the owner plays it at the default 1.4x
        REQUIRE(preset->depth_multiplier == 1); // AzaharFrame presets: the owner's 10x
    }
    const auto* asphalt = FindImmersivePreset(0x0004000000034C00ull);
    REQUIRE(asphalt != nullptr);
    REQUIRE(asphalt->mode == 3);
    REQUIRE(asphalt->register_offset == 8);
    // AzaharFrame depth: unmeasured games keep 1x, the FOV setting and the eye search.
    REQUIRE(asphalt->depth_multiplier == 0);
    REQUIRE(asphalt->game_fov == 0);
    REQUIRE(asphalt->eye_indicator == nullptr);
}

// AzaharFrame depth: the measured games (IMMERSIVE.md, "Tuned games").
TEST_CASE("FrameXr presets: measured angles, eyes and depth", "[frame_xr]") {
    const auto* mk7 = FindImmersivePreset(0x0004000000030800ull);
    REQUIRE(mk7->depth_multiplier == 1);
    REQUIRE(mk7->game_fov == 40);
    REQUIRE(std::string(mk7->eye_indicator) == "87,2");
    const auto* sm3dl = FindImmersivePreset(0x0004000000054000ull);
    REQUIRE(sm3dl->game_fov == 45);
    REQUIRE(std::string(sm3dl->eye_indicator) == "87,2");
    // AzaharFrame presets: the owner's tuned Mario Kart 7 and Super Mario 3D Land.
    REQUIRE(mk7->narrow == 30);
    REQUIRE(mk7->narrow_y == 15);
    REQUIRE(mk7->screen_size == 2.022f);
    REQUIRE(mk7->resolution == 3);
    REQUIRE(mk7->texture_filter == 3);
    REQUIRE(sm3dl->factor_3d == 100);
    REQUIRE(sm3dl->resolution == 4);
    REQUIRE(sm3dl->screen_size == 0.0f);
    REQUIRE(sm3dl->narrow == -1);
    // Ocarina of Time 3D and Majora's Mask 3D: mode 2, its own register (no register preset).
    for (const uint64_t id : {0x0004000000033500ull, 0x0004000000033600ull, 0x0004000000033400ull,
                              0x0004000000125500ull, 0x0004000000125600ull,
                              0x00040000000D6E00ull}) {
        const auto* zelda = FindImmersivePreset(id);
        REQUIRE(zelda != nullptr);
        REQUIRE(zelda->mode == 2);
        REQUIRE(zelda->register_offset == -1);
        REQUIRE(zelda->eye_indicator == nullptr);
        REQUIRE(zelda->depth_multiplier == 1);
        REQUIRE(zelda->game_fov == 55);
    }
    // AzaharFrame immersive2: Majora's Mask 3D's measured angle swings in play: its 55 degrees
    // hold (Match Game Camera off); Ocarina of Time 3D keeps the default (on).
    REQUIRE(FindImmersivePreset(0x0004000000125600ull)->match_camera == 0);
    // AzaharFrame presets: Ocarina of Time 3D is exactly Majora's Mask 3D, in every region.
    REQUIRE(FindImmersivePreset(0x0004000000033500ull) ==
            FindImmersivePreset(0x0004000000125600ull));
    for (const uint64_t id : {0x0004000000033400ull, 0x0004000000033600ull, 0x000400000008F800ull,
                              0x000400000008F900ull, 0x0004000000125500ull,
                              0x00040000000D6E00ull}) {
        REQUIRE(FindImmersivePreset(id) == FindImmersivePreset(0x0004000000125600ull));
    }
    // Values in range.
    std::size_t count = 0;
    const auto* titles = ImmersivePresetTitles(count);
    for (std::size_t i = 0; i < count; ++i) {
        const ImmersivePreset& p = *titles[i].preset;
        REQUIRE((p.wider_level >= -1 && p.wider_level <= 4));
        REQUIRE((p.narrow >= -1 && p.narrow <= 30));
        REQUIRE((p.narrow_y >= -1 && p.narrow_y <= 30));
        REQUIRE((p.factor_3d >= -1 && p.factor_3d <= 100));
        REQUIRE((p.resolution >= -1 && p.resolution <= 18));
        REQUIRE((p.texture_filter >= -1 && p.texture_filter <= 5));
        REQUIRE((!p.own_placement || (p.place_distance >= 0.3f && p.place_distance <= 3.0f)));
        REQUIRE(p.game_fov <= 110); // the setting's range
        REQUIRE((p.screen_size == 0.0f || (p.screen_size >= 0.6f && p.screen_size <= 2.5f)));
        REQUIRE((p.match_camera >= -1 && p.match_camera <= 1));
    }
}

TEST_CASE("FrameXr presets: other games have none, ids are unique", "[frame_xr]") {
    REQUIRE(FindImmersivePreset(0) == nullptr);
    REQUIRE(FindImmersivePreset(0x0004000000068E00ull) == nullptr); // Pushmo
    std::size_t count = 0;
    const auto* titles = ImmersivePresetTitles(count);
    REQUIRE(count > 0);
    std::set<uint64_t> seen;
    for (std::size_t i = 0; i < count; ++i) {
        REQUIRE(seen.insert(titles[i].title_id).second);
        // AzaharFrame immersive: 0 only for a game that starts in the normal view.
        REQUIRE((titles[i].preset->mode == 0 || titles[i].preset->mode == 2 ||
                 titles[i].preset->mode == 3));
    }
}

// AzaharFrame immersive: Kid Icarus: Uprising starts in the normal view, in every region: its
// preset keeps Immersive Mode Off, so the player can still turn it on.
TEST_CASE("FrameXr presets: Kid Icarus: Uprising starts in the normal view", "[frame_xr]") {
    for (const uint64_t id : {0x0004000000030100ull, 0x0004000000030200ull, 0x0004000000030300ull}) {
        const auto* kiu = FindImmersivePreset(id);
        REQUIRE(kiu != nullptr);
        REQUIRE(kiu->mode == 0);
        REQUIRE(kiu->depth_multiplier == 0);
        REQUIRE(kiu->game_fov == 0);
        REQUIRE(kiu->wider_level == -1);
        REQUIRE(kiu->narrow == -1);
        REQUIRE(kiu->match_camera == -1);
    }
}

namespace {
// Back to the defaults, with nothing the game's own.
void ResetPresetSettings() {
    auto& v = Settings::values;
    v.vr_immersive_mode.SetGlobal(true);
    v.vr_immersive_mode.SetValue(0);
    v.vr_si_mode_register_offset.SetGlobal(true);
    v.vr_immersive_wider_view_level.SetGlobal(true);
    v.vr_immersive_match_camera.SetGlobal(true);
    v.vr_immersive_positional_game_scaler.SetGlobal(true);
    v.vr_immersive_game_fov.SetGlobal(true);
    v.vr_immersive_eye_indicator.SetGlobal(true);
    v.vr_immersive_squeeze.SetGlobal(true);
    v.vr_immersive_squeeze_y.SetGlobal(true);
    v.factor_3d.SetGlobal(true);
    v.resolution_factor.SetGlobal(true);
    v.texture_filter.SetGlobal(true);
    v.vr_immersive_wider_view_level.SetValue(v.vr_immersive_wider_view_level.GetDefault());
    v.vr_immersive_match_camera.SetValue(v.vr_immersive_match_camera.GetDefault());
    v.vr_immersive_positional_game_scaler.SetValue(0);
    v.vr_immersive_game_fov.SetValue(v.vr_immersive_game_fov.GetDefault());
    v.vr_immersive_eye_indicator.SetValue("");
    v.vr_immersive_squeeze.SetValue(v.vr_immersive_squeeze.GetDefault());
    v.vr_immersive_squeeze_y.SetValue(v.vr_immersive_squeeze_y.GetDefault());
    v.vr_si_mode_register_offset.SetValue(-1);
    v.factor_3d.SetValue(v.factor_3d.GetDefault());
    v.resolution_factor.SetValue(v.resolution_factor.GetDefault());
    v.texture_filter.SetValue(v.texture_filter.GetDefault());
}
} // namespace

// AzaharFrame presets: a player with no values of their own gets the preset: Ocarina of Time 3D
// and Majora's Mask 3D, every region, the same set.
TEST_CASE("FrameXr presets: a fresh user gets the preset of the game", "[frame_xr]") {
    for (const uint64_t id : {0x0004000000033500ull, 0x0004000000033600ull, 0x0004000000033400ull,
                              0x000400000008F800ull, 0x000400000008F900ull,
                              0x0004000000125500ull, 0x0004000000125600ull,
                              0x00040000000D6E00ull}) {
        ResetPresetSettings();
        auto& v = Settings::values;
        ApplyPresetSettings(*FindImmersivePreset(id));
        REQUIRE(v.vr_immersive_mode.GetValue() == 2);
        REQUIRE(!v.vr_immersive_mode.UsingGlobal());
        REQUIRE(v.vr_immersive_game_fov.GetValue() == 55);
        REQUIRE(v.vr_immersive_match_camera.GetValue() == false);
        REQUIRE(v.vr_immersive_positional_game_scaler.GetValue() == 1);
        REQUIRE(v.vr_immersive_squeeze.GetValue() == 1);
        REQUIRE(v.factor_3d.GetValue() == 30);
        REQUIRE(v.vr_si_mode_register_offset.GetValue() == -1);
    }
    ResetPresetSettings();
    ApplyPresetSettings(*FindImmersivePreset(0x0004000000030800ull)); // Mario Kart 7
    REQUIRE(Settings::values.vr_immersive_mode.GetValue() == 3);
    REQUIRE(Settings::values.vr_si_mode_register_offset.GetValue() == 90);
    REQUIRE(Settings::values.resolution_factor.GetValue() == 3);
    REQUIRE(Settings::values.vr_immersive_squeeze.GetValue() == 30);
    REQUIRE(Settings::values.vr_immersive_eye_indicator.GetValue() == "87,2");
    ResetPresetSettings();
}

// AzaharFrame presets: a value of the game's own, or a changed global one, is never replaced.
TEST_CASE("FrameXr presets: the player's own values win", "[frame_xr]") {
    auto& v = Settings::values;
    ResetPresetSettings();
    v.vr_immersive_mode.SetValue(0); // global Off, never touched: preset applies (above)
    // This game's own Immersive Mode (even Off), 3D Depth and squeeze.
    v.vr_immersive_mode.SetGlobal(false);
    v.vr_immersive_mode.SetValue(3);
    v.factor_3d.SetGlobal(false);
    v.factor_3d.SetValue(77);
    v.vr_immersive_squeeze.SetGlobal(false);
    v.vr_immersive_squeeze.SetValue(0);
    // A global value the player changed.
    v.resolution_factor.SetValue(7);
    ApplyPresetSettings(*FindImmersivePreset(0x0004000000033500ull));
    REQUIRE(v.vr_immersive_mode.GetValue() == 3);
    REQUIRE(v.factor_3d.GetValue() == 77);
    REQUIRE(v.vr_immersive_squeeze.GetValue() == 0);
    ApplyPresetSettings(*FindImmersivePreset(0x0004000000030800ull));
    REQUIRE(v.resolution_factor.GetValue() == 7);
    REQUIRE(v.resolution_factor.UsingGlobal());
    ResetPresetSettings();
}
