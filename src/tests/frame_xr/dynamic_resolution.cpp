// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: Dynamic Resolution's decisions (frame_xr/dynamic_resolution.h): only when
// the GPU is the limit, one step at a time, only after seconds in a row, never up and down.

#include <cmath>
#include <cstddef>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/dynamic_resolution.h"
#include "video_core/vr_dynamic_resolution.h" // AzaharFrame ui2

using namespace FrameXr::DynamicResolution;

namespace {

constexpr double kPeriod = 1000.0 / 120.0;

Second Fine() {
    return {1.0, 120, kPeriod, 60, 60, 2.0, true};
}
Second GpuBehind() {
    return {1.0, 120, kPeriod, 50, 50, 25.0, true};
}
Second CpuBound() { // frames missed, the GPU with room: the emulation thread is late
    return {1.0, 100, kPeriod, 45, 45, 3.0, true};
}
Second Neutral() {
    return {1.0, 120, kPeriod, 60, 60, 6.0, true};
}

int Run(Controller& c, const Second& s, int seconds, int max_drop = 3) {
    int d = c.Drop();
    for (int i = 0; i < seconds; i++) {
        d = c.Judge(s, max_drop);
    }
    return d;
}

} // namespace

TEST_CASE("Dynamic Resolution: what a bad and a good second are", "[frame_xr]") {
    CHECK(Controller::Bad(GpuBehind()));
    CHECK_FALSE(Controller::Bad(CpuBound())); // a lower resolution would not help
    // Near the budget (8.3 ms at 120 Hz) is the limit already.
    CHECK(Controller::Bad({1.0, 120, kPeriod, 60, 60, 7.6, true}));
    CHECK_FALSE(Controller::Bad({1.0, 120, kPeriod, 60, 60, 7.0, true}));
    CHECK_FALSE(Controller::Bad(Fine()));
    CHECK(Controller::Good(Fine(), 3));
    CHECK_FALSE(Controller::Good(Neutral(), 3));
    // AzaharFrame flash3: room is the next factor's predicted cost, the time now times its pixels
    // over this factor's, under three quarters of the bad line (5.6 ms at 120 Hz). Session 573's
    // MK7 at 3x: 3.0 ms predicts 5.3 ms at 4x (room), 4.5 ms (its median) 8.0 ms (none).
    CHECK(std::abs(Controller::PredictUp(3.0, 3) - 3.0 * 16.0 / 9.0) < 1e-9);
    CHECK(Controller::Good({1.0, 120, kPeriod, 60, 60, 3.0, true}, 3));
    CHECK_FALSE(Controller::Good({1.0, 120, kPeriod, 60, 60, 3.2, true}, 3));
    CHECK_FALSE(Controller::Good({1.0, 120, kPeriod, 60, 60, 4.5, true}, 3));
    // The higher the factor, the smaller the step: 3.5 ms is room at 5x (to 6x: 5.0 ms) and 4x
    // (5.5 ms), not at 2x (7.9 ms).
    CHECK(Controller::Good({1.0, 120, kPeriod, 60, 60, 3.5, true}, 5));
    CHECK(Controller::Good({1.0, 120, kPeriod, 60, 60, 3.5, true}, 4));
    CHECK_FALSE(Controller::Good({1.0, 120, kPeriod, 60, 60, 3.5, true}, 2));
    // 1x to 2x is four times the pixels.
    CHECK(Controller::Good({1.0, 120, kPeriod, 60, 60, 1.3, true}, 1));
    CHECK_FALSE(Controller::Good({1.0, 120, kPeriod, 60, 60, 1.5, true}, 1));
    CHECK_FALSE(Controller::Good({1.0, 120, kPeriod, 60, 60, 1.5, true}, 0)); // as 1x
    CHECK_FALSE(Controller::Bad({1.0, 120, kPeriod, 60, 60, 2.9, true}));
    CHECK_FALSE(Controller::Bad(Neutral()));
    // The game's own rate does not count: a 30 fps game with the headset at full rate is fine.
    Second thirty = Fine();
    thirty.emu_frames = thirty.gpu_frames = 30;
    CHECK(Controller::Good(thirty, 3));
    // 90 Hz: 90 frames are all of them.
    CHECK(Controller::Good({1.0, 90, 1000.0 / 90.0, 30, 30, 2.0, true}, 3));
    // Too few timed frames say nothing either way.
    CHECK_FALSE(Controller::Good({1.0, 120, kPeriod, 60, 4, 1.0, true}, 3));
    CHECK(Controller::Bad({1.0, 90, 1000.0 / 90.0, 30, 30, 10.5, true}));
    // Too few timed frames to say the GPU is behind.
    CHECK_FALSE(Controller::Bad({1.0, 120, kPeriod, 60, 3, 30.0, true}));
}

