// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <random>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/frame_vr_depth.h"
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

// ---- The stereo cap (mode 2): the slider the game is shown so its far scenery never diverges.

namespace {

// A game whose own 3D is linear in the slider (as measured: Ocarina of Time 3D 0.117 of its width
// per slider unit): the far width it draws at slider `slider`.
float GameFarWidth(float per, float slider) {
    return per * slider;
}

} // namespace

TEST_CASE("Stereo cap: Ocarina of Time 3D on the owner's screen", "[frame_xr][stereo]") {
    const float tan_half_y = std::tan(41.0f * kDegToRad);
    const float aspect = std::tan(52.5f * kDegToRad) / tan_half_y;
    const HeadLocked s = Screen(1.0f, tan_half_y, aspect, 3.0f);
    const float h = 0.0699f / 2.0f;
    const float per = 67.0f / 2000.0f / 0.30f; // 3D Depth 30: 67 of 2000 pixels
    const float limit = StereoSliderLimit(s, h, per);
    // 8 % puts it at parallel (the natural scale above); the 0.05 degree margin is 2.6 mm of
    // the 69.9 at 3 m: 7.7 %.
    CHECK(limit == Approx(0.0771f).margin(0.0005f));
    const StereoFar at = StereoFarOnQuad(s, 2, h, GameFarWidth(per, limit));
    CHECK(at.vergence_deg == Approx(kStereoMarginDeg).margin(1e-3));
    CHECK_FALSE(at.diverges);
    // Not known yet: no limit.
    CHECK(StereoSliderLimit(s, h, -1.0f) < 0.0f);
    CHECK(StereoSliderLimit(s, h, 0.0f) < 0.0f);
}

TEST_CASE("Stereo cap: the limit, any settings", "[frame_xr][stereo]") {
    std::mt19937 rng(20261114);
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    for (int run = 0; run < 5000; ++run) {
        const HeadLocked s =
            Screen(uni(1.0f, 1.6f), uni(0.2f, 1.4f), uni(1.2f, 2.4f), uni(0.4f, 5.0f));
        const float h = uni(0.0225f, 0.0425f);
        const float per = uni(0.005f, 0.4f);
        INFO("run " << run << " factor " << s.factor << " tan " << s.tan_half_y << " aspect "
                    << s.aspect << " d " << s.distance << " h " << h << " per " << per);
        const float limit = StereoSliderLimit(s, h, per);
        REQUIRE(limit >= 0.0f);
        // At the limit the eyes are the margin inwards of parallel: never diverging.
        const StereoFar at = StereoFarOnQuad(s, 2, h, GameFarWidth(per, limit));
        CHECK(at.vergence_deg == Approx(kStereoMarginDeg).margin(2e-3));
        CHECK_FALSE(at.diverges);
        CHECK(at.vergence_deg >= 0.0f);
        // Any slider under it is nearer still; any above it, past the margin.
        const float under = limit * uni(0.0f, 1.0f);
        CHECK(StereoFarOnQuad(s, 2, h, GameFarWidth(per, under)).vergence_deg >=
              at.vergence_deg - 1e-4f);
        CHECK(StereoFarOnQuad(s, 2, h, GameFarWidth(per, limit * 1.05f + 1e-3f)).vergence_deg <
              kStereoMarginDeg);
        // Live terms: a wider margin (render factor) or a deeper game changes it as it should.
        CHECK(StereoSliderLimit(s, h, 2.0f * per) == Approx(limit / 2.0f).epsilon(1e-4));
        HeadLocked nearer = s;
        nearer.distance = s.distance * 0.5f; // the same angles nearer: smaller in metres
        CHECK(StereoSliderLimit(nearer, h, per) >= limit - 1e-5f);
    }
}

