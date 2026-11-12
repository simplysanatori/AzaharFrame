// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/mem_guard.h"

// AzaharFrame modfix
using namespace FrameXr;

TEST_CASE("Mem guard: browser headroom", "[frame_xr]") {
    const auto info = [](long avail_kb, long swap_free_kb) {
        return "MemTotal:       16033352 kB\nMemFree:          140000 kB\nMemAvailable:   " +
               std::to_string(avail_kb) + " kB\nSwapTotal:       9064952 kB\nSwapFree:       " +
               std::to_string(swap_free_kb) + " kB\n";
    };
    SECTION("adds available memory and free swap") {
        REQUIRE(MemoryHeadroomMb(info(2048 * 1024, 1024 * 1024)) == 3072);
    }
    SECTION("the crash: little memory, swap full") {
        REQUIRE(BrowserWouldRunOutOfMemory(info(700 * 1024, 0)));
    }
    SECTION("enough room") {
        REQUIRE_FALSE(BrowserWouldRunOutOfMemory(info(4788 * 1024, 2410 * 1024)));
    }
    SECTION("unreadable meminfo does not block the link") {
        REQUIRE(MemoryHeadroomMb("") == -1);
        REQUIRE_FALSE(BrowserWouldRunOutOfMemory(""));
    }
}