TEST_CASE("Dynamic Resolution: CPU-bound seconds never lower it, 1x is the floor",
          "[frame_xr]") {
    Controller c;
    CHECK(Run(c, CpuBound(), 1000) == 0);
    // Internal Resolution 1x: nothing to take off however far behind the GPU is.
    CHECK(Run(c, GpuBehind(), 1000, 0) == 0);
}

TEST_CASE("Dynamic Resolution: down one step after bad seconds in a row", "[frame_xr]") {
    Controller c;
    CHECK(Run(c, GpuBehind(), Controller::kDownAfter - 1) == 0);
    CHECK(c.Judge(GpuBehind(), 3) == 1);
    // The seconds after a step are not judged (its own hitch).
    CHECK(Run(c, GpuBehind(), Controller::kSettle) == 1);
    // Still behind: another step, after as many bad seconds again.
    CHECK(Run(c, GpuBehind(), Controller::kDownAfter) == 2);
    // Never past the factor less 1.
    CHECK(Run(c, GpuBehind(), 100, 2) == 2);
}

TEST_CASE("Dynamic Resolution: a single bad second, or broken runs, change nothing",
          "[frame_xr]") {
    Controller c;
    for (int i = 0; i < 50; i++) {
        c.Judge(GpuBehind(), 3);
        c.Judge(GpuBehind(), 3);
        c.Judge(Fine(), 3);
    }
    CHECK(c.Drop() == 0);
}

TEST_CASE("Dynamic Resolution: up one step after many good seconds", "[frame_xr]") {
    Controller c;
    Run(c, GpuBehind(), Controller::kDownAfter + Controller::kSettle + Controller::kDownAfter);
    REQUIRE(c.Drop() == 2);
    Run(c, Fine(), Controller::kSettle + Controller::kHoldAfterDown);
    CHECK(Run(c, Fine(), Controller::kUpAfter - 1) == 2);
    CHECK(c.Judge(Fine(), 3) == 1);
    // A neutral second breaks the run.
    Run(c, Fine(), Controller::kSettle + Controller::kUpAfter - 1);
    c.Judge(Neutral(), 3);
    CHECK(Run(c, Fine(), Controller::kUpAfter - 1) == 1);
    CHECK(c.Judge(Fine(), 3) == 0);
    CHECK(Run(c, Fine(), 100) == 0); // never above the setting
}

TEST_CASE("Dynamic Resolution: a step up that cannot be held is not tried again soon",
          "[frame_xr]") {
    Controller c;
    Run(c, GpuBehind(), Controller::kDownAfter); // 1
    Run(c, Fine(), Controller::kSettle + Controller::kHoldAfterDown + Controller::kUpAfter); // 0
    REQUIRE(c.Drop() == 0);
    // Behind again at once: down, and the level is blocked.
    Run(c, Fine(), Controller::kSettle);
    Run(c, GpuBehind(), Controller::kDownAfter);
    REQUIRE(c.Drop() == 1);
    Run(c, Fine(), Controller::kSettle);
    CHECK(Run(c, Fine(), Controller::kRetryWait - 30) == 1);
    // After the wait it tries once more...
    int waited = 0;
    while (c.Drop() != 0 && waited < 100) {
        c.Judge(Fine(), 3);
        waited++;
    }
    CHECK(c.Drop() == 0);
    CHECK(waited <= 30);
    // ...and when that fails too, not again in this game.
    Run(c, GpuBehind(), Controller::kSettle + Controller::kDownAfter);
    REQUIRE(c.Drop() == 1);
    CHECK(Run(c, Fine(), 2000) == 1);
    // A new game (no screens) starts from the setting.
    Second none = Fine();
    none.live = false;
    CHECK(c.Judge(none, 3) == 0);
}

