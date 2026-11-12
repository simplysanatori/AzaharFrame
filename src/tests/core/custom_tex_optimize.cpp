// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ui3: Optimize for Frame's file side (core/custom_tex/optimize.h): the pair, what the
// converter prints, the Customize choices carried both ways, the packs asked about once.

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include "core/custom_tex/optimize.h"
#include "core/custom_tex/parts.h"

namespace fs = std::filesystem;
using namespace CustomTex::Optimize;

namespace {

fs::path TempDir(const std::string& name) {
    const fs::path p = fs::temp_directory_path() /
                       ("azf-optimize-" + name + "-" + std::to_string(::getpid()));
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}

void Write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::string Read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

} // namespace

TEST_CASE("optimize: the copy's name and the pair", "[texparts][optimize]") {
    CHECK(OptimizedPath("/l/texture-packs/0004000000033500/4K") ==
          "/l/texture-packs/0004000000033500/4K-astc4x4");
    CHECK(OptimizedPath("/l/textures/0004000000033500-original/") ==
          "/l/textures/0004000000033500-astc4x4");

    const fs::path root = TempDir("pair");
    const fs::path png = root / "4K (PNG)";
    Write(png / "tex1_64x64_0123456789ABCDEF_14.png", "x");
    // A PNG pack with no copy yet; a half-done copy (no marker) is not one.
    fs::create_directories(OptimizedPath(png.string()));
    Pair p = PairOf(png.string(), true);
    CHECK(p.png == png.string());
    CHECK(p.optimized.empty());
    CHECK_FALSE(IsOptimized(OptimizedPath(png.string())));
    // The marker is written last: now it is the pair's copy, from either side.
    Write(fs::path(OptimizedPath(png.string())) / kMarker,
          R"j({"format": "ASTC 4x4", "source": "../4K (PNG)", "textures": 1})j");
    p = PairOf(png.string(), true);
    CHECK(p.optimized == OptimizedPath(png.string()));
    const Pair q = PairOf(OptimizedPath(png.string()), false);
    CHECK(q.optimized == OptimizedPath(png.string()));
    CHECK(q.png == png.string());
    CHECK(SourceOf(OptimizedPath(png.string())) == png.string());
    // Not PNG and not a copy: no pair.
    const Pair none = PairOf((root / "KTX").string(), false);
    CHECK(none.png.empty());
    CHECK(none.optimized.empty());
    // A copy whose original was deleted has nothing to go back to.
    fs::remove_all(png);
    CHECK(PairOf(OptimizedPath(png.string()), false).png.empty());
    // A broken marker names no source.
    Write(fs::path(OptimizedPath(png.string())) / kMarker, "{not json");
    CHECK_FALSE(SourceOf(OptimizedPath(png.string())).has_value());
    fs::remove_all(root);
}

TEST_CASE("optimize: a failed run in words", "[texparts][optimize]") {
    CHECK(FailureText(kExitNoSpace) == "Not Enough Space");
    CHECK(FailureText(2) == "Pack Not Readable");
    CHECK(FailureText(1) == "Conversion Failed");
}

TEST_CASE("optimize: the Customize choices go with the switch, both ways", "[texparts][optimize]") {
    const fs::path root = TempDir("parts");
    const fs::path png = root / "png", copy = root / "png-astc4x4";
    fs::create_directories(png);
    fs::create_directories(copy);
    const std::string choices = R"({"off": ["UI"]})";
    Write(png / CustomTex::Parts::kStateFile, choices);
    REQUIRE(CarryParts(png.string(), copy.string()));
    CHECK(Read(copy / CustomTex::Parts::kStateFile) == choices);
    // Changed on the copy, then back to the original.
    const std::string later = R"({"off": ["UI", "Effects"]})";
    Write(copy / CustomTex::Parts::kStateFile, later);
    REQUIRE(CarryParts(copy.string(), png.string()));
    CHECK(Read(png / CustomTex::Parts::kStateFile) == later);
    // No choices on one side: none on the other (all parts on).
    fs::remove(copy / CustomTex::Parts::kStateFile);
    REQUIRE(CarryParts(copy.string(), png.string()));
    CHECK_FALSE(fs::exists(png / CustomTex::Parts::kStateFile));
    CHECK_FALSE(CarryParts(png.string(), (root / "missing").string()));
    fs::remove_all(root);
}

