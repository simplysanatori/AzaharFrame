// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame memguard: may a texture pack be preloaded? Preloading reads every file of the pack
// into memory before the game starts. On the Frame that memory is shared with SteamVR, the
// passthrough cameras, the compositor and the GPU (unified memory). The old rule allowed all of
// RAM but 2 GiB: a 6 GB pack (Ocarina of Time 3D 4K as ASTC 4x4) filled what SteamVR left, the
// system thrashed in zram, and a watchdog rebooted the headset with no OOM kill.
//
// The rule now: the pack's estimated size must fit in what the system can still hand out
// (MemAvailable) less a reserve for everything else, and in what this process's memory cgroup
// still allows (the launcher's scope, azaharframe.sh) less a reserve for the game itself. While
// preloading the same room is read again every few textures, and the preload stops when it falls
// under a reserve. Pure parsing and arithmetic, plus ReadRoom (unit-tested with a fake /proc and
// /sys/fs/cgroup in src/tests/frame_xr/preload_budget.cpp).

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace VideoCore::PreloadBudget {

constexpr std::uint64_t kMiB = std::uint64_t{1} << 20;
constexpr std::uint64_t kGiB = std::uint64_t{1} << 30;

/// Left to the rest of the system: SteamVR, passthrough, the compositor, Steam. At least 4 GiB,
/// or a quarter of RAM when that is more.
constexpr std::uint64_t kMinSystemReserve = 4 * kGiB;
/// Left inside this process's own memory limit (cgroup): the game, the emulator, the GPU copies
/// of the textures it uploads. At least 1.5 GiB, or 20 % of the limit when that is more.
constexpr std::uint64_t kMinCgroupReserve = 1536 * kMiB;
/// The estimate must fit with this share to spare (file sizes are not the whole cost: the
/// vectors, the material map, the page cache charged while reading).
constexpr double kMargin = 1.10;
/// The room is read again after this many materials or bytes, whichever comes first.
constexpr std::size_t kRecheckEveryMaterials = 8;
constexpr std::uint64_t kRecheckEveryBytes = 64 * kMiB;

inline std::uint64_t SystemReserve(std::uint64_t total_bytes) {
    return std::max(kMinSystemReserve, total_bytes / 4);
}

inline std::uint64_t CgroupReserve(std::uint64_t limit_bytes) {
    return std::max(kMinCgroupReserve, limit_bytes / 5);
}

/// What the system and this process's cgroup can still hand out. Bytes; -1 when unknown.
struct Room {
    std::int64_t total = -1;     ///< MemTotal
    std::int64_t available = -1; ///< MemAvailable
    bool cgroup_limited = false;
    std::uint64_t cgroup_limit =
        0; ///< the tightest memory.max / memory.high of this cgroup or a parent
    std::uint64_t cgroup_used =
        0;              ///< that cgroup's memory.current less its page cache (file - shmem)
    std::string cgroup; ///< which cgroup set the limit (for the log)
    std::uint64_t memory_max = 0; ///< the smallest memory.max of this cgroup or a parent; 0 none

    [[nodiscard]] std::int64_t CgroupFree() const {
        return cgroup_limit > cgroup_used ? static_cast<std::int64_t>(cgroup_limit - cgroup_used)
                                          : 0;
    }
};

/// MemTotal and MemAvailable from the text of /proc/meminfo, in bytes (-1 when missing).
inline void ParseMeminfo(std::string_view text, Room& room) {
    std::istringstream in{std::string{text}};
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string key;
        std::int64_t kb = 0;
        if (!(ls >> key >> kb)) {
            continue;
        }
        if (key == "MemTotal:") {
            room.total = kb * 1024;
        } else if (key == "MemAvailable:") {
            room.available = kb * 1024;
        }
    }
}

/// The cgroup v2 path of the process from the text of /proc/<pid>/cgroup ("0::/a/b"), or "".
inline std::string ParseCgroupPath(std::string_view text) {
    std::istringstream in{std::string{text}};
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("0::", 0) == 0) {
            return line.substr(3);
        }
    }
    return {};
}