TEST_CASE("Stereo cap: the slider sent, any settings", "[frame_xr][stereo]") {
    std::mt19937 rng(20261115);
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    for (int run = 0; run < 5000; ++run) {
        const HeadLocked s =
            Screen(uni(1.0f, 1.6f), uni(0.2f, 1.4f), uni(1.2f, 2.4f), uni(0.4f, 5.0f));
        const float h = uni(0.0225f, 0.0425f);
        const float per = uni(0.005f, 0.4f);
        const uint32_t depth = static_cast<uint32_t>(uni(1.0f, 100.99f));
        const float user = static_cast<float>(depth) / 100.0f;
        const int multiplier = static_cast<int>(uni(0.0f, 3.99f));
        INFO("run " << run << " factor " << s.factor << " tan " << s.tan_half_y << " aspect "
                    << s.aspect << " d " << s.distance << " h " << h << " per " << per
                    << " depth " << depth);
        StereoCap cap;
        int64_t t = 1000;
        // Not measured yet: the user's slider, so the game's 3D can be measured.
        CHECK(cap.Update(user, -1.0f, t) == user);
        CHECK_FALSE(cap.Limited());
        // Each frame (90 a second) the limit from a measure with a little noise.
        float sent = user, last = -1.0f;
        float biggest_step = 0.0f;
        for (int frame = 0; frame < 180; ++frame) {
            t += 11;
            const float noisy = per * (1.0f + uni(-0.002f, 0.002f));
            const float limit = StereoSliderLimit(s, h, noisy);
            sent = cap.Update(user, limit, t);
            if (last >= 0.0f) {
                biggest_step = std::max(biggest_step, std::abs(sent - last));
            }
            last = sent;
            // Never diverging, from the first known limit on.
            CHECK_FALSE(StereoFarOnQuad(s, 2, h, GameFarWidth(per, sent)).diverges);
            CHECK(sent <= user);
            // The Full modes' head motion: the user's 3D Depth, whatever is sent.
            CHECK(GamePositionScale(multiplier, depth) ==
                  std::pow(10.0f, static_cast<float>(multiplier)) *
                      static_cast<float>(depth / 10));
        }
        const float limit = StereoSliderLimit(s, h, per);
        if (user <= limit * (1.0f - 0.003f)) {
            // Under the limit: exactly the user's slider, never limited.
            CHECK(sent == user);
            CHECK_FALSE(cap.Limited());
        } else if (user > limit * (1.0f + 0.003f) + StereoCap::kStep) {
            // Over it: the limit (within the noise and a step), and said so.
            CHECK(cap.Limited());
            CHECK(sent == Approx(limit).margin(limit * 0.003f + StereoCap::kStep));
        }
        // No jitter: after the first limit, the measure's noise (0.4 % from end to end) moves the
        // slider by at most that, downwards only (a higher limit needs a whole step).
        CHECK(biggest_step <= 0.0041f * limit + 1e-6f);
        // The shared page and HID make the slider the game sees the same way.
        CHECK(Settings::FrameVrSlider(user, 1.0f, -1.0f) == user);
        CHECK(Settings::FrameVrSlider(user, 1.0f, sent) == Approx(sent));
        CHECK(Settings::FrameVrSlider(user, 1.0f, user + 0.1f) == user);
    }
}

TEST_CASE("Stereo cap: smooth, and the user's own changes at once", "[frame_xr][stereo]") {
    StereoCap cap;
    int64_t t = 1000;
    // The first limit at once.
    CHECK(cap.Update(0.30f, 0.08f, t) == Approx(0.08f));
    CHECK(cap.Limited());
    CHECK(cap.Target() == Approx(0.08f));
    // A higher limit by less than a step: nothing changes.
    t += 11;
    CHECK(cap.Update(0.30f, 0.084f, t) == Approx(0.08f));
    // By more: the slider climbs to it at kUpPerS, not in one frame.
    t += 11;
    const float up = cap.Update(0.30f, 0.12f, t);
    CHECK(up == Approx(0.08f + StereoCap::kUpPerS * 0.011f).margin(1e-5));
    for (int i = 0; i < 40; ++i) {
        t += 11;
        cap.Update(0.30f, 0.12f, t);
    }
    CHECK(cap.Update(0.30f, 0.12f, t + 11) == Approx(0.12f));
    t += 11;
    // A lower limit: down at kDownPerS (a quarter of a second for the whole slider at most).
    t += 11;
    const float down = cap.Update(0.30f, 0.06f, t);
    CHECK(down == Approx(0.12f - StereoCap::kDownPerS * 0.011f).margin(1e-5));
    for (int i = 0; i < 2; ++i) {
        t += 11;
        cap.Update(0.30f, 0.06f, t);
    }
    t += 11;
    CHECK(cap.Update(0.30f, 0.06f, t) == Approx(0.06f));
    // The user lowers 3D Depth under the limit: through at once, and no longer limited.
    t += 11;
    CHECK(cap.Update(0.04f, 0.06f, t) == 0.04f);
    CHECK_FALSE(cap.Limited());
    // And raises it again, under the limit: at once too.
    t += 11;
    CHECK(cap.Update(0.05f, 0.06f, t) == 0.05f);
    // Over it: the limit at once.
    t += 11;
    CHECK(cap.Update(0.50f, 0.06f, t) == Approx(0.06f));
    CHECK(cap.Limited());
    // Not measured for a while (a menu, a load): the last limit holds.
    t += 11;
    CHECK(cap.Update(0.50f, -1.0f, t) == Approx(0.06f));
    // Reset (a new game, another mode): the user's slider.
    cap.Reset();
    CHECK(cap.Update(0.50f, -1.0f, t) == 0.50f);
    CHECK(cap.Target() < 0.0f);
}

