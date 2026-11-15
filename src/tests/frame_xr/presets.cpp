// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <set>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "common/settings.h"
#include "frame_xr/controls_presets.h" // AzaharFrame kiu
#include "frame_xr/input/gyro_aim.h"   // AzaharFrame kiugyro
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
    REQUIRE(asphalt->scale_x == 40); // AzaharFrame fovaxis: as Super Mario 3D Land
    REQUIRE(asphalt->scale_y == 40);
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
    // 1.2.0 fix lane B (triage item 16): Mario Kart 7 as the owner set it in session 651 (the
    // game's file and its last placement).
    REQUIRE(mk7->scale_x == 55);
    REQUIRE(mk7->scale_y == 70);
    REQUIRE(mk7->scale_mode == 1); // Field of View
    REQUIRE(mk7->wider_level == 3);
    REQUIRE(mk7->screen_size == 2.05f);
    REQUIRE(mk7->resolution == 4);
    REQUIRE(mk7->texture_filter == static_cast<int>(Settings::TextureFilter::Bicubic));
    REQUIRE(mk7->match_camera == 2); // Detect Game FOV Always
    REQUIRE(mk7->own_placement);
    REQUIRE(mk7->place_pitch_deg == -5.6f);
    REQUIRE(mk7->place_distance == 3.0f);
    REQUIRE(mk7->world_scale == 10.0f);
    REQUIRE(mk7->immersive_depth == 85);
    REQUIRE(sm3dl->world_scale == 0.0f); // the others keep World Scale and 3D Depth
    REQUIRE(sm3dl->immersive_depth == -1);
    REQUIRE(sm3dl->factor_3d == 100);
    REQUIRE(sm3dl->resolution == 4);
    REQUIRE(sm3dl->screen_size == 0.0f);
    // AzaharFrame fovaxis: Render Margin applies with Stretch only; +40 % both ways is the 1.4x
    // the default margin gave in Field of View.
    REQUIRE(sm3dl->scale_x == 40);
    REQUIRE(sm3dl->scale_y == 40);
    REQUIRE(sm3dl->scale_mode == -1);
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
        REQUIRE(zelda->scale_x == 40); // AzaharFrame fovaxis: as Super Mario 3D Land
        REQUIRE(zelda->scale_y == 40);
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
        // AzaharFrame kiu, fov100: the scales' range is -100 to +100 %
        REQUIRE((p.scale_x >= -100 && p.scale_x <= 100 && p.scale_x % 5 == 0));
        REQUIRE((p.scale_y >= -100 && p.scale_y <= 100 && p.scale_y % 5 == 0));
        REQUIRE((p.factor_3d >= -1 && p.factor_3d <= 100));
        REQUIRE((p.resolution >= -1 && p.resolution <= 18));
        REQUIRE((p.texture_filter >= -1 && p.texture_filter <= 5));
        REQUIRE((!p.own_placement || (p.place_distance >= 0.3f && p.place_distance <= 3.0f)));
        REQUIRE(p.game_fov <= 110); // the setting's range
        REQUIRE((p.screen_size == 0.0f || (p.screen_size >= 0.6f && p.screen_size <= 2.5f)));
        REQUIRE((p.match_camera >= -1 && p.match_camera <= 2)); // AzaharFrame fovonce
        // 1.2.0 fix lane B: World Scale's range (0 = keep), the Full modes' 3D Depth.
        REQUIRE((p.world_scale == 0.0f || (p.world_scale >= 10.0f && p.world_scale <= 400.0f)));
        REQUIRE((p.immersive_depth >= -1 && p.immersive_depth <= 100));
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
    // AzaharFrame kiu: 0004000000030000 too (Japan, as the C-Stick & Gyro mod lists it).
    for (const uint64_t id : {0x0004000000030000ull, 0x0004000000030100ull, 0x0004000000030200ull,
                              0x0004000000030300ull}) {
        const auto* kiu = FindImmersivePreset(id);
        REQUIRE(kiu != nullptr);
        REQUIRE(kiu->mode == 0);
        REQUIRE(kiu->depth_multiplier == 0);
        REQUIRE(kiu->game_fov == 0);
        REQUIRE(kiu->wider_level == -1);
        REQUIRE(kiu->scale_x == 0);
        REQUIRE(kiu->match_camera == -1);
        // Graphics only; Immersive stays the player's to turn on.
        REQUIRE(kiu->resolution == 4);
        REQUIRE(kiu->texture_filter == 2);
        REQUIRE(kiu->register_offset == -1);
        REQUIRE(kiu->eye_indicator == nullptr);
        REQUIRE(!kiu->own_placement);
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
    v.vr_immersive_match_camera_mode.SetGlobal(true);
    v.vr_immersive_positional_game_scaler.SetGlobal(true);
    v.vr_immersive_game_fov.SetGlobal(true);
    v.vr_immersive_eye_indicator.SetGlobal(true);
    v.vr_immersive_scale_x.SetGlobal(true); // AzaharFrame kiu
    v.vr_immersive_scale_y.SetGlobal(true);
    v.factor_3d.SetGlobal(true);
    v.resolution_factor.SetGlobal(true);
    v.texture_filter.SetGlobal(true);
    // 1.2.0 fix lane B: Scale Mode, World Scale (and its "own" mark), the Full modes' 3D Depth.
    v.vr_immersive_scale_mode.SetGlobal(true);
    v.vr_immersive_scale_mode.SetValue(v.vr_immersive_scale_mode.GetDefault());
    v.vr_immersive_world_scale.SetGlobal(true);
    v.vr_immersive_world_scale.SetValue(v.vr_immersive_world_scale.GetDefault());
    v.vr_immersive_world_set.SetGlobal(true);
    v.vr_immersive_world_set.SetValue(false);
    v.vr_immersive_depth.SetGlobal(true);
    v.vr_immersive_depth.SetValue(v.vr_immersive_depth.GetDefault());
    v.vr_immersive_wider_view_level.SetValue(v.vr_immersive_wider_view_level.GetDefault());
    v.vr_immersive_match_camera_mode.SetValue(v.vr_immersive_match_camera_mode.GetDefault());
    v.vr_immersive_positional_game_scaler.SetValue(0);
    v.vr_immersive_game_fov.SetValue(v.vr_immersive_game_fov.GetDefault());
    v.vr_immersive_eye_indicator.SetValue("");
    v.vr_immersive_scale_x.SetValue(v.vr_immersive_scale_x.GetDefault());
    v.vr_immersive_scale_y.SetValue(v.vr_immersive_scale_y.GetDefault());
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
        REQUIRE(v.vr_immersive_match_camera_mode.GetValue() == 0); // Off
        REQUIRE(v.vr_immersive_positional_game_scaler.GetValue() == 1);
        // AzaharFrame kiu: no Horizontal Scale any more (was Narrow Screen 10 %). AzaharFrame
        // fovaxis: +40 % both ways, the 1.4x the default Render Margin gave in Field of View.
        REQUIRE(v.vr_immersive_scale_x.GetValue() == 40);
        REQUIRE(!v.vr_immersive_scale_x.UsingGlobal());
        REQUIRE(v.vr_immersive_scale_y.GetValue() == 40);
        REQUIRE(v.factor_3d.GetValue() == 30);
        REQUIRE(v.vr_si_mode_register_offset.GetValue() == -1);
    }
    ResetPresetSettings();
    ApplyPresetSettings(*FindImmersivePreset(0x0004000000030800ull)); // Mario Kart 7
    REQUIRE(Settings::values.vr_immersive_mode.GetValue() == 3);
    REQUIRE(Settings::values.vr_si_mode_register_offset.GetValue() == 90);
    REQUIRE(Settings::values.vr_immersive_eye_indicator.GetValue() == "87,2");
    // 1.2.0 fix lane B (triage item 16): the owner's session-651 values.
    REQUIRE(Settings::values.resolution_factor.GetValue() == 4);
    REQUIRE(Settings::values.texture_filter.GetValue() == Settings::TextureFilter::Bicubic);
    REQUIRE(Settings::values.vr_immersive_scale_x.GetValue() == 55);
    REQUIRE(Settings::values.vr_immersive_scale_y.GetValue() == 70);
    REQUIRE(Settings::values.vr_immersive_scale_mode.GetValue() == 1); // Field of View
    REQUIRE(Settings::values.vr_immersive_match_camera_mode.GetValue() == 2); // Always
    REQUIRE(Settings::values.vr_immersive_world_scale.GetValue() == 10.0f);
    REQUIRE(!Settings::values.vr_immersive_world_scale.UsingGlobal());
    REQUIRE(Settings::values.vr_immersive_world_set.GetValue());
    REQUIRE(!Settings::values.vr_immersive_world_set.UsingGlobal());
    REQUIRE(Settings::values.vr_immersive_depth.GetValue() == 85u);
    REQUIRE(!Settings::values.vr_immersive_depth.UsingGlobal());
    // The globals untouched.
    REQUIRE(Settings::values.vr_immersive_world_scale.GetValue(true) == 100.0f);
    REQUIRE(Settings::values.vr_immersive_depth.GetValue(true) == 100u);
    // AzaharFrame fovonce: Super Mario 3D Land keeps its "on": Always.
    ResetPresetSettings();
    ApplyPresetSettings(*FindImmersivePreset(0x0004000000054100ull));
    REQUIRE(Settings::values.vr_immersive_match_camera_mode.GetValue() == 2);
    ResetPresetSettings();
}

