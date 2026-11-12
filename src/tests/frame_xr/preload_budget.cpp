// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <array>
#include <filesystem>
#include <fstream>
#include <catch2/catch_test_macros.hpp>
#include "video_core/custom_textures/preload_budget.h"

// AzaharFrame memguard
using namespace VideoCore::PreloadBudget;

namespace {

constexpr std::uint64_t kMiBu = 1024 * 1024;

// The Frame: 15.3 GiB of RAM (MemTotal 16033352 kB).
constexpr std::int64_t kFrameTotal = 16033352LL * 1024;
// Ocarina of Time 3D, Henriko Magnifico 4K v6.0.0 as ASTC 4x4: 2308 KTX files, 6034295947 bytes.
constexpr std::uint64_t kOotPack = 6034295947ULL;

Room System(std::int64_t available_mib) {
    Room r;
    r.total = kFrameTotal;
    r.available = available_mib * static_cast<std::int64_t>(kMiBu);
    return r;
}

std::string Meminfo(long total_kb, long avail_kb) {
    return "MemTotal:       " + std::to_string(total_kb) +
           " kB\nMemFree:          140000 kB\nMemAvailable:   " + std::to_string(avail_kb) +
           " kB\nSwapTotal:       9064952 kB\nSwapFree:       9064952 kB\n";
}

void Write(const std::filesystem::path& p, const std::string& text) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

} // namespace

TEST_CASE("Preload budget: parsing", "[frame_xr]") {
    Room r;
    ParseMeminfo(Meminfo(16033352, 9482960), r);
    REQUIRE(r.total == 16033352LL * 1024);
    REQUIRE(r.available == 9482960LL * 1024);

    Room none;
    ParseMeminfo("", none);
    REQUIRE(none.total == -1);
    REQUIRE(none.available == -1);

    REQUIRE(ParseCgroupPath("0::/user.slice/user-1000.slice/user@1000.service/app.slice/"
                            "azaharframe-42.scope\n") ==
            "/user.slice/user-1000.slice/user@1000.service/app.slice/azaharframe-42.scope");
    REQUIRE(ParseCgroupPath("12:memory:/old\n0::/new\n") == "/new");
    REQUIRE(ParseCgroupPath("") == "");

    REQUIRE_FALSE(ParseLimit("max\n").has_value());
    REQUIRE(ParseLimit("3221225472\n").value() == 3221225472ULL);
    REQUIRE_FALSE(ParseLimit("").has_value());
    REQUIRE_FALSE(ParseLimit("12k").has_value());

    const std::string stat = "anon 1000\nfile 500\nkernel 20\nshmem 120\nfile_mapped 3\n";
    REQUIRE(ParseStat(stat, "file") == 500);
    REQUIRE(ParseStat(stat, "shmem") == 120);
    REQUIRE(ParseStat(stat, "file_mapped") == 3);
    REQUIRE(ParseStat(stat, "missing") == 0);
}

TEST_CASE("Preload budget: reserves", "[frame_xr]") {
    // A quarter of the Frame's RAM is under 4 GiB: 4 GiB.
    REQUIRE(SystemReserve(static_cast<std::uint64_t>(kFrameTotal)) == 4 * kGiB);
    REQUIRE(SystemReserve(32 * kGiB) == 8 * kGiB);
    REQUIRE(SystemReserve(0) == 4 * kGiB);
    REQUIRE(CgroupReserve(3 * kGiB) == 1536 * kMiB);
    REQUIRE(CgroupReserve(10 * kGiB) == 2 * kGiB);
}

