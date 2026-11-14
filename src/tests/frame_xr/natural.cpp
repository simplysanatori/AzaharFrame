// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/frame_vr_depth.h"
#include "frame_xr/immersive_layout.h"
#include "video_core/vr_view_matrix.h"

// AzaharFrame natural: 3D Depth as a share of the natural 3D, leaning from the game's own 3D,
// and mode 3's stereo with mode 2's geometry (frame_xr/immersive_layout.h "natural",
// video_core/vr_view_matrix.h StereoFrame::EyeApart, EyePerFar). The geometry is checked by
// tracing each eye's ray through where the game's picture puts a point on the screen and
// intersecting the two: what the player sees, with no small-angle approximation.
using Catch::Approx;
using namespace FrameXr::Immersive::Layout;
namespace VI = VideoCore::Immersive;

namespace {

HeadLocked Screen(float factor, float tan_half_y, float aspect, float distance) {
    HeadLocked s;
    s.factor = factor;
    s.factor_y = factor;
    s.tan_half_y = tan_half_y;
    s.aspect = aspect;
    s.distance = distance;
    return s;
}

// A game's own 3D, as the 3DS SDK's stereo camera makes it: at slider 1 its eyes are `e_full`
// apart, each projection sheared so depth `zero` (its zero-parallax distance) has no disparity;
// `p00` its projection's scale across.
struct Game {
    double p00, zero, e_full;
    // The far scenery's share of the picture's width per slider unit (FarPerSlider).
    float FarPerSlider() const {
        return static_cast<float>(p00 * e_full / (2.0 * zero));
    }
    // The eyes apart per far width (EyePerFar): the same at any slider.
    float EyePerFar() const {
        return static_cast<float>(2.0 * zero / p00);
    }
};

struct Seen {
    double x, z; // in front of the player: z < 0
    bool parallel;
};

// Where the player sees the point (x, z) of the game's camera space (z > 0 ahead): mode 2, the
// game's own two eyes at `slider` on one quad at the screen's distance.
Seen SeenMode2(const HeadLocked& s, double h, const Game& g, double slider, double x, double z) {
    const double e = g.e_full * slider;
    const double w = static_cast<double>(s.width()) / s.factor; // the picture on the quad
    double u[2], ex[2];
    for (int i = 0; i < 2; ++i) {
        const double xi = i == 0 ? -e / 2.0 : e / 2.0; // the game's eye
        const double ndc = g.p00 * (x - xi) / z + g.p00 * xi / g.zero;
        u[i] = ndc * w / 2.0;
        ex[i] = i == 0 ? -h : h;
    }
    // Rays (ex, 0) -> (u, -d): ex + t (u - ex) equal for both eyes.
    const double d = s.distance;
    const double den = (u[1] - ex[1]) - (u[0] - ex[0]);
    if (std::abs(den) < 1e-12) {
        return {0.0, -1e30, true};
    }
    const double t = (ex[0] - ex[1]) / den;
    return {ex[0] + t * (u[0] - ex[0]), -t * d, false};
}

// Mode 3: our own eyes `scaler x h` to each side (parallel game cameras, the game's slider 0),
// each eye's quad `quad_half` from the middle.
Seen SeenMode3(const HeadLocked& s, double h, const Game& g, const Mode3Stereo& m, double x,
               double z) {
    const double w = static_cast<double>(s.width()) / s.factor;
    double u[2], ex[2];
    for (int i = 0; i < 2; ++i) {
        const double side = i == 0 ? -1.0 : 1.0;
        const double xi = side * m.eye_scaler * h;
        const double ndc = g.p00 * (x - xi) / z;
        u[i] = side * m.quad_half + ndc * w / 2.0;
        ex[i] = side * h;
    }
    const double d = s.distance;
    const double den = (u[1] - ex[1]) - (u[0] - ex[0]);
    if (std::abs(den) < 1e-12) {
        return {0.0, -1e30, true};
    }
    const double t = (ex[0] - ex[1]) / den;
    return {ex[0] + t * (u[0] - ex[0]), -t * d, false};
}

// The eyes' angle inwards at a point straight ahead at infinity (degrees; 0 parallel).
double FarVergenceMode2(const HeadLocked& s, double h, const Game& g, double slider) {
    const double far_m = g.FarPerSlider() * slider * s.width() / s.factor;
    return 2.0 * std::atan((h - far_m / 2.0) / s.distance) * 180.0 / 3.14159265358979;
}

void Matrix(float m[3][4], float tx, float ty, float tz, float yaw = 0.0f) {
    const float c = std::cos(yaw), sn = std::sin(yaw);
    const float r[3][3] = {{c, 0.0f, -sn}, {0.0f, 1.0f, 0.0f}, {sn, 0.0f, c}};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m[i][j] = r[i][j];
        }
    }
    m[0][3] = tx;
    m[1][3] = ty;
    m[2][3] = tz;
}

} // namespace

