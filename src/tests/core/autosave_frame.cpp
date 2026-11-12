// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame autosave: the files (rotation, Keep lowered, one game's Keep trimming only its own),
// the trigger rules (the minute's throttle, the start's grace, what blocks one), the undo file,
// independence from the slots, autosaves from another build, the texts (the Emulation menu's
// entries as its slots are written, "3 Minutes Ago"), the per-game values, and the writer.

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include "common/scm_rev.h"
#include "common/settings.h"
#include "common/zstd_compression.h"
#include "core/autosave.h"
#include "core/savestate.h"

namespace fs = std::filesystem;
using namespace Core;
namespace A = Core::Autosave;

namespace {

constexpr u64 kTitle = 0x0004000000055D00ULL;
constexpr u64 kOther = 0x0004000000033500ULL;
const std::string kOtherRev(40, '0');
constexpr s64 kBase = 1'794'000'000; // 2026-11-06 21:20:00 UTC

struct TempDir {
    fs::path path;
    explicit TempDir(const char* name)
        : path(fs::temp_directory_path() /
               (std::string("azf_autosave_") + name + "_" + std::to_string(getpid()))) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    std::string Dir() const {
        return path.string() + "/";
    }
};

/// An autosave of `title` made at kBase + `t` s, by this build (or another), with its picture.
std::string Make(const std::string& dir, u64 title, s64 t, bool other = false) {
    const std::string path = A::PathFor(dir, title, 0, kBase + t);
    const std::vector<u8> body{1, 2, 3};
    fs::create_directories(fs::path(path).parent_path());
    WriteStateFile(path,
                   other
                       ? MakeStateHeader(title, kOtherRev, "1.1.1", "", static_cast<u64>(kBase + t))
                       : CurrentStateHeader(title, static_cast<u64>(kBase + t)),
                   body);
    std::ofstream(A::ThumbPathFor(path), std::ios::binary) << "png";
    return path;
}

std::size_t Count(const std::string& dir, const std::string& ext) {
    std::size_t n = 0;
    for (const auto& e : fs::directory_iterator(dir)) {
        n += e.path().extension() == ext;
    }
    return n;
}

A::Gates Running() {
    A::Gates g;
    g.game = g.playing = g.frames_recent = true;
    return g;
}

} // namespace

TEST_CASE("Autosave: settings' ranges", "[autosave][frame_xr]") {
    CHECK(A::ClampEvery(0) == 1);
    CHECK(A::ClampEvery(4) == 5);
    CHECK(A::ClampEvery(7) == 5);
    CHECK(A::ClampEvery(8) == 10);
    CHECK(A::ClampEvery(99) == 15);
    CHECK(A::EveryIndex(5) == 2);
    CHECK(A::EveryAt(4) == 15);
    CHECK(A::ClampKeep(0) == 1);
    CHECK(A::ClampKeep(11) == 10);
    // The defaults (owner decision): off, every 5 minutes, keep 3
    CHECK(Settings::values.autosave_enabled.GetDefault() == false);
    CHECK(Settings::values.autosave_every.GetDefault() == 5);
    CHECK(Settings::values.autosave_keep.GetDefault() == 3);
}

TEST_CASE("Autosave: the game's own values win, else the global ones", "[autosave][frame_xr]") {
    auto& v = Settings::values;
    v.autosave_enabled.SetGlobal(true);
    v.autosave_keep.SetGlobal(true);
    v.autosave_enabled.SetValue(false);
    v.autosave_keep.SetValue(3);
    // No value of its own: the global default applies
    v.autosave_enabled.SetGlobal(false); // a game runs
    v.autosave_keep.SetGlobal(false);
    v.autosave_enabled.SetGlobal(true); // ... with no value of its own for Autosave
    v.autosave_keep.SetGlobal(true);
    CHECK(v.autosave_enabled.GetValue() == false);
    CHECK(v.autosave_keep.GetValue() == 3);
    // Its own (Per-Game, as the VR menu's Apply and the Properties do): it wins
    v.autosave_enabled.SetGlobal(false);
    v.autosave_enabled.SetValue(true);
    v.autosave_keep.SetGlobal(false);
    v.autosave_keep.SetValue(7);
    CHECK(v.autosave_enabled.GetValue() == true);
    CHECK(v.autosave_keep.GetValue() == 7);
    CHECK(v.autosave_keep.GetValue(true) == 3); // the global one is untouched
    // The game stops (RestoreGlobalState): the global values again
    Settings::RestoreGlobalState(false);
    CHECK(v.autosave_enabled.GetValue() == false);
    CHECK(v.autosave_keep.GetValue() == 3);
}