TEST_CASE("Preload budget: the decision", "[frame_xr]") {
    SECTION("the crash: OoT3D 4K (5.6 GiB) with SteamVR running is not preloaded") {
        // What SteamVR, passthrough and the compositor leave is about 6 to 9 GiB.
        REQUIRE_FALSE(ShouldPreload(kOotPack, System(6000)));
        REQUIRE_FALSE(ShouldPreload(kOotPack, System(9000)));
        // It would need about 10.3 GiB available (5.6 x 1.1 + the 4 GiB reserve).
        REQUIRE(ShouldPreload(kOotPack, System(10500)));
    }
    SECTION("a small pack is preloaded") {
        REQUIRE(ShouldPreload(800 * kMiBu, System(6000)));
        REQUIRE(Budget(System(6000)) == static_cast<std::int64_t>((6000 - 4096) * kMiBu));
    }
    SECTION("nothing left after the reserve") {
        REQUIRE(Budget(System(3000)) == 0);
        REQUIRE_FALSE(ShouldPreload(1, System(3000)));
        REQUIRE(ShouldPreload(0, System(3000)));
    }
    SECTION("the margin: the estimate must fit with 10 % to spare") {
        const std::uint64_t budget = static_cast<std::uint64_t>(Budget(System(6096)));
        REQUIRE(budget == 2000 * kMiBu);
        REQUIRE(ShouldPreload(1800 * kMiBu, System(6096)));
        REQUIRE_FALSE(ShouldPreload(1900 * kMiBu, System(6096)));
    }
    SECTION("the reserve can be set (AZAHARFRAME_PRELOAD_RESERVE_MB)") {
        REQUIRE(Budget(System(6000), 2048 * kMiBu) == static_cast<std::int64_t>(3952 * kMiBu));
    }
    SECTION("unknown memory: preloaded as before") {
        Room unknown;
        REQUIRE(Budget(unknown) == -1);
        REQUIRE(ShouldPreload(kOotPack, unknown));
        REQUIRE_FALSE(ShouldStop(unknown));
    }
    SECTION("a 3 GiB cgroup refuses the pack, however much the system has") {
        Room r = System(12000);
        r.cgroup_limited = true;
        r.cgroup_limit = 3 * kGiB;
        r.cgroup_used = 600 * kMiBu;
        REQUIRE(Budget(r) == static_cast<std::int64_t>((3072 - 600 - 1536) * kMiBu));
        REQUIRE_FALSE(ShouldPreload(kOotPack, r));
        REQUIRE(ShouldPreload(500 * kMiBu, r));
    }
    SECTION("the launcher's scope (65 % of the Frame): the system is the limit") {
        Room r = System(6000);
        r.cgroup_limited = true;
        r.cgroup_limit = static_cast<std::uint64_t>(kFrameTotal) * 60 / 100; // MemoryHigh
        r.cgroup_used = 1500 * kMiBu;
        REQUIRE(Budget(r) == Budget(System(6000)));
    }
    SECTION("a cgroup over its limit has no room") {
        Room r = System(12000);
        r.cgroup_limited = true;
        r.cgroup_limit = 2 * kGiB;
        r.cgroup_used = 3 * kGiB;
        REQUIRE(r.CgroupFree() == 0);
        REQUIRE(Budget(r) == 0);
        REQUIRE(ShouldStop(r));
    }
}

TEST_CASE("Preload budget: stopping part way", "[frame_xr]") {
    REQUIRE_FALSE(ShouldStop(System(6000)));
    REQUIRE_FALSE(ShouldStop(System(4096)));
    REQUIRE(ShouldStop(System(4095)));
    REQUIRE(ShouldStop(System(4095), 0));
    REQUIRE_FALSE(ShouldStop(System(4095), 2048 * kMiBu));
    Room r = System(12000);
    r.cgroup_limited = true;
    r.cgroup_limit = 3 * kGiB;
    r.cgroup_used = 1500 * kMiBu;
    REQUIRE_FALSE(ShouldStop(r)); // 1572 MiB left, reserve 1536
    r.cgroup_used = 1600 * kMiBu;
    REQUIRE(ShouldStop(r));
}

TEST_CASE("Preload budget: file costs", "[frame_xr]") {
    REQUIRE(FileCost(false, 1398240) == 1398240);
    REQUIRE(FileCost(true, 100000, 1024, 512) == 1024 * 512 * 4);
    REQUIRE(FileCost(true, 100000) == 400000);

    std::array<std::uint8_t, 24> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0,    13,
                                     'I',  'H', 'D', 'R', 0,    0,    0x10, 0,    0, 0, 0x08, 0};
    std::uint32_t w = 0;
    std::uint32_t h = 0;
    REQUIRE(ParsePngSize(png, w, h));
    REQUIRE(w == 4096);
    REQUIRE(h == 2048);
    png[1] = 'X';
    REQUIRE_FALSE(ParsePngSize(png, w, h));
    REQUIRE_FALSE(ParsePngSize(std::span<const std::uint8_t>{png}.first(10), w, h));
}

