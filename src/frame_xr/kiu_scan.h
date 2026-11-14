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

#include <array>
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

// ---- AzaharFrame kiuguns: the equipped weapon and the shots, in the game's own code ----
//
// Weapon: the tail of the game's getWeaponId(character) (EUR retail 0x4AF16C):
//     ldrb r1,[r4,#0x2c]; pop {r4-r8,lr}; add r2,r1,r1,lsl#3; rsb r1,r2,r1,lsl#6;
//     add r0,r0,r1,lsl#4; b <get>       with <get> = ldrsb r0,[r0,#FIELD]; bx lr
//   and `ldr r5,=PTR` in the 0x20 bytes before it. The fighters' records are at [PTR] + slot *
//   0x370; the weapon is the signed byte at +FIELD (5): -1 none, else class * 12 + index. Found
//   once in each of the 11 builds checked (EUR retail, EUR, USA and JPN with and without 16:9 /
//   21:9 C-Stick & Gyro mod builds); PTR 0xD636B4 (EUR, USA), 0xD6364C (JPN).
// Shots: the tail of the game's first shot function (EUR 0x4CFECC), just after its call to the
//   attack maker: the lifetime "shots fired" record of the save, +1 per shot, capped at
//   999,999,999:
//     ldr r4,=CACHE; ldr r0,[r4]; cmp r0,#0; bne +1; bl <getter>; str r0,[r4];
//     add r2,r0,#A; add r2,r2,#B; ldr r1,=999999999; ldr r0,[r2,#C]; add r0,r0,#1; cmp r0,#1;
//     blo; cmp r0,r1; bls; mov r0,r1; str r0,[r2,#C]
//   (the ldr offsets and the bl masked). The record is at [CACHE] + A + B + C (0xEF9814 in
//   every build). Found once in each of the 11 builds. Only the game's three shot functions
//   write it; the per-stage attack serial (EUR 0xD76B14) also counts other attacks.

/// The fighters' records and the equipped weapon's byte in them.
struct WeaponAddresses {
    bool found = false;
    uint32_t ptr = 0;   ///< where the records' address is kept (0xD636B4 in EUR)
    uint32_t field = 0; ///< the weapon's offset in a record (5)
};
WeaponAddresses ScanForWeapon(const uint8_t* code, std::size_t size, uint32_t base);

/// The shots-fired record.
struct ShotAddresses {
    bool found = false;
    uint32_t cache = 0;  ///< where the record's object address is kept
    uint32_t offset = 0; ///< the counter's offset in that object (0x16454)
};
ShotAddresses ScanForShots(const uint8_t* code, std::size_t size, uint32_t base);

inline constexpr uint32_t kFighterRecordBytes = 0x370;
inline constexpr int kFighterSlots = 6;
inline constexpr int kWeaponCount = 108;
inline constexpr uint32_t kShotsCap = 999'999'999u;

/// One read of the fighters' records (s16 kind at +0, s8 weapon at +field), slots 0 to 5.
struct FighterRecords {
    std::array<int16_t, kFighterSlots> kind{};
    std::array<int8_t, kFighterSlots> weapon{};
};
/// The weapon of the player (slot 0) in single player: -1 for none or when unsure (slot 0 not a
/// fighter of kind 1 or 2, or another slot a fighter too: multiplayer, whose local slot is not
/// known), or -2 for values out of range (not the record: a guard).
int WeaponFromRecords(const FighterRecords& r);

/// A weapon id shown only once it has held for kSettleNs of the game's time (a menu scrolling
/// through weapons does not load each).
class WeaponSettle {
public:
    static constexpr int64_t kSettleNs = 500'000'000;
    /// `raw`: this read's id (-1 none). Returns the settled id.
    int Update(int64_t now_ns, int raw);
    int Settled() const {
        return settled_;
    }
    void Reset() {
        settled_ = -1;
        candidate_ = -1;
        since_ns_ = 0;
        started_ = false;
    }

private:
    int settled_ = -1;
    int candidate_ = -1;
    int64_t since_ns_ = 0;
    bool started_ = false;
};

/// The shots from the record's reads: how many new ones since the last read. A drop (a save
/// loaded, another file) or a jump of more than kMaxStep (the same) counts none and starts again.
class ShotCounter {
public:
    static constexpr uint32_t kMaxStep = 16;
    uint32_t Update(uint32_t value);
    void Reset() {
        has_ = false;
    }

private:
    bool has_ = false;
    uint32_t last_ = 0;
};

/// Kid Icarus: Uprising's title ids (Japan as the mod lists it, then the compatibility list's).
constexpr bool IsKidIcarus(uint64_t title_id) {
    return title_id == 0x0004000000030000ull || title_id == 0x0004000000030100ull ||
           title_id == 0x0004000000030200ull || title_id == 0x0004000000030300ull;
}

} // namespace FrameXr::Kiu
