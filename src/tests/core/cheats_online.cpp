// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cheats: tree parsing, source tags, merge, status text, the fix install path and the
// list API (no network: a fake fetch function).

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include "common/file_util.h"
#include "core/cheats/cheats.h"
#include "core/cheats/online.h"
#include "core/core.h"

using namespace Cheats;
using namespace Cheats::Online;

namespace {

constexpr u64 kTitle = 0x0004000000173700ULL;

const char* kTree = R"({"sha":"x","truncated":false,"tree":[
 {"path":"README.md","type":"blob"},
 {"path":"games","type":"tree"},
 {"path":"games/stella-glow-usa/cheats/0004000000173700.txt","type":"blob"},
 {"path":"games/stella-glow-usa/patch/0004000000173700/code.ips","type":"blob"},
 {"path":"games/other/cheats/0004000000999999.txt","type":"blob"},
 {"path":"games/other/patch/0004000000999999/code.ips","type":"blob"}]})";

const char* kDev = "[No Bloom (USA)]\n*Zeroes the bloom\n50109558 ED8DBA02\n00109558 EDCDBA02\n\n"
                   "[Shared Name]\n00000000 00000001\n";
const char* kSharkive = "---\n[Shared Name]\n00000000 00000002\n\n[Max Gold Rev0]\n00000000 00000003\n"
                        "\n[Max Items Rev1]\n00000000 00000004\n";

std::string TempDir(const char* name) {
    const std::string dir = "/tmp/azf_cheats_test_" + std::string(name) + "/";
    FileUtil::DeleteDirRecursively(dir);
    return dir;
}

} // namespace

TEST_CASE("Online: tree parsing", "[cheats_online]") {
    auto files = ParseTree(kTree, kTitle);
    REQUIRE(files.has_value());
    REQUIRE(files->cheats == "games/stella-glow-usa/cheats/0004000000173700.txt");
    REQUIRE(files->patch == "games/stella-glow-usa/patch/0004000000173700/code.ips");

    files = ParseTree(kTree, 0x0004000000999999ULL);
    REQUIRE(files->cheats == "games/other/cheats/0004000000999999.txt");

    files = ParseTree(kTree, 0x0004000000111111ULL);
    REQUIRE(files.has_value());
    REQUIRE(files->cheats.empty());
    REQUIRE(files->patch.empty());

    std::string error;
    REQUIRE_FALSE(ParseTree("not json", kTitle, &error).has_value());
    REQUIRE_FALSE(error.empty());
    REQUIRE_FALSE(ParseTree(R"({"message":"API rate limit exceeded"})", kTitle, &error).has_value());
    REQUIRE_FALSE(ParseTree("[]", kTitle, &error).has_value());
}

TEST_CASE("Online: source tags", "[cheats_online]") {
    REQUIRE(SourceOf("") == Source::User);
    REQUIRE(SourceOf("my own note\n") == Source::User);
    REQUIRE(SourceOf("source: Developer\nnote\n") == Source::Developer);
    REQUIRE(SourceOf("note\nsource: Sharkive\n") == Source::Sharkive);
    REQUIRE(WithSource("note\n", Source::Developer) == "source: Developer\nnote\n");
    REQUIRE(WithSource("source: Sharkive\nnote\n", Source::Developer) == "source: Developer\nnote\n");
    REQUIRE(VersionNote("source: Sharkive\nVersion note: for v1.0, installed v1.1\n") ==
            "for v1.0, installed v1.1");
    REQUIRE(VersionNote("nothing").empty());
}

TEST_CASE("Online: merge", "[cheats_online]") {
    const std::vector<Fetched> fetched{{Source::Developer, Sharkive::Parse(kDev)},
                                       {Source::Sharkive, Sharkive::Parse(kSharkive)}};
    // nothing installed known: no marks
    auto m = Merge(fetched, {"Mine"}, {});
    REQUIRE(m.developer == 2);
    REQUIRE(m.sharkive == 2); // "Shared Name" came from the Developer first
    REQUIRE(m.mismatched == 0);
    REQUIRE(m.added.size() == 4);
    REQUIRE(m.added[0].name == "No Bloom (USA)");
    REQUIRE(m.added[0].comments == "source: Developer\nZeroes the bloom\n");
    REQUIRE(m.added[1].name == "Shared Name");
    REQUIRE(m.added[1].code == "00000000 00000001");
    REQUIRE(SourceOf(m.added[2].comments) == Source::Sharkive);

    // the user's own cheat of the same name is kept and not duplicated
    m = Merge(fetched, {"Shared Name", "No Bloom (USA)"}, {});
    REQUIRE(m.developer == 0);
    REQUIRE(m.sharkive == 2);

    // a cartridge at Rev1: the Rev0 cheat is flagged, for both sources
    Sharkive::InstalledVersion rev1;
    rev1.revision = 1;
    rev1.source = Sharkive::VersionSource::Ncch;
    m = Merge(fetched, {}, rev1);
    REQUIRE(m.by_revision);
    REQUIRE(m.mismatched == 1);
    REQUIRE(VersionNote(m.added[2].comments) == "for Rev0, this cartridge is Rev1");
}

