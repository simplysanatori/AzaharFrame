// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texlayers: several texture packs on at once (core/custom_tex/layers.h).

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include "core/custom_tex/layers.h"

namespace fs = std::filesystem;
using namespace CustomTex;
using namespace CustomTex::Layers;

namespace {

constexpr u64 kTitle = 0x0004000000055D00ULL;

std::string Tex(const std::string& dir, const std::string& hash, const char* ext = "png") {
    return (dir.empty() ? "" : dir + "/") + "tex1_64x64_" + hash + "_3." + ext;
}

PackData MakePack(const std::string& id, const std::vector<std::string>& paths) {
    PackData d;
    d.pack = id;
    for (const auto& p : paths) {
        d.files.push_back({p, 100});
    }
    d.layout = Parts::Build(d.files);
    return d;
}

/// "pack:file" for each file the loader reads, in order.
std::vector<std::string> Loaded(const State& s, const std::vector<PackData>& packs) {
    std::vector<std::string> out;
    for (const Placed& p : LoadOrder(s, packs)) {
        out.push_back(packs[p.pack].pack + ":" + packs[p.pack].files[p.file].path);
    }
    return out;
}

/// The pack a hash comes from: the first file read with it, as the loader keeps it.
std::string Winner(const State& s, const std::vector<PackData>& packs, const std::string& hash) {
    for (const std::string& f : Loaded(s, packs)) {
        if (f.find("_" + hash + "_") != std::string::npos) {
            return f.substr(0, f.find(':'));
        }
    }
    return {};
}

State Fitted(State s, const std::vector<PackData>& packs) {
    for (const PackData& d : packs) {
        Fit(s, d.pack, PackKeys(d.layout, d.state));
    }
    return s;
}

std::vector<std::string> Keys(const State& s) {
    std::vector<std::string> out;
    for (const Entry& e : s.entries) {
        out.push_back(e.pack + "/" + e.key);
    }
    return out;
}

struct Temp {
    fs::path dir;
    Temp() {
        dir = fs::temp_directory_path() / ("af-texlayers-" + std::to_string(::getpid()));
        fs::remove_all(dir);
        fs::create_directories(dir);
    }
    ~Temp() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    void Write(const fs::path& rel, const std::string& text = "x") const {
        fs::create_directories((dir / rel).parent_path());
        std::ofstream(dir / rel) << text;
    }
};

// Modern Sprites: a pack of six generation folders (splits); 5X Redrawn UI: a pack of 23 folders
// (one entry), with a "pokemon sprites" folder that has some of the same textures.
std::vector<std::string> SpritesFiles() {
    std::vector<std::string> f;
    for (int g = 1; g <= 6; g++) {
        f.push_back(Tex("Gen " + std::to_string(g), "00000000000000A" + std::to_string(g)));
    }
    return f;
}
std::vector<std::string> UiFiles() {
    std::vector<std::string> f;
    for (int i = 0; i < 22; i++) {
        f.push_back(Tex("folder " + std::to_string(100 + i), "0000000000000B" + std::to_string(10 + i)));
    }
    f.push_back(Tex("pokemon sprites", "00000000000000A1"));
    f.push_back(Tex("pokemon sprites", "00000000000000A2"));
    return f;
}

} // namespace

TEST_CASE("texlayers: which packs split", "[texlayers]") {
    const PackData sprites = MakePack("S", SpritesFiles());
    const PackData ui = MakePack("U", UiFiles());
    const PackData flat = MakePack("F", {Tex("", "0000000000000C01"), Tex("", "0000000000000C02")});
    const PackData one = MakePack("O", {Tex("only", "0000000000000D01")});
    CHECK(Splits(sprites.layout));                // 6 folders
    CHECK_FALSE(Splits(ui.layout));               // 23: one entry
    CHECK_FALSE(Splits(flat.layout));             // no folders
    CHECK_FALSE(Splits(one.layout));              // one folder
    CHECK(PackKeys(sprites.layout, sprites.state).size() == 6);
    CHECK(PackKeys(ui.layout, ui.state) == std::vector<std::string>{""});
    CHECK(PackKeys(flat.layout, flat.state) == std::vector<std::string>{""});
}

