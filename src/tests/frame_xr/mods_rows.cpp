// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fixes7: the Download Packs and Install Mods rows' words for a download's progress by
// size, a kept partial download (Resume), and Optimize for Frame's progress line.

#include <cstdint>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/mods_segments.h"

using namespace FrameXr::Menu;

namespace {
constexpr uint64_t kMiB = 1024ull * 1024;
constexpr uint64_t kGiB = 1024ull * kMiB;
} // namespace

TEST_CASE("fixes7: progress by size", "[frame_xr][fixes7]") {
    CHECK(ProgressSizeText(kGiB * 12 / 10, kGiB * 34 / 10) == "1.2 of 3.4 GB");
    CHECK(ProgressSizeText(120 * kMiB, kGiB * 34 / 10) == "120 MB of 3.4 GB");
    CHECK(ProgressSizeText(0, kGiB * 34 / 10) == "0 of 3.4 GB");
    CHECK(ProgressSizeText(120 * kMiB, 0) == "120 MB"); // no total known
    CHECK(ProgressSizeText(12 * kMiB, 30 * kMiB) == "12 of 30 MB");
    // more than the catalog said: the total follows
    CHECK(ProgressSizeText(40 * kMiB, 30 * kMiB) == "40 of 40 MB");
}

TEST_CASE("fixes7: a kept partial download says Resume", "[frame_xr][fixes7]") {
    CHECK(ResumeText(0, kGiB).empty());
    CHECK(ResumeText(kGiB * 12 / 10, kGiB * 34 / 10) == "1.2 of 3.4 GB Downloaded");
    CHECK(ResumeText(kGiB * 12 / 10, 0) == "1.2 GB Downloaded");

    PackOffer pack;
    pack.downloads = "10K+ Downloads";
    pack.updated = "Updated Mar 2026";
    CHECK(PackButtonLabel(pack) == "Download");
    pack.resume = ResumeText(kGiB * 12 / 10, kGiB * 34 / 10);
    CHECK(PackButtonLabel(pack) == "Resume");
    CHECK(PackOfferParts(pack) == std::vector<std::string>{"1.2 of 3.4 GB Downloaded"});
    pack.state = PackOfferState::Failed;
    pack.note = "No connection";
    CHECK(PackButtonLabel(pack) == "Try Again"); // goes on from the kept part too
    CHECK(PackOfferParts(pack) == std::vector<std::string>{"No connection"});
    pack.state = PackOfferState::Downloading;
    pack.note = "Preparing";
    CHECK(PackOfferParts(pack) == std::vector<std::string>{"Preparing"});

    ModOffer mod;
    mod.mod_version = "v172";
    mod.downloads = "1K+ Downloads";
    CHECK(OfferButtonLabel(mod) == "Install");
    mod.resume = ResumeText(12 * kMiB, 30 * kMiB);
    CHECK(OfferButtonLabel(mod) == "Resume");
    CHECK(OfferParts(mod) == std::vector<std::string>{"v172", "12 of 30 MB Downloaded"});
    mod.state = OfferState::Installing;
    mod.note = "Downloading 40% · 12 of 30 MB";
    CHECK(OfferButtonLabel(mod) == "Install"); // (Cancel shows while it installs)
    CHECK(OfferParts(mod) == std::vector<std::string>{"v172", "Downloading 40% · 12 of 30 MB"});
}

TEST_CASE("fixes7: Optimize for Frame's progress is one measure", "[frame_xr][fixes7]") {
    PackOptimize o;
    o.state = OptimizeState::Converting;
    o.done = 137;
    o.total = 1624;
    o.permille = 491;
    o.done_bytes = kGiB * 3;
    o.total_bytes = kGiB * 61 / 10;
    // the size of the optimized copy written of its estimate, and the percent of that
    CHECK(OptimizeText(o).line == "3.0 of 6.1 GB · 49%");
    CHECK(OptimizeText(o).bar);
    // an older converter with no sizes: the count as before
    o.done_bytes = o.total_bytes = 0;
    CHECK(OptimizeText(o).line == "137 of 1,624 Textures · 49%");
}
