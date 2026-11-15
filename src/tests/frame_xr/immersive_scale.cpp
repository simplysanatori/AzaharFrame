// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu: Horizontal Scale and Vertical Scale (they replace Narrow Screen and Narrow
// Height): the setting, the D-pad steps, the toasts, the migration of the old values, and the
// head-locked screen at both signs.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h" // AzaharFrame fov100: the setting's range
#include "frame_xr/immersive_layout.h"
#include "frame_xr/immersive_names.h" // AzaharFrame fovaxis: 3D Depth's toast
#include "video_core/shader/generator/glsl_shader_gen.h" // AzaharFrame fovscale

using Catch::Approx;
using namespace FrameXr::Immersive::Layout;
using FrameXr::Input::Posef; // AzaharFrame fov100
using FrameXr::Input::Quatf;
namespace In = FrameXr::Input;

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
    // On the 5 % grid, -100 to +100 (AzaharFrame fov100; -30 to +30 before), halves away from 0
    // the same either side.
    REQUIRE(ScalePercent(0) == 0);
    REQUIRE(ScalePercent(10) == 10);
    REQUIRE(ScalePercent(-10) == -10);
    REQUIRE(ScalePercent(7) == 5);
    REQUIRE(ScalePercent(-7) == -5);
    REQUIRE(ScalePercent(8) == 10);
    REQUIRE(ScalePercent(-8) == -10);
    REQUIRE(ScalePercent(30) == 30);
    REQUIRE(ScalePercent(31) == 30);
    REQUIRE(ScalePercent(33) == 35);
    REQUIRE(ScalePercent(-99) == -100);
    REQUIRE(ScalePercent(-100) == -100);
    REQUIRE(ScalePercent(-101) == -100);
    REQUIRE(ScalePercent(1000) == 100);
    REQUIRE(ScalePercent(-1000) == -100);
    for (int v = -140; v <= 140; ++v) {
        REQUIRE(ScalePercent(-v) == -ScalePercent(v));
        REQUIRE(ScalePercent(v) % kScaleStepPercent == 0);
        REQUIRE(ScalePercent(ScalePercent(v)) == ScalePercent(v));
    }
    // Positive is larger: the quad's narrowing (Options::squeeze) is its negative.
    REQUIRE(SqueezeForScale(0) == 0.0f);
    REQUIRE(SqueezeForScale(-10) == Approx(0.1f));
    REQUIRE(SqueezeForScale(15) == Approx(-0.15f));
    REQUIRE(SqueezeForScale(30) == Approx(-0.3f));
    REQUIRE(SqueezeForScale(99) == Approx(-1.0f));
}

TEST_CASE("FrameXr immersive scale: D-pad steps and toasts", "[frame_xr][kiu]") {
    REQUIRE(SteppedScale(0, 1) == 5);
    REQUIRE(SteppedScale(0, -1) == -5);
    REQUIRE(SteppedScale(-10, 2) == 0);
    REQUIRE(SteppedScale(30, 1) == 35); // AzaharFrame fov100: on past the old ends
    REQUIRE(SteppedScale(-30, -1) == -35);
    REQUIRE(SteppedScale(100, 1) == 100);
    REQUIRE(SteppedScale(-100, -1) == -100);
    REQUIRE(SteppedScale(-95, -3) == -100);
    REQUIRE(SteppedScale(7, 1) == 10); // a hand-edited value goes onto the grid first
    for (int v = -100; v <= 100; v += 5) {
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
    REQUIRE(ScaleToast(99, true) == "Vertical Scale: +100%");
    REQUIRE(ScaleToast(-100, false) == "Horizontal Scale: -100%");
    REQUIRE(ScaleToast(-35, false) == "Horizontal Scale: -35%");
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

// ---- AzaharFrame fovscale: the scales as field of view (ScaleSplit) ----

namespace {
// What the game draws across over what the quad shows across, against the same up and down: 1
// when the picture has one scale both ways (a circle in the game is a circle on the screen).
float PictureShape(const HeadLocked& s, float game_aspect) {
    const float across = s.factor * game_aspect / (s.aspect * s.tan_half_y);
    const float up = s.factor_y / s.tan_half_y;
    return across / up;
}
} // namespace

TEST_CASE("FrameXr fovscale: the split of one axis", "[frame_xr][fovscale]") {
    for (uint32_t level = 0; level <= kMaxWiderLevel; ++level) {
        const float b = WiderFactor(level);
        for (int scale = -kMaxScalePercent; scale <= kMaxScalePercent; scale += kScaleStepPercent) {
            const float k = 1.0f - SqueezeForScale(scale);
            INFO("Render Margin " << b << " scale " << scale);
            // Stretch: today's numbers, bit for bit (the factor as is, the whole scale a stretch).
            const AxisSplit st = ScaleSplit(b, k, 0.0f);
            REQUIRE(st.drawn == b);
            REQUIRE(st.stretch == k);
            // Field of View: drawn x stretch is always the factor x the scale.
            const AxisSplit fv = ScaleSplit(b, k, 1.0f);
            REQUIRE(fv.drawn * fv.stretch == Approx(b * k).epsilon(1e-6));
            REQUIRE(fv.drawn >= kCropFactorMin);
            REQUIRE(fv.drawn <= kFovFactorMax);
            if (b * k <= kFovFactorMax) {
                // Inside the limits: all of it drawn, nothing stretched (AzaharFrame fovaxis:
                // below the game's own frame too, a crop).
                REQUIRE(fv.stretch == Approx(1.0f).margin(1e-6));
                REQUIRE(fv.drawn == Approx(b * k).epsilon(1e-6));
            } else {
                // Past 1.6x: 1.6x drawn, the rest a stretch (larger).
                REQUIRE(fv.drawn == kFovFactorMax);
                REQUIRE(fv.stretch > 1.0f);
            }
        }
    }
    // Render Margin Off: a negative scale crops (AzaharFrame fovaxis; a stretch before), a
    // positive one is all drawn up to 1.6x.
    REQUIRE(ScaleSplit(1.0f, 0.7f, 1.0f).drawn == Approx(0.7f));
    REQUIRE(ScaleSplit(1.0f, 0.7f, 1.0f).stretch == Approx(1.0f));
    REQUIRE(ScaleSplit(1.0f, 1.3f, 1.0f).drawn == Approx(1.3f));
    REQUIRE(ScaleSplit(1.0f, 1.3f, 1.0f).stretch == Approx(1.0f));
    // 1.4x at -30 %: draws 0.98x (fovscale's "draws 1.00x, stretch 0.98" before).
    REQUIRE(ScaleSplit(1.4f, 0.7f, 1.0f).drawn == Approx(0.98f));
    REQUIRE(ScaleSplit(1.4f, 0.7f, 1.0f).stretch == Approx(1.0f));
    // The upper limit: 1.6x (AzaharFrame fovaxis; fov100's 2.8x, fovscale's 1.82x); past it the
    // rest is a stretch.
    REQUIRE(kFovFactorMax == Approx(1.6f));
    REQUIRE(ScaleSplit(1.4f, 1.1f, 1.0f).drawn == Approx(1.54f));
    REQUIRE(ScaleSplit(1.4f, 1.1f, 1.0f).stretch == Approx(1.0f));
    REQUIRE(ScaleSplit(1.4f, 1.3f, 1.0f).drawn == Approx(kFovFactorMax));
    REQUIRE(ScaleSplit(1.4f, 1.3f, 1.0f).stretch == Approx(1.82f / kFovFactorMax));
    REQUIRE(ScaleSplit(1.4f, 2.0f, 1.0f).drawn == Approx(kFovFactorMax));
    REQUIRE(ScaleSplit(1.4f, 2.0f, 1.0f).stretch == Approx(1.75f));
    REQUIRE(ScaleSplit(1.4f, 2.5f, 1.0f).drawn == Approx(kFovFactorMax));
    REQUIRE(ScaleSplit(1.4f, 2.5f, 1.0f).stretch == Approx(1.4f * 2.5f / kFovFactorMax));
    // Not a number or nonsense: the factor as drawn, no stretch.
    REQUIRE(ScaleSplit(1.4f, std::nanf(""), 1.0f).stretch == 1.0f);
    REQUIRE(ScaleSplit(1.4f, 0.8f, std::nanf("")).stretch == 0.8f); // Stretch
}

TEST_CASE("FrameXr fovscale: no jump at a limit, nor while the menu rule eases",
          "[frame_xr][fovscale]") {
    // The scale swept finely through both limits, and the Field of View share from 0 to 1 (mode
    // 2's menu rule): drawn and stretch move continuously, and their product is the scale.
    for (const float b : {1.0f, 1.1f, 1.2f, 1.3f, 1.4f}) {
        for (const float share : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            AxisSplit last = ScaleSplit(b, 0.5f, share);
            for (int i = 1; i <= 2000; ++i) {
                const float k = 0.5f + i * 0.001f; // 0.5 to 2.5: past both limits
                const AxisSplit now = ScaleSplit(b, k, share);
                INFO("factor " << b << " share " << share << " k " << k);
                REQUIRE(now.drawn * now.stretch == Approx(b * k).epsilon(1e-5));
                REQUIRE(std::abs(now.drawn - last.drawn) < 0.004f);
                REQUIRE(std::abs(now.stretch - last.stretch) < 0.004f);
                last = now;
            }
        }
        for (const float k : {0.7f, 0.85f, 1.15f, 1.3f}) {
            AxisSplit last = ScaleSplit(b, k, 0.0f);
            for (int i = 1; i <= 1000; ++i) {
                const AxisSplit now = ScaleSplit(b, k, i / 1000.0f);
                REQUIRE(std::abs(now.drawn - last.drawn) < 0.002f);
                REQUIRE(std::abs(now.stretch - last.stretch) < 0.002f);
                last = now;
            }
            REQUIRE(last.drawn == ScaleSplit(b, k, 1.0f).drawn);
            REQUIRE(last.stretch == ScaleSplit(b, k, 1.0f).stretch);
        }
    }
}

// The solver in both modes over random settings: the quad is the same, Stretch is today's
// numbers bit for bit, Field of View has one picture scale and equal gains within its limits,
// and in both the gain times the quad's size is what the game draws (the world locked).
TEST_CASE("FrameXr fovscale: the head-locked screen in both modes", "[frame_xr][fovscale]") {
    std::mt19937 rng(20261114u);
    std::uniform_int_distribution<int> scale(-6, 6), level(0, 4), pick(0, 9);
    std::uniform_real_distribution<float> fov(30.0f, 90.0f), live(30.0f, 90.0f),
        half(35.0f, 60.0f), size(0.6f, 2.5f), aspect(1.3f, 2.4f), menu(0.0f, 1.0f);
    int inside = 0, at_limit = 0;
    for (int i = 0; i < 600; ++i) {
        Options o;
        const int sx = scale(rng) * kScaleStepPercent;
        const int sy = scale(rng) * kScaleStepPercent;
        o.squeeze = SqueezeForScale(sx);
        o.squeeze_y = SqueezeForScale(sy);
        o.wider = WiderFactor(static_cast<uint32_t>(level(rng)));
        o.live_fov_y_deg = pick(rng) < 3 ? 0.0f : live(rng);
        // Mode 2's menu rule now and then (drawn with less, sized with the margin).
        const bool in_menu = pick(rng) == 0;
        if (in_menu) {
            o.size_wider = o.wider;
            o.wider = MenuExact::Factor(o.wider, menu(rng));
        }
        Placement p;
        p.size = size(rng);
        const float a = aspect(rng);
        const float game_fov = fov(rng);
        const ViewFov view = Symmetric(half(rng));
        INFO("case " << i << " scales " << sx << ", " << sy << " margin " << o.wider);
        Options os = o, of = o;
        os.scale_fov = 0.0f;
        of.scale_fov = 1.0f;
        const HeadLocked st = SolveHeadLocked(p, game_fov, a, view, 0.0f, os);
        const HeadLocked fv = SolveHeadLocked(p, game_fov, a, view, 0.0f, of);
        // The default Options are Stretch: exactly the numbers of every build before.
        const HeadLocked def = SolveHeadLocked(p, game_fov, a, view, 0.0f, o);
        REQUIRE(def.factor == st.factor);
        REQUIRE(def.gain_x == st.gain_x);
        // Stretch, bit for bit: the scale-0 screen's factor and gain over the scale.
        Options o0 = os;
        o0.squeeze = o0.squeeze_y = 0.0f;
        const HeadLocked z = SolveHeadLocked(p, game_fov, a, view, 0.0f, o0);
        REQUIRE(st.factor == z.factor);
        REQUIRE(st.factor_y == z.factor_y);
        if (z.gain > kMinGain && z.gain < kMaxGain) {
            REQUIRE(st.gain == std::clamp(z.gain / (1.0f - o.squeeze_y), kMinGain, kMaxGain));
        }
        if (z.gain_x > kMinGain && z.gain_x < kMaxGain) {
            REQUIRE(st.gain_x == std::clamp(z.gain_x / (1.0f - o.squeeze), kMinGain, kMaxGain));
        }
        // The quad is the same in both modes (an older build shows the same screen).
        REQUIRE(fv.tan_half_y == st.tan_half_y);
        REQUIRE(fv.aspect == st.aspect);
        REQUIRE(fv.width() == st.width());
        REQUIRE(fv.height() == st.height());
        REQUIRE(fv.distance == st.distance);
        REQUIRE(fv.pitch_rad == st.pitch_rad);
        // Both: what is drawn times what is stretched is the scaled factor, per axis.
        REQUIRE(fv.factor * fv.stretch_x == Approx(st.factor * st.stretch_x).epsilon(1e-5));
        REQUIRE(fv.factor_y * fv.stretch_y == Approx(st.factor_y * st.stretch_y).epsilon(1e-5));
        // Both: the gain times the quad's half size is what the game draws (the world locked on
        // each axis), wherever no gain limit is hit.
        const float t_live = std::tan((o.live_fov_y_deg > 0.0f ? o.live_fov_y_deg : game_fov) *
                                      kDegToRad / 2.0f);
        const float dos = o.size_wider > 0.0f ? o.wider / o.size_wider : 1.0f;
        for (const HeadLocked* s : {&st, &fv}) {
            if (s->gain_x > kMinGain && s->gain_x < kMaxGain && s->gain > kMinGain &&
                s->gain < kMaxGain && !in_menu) {
                REQUIRE(s->gain_x * s->aspect * s->tan_half_y ==
                        Approx(s->factor * a * t_live / dos).epsilon(1e-3));
                REQUIRE(s->gain * s->tan_half_y == Approx(s->factor_y * t_live / dos).epsilon(1e-3));
            }
        }
        // Field of View inside its limits: one picture scale both ways and equal gains.
        const bool fits = fv.stretch_x == Approx(1.0f).margin(1e-5) &&
                          fv.stretch_y == Approx(1.0f).margin(1e-5);
        if (fits) {
            inside++;
            REQUIRE(PictureShape(fv, a) == Approx(1.0f).epsilon(1e-4));
            REQUIRE(fv.gain_x == Approx(fv.gain).epsilon(1e-4));
            // Stretch on the same settings: the shape is the scales' ratio.
            REQUIRE(PictureShape(st, a) == Approx((1.0f - o.squeeze_y) / (1.0f - o.squeeze))
                                               .epsilon(1e-4));
        } else {
            // At a limit: what is left is the shape (the stretch left on each axis).
            at_limit++;
            REQUIRE(PictureShape(fv, a) ==
                    Approx(fv.stretch_y / fv.stretch_x).epsilon(1e-4));
        }
    }
    std::printf("fovscale solver: 600 settings, %d within Field of View's limits, %d at one\n",
                inside, at_limit);
    REQUIRE(inside > 200);
    REQUIRE(at_limit > 20);
}

// The game-side stereo measures: far scenery on the quad and the stereo slider's limit follow
// the drawn factor and the quad's width, so in Field of View Horizontal Scale cancels out; in
// Stretch they grow with it, as before.
TEST_CASE("FrameXr fovscale: stereo does not change with Horizontal Scale in Field of View",
          "[frame_xr][fovscale]") {
    const ViewFov view = Symmetric(50);
    const float half_ipd = 0.032f, far_width = 0.004f, per = 0.0002f;
    for (const float fov_share : {0.0f, 1.0f}) {
        Options o0;
        o0.wider = kFactorMax; // Render Margin 1.4x
        o0.scale_fov = fov_share;
        const HeadLocked z = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o0);
        // Inside the limits at 1.4x (AzaharFrame fovaxis: up to +10 %, 1.54x; down to the crop)
        for (int scale = -30; scale <= 10; scale += 5) {
            Options o = o0;
            o.squeeze = SqueezeForScale(scale);
            const HeadLocked s = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o);
            const float k = 1.0f + scale / 100.0f;
            INFO("share " << fov_share << " scale " << scale);
            const float want = fov_share == 1.0f ? 1.0f : k;
            REQUIRE(GameFarOnQuad(s, far_width) ==
                    Approx(GameFarOnQuad(z, far_width) * want).epsilon(1e-4));
            REQUIRE(StereoSliderLimit(s, half_ipd, per, 0.0f) ==
                    Approx(StereoSliderLimit(z, half_ipd, per, 0.0f) / want).epsilon(1e-4));
            REQUIRE(StereoFarOnQuad(s, 3, half_ipd, far_width).separation_m ==
                    Approx(StereoFarOnQuad(z, 3, half_ipd, far_width).separation_m +
                           GameFarOnQuad(z, far_width) * (want - 1.0f))
                        .epsilon(1e-4));
        }
    }
}