TEST_CASE("Natural: the game's eyes apart from its two cameras", "[frame_xr][natural]") {
    float a[3][4], b[3][4];
    // Ocarina of Time 3D's eye matrix (register 4, measured): the identity, one eye moved.
    Matrix(a, 0.0f, 0.0f, 0.0f);
    Matrix(b, -0.6501f, 0.0f, 0.0f);
    CHECK(VI::StereoFrame::EyeApart(a, b) == Approx(0.6501f));
    // A world view (mode 3), turned and far from the origin: only the eyes' x differs.
    Matrix(a, 1203.5f + 1.7f, -40.25f, 5321.0f, 0.7f);
    Matrix(b, 1203.5f - 1.7f, -40.25f, 5321.0f, 0.7f);
    CHECK(VI::StereoFrame::EyeApart(a, b) == Approx(3.4f).epsilon(2e-3));
    // The same matrix: no 3D.
    CHECK(VI::StereoFrame::EyeApart(a, a) == 0.0f);
    // Two different cameras (turned apart), or eyes along the view: not an eye pair.
    Matrix(b, 1203.5f - 1.7f, -40.25f, 5321.0f, 0.71f);
    CHECK(VI::StereoFrame::EyeApart(a, b) < 0.0f);
    Matrix(a, 0.0f, 0.0f, 0.0f);
    Matrix(b, 0.1f, 0.0f, 0.5f);
    CHECK(VI::StereoFrame::EyeApart(a, b) < 0.0f);
}

TEST_CASE("Natural: a frame's two eyes, read from its draws", "[frame_xr][natural]") {
    VI::StereoFrame f;
    float left[3][4], right[3][4], other[3][4];
    Matrix(left, 0.0f, 0.0f, 0.0f);
    Matrix(right, -0.65f, 0.0f, 0.0f);
    Matrix(other, 3.0f, 2.0f, 1.0f, 0.3f); // a later draw of the eye with something else
    for (int i = 0; i < 30; ++i) {
        f.Draw(0.0f, std::nanf(""), 0.0f, i == 0 ? left : other, 1.15f);
    }
    for (int i = 0; i < 30; ++i) {
        f.Draw(-0.0149f, std::nanf(""), 0.0f, i == 0 ? right : other, 1.15f);
    }
    const VI::StereoFrame::Result r = f.End();
    REQUIRE(r.eyes == 2);
    CHECK(r.far_ndc == Approx(0.0149f));
    CHECK(r.eye_apart == Approx(0.65f));
    CHECK(r.p00 == Approx(1.15f));
    // Without matrices (not read): no eyes apart.
    VI::StereoFrame g;
    g.Draw(0.0f);
    g.Draw(-0.0149f);
    CHECK(g.End().eye_apart < 0.0f);
    // One eye only: none.
    VI::StereoFrame once;
    once.Draw(0.0f, std::nanf(""), 0.0f, left);
    CHECK(once.End().eye_apart < 0.0f);
}

TEST_CASE("Natural: eyes per far width, the same at any slider", "[frame_xr][natural]") {
    const Game g{1.15, 49.6, 10.0};
    VI::EyePerFar m;
    CHECK(m.Value() < 0.0f);
    for (const double slider : {0.01, 0.065, 0.3, 1.0}) {
        const float far_ndc = 2.0f * g.FarPerSlider() * static_cast<float>(slider);
        m.Swap(2, far_ndc, static_cast<float>(g.e_full * slider), static_cast<float>(g.p00));
        CHECK(m.Value() == Approx(g.EyePerFar()).epsilon(1e-4));
        CHECK(m.ZeroParallax() == Approx(g.zero).epsilon(1e-4));
    }
    // Frames without both eyes, or no far disparity to divide by, change nothing.
    const float before = m.Value();
    m.Swap(1, 0.01f, 1.0f, 1.0f);
    m.Swap(2, 0.0f, 1.0f, 1.0f);
    m.Swap(2, 0.01f, -1.0f, 1.0f);
    CHECK(m.Value() == before);
    CHECK(m.Count() == 4);
}

TEST_CASE("Natural: 3D Depth percent and its default", "[frame_xr][natural]") {
    // The new setting's own value or default.
    CHECK(NaturalDepthPercent(false, 100, false, 90) == 100);
    CHECK(NaturalDepthPercent(true, 40, true, 0) == 40);
    CHECK(NaturalDepthPercent(true, 140, false, 0) == 100);
    // A game the player made flat (its own 3DS 3D Depth 0) stays flat until set.
    CHECK(NaturalDepthPercent(false, 100, true, 0) == 0);
    // Every other saved value: the natural 3D (what the cap showed in 1.2.0 for 30 and 50).
    for (const uint32_t old : {1u, 10u, 30u, 50u, 90u, 100u}) {
        CHECK(NaturalDepthPercent(false, 100, true, old) == 100);
    }
}

