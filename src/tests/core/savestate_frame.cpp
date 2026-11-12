// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame savestate: what a state's header says (the build that made it, as text), the
// mismatch check and its Load Anyway flag, a failed restore as a clean error, a slot saved over,
// and Load Anyway's save data backups.

#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include <boost/archive/archive_exception.hpp>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include "common/scm_rev.h"
#include "common/zstd_compression.h"
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
    // 1.2.0 on: build_version "AZF 1.2.0"
    CHECK(FrameVersionOf("1.2.0", "AZF 1.2.0") == "1.2.0");
    CHECK(FrameVersionOf("a1b2c3d", "AZF a1b2c3d") == "a1b2c3d");
    // Before 1.2.0 the file had only build_name ("1.1.1")
    CHECK(FrameVersionOf("1.1.1", "") == "1.1.1");
    // Upstream Azahar ("2124.1") and nothing at all: another version of Azahar
    CHECK(FrameVersionOf("2124.1", "2124.1") == "");
    CHECK(FrameVersionOf("", "") == "");
}

TEST_CASE("Save states: the mismatch dialog's words", "[savestate_frame]") {
    CHECK(SaveStateMismatchTitle("1.1.1", "1.2.0") == "This Save State Is From an Older Version");
    CHECK(SaveStateMismatchTitle("1.1.10", "1.1.9") == "This Save State Is From a Newer Version");
    CHECK(SaveStateMismatchTitle("1.3.0-rc1", "1.2.0") == "This Save State Is From a Newer Version");
    CHECK(SaveStateMismatchTitle("", "1.2.0") == "This Save State Is From Another Version");
    CHECK(SaveStateMismatchTitle("1.2.0", "a1b2c3d") == "This Save State Is From Another Version");
    CHECK(SaveStateMismatchTitle("1.2.0", "1.2.0") == "This Save State Is From Another Version");

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
    const auto mine = MakeStateHeader(kTitle, ours, "1.2.0", "AZF 1.2.0", 1);
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

    // The moment before a load (Undo Load's file, 98) is a file of its own, never in the list
    WriteStateFile(StatePathIn(states.Dir(), kTitle, 0, 98), CurrentStateHeader(kTitle, 300), body);
    CHECK(ListSaveStatesIn(states.Dir(), kTitle, 0).size() == 1);
}

TEST_CASE("Save states: Load Anyway's save data backups", "[savestate_frame]") {
    TempDir user("user");
    const fs::path save = user.path / "sdmc/title/00040000/00055d00/data/00000001";
    const fs::path ext = user.path / "sdmc/extdata/00000000/00000335";
    WriteText(save / "system.dat", "v1");
    WriteText(save / "sub/slot.bin", "game");
    WriteText(ext / "user/town.dat", "town v1");
    const std::vector<SaveBackup::Part> parts{{SaveBackup::kSavePart, save.string()},
                                              {SaveBackup::kExtdataPart, ext.string()}};
    const std::string root = SaveBackup::Root(user.Dir(), kTitle);
    CHECK(root == user.Dir() + "save-backups/0004000000055D00/");
    CHECK(SaveBackup::Stamp(0).size() == 15);

    // Rotation: the newest three are kept; each holds the save data and the extra data
    std::string first;
    for (int i = 1; i <= 5; i++) {
        const std::string b = SaveBackup::Backup(parts, root, "20261112-10000" + std::to_string(i));
        REQUIRE(!b.empty());
        if (i == 5) {
            first = b;
        }
    }
    const auto list = SaveBackup::List(root);
    REQUIRE(list.size() == SaveBackup::kKeep);
    CHECK(list[0] == first);
    CHECK(list[2].find("20261112-100003") != std::string::npos);
    CHECK(ReadText(fs::path(first) / "save/sub/slot.bin") == "game");
    CHECK(ReadText(fs::path(first) / "extdata/user/town.dat") == "town v1");
    // The same second twice: a second folder, not a mix
    CHECK(SaveBackup::Backup(parts, root, "20261112-100005") != first);
    // No folder at all: no backup; a game with no extra data: the save data alone
    CHECK(SaveBackup::Backup({{SaveBackup::kSavePart, (user.path / "none").string()}}, root,
                             "20261112-110000")
              .empty());
    const std::string save_only =
        SaveBackup::Backup({{SaveBackup::kSavePart, save.string()},
                            {SaveBackup::kExtdataPart, (user.path / "none").string()}},
                           root, "20261112-110001");
    REQUIRE(!save_only.empty());
    CHECK(fs::exists(fs::path(save_only) / "save/system.dat"));
    CHECK(!fs::exists(fs::path(save_only) / "extdata"));
    CHECK(SaveBackup::List(root).size() == SaveBackup::kKeep);
}

