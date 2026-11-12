// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flat

#include <catch2/catch_test_macros.hpp>

#include "frame_xr/input/flat_vr_hold.h"

using FrameXr::Input::FlatVrHold;
using Event = FlatVrHold::Event;

namespace {
constexpr int64_t kMs = 1'000'000;

/// Menu down from `from` to `to` ms in 10 ms steps; the events seen, then let go at `to`.
struct Run {
    int ask = 0;
    int close = 0;
    int64_t ask_at = -1;
};
Run Press(FlatVrHold& h, int64_t from, int64_t to, bool in_menu, bool chord = false,
          bool asking = false) {
    Run r;
    for (int64_t t = from; t < to; t += 10) {
        const Event e = h.Update(true, chord, in_menu, asking, t * kMs);
        if (e == Event::AskVr) {
            r.ask++;
            r.ask_at = t;
        }
        r.close += e == Event::CloseFlatMenu;
    }
    const Event e = h.Update(false, false, in_menu, asking, to * kMs);
    r.close += e == Event::CloseFlatMenu;
    r.ask += e == Event::AskVr;
    return r;
}
FlatVrHold Armed() {
    FlatVrHold h;
    h.Update(false, false, false, false, 0);
    return h;
}
} // namespace

TEST_CASE("FlatVrHold: 3 s asks once, never earlier", "[frame_xr]") {
    FlatVrHold h = Armed();
    Run r = Press(h, 100, 2990, false);
    REQUIRE(r.ask == 0);
    r = Press(h, 3000, 9000, false);
    REQUIRE(r.ask == 1);
    REQUIRE(r.ask_at == 6000);
}

TEST_CASE("FlatVrHold: the press that opened the Flat menu goes on to ask", "[frame_xr]") {
    FlatVrHold h = Armed();
    // The press starts in the game; at 1 s the Flat menu opens (in_flat_menu from then on).
    for (int64_t t = 100; t < 1100; t += 10) {
        REQUIRE(h.Update(true, false, false, false, t * kMs) == Event::None);
    }
    int ask = 0;
    for (int64_t t = 1100; t < 3200; t += 10) {
        ask += h.Update(true, false, true, false, t * kMs) == Event::AskVr;
    }
    REQUIRE(ask == 1);
    REQUIRE_FALSE(h.StartedInFlatMenu()); // No closes the Flat menu this press opened
    REQUIRE(h.Update(false, false, true, false, 3200 * kMs) == Event::None);
}

TEST_CASE("FlatVrHold: let go between 1 and 3 s does nothing more", "[frame_xr]") {
    FlatVrHold h = Armed();
    // Opened the Flat menu in a game (started outside it): letting go keeps the menu open.
    for (int64_t t = 0; t < 1000; t += 10) {
        h.Update(true, false, false, false, t * kMs);
    }
    REQUIRE(h.Update(true, false, true, false, 2000 * kMs) == Event::None);
    REQUIRE(h.Update(false, false, true, false, 2500 * kMs) == Event::None);
}

TEST_CASE("FlatVrHold: in the Flat menu a 1-3 s press closes it on release", "[frame_xr]") {
    FlatVrHold h = Armed();
    Run r = Press(h, 100, 600, true);
    REQUIRE(r.close == 0); // too short
    r = Press(h, 1000, 2500, true);
    REQUIRE(r.close == 1);
    REQUIRE(r.ask == 0);
    r = Press(h, 5000, 8500, true); // held to 3 s: asks, and no close after
    REQUIRE(r.ask == 1);
    REQUIRE(h.StartedInFlatMenu()); // No leaves the Flat menu open
    REQUIRE(r.close == 0);
}

TEST_CASE("FlatVrHold: the chord, a held-over press and an open question count nothing",
          "[frame_xr]") {
    FlatVrHold h; // not armed: Menu held from VR
    for (int64_t t = 0; t < 5000; t += 10) {
        REQUIRE(h.Update(true, false, false, false, t * kMs) == Event::None);
    }
    h.Update(false, false, false, false, 5000 * kMs);
    Run r = Press(h, 6000, 10000, false, true); // a bumper or View with Menu
    REQUIRE(r.ask == 0);
    r = Press(h, 11000, 15000, true, false, true); // the question already shows
    REQUIRE(r.ask == 0);
    REQUIRE(r.close == 0);
    h.Reset();
    r = Press(h, 16000, 20000, false);
    REQUIRE(r.ask == 0); // Reset: unarmed until Menu is seen up
}