TEST_CASE("Natural: the fallback scale", "[frame_xr][natural]") {
    // 1.2.0's scale: 10^multiplier x (3D Depth / 10, an integer).
    CHECK(GamePositionScale(1, 30) == Approx(30.0f));
    CHECK(GamePositionScale(1, 50) == Approx(50.0f));
    CHECK(GamePositionScale(2, 100) == Approx(1000.0f));
    CHECK(GamePositionScale(1, 9) == 0.0f);
    const HeadLocked s = Screen(1.4f, 0.8f, 1.6f, 3.0f);
    CHECK(NaturalLeaning(s, -1.0f) < 0.0f);
    CHECK(NaturalLeaning(s, 0.0f) < 0.0f);
    CHECK(EyePerFarFor(s, 0.0f) < 0.0f);
    // The fallback's K leans exactly by 1.2.0's scale on the screen it is made for.
    CHECK(NaturalLeaning(s, EyePerFarFor(s, 30.0f)) == Approx(30.0f));
}

TEST_CASE("Natural: Ocarina of Time 3D on the owner's screen", "[frame_xr][natural]") {
    // Measured (lane natural probe): eyes 0.65 units apart at slider 0.065, far 0.0149 NDC,
    // P00 1.15: 87 units per picture width, zero parallax 49.6 units. The screen 105 degrees
    // wide at 3 m.
    const Game g{1.15, 49.6, 10.0};
    const float tan_half_y = std::tan(41.0f * kDegToRad);
    const float aspect = std::tan(52.5f * kDegToRad) / tan_half_y;
    const float h = 0.064f / 2.0f;
    for (const float factor : {1.0f, 1.4f}) {
        const HeadLocked s = Screen(factor, tan_half_y, aspect, 3.0f);
        const float leaning = NaturalLeaning(s, g.EyePerFar());
        // 87 units across a 7.8 m picture (x factor): about 11 units per metre at Wider View
        // Off, 15.6 at 1.4x.
        CHECK(leaning == Approx(g.EyePerFar() * factor / s.width()).epsilon(1e-5));
        CHECK(leaning == Approx(11.1f * factor).epsilon(0.02));
        // The head moving one IPD moves the camera the game's eyes at the natural slider.
        const float natural = StereoSliderLimit(s, h, g.FarPerSlider(), kNaturalMarginDeg);
        CHECK(leaning * 2.0f * h == Approx(g.e_full * natural).epsilon(1e-4));
    }
}

TEST_CASE("Natural: the geometry, any settings", "[frame_xr][natural]") {
    std::mt19937 rng(20261113);
    auto uni = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    for (int run = 0; run < 4000; ++run) {
        const HeadLocked s = Screen(static_cast<float>(uni(1.0, 1.6)),
                                    static_cast<float>(uni(0.2, 1.4)),
                                    static_cast<float>(uni(1.2, 2.4)),
                                    static_cast<float>(uni(0.3, 5.0)));
        const float h = static_cast<float>(uni(0.0225, 0.0425));
        const Game g{uni(0.6, 3.0), uni(5.0, 800.0), uni(0.05, 80.0)};
        const double f = run % 5 == 0 ? 1.0 : uni(0.0, 1.0);
        INFO("run " << run << " factor " << s.factor << " tan " << s.tan_half_y << " aspect "
                    << s.aspect << " d " << s.distance << " h " << h << " p00 " << g.p00
                    << " zero " << g.zero << " e " << g.e_full << " f " << f);
        const float leaning = NaturalLeaning(s, g.EyePerFar());
        REQUIRE(leaning > 0.0f);
        const float natural = StereoSliderLimit(s, h, g.FarPerSlider(), kNaturalMarginDeg);
        REQUIRE(natural > 0.0f);
        // 1 IPD of head = the game's eyes at the natural slider, at any IPD and 3D Depth.
        CHECK(leaning * 2.0f * h == Approx(g.e_full * natural).epsilon(1e-3));
        // Mode 2 at 3D Depth f: the slider f x natural.
        const double slider = f * natural;
        // The far scenery: (1 - f) of the screen's vergence, never diverging.
        const double screen_deg = 2.0 * std::atan(h / s.distance) * 180.0 / 3.14159265358979;
        const double far_deg = FarVergenceMode2(s, h, g, slider);
        CHECK(far_deg >= -1e-3);
        CHECK(std::tan(far_deg / 2.0 * 3.14159265358979 / 180.0) ==
              Approx((1.0 - f) * h / s.distance).margin(1e-6));
        (void)screen_deg;
        // What sits at the game's screen plane is on the screen, at 1 / leaning metres a unit,
        // at any 3D Depth above 0: leaning keeps it in place.
        const double x0 = uni(-0.5, 0.5) * g.zero;
        if (f > 0.02) {
            const Seen p = SeenMode2(s, h, g, slider, x0, g.zero);
            CHECK(p.z == Approx(-s.distance).epsilon(1e-6));
            CHECK(p.x == Approx(x0 / leaning).epsilon(1e-4).margin(1e-7));
        }
        // At 100 %: a true-scale copy of the game's world at 1 / leaning metres a unit (depth
        // also x the gain, which only the screen's size changes).
        const double gain = 2.0 * s.distance * s.factor / (g.p00 * s.width());
        const double z = uni(0.2, 20.0) * g.zero;
        const double x = uni(-0.5, 0.5) * z;
        const Seen at = SeenMode2(s, h, g, natural, x, z);
        CHECK(at.x == Approx(x / leaning).epsilon(1e-3).margin(1e-6));
        CHECK(-at.z == Approx(z * gain / leaning).epsilon(1e-3));
        // Mode 3 with its own eyes at 100 %: the same point where mode 2 shows it.
        const float game_far_m = 0.0f; // the game's own 0.01 slider: tested below
        const Mode3Stereo full = Mode3Natural(1.0f, leaning, h, game_far_m);
        const Seen three = SeenMode3(s, h, g, full, x, z);
        CHECK(three.x == Approx(at.x).epsilon(1e-3).margin(1e-6));
        CHECK(three.z == Approx(at.z).epsilon(1e-3));
        // Below 100 % (the owner's decision 2): our eyes f x as far apart, the quads still the
        // IPD apart, so infinity stays parallel and the relief is f times less.
        const Mode3Stereo m3 = Mode3Natural(static_cast<float>(f), leaning, h, game_far_m);
        CHECK(m3.eye_scaler == Approx(f * leaning).margin(1e-6));
        CHECK(m3.quad_half == Approx(h));
        CHECK(StereoFarOnQuad(s, 3, h, 0.0f, m3.quad_half).vergence_deg ==
              Approx(0.0f).margin(1e-4));
        // And 3D Depth 0: no eye offset at all.
        const Mode3Stereo flat = Mode3Natural(0.0f, leaning, h, game_far_m);
        CHECK(flat.eye_scaler == 0.0f);
    }
}