// The vertex shader's per-axis factors (Field of View draws them unequal): every vertex of the
// top screen is divided by `factor` across (clip y: the 3DS top screen is drawn turned) and by
// `factor_y` up and down (clip x), and a 2D vertex (w = 1) on or past an edge of the view keeps
// its place on that axis. The shader's own text is pinned, so the model below is that text.
TEST_CASE("FrameXr fovscale: the vertex shader's factors with unequal axes",
          "[frame_xr][fovscale]") {
    const std::string vs =
        Pica::Shader::Generator::GLSL::GenerateTrivialVertexShader(false, false);
    const std::string vr_factor = R"(vec2 VrFactor(vec4 pos) {
    vec2 f = vec2(vr_immersive_mode_factor_y, vr_immersive_mode_factor);
    if (abs(pos.w - 1.0) < 0.0001) {
        f = mix(f, vec2(1.0), greaterThanEqual(abs(pos.xy), vec2(0.999)));
    }
    return f;
})";
    REQUIRE(vs.find(vr_factor) != std::string::npos);
    REQUIRE(vs.find("gl_Position.xy /= VrFactor(gl_Position);") != std::string::npos);
    REQUIRE(vs.find("float vr_immersive_mode_factor_y;") != std::string::npos);
    struct V {
        float x, y, w;
    };
    const auto vr = [](V p, float factor, float factor_y) { // the text above, in C++
        float fx = factor_y, fy = factor;
        if (std::abs(p.w - 1.0f) < 0.0001f) {
            fx = std::abs(p.x) >= 0.999f ? 1.0f : fx;
            fy = std::abs(p.y) >= 0.999f ? 1.0f : fy;
        }
        return V{p.x / fx, p.y / fy, p.w};
    };
    // Field of View at Horizontal -30 %, Vertical -15 %, Render Margin 1.4x: 0.98x across (a
    // crop, AzaharFrame fovaxis; 1.00x before), 1.19x up and down.
    const AxisSplit across = ScaleSplit(1.4f, 0.7f, 1.0f), up = ScaleSplit(1.4f, 0.85f, 1.0f);
    REQUIRE(across.drawn == Approx(0.98f));
    REQUIRE(up.drawn == Approx(1.19f));
    // A world vertex (w != 1): each axis by its own factor.
    const V world = vr({0.5f, 0.5f, 2.0f}, across.drawn, up.drawn);
    REQUIRE(world.x == Approx(0.5f / up.drawn));
    REQUIRE(world.y == Approx(0.5f / across.drawn));
    // 2D inside the view (the HUD): each axis by its own factor, as the world.
    const V hud = vr({0.5f, -0.5f, 1.0f}, 1.3f, 1.1f);
    REQUIRE(hud.x == Approx(0.5f / 1.1f));
    REQUIRE(hud.y == Approx(-0.5f / 1.3f));
    // 2D on an edge keeps its place on that axis only; a full-screen pass stays full screen.
    const V edge_x = vr({1.0f, 0.5f, 1.0f}, 1.3f, 1.1f);
    REQUIRE(edge_x.x == 1.0f);
    REQUIRE(edge_x.y == Approx(0.5f / 1.3f));
    const V edge_y = vr({0.25f, -1.0f, 1.0f}, 1.3f, 1.1f);
    REQUIRE(edge_y.x == Approx(0.25f / 1.1f));
    REQUIRE(edge_y.y == -1.0f);
    const V corner = vr({-1.0f, 1.0f, 1.0f}, 1.82f, 1.4f);
    REQUIRE(corner.x == -1.0f);
    REQUIRE(corner.y == 1.0f);
}

// ---- AzaharFrame fov100: the scales from -100 % to +100 % ----

namespace {
// What every build before fov100 did with a scale: on the 5 % grid within -30 to +30 %, the
// squeeze -scale / 100, and the Field of View split with its upper limit at 1.4x x 1.3.
int OldScalePercent(int setting) {
    const int c = std::clamp(setting, -30, 30);
    return c >= 0 ? (c + 2) / 5 * 5 : -((-c + 2) / 5 * 5);
}
float OldSqueeze(int setting) {
    return -static_cast<float>(OldScalePercent(setting)) / 100.0f;
}
AxisSplit OldSplit(float factor, float k, float fov) {
    constexpr float old_max = kFactorMax * (1.0f + 30.0f / 100.0f);
    if (!(fov > 0.0f)) {
        return {factor, k};
    }
    const float want = factor * (fov >= 1.0f ? k : std::pow(k, fov));
    const float drawn = std::clamp(want, kFactorMin, std::max(old_max, factor));
    return {drawn, factor * k / drawn};
}
// AzaharFrame fovaxis: where the split is still the old one: Stretch always; a Field of View
// share where the old split drew between the game's own frame and 1.6x (below it crops now,
// above it the rest is a stretch).
bool FovaxisSame(float factor, float k, float fov) {
    if (!(fov > 0.0f)) {
        return true;
    }
    const float want = factor * (fov >= 1.0f ? k : std::pow(k, fov));
    return want >= kFactorMin && want <= kFovFactorMax;
}
bool Finite(const HeadLocked& s) {
    for (const float f : {s.factor, s.factor_y, s.tan_half_y, s.aspect, s.pitch_rad, s.distance,
                          s.gain, s.gain_x, s.stretch_x, s.stretch_y, s.yaw_rad}) {
        if (!std::isfinite(f)) {
            return false;
        }
    }
    return true;
}
} // namespace

