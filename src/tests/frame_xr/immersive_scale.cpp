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
#include "frame_xr/immersive_layout.h"
#include "video_core/shader/generator/glsl_shader_gen.h" // AzaharFrame fovscale

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
            REQUIRE(fv.drawn >= kFactorMin);
            REQUIRE(fv.drawn <= kFovFactorMax);
            if (b * k >= kFactorMin) {
                // Inside the limits: all of it drawn, nothing stretched.
                REQUIRE(fv.stretch == Approx(1.0f).margin(1e-6));
                REQUIRE(fv.drawn == Approx(b * k).epsilon(1e-6));
            } else {
                // Below the game's own frame: the frame drawn, the rest a stretch (narrower).
                REQUIRE(fv.drawn == kFactorMin);
                REQUIRE(fv.stretch < 1.0f);
            }
        }
    }
    // Render Margin Off: a negative scale is all stretch (as Stretch), a positive one all drawn.
    REQUIRE(ScaleSplit(1.0f, 0.7f, 1.0f).drawn == kFactorMin);
    REQUIRE(ScaleSplit(1.0f, 0.7f, 1.0f).stretch == Approx(0.7f));
    REQUIRE(ScaleSplit(1.0f, 1.3f, 1.0f).drawn == Approx(1.3f));
    REQUIRE(ScaleSplit(1.0f, 1.3f, 1.0f).stretch == Approx(1.0f));
    // 1.4x at -30 %: the design's "draws 1.00x, stretch 0.98".
    REQUIRE(ScaleSplit(1.4f, 0.7f, 1.0f).drawn == kFactorMin);
    REQUIRE(ScaleSplit(1.4f, 0.7f, 1.0f).stretch == Approx(0.98f));
    // The upper limit: 1.4x x 1.3 is exactly kFovFactorMax; past it (a hand-edited factor) the
    // rest is a stretch.
    REQUIRE(kFovFactorMax == Approx(1.82f));
    REQUIRE(ScaleSplit(1.4f, 1.3f, 1.0f).stretch == Approx(1.0f));
    REQUIRE(ScaleSplit(1.4f, 1.5f, 1.0f).drawn == Approx(kFovFactorMax));
    REQUIRE(ScaleSplit(1.4f, 1.5f, 1.0f).stretch == Approx(1.4f * 1.5f / kFovFactorMax));
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
        for (int scale = -25; scale <= 30; scale += 5) { // inside the limits at 1.4x
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
    // Field of View at Horizontal -30 %, Vertical -15 %, Render Margin 1.4x: 1.00x across, 1.19x
    // up and down.
    const AxisSplit across = ScaleSplit(1.4f, 0.7f, 1.0f), up = ScaleSplit(1.4f, 0.85f, 1.0f);
    REQUIRE(across.drawn == 1.0f);
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