TEST_CASE("Natural: World Scale's stops and names", "[frame_xr][natural]") {
    CHECK(kWorldScaleStops.front() == kWorldScaleMin);
    CHECK(kWorldScaleStops.back() == kWorldScaleMax);
    CHECK(kWorldScaleStops[static_cast<std::size_t>(WorldScaleStop(100.0f))] == 100.0f);
    CHECK(WorldScaleStop(12.0f) == 2); // small values are one stop each
    for (std::size_t i = 1; i < kWorldScaleStops.size(); ++i) {
        const float ratio = kWorldScaleStops[i] / kWorldScaleStops[i - 1];
        CHECK(ratio > 1.07f);
        CHECK(ratio < 1.17f);
        CHECK(WorldScaleStop(kWorldScaleStops[i]) == static_cast<int>(i));
    }
    CHECK(WorldScaleStop(1000.0f) == static_cast<int>(kWorldScaleStops.size()) - 1);
    CHECK(WorldScaleStop(1.0f) == 0);
    CHECK(WorldScaleText(100.0f) == "Automatic");
    CHECK(WorldScaleText(100.4f) == "Automatic");
    CHECK(WorldScaleText(12.0f) == "12%");
    CHECK(WorldScaleText(5.0f) == "10%");
    CHECK(WorldScaleText(std::nanf("")) == "Automatic");
    CHECK(FrameXr::Immersive::Names::WorldScaleShortText(100.0f) == "Auto");
    CHECK(FrameXr::Immersive::Names::WorldScaleShortText(400.0f) == "400%");
    CHECK(WorldScaleToast(45.0f) == "World Scale: 45%");
    CHECK(WorldScaleToast(100.0f) == "World Scale: Automatic");
    // The distance: 100 % is the default 3 m; Follow off divides by the screen's size.
    CHECK(FullDistance(100.0f, 1.7f, true) == Approx(kWorldRefDistance));
    CHECK(FullDistance(10.0f, 1.0f, true) == Approx(0.3f));
    CHECK(FullDistance(400.0f, 1.0f, true) == Approx(12.0f));
    CHECK(FullDistance(100.0f, 2.0f, false) == Approx(1.5f));
    CHECK(FullDistance(10.0f, 2.5f, false) == Approx(0.12f));
    CHECK(FullDistance(400.0f, 0.6f, false) == Approx(kFullMaxDistance));
    CHECK(FullDistance(400.0f, 0.5f, false) == Approx(kFullMaxDistance)); // clamped
    CHECK(WorldScaleForDistance(FullDistance(37.0f, 1.3f, false), 1.3f, false) ==
          Approx(37.0f));
}

