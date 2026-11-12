// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame compat: catalog entries for another region, release or version of the game
// (core/mods/compat.h), Install Mods' order and the crash guard's marker (core/mods/crash_guard.h).

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "core/mods/catalog.h"
#include "core/mods/compat.h"
#include "core/mods/crash_guard.h"
#include "core/mods/library.h"

using namespace Mods;
using namespace Mods::Compat;
namespace fs = std::filesystem;

namespace {

constexpr u64 kOotJpn = 0x0004000000033400ULL;
constexpr u64 kOotUsa = 0x0004000000033500ULL;
constexpr u64 kOotEur = 0x0004000000033600ULL;
constexpr u64 kMmUsa = 0x0004000000125500ULL;
constexpr u64 kMmEur = 0x0004000000125600ULL;
constexpr u64 kFatesSe = 0x000400000012DC00ULL;  // Japan, Special Edition
constexpr u64 kFatesBr = 0x000400000012DD00ULL;  // Japan, Birthright

Families OotAndMm() {
    Families f;
    f.Add("The Legend of Zelda: Ocarina of Time 3D", {kOotUsa, kOotEur});
    f.Add("The Legend of Zelda: Ocarina of Time 3D (Japan)", {kOotJpn});
    f.Add("The Legend of Zelda: Majora's Mask 3D", {kMmUsa});
    f.Add("The Legend of Zelda: Majora's Mask 3D", {kMmEur});
    f.Add("Fire Emblem Fates", {kFatesSe, kFatesBr});
    return f;
}

Game GameOf(u64 tid, std::optional<Version> version = Version{1, 0}) {
    Game g;
    g.title_id = tid;
    g.region = RegionOfTitle(tid);
    g.version = version;
    return g;
}

std::string TempDir(const char* name) {
    const std::string dir = std::string("/tmp/azf_compat_test_") + name + "/";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

} // namespace

TEST_CASE("Compat: the same game across regions and releases", "[mods][compat]") {
    const Families f = OotAndMm();
    CHECK(f.SameGame(kOotUsa, kOotEur));       // one entry lists both
    CHECK(f.SameGame(kOotUsa, kOotJpn));       // the same title once "(Japan)" goes
    CHECK(f.SameGame(kMmUsa, kMmEur));         // two entries, one title
    CHECK(f.SameGame(kFatesSe, kFatesBr));
    CHECK_FALSE(f.SameGame(kOotUsa, kMmUsa));  // never two games
    CHECK_FALSE(f.SameGame(kOotUsa, 0x0004000000099900ULL));
    CHECK(NormalizeTitle("The Legend of Zelda:  Ocarina of Time 3D (USA) ") ==
          "the legend of zelda: ocarina of time 3d");
    CHECK(RegionOfTitle(kOotEur) == Region::Europe);
    CHECK(RegionOfTitle(0x0004000000099900ULL) == Region::Unknown);
}

TEST_CASE("Compat: the verdicts", "[mods][compat]") {
    const Families f = OotAndMm();

    SECTION("for this game") {
        const Verdict v = Judge({{kOotUsa, kOotEur}, "", {}}, GameOf(kOotEur), f);
        CHECK(v.shown);
        CHECK(v.Fits());
        CHECK(v.Reason().empty());
    }
    SECTION("another game is not shown") {
        CHECK_FALSE(Judge({{kMmUsa}, "", {}}, GameOf(kOotEur), f).shown);
    }
    SECTION("another region") {
        const Verdict v = Judge({{kOotUsa}, "", {}}, GameOf(kOotEur), f);
        CHECK(v.shown);
        CHECK(v.kind == Mismatch::Region);
        CHECK(v.RegionChip() == "USA Only");
        CHECK(v.Reason() == "Made for the USA version. Yours is the European version. It may not work.");
        CHECK(v.Body("mod") == "This mod was made for the USA version of the game, and yours is the "
                               "European version. It may not work, or the game may not start.");
    }
    SECTION("two other regions") {
        const Verdict v = Judge({{kOotJpn, kOotUsa}, "", {}}, GameOf(kOotEur), f);
        CHECK(v.kind == Mismatch::Region);
        CHECK(v.RegionChip() == "USA and Japan");
        CHECK(v.Reason() ==
              "Made for the USA and Japanese versions. Yours is the European version. It may not work.");
        CHECK(v.Body("texture pack") ==
              "This texture pack was made for the USA and Japanese versions of the game, and yours is "
              "the European version. It may not work, or the game may not start.");
    }
    SECTION("the game's region is unknown") {
        Game g = GameOf(kOotEur);
        g.region = Region::Unknown;
        const Verdict v = Judge({{kOotUsa}, "", {}}, g, f);
        CHECK(v.kind == Mismatch::Region);
        CHECK(v.Reason() == "Made for the USA version. It may not work.");
    }
    SECTION("another release, same region") {
        const Verdict v = Judge({{kFatesBr}, "", {}}, GameOf(kFatesSe), f);
        CHECK(v.kind == Mismatch::Release);
        CHECK(v.RegionChip().empty());
        CHECK(v.Reason() == "Made for another release of this game. It may not work.");
        CHECK(v.Body("mod") ==
              "This mod was made for another release of the game. It may not work, or the game may not start.");
    }
    SECTION("another version") {
        const Verdict v = Judge({{kOotUsa}, "1.1", {}}, GameOf(kOotUsa, Version{1, 0}), f);
        CHECK(v.kind == Mismatch::Version);
        CHECK(v.Reason() == "Made for game version 1.1. Yours is 1.0. It may not work.");
        CHECK(v.Body("mod") == "This mod was made for game version 1.1, and yours is version 1.0. "
                               "It may not work, or the game may not start.");
        CHECK(Judge({{kOotUsa}, "1.0", {}}, GameOf(kOotUsa), f).Fits());
        CHECK(Judge({{kOotUsa}, "Card 1.1", {}}, GameOf(kOotUsa), f).Fits()); // not a plain version
        CHECK(Judge({{kOotUsa}, "1.1", {}}, GameOf(kOotUsa, std::nullopt), f).Fits()); // unknown
    }
    SECTION("an update whose version is not known") { // AzaharFrame echoes
        Game g = GameOf(kOotUsa, std::nullopt);
        g.updates = 5;
        const Verdict v = Judge({{kOotUsa}, "1.0", {}}, g, f);
        CHECK(v.kind == Mismatch::Version);
        CHECK(v.Reason() == "Made for game version 1.0. Yours has an update installed. It may not work.");
        CHECK(v.Body("mod") == "This mod was made for game version 1.0, and yours has an update "
                               "installed. It may not work, or the game may not start.");
        CHECK(Judge({{kOotUsa}, "1.5", {}}, g, f).Fits()); // nothing to compare with
        CHECK(Judge({{kOotUsa}, "1.0", {}}, GameOf(kOotUsa), f).Fits()); // no update
    }
    SECTION("another build, by the code's CRC") {
        Game g = GameOf(kMmUsa);
        g.code_crc = 0xffdf1ee8u;
        const Verdict other = Judge({{kMmUsa}, "1.0", {0xc75021d9u}}, g, f, "Card 1.1");
        CHECK(other.kind == Mismatch::Build);
        CHECK(other.Reason() == "Made for another build of the game. Yours matches Card 1.1. It may not work.");
        CHECK(Judge({{kMmUsa}, "Card 1.1", {0xffdf1ee8u}}, g, f, "Card 1.1").Fits());
        // no entry matches the code: the version decides
        CHECK(Judge({{kMmUsa}, "1.0", {0xc75021d9u}}, g, f, "").Fits());
        CHECK(Judge({{kMmUsa}, "1.1", {0xc75021d9u}}, g, f, "").kind == Mismatch::Version);
    }
    SECTION("every reason says may") {
        for (const Verdict& v : {Judge({{kOotUsa}, "", {}}, GameOf(kOotEur), f),
                                 Judge({{kFatesBr}, "", {}}, GameOf(kFatesSe), f),
                                 Judge({{kOotUsa}, "1.1", {}}, GameOf(kOotUsa), f)}) {
            CHECK(v.Reason().find("may not work") != std::string::npos);
            CHECK(v.Body("mod").find("may not") != std::string::npos);
        }
    }
}

TEST_CASE("Compat: region chips", "[mods][compat]") {
    Verdict v;
    v.kind = Mismatch::Region;
    v.regions = {Region::Europe};
    CHECK(v.RegionChip() == "Europe Only");
    v.regions = {Region::Japan};
    CHECK(v.RegionChip() == "Japan Only");
    v.regions = {Region::Usa, Region::Europe, Region::Japan};
    CHECK(v.RegionChip() == "USA, Europe and Japan");
    v.regions = {Region::Usa, Region::Europe, Region::Japan, Region::Korea};
    CHECK(v.RegionChip() == "Other Regions");
}

TEST_CASE("Compat: versions", "[mods][compat]") {
    CHECK(ParseVersion("1.1") == Version{1, 1});
    CHECK(ParseVersion("v1.10") == Version{1, 10});
    CHECK_FALSE(ParseVersion("Card 1.1"));
    CHECK_FALSE(ParseVersion("Fallback"));
    CHECK_FALSE(ParseVersion("1"));
    CHECK_FALSE(ParseVersion(""));
    CHECK(CompareVersions({1, 0}, {1, 1}) < 0);
    CHECK(CompareVersions({1, 10}, {1, 9}) > 0);
    CHECK(CompareVersions({2, 0}, {1, 9}) > 0);
    CHECK(CompareVersions({1, 1}, {1, 1}) == 0);
    CHECK(VersionText({1, 1}) == "1.1");

    std::string source;
    CHECK(GameVersion(Version{1, 1}, Version{1, 0}, 0, &source) == Version{1, 1});
    CHECK(source == "update");
    CHECK(GameVersion(std::nullopt, Version{1, 0}, std::nullopt, &source) == Version{1, 0});
    CHECK(source == "installed title");
    CHECK(GameVersion(std::nullopt, std::nullopt, 0, &source) == Version{1, 0});
    CHECK(source == "assumed");
    CHECK_FALSE(GameVersion(std::nullopt, std::nullopt, 1, &source)); // a later cartridge
}

TEST_CASE("Compat: Install Mods lists other versions after, by downloads", "[mods][compat]") {
    const char* catalog = R"({"schema":1,"mods":[
 {"id":"usa-only","name":"USA Mod","title":"Game","title_ids":["0004000000033500"],"author":"a",
  "licence":"MIT","downloads":90000,
  "source":{"type":"owner_repo_path","path":"games/g/a/code.ips"},
  "install":{"rules":[{"from":"code.ips","to":"code.ips"}]}},
 {"id":"both-small","name":"Both Small","title":"Game","title_ids":["0004000000033500","0004000000033600"],
  "author":"b","licence":"MIT","downloads":150,
  "source":{"type":"owner_repo_path","path":"games/g/b/code.ips"},
  "install":{"rules":[{"from":"code.ips","to":"code.ips"}]}},
 {"id":"both-big","name":"Both Big","title":"Game","title_ids":["0004000000033500","0004000000033600"],
  "author":"c","licence":"MIT","downloads":12000,
  "source":{"type":"owner_repo_path","path":"games/g/c/code.ips"},
  "install":{"rules":[{"from":"code.ips","to":"code.ips"}]}},
 {"id":"both-unknown","name":"A Unknown","title":"Game","title_ids":["0004000000033500","0004000000033600"],
  "author":"d","licence":"MIT",
  "source":{"type":"owner_repo_path","path":"games/g/d/code.ips"},
  "install":{"rules":[{"from":"code.ips","to":"code.ips"}]}},
 {"id":"other-game","name":"Other Game","title":"Other","title_ids":["0004000000125500"],
  "author":"e","licence":"MIT",
  "source":{"type":"owner_repo_path","path":"games/g/e/code.ips"},
  "install":{"rules":[{"from":"code.ips","to":"code.ips"}]}}
]})";
    const auto parsed = ParseCatalog(catalog);
    REQUIRE(parsed.has_value());
    Library library(TempDir("list"), *parsed,
                    [](const std::string&, const std::string&, std::size_t) { return HttpReply{}; });

    // Without the game: only its own entries, the most downloaded first.
    auto list = library.ListAvailable(kOotEur);
    REQUIRE(list.size() == 3);
    CHECK(list[0].id == "both-big");
    CHECK(list[1].id == "both-small");
    CHECK(list[2].id == "both-unknown"); // unknown counts last
    CHECK(list[0].downloads == 12000u);

    // With it: the USA one after them, marked; the other game never.
    const Game g = GameOf(kOotEur);
    list = library.ListAvailable(kOotEur, std::nullopt, &g);
    REQUIRE(list.size() == 4);
    CHECK(list[0].id == "both-big");
    CHECK(list[1].id == "both-small");
    CHECK(list[2].id == "both-unknown");
    CHECK(list[3].id == "usa-only");
    CHECK(list[3].compat.kind == Mismatch::Region);
    CHECK(list[3].compat.RegionChip() == "USA Only");
    for (int i = 0; i < 3; i++) {
        CHECK(list[i].compat.Fits());
    }
}