// Rollback-safe: every value an earlier build could save (-30 to +30 %, hand-edited ones too)
// gives the same squeeze, the same size and the same split, bit for bit.
TEST_CASE("FrameXr fov100: the old range keeps its meaning", "[frame_xr][fov100]") {
    REQUIRE(kMaxScalePercent == 100);
    REQUIRE(kLinearScalePercent == 30);
    for (int v = -30; v <= 30; ++v) {
        INFO("setting " << v);
        REQUIRE(ScalePercent(v) == OldScalePercent(v));
        REQUIRE(SqueezeForScale(v) == OldSqueeze(v)); // bit for bit
        REQUIRE(ScaleFactor(v) == 1.0f - OldSqueeze(v));
        REQUIRE(ScaleFactor(v) == Approx(1.0f + OldScalePercent(v) / 100.0f).epsilon(1e-7));
    }
    // The split of each old value in both modes, at every Render Margin and menu-rule share.
    // AzaharFrame fovaxis: the same wherever the old split drew between the game's own frame and
    // 1.6x (FovaxisSame); below the frame it crops now, above 1.6x it stretches the rest.
    for (uint32_t level = 0; level <= kMaxWiderLevel; ++level) {
        const float b = WiderFactor(level);
        for (int p = -30; p <= 30; p += 5) {
            const float k = 1.0f - SqueezeForScale(p);
            for (const float share : {0.0f, 0.3f, 0.7f, 1.0f}) {
                const AxisSplit now = ScaleSplit(b, k, share), old = OldSplit(b, k, share);
                if (FovaxisSame(b, k, share)) {
                    REQUIRE(now.drawn == old.drawn);
                    REQUIRE(now.stretch == old.stretch);
                } else {
                    REQUIRE(now.drawn * now.stretch == Approx(old.drawn * old.stretch));
                }
            }
        }
    }
    // The solved screen of old values, in both modes: the quad, factors and gains follow the
    // same split and size as before (the solver's own clamps are wider than the old range).
    std::mt19937 rng(20261114u);
    std::uniform_int_distribution<int> scale(-6, 6), level(0, 4);
    std::uniform_real_distribution<float> fov(30.0f, 90.0f), half(35.0f, 60.0f),
        size(kMinSize, kMaxSize), aspect(1.3f, 2.4f);
    for (int i = 0; i < 500; ++i) {
        Options o;
        const int sx = scale(rng) * 5, sy = scale(rng) * 5;
        o.squeeze = SqueezeForScale(sx);
        o.squeeze_y = SqueezeForScale(sy);
        o.wider = WiderFactor(static_cast<uint32_t>(level(rng)));
        Placement p;
        p.size = size(rng);
        const float a = aspect(rng), game_fov = fov(rng);
        const ViewFov view = Symmetric(half(rng));
        Options o0 = o;
        o0.squeeze = o0.squeeze_y = 0.0f;
        const HeadLocked z = SolveHeadLocked(p, game_fov, a, view, 0.0f, o0);
        for (const float share : {0.0f, 1.0f}) {
            o.scale_fov = share;
            const HeadLocked s = SolveHeadLocked(p, game_fov, a, view, 0.0f, o);
            INFO("case " << i << " scales " << sx << ", " << sy << " share " << share);
            const AxisSplit ax = OldSplit(z.factor, 1.0f - OldSqueeze(sx), share);
            const AxisSplit up = OldSplit(z.factor_y, 1.0f - OldSqueeze(sy), share);
            if (!FovaxisSame(z.factor, 1.0f - OldSqueeze(sx), share) ||
                !FovaxisSame(z.factor_y, 1.0f - OldSqueeze(sy), share)) {
                continue; // AzaharFrame fovaxis: a crop or past 1.6x (the tests below)
            }
            // The same split; up and down only to float rounding: factor_y is the factor again
            // through the screen's shape (a last-bit difference), and at 1.4x and +30 % the old
            // limit (1.4 x 1.3, rounded) could take that last bit off. 1e-6 is far below a pixel.
            REQUIRE(s.factor == ax.drawn);
            REQUIRE(s.stretch_x == ax.stretch);
            REQUIRE(s.factor_y == Approx(up.drawn).epsilon(1e-6));
            REQUIRE(s.stretch_y == Approx(up.stretch).epsilon(1e-6));
            REQUIRE(s.tan_half_y == z.tan_half_y * (1.0f - OldSqueeze(sy)));
            REQUIRE(s.width() == Approx(z.width() * (1.0f + sx / 100.0f)).epsilon(1e-5));
        }
    }
}

// Below -30 % the size follows 0.7 x e^((scale + 30) / 70): the line's value and slope at -30 %
// (no jump, no kink), the same share smaller each step, never 0; above, the line to 2 at +100 %.
TEST_CASE("FrameXr fov100: the size from -100 % to +100 %", "[frame_xr][fov100]") {
    REQUIRE(ScaleFactor(-100) == Approx(kMinScaleFactor).epsilon(1e-6));
    REQUIRE(kMinScaleFactor == Approx(0.7 / std::exp(1.0)).epsilon(1e-7));
    REQUIRE(ScaleFactor(100) == kMaxScaleFactor);
    REQUIRE(ScaleFactor(100) == 2.0f);
    REQUIRE(ScaleFactor(-30) == Approx(0.7f));
    REQUIRE(ScaleFactor(-35) == Approx(0.7 * std::exp(-5.0 / 70.0)));
    REQUIRE(ScaleFactor(-65) == Approx(0.7 * std::exp(-0.5)));
    // Clamped like the setting.
    REQUIRE(ScaleFactor(-1000) == ScaleFactor(-100));
    REQUIRE(ScaleFactor(1000) == ScaleFactor(100));
    REQUIRE(SqueezeForScale(-100) == Approx(1.0f - kMinScaleFactor));
    REQUIRE(SqueezeForScale(100) == Approx(-1.0f));
    // Value and slope match at -30 % (the curve's slope there is 1/100 a percent, the line's).
    const double slope = (0.7 - 0.7 * std::exp(-1e-3 / 70.0)) / 1e-3;
    REQUIRE(slope == Approx(0.01).epsilon(1e-4));
    // Every step larger than the one below; below -30 % each the same share (e^(5/70)) of it.
    float last = ScaleFactor(-100);
    for (int p = -95; p <= 100; p += 5) {
        const float k = ScaleFactor(p);
        INFO("scale " << p);
        REQUIRE(k > last);
        REQUIRE(k > 0.0f);
        REQUIRE(std::isfinite(k));
        if (p <= -30) {
            REQUIRE(k / last == Approx(std::exp(5.0 / 70.0)).epsilon(1e-5));
        } else {
            REQUIRE(k - last == Approx(0.05f).epsilon(1e-4)); // the line, 5 % a step
        }
        last = k;
    }
    // The step from -30 % to -35 % is as big as the line's next one would have been, near enough.
    REQUIRE(ScaleFactor(-30) - ScaleFactor(-35) == Approx(0.05f).epsilon(0.04));
}

// The ends in both Scale Modes, as the immersive screen: everything finite and usable, the quad
// exactly the size the scale gives, the gains within their limits, the drawn view under 180
// degrees, and the HUD (the game's own frame) on a quarter of the default screen at -100 %.
TEST_CASE("FrameXr fov100: -100 % and +100 % in both modes", "[frame_xr][fov100]") {
    std::mt19937 rng(20261115u);
    std::uniform_int_distribution<int> level(0, 4), pick(0, 9);
    std::uniform_real_distribution<float> fov(30.0f, 110.0f), live(10.0f, 150.0f),
        half(35.0f, 60.0f), size(kMinSize, kMaxSize), aspect(1.2f, 3.0f), menu(0.0f, 1.0f),
        dist(kMinDistance, kMaxDistance), turn(-1.0f, 1.0f);
    const int ends[] = {-100, -95, -65, -35, -30, 0, 30, 35, 65, 95, 100};
    int checked = 0;
    float widest_deg = 0.0f;
    for (int i = 0; i < 3000; ++i) {
        Options o;
        const int sx = ends[rng() % 11];
        const int sy = ends[rng() % 11];
        o.squeeze = SqueezeForScale(sx);
        o.squeeze_y = SqueezeForScale(sy);
        o.wider = WiderFactor(static_cast<uint32_t>(level(rng)));
        o.live_fov_y_deg = pick(rng) < 3 ? 0.0f : live(rng);
        const bool in_menu = pick(rng) == 0;
        if (in_menu) {
            o.size_wider = o.wider;
            o.wider = MenuExact::Factor(o.wider, menu(rng));
        }
        Placement p;
        p.size = size(rng);
        p.distance = dist(rng);
        const float a = aspect(rng), game_fov = fov(rng);
        const ViewFov view = Symmetric(half(rng));
        Options o0 = o;
        o0.squeeze = o0.squeeze_y = 0.0f;
        o0.scale_fov = 0.0f;
        const HeadLocked z = SolveHeadLocked(p, game_fov, a, view, 0.0f, o0);
        for (const float share : {0.0f, 1.0f}) {
            o.scale_fov = share;
            const HeadLocked s = SolveHeadLocked(p, game_fov, a, view, 0.0f, o);
            INFO("case " << i << " scales " << sx << ", " << sy << " share " << share << " margin "
                         << o.wider);
            REQUIRE(Finite(s));
            // The quad: exactly the scale's size on each axis, never 0 or inverted.
            REQUIRE(s.width() > 0.0f);
            REQUIRE(s.height() > 0.0f);
            REQUIRE(s.width() == Approx(z.width() * ScaleFactor(sx)).epsilon(1e-4));
            REQUIRE(s.height() == Approx(z.height() * ScaleFactor(sy)).epsilon(1e-4));
            REQUIRE(s.width() >= z.width() * kMinScaleFactor * 0.9999f);
            REQUIRE(s.width() <= z.width() * kMaxScaleFactor * 1.0001f);
            // The split: drawn x stretch is the factor x the size; a factor of at least the game's
            // own frame in Stretch (AzaharFrame fovaxis: Field of View crops, down to
            // kCropFactorMin), at most kFovFactorMax; a stretch above 0.
            const float floor = share == 0.0f ? kFactorMin : kCropFactorMin;
            REQUIRE(s.factor >= floor);
            REQUIRE(s.factor_y >= floor * (1.0f - 1e-6f)); // through the shape: rounding
            REQUIRE(s.factor <= std::max(kFovFactorMax, z.factor) * 1.0001f);
            REQUIRE(s.stretch_x > 0.0f);
            REQUIRE(s.stretch_y > 0.0f);
            REQUIRE(s.factor * s.stretch_x == Approx(z.factor * ScaleFactor(sx)).epsilon(1e-5));
            REQUIRE(s.factor_y * s.stretch_y == Approx(z.factor_y * ScaleFactor(sy)).epsilon(1e-5));
            // The gains: within their limits, and over the stretch left where none is hit.
            REQUIRE(s.gain >= kMinGain);
            REQUIRE(s.gain <= kMaxGain);
            REQUIRE(s.gain_x >= kMinGain);
            REQUIRE(s.gain_x <= kMaxGain);
            // The game's drawn view (its camera times the factor, as tangents): under 180 degrees.
            const float t = std::tan(
                std::clamp(o.live_fov_y_deg > 0.0f ? o.live_fov_y_deg : game_fov, 10.0f, 150.0f) *
                kDegToRad / 2.0f);
            const float across_deg = 2.0f * std::atan(s.factor * a * t) / kDegToRad;
            const float up_deg = 2.0f * std::atan(s.factor_y * t) / kDegToRad;
            REQUIRE(across_deg < 180.0f);
            REQUIRE(up_deg < 180.0f);
            widest_deg = std::max({widest_deg, across_deg, up_deg});
            // Field of View (AzaharFrame fovaxis): +100 % draws up to 1.6x and stretches the rest;
            // -100 % crops: a quarter of the frame drawn, no stretch, the HUD's place kept (the
            // game's own frame as wide on the screen as at 0 %, so mostly past the edges).
            if (share == 1.0f && !in_menu) {
                if (sx == 100) {
                    REQUIRE(s.factor == Approx(std::min(z.factor * 2.0f, kFovFactorMax)));
                    REQUIRE(s.stretch_x ==
                            Approx(z.factor * 2.0f / std::min(z.factor * 2.0f, kFovFactorMax)));
                }
                if (sx == -100) {
                    REQUIRE(s.factor == Approx(z.factor * kMinScaleFactor).epsilon(1e-5));
                    REQUIRE(s.stretch_x == Approx(1.0f).epsilon(1e-5));
                    REQUIRE(s.width() / s.factor == Approx(z.width() / z.factor).epsilon(1e-4));
                }
            }
            // Stretch: the HUD inside the game's own frame at -100 % is on at most a quarter of
            // the default screen's width (the "HUD in view" end).
            if (sx == -100 && !in_menu && share == 0.0f) {
                REQUIRE(s.width() / s.factor <= z.width() * kMinScaleFactor * 1.0001f);
            }
            // The head-driven camera and the quad's pose: finite, unit rotations.
            const Quatf head =
                In::Normalized(Quatf{turn(rng) * 0.3f, turn(rng) * 0.5f, turn(rng) * 0.2f, 1.0f});
            const Quatf cam = GameCameraRotation(head, s);
            REQUIRE(std::abs(cam.x * cam.x + cam.y * cam.y + cam.z * cam.z + cam.w * cam.w - 1.0f) <
                    1e-4f);
            for (int eye = 0; eye < 2; ++eye) {
                const Posef q = HeadLockedQuadPose(s, EyeX(eye, 0.032f));
                REQUIRE(std::isfinite(q.position.x));
                REQUIRE(std::isfinite(q.position.z));
            }
            // Natural 3D and World Scale's measures: finite (the slider limit may be "none").
            const StereoFar far2 = StereoFarOnQuad(s, 2, 0.032f, 0.004f);
            const StereoFar far3 = StereoFarOnQuad(s, 3, 0.032f, 0.004f);
            REQUIRE(std::isfinite(far2.vergence_deg));
            REQUIRE(std::isfinite(far3.vergence_deg));
            REQUIRE(std::isfinite(far2.natural_scale));
            REQUIRE(far2.natural_scale > 0.0f);
            const float limit = StereoSliderLimit(s, 0.032f, 0.0002f, kNaturalMarginDeg);
            REQUIRE(!std::isnan(limit));
            REQUIRE(std::isfinite(GameFarOnQuad(s, 0.004f)));
            REQUIRE(ParallelDistance(s, 0.032f, 0.004f) > 0.0f);
            const float leaning = NaturalLeaning(s, 0.5f);
            REQUIRE(std::isfinite(leaning));
            REQUIRE(leaning > 0.0f);
            REQUIRE(EyePerFarFor(s, leaning) == Approx(0.5f).epsilon(1e-4));
            const float ws = MigrateWorldScaleMode3(s, 0.5f, 0.8f, p.size, true);
            REQUIRE(std::isfinite(ws));
            REQUIRE(ws >= kWorldScaleMin);
            REQUIRE(ws <= kWorldScaleMax);
            checked++;
        }
    }
    std::printf("fov100: %d screens at the scales' ends in both modes; widest drawn view %.1f "
                "degrees\n",
                checked, widest_deg);
    REQUIRE(checked == 6000);
}

