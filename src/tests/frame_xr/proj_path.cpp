// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame projdef: the top screen's path (frame_xr/vis/proj_path.h): the projection layer in
// the Full modes with Motion Clarity off, the quads otherwise, the test switch over both, and no
// change for a pause or a Quick Load.

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/vis/proj_path.h"

namespace {

using FrameXr::Vis::PathChoice;
using FrameXr::Vis::PathInputs;
using FrameXr::Vis::PathWhy;
constexpr int64_t kSecond = 1'000'000'000;

PathInputs Game(bool full, bool clarity, int64_t t) {
    PathInputs in;
    in.game_on_view = true;
    in.full_mode = full;
    in.clarity = clarity;
    in.now_ns = t;
    return in;
}

PathInputs NoGame(int64_t t) {
    PathInputs in;
    in.now_ns = t;
    return in;
}

} // namespace

TEST_CASE("Projection path: the Full modes without Motion Clarity", "[frame_xr][projdef]") {
    PathChoice p;
    CHECK_FALSE(p.Next(NoGame(0))); // home: quads
    CHECK(p.Why() == PathWhy::Start);
    CHECK_FALSE(p.TakeChanged());
    CHECK(p.Next(Game(true, false, 1)));
    CHECK(p.Why() == PathWhy::Full);
    CHECK(p.TakeChanged());
    CHECK_FALSE(p.TakeChanged());
    // Motion Clarity in the Full modes: the quads, flashed.
    CHECK_FALSE(p.Next(Game(true, true, 2)));
    CHECK(p.Why() == PathWhy::Clarity);
    CHECK(p.TakeChanged());
    // The normal and the wide view: the quads, whatever Motion Clarity says.
    CHECK_FALSE(p.Next(Game(false, false, 3)));
    CHECK(p.Why() == PathWhy::NotFull);
    CHECK_FALSE(p.Next(Game(false, true, 4)));
}

TEST_CASE("Projection path: no game on view holds the choice a while", "[frame_xr][projdef]") {
    PathChoice p;
    CHECK(p.Next(Game(true, false, 0)));
    p.TakeChanged();
    // A Quick Load or a boot: the screens off for 2 s, then back. No change, no new pass.
    for (int64_t t = 1; t <= 2; t++) {
        CHECK(p.Next(NoGame(t * kSecond)));
        CHECK(p.Why() == PathWhy::Held);
    }
    CHECK(p.Next(Game(true, false, 3 * kSecond)));
    CHECK_FALSE(p.TakeChanged());
    // The game closed: the quads after the hold.
    CHECK(p.Next(NoGame(10 * kSecond)));
    CHECK(p.Next(NoGame(10 * kSecond + FrameXr::Vis::kNoGameHoldNs - 1)));
    CHECK_FALSE(p.Next(NoGame(10 * kSecond + FrameXr::Vis::kNoGameHoldNs)));
    CHECK(p.Why() == PathWhy::NoGame);
    CHECK(p.TakeChanged());
    // The quads with no game stay the quads.
    CHECK_FALSE(p.Next(NoGame(60 * kSecond)));
    CHECK_FALSE(p.TakeChanged());
}

TEST_CASE("Projection path: the test switch overrides it both ways", "[frame_xr][projdef]") {
    using FrameXr::Vis::ProjForcedFrom;
    CHECK(ProjForcedFrom(std::string{"1"}) == std::optional<bool>{true});
    CHECK(ProjForcedFrom(std::string{"0"}) == std::optional<bool>{false});
    CHECK_FALSE(ProjForcedFrom(std::nullopt).has_value());
    CHECK_FALSE(ProjForcedFrom(std::string{""}).has_value());
    CHECK_FALSE(ProjForcedFrom(std::string{"on"}).has_value());

    PathChoice p;
    PathInputs in = Game(true, false, 0);
    in.forced = false; // =0: the quads in a Full mode
    CHECK_FALSE(p.Next(in));
    CHECK(p.Why() == PathWhy::Forced);
    in = Game(false, true, 1);
    in.forced = true; // =1: the projection layer anywhere, Motion Clarity or not
    CHECK(p.Next(in));
    in = NoGame(2);
    in.forced = true;
    CHECK(p.Next(in)); // home too, as the test builds did
    // The switch lifted with no game on view: the quads at once (no hold for a forced choice).
    CHECK_FALSE(p.Next(NoGame(3)));
    CHECK(p.Why() == PathWhy::NoGame);
    // Lifted in a game: the settings' choice.
    CHECK(p.Next(Game(true, false, 4)));
    CHECK(p.Why() == PathWhy::Full);
}

TEST_CASE("Projection path: every reason has a log text", "[frame_xr][projdef]") {
    for (const PathWhy w : {PathWhy::Start, PathWhy::Forced, PathWhy::Full, PathWhy::Clarity,
                            PathWhy::NotFull, PathWhy::Held, PathWhy::NoGame}) {
        CHECK(std::string{FrameXr::Vis::PathWhyText(w)}.size() > 3);
    }
}