TEST_CASE("Online: status text", "[cheats_online]") {
    DownloadResult r;
    REQUIRE(StatusText(r) == "No cheats for this game");
    r.found_any = true;
    REQUIRE(StatusText(r) == "No new cheats");
    r.developer_added = 1;
    REQUIRE(StatusText(r) == "1 cheat added");
    r.sharkive_added = 11;
    r.mismatched = 3;
    REQUIRE(StatusText(r) == "12 cheats added, 3 for another version");
    r.update_version = "v1.1";
    REQUIRE(StatusText(r) == "12 cheats added, 3 for another version (update v1.1 installed)");
    r.by_revision = true;
    r.update_version.clear();
    REQUIRE(StatusText(r) == "12 cheats added, 3 for another revision");
    r.error = "GitHub rate limit reached, try again later";
    REQUIRE(StatusText(r) == "Download failed: GitHub rate limit reached, try again later");
}

TEST_CASE("Online: fix install path", "[cheats_online]") {
    const std::string dir = TempDir("fix");
    const std::string ips("PATCH\x00\x10\x00\x00\x01\x42" "EOF", 14);
    const std::string ips2("PATCH\x00\x10\x00\x00\x01\x43" "EOF", 14);

    REQUIRE(GetFixStateIn(dir) == FixState::NotInstalled);
    REQUIRE(InstallFixTo(dir, "junk").result == FixResult::Error);
    REQUIRE(InstallFixTo(dir, ips).result == FixResult::Ok);
    REQUIRE(GetFixStateIn(dir) == FixState::Installed);
    REQUIRE(InstallFixTo(dir, ips).result == FixResult::AlreadyInstalled);
    REQUIRE(InstallFixTo(dir, ips2).result == FixResult::Ok); // ours, replaced by the new copy
    REQUIRE(RemoveFixFrom(dir).result == FixResult::Ok);
    REQUIRE(GetFixStateIn(dir) == FixState::NotInstalled);
    REQUIRE_FALSE(FileUtil::Exists(dir + "code.ips"));
    REQUIRE(RemoveFixFrom(dir).result == FixResult::NotAvailable);

    // the player's own code.ips is never overwritten or removed
    FileUtil::CreateFullPath(dir + "x");
    {
        FileUtil::IOFile own(dir + "code.ips", "wb");
        own.WriteString("PATCH-OWN-EOF");
    }
    REQUIRE(GetFixStateIn(dir) == FixState::OtherPatch);
    REQUIRE(InstallFixTo(dir, ips).result == FixResult::Conflict);
    REQUIRE(RemoveFixFrom(dir).result == FixResult::Conflict);
    REQUIRE(FileUtil::GetSize(dir + "code.ips") == 13);
    FileUtil::DeleteDirRecursively(dir);
}

TEST_CASE("Online: fix install with a fetch function", "[cheats_online]") {
    const std::string dir = TempDir("fixget");
    int calls = 0;
    const HttpGet get = [&](const std::string& host, const std::string& path) {
        ++calls;
        HttpResult r;
        r.status = 200;
        if (host == "https://api.github.com") {
            r.body = kTree;
        } else {
            REQUIRE(path == "/simplysanatori/3ds-game-fixes/main/games/stella-glow-usa/patch/"
                            "0004000000173700/code.ips");
            r.body = std::string("PATCH\x00\x10\x00\x00\x01\x42" "EOF", 14);
        }
        return r;
    };
    REQUIRE(InstallFixWith(get, kTitle, dir).result == FixResult::Ok);
    REQUIRE(calls == 2);
    REQUIRE(FixAvailable(kTitle));
    REQUIRE(FileUtil::GetSize(dir + "code.ips") == 14);
    REQUIRE(InstallFixWith(get, 0x0004000000111111ULL, TempDir("fixnone")).result ==
            FixResult::NotAvailable);

    const HttpGet limited = [](const std::string&, const std::string&) {
        HttpResult r;
        r.status = 403;
        r.error = "GitHub rate limit reached, try again later";
        return r;
    };
    const auto out = InstallFixWith(limited, kTitle, TempDir("fixlimit"));
    REQUIRE(out.result == FixResult::Error);
    REQUIRE(out.error == "GitHub rate limit reached, try again later");
    FileUtil::DeleteDirRecursively(dir);
}