// Stretch at the ends: the picture's shape is the sizes' ratio (7.8 at -100 % across and +100 %
// up and down), finite; Field of View within its limits keeps one scale both ways.
TEST_CASE("FrameXr fov100: the picture's shape at the ends", "[frame_xr][fov100]") {
    const ViewFov view = Symmetric(50);
    for (const int sx : {-100, 100}) {
        for (const int sy : {-100, 100}) {
            Options o;
            o.wider = kFactorMax;
            o.squeeze = SqueezeForScale(sx);
            o.squeeze_y = SqueezeForScale(sy);
            o.scale_fov = 0.0f;
            const HeadLocked st = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o);
            INFO("scales " << sx << ", " << sy);
            REQUIRE(PictureShape(st, kAspect3ds) ==
                    Approx(ScaleFactor(sy) / ScaleFactor(sx)).epsilon(1e-4));
            o.scale_fov = 1.0f;
            const HeadLocked fv = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o);
            REQUIRE(std::isfinite(PictureShape(fv, kAspect3ds)));
            if (sx == 100 && sy == 100) {
                REQUIRE(PictureShape(fv, kAspect3ds) == Approx(1.0f).epsilon(1e-4));
                REQUIRE(fv.gain_x == Approx(fv.gain).epsilon(1e-4));
            }
        }
    }
    Options o;
    o.squeeze = SqueezeForScale(-100);
    o.squeeze_y = SqueezeForScale(100);
    const HeadLocked st = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o);
    REQUIRE(PictureShape(st, kAspect3ds) == Approx(2.0f / kMinScaleFactor).epsilon(1e-4));
    REQUIRE(2.0f / kMinScaleFactor == Approx(7.77f).epsilon(1e-3));
}

// The settings themselves: -100 to +100, what a reader clamps to. An older build's range was -30
// to +30: a value past it comes back as its end, one inside it unchanged (old -> new -> old ->
// new); the newer build never changes what a value of the old range means.
TEST_CASE("FrameXr fov100: the setting's range and a round trip through an older build",
          "[frame_xr][fov100]") {
    auto& v = Settings::values;
    for (auto* s : {&v.vr_immersive_scale_x, &v.vr_immersive_scale_y}) {
        const bool global = s->UsingGlobal();
        const s32 before = s->GetValue();
        s->SetGlobal(true);
        const s32 global_before = s->GetValue(true);
        REQUIRE(s->GetDefault() == 40); // 1.2.0; 0 before, which an older build still reads
        s->SetValue(-100);
        REQUIRE(s->GetValue() == -100);
        s->SetValue(100);
        REQUIRE(s->GetValue() == 100);
        s->SetValue(-150);
        REQUIRE(s->GetValue() == -100);
        s->SetValue(1000);
        REQUIRE(s->GetValue() == 100);
        s->SetValue(-80);
        REQUIRE(s->GetValue() == -80);
        s->SetValue(global_before);
        s->SetGlobal(global);
        if (!global) {
            s->SetValue(before);
        }
    }
    // An older build's setting (the same key, -30 to +30) reading what this one wrote, and back.
    Settings::Setting<s32, true> older{0, -30, 30, "vr_immersive_scale_x"};
    Settings::Setting<s32, true> newer{0, -100, 100, "vr_immersive_scale_x"};
    for (const s32 saved : {-100, -80, -35, -30, -15, 0, 25, 30, 35, 100}) {
        newer.SetValue(saved);
        older.SetValue(newer.GetValue()); // the older build reads the file
        newer.SetValue(older.GetValue()); // and saves it; this one reads it again
        INFO("saved " << saved);
        if (saved >= -30 && saved <= 30) {
            REQUIRE(newer.GetValue() == saved);
            REQUIRE(SqueezeForScale(newer.GetValue()) == OldSqueeze(older.GetValue()));
        } else {
            REQUIRE(newer.GetValue() == (saved < 0 ? -30 : 30)); // the older build's end
        }
        // Whatever the older build shows is what this one shows for the same value.
        REQUIRE(ScaleFactor(older.GetValue()) == 1.0f - OldSqueeze(older.GetValue()));
    }
}

// ---- AzaharFrame fovaxis: crop by default, the caps against warping, Render Margin per axis ----

// Field of View is "screen size only": below the game's own frame it crops (the world keeps its
// shape, the gains stay the base gain), up to 1.6x it draws the world to match; Stretch is
// fovscale's squeeze, bit for bit while caps are off.
TEST_CASE("FrameXr fovaxis: Field of View crops and keeps the true shape", "[frame_xr][fovaxis]") {
    const ViewFov view = Symmetric(50);
    int crops = 0;
    for (uint32_t level = 0; level <= kMaxWiderLevel; ++level) {
        for (int sx = -100; sx <= 100; sx += 5) {
            for (const int sy : {-100, -45, -15, 0, 10, 60, 100}) {
                Options o;
                o.wider = WiderFactor(level);
                o.squeeze = SqueezeForScale(sx);
                o.squeeze_y = SqueezeForScale(sy);
                o.scale_fov = 1.0f;
                o.caps = 1.0f;
                Options o0 = o;
                o0.squeeze = o0.squeeze_y = 0.0f;
                const HeadLocked z = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o0);
                const HeadLocked s = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o);
                INFO("level " << level << " scales " << sx << ", " << sy);
                REQUIRE(Finite(s));
                // The screen is the scale's size, whatever is drawn on it.
                REQUIRE(s.width() == Approx(z.width() * ScaleFactor(sx)).epsilon(1e-4));
                REQUIRE(s.height() == Approx(z.height() * ScaleFactor(sy)).epsilon(1e-4));
                const float bx = WiderFactor(level) * ScaleFactor(sx);
                const float by = WiderFactor(level) * ScaleFactor(sy);
                if (bx <= kFovFactorMax && by <= kFovFactorMax) {
                    // True shape: one scale both ways, the base gains, nothing stretched.
                    REQUIRE(s.stretch_x == Approx(1.0f).margin(1e-5));
                    REQUIRE(s.stretch_y == Approx(1.0f).margin(1e-5));
                    REQUIRE(PictureShape(s, kAspect3ds) == Approx(1.0f).epsilon(1e-4));
                    REQUIRE(s.gain_x == Approx(z.gain_x).epsilon(1e-4));
                    REQUIRE(s.gain == Approx(z.gain).epsilon(1e-4));
                    // The game's own frame (and the HUD in it) keeps its place on the screen:
                    // narrower crops it, never squeezes it.
                    REQUIRE(s.width() / s.factor == Approx(z.width() / z.factor).epsilon(1e-4));
                    REQUIRE(s.height() / s.factor_y ==
                            Approx(z.height() / z.factor_y).epsilon(1e-4));
                    crops += s.factor < kFactorMin;
                }
                // The note (Render Margin does not apply in Field of View: the scales alone):
                // exactly where Field of View cannot keep the true shape (1 % slack).
                const WarpAxes w = ScalesMayWarp(level, level, sx, sy, ScaleMode::FieldOfView);
                REQUIRE(w.x == (ScaleFactor(sx) > kFovFactorMax * 1.0101f));
                REQUIRE(w.y == (ScaleFactor(sy) > kFovFactorMax * 1.0101f));
            }
        }
    }
    std::printf("fovaxis: %d true-shape screens drawn as a crop\n", crops);
    REQUIRE(crops > 200);
}