/// memory.max / memory.high: a byte count, or nullopt for "max" (no limit) and junk.
inline std::optional<std::uint64_t> ParseLimit(std::string_view text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) {
        text.remove_suffix(1);
    }
    if (text.empty() || text == "max") {
        return std::nullopt;
    }
    std::uint64_t v = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        v = v * 10 + static_cast<std::uint64_t>(c - '0');
    }
    return v;
}

/// One field ("file", "shmem") of the text of memory.stat; 0 when missing.
inline std::uint64_t ParseStat(std::string_view text, std::string_view field) {
    std::istringstream in{std::string{text}};
    std::string key;
    std::uint64_t v = 0;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        if ((ls >> key >> v) && key == field) {
            return v;
        }
    }
    return 0;
}

inline std::optional<std::string> ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return std::nullopt;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/// Reads the room now. `proc` and `cgroup_root` are for tests ("/proc", "/sys/fs/cgroup").
/// Every cgroup from this process's own up to the root is looked at; the one with the least room
/// left under its memory.max (or memory.high, which throttles) is the one that counts.
inline Room ReadRoom(const std::string& proc = "/proc",
                     const std::string& cgroup_root = "/sys/fs/cgroup") {
    Room room;
    if (const auto meminfo = ReadText(proc + "/meminfo")) {
        ParseMeminfo(*meminfo, room);
    }
    const auto self = ReadText(proc + "/self/cgroup");
    std::string path = self ? ParseCgroupPath(*self) : std::string{};
    while (!path.empty() && path != "/") {
        const std::string dir = cgroup_root + path;
        std::optional<std::uint64_t> limit;
        if (const auto max = ReadText(dir + "/memory.max")) {
            limit = ParseLimit(*max);
            if (limit && (room.memory_max == 0 || *limit < room.memory_max)) {
                room.memory_max = *limit;
            }
        }
        if (const auto high = ReadText(dir + "/memory.high")) {
            if (const auto h = ParseLimit(*high); h && (!limit || *h < *limit)) {
                limit = h;
            }
        }
        if (limit) {
            const auto current = ReadText(dir + "/memory.current");
            const auto stat = ReadText(dir + "/memory.stat");
            const std::uint64_t used_all = current ? ParseLimit(*current).value_or(0) : 0;
            const std::uint64_t file = stat ? ParseStat(*stat, "file") : 0;
            const std::uint64_t shmem = stat ? ParseStat(*stat, "shmem") : 0;
            const std::uint64_t cache = file > shmem ? file - shmem : 0;
            const std::uint64_t used = used_all > cache ? used_all - cache : 0;
            const std::int64_t left = *limit > used ? static_cast<std::int64_t>(*limit - used) : 0;
            if (!room.cgroup_limited || left < room.CgroupFree()) {
                room.cgroup_limited = true;
                room.cgroup_limit = *limit;
                room.cgroup_used = used;
                room.cgroup = path;
            }
        }
        const std::size_t slash = path.find_last_of('/');
        path = slash == std::string::npos || slash == 0 ? std::string{} : path.substr(0, slash);
    }
    return room;
}

/// What may be preloaded now, in bytes: the smaller of the system's and the cgroup's room after
/// their reserves. -1 when neither is known (no /proc/meminfo, no cgroup limit).
/// `system_reserve` 0: SystemReserve(total).
inline std::int64_t Budget(const Room& room, std::uint64_t system_reserve = 0) {
    std::int64_t budget = -1;
    if (room.available >= 0) {
        const std::uint64_t reserve =
            system_reserve
                ? system_reserve
                : SystemReserve(static_cast<std::uint64_t>(std::max<std::int64_t>(room.total, 0)));
        budget = std::max<std::int64_t>(room.available - static_cast<std::int64_t>(reserve), 0);
    }
    if (room.cgroup_limited) {
        const std::int64_t cg = std::max<std::int64_t>(
            room.CgroupFree() - static_cast<std::int64_t>(CgroupReserve(room.cgroup_limit)), 0);
        budget = budget < 0 ? cg : std::min(budget, cg);
    }
    return budget;
}

