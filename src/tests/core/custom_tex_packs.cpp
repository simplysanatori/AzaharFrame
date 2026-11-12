// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texdata: pack discovery, pack.json, format detection, names, the symlink switch and the
// backup of a real folder (all in a temp dir).

#include <atomic>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <catch2/catch_test_macros.hpp>
#include "core/custom_tex/packs.h"

namespace fs = std::filesystem;
using namespace CustomTex;

namespace {

constexpr u64 kTitle = 0x0004000000033500ULL;
const std::string kTid = "0004000000033500";

struct Temp {
    fs::path root;
    Temp() {
        static int n = 0;
        root = fs::temp_directory_path() / ("azf_texpacks_" + std::to_string(::getpid()) + "_" +
                                            std::to_string(n++));
        fs::remove_all(root);
        fs::create_directories(root / "load");
    }
    ~Temp() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    fs::path load() const { return root / "load"; }
};

void Put(const fs::path& file, const std::string& content = "x") {
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << content;
}

Paths MakePaths(const Temp& t, std::vector<std::string> extra = {}) {
    return Paths{t.load().generic_string(), std::move(extra)};
}

const Pack* Find(const std::vector<Pack>& v, const std::string& name) {
    for (const Pack& p : v) {
        if (p.name == name) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("custom_tex: pack.json parse", "[core][custom_tex]") {
    const PackJson j = ParsePackJson(R"({"name":"Ultra","author":"A","version":2,"description":"d",
        "resolution":"4K","options":{"skip_mipmap":false}})");
    REQUIRE(j.name == "Ultra");
    REQUIRE(j.author == "A");
    REQUIRE(j.version == "2");
    REQUIRE(j.description == "d");
    REQUIRE(j.resolution == "4K");
    REQUIRE(ParsePackJson("not json").name.empty());
    REQUIRE(ParsePackJson("").author.empty());
    REQUIRE(ParsePackJson(R"({"name":5.5,"author":["x"]})").author.empty());
    REQUIRE(ParsePackJson(R"(// c
        {"name":"C"})").name == "C"); // comments allowed like Azahar's own reader
}

TEST_CASE("custom_tex: names and resolution", "[core][custom_tex]") {
    REQUIRE(DeriveName("textures-4k", kTid + "-astc4x4", kTid) == "4K");
    REQUIRE(DeriveName("textures-1080p", kTid, kTid) == "1080p");
    REQUIRE(DeriveName("textures", kTid, kTid) == "Default");
    REQUIRE(DeriveName("misc", kTid + "-fancy", kTid) == "Fancy");
    REQUIRE(ResolutionHint("textures-4k", kTid + "-astc4x4") == "4K");
    REQUIRE(ResolutionHint("x", "1080P-pack") == "1080p");
    REQUIRE(ResolutionHint("x", "y").empty());
}

TEST_CASE("custom_tex: format detection", "[core][custom_tex]") {
    Temp t;
    REQUIRE(DetectFormat((t.root / "none").generic_string()) == Format::Unknown);
    Put(t.root / "png" / "a.png");
    REQUIRE(DetectFormat((t.root / "png").generic_string()) == Format::Png);
    Put(t.root / "ktx" / "sub" / "a.KTX");
    REQUIRE(DetectFormat((t.root / "ktx").generic_string()) == Format::Ktx);
    Put(t.root / "dds" / "a.dds");
    REQUIRE(DetectFormat((t.root / "dds").generic_string()) == Format::Dds);
    Put(t.root / "mix" / "a.png");
    Put(t.root / "mix" / "b.ktx");
    REQUIRE(DetectFormat((t.root / "mix").generic_string()) == Format::Mixed);
    REQUIRE(FormatName(Format::Ktx) == "ASTC/KTX");
}

TEST_CASE("custom_tex: discovery in managed place and extra roots", "[core][custom_tex]") {
    Temp t;
    // the owner's layout
    Put(t.root / "desk" / "textures-4k" / kTid / "pack.json", R"({"author":"Gamer","version":"1.2"})");
    Put(t.root / "desk" / "textures-4k" / kTid / "a.png");
    Put(t.root / "desk" / "textures-4k" / (kTid + "-astc4x4") / "pack.json", "{}");
    Put(t.root / "desk" / "textures-4k" / (kTid + "-astc4x4") / "a.ktx");
    Put(t.root / "desk" / "textures-1080p" / (kTid + "-astc4x4") / "a.ktx");
    Put(t.root / "desk" / "textures-1080p" / "0004000000099999" / "a.ktx"); // another game
    // managed
    Put(t.load() / "texture-packs" / kTid / "Mine" / "pack.json", R"({"name":"My Pack"})");
    Put(t.load() / "texture-packs" / kTid / "Mine" / "a.png");

    const Paths paths = MakePaths(t, {(t.root / "desk" / "textures-*").generic_string()});
    const auto packs = ListPacks(paths, kTitle);
    REQUIRE(packs.size() == 4);
    const Pack* png4k = Find(packs, "4K (PNG)");
    REQUIRE(png4k);
    REQUIRE(png4k->author == "Gamer");
    REQUIRE(png4k->version == "1.2");
    REQUIRE(png4k->resolution == "4K");
    REQUIRE(png4k->format == Format::Png);
    REQUIRE_FALSE(png4k->managed);
    REQUIRE(Find(packs, "4K"));
    REQUIRE(Find(packs, "4K")->format == Format::Ktx);
    REQUIRE(Find(packs, "1080p"));
    const Pack* mine = Find(packs, "My Pack");
    REQUIRE(mine);
    REQUIRE(mine->managed);
    for (const Pack& p : packs) {
        REQUIRE_FALSE(p.active);
        REQUIRE_FALSE(p.size_bytes.has_value()); // sizes are lazy
    }
    // a plain (non-glob) root works too
    const auto packs2 = ListPacks(MakePaths(t, {(t.root / "desk" / "textures-1080p").generic_string()}), kTitle);
    REQUIRE(packs2.size() == 2); // 1080p + managed
}

TEST_CASE("custom_tex: switch by symlink, active flag, deactivate", "[core][custom_tex]") {
    Temp t;
    Put(t.root / "desk" / "textures-4k" / (kTid + "-astc4x4") / "a.ktx", "1234");
    Put(t.root / "desk" / "textures-1080p" / (kTid + "-astc4x4") / "a.ktx", "12");
    const Paths paths = MakePaths(t, {(t.root / "desk" / "textures-*").generic_string()});
    REQUIRE_FALSE(ActivePack(paths, kTitle).has_value());

    auto packs = ListPacks(paths, kTitle);
    const Pack p4k = *Find(packs, "4K");
    const Pack p1080 = *Find(packs, "1080p");

    REQUIRE(Activate(paths, kTitle, p4k).ok);
    REQUIRE(fs::is_symlink(LoadFolder(paths, kTitle)));
    REQUIRE(ActivePack(paths, kTitle)->id == p4k.id);
    REQUIRE(fs::exists(fs::path(LoadFolder(paths, kTitle)) / "a.ktx"));

    REQUIRE(Activate(paths, kTitle, p1080).ok);
    REQUIRE(ActivePack(paths, kTitle)->id == p1080.id);
    packs = ListPacks(paths, kTitle);
    REQUIRE(packs.size() == 2); // the link target is the same pack, not a third entry
    REQUIRE(Find(packs, "1080p")->active);
    REQUIRE_FALSE(Find(packs, "4K")->active);
    REQUIRE(Activate(paths, kTitle, p1080).ok); // already active

    REQUIRE(Deactivate(paths, kTitle).ok);
    REQUIRE_FALSE(fs::exists(fs::symlink_status(LoadFolder(paths, kTitle))));
    REQUIRE_FALSE(ActivePack(paths, kTitle).has_value());
    // nothing was deleted
    REQUIRE(fs::exists(t.root / "desk" / "textures-4k" / (kTid + "-astc4x4") / "a.ktx"));
    REQUIRE(fs::exists(t.root / "desk" / "textures-1080p" / (kTid + "-astc4x4") / "a.ktx"));
    REQUIRE(Deactivate(paths, kTitle).ok); // nothing to do

    Pack missing = p4k;
    missing.path = (t.root / "gone").generic_string();
    REQUIRE_FALSE(Activate(paths, kTitle, missing).ok);
}

TEST_CASE("custom_tex: a real folder is moved into the library, not deleted", "[core][custom_tex]") {
    Temp t;
    const Paths paths = MakePaths(t, {(t.root / "desk").generic_string()});
    Put(t.load() / "textures" / kTid / "a.png", "keep");
    Put(t.load() / "textures" / kTid / "pack.json", R"({"name":"Hand Made"})");
    Put(t.root / "desk" / kTid / "b.png");
    // the real folder is itself reported as the active pack
    REQUIRE(ActivePack(paths, kTitle)->active);
    REQUIRE(ActivePack(paths, kTitle)->name == "Hand Made");

    const Pack other = *Find(ListPacks(paths, kTitle), "Default (PNG)");
    const Result r = Activate(paths, kTitle, other);
    REQUIRE(r.ok);
    REQUIRE(r.backed_up == (t.load() / "texture-packs" / kTid / "Hand Made").generic_string());
    REQUIRE(fs::is_symlink(LoadFolder(paths, kTitle)));
    std::ifstream f(fs::path(r.backed_up) / "a.png");
    std::string content;
    f >> content;
    REQUIRE(content == "keep");
    const auto packs = ListPacks(paths, kTitle);
    REQUIRE(Find(packs, "Hand Made")->managed);

    // a second real folder gets a unique name
    REQUIRE(Deactivate(paths, kTitle).ok);
    Put(t.load() / "textures" / kTid / "pack.json", R"({"name":"Hand Made"})");
    const Result r2 = Deactivate(paths, kTitle); // real folder, "None": moved away
    REQUIRE(r2.ok);
    REQUIRE(r2.backed_up == (t.load() / "texture-packs" / kTid / "Hand Made 2").generic_string());
    REQUIRE_FALSE(fs::exists(LoadFolder(paths, kTitle)));
}

TEST_CASE("custom_tex: an empty real folder is just removed", "[core][custom_tex]") {
    Temp t;
    const Paths paths = MakePaths(t);
    Put(t.root / "p" / "a.png");
    fs::create_directories(t.load() / "textures" / kTid);
    Pack p;
    p.path = (t.root / "p").generic_string();
    const Result r = Activate(paths, kTitle, p);
    REQUIRE(r.ok);
    REQUIRE(r.backed_up.empty());
    REQUIRE(fs::is_symlink(LoadFolder(paths, kTitle)));
}

TEST_CASE("custom_tex: sizes are lazy and cached", "[core][custom_tex]") {
    Temp t;
    ForgetSizes();
    Put(t.root / "p" / "a.png", std::string(1000, 'a'));
    Put(t.root / "p" / "s" / "b.png", std::string(234, 'b'));
    const std::string path = (t.root / "p").generic_string();
    REQUIRE_FALSE(CachedSize(path).has_value());
    std::atomic_bool cancel{true};
    REQUIRE_FALSE(ComputeSize(path, &cancel).has_value());
    REQUIRE_FALSE(CachedSize(path).has_value());
    REQUIRE(*ComputeSize(path) == 1234);
    REQUIRE(*CachedSize(path) == 1234);

    const Paths paths = MakePaths(t, {(t.root).generic_string()});
    Put(t.root / kTid / "x.png", "12345");
    ComputeSize((t.root / kTid).generic_string());
    const auto packs = ListPacks(paths, kTitle);
    REQUIRE(packs.size() == 1);
    REQUIRE(*packs[0].size_bytes == 5);
    ForgetSizes();
}

TEST_CASE("custom_tex: roots file", "[core][custom_tex]") {
    Temp t;
    REQUIRE(WriteExtraRoots(t.load().generic_string(), {"/a/b", "/c/textures-*"}));
    const auto roots = ReadExtraRoots(t.load().generic_string());
    REQUIRE(roots.size() >= 2);
    REQUIRE(roots[0] == "/a/b");
    REQUIRE(roots[1] == "/c/textures-*");
    REQUIRE_FALSE(kReloadSupported);
}
