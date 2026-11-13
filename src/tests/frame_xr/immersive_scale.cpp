// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu: Horizontal Scale and Vertical Scale (they replace Narrow Screen and Narrow
// Height): the setting, the D-pad steps, the toasts, the migration of the old values, and the
// head-locked screen at both signs.

#include <algorithm>
#include <cmath>
#include <random>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/immersive_layout.h"

using Catch::Approx;
using namespace FrameXr::Immersive::Layout;

namespace {
ViewFov Symmetric(float half_deg) {
    ViewFov v;
    v.left = -half_deg * kDegToRad;
    v.right = half_deg * kDegToRad;
    v.up = half_deg * kDegToRad;
    v.down = -half_deg * kDegToRad;
    return v;
}
constexpr float kAspect3ds = 400.0f / 240.0f;
} // namespace

TEST_CASE("FrameXr immersive scale: the setting", "[frame_xr][kiu]") {
    // On the 5 % grid, -30 to +30, halves away from 0 the same either side.
    REQUIRE(ScalePercent(0) == 0);
    REQUIRE(ScalePercent(10) == 10);
    REQUIRE(ScalePercent(-10) == -10);
    REQUIRE(ScalePercent(7) == 5);
    REQUIRE(ScalePercent(-7) == -5);
    REQUIRE(ScalePercent(8) == 10);
    REQUIRE(ScalePercent(-8) == -10);
    REQUIRE(ScalePercent(30) == 30);
    REQUIRE(ScalePercent(31) == 30);
    REQUIRE(ScalePercent(-99) == -30);
    REQUIRE(ScalePercent(1000) == 30);
    for (int v = -40; v <= 40; ++v) {
        REQUIRE(ScalePercent(-v) == -ScalePercent(v));
        REQUIRE(ScalePercent(v) % kScaleStepPercent == 0);
        REQUIRE(ScalePercent(ScalePercent(v)) == ScalePercent(v));
    }
    // Positive is larger: the quad's narrowing (Options::squeeze) is its negative.
    REQUIRE(SqueezeForScale(0) == 0.0f);
    REQUIRE(SqueezeForScale(-10) == Approx(0.1f));
    REQUIRE(SqueezeForScale(15) == Approx(-0.15f));
    REQUIRE(SqueezeForScale(99) == Approx(-0.3f));
}

TEST_CASE("FrameXr immersive scale: D-pad steps and toasts", "[frame_xr][kiu]") {
    REQUIRE(SteppedScale(0, 1) == 5);
    REQUIRE(SteppedScale(0, -1) == -5);
    REQUIRE(SteppedScale(-10, 2) == 0);
    REQUIRE(SteppedScale(30, 1) == 30);
    REQUIRE(SteppedScale(-30, -1) == -30);
    REQUIRE(SteppedScale(7, 1) == 10); // a hand-edited value goes onto the grid first
    for (int v = -30; v <= 30; v += 5) {
        for (const int steps : {-1, 1}) {
            const int next = SteppedScale(v, steps);
            REQUIRE(next >= -kMaxScalePercent);
            REQUIRE(next <= kMaxScalePercent);
            REQUIRE(next % kScaleStepPercent == 0);
        }
    }
    REQUIRE(ScaleToast(-10, false) == "Horizontal Scale: -10%");
    REQUIRE(ScaleToast(0, false) == "Horizontal Scale: 0%");
    REQUIRE(ScaleToast(5, false) == "Horizontal Scale: +5%");
    REQUIRE(ScaleToast(30, true) == "Vertical Scale: +30%");
    REQUIRE(ScaleToast(-15, true) == "Vertical Scale: -15%");
    REQUIRE(ScaleToast(0, true) == "Vertical Scale: 0%");
    REQUIRE(ScaleToast(99, true) == "Vertical Scale: +30%");
    REQUIRE(WiderToast(0) == "Render Margin: Off");
    REQUIRE(WiderToast(2) == "Render Margin: 1.2x");
}

