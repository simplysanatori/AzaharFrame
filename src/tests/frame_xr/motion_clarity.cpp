// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash: Motion Clarity's scheduling (frame_xr/motion_clarity.h): which refreshes show
// the picture and which the black backing, at 60 and 30 pictures a second on 120 Hz, through
// stalls, late pictures, the menu and a pause; the override, the setting and the log.

#include <cstdint>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/motion_clarity.h"

namespace {

namespace MC = FrameXr::MotionClarity;

/// Runs `refreshes` refreshes of a game whose pictures each stay `every` refreshes (the picture id
/// counts up from `first`), and returns the decisions as a string: 'P' shown, 'B' backing.
std::string Run(MC::Scheduler& s, uint32_t every, int refreshes, uint64_t& picture,
                bool on = true, bool steady = true, std::vector<MC::Why>* whys = nullptr) {
    std::string out;
    for (int i = 0; i < refreshes; i++) {
        if (i % static_cast<int>(every) == 0) {
            picture++;
        }
        const MC::Decision d = s.Next({on, steady, picture});
        out += d.show ? 'P' : 'B';
        if (whys) {
            whys->push_back(d.why);
        }
    }
    return out;
}

} // namespace

TEST_CASE("Motion Clarity: 60 pictures on 120 Hz, each on one refresh", "[frame_xr][flash]") {
    MC::Scheduler s;
    uint64_t pic = 0;
    // Learning: 8 pictures (16 refreshes) held, then the rate is known.
    const std::string learn = Run(s, 2, 18, pic);
    CHECK(learn.substr(0, 17) == std::string(17, 'P'));
    CHECK(s.Fast());
    CHECK(s.Mean() == 2.0f);
    CHECK(s.LateAfter() == 3u);
    // From then on: lit, backing, lit, backing...
    std::vector<MC::Why> whys;
    const std::string steady = Run(s, 2, 40, pic, true, true, &whys);
    std::string expect;
    for (int i = 0; i < 20; i++) {
        expect += "PB";
    }
    CHECK(learn.substr(16) == "PB");
    CHECK(steady == expect);
    CHECK(whys[0] == MC::Why::Fresh);
    CHECK(whys[1] == MC::Why::Dark);
    CHECK(MC::LitShare(20, 20) == 0.5f);
}

TEST_CASE("Motion Clarity: a 30 a second game is held, or shown twice when asked",
          "[frame_xr][flash]") {
    MC::Scheduler s;
    uint64_t pic = 0;
    Run(s, 4, 40, pic);
    CHECK_FALSE(s.Fast());
    CHECK(s.Mean() == 4.0f);
    std::vector<MC::Why> whys;
    CHECK(Run(s, 4, 16, pic, true, true, &whys) == std::string(16, 'P')); // Slow::Hold
    CHECK(whys.back() == MC::Why::Slow);
    s.SetSlow(MC::Slow::Twice);
    CHECK(Run(s, 4, 16, pic) == "PPBBPPBBPPBBPPBB");
    // 20 a second (6 refreshes each) is past kTwiceMax: held even with Twice.
    MC::Scheduler t(MC::Slow::Twice);
    pic = 100;
    Run(t, 6, 60, pic);
    CHECK(Run(t, 6, 12, pic) == std::string(12, 'P'));
}

TEST_CASE("Motion Clarity: the rate changes with hysteresis", "[frame_xr][flash]") {
    MC::Scheduler s;
    uint64_t pic = 0;
    Run(s, 2, 40, pic);
    REQUIRE(s.Fast());
    // A game that drops to 40 a second (3 refreshes each): mean 3.0, still fast (<= kFastLeave).
    Run(s, 3, 48, pic);
    CHECK(s.Mean() == 3.0f);
    CHECK(s.Fast());
    // 30 a second: leaves.
    Run(s, 4, 64, pic);
    CHECK_FALSE(s.Fast());
    // Back at 40 a second: does not enter again (> kFastEnter).
    Run(s, 3, 48, pic);
    CHECK_FALSE(s.Fast());
    // At 60: enters.
    Run(s, 2, 32, pic);
    CHECK(s.Fast());
}

