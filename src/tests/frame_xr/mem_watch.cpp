// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <filesystem>
#include <fstream>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/mem_watch.h"

// AzaharFrame memwarn
using namespace FrameXr::MemWatch;

namespace {

namespace fs = std::filesystem;
constexpr std::uint64_t kMiB = 1024 * 1024;
constexpr std::uint64_t kMax = 8396 * kMiB; // 8.2 GB, a launcher limit on the Frame

void Write(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

// A fake /proc and /sys/fs/cgroup with the scope's files.
struct FakeScope {
    fs::path root;
    fs::path dir;
    explicit FakeScope(const std::string& name, const std::string& max = std::to_string(kMax)) {
        root = fs::temp_directory_path() / ("azaharframe-mem-watch-" + name);
        fs::remove_all(root);
        const std::string cg = "/user.slice/user-1000.slice/user@1000.service/app.slice/"
                               "azaharframe-42.scope";
        Write(root / "proc/self/cgroup", "0::" + cg + "\n");
        dir = root / ("sys" + cg);
        Write(dir / "memory.max", max + "\n");
        Write(dir / "memory.high", std::to_string(kMax - kMax / 12) + "\n");
        Set(2 * 1024 * kMiB, 0, 0, 0, 0);
    }
    ~FakeScope() {
        fs::remove_all(root);
    }
    void Set(std::uint64_t current, std::uint64_t file, std::uint64_t shmem, std::uint64_t swap,
             std::uint64_t high_events) {
        Write(dir / "memory.current", std::to_string(current) + "\n");
        Write(dir / "memory.swap.current", std::to_string(swap) + "\n");
        Write(dir / "memory.stat", "anon 1000\nfile " + std::to_string(file) +
                                       "\nkernel 4096\nshmem " + std::to_string(shmem) +
                                       "\nfile_mapped 0\n");
        Write(dir / "memory.events", "low 0\nhigh " + std::to_string(high_events) +
                                         "\nmax 0\noom 0\noom_kill 0\noom_group_kill 0\n");
    }
    std::string Proc() const {
        return (root / "proc").string();
    }
    std::string Sys() const {
        return (root / "sys").string();
    }
};

Sample At(double share, std::uint64_t high_events = 0) {
    Sample s;
    s.ok = true;
    s.max = kMax;
    s.current = static_cast<std::uint64_t>(share * static_cast<double>(kMax)) + 1; // not under
    s.high_events = high_events;
    return s;
}

} // namespace

TEST_CASE("Memory watch: the scope's files", "[frame_xr]") {
    FakeScope f("files");
    const std::string dir = ScopeDir(f.Proc(), f.Sys());
    REQUIRE(dir == f.dir.string());
    // 7 GB in the group, 1 GB of it page cache (file 1.5 GB of which 0.5 GB shmem: GPU buffers),
    // 0.3 GB in swap, 4 "high" events
    f.Set(7168 * kMiB, 1536 * kMiB, 512 * kMiB, 300 * kMiB, 4);
    const Sample s = Read(dir);
    REQUIRE(s.ok);
    CHECK(s.max == kMax);
    CHECK(s.high == kMax - kMax / 12);
    CHECK(s.cache == 1024 * kMiB);
    CHECK(s.swap == 300 * kMiB);
    CHECK(s.high_events == 4);
    CHECK(s.Used() == (7168 - 1024 + 300) * kMiB);
    // Shmem larger than file (it is counted in file; never wraps)
    f.Set(3000 * kMiB, 100 * kMiB, 200 * kMiB, 0, 0);
    CHECK(Read(dir).Used() == 3000 * kMiB);
}

TEST_CASE("Memory watch: no scope, or no limit", "[frame_xr]") {
    { // memory.max "max": nothing to watch
        FakeScope f("nolimit", "max");
        CHECK(ScopeDir(f.Proc(), f.Sys()).empty());
        CHECK_FALSE(Read(f.dir.string()).ok);
    }
    { // not in a cgroup of its own (the root), or no /proc at all
        FakeScope f("root");
        Write(f.root / "proc/self/cgroup", "0::/\n");
        CHECK(ScopeDir(f.Proc(), f.Sys()).empty());
        CHECK(ScopeDir((f.root / "nowhere").string(), f.Sys()).empty());
    }
    { // the folder went away: a sample that is not ok, and the watcher does nothing
        CHECK_FALSE(Read("/nonexistent/azaharframe-scope").ok);
        Watcher w;
        CHECK(w.Step(Read("/nonexistent/azaharframe-scope"), 0) == Action::None);
        CHECK_FALSE(w.Critical());
    }
}

TEST_CASE("Memory watch: Level 1, the toast, with hysteresis", "[frame_xr]") {
    Watcher w;
    CHECK(w.Step(At(0.50), 0) == Action::None);
    CHECK(w.Step(At(0.849), 2'000) == Action::None);
    CHECK(w.Step(At(0.86), 4'000) == Action::Warn);
    CHECK_FALSE(w.Critical());
    // Staying up there: once
    CHECK(w.Step(At(0.90), 6'000) == Action::None);
    CHECK(w.Step(At(0.86), 200'000) == Action::None); // never went under 80 %
    // Under 80 % and back, but within 2 minutes of the last one: waits for the 2 minutes
    CHECK(w.Step(At(0.79), 202'000) == Action::None);
    Watcher v;
    CHECK(v.Step(At(0.86), 0) == Action::Warn);
    CHECK(v.Step(At(0.79), 2'000) == Action::None);
    CHECK(v.Step(At(0.86), 4'000) == Action::None);
    CHECK(v.Step(At(0.86), 119'999) == Action::None);
    CHECK(v.Step(At(0.86), 120'000) == Action::Warn);
    // Dipping to 82 % is not enough to show it again
    CHECK(v.Step(At(0.82), 300'000) == Action::None);
    CHECK(v.Step(At(0.86), 302'000) == Action::None);
    CHECK(v.Step(At(0.79), 304'000) == Action::None);
    CHECK(v.Step(At(0.87), 306'000) == Action::Warn);
}

TEST_CASE("Memory watch: Level 2, the pause", "[frame_xr]") {
    Watcher w;
    CHECK(w.Step(At(0.86), 0) == Action::Warn);
    CHECK(w.Step(At(0.929), 2'000) == Action::None);
    CHECK_FALSE(w.Critical());
    CHECK(w.Step(At(0.93), 4'000) == Action::Pause);
    CHECK(w.Critical());
    // Resumed and still there: no second dialog every 2 s
    CHECK(w.Step(At(0.94), 6'000) == Action::None);
    CHECK(w.Critical());
    // Turn Off Custom Textures freed some: under 93 %, still paused for (autosaves wait)
    CHECK(w.Step(At(0.90), 8'000) == Action::None);
    CHECK(w.Critical());
    // Under 88 %: re-armed, and no toast for a level it already passed
    CHECK(w.Step(At(0.87), 10'000) == Action::None);
    CHECK_FALSE(w.Critical());
    CHECK(w.Step(At(0.95), 12'000) == Action::Pause);
    // Resumed at 95 % and it grows on: 3 % more pauses again, the last chance before the limit
    CHECK(w.Step(At(0.97), 14'000) == Action::None);
    CHECK(w.Step(At(0.981), 16'000) == Action::Pause);
    CHECK(w.Critical());

    // Straight past both levels in one step: the pause, not the toast
    Watcher j;
    CHECK(j.Step(At(0.50), 0) == Action::None);
    CHECK(j.Step(At(0.96), 2'000) == Action::Pause);
    CHECK(j.Step(At(0.86), 4'000) == Action::None); // no toast right after the dialog
}

TEST_CASE("Memory watch: a memory.events high step", "[frame_xr]") {
    Watcher w;
    // The first sample only sets the count (the scope may have had events before)
    CHECK(w.Step(At(0.50, 7), 0) == Action::None);
    // Under Level 1 a step is the page cache being reclaimed: nothing
    CHECK(w.Step(At(0.60, 9), 2'000) == Action::None);
    CHECK_FALSE(w.Critical());
    CHECK(w.Step(At(0.86, 9), 4'000) == Action::Warn);
    // At Level 1 the kernel throttles at MemoryHigh: pause
    CHECK(w.Step(At(0.87, 12), 6'000) == Action::Pause);
    CHECK(w.Critical());
    // More events at the same level: not again; re-armed only 5 % under where it paused
    CHECK(w.Step(At(0.87, 30), 8'000) == Action::None);
    CHECK(w.Step(At(0.83, 30), 10'000) == Action::None);
    CHECK(w.Critical());
    CHECK(w.Step(At(0.81, 30), 12'000) == Action::None);
    CHECK_FALSE(w.Critical());
    CHECK(w.Step(At(0.86, 31), 14'000) == Action::Pause);
}

TEST_CASE("Memory watch: the texts", "[frame_xr]") {
    const std::uint64_t used = static_cast<std::uint64_t>(7.1 * 1024) * kMiB + kMiB;
    CHECK(UsedOf(used, kMax) == "7.1 of 8.2 GB");
    CHECK(WarnText(used, kMax) == "Running Low on Memory: 7.1 of 8.2 GB");
    const std::uint64_t used2 = static_cast<std::uint64_t>(7.6 * 1024) * kMiB + kMiB;
    const PauseText t = MakePauseText(used2, kMax, true);
    CHECK(t.title == "Azahar Is Almost Out of Memory");
    CHECK(t.body == "Using 7.6 of 8.2 GB. The game is paused so Azahar isn't closed and your "
                    "progress stays safe. Turn off custom textures, then save in the game.");
    CHECK(t.textures == "Turn Off Custom Textures");
    CHECK(t.resume == "Resume");
    CHECK(t.stop == "Stop Game");
    const PauseText n = MakePauseText(used2, kMax, false);
    CHECK(n.textures.empty());
    CHECK(n.body.find("Save in the game, then stop it.") != std::string::npos);
}

// AzaharFrame savemem: a save state's memory, held for a moment (common/transient_memory.h)
TEST_CASE("Memory watch: a save's own spike does not pause", "[frame_xr][savemem]") {
    const auto mib = [](double share) {
        return static_cast<std::uint64_t>(share * static_cast<double>(kMax));
    };
    Watcher w;
    CHECK(w.Step(At(0.70), 0) == Action::None);
    // A quick save holds 25 % of the limit for a moment: 95 % with it, 70 % without
    CHECK(w.Step(At(0.95, 3), 2'000, mib(0.25)) == Action::None);
    CHECK(w.Spared());
    CHECK_FALSE(w.Critical()); // the autosaves need not wait for it either
    // Its "high" events (the save crossed memory.high) are its own: not a step, at the sample
    // right after it either. 86 % without it is the game's: the toast, not the pause.
    CHECK(w.Step(At(0.86, 9), 2'250) == Action::Warn);
    CHECK_FALSE(w.Spared());
    CHECK(w.Step(At(0.86, 9), 4'250) == Action::None);
    // A later step is the game's again: pauses at Level 1
    CHECK(w.Step(At(0.86, 10), 6'250) == Action::Pause);
}

TEST_CASE("Memory watch: near the limit without the save, it still pauses",
          "[frame_xr][savemem]") {
    const auto mib = [](double share) {
        return static_cast<std::uint64_t>(share * static_cast<double>(kMax));
    };
    {
        // 99 % with the save's 4 %, 95 % without: over Level 2 either way
        Watcher w;
        CHECK(w.Step(At(0.99), 0, mib(0.04)) == Action::Pause);
        CHECK(w.Critical());
        CHECK_FALSE(w.Spared());
    }
    {
        // Spared during the save, but the memory stays once it ends: the next sample pauses
        Watcher w;
        CHECK(w.Step(At(0.97), 0, mib(0.15)) == Action::None);
        CHECK(w.Spared());
        CHECK(w.Step(At(0.95), 250) == Action::Pause);
    }
    {
        // A save's memory is never more than all of it
        Watcher w;
        CHECK(w.Step(At(0.50), 0, kMax * 2) == Action::None);
    }
}

TEST_CASE("Memory watch: custom textures, only when they matter", "[frame_xr][savemem]") {
    constexpr std::uint64_t kSmall = 4379 * kMiB; // the owner's Frame
    CHECK_FALSE(TexturesMatter(0, kSmall));        // none loaded (Kid Icarus, no pack)
    CHECK_FALSE(TexturesMatter(64 * kMiB, kSmall));
    CHECK_FALSE(TexturesMatter(400 * kMiB, kSmall)); // under 10 % of 4.3 GB
    CHECK(TexturesMatter(500 * kMiB, kSmall));
    CHECK(TexturesMatter(kMax / 10 + 1, kMax));
    CHECK_FALSE(TexturesMatter(100 * kMiB, 600 * kMiB)); // the 128 MiB floor
    CHECK(TexturesMatter(128 * kMiB, 600 * kMiB));
    // The dialog's advice follows (MakePauseText's flag is TexturesMatter's answer)
    CHECK(MakePauseText(4000 * kMiB, kSmall, false).body ==
          "Using 3.9 of 4.3 GB. The game is paused so Azahar isn't closed and your progress "
          "stays safe. Save in the game, then stop it.");
    CHECK(MakePauseText(4000 * kMiB, kSmall, false).textures.empty());
}
