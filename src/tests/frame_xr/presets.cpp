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
        REQUIRE_FALSE(preset->wider_view); // off by default in every game
    }
    for (const uint64_t id : {0x0004000000054000ull, 0x0004000000053F00ull, 0x0004000000054100ull,
                              0x0004000000089D00ull, 0x0004000000089E00ull}) {
        const auto* preset = FindImmersivePreset(id);
        REQUIRE(preset != nullptr);
        REQUIRE(preset->mode == 3);
        REQUIRE(preset->register_offset == 90);
        REQUIRE_FALSE(preset->wider_view); // Super Mario 3D Land shows black boxes with it
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
    // AzaharFrame immersive: Mario Kart 7's screen a little bigger ("closer"), the rest at the
    // size that fills the view.
    REQUIRE(mk7->screen_size > 1.0f);
    REQUIRE(mk7->screen_size <= 1.2f);
    REQUIRE(sm3dl->screen_size == 0.0f);
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
    // Nothing turns Wider View or Narrow Screen on.
    std::size_t count = 0;
    const auto* titles = ImmersivePresetTitles(count);
    for (std::size_t i = 0; i < count; ++i) {
        REQUIRE_FALSE(titles[i].preset->wider_view);
        REQUIRE(titles[i].preset->squeeze == 0);
        REQUIRE(titles[i].preset->game_fov <= 110); // the setting's range
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
        REQUIRE((titles[i].preset->mode == 2 || titles[i].preset->mode == 3));
    }
}

// AzaharFrame immersive: Kid Icarus: Uprising has no immersive mode, in any region.
TEST_CASE("FrameXr presets: games without immersive mode", "[frame_xr]") {
    for (const uint64_t id : {0x0004000000030100ull, 0x0004000000030200ull, 0x0004000000030300ull}) {
        REQUIRE_FALSE(ImmersiveOffered(id));
        REQUIRE(FindImmersivePreset(id) == nullptr);
    }
    REQUIRE(ImmersiveOffered(0));
    REQUIRE(ImmersiveOffered(0x0004000000030800ull)); // Mario Kart 7
    REQUIRE(ImmersiveOffered(0x0004000000033500ull)); // Ocarina of Time 3D
    // No game is both offered a preset and refused.
    std::size_t refused = 0;
    const uint64_t* ids = ImmersiveNotOfferedTitles(refused);
    REQUIRE(refused == 3);
    for (std::size_t i = 0; i < refused; ++i) {
        REQUIRE(FindImmersivePreset(ids[i]) == nullptr);
    }
}
