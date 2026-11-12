// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame savestate: what a state's header says (the build that made it, as text), the
// mismatch check and its Load Anyway flag, a failed restore as a clean error, a slot saved over,
// and Load Anyway's save data backups.

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>
#include <boost/archive/archive_exception.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/scm_rev.h"
#include "core/save_backup.h"
#include "core/savestate.h"

namespace fs = std::filesystem;
using namespace Core;

namespace {

constexpr u64 kTitle = 0x0004000000055D00ULL;
const std::string kOtherRev(40, '0'); // a revision no build has

struct TempDir {
    fs::path path;
    explicit TempDir(const char* name)
        : path(fs::temp_directory_path() /
               (std::string("azf_savestate_") + name + "_" + std::to_string(getpid()))) {
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

void WriteText(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

std::string ReadText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

} // namespace

TEST_CASE("Save states: the version a header names", "[savestate_frame]") {
    // 2.0.0 on: build_version "AZF 2.0.0"
    CHECK(FrameVersionOf("2.0.0", "AZF 2.0.0") == "2.0.0");
    CHECK(FrameVersionOf("a1b2c3d", "AZF a1b2c3d") == "a1b2c3d");
    // Before 2.0.0 the file had only build_name ("1.1.1")
    CHECK(FrameVersionOf("1.1.1", "") == "1.1.1");
    // Upstream Azahar ("2124.1") and nothing at all: another version of Azahar
    CHECK(FrameVersionOf("2124.1", "2124.1") == "");
    CHECK(FrameVersionOf("", "") == "");
}

TEST_CASE("Save states: the mismatch dialog's words", "[savestate_frame]") {
    CHECK(SaveStateMismatchTitle("1.1.1", "2.0.0") == "This Save State Is From an Older Version");
    CHECK(SaveStateMismatchTitle("1.1.10", "1.1.9") == "This Save State Is From a Newer Version");
    // 2.0.0 is newer than every 1.x release
    for (const char* old : {"1.0.0", "1.1.0", "1.1.1", "1.9.99", "1.99.0"}) {
        CHECK(SaveStateMismatchTitle(old, "2.0.0") == "This Save State Is From an Older Version");
        CHECK(SaveStateMismatchTitle("2.0.0", old) == "This Save State Is From a Newer Version");
    }
    CHECK(SaveStateMismatchTitle("2.1.0-rc1", "2.0.0") == "This Save State Is From a Newer Version");
    CHECK(SaveStateMismatchTitle("", "2.0.0") == "This Save State Is From Another Version");
    CHECK(SaveStateMismatchTitle("2.0.0", "a1b2c3d") == "This Save State Is From Another Version");
    CHECK(SaveStateMismatchTitle("2.0.0", "2.0.0") == "This Save State Is From Another Version");

    CHECK(SaveStateMismatchBody("1.1.1") ==
          "It was made with Azahar for Frame 1.1.1. It may not load, and it could corrupt this "
          "game's progress. Save states are NOT a replacement for saving in the game.");
    CHECK(SaveStateMismatchBody("") ==
          "It was made with another version of Azahar. It may not load, and it could corrupt this "
          "game's progress. Save states are NOT a replacement for saving in the game.");
    CHECK(SaveStateFromText("1.1.1") == "From 1.1.1");
    CHECK(SaveStateFromText("") == "From Another Version");
    CHECK(std::string(kSaveStatesAboutBody).find("NOT a replacement for saving in the game") !=
          std::string::npos);
}

TEST_CASE("Save states: the build check and Load Anyway's flag", "[savestate_frame]") {
    const std::string ours = Common::g_scm_rev;
    const auto mine = MakeStateHeader(kTitle, ours, "2.0.0", "AZF 2.0.0", 1);
    const auto older = MakeStateHeader(kTitle, kOtherRev, "1.1.1", "", 1);
    const auto other_game = MakeStateHeader(kTitle + 1, kOtherRev, "1.1.1", "", 1);

    // Strict by default: only this build's states
    CHECK(CheckStateHeader(mine, kTitle, ours, false) == HeaderCheck::Ok);
    CHECK(CheckStateHeader(older, kTitle, ours, false) == HeaderCheck::OtherBuild);
    // The flag skips the build check only
    CHECK(CheckStateHeader(older, kTitle, ours, true) == HeaderCheck::Ok);
    CHECK(CheckStateHeader(other_game, kTitle, ours, true) == HeaderCheck::OtherGame);
    auto not_state = older;
    not_state[0] = 'X';
    CHECK(CheckStateHeader(not_state, kTitle, ours, true) == HeaderCheck::NotAState);
    CHECK(CheckStateHeader(std::span<const u8>(older.data(), 100), kTitle, ours, true) ==
          HeaderCheck::NotAState);
}

TEST_CASE("Save states: a failed restore is a clean error", "[savestate_frame]") {
    const auto message = [](const std::function<void()>& f) -> std::string {
        try {
            RestoreGuarded(f);
        } catch (const SaveStateHalfLoaded& e) {
            return e.what();
        }
        return "no error";
    };
    CHECK(message([] {}) == "no error");
    CHECK(message([] { throw std::runtime_error("bad"); }) == kSaveStateNotLoaded);
    CHECK(message([] {
              throw boost::archive::archive_exception(
                  boost::archive::archive_exception::unsupported_version);
          }) == kSaveStateNotLoaded);
    CHECK(message([] { throw 42; }) == kSaveStateNotLoaded);
    CHECK(std::string(kSaveStateNotLoaded) == "This Save State Couldn't Be Loaded");
}

TEST_CASE("Save states: a slot saved over is this build's at once", "[savestate_frame]") {
    TempDir states("states");
    const std::vector<u8> body{1, 2, 3, 4};
    const std::string path = StatePathIn(states.Dir(), kTitle, 0, 3);

    // A 1.1.1 state in slot 3: dimmed, "From 1.1.1"
    WriteStateFile(path, MakeStateHeader(kTitle, kOtherRev, "1.1.1", "", 100), body);
    auto list = ListSaveStatesIn(states.Dir(), kTitle, 0);
    REQUIRE(list.size() == 1);
    CHECK(list[0].slot == 3);
    CHECK(list[0].status != SaveStateInfo::ValidationStatus::OK);
    CHECK(SaveStateFromText(FrameVersionOf(list[0].build_name, list[0].build_version)) ==
          "From 1.1.1");

    // Saved over (what System::SaveState writes): OK on the next read, nothing kept from before
    WriteStateFile(path, CurrentStateHeader(kTitle, 200), body);
    list = ListSaveStatesIn(states.Dir(), kTitle, 0);
    REQUIRE(list.size() == 1);
    CHECK(list[0].status == SaveStateInfo::ValidationStatus::OK);
    CHECK(list[0].time == 200);
    CHECK(list[0].build_name.empty());
    const auto info = GetSaveStateInfoIn(states.Dir(), kTitle, 0, 3);
    CHECK(info.slot == 3);
    CHECK(info.status == SaveStateInfo::ValidationStatus::OK);

    // Load Anyway's moment before (slot 99) is a file of its own, never in the list
    WriteStateFile(StatePathIn(states.Dir(), kTitle, 0, kBeforeLoadAnywaySlot),
                   CurrentStateHeader(kTitle, 300), body);
    CHECK(ListSaveStatesIn(states.Dir(), kTitle, 0).size() == 1);
}

TEST_CASE("Save states: Load Anyway's save data backups", "[savestate_frame]") {
    TempDir user("user");
    const fs::path save = user.path / "sdmc/title/00040000/00055d00/data/00000001";
    WriteText(save / "system.dat", "v1");
    WriteText(save / "sub/slot.bin", "game");
    const std::string root = SaveBackup::Root(user.Dir(), kTitle);
    CHECK(root == user.Dir() + "save-backups/0004000000055D00/");
    CHECK(SaveBackup::Stamp(0).size() == 15);

    // Rotation: the newest three are kept
    std::string first;
    for (int i = 1; i <= 5; i++) {
        const std::string b =
            SaveBackup::Backup(save.string(), root, "20261112-10000" + std::to_string(i));
        REQUIRE(!b.empty());
        if (i == 5) {
            first = b;
        }
    }
    const auto list = SaveBackup::List(root);
    REQUIRE(list.size() == SaveBackup::kKeep);
    CHECK(list[0] == first);
    CHECK(list[2].find("20261112-100003") != std::string::npos);
    CHECK(ReadText(fs::path(first) / "sub/slot.bin") == "game");
    // The same second twice: a second folder, not a mix
    CHECK(SaveBackup::Backup(save.string(), root, "20261112-100005") != first);
    // Not a folder: no backup
    CHECK(SaveBackup::Backup((user.path / "none").string(), root, "20261112-110000").empty());

    // Restore: the backup's files in place of the save data's, nothing left beside it
    WriteText(save / "system.dat", "broken");
    WriteText(save / "extra.bin", "x");
    REQUIRE(SaveBackup::Restore(first, save.string()));
    CHECK(ReadText(save / "system.dat") == "v1");
    CHECK(ReadText(save / "sub/slot.bin") == "game");
    CHECK(!fs::exists(save / "extra.bin"));
    CHECK(!fs::exists(save.string() + ".old"));
    CHECK(!fs::exists(save.string() + ".restore"));
    CHECK(!SaveBackup::Restore((user.path / "none").string(), save.string()));
    CHECK(ReadText(save / "system.dat") == "v1");

    // The marker: armed until disarmed
    CHECK(!SaveBackup::Armed(root));
    SaveBackup::Arm(root, first, save.string());
    const auto pending = SaveBackup::Armed(root);
    REQUIRE(pending);
    CHECK(pending->backup == first);
    CHECK(pending->save_dir == save.string());
    SaveBackup::Disarm(root);
    CHECK(!SaveBackup::Armed(root));
}