TEST_CASE("texlayers: the higher entry wins, entries of two packs interleave", "[texlayers]") {
    const std::vector<PackData> packs{MakePack("S", SpritesFiles()), MakePack("U", UiFiles())};
    State s;
    Add(s, "U", PackKeys(packs[1].layout, packs[1].state));
    Add(s, "S", PackKeys(packs[0].layout, packs[0].state)); // on top, as a group
    CHECK(Keys(s) == std::vector<std::string>{"S/Gen 1", "S/Gen 2", "S/Gen 3", "S/Gen 4",
                                              "S/Gen 5", "S/Gen 6", "U/"});
    CHECK(Winner(s, packs, "00000000000000A1") == "S");
    CHECK(Winner(s, packs, "00000000000000A2") == "S");
    // 5X above Gen 2: it wins Gen 2's texture, Gen 1 still wins its own
    REQUIRE(Move(s, 6, -1));
    REQUIRE(Move(s, 5, -1));
    REQUIRE(Move(s, 4, -1));
    REQUIRE(Move(s, 3, -1));
    CHECK(Keys(s) == std::vector<std::string>{"S/Gen 1", "S/Gen 2", "U/", "S/Gen 3", "S/Gen 4",
                                              "S/Gen 5", "S/Gen 6"});
    CHECK(Winner(s, packs, "00000000000000A1") == "S");
    CHECK(Winner(s, packs, "00000000000000A2") == "S");
    REQUIRE(Move(s, 2, -1));
    CHECK(Winner(s, packs, "00000000000000A1") == "S");
    CHECK(Winner(s, packs, "00000000000000A2") == "U");
    CHECK_FALSE(Move(s, 0, -1));
    CHECK_FALSE(Move(s, 6, 1));
    // Every file once: 6 + 24
    CHECK(Loaded(s, packs).size() == 30);

    // What it replaces: 5X (now over Gen 2) replaces one of Modern Sprites'; Gen 1 one of 5X's
    const auto r = Replaces(s, packs);
    REQUIRE(r.size() == s.entries.size());
    CHECK(r[0] == std::vector<std::pair<std::string, u64>>{{"U", 1}}); // Gen 1 over 5X's A1
    CHECK(r[1] == std::vector<std::pair<std::string, u64>>{{"S", 1}}); // 5X over Gen 2
    CHECK(r[2].empty());
    CHECK(ReplacesText({{"5X Redrawn UI HD", 120}}) == "Replaces 120 Textures From 5X Redrawn UI HD");
    CHECK(ReplacesText({{"A", 1}}) == "Replaces 1 Texture From A");
    CHECK(ReplacesText({{"A", 1000}, {"B", 234}}) == "Replaces 1,234 Textures From A and B");
    CHECK(ReplacesText({{"A", 3}, {"B", 2}, {"C", 1}}) == "Replaces 6 Textures From A and 2 More");
    CHECK(ReplacesText({}).empty());
}

