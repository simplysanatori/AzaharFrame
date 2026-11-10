// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cstdlib>

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/flat_fit.h"

// AzaharFrame flatdialog
using namespace FrameXr;

namespace {

constexpr WindowSize kMain{1252, 704}; // the main window in Flat (FrameMirror::FlatWindowSize)
constexpr WindowSize kNoMax{16777215, 16777215}; // QWIDGETSIZE_MAX

bool Is16x9(WindowSize s) {
    // Within a pixel of 16:9 (even widths).
    return std::abs(s.width * 9 - s.height * 16) <= 16;
}

} // namespace

TEST_CASE("FrameXr flat fit: the area is the largest 16:9 size in the window", "[frame_xr]") {
    const WindowSize a = FlatArea(kMain);
    REQUIRE(a.width == 1252);
    REQUIRE(a.height == 704);
    const WindowSize wide = FlatArea({1920, 800});
    REQUIRE(wide.height == 800);
    REQUIRE(wide.width == 1422);
    const WindowSize tall = FlatArea({1000, 1000});
    REQUIRE(tall.width == 1000);
    REQUIRE(tall.height == 563);
    REQUIRE(FlatArea({0, 704}).width == 0);
}

TEST_CASE("FrameXr flat fit: a tall Configure gets the whole area and needs scrolling",
          "[frame_xr]") {
    // Taller than the theater: OK and Cancel were under its bottom edge.
    const FlatDialogFit fit = FitFlatDialog(kMain, {900, 1100}, {880, 1100}, kNoMax);
    REQUIRE(fit.too_small);
    REQUIRE(fit.size.width == 1252);
    REQUIRE(fit.size.height == 1100); // its minimum, until the content scrolls
    // With the content in a scroll area its minimum is small: the whole area, 16:9.
    const FlatDialogFit scrolled = FitFlatDialog(kMain, {900, 1100}, {300, 200}, kNoMax);
    REQUIRE_FALSE(scrolled.too_small);
    REQUIRE(scrolled.size.width == 1252);
    REQUIRE(scrolled.size.height == 704);
}

TEST_CASE("FrameXr flat fit: a dialog is 16:9, never above the area nor below half of it",
          "[frame_xr]") {
    const FlatDialogFit mid = FitFlatDialog(kMain, {800, 600}, {400, 300}, kNoMax);
    REQUIRE_FALSE(mid.too_small);
    REQUIRE(Is16x9(mid.size));
    REQUIRE(mid.size.width >= 800);
    REQUIRE(mid.size.height >= 600);
    REQUIRE(mid.size.width <= kMain.width);
    REQUIRE(mid.size.height <= kMain.height);

    const FlatDialogFit small = FitFlatDialog(kMain, {200, 100}, {100, 50}, kNoMax);
    REQUIRE(small.size.width == 626);
    REQUIRE(small.size.height == 352);

    const FlatDialogFit big = FitFlatDialog(kMain, {1280, 720}, {600, 400}, kNoMax);
    REQUIRE(big.size.width == 1252);
    REQUIRE(big.size.height == 704);
    REQUIRE((big.size.width % 2) == 0);
}

TEST_CASE("FrameXr flat fit: a fixed size is kept", "[frame_xr]") {
    // A message box fixes its size (minimum = maximum).
    const FlatDialogFit box = FitFlatDialog(kMain, {420, 160}, {420, 160}, {420, 160});
    REQUIRE_FALSE(box.too_small);
    REQUIRE(box.size.width == 420);
    REQUIRE(box.size.height == 160);
    // A maximum height only: 16:9 width, its height.
    const FlatDialogFit capped = FitFlatDialog(kMain, {700, 300}, {300, 200}, {kNoMax.width, 300});
    REQUIRE(capped.size.height == 300);
    REQUIRE(capped.size.width >= 700);
}

TEST_CASE("FrameXr flat fit: a Fullscreen main window and no main window", "[frame_xr]") {
    const FlatDialogFit full = FitFlatDialog({1920, 1080}, {900, 1100}, {300, 200}, kNoMax);
    REQUIRE(full.size.width == 1920);
    REQUIRE(full.size.height == 1080);
    const FlatDialogFit none = FitFlatDialog({0, 0}, {640, 480}, {100, 100}, kNoMax);
    REQUIRE(none.size.width == 640);
    REQUIRE(none.size.height == 480);
}
