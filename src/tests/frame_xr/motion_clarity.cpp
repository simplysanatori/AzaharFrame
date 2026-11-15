// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash: Motion Clarity's scheduling (frame_xr/motion_clarity.h): which refreshes show
// the picture and which the black backing, at 60 and 30 pictures a second on 120 Hz, through
// stalls, late pictures, the menu and a pause; the override, the setting and the log.
// flash2: the same share of light in the menu, a pause, learning, late pictures and gaps (no
// brightness step), and the layer list through a menu.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
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
    // Learning: 8 pictures (16 refreshes), then the rate is known. flash2: flashed on the beat
    // meanwhile (it was held lit: a bright step at every start).
    std::vector<MC::Why> learn_whys;
    const std::string learn = Run(s, 2, 18, pic, true, true, &learn_whys);
    CHECK(learn == "PBPBPBPBPBPBPBPBPB");
    CHECK(learn_whys[0] == MC::Why::Learning);
    CHECK(learn_whys[16] == MC::Why::Fresh);
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

TEST_CASE("Motion Clarity: a late picture or a stall flashes on, never dark for long",
          "[frame_xr][flash]") {
    MC::Scheduler s;
    uint64_t pic = 0;
    Run(s, 2, 40, pic);
    REQUIRE(s.Fast());
    // One picture late by two refreshes (4 refreshes): lit, dark, dark, shown again one refresh
    // after the next was due; flash2: the next picture, due on the refresh after a lit one, waits
    // one refresh (it made a lit run of 2 before), and the game's beat goes on from it.
    std::string out;
    pic++;
    for (int i = 0; i < 4; i++) {
        out += s.Next({true, true, pic}).show ? 'P' : 'B';
    }
    pic++;
    out += s.Next({true, true, pic}).show ? 'P' : 'B';
    out += s.Next({true, true, pic}).show ? 'P' : 'B';
    out += Run(s, 2, 8, pic);
    CHECK(out == "PBBPBPBPBPBPBP");
    // flash2: a stall of 2 s (a savestate loading, a race starting): the frozen picture on every
    // other refresh after the first late one, as bright as in play, never lit twice in a row.
    pic++;
    std::string stall;
    MC::Why last = MC::Why::Off;
    for (int i = 0; i < 240; i++) {
        const MC::Decision d = s.Next({true, true, pic});
        stall += d.show ? 'P' : 'B';
        if (d.show) {
            last = d.why;
        }
    }
    CHECK(stall.substr(0, 6) == "BPBBPB"); // waits a refresh, lit, waits for the next, late
    CHECK(stall.find("PP") == std::string::npos);
    CHECK(stall.find("BBB") == std::string::npos);
    CHECK(std::count(stall.begin(), stall.end(), 'P') == 119);
    CHECK(last == MC::Why::Late);
    // The stall's long interval is not taken for the game's rate: still fast, and a late picture
    // still waits only one refresh past its time, with the odd dropped frame in the mean.
    CHECK(s.Fast());
    CHECK(s.LateAfter() == 3u);
    Run(s, 2, 16, pic);
    CHECK(s.Fast());
}

