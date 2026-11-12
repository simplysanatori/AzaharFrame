// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cheats: Sharkive parse and merge (no network).

#include <catch2/catch_test_macros.hpp>
#include "core/cheats/sharkive.h"

using namespace Cheats::Sharkive;

TEST_CASE("Sharkive: url path", "[sharkive]") {
    REQUIRE(UrlPath(0x0004000000030800ULL) == "/FlagBrew/Sharkive/master/3ds/0004000000030800.txt");
    REQUIRE(UrlPath(0xABCDULL) == "/FlagBrew/Sharkive/master/3ds/000000000000ABCD.txt");
}

TEST_CASE("Sharkive: parse", "[sharkive]") {
    const auto entries = Parse("---\r\n"
                               "[Infinite Health]\r\n"
                               "*citra_enabled\r\n"
                               "*Keeps hearts full\r\n"
                               "0A1B2C3D 00000063\r\n"
                               "D3000000 00000000\r\n"
                               "\r\n"
                               "[Broken]\n"
                               "0A1B2C3D 00000063\n"
                               "not a code\n"
                               "\n"
                               "[Empty]\n"
                               "\n"
                               "[Infinite Health]\n"
                               "11111111 22222222\n"
                               "\n"
                               "[ Max Rupees ]\n"
                               "abcdef01 00000001");
    REQUIRE(entries.size() == 2);
    REQUIRE(entries[0].name == "Infinite Health");
    REQUIRE(entries[0].comments == "Keeps hearts full\n");
    REQUIRE(entries[0].code == "0A1B2C3D 00000063\nD3000000 00000000");
    REQUIRE(entries[1].name == "Max Rupees");
    REQUIRE(entries[1].code == "abcdef01 00000001");
}

TEST_CASE("Sharkive: junk and empty input", "[sharkive]") {
    REQUIRE(Parse("").empty());
    REQUIRE(Parse("<html>404: Not Found</html>").empty());
    REQUIRE(Parse("0A1B2C3D 00000063\n").empty()); // no name
}

TEST_CASE("Sharkive: merge keeps what the user has", "[sharkive]") {
    const auto downloaded = Parse("[A]\n00000000 00000001\n\n[B]\n00000000 00000002\n\n[C]\n"
                                  "00000000 00000003\n");
    REQUIRE(downloaded.size() == 3);
    const auto added = NewEntries(downloaded, {"B ", "Mine"});
    REQUIRE(added.size() == 2);
    REQUIRE(added[0].name == "A");
    REQUIRE(added[1].name == "C");
    REQUIRE(NewEntries(downloaded, {"A", "B", "C"}).empty());
    REQUIRE(NewEntries(downloaded, {}).size() == 3);
}

TEST_CASE("Sharkive: version tags", "[sharkive]") {
    using V = Version;
    REQUIRE(TagVersion("All Karts, Game v1.0") == V{1, 0});
    REQUIRE(TagVersion("All Gliders, Rev2 Game v1.1") == V{1, 1});
    REQUIRE(TagVersion("Full Course Laps - USA Rev0 v1.1") == V{1, 1});
    REQUIRE(TagVersion("Coin Modifier Rev 0 V 1.1 USA") == V{1, 1});
    REQUIRE(TagVersion("Max/Inf HP v1.5") == V{1, 5});
    REQUIRE(TagVersion("Wins and Losses Modifier rev0 1.1 ONLY") == V{1, 1});
    REQUIRE(TagVersion("Coins", "Works on (v2.0)") == V{2, 0});
    // not versions
    REQUIRE_FALSE(TagVersion("Refill HP v1").has_value());
    REQUIRE_FALSE(TagVersion("Trick Anywhere - Rev0").has_value());
    REQUIRE_FALSE(TagVersion("Money 9.999").has_value());
    REQUIRE_FALSE(TagVersion("Money 999.999 only").has_value());
    REQUIRE_FALSE(TagVersion("Invincible").has_value());
    REQUIRE_FALSE(TagVersion("Move speed (Press L)").has_value());
    REQUIRE_FALSE(TagVersion("Rev1 Value 1.1").has_value());
}

