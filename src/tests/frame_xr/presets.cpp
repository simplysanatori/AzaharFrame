// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <set>
#include <string>
#include <catch2/catch_test_macros.hpp>
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
        REQUIRE(preset->wider_level == -1); // AzaharFrame immersive2: the default, 1.4x
    }
    for (const uint64_t id : {0x0004000000054000ull, 0x0004000000053F00ull, 0x0004000000054100ull,
                              0x0004000000089D00ull, 0x0004000000089E00ull}) {
        const auto* preset = FindImmersivePreset(id);
        REQUIRE(preset != nullptr);
        REQUIRE(preset->mode == 3);
        REQUIRE(preset->register_offset == 90);
        REQUIRE(preset->wider_level == -1); // the owner plays it at the default 1.4x
        REQUIRE(preset->depth_multiplier == 2); // AzaharFrame depth: 100x, or no visible 3D
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
    REQUIRE(mk7->game_fov == 52);
    REQUIRE(std::string(mk7->eye_indicator) == "87,2");
    const auto* sm3dl = FindImmersivePreset(0x0004000000054000ull);
    REQUIRE(sm3dl->game_fov == 45);
    REQUIRE(std::string(sm3dl->eye_indicator) == "87,2");
    // AzaharFrame immersive2: Mario Kart 7 without Narrow Screen (it stretched), its screen 10 %
    // smaller instead; the rest at the true-scale default.
    REQUIRE(mk7->narrow == 0);
    REQUIRE(mk7->screen_size == 0.9f);
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
    REQUIRE(FindImmersivePreset(0x0004000000033500ull)->match_camera == -1);
    // Nothing changes Wider View; Narrow Screen is the default or off; values in range.
    std::size_t count = 0;
    const auto* titles = ImmersivePresetTitles(count);
    for (std::size_t i = 0; i < count; ++i) {
        const ImmersivePreset& p = *titles[i].preset;
        REQUIRE(p.wider_level == -1);
        REQUIRE((p.narrow == -1 || p.narrow == 0));
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
// preset keeps Immersive Mode Off and sets nothing else, so the player can still turn it on.
TEST_CASE("FrameXr presets: Kid Icarus: Uprising starts in the normal view", "[frame_xr]") {
    for (const uint64_t id : {0x0004000000030100ull, 0x0004000000030200ull, 0x0004000000030300ull}) {
        const auto* kiu = FindImmersivePreset(id);
        REQUIRE(kiu != nullptr);
        REQUIRE(kiu->mode == 0);
        REQUIRE(kiu->register_offset == -1);
        REQUIRE(kiu->depth_multiplier == 0);
        REQUIRE(kiu->game_fov == 0);
        REQUIRE(kiu->eye_indicator == nullptr);
        REQUIRE(kiu->wider_level == -1);
        REQUIRE(kiu->narrow == -1);
        REQUIRE(kiu->screen_size == 0.0f);
        REQUIRE(kiu->match_camera == -1);
    }
}