TEST_CASE("Stereo cap: the game's 3D per slider unit, measured", "[frame_xr][stereo]") {
    VI::FarPerSlider m;
    CHECK(m.Value() < 0.0f);
    // A change of the slider is not measured until it held kStableSwaps swaps.
    for (int i = 0; i < VI::FarPerSlider::kStableSwaps; ++i) {
        m.Swap(2, 0.117f * 0.3f, 0.3f);
    }
    CHECK(m.Value() < 0.0f);
    m.Swap(2, 0.117f * 0.3f, 0.3f);
    CHECK(m.Value() == Approx(0.117f));
    // Another slider, the same game: the same per unit (it is linear).
    for (int i = 0; i < 20; ++i) {
        m.Swap(2, 0.117f * 0.08f, 0.08f);
    }
    CHECK(m.Value() == Approx(0.117f));
    // The frame just after a change (the game read the old slider) does not count.
    m.Swap(2, 0.117f * 0.08f, 0.5f);
    CHECK(m.Value() == Approx(0.117f));
    // One eye, no 3D or no far point: not measured.
    for (int i = 0; i < 20; ++i) {
        m.Swap(1, 0.0f, 0.5f);
        m.Swap(2, 0.0f, 0.5f);
    }
    CHECK(m.Value() == Approx(0.117f));
    // A few odd frames are out-voted (the median).
    for (int i = 0; i < 3; ++i) {
        m.Swap(2, 0.3f, 0.5f);
    }
    CHECK(m.Value() == Approx(0.117f));
    m.Reset();
    CHECK(m.Value() < 0.0f);
}

TEST_CASE("Stereo cap: the slider shown", "[frame_xr][stereo]") {
    for (int d = 0; d <= 100; ++d) {
        const float depth = static_cast<float>(d) / 100.0f;
        CHECK(Settings::FrameVrSlider(depth, 1.0f, -1.0f) == depth);
        CHECK(Settings::FrameVrSlider(depth, 0.5f, -1.0f) == depth * 0.5f);
        CHECK(Settings::FrameVrSlider(depth, 1.0f, 0.08f) == std::min(depth, 0.08f));
        CHECK(Settings::FrameVrSlider(depth, 1.0f, 0.0f) == 0.0f);
        // Head motion: CitraVR's 10^m x (3D Depth / 10, an integer).
        for (int m = 0; m < 4; ++m) {
            CHECK(GamePositionScale(m, static_cast<uint32_t>(d)) ==
                  std::pow(10.0f, static_cast<float>(m)) * static_cast<float>(d / 10));
        }
    }
}

// ---- The menu rule (mode 2): a screen of the game's own at its own size.