TEST_CASE("Natural: World Scale, any settings", "[frame_xr][natural]") {
    // World Scale sets the screen's distance at the angle as set; the head and the 3D follow it.
    std::mt19937 rng(1113);
    auto uni = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    for (int run = 0; run < 3000; ++run) {
        const float size = static_cast<float>(uni(0.6, 2.5));
        const bool follow = run % 2 == 0;
        // The screen at size 1 and at this size (tan grows with the size).
        const float tan1 = static_cast<float>(uni(0.15, 0.55));
        const float aspect = static_cast<float>(uni(1.2, 2.4));
        const float factor = static_cast<float>(uni(1.0, 1.6));
        const float percent = static_cast<float>(std::exp(uni(std::log(10.0), std::log(400.0))));
        const float d = FullDistance(percent, size, follow);
        const HeadLocked s = Screen(factor, tan1 * size, aspect, d);
        const float h = static_cast<float>(uni(0.0225, 0.0425));
        const Game g{uni(0.6, 3.0), uni(5.0, 800.0), uni(0.05, 80.0)};
        const double f = run % 3 == 0 ? 1.0 : uni(0.0, 1.0);
        INFO("run " << run << " size " << size << " follow " << follow << " percent " << percent
                    << " d " << d << " f " << f);
        const float leaning = NaturalLeaning(s, g.EyePerFar());
        REQUIRE(leaning > 0.0f);
        // Matched: one IPD of head = the game's eyes at its natural slider on this screen.
        const float natural = StereoSliderLimit(s, h, g.FarPerSlider(), kNaturalMarginDeg);
        CHECK(leaning * 2.0f * h == Approx(g.e_full * natural).epsilon(1e-3));
        // The world's size (metres a unit = 1 / leaning) is World Scale x Automatic's, times
        // the size with Follow on and whatever the size with it off.
        const HeadLocked automatic =
            Screen(factor, tan1 * (follow ? size : 1.0f), aspect, kWorldRefDistance);
        const float auto_leaning = NaturalLeaning(automatic, g.EyePerFar());
        if (d > kFullMinDistance && d < kFullMaxDistance) {
            CHECK(auto_leaning / leaning == Approx(percent / 100.0f).epsilon(1e-4));
        }
        // Mode 2 never diverges at any 3D Depth; mode 3's far scenery is parallel at any.
        CHECK(FarVergenceMode2(s, h, g, f * natural) >= -1e-3);
        const Mode3Stereo m3 = Mode3Natural(static_cast<float>(f), leaning, h, 0.0f);
        CHECK_FALSE(StereoFarOnQuad(s, 3, h, 0.0f, m3.quad_half).diverges);
        // At 100 % both modes show each point at the same place, at 1 / leaning metres a unit.
        const double z = uni(0.2, 20.0) * g.zero;
        const double x = uni(-0.5, 0.5) * z;
        const Seen two = SeenMode2(s, h, g, natural, x, z);
        const Seen three = SeenMode3(s, h, g, Mode3Natural(1.0f, leaning, h, 0.0f), x, z);
        CHECK(two.x == Approx(x / leaning).epsilon(1e-3).margin(1e-6));
        CHECK(three.x == Approx(two.x).epsilon(1e-3).margin(1e-6));
        CHECK(three.z == Approx(two.z).epsilon(1e-3));
    }
}

