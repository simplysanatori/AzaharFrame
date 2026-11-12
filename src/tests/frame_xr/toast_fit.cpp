// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: a toast's box fits its text (frame_xr/toast_fit.h).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/toast_fit.h"

using Catch::Approx;
using namespace FrameXr::ToastFit;

TEST_CASE("Toasts: the box fits the text, never cut", "[frame_xr]") {
    const auto never = [] {
        FAIL("two lines asked for a line that fits");
        return 0.0f;
    };
    // Short: the size every toast had.
    Box b = Fit(200.0f, never);
    CHECK(b.box_px == kMinBoxPx);
    CHECK_FALSE(b.two_lines);
    CHECK(b.width_m == Approx(0.45f));
    // "Narrow Screen: From the Next Start" in the 40 px font is about 700 px: a wider box,
    // the text whole, the same scale up to kMaxWidthM.
    b = Fit(700.0f, never);
    CHECK(b.box_px >= 700 + 2 * static_cast<int>(kPadPx));
    CHECK_FALSE(b.two_lines);
    CHECK(b.width_m <= kMaxWidthM + 1e-6f);
    CHECK(b.height_m == Approx(b.width_m * kRowPx / b.box_px));
    // Too long for a row: two lines in the small font, in a box that holds them.
    b = Fit(1500.0f, [] { return 600.0f; });
    CHECK(b.two_lines);
    CHECK(b.box_px >= 600 + 2 * static_cast<int>(kPadPx));
    CHECK(b.box_px <= kAtlasW);
    // Never past the row.
    b = Fit(5000.0f, [] { return 5000.0f; });
    CHECK(b.box_px == kAtlasW);
}

// AzaharFrame ui3: a one-shot message stays five seconds, the answer to a press two (a newer toast
// replaces either at once: hud.cpp).
TEST_CASE("Toasts: how long they stay", "[frame_xr]") {
    using FrameXr::ToastTime;
    CHECK(DurationNs(ToastTime::Short) == 2'000'000'000);
    CHECK(DurationNs(ToastTime::Long) == 5'000'000'000);
    CHECK(DurationNs(ToastTime::Long) >= 4'000'000'000);
}
