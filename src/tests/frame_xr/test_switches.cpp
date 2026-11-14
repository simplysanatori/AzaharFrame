// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame integ9: the test-switch file read again while running (common/frame_test_switches.h),
// what applies live, the log and the toast for a change, and the values of the switches made for
// the headset A/B (frame_xr/test_switches.h).

#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/frame_test_switches.h"
#include "frame_xr/test_switches.h"

using Catch::Approx;
namespace FTS = Common::FrameTestSwitches;
namespace TS = FrameXr::TestSwitches;

namespace {

// The environment the LiveFile sees in these tests (never the process's own).
std::map<std::string, std::string>& FakeEnv() {
    static std::map<std::string, std::string> env;
    return env;
}
const char* GetFakeEnv(const char* name) {
    const auto it = FakeEnv().find(name);
    return it == FakeEnv().end() ? nullptr : it->second.c_str();
}

/// A switch file in the working directory (the build tree), written with a given mtime so a
/// rewrite within the file system's time step is still seen as a change.
struct SwitchFile {
    std::string path = "frame_test_switches_live_test.txt";
    long long tick = 1'700'000'000;
    void Write(const std::string& text) {
        {
            std::ofstream out(path, std::ios::trunc);
            out << text;
        }
        const timespec times[2] = {{tick, 0}, {tick, 0}};
        ++tick;
        ::utimensat(AT_FDCWD, path.c_str(), times, 0);
    }
    void Remove() {
        std::remove(path.c_str());
    }
    ~SwitchFile() {
        Remove();
    }
};

const FTS::Change* Find(const std::vector<FTS::Change>& changes, const std::string& name) {
    for (const FTS::Change& c : changes) {
        if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("Test switches: the file's lines", "[frame_xr][test_switches]") {
    const FTS::Values v = FTS::ParseText("# a comment\n"
                                         "AZAHARFRAME_IMMERSIVE_TAG_OFFSET = -1  # earlier\n"
                                         "  AZAHARFRAME_IMMERSIVE_POSITION_SCALE=0.5\n"
                                         "AZAHARFRAME_XR_SINGLE_FLASH=0\n"
                                         "AZAHARFRAME_XR_SINGLE_FLASH=1\n" // the last one wins
                                         "HOME=/nowhere\n"                 // not the app's
                                         "AZAHARFRAME_BAD NAME=1\n"
                                         "no equals sign\n"
                                         "\r\n");
    REQUIRE(v.size() == 3);
    CHECK(v.at("AZAHARFRAME_IMMERSIVE_TAG_OFFSET") == "-1");
    CHECK(v.at("AZAHARFRAME_IMMERSIVE_POSITION_SCALE") == "0.5");
    CHECK(v.at("AZAHARFRAME_XR_SINGLE_FLASH") == "1");

    const auto d = FTS::Diff({{"AZAHARFRAME_A", "1"}, {"AZAHARFRAME_B", "2"}},
                             {{"AZAHARFRAME_A", "1"}, {"AZAHARFRAME_B", "3"}, {"AZAHARFRAME_C", "4"}});
    REQUIRE(d.size() == 2);
    CHECK(Find(d, "AZAHARFRAME_B")->value == "3");
    CHECK(Find(d, "AZAHARFRAME_C")->value == "4");
    const auto gone = FTS::Diff({{"AZAHARFRAME_A", "1"}}, {});
    REQUIRE(gone.size() == 1);
    CHECK(gone[0].removed);
}

TEST_CASE("Test switches: the values of the A/B switches", "[frame_xr][test_switches]") {
    CHECK(TS::TagOffsetFrom(std::nullopt) == 0);
    CHECK(TS::TagOffsetFrom(std::string("-1")) == -1);
    CHECK(TS::TagOffsetFrom(std::string("0")) == 0);
    CHECK(TS::TagOffsetFrom(std::string("+1")) == 1);
    CHECK(TS::TagOffsetFrom(std::string("1")) == 1);
    CHECK(TS::TagOffsetFrom(std::string("2")) == 0);
    CHECK(TS::TagOffsetFrom(std::string("x")) == 0);

    CHECK(TS::PositionScaleFrom(std::nullopt) == 1.0f);
    CHECK(TS::PositionScaleFrom(std::string("")) == 1.0f);
    CHECK(TS::PositionScaleFrom(std::string("abc")) == 1.0f);
    CHECK(TS::PositionScaleFrom(std::string("nan")) == 1.0f);
    CHECK(TS::PositionScaleFrom(std::string("0")) == 0.0f);
    CHECK(TS::PositionScaleFrom(std::string("0.5")) == Approx(0.5f));
    CHECK(TS::PositionScaleFrom(std::string("2")) == Approx(2.0f));
    CHECK(TS::PositionScaleFrom(std::string("-3")) == 0.0f);
    CHECK(TS::PositionScaleFrom(std::string("50")) == TS::kPositionScaleMax);

    CHECK(TS::OnFrom(std::string("1")));
    CHECK_FALSE(TS::OnFrom(std::string("0")));
    CHECK_FALSE(TS::OnFrom(std::string("")));
    CHECK_FALSE(TS::OnFrom(std::nullopt));

    // AzaharFrame kiugyro: C-Stick at Edge is shelved: its stored value (an older build's, or the
    // earlier Kid Icarus preset's) acts only with the switch, which never turns it on alone.
    CHECK_FALSE(TS::CStickEdgeOn(std::nullopt, true));
    CHECK_FALSE(TS::CStickEdgeOn(std::string("0"), true));
    CHECK(TS::CStickEdgeOn(std::string("1"), true));
    CHECK_FALSE(TS::CStickEdgeOn(std::string("1"), false));
    CHECK(TS::ApplyFor(TS::kCStickEdgeSwitch) == TS::Apply::Live);

    // AzaharFrame heldquad: HOLD is warp unless it says rigid.
    CHECK(TS::HoldWarpFrom(std::nullopt));
    CHECK(TS::HoldWarpFrom(std::string("warp")));
    CHECK(TS::HoldWarpFrom(std::string("")));
    CHECK(TS::HoldWarpFrom(std::string("x")));
    CHECK_FALSE(TS::HoldWarpFrom(std::string("rigid")));
}

TEST_CASE("Test switches: what applies live, the log and the toast", "[frame_xr][test_switches]") {
    for (const char* live : {"AZAHARFRAME_IMMERSIVE_TAG_OFFSET", "AZAHARFRAME_IMMERSIVE_POSITION_SCALE",
                             "AZAHARFRAME_XR_SINGLE_FLASH", "AZAHARFRAME_IMMERSIVE_ANCHOR_DEPTH",
                             "AZAHARFRAME_IMMERSIVE_WORLD_TAG", "AZAHARFRAME_XR_REFRESH_RATE",
                             "AZAHARFRAME_IMMERSIVE_HOLD", "AZAHARFRAME_XR_IMAGE_LAG",
                             // AzaharFrame projscreen
                             "AZAHARFRAME_XR_SCREENS_IN_PROJECTION", "AZAHARFRAME_XR_PROJ_SCALE",
                             "AZAHARFRAME_XR_PROJ_MARKS"}) {
        CHECK(TS::ApplyFor(live) == TS::Apply::Live);
    }
    for (const char* restart : {"AZAHARFRAME_IMMERSIVE_DISTANCE", "AZAHARFRAME_IMMERSIVE_FOV",
                                "AZAHARFRAME_XR_PACKED", "AZAHARFRAME_IMMERSIVE_FRAME_TRACE"}) {
        CHECK(TS::ApplyFor(restart) == TS::Apply::Restart);
    }
    // AzaharFrame imglag: the old A/B switch is no longer read: said so, and no toast.
    CHECK(TS::ApplyFor("AZAHARFRAME_IMMERSIVE_ANCHOR_LAG") == TS::Apply::Retired);
    const FTS::Change retired{"AZAHARFRAME_IMMERSIVE_ANCHOR_LAG", "-1", false, false};
    CHECK(TS::LogLine(retired) == "test switch AZAHARFRAME_IMMERSIVE_ANCHOR_LAG=-1 (no longer read)");
    CHECK(TS::ToastText({retired}).empty());

    const FTS::Change tag{"AZAHARFRAME_IMMERSIVE_TAG_OFFSET", "+1", false, false};
    const FTS::Change dist{"AZAHARFRAME_IMMERSIVE_DISTANCE", "2", false, false};
    const FTS::Change gone{"AZAHARFRAME_XR_SINGLE_FLASH", "", true, false};
    const FTS::Change env{"AZAHARFRAME_IMMERSIVE_POSITION_SCALE", "0", false, true};
    CHECK(TS::LogLine(tag) == "test switch AZAHARFRAME_IMMERSIVE_TAG_OFFSET=+1 (live)");
    CHECK(TS::LogLine(dist) == "test switch AZAHARFRAME_IMMERSIVE_DISTANCE=2 (restart needed)");
    CHECK(TS::LogLine(gone) == "test switch AZAHARFRAME_XR_SINGLE_FLASH removed (its default) (live)");
    CHECK(TS::LogLine(env).find("ignored: set in the environment") != std::string::npos);

    CHECK(TS::ToastText({tag}) == "Test Switch: AZAHARFRAME_IMMERSIVE_TAG_OFFSET=+1");
    CHECK(TS::ToastText({dist}) ==
          "Test Switch: AZAHARFRAME_IMMERSIVE_DISTANCE=2 (Restart to Apply)");
    CHECK(TS::ToastText({gone}) == "Test Switch: AZAHARFRAME_XR_SINGLE_FLASH Cleared");
    CHECK(TS::ToastText({env}).empty()); // the environment wins: nothing changed
    CHECK(TS::ToastText({tag, env}) == "Test Switch: AZAHARFRAME_IMMERSIVE_TAG_OFFSET=+1");
    CHECK(TS::ToastText({tag, gone}) == "Test Switches: AZAHARFRAME_IMMERSIVE_TAG_OFFSET=+1 · "
                                        "AZAHARFRAME_XR_SINGLE_FLASH Cleared");
    CHECK(TS::ToastText({tag, gone, dist}) == "Test Switches: 3 Changed");
}

TEST_CASE("Test switches: the file read again while running", "[frame_xr][test_switches]") {
    FakeEnv().clear();
    FakeEnv()["AZAHARFRAME_IMMERSIVE_FOV"] = "60"; // in the environment, not the file
    FakeEnv()["AZAHARFRAME_IMMERSIVE_LOCAL"] = "1"; // in both: the environment wins
    SwitchFile file;
    file.Write("AZAHARFRAME_IMMERSIVE_TAG_OFFSET=-1\nAZAHARFRAME_IMMERSIVE_LOCAL=0\n");

    FTS::LiveFile live(std::string(), &GetFakeEnv);
    std::ifstream in(file.path);
    live.Start(file.path, FTS::Parse(in), {"AZAHARFRAME_IMMERSIVE_LOCAL"});
    CHECK(live.Value("AZAHARFRAME_IMMERSIVE_TAG_OFFSET") == std::optional<std::string>("-1"));
    CHECK(live.Value("AZAHARFRAME_IMMERSIVE_LOCAL") == std::optional<std::string>("1"));
    CHECK(live.Value("AZAHARFRAME_IMMERSIVE_FOV") == std::optional<std::string>("60"));
    CHECK_FALSE(live.Value("AZAHARFRAME_IMMERSIVE_POSITION_SCALE").has_value());

    // Unchanged: nothing read, nothing changes (the once-a-second poll is a stat only).
    CHECK(live.Poll().empty());
    CHECK(live.Poll().empty());

    // A value changed, one added, one the environment holds touched in the file.
    file.Write("AZAHARFRAME_IMMERSIVE_TAG_OFFSET=+1\nAZAHARFRAME_IMMERSIVE_LOCAL=0\n"
               "AZAHARFRAME_IMMERSIVE_POSITION_SCALE=0.5\nAZAHARFRAME_IMMERSIVE_FOV=90\n");
    auto changes = live.Poll();
    REQUIRE(changes.size() == 3);
    CHECK(Find(changes, "AZAHARFRAME_IMMERSIVE_TAG_OFFSET")->value == "+1");
    CHECK_FALSE(Find(changes, "AZAHARFRAME_IMMERSIVE_TAG_OFFSET")->env_wins);
    CHECK(Find(changes, "AZAHARFRAME_IMMERSIVE_POSITION_SCALE")->value == "0.5");
    CHECK(Find(changes, "AZAHARFRAME_IMMERSIVE_FOV")->env_wins);
    CHECK(live.Value("AZAHARFRAME_IMMERSIVE_TAG_OFFSET") == std::optional<std::string>("+1"));
    CHECK(live.Value("AZAHARFRAME_IMMERSIVE_POSITION_SCALE") == std::optional<std::string>("0.5"));
    CHECK(live.Value("AZAHARFRAME_IMMERSIVE_FOV") == std::optional<std::string>("60"));
    CHECK(live.Value("AZAHARFRAME_IMMERSIVE_LOCAL") == std::optional<std::string>("1"));
    CHECK(live.Poll().empty());

    // The same text written again (its time changes): read, but nothing changed.
    file.Write("AZAHARFRAME_IMMERSIVE_TAG_OFFSET=+1\nAZAHARFRAME_IMMERSIVE_LOCAL=0\n"
               "AZAHARFRAME_IMMERSIVE_POSITION_SCALE=0.5\nAZAHARFRAME_IMMERSIVE_FOV=90\n");
    CHECK(live.Poll().empty());

    // A line taken out: back to its default (not to the value the start had).
    file.Write("AZAHARFRAME_IMMERSIVE_POSITION_SCALE=0.5\n");
    changes = live.Poll();
    REQUIRE(Find(changes, "AZAHARFRAME_IMMERSIVE_TAG_OFFSET") != nullptr);
    CHECK(Find(changes, "AZAHARFRAME_IMMERSIVE_TAG_OFFSET")->removed);
    CHECK_FALSE(live.Value("AZAHARFRAME_IMMERSIVE_TAG_OFFSET").has_value());
    CHECK(live.Value("AZAHARFRAME_IMMERSIVE_LOCAL") == std::optional<std::string>("1"));

    // The file gone: every switch it set back to its default; back again: its values again.
    file.Remove();
    changes = live.Poll();
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].removed);
    CHECK_FALSE(live.Value("AZAHARFRAME_IMMERSIVE_POSITION_SCALE").has_value());
    file.Write("AZAHARFRAME_IMMERSIVE_TAG_OFFSET=-1\n");
    changes = live.Poll();
    REQUIRE(changes.size() == 1);
    CHECK(live.Value("AZAHARFRAME_IMMERSIVE_TAG_OFFSET") == std::optional<std::string>("-1"));

    // A secret (a DSN, a token, a key) takes its value but is never returned: never logged or
    // shown in a toast.
    file.Write("AZAHARFRAME_IMMERSIVE_TAG_OFFSET=-1\nAZAHARFRAME_SENTRY_DSN=https://x@y/1\n");
    CHECK(live.Poll().empty());
    CHECK(live.Value("AZAHARFRAME_SENTRY_DSN") == std::optional<std::string>("https://x@y/1"));
    file.Write("AZAHARFRAME_IMMERSIVE_TAG_OFFSET=0\n");
    changes = live.Poll();
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].name == "AZAHARFRAME_IMMERSIVE_TAG_OFFSET");
    CHECK_FALSE(live.Value("AZAHARFRAME_SENTRY_DSN").has_value());
    FakeEnv().clear();
}

TEST_CASE("Test switches: SINGLE_FLASH leaves out a picture shown once, live",
          "[frame_xr][test_switches]") {
    TS::SingleFlash f;
    CHECK_FALSE(f.Hide(true, false, 0)); // no picture: nothing to hide
    CHECK_FALSE(f.Hide(true, true, 1));  // a new one: shown
    CHECK(f.Hide(true, true, 1));        // again: left out
    CHECK(f.Hide(true, true, 1));
    CHECK_FALSE(f.Hide(false, true, 1)); // off: shown again
    CHECK_FALSE(f.Hide(false, true, 2)); // off, followed
    CHECK(f.Hide(true, true, 2));        // on live: the picture shown while off is not new
    CHECK_FALSE(f.Hide(true, true, 3));
}
