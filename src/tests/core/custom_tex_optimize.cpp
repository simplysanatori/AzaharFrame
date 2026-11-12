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

TEST_CASE("optimize: what the converter prints", "[texparts][optimize]") {
    const auto e = ParseEstimate("estimate 2308 4700000000 1200000000\n");
    REQUIRE(e.has_value());
    CHECK(e->textures == 2308);
    CHECK(e->total_bytes == 4'700'000'000ULL);
    CHECK(e->need_bytes == 1'200'000'000ULL);
    CHECK_FALSE(ParseEstimate("estimate 1 2").has_value());
    CHECK_FALSE(ParseEstimate("estimated 1 2 3").has_value());
    CHECK_FALSE(ParseEstimate("estimate 1 2 x").has_value());
    const auto p = ParseProgress("progress 1234 2308 534");
    REQUIRE(p.has_value());
    CHECK(p->done == 1234);
    CHECK(p->total == 2308);
    CHECK(p->permille == 534);
    CHECK_FALSE(ParseProgress("progress 3 2 10").has_value());    // done past the total
    CHECK_FALSE(ParseProgress("progress 1 2 1001").has_value());  // past 100 %
    CHECK_FALSE(ParseProgress("2308 PNG textures, 0 already converted").has_value());
    CHECK(FailureText(kExitNoSpace) == "Not Enough Space");
    CHECK(FailureText(kExitNoEncoder) == "Encoder Not Built");
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
