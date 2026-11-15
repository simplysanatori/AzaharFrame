// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: Dynamic Resolution's decisions (frame_xr/dynamic_resolution.h): only when
// the GPU is the limit, one step at a time, only after seconds in a row, never up and down.
// AzaharFrame fixd: up by the measured cost, longer cool-downs, the level remembered per game.

#include <cmath>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/dynamic_resolution.h"
#include "video_core/vr_dynamic_resolution.h" // AzaharFrame ui2

using namespace FrameXr::DynamicResolution;

// AzaharFrame fixd: the tests link dynamic_resolution.cpp without the frame loop; the title ID it
// asks for by default (frame_xr.h, immersive.cpp in the app) is none here. The remembered-level
// tests set a LevelStore with their own.
namespace FrameXr {
uint64_t GameTitleId() {
    return 0;
}
} // namespace FrameXr

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
    CHECK(Controller{}.Good(Fine(), 3));
    CHECK_FALSE(Controller{}.Good(Neutral(), 3));
    // AzaharFrame flash3: room is the next factor's predicted cost, the time now times its pixels
    // over this factor's, under three quarters of the bad line (5.6 ms at 120 Hz). Session 573's
    // MK7 at 3x: 3.0 ms predicts 5.3 ms at 4x (room), 4.5 ms (its median) 8.0 ms (none).
    CHECK(std::abs(Controller{}.PredictUp(3.0, 3) - 3.0 * 16.0 / 9.0) < 1e-9);
    CHECK(Controller{}.Good({1.0, 120, kPeriod, 60, 60, 3.0, true}, 3));
    CHECK_FALSE(Controller{}.Good({1.0, 120, kPeriod, 60, 60, 3.2, true}, 3));
    CHECK_FALSE(Controller{}.Good({1.0, 120, kPeriod, 60, 60, 4.5, true}, 3));
    // The higher the factor, the smaller the step: 3.5 ms is room at 5x (to 6x: 5.0 ms) and 4x
    // (5.5 ms), not at 2x (7.9 ms).
    CHECK(Controller{}.Good({1.0, 120, kPeriod, 60, 60, 3.5, true}, 5));
    CHECK(Controller{}.Good({1.0, 120, kPeriod, 60, 60, 3.5, true}, 4));
    CHECK_FALSE(Controller{}.Good({1.0, 120, kPeriod, 60, 60, 3.5, true}, 2));
    // 1x to 2x is four times the pixels.
    CHECK(Controller{}.Good({1.0, 120, kPeriod, 60, 60, 1.3, true}, 1));
    CHECK_FALSE(Controller{}.Good({1.0, 120, kPeriod, 60, 60, 1.5, true}, 1));
    CHECK_FALSE(Controller{}.Good({1.0, 120, kPeriod, 60, 60, 1.5, true}, 0)); // as 1x
    CHECK_FALSE(Controller::Bad({1.0, 120, kPeriod, 60, 60, 2.9, true}));
    CHECK_FALSE(Controller::Bad(Neutral()));
    // The game's own rate does not count: a 30 fps game with the headset at full rate is fine.
    Second thirty = Fine();
    thirty.emu_frames = thirty.gpu_frames = 30;
    CHECK(Controller{}.Good(thirty, 3));
    // 90 Hz: 90 frames are all of them.
    CHECK(Controller{}.Good({1.0, 90, 1000.0 / 90.0, 30, 30, 2.0, true}, 3));
    // Too few timed frames say nothing either way.
    CHECK_FALSE(Controller{}.Good({1.0, 120, kPeriod, 60, 4, 1.0, true}, 3));
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
    // ...AzaharFrame fixd: and when that fails too, it waits kRetryGrowth times as long...
    Run(c, GpuBehind(), Controller::kSettle + Controller::kDownAfter);
    REQUIRE(c.Drop() == 1);
    CHECK(c.Blocked(0));
    CHECK(Run(c, Fine(), Controller::kRetryWait * Controller::kRetryGrowth - 30) == 1);
    waited = 0;
    while (c.Drop() != 0 && waited < 100) {
        c.Judge(Fine(), 3);
        waited++;
    }
    CHECK(c.Drop() == 0);
    CHECK(waited <= 30);
    // ...and after the kFailsForGame-th failure not again in this game.
    Run(c, GpuBehind(), Controller::kSettle + Controller::kDownAfter);
    REQUIRE(c.Drop() == 1);
    CHECK(Run(c, Fine(), 5000) == 1);
    CHECK(c.Blocked(0));
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
    Run(c, GpuBehind(), 3 * Controller::kDownAfter + 2 * Controller::kSettle);
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
    // AzaharFrame fixa: Internal Resolution Auto, as this case was written for (the default is 4x
    // now, which would be the floor: no step under 4x).
    auto& factor = Settings::values.resolution_factor;
    const auto was_factor = factor.GetValue();
    factor.SetValue(0);
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
    factor.SetValue(was_factor);
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
    // kDownAfter in a row still step it down.
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
        // A GPU whose cost jumps past the pixels' prediction at 6x (5x has room by the pixels,
        // 6x is over the budget). flash3 tried 6x again after kRetryWait and once more (5
        // changes). AzaharFrame fixd: the first step down's run was the GPU behind (an upper
        // bound only), so 6x is tried once; that step up measures 6x against 5x, and it stays at
        // 5x.
        const double jump[] = {0.0, 1.0, 1.5, 2.0, 2.5, 3.5, 10.0};
        Controller c;
        const auto drawn = Model(c, 6, jump, 2000);
        CHECK(drawn.back() == 5);
        CHECK(Changes(drawn) == 3); // 6 -> 5, the one try: 5 -> 6 -> 5
        CHECK(c.Ratio(5) > Controller::PixelRatio(5));
        CHECK_FALSE(c.Good({1.0, 120, kPeriod, 60, 60, 3.5, true, 5}, 5));
    }
    {
        // The same from 5x, where the pixels say there is room: one try, then the measured
        // ratio keeps it at 5x (no second try, no bouncing).
        const double jump[] = {0.0, 1.0, 1.5, 2.0, 2.5, 3.5, 10.0};
        Controller c;
        const double heavy[] = {0.0, 9, 9, 9, 9, 9, 9};
        Model(c, 6, heavy, Controller::kDownAfter); // to 5x, measured at the bad line only
        REQUIRE(6 - c.Drop() == 5);
        const auto drawn = Model(c, 6, jump, 3000);
        int ups = 0;
        for (std::size_t i = 1; i < drawn.size(); i++) {
            ups += drawn[i] > drawn[i - 1];
        }
        CHECK(ups <= 1);
        CHECK(drawn.back() == 5);
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

// AzaharFrame flash3: the floor. The Internal Resolution setting itself (kFloorAtSetting, the
// owner's choice): only the immersive modes' two extra steps come off. 1.1.1's rule (false) went
// down to 1x, under the setting in the immersive modes.
TEST_CASE("Dynamic Resolution: the floor", "[frame_xr]") {
    // 1.1.1's rule.
    CHECK(MaxDrop(6, 4, false) == 5); // immersive over a 4x setting: down to 1x
    CHECK(MaxDrop(4, 4, false) == 3);
    CHECK(MaxDrop(6, 0, false) == 5); // Auto
    CHECK(MaxDrop(3, 1, false) == 0); // the setting at 1x: nothing to do (the row is locked)
    CHECK(MaxDrop(1, 0, false) == 0);
    CHECK(MaxDrop(0, 4, false) == 0); // the renderer's factor not known yet
    // The rule now: never under the setting; only the immersive modes' steps come off.
    CHECK(MaxDrop(6, 4, true) == 2);
    CHECK(MaxDrop(4, 4, true) == 0); // outside immersive it has nothing to do
    CHECK(MaxDrop(6, 0, true) == 5); // Auto: no setting to stop at
    CHECK(MaxDrop(3, 1, true) == 0);
    CHECK(MaxDrop(8, 7, true) == 1); // immersive capped at 8x
    CHECK(FloorFactor(6, 4, true) == 4);
    CHECK(FloorFactor(6, 4, false) == 1);
    // The default is the setting.
    CHECK(kFloorAtSetting);
    CHECK(MaxDrop(6, 4) == 2);
    CHECK(FloorFactor(6, 4) == 4);
}

// AzaharFrame flash3: session 573's case through the frame loop's side: Internal Resolution 4x,
// immersive (the renderer draws 6x), the GPU far behind: 6x, 5x, 4x and no further.
TEST_CASE("Dynamic Resolution: never under the Internal Resolution setting", "[frame_xr]") {
    namespace V = VideoCore::DynamicResolution;
    auto& setting = Settings::values.vr_dynamic_resolution;
    auto& factor = Settings::values.resolution_factor;
    const bool was = setting.GetValue();
    const auto was_factor = factor.GetValue();
    setting.SetValue(true);
    factor.SetValue(4);
    V::Apply(6);
    Install();
    for (int i = 0; i < 200; i++) {
        NoteSecond(GpuBehind());
    }
    CHECK(Drop() == 2);
    CHECK(V::Apply(6) == 4);
    // Outside immersive the renderer's factor is the setting: nothing to take off.
    Reset();
    V::Apply(4);
    for (int i = 0; i < 200; i++) {
        NoteSecond(GpuBehind());
    }
    CHECK(Drop() == 0);
    CHECK(V::Apply(4) == 4);
    Uninstall();
    setting.SetValue(was);
    factor.SetValue(was_factor);
    V::g_base = 0;
}

// AzaharFrame fixd (session 651, the owner: it steps down too quickly): five bad seconds in a row,
// not three.
TEST_CASE("Dynamic Resolution: slower down", "[frame_xr][fixd]") {
    CHECK(Controller::kDownAfter == 5);
    Controller c;
    CHECK(Run(c, GpuBehind(), 4) == 0);
    CHECK(c.Judge(GpuBehind(), 3) == 1);
    CHECK(c.LastStep() == 1);
    // four bad seconds and an ordinary one, again and again: never
    Controller d;
    for (int i = 0; i < 40; i++) {
        Run(d, GpuBehind(), 4);
        d.Judge(Neutral(), 3);
    }
    CHECK(d.Drop() == 0);
}

// AzaharFrame fixd: the next factor's cost from the ratio measured in this game where it has
// stepped between the two; the pixels before that.
TEST_CASE("Dynamic Resolution: up on the measured cost", "[frame_xr][fixd]") {
    Controller fresh;
    CHECK(fresh.Ratio(3) == Controller::PixelRatio(3)); // nothing measured: the pixels
    CHECK(fresh.MeasuredUp(3) == 0.0);
    CHECK(fresh.Cost(3) == 0.0);
    CHECK(std::abs(fresh.PredictUp(3.0, 3) - 3.0 * 16.0 / 9.0) < 1e-9);
    CHECK(std::abs(Controller::PixelRatio(4) - 25.0 / 16.0) < 1e-9);

    // A heavy scene: 4x at 8.0 ms (over the bad line), 3x at 7.0 ms: down to 3x after five bad
    // seconds; the seconds after the settle measure 3x in the same scene: x1.14.
    const auto at = [](double ms, int f) { return Second{1.0, 120, kPeriod, 60, 60, ms, true, f}; };
    Controller c;
    Run(c, at(8.0, 4), Controller::kDownAfter, 1);
    REQUIRE(c.Drop() == 1);
    Run(c, at(7.0, 3), Controller::kSettle + Controller::kMeasureSeconds, 1);
    CHECK(std::abs(c.MeasuredDown(3) - 8.0 / 7.0) < 1e-6);
    CHECK(std::abs(c.Ratio(3) - 8.0 / 7.0) < 1e-6);
    // A light scene at 3x, 4.0 ms: the pixels predict 7.1 ms at 4x (no room), the measured ratio
    // 4.6 ms (room): it climbs back.
    CHECK_FALSE(fresh.Good(at(4.0, 3), 3));
    CHECK(c.Good(at(4.0, 3), 3));
    // (the hold's first seconds went by in the measurement)
    CHECK(Run(c, at(4.0, 3),
              Controller::kHoldAfterDown - Controller::kMeasureSeconds + Controller::kUpAfter, 1) == 0);
    CHECK(c.LastStep() == -1);

    // A step up measures both ways. MK7: 4x at 3.4 ms (5.3 ms by the pixels: room) up to 5x,
    // which takes 6.0 ms there (under the line): x1.76, more than the pixels' x1.56, so the next
    // time 4x at 3.4 ms has no room (6.0 ms), 3.0 ms still has.
    Controller k;
    k.Start(1);
    Run(k, at(3.4, 4), Controller::kSettle + Controller::kHoldAfterDown + Controller::kUpAfter, 1);
    REQUIRE(k.Drop() == 0);
    REQUIRE(k.LastStep() == -1);
    Run(k, at(6.0, 5), Controller::kSettle + Controller::kMeasureSeconds, 1);
    CHECK(std::abs(k.MeasuredUp(4) - 6.0 / 3.4) < 1e-6);
    CHECK(std::abs(k.Ratio(4) - 6.0 / 3.4) < 1e-6);
    CHECK(fresh.Good(at(3.4, 4), 4));
    CHECK_FALSE(k.Good(at(3.4, 4), 4));
    CHECK(k.Good(at(3.0, 4), 4));
    // ... up to 1.25 times the pixels' (a scene that got heavier under the measurement)
    Controller u;
    u.Start(1);
    Run(u, at(2.0, 4), Controller::kSettle + Controller::kHoldAfterDown + Controller::kUpAfter, 1);
    REQUIRE(u.Drop() == 0);
    Run(u, at(7.0, 5), Controller::kSettle + Controller::kMeasureSeconds, 1);
    CHECK(std::abs(u.MeasuredUp(4) - Controller::kRatioOverPixels * Controller::PixelRatio(4)) < 1e-6);

    // A step down's run is the GPU behind (the cost plus the wait): a scene that got lighter
    // right after it (the race ended) says nothing, it never makes the step up dearer than the
    // pixels say.
    Controller e;
    Run(e, at(9.0, 5), Controller::kDownAfter, 1);
    Run(e, at(1.0, 4), Controller::kSettle + Controller::kMeasureSeconds, 1);
    CHECK(e.MeasuredDown(4) == 0.0);
    CHECK(e.Ratio(4) == Controller::PixelRatio(4));
    CHECK(e.Good(at(2.0, 4), 4)); // a light scene climbs as by the pixels
    CHECK_FALSE(e.Good(at(3.8, 4), 4));
    // ... and it never goes under a step up's own measurement: k measured x1.76 going up, then
    // a step down whose 4x is 5.0 ms against 5x's 7.6 (x1.52 at most).
    Run(k, at(7.6, 5), Controller::kDownAfter, 1);
    REQUIRE(k.Drop() == 1);
    Run(k, at(5.0, 4), Controller::kSettle + Controller::kMeasureSeconds, 1);
    CHECK(std::abs(k.MeasuredDown(4) - 7.6 / 5.0) < 1e-6);
    CHECK(std::abs(k.Ratio(4) - 6.0 / 3.4) < 1e-6);

    // Both at the bad line says nothing: no ratio.
    Controller b;
    Run(b, at(9.0, 5), Controller::kDownAfter, 1);
    Run(b, at(8.0, 4), Controller::kSettle + Controller::kMeasureSeconds, 1);
    CHECK(b.MeasuredDown(4) == 0.0);
    CHECK(b.MeasuredUp(4) == 0.0);
}

// AzaharFrame fixd: the medians per factor. Session 651's Majora's Mask: 6x at 3.4 ms most of the
// time, a heavy stretch steps it down, then 5x at 5.4 ms: by the pixels 7.8 ms at 6x (no room,
// so it stayed at 5x for good), by the medians 5.4 ms (room): it comes back.
TEST_CASE("Dynamic Resolution: up on the medians per factor", "[frame_xr][fixd]") {
    const auto at = [](double ms, int f) { return Second{1.0, 120, kPeriod, 60, 60, ms, true, f}; };
    Controller c;
    Run(c, at(3.4, 6), 60, 2);
    CHECK(c.Cost(6) == static_cast<float>(3.4));
    Run(c, at(9.0, 6), Controller::kDownAfter, 2);
    REQUIRE(c.Drop() == 1);
    CHECK(c.Cost(6) == static_cast<float>(3.4)); // a median: five heavy seconds do not move it
    CHECK(c.MeasuredDown(5) == 0.0);             // 9.0 against 5.4: more than the pixels say
    Controller pixels;
    CHECK_FALSE(pixels.Good(at(5.4, 5), 5));
    // its first seconds at 5x: not known yet (kCostSeconds), then the medians' x1 (never under 1)
    int up_after = 0;
    for (int t = 1; t <= 60 && c.Drop() == 1; t++) {
        if (t == Controller::kSettle + Controller::kCostSeconds - 1) {
            CHECK(c.Cost(5) == 0.0);
        }
        c.Judge(at(5.4, 5), 2);
        up_after = t;
    }
    CHECK(c.Drop() == 0);
    CHECK(c.LastStep() == -1);
    CHECK(up_after == Controller::kSettle + Controller::kCostSeconds - 1 + Controller::kUpAfter);
    CHECK(c.Ratio(5) == Controller::kRatioMin);
    CHECK(c.RatioWhy(5) == "x1.00 by the medians of 5x and 6x");

    // A Load in the same game (session 651 loaded a state 17 s after that step down) starts at
    // the remembered 5x and keeps the medians: it comes back as soon as it may.
    c.Start(1, true);
    CHECK(c.Cost(6) == static_cast<float>(3.4));
    CHECK(c.Ratio(5) == Controller::kRatioMin);
    CHECK(Run(c, at(5.4, 5), Controller::kSettle + Controller::kHoldAfterDown + Controller::kUpAfter - 1, 2) == 1);
    CHECK(c.Judge(at(5.4, 5), 2) == 0);
    // another game starts from nothing
    c.Start(1);
    CHECK(c.Cost(6) == 0.0);
    CHECK(c.Ratio(5) == Controller::PixelRatio(5));

    // The medians never make it dearer than the pixels: 6x heavy all along (session 651's
    // 0004000000054000: 14.8 ms at 6x, 4.1 ms at 5x).
    Controller h;
    Run(h, at(14.8, 6), Controller::kDownAfter, 2);
    Run(h, at(3.0, 5), 30, 2);
    CHECK(h.CostRatio(5) == 0.0); // 6x has five seconds: not known
    CHECK(h.Ratio(5) == Controller::PixelRatio(5));
}

// AzaharFrame fixd: a failed level waits longer each time (no oscillation): kRetryWait, then
// kRetryGrowth times that, then the rest of the game.
TEST_CASE("Dynamic Resolution: a failed level waits longer each time", "[frame_xr][fixd]") {
    CHECK(Controller::kRetryWait == 180);
    CHECK(Controller::kFailWindow == 60);
    // 5x is over the line in this scene, 4x at 2.5 ms; the measured ratio is kept at 1.25 times
    // the pixels' (2.5 ms predicts 4.9 ms: room by the rule), so only the waits keep it from
    // bouncing.
    const auto at = [](double ms, int f) { return Second{1.0, 120, kPeriod, 60, 60, ms, true, f}; };
    Controller c;
    std::vector<int> ups_at;
    int t = 0;
    for (; t < 3000; t++) {
        const int f = 5 - c.Drop();
        const int before = c.Drop();
        c.Judge(at(f == 5 ? 9.0 : 2.5, f), 1);
        if (c.Drop() < before) {
            ups_at.push_back(t);
        }
    }
    REQUIRE(ups_at.size() == 3);
    // the waits between the tries grow: 180 s, then 540 s; after the third failure none
    const int gap1 = ups_at[1] - ups_at[0], gap2 = ups_at[2] - ups_at[1];
    CHECK(gap1 >= Controller::kRetryWait);
    CHECK(gap2 >= Controller::kRetryWait * Controller::kRetryGrowth);
    CHECK(gap2 > gap1 * 2);
    CHECK(c.Drop() == 1);
    CHECK(c.Blocked(0));
}

// AzaharFrame fixd: where a game starts: the level remembered for it, never above the ceiling nor
// under the floor.
TEST_CASE("Dynamic Resolution: the start from a remembered level", "[frame_xr][fixd]") {
    CHECK(StartDrop(6, 4, 2) == 2);
    CHECK(StartDrop(6, 5, 2) == 1);
    CHECK(StartDrop(6, 0, 2) == 0); // none remembered: the ceiling
    CHECK(StartDrop(6, 6, 2) == 0);
    CHECK(StartDrop(6, 7, 2) == 0); // remembered above today's ceiling
    CHECK(StartDrop(6, 3, 2) == 2); // under today's floor (a higher setting now): the floor
    CHECK(StartDrop(4, 3, 0) == 0); // outside the Full modes nothing comes off
    CHECK(StartDrop(0, 4, 0) == 0); // the renderer's factor not known yet
    Controller c;
    c.Start(2);
    CHECK(c.Drop() == 2);
    CHECK(c.LastStep() == 0);
    // its first seconds are not judged (the step's own hitch), and good ones wait a hold
    CHECK(Run(c, GpuBehind(), Controller::kSettle) == 2);
    CHECK(Run(c, Fine(), Controller::kHoldAfterDown + Controller::kUpAfter - 1) == 2);
    CHECK(c.Judge(Fine(), 3) == 1);
    c.Start(0);
    CHECK(c.Drop() == 0);
}

TEST_CASE("Dynamic Resolution: the remembered levels' file", "[frame_xr][fixd]") {
    const std::vector<std::pair<uint64_t, int>> levels{{0x0004000000030800ULL, 4},
                                                       {0x0004000000125600ULL, 5}};
    const std::string text = FormatStore(levels);
    CHECK(text == "0004000000030800 4\n0004000000125600 5\n");
    CHECK(ParseStore(text) == levels);
    // lines this build cannot read are left out; the last line for a game wins
    const auto parsed = ParseStore("# a comment\n0004000000030800 4\nzz 3\n0004000000030800 5\n"
                                   "0000000000000000 4\n0004000000054000 0\n0004000000054000 99\n"
                                   "0004000000054000 4 extra\n000400000003080 4\n\n");
    CHECK(parsed == std::vector<std::pair<uint64_t, int>>{{0x0004000000030800ULL, 5}});
    CHECK(FormatStore({{0, 4}}).empty());
}

// AzaharFrame fixd: through the frame loop's side: a level stepped down to is remembered for the
// game, a Load (the game's screens off) starts there, not at the ceiling, and is no step up; the
// next boot (a new frame loop) reads it back.
TEST_CASE("Dynamic Resolution: the level remembered per game", "[frame_xr][fixd]") {
    namespace V = VideoCore::DynamicResolution;
    auto& setting = Settings::values.vr_dynamic_resolution;
    auto& factor = Settings::values.resolution_factor;
    const bool was = setting.GetValue();
    const auto was_factor = factor.GetValue();
    setting.SetValue(true);
    factor.SetValue(4);
    std::map<uint64_t, int> disk;
    int saves = 0;
    uint64_t title = 0x0004000000030800ULL;
    const auto store = [&] {
        LevelStore s;
        s.load = [&] { return std::vector<std::pair<uint64_t, int>>(disk.begin(), disk.end()); };
        s.save = [&](std::vector<std::pair<uint64_t, int>> levels) {
            disk = std::map<uint64_t, int>(levels.begin(), levels.end());
            saves++;
        };
        s.title = [&] { return title; };
        return s;
    };
    SetLevelStore(store());
    V::Apply(6); // a Full mode over 4x
    Install();
    const auto run = [](const Second& s, int n) {
        for (int i = 0; i < n; i++) {
            NoteSecond(s);
        }
    };
    Second off = Fine();
    off.live = false;
    run(GpuBehind(), Controller::kDownAfter);
    CHECK(Drop() == 1);
    CHECK(RememberedFactor(title) == 5); // stepped down to it: remembered at once
    CHECK(disk[title] == 5);
    run(GpuBehind(), Controller::kSettle + Controller::kDownAfter);
    CHECK(Drop() == 2);
    CHECK(disk[title] == 4);
    // A Load: the screens go off. It starts at 4x again, not at 6x.
    run(off, 3);
    CHECK(Drop() == 2);
    CHECK(V::Apply(6) == 4);
    run(Fine(), 1);
    CHECK(Drop() == 2);
    // Another game with nothing remembered starts at the ceiling.
    title = 0x0004000000054000ULL;
    run(off, 1);
    CHECK(Drop() == 0);
    // A level held kRememberAfter judged seconds is remembered too (6x here).
    run(Neutral(), kRememberAfter + 1);
    CHECK(RememberedFactor(title) == 6);
    Uninstall();

    // The next boot: a new frame loop reads the levels back, and a game already running when VR
    // starts goes to its level at the first second.
    title = 0x0004000000030800ULL;
    SetLevelStore(store());
    CHECK(RememberedFactor(title) == 4);
    Install();
    run(Neutral(), 1);
    CHECK(Drop() == 2);
    // A new Internal Resolution starts from the setting (the player's choice), not the level.
    Reset();
    CHECK(Drop() == 0);
    run(Neutral(), 1);
    CHECK(Drop() == 0);
    Uninstall();

    // Turned off in one game (6x), another game started meanwhile, turned on: the kept copy was
    // the other game's: this game's remembered level (4x).
    title = 0x0004000000054000ULL;
    Install();
    run(Neutral(), 1);
    REQUIRE(Drop() == 0);
    setting.SetValue(false);
    SetEnabled(false);
    run(off, 1);
    title = 0x0004000000030800ULL;
    run(off, 1);
    run(Fine(), 1);
    setting.SetValue(true);
    SetEnabled(true);
    run(Neutral(), 1);
    CHECK(Drop() == 2);
    Uninstall();

    // No title ID (homebrew): nothing remembered, nothing read.
    title = 0;
    const int saved_before = saves;
    Install();
    run(GpuBehind(), Controller::kDownAfter);
    CHECK(Drop() == 1);
    CHECK(saves == saved_before);
    Uninstall();

    SetLevelStore({}); // nothing for the other tests
    setting.SetValue(was);
    factor.SetValue(was_factor);
    V::g_base = 0;
}