TEST_CASE("Natural: the migration keeps 1.2.0's look and leaning", "[frame_xr][natural]") {
    std::mt19937 rng(4242);
    auto uni = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    int in_range = 0;
    for (int run = 0; run < 3000; ++run) {
        const float size = static_cast<float>(uni(0.6, 2.5));
        const bool follow = run % 2 == 0;
        const float old_distance = static_cast<float>(uni(0.3, 3.0));
        const HeadLocked saved = Screen(static_cast<float>(uni(1.0, 1.6)),
                                        static_cast<float>(uni(0.15, 0.55)) * size,
                                        static_cast<float>(uni(1.2, 2.4)), old_distance);
        const float k = static_cast<float>(std::exp(uni(std::log(2.0), std::log(2000.0))));
        const float old = GamePositionScale(static_cast<int>(run % 3),
                                            static_cast<uint32_t>(uni(10.0, 100.0)));
        INFO("run " << run << " size " << size << " follow " << follow << " K " << k
                    << " old " << old << " d " << old_distance);
        // Mode 3: today's leaning and eye offset, exactly, wherever the screen ends up.
        bool exact = false;
        const float p3 = MigrateWorldScaleMode3(saved, k, old, size, follow, &exact);
        CHECK(p3 >= kWorldScaleMin);
        CHECK(p3 <= kWorldScaleMax);
        HeadLocked moved = saved;
        moved.distance = FullDistance(p3, size, follow);
        const float raw = WorldScaleForDistance(
            k / (old * saved.width() / saved.factor / saved.distance), size, follow);
        if (raw > kWorldScaleMin && raw < kWorldScaleMax &&
            moved.distance > kFullMinDistance && moved.distance < kFullMaxDistance) {
            ++in_range;
            const float leaning = NaturalLeaning(moved, k);
            CHECK(leaning == Approx(old).epsilon(1e-4));
            CHECK(Mode3Natural(1.0f, leaning, 0.032f, 0.0f).eye_scaler ==
                  Approx(old).epsilon(1e-4)); // 1.2.0: scaler x half the IPD
            CHECK(exact); // so it is made at once
        } else if (!(raw >= kWorldScaleMin && raw <= kWorldScaleMax) ||
                   !(moved.distance >= kFullMinDistance && moved.distance <= kFullMaxDistance)) {
            CHECK_FALSE(exact); // clamped: the leaning changes, so it is eased (the ramps)
        }
        // Mode 2: today's screen, exactly.
        const float p2 = MigrateWorldScaleMode2(old_distance, size, follow);
        const float back = FullDistance(p2, size, follow);
        if (WorldScaleForDistance(old_distance, size, follow) >= kWorldScaleMin &&
            WorldScaleForDistance(old_distance, size, follow) <= kWorldScaleMax &&
            back > kFullMinDistance) {
            CHECK(back == Approx(old_distance).epsilon(1e-5));
        }
    }
    CHECK(in_range > 1000);
    // 1.2.0 did not move the camera (3D Depth under 10 %): Automatic.
    bool exact = true;
    CHECK(MigrateWorldScaleMode3(Screen(1.3f, 0.45f, 1.67f, 0.371f), 73.0f, 0.0f, 1.0f, true,
                                 &exact) == kWorldScaleAuto);
    CHECK_FALSE(exact); // the leaning changes from none: eased
    CHECK(MigrateWorldScaleMode3(Screen(1.3f, 0.45f, 1.67f, 0.371f), -1.0f, 50.0f, 1.0f, true) ==
          kWorldScaleAuto);
    // Mario Kart 7 (K 73 measured) on a screen like the owner's at 0.371 m, 1.2.0's 50 units per
    // metre: a small world, about 42 %.
    const HeadLocked mk7 = Screen(1.3f, 0.45f, 1.67f, 0.371f);
    const float p = MigrateWorldScaleMode3(mk7, 73.0f, 50.0f, 1.0f, true);
    WARN("Mario Kart 7 migrates to World Scale " << p << " %, screen at "
                                                 << FullDistance(p, 1.0f, true) << " m");
    CHECK(p == Approx(42.0f).margin(2.0f));
    // Ocarina of Time 3D at 3 m: Automatic.
    CHECK(MigrateWorldScaleMode2(3.0f, 1.0f, true) == Approx(100.0f));
}

TEST_CASE("Natural: mode 3 takes the game's own 0.01 slider back out", "[frame_xr][natural]") {
    const HeadLocked s = Screen(1.3f, 0.45f, 1.67f, 0.37f);
    const float h = 0.032f;
    // Mario Kart 7's own far scenery at 0.01: 0.05 % of the width.
    const float game_m = GameFarOnQuad(s, 0.0005f);
    CHECK(game_m == Approx(0.0005f / 1.3f * s.width()));
    const Mode3Stereo m = Mode3Natural(1.0f, 50.0f, h, game_m);
    // The far scenery apart on the quads: exactly the IPD (parallel), not more.
    const StereoFar st = StereoFarOnQuad(s, 3, h, 0.0005f, m.quad_half);
    CHECK(st.vergence_deg == Approx(0.0f).margin(1e-4));
    CHECK_FALSE(st.diverges);
    // The eyes on their quads before: 0.05 to 0.08 degrees past parallel.
    CHECK(StereoFarOnQuad(s, 3, h, 0.0005f).vergence_deg < -0.03f);
    CHECK(m.eye_scaler == Approx(50.0f));
    // Half the depth: half our eyes' distance, the quads where they were (infinity parallel).
    const Mode3Stereo half = Mode3Natural(0.5f, 50.0f, h, game_m);
    CHECK(half.eye_scaler == Approx(25.0f));
    CHECK(half.quad_half == Approx(h - game_m / 2.0f));
    CHECK(StereoFarOnQuad(s, 3, h, 0.0005f, half.quad_half).vergence_deg ==
          Approx(0.0f).margin(1e-4));
}

