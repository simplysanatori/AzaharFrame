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