// AzaharFrame presets: a value of the game's own, or a changed global one, is never replaced.
TEST_CASE("FrameXr presets: the player's own values win", "[frame_xr]") {
    auto& v = Settings::values;
    ResetPresetSettings();
    v.vr_immersive_mode.SetValue(0); // global Off, never touched: preset applies (above)
    // This game's own Immersive Mode (even Off), 3D Depth and Horizontal Scale (even 0).
    v.vr_immersive_mode.SetGlobal(false);
    v.vr_immersive_mode.SetValue(3);
    v.factor_3d.SetGlobal(false);
    v.factor_3d.SetValue(77);
    v.vr_immersive_scale_x.SetGlobal(false);
    v.vr_immersive_scale_x.SetValue(0);
    // A global value the player changed.
    v.resolution_factor.SetValue(7);
    ApplyPresetSettings(*FindImmersivePreset(0x0004000000033500ull));
    REQUIRE(v.vr_immersive_mode.GetValue() == 3);
    REQUIRE(v.factor_3d.GetValue() == 77);
    REQUIRE(v.vr_immersive_scale_x.GetValue() == 0);
    ApplyPresetSettings(*FindImmersivePreset(0x0004000000030800ull)); // MK7: scale -30
    REQUIRE(v.vr_immersive_scale_x.GetValue() == 0); // still the game's own 0
    REQUIRE(v.resolution_factor.GetValue() == 7);
    REQUIRE(v.resolution_factor.UsingGlobal());
    ResetPresetSettings();
}

