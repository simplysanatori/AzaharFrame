// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/startup_escape.h"

// AzaharFrame memguard
using FrameXr::StartupEscape;
using A = StartupEscape::Action;

namespace {
constexpr std::int64_t kMs = 1'000'000;
} // namespace

TEST_CASE("Startup escape: Menu held 3 s stops a starting game", "[frame_xr]") {
    StartupEscape e;
    std::int64_t t = 0;
    REQUIRE(e.Update(true, true, false, t) == A::None); // let go: armed
    t += 10 * kMs;
    REQUIRE(e.Update(true, true, true, t) == A::None);
    REQUIRE(e.Update(true, true, true, t + 1400 * kMs) == A::None);
    REQUIRE(e.Update(true, true, true, t + 1500 * kMs) == A::ShowStopping);
    REQUIRE(e.Update(true, true, true, t + 2000 * kMs) == A::None);
    REQUIRE(e.Update(true, true, true, t + 3000 * kMs) == A::Stop);
    REQUIRE(e.StopRequested());
    REQUIRE(e.Update(true, true, true, t + 3100 * kMs) == A::None);
    // The game stops: all forgotten.
    REQUIRE(e.Update(false, false, false, t + 4000 * kMs) == A::None);
    REQUIRE_FALSE(e.StopRequested());
}

TEST_CASE("Startup escape: let go early takes the toast back", "[frame_xr]") {
    StartupEscape e;
    REQUIRE(e.Update(true, true, false, 0) == A::None);
    REQUIRE(e.Update(true, true, true, 0) == A::None);
    REQUIRE(e.Update(true, true, true, 2000 * kMs) == A::ShowStopping);
    REQUIRE(e.Update(true, true, false, 2100 * kMs) == A::HideStopping);
    // A new hold starts from zero.
    REQUIRE(e.Update(true, true, true, 2200 * kMs) == A::None);
    REQUIRE(e.Update(true, true, true, 5100 * kMs) == A::ShowStopping);
    REQUIRE(e.Update(true, true, true, 5200 * kMs) == A::Stop);
}

TEST_CASE("Startup escape: a button held from before the start does nothing", "[frame_xr]") {
    StartupEscape e;
    REQUIRE(e.Update(true, true, true, 0) == A::None);
    REQUIRE(e.Update(true, true, true, 5000 * kMs) == A::None);
    REQUIRE(e.Update(true, true, false, 5100 * kMs) == A::None);
    REQUIRE(e.Update(true, true, true, 5200 * kMs) == A::None);
    REQUIRE(e.Update(true, true, true, 8200 * kMs) == A::Stop);
}

TEST_CASE("Startup escape: live screens hand over to the game's controls", "[frame_xr]") {
    StartupEscape e;
    REQUIRE(e.Update(true, true, false, 0) == A::None);
    REQUIRE(e.Update(true, true, true, 0) == A::None);
    REQUIRE(e.Update(true, true, true, 1600 * kMs) == A::ShowStopping);
    REQUIRE(e.Update(false, true, true, 1700 * kMs) == A::HideStopping);
    REQUIRE(e.Update(false, true, true, 9000 * kMs) == A::None); // the mapper's View hold now
    REQUIRE_FALSE(e.StopRequested());
}

TEST_CASE("Startup escape: a stop that does not happen quits", "[frame_xr]") {
    StartupEscape e;
    REQUIRE(e.Update(true, true, false, 0) == A::None);
    REQUIRE(e.Update(true, true, true, 0) == A::None);
    REQUIRE(e.Update(true, true, true, 3000 * kMs) == A::Stop);
    REQUIRE(e.Update(true, true, false, 12'900 * kMs) == A::None);
    REQUIRE(e.Update(true, true, false, 13'000 * kMs) == A::Quit);
    REQUIRE(e.Update(true, true, false, 14'000 * kMs) == A::None); // once
    // Screens went live meanwhile: still stuck in the stop, still quits.
    StartupEscape f;
    REQUIRE(f.Update(true, true, false, 0) == A::None);
    REQUIRE(f.Update(true, true, true, 0) == A::None);
    REQUIRE(f.Update(true, true, true, 3000 * kMs) == A::Stop);
    REQUIRE(f.Update(false, true, false, 13'000 * kMs) == A::Quit);
}