// The caps: Field of View draws at most 1.6x per axis; Stretch keeps the axes within 1.5 of each
// other and each stretch within 1/1.6 to 1/0.6 (the gains the scales give within 0.6 to 1.6),
// drawing more or less instead, never below the game's own frame (the HUD stays). What they
// cannot take shows the note, on the axis that is past them.
TEST_CASE("FrameXr fovaxis: the caps against warping", "[frame_xr][fovaxis]") {
    REQUIRE(kFovFactorMax == 1.6f);
    REQUIRE(kMaxStretchRatio == 1.5f);
    REQUIRE(kScaleGainMin == 0.6f);
    REQUIRE(kScaleGainMax == 1.6f);
    int capped = 0, noted = 0;
    for (const uint32_t lx : {0u, 2u, 4u}) {
        for (const uint32_t ly : {0u, 3u, 4u}) {
            for (int sx = -100; sx <= 100; sx += 5) {
                for (int sy = -100; sy <= 100; sy += 5) {
                    const float fx = WiderFactor(lx), fy = WiderFactor(ly);
                    const float kx = ScaleFactor(sx), ky = ScaleFactor(sy);
                    INFO("levels " << lx << ", " << ly << " scales " << sx << ", " << sy);
                    for (const float fov : {0.0f, 1.0f}) {
                        const ScalePicture p = SplitScales(fx, kx, fy, ky, fov, 1.0f);
                        for (const AxisSplit* a : {&p.x, &p.y}) {
                            REQUIRE(std::isfinite(a->drawn));
                            REQUIRE(std::isfinite(a->stretch));
                            REQUIRE(a->drawn > 0.0f);
                            REQUIRE(a->stretch > 0.0f);
                            REQUIRE(a->drawn <= kFovFactorMax * 1.00001f);
                            // Stretch keeps the whole frame: never a crop.
                            REQUIRE(a->drawn >= (fov == 0.0f ? kFactorMin : kCropFactorMin));
                            // The gain the scale gives, within 0.6 to 1.6.
                            const float g = 1.0f / GainStretch(a->stretch, 1.0f);
                            REQUIRE(g >= kScaleGainMin * 0.9999f);
                            REQUIRE(g <= kScaleGainMax * 1.0001f);
                        }
                        // drawn x stretch is the factor x the size on each axis (x the screen
                        // left where Stretch trims the margin's padding at the draw's limit).
                        REQUIRE(p.x.drawn * p.x.stretch ==
                                Approx(fx * kx * p.x.screen).epsilon(1e-5));
                        REQUIRE(p.y.drawn * p.y.stretch ==
                                Approx(fy * ky * p.y.screen).epsilon(1e-5));
                        for (const AxisSplit* a : {&p.x, &p.y}) {
                            REQUIRE(a->screen <= 1.0f);
                            if (a->screen < 1.0f) {
                                REQUIRE(fov == 0.0f);
                                REQUIRE(a->drawn == Approx(kFovFactorMax));
                            }
                        }
                        // Stretch: the frame's split is the margin-free one (the owner's rule).
                        if (fov == 0.0f) {
                            const ScalePicture f = SplitScales(1.0f, kx, 1.0f, ky, 0.0f, 1.0f);
                            REQUIRE(p.x.stretch == f.x.stretch);
                            REQUIRE(p.y.stretch == f.y.stretch);
                            REQUIRE(p.warp_x == f.warp_x);
                            REQUIRE(p.warp_y == f.warp_y);
                        }
                        const float ratio = std::abs(std::log(p.x.stretch / p.y.stretch));
                        const bool past_ratio = ratio > std::log(kMaxStretchRatio) + 0.0101f;
                        const auto out = [](float r) {
                            return std::log(r) < std::log(1.0f / kScaleGainMax) - 0.0101f ||
                                   std::log(r) > std::log(1.0f / kScaleGainMin) + 0.0101f;
                        };
                        if (fov == 0.0f) {
                            // Inside the caps: Stretch exactly as before.
                            const ScalePicture none = SplitScales(fx, kx, fy, ky, 0.0f, 0.0f);
                            const bool inside =
                                std::abs(std::log(kx / ky)) <= std::log(kMaxStretchRatio) &&
                                !out(kx) && !out(ky) &&
                                std::log(kx) >= std::log(1.0f / kScaleGainMax) &&
                                std::log(kx) <= std::log(1.0f / kScaleGainMin) &&
                                std::log(ky) >= std::log(1.0f / kScaleGainMax) &&
                                std::log(ky) <= std::log(1.0f / kScaleGainMin);
                            if (inside) {
                                REQUIRE(p.x.drawn == none.x.drawn);
                                REQUIRE(p.x.stretch == none.x.stretch);
                                REQUIRE(p.y.drawn == none.y.drawn);
                                REQUIRE(p.y.stretch == none.y.stretch);
                                REQUIRE(!p.warp_x);
                                REQUIRE(!p.warp_y);
                            } else {
                                capped++;
                            }
                            // Past the ratio only where a frame's draw is at a limit (the
                            // margin-free split: the margins go around it).
                            if (past_ratio) {
                                const ScalePicture f = SplitScales(1.0f, kx, 1.0f, ky, 0.0f, 1.0f);
                                const auto at_limit = [](float d) {
                                    return d <= kFactorMin * 1.0001f ||
                                           d >= kFovFactorMax * 0.9999f;
                                };
                                REQUIRE((at_limit(f.x.drawn) || at_limit(f.y.drawn)));
                            }
                        }
                        // The note: past what the caps took, on that axis.
                        if (past_ratio || out(p.x.stretch) || out(p.y.stretch)) {
                            REQUIRE((p.warp_x || p.warp_y));
                            noted++;
                        }
                        if (out(p.x.stretch)) {
                            REQUIRE(p.warp_x);
                        }
                        if (out(p.y.stretch)) {
                            REQUIRE(p.warp_y);
                        }
                        // ScalesMayWarp is this split with the caps on (in Stretch: Field of
                        // View draws without the margins, the crop test checks it).
                        if (fov == 0.0f) {
                            const WarpAxes w = ScalesMayWarp(lx, ly, sx, sy, ScaleMode::Stretch);
                            REQUIRE(w.x == p.warp_x);
                            REQUIRE(w.y == p.warp_y);
                        }
                    }
                }
            }
        }
    }
    std::printf("fovaxis caps: %d Stretch settings capped, %d past what the caps take\n", capped,
                noted);
    REQUIRE(capped > 1000);
    REQUIRE(noted > 100);
    // Continuous in the caps' weight (mode 2's menu rule eases them): no jump.
    for (const int sx : {-100, -60, -30, 0, 50, 100}) {
        for (const int sy : {-100, -15, 0, 30, 100}) {
            ScalePicture last = SplitScales(1.4f, ScaleFactor(sx), 1.2f, ScaleFactor(sy), 0.0f, 0.0f);
            for (int i = 1; i <= 400; ++i) {
                const ScalePicture now =
                    SplitScales(1.4f, ScaleFactor(sx), 1.2f, ScaleFactor(sy), 0.0f, i / 400.0f);
                INFO("scales " << sx << ", " << sy << " weight " << i / 400.0f);
                REQUIRE(std::abs(now.x.drawn - last.x.drawn) < 0.02f);
                REQUIRE(std::abs(now.y.drawn - last.y.drawn) < 0.02f);
                last = now;
            }
        }
    }
}

// hold.py ports SplitScales and GainStretch for its gain prediction: the same table here and in
// its selftest (base gain 1, Render Margin 1.4x both ways, which applies with Stretch only).
TEST_CASE("FrameXr fovaxis: hold.py's table", "[frame_xr][fovaxis]") {
    struct Row {
        int sx, sy;
        bool fov;
        float gx, gy;
    };
    const Row rows[] = {
        {-30, -15, false, 1.0f / 0.7f, 1.0f / 0.85f}, // Mario Kart 7's preset: inside the caps
        {-30, 0, false, 1.0f / 0.7f, 1.0f},
        {-30, -15, true, 1.0f, 1.0f}, // crops: true shape
        {-100, -100, true, 1.0f, 1.0f},
        {0, 0, false, 1.0f, 1.0f},
        {30, 0, true, 1.0f, 1.0f},          // the scales alone: 1.3x drawn
        {70, 0, true, 1.6f / 1.7f, 1.0f},   // 1.7x wanted, 1.6x drawn, the rest a stretch
        {100, 100, true, 0.8f, 0.8f},       // 2x wanted: 1.6x drawn, stretch 1.25
        {-100, 0, false, 1.6f, 1.6f},       // the frame cannot draw less: the tall axis 1.6x
        {100, 0, false, 1.0f / 1.5f, 1.0f}, // the frame drawn 1.33x: the ratio 1.5
        {-30, 30, false, 1.0f / 0.7f, 1.0f / 1.05f},
        {-100, -100, false, 1.6f, 1.6f},
    };
    const ViewFov view = Symmetric(60); // fits: the base gain is 1, no gain limit near

    for (const Row& r : rows) {
        Options o;
        o.wider = MarginFactor(kMaxWiderLevel, r.fov ? ScaleMode::FieldOfView : ScaleMode::Stretch);
        o.squeeze = SqueezeForScale(r.sx);
        o.squeeze_y = SqueezeForScale(r.sy);
        o.scale_fov = r.fov ? 1.0f : 0.0f;
        o.caps = 1.0f;
        Options o0 = o;
        o0.squeeze = o0.squeeze_y = 0.0f;
        const HeadLocked z = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o0);
        const HeadLocked s = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o);
        INFO("scales " << r.sx << ", " << r.sy << (r.fov ? " Field of View" : " Stretch"));
        REQUIRE(s.gain_x / z.gain_x == Approx(r.gx).epsilon(1e-4));
        REQUIRE(s.gain / z.gain == Approx(r.gy).epsilon(1e-4));
    }
    // Stretch at -30 % and +30 % (the old range's widest): the ratio 1.86 capped to 1.5.
    const ScalePicture p = SplitScales(1.4f, 0.7f, 1.4f, 1.3f, 0.0f, 1.0f);
    REQUIRE(p.x.drawn == Approx(1.4f)); // the margin around the frame, which keeps its draw
    // the frame draws 1.24x up and down; with the margin that is 1.73x: trimmed to 1.6x
    REQUIRE(p.y.drawn == Approx(kFovFactorMax));
    REQUIRE(p.y.screen == Approx(kFovFactorMax / (1.4f * 1.3f / 1.05f)));
    REQUIRE(p.y.stretch / p.x.stretch == Approx(1.5f).epsilon(1e-4));
    REQUIRE(!p.warp_x);
    REQUIRE(!p.warp_y);
}

// The menu dump's notes are what ScalesMayWarp gives for its settings.
TEST_CASE("FrameXr fovaxis: May Warp for the menu dump's settings", "[frame_xr][fovaxis]") {
    // Field of View: the scales alone (Render Margin applies with Stretch): +70 % wants 1.7x,
    // +60 % is 1.6x, whatever the margins.
    REQUIRE(ScalesMayWarp(4, 1, 70, 0, ScaleMode::FieldOfView) == WarpAxes{true, false});
    REQUIRE(ScalesMayWarp(4, 1, 60, 0, ScaleMode::FieldOfView) == WarpAxes{false, false});
    REQUIRE(ScalesMayWarp(4, 1, 30, 0, ScaleMode::FieldOfView) == WarpAxes{false, false});
    REQUIRE(ScalesMayWarp(4, 1, 0, 0, ScaleMode::FieldOfView) == WarpAxes{false, false});
    // Stretch, Render Margin 1.4x: +30 % on one axis, 0 % on the other is within the caps.
    REQUIRE(ScalesMayWarp(4, 1, 30, 0, ScaleMode::Stretch) == WarpAxes{false, false});
    // Stretch at -100 % and +100 %: the narrow axis past the gains' room (the caps act on the
    // game's own frame, which cannot be drawn smaller; the tall one draws more instead).
    REQUIRE(ScalesMayWarp(4, 4, -100, 100, ScaleMode::Stretch) == WarpAxes{true, false});
    // Mario Kart 7's preset in Stretch (Render Margin 1.3x): no note.
    REQUIRE(ScalesMayWarp(3, 3, -30, -15, ScaleMode::Stretch) == WarpAxes{false, false});
    // Crops never warp.
    REQUIRE(ScalesMayWarp(0, 0, -100, -100, ScaleMode::FieldOfView) == WarpAxes{false, false});
    // Render Margin Off with Field of View: +60 % is still 1.6x.
    REQUIRE(ScalesMayWarp(0, 0, 60, 60, ScaleMode::FieldOfView) == WarpAxes{false, false});
    REQUIRE(ScalesMayWarp(0, 0, 70, 0, ScaleMode::FieldOfView) == WarpAxes{true, false});
}