TEST_CASE("Online: download, list and enable", "[cheats_online]") {
    FileUtil::DeleteDirRecursively("/tmp/azf_cheats_test_user/");
    FileUtil::SetUserPath("/tmp/azf_cheats_test_user/");
    FileUtil::CreateFullPath("/tmp/azf_cheats_test_user/cheats/x");
    auto& system = Core::System::GetInstance();
    CheatEngine engine(system);

    std::vector<std::string> requested;
    const HttpGet get = [&](const std::string& host, const std::string& path) {
        requested.push_back(host + path);
        HttpResult r;
        r.status = 200;
        if (host == "https://api.github.com") {
            r.body = kTree;
        } else if (path.find("Sharkive") != std::string::npos) {
            r.body = kSharkive;
        } else {
            r.body = kDev;
        }
        return r;
    };
    const auto result = DownloadCheatsWith(get, engine, kTitle, "");
    REQUIRE(result.error.empty());
    REQUIRE(result.developer_added == 2);
    REQUIRE(result.sharkive_added == 2);
    REQUIRE(result.fix_available);
    REQUIRE(requested.size() == 3); // the tree (one request), the Developer file, Sharkive's
    REQUIRE(requested[0] == "https://api.github.com/repos/simplysanatori/3ds-game-fixes/git/trees/"
                            "main?recursive=1");

    auto list = ListCheats(engine, kTitle);
    REQUIRE(list.size() == 4);
    REQUIRE(list[0].name == "No Bloom (USA)");
    REQUIRE(list[0].source == Source::Developer);
    REQUIRE_FALSE(list[0].enabled);
    REQUIRE(list[2].source == Source::Sharkive);

    REQUIRE(SetCheatEnabled(engine, kTitle, 0, true));
    REQUIRE_FALSE(SetCheatEnabled(engine, kTitle, 99, true));

    // a second download adds nothing and keeps the enabled state; the file survives a reload
    const auto again = DownloadCheatsWith(get, engine, kTitle, "");
    REQUIRE(again.developer_added + again.sharkive_added == 0);
    CheatEngine fresh(system);
    list = ListCheats(fresh, kTitle);
    REQUIRE(list.size() == 4);
    REQUIRE(list[0].enabled);
    REQUIRE(list[0].source == Source::Developer);
    FileUtil::DeleteDirRecursively("/tmp/azf_cheats_test_user/");
}

// AzaharFrame integ: a download that ends after another game started must not land in that game's
// list (nor switch the engine back to the first game's file).
TEST_CASE("Online: a download for another title is dropped", "[cheats_online]") {
    constexpr u64 kOther = 0x0004000000999999ULL;
    // SetUserPath keeps the first test's folders (the paths are set once), so use whatever it gives
    FileUtil::SetUserPath("/tmp/azf_cheats_test_stale/");
    const std::string cheats_dir = FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir);
    FileUtil::DeleteDirRecursively(cheats_dir);
    FileUtil::CreateFullPath(cheats_dir + "x");
    auto& system = Core::System::GetInstance();
    CheatEngine engine(system);

    const HttpGet get = [](const std::string& host, const std::string& path) {
        HttpResult r;
        r.status = 200;
        if (host == "https://api.github.com") {
            r.body = kTree;
        } else if (path.find("Sharkive") != std::string::npos) {
            r.body = kSharkive;
        } else {
            r.body = kDev;
        }
        return r;
    };
    const auto cheat_file = [&](u64 id) { return fmt::format("{}{:016X}.txt", cheats_dir, id); };

    // started for kTitle; kOther starts (and loads its cheat file) before the download ends
    engine.LoadCheatFile(kTitle);
    const Fetch fetch = FetchCheatsWith(get, kTitle, "");
    REQUIRE(fetch.title_id == kTitle);
    REQUIRE(fetch.result.error.empty());
    REQUIRE(fetch.fetched.size() == 2);
    engine.LoadCheatFile(kOther);

    // the frontend says kOther runs
    auto result = ApplyFetch(engine, fetch, kOther);
    REQUIRE(result.stale);
    REQUIRE(result.developer_added + result.sharkive_added == 0);
    REQUIRE(engine.GetLoadedTitleId() == kOther);
    REQUIRE(engine.GetCheats().empty());
    REQUIRE_FALSE(FileUtil::Exists(cheat_file(kTitle)));
    REQUIRE_FALSE(FileUtil::Exists(cheat_file(kOther)));
    REQUIRE(StatusText(result) == "Nothing added: the game changed");

    // a stale running title from the caller is caught by the engine's own title too
    result = ApplyFetch(engine, fetch, kTitle);
    REQUIRE(result.stale);
    REQUIRE(engine.GetLoadedTitleId() == kOther);
    REQUIRE(engine.GetCheats().empty());

    // the same title running again (restarted): the download is applied
    engine.LoadCheatFile(kTitle);
    result = ApplyFetch(engine, fetch, kTitle);
    REQUIRE_FALSE(result.stale);
    REQUIRE(result.developer_added == 2);
    REQUIRE(result.sharkive_added == 2);
    REQUIRE(engine.GetCheats().size() == 4);
    REQUIRE(FileUtil::Exists(cheat_file(kTitle)));

    // a failed fetch stays a failure, never stale
    const HttpGet down = [](const std::string&, const std::string&) {
        HttpResult r;
        r.error = "offline";
        return r;
    };
    const Fetch failed = FetchCheatsWith(down, kTitle, "");
    result = ApplyFetch(engine, failed, kOther);
    REQUIRE_FALSE(result.stale);
    REQUIRE(result.error == "offline");
    FileUtil::DeleteDirRecursively(cheats_dir);
    FileUtil::DeleteDirRecursively("/tmp/azf_cheats_test_stale/");
}