TEST_CASE("Natural: mode 2's slider without jitter", "[frame_xr][natural]") {
    NaturalSlider n;
    int64_t t = 1000;
    // Not measured yet: a small probe, so the game draws two eyes.
    CHECK(n.Update(1.0f, -1.0f, t) == Approx(NaturalSlider::kProbe));
    CHECK(n.Update(0.0f, -1.0f, t += 11) == 0.0f);
    // The first limit at once.
    CHECK(n.Update(1.0f, 0.067f, t += 11) == Approx(0.067f));
    // A lower limit (a bigger screen): at once, never past parallel.
    CHECK(n.Update(1.0f, 0.05f, t += 11) == Approx(0.05f));
    // A slightly higher one (noise): held.
    CHECK(n.Update(1.0f, 0.0504f, t += 11) == Approx(0.05f));
    // Higher by more than the step: eased up.
    float v = n.Update(1.0f, 0.06f, t += 11);
    CHECK(v > 0.05f);
    CHECK(v < 0.06f);
    for (int i = 0; i < 300; ++i) {
        v = n.Update(1.0f, 0.06f, t += 11);
    }
    CHECK(v == Approx(0.06f));
    // A new 3D Depth goes through at once.
    CHECK(n.Update(0.5f, 0.06f, t += 11) == Approx(0.03f));
    CHECK(n.Update(0.0f, 0.06f, t += 11) == 0.0f);
    // The 3DS's slider stops at 1: more is clamped and said.
    CHECK(n.Update(1.0f, 1.7f, t += 11) <= 1.0f);
    CHECK(n.Clamped());
    // The HID sends it as it is.
    CHECK(Settings::FrameVrSlider(0.3f, 1.0f, -1.0f, 0.05f) == Approx(0.05f));
    CHECK(Settings::FrameVrSlider(0.3f, 1.0f, -1.0f, 1.5f) == Approx(1.0f));
    CHECK(Settings::FrameVrSlider(0.3f, 1.0f, -1.0f, -1.0f) == Approx(0.3f));
}

TEST_CASE("Natural: the slider over a resize sweep never diverges", "[frame_xr][natural]") {
    std::mt19937 rng(77);
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    for (int run = 0; run < 200; ++run) {
        NaturalSlider n;
        const float h = uni(0.0225f, 0.0425f);
        const float per = uni(0.01f, 0.4f);
        const float f = uni(0.0f, 1.0f);
        float distance = uni(0.5f, 4.0f);
        int64_t t = 5000;
        for (int frame = 0; frame < 400; ++frame) {
            distance = std::clamp(distance * uni(0.97f, 1.03f), 0.3f, 6.0f);
            const HeadLocked s = Screen(1.4f, 0.8f, 1.67f, distance);
            const float limit = StereoSliderLimit(s, h, per);
            const float sent = n.Update(f, limit, t += 11);
            REQUIRE(sent <= f * limit + 1e-6f);
            const StereoFar st = StereoFarOnQuad(s, 2, h, per * sent);
            CHECK_FALSE(st.diverges);
        }
    }
}

TEST_CASE("Natural: the game's K and the distance never jump", "[frame_xr][natural]") {
    using S = EyePerFarInUse;
    {
        S k;
        int64_t t = 100;
        // Nothing measured: the fallback, at once (nothing to move from).
        CHECK(k.Update(-1.0f, 0, -1.0f, 400.0f, t) == Approx(400.0f));
        CHECK(k.From() == S::kFallback);
        // Read in fewer frames than kSaveAfter: not yet.
        CHECK(k.Update(80.0f, S::kSaveAfter - 1, -1.0f, 400.0f, t += 11) == Approx(400.0f));
        // Measured: towards it, at most a factor 2 a second, never a step.
        float prev = k.Value(), v = prev;
        int frames = 0;
        for (; frames < 3000 && std::abs(v - 80.0f) > 0.01f; ++frames) {
            v = k.Update(80.0f, S::kSaveAfter + frames, -1.0f, 400.0f, t += 11);
            CHECK(v <= prev + 1e-3f);
            CHECK(v >= prev / std::exp(RatioRamp::kMaxRatePerS * 0.011f) - 1e-3f);
            prev = v;
        }
        CHECK(k.From() == S::kMeasured);
        CHECK(v == Approx(80.0f).margin(0.01f));
        CHECK(frames * 11 >= 2300); // ln 5 / ln 2 s and the easing's tail
        // A frame without the game's eyes (a menu, 3D Depth 0): the last measure holds.
        CHECK(k.Update(-1.0f, 0, 70.0f, 400.0f, t += 11) == Approx(80.0f).margin(0.01f));
        CHECK(k.From() == S::kMeasured);
    }
    {
        // An earlier run's K beats the fallback; Set takes a value at once.
        S k;
        CHECK(k.Update(-1.0f, 0, 70.0f, 400.0f, 100) == Approx(70.0f));
        CHECK(k.From() == S::kSaved);
        k.Set(90.0f);
        CHECK(k.Value() == 90.0f);
        CHECK(k.Update(90.0f, 50, 70.0f, 400.0f, 111) == Approx(90.0f));
    }
    {
        // The distance (World Scale, Follow Screen Size, a resize with it off): at most a
        // factor 4 a second, whatever the target does.
        RatioRamp d{1.3862944f};
        int64_t t = 0;
        CHECK(d.Update(3.0f, t) == 3.0f);
        float prev = d.Value();
        std::mt19937 rng(9);
        for (int i = 0; i < 3000; ++i) {
            const float target = i % 400 < 200 ? 0.3f : 12.0f;
            const float v = d.Update(target * std::uniform_real_distribution<float>(0.9f, 1.1f)(rng),
                                     t += 11);
            CHECK(v <= prev * std::exp(1.3862944f * 0.011f) + 1e-5f);
            CHECK(v >= prev / std::exp(1.3862944f * 0.011f) - 1e-5f);
            prev = v;
        }
    }
}