// Render Margin per axis: the screen sized with each axis's own margin, the game drawing each,
// one picture scale both ways; the same margins on both axes give every build before's numbers.
TEST_CASE("FrameXr fovaxis: Render Margin per axis on the screen", "[frame_xr][fovaxis]") {
    std::mt19937 rng(20261116u);
    std::uniform_int_distribution<int> level(0, 4), scale(-20, 20), pick(0, 9);
    std::uniform_real_distribution<float> fov(30.0f, 90.0f), half(35.0f, 60.0f),
        size(kMinSize, kMaxSize), aspect(1.3f, 2.4f), menu(0.0f, 1.0f);
    for (int i = 0; i < 2000; ++i) {
        const uint32_t lx = static_cast<uint32_t>(level(rng)), ly = static_cast<uint32_t>(level(rng));
        const int sx = scale(rng) * 5, sy = scale(rng) * 5;
        Placement p;
        p.size = size(rng);
        const float a = aspect(rng), game_fov = fov(rng);
        const ViewFov view = Symmetric(half(rng));
        INFO("case " << i << " levels " << lx << ", " << ly << " scales " << sx << ", " << sy);
        for (const float share : {0.0f, 1.0f}) {
            Options o;
            o.wider = WiderFactor(lx);
            o.wider_y = WiderFactor(ly);
            o.squeeze = SqueezeForScale(sx);
            o.squeeze_y = SqueezeForScale(sy);
            o.scale_fov = share;
            o.caps = 1.0f;
            // Equal levels: wider_y given or not, the same numbers (bit for bit).
            if (lx == ly) {
                Options same = o;
                same.wider_y = 0.0f;
                const HeadLocked a1 = SolveHeadLocked(p, game_fov, a, view, 0.0f, o);
                const HeadLocked a2 = SolveHeadLocked(p, game_fov, a, view, 0.0f, same);
                REQUIRE(a1.factor == a2.factor);
                REQUIRE(a1.factor_y == a2.factor_y);
                REQUIRE(a1.tan_half_y == a2.tan_half_y);
                REQUIRE(a1.aspect == a2.aspect);
                REQUIRE(a1.gain == a2.gain);
                REQUIRE(a1.gain_x == a2.gain_x);
            }
            Options o0 = o;
            o0.squeeze = o0.squeeze_y = 0.0f;
            const HeadLocked z = SolveHeadLocked(p, game_fov, a, view, 0.0f, o0);
            const HeadLocked s = SolveHeadLocked(p, game_fov, a, view, 0.0f, o);
            REQUIRE(Finite(s));
            REQUIRE(Finite(z));
            // At 0 %: the game draws each axis's own margin, and the picture keeps one scale.
            REQUIRE(z.factor == Approx(WiderFactor(lx)).epsilon(1e-5));
            REQUIRE(z.factor_y == Approx(WiderFactor(ly)).epsilon(1e-5));
            REQUIRE(PictureShape(z, a) == Approx(1.0f).epsilon(1e-4));
            REQUIRE(z.stretch_x == 1.0f);
            REQUIRE(z.stretch_y == 1.0f);
            // The game's own frame has its own shape on the screen at any margins (the world
            // keeps its shape): its width over height is the game's.
            const float frame_w = z.width() / z.factor, frame_h = z.height() / z.factor_y;
            REQUIRE(frame_w / frame_h == Approx(a).epsilon(1e-4));
            // The gains the same both ways at 0 % (the base gain).
            REQUIRE(z.gain_x == Approx(z.gain).epsilon(1e-5));
            // The scaled screen: the scale's size (in Stretch, times what is left of the
            // margin's padding where it is trimmed at 1.6x: SplitScales' `screen`).
            const ScalePicture sp =
                SplitScales(WiderFactor(lx), ScaleFactor(sx), WiderFactor(ly), ScaleFactor(sy),
                            share, 1.0f);
            REQUIRE(s.width() == Approx(z.width() * ScaleFactor(sx) * sp.x.screen).epsilon(1e-4));
            REQUIRE(s.height() == Approx(z.height() * ScaleFactor(sy) * sp.y.screen).epsilon(1e-4));
            REQUIRE(s.gain >= kMinGain);
            REQUIRE(s.gain <= kMaxGain);
            REQUIRE(s.gain_x >= kMinGain);
            REQUIRE(s.gain_x <= kMaxGain);
        }
        // Mode 2's menu rule with unequal margins: the screen sized with the smaller margin on
        // both axes at full weight, so the game's own frame (drawn 1x both ways) is not stretched.
        if (lx != ly && pick(rng) < 5) {
            const float w = menu(rng);
            const float fx = WiderFactor(lx), fy = WiderFactor(ly);
            const SizeFactors sz = MenuSizeFactors(fx, fy, w);
            Options o;
            o.size_wider = sz.x;
            o.size_wider_y = sz.y;
            o.wider = MenuExact::Factor(fx, w);
            o.wider_y = MenuExact::Factor(fy, w);
            const HeadLocked s = SolveHeadLocked(p, game_fov, a, view, 0.0f, o);
            REQUIRE(Finite(s));
            REQUIRE(PictureShape(s, a) == Approx(1.0f).epsilon(1e-4)); // never stretched
            Options full = o;
            const SizeFactors one = MenuSizeFactors(fx, fy, 1.0f);
            full.size_wider = one.x;
            full.size_wider_y = one.y;
            full.wider = full.wider_y = 1.0f;
            const HeadLocked m = SolveHeadLocked(p, game_fov, a, view, 0.0f, full);
            REQUIRE(m.factor == Approx(1.0f));
            REQUIRE(m.factor_y == Approx(1.0f).epsilon(1e-5));
            REQUIRE(m.width() / m.height() == Approx(a).epsilon(1e-4)); // the frame's shape
        }
    }
    // MenuSizeFactors: equal margins keep the padded size (as before); unequal ease to the
    // smaller one on both axes.
    REQUIRE(MenuSizeFactors(1.4f, 1.4f, 0.7f).x == 1.4f);
    REQUIRE(MenuSizeFactors(1.4f, 1.4f, 0.7f).y == 1.4f);
    REQUIRE(MenuSizeFactors(1.4f, 1.1f, 0.0f).x == 1.4f);
    REQUIRE(MenuSizeFactors(1.4f, 1.1f, 1.0f).x == Approx(1.1f));
    REQUIRE(MenuSizeFactors(1.4f, 1.1f, 1.0f).y == Approx(1.1f));
    REQUIRE(MenuSizeFactors(1.4f, 1.1f, 0.5f).x == Approx(1.25f));
    // The toast of the gesture.
    REQUIRE(MarginToast(2, 2) == "Render Margin: 1.2x");
    REQUIRE(MarginToast(0, 0) == "Render Margin: Off");
    REQUIRE(MarginToast(4, 1) == "Render Margin: H 1.4x, V 1.1x");
    REQUIRE(MarginToast(0, 3) == "Render Margin: H Off, V 1.3x");
}

// The solver over the whole ranges with the caps on (what immersive.cpp gives it): finite,
// positive, gains within their limits and the scales' part of them within 0.6 to 1.6.
TEST_CASE("FrameXr fovaxis: the solver over the ranges", "[frame_xr][fovaxis]") {
    std::mt19937 rng(20261117u);
    std::uniform_int_distribution<int> level(0, 4), scale(-20, 20), pick(0, 9);
    std::uniform_real_distribution<float> fov(30.0f, 110.0f), live(10.0f, 150.0f),
        half(35.0f, 60.0f), size(kMinSize, kMaxSize), aspect(1.2f, 3.0f), menu(0.0f, 1.0f);
    int n = 0;
    for (int i = 0; i < 6000; ++i) {
        Options o;
        const float fx = WiderFactor(static_cast<uint32_t>(level(rng)));
        const float fy = WiderFactor(static_cast<uint32_t>(level(rng)));
        o.wider = fx;
        o.wider_y = fy == fx ? 0.0f : fy;
        const int sx = scale(rng) * 5, sy = scale(rng) * 5;
        o.squeeze = SqueezeForScale(sx);
        o.squeeze_y = SqueezeForScale(sy);
        o.live_fov_y_deg = pick(rng) < 3 ? 0.0f : live(rng);
        const bool stretch = pick(rng) < 5;
        float w = pick(rng) == 0 ? menu(rng) : 0.0f;
        if (w > 0.0f) {
            const SizeFactors sz = MenuSizeFactors(fx, fy, w);
            o.size_wider = sz.x;
            o.size_wider_y = o.wider_y > 0.0f ? sz.y : 0.0f;
            o.wider = MenuExact::Factor(fx, w);
            if (o.wider_y > 0.0f) {
                o.wider_y = MenuExact::Factor(fy, w);
            }
        }
        o.scale_fov = stretch ? 0.0f : 1.0f - w;
        o.caps = 1.0f - w;
        Placement p;
        p.size = size(rng);
        const ViewFov view = Symmetric(half(rng));
        const HeadLocked s = SolveHeadLocked(p, fov(rng), aspect(rng), view, 0.0f, o);
        INFO("case " << i << " scales " << sx << ", " << sy << " margins " << fx << ", " << fy);
        REQUIRE(Finite(s));
        REQUIRE(s.width() > 0.0f);
        REQUIRE(s.height() > 0.0f);
        REQUIRE(s.factor > 0.0f);
        REQUIRE(s.factor_y > 0.0f);
        REQUIRE(s.factor <= kFovFactorMax * 1.0001f);
        REQUIRE(s.factor_y <= kFovFactorMax * 1.0001f);
        REQUIRE(s.gain >= kMinGain);
        REQUIRE(s.gain <= kMaxGain);
        REQUIRE(s.gain_x >= kMinGain);
        REQUIRE(s.gain_x <= kMaxGain);
        if (w == 0.0f) {
            // The scales' part of each gain within 0.6 to 1.6.
            const float gx = 1.0f / GainStretch(s.stretch_x, 1.0f);
            const float gy = 1.0f / GainStretch(s.stretch_y, 1.0f);
            REQUIRE(gx >= kScaleGainMin * 0.9999f);
            REQUIRE(gx <= kScaleGainMax * 1.0001f);
            REQUIRE(gy >= kScaleGainMin * 0.9999f);
            REQUIRE(gy <= kScaleGainMax * 1.0001f);
        }
        n++;
    }
    REQUIRE(n == 6000);
}

// ---- AzaharFrame fovaxis: Render Margin with Stretch only, the frame kept, Fill View ----

