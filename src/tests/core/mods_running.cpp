// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame catalog3: downloads while a game runs. The running game's mods/<TID>/ and
// load/textures/<TID>/ are never touched: an install there is staged in the library and goes in when
// the game stops or at the next boot; a removal waits the same way. And the download runs at the
// lowest priority (nice 19, idle I/O).

#include <atomic>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <map>
#include <mutex>
#include <thread>
#include <tuple>
#include <catch2/catch_test_macros.hpp>
#include "core/mods/library.h"
#ifdef __linux__
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

using namespace Mods;
namespace fs = std::filesystem;
using V = std::vector<std::string>;

namespace {

constexpr u64 kTid = 0x0004000000033500ULL;
constexpr u64 kOther = 0x0004000000033600ULL;
constexpr const char* kTidText = "0004000000033500";

std::string TempDir(const char* name) {
    const std::string dir = std::string("/tmp/azf_running_test_") + name + "/";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

std::string Read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

V Tree(const std::string& dir) {
    V out;
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        return out;
    }
    for (const auto& e : fs::recursive_directory_iterator(dir)) {
        if (e.is_regular_file()) {
            out.push_back(e.path().lexically_relative(dir).generic_string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Two mods for the OoT title, both writing romfs/a.bin (they clash); "one" also has a texture.
const char* kCatalog = R"({"schema":1,"mods":[
 {"id":"one","name":"One","title_ids":["0004000000033500","0004000000033600"],"author":"A",
  "source":{"type":"github_repo_path","repo":"o/r","ref":"main","path":"one/a.bin"},
  "install":{"rules":[{"from":"a.bin","to":"romfs/a.bin"}]}},
 {"id":"two","name":"Two","title_ids":["0004000000033500"],"author":"B",
  "source":{"type":"github_repo_path","repo":"o/r","ref":"main","path":"two/a.bin"},
  "install":{"rules":[{"from":"a.bin","to":"romfs/a.bin"}]}},
 {"id":"tex","name":"Tex","title_ids":["0004000000033500"],"author":"C",
  "source":{"type":"github_repo_path","repo":"o/r","ref":"main","path":"tex/tex1_1x1_AAAA_1_mip0.png"},
  "textures":{"rules":[{"from":"tex1_1x1_AAAA_1_mip0.png","to":"UI/tex1_1x1_AAAA_1_mip0.png"}]}}
]})";

Library Make(const std::string& dir) {
    auto parsed = ParseCatalog(kCatalog);
    REQUIRE(parsed.has_value());
    return Library(dir, *parsed, [](const std::string&, const std::string& path, std::size_t) {
        HttpReply r;
        r.status = 200;
        if (path.find("/one/a.bin") != std::string::npos) {
            r.body = "ONE";
        } else if (path.find("/two/a.bin") != std::string::npos) {
            r.body = "TWO";
        } else if (path.find("/tex/") != std::string::npos) {
            r.body = "PNG";
        } else {
            r.status = 404;
        }
        return r;
    });
}

} // namespace

TEST_CASE("Running: an install for the running game is staged, then applied when it stops", "[mods][running]") {
    const std::string dir = TempDir("stage");
    auto library = Make(dir);
    const std::string mods = dir + "mods/" + kTidText + "/";
    const std::string tex = dir + "textures/" + kTidText + "/tex/";
    library.SetRunningTitle(kTid);
    CHECK_FALSE(library.Busy()); // AzaharFrame jsoncrash: idle, so a list does not wait

    const auto one = library.Install(kTid, "one");
    REQUIRE(one.result == InstallResult::Ok);
    CHECK(one.staged);
    const auto textured = library.Install(kTid, "tex");
    REQUIRE(textured.result == InstallResult::Ok);
    CHECK(textured.staged);
    CHECK(textured.textures_installed);
    CHECK(library.GetState().text == "Tex Downloaded");
    // nothing in the running game's folders
    CHECK(Tree(mods).empty());
    CHECK(Tree(dir + "textures/").empty());
    // downloaded into the library, shown as installed and waiting
    CHECK(Read(dir + "mod-library/" + kTidText + "/one/files/romfs/a.bin") == "ONE");
    const auto loaded = library.ListLoaded(kTid);
    REQUIRE(loaded.size() == 2);
    for (const auto& m : loaded) {
        CHECK(m.state == ModState::Active);
        CHECK(m.pending == "activate");
    }
    for (const auto& a : library.ListAvailable(kTid)) {
        if (a.id == "one" || a.id == "tex") {
            CHECK(a.active);
            CHECK(a.pending);
        }
    }
    // installing it again is no news
    CHECK(library.Install(kTid, "one").result == InstallResult::AlreadyInstalled);
    CHECK(Tree(mods).empty());
    // another game's folders are not the running game's: a mod for it goes in at once
    CHECK(library.Install(kOther, "one").result == InstallResult::Ok);
    CHECK_FALSE(library.Install(kOther, "one").staged);
    CHECK(Read(dir + "mods/" + std::string("0004000000033600") + "/romfs/a.bin") == "ONE");

    // the game is still running: applying changes nothing for it
    CHECK(library.ApplyPending() == 0);
    CHECK(Tree(mods).empty());

    // it stops
    library.SetRunningTitle(0);
    CHECK(library.ApplyPending() == 2);
    CHECK(Tree(mods) == V{"romfs/a.bin"});
    CHECK(Read(mods + "romfs/a.bin") == "ONE");
    CHECK(Read(tex + "UI/tex1_1x1_AAAA_1_mip0.png") == "PNG");
    for (const auto& m : library.ListLoaded(kTid)) {
        CHECK(m.pending.empty());
        CHECK(m.state == ModState::Active);
    }
    CHECK(library.ApplyPending() == 0); // once
}

TEST_CASE("Running: a staged mod is applied at the next boot of the game", "[mods][running]") {
    const std::string dir = TempDir("boot");
    {
        auto library = Make(dir);
        library.SetRunningTitle(kTid);
        REQUIRE(library.Install(kTid, "one").staged);
        // the program exits without the game stopping cleanly: the manifest keeps the change
    }
    auto library = Make(dir); // the next run
    const std::string mods = dir + "mods/" + kTidText + "/";
    CHECK(Tree(mods).empty());
    const auto before = library.ListLoaded(kTid);
    REQUIRE(before.size() == 1);
    CHECK(before[0].pending == "activate");
    // BootGame: LoadROM clears the running title and applies what waited, before the loader reads
    library.SetRunningTitle(0);
    CHECK(library.ApplyPending() == 1);
    CHECK(Read(mods + "romfs/a.bin") == "ONE");
    library.SetRunningTitle(kTid); // and the game runs
    CHECK(Read(mods + "romfs/a.bin") == "ONE");
}

TEST_CASE("Running: removing an installed mod waits for the game to stop", "[mods][running]") {
    const std::string dir = TempDir("remove");
    auto library = Make(dir);
    const std::string mods = dir + "mods/" + kTidText + "/";
    REQUIRE(library.Install(kTid, "one").result == InstallResult::Ok);
    REQUIRE(library.Install(kTid, "tex").result == InstallResult::Ok);
    CHECK(Read(mods + "romfs/a.bin") == "ONE");
    library.SetRunningTitle(kTid);

    SECTION("Remove") {
        REQUIRE(library.Remove(kTid, "one").ok);
        CHECK(Read(mods + "romfs/a.bin") == "ONE"); // the running game keeps its files
        CHECK(fs::exists(dir + "mod-library/" + kTidText + "/one"));
        const auto list = library.ListLoaded(kTid);
        const auto one = std::find_if(list.begin(), list.end(), [](const auto& m) { return m.id == "one"; });
        REQUIRE(one != list.end());
        CHECK(one->state == ModState::Inactive);
        CHECK(one->pending == "remove");
        for (const auto& a : library.ListAvailable(kTid)) {
            if (a.id == "one") {
                CHECK_FALSE(a.active);
                CHECK_FALSE(a.in_library);
            }
        }
        // the clash is gone for the mods that follow: "two" is staged without a conflict
        const auto two = library.Install(kTid, "two");
        CHECK(two.result == InstallResult::Ok);
        CHECK(two.staged);
        library.SetRunningTitle(0);
        CHECK(library.ApplyPending() == 2);
        CHECK(Read(mods + "romfs/a.bin") == "TWO");
        CHECK_FALSE(fs::exists(dir + "mod-library/" + kTidText + "/one"));
        CHECK(fs::exists(dir + "textures/" + kTidText + "/tex/UI/tex1_1x1_AAAA_1_mip0.png"));
    }
    SECTION("Remove with textures") {
        REQUIRE(library.Remove(kTid, "tex").ok);
        CHECK(fs::exists(dir + "textures/" + kTidText + "/tex/UI/tex1_1x1_AAAA_1_mip0.png"));
        library.SetRunningTitle(0);
        library.ApplyPending();
        CHECK(Tree(dir + "textures/").empty());
        CHECK_FALSE(fs::exists(dir + "mod-library/" + kTidText + "/tex"));
    }
    SECTION("turned off, then on again before the game stops") {
        REQUIRE(library.Deactivate(kTid, "one").ok);
        CHECK(Read(mods + "romfs/a.bin") == "ONE");
        for (const auto& m : library.ListLoaded(kTid)) {
            CHECK((m.id == "one") == (m.state != ModState::Active));
        }
        CHECK(library.Install(kTid, "one").result == InstallResult::AlreadyInstalled); // the change is dropped
        library.SetRunningTitle(0);
        CHECK(library.ApplyPending() == 0);
        CHECK(Read(mods + "romfs/a.bin") == "ONE");
    }
    SECTION("turned off") {
        REQUIRE(library.Deactivate(kTid, "one").ok);
        library.SetRunningTitle(0);
        CHECK(library.ApplyPending() == 1);
        CHECK(Tree(mods).empty());
        CHECK(fs::exists(dir + "mod-library/" + kTidText + "/one")); // kept in the library
        CHECK(library.Install(kTid, "one").result == InstallResult::Ok);
        CHECK(Read(mods + "romfs/a.bin") == "ONE");
    }
    SECTION("a staged mod removed before the game stops is never applied") {
        REQUIRE(library.Install(kOther, "one").result == InstallResult::Ok); // (not the running game)
        library.SetRunningTitle(kTid);
        REQUIRE(library.Remove(kTid, "one").ok);
        REQUIRE(library.Remove(kTid, "tex").ok);
        library.SetRunningTitle(0);
        library.ApplyPending();
        CHECK(Tree(mods).empty());
        CHECK(Tree(dir + "textures/").empty());
    }
}

TEST_CASE("Running: a clash while staging asks, Replace swaps both on stop", "[mods][running]") {
    const std::string dir = TempDir("clash");
    auto library = Make(dir);
    const std::string mods = dir + "mods/" + kTidText + "/";
    library.SetRunningTitle(kTid);
    REQUIRE(library.Install(kTid, "one").staged);
    const auto clash = library.Install(kTid, "two");
    REQUIRE(clash.result == InstallResult::Conflict);
    CHECK(clash.conflict_id == "one");
    const auto replaced = library.Install(kTid, "two", true);
    REQUIRE(replaced.result == InstallResult::Ok);
    CHECK(replaced.staged);
    CHECK(Tree(mods).empty());
    library.SetRunningTitle(0);
    library.ApplyPending();
    CHECK(Read(mods + "romfs/a.bin") == "TWO");
    const auto list = library.ListLoaded(kTid);
    for (const auto& m : list) {
        CHECK((m.id == "two") == (m.state == ModState::Active));
    }
}

namespace {
struct Calls {
    std::mutex mutex;
    std::vector<std::tuple<int, int, int>> nice, io;
    int tid_calls = 0;
};
} // namespace

TEST_CASE("Running: the install lowers its thread's priority (mocked calls)", "[mods][running]") {
    Calls calls;
    Multipart::PriorityBackend backend;
    backend.thread_id = [&] {
        std::lock_guard lock{calls.mutex};
        ++calls.tid_calls;
        return 4242;
    };
    backend.set_priority = [&](int which, int who, int prio) {
        std::lock_guard lock{calls.mutex};
        calls.nice.emplace_back(which, who, prio);
        return 0;
    };
    backend.set_io_priority = [&](int which, int who, int ioprio) {
        std::lock_guard lock{calls.mutex};
        calls.io.emplace_back(which, who, ioprio);
        return 0L;
    };
    Multipart::SetPriorityBackend(backend);
    const std::string dir = TempDir("prio");
    auto library = Make(dir);
    REQUIRE(library.Install(kTid, "one").result == InstallResult::Ok);
    Multipart::SetPriorityBackend({}); // the real calls again
    REQUIRE(calls.nice.size() == 1);
    REQUIRE(calls.io.size() == 1);
    CHECK(calls.nice[0] == std::make_tuple(0, 4242, 19));      // PRIO_PROCESS, the thread, nice 19
    CHECK(calls.io[0] == std::make_tuple(1, 4242, 3 << 13));   // IOPRIO_WHO_PROCESS, class idle
    CHECK(Read(dir + "mods/" + kTidText + "/romfs/a.bin") == "ONE");
    CHECK(Multipart::LowerThisThreadPriority() == true); // (with no backend set: the real one, below)
}

#ifdef __linux__
TEST_CASE("Running: the real calls set nice 19 and idle I/O on the thread", "[mods][running]") {
    int nice_value = -99;
    long ioprio = -1;
    std::thread worker([&] {
        REQUIRE(Multipart::LowerThisThreadPriority());
        const int tid = static_cast<int>(::syscall(SYS_gettid));
        errno = 0;
        nice_value = ::getpriority(PRIO_PROCESS, static_cast<id_t>(tid));
        ioprio = ::syscall(SYS_ioprio_get, 1, tid);
    });
    worker.join();
    CHECK(nice_value >= 19);
    CHECK((ioprio >> 13) == 3); // IOPRIO_CLASS_IDLE
}
#endif