TEST_CASE("Compat: the crash guard's marker", "[mods][compat]") {
    using CrashGuard::Guard;
    using CrashGuard::Item;
    const std::string dir = TempDir("guard");
    const Item bloom{Item::Kind::Mod, "no-bloom", "No Bloom"};
    const Item hd{Item::Kind::Pack, "hd", "HD Textures"};
    const Item old{Item::Kind::Mod, "old", "Old Mod"};

    SECTION("the game closed unexpectedly: the new ones come back, the old one stays") {
        Guard g(dir);
        g.NoteOn(kOotUsa, bloom);
        g.NoteOn(kOotUsa, hd);
        CHECK(g.Arm(kOotUsa, {bloom, hd, old}) == std::vector<Item>{bloom, hd});
        CHECK(g.Armed(kOotUsa));
        // no clean run, next start (a fresh guard: the files carry it)
        Guard next(dir);
        CHECK(next.TakeCrashed(kOotUsa) == std::vector<Item>{bloom, hd});
        CHECK_FALSE(next.Armed(kOotUsa));
        CHECK(next.New(kOotUsa).empty());
        CHECK(next.TakeCrashed(kOotUsa).empty()); // once only
    }
    SECTION("60 seconds of running, or a normal stop: cleared, never fires") {
        Guard g(dir);
        g.NoteOn(kOotUsa, bloom);
        g.Arm(kOotUsa, {bloom});
        g.Clean(kOotUsa);
        CHECK_FALSE(g.Armed(kOotUsa));
        CHECK(g.New(kOotUsa).empty()); // had its clean run
        g.Arm(kOotUsa, {bloom});
        CHECK_FALSE(g.Armed(kOotUsa)); // nothing new: no marker
        CHECK(Guard(dir).TakeCrashed(kOotUsa).empty());
    }
    SECTION("turned off before the run: not armed") {
        Guard g(dir);
        g.NoteOn(kOotUsa, bloom);
        g.NoteOff(kOotUsa, Item::Kind::Mod, "no-bloom");
        CHECK(g.Arm(kOotUsa, {bloom}).empty());
        CHECK(g.TakeCrashed(kOotUsa).empty());
    }
    SECTION("new but not on at the start: waits for a run it is on in") {
        Guard g(dir);
        g.NoteOn(kOotUsa, hd);
        CHECK(g.Arm(kOotUsa, {bloom}).empty());
        g.Clean(kOotUsa);
        CHECK(g.New(kOotUsa) == std::vector<Item>{hd});
    }
    SECTION("titles are apart") {
        Guard g(dir);
        g.NoteOn(kOotUsa, bloom);
        g.Arm(kOotUsa, {bloom});
        CHECK(g.TakeCrashed(kMmUsa).empty());
        CHECK(g.Armed(kOotUsa));
    }
    CHECK(CrashGuard::TurnedOffText({"No Bloom"}) ==
          "Turned Off No Bloom: The game closed unexpectedly after it was installed.");
    CHECK(CrashGuard::TurnedOffText({"No Bloom", "HD Textures"}) ==
          "Turned Off No Bloom and HD Textures: The game closed unexpectedly after they were installed.");
}