// AzaharFrame kiu: Kid Icarus: Uprising's graphics preset in Japan's 0004000000030000 too, only
// as starting values: a fresh game gets them, the game's own value and a changed global one
// stay, and Immersive Mode is never set.
TEST_CASE("FrameXr presets: Kid Icarus: Uprising (Japan, 30000) gives starting values only",
          "[frame_xr][kiu]") {
    auto& v = Settings::values;
    const ImmersivePreset& kiu = *FindImmersivePreset(0x0004000000030000ull);
    ResetPresetSettings();
    ApplyPresetSettings(kiu);
    CHECK(v.resolution_factor.GetValue() == 4);
    CHECK(!v.resolution_factor.UsingGlobal()); // the game's, the global value untouched
    CHECK(v.resolution_factor.GetValue(true) == v.resolution_factor.GetDefault());
    CHECK(static_cast<int>(v.texture_filter.GetValue()) == 2);
    CHECK(v.vr_immersive_mode.UsingGlobal());
    CHECK(v.vr_immersive_mode.GetValue() == 0u);
    ResetPresetSettings();
    v.resolution_factor.SetGlobal(false); // this game's own value
    v.resolution_factor.SetValue(2);
    v.texture_filter.SetValue(Settings::TextureFilter::Anime4K); // a global value chosen
    ApplyPresetSettings(kiu);
    CHECK(v.resolution_factor.GetValue() == 2);
    CHECK(v.texture_filter.UsingGlobal());
    CHECK(static_cast<int>(v.texture_filter.GetValue()) == 1);
    ResetPresetSettings();
}