TEST_CASE("texlayers: with each pack's parts and order", "[texlayers]") {
    // Two split packs; P's own Customize order puts Effects first; a part off is left out.
    PackData p = MakePack("P", {Tex("Effects", "0000000000000001"), Tex("UI", "0000000000000001"),
                            Tex("UI", "0000000000000002"), Tex("Base", "0000000000000003"),
                            Tex("", "0000000000000009")}); // a loose file
    PackData q = MakePack("Q", {Tex("Fonts", "0000000000000002"), Tex("Menus", "0000000000000003")});
    REQUIRE(Splits(p.layout));
    REQUIRE(Splits(q.layout));
    Parts::Move(p.state, p.layout, "UI", -1); // UI, Effects, Base (a base is last by default)
    std::vector<PackData> packs{p, q};
    State s;
    Add(s, "P", PackKeys(p.layout, p.state));
    Add(s, "Q", PackKeys(q.layout, q.state));
    CHECK(Keys(s) == std::vector<std::string>{"Q/Fonts", "Q/Menus", "P/UI", "P/Effects", "P/Base"});
    CHECK(Winner(s, packs, "0000000000000001") == "P");       // only P has it: UI's first
    CHECK(Loaded(s, packs)[2] == "P:" + Tex("UI", "0000000000000001"));
    CHECK(Winner(s, packs, "0000000000000002") == "Q");       // Fonts over UI
    CHECK(Winner(s, packs, "0000000000000003") == "Q");       // Menus over Base
    // P's loose file goes with its lowest entry
    CHECK(Loaded(s, packs).back() == "P:" + Tex("", "0000000000000009"));
    // A part turned off (Q's Fonts): P's UI wins its texture
    Parts::SetPart(packs[1].state, packs[1].layout, "Fonts", false);
    CHECK(Winner(s, packs, "0000000000000002") == "P");
    CHECK(Loaded(s, packs).size() == 6);
    // Customize's Move Up in Q moves its entries in the places they hold
    SetPackOrder(s, "Q", {"Menus", "Fonts"});
    CHECK(Keys(s) == std::vector<std::string>{"Q/Menus", "Q/Fonts", "P/UI", "P/Effects", "P/Base"});
    CHECK(PackOrder(s, "P") == std::vector<std::string>{"UI", "Effects", "Base"});
}

TEST_CASE("texlayers: ASTC and PNG packs mix; one pack off", "[texlayers]") {
    // An optimized (KTX) pack and a PNG pack: the files are read as they are, the higher wins.
    const std::vector<PackData> packs{
        MakePack("K", {Tex("", "0000000000000001", "ktx"), Tex("", "0000000000000002", "ktx")}),
        MakePack("N", {Tex("", "0000000000000002", "png"), Tex("", "0000000000000003", "png")})};
    State s;
    Add(s, "K", {""});
    Add(s, "N", {""});
    CHECK(Loaded(s, packs) == std::vector<std::string>{
                                  "N:" + Tex("", "0000000000000002", "png"),
                                  "N:" + Tex("", "0000000000000003", "png"),
                                  "K:" + Tex("", "0000000000000001", "ktx"),
                                  "K:" + Tex("", "0000000000000002", "ktx")});
    CHECK(Winner(s, packs, "0000000000000002") == "N");
    REQUIRE(Move(s, 1, -1));
    CHECK(Winner(s, packs, "0000000000000002") == "K");
    // N off: K only
    Remove(s, "N");
    CHECK(Loaded(s, packs) == std::vector<std::string>{"K:" + Tex("", "0000000000000001", "ktx"),
                                                       "K:" + Tex("", "0000000000000002", "ktx")});
    CHECK(Packs(s) == std::vector<std::string>{"K"});
}

TEST_CASE("texlayers: a pack layered on a base goes above it", "[texlayers]") {
    State s;
    Add(s, "Sprites", {"Gen 1", "Gen 2"});
    // The base turned on after: under the packs layered on it, not on top
    Add(s, "UI", {""}, {"Sprites"});
    CHECK(Keys(s) == std::vector<std::string>{"Sprites/Gen 1", "Sprites/Gen 2", "UI/"});
    // Another pack: on top
    Add(s, "Font", {""});
    CHECK(Packs(s) == std::vector<std::string>{"Font", "Sprites", "UI"});
    Add(s, "Font", {""}); // already on: nothing changes
    CHECK(s.entries.size() == 4);
}

