// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texparts: the parts of a texture pack (core/custom_tex/parts.h).

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include "core/custom_tex/parts.h"

namespace fs = std::filesystem;
using namespace CustomTex::Parts;

namespace {

std::string Tex(const std::string& dir, const std::string& hash) {
    return dir + "/tex1_64x64_" + hash + "_3.png";
}

std::vector<File> Files(const std::vector<std::string>& paths) {
    std::vector<File> out;
    for (const auto& p : paths) {
        out.push_back({p, 100});
    }
    return out;
}

std::vector<std::string> Names(const Layout& l) {
    std::vector<std::string> out;
    for (const auto& g : l.groups) {
        out.push_back(g.name);
    }
    return out;
}

std::vector<std::string> Loaded(const Layout& l, const State& s, const std::vector<File>& files) {
    std::vector<std::string> out;
    for (const std::size_t i : LoadOrder(l, s, files)) {
        out.push_back(files[i].path);
    }
    return out;
}

struct Temp {
    fs::path dir;
    Temp() {
        dir = fs::temp_directory_path() / ("af-texparts-" + std::to_string(::getpid()));
        fs::remove_all(dir);
        fs::create_directories(dir);
    }
    ~Temp() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

/// MM3DHD's layout, with a hash Main and Effects share.
const std::vector<File> kMM = Files({
    Tex("[Main]", "000000000000000A"),
    Tex("[Main]", "000000000000000B"),
    Tex("[Fonts] Original N64", "0000000000000F01"),
    Tex("[Fonts] Original Pizza", "0000000000000F01"),
    Tex("[Fonts] Modern", "0000000000000F01"),
    Tex("[Link] Nerrel Style", "0000000000000C01"),
    Tex("EFFECTS", "000000000000000A"),
    Tex("AI Upscaled or WIP/Map", "0000000000000D01"),
    Tex("AI Upscaled or WIP/wip", "0000000000000D02"),
    "readme.txt",
    "tex1_8x8_00000000000000EE_3.png",
});

} // namespace

TEST_CASE("texparts: names", "[texparts]") {
    CHECK(DisplayName("ENVIRONMENT") == "Environment");
    CHECK(DisplayName("UI") == "UI");
    CHECK(DisplayName("HUD ICONS") == "HUD Icons");
    CHECK(DisplayName("Link's Tunic") == "Link's Tunic");
    CHECK(TextureHash("tex1_64x64_1E3BCF1097CE56AC_3.png") == 0x1E3BCF1097CE56ACULL);
    CHECK(TextureHash("readme.txt") == 0);
    CHECK(IsTextureFile("tex1_8x8_AB_1.ktx"));
    CHECK(IsTextureFile("TEX1_8x8_AB_1.PNG"));
    CHECK_FALSE(IsTextureFile("tex2_8x8_AB_1.png"));
}

TEST_CASE("texparts: folders, tags and the base", "[texparts]") {
    const Layout l = Build(kMM);
    // by name, the base (Main) last
    CHECK(Names(l) == std::vector<std::string>{"AI Upscaled or WIP", "Effects", "Fonts",
                                               "Link: Nerrel Style", "Main"});
    CHECK(l.loose_textures == 1);
    const Group* ai = l.FindGroup("AI Upscaled or WIP");
    REQUIRE(ai != nullptr);
    CHECK(ai->kind == Kind::Folder);
    REQUIRE(ai->parts.size() == 2);
    CHECK(ai->parts[0].key == "AI Upscaled or WIP/Map");
    const Group* fonts = l.FindGroup("choice:fonts");
    REQUIRE(fonts != nullptr);
    CHECK(fonts->kind == Kind::Choice);
    REQUIRE(fonts->parts.size() == 3);
    CHECK(fonts->parts[0].name == "Modern");
    CHECK(fonts->parts[1].name == "Original N64");
    // choices of one group do not conflict; Main and Effects do
    CHECK(fonts->parts[0].conflicts.empty());
    CHECK(l.FindPart("EFFECTS")->conflicts == std::vector<std::string>{"[Main]"});
    CHECK(l.FindPart("[Main]")->conflicts == std::vector<std::string>{"EFFECTS"});
    CHECK(l.FindGroup("[Main]")->base);
    CHECK(AnyConflicts(l));
    CHECK(GroupConflicts(l, *l.FindGroup("EFFECTS")));
    CHECK_FALSE(GroupConflicts(l, *fonts));
    CHECK(l.PartCount() == 8);
}

TEST_CASE("texparts: wrappers and a choice's common words", "[texparts]") {
    const auto files = Files({
        Tex("Wrap/Inner/Touchless Layout (Vanilla) Xbox UI", "0000000000000001"),
        Tex("Wrap/Inner/Touchless Layout (Vanilla) Dualshock UI", "0000000000000001"),
        Tex("Wrap/Inner/Touchless Layout (Vanilla) Nintendo UI", "0000000000000001"),
        Tex("Wrap/Inner/Unedited textures (Vanilla)", "0000000000000001"),
    });
    Hints h;
    h.choose_one = "Controller";
    h.off = {"Unedited textures*"};
    const Layout l = Build(files, h);
    CHECK(l.root == "Wrap/Inner");
    REQUIRE(l.groups.size() == 1);
    const Group& g = l.groups[0];
    CHECK(g.name == "Controller");
    REQUIRE(g.parts.size() == 4);
    CHECK(g.parts[0].name == "Dualshock UI");
    CHECK(g.parts[2].name == "Unedited textures (Vanilla)");
    CHECK(g.parts[3].name == "Xbox UI");
    State s;
    s.hints = h;
    // the first choice not off by a hint
    CHECK(Chosen(l, s, g) == "Wrap/Inner/Touchless Layout (Vanilla) Dualshock UI");
    CHECK(CountOn(l, s) == std::pair<int, int>{1, 1});
}

TEST_CASE("texparts: hints keep and off", "[texparts]") {
    const auto files = Files({
        Tex("N64 Patch/Omni Xbox UI for PR", "0000000000000001"),
        Tex("N64 Patch/Omni Dualshock UI for PR", "0000000000000002"),
        Tex("Omni Xbox UI for PR", "0000000000000001"),
        Tex("Omni Dualshock UI for PR", "0000000000000002"),
        Tex("Unedited textures (for personal use)", "0000000000000003"),
    });
    Hints h;
    h.choose_one = "Style";
    h.keep = {"N64 Patch"};
    h.off = {"N64 Patch", "Unedited textures*"};
    const Layout l = Build(files, h);
    CHECK(Names(l) == std::vector<std::string>{"N64 Patch", "Style"});
    State s;
    s.hints = h;
    const Group& patch = *l.FindGroup("N64 Patch");
    CHECK(patch.kind == Kind::Folder);
    CHECK(GroupValue(l, s, patch) == 0);
    CHECK(Chosen(l, s, *l.FindGroup("choice:style")) == "Omni Dualshock UI for PR");
    // the patch is above the style, so it wins once it is on
    SetGroup(s, l, patch, true);
    CHECK(GroupValue(l, s, patch) == 2);
    const auto loaded = Loaded(l, s, files);
    REQUIRE(loaded.size() == 3);
    CHECK(loaded[0] == Tex("N64 Patch/Omni Dualshock UI for PR", "0000000000000002"));
    CHECK(loaded[2] == Tex("Omni Dualshock UI for PR", "0000000000000002"));
}

TEST_CASE("texparts: the higher part loads first, parts off are left out", "[texparts]") {
    const Layout l = Build(kMM);
    State s;
    auto loaded = Loaded(l, s, kMM);
    // defaults: every checkbox on, the first font, Main (the base) last, loose files after all
    REQUIRE(loaded.size() == 9);
    CHECK(loaded[0].rfind("AI Upscaled or WIP/Map/", 0) == 0);
    CHECK(loaded[2].rfind("EFFECTS/", 0) == 0);
    CHECK(loaded[3].rfind("[Fonts] Modern/", 0) == 0);
    CHECK(loaded[4].rfind("[Link] Nerrel Style/", 0) == 0);
    CHECK(loaded[5].rfind("[Main]/", 0) == 0);
    CHECK(loaded[7] == "readme.txt");
    CHECK(loaded[8] == "tex1_8x8_00000000000000EE_3.png");

    // Main above Effects: Main's file of the shared hash is read first
    Move(s, l, "[Main]", -1);
    Move(s, l, "[Main]", -1);
    Move(s, l, "[Main]", -1);
    CHECK(Order(l, s)[1] == 4);
    loaded = Loaded(l, s, kMM);
    const auto main_at = std::find(loaded.begin(), loaded.end(), Tex("[Main]", "000000000000000A"));
    const auto fx_at = std::find(loaded.begin(), loaded.end(), Tex("EFFECTS", "000000000000000A"));
    CHECK(main_at < fx_at);
    // moves past the ends do nothing
    Move(s, l, "AI Upscaled or WIP", -1);
    CHECK(Order(l, s)[0] == 0);

    SetPart(s, l, "EFFECTS", false);
    SetPart(s, l, "AI Upscaled or WIP/wip", false);
    Choose(s, l, *l.FindGroup("choice:fonts"), "");
    loaded = Loaded(l, s, kMM);
    CHECK(loaded.size() == 6);
    CHECK(std::find(loaded.begin(), loaded.end(), Tex("EFFECTS", "000000000000000A")) ==
          loaded.end());
    CHECK(GroupValue(l, s, *l.FindGroup("AI Upscaled or WIP")) == 1);
    CHECK(CountOn(l, s) == std::pair<int, int>{3, 6});
    CHECK(TotalsOn(l, s) == std::pair<u64, u64>{5, 500}); // the loose texture counts too

    SetAll(s, l, true);
    CHECK(CountOn(l, s) == std::pair<int, int>{5, 6}); // the font stays off
    SetAll(s, l, false);
    CHECK(CountOn(l, s) == std::pair<int, int>{0, 6});
}

TEST_CASE("texparts: state json and files", "[texparts]") {
    const Layout l = Build(kMM);
    State s;
    s.hints.off = {"EFFECTS"};
    CHECK_FALSE(IsOn(l, s, *l.FindGroup("EFFECTS"), *l.FindPart("EFFECTS")));
    Choose(s, l, *l.FindGroup("choice:fonts"), "[Fonts] Original Pizza");
    Move(s, l, "[Main]", -1);
    CHECK(s.set);
    CHECK(s.off.count("EFFECTS") == 1); // the hint's default carried into the state
    const State back = ParseState(StateJson(s));
    CHECK(back == s);
    CHECK(ParseState("not json") == State{});
    CHECK_FALSE(ParseState(R"({"set": false, "off": ["x"]})").set);

    Temp t;
    std::ofstream(t.dir / "a.png") << "x";
    fs::create_directories(t.dir / "P");
    fs::create_directories(t.dir / "Q");
    std::ofstream(t.dir / "P" / "tex1_8x8_00000000000000AB_3.png") << "xyz";
    std::ofstream(t.dir / "P" / "tex1_8x8_00000000000000AC_3.png") << "xyzw";
    std::ofstream(t.dir / "Q" / "tex1_8x8_00000000000000AD_3.ktx") << "k";
    const auto files = ListFiles(t.dir.string());
    CHECK(files.size() == 4);
    CHECK(ReadState(t.dir.string()) == State{});
    Hints h;
    h.choose_one = "Style";
    REQUIRE(WriteHints(t.dir.string(), h));
    CHECK(ReadState(t.dir.string()).hints == h);
    REQUIRE(WriteState(t.dir.string(), s));
    CHECK(ReadState(t.dir.string()) == s);
    // hints never replace the player's choices
    REQUIRE(WriteHints(t.dir.string(), h));
    CHECK(ReadState(t.dir.string()) == s);
    const Layout scanned = Scan(t.dir.string(), {});
    REQUIRE(scanned.groups.size() == 2);
    // the largest PNG of the part; none for a part of KTX files only
    CHECK(fs::path(scanned.groups[0].parts[0].preview).filename() ==
          "tex1_8x8_00000000000000AC_3.png");
    CHECK(scanned.groups[1].parts[0].preview.empty());
}

TEST_CASE("texparts: texts", "[texparts]") {
    const Layout l = Build(kMM);
    State s;
    CHECK(CountText(1, "Texture", "Textures") == "1 Texture");
    CHECK(CountText(1434, "Texture", "Textures") == "1,434 Textures");
    CHECK(SizeText(4496484836ull) == "4.2 GB"); // AzaharFrame texrow: 1024s, as the menu's sizes
    CHECK(SizeText(73494804) == "70 MB");
    CHECK(PartDetail(l, *l.FindPart("EFFECTS")) == "1 Texture · 100 B · Conflicts with Main");
    CHECK(GroupDetail(l, *l.FindGroup("AI Upscaled or WIP")) == "2 Textures · 200 B");
    CHECK(ConflictNames(l, *l.FindPart("[Main]")) == "Effects");
    CHECK(PartsOnText(l, s).empty()); // every part on
    SetPart(s, l, "EFFECTS", false);
    CHECK(PartsOnText(l, s) == "5 of 6 Parts");
    CHECK(CanMove(l, *l.FindGroup("EFFECTS")));
    CHECK_FALSE(CanMove(l, *l.FindGroup("choice:fonts")));
}

TEST_CASE("texparts: the size of what is on counts loose textures", "[texparts]") {
    // loose only (a flat or optimized copy), 100 bytes each
    {
        const Layout l = Build(Files({"tex1_64x64_A_3.ktx", "tex1_64x64_B_3.ktx"}));
        const State s;
        CHECK(l.loose_textures == 2);
        CHECK(l.loose_bytes == 200);
        CHECK(TotalsOn(l, s) == std::pair<u64, u64>{2, 200});
    }
    // loose beside parts: the parts that are off drop out, the loose ones stay
    {
        const Layout l = Build(Files({"tex1_64x64_A_3.png", Tex("UI", "B"), Tex("Fonts", "C")}));
        State s;
        const auto all = TotalsOn(l, s);
        CHECK(all.second == 100 + 100 + 100);
        SetAll(s, l, false);
        CHECK(TotalsOn(l, s) == std::pair<u64, u64>{1, 100});
    }
    // two layered packs: the sizes of both add up
    {
        const Layout a = Build(Files({"tex1_64x64_A_3.png", "tex1_64x64_B_3.png"}));
        const Layout b = Build(Files({Tex("UI", "C")}));
        const State s;
        CHECK(TotalsOn(a, s).second + TotalsOn(b, s).second == 300);
    }
    // nothing on
    {
        const Layout l = Build(Files({}));
        CHECK(TotalsOn(l, State{}) == std::pair<u64, u64>{0, 0});
    }
}