TEST_CASE("Menu rule: the game's world drawn once after two eyes", "[frame_xr][stereo]") {
    using W = VI::WorldOnceRun;
    SECTION("gameplay: two eyes every frame, standing still too") {
        W w;
        for (int i = 0; i < 600; ++i) {
            CHECK_FALSE(w.Swap(2, 0.3f));
        }
    }
    SECTION("Ocarina of Time 3D's item screen: after kSwaps in a row, until two eyes again") {
        W w;
        for (int i = 0; i < 60; ++i) {
            w.Swap(2, 0.3f);
        }
        for (int i = 0; i < W::kSwaps - 1; ++i) {
            CHECK_FALSE(w.Swap(1, 0.3f));
        }
        CHECK(w.Swap(1, 0.3f));
        for (int i = 0; i < 100; ++i) {
            CHECK(w.Swap(i % 7 ? 1 : 0, 0.3f)); // the world once, a swap with none
        }
        CHECK_FALSE(w.Swap(2, 0.3f));
        CHECK_FALSE(w.On());
    }
    SECTION("never two eyes before (Mario Kart 7's menus, a title screen in 2D)") {
        W w;
        for (int i = 0; i < 300; ++i) {
            CHECK_FALSE(w.Swap(i % 3 ? 1 : 0, 0.3f));
        }
    }
    SECTION("the world on every other swap (Super Mario 3D Land's title)") {
        W w;
        for (int i = 0; i < 300; ++i) {
            CHECK_FALSE(w.Swap(i % 2 ? 2 : 0, 0.3f));
        }
    }
    SECTION("3D off: one eye is all there is") {
        W w;
        for (int i = 0; i < 60; ++i) {
            w.Swap(2, 0.3f);
        }
        for (int i = 0; i < 60; ++i) {
            CHECK_FALSE(w.Swap(1, 0.0f));
        }
    }
    SECTION("a dropped eye or two in gameplay is not a menu") {
        W w;
        for (int i = 0; i < 600; ++i) {
            CHECK_FALSE(w.Swap(i % 50 < W::kSwaps - 1 ? 1 : 2, 0.3f));
        }
    }
}

TEST_CASE("Menu rule: eased in and out over a quarter of a second", "[frame_xr][stereo]") {
    MenuExact m;
    int64_t t = 1000;
    CHECK(m.Update(false, t) == 0.0f);
    float last = 0.0f;
    int frames_in = 0;
    while (m.Weight() < 1.0f && frames_in < 1000) {
        t += 8;
        const float w = m.Update(true, t);
        CHECK(w >= last);
        CHECK(w - last <= 1.5f * 8.0f / MenuExact::kEaseMs + 1e-5f); // smooth: no jump
        last = w;
        ++frames_in;
    }
    CHECK(frames_in * 8 == Approx(MenuExact::kEaseMs).margin(8.0));
    // Exactly the game's frame at the end, whatever Wider View is.
    for (const float wider : {1.0f, 1.1f, 1.2f, 1.3f, 1.4f}) {
        CHECK(MenuExact::Factor(wider, m.Weight()) == 1.0f);
        CHECK(MenuExact::Factor(wider, 0.0f) == wider);
    }
    int frames_out = 0;
    while (m.Weight() > 0.0f && frames_out < 1000) {
        t += 8;
        const float w = m.Update(false, t);
        CHECK(w <= last);
        last = w;
        ++frames_out;
    }
    CHECK(frames_out * 8 == Approx(MenuExact::kEaseMs).margin(8.0));
    // A long gap (a pause) is not a jump either.
    t += 10000;
    CHECK(m.Update(true, t) <= 1.5f * 100.0f / MenuExact::kEaseMs);
}

TEST_CASE("Menu rule: the screen at factor 1 is the game's frame, any settings",
          "[frame_xr][stereo]") {
    std::mt19937 rng(20261116);
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    for (int run = 0; run < 2000; ++run) {
        Options o;
        const uint32_t level = static_cast<uint32_t>(uni(0.0f, 4.99f));
        o.wider = MenuExact::Factor(WiderFactor(level), 1.0f);
        o.squeeze = uni(0.0f, 0.3f);
        o.squeeze_y = uni(0.0f, 0.3f);
        Placement p;
        p.size = uni(0.6f, 1.6f);
        const float fov = uni(40.0f, 70.0f);
        const float aspect = uni(1.3f, 2.0f);
        ViewFov view;
        view.left = -0.9f;
        view.right = 0.9f;
        view.down = -0.95f;
        view.up = 0.8f;
        view.outer_left = -0.95f;
        view.outer_right = 0.75f;
        view.outer_down = -0.95f;
        view.outer_up = 0.8f;
        const HeadLocked s = SolveHeadLocked(p, fov, aspect, view, 0.0f, o, {});
        INFO("run " << run << " level " << level << " fov " << fov << " aspect " << aspect);
        // No margin either way: the game draws exactly its own frame.
        CHECK(s.factor == 1.0f);
        CHECK(s.factor_y == Approx(1.0f).epsilon(1e-5));
    }
}
