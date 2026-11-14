// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "frame_xr/kiu_scan.h"

// AzaharFrame kiugyro (kiu_scan.h)
namespace FrameXr::Kiu {

namespace {
uint32_t Word(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
           static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

// An ARM `ldr Rd, [pc, #imm]` at `at` (index into data): the literal it loads, if inside.
bool PcLiteral(const uint8_t* data, std::size_t size, std::size_t at, uint32_t rd,
               uint32_t& out) {
    const uint32_t w = Word(data + at);
    if ((w & 0x0F7F0000u) != 0x051F0000u || ((w >> 28) & 0xFu) != 0xEu ||
        ((w >> 12) & 0xFu) != rd) {
        return false; // not an unconditional ldr Rd, [pc, #+-imm]
    }
    const std::size_t imm = w & 0xFFFu;
    const bool up = (w >> 23) & 1u;
    const std::size_t pc = at + 8;
    if (!up && imm > pc) {
        return false;
    }
    const std::size_t lit = up ? pc + imm : pc - imm;
    if (lit + 4 > size || (lit & 3u) != 0) {
        return false;
    }
    out = Word(data + lit);
    return true;
}

constexpr uint8_t kStateTail[] = {0x01, 0x5c, 0x40, 0xe2, 0x1c, 0x60, 0x94, 0xe5,
                                  0x06, 0x00, 0x51, 0xe1};
constexpr uint8_t kStateHead[] = {0x70, 0x40, 0x2d, 0xe9};
constexpr uint8_t kOptionsTail[] = {0x00, 0xc0, 0x9c, 0xe5, 0x00, 0x00, 0x5c, 0xe3, 0x06, 0x00,
                                    0x00, 0x0a, 0x16, 0xca, 0x8c, 0xe2, 0xe8, 0xc3, 0x9c, 0xe5,
                                    0x0e, 0xc6, 0x0c, 0xe2, 0x02, 0x06, 0x5c, 0xe3};
constexpr uint8_t kOffsets[] = {0x00, 0x40, 0xa0, 0xe1, 0x01, 0x50, 0xa0, 0xe1, 0x02, 0x60, 0xa0,
                                0xe1, 0x03, 0x70, 0xa0, 0xe1, 0x20, 0x80, 0x9d, 0xe5, 0x24, 0xc0,
                                0x9d, 0xe5, 0xa4, 0x90, 0x94, 0xe5, 0xa8, 0xa0, 0x94, 0xe5};

bool At(const uint8_t* data, std::size_t size, std::size_t i, const uint8_t* pat,
        std::size_t len) {
    return i + len <= size && std::memcmp(data + i, pat, len) == 0;
}
} // namespace

ModAddresses ScanForMod(const uint8_t* data, std::size_t size, uint32_t base) {
    ModAddresses out;
    if (data == nullptr || size < 64) {
        return out;
    }
    int states = 0, options = 0, offsets = 0;
    uint32_t state = 0, options_ptr = 0;
    for (std::size_t i = 8; i + 4 <= size; i += 4) {
        if (data[i] == kStateTail[0] && At(data, size, i, kStateTail, sizeof(kStateTail)) &&
            At(data, size, i - 8, kStateHead, sizeof(kStateHead))) {
            uint32_t lit = 0;
            if (PcLiteral(data, size, i - 4, 4, lit)) {
                states++;
                state = lit;
            }
        }
        if (data[i] == kOptionsTail[0] && At(data, size, i, kOptionsTail, sizeof(kOptionsTail))) {
            uint32_t lit = 0;
            if (PcLiteral(data, size, i - 4, 12, lit)) {
                options++;
                options_ptr = lit;
            }
        }
        if (data[i] == kOffsets[0] && At(data, size, i, kOffsets, sizeof(kOffsets))) {
            offsets++;
        }
    }
    (void)base; // the literals are absolute addresses already
    if (states == 1 && options == 1 && offsets >= 1 && state != 0 && options_ptr != 0) {
        out.found = true;
        out.state = state;
        out.options_ptr = options_ptr;
    }
    return out;
}

// ---- AzaharFrame kiuguns ----

namespace {

/// Masked words (value, mask), word aligned.
struct Masked {
    uint32_t value;
    uint32_t mask;
};

bool Matches(const uint8_t* data, std::size_t size, std::size_t at, const Masked* words,
             std::size_t n) {
    if (at + n * 4 > size) {
        return false;
    }
    for (std::size_t k = 0; k < n; k++) {
        if ((Word(data + at + k * 4) & words[k].mask) != (words[k].value & words[k].mask)) {
            return false;
        }
    }
    return true;
}

/// The target of a `b`/`bl` at `at` (an index), if inside.
bool BranchTarget(const uint8_t* data, std::size_t size, std::size_t at, std::size_t& out) {
    const uint32_t w = Word(data + at);
    if (((w >> 25) & 7u) != 5u) {
        return false;
    }
    int32_t off = static_cast<int32_t>(w & 0xFFFFFFu);
    if (off & 0x800000) {
        off -= 0x1000000;
    }
    const int64_t t = static_cast<int64_t>(at) + 8 + int64_t{off} * 4;
    if (t < 0 || static_cast<std::size_t>(t) + 8 > size) {
        return false;
    }
    out = static_cast<std::size_t>(t);
    return true;
}

/// An ARM data-processing immediate (rotated 8 bits).
uint32_t DpImmediate(uint32_t w) {
    const uint32_t rot = ((w >> 8) & 0xFu) * 2;
    const uint32_t imm = w & 0xFFu;
    return rot ? (imm >> rot) | (imm << (32 - rot)) : imm;
}

constexpr uint32_t kAll = 0xFFFFFFFFu;
constexpr uint32_t kLdrPc = 0xFFFFF000u; // the opcode and register, not the offset
constexpr uint32_t kBl = 0xFF000000u;

// ldrb r1,[r4,#0x2c]; pop {r4-r8,lr}; add r2,r1,r1,lsl#3; rsb r1,r2,r1,lsl#6; add r0,r0,r1,lsl#4
constexpr Masked kWeaponTail[] = {{0xE5D4102Cu, kAll}, {0xE8BD41F0u, kAll}, {0xE0812181u, kAll},
                                  {0xE0621301u, kAll}, {0xE0800201u, kAll}};
// ldr r4,=CACHE; ldr r0,[r4]; cmp r0,#0; bne; bl GETTER; str r0,[r4]; add r2,r0,#A;
// add r2,r2,#B; ldr r1,=999999999; ldr r0,[r2,#C]; add r0,r0,#1; cmp r0,#1; blo; cmp r0,r1; bls;
// mov r0,r1; str r0,[r2,#C]
constexpr Masked kShotTail[] = {
    {0xE59F4050u, kLdrPc}, {0xE5940000u, kAll}, {0xE3500000u, kAll}, {0x1A000001u, kAll},
    {0xEBFB9BEBu, kBl},    {0xE5840000u, kAll}, {0xE2802A16u, kAll}, {0xE2822F9Eu, kAll},
    {0xE59F1034u, kLdrPc}, {0xE59201DCu, kAll}, {0xE2800001u, kAll}, {0xE3500001u, kAll},
    {0x3A000001u, kAll},   {0xE1500001u, kAll}, {0x9A000000u, kAll}, {0xE1A00001u, kAll},
    {0xE58201DCu, kAll}};

} // namespace

WeaponAddresses ScanForWeapon(const uint8_t* code, std::size_t size, uint32_t base) {
    WeaponAddresses out;
    if (code == nullptr || size < 64) {
        return out;
    }
    int found = 0;
    uint32_t ptr = 0, field = 0;
    const std::size_t n = sizeof(kWeaponTail) / sizeof(kWeaponTail[0]);
    for (std::size_t i = 0x20; i + n * 4 + 4 <= size; i += 4) {
        if (code[i] != 0x2C || !Matches(code, size, i, kWeaponTail, n)) {
            continue;
        }
        std::size_t get = 0;
        if (!BranchTarget(code, size, i + n * 4, get) || (Word(code + i + n * 4) >> 24) != 0xEAu) {
            continue; // not followed by `b`
        }
        const uint32_t g = Word(code + get);
        if ((g & 0xFFFFF0F0u) != 0xE1D000D0u || Word(code + get + 4) != 0xE12FFF1Eu) {
            continue; // not `ldrsb r0,[r0,#imm]; bx lr`
        }
        uint32_t lit = 0;
        bool have = false;
        for (std::size_t b = i - 0x20; b < i; b += 4) {
            uint32_t v = 0;
            if (PcLiteral(code, size, b, 5, v)) {
                lit = v;
                have = true;
            }
        }
        if (!have) {
            continue;
        }
        found++;
        ptr = lit;
        field = ((g >> 4) & 0xF0u) | (g & 0xFu);
    }
    (void)base;
    if (found == 1 && ptr != 0 && field == 5) {
        out = {true, ptr, field};
    }
    return out;
}

ShotAddresses ScanForShots(const uint8_t* code, std::size_t size, uint32_t base) {
    ShotAddresses out;
    if (code == nullptr || size < 128) {
        return out;
    }
    int found = 0;
    uint32_t cache = 0, offset = 0;
    const std::size_t n = sizeof(kShotTail) / sizeof(kShotTail[0]);
    for (std::size_t i = 0; i + n * 4 <= size; i += 4) {
        if (code[i + 4] != 0x00 || !Matches(code, size, i, kShotTail, n)) {
            continue;
        }
        uint32_t c = 0, cap = 0;
        if (!PcLiteral(code, size, i, 4, c) || !PcLiteral(code, size, i + 32, 1, cap) ||
            cap != kShotsCap) {
            continue;
        }
        found++;
        cache = c;
        offset = DpImmediate(Word(code + i + 24)) + DpImmediate(Word(code + i + 28)) +
                 (Word(code + i + 36) & 0xFFFu);
    }
    (void)base;
    if (found == 1 && cache != 0 && offset != 0) {
        out = {true, cache, offset};
    }
    return out;
}

int WeaponFromRecords(const FighterRecords& r) {
    for (int s = 0; s < kFighterSlots; s++) {
        const int w = r.weapon[static_cast<std::size_t>(s)];
        if (w < -1 || w >= kWeaponCount) {
            return -2;
        }
    }
    const int16_t kind = r.kind[0];
    if (kind != 1 && kind != 2) {
        return -1;
    }
    for (int s = 1; s < kFighterSlots; s++) {
        const int16_t k = r.kind[static_cast<std::size_t>(s)];
        if (k == 1 || k == 2) {
            return -1; // another fighter: multiplayer, the local slot is not known
        }
    }
    return r.weapon[0];
}

int WeaponSettle::Update(int64_t now_ns, int raw) {
    if (!started_ || raw != candidate_) {
        started_ = true;
        candidate_ = raw;
        since_ns_ = now_ns;
    }
    if (candidate_ != settled_ && now_ns - since_ns_ >= kSettleNs) {
        settled_ = candidate_;
    }
    return settled_;
}

uint32_t ShotCounter::Update(uint32_t value) {
    if (!has_ || value < last_ || value - last_ > kMaxStep) {
        has_ = true;
        last_ = value;
        return 0;
    }
    const uint32_t n = value - last_;
    last_ = value;
    return n;
}

} // namespace FrameXr::Kiu