/// Preload a pack estimated at `need` bytes? Unknown room (-1): yes, as before.
inline bool ShouldPreload(std::uint64_t need, const Room& room, std::uint64_t system_reserve = 0) {
    const std::int64_t budget = Budget(room, system_reserve);
    if (budget < 0) {
        return true;
    }
    return static_cast<double>(need) * kMargin <= static_cast<double>(budget);
}

/// Stop a preload under way? Yes once the system or the cgroup has less room than its reserve.
inline bool ShouldStop(const Room& room, std::uint64_t system_reserve = 0) {
    if (room.available >= 0) {
        const std::uint64_t reserve =
            system_reserve
                ? system_reserve
                : SystemReserve(static_cast<std::uint64_t>(std::max<std::int64_t>(room.total, 0)));
        if (room.available < static_cast<std::int64_t>(reserve)) {
            return true;
        }
    }
    return room.cgroup_limited &&
           room.CgroupFree() < static_cast<std::int64_t>(CgroupReserve(room.cgroup_limit));
}

/// Width and height from the first 24 bytes of a PNG (signature, IHDR length, "IHDR", w, h).
inline bool ParsePngSize(std::span<const std::uint8_t> head, std::uint32_t& width,
                         std::uint32_t& height) {
    static constexpr std::uint8_t kSig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (head.size() < 24 || !std::equal(kSig, kSig + 8, head.begin()) || head[12] != 'I' ||
        head[13] != 'H' || head[14] != 'D' || head[15] != 'R') {
        return false;
    }
    const auto be32 = [&](std::size_t at) {
        return (std::uint32_t{head[at]} << 24) | (std::uint32_t{head[at + 1]} << 16) |
               (std::uint32_t{head[at + 2]} << 8) | std::uint32_t{head[at + 3]};
    };
    width = be32(16);
    height = be32(20);
    return width > 0 && height > 0 && width <= 16384 && height <= 16384;
}

/// The memory one preloaded file holds: a DDS or KTX file as it is on disk; a PNG decoded to RGBA8
/// (width x height x 4; when the header could not be read, 4x the file, a typical PNG ratio).
inline std::uint64_t FileCost(bool is_png, std::uint64_t file_size, std::uint32_t png_width = 0,
                              std::uint32_t png_height = 0) {
    if (!is_png) {
        return file_size;
    }
    if (png_width && png_height) {
        return std::uint64_t{png_width} * png_height * 4;
    }
    return file_size * 4;
}

/// The system reserve set for this run (AZAHARFRAME_PRELOAD_RESERVE_MB), 0 for the default.
inline std::uint64_t ReserveFromEnv() {
    const char* env = std::getenv("AZAHARFRAME_PRELOAD_RESERVE_MB");
    return env && *env ? std::strtoull(env, nullptr, 10) * kMiB : 0;
}

/// What the Preload Custom Textures question shows, from the same numbers the preload decides
/// with (Budget, ShouldPreload), so the two cannot disagree.
struct Summary {
    bool has_pack = false;        ///< a game runs with a pack loaded: `need` is its estimate
    std::uint64_t need = 0;       ///< CustomTexManager::PreloadEstimate
    std::int64_t total = -1;      ///< MemTotal
    std::int64_t in_use = -1;     ///< MemTotal - MemAvailable
    std::uint64_t memory_max = 0; ///< Azahar's limit (the launcher's scope), 0 none
    bool fits = true;             ///< ShouldPreload
    std::int64_t spare = 0;       ///< what the budget has left after the pack and its margin
};

inline Summary Summarize(std::optional<std::uint64_t> need, const Room& room,
                         std::uint64_t system_reserve = 0) {
    Summary s;
    s.total = room.total;
    s.in_use = room.total >= 0 && room.available >= 0 ? room.total - room.available : -1;
    s.memory_max = room.memory_max;
    if (need && *need > 0) {
        s.has_pack = true;
        s.need = *need;
        s.fits = ShouldPreload(*need, room, system_reserve);
        const std::int64_t budget = Budget(room, system_reserve);
        s.spare = budget < 0
                      ? -1
                      : budget - static_cast<std::int64_t>(static_cast<double>(*need) * kMargin);
    }
    return s;
}