TEST_CASE("Preload budget: reading the room", "[frame_xr]") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "azaharframe-preload-budget-test";
    fs::remove_all(root);
    const fs::path proc = root / "proc";
    const fs::path cg = root / "cgroup";
    Write(proc / "meminfo", Meminfo(16033352, 9482960));
    const std::string leaf = "/user.slice/user@1000.service/app.slice/azaharframe-7.scope";
    Write(proc / "self" / "cgroup", "0::" + leaf + "\n");

    SECTION("no limit anywhere") {
        Write(cg / ("." + leaf) / "memory.max", "max\n");
        const Room r = ReadRoom(proc.string(), cg.string());
        REQUIRE(r.available == 9482960LL * 1024);
        REQUIRE(r.total == 16033352LL * 1024);
        REQUIRE_FALSE(r.cgroup_limited);
    }
    SECTION("the scope's MemoryHigh, less its page cache") {
        const fs::path d = cg / ("." + leaf);
        Write(d / "memory.max", "3221225472\n");
        Write(d / "memory.high", "2147483648\n");
        Write(d / "memory.current", std::to_string(1000 * kMiBu) + "\n");
        Write(d / "memory.stat", "anon 500\nfile " + std::to_string(300 * kMiBu) + "\nshmem " +
                                     std::to_string(100 * kMiBu) + "\n");
        const Room r = ReadRoom(proc.string(), cg.string());
        REQUIRE(r.cgroup_limited);
        REQUIRE(r.cgroup_limit == 2 * kGiB);
        REQUIRE(r.cgroup_used == 800 * kMiBu); // 1000 - (300 - 100)
        REQUIRE(r.cgroup == leaf);
    }
    SECTION("a parent with less room counts") {
        const fs::path d = cg / ("." + leaf);
        Write(d / "memory.max", std::to_string(8 * kGiB) + "\n");
        Write(d / "memory.current", std::to_string(1 * kGiB) + "\n");
        const fs::path parent = cg / "user.slice";
        Write(parent / "memory.max", std::to_string(4 * kGiB) + "\n");
        Write(parent / "memory.current", std::to_string(3 * kGiB) + "\n");
        const Room r = ReadRoom(proc.string(), cg.string());
        REQUIRE(r.cgroup == "/user.slice");
        REQUIRE(r.CgroupFree() == static_cast<std::int64_t>(kGiB));
    }
    SECTION("no /proc at all") {
        const Room r = ReadRoom((root / "none").string(), cg.string());
        REQUIRE(r.available == -1);
        REQUIRE_FALSE(r.cgroup_limited);
    }
    fs::remove_all(root);
}

TEST_CASE("Preload budget: the question's text", "[frame_xr]") {
    Room r = System(6000); // 15.3 GB, 9.4 GB in use
    r.cgroup_limited = true;
    r.memory_max = 10 * kGiB;
    r.cgroup_limit = 9 * kGiB;
    r.cgroup_used = 1 * kGiB;
    SECTION("too large: the OoT3D pack") {
        const ConfirmText t = MakeConfirmText(Summarize(kOotPack, r));
        REQUIRE(t.title == "Preload Textures?");
        REQUIRE(t.rows.size() == 3);
        REQUIRE(t.rows[0] == std::pair<std::string, std::string>{"This Pack", "5.6 GB"});
        REQUIRE(t.rows[1] == std::pair<std::string, std::string>{"In Use Now", "9.4 of 15.3 GB"});
        REQUIRE(t.rows[2] == std::pair<std::string, std::string>{"Azahar's Limit", "10.0 GB"});
        REQUIRE(t.status == Status::TooLarge);
        REQUIRE(t.status_text == "Too Large to Preload Right Now");
        REQUIRE(t.body.find("Azahar closes") != std::string::npos);
        REQUIRE(t.confirm == "Turn On");
    }
    SECTION("fits: the spare is the budget less the pack and its margin") {
        const Summary s = Summarize(1 * kGiB, r);
        REQUIRE(s.fits == ShouldPreload(1 * kGiB, r));
        const ConfirmText t = MakeConfirmText(s);
        REQUIRE(t.status == Status::Fits);
        // budget 1904 MiB (6000 - 4096), less 1.1 GiB = 777.6 MiB
        REQUIRE(t.status_text == "Fits With 0.8 GB to Spare");
    }
    SECTION("no pack, no limit") {
        const ConfirmText t = MakeConfirmText(Summarize(std::nullopt, System(6000)));
        REQUIRE(t.rows.size() == 2);
        REQUIRE(t.rows[0].first == "In Use Now");
        REQUIRE(t.rows[1].second == "No Limit");
        REQUIRE(t.status == Status::None);
        REQUIRE(t.body.find("Azahar closes") == std::string::npos);
    }
    REQUIRE(std::string(kTooLargeToast) == "Texture Pack Too Large to Preload");
    REQUIRE(std::string(kStoppedToast) == "Preload Stopped: Low Memory");
}