TEST_CASE("optimize: each pack is asked about once", "[texparts][optimize]") {
    const fs::path load = TempDir("asked");
    CHECK_FALSE(AskedBefore(load.string(), "/a/4K (PNG)"));
    MarkAsked(load.string(), "/a/4K (PNG)");
    MarkAsked(load.string(), "/a/4K (PNG)");
    CHECK(AskedBefore(load.string(), "/a/4K (PNG)"));
    CHECK_FALSE(AskedBefore(load.string(), "/a/4K"));
    CHECK(Read(load / "texture-packs" / "optimize-asked.txt") == "/a/4K (PNG)\n");
    CHECK(kNudgeBytes == 1'000'000'000ULL);
    fs::remove_all(load);
}

// AzaharFrame ui3: Remove Original deletes only the folder the copy's marker names, inside the
// managed texture folders, never a link, the copy, a folder round it, or what the game reads.
TEST_CASE("optimize: Remove Original's guard", "[texparts][optimize][remove]") {
    const fs::path root = TempDir("remove");
    const fs::path load = root / "load";
    const fs::path packs = load / "texture-packs" / "0004000000033500";
    const fs::path outside = root / "outside";
    fs::create_directories(load / "textures");
    fs::create_directories(outside);
    Write(outside / "keep.txt", "mine");
    const fs::path png = packs / "4K";
    const fs::path copy = packs / "4K-astc4x4";
    Write(png / "tex1_64x64_0123456789ABCDEF_14.png", "x");
    Write(png / ".azaharframe-download.json", R"({"id":"oot3d-4k","source":"github"})");
    fs::create_symlink(outside / "keep.txt", png / "a-link-out.txt"); // removed, never followed
    const auto marker = [&](const std::string& source) {
        Write(copy / kMarker, source.empty() ? std::string(R"({"format":"ASTC 4x4"})")
                                             : R"({"format":"ASTC 4x4","source":")" + source + "\"}");
    };
    marker("../4K");
    Write(copy / "tex1_64x64_0123456789ABCDEF_14.ktx", "k");
    const fs::path active = load / "textures" / "0004000000033500";
    fs::create_symlink(copy, active);
    const auto roots = ManagedRoots(load.string());

    // The right one, the copy in use: allowed, and it is a catalog pack ("download it again").
    RemoveCheck c = CheckRemoveOriginal(copy.string(), roots, active.string());
    REQUIRE(c.Ok());
    CHECK(c.original == fs::canonical(png).string());
    CHECK(FromCatalog(c.original));
    // Through the game's link to the copy (as Flat names it): the same folder.
    CHECK(CheckRemoveOriginal(active.string(), roots, active.string()).original == c.original);

    // What the game reads now: never.
    fs::remove(active);
    fs::create_symlink(png, active);
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error == "Original Is in Use");
    fs::remove(active);
    fs::create_symlink(copy, active);

    // A wrong source: the copy itself, the folder holding both (a root's child) and a root, another
    // optimized copy, outside the texture folders.
    marker(".");
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error == "Not the Original");
    marker("..");
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error == "Not the Original");
    marker("../..");
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error ==
          "Original Outside the Texture Folders");
    Write(packs / "Other-astc4x4" / kMarker, "{}");
    marker("../Other-astc4x4");
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error == "Not the Original");
    marker(outside.string());
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error ==
          "Original Outside the Texture Folders");
    marker("../../../../outside");
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error ==
          "Original Outside the Texture Folders");

    // A link: never followed, as the source or on the way to it.
    fs::create_symlink(outside, packs / "escape");
    marker("../escape");
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error == "Original Is a Link");
    fs::create_directories(outside / "pack");
    marker("../escape/pack");
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error ==
          "Original Outside the Texture Folders");
    std::string error;
    CHECK_FALSE(RemoveOriginal(copy.string(), roots, active.string(), &error));
    CHECK(fs::is_directory(outside / "pack"));

    // Missing: no source in the marker, or the folder is gone.
    marker("");
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error == "Original Not Found");
    marker("../Gone");
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error == "Original Not Found");
    CHECK(CheckRemoveOriginal(png.string(), roots, active.string()).error == "Not Optimized");

    // Removed: the original only; the copy, its choices and the file a link inside pointed at stay.
    marker("../4K");
    Write(copy / CustomTex::Parts::kStateFile, R"({"off":["UI"]})");
    REQUIRE(RemoveOriginal(copy.string(), roots, active.string(), &error));
    CHECK_FALSE(fs::exists(png));
    CHECK(fs::is_regular_file(copy / "tex1_64x64_0123456789ABCDEF_14.ktx"));
    CHECK(fs::is_regular_file(copy / CustomTex::Parts::kStateFile));
    CHECK(Read(outside / "keep.txt") == "mine");
    CHECK(PairOf(copy.string(), false).png.empty()); // Original Removed
    CHECK(CheckRemoveOriginal(copy.string(), roots, active.string()).error == "Original Not Found");

    // Install from Zip or your own: "add it again".
    Write(packs / "Mine" / ".azaharframe-download.json", R"({"id":"zip:Mine","source":"zip"})");
    CHECK_FALSE(FromCatalog((packs / "Mine").string()));
    CHECK_FALSE(FromCatalog((packs / "Gone").string()));
    fs::remove_all(root);
}