TEST_CASE("Save states: Restore keeps the save data it replaces", "[savestate_frame]") {
    TempDir user("restore");
    const fs::path save = user.path / "sdmc/title/00040000/00055d00/data/00000001";
    const fs::path ext = user.path / "sdmc/extdata/00000000/00000335";
    WriteText(save / "system.dat", "v1");
    WriteText(save / "sub/slot.bin", "game");
    WriteText(ext / "town.dat", "town v1");
    const std::vector<SaveBackup::Part> parts{{SaveBackup::kSavePart, save.string()},
                                              {SaveBackup::kExtdataPart, ext.string()}};
    const std::string root = SaveBackup::Root(user.Dir(), kTitle);
    const std::string backup = SaveBackup::Backup(parts, root, "20261112-102600");
    REQUIRE(!backup.empty());

    // Played on after Load Anyway: newer progress in both folders
    WriteText(save / "system.dat", "v2 three hours later");
    WriteText(save / "extra.bin", "x");
    WriteText(ext / "town.dat", "town v2");

    std::string kept;
    REQUIRE(SaveBackup::Restore(backup, parts, root, "20261112-133000", &kept));
    // The backup's files are in place, nothing left beside them
    CHECK(ReadText(save / "system.dat") == "v1");
    CHECK(ReadText(save / "sub/slot.bin") == "game");
    CHECK(!fs::exists(save / "extra.bin"));
    CHECK(ReadText(ext / "town.dat") == "town v1");
    CHECK(!fs::exists(save.string() + ".old"));
    CHECK(!fs::exists(save.string() + ".restore"));
    CHECK(!fs::exists(ext.string() + ".old"));
    // The newer progress was kept first, in the rotation, newest
    REQUIRE(!kept.empty());
    CHECK(fs::path(kept).filename().string() == "20261112-133000-before-restore");
    CHECK(ReadText(fs::path(kept) / "save/system.dat") == "v2 three hours later");
    CHECK(ReadText(fs::path(kept) / "save/extra.bin") == "x");
    CHECK(ReadText(fs::path(kept) / "extdata/town.dat") == "town v2");
    CHECK(SaveBackup::List(root).front() == kept);
    // ...and can be restored in turn (changed my mind), keeping what it replaces again
    std::string kept2;
    REQUIRE(SaveBackup::Restore(kept, parts, root, "20261112-133100", &kept2));
    CHECK(ReadText(save / "system.dat") == "v2 three hours later");
    CHECK(ReadText(fs::path(kept2) / "save/system.dat") == "v1");
    CHECK(SaveBackup::List(root).size() == SaveBackup::kKeep);

    // A part the backup does not hold is left as it is
    const std::string save_only =
        SaveBackup::Backup({{SaveBackup::kSavePart, save.string()}}, root, "20261112-140000");
    WriteText(ext / "town.dat", "town v3");
    REQUIRE(SaveBackup::Restore(save_only, parts, root, "20261112-140100"));
    CHECK(ReadText(ext / "town.dat") == "town v3");

    // Nothing to restore from: nothing changes
    CHECK(!SaveBackup::Restore((user.path / "none").string(), parts, root, "20261112-150000"));
    CHECK(ReadText(save / "system.dat") == "v2 three hours later");
    // A game with no save data yet: restored, nothing to keep
    fs::remove_all(save);
    fs::remove_all(ext);
    std::string none;
    REQUIRE(
        SaveBackup::Restore(SaveBackup::List(root).front(), parts, root, "20261112-150100", &none));
    CHECK(none.empty());
}

TEST_CASE("Save states: Restore's question and the marker", "[savestate_frame]") {
    TempDir user("marker");
    const std::string root = SaveBackup::Root(user.Dir(), kTitle);
    const s64 now = 1'794'000'000;
    const std::string today = SaveBackup::Stamp(now).substr(0, 8);
    CHECK(SaveBackup::TimeText(root + today + "-102612", now) == "10:26");
    CHECK(SaveBackup::TimeText(root + "20200101-102612", now) == "2020-01-01 10:26");
    CHECK(SaveBackup::TimeText(root + today + "-102612-2", now) == "10:26");
    CHECK(SaveBackup::TimeText(root + "backup", now).empty());
    CHECK(SaveBackup::RestoreQuestion("10:26") == "Restore Save Data From 10:26?");
    CHECK(SaveBackup::RestoreQuestion("") == "Restore Save Data?");
    CHECK(std::string(SaveBackup::kRestoreButton) == "Restore");
    CHECK(std::string(SaveBackup::kKeepCurrentButton) == "Keep Current");
    CHECK(SaveBackup::kOfferPlayMinutes == 10);

    // The marker: armed until disarmed, with each part's folder
    const std::vector<SaveBackup::Part> parts{{"save", "/x/save dir"}, {"extdata", "/x/ext"}};
    CHECK(!SaveBackup::Armed(root));
    SaveBackup::Arm(root, root + "20261112-102612", parts);
    const auto pending = SaveBackup::Armed(root);
    REQUIRE(pending);
    CHECK(pending->backup == root + "20261112-102612");
    CHECK(pending->parts == parts);
    SaveBackup::Disarm(root);
    CHECK(!SaveBackup::Armed(root));
    // A marker of the old form (no parts) is not offered
    WriteText(fs::path(root) / "load-anyway.pending", "/a/backup\n/a/save\n");
    CHECK(!SaveBackup::Armed(root));
}