/// Gigabytes with one decimal, as SteamOS and `free -h` count them (1 GB = 1024^3 bytes): the
/// Frame's RAM reads 15.3 GB, not a rounded 16.
inline std::string GbText(std::int64_t bytes) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f GB",
                  static_cast<double>(std::max<std::int64_t>(bytes, 0)) /
                      static_cast<double>(kGiB));
    return buf;
}

/// The number alone ("6.3"), for "6.3 of 15.3 GB".
inline std::string GbNumber(std::int64_t bytes) {
    std::string t = GbText(bytes);
    return t.substr(0, t.size() - 3);
}

enum class Status { None, Fits, TooLarge };

struct ConfirmText {
    std::string title;
    std::string body;                                      ///< the supporting text
    std::vector<std::pair<std::string, std::string>> rows; ///< label, value
    Status status = Status::None; ///< None when no pack is loaded (no status row)
    std::string status_text;
    std::string confirm;
};

/// The question asked before Preload Custom Textures is turned on (VR menu and Flat): a Material 3
/// dialog, the title, two sentences, the numbers, a status row, Cancel and Turn On.
inline ConfirmText MakeConfirmText(const Summary& s) {
    ConfirmText t;
    t.title = "Preload Textures?";
    // Honest: without the launcher's scope nothing closes Azahar before the Frame runs out.
    t.body = s.memory_max ? "The whole pack loads into memory when the game starts, so textures "
                            "never pop in. If memory runs low, the preload is skipped, and Azahar "
                            "closes before your Frame can freeze."
                          : "The whole pack loads into memory when the game starts, so textures "
                            "never pop in. If memory runs low, the preload is skipped.";
    if (s.has_pack) {
        t.rows.emplace_back("This Pack", GbText(static_cast<std::int64_t>(s.need)));
    }
    t.rows.emplace_back("In Use Now", s.in_use < 0 ? std::string("Unknown")
                                                   : GbNumber(s.in_use) + " of " + GbText(s.total));
    t.rows.emplace_back("Azahar's Limit", s.memory_max
                                              ? GbText(static_cast<std::int64_t>(s.memory_max))
                                              : std::string("No Limit"));
    if (s.has_pack) {
        if (s.fits) {
            t.status = Status::Fits;
            t.status_text =
                s.spare >= 0 ? "Fits With " + GbText(s.spare) + " to Spare" : std::string("Fits");
        } else {
            t.status = Status::TooLarge;
            t.status_text = "Too Large to Preload Right Now";
        }
    }
    t.confirm = "Turn On";
    return t;
}

/// The toasts when a preload did not happen (both then load as needed).
constexpr const char* kTooLargeToast = "Texture Pack Too Large to Preload";
constexpr const char* kStoppedToast = "Preload Stopped: Low Memory";

/// The message the start after an out-of-memory kill shows once (VR and Flat). `limit_mib`: the
/// limit the launcher had set (its out-of-memory file), 0 when unknown.
struct OutOfMemoryText {
    std::string title;
    std::string body;
};
inline OutOfMemoryText MakeOutOfMemoryText(std::uint64_t limit_mib) {
    OutOfMemoryText t;
    t.title = "Azahar Ran Out of Memory";
    const std::string used =
        limit_mib ? "the " + GbText(static_cast<std::int64_t>(limit_mib * kMiB)) + " it may use"
                  : "the memory it may use";
    t.body = "Last time, Azahar needed more than " + used +
             ", so it was closed before your Frame could freeze. A large texture pack is the "
             "usual cause, more so with Preload Textures on. Turn Preload Textures off, or use "
             "Optimize for Frame on the pack.";
    return t;
}

} // namespace VideoCore::PreloadBudget