// AzaharFrame kiu: the Controls preset gives starting values only: never over the game's own
// value or a global value the player chose, and once per game (vr_controls_preset_done), so what
// the player chooses afterwards stays, Use Global Value too.
// AzaharFrame kiugyro: with the C-Stick & Gyro mod, Kid Icarus: Uprising starts with Gyro Aim
// Light Gun and Motion Controls Right Controller; C-Stick at Edge is no longer given.
namespace {
void ResetControlsSettings() {
    auto& v = Settings::values;
    v.vr_bottom_screen_reticle.SetGlobal(true);
    v.vr_bottom_screen_reticle.SetValue(0);
    v.vr_motion_controls.SetGlobal(true);
    v.vr_motion_controls.SetValue(1);
    v.vr_reticle_edge_cstick.SetGlobal(true);
    v.vr_reticle_edge_cstick.SetValue(false);
    v.vr_reticle_stick_moves.SetGlobal(true);
    v.vr_reticle_stick_moves.SetValue(false);
    v.vr_gyro_aim.SetGlobal(true);
    v.vr_gyro_aim.SetValue(0);
    v.vr_gyro_sensitivity.SetGlobal(true);
    v.vr_gyro_sensitivity.SetValue(1.0f);
    v.vr_controls_preset_done.SetGlobal(true);
    v.vr_controls_preset_done.SetValue(0);
}
} // namespace

