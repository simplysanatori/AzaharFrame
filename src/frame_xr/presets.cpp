// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <iterator>

#include "frame_xr/presets.h"

namespace FrameXr {

namespace {

// CitraVR's v0.4 notes: "Mario Kart 7 or Super Mario 3D Land: Super Immersive Mode Profile 2
// (our mode 3), Register Offset 90. Asphalt: Profile 2, Register Offset 8."
// Wider View stays off in every preset: it is the player's choice per game. Super Mario 3D
// Land shows black boxes at the edges with it.
//
// AzaharFrame depth: the rest was measured in mock runs of each game's play (IMMERSIVE.md,
// "Tuned games"). Game FOV is the camera's vertical angle from its projection matrix: the
// screen is sized with the measured angle while the game runs, and this is the value before it
// is seen. The eye indicator is the one CitraVR's search finds, set so the 3D is there from the
// first frame. The Depth Factor Multiplier makes the eye distance (6.5 cm x 3D Depth / 10 x
// 10^value, in the game's units) about the game's own at full 3D, at a 3D Depth of 90.
// Nintendo's games keep the projection in registers 86 to 89, the view matrix in 90 to 93 and
// the eye in 87,2.
//
// Mario Kart 7: 52 degrees racing (40 in the menus), the 3DS's eyes 3.4 units apart: 10x.
constexpr ImmersivePreset kMarioKart7{"Mario Kart 7", 3, 90, false, 1, 52, "87,2"};
// Super Mario 3D Land: 45 degrees on the title and in the levels; the 3DS's eyes 58 units
// apart in a level (the title camera is about 1500 units away): 100x. At 1x its 3D could not
// be seen.
constexpr ImmersivePreset kSuperMario3DLand{"Super Mario 3D Land", 3, 90, false, 2, 45, "87,2"};
// Kid Icarus: Uprising: 45 degrees flying, the 3DS's eyes about 6.3 units apart: 10x. The
// reticle aims through the game's own camera, so look ahead when you shoot (IMMERSIVE.md).
constexpr ImmersivePreset kKidIcarusUprising{"Kid Icarus: Uprising", 3, 90, false, 1, 45,
                                            "87,2"};
// Ocarina of Time 3D and Majora's Mask 3D: CitraVR's mode 2 (made for them; the view matrix is
// register 4). The picture is one flat screen, so the multiplier only moves the camera when
// you lean: Link is about 56 units tall, so 10x at a 3D Depth of 30 is life size, 10x at 50
// (the default) a little more. About 55 degrees in play (35 to 70 in cutscenes).
constexpr ImmersivePreset kOcarinaOfTime3D{"Ocarina of Time 3D", 2, -1, false, 1, 55};
constexpr ImmersivePreset kMajorasMask3D{"Majora's Mask 3D", 2, -1, false, 1, 55};
constexpr ImmersivePreset kAsphalt3D{"Asphalt 3D", 3, 8};

// Title ids: Azahar's compatibility list (every release of the game it knows) and, for Super
// Mario 3D Land, src/common/hacks/hack_list.cpp. Other regions of Asphalt 3D, and Kid Icarus:
// Uprising outside Japan and the USA, are not confirmed.
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
    // Kid Icarus: Uprising
    {0x0004000000030100, &kKidIcarusUprising}, // JPN
    {0x0004000000030200, &kKidIcarusUprising}, // USA
    // The Legend of Zelda: Ocarina of Time 3D
    {0x0004000000033400, &kOcarinaOfTime3D},
    {0x0004000000033500, &kOcarinaOfTime3D},
    {0x0004000000033600, &kOcarinaOfTime3D},
    {0x000400000008F800, &kOcarinaOfTime3D},
    {0x000400000008F900, &kOcarinaOfTime3D},
    // The Legend of Zelda: Majora's Mask 3D
    {0x0004000000125500, &kMajorasMask3D},
    {0x0004000000125600, &kMajorasMask3D},
    {0x00040000000D6E00, &kMajorasMask3D},
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
