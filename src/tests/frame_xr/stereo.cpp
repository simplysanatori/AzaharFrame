// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <random>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/immersive_layout.h"
#include "video_core/vr_view_matrix.h"

// AzaharFrame stereo: the game's own 3D, read from its two eyes' projections, on the Full
// modes' screen (video_core/vr_view_matrix.h FarAcrossFromRows, StereoFrame;
// frame_xr/immersive_layout.h StereoFarOnQuad).
using Catch::Approx;
using namespace FrameXr::Immersive::Layout;
namespace VI = VideoCore::Immersive;

namespace {

// A top screen camera 55 degrees high, 5:3, its far point `shear` across. `turned`: the 3DS's
// own layout, the horizontal scale in the second row.
void Camera(float r[4][4], float shear, bool turned) {
    const float sy = 1.0f / std::tan(27.5f * kDegToRad);
    const float sx = sy / (5.0f / 3.0f);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r[i][j] = 0.0f;
        }
    }
    float* h = turned ? r[1] : r[0];
    float* v = turned ? r[0] : r[1];
    h[0] = sx;
    h[2] = shear;
    v[1] = sy;
    r[2][2] = -1.0f;
    r[2][3] = -0.2f;
    r[3][2] = -1.0f;
}

HeadLocked Screen(float factor, float tan_half_y, float aspect, float distance) {
    HeadLocked s;
    s.factor = factor;
    s.factor_y = factor;
    s.tan_half_y = tan_half_y;
    s.aspect = aspect;
    s.distance = distance;
    return s;
}

} // namespace

TEST_CASE("Stereo: a camera's far point across", "[frame_xr][stereo]") {
    float r[4][4];
    for (const bool turned : {false, true}) {
        float ndc = 1.0f;
        Camera(r, 0.0f, turned);
        REQUIRE(VI::FarAcrossFromRows(r, ndc));
        CHECK(ndc == Approx(0.0f).margin(1e-7));
        // A shear moves the point at infinity (z -> -inf: x/w = shear / -w's z).
        Camera(r, 0.0335f, turned);
        REQUIRE(VI::FarAcrossFromRows(r, ndc));
        CHECK(ndc == Approx(-0.0335f));
        Camera(r, -0.0335f, turned);
        REQUIRE(VI::FarAcrossFromRows(r, ndc));
        CHECK(ndc == Approx(0.0335f));
    }
    // Not a top screen camera: orthographic (a 2D layout), or a narrow shadow camera.
    Camera(r, 0.01f, false);
    r[3][2] = 0.0f;
    r[3][3] = 1.0f;
    float ndc = 0.0f;
    CHECK_FALSE(VI::FarAcrossFromRows(r, ndc));
    Camera(r, 0.01f, false);
    r[0][0] = r[1][1]; // 1:1, narrower than any top screen
    CHECK_FALSE(VI::FarAcrossFromRows(r, ndc));
}

TEST_CASE("Stereo: the game's eyes in one frame", "[frame_xr][stereo]") {
    VI::StereoFrame f;
    SECTION("two eyes, as the game's 3D draws them") {
        for (int i = 0; i < 42; ++i) {
            f.Draw(-0.0335f + 1e-5f * static_cast<float>(i % 3));
            f.Draw(0.0335f);
        }
        const auto r = f.End();
        CHECK(r.draws == 84);
        CHECK(r.eyes == 2);
        CHECK(r.far_ndc == Approx(0.067f).margin(1e-4));
    }
    SECTION("drawn once (no 3D, or a paused world behind a menu)") {
        for (int i = 0; i < 42; ++i) {
            f.Draw(0.0f);
        }
        const auto r = f.End();
        CHECK(r.eyes == 1);
        CHECK(r.far_ndc == 0.0f);
    }
    SECTION("a few draws of another camera do not make a second eye") {
        for (int i = 0; i < 40; ++i) {
            f.Draw(0.0f);
        }
        for (int i = 0; i < 4; ++i) {
            f.Draw(0.2f);
        }
        CHECK(f.End().eyes == 1);
    }
    SECTION("but they do not hide the two eyes either") {
        for (int i = 0; i < 40; ++i) {
            f.Draw(-0.01f);
            f.Draw(0.01f);
        }
        for (int i = 0; i < 4; ++i) {
            f.Draw(0.3f);
        }
        const auto r = f.End();
        CHECK(r.eyes == 2);
        CHECK(r.far_ndc == Approx(0.02f));
    }
    SECTION("no world camera") {
        CHECK(f.End().eyes == 0);
    }
    SECTION("each frame starts afresh") {
        f.Draw(-0.02f);
        f.Draw(0.02f);
        CHECK(f.End().eyes == 2);
        f.Draw(0.0f);
        CHECK(f.End().eyes == 1);
        CHECK(f.End().eyes == 0);
    }
}