// AzaharFrame echoes: the European Fire Emblem Echoes update is TMD version 1024, read as 1.0,
// while the game and its mods call it 1.1. The shipped catalog's EU code patches carry their
// build's code CRC32, so the code decides: on the 1.1 code the 1.1 patches fit and the 1.0
// Ghosting Shader Fix does not, whatever the version reads.
TEST_CASE("Compat: Echoes EU code patches follow the game's code", "[mods][compat]") {
    constexpr u64 kEchoesEur = 0x00040000001B4100ULL;
    const auto parsed = ParseCatalog(DefaultCatalogJson());
    REQUIRE(parsed.has_value());
    Library library(TempDir("echoes"), *parsed,
                    [](const std::string&, const std::string&, std::size_t) { return HttpReply{}; });
    const auto find = [](const std::vector<AvailableMod>& list, const std::string& id) {
        const auto it = std::find_if(list.begin(), list.end(), [&](const auto& a) { return a.id == id; });
        REQUIRE(it != list.end());
        return *it;
    };

    Game updated = GameOf(kEchoesEur, Version{1, 0}); // the update's TMD 1024
    updated.code_crc = 0x23cc4afbu;
    auto list = library.ListAvailable(kEchoesEur, std::nullopt, &updated);
    for (const char* id : {"echoes-ghosting-shader-fix-eu-v11", "echoes-60fps-eu",
                           "echoes-continuous-player-phase-music",
                           "echoes-continuous-player-phase-music-combat"}) {
        const auto a = find(list, id);
        CHECK(a.compat.Fits());
        CHECK(a.matches_game);
    }
    const auto old_fix = find(list, "echoes-ghosting-shader-fix-eu-v10");
    CHECK(old_fix.compat.kind == Mismatch::Build);
    CHECK(old_fix.compat.your_build == "1.1");
    CHECK_FALSE(old_fix.matches_game);

    Game card = GameOf(kEchoesEur); // the cartridge alone: 1.0 code
    card.code_crc = 0x40d93b41u;
    list = library.ListAvailable(kEchoesEur, std::nullopt, &card);
    CHECK(find(list, "echoes-ghosting-shader-fix-eu-v10").compat.Fits());
    CHECK(find(list, "echoes-ghosting-shader-fix-eu-v10").matches_game);
    CHECK(find(list, "echoes-ghosting-shader-fix-eu-v11").compat.kind == Mismatch::Build);
    CHECK(find(list, "echoes-60fps-eu").compat.kind == Mismatch::Build);
}

// AzaharFrame echoes: True 60 FPS and the Ghosting Shader Fix are both code patches, so the Mods tab
// asks which one to use (Install returns Conflict); the notes say so too.
TEST_CASE("Compat: Echoes True 60 FPS and the Ghosting Shader Fix conflict", "[mods][compat]") {
    const auto parsed = ParseCatalog(DefaultCatalogJson());
    REQUIRE(parsed.has_value());
    for (const char* region : {"us", "eu", "jp"}) {
        const auto* fps = parsed->Find(std::string("echoes-60fps-") + region);
        REQUIRE(fps != nullptr);
        CHECK(fps->notes.find("Can't be combined with the Ghosting Shader Fix") != std::string::npos);
        for (const char* v : {"v10", "v11"}) {
            const auto* fix = parsed->Find(std::string("echoes-ghosting-shader-fix-") + region + "-" + v);
            REQUIRE(fix != nullptr);
            REQUIRE(fps->rules.size() == 1);
            REQUIRE(fix->rules.size() == 1);
            CHECK(ConflictKey(fps->rules[0].to) == ConflictKey(fix->rules[0].to));
        }
    }
}