// AzaharFrame ui2: after a step down it holds a few seconds before good seconds count, so it
// does not bounce straight back up; it can still go further down while it holds.
TEST_CASE("Dynamic Resolution: it holds after a step down", "[frame_xr]") {
    constexpr int kEarliestUp =
        Controller::kSettle + Controller::kHoldAfterDown + Controller::kUpAfter;
    Controller c;
    Run(c, GpuBehind(), Controller::kDownAfter);
    REQUIRE(c.Drop() == 1);
    CHECK(Run(c, Fine(), kEarliestUp - 1) == 1);
    CHECK(c.Judge(Fine(), 3) == 0);

    // Bad seconds while it holds still step it down.
    Controller d;
    Run(d, GpuBehind(), Controller::kDownAfter);
    Run(d, GpuBehind(), Controller::kSettle);
    REQUIRE(d.Drop() == 1);
    CHECK(Run(d, GpuBehind(), Controller::kDownAfter) == 2);

    // Neutral seconds in the hold do not shorten it: still the full run of good ones after.
    Controller e;
    Run(e, GpuBehind(), Controller::kDownAfter);
    Run(e, Neutral(), Controller::kSettle + Controller::kHoldAfterDown);
    CHECK(Run(e, Fine(), Controller::kUpAfter - 1) == 1);
    CHECK(e.Judge(Fine(), 3) == 0);

    // A pause does not use the hold up: only judged seconds count.
    Controller f;
    Run(f, GpuBehind(), Controller::kDownAfter);
    Second paused = Fine();
    paused.emu_frames = paused.gpu_frames = 0;
    Run(f, paused, 100);
    CHECK(Run(f, Fine(), kEarliestUp - 1) == 1);
    CHECK(f.Judge(Fine(), 3) == 0);
}

TEST_CASE("Dynamic Resolution: paused seconds are not judged", "[frame_xr]") {
    Controller c;
    Second paused = GpuBehind();
    paused.emu_frames = 0;
    paused.gpu_frames = 0;
    CHECK(Run(c, paused, 100) == 0);
    // A pause breaks a bad run too.
    c.Judge(GpuBehind(), 3);
    c.Judge(GpuBehind(), 3);
    c.Judge(paused, 3);
    CHECK(c.Judge(GpuBehind(), 3) == 0);
    // The factor went down under the steps taken: they follow it.
    Run(c, GpuBehind(), 20);
    REQUIRE(c.Drop() == 3);
    CHECK(c.Judge(paused, 1) == 1);
    c.Reset();
    CHECK(c.Drop() == 0);
}

