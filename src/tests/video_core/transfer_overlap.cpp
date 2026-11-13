// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame feshimmer: display transfers whose output overlaps their input in memory take
// the software path (rasterizer_cache.h AccelerateDisplayTransfer).

#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "video_core/rasterizer_cache/utils.h"

using VideoCore::TransferOverlaps;

TEST_CASE("TransferOverlaps finds Fire Emblem's in-place glow downscales", "[video_core]") {
    // Echoes: 128x208 RGBA8 at 0x18108D00 halved to 64x104 at the same address
    REQUIRE(TransferOverlaps(0x18108D00, 128 * 208 * 4, 0x18108D00, 64 * 104 * 4));
    // then 64x128 at 0x18108D00 halved into 0x1810F500, inside the input
    REQUIRE(TransferOverlaps(0x18108D00, 64 * 128 * 4, 0x1810F500, 32 * 64 * 4));
    // the frame's own transfers do not overlap: 256x400 at 0x18000000 into the glow buffer
    REQUIRE_FALSE(TransferOverlaps(0x18000000, 256 * 400 * 4, 0x18108D00, 128 * 200 * 4));
    // touching ends are not an overlap
    REQUIRE_FALSE(TransferOverlaps(0x1000, 0x100, 0x1100, 0x100));
    REQUIRE_FALSE(TransferOverlaps(0x1100, 0x100, 0x1000, 0x100));
    REQUIRE_FALSE(TransferOverlaps(0x1000, 0, 0x1000, 0x100));
}

// AzaharFrame accxfer: Accurate Image Copies sends every display transfer to the software path.
// AzaharFrame xfergpu: off, an overlapping one goes there only when the GPU path can't do it.
TEST_CASE("Accurate Image Copies sends every display transfer to the software path",
          "[video_core]") {
    using VideoCore::DisplayTransferInSoftware;
    // the frame's own transfer: hardware when off, software when on
    REQUIRE_FALSE(DisplayTransferInSoftware(false, 0x18000000, 256 * 400 * 4, 0x18108D00,
                                            128 * 200 * 4, true));
    REQUIRE(DisplayTransferInSoftware(true, 0x18000000, 256 * 400 * 4, 0x18108D00,
                                      128 * 200 * 4, true));
    // an in-place downscale that keeps its format and tiling stays on the GPU when off
    REQUIRE_FALSE(
        DisplayTransferInSoftware(false, 0x18108D00, 128 * 208 * 4, 0x18108D00, 64 * 104 * 4, true));
    REQUIRE(DisplayTransferInSoftware(true, 0x18108D00, 128 * 208 * 4, 0x18108D00, 64 * 104 * 4,
                                      true));
    // one that changes the format or tiling in place goes to software
    REQUIRE(DisplayTransferInSoftware(false, 0x18108D00, 128 * 208 * 4, 0x18108D00, 64 * 104 * 2,
                                      false));
    // the GPU path's condition does not matter without an overlap
    REQUIRE_FALSE(DisplayTransferInSoftware(false, 0x1000, 0x100, 0x2000, 0x100, false));
    static_assert(DisplayTransferInSoftware(true, 0x1000, 0x100, 0x2000, 0x100, true));
    static_assert(!DisplayTransferInSoftware(false, 0x1000, 0x100, 0x2000, 0x100, true));
}

namespace {
struct Chunk {
    u32 start;
    u32 end;
    bool operator==(const Chunk&) const = default;
};
std::vector<Chunk> Chunks(u32 start, u32 end, u32 dst_addr, u32 dst_row, u32 src_addr,
                          u32 src_row) {
    std::vector<Chunk> out;
    VideoCore::ForEachRestrideChunk(start, end, dst_addr, dst_row, src_addr, src_row,
                                    [&](u32 s, u32 e) { out.push_back({s, e}); });
    return out;
}
} // namespace

// AzaharFrame xfergpu: ValidateByRestride copies memory between surfaces of different strides
// one row of tiles at a time; each piece must lie in one row of tiles of both.
TEST_CASE("ForEachRestrideChunk splits at the tile rows of both strides", "[video_core]") {
    constexpr u32 base = 0x18108D00;
    constexpr u32 row128 = 128 * 8 * 4; // RGBA8, tiled: one row of tiles of a 128-wide surface
    constexpr u32 row64 = 64 * 8 * 4;
    constexpr u32 row32 = 32 * 8 * 4;
    // Echoes: the tail of the 64-wide input of the second halving (rows 104-127) from the
    // 128-wide glow buffer: a right half, then a whole row as two halves
    REQUIRE(Chunks(base + 0x6800, base + 0x8000, base, row64, base, row128) ==
            std::vector<Chunk>{{base + 0x6800, base + 0x7000},
                               {base + 0x7000, base + 0x7800},
                               {base + 0x7800, base + 0x8000}});
    // the 32-wide glow texture at 0x1810F500 from the 128-wide buffer: one piece per row of
    // the narrower surface
    const auto glow = Chunks(base + 0x6800, base + 0x8800, base + 0x6800, row32, base, row128);
    REQUIRE(glow.size() == 8);
    for (u32 i = 0; i < glow.size(); i++) {
        REQUIRE(glow[i] == Chunk{base + 0x6800 + i * row32, base + 0x6800 + (i + 1) * row32});
    }
    // the same stride is one piece per row
    REQUIRE(Chunks(base, base + 2 * row64, base, row64, base, row64) ==
            std::vector<Chunk>{{base, base + row64}, {base + row64, base + 2 * row64}});
    // a wider destination is cut at the source's rows
    REQUIRE(Chunks(base, base + row128, base, row128, base, row64) ==
            std::vector<Chunk>{{base, base + row64}, {base + row64, base + row128}});
    // nothing for an empty range
    REQUIRE(Chunks(base, base, base, row64, base, row128).empty());
}