TEST_CASE("FrameXr Controls preset: starting values only", "[frame_xr][kiu][kiugyro]") {
    auto& v = Settings::values;
    // Every region, Japan's 0004000000030000 too: the same starting-values path.
    const uint64_t id = GENERATE(0x0004000000030000ull, 0x0004000000030100ull,
                                 0x0004000000030200ull, 0x0004000000030300ull);
    INFO("title " << std::hex << id);
    REQUIRE(FindControlsPreset(id) != nullptr);
    const ControlsPreset& kiu = *FindControlsPreset(id);
    SECTION("a fresh game with the mod: Light Gun from the right controller") {
        ResetControlsSettings();
        CHECK(ApplyControlsPreset(kiu, true) ==
              (kPresetReticle | kPresetMotionControls | kPresetGyroAim));
        CHECK(v.vr_bottom_screen_reticle.GetValue() == 3);
        CHECK(v.vr_motion_controls.GetValue() == 1);
        CHECK(!v.vr_motion_controls.UsingGlobal());
        CHECK(v.vr_gyro_aim.GetValue() == 2);
        CHECK(!v.vr_gyro_aim.UsingGlobal());
        CHECK(v.vr_gyro_aim.GetValue(true) == 0); // the global value stays Off
        // C-Stick at Edge is not given (the feature stays), nor Right Stick Moves Touch.
        CHECK(!v.vr_reticle_edge_cstick.GetValue());
        CHECK(v.vr_reticle_edge_cstick.UsingGlobal());
        CHECK(!v.vr_reticle_stick_moves.GetValue());
        CHECK(v.vr_controls_preset_done.GetValue() ==
              (kPresetReticle | kPresetMotionControls | kPresetGyroAim));
        // Booting again changes nothing.
        CHECK(ApplyControlsPreset(kiu, true) == 0u);
    }
    SECTION("a fresh game without the mod: as before (Motion Controls Off, no Gyro Aim)") {
        ResetControlsSettings();
        CHECK(ApplyControlsPreset(kiu, false) == (kPresetReticle | kPresetMotionControls));
        CHECK(v.vr_motion_controls.GetValue() == 0);
        CHECK(v.vr_gyro_aim.UsingGlobal());
        CHECK(v.vr_gyro_aim.GetValue() == 0);
        CHECK(v.vr_controls_preset_done.GetValue() == 3u);
        // The mod installed later: Gyro Aim given then; Motion Controls, decided, stays Off,
        // and Gyro Aim takes the right controller (GyroAimSource).
        CHECK(ApplyControlsPreset(kiu, true) == kPresetGyroAim);
        CHECK(v.vr_gyro_aim.GetValue() == 2);
        CHECK(v.vr_motion_controls.GetValue() == 0);
        CHECK(Input::GyroAimSource(v.vr_motion_controls.GetValue()) == 1u);
    }
    SECTION("a game that had the earlier preset (Motion Controls Off, C-Stick at Edge on)") {
        ResetControlsSettings();
        v.vr_motion_controls.SetGlobal(false);
        v.vr_motion_controls.SetValue(0);
        v.vr_reticle_edge_cstick.SetGlobal(false);
        v.vr_reticle_edge_cstick.SetValue(true);
        v.vr_bottom_screen_reticle.SetGlobal(false);
        v.vr_bottom_screen_reticle.SetValue(2); // the owner's Right Controller
        v.vr_controls_preset_done.SetGlobal(false);
        v.vr_controls_preset_done.SetValue(kPresetReticle | kPresetMotionControls |
                                           kPresetEdgeCStick);
        // Only the new value: none of the game's values is overwritten.
        CHECK(ApplyControlsPreset(kiu, true) == kPresetGyroAim);
        CHECK(v.vr_gyro_aim.GetValue() == 2);
        CHECK(v.vr_motion_controls.GetValue() == 0);
        CHECK(v.vr_reticle_edge_cstick.GetValue());
        CHECK(v.vr_bottom_screen_reticle.GetValue() == 2);
    }
    SECTION("a value the player saved for the game is never overwritten") {
        ResetControlsSettings();
        v.vr_bottom_screen_reticle.SetGlobal(false);
        v.vr_bottom_screen_reticle.SetValue(2);
        v.vr_motion_controls.SetGlobal(false);
        v.vr_motion_controls.SetValue(3);
        v.vr_gyro_aim.SetGlobal(false);
        v.vr_gyro_aim.SetValue(0); // Gyro Aim Off for this game
        CHECK(ApplyControlsPreset(kiu, true) == 0u);
        CHECK(v.vr_bottom_screen_reticle.GetValue() == 2);
        CHECK(v.vr_motion_controls.GetValue() == 3);
        CHECK(v.vr_gyro_aim.GetValue() == 0);
        // Marked: a later Use Global Value for them stays too.
        CHECK(v.vr_controls_preset_done.GetValue() ==
              (kPresetReticle | kPresetMotionControls | kPresetGyroAim));
    }
    SECTION("a global value the player chose is left alone, and not marked") {
        ResetControlsSettings();
        v.vr_motion_controls.SetValue(3);     // global Headset
        v.vr_bottom_screen_reticle.SetValue(1); // global Left Controller
        v.vr_gyro_aim.SetValue(1);            // global Gyro
        CHECK(ApplyControlsPreset(kiu, true) == 0u);
        CHECK(v.vr_motion_controls.UsingGlobal());
        CHECK(v.vr_motion_controls.GetValue() == 3);
        CHECK(v.vr_bottom_screen_reticle.GetValue() == 1);
        CHECK(v.vr_gyro_aim.UsingGlobal());
        CHECK(v.vr_gyro_aim.GetValue() == 1);
        CHECK(v.vr_controls_preset_done.GetValue() == 0u);
    }
    SECTION("after the preset, the player's choice stays: a new value, or Use Global Value") {
        ResetControlsSettings();
        ApplyControlsPreset(kiu, true);
        v.vr_motion_controls.SetGlobal(true);
        v.vr_gyro_aim.SetGlobal(true);
        v.vr_bottom_screen_reticle.SetValue(2);
        CHECK(ApplyControlsPreset(kiu, true) == 0u);
        CHECK(v.vr_motion_controls.UsingGlobal());
        CHECK(v.vr_gyro_aim.UsingGlobal());
        CHECK(v.vr_gyro_aim.GetValue() == 0);
        CHECK(v.vr_bottom_screen_reticle.GetValue() == 2);
    }
    SECTION("a value added to the preset later is given once, the old ones stay decided") {
        ResetControlsSettings();
        v.vr_controls_preset_done.SetGlobal(false);
        v.vr_controls_preset_done.SetValue(kPresetReticle); // an earlier build gave the reticle
        CHECK(ApplyControlsPreset(kiu, true) == (kPresetMotionControls | kPresetGyroAim));
        CHECK(v.vr_bottom_screen_reticle.UsingGlobal()); // Use Global Value chosen back then
        CHECK(v.vr_bottom_screen_reticle.GetValue() == 0);
        CHECK(v.vr_gyro_aim.GetValue() == 2);
    }
    ResetControlsSettings();
}