TEST_CASE("Autosave: names, rotation and Keep lowered", "[autosave][frame_xr]") {
    TempDir states("rotate");
    const std::string dir = A::Dir(states.Dir());
    CHECK(dir == states.Dir() + "autosave/");
    CHECK(A::Prefix(kTitle, 0) == "0004000000055D00.");
    const std::string first = A::PathFor(dir, kTitle, 0, kBase);
    CHECK(fs::path(first).filename().string() == "0004000000055D00.20261106-212000.cst");
    CHECK(A::ThumbPathFor(first) == dir + "0004000000055D00.20261106-212000.png");

    // Keep 3: the fourth makes the oldest go, with its picture
    for (int i = 0; i < 4; i++) {
        Make(dir, kTitle, i * 300);
        A::Prune(dir, kTitle, 0, 3);
    }
    auto list = A::List(dir, kTitle, 0);
    REQUIRE(list.size() == 3);
    CHECK(list[0].time == static_cast<u64>(kBase + 900)); // newest first
    CHECK(list[2].time == static_cast<u64>(kBase + 300));
    CHECK(!list[0].thumb.empty());
    CHECK(!fs::exists(first));
    CHECK(!fs::exists(A::ThumbPathFor(first)));
    CHECK(Count(dir, ".png") == 3);

    // Keep lowered to 1: the two older go at once
    CHECK(A::Prune(dir, kTitle, 0, 1) == 2);
    list = A::List(dir, kTitle, 0);
    REQUIRE(list.size() == 1);
    CHECK(list[0].time == static_cast<u64>(kBase + 900));
    CHECK(Count(dir, ".png") == 1);

    // A write cut short and a picture whose state is gone are cleaned up too
    std::ofstream(dir + "0004000000055D00.20261107-030000.cst.tmp") << "x";
    std::ofstream(dir + "0004000000055D00.20261107-030500.png") << "x";
    A::Prune(dir, kTitle, 0, 1);
    CHECK(!fs::exists(dir + "0004000000055D00.20261107-030000.cst.tmp"));
    CHECK(!fs::exists(dir + "0004000000055D00.20261107-030500.png"));
}

TEST_CASE("Autosave: one game's Keep trims only its own", "[autosave][frame_xr]") {
    TempDir states("pergame");
    const std::string dir = A::Dir(states.Dir());
    for (int i = 0; i < 5; i++) {
        Make(dir, kTitle, i * 60);
        Make(dir, kOther, i * 60);
    }
    // Ocarina of Time 3D has Keep 2 of its own; the other game keeps the global 5
    A::Prune(dir, kTitle, 0, 2);
    CHECK(A::List(dir, kTitle, 0).size() == 2);
    CHECK(A::List(dir, kOther, 0).size() == 5);
    // A movie's autosaves are a game of their own too
    const std::string movie = A::PathFor(dir, kTitle, 7, kBase);
    WriteStateFile(movie, CurrentStateHeader(kTitle, kBase), std::vector<u8>{1});
    A::Prune(dir, kTitle, 0, 1);
    CHECK(fs::exists(movie));
    CHECK(A::List(dir, kTitle, 7).size() == 1);
}

