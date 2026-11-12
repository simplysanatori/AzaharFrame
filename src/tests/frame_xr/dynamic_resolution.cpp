// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: Dynamic Resolution's decisions (frame_xr/dynamic_resolution.h): only when
// the GPU is the limit, one step at a time, only after seconds in a row, never up and down.

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
    CHECK(Controller::Good(Fine()));
    CHECK_FALSE(Controller::Good(Neutral()));
    CHECK_FALSE(Controller::Bad(Neutral()));
    // The game's own rate does not count: a 30 fps game with the headset at full rate is fine.
    Second thirty = Fine();
    thirty.emu_frames = thirty.gpu_frames = 30;
    CHECK(Controller::Good(thirty));
    // 90 Hz: 90 frames are all of them.
    CHECK(Controller::Good({1.0, 90, 1000.0 / 90.0, 30, 30, 2.0, true}));
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
    Run(c, Fine(), Controller::kSettle);
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
    Run(c, Fine(), Controller::kSettle + Controller::kUpAfter); // back to 0
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