TEST_CASE("Motion Clarity: a late picture or a stall is held, never dark for long",
          "[frame_xr][flash]") {
    MC::Scheduler s;
    uint64_t pic = 0;
    Run(s, 2, 40, pic);
    REQUIRE(s.Fast());
    // One picture late by two refreshes (4 refreshes): lit, dark, dark, held again, then the next.
    std::string out;
    pic++;
    for (int i = 0; i < 4; i++) {
        out += s.Next({true, true, pic}).show ? 'P' : 'B';
    }
    pic++;
    out += s.Next({true, true, pic}).show ? 'P' : 'B';
    CHECK(out == "PBBPP");
    // A stall of 2 s: held all the way after the third refresh, and the decision says why.
    pic++;
    std::string stall;
    MC::Why last = MC::Why::Off;
    for (int i = 0; i < 240; i++) {
        const MC::Decision d = s.Next({true, true, pic});
        stall += d.show ? 'P' : 'B';
        last = d.why;
    }
    CHECK(stall.substr(0, 3) == "PBB");
    CHECK(stall.substr(3) == std::string(237, 'P'));
    CHECK(last == MC::Why::Late);
    // The stall's long interval is not taken for the game's rate: still fast, and a late picture
    // still waits only one refresh past its time, with the odd dropped frame in the mean.
    CHECK(s.Fast());
    CHECK(s.LateAfter() == 3u);
    Run(s, 2, 16, pic);
    CHECK(s.Fast());
}

TEST_CASE("Motion Clarity: menus, pauses and off are held; turning on never hides a new picture",
          "[frame_xr][flash]") {
    MC::Scheduler s;
    uint64_t pic = 0;
    Run(s, 2, 40, pic);
    std::vector<MC::Why> whys;
    // The VR menu open (not steady): every refresh shows the picture.
    CHECK(Run(s, 2, 10, pic, true, false, &whys) == std::string(10, 'P'));
    CHECK(whys.back() == MC::Why::Unsteady);
    // Paused under the menu (no new pictures, not steady), then the menu closes: the old picture
    // is held (late), and the next new one flashes again; the rate is still known.
    for (int i = 0; i < 120; i++) {
        CHECK(s.Next({true, false, pic}).show);
    }
    CHECK(s.Next({true, true, pic}).show); // closed: still the paused picture, held
    CHECK(s.Fast());
    pic++;
    CHECK(s.Next({true, true, pic}).show);
    CHECK_FALSE(s.Next({true, true, pic}).show);
    // Off: held, but followed, so turning on at the second refresh of a picture shows the backing
    // and the first refresh of the next picture shows it.
    pic++;
    CHECK(s.Next({false, true, pic}).why == MC::Why::Off);
    CHECK_FALSE(s.Next({true, true, pic}).show);
    pic++;
    CHECK(s.Next({true, true, pic}).why == MC::Why::Fresh);
    // No picture (the game stopped): learns anew.
    CHECK(s.Next({true, true, 0}).show);
    CHECK_FALSE(s.Fast());
    CHECK(s.Mean() == 0.0f);
}

TEST_CASE("Motion Clarity: the override, the setting and the picture ids", "[frame_xr][flash]") {
    CHECK(MC::OverrideFrom(std::nullopt) == std::nullopt);
    CHECK(MC::OverrideFrom(std::string("1")) == true);
    CHECK(MC::OverrideFrom(std::string("0")) == false);
    CHECK(MC::OverrideFrom(std::string("")) == std::nullopt);
    // The setting only in the Full modes (head-driven); the switch's 1 in every mode, 0 never.
    CHECK(MC::InEffect(true, std::nullopt, true));
    CHECK_FALSE(MC::InEffect(true, std::nullopt, false));
    CHECK_FALSE(MC::InEffect(false, std::nullopt, true));
    CHECK(MC::InEffect(false, true, false));
    CHECK_FALSE(MC::InEffect(true, false, true));
    CHECK(MC::SlowFrom(std::string("twice")) == MC::Slow::Twice);
    CHECK(MC::SlowFrom(std::string("hold")) == MC::Slow::Hold);
    CHECK(MC::SlowFrom(std::nullopt) == MC::Slow::Hold);
    // Two releases of one game picture are one picture; without the swap count each release is.
    CHECK(MC::PictureId(5, 10) == MC::PictureId(5, 11));
    CHECK(MC::PictureId(0, 10) != MC::PictureId(0, 11));
    CHECK(MC::PictureId(5, 10) != MC::PictureId(0, 10));
    CHECK(MC::PictureId(0, 0) == 0u);
    // On by default, per game and global, a new key.
    CHECK(Settings::values.vr_motion_clarity.GetDefault());
    CHECK(Settings::values.vr_motion_clarity.GetLabel() == "vr_motion_clarity");
}

