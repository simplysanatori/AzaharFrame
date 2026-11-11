// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame gamelist: the game list with the controller (frame_xr/input/list_nav.h).

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/list_nav.h"

using namespace FrameXr::Input;

namespace {

constexpr int64_t kMs = 1'000'000;

struct Pad {
    bool up = false, down = false, a = false;
    float stick = 0.0f;
};

/// Runs frames 8 ms apart (Flat's rate) and counts what comes out.
struct Runner {
    ListNav nav;
    int64_t now = 0;
    int steps_up = 0, steps_down = 0, starts = 0;
    void Run(const Pad& p, int64_t ms, bool enabled = true) {
        for (int64_t t = 0; t < ms; t += 8) {
            const ListNav::Result r = nav.Update(enabled, p.up, p.down, p.stick, p.a, now);
            steps_up += r.step < 0;
            steps_down += r.step > 0;
            starts += r.activate;
            now += 8 * kMs;
        }
    }
};

} // namespace

TEST_CASE("FrameXr list nav: a D-pad press steps once, a hold repeats", "[frame_xr]") {
    Runner r;
    r.Run({}, 50);
    r.Run({.down = true}, 100);
    CHECK(r.steps_down == 1);
    r.Run({}, 50);
    r.Run({.up = true}, 50);
    CHECK(r.steps_up == 1);
    r.Run({}, 50);
    // Held 1 s: one at once, then after 400 ms every 110 ms: 1 + floor(600 / 110) + 1.
    r.steps_down = 0;
    r.Run({.down = true}, 1000);
    CHECK(r.steps_down >= 6);
    CHECK(r.steps_down <= 7);
    // Both directions at once: nothing.
    r.Run({}, 50);
    r.steps_up = r.steps_down = 0;
    r.Run({.up = true, .down = true}, 500);
    CHECK(r.steps_up + r.steps_down == 0);
}

TEST_CASE("FrameXr list nav: the left stick steps with hysteresis", "[frame_xr]") {
    Runner r;
    r.Run({.stick = 0.5f}, 200); // under stick_on
    CHECK(r.steps_up == 0);
    r.Run({.stick = 0.7f}, 50);
    CHECK(r.steps_up == 1);
    r.Run({.stick = 0.45f}, 300); // above stick_off: still held, no new step
    CHECK(r.steps_up == 1);
    r.Run({.stick = 0.3f}, 50); // let go
    r.Run({.stick = -0.8f}, 50); // down (OpenXR: negative y is down)
    CHECK(r.steps_down == 1);
}

TEST_CASE("FrameXr list nav: A twice quickly starts, once or slowly does not", "[frame_xr]") {
    Runner r;
    r.Run({}, 50);
    // One tap.
    r.Run({.a = true}, 80);
    r.Run({}, 600);
    CHECK(r.starts == 0);
    // Two taps 100 ms apart, press to press 180 ms.
    r.Run({.a = true}, 80);
    r.Run({}, 100);
    r.Run({.a = true}, 80);
    CHECK(r.starts == 1);
    r.Run({}, 600);
    // Two taps 500 ms apart: too slow.
    r.Run({.a = true}, 80);
    r.Run({}, 420);
    r.Run({.a = true}, 80);
    CHECK(r.starts == 1);
    r.Run({}, 600);
    // Three quick taps: one start (the third begins a new pair).
    r.starts = 0;
    for (int i = 0; i < 3; i++) {
        r.Run({.a = true}, 64);
        r.Run({}, 64);
    }
    CHECK(r.starts == 1);
}

TEST_CASE("FrameXr list nav: the double tap's limit is the interval", "[frame_xr]") {
    ListNav nav;
    const int64_t limit = ListNavTiming{}.double_tap_ns;
    nav.Update(true, false, false, 0.0f, false, 0);
    nav.Update(true, false, false, 0.0f, true, 1 * kMs);
    nav.Update(true, false, false, 0.0f, false, 2 * kMs);
    CHECK(nav.Update(true, false, false, 0.0f, true, 1 * kMs + limit).activate);

    ListNav late;
    late.Update(true, false, false, 0.0f, false, 0);
    late.Update(true, false, false, 0.0f, true, 1 * kMs);
    late.Update(true, false, false, 0.0f, false, 2 * kMs);
    CHECK(!late.Update(true, false, false, 0.0f, true, 2 * kMs + limit).activate);
}

TEST_CASE("FrameXr list nav: nothing while not allowed, nor from a button held before",
          "[frame_xr]") {
    Runner r;
    r.Run({.down = true}, 500, false);
    r.Run({.a = true}, 50, false);
    r.Run({}, 50, false);
    r.Run({.a = true}, 50, false);
    CHECK(r.steps_down + r.starts == 0);
    // A direction held while the list becomes allowed (a dialog closes): no step until let go.
    r.Run({.down = true}, 100, false);
    r.Run({.down = true}, 800, true);
    CHECK(r.steps_down == 0);
    r.Run({}, 50);
    r.Run({.down = true}, 50);
    CHECK(r.steps_down == 1);
    // A first tap before a dialog closed and a second one after it: no start.
    r.Run({}, 50);
    r.Run({.a = true}, 50, false);
    r.Run({}, 50, false);
    r.Run({.a = true}, 50, true);
    CHECK(r.starts == 0);
    // An A held when the panel appears is no tap.
    ListNav fresh;
    CHECK(!fresh.Update(true, false, false, 0.0f, true, 0).activate);
    CHECK(!fresh.Update(true, false, false, 0.0f, false, 10 * kMs).activate);
    CHECK(!fresh.Update(true, false, false, 0.0f, true, 20 * kMs).activate);
    CHECK(fresh.Update(true, false, false, 0.0f, false, 30 * kMs).step == 0);
}

TEST_CASE("FrameXr list nav: the game list takes the controller only when nothing is over it",
          "[frame_xr]") {
    ListNavState s;
    s.list_visible = true;
    s.main_active = true;
    CHECK(ListNavAllowed(s));
    s.vr_showing = true;
    CHECK(ListNavAllowed(s));

    auto with = [&](auto change) {
        ListNavState t = s;
        change(t);
        return ListNavAllowed(t);
    };
    CHECK(!with([](ListNavState& t) { t.game_running = true; }));
    CHECK(!with([](ListNavState& t) { t.list_visible = false; }));
    CHECK(!with([](ListNavState& t) { t.popup_open = true; }));
    CHECK(!with([](ListNavState& t) { t.dialog_open = true; }));
    // Another window has the keys: never.
    CHECK(!with([](ListNavState& t) { t.main_active = false; }));
    // No window has the keys: fine in VR (gamescope), not in Flat (another app may).
    CHECK(with([](ListNavState& t) {
        t.main_active = false;
        t.none_active = true;
    }));
    CHECK(!with([](ListNavState& t) {
        t.main_active = false;
        t.none_active = true;
        t.vr_showing = false;
    }));
}