TEST_CASE("FrameXr Controls preset: C-Stick at Edge still needs the C-Stick mod",
          "[frame_xr][kiu][kiuedge]") {
    // AzaharFrame kiugyro: no preset gives it now; the gating stays for one that does.
    auto& v = Settings::values;
    constexpr ControlsPreset edge{.name = "test",
                                  .bottom_screen_reticle = 3,
                                  .motion_controls = 0,
                                  .edge_cstick = true,
                                  .edge_cstick_needs_mod = true};
    SECTION("no mod: the setting is left alone and not marked, the rest is given") {
        ResetControlsSettings();
        CHECK(ApplyControlsPreset(edge, false) == (kPresetReticle | kPresetMotionControls));
        CHECK(!v.vr_reticle_edge_cstick.GetValue());
        CHECK(v.vr_reticle_edge_cstick.UsingGlobal());
        CHECK(v.vr_controls_preset_done.GetValue() == 3u);
        // The mod installed later: given then.
        CHECK(ApplyControlsPreset(edge, true) == kPresetEdgeCStick);
        CHECK(v.vr_reticle_edge_cstick.GetValue());
    }
    SECTION("the player's own choice is never overwritten, mod or not") {
        ResetControlsSettings();
        v.vr_reticle_edge_cstick.SetGlobal(false);
        v.vr_reticle_edge_cstick.SetValue(false); // turned off for this game
        CHECK(ApplyControlsPreset(edge, true) == (kPresetReticle | kPresetMotionControls));
        CHECK(!v.vr_reticle_edge_cstick.GetValue());
        ResetControlsSettings();
        v.vr_reticle_edge_cstick.SetValue(true); // global on
        CHECK(ApplyControlsPreset(edge, false) == (kPresetReticle | kPresetMotionControls));
        CHECK(v.vr_reticle_edge_cstick.GetValue());
        CHECK(v.vr_reticle_edge_cstick.UsingGlobal());
    }
    ResetControlsSettings();
}

// AzaharFrame kiugyro: rollback. Builds before Gyro Aim keep vr_controls_preset_done (all its
// bits) but drop vr_gyro_aim when they write the game's file; the mirror in bits 8 and 9 brings
// the game's choice back. Old build = the key dropped, every other value kept.
namespace {
/// What a game's file holds across builds (the keys either build writes).
struct GameFile {
    bool has_gyro_aim = false;
    uint32_t gyro_aim = 0;
    bool has_done = false;
    uint32_t done = 0;
};
GameFile WriteNew() { // QtConfig::WriteVrValues for a game, this build
    auto& v = Settings::values;
    MirrorGyroAim();
    GameFile f;
    f.has_gyro_aim = !v.vr_gyro_aim.UsingGlobal();
    f.gyro_aim = v.vr_gyro_aim.GetValue();
    f.has_done = !v.vr_controls_preset_done.UsingGlobal();
    f.done = v.vr_controls_preset_done.GetValue();
    return f;
}
void ReadNew(const GameFile& f) { // QtConfig::ReadVrValues for a game, this build
    auto& v = Settings::values;
    v.vr_gyro_aim.SetGlobal(!f.has_gyro_aim);
    if (f.has_gyro_aim) {
        v.vr_gyro_aim.SetValue(f.gyro_aim);
    }
    v.vr_controls_preset_done.SetGlobal(!f.has_done);
    if (f.has_done) {
        v.vr_controls_preset_done.SetValue(f.done);
    }
    RestoreGyroAim();
}
GameFile OldBuildRoundTrip(GameFile f) { // an older build reads and writes the file
    f.has_gyro_aim = false;
    return f;
}
} // namespace

