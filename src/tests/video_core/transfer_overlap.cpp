// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame feshimmer: display transfers whose output overlaps their input in memory take
// the software path (rasterizer_cache.h AccelerateDisplayTransfer).

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

// AzaharFrame accxfer: Accurate Image Copies sends every display transfer to the software path;
// off, only the overlapping ones go.
TEST_CASE("Accurate Image Copies sends every display transfer to the software path",
          "[video_core]") {
    using VideoCore::DisplayTransferInSoftware;
    // the frame's own transfer: hardware when off, software when on
    REQUIRE_FALSE(DisplayTransferInSoftware(false, 0x18000000, 256 * 400 * 4, 0x18108D00,
                                            128 * 200 * 4));
    REQUIRE(DisplayTransferInSoftware(true, 0x18000000, 256 * 400 * 4, 0x18108D00,
                                      128 * 200 * 4));
    // an in-place downscale goes to software either way
    REQUIRE(DisplayTransferInSoftware(false, 0x18108D00, 128 * 208 * 4, 0x18108D00, 64 * 104 * 4));
    REQUIRE(DisplayTransferInSoftware(true, 0x18108D00, 128 * 208 * 4, 0x18108D00, 64 * 104 * 4));
    static_assert(DisplayTransferInSoftware(true, 0x1000, 0x100, 0x2000, 0x100));
    static_assert(!DisplayTransferInSoftware(false, 0x1000, 0x100, 0x2000, 0x100));
}
