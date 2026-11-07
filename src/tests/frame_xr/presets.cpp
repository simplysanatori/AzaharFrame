// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <set>
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
        REQUIRE(preset->wider_view); // Mario Kart 7 holds up past its own view
    }
    for (const uint64_t id : {0x0004000000054000ull, 0x0004000000053F00ull, 0x0004000000054100ull,
                              0x0004000000089D00ull, 0x0004000000089E00ull}) {
        const auto* preset = FindImmersivePreset(id);
        REQUIRE(preset != nullptr);
        REQUIRE(preset->mode == 3);
        REQUIRE(preset->register_offset == 90);
        REQUIRE_FALSE(preset->wider_view); // Super Mario 3D Land shows black boxes with it
    }
    const auto* asphalt = FindImmersivePreset(0x0004000000034C00ull);
    REQUIRE(asphalt != nullptr);
    REQUIRE(asphalt->mode == 3);
    REQUIRE(asphalt->register_offset == 8);
}

TEST_CASE("FrameXr presets: other games have none, ids are unique", "[frame_xr]") {
    REQUIRE(FindImmersivePreset(0) == nullptr);
    REQUIRE(FindImmersivePreset(0x0004000000033500ull) == nullptr); // Ocarina of Time 3D
    std::size_t count = 0;
    const auto* titles = ImmersivePresetTitles(count);
    REQUIRE(count > 0);
    std::set<uint64_t> seen;
    for (std::size_t i = 0; i < count; ++i) {
        REQUIRE(seen.insert(titles[i].title_id).second);
        REQUIRE(titles[i].preset->mode == 3);
    }
}