TEST_CASE("FrameXr Gyro Aim: rollback-safe across older builds", "[frame_xr][kiugyro]") {
    auto& v = Settings::values;
    const ControlsPreset& kiu = *FindControlsPreset(0x0004000000030200ull);
    const auto boot_new = [&](const GameFile& f) {
        ReadNew(f);
        ApplyControlsPreset(kiu, true);
    };
    SECTION("the preset's Light Gun survives new -> old -> new -> old -> new") {
        ResetControlsSettings();
        boot_new({});
        REQUIRE(v.vr_gyro_aim.GetValue() == 2);
        GameFile f = WriteNew();
        for (int i = 0; i < 2; i++) {
            f = OldBuildRoundTrip(f);
            ResetControlsSettings();
            boot_new(f);
            CHECK(!v.vr_gyro_aim.UsingGlobal());
            CHECK(v.vr_gyro_aim.GetValue() == 2);
            f = WriteNew();
        }
    }
    SECTION("the player's Off, and Gyro, survive too") {
        for (uint32_t choice : {0u, 1u}) {
            ResetControlsSettings();
            boot_new({});
            v.vr_gyro_aim.SetValue(choice); // the game's own value
            GameFile f = OldBuildRoundTrip(WriteNew());
            ResetControlsSettings();
            boot_new(f);
            CHECK(!v.vr_gyro_aim.UsingGlobal());
            CHECK(v.vr_gyro_aim.GetValue() == choice);
        }
    }
    SECTION("Use Global Value stays global after an older build") {
        ResetControlsSettings();
        boot_new({});
        v.vr_gyro_aim.SetGlobal(true);
        GameFile f = OldBuildRoundTrip(WriteNew());
        CHECK((f.done & kGyroAimMirrorMask) == 0u);
        ResetControlsSettings();
        boot_new(f);
        CHECK(v.vr_gyro_aim.UsingGlobal());
        CHECK(v.vr_gyro_aim.GetValue() == 0);
    }
    SECTION("the bits an older build knows keep their meaning") {
        ResetControlsSettings();
        boot_new({});
        const GameFile f = WriteNew();
        CHECK((f.done & 7u) == (kPresetReticle | kPresetMotionControls));
        CHECK((f.done & kPresetGyroAim) != 0u);
        CHECK((f.done & kGyroAimMirrorMask) == (3u << kGyroAimMirrorShift));
    }
    SECTION("a file from before Gyro Aim: nothing to restore, the preset gives it once") {
        ResetControlsSettings();
        GameFile old;
        old.has_done = true;
        old.done = kPresetReticle | kPresetMotionControls | kPresetEdgeCStick;
        boot_new(old);
        CHECK(v.vr_gyro_aim.GetValue() == 2);
    }
    ResetControlsSettings();
}

TEST_CASE("FrameXr Gyro Sensitivity: a new key with its default", "[frame_xr][kiugyro]") {
    // Missing from a file (an older build's, or never set): the default 1.0x, global or game.
    Settings::values.vr_gyro_sensitivity.SetGlobal(true);
    Settings::values.vr_gyro_sensitivity.SetValue(Settings::values.vr_gyro_sensitivity.GetDefault());
    CHECK(Settings::values.vr_gyro_sensitivity.GetValue() == 1.0f);
    CHECK(Settings::values.vr_light_gun_turn_zone.GetDefault() == 10u);
    CHECK(Settings::values.vr_light_gun_turn_speed.GetDefault() == 1.0f);
    CHECK(Settings::values.vr_gyro_aim.GetDefault() == 0u);
    CHECK(Settings::values.vr_light_gun_recoil.GetDefault()); // Recoil on
}

namespace {
Mods::ModInfo MakeMod(std::string id, bool managed, Mods::ModState state = Mods::ModState::Active,
                      std::vector<std::string> files = {}) {
    Mods::ModInfo m;
    m.id = id;
    m.name = id;
    m.managed = managed;
    m.state = state;
    m.files = std::move(files);
    return m;
}
} // namespace