TEST_CASE("Sharkive: title version and matching", "[sharkive]") {
    REQUIRE(FromTitleVersion(0x0410) == Version{1, 1});
    REQUIRE(FromTitleVersion(0x0400) == Version{1, 0});
    REQUIRE(FromTitleVersion(0x0850) == Version{2, 5});
    REQUIRE(ToString(Version{1, 1}) == "v1.1");

    const std::optional<Version> v10 = Version{1, 0}, v11 = Version{1, 1};
    REQUIRE(VersionMatches(std::nullopt, v10));
    REQUIRE(VersionMatches(v10, v10));
    REQUIRE_FALSE(VersionMatches(v11, v10));
    REQUIRE(VersionMatches(v11, std::nullopt));

    auto entries = Parse("[A Game v1.0]\n00000000 00000001\n\n[B Game v1.1]\n00000000 00000002\n\n"
                         "[C]\n00000000 00000003\n");
    REQUIRE(MarkOtherVersions(entries, v11) == 1);
    REQUIRE(entries[0].comments == "Version note: for v1.0, installed v1.1\n");
    REQUIRE(entries[1].comments.empty());
    REQUIRE(entries[2].comments.empty());
    REQUIRE(MarkOtherVersions(entries, std::nullopt) == 0);
}

TEST_CASE("Sharkive: installed version order", "[sharkive]") {
    const auto none = SelectInstalledVersion(0, std::nullopt, std::nullopt, std::nullopt);
    REQUIRE_FALSE(none.version.has_value());
    REQUIRE_FALSE(none.revision.has_value());
    REQUIRE(none.source == VersionSource::Unknown);

    // update > base > ROM
    auto r = SelectInstalledVersion(0, 0x0410, 0x0400, 0x0002);
    REQUIRE_FALSE(r.version.has_value()); // an update the list does not name: not known
    REQUIRE(r.updates == 1);
    REQUIRE(r.tmd == 0x0410);
    REQUIRE(r.source == VersionSource::UpdateTmd);
    REQUIRE_FALSE(r.revision.has_value());
    r = SelectInstalledVersion(0, std::nullopt, 0, 0x0002);
    REQUIRE(r.version == Version{1, 0}); // an installed title as released
    REQUIRE(r.updates == 0);
    REQUIRE(r.source == VersionSource::BaseTmd);
    // an update of 0 still wins over the ROM
    r = SelectInstalledVersion(0, 0, std::nullopt, 0x0002);
    REQUIRE(r.source == VersionSource::UpdateTmd);
    REQUIRE(r.version == Version{1, 0});

    // cartridge: remaster / 2 is the revision, no version
    r = SelectInstalledVersion(0, std::nullopt, std::nullopt, 0x0002);
    REQUIRE(r.source == VersionSource::Ncch);
    REQUIRE(r.revision == 1);
    REQUIRE_FALSE(r.version.has_value());
    r = SelectInstalledVersion(0, std::nullopt, std::nullopt, 4);
    REQUIRE(r.revision == 2);
    // zero is ambiguous: unknown
    r = SelectInstalledVersion(0, std::nullopt, std::nullopt, 0);
    REQUIRE(r.source == VersionSource::Unknown);
    REQUIRE_FALSE(r.revision.has_value());
    // odd: not understood
    REQUIRE(SelectInstalledVersion(0, std::nullopt, std::nullopt, 3).source == VersionSource::Unknown);
}