TEST_CASE("Autosave: apart from the slots, and the undo file", "[autosave][frame_xr]") {
    TempDir states("slots");
    const std::string dir = A::Dir(states.Dir());
    Make(dir, kTitle, 0);
    // The slots (Quick Save is 0) never see an autosave: "Load from Newest Slot" ignores them
    WriteStateFile(StatePathIn(states.Dir(), kTitle, 0, 2), CurrentStateHeader(kTitle, kBase - 9),
                   std::vector<u8>{1});
    const auto slots = ListSaveStatesIn(states.Dir(), kTitle, 0);
    REQUIRE(slots.size() == 1);
    CHECK(slots[0].slot == 2);
    // Saving a slot leaves the autosaves alone, and the other way round
    A::Prune(dir, kTitle, 0, 1);
    CHECK(fs::exists(StatePathIn(states.Dir(), kTitle, 0, 2)));
    CHECK(A::List(dir, kTitle, 0).size() == 1);

    // Undo Load's file: beside the slots, outside 0 to 10, never in the slot list
    CHECK(A::kUndoLoadSlot > SaveStateSlotCount);
    const std::string undo = StatePathIn(states.Dir(), kTitle, 0, A::kUndoLoadSlot);
    CHECK(fs::path(undo).filename().string() == "0004000000055D00.98.cst");
    WriteStateFile(undo, CurrentStateHeader(kTitle, kBase), std::vector<u8>{1});
    CHECK(ListSaveStatesIn(states.Dir(), kTitle, 0).size() == 1);
    CHECK(GetSaveStateInfoIn(states.Dir(), kTitle, 0, A::kUndoLoadSlot).slot == A::kUndoLoadSlot);
    CHECK(A::List(dir, kTitle, 0).size() == 1);
}

TEST_CASE("Autosave: from another build", "[autosave][frame_xr]") {
    TempDir states("other");
    const std::string dir = A::Dir(states.Dir());
    const std::string old = Make(dir, kTitle, 0, true);
    Make(dir, kTitle, 60);
    const auto list = A::List(dir, kTitle, 0);
    REQUIRE(list.size() == 2);
    CHECK(!list[0].other);
    CHECK(list[1].other);
    CHECK(list[1].version == "1.1.1");
    CHECK(SaveStateFromText(list[1].version) == "From 1.1.1");
    // The same rules as the slots: Load Anyway's question, with this build's version as 1.2.0
    CHECK(SaveStateMismatchTitle(list[1].version, "1.2.0") ==
          "This Save State Is From an Older Version");
    CHECK(SaveStateMismatchTitle("1.3.0", "1.2.0") == "This Save State Is From a Newer Version");
    const auto info = GetSaveStateInfoAt(old, kTitle);
    CHECK(info.slot == 0);
    CHECK(info.status != SaveStateInfo::ValidationStatus::OK);
    // Another game's file in the folder is no autosave of this one
    CHECK(GetSaveStateInfoAt(old, kOther).slot != 0);
}