TEST_CASE("FrameXr immersive scale: Narrow Screen and Narrow Height carry over", "[frame_xr][kiu]") {
    // -NarrowPercent, with the legacy rule (1 to 3 were 10 % steps).
    REQUIRE(ScaleFromNarrow(0) == 0);
    REQUIRE(ScaleFromNarrow(1) == -10);
    REQUIRE(ScaleFromNarrow(2) == -20);
    REQUIRE(ScaleFromNarrow(3) == -30);
    REQUIRE(ScaleFromNarrow(5) == -5);
    REQUIRE(ScaleFromNarrow(10) == -10);
    REQUIRE(ScaleFromNarrow(15) == -15);
    REQUIRE(ScaleFromNarrow(30) == -30);
    REQUIRE(ScaleFromNarrow(99) == -30);
    // The screen comes out exactly as before: the same narrowing.
    for (uint32_t old = 0; old <= 40; ++old) {
        REQUIRE(SqueezeForScale(ScaleFromNarrow(old)) == Approx(SqueezeFor(old)));
    }
}

TEST_CASE("FrameXr immersive scale: the screen at both signs", "[frame_xr][kiu]") {
    const ViewFov view = Symmetric(50);
    const HeadLocked plain = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view);
    for (const int x : {-30, -15, -5, 0, 5, 15, 30}) {
        for (const int y : {-30, -10, 0, 10, 30}) {
            Options o;
            o.squeeze = SqueezeForScale(x);
            o.squeeze_y = SqueezeForScale(y);
            const HeadLocked s = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o);
            // The quad: that much wider (taller) at a positive scale, narrower (lower) below 0.
            REQUIRE(s.width() == Approx(plain.width() * (1.0f + x / 100.0f)).epsilon(1e-4));
            REQUIRE(s.height() == Approx(plain.height() * (1.0f + y / 100.0f)).epsilon(1e-4));
            // The gain of each axis over the same: the world stays locked at both signs.
            const auto clamped = [](float g) { return std::clamp(g, kMinGain, kMaxGain); };
            REQUIRE(s.gain_x == Approx(clamped(plain.gain_x / (1.0f + x / 100.0f))));
            REQUIRE(s.gain == Approx(clamped(plain.gain / (1.0f + y / 100.0f))));
            REQUIRE(s.factor == Approx(plain.factor)); // the game draws the same picture
            REQUIRE(s.factor_y == Approx(plain.factor_y));
        }
    }
}

// Whatever the settings: the gain times the quad's half size stays what the game draws (its
// factor times its camera), so the world keeps its scale on each axis at any scale, wherever no
// gain limit is hit.
TEST_CASE("FrameXr immersive scale: the world-scale invariant over a settings sweep",
          "[frame_xr][kiu]") {
    std::mt19937 rng(20261113u);
    std::uniform_int_distribution<int> scale(-6, 6);
    std::uniform_real_distribution<float> fov(30.0f, 90.0f), live(30.0f, 90.0f),
        half(35.0f, 60.0f), size(0.6f, 1.6f), wider(kFactorMin, kFactorMax),
        aspect(1.3f, 2.4f);
    int checked = 0;
    for (int i = 0; i < 4000; ++i) {
        Options o;
        const int sx = scale(rng) * kScaleStepPercent;
        const int sy = scale(rng) * kScaleStepPercent;
        o.squeeze = SqueezeForScale(sx);
        o.squeeze_y = SqueezeForScale(sy);
        o.wider = wider(rng);
        o.live_fov_y_deg = live(rng);
        Placement p;
        p.size = size(rng);
        const float a = aspect(rng);
        const float game_fov = fov(rng);
        const ViewFov view = Symmetric(half(rng));
        const HeadLocked s = SolveHeadLocked(p, game_fov, a, view, 0.0f, o);
        const float t_live = std::tan(o.live_fov_y_deg * kDegToRad / 2.0f);
        const bool free_x = s.gain_x > kMinGain && s.gain_x < kMaxGain;
        const bool free_y = s.gain > kMinGain && s.gain < kMaxGain;
        if (free_x) {
            REQUIRE(s.gain_x * s.aspect * s.tan_half_y ==
                    Approx(s.factor * a * t_live).epsilon(1e-3));
        }
        if (free_y) {
            REQUIRE(s.gain * s.tan_half_y == Approx(s.factor_y * t_live).epsilon(1e-3));
        }
        // The quad's own size follows the scale exactly, clamped gains or not.
        Options o0 = o;
        o0.squeeze = 0.0f;
        o0.squeeze_y = 0.0f;
        const HeadLocked z = SolveHeadLocked(p, game_fov, a, view, 0.0f, o0);
        REQUIRE(s.width() == Approx(z.width() * (1.0f + sx / 100.0f)).epsilon(1e-4));
        REQUIRE(s.height() == Approx(z.height() * (1.0f + sy / 100.0f)).epsilon(1e-4));
        checked += free_x && free_y;
    }
    REQUIRE(checked > 2000);
}