TEST_CASE("Sharkive: revision tags and marking", "[sharkive]") {
    REQUIRE(TagRevision("All Karts, Rev1 Game v1.1") == 1);
    REQUIRE(TagRevision("Full Course Laps - USA Rev0 v1.1") == 0);
    REQUIRE(TagRevision("Coin Modifier Rev 0 V 1.1 USA") == 0);
    REQUIRE(TagRevision("Disable Star music - rev0") == 0);
    REQUIRE(TagRevision("Unlock rev02") == 2);
    REQUIRE(TagRevision("Coins", "for rev1") == 1);
    REQUIRE_FALSE(TagRevision("All Karts, Game v1.0").has_value());
    REQUIRE_FALSE(TagRevision("Reverse Course").has_value());
    REQUIRE_FALSE(TagRevision("Prevent Death 2").has_value());

    auto entries = Parse("[A Rev0 v1.1]\n00000000 00000001\n\n[B Rev1]\n00000000 00000002\n\n"
                         "[C Game v1.0]\n00000000 00000003\n");
    REQUIRE(MarkOtherRevisions(entries, 1) == 1);
    REQUIRE(entries[0].comments == "Version note: for Rev0, this cartridge is Rev1\n");
    REQUIRE(entries[1].comments.empty());
    REQUIRE(entries[2].comments.empty()); // version-only tags stay unflagged
}

// AzaharFrame echoes: the version the game shows, not the TMD's bits (Mario Kart 7's last update,
// "Ver. 1.2", is TMD 3072; Fire Emblem Echoes' only update, "ver 1.1", is TMD 1024).
TEST_CASE("Sharkive: the version a game shows for its update", "[sharkive]") {
    REQUIRE(FromTitleVersion(3072) == Version{3, 0}); // the raw split, for logs only
    REQUIRE(GameVersionOf(0x0004000000030800ULL, 3072) == Version{1, 2});
    REQUIRE(GameVersionOf(0x0004000E00030800ULL, 3072) == Version{1, 2}); // the update's own ID
    REQUIRE(GameVersionOf(0x00040000001B4100ULL, 1024) == Version{1, 1});
    REQUIRE(GameVersionOf(0x00040000001B4000ULL, 1024) == Version{1, 1});
    REQUIRE(GameVersionOf(0x00040000001A2B00ULL, 1024) == Version{1, 1});
    REQUIRE(GameVersionOf(0x0004000000055D00ULL, 0) == Version{1, 0}); // as released
    REQUIRE(GameVersionOf(0x0004000000055D00ULL, 15) == Version{1, 0});
    REQUIRE_FALSE(GameVersionOf(0x0004000000055D00ULL, 5216).has_value()); // not listed
    REQUIRE_FALSE(GameVersionOf(0x0004000000030700ULL, 3072).has_value()); // only the USA one is

    // Mario Kart 7 with its update: v1.0 and v1.1 cheats are for another version, named as 1.2
    const auto mk7 = SelectInstalledVersion(0x0004000000030800ULL, 3072, std::nullopt, 0x0002);
    REQUIRE(mk7.version == Version{1, 2});
    REQUIRE(mk7.updates == 3);
    auto entries = Parse("[All Karts, Game v1.0]\n00000000 00000001\n\n"
                         "[Manual Mode, Game v1.1 USA]\n00000000 00000002\n\n[C]\n00000000 00000003\n");
    REQUIRE(MarkOtherVersions(entries, mk7) == 2);
    REQUIRE(entries[0].comments == "Version note: for v1.0, installed v1.2\n");
    REQUIRE(entries[1].comments == "Version note: for v1.1, installed v1.2\n");
    REQUIRE(entries[2].comments.empty());

    // an update the list does not name: a v1.0 cheat is not for it, any other tag may be
    const auto other = SelectInstalledVersion(0x0004000000055D00ULL, 5216, std::nullopt, std::nullopt);
    REQUIRE_FALSE(other.version.has_value());
    REQUIRE_FALSE(VersionMatches(Version{1, 0}, other));
    REQUIRE(VersionMatches(Version{1, 5}, other));
    REQUIRE(VersionMatches(std::nullopt, other));
    entries = Parse("[A v1.0]\n00000000 00000001\n\n[B v1.5]\n00000000 00000002\n");
    REQUIRE(MarkOtherVersions(entries, other) == 1);
    REQUIRE(entries[0].comments == "Version note: for v1.0, this game has an update\n");
    REQUIRE(entries[1].comments.empty());
}