// AzaharFrame ui2: the owner's report 14: on at the start it worked; turned off in the game and
// on again it did not seem to. Off is the full factor at once; on again carries on at the level
// it had, and it still steps from there. The frame loop's side, with the setting.
TEST_CASE("Dynamic Resolution: off and on again in a game", "[frame_xr]") {
    namespace V = VideoCore::DynamicResolution;
    auto& setting = Settings::values.vr_dynamic_resolution;
    const bool was = setting.GetValue();
    setting.SetValue(true);
    V::Apply(6); // the renderer's factor: up to 5 steps
    Install();
    const auto run = [](const Second& s, int n) {
        for (int i = 0; i < n; i++) {
            NoteSecond(s);
        }
    };
    const auto drawn = [] { return V::Apply(6); };
    run(GpuBehind(), Controller::kDownAfter + Controller::kSettle + Controller::kDownAfter);
    REQUIRE(Drop() == 2);
    REQUIRE(drawn() == 4);

    // Off in the menu: the full factor at once.
    setting.SetValue(false);
    SetEnabled(false);
    CHECK(Drop() == 0);
    CHECK(drawn() == 6);
    run(GpuBehind(), 20); // nothing judged while it is off
    CHECK(drawn() == 6);

    // On again: where it was, and it carries on from there.
    setting.SetValue(true);
    SetEnabled(true);
    CHECK(Drop() == 2);
    CHECK(drawn() == 4);
    run(GpuBehind(), Controller::kSettle); // the step's own hitch is not judged
    CHECK(Drop() == 2);
    run(GpuBehind(), Controller::kDownAfter);
    CHECK(Drop() == 3);
    CHECK(drawn() == 3);

    // Changed elsewhere (Azahar's window): NoteSecond sees it the same way.
    setting.SetValue(false);
    run(Fine(), 1);
    CHECK(drawn() == 6);
    setting.SetValue(true);
    run(GpuBehind(), 1);
    CHECK(Drop() == 3);

    // Without anything learnt (a new game) it starts from the setting and still steps down.
    Reset();
    setting.SetValue(false);
    SetEnabled(false);
    setting.SetValue(true);
    SetEnabled(true);
    CHECK(Drop() == 0);
    run(GpuBehind(), Controller::kDownAfter);
    CHECK(Drop() == 1);
    // A game started after it was turned off forgets the old level.
    setting.SetValue(false);
    SetEnabled(false);
    Reset();
    setting.SetValue(true);
    SetEnabled(true);
    CHECK(Drop() == 0);

    Uninstall();
    setting.SetValue(was);
    V::g_base = 0;
}

// AzaharFrame flash3 (report 70, session 573): "3 bad seconds in a row" was not enforced: an
// ordinary second left the bad count as it was, and the step to 3x counted a bad second from
// 24 s before. Any second that is not bad now ends the run.
TEST_CASE("Dynamic Resolution: bad seconds must come in a row", "[frame_xr]") {
    Controller c;
    // Bad, bad, an ordinary second (neither bad nor good), bad: no step.
    for (int i = 0; i < 50; i++) {
        c.Judge(GpuBehind(), 3);
        c.Judge(GpuBehind(), 3);
        c.Judge(Neutral(), 3);
    }
    CHECK(c.Drop() == 0);
    // A CPU-bound second, or one with too few timed frames, ends the run too.
    Second few = GpuBehind();
    few.gpu_frames = 3;
    for (int i = 0; i < 50; i++) {
        c.Judge(GpuBehind(), 3);
        c.Judge(GpuBehind(), 3);
        c.Judge(i % 2 ? CpuBound() : few, 3);
    }
    CHECK(c.Drop() == 0);
    // Session 573's 4x stretch before the step to 3x: a bad second every few seconds among
    // ordinary ones (5-6 ms, the 4x median 5.1) is not a GPU that cannot keep up.
    Controller d;
    const double seconds[] = {5.1, 7.9, 5.0, 5.3, 6.1, 4.8, 8.1, 5.2, 5.6, 4.9, 5.4, 7.7,
                              5.0, 5.2, 6.0, 4.7, 5.5, 5.1, 8.4, 5.3, 4.9, 5.2, 9.4, 5.0};
    for (int round = 0; round < 10; round++) {
        for (const double ms : seconds) {
            d.Judge({1.0, 120, kPeriod, 60, 60, ms, true, 4}, 5);
        }
    }
    CHECK(d.Drop() == 0);
    // Three in a row still step it down.
    for (int i = 0; i < Controller::kDownAfter; i++) {
        d.Judge({1.0, 120, kPeriod, 60, 60, 9.0, true, 4}, 5);
    }
    CHECK(d.Drop() == 1);
}

namespace {

/// A GPU that takes `cost[factor]` ms a frame (the factor drawn from the controller's steps under
/// `base`); returns the factors it drew, one per second.
std::vector<int> Model(Controller& c, int base, const double* cost, int seconds) {
    std::vector<int> drawn;
    for (int i = 0; i < seconds; i++) {
        const int factor = base - c.Drop();
        c.Judge({1.0, 120, kPeriod, 60, 60, cost[factor], true, factor}, base - 1);
        drawn.push_back(base - c.Drop());
    }
    return drawn;
}

int Changes(const std::vector<int>& v) {
    int n = 0;
    for (std::size_t i = 1; i < v.size(); i++) {
        n += v[i] != v[i - 1];
    }
    return n;
}

} // namespace