TEST_CASE("FrameXr fovaxis: Render Margin applies with Stretch only", "[frame_xr][fovaxis]") {
    for (uint32_t level = 0; level <= kMaxWiderLevel; ++level) {
        REQUIRE(MarginFactor(level, ScaleMode::Stretch) == WiderFactor(level));
        REQUIRE(MarginFactor(level, ScaleMode::FieldOfView) == kFactorMin);
    }
    // Field of View: the scales alone decide what is drawn; Render Margin changes nothing.
    const ViewFov view = Symmetric(50);
    for (const int sx : {-100, -30, 0, 40, 100}) {
        Options o;
        o.squeeze = SqueezeForScale(sx);
        o.squeeze_y = SqueezeForScale(-sx / 2);
        o.scale_fov = 1.0f;
        o.caps = 1.0f;
        o.wider = MarginFactor(0, ScaleMode::FieldOfView);
        const HeadLocked off = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o);
        o.wider = MarginFactor(4, ScaleMode::FieldOfView);
        const HeadLocked m = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, o);
        REQUIRE(m.factor == off.factor);
        REQUIRE(m.factor_y == off.factor_y);
        REQUIRE(m.width() == off.width());
        REQUIRE(m.gain_x == off.gain_x);
        // +40 % draws the 1.4x the default Render Margin gave (the presets' value).
        if (sx == 40) {
            REQUIRE(m.factor == Approx(1.4f));
        }
    }
}

namespace {
// The game's own frame (where the HUD is drawn) on view: half its width and height over the
// screen's distance (the quad over what the game draws on it).
struct FrameTans {
    float x;
    float y;
};
FrameTans GameFrame(const HeadLocked& s) {
    return {s.tan_half_y * s.aspect / s.factor, s.tan_half_y / s.factor_y};
}
ViewFov Lopsided() { // canted, uneven, as a headset's eyes are
    ViewFov v;
    v.left = -52.0f * kDegToRad;
    v.right = 44.0f * kDegToRad;
    v.up = 43.0f * kDegToRad;
    v.down = -53.0f * kDegToRad;
    v.outer_left = -56.0f * kDegToRad;
    v.outer_right = 56.0f * kDegToRad;
    v.outer_up = 43.0f * kDegToRad;
    v.outer_down = -53.0f * kDegToRad;
    return v;
}
} // namespace

