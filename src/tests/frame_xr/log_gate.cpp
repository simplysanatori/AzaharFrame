// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame integ8: the Full modes' per-second stereo and world-pose lines are written on a
// meaningful change, else as a summary at most every 10 s.

#include <catch2/catch_test_macros.hpp>
#include "common/frame_log_gate.h"
#include "video_core/vr_immersive.h"

using Common::FrameLog::ChangeGate;
using Common::FrameLog::Moved;
using Say = ChangeGate::Say;

TEST_CASE("Log gate: the first line, a change at once, else every 10 s", "[frame_xr][integ8]") {
    ChangeGate g;
    CHECK(g.Update(1000, false) == Say::First);
    // An hour of seconds with nothing changing: one line every 10 s, never more.
    int lines = 0;
    for (int64_t t = 2000; t <= 3'601'000; t += 1000) {
        const Say s = g.Update(t, false);
        if (s != Say::Nothing) {
            CHECK(s == Say::Summary);
            ++lines;
        }
    }
    CHECK(lines == 360);
    // A change is said at once, and the summary waits 10 s from it.
    CHECK(g.Update(3'602'000, true) == Say::Changed);
    for (int64_t t = 3'603'000; t < 3'612'000; t += 1000) {
        CHECK(g.Update(t, false) == Say::Nothing);
    }
    CHECK(g.Update(3'612'000, false) == Say::Summary);
    g.Reset();
    CHECK(g.Update(3'613'000, false) == Say::First);
}

TEST_CASE("Log gate: what counts as moved", "[frame_xr][integ8]") {
    // Unknown (below 0) on one side only: moved; on both: not.
    CHECK(Moved(-1.0f, 0.0f, 0.1f, 0.0f));
    CHECK(Moved(0.3f, -1.0f, 0.1f, 0.0f));
    CHECK_FALSE(Moved(-1.0f, -1.0f, 0.1f, 0.0f));
    // Both limits must be passed.
    CHECK_FALSE(Moved(1.0f, 1.05f, 0.1f, 0.0f));
    CHECK(Moved(1.0f, 1.2f, 0.1f, 0.0f));
    CHECK_FALSE(Moved(0.001f, 0.002f, 0.1f, 0.005f)); // twice as much, but tiny
    CHECK(Moved(2.0f, 2.2f, 0.0f, 0.1f));
    CHECK_FALSE(Moved(2.0f, 2.05f, 0.0f, 0.1f));
}

TEST_CASE("Log gate: the stereo and world-pose lines' changes", "[frame_xr][integ8]") {
    VideoCore::Immersive::FrameTraceLog lg;
    lg.said_main = 2;
    lg.said_3d = false;
    lg.said_far = 0.0344f;
    lg.said_per = 0.0045f;
    lg.said_slider = 0.08f;
    // Noise of a second in play: no line.
    CHECK_FALSE(lg.StereoChanged(2, false, 0.0350f, 0.0046f, 0.081f));
    // The game's eyes in most swaps (a menu: its world drawn once), the 3D format, far scenery
    // lost or moved a lot, the measure, the slider shown: a line at once.
    CHECK(lg.StereoChanged(1, false, 0.0344f, 0.0045f, 0.08f));
    CHECK(lg.StereoChanged(2, true, 0.0344f, 0.0045f, 0.08f));
    CHECK(lg.StereoChanged(2, false, -1.0f, 0.0045f, 0.08f));
    CHECK(lg.StereoChanged(2, false, 0.0200f, 0.0045f, 0.08f));
    CHECK(lg.StereoChanged(2, false, 0.0344f, -1.0f, 0.08f));
    CHECK(lg.StereoChanged(2, false, 0.0344f, 0.0060f, 0.08f));
    CHECK(lg.StereoChanged(2, false, 0.0344f, 0.0045f, 0.30f));

    lg.said_earlier = false;
    lg.said_without_world = false;
    lg.said_follows = true;
    CHECK_FALSE(lg.WorldChanged(false, false, true));
    CHECK(lg.WorldChanged(true, false, true));
    CHECK(lg.WorldChanged(false, true, true));
    CHECK(lg.WorldChanged(false, false, false));
}