TEST_CASE("texlayers: fitting the list to a pack's groups", "[texlayers]") {
    State s{{{"A", ""}, {"B", "x"}, {"B", "gone"}, {"C", ""}, {"B", "y"}}};
    Fit(s, "A", {"a1", "a2"}); // a whole pack that now splits: its groups where it was
    Fit(s, "B", {"x", "y", "z"}); // a key gone, a new one after its last entry
    CHECK(Keys(s) == std::vector<std::string>{"A/a1", "A/a2", "B/x", "C/", "B/y", "B/z"});
    Fit(s, "B", {""}); // and back to one entry, at its first place
    CHECK(Keys(s) == std::vector<std::string>{"A/a1", "A/a2", "B/", "C/"});
    Fit(s, "D", {"d"}); // not on: left out
    CHECK(s.entries.size() == 4);
    Replace(s, "B", "B2"); // a pair: in its place
    CHECK(Packs(s) == std::vector<std::string>{"A", "B2", "C"});
    Replace(s, "A", "C"); // the other already on: it just goes
    CHECK(Packs(s) == std::vector<std::string>{"B2", "C"});
    // The file
    const State back = ParseState(StateJson(s));
    CHECK(back == s);
    CHECK(ParseState("not json").entries.empty());
    CHECK(ParseState(R"({"entries":[{"pack":5},{"key":"x"},{"pack":"P"},{"pack":"P"},"no"]})")
              .entries == std::vector<Entry>{{"P", ""}});
}

TEST_CASE("texlayers: the link and the saved list", "[texlayers]") {
    const auto exists = [](const std::string& p) { return p != "gone"; };
    const auto partner = [](const std::string& p) {
        return p == "png" ? std::string("png-astc4x4") : p == "png-astc4x4" ? "png" : "";
    };
    // Before this feature: the one pack the link points at
    CHECK(Resolve(std::nullopt, "A", exists, partner).entries == std::vector<Entry>{{"A", ""}});
    CHECK(Resolve(std::nullopt, "", exists, partner).entries.empty());
    // The list as saved, packs gone left out
    const State saved{{{"B", "k"}, {"gone", ""}, {"A", ""}}};
    CHECK(Resolve(saved, "B", exists, partner).entries ==
          std::vector<Entry>{{"B", "k"}, {"A", ""}});
    // No link: no pack is read
    CHECK(Resolve(saved, "", exists, partner).entries.empty());
    // The link moved to a pack not in the list (switched elsewhere): that pack alone
    CHECK(Resolve(saved, "Z", exists, partner).entries == std::vector<Entry>{{"Z", ""}});
    // ... unless it is the other pack of an Optimize pair in the list: in its place
    const State pair{{{"png", ""}, {"A", ""}}};
    CHECK(Resolve(pair, "png-astc4x4", exists, partner).entries ==
          std::vector<Entry>{{"png-astc4x4", ""}, {"A", ""}});
}

