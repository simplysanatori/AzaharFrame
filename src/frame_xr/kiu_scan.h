// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiugyro: finds, in Kid Icarus: Uprising's code as loaded, the two addresses Light
// Gun reads through (kiu_probe.h), by code pattern rather than by a table of builds. Pure (a byte
// buffer in, addresses out), so it is unit tested. The patterns and why (Darkshade's C-Stick &
// Gyro mod v172; handoff/reports/37/kiugyro/design.md):
//
//   State: the mod's hook on the game's reticle registration (0xF45398 in every v172 build):
//       push {r4-r6,lr}; ldr r4,=STATE; sub r5,r0,#0x100; ldr r6,[r4,#0x1c]; cmp r1,r6
//     STATE + 0x20 holds the current player's reticle object.
//   Options: the mod's gyro scheme check (0xD33978):
//       ldr ip,=OPTIONS; ldr ip,[ip]; cmp ip,#0; beq; add ip,ip,#0x16000; ldr ip,[ip,#0x3e8];
//       and ip,ip,#0xe00000; cmp ip,#0x200000
//     [OPTIONS] + 0x163E8, bits 21 to 23: the game's control scheme (1 = the mod's gyro).
//   Offsets: the mod's box hook (0xF453E4) reads the reticle at +0xA4 and +0xA8:
//       mov r4,r0; mov r5,r1; mov r6,r2; mov r7,r3; ldr r8,[sp,#0x20]; ldr ip,[sp,#0x24];
//       ldr sb,[r4,#0xa4]; ldr sl,[r4,#0xa8]
//     so the object layout the probe reads is the one this build uses.
// All three are found once each in the 12 code.bin of the v172 zip (3 regions x base, 16:9,
// 21:9, plus the 3DS builds), and none in the game without the mod. A build that changes them is
// not read (Light Gun falls back to Gyro).

#pragma once

#include <cstddef>
#include <cstdint>

namespace FrameXr::Kiu {

struct ModAddresses {
    bool found = false;
    uint32_t state = 0;       ///< the mod's state block (0xF35D40 in v172)
    uint32_t options_ptr = 0; ///< where the game keeps its options object's address
};

/// Searches `size` bytes loaded at guest address `base` (4-byte aligned). Found only when the
/// state and options patterns are there exactly once each and the offsets pattern at least once.
ModAddresses ScanForMod(const uint8_t* data, std::size_t size, uint32_t base);

/// The reticle object's fields (the game's own layout, design.md fact 8).
inline constexpr uint32_t kStateReticle = 0x20;
inline constexpr uint32_t kReticleGround = 0x81; ///< u8, 1: the ground aim box
inline constexpr uint32_t kReticleX = 0xA4;      ///< float, screen pixels
inline constexpr uint32_t kReticleY = 0xA8;
inline constexpr uint32_t kReticleW = 0xE0;
inline constexpr uint32_t kReticleCx = 0xE4;
inline constexpr uint32_t kReticleH = 0xE8;
inline constexpr uint32_t kReticleCy = 0xEC;
inline constexpr uint32_t kReticleSpan = 0xF0;
inline constexpr uint32_t kOptionsScheme = 0x163E8;
inline constexpr uint32_t kSchemeGyro = 1;

constexpr uint32_t SchemeOf(uint32_t options_word) {
    return (options_word >> 21) & 7u;
}

/// Kid Icarus: Uprising's title ids (Japan as the mod lists it, then the compatibility list's).
constexpr bool IsKidIcarus(uint64_t title_id) {
    return title_id == 0x0004000000030000ull || title_id == 0x0004000000030100ull ||
           title_id == 0x0004000000030200ull || title_id == 0x0004000000030300ull;
}

} // namespace FrameXr::Kiu