TEST_CASE("Stereo: Ocarina of Time 3D's far scenery on the owner's screen", "[frame_xr][stereo]") {
    // Mock dump: 3D Depth 30, far scenery 67 of 2000 pixels apart; the screen 105 x 82
    // degrees at 3 m, factor 1, IPD 69.9 mm: 5.0 degrees apart, the eyes 1.33 at most.
    const float tan_half_y = std::tan(41.0f * kDegToRad);
    const float aspect = std::tan(52.5f * kDegToRad) / tan_half_y;
    const HeadLocked s = Screen(1.0f, tan_half_y, aspect, 3.0f);
    const StereoFar st = StereoFarOnQuad(s, 2, 0.0699f / 2.0f, 67.0f / 2000.0f);
    CHECK(st.far_deg == Approx(5.0f).margin(0.02f));
    CHECK(st.allow_deg == Approx(1.335f).margin(0.01f));
    CHECK(st.vergence_deg == Approx(-3.67f).margin(0.02f));
    CHECK(st.diverges);
    // 3D Depth x natural_scale: 30 % gives the far scenery at infinity at 8 %.
    CHECK(30.0f * st.natural_scale == Approx(8.0f).margin(0.05f));
    // At 3D Depth 10 (22.3 pixels) still 0.33 degrees past parallel.
    const StereoFar st10 = StereoFarOnQuad(s, 2, 0.0699f / 2.0f, 22.3f / 2000.0f);
    CHECK(st10.vergence_deg == Approx(-0.33f).margin(0.02f));
    // No 3D: all of it on the screen, 3 m away.
    const StereoFar flat = StereoFarOnQuad(s, 2, 0.0699f / 2.0f, 0.0f);
    CHECK(flat.far_deg == 0.0f);
    CHECK(flat.vergence_deg == Approx(flat.allow_deg));
    CHECK_FALSE(flat.diverges);
    CHECK(flat.natural_scale == 0.0f);
}

TEST_CASE("Stereo: the far scenery on the screen, any settings", "[frame_xr][stereo]") {
    std::mt19937 rng(20261113);
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    for (int run = 0; run < 5000; ++run) {
        const uint32_t mode = run % 2 ? 3u : 2u;
        const HeadLocked s =
            Screen(uni(1.0f, 1.6f), uni(0.2f, 1.4f), uni(1.2f, 2.4f), uni(0.4f, 5.0f));
        const float h = uni(0.0225f, 0.0425f);
        const float w = uni(0.0f, 0.12f);
        const StereoFar st = StereoFarOnQuad(s, mode, h, w);
        INFO("run " << run << " mode " << mode << " factor " << s.factor << " tan " << s.tan_half_y
                    << " aspect " << s.aspect << " d " << s.distance << " h " << h << " w " << w);
        // On the quad: the game's share of its own picture, 1/factor of the quad's width.
        const float game_m = w / s.factor * s.width();
        CHECK(st.separation_m == Approx(game_m + (mode == 3 ? 2.0f * h : 0.0f)).margin(1e-6));
        // Divergence exactly when the far points are farther apart than the eyes.
        CHECK(st.diverges == (st.vergence_deg < -kDivergeToleranceDeg));
        CHECK((st.vergence_deg < 0.0f) == (st.separation_m > 2.0f * h));
        CHECK((st.far_deg > st.allow_deg) == (st.separation_m > 2.0f * h));
        // More of the game's 3D only turns the eyes outwards.
        const StereoFar more = StereoFarOnQuad(s, mode, h, w + 0.01f);
        CHECK(more.vergence_deg < st.vergence_deg);
        if (mode == 2) {
            // The game's 3D times natural_scale puts the far scenery at infinity.
            if (game_m > 1e-4f) {
                const StereoFar nat = StereoFarOnQuad(s, mode, h, w * st.natural_scale);
                CHECK(nat.vergence_deg == Approx(0.0f).margin(1e-3));
                CHECK(nat.far_deg == Approx(nat.allow_deg).margin(1e-3));
            }
            // Without it the screen's own distance: the eyes turned in by the most.
            const StereoFar flat = StereoFarOnQuad(s, mode, h, 0.0f);
            CHECK(flat.vergence_deg == Approx(flat.allow_deg).margin(1e-4));
        } else {
            // Mode 3: each eye's quad on its eye; with no game 3D, parallel at infinity.
            const StereoFar off = StereoFarOnQuad(s, mode, h, 0.0f);
            CHECK(off.vergence_deg == Approx(0.0f).margin(1e-4));
            CHECK_FALSE(off.diverges);
            CHECK(st.natural_scale == Approx(0.0f).margin(1e-6));
        }
        // A wider view (bigger factor) shows the same picture smaller: less apart.
        HeadLocked wider = s;
        wider.factor = s.factor * 1.2f;
        CHECK(StereoFarOnQuad(wider, mode, h, w).separation_m <= st.separation_m + 1e-7f);
    }
}