TEST_CASE("Motion Clarity: menus and pauses flash on the beat; turning on never hides a new "
          "picture",
          "[frame_xr][flash]") {
    MC::Scheduler s;
    uint64_t pic = 0;
    Run(s, 2, 40, pic);
    std::vector<MC::Why> whys;
    // flash2: the VR menu open (not steady) with the game's pictures still coming: each on one
    // refresh, as in play (it was held lit: 100 %).
    CHECK(Run(s, 2, 10, pic, true, false, &whys) == "PBPBPBPBPB");
    CHECK(whys[0] == MC::Why::Unsteady);
    CHECK(whys[1] == MC::Why::Dark);
    // Paused under the menu (no new pictures, not steady): the paused picture on every other
    // refresh. The menu closes: still the paused picture, on the same beat; the next new one
    // flashes again; the rate is still known.
    std::string paused;
    for (int i = 0; i < 120; i++) {
        paused += s.Next({true, false, pic}).show ? 'P' : 'B';
    }
    std::string expect;
    for (int i = 0; i < 60; i++) {
        expect += "PB";
    }
    CHECK(paused == expect);
    CHECK(s.Next({true, true, pic}).show); // closed: still the paused picture, on the beat
    CHECK(s.Fast());
    pic++;
    CHECK_FALSE(s.Next({true, true, pic}).show); // due right after a lit refresh: waits one
    CHECK(s.Next({true, true, pic}).why == MC::Why::Fresh);
    pic++;
    CHECK_FALSE(s.Next({true, true, pic}).show); // the game's beat goes on from there
    CHECK(s.Next({true, true, pic}).why == MC::Why::Fresh);
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

TEST_CASE("Motion Clarity: a short gap without a picture keeps the rate", "[frame_xr][flash]") {
    // flash2: a Dynamic Resolution step makes the top swapchain anew; it has no image for a
    // refresh or two (picture 0). The rate learnt before is kept and the next picture flashes at
    // once (it held 18 refreshes lit while the rate was learnt again: a bright flash).
    constexpr int64_t kPeriod = 8'333'333; // 120 Hz
    int64_t t = 1'000'000'000;
    uint64_t pic = 0;
    MC::Scheduler s;
    const auto next = [&](uint64_t p) {
        t += kPeriod;
        return s.Next({true, true, p, t});
    };
    for (int i = 0; i < 40; i++) {
        next(i % 2 == 0 ? ++pic : pic);
    }
    REQUIRE(s.Fast());
    // A gap of 3 refreshes, then the new swapchain's pictures: lit, backing, lit, backing...
    for (int i = 0; i < 3; i++) {
        CHECK(next(0).show);
    }
    CHECK(s.Fast());
    CHECK(s.Mean() == 2.0f);
    std::string out;
    for (int i = 0; i < 8; i++) {
        out += next(i % 2 == 0 ? ++pic : pic).show ? 'P' : 'B';
    }
    CHECK(out == "PBPBPBPB");
    // The gap is not taken for an interval: the rate is still 2.
    CHECK(s.Mean() == 2.0f);
    // Up to kGapKeepNs (0.5 s, 60 refreshes at 120 Hz) since the last picture: kept.
    for (int i = 0; i < 59; i++) {
        next(0);
    }
    CHECK(s.Fast());
    CHECK(next(++pic).why == MC::Why::Fresh);
    CHECK_FALSE(next(pic).show);
    // Longer (a game stopped, another starting): learnt anew.
    for (int i = 0; i < 61; i++) {
        next(0);
    }
    CHECK_FALSE(s.Fast());
    CHECK(s.Mean() == 0.0f);
    CHECK(next(++pic).why == MC::Why::Learning);
}

namespace {

/// flash2: a session at 120 Hz through the scheduler and the log, as app.cpp drives them: each
/// refresh's facts, and the decisions as 'P' (lit), 'B' (backing) or '-' (no picture).
struct Session {
    static constexpr int64_t kPeriod = 8'333'333;
    MC::Scheduler s;
    MC::Log log;
    int64_t t = 1'000'000'000;
    uint64_t pic = 0;
    std::string out;

    void Refresh(bool steady, bool new_picture, bool on = true, bool gap = false) {
        if (new_picture) {
            pic++;
        }
        t += kPeriod;
        const MC::Decision d = s.Next({on, steady, gap ? 0 : pic, t, kPeriod});
        log.Note(!d.show ? MC::Why::Dark : d.why);
        out += gap ? '-' : d.show ? 'P' : 'B';
    }
    /// `n` refreshes of a game at 60 pictures a second (`steady`: played, else in the menu).
    void Play(int n, bool steady = true) {
        for (int i = 0; i < n; i++) {
            Refresh(steady, i % 2 == 0);
        }
    }
    /// `n` refreshes of a still picture (paused, a stall).
    void Still(int n, bool steady) {
        for (int i = 0; i < n; i++) {
            Refresh(steady, false);
        }
    }
    /// flash3: played pictures on view these many refreshes each.
    void Intervals(const std::vector<int>& intervals) {
        for (const int n : intervals) {
            for (int i = 0; i < n; i++) {
                Refresh(true, i == 0);
            }
        }
    }
    /// flash3: `n` refreshes without a picture (a swapchain made anew, a game stopping).
    void Gap(int n) {
        for (int i = 0; i < n; i++) {
            Refresh(true, false, true, true);
        }
    }
};

/// flash3: `n` intervals of `every` refreshes.
std::vector<int> Same(int n, int every = 2) {
    return std::vector<int>(static_cast<std::size_t>(n), every);
}

std::vector<int> Join(std::initializer_list<std::vector<int>> parts) {
    std::vector<int> all;
    for (const auto& p : parts) {
        all.insert(all.end(), p.begin(), p.end());
    }
    return all;
}

/// The lit share of each whole second (120 refreshes) of `s` from `from`, gaps left out.
std::vector<float> LitPerSecond(const std::string& s, std::size_t from = 0) {
    std::vector<float> shares;
    for (std::size_t at = from; at + 120 <= s.size(); at += 120) {
        const std::string sec = s.substr(at, 120);
        const auto lit = std::count(sec.begin(), sec.end(), 'P');
        const auto dark = std::count(sec.begin(), sec.end(), 'B');
        shares.push_back(lit + dark == 0 ? 0.0f : static_cast<float>(lit) / (lit + dark));
    }
    return shares;
}

} // namespace

TEST_CASE("Motion Clarity: the VR menu opens and closes at 50 % with no step",
          "[frame_xr][flash]") {
    // flash2: the owner's headset session (report 64): each VR menu went 50 % -> 100 % lit for
    // 1-9 s. Racing, the menu opened (immersive paused: not steady; the game's pictures stop a
    // few refreshes later, paused), 4 s in the menu, closed, racing again; then the Quick Load
    // dialog's menu the same way.
    Session x;
    x.Play(240); // learnt, flashing
    REQUIRE(x.s.Fast());
    const std::size_t from = x.out.size();
    for (int round = 0; round < 2; round++) {
        x.Play(6, false);        // the menu shows, the last pictures of the game
        x.Still(480, false);     // paused under it
        x.Still(3, true);        // closed: the game resumes a few refreshes later
        x.Play(600);             // racing
    }
    const std::string run = x.out.substr(from);
    CHECK(run.find("PP") == std::string::npos);
    CHECK(run.find("BBB") == std::string::npos);
    for (const float share : LitPerSecond(run)) {
        CHECK(share >= 0.45f);
        CHECK(share <= 0.55f);
    }
    CHECK(x.log.Steps() == 0);
    CHECK(x.s.Fast());
    // The log says so: flashing, 50 %, no step, no lit run longer than one refresh.
    const std::string line = x.log.Second(x.s.Mean(), Session::kPeriod, true);
    CHECK(line.find("Motion Clarity flashing") == 0);
    CHECK(line.find("brightness steps 0, longest lit run 1;") != std::string::npos);
}

TEST_CASE("Motion Clarity: the menu at the game's start and the bottom screen in focus",
          "[frame_xr][flash]") {
    // flash2: a game starting (the rate not known yet) and the menu opened before it is
    // learnt; the bottom screen in focus with the game running (not steady, pictures coming):
    // all at the same share, never a lit run.
    Session x;
    x.Play(10);
    x.Still(240, false); // the menu, the rate still not known
    CHECK_FALSE(x.s.Fast());
    x.Play(240);
    CHECK(x.s.Fast());
    x.Play(480, false); // the bottom screen in focus: the game runs, immersive paused
    x.Play(240);
    CHECK(x.out.find("PP") == std::string::npos);
    for (const float share : LitPerSecond(x.out)) {
        CHECK(share >= 0.45f);
        CHECK(share <= 0.55f);
    }
    CHECK(x.log.Steps() == 0);
}

TEST_CASE("Motion Clarity: late pictures make no lit run; a long stall stays at 50 %",
          "[frame_xr][flash]") {
    // flash2: a game that drops or delays a picture now and then (intervals of 1, 3 and 4
    // refreshes among the 2s), then a 3 s stall (a savestate loading, a race starting).
    Session x;
    x.Play(240);
    REQUIRE(x.s.Fast());
    const std::size_t from = x.out.size();
    const int intervals[] = {2, 2, 3, 1, 2, 4, 2, 2, 1, 3, 2, 2, 2, 5, 2, 1, 2, 3};
    for (int k = 0; k < 20; k++) {
        for (const int n : intervals) {
            for (int i = 0; i < n; i++) {
                x.Refresh(true, i == 0);
            }
        }
    }
    const std::string jitter = x.out.substr(from);
    CHECK(jitter.find("PP") == std::string::npos);
    CHECK(jitter.find("BBB") == std::string::npos);
    CHECK(x.s.Fast());
    const std::size_t stall_at = x.out.size();
    x.Still(360, true);
    const std::string stall = x.out.substr(stall_at);
    CHECK(stall.find("PP") == std::string::npos);
    CHECK(stall.find("BBB") == std::string::npos); // never dark longer than a picture's time
    for (const float share : LitPerSecond(stall)) {
        CHECK(share >= 0.45f);
        CHECK(share <= 0.55f);
    }
    x.Play(240);
    CHECK(x.out.substr(stall_at).find("PP") == std::string::npos);
    CHECK(x.log.Steps() == 0);
    CHECK(x.s.Fast());
}

TEST_CASE("Motion Clarity: a 30 a second game stays held, in play and in the menu",
          "[frame_xr][flash]") {
    // flash2: unchanged: lit on every refresh (25 % flashing at 30 Hz would flicker). Its menu
    // is held too, so the menu makes no step; a 60 <-> 30 change still does (the hysteresis).
    Session x;
    for (int i = 0; i < 400; i++) {
        x.Refresh(true, i % 4 == 0);
    }
    REQUIRE_FALSE(x.s.Fast());
    const std::size_t from = x.out.size();
    std::vector<MC::Why> whys;
    for (int i = 0; i < 240; i++) {
        x.Refresh(true, i % 4 == 0);
    }
    x.Still(240, false); // the menu
    for (int i = 0; i < 240; i++) {
        x.Refresh(true, i % 4 == 0);
    }
    CHECK(x.out.substr(from) == std::string(720, 'P'));
    const uint32_t steps = x.log.Steps();
    // 60 a second again: flashing (one step down), then 30 (one step up).
    x.Play(360);
    CHECK(x.s.Fast());
    for (int i = 0; i < 400; i++) {
        x.Refresh(true, i % 4 == 0);
    }
    CHECK_FALSE(x.s.Fast());
    CHECK(x.log.Steps() == steps + 2);
}

TEST_CASE("Motion Clarity: the top screen's layers stay through a menu", "[frame_xr][flash]") {
    // flash2: in effect for the whole of a Full mode, engaged or not, so the backing stays under
    // each eye's screen when the menu opens and immersive pauses; the screens' part of the list
    // (backing, screen per eye: their swapchains) is the same on every refresh, lit or dark.
    // Before, the menu turned it off: the backing went and the list lost two layers.
    CHECK(MC::InEffect(true, false, std::nullopt, /*full_mode=*/true));
    Session x;
    std::vector<std::string> lists;
    const auto refresh = [&](bool engaged, bool new_picture) {
        x.Refresh(engaged, new_picture);
        const bool dark = x.out.back() == 'B';
        const MC::TopLayers tl = MC::TopLayersFor(MC::InEffect(true, false, std::nullopt, true),
                                                  true, true, dark);
        CHECK_FALSE(tl.omit);
        std::string list;
        for (const char* eye : {"L", "R"}) {
            if (tl.backing) {
                list += std::string("backing") + eye + " ";
            }
            list += std::string("top") + eye + " "; // collapsed or not: the same swapchain
        }
        lists.push_back(list);
    };
    for (int i = 0; i < 240; i++) {
        refresh(true, i % 2 == 0);
    }
    for (int i = 0; i < 360; i++) {
        refresh(false, i < 4 && i % 2 == 0); // the menu
    }
    for (int i = 0; i < 240; i++) {
        refresh(true, i % 2 == 0);
    }
    for (const std::string& list : lists) {
        CHECK(list == "backingL topL backingR topR ");
    }
    CHECK(x.out.find('B', 240) < 250);       // dark refreshes in the menu too
    CHECK(x.out.substr(240, 360).find("PP") == std::string::npos);
}

TEST_CASE("Motion Clarity: the override, the setting and the picture ids", "[frame_xr][flash]") {
    CHECK(MC::OverrideFrom(std::nullopt) == std::nullopt);
    CHECK(MC::OverrideFrom(std::string("1")) == true);
    CHECK(MC::OverrideFrom(std::string("0")) == false);
    CHECK(MC::OverrideFrom(std::string("")) == std::nullopt);
    // Each mode's own setting (fixa: the Full modes' in them, flash2: engaged or paused for the
    // menu; the Off and Wide modes' in those); the switch's 1 in every mode, 0 never.
    CHECK(MC::InEffect(true, false, std::nullopt, true));
    CHECK_FALSE(MC::InEffect(true, false, std::nullopt, false));
    CHECK_FALSE(MC::InEffect(false, true, std::nullopt, true));
    CHECK(MC::InEffect(false, true, std::nullopt, false));
    CHECK(MC::InEffect(false, false, true, false));
    CHECK_FALSE(MC::InEffect(true, true, false, true));
    CHECK(MC::SlowFrom(std::string("twice")) == MC::Slow::Twice);
    CHECK(MC::SlowFrom(std::string("hold")) == MC::Slow::Hold);
    CHECK(MC::SlowFrom(std::nullopt) == MC::Slow::Hold);
    // Two releases of one game picture are one picture; without the swap count each release is.
    CHECK(MC::PictureId(5, 10) == MC::PictureId(5, 11));
    CHECK(MC::PictureId(0, 10) != MC::PictureId(0, 11));
    CHECK(MC::PictureId(5, 10) != MC::PictureId(0, 10));
    CHECK(MC::PictureId(0, 0) == 0u);
}

// AzaharFrame projdef; fixa: two independent settings, each read alone, both off by default; the
// development builds' master key (vr_motion_clarity) is no longer a setting at all, so it is never
// written: older builds keep reading what their files have.
TEST_CASE("Motion Clarity: the Full modes' and the Off and Wide modes' settings",
          "[frame_xr][projdef][fixa]") {
    CHECK_FALSE(Settings::values.vr_motion_clarity_immersive.GetDefault());
    CHECK(Settings::values.vr_motion_clarity_immersive.GetLabel() ==
          "vr_motion_clarity_immersive");
    CHECK_FALSE(Settings::values.vr_motion_clarity_normal.GetDefault());
    CHECK(Settings::values.vr_motion_clarity_normal.GetLabel() == "vr_motion_clarity_normal");
    // The defaults: in effect nowhere.
    for (const bool full : {false, true}) {
        CHECK_FALSE(MC::InEffect(false, false, std::nullopt, full));
    }
    // Each one in its own modes only.
    CHECK(MC::InEffect(true, false, std::nullopt, true));
    CHECK_FALSE(MC::InEffect(true, false, std::nullopt, false));
    CHECK(MC::InEffect(false, true, std::nullopt, false));
    CHECK_FALSE(MC::InEffect(false, true, std::nullopt, true));
    // AZAHARFRAME_XR_SINGLE_FLASH still overrides.
    CHECK(MC::InEffect(false, false, true, true));
    CHECK(MC::InEffect(false, false, true, false));
    CHECK_FALSE(MC::InEffect(true, true, false, false));
}

// AzaharFrame fixa: only the top screen flashes. In the Off and Wide modes it is held while
// swapped out of the Primary slot (the bottom screen never flashes); the Full modes keep flash2's
// rule (the top screen flashes on the beat while the bottom one is in focus).
TEST_CASE("Motion Clarity: held for a swap in the Off and Wide modes only", "[frame_xr][fixa]") {
    CHECK_FALSE(MC::HeldForSwap(false, false));
    CHECK(MC::HeldForSwap(false, true));
    CHECK_FALSE(MC::HeldForSwap(true, false));
    CHECK_FALSE(MC::HeldForSwap(true, true));
}

// AzaharFrame fixa: Match Bottom Screen Brightness: the bottom screen dimmed to the top one's
// time-averaged light while it flashes, never brightened, and as it is otherwise.
TEST_CASE("Motion Clarity: Match Bottom Screen Brightness", "[frame_xr][fixa]") {
    CHECK_FALSE(Settings::values.vr_motion_clarity_match_bottom.GetDefault());
    CHECK(Settings::values.vr_motion_clarity_match_bottom.GetLabel() ==
          "vr_motion_clarity_match_bottom");
    constexpr int64_t k120 = 8'333'333, k90 = 11'111'111, k60 = 16'666'667;
    const auto near = [](float a, float b) { return std::abs(a - b) < 0.002f; };
    CHECK(near(MC::BottomDim(true, true, k120), 0.5f));
    CHECK(near(MC::BottomDim(true, true, k90), 2.0f / 3.0f));
    CHECK(MC::BottomDim(true, true, k60) == 1.0f); // every refresh lit: nothing to match
    CHECK(near(MC::BottomDim(true, true, 0), 0.5f)); // period not known: Beat's 2
    // Off, or the top screen not flashing: as it is.
    CHECK(MC::BottomDim(false, true, k120) == 1.0f);
    CHECK(MC::BottomDim(true, false, k120) == 1.0f);
    for (const int64_t p : {int64_t{0}, int64_t{1}, k120, k90, k60, int64_t{100'000'000}}) {
        CHECK(MC::BottomDim(true, true, p) <= 1.0f);
        CHECK(MC::BottomDim(true, true, p) > 0.0f);
    }
    // The method: the layer's colour scale when the runtime has it, else the shader.
    CHECK(MC::DimMethodFor(true) == MC::DimMethod::ColorScale);
    CHECK(MC::DimMethodFor(false) == MC::DimMethod::Shader);
    CHECK(std::string(MC::DimMethodName(MC::DimMethod::ColorScale)).find("colour scale") !=
          std::string::npos);
    CHECK(std::string(MC::DimMethodName(MC::DimMethod::Shader)).find("shader") !=
          std::string::npos);
    // The shader's push constant: thousandths, 1 to 999; 0 for none.
    CHECK(MC::DimPermille(1.0f) == 0);
    CHECK(MC::DimPermille(1.5f) == 0);
    CHECK(MC::DimPermille(0.5f) == 500);
    CHECK(MC::DimPermille(0.0f) == 1);
    CHECK(MC::DimPermille(0.9999f) == 999);
}

// AzaharFrame fixa: the Off and Wide modes flash at the game's full rate as the Full modes do,
// keep flash2's steady brightness through the menu, and hold a slow game.
TEST_CASE("Motion Clarity: the Off and Wide modes flash like the Full modes",
          "[frame_xr][fixa]") {
    const bool on = MC::InEffect(false, true, std::nullopt, /*full_mode=*/false);
    REQUIRE(on);
    MC::Scheduler s;
    uint64_t picture = 0;
    Run(s, 2, 40, picture, on, true); // learnt
    CHECK(s.Fast());
    CHECK(Run(s, 2, 20, picture, on, true) == "PBPBPBPBPBPBPBPBPBPB");
    // The menu (not played): on the beat, never two lit in a row.
    const std::string menu = Run(s, 2, 40, picture, on, false);
    CHECK(menu.find("PP") == std::string::npos);
    const auto lit = std::count(menu.begin(), menu.end(), 'P');
    CHECK(lit >= 19);
    CHECK(lit <= 21);
    // A 30 a second game: held, as in the Full modes.
    MC::Scheduler slow;
    uint64_t p2 = 0;
    const std::string held = Run(slow, 4, 200, p2, on, true);
    CHECK(held.substr(100).find('B') == std::string::npos);
}

// AzaharFrame fixa: the backing goes (and the screen is held) while its layers would go over the
// runtime's limit, and comes back only after it has fit for a while.
TEST_CASE("Motion Clarity: the backing's room under the layer limit", "[frame_xr][fixa]") {
    using Room = MC::BackingRoom;
    // The VR menu with both controllers in a Full mode: 16 with the backing, at the limit.
    CHECK(Room::Fits(16, 14, 2));
    // The Off mode has the Secondary screen's layer too: 17 would cut the last layers.
    CHECK_FALSE(Room::Fits(16, 15, 2));
    Room r;
    CHECK(r.Next(true));
    CHECK_FALSE(r.Next(false));
    CHECK(r.TimesOut() == 1);
    for (uint32_t i = 1; i < Room::kBackAfter; i++) {
        CHECK_FALSE(r.Next(true));
    }
    CHECK(r.Next(true)); // back after kBackAfter refreshes that fit
    CHECK_FALSE(r.Next(false));
    CHECK_FALSE(r.Next(true)); // the count starts again
    CHECK(r.TimesOut() == 2);
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

// flash3 (report 70, session 573): after Quick Load the screen was held lit for 19 refreshes
// (2 brightness steps): the rate was learnt again after the load (a gap), and one late picture
// of 11-12 refreshes among the first 8 intervals made the mean 3.1, a "slow game" (Slow::Hold)
// until it left the history. The pacing lines of t=17.8-19.8: on view 1/2/3/4/5/6+ refreshes
// 1/30/0/0/0/1, 0/57/0/1/0/0, 1/57/2/0/0/0. A hitch is not taken for the rate.
TEST_CASE("Motion Clarity: Quick Load's late pictures make no lit run", "[frame_xr][flash]") {
    for (const int late : {6, 9, 11, 12}) {
        for (const int at : {0, 3, 7, 12}) { // the hitch inside learning, or just after
            Session x;
            x.Play(240);
            REQUIRE(x.s.Fast());
            x.Still(60, false); // Quick Load's dialog under the VR menu
            x.Gap(90);          // the screens made anew: the rate is learnt again
            x.Still(214, true); // the savestate's stall (1785 ms) on the first picture
            const std::size_t from = x.out.size();
            std::vector<int> first = Join({{2, 1}, Same(30)});
            first.insert(first.begin() + at, late);
            x.Intervals(Join({first, Same(28), {4}, Same(29), {1}, Same(28), {3, 3}, Same(55)}));
            x.Play(240);
            const std::string run = x.out.substr(from);
            INFO("late " << late << " at " << at);
            CHECK(run.find("PPPPPP") == std::string::npos);
            CHECK(run.find("PPP") == std::string::npos);
            CHECK(x.log.Steps() == 0);
            CHECK(x.s.Fast());
            CHECK(x.s.Mean() <= 2.25f);
            CHECK(x.s.Hitches() >= 2);
            for (const float share : LitPerSecond(run)) {
                CHECK(share <= 0.55f);
            }
        }
    }
}

// flash3: 1.1.1-571's race start ("slow game 33" at t=18-20): stalls and late pictures as the
// countdown starts (pacing at t=15.7-21.8: on view 1/9/0/0/0/1, 9/43/2/1/0/2, 2/28/0/0/0/2, a
// second with no new picture, 2/38/1/0/0/1, 1/53/2/1/0/0), the long ones together, from a known
// rate and while it is learnt.
TEST_CASE("Motion Clarity: a race start's hitches make no lit run", "[frame_xr][flash]") {
    for (const bool learnt : {true, false}) {
        Session x;
        if (learnt) {
            x.Play(240);
            REQUIRE(x.s.Fast());
        } else {
            x.Play(6);
        }
        const std::size_t from = x.out.size();
        x.Intervals(Join({{1}, Same(9), {8}}));
        x.Intervals(Join({{4, 7, 8, 3, 3}, Same(9, 1), Same(43), {6}}));
        x.Intervals(Join({{1, 1}, Same(28), {9, 10}}));
        x.Still(120, true); // the countdown's stall: no new picture for a second
        x.Intervals(Join({{1, 1}, Same(38), {3, 7}}));
        x.Intervals(Join({{1}, Same(53), {3, 3, 4}}));
        x.Play(480);
        const std::string run = x.out.substr(from);
        INFO("learnt " << learnt);
        CHECK(run.find("PPP") == std::string::npos);
        CHECK(x.log.Steps() == 0);
        CHECK(x.s.Fast());
    }
}

// flash3: a game that really runs at 30 a second is still held (Slow::Hold): from 60 after a
// steady run of kSlowRun intervals of 4 +-1 (flashing on the beat until then), and while the
// rate is learnt; a 20 a second game (6 each) the same way.
TEST_CASE("Motion Clarity: a real 30 a second game is still held", "[frame_xr][flash]") {
    Session x;
    x.Play(240);
    REQUIRE(x.s.Fast());
    std::vector<MC::Why> whys;
    // 7 intervals of 4: still the full rate's game, late pictures on the beat.
    const std::size_t from = x.out.size();
    x.Intervals(Same(MC::kSlowRun - 1, 4));
    CHECK(x.s.Fast());
    CHECK(x.out.substr(from).find("PP") == std::string::npos);
    // The 8th: the rate is 30 a second now, held.
    x.Intervals(Same(2, 4));
    CHECK_FALSE(x.s.Fast());
    CHECK(x.s.Mean() == 4.0f);
    const std::size_t held = x.out.size();
    x.Intervals(Same(30, 4));
    CHECK(x.out.substr(held) == std::string(120, 'P'));
    CHECK(x.log.Steps() == 1); // one step, as before (the hysteresis)
    // With the game's own jitter (3 to 5 refreshes).
    Session y;
    y.Play(240);
    y.Intervals({4, 3, 5, 4, 4, 5, 3, 4, 4});
    CHECK_FALSE(y.s.Fast());
    // Learnt from the start, 30 and 20 a second.
    for (const int every : {4, 6}) {
        Session z;
        z.Intervals(Same(12, every));
        CHECK_FALSE(z.s.Fast());
        CHECK(z.s.Mean() == static_cast<float>(every));
        const std::size_t at = z.out.size();
        z.Intervals(Same(10, every));
        CHECK(z.out.substr(at) == std::string(static_cast<std::size_t>(10 * every), 'P'));
    }
    // A run broken by the full rate's pictures is not a slow game.
    Session w;
    w.Play(240);
    for (int k = 0; k < 20; k++) {
        w.Intervals(Join({Same(MC::kSlowRun - 1, 4), {2, 2}}));
    }
    CHECK(w.s.Fast());
    CHECK(w.log.Steps() == 0);
}