TEST_CASE("Save states: a load that fails before restoring gets the clean notice",
          "[savestate_frame]") {
    // A state cut short (a slot written over on a full disk): its body does not decompress, so
    // LoadStateAt stops before the system is touched (a plain error, not SaveStateHalfLoaded)
    std::string raw(1 << 16, '\0');
    u32 x = 12345;
    for (char& c : raw) { // not too compressible
        x = x * 1103515245u + 12345u;
        c = static_cast<char>((x >> 16) & 0x3f);
    }
    auto body = Common::Compression::CompressDataZSTDDefault(
        std::span<const u8>{reinterpret_cast<const u8*>(raw.data()), raw.size()});
    REQUIRE(body.size() > 64);
    body.resize(body.size() / 2);
    CHECK(Common::Compression::DecompressDataZSTD(body).empty());
    CHECK(std::string(kSaveStateNotLoaded) == "This Save State Couldn't Be Loaded");
    CHECK(std::string(kSaveStateNotLoadedBody) == "The game and its save data weren't changed.");
}

// AzaharFrame advfix
TEST_CASE("Save states: a save that fails says why", "[savestate_frame]") {
    CHECK(std::string(kSaveFailedTitle) == "Couldn't Save State");
    CHECK(SaveFailedBody(SaveFailure::Unknown) ==
          "The game is still running. Try another slot, or free up space on the Frame.");
    CHECK(SaveFailedBody(SaveFailure::NoSpace) ==
          "Not Enough Space\nThe game is still running. Try another slot, or free up space on "
          "the Frame.");
    CHECK(SaveFailedBody(SaveFailure::Write).rfind("Couldn't Write the File\n", 0) == 0);
    TempDir states("savefail");
    // More than the disk holds: Not Enough Space; a folder in the way: Couldn't Write the File
    CHECK(WriteFailureAt(states.Dir() + "x/0004000000055D00.01.cst", std::size_t(1) << 62) ==
          SaveFailure::NoSpace);
    WriteText(states.path / "blocked", "a file where the folder would be");
    const std::string path = states.Dir() + "blocked/0004000000055D00.01.cst";
    CHECK_THROWS(WriteStateFile(path, CurrentStateHeader(kTitle, 1), std::vector<u8>{1, 2}));
    CHECK(WriteFailureAt(path, 300) == SaveFailure::Write);
    const SaveStateWriteFailed e(SaveFailure::NoSpace, "details");
    CHECK(e.failure == SaveFailure::NoSpace);
    CHECK(std::string(e.what()) == "details");
}

TEST_CASE("Save states: a failed save leaves the old slot loadable", "[savestate_frame]") {
    TempDir states("atomic");
    const std::string path = StatePathIn(states.Dir(), kTitle, 0, 1);
    const std::string raw(4096, 'g');
    const auto body = Common::Compression::CompressDataZSTDDefault(
        std::span<const u8>{reinterpret_cast<const u8*>(raw.data()), raw.size()});
    WriteStateFile(path, CurrentStateHeader(kTitle, 100), body);
    const std::string before = ReadText(path);
    // The next save of the slot fails (its temporary file cannot be made)
    fs::create_directories(path + ".tmp/in the way");
    CHECK_THROWS(
        WriteStateFile(path, CurrentStateHeader(kTitle, 200), std::vector<u8>(64 * 1024, 7)));
    // The old state is there whole: same bytes, its header, a body that decompresses
    CHECK(ReadText(path) == before);
    const auto info = GetSaveStateInfoIn(states.Dir(), kTitle, 0, 1);
    CHECK(info.slot == 1);
    CHECK(info.status == SaveStateInfo::ValidationStatus::OK);
    CHECK(info.time == 100);
    const std::string file = ReadText(path);
    REQUIRE(file.size() > 256);
    const auto back =
        Common::Compression::DecompressDataZSTD(std::vector<u8>(file.begin() + 256, file.end()));
    CHECK(back.size() == raw.size());
    // Saved again once the way is clear: the new state, no temporary file left
    fs::remove_all(path + ".tmp");
    WriteStateFile(path, CurrentStateHeader(kTitle, 300), body);
    CHECK(GetSaveStateInfoIn(states.Dir(), kTitle, 0, 1).time == 300);
    CHECK(!fs::exists(path + ".tmp"));
    CHECK(ListSaveStatesIn(states.Dir(), kTitle, 0).size() == 1);
}