// AzaharFrame flash3: a step up is judged by its predicted cost (the GPU's time times the next
// factor's pixels over this one's) against three quarters of the bad line, so it climbs back
// where the GPU has room, and the failed-step blocking keeps it from going up and down.
TEST_CASE("Dynamic Resolution: it climbs back where the GPU allows, without bouncing",
          "[frame_xr]") {
    // Session 573's MK7 at 120 Hz, the median per factor (6x over the budget).
    const double mk7[] = {0.0, 2.0, 2.6, 3.0, 5.1, 6.0, 10.6};
    {
        // From 3x (where the old rule left it): up to 4x, where 5.1 ms predicts 8.0 ms at 5x
        // (no room), and it stays there.
        Controller c;
        Model(c, 6, mk7, 0);
        const double heavy[] = {0.0, 9, 9, 9, 9, 9, 9}; // down to 3x first
        Model(c, 6, heavy, 3 * (Controller::kDownAfter + Controller::kSettle));
        REQUIRE(6 - c.Drop() == 3);
        const auto drawn = Model(c, 6, mk7, 1000);
        CHECK(drawn.back() == 4);
        CHECK(Changes(drawn) == 1);
    }
    {
        // A GPU whose cost jumps past the prediction at 6x (5x has room by the rule, 6x is over
        // the budget): it tries 6x, comes back down, waits kRetryWait, tries once more, and then
        // stays at 5x for the rest of the game.
        const double jump[] = {0.0, 1.0, 1.5, 2.0, 2.5, 3.5, 10.0};
        Controller c;
        const auto drawn = Model(c, 6, jump, 2000);
        CHECK(drawn.back() == 5);
        CHECK(Changes(drawn) == 5); // 6 -> 5, 5 -> 6, 6 -> 5, 5 -> 6, 6 -> 5
        int ups = 0;
        for (std::size_t i = 1; i < drawn.size(); i++) {
            ups += drawn[i] > drawn[i - 1];
        }
        CHECK(ups == 2);
    }
    {
        // A light game: back to the full factor and no change after.
        const double light[] = {0.0, 0.5, 0.8, 1.2, 1.8, 2.6, 3.5};
        Controller c;
        Model(c, 6, mk7, 60);
        REQUIRE(c.Drop() >= 1);
        const auto drawn = Model(c, 6, light, 600);
        CHECK(drawn.back() == 6);
        CHECK(c.Drop() == 0);
    }
}

// AzaharFrame flash3: the floor. 1x by default (as 1.1.1: under the setting in the immersive
// modes, which add two steps); with kFloorAtSetting, the Internal Resolution setting itself.
TEST_CASE("Dynamic Resolution: the floor", "[frame_xr]") {
    // Today's rule (the default).
    CHECK(MaxDrop(6, 4, false) == 5); // immersive over a 4x setting: down to 1x
    CHECK(MaxDrop(4, 4, false) == 3);
    CHECK(MaxDrop(6, 0, false) == 5); // Auto
    CHECK(MaxDrop(3, 1, false) == 0); // the setting at 1x: nothing to do (the row is locked)
    CHECK(MaxDrop(1, 0, false) == 0);
    CHECK(MaxDrop(0, 4, false) == 0); // the renderer's factor not known yet
    // The alternative: never under the setting; only the immersive modes' steps come off.
    CHECK(MaxDrop(6, 4, true) == 2);
    CHECK(MaxDrop(4, 4, true) == 0); // outside immersive it has nothing to do
    CHECK(MaxDrop(6, 0, true) == 5); // Auto: no setting to stop at
    CHECK(MaxDrop(3, 1, true) == 0);
    CHECK(MaxDrop(8, 7, true) == 1); // immersive capped at 8x
    CHECK(FloorFactor(6, 4, true) == 4);
    CHECK(FloorFactor(6, 4, false) == 1);
    // The one-line switch keeps today's rule.
    CHECK(MaxDrop(6, 4) == MaxDrop(6, 4, kFloorAtSetting));
    CHECK_FALSE(kFloorAtSetting);
}