// The owner's rule: a larger Render Margin never moves or resizes the game's own frame (and the
// HUD on it) on view, it only adds picture around it. The frame's four corners are at
// (pitch, +-atan(x)), (+-atan(y)): the pitch, the yaw and both tangents must stay, and so must
// the gains. The view-fill cap is taken on the frame, so wide cameras (110 degrees) too.
TEST_CASE("FrameXr fovaxis: a Render Margin step never moves the game's frame",
          "[frame_xr][fovaxis]") {
    int checked = 0, at_caps = 0, trimmed = 0;
    for (const ViewFov& view : {Symmetric(40), Symmetric(55), Lopsided()}) {
        for (const float eye_shift : {0.0f, 0.0107f}) { // mode 3, mode 2 at 3 m
            for (const float cam : {30.0f, 55.0f, 90.0f, 110.0f}) {
                for (const float size : {0.6f, 1.0f, 2.5f}) {
                    for (const int sx : {-100, -30, 0, 25, 100}) {
                        for (const int sy : {-60, 0, 40}) {
                            for (const ScaleMode mode :
                                 {ScaleMode::Stretch, ScaleMode::FieldOfView}) {
                                for (int axis = 0; axis < 2; ++axis) {
                                    Placement p;
                                    p.size = size;
                                    p.pitch_deg = -8.0f;
                                    const auto solve = [&](uint32_t level) {
                                        const uint32_t lx = axis == 0 ? level : 2;
                                        const uint32_t ly = axis == 1 ? level : 2;
                                        Options o;
                                        o.wider = MarginFactor(lx, mode);
                                        const float fy = MarginFactor(ly, mode);
                                        o.wider_y = fy == o.wider ? 0.0f : fy;
                                        o.squeeze = SqueezeForScale(sx);
                                        o.squeeze_y = SqueezeForScale(sy);
                                        o.scale_fov = mode == ScaleMode::FieldOfView ? 1.0f : 0.0f;
                                        o.caps = 1.0f;
                                        return SolveHeadLocked(p, cam, kAspect3ds, view, eye_shift,
                                                               o);
                                    };
                                    const HeadLocked off = solve(0);
                                    const FrameTans f0 = GameFrame(off);
                                    for (uint32_t level = 1; level <= kMaxWiderLevel; ++level) {
                                        const uint32_t lx = axis == 0 ? level : 2;
                                        const uint32_t ly = axis == 1 ? level : 2;
                                        const WarpAxes w = ScalesMayWarp(lx, ly, sx, sy, mode);
                                        at_caps += w.x || w.y; // checked all the same
                                        const HeadLocked s = solve(level);
                                        const FrameTans f = GameFrame(s);
                                        INFO("cam " << cam << " size " << size << " scales "
                                                    << sx << ", " << sy << " axis " << axis
                                                    << " level " << level << " mode "
                                                    << static_cast<int>(mode));
                                        REQUIRE(f.x == Approx(f0.x).epsilon(1e-5));
                                        REQUIRE(f.y == Approx(f0.y).epsilon(1e-5));
                                        REQUIRE(s.pitch_rad == Approx(off.pitch_rad).margin(1e-7));
                                        REQUIRE(s.yaw_rad == off.yaw_rad);
                                        REQUIRE(s.gain == Approx(off.gain).epsilon(1e-5));
                                        REQUIRE(s.gain_x == Approx(off.gain_x).epsilon(1e-5));
                                        // The padding goes outward: the screen never shrinks on
                                        // that axis (it stops growing where it is trimmed).
                                        if (mode == ScaleMode::Stretch) {
                                            REQUIRE((axis == 0 ? s.width() >= off.width() * 0.99999f
                                                               : s.height() >= off.height() * 0.99999f));
                                            trimmed += (axis == 0 ? s.width() : s.height()) <
                                                       (axis == 0 ? off.width() : off.height()) *
                                                           WiderFactor(level) * 0.9999f;
                                        }
                                        checked++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    std::printf("fovaxis frame: %d margin steps keep the frame (%d of them past the caps, %d with "
                "the padding trimmed at 1.6x)\n",
                checked, at_caps, trimmed);
    REQUIRE(checked > 3000);
}

namespace {
// Fill View's result, solved: the screen's half width and height over its distance and the
// cover it was filled to (across at kFillAcrossShare of it).
struct FilledScreen {
    Filled f;
    HeadLocked s;
    Cover cover;
};
Options FillOptions(ScaleMode mode, uint32_t level) {
    Options o;
    o.wider = MarginFactor(level, mode);
    o.scale_fov = mode == ScaleMode::FieldOfView ? 1.0f : 0.0f;
    o.caps = 1.0f;
    return o;
}
FilledScreen FillAndSolve(const Placement& p, float cam, float aspect, const ViewFov& view,
                          float eye_shift, ScaleMode mode, uint32_t level, int scale_y = 20) {
    Options o = FillOptions(mode, level);
    o.squeeze = SqueezeForScale(-35); // whatever Horizontal Scale was: Fill View sets it
    o.squeeze_y = SqueezeForScale(scale_y);
    FilledScreen r;
    r.f = FillView(p, cam, aspect, view, eye_shift, scale_y, o);
    Placement q = p;
    q.size = r.f.size;
    o.squeeze = SqueezeForScale(r.f.scale_x);
    o.squeeze_y = SqueezeForScale(r.f.scale_y);
    r.s = SolveHeadLocked(q, cam, aspect, view, eye_shift, o);
    r.cover = DefaultCover(view, r.s.pitch_rad, eye_shift);
    return r;
}
} // namespace

// 1.2.0 (session 651, triage item 2): Fill View fills across only. The width is exactly
// kFillAcrossShare of what covers either eye's view; Horizontal Scale is kFillScalePercent or
// the largest step under it the width allows, not warping where a step that does not warp
// exists; the size makes up the rest (at least 1); Vertical Scale and the height are the
// player's, untouched.
TEST_CASE("FrameXr fovaxis: Fill View fills across only", "[frame_xr][fovaxis]") {
    int n = 0, at_rule = 0, lowered = 0, warps = 0, partial = 0;
    float least_share_y = 10.0f, most_share_y = 0.0f;
    for (const ViewFov& view : {Symmetric(40), Symmetric(55), Lopsided()}) {
        for (const float eye_shift : {0.0f, 0.0107f, 0.086f}) { // mode 3; mode 2 at 3 m, 0.37 m
            for (const float pitch : {-20.0f, -9.21f, 0.0f, 10.0f}) {
                for (const float cam : {30.0f, 45.0f, 60.0f, 90.0f}) {
                    for (const float aspect : {kAspect3ds, 4.0f / 3.0f, 16.0f / 9.0f}) {
                        for (const ScaleMode mode : {ScaleMode::Stretch, ScaleMode::FieldOfView}) {
                            for (const uint32_t level : {0u, 4u}) {
                                for (const int scale_y : {-40, 0, 45, 100}) {
                                    Placement p;
                                    p.pitch_deg = pitch;
                                    p.distance = eye_shift > 0.05f ? 0.37f : 3.0f;
                                    p.size = 1.7f; // whatever it was: Fill View sets it
                                    const FilledScreen r = FillAndSolve(
                                        p, cam, aspect, view, eye_shift, mode, level, scale_y);
                                    INFO("pitch " << pitch << " cam " << cam << " aspect "
                                                  << aspect << " eye " << eye_shift << " mode "
                                                  << static_cast<int>(mode) << " level " << level
                                                  << " V " << scale_y << " -> H " << r.f.scale_x
                                                  << " size " << r.f.size);
                                    // Vertical Scale untouched.
                                    REQUIRE(r.f.scale_y == scale_y);
                                    REQUIRE(r.f.scale_x % kScaleStepPercent == 0);
                                    REQUIRE(r.f.scale_x <= kFillScalePercent);
                                    REQUIRE(Finite(r.s));
                                    const Options o = FillOptions(mode, level);
                                    Placement one = p;
                                    one.size = 1.0f;
                                    const auto x_at = [&](int sx) {
                                        Options oo = o;
                                        oo.squeeze = SqueezeForScale(sx);
                                        oo.squeeze_y = SqueezeForScale(scale_y);
                                        const HeadLocked h =
                                            SolveHeadLocked(one, cam, aspect, view, eye_shift, oo);
                                        return h.tan_half_y * h.aspect;
                                    };
                                    const float target = kFillAcrossShare * r.cover.tan_x;
                                    const bool warp_x =
                                        ScalesMayWarp(level, level, r.f.scale_x, scale_y, mode).x;
                                    if (!r.f.covers) {
                                        REQUIRE((r.f.size == kMaxSize || r.f.size == kMinSize));
                                        partial++;
                                        continue;
                                    }
                                    // The width exactly at the share, never by shrinking (size
                                    // at least 1).
                                    REQUIRE(r.s.tan_half_y * r.s.aspect ==
                                            Approx(target).epsilon(1e-4));
                                    REQUIRE(r.f.size >= 1.0f - 1e-5f);
                                    // The height is the player's: the screen at Fill View's
                                    // size with Vertical Scale as it was.
                                    Options oy = o;
                                    oy.squeeze = SqueezeForScale(r.f.scale_x);
                                    oy.squeeze_y = SqueezeForScale(scale_y);
                                    REQUIRE(r.s.tan_half_y ==
                                            Approx(SolveHeadLocked(one, cam, aspect, view,
                                                                   eye_shift, oy)
                                                       .tan_half_y *
                                                   r.f.size)
                                                .epsilon(1e-4));
                                    REQUIRE(r.f.share_y ==
                                            Approx(r.s.tan_half_y / r.cover.tan_y).epsilon(1e-3));
                                    // The largest: the next step up is past the rule, wider
                                    // than the share, or warps where this one does not.
                                    const int up = r.f.scale_x + kScaleStepPercent;
                                    if (up <= kFillScalePercent &&
                                        x_at(up) > x_at(r.f.scale_x) * (1.0f + 1e-5f)) {
                                        REQUIRE((x_at(up) > target * (1.0f - 1e-5f) ||
                                                 (!warp_x &&
                                                  ScalesMayWarp(level, level, up, scale_y, mode)
                                                      .x)));
                                    }
                                    if (warp_x) {
                                        // Only when no step at or under the width avoids it.
                                        for (int s = -kMaxScalePercent; s <= kFillScalePercent;
                                             s += kScaleStepPercent) {
                                            if (x_at(s) <= target) {
                                                REQUIRE(ScalesMayWarp(level, level, s, scale_y,
                                                                      mode)
                                                            .x);
                                            }
                                        }
                                        warps++;
                                    }
                                    at_rule += r.f.scale_x == kFillScalePercent;
                                    lowered += r.f.scale_x < kFillScalePercent;
                                    least_share_y = std::min(least_share_y, r.f.share_y);
                                    most_share_y = std::max(most_share_y, r.f.share_y);
                                    n++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    std::printf("Fill View across: %d cases exact (%d at +%d%%, %d lower, %d warp across with "
                "no other choice), %d partial; the height %.0f%% to %.0f%% of the view's\n",
                n, at_rule, kFillScalePercent, lowered, warps, partial, least_share_y * 100.0f,
                most_share_y * 100.0f);
    REQUIRE(n > 1000);
    REQUIRE(at_rule > n / 2); // the rule itself is the usual result
}

// Symmetric view as wide as the Frame's (either eye about 59 degrees to the side), level, mode
// 3, Field of View: the screen's half width is kFillAcrossShare of the cover's (the view's edge
// plus kFillMarginDeg), Horizontal Scale +50 % (no warp: Field of View draws up to 1.6x), and
// the height exactly as Vertical Scale had it. (A narrow view with a wide camera needs less
// across than the screen at its own scale: the scale steps down instead, the size stays 1 up.)
TEST_CASE("FrameXr fovaxis: Fill View's width", "[frame_xr][fovaxis]") {
    for (const float half : {55.0f, 60.0f}) {
        for (const int scale_y : {0, 75}) {
            Placement p;
            p.pitch_deg = 0.0f;
            const FilledScreen r = FillAndSolve(p, 45.0f, kAspect3ds, Symmetric(half), 0.0f,
                                                ScaleMode::FieldOfView, 0, scale_y);
            REQUIRE(r.f.covers);
            REQUIRE(r.f.scale_x == kFillScalePercent);
            REQUIRE(r.f.scale_y == scale_y);
            REQUIRE(!ScalesMayWarp(0, 0, r.f.scale_x, 0, ScaleMode::FieldOfView).x);
            const float x = r.s.tan_half_y * r.s.aspect;
            REQUIRE(x == Approx(kFillAcrossShare *
                                std::tan((half + kFillMarginDeg) * kDegToRad))
                             .epsilon(1e-4));
            REQUIRE(r.f.size > 1.0f);
        }
    }
}

// Session 651's three Fill View presses, redone (triage item 2): in Field of View the quad's
// width is the size times 1 + Horizontal Scale / 100 (ScaleFactor), so the old press's result
// (width exactly the cover) gives the cover, and the new one takes kFillAcrossShare of it at
// +50 %. The owner's settled screens are close; the press is a starting point.
TEST_CASE("FrameXr fovaxis: Fill View against session 651", "[frame_xr][fovaxis]") {
    struct Press {
        const char* game;
        float old_size;
        int old_x;
        float owner_size;
        int owner_x;
    };
    for (const Press& press : {Press{"OoT", 1.646f, 60, 1.12f, 40},
                               Press{"OoT", 1.827f, 50, 1.12f, 40},
                               Press{"0004000000125600", 1.533f, 55, 1.39f, 20},
                               Press{"MK7", 2.273f, 50, 2.05f, 55}}) {
        const float cover = press.old_size * ScaleFactor(press.old_x);
        const float size = kFillAcrossShare * cover / ScaleFactor(kFillScalePercent);
        const float owner = press.owner_size * ScaleFactor(press.owner_x);
        std::printf("Fill View, %s: was size %.3f at %+d%%; now size %.3f at %+d%% (width %.2f "
                    "of the old); the owner's size %.2f at %+d%% (width %.2f of the old)\n",
                    press.game, press.old_size, press.old_x, size, kFillScalePercent,
                    kFillAcrossShare, press.owner_size, press.owner_x, owner / cover);
        REQUIRE(size >= 1.0f);
        REQUIRE(size <= kMaxSize);
        // Within a third of the owner's size either way.
        REQUIRE(std::abs(std::log(size / press.owner_size)) < std::log(1.34f));
    }
}

// Fill View sets values once; they are ordinary after: a D-pad or menu step moves from them in
// the pressed direction (they are on the grid, so the first step is one step), and the grip's
// resize goes on from the filled size. Both axes, both directions, both modes.
TEST_CASE("FrameXr fovaxis: adjusting after Fill View", "[frame_xr][fovaxis]") {
    for (const ScaleMode mode : {ScaleMode::Stretch, ScaleMode::FieldOfView}) {
        for (const float cam : {45.0f, 60.0f}) {
            Placement p;
            p.pitch_deg = -9.21f;
            const FilledScreen r = FillAndSolve(p, cam, kAspect3ds, Lopsided(), 0.0107f, mode, 4);
            for (const int scale : {r.f.scale_x, r.f.scale_y}) {
                REQUIRE(ScalePercent(scale) == scale); // shown as it is
                if (scale < kMaxScalePercent) {
                    REQUIRE(SteppedScale(scale, 1) == scale + kScaleStepPercent);
                }
                if (scale > -kMaxScalePercent) {
                    REQUIRE(SteppedScale(scale, -1) == scale - kScaleStepPercent);
                }
            }
            // The screen follows a step at once, in that direction.
            Options o;
            o.wider = MarginFactor(4, mode);
            o.scale_fov = mode == ScaleMode::FieldOfView ? 1.0f : 0.0f;
            o.caps = 1.0f;
            Placement q = p;
            q.size = r.f.size;
            const auto solve = [&](int sx, int sy, float size) {
                Options oo = o;
                oo.squeeze = SqueezeForScale(sx);
                oo.squeeze_y = SqueezeForScale(sy);
                Placement qq = q;
                qq.size = size;
                return SolveHeadLocked(qq, cam, kAspect3ds, Lopsided(), 0.0107f, oo);
            };
            const HeadLocked at = solve(r.f.scale_x, r.f.scale_y, r.f.size);
            INFO("mode " << static_cast<int>(mode) << " cam " << cam << " filled " << r.f.scale_x
                         << ", " << r.f.scale_y << " size " << r.f.size);
            // Never backwards; in Field of View always a change (in Stretch a step past where the
            // margin's padding is trimmed can leave that side as it is).
            const bool fov = mode == ScaleMode::FieldOfView;
            const auto grows = [fov](float after, float before) {
                return fov ? after > before : after >= before;
            };
            if (r.f.scale_x < kMaxScalePercent) {
                REQUIRE(grows(solve(SteppedScale(r.f.scale_x, 1), r.f.scale_y, r.f.size).width(),
                              at.width()));
            }
            if (r.f.scale_x > -kMaxScalePercent) {
                REQUIRE(grows(at.width(),
                              solve(SteppedScale(r.f.scale_x, -1), r.f.scale_y, r.f.size).width()));
            }
            if (r.f.scale_y < kMaxScalePercent) {
                REQUIRE(grows(solve(r.f.scale_x, SteppedScale(r.f.scale_y, 1), r.f.size).height(),
                              at.height()));
            }
            if (r.f.scale_y > -kMaxScalePercent) {
                REQUIRE(grows(at.height(),
                              solve(r.f.scale_x, SteppedScale(r.f.scale_y, -1), r.f.size).height()));
            }
            // The grip's resize (a factor on the size) goes on smoothly from the filled size.
            const HeadLocked bigger = solve(r.f.scale_x, r.f.scale_y, r.f.size * 1.01f);
            const HeadLocked smaller = solve(r.f.scale_x, r.f.scale_y, r.f.size * 0.99f);
            REQUIRE(bigger.width() == Approx(at.width() * 1.01f).epsilon(1e-4));
            REQUIRE(smaller.width() == Approx(at.width() * 0.99f).epsilon(1e-4));
            REQUIRE(FillToast(r.f, false).rfind("Fill View: Horizontal ", 0) == 0);
        }
    }
    Filled f;
    f.scale_x = 35;
    f.scale_y = -5;
    REQUIRE(FillToast(f, false) == "Fill View: Horizontal +35%, Vertical -5%");
    REQUIRE(FillToast(f, true) == "Fill View: Horizontal +35%, Vertical -5% (May Warp)");
}

// AzaharFrame fovaxis: grip + D-pad up or down in the Full modes: Render Margin with Stretch,
// 3D Depth with Field of View (Render Margin's rows are hidden there), the 3D Depth row's 5 %
// steps from 0 to 100 %.
TEST_CASE("FrameXr fovaxis: grip + D-pad up and down by Scale Mode", "[frame_xr][fovaxis]") {
    CHECK(GripUpDownFor(ScaleMode::Stretch) == GripUpDown::RenderMargin);
    CHECK(GripUpDownFor(ScaleMode::FieldOfView) == GripUpDown::Depth3d);
    CHECK(kDepthStepPercent == 5);
    // One step each way, on the grid.
    CHECK(SteppedDepth(80, 1) == 85);
    CHECK(SteppedDepth(80, -1) == 75);
    CHECK(SteppedDepth(50, 3) == 65);
    CHECK(SteppedDepth(50, -3) == 35);
    // Off the grid (Azahar's window takes any value): the first step goes to the next multiple.
    CHECK(SteppedDepth(42, 1) == 45);
    CHECK(SteppedDepth(42, -1) == 40);
    CHECK(SteppedDepth(42, 2) == 50);
    // At the limits: nothing past them.
    CHECK(SteppedDepth(100, 1) == 100);
    CHECK(SteppedDepth(0, -1) == 0);
    CHECK(SteppedDepth(97, 1) == 100);
    CHECK(SteppedDepth(3, -1) == 0);
    CHECK(SteppedDepth(95, 4) == 100);
    CHECK(SteppedDepth(5, -4) == 0);
    CHECK(SteppedDepth(150, -1) == 95); // out of range: from 100
    CHECK(SteppedDepth(-20, 1) == 5);   // from 0
    CHECK(SteppedDepth(60, 0) == 60);
    // Every value, both ways: within range, on the grid after a step, monotonic.
    for (int p = 0; p <= 100; ++p) {
        const int up = SteppedDepth(p, 1), down = SteppedDepth(p, -1);
        CHECK(up >= p);
        CHECK(down <= p);
        CHECK(up % kDepthStepPercent == 0);
        CHECK(down % kDepthStepPercent == 0);
        CHECK((up > p || p == 100));
        CHECK((down < p || p == 0));
        CHECK(up - p <= kDepthStepPercent);
        CHECK(p - down <= kDepthStepPercent);
    }
    // The toast is 3D Depth's own: "3D Depth: 80%".
    CHECK(FrameXr::Immersive::Names::DepthToast(80) == "3D Depth: 80%");
    CHECK(FrameXr::Immersive::Names::DepthToast(0) == "3D Depth: 0%");
}
