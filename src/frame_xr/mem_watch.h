// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame memwarn: a warning before the memory limit, and a pause before the kill. The
// launcher runs the emulator in a user scope with MemoryMax, MemoryHigh 1/12 under it and some
// swap (steam-frame/azaharframe.sh). Past MemoryMax the kernel kills it, and whatever was not
// saved in the game is lost. So the scope's own counters are read every 2 s (citra_qt/
// frame_memwarn.cpp, on a thread of its own) and:
//
//   Level 1, at kWarnAt of memory.max: a toast with the numbers ("Running Low on Memory: 7.1 of
//     8.2 GB"). Again only after falling under kWarnRearm and coming back, and at most once in
//     kWarnGapMs.
//   Level 2, at kPauseAt (or a memory.events "high" step while at Level 1): the game pauses and a
//     dialog says why, with Turn Off Custom Textures (when they are on), Resume and Stop Game.
//     Again only after falling under kPauseRearm, or after growing kPauseAgain more (the last
//     chance before the limit). While at Level 2 (Critical) the autosaves of the menu and the
//     timer wait: writing a save state needs about 0.9 GB more for a moment.
//
// "Used" is what the kernel cannot simply drop: memory.current less the page cache (memory.stat
// file - shmem; GPU buffers are shmem and stay counted) plus what went to swap. The page cache
// alone often reaches MemoryHigh while a texture pack is read, which is why a "high" event counts
// only at Level 1 or above. Pure parsing and rules (unit-tested with fake cgroup files in
// src/tests/frame_xr/mem_watch.cpp).

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include "video_core/custom_textures/preload_budget.h"

namespace FrameXr::MemWatch {

constexpr double kWarnAt = 0.85;     ///< Level 1
constexpr double kWarnRearm = 0.80;  ///< Level 1 shows again only after falling under this
constexpr double kPauseAt = 0.93;    ///< Level 2
constexpr double kPauseRearm = 0.88; ///< Level 2 pauses again after falling under this (or 5 %
                                     ///< under where it paused, when a "high" step paused it)...
constexpr double kPauseAgain = 0.03; ///< ...or after growing this share of the limit more
constexpr double kPauseRearmBelowPause = 0.05;
constexpr std::int64_t kWarnGapMs = 120'000;
constexpr std::int64_t kPeriodMs = 2'000;

/// The scope's counters at one moment.
struct Sample {
    bool ok = false;               ///< the cgroup has a memory.max (not "max")
    std::uint64_t current = 0;     ///< memory.current
    std::uint64_t max = 0;         ///< memory.max
    std::uint64_t high = 0;        ///< memory.high (0: "max", none)
    std::uint64_t swap = 0;        ///< memory.swap.current
    std::uint64_t cache = 0;       ///< memory.stat file - shmem
    std::uint64_t high_events = 0; ///< memory.events "high"

