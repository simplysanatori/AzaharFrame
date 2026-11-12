// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ui2: cheats typed in or read from a file (frame_xr/menu/cheat_entry.h).

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/cheat_entry.h"

using namespace FrameXr::Menu;

TEST_CASE("Cheat entry: names", "[frame_xr][cheats]") {
    CHECK(CheckCheatName("Infinite Hearts").empty());
    CHECK(CheckCheatName("  ") == "Enter a Name");
    CHECK(!CheckCheatName("A [B]").empty()); // "[" starts a cheat in the file
}

TEST_CASE("Cheat entry: a typed code", "[frame_xr][cheats]") {
    std::string code = "unchanged";
    CHECK(NormalizeCheatCode("12345678 9abcdef0\nD3000000 00000000", code).empty());
    CHECK(code == "12345678 9ABCDEF0\nD3000000 00000000");
    // More spaces, one 16-digit word, several pairs on a line, blank lines, CR LF.
    CHECK(NormalizeCheatCode("  12345678   00000001 \r\n\r\n2000000000000002 30000000 00000003",
                             code)
              .empty());
    CHECK(code == "12345678 00000001\n20000000 00000002\n30000000 00000003");
    code = "kept";
    CHECK(NormalizeCheatCode("12345678 0000001", code) == "Line 1: Not a Code");
    CHECK(NormalizeCheatCode("12345678 00000001\n12345678", code) == "Line 2: Not a Code");
    CHECK(NormalizeCheatCode("12345678 0000000G", code) == "Line 1: Not a Code");
    CHECK(NormalizeCheatCode("\n  \n", code) == "Enter a Code");
    CHECK(code == "kept");
}

TEST_CASE("Cheat entry: Citra's cheat file", "[frame_xr][cheats]") {
    const std::string file = "[Infinite Health]\n"
                             "*citra_enabled\n"
                             "*source: Sharkive\n"
                             "12345678 00000063\n"
                             "\n"
                             "[Broken]\n"
                             "12345678 0000\n"
                             "[Max Money]\r\n"
                             "2000AAAA 0000270F\r\n"
                             "2000AAAB 0000270F\r\n"
                             "[Name Only]\n";
    const CheatFileText t = ParseCheatFile(file, "Codes");
    REQUIRE(t.cheats.size() == 2);
    CHECK(t.cheats[0].name == "Infinite Health");
    CHECK(t.cheats[0].enabled);
    CHECK(t.cheats[0].comments == "source: Sharkive\n");
    CHECK(t.cheats[0].code == "12345678 00000063");
    CHECK(t.cheats[1].name == "Max Money");
    CHECK_FALSE(t.cheats[1].enabled);
    CHECK(t.cheats[1].code == "2000AAAA 0000270F\n2000AAAB 0000270F");
    CHECK(t.skipped == 2); // Broken, and a name with no code

    // A plain list of code lines: one cheat named after the file.
    const CheatFileText plain = ParseCheatFile("12345678 00000001\n87654321 00000002\n", "Codes");
    REQUIRE(plain.cheats.size() == 1);
    CHECK(plain.cheats[0].name == "Codes");
    CHECK(plain.cheats[0].code == "12345678 00000001\n87654321 00000002");
    // Not a cheat file at all.
    const CheatFileText none = ParseCheatFile("Hello, this is a readme.\n", "Readme");
    CHECK(none.cheats.empty());
    CHECK(none.skipped == 1);
}

TEST_CASE("Cheat entry: which file of a zip", "[frame_xr][cheats]") {
    const uint64_t tid = 0x0004000000033500ULL;
    CHECK(PickCheatFile({"readme.txt", "cheats/0004000000033500.txt", "other/00040000000F0000.txt"},
                        tid) == "cheats/0004000000033500.txt");
    CHECK(PickCheatFile({"Pack/codes.TXT", "Pack/image.png", "__MACOSX/Pack/._codes.TXT"}, tid) ==
          "Pack/codes.TXT");
    CHECK(PickCheatFile({"a.txt", "00040000000F0000.txt"}, tid) == "00040000000F0000.txt");
    CHECK(PickCheatFile({"a.txt", "b.txt"}, tid).empty());
    CHECK(PickCheatFile({"code.ips"}, tid).empty());
    CHECK(IsTitleIdName("0004000000033500"));
    CHECK_FALSE(IsTitleIdName("codes"));
    CHECK(CheatsAddedText(3) == "3 Cheats Added");
    CHECK(CheatsAddedText(1) == "1 Cheat Added");
    CHECK(CheatsAddedText(0) == "No New Cheats");
}
