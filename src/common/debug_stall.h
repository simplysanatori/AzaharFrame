// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: test-only stalls, to check that the XR frame loop keeps its pace while the
// emulator is busy. AZAHARFRAME_DEBUG_STALL=<where>:<ms>[:<every s>] sleeps <ms> once every
// <every s> seconds (default 5) at one place:
//   tick     the emu thread, after the frame (RasterizerCache::TickFrame), no lock held
//   submit   the Vulkan worker, inside the scheduler's submit mutex (the queue lock)
//   acquire  the emu thread, between acquiring the XR screen images and queueing their release
//   release  the XR release thread, after the GPU finished, before xrReleaseSwapchainImage
//   picture  the emu thread, at a VBlank that shows a new game picture (the game swapped its top
//            screen), before the picture is drawn: <ms> at every <every>-th one (default each),
//            no period in seconds. A game whose frame with the new picture runs long and whose
//            next VBlank (the same picture again) catches up at once, as Kid Icarus Uprising
//            does in play (AzaharFrame pacing2).
// Unset (the default): Maybe() is one relaxed load.

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include "common/test_hooks.h"
#include "common/logging/log.h"

namespace Common::DebugStall {

enum class Where { None, Tick, Submit, Acquire, Release, Picture };

struct Config {
    Where where = Where::None;
    int ms = 0;
    int every_s = 5;
};

inline const Config& Get() {
    static const Config config = [] {
        Config c;
        const char* v = Common::TestHookEnv("AZAHARFRAME_DEBUG_STALL");
        if (!v || !*v) {
            return c;
        }
        const std::string s{v};
        const auto colon = s.find(':');
        const std::string name = s.substr(0, colon);
        if (name == "tick") {
            c.where = Where::Tick;
        } else if (name == "submit") {
            c.where = Where::Submit;
        } else if (name == "acquire") {
            c.where = Where::Acquire;
        } else if (name == "release") {
            c.where = Where::Release;
        } else if (name == "picture") {
            c.where = Where::Picture;
            c.every_s = 1; // here: every n-th new picture
        }
        if (colon != std::string::npos) {
            c.ms = std::atoi(s.c_str() + colon + 1);
            const auto second = s.find(':', colon + 1);
            if (second != std::string::npos) {
                c.every_s = std::max(1, std::atoi(s.c_str() + second + 1));
            }
        }
        if (c.where == Where::None || c.ms <= 0) {
            c.where = Where::None;
        } else {
            LOG_WARNING(Render, "AZAHARFRAME_DEBUG_STALL={}: test stalls of {} ms every {} s", v,
                        c.ms, c.every_s);
        }
        return c;
    }();
    return config;
}

/// Sleeps when `where` is the configured place and its period has passed.
inline void Maybe(Where where) {
    const Config& c = Get();
    if (c.where != where) [[likely]] {
        return;
    }
    if (where == Where::Picture) {
        static std::atomic<long long> pictures{0};
        if (pictures.fetch_add(1, std::memory_order_relaxed) % c.every_s == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(c.ms));
        }
        return;
    }
    static std::atomic<long long> last_s{0};
    const long long now_s = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
    long long last = last_s.load(std::memory_order_relaxed);
    if (last == 0) {
        // The first one a period after the first call (the game is up by then).
        last_s.compare_exchange_strong(last, now_s);
        return;
    }
    if (now_s - last < c.every_s || !last_s.compare_exchange_strong(last, now_s)) {
        return;
    }
    LOG_WARNING(Render, "AZAHARFRAME_DEBUG_STALL: sleeping {} ms", c.ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(c.ms));
}

} // namespace Common::DebugStall