TEST_CASE("Motion Clarity: the 30 a second game through the release serials", "[frame_xr][flash]") {
    // A 30 a second game releases each picture twice (60 releases a second): two serials, one
    // game picture. With the swap count both releases are one picture, so it is learnt as 4
    // refreshes each and held; by serial alone it would flash every 2 refreshes (a 30 Hz picture
    // shown twice, 16.7 ms apart).
    MC::Scheduler s;
    uint64_t serial = 0, swaps = 0;
    for (int i = 0; i < 64; i++) {
        if (i % 2 == 0) {
            serial++;
        }
        if (i % 4 == 0) {
            swaps++;
        }
        s.Next({true, true, MC::PictureId(swaps + 1, serial)});
    }
    CHECK(s.Mean() == 4.0f);
    CHECK_FALSE(s.Fast());
}

TEST_CASE("Motion Clarity: the log line", "[frame_xr][flash]") {
    MC::Log log;
    // Off: nothing.
    for (int i = 0; i < 120; i++) {
        log.Note(MC::Why::Off);
    }
    CHECK(log.Second(0.0f, 8'333'333, true).empty());
    // Flashing: a line at once (the state changed).
    for (int i = 0; i < 60; i++) {
        log.Note(MC::Why::Fresh);
        log.Note(MC::Why::Dark);
    }
    const std::string line = log.Second(2.0f, 8'333'333, true);
    CHECK(line.rfind("Motion Clarity flashing: a game picture every 2.00 refreshes (60.0 a second "
                     "at 120 Hz); over 1 s, lit 60 of 120 refreshes (50 %), black backing 60;",
                     0) == 0);
    // The same state: quiet until kLogSeconds have passed.
    for (uint32_t sec = 1; sec < MC::Log::kLogSeconds; sec++) {
        log.Note(MC::Why::Fresh);
        log.Note(MC::Why::Dark);
        CHECK(log.Second(2.0f, 8'333'333, true).empty());
    }
    log.Note(MC::Why::Fresh);
    log.Note(MC::Why::Dark);
    CHECK_FALSE(log.Second(2.0f, 8'333'333, true).empty());
    // The menu opens: holding, at once.
    log.Note(MC::Why::Unsteady);
    CHECK(log.Second(2.0f, 8'333'333, true).rfind("Motion Clarity holding", 0) == 0);
}

TEST_CASE("Motion Clarity: the top screen's layers keep their shape on every refresh",
          "[frame_xr][flash]") {
    // In effect, with the backing: a backing under the screen on lit and dark refreshes alike; a
    // dark refresh only collapses the screen. The list's shape (and every overlay's texture) is
    // the same on both.
    const MC::TopLayers lit = MC::TopLayersFor(true, true, true, false);
    const MC::TopLayers dark = MC::TopLayersFor(true, true, true, true);
    CHECK(lit.backing);
    CHECK(dark.backing);
    CHECK_FALSE(lit.collapse);
    CHECK(dark.collapse);
    CHECK_FALSE(lit.omit);
    CHECK_FALSE(dark.omit);
    // Its image not there yet: nothing hidden (no backing to show), held.
    const MC::TopLayers early = MC::TopLayersFor(true, true, false, true);
    CHECK_FALSE(early.backing);
    CHECK_FALSE(early.collapse);
    CHECK_FALSE(early.omit);
    // AZAHARFRAME_XR_FLASH_BACKING=0: the bare switch, the layer left out.
    const MC::TopLayers bare = MC::TopLayersFor(true, false, true, true);
    CHECK(bare.omit);
    CHECK_FALSE(bare.backing);
    // Off: nothing at all, whatever the rest.
    const MC::TopLayers off = MC::TopLayersFor(false, true, true, true);
    CHECK_FALSE(off.backing);
    CHECK_FALSE(off.collapse);
    CHECK_FALSE(off.omit);
    // Collapsed: under a millimetre for a 25 m immersive screen, never zero.
    CHECK(25.0f * MC::kCollapse < 0.001f);
    CHECK(MC::kCollapse > 0.0f);
}
