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

} // namespace FrameXr::Kiu