    /// What counts toward the limit and cannot just be dropped: RAM less the page cache, plus swap.
    [[nodiscard]] std::uint64_t Used() const {
        return (current > cache ? current - cache : 0) + swap;
    }
};

/// The folder of this process's cgroup (`cgroup_root` + the path in `proc`/self/cgroup) when it
/// has a memory limit, else "" (no scope, or memory.max is "max"). `proc` and `cgroup_root` are
/// for tests.
inline std::string ScopeDir(const std::string& proc = "/proc",
                            const std::string& cgroup_root = "/sys/fs/cgroup") {
    namespace PB = VideoCore::PreloadBudget;
    const auto self = PB::ReadText(proc + "/self/cgroup");
    const std::string path = self ? PB::ParseCgroupPath(*self) : std::string{};
    if (path.empty() || path == "/") {
        return {};
    }
    const std::string dir = cgroup_root + path;
    const auto max = PB::ReadText(dir + "/memory.max");
    if (!max || !PB::ParseLimit(*max)) {
        return {};
    }
    return dir;
}

/// Reads the counters of `dir` (ScopeDir's). Five small files: cheap every 2 s.
inline Sample Read(const std::string& dir) {
    namespace PB = VideoCore::PreloadBudget;
    Sample s;
    const auto max = PB::ReadText(dir + "/memory.max");
    const auto max_v = max ? PB::ParseLimit(*max) : std::nullopt;
    const auto current = PB::ReadText(dir + "/memory.current");
    const auto current_v = current ? PB::ParseLimit(*current) : std::nullopt;
    if (!max_v || *max_v == 0 || !current_v) {
        return s;
    }
    s.ok = true;
    s.max = *max_v;
    s.current = *current_v;
    if (const auto high = PB::ReadText(dir + "/memory.high")) {
        s.high = PB::ParseLimit(*high).value_or(0);
    }
    if (const auto swap = PB::ReadText(dir + "/memory.swap.current")) {
        s.swap = PB::ParseLimit(*swap).value_or(0);
    }
    if (const auto stat = PB::ReadText(dir + "/memory.stat")) {
        const std::uint64_t file = PB::ParseStat(*stat, "file");
        const std::uint64_t shmem = PB::ParseStat(*stat, "shmem");
        s.cache = file > shmem ? file - shmem : 0;
    }
    if (const auto events = PB::ReadText(dir + "/memory.events")) {
        s.high_events = PB::ParseStat(*events, "high");
    }
    return s;
}

enum class Action { None, Warn, Pause };

/// The two levels' rules, fed one Sample at a time.
class Watcher {
public:
    Action Step(const Sample& s, std::int64_t now_ms) {
        if (!s.ok || s.max == 0) {
            critical = false;
            return Action::None;
        }
        const std::uint64_t used = s.Used();
        const double share = static_cast<double>(used) / static_cast<double>(s.max);
        const bool high_step = have_events && s.high_events > last_high_events;
        last_high_events = s.high_events;
        have_events = true;

        if (share < kWarnRearm) {
            warn_armed = true;
        }
        if (pause_latched && share < std::min(kPauseRearm, static_cast<double>(pause_used) /
                                                                   static_cast<double>(s.max) -
                                                               kPauseRearmBelowPause)) {
            pause_latched = false;
        }
        Action a = Action::None;
        if (share >= kPauseAt || (high_step && share >= kWarnAt)) {
            const auto again = static_cast<std::uint64_t>(kPauseAgain * static_cast<double>(s.max));
            if (!pause_latched || used >= pause_used + again) {
                pause_latched = true;
                pause_used = used;
                a = Action::Pause;
                // The dialog says it all: no toast on top of it
                warn_armed = false;
                warned_any = true;
                last_warn_ms = now_ms;
            }
        }
        if (a == Action::None && share >= kWarnAt && warn_armed &&
            (!warned_any || now_ms - last_warn_ms >= kWarnGapMs)) {
            a = Action::Warn;
            warn_armed = false;
            warned_any = true;
            last_warn_ms = now_ms;
        }
        critical = share >= kPauseAt || pause_latched;
        return a;
    }

    /// At Level 2 (or paused for it and not yet under kPauseRearm): autosaves wait.
    [[nodiscard]] bool Critical() const {
        return critical;
    }

private:
    bool warn_armed = true;
    bool warned_any = false;
    std::int64_t last_warn_ms = 0;
    bool pause_latched = false;
    std::uint64_t pause_used = 0;
    bool have_events = false;
    std::uint64_t last_high_events = 0;
    bool critical = false;
};

// ---- Texts (Title Case like Azahar's own; the numbers as SteamOS counts them, GbText) ----------

/// "7.1 of 8.2 GB"
inline std::string UsedOf(std::uint64_t used, std::uint64_t max) {
    namespace PB = VideoCore::PreloadBudget;
    return PB::GbNumber(static_cast<std::int64_t>(used)) + " of " +
           PB::GbText(static_cast<std::int64_t>(max));
}

/// Level 1's toast: "Running Low on Memory: 7.1 of 8.2 GB".
inline std::string WarnText(std::uint64_t used, std::uint64_t max) {
    return "Running Low on Memory: " + UsedOf(used, max);
}

struct PauseText {
    std::string title;
    std::string body;
    std::string textures; ///< "" when custom textures are off (no such button)
    std::string resume;
    std::string stop;
};

/// Level 2's dialog. No Save State button on purpose: writing one needs about 0.9 GB more for a
/// moment (the serialized system and its compressed copy), which at this level could cross the
/// limit and get Azahar closed in the middle of it. Saving in the game costs nothing.
inline PauseText MakePauseText(std::uint64_t used, std::uint64_t max, bool custom_textures) {
    PauseText t;
    t.title = "Azahar Is Almost Out of Memory";
    t.body = "Using " + UsedOf(used, max) +
             ". The game is paused so Azahar isn't closed and your progress stays safe. " +
             (custom_textures ? "Turn off custom textures, then save in the game."
                              : "Save in the game, then stop it.");
    if (custom_textures) {
        t.textures = "Turn Off Custom Textures";
    }
    t.resume = "Resume";
    t.stop = "Stop Game";
    return t;
}

} // namespace FrameXr::MemWatch
