// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <iterator>

#include "frame_xr/presets.h"

namespace FrameXr {

namespace {

// CitraVR's v0.4 notes: "Mario Kart 7 or Super Mario 3D Land: Super Immersive Mode Profile 2
// (our mode 3), Register Offset 90. Asphalt: Profile 2, Register Offset 8."
constexpr ImmersivePreset kMarioKart7{"Mario Kart 7", 3, 90};
constexpr ImmersivePreset kSuperMario3DLand{"Super Mario 3D Land", 3, 90};
constexpr ImmersivePreset kAsphalt3D{"Asphalt 3D", 3, 8};

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
    // Asphalt 3D
    {0x0004000000034C00, &kAsphalt3D},
};

} // namespace

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