TEST_CASE("Autosave: at most one a minute, none in the first minute", "[autosave][frame_xr]") {
    A::Scheduler s;
    const A::Gates g = Running();
    CHECK(s.Blocked(A::Reason::Timer, 0, g) == "no game");
    s.Boot(0);
    CHECK(s.Blocked(A::Reason::MenuOpen, 59'999, g) == "the game started less than a minute ago");
    CHECK(s.Blocked(A::Reason::MenuOpen, 60'000, g).empty());
    s.Fired(60'000);
    CHECK(s.Blocked(A::Reason::Quit, 100'000, g) == "one was made less than a minute ago");
    CHECK(s.Blocked(A::Reason::Timer, 119'999, g) == "one was made less than a minute ago");
    CHECK(s.Blocked(A::Reason::Timer, 120'000, g).empty());

    // The timer counts play, not the clock: paused minutes do not count, a long gap is 2 s
    A::Scheduler t;
    t.Boot(0);
    for (s64 ms = 0; ms <= 300'000; ms += 1000) {
        t.Tick(ms, true);
    }
    CHECK(t.Due(300'000));
    t.Fired(300'000);
    CHECK(!t.Due(300'000));
    for (s64 ms = 301'000; ms <= 900'000; ms += 1000) {
        t.Tick(ms, false); // paused for ten minutes
    }
    CHECK(t.PlayMs() == 0);
    t.Tick(901'000, true);
    t.Tick(5'000'000, true); // the machine slept
    CHECK(t.PlayMs() <= 3000);
    t.Stop();
    CHECK(!t.Due(0));
}

TEST_CASE("Autosave: what keeps one from being made", "[autosave][frame_xr]") {
    A::Scheduler s;
    s.Boot(0);
    const s64 now = 120'000;
    A::Gates g = Running();
    g.loading_state = true;
    CHECK(s.Blocked(A::Reason::Timer, now, g) == "a state is being saved or loaded");
    g = Running();
    g.shaders_busy = true;
    CHECK(s.Blocked(A::Reason::MenuOpen, now, g) == "shaders are being prepared");
    g = Running();
    g.playing = false;
    g.paused = true;
    CHECK(s.Blocked(A::Reason::Timer, now, g) == "the game is paused");
    // Closed while paused (from the menu): the paused game is a sound state
    CHECK(s.Blocked(A::Reason::Quit, now, g).empty());
    g = Running();
    g.frames_recent = false; // standing still (a loading screen that hangs, the game is gone)
    CHECK(s.Blocked(A::Reason::Timer, now, g) == "the game is not running");
    CHECK(s.Blocked(A::Reason::MenuOpen, now, g) == "the game is not running");
    CHECK(s.Blocked(A::Reason::Quit, now, g) == "the game is not running");
    g = Running();
    g.game = false;
    CHECK(s.Blocked(A::Reason::Quit, now, g) == "no game");
    // AzaharFrame memwarn: almost at the memory limit, none of them (a save state needs ~0.9 GB)
    g = Running();
    g.memory_low = true;
    CHECK(s.Blocked(A::Reason::Timer, now, g) == "memory is almost full");
    CHECK(s.Blocked(A::Reason::MenuOpen, now, g) == "memory is almost full");
    g.playing = false;
    g.paused = true; // paused by the memory dialog, then Stop Game
    CHECK(s.Blocked(A::Reason::Quit, now, g) == "memory is almost full");
    g.memory_low = false;
    CHECK(s.Blocked(A::Reason::Quit, now, g).empty());
}

TEST_CASE("Autosave: the texts", "[autosave][frame_xr]") {
    const std::string dt = "2026-11-10 09:00";
    CHECK(A::Ago(1000, 1059, 5, 5, dt) == "Just Now");
    CHECK(A::Ago(1000, 1000 - 30, 5, 5, dt) == "Just Now"); // a clock set back
    CHECK(A::Ago(1000, 1060, 5, 5, dt) == "1 Minute Ago");
    CHECK(A::Ago(1000, 1000 + 12 * 60 + 30, 5, 5, dt) == "12 Minutes Ago");
    CHECK(A::Ago(1000, 1000 + 3600, 5, 5, dt) == "1 Hour Ago");
    CHECK(A::Ago(1000, 1000 + 3 * 3600 + 5, 5, 5, dt) == "3 Hours Ago");
    CHECK(A::Ago(1000, 1000 + 3 * 3600, 5, 6, dt) == "Yesterday");
    CHECK(A::Ago(1000, 1000 + 50 * 3600, 5, 7, dt) == dt);
    // The Emulation menu's entries are written as its slots are ("Slot 1 - 2026-11-12 10:14:05")
    CHECK(A::MenuEntry("Slot 1", "2026-11-12 10:14:05") == "Slot 1 - 2026-11-12 10:14:05");
    CHECK(A::MenuEntry("Autosave 1", "2026-11-12 10:14:05") == "Autosave 1 - 2026-11-12 10:14:05");
    CHECK(A::MenuEntry("Autosave 3", "") == "Autosave 3");
    CHECK(A::MenuEntry("Undo Load", "2026-11-12 10:31:47") == "Undo Load - 2026-11-12 10:31:47");
    CHECK(A::LocalDateTimeSeconds(static_cast<u64>(kBase)).size() == 19);
    CHECK(A::LocalDateTime(static_cast<u64>(kBase)).size() == 16);
    CHECK(A::LocalTime(static_cast<u64>(kBase)).size() == 5);
}

TEST_CASE("Autosave: the writer", "[autosave][frame_xr]") {
    TempDir states("writer");
    const std::string dir = A::Dir(states.Dir());
    for (int i = 0; i < 3; i++) {
        Make(dir, kTitle, i * 60);
    }
    A::Writer::Job job;
    job.dir = dir;
    job.path = A::PathFor(dir, kTitle, 0, kBase + 600);
    job.header = CurrentStateHeader(kTitle, static_cast<u64>(kBase + 600));
    job.raw = std::string(1 << 20, 'z');
    job.program_id = kTitle;
    job.keep = 2;
    job.reason = A::Reason::Timer;
    const A::Writer::Done done = A::Writer::Write(job);
    CHECK(done.ok);
    CHECK(done.raw_bytes == (1u << 20));
    CHECK(done.file_bytes < done.raw_bytes / 10); // compressed
    CHECK(job.raw.empty());                       // let go once compressed
    // Written whole (a state of this game from this build), and the oldest went (Keep 2)
    const auto info = GetSaveStateInfoAt(job.path, kTitle);
    CHECK(info.slot == 0);
    CHECK(info.status == SaveStateInfo::ValidationStatus::OK);
    const auto list = A::List(dir, kTitle, 0);
    REQUIRE(list.size() == 2);
    CHECK(list[0].path == job.path);
    // Its body is the zstd of the system's bytes
    std::ifstream in(job.path, std::ios::binary);
    std::vector<u8> file{std::istreambuf_iterator<char>(in), {}};
    REQUIRE(file.size() > 256);
    const auto raw =
        Common::Compression::DecompressDataZSTD(std::vector<u8>(file.begin() + 256, file.end()));
    CHECK(raw.size() == (1u << 20));
    CHECK(fs::directory_iterator(dir) != fs::directory_iterator{});

    // The picture: BGRA in, a PNG out
    std::vector<u8> bgra(400 * 240 * 4, 0x80);
    const auto png = A::Writer::EncodeThumb(bgra, 400, 240);
    REQUIRE(png.size() > 8);
    CHECK(png[1] == 'P');
    CHECK(A::Writer::EncodeThumb({}, 400, 240).empty());

    // On its thread: in order, the picture after its state, idle at the end
    A::Writer w;
    int states_done = 0;
    w.SetOnDone([&](const A::Writer::Done& d) { states_done += d.ok; });
    A::Writer::Job j2 = job;
    j2.path = A::PathFor(dir, kTitle, 0, kBase + 900);
    j2.raw = std::string(4096, 'a');
    w.Submit(std::move(j2));
    w.SubmitThumb(A::ThumbPathFor(A::PathFor(dir, kTitle, 0, kBase + 900)), bgra, 400, 240);
    w.WaitIdle();
    CHECK(!w.Busy());
    CHECK(states_done == 1);
    CHECK(fs::exists(A::ThumbPathFor(A::PathFor(dir, kTitle, 0, kBase + 900))));
}

// AzaharFrame advfix
TEST_CASE("Autosave: the menu-open autosave has a spot of its own", "[autosave][frame_xr]") {
    TempDir states("menuopen");
    const std::string dir = A::Dir(states.Dir());
    const std::string menu = A::MenuPathFor(dir, kTitle, 0);
    CHECK(fs::path(menu).filename().string() == "0004000000055D00.menu.cst");
    CHECK(A::MenuPathFor(dir, kTitle, 7) ==
          dir + "0004000000055D00.movie0000000000000007.menu.cst");

    // Keep 3: three timer autosaves (T-12, T-7, T-2 minutes)
    for (int i = 0; i < 3; i++) {
        Make(dir, kTitle, i * 300);
    }
    const auto write = [&](s64 t) {
        A::Writer::Job job;
        job.dir = dir;
        job.path = A::MenuPathFor(dir, kTitle, 0);
        job.header = CurrentStateHeader(kTitle, static_cast<u64>(kBase + t));
        job.raw = std::string(4096, 'm');
        job.program_id = kTitle;
        job.keep = 3;
        job.reason = A::Reason::MenuOpen;
        return A::Writer::Write(job);
    };
    // The menu opened twice: one file, replaced; the timer's three all stay
    REQUIRE(write(700).ok);
    REQUIRE(write(800).ok);
    auto list = A::List(dir, kTitle, 0);
    REQUIRE(list.size() == 4);
    CHECK(list[0].path == menu); // the newest, in the list like the others
    CHECK(list[0].menu);
    CHECK(list[0].time == static_cast<u64>(kBase + 800));
    CHECK(!list[1].menu);
    CHECK(list[3].time == static_cast<u64>(kBase)); // the oldest timer autosave is still there
    // Not counted in Keep: the timer's rotation prunes only its own
    Make(dir, kTitle, 900);
    CHECK(A::Prune(dir, kTitle, 0, 3) == 1);
    list = A::List(dir, kTitle, 0);
    REQUIRE(list.size() == 4);
    CHECK(fs::exists(menu));
    CHECK(A::Prune(dir, kTitle, 0, 1) == 2);
    CHECK(fs::exists(menu));
    CHECK(A::PruneAll(dir, 1) == 0);
    CHECK(fs::exists(menu));
    // Its picture is its own too, and is not taken for an orphan
    std::ofstream(A::ThumbPathFor(menu)) << "png";
    A::Prune(dir, kTitle, 0, 1);
    CHECK(fs::exists(A::ThumbPathFor(menu)));
    // The menu-open autosave is older than the timer's newest now: sorted by time
    list = A::List(dir, kTitle, 0);
    REQUIRE(list.size() == 2);
    CHECK(!list[0].menu);
    CHECK(list[1].menu);
}

TEST_CASE("Autosave: what the lists show", "[autosave][frame_xr]") {
    std::vector<A::Entry> all(6);
    for (std::size_t i = 0; i < all.size(); i++) {
        all[i].time = 100 - i;
        all[i].path = std::to_string(i);
    }
    all[2].menu = true;
    // Keep 3: the menu-open one and the newest three of the rest
    auto shown = A::Shown(all, 3);
    REQUIRE(shown.size() == 4);
    CHECK(shown[0].path == "0");
    CHECK(shown[2].path == "2");
    CHECK(shown[3].path == "3");
    // Keep 10 and the menu's: eleven rows at most
    std::vector<A::Entry> many(12);
    many[0].menu = true;
    CHECK(A::Shown(many, 10).size() == 11);
    CHECK(A::Shown({}, 3).empty());
}

TEST_CASE("Autosave: the scope's toast and Undo Load's text", "[autosave][frame_xr]") {
    CHECK(A::ScopeToast(true, "Ocarina of Time 3D") == "Autosave On for Ocarina of Time 3D");
    CHECK(A::ScopeToast(false, "Ocarina of Time 3D") == "Autosave Off for Ocarina of Time 3D");
    // Long titles cut to 24 characters with "...", no space before the dots
    CHECK(A::ScopeToast(true, "The Legend of Zelda: Majora's Mask 3D") ==
          "Autosave On for The Legend of Zelda: Maj...");
    CHECK(A::ScopeToast(true, "Kirby Planet Robobot an d") ==
          "Autosave On for Kirby Planet Robobot an...");
    CHECK(A::ScopeToast(true, "Kirby Planet Robobot and so") ==
          "Autosave On for Kirby Planet Robobot and...");
    CHECK(A::ScopeToast(true, "Pok\xC3\xA9mon Ultra Sun") ==
          "Autosave On for Pok\xC3\xA9mon Ultra Sun");
    CHECK(A::ScopeToast(true, "") == "Autosave On for This Game");
    const u64 t = static_cast<u64>(kBase);
    const std::string hm = A::LocalTime(t);
    CHECK(A::UndoText(-1, t) == "Autosave Loaded: " + hm);
    CHECK(A::UndoText(0, t) == "Quick Save Loaded: " + hm);
    CHECK(A::UndoText(3, t) == "Slot 3 Loaded: " + hm);
}

TEST_CASE("Autosave: a failed one is told, not too often", "[autosave][frame_xr]") {
    CHECK(A::FailureToast(A::Failure::None).empty());
    CHECK(A::FailureToast(A::Failure::Busy).empty()); // a skip, not a failure
    CHECK(A::FailureToast(A::Failure::Serialize) == "Couldn't Autosave");
    CHECK(A::FailureToast(A::Failure::Compress) == "Couldn't Autosave");
    CHECK(A::FailureToast(A::Failure::NoSpace) == "Couldn't Autosave: Not Enough Space");
    CHECK(A::FailureToast(A::Failure::Write) == "Couldn't Autosave: Couldn't Write the File");
    A::FailureNotice n;
    CHECK(n.Show(A::Failure::Busy, 0).empty());
    CHECK(n.Show(A::Failure::NoSpace, 1000) == "Couldn't Autosave: Not Enough Space");
    CHECK(n.Show(A::Failure::NoSpace, 1000 + A::kFailureToastGapMs - 1).empty());
    CHECK(n.Show(A::Failure::Write, 1000 + A::kFailureToastGapMs) ==
          "Couldn't Autosave: Couldn't Write the File");
    // Why a write failed: more than the disk holds is Not Enough Space
    TempDir states("space");
    CHECK(A::WriteFailure(states.Dir() + "autosave/x.cst", std::size_t(1) << 62) ==
          A::Failure::NoSpace);
    CHECK(A::WriteFailure(states.Dir() + "x.cst", 10) == A::Failure::Write);
    // The writer says so: a folder that cannot be made (a file in its place)
    std::ofstream(states.Dir() + "blocked") << "file";
    A::Writer::Job job;
    job.dir = states.Dir() + "blocked/";
    job.path = states.Dir() + "blocked/x.cst";
    job.raw = std::string(100, 'a');
    job.program_id = kTitle;
    const auto done = A::Writer::Write(job);
    CHECK(!done.ok);
    CHECK(done.failure == A::Failure::Write);
}

TEST_CASE("Autosave: loaded by what the list showed, not its place", "[autosave][frame_xr]") {
    TempDir states("identity");
    const std::string dir = A::Dir(states.Dir());
    const std::string t0 = Make(dir, kTitle, 0);
    Make(dir, kTitle, 300);
    const std::string menu = A::MenuPathFor(dir, kTitle, 0);
    WriteStateFile(menu, CurrentStateHeader(kTitle, static_cast<u64>(kBase + 400)),
                   std::vector<u8>{1});
    const auto shown = A::List(dir, kTitle, 0); // as the card drew it: menu, +300, +0
    REQUIRE(shown.size() == 3);
    const std::string menu_id = A::EntryId(shown[0]);
    const std::string oldest_id = A::EntryId(shown[2]);
    CHECK(menu_id == "0004000000055D00.menu.cst@" + std::to_string(kBase + 400));
    CHECK(A::FindEntry(shown, oldest_id) == 2);
    CHECK(A::SameState(menu, kTitle, static_cast<u64>(kBase + 400)));

    // A timer autosave lands and the menu-open one is replaced before the tap reaches the
    // emulator: the places shift, the identities still find the right ones
    Make(dir, kTitle, 600);
    WriteStateFile(menu, CurrentStateHeader(kTitle, static_cast<u64>(kBase + 700)),
                   std::vector<u8>{1});
    auto now = A::List(dir, kTitle, 0);
    REQUIRE(now.size() == 4);
    CHECK(A::FindEntry(now, oldest_id) == 3); // was 2
    // The menu-open autosave the card showed is no longer in its file: not loaded at all
    CHECK(A::FindEntry(now, menu_id) == -1);
    CHECK(!A::SameState(menu, kTitle, static_cast<u64>(kBase + 400)));
    CHECK(A::SameState(menu, kTitle, 0));
    // One pruned since: gone, nothing else in its place
    fs::remove(t0);
    now = A::List(dir, kTitle, 0);
    CHECK(A::FindEntry(now, oldest_id) == -1);
    CHECK(!A::SameState(t0, kTitle, 0));
    CHECK(A::FindEntry(now, "") == -1);
}
