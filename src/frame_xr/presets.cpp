// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <iterator>

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
// Mario Kart 7: 52 degrees racing (40 in the menus), the 3DS's eyes 3.4 units apart: 10x.
// AzaharFrame immersive2: the owner found it stretched at the view-filling size (gain 0.4 to
// 0.5: a perspective picture magnified 2 to 2.5x in the middle is drawn 1.4 to 1.6 times taller
// than wide 35 degrees to the side) and with Narrow Screen 20 % (20 % thinner still); 10 % was
// "okay-ish". So no narrowing, and the screen 10 % smaller instead: the HUD sits where 10 % put
// it across, and the world keeps its shape (gain 1.11 both ways).
constexpr ImmersivePreset kMarioKart7{.name = "Mario Kart 7",
                                      .mode = 3,
                                      .register_offset = 90,
                                      .depth_multiplier = 1,
                                      .game_fov = 52,
                                      .eye_indicator = "87,2",
                                      .narrow = 0,
                                      .screen_size = 0.9f};
// Super Mario 3D Land: 45 degrees on the title and in the levels; the 3DS's eyes 58 units
// apart in a level (the title camera is about 1500 units away): 100x. At 1x its 3D could not
// be seen. Mock runs at 1.4x showed black below the floor of its levels; the owner plays it at
// 1.4x, so the default stays.
constexpr ImmersivePreset kSuperMario3DLand{.name = "Super Mario 3D Land",
                                            .mode = 3,
                                            .register_offset = 90,
                                            .depth_multiplier = 2,
                                            .game_fov = 45,
                                            .eye_indicator = "87,2"};
// Ocarina of Time 3D and Majora's Mask 3D: CitraVR's mode 2 (made for them; the view matrix is
// register 4). The picture is one flat screen, so the multiplier only moves the camera when
// you lean: Link is about 56 units tall, so 10x at a 3D Depth of 30 is life size, 10x at 50
// (the default) a little more. About 55 degrees in play (35 to 70 in cutscenes).
constexpr ImmersivePreset kOcarinaOfTime3D{.name = "Ocarina of Time 3D",
                                           .mode = 2,
                                           .register_offset = -1,
                                           .depth_multiplier = 1,
                                           .game_fov = 55};
// AzaharFrame immersive2: Majora's Mask 3D's measured angle swings between 30 and 55 degrees in
// play (the owner's headset log), so the gain swung with it; the owner turned Match Game Camera
// off, and its 55 degrees hold.
constexpr ImmersivePreset kMajorasMask3D{.name = "Majora's Mask 3D",
                                         .mode = 2,
                                         .register_offset = -1,
                                         .depth_multiplier = 1,
                                         .game_fov = 55,
                                         .match_camera = 0};
constexpr ImmersivePreset kAsphalt3D{.name = "Asphalt 3D", .mode = 3, .register_offset = 8};

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

// AzaharFrame immersive: Kid Icarus: Uprising has no immersive mode. It aims with a reticle the
// game draws on the screen (2D) and shoots along its own camera; in the Full modes the world is
// drawn from a camera turned with the head, which the game does not know about, so its shots
// and their effects fly off where the unturned camera looked while the reticle stays on the
// screen: they come apart whenever the head turns. Nothing in the drawing can join them again
// (the reticle cannot be told from the rest of the HUD). Wide (mode 1) keeps them together but
// adds little to this game; it plays in the normal view. Japan (compatibility list), the USA
// (the owner's copy) and Europe (030300, not in the compatibility list).
constexpr uint64_t kNotOffered[] = {
    0x0004000000030100, // Kid Icarus: Uprising, JPN
    0x0004000000030200, // USA
    0x0004000000030300, // EUR
};

} // namespace

bool ImmersiveOffered(uint64_t title_id) {
    for (const uint64_t id : kNotOffered) {
        if (id == title_id) {
            return false;
        }
    }
    return true;
}

const uint64_t* ImmersiveNotOfferedTitles(std::size_t& count) {
    count = std::size(kNotOffered);
    return kNotOffered;
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
