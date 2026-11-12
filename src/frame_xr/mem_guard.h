// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame modfix: should Steam's browser be opened from the game? On the Frame the browser
// (steamwebhelper, plus SteamVR's own) needs a few hundred MB to start. When memory and swap are
// nearly used up, that start makes the kernel kill the emulator (it carries oom_score_adj 900, so
// it goes first, whatever its own small size: seen as "Killed", exit 137, 12 s after a "Created
// by" link was opened). So the link is only opened with room to spare. Pure parsing of
// /proc/meminfo text (unit-tested in src/tests/frame_xr/mem_guard.cpp).

#pragma once

#include <cstdint>
#include <sstream>
#include <string>

namespace FrameXr {

/// Memory the system can still hand out without killing anything, in MB: MemAvailable + SwapFree
/// from the text of /proc/meminfo. -1 when MemAvailable is missing.
inline std::int64_t MemoryHeadroomMb(const std::string& meminfo) {
    std::int64_t available = -1;
    std::int64_t swap_free = 0;
    std::istringstream in(meminfo);
    std::string key;
    std::int64_t kb = 0;
    std::string unit;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        if (!(ls >> key >> kb)) {
            continue;
        }
        if (key == "MemAvailable:") {
            available = kb;
        } else if (key == "SwapFree:") {
            swap_free = kb;
        }
    }
    return available < 0 ? -1 : (available + swap_free) / 1024;
}

/// Room needed to open the browser: its processes plus the pages they load.
constexpr std::int64_t kBrowserNeedsMb = 1500;

/// True when opening the browser now is likely to get the emulator killed. An unreadable
/// /proc/meminfo is not "low": the link opens as before.
inline bool BrowserWouldRunOutOfMemory(const std::string& meminfo) {
    const std::int64_t headroom = MemoryHeadroomMb(meminfo);
    return headroom >= 0 && headroom < kBrowserNeedsMb;
}

} // namespace FrameXr