// AzaharFrame natural: rollback. A game's file (its keys as text) goes old -> new -> old -> new.
// 1.2.0 knows only its own keys and drops the others when it saves the file; 1.2.1 reads the old
// keys only, never writes them with another meaning, and makes World Scale from them whenever
// the game has none of its own (WorldScaleToMigrate). So each 1.2.1 run after a 1.2.0 save
// shows the game as a first 1.2.1 run would with those old keys, and 1.2.0 always finds its keys
// as it left them.
TEST_CASE("Natural: World Scale's migration survives old -> new -> old -> new", "[frame_xr][natural]") {
    using File = std::map<std::string, std::string>;
    const std::set<std::string> old_keys{"vr_immersive_positional_game_scaler", "factor_3d",
                                         "distance", "size"};
    // 1.2.0 saving the game's file: its own keys, as they are (or changed by the player).
    const auto old_saves = [&](File f, const File& changes = {}) {
        File out;
        for (const auto& [k, v] : f) {
            if (old_keys.count(k)) {
                out[k] = v;
            }
        }
        for (const auto& [k, v] : changes) {
            out[k] = v;
        }
        return out;
    };
    // 1.2.1 running the game in mode 2 (Follow Screen Size on), then saving its file: the World
    // Scale it uses and the screen's distance it shows.
    struct Run {
        File file;
        float world_scale;
        float distance;
    };
    const auto new_runs = [](File f, std::optional<float> grip = std::nullopt) {
        const bool own = f.count("vr_immersive_world_scale") != 0;
        const bool set = f.count("vr_immersive_world_set") && f["vr_immersive_world_set"] == "true";
        const float size = std::stof(f["size"]);
        float ws = own ? std::stof(f["vr_immersive_world_scale"]) : kWorldScaleAuto;
        if (WorldScaleToMigrate(own, set)) {
            ws = MigrateWorldScaleMode2(std::stof(f["distance"]), size, true);
            f["vr_immersive_world_scale"] = std::to_string(ws);
            f["vr_immersive_world_set"] = "true";
        }
        if (grip) { // the player's own World Scale
            ws = *grip;
            f["vr_immersive_world_scale"] = std::to_string(ws);
        }
        return Run{f, ws, FullDistance(ws, size, true)};
    };
    const File v120{{"vr_immersive_positional_game_scaler", "1"},
                    {"factor_3d", "30"},
                    {"distance", "1.10"},
                    {"size", "1.30"}};
    // Old -> new: the screen where 1.2.0 had it; the old keys as they were.
    const Run a = new_runs(v120);
    CHECK(a.distance == Approx(1.10f).epsilon(1e-4));
    CHECK(old_saves(a.file) == v120);
    // -> old (it saves: the new keys go) -> new: made again, the same.
    const Run b = new_runs(old_saves(a.file));
    CHECK(b.world_scale == Approx(a.world_scale));
    CHECK(b.file == a.file);
    // -> old without a save (the new keys stay) -> new: kept, not made again.
    const Run c = new_runs(a.file);
    CHECK(c.file == a.file);
    // The player's own World Scale in 1.2.1 is kept by 1.2.1 ...
    const Run d = new_runs(a.file, 63.0f);
    CHECK(new_runs(d.file).world_scale == Approx(63.0f));
    CHECK(old_saves(d.file) == v120); // ... and never touches 1.2.0's keys;
    // after 1.2.0 saves the file it is made from the old keys again (1.2.0 cannot keep it).
    CHECK(new_runs(old_saves(d.file)).world_scale == Approx(a.world_scale));
    // 1.2.0 moves the screen: the next 1.2.1 run follows the old key, in step.
    const Run e = new_runs(old_saves(d.file, {{"distance", "2.00"}}));
    CHECK(e.distance == Approx(2.00f).epsilon(1e-4));
    CHECK(old_saves(e.file).at("distance") == "2.00");
    // Use Global Values for All in 1.2.1: the game's World Scale goes, its migrated flag stays:
    // the global value is used, not made again.
    File global = e.file;
    global.erase("vr_immersive_world_scale");
    CHECK_FALSE(WorldScaleToMigrate(false, true));
    CHECK(new_runs(global).world_scale == kWorldScaleAuto);
    // A file 1.2.0 never saw a 1.2.1 key in: always made.
    CHECK(WorldScaleToMigrate(false, false));
    CHECK_FALSE(WorldScaleToMigrate(true, false));
    CHECK_FALSE(WorldScaleToMigrate(true, true));
}