TEST_CASE("texlayers: on disk, the loader's files and the link", "[texlayers]") {
    Temp t;
    Paths paths{(t.dir / "load").generic_string(), {}};
    const fs::path lib = t.dir / "load" / "texture-packs" / TitleFolder(kTitle);
    for (const std::string& f : SpritesFiles()) {
        t.Write(fs::path("load/texture-packs") / TitleFolder(kTitle) / "Sprites" / f);
    }
    for (const std::string& f : UiFiles()) {
        t.Write(fs::path("load/texture-packs") / TitleFolder(kTitle) / "UI" / f);
    }
    t.Write(fs::path("load/texture-packs") / TitleFolder(kTitle) / "UI" / "pack.json",
            R"({"name": "5X Redrawn UI HD"})");
    t.Write(fs::path("load/texture-packs") / TitleFolder(kTitle) / "Sprites" / "pack.json",
            R"({"name": "Modern Pokemon Sprites", "layer_on": "pokemon-xy-5x-redrawn-ui"})");
    t.Write(fs::path("load/texture-packs") / TitleFolder(kTitle) / "UI" / ".azaharframe-download.json",
            R"({"id": "pokemon-xy-5x-redrawn-ui", "source": "gamebanana"})");
    const std::string sprites = fs::weakly_canonical(lib / "Sprites").generic_string();
    const std::string ui = fs::weakly_canonical(lib / "UI").generic_string();
    CHECK(BaseOf(sprites) == "pokemon-xy-5x-redrawn-ui");
    CHECK(CatalogId(ui) == "pokemon-xy-5x-redrawn-ui");

    // One pack on, as before this feature: the loader reads the link as it did
    std::vector<Pack> list = ListPacks(paths, kTitle);
    REQUIRE(list.size() == 2);
    const Pack& ui_pack = list[0].id == ui ? list[0] : list[1];
    REQUIRE(Activate(paths, kTitle, ui_pack).ok);
    CHECK_FALSE(LoaderFiles(paths.load_dir, kTitle).has_value());
    CHECK(Packs(Current(paths, kTitle)) == std::vector<std::string>{ui});

    // Modern Sprites on (Game, as the menus do it): above 5X, the link at it
    Game g = Game::Load(paths, kTitle);
    CHECK(g.IsOn(ui));
    g.SetOn(sprites, true);
    CHECK(Packs(g.list) == std::vector<std::string>{sprites, ui});
    REQUIRE(g.Save(paths, kTitle).ok);
    CHECK(Linked(paths, kTitle) == sprites);
    const auto files = LoaderFiles(paths.load_dir, kTitle);
    REQUIRE(files.has_value());
    CHECK(files->packs == std::vector<std::string>{sprites, ui});
    CHECK(files->files.size() == 6 + 24);
    CHECK(files->files[0].first == sprites + "/" + Tex("Gen 1", "00000000000000A1"));
    CHECK(files->files.back().first.starts_with(ui + "/"));
    const auto rows = g.Rows();
    REQUIRE(rows.size() == 7);
    CHECK(rows[0].name == "Gen 1");
    CHECK(rows[0].detail == "Modern Pokemon Sprites · Replaces 1 Texture From 5X Redrawn UI HD");
    CHECK(rows[0].value == 2);
    CHECK(rows[6].name == "5X Redrawn UI HD");
    CHECK(rows[6].detail == "24 Textures");
    CHECK(rows[6].value == -1);
    CHECK_FALSE(rows[0].up);
    CHECK(rows[6].up);
    CHECK_FALSE(rows[6].down);

    // A group off from the list: saved in the pack, the loader leaves it out
    REQUIRE(g.Toggle(0));
    REQUIRE(g.Save(paths, kTitle).ok);
    CHECK(Parts::ReadState(sprites).off.count("Gen 1") == 1);
    CHECK(LoaderFiles(paths.load_dir, kTitle)->files.size() == 5 + 24);

    // Moved: the pack's own Customize order follows
    REQUIRE(g.Move(1, 1)); // Gen 2 under Gen 3
    REQUIRE(g.Save(paths, kTitle).ok);
    const Parts::State st = Parts::ReadState(sprites);
    REQUIRE(st.order.size() == 6);
    CHECK(st.order[1] == "Gen 3");
    CHECK(st.order[2] == "Gen 2");

    // 5X off, then on again: under Modern Sprites (it is layered on 5X)
    Game g2 = Game::Load(paths, kTitle);
    CHECK(g2.list == g.list);
    g2.SetOn(ui, false);
    REQUIRE(g2.Save(paths, kTitle).ok);
    CHECK_FALSE(LoaderFiles(paths.load_dir, kTitle).has_value());
    g2.SetOn(ui, true);
    CHECK(Packs(g2.list) == std::vector<std::string>{sprites, ui});
    // Everything off: no link
    g2.SetOn(ui, false);
    g2.SetOn(sprites, false);
    REQUIRE(g2.Save(paths, kTitle).ok);
    CHECK(Linked(paths, kTitle).empty());
    CHECK(Current(paths, kTitle).entries.empty());
    // TurnOn / TurnOff (the crash guard, Delete)
    REQUIRE(TurnOn(paths, kTitle, ui).ok);
    REQUIRE(TurnOn(paths, kTitle, sprites).ok);
    CHECK(Packs(Current(paths, kTitle)) == std::vector<std::string>{sprites, ui});
    CHECK(ActivePacks(paths, kTitle).size() == 2);
    REQUIRE(TurnOff(paths, kTitle, sprites).ok);
    CHECK(Linked(paths, kTitle) == ui);
    CHECK(Packs(Current(paths, kTitle)) == std::vector<std::string>{ui});
}
