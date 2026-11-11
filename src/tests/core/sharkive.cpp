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
    REQUIRE(entries[0].comments == "Sharkive: for v1.0, installed v1.1\n");
    REQUIRE(entries[1].comments.empty());
    REQUIRE(entries[2].comments.empty());
    REQUIRE(MarkOtherVersions(entries, std::nullopt) == 0);
}