TEST_CASE("FrameXr CStickModInstalled: catalogue ids and files", "[frame_xr][kiu][kiuedge]") {
    using Mods::ModState;
    CHECK(!CStickModInstalled(std::vector<Mods::ModInfo>{}));
    for (const char* id : {"kiu-cstick-gyro", "kiu-cstick-gyro-widescreen-16-9", "kiu-cstick-aim"}) {
        INFO(id);
        CHECK(CStickModInstalled({MakeMod(id, true)}));
        CHECK(!CStickModInstalled({MakeMod(id, true, ModState::Inactive)})); // library only
    }
    // Other mods of the game do not count, managed (60 FPS fix, soundtrack) or by hand.
    CHECK(!CStickModInstalled({MakeMod("kiu-cd-quality-soundtrack", true),
                               MakeMod("unmanaged:romfs", false, ModState::Active,
                                       {"romfs/a.bin"}),
                               MakeMod("unmanaged:code.ips", false, ModState::Active,
                                       {"exefs/code.ips"})}));
    // By hand: a full code replacement, or a cstick name.
    CHECK(CStickModInstalled({MakeMod("unmanaged:exefs", false, ModState::Active,
                                      {"exefs/code.bin", "exheader.bin"})}));
    CHECK(CStickModInstalled({MakeMod("unmanaged:kiu-cstick-aim", false, ModState::Active,
                                      {"kiu-cstick-aim/code.ips"})}));
    CHECK(CStickModInstalled({MakeMod("zip-C-Stick Aim", true)}));
}

// 1.2.0 fix lane B (triage item 16): World Scale and the Full modes' 3D Depth are starting values
// as the rest: a game's own World Scale (the grip's, or the migration's), its own 3D Depth, or a
// global value the player chose stay, and then the "own World Scale" mark is not written either.
TEST_CASE("FrameXr presets: World Scale and 3D Depth only where unset", "[frame_xr][fixb]") {
    auto& v = Settings::values;
    const ImmersivePreset& mk7 = *FindImmersivePreset(0x0004000000030800ull);
    SECTION("the game's own World Scale and 3D Depth") {
        ResetPresetSettings();
        v.vr_immersive_world_scale.SetGlobal(false);
        v.vr_immersive_world_scale.SetValue(45.0f);
        v.vr_immersive_world_set.SetGlobal(false);
        v.vr_immersive_world_set.SetValue(true);
        v.vr_immersive_depth.SetGlobal(false);
        v.vr_immersive_depth.SetValue(100);
        ApplyPresetSettings(mk7);
        CHECK(v.vr_immersive_world_scale.GetValue() == 45.0f);
        CHECK(v.vr_immersive_depth.GetValue() == 100u);
        CHECK(!v.vr_immersive_depth.UsingGlobal());
    }
    SECTION("a global World Scale and 3D Depth the player chose") {
        ResetPresetSettings();
        v.vr_immersive_world_scale.SetValue(150.0f);
        v.vr_immersive_depth.SetValue(60);
        ApplyPresetSettings(mk7);
        CHECK(v.vr_immersive_world_scale.UsingGlobal());
        CHECK(v.vr_immersive_world_scale.GetValue() == 150.0f);
        CHECK(v.vr_immersive_world_set.UsingGlobal()); // not marked: the migration decides
        CHECK(!v.vr_immersive_world_set.GetValue());
        CHECK(v.vr_immersive_depth.UsingGlobal());
        CHECK(v.vr_immersive_depth.GetValue() == 60u);
    }
    SECTION("not in another mode") {
        ResetPresetSettings();
        v.vr_immersive_mode.SetGlobal(false);
        v.vr_immersive_mode.SetValue(2); // the player's own mode 2 for this game
        ApplyPresetSettings(mk7);
        CHECK(v.vr_immersive_world_scale.UsingGlobal());
        CHECK(v.vr_immersive_depth.UsingGlobal());
    }
    ResetPresetSettings();
}
