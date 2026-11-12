// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame stall: "Preparing Shaders n / m" only while the game stands for its builds.

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/shader_notice.h"

using FrameXr::ShaderNotice;

namespace {
constexpr std::int64_t kMs = 1000;
}

TEST_CASE("ShaderNotice: builds while frames flow say nothing", "[frame_xr]") {
    ShaderNotice n;
    std::uint64_t queued = 0, done = 0;
    for (std::int64_t t = 1000 * kMs; t < 3000 * kMs; t += 100 * kMs) {
        queued += 2;
        done += 2;
        const auto out = n.Update(t, queued, done, t - 16 * kMs, true);
        REQUIRE(out.show.empty());
        REQUIRE(out.hide.empty());
    }
}

TEST_CASE("ShaderNotice: a game standing for its builds, then going on", "[frame_xr]") {
    ShaderNotice n;
    const std::int64_t frame_end = 1000 * kMs; // the last frame, then the load screen stands
    std::int64_t t = 1000 * kMs;
    REQUIRE(n.Update(t, 10, 10, frame_end, true).show.empty()); // ten earlier builds, all done
    t += 200 * kMs;
    REQUIRE(n.Update(t, 50, 12, frame_end, true).show.empty()); // not standing long enough yet
    t += 300 * kMs;
    auto out = n.Update(t, 60, 20, frame_end, true);
    REQUIRE(out.show == "Preparing Shaders 10 / 50");
    t += 200 * kMs;
    out = n.Update(t, 60, 40, frame_end, true);
    REQUIRE(out.show == "Preparing Shaders 30 / 50");
    t += 200 * kMs;
    REQUIRE(n.Update(t, 60, 40, frame_end, true).show.empty()); // same text: not again yet
    t += 1000 * kMs;
    REQUIRE(n.Update(t, 60, 40, frame_end, true).show == "Preparing Shaders 30 / 50"); // again
    t += 200 * kMs;
    out = n.Update(t, 60, 60, t - 10 * kMs, true); // all built, frames again
    REQUIRE(out.hide == "Preparing Shaders 30 / 50");
    REQUIRE_FALSE(n.Active());
    t += 2000 * kMs;
    REQUIRE(n.Update(t, 60, 60, frame_end, true).show.empty()); // standing, nothing building
}

TEST_CASE("ShaderNotice: paused or stopped takes it back", "[frame_xr]") {
    ShaderNotice n;
    REQUIRE(n.Update(1000 * kMs, 5, 0, 0, true).show == "Preparing Shaders 0 / 5"); // no frame yet
    const auto out = n.Update(1100 * kMs, 6, 1, 0, false);
    REQUIRE(out.hide == "Preparing Shaders 0 / 5");
    REQUIRE(out.show.empty());
    REQUIRE_FALSE(n.Active());
}
