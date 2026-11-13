// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <optional>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/immersive_layout.h"
#include "video_core/vr_immersive.h" // AzaharFrame depth: MeasureDraw
#include "video_core/vr_view_matrix.h"

// AzaharFrame: the immersive screen's placement (frame_xr/immersive_layout.h).
using Catch::Approx;
using namespace FrameXr::Immersive::Layout;
using FrameXr::Input::Posef;
using FrameXr::Input::Quatf;
using FrameXr::Input::Vec3f;
namespace In = FrameXr::Input;

namespace {

constexpr float kAspect3ds = 400.0f / 240.0f;

float Rad(float deg) {
    return deg * kDegToRad;
}

ViewFov Symmetric(float half_deg) {
    ViewFov v;
    v.left = -Rad(half_deg);
    v.right = Rad(half_deg);
    v.up = Rad(half_deg);
    v.down = -Rad(half_deg);
    return v;
}

Quatf YawRotation(float yaw_rad) {
    return {0.0f, std::sin(yaw_rad / 2), 0.0f, std::cos(yaw_rad / 2)};
}

Vec3f Forward(const Quatf& q) {
    return In::Rotate(q, Vec3f{0.0f, 0.0f, -1.0f});
}

float Elevation(const Vec3f& v) {
    return std::atan2(v.y, std::hypot(v.x, v.z));
}

// The four corners of a quad (pose, size) in its space.
std::array<Vec3f, 4> Corners(const Posef& pose, float w, float h) {
    std::array<Vec3f, 4> c{};
    int i = 0;
    for (const float sx : {-0.5f, 0.5f}) {
        for (const float sy : {-0.5f, 0.5f}) {
            c[static_cast<std::size_t>(i++)] =
                pose.position + In::Rotate(pose.orientation, Vec3f{sx * w, sy * h, 0.0f});
        }
    }
    return c;
}

In::InputFrame Hands(float grip_right, const Posef& aim_right, In::Vec2f stick_right = {}) {
    In::InputFrame f;
    f.hand[0].active = true;
    f.hand[1].active = true;
    f.hand[1].grip = grip_right;
    f.hand[1].aim = aim_right;
    f.hand[1].stick = stick_right;
    return f;
}

Posef AimPitched(float pitch_deg) {
    return {PitchRotation(Rad(pitch_deg)), Vec3f{}};
}

} // namespace

TEST_CASE("FrameXr immersive: the view both eyes see", "[frame_xr]") {
    const EyeFov left{Rad(-50), Rad(40), Rad(45), Rad(-50)};
    const EyeFov right{Rad(-40), Rad(50), Rad(44), Rad(-52)};
    const ViewFov v = BothEyes(left, right);
    REQUIRE(v.left == Approx(Rad(-40)));
    REQUIRE(v.right == Approx(Rad(40)));
    REQUIRE(v.up == Approx(Rad(44)));
    REQUIRE(v.down == Approx(Rad(-50)));
    REQUIRE(ViewMiddlePitch(v) == Approx(Rad(-3)));

    SECTION("canted eyes turn their angles") {
        EyeFov l = left, r = right;
        l.yaw = Rad(5);   // turned left
        r.yaw = Rad(-5);  // turned right
        const ViewFov c = BothEyes(l, r);
        REQUIRE(c.left == Approx(Rad(-45)));
        REQUIRE(c.right == Approx(Rad(45)));
    }
    SECTION("either eye sees farther to its own side") { // AzaharFrame immersive
        REQUIRE(v.outer_left == Approx(Rad(-50)));
        REQUIRE(v.outer_right == Approx(Rad(50)));
    }
    SECTION("no overlap keeps the fallback") {
        const EyeFov a{Rad(-50), Rad(-30), Rad(45), Rad(-45)};
        const EyeFov b{Rad(30), Rad(50), Rad(45), Rad(-45)};
        const ViewFov f = BothEyes(a, b);
        REQUIRE(f.right == Approx(Rad(kFallbackHalfFovDeg)));
    }
}

TEST_CASE("FrameXr immersive: fitting the screen in the view", "[frame_xr]") {
    SECTION("a square quad in a 90 degree view fills it") {
        REQUIRE(FitTanHalfY(Symmetric(45), 1.0f, 0.0f, 0.0f) == Approx(1.0f).epsilon(1e-3));
    }
    SECTION("a 5:3 quad is held by its width") {
        REQUIRE(FitTanHalfY(Symmetric(45), kAspect3ds, 0.0f, 0.0f) ==
                Approx(1.0f / kAspect3ds).epsilon(1e-3));
    }
    SECTION("the margin and a pitch away from the middle make it smaller") {
        const float none = FitTanHalfY(Symmetric(45), 1.0f, 0.0f, 0.0f);
        REQUIRE(FitTanHalfY(Symmetric(45), 1.0f, 0.0f, Rad(3)) < none);
        REQUIRE(FitTanHalfY(Symmetric(45), 1.0f, Rad(-10), 0.0f) < none);
    }
    SECTION("in a view with more room below, a lower screen can be bigger") {
        ViewFov v = Symmetric(45);
        v.up = Rad(35);
        v.down = Rad(-55);
        REQUIRE(FitTanHalfY(v, 1.0f, ViewMiddlePitch(v), 0.0f) >
                FitTanHalfY(v, 1.0f, 0.0f, 0.0f));
    }
}

// AzaharFrame immersive: the default fills the view (the owner: no black edge from any angle,
// and after the next test no black bars at the top and bottom either).
namespace {
// How far past the corners of what either eye sees the screen reaches, the least over the four
// corners and both axes, degrees (negative: a corner shows past the screen's edge).
float CoverSpareDeg(const ViewFov& v, const HeadLocked& s) {
    float spare = 1e9f;
    const Quatf to_screen = In::Conjugate(PitchRotation(s.pitch_rad));
    for (const float h : {v.outer_left, v.outer_right}) {
        for (const float e : {v.outer_down, v.outer_up}) {
            const Vec3f d = In::Rotate(to_screen, Vec3f{std::tan(h), std::tan(e), -1.0f});
            const float x = std::atan(std::abs(d.x / -d.z));
            const float y = std::atan(std::abs(d.y / -d.z));
            spare = std::min({spare, std::atan(s.tan_half_y * s.aspect) - x,
                              std::atan(s.tan_half_y) - y});
        }
    }
    return spare / Rad(1.0f);
}
} // namespace

TEST_CASE("FrameXr immersive: the default is the world at its own scale", "[frame_xr]") {
    // AzaharFrame immersive2. The Frame (a headset log): both eyes see -50.3 to 50.4 across,
    // -59.6 to 49.5 up; the left eye sees to -59.1 on its side, the right eye to 59.1 on its,
    // either eye -60.0 to 49.5 up.
    ViewFov frame;
    frame.left = Rad(-50.3f);
    frame.right = Rad(50.4f);
    frame.down = Rad(-59.6f);
    frame.up = Rad(49.5f);
    frame.outer_left = Rad(-59.1f);
    frame.outer_right = Rad(59.1f);
    frame.outer_down = Rad(-60.0f);
    frame.outer_up = Rad(49.5f);
    for (const float fov : {45.0f, 52.0f, 55.0f, 60.0f}) {
        for (const float aspect : {kAspect3ds, 16.0f / 9.0f}) {
            for (const uint32_t level : {0u, 4u}) {
                Options o;
                o.wider = WiderFactor(level);
                const HeadLocked s = SolveHeadLocked(Placement{}, fov, aspect, frame, 0.0f, o);
                const float t = std::tan(Rad(fov / 2));
                // Gain 1: as wide as the game draws, nothing magnified.
                REQUIRE(s.gain == Approx(1.0f));
                REQUIRE(s.gain_x == Approx(1.0f));
                REQUIRE(s.tan_half_y * s.aspect == Approx(o.wider * aspect * t));
                // Inside what fills the view, centred on its middle.
                REQUIRE(CoverSpareDeg(frame, s) <= kFillMarginDeg + 0.01f);
                REQUIRE(s.pitch_rad == Approx(ViewMiddlePitch(frame)));
                // AzaharFrame oot: the game's own shape, widened evenly by Render Margin.
                REQUIRE(s.factor == Approx(o.wider));
                REQUIRE(s.factor_y == Approx(s.factor));
                REQUIRE(s.aspect == Approx(aspect));
                // Not stretched: the same scale both ways.
                REQUIRE(s.factor * t * aspect / (s.tan_half_y * s.aspect) == Approx(s.gain));
                REQUIRE(s.factor_y * t / s.tan_half_y == Approx(s.gain));
            }
        }
    }
    SECTION("the owner's Majora's Mask 3D: 94 x 83 degrees at gain 1.03") {
        // Mode 2 at 3 m, 55 degrees, Render Margin 1.4x, Horizontal Scale -10 % (the owner's log).
        Options o;
        o.wider = WiderFactor(4);
        o.squeeze = SqueezeFor(10);
        Placement p;
        REQUIRE(p.distance == Approx(3.0f)); // kDefaultHeadDistance
        const HeadLocked s = SolveHeadLocked(p, 55.0f, kAspect3ds, frame, 0.032f / 3.0f, o);
        const float across = 2.0f * std::atan(s.tan_half_y * s.aspect) / Rad(1.0f);
        const float high = 2.0f * std::atan(s.tan_half_y) / Rad(1.0f);
        // Gain 1.03 there: a screen 1.03 times smaller in tangents than this one.
        const auto at_gain_1 = [](float deg) {
            return 2.0f * std::atan(std::tan(Rad(deg / 2.0f)) * 1.03f) / Rad(1.0f);
        };
        // AzaharFrame oot: as wide as before, and as high as the game's own shape (the screen took
        // the view's shape before: 83 degrees high, the game drawing 1.75 times its height).
        REQUIRE(across == Approx(at_gain_1(94.0f)).margin(1.0f));
        const float high_own = 2.0f * std::atan(1.4f * std::tan(Rad(27.5f)) / 1.03f) / Rad(1.0f);
        REQUIRE(high == Approx(at_gain_1(high_own)).margin(1.0f));
        REQUIRE(high < 83.0f - 5.0f);
        REQUIRE(s.factor_y == Approx(1.4f));
        REQUIRE(s.gain == Approx(1.0f));
        REQUIRE(s.gain_x == Approx(1.0f / 0.9f));
    }
    SECTION("Render Margin: the default grows with the factor, the gain stays") {
        Options wider;
        wider.wider = WiderFactor(4);
        const HeadLocked off = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, frame);
        const HeadLocked on = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, frame, 0.0f, wider);
        REQUIRE(on.tan_half_y * on.aspect == Approx(off.tan_half_y * off.aspect * 1.4f));
        REQUIRE(on.tan_half_y == Approx(off.tan_half_y * 1.4f));
        REQUIRE(on.gain == Approx(off.gain));
        REQUIRE(on.factor_y == Approx(off.factor_y * 1.4f));
    }
    SECTION("a very wide camera: no bigger than filling the view") {
        Options wider;
        wider.wider = WiderFactor(4);
        const HeadLocked s = SolveHeadLocked(Placement{}, 110.0f, 16.0f / 9.0f, frame, 0.0f, wider);
        // AzaharFrame oot: in the picture's own shape it fills the view across, at most.
        const Cover cover = DefaultCover(frame, ViewMiddlePitch(frame), 0.0f);
        REQUIRE(s.tan_half_y * s.aspect == Approx(cover.tan_x));
        REQUIRE(s.tan_half_y <= cover.tan_y);
        REQUIRE(s.gain > 1.0f);
        REQUIRE(s.gain <= kMaxGain);
        REQUIRE(s.factor_y * std::tan(Rad(55)) / s.tan_half_y == Approx(s.gain));
    }
    SECTION("a narrow camera: a small screen, still the world's own scale") {
        const HeadLocked s = SolveHeadLocked(Placement{}, 30.0f, kAspect3ds, frame);
        REQUIRE(s.gain == Approx(1.0f));
        REQUIRE(s.tan_half_y * s.aspect == Approx(kAspect3ds * std::tan(Rad(15))));
    }
    SECTION("a view no taller than the picture: the picture's own shape") {
        ViewFov wide = Symmetric(45);
        wide.outer_left = Rad(-70.0f);
        wide.outer_right = Rad(70.0f);
        const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, wide);
        REQUIRE(s.aspect == Approx(kAspect3ds));
        REQUIRE(s.factor_y == Approx(s.factor));
    }
    SECTION("a view much taller than the picture: still the picture's own shape") {
        ViewFov tall = Symmetric(60);
        tall.outer_left = Rad(-30.0f);
        tall.outer_right = Rad(30.0f);
        tall.left = Rad(-30.0f);
        tall.right = Rad(30.0f);
        Options wider;
        wider.wider = WiderFactor(4);
        const HeadLocked s = SolveHeadLocked(Placement{}, 40.0f, kAspect3ds, tall, 0.0f, wider);
        REQUIRE(s.factor_y == Approx(s.factor));
        REQUIRE(s.aspect == Approx(kAspect3ds));
        REQUIRE(s.gain <= kMaxGain);
        REQUIRE(s.factor_y * std::tan(Rad(20)) / s.tan_half_y == Approx(s.gain));
    }
    SECTION("what fills the view: at least kMinFillHalfHeightDeg high, wider for mode 2") {
        ViewFov v = Symmetric(20);
        v.outer_left = Rad(-60.0f);
        v.outer_right = Rad(60.0f);
        v.outer_up = Rad(20.0f);
        v.outer_down = Rad(-20.0f);
        REQUIRE(std::atan(DefaultCover(v, 0.0f, 0.0f).tan_y) ==
                Approx(Rad(kMinFillHalfHeightDeg)));
        const float shift = 0.032f / kDefaultDistance;
        REQUIRE(DefaultCover(frame, ViewMiddlePitch(frame), shift).tan_x ==
                Approx(DefaultCover(frame, ViewMiddlePitch(frame), 0.0f).tan_x + shift));
    }
    SECTION("moving the screen up or down keeps its size") {
        Placement low;
        low.pitch_deg = -15.0f;
        const HeadLocked a = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, frame);
        const HeadLocked b = SolveHeadLocked(low, 60.0f, kAspect3ds, frame);
        REQUIRE(b.tan_half_y == Approx(a.tan_half_y));
        REQUIRE(b.aspect == Approx(a.aspect));
    }
    SECTION("both eyes' FOVs: either eye's reach up and down") {
        const EyeFov l{Rad(-59.1f), Rad(50.4f), Rad(49.5f), Rad(-60.0f)};
        const EyeFov r{Rad(-50.3f), Rad(59.1f), Rad(49.0f), Rad(-59.6f)};
        const ViewFov v = BothEyes(l, r);
        REQUIRE(v.outer_up == Approx(Rad(49.5f)));
        REQUIRE(v.outer_down == Approx(Rad(-60.0f)));
        REQUIRE(v.up == Approx(Rad(49.0f)));
    }
}

// AzaharFrame immersive: a zoom (aiming) changes the gain, never the screen.
TEST_CASE("FrameXr immersive: a zoom keeps the screen's size and shape", "[frame_xr]") {
    const ViewFov view = Symmetric(50);
    const HeadLocked rest = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view);
    Options zoom;
    zoom.live_fov_y_deg = 40.0f;
    const HeadLocked aim = SolveHeadLocked(Placement{}, 55.0f, kAspect3ds, view, 0.0f, zoom);
    REQUIRE(aim.tan_half_y == Approx(rest.tan_half_y));
    REQUIRE(aim.aspect == Approx(rest.aspect));
    // The picture is magnified as on the 3DS; the gain keeps the middle locked.
    REQUIRE(aim.tan_half_y * aim.gain == Approx(aim.factor_y * std::tan(Rad(20))));
    REQUIRE(aim.factor_y == Approx(rest.factor_y));
    REQUIRE(aim.gain < rest.gain);
}

TEST_CASE("FrameXr immersive: size, factor and gain", "[frame_xr]") {
    const float t = std::tan(Rad(30)); // a game camera 60 degrees high
    const ViewFov roomy = Symmetric(70);
    const HeadLocked base = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, roomy);
    SECTION("resizing never widens the game's view") {
        for (const float size : {kMinSize, 0.8f, 1.0f, 1.5f, 2.0f, kMaxSize}) {
            Placement p;
            p.size = size;
            const HeadLocked s = SolveHeadLocked(p, 60.0f, kAspect3ds, roomy);
            REQUIRE(s.factor == Approx(kFactorMin));
            REQUIRE(s.tan_half_y == Approx(base.tan_half_y * size));
            // Screen x gain = the game's angle wherever the gain is inside its limits.
            if (s.gain > kMinGain && s.gain < kMaxGain) {
                REQUIRE(s.tan_half_y * s.gain == Approx(t * s.factor_y));
            }
            REQUIRE(s.gain >= kMinGain);
            REQUIRE(s.gain <= kMaxGain);
        }
    }
    SECTION("the default of a 60 degree game is inside the gain range; smaller has room") {
        REQUIRE(base.gain >= kMinGain);
        Placement small;
        small.size = kMinSize;
        REQUIRE(SolveHeadLocked(small, 60.0f, kAspect3ds, roomy).gain < kMaxGain);
        REQUIRE(kMaxSize >= 2.0f);
    }
    SECTION("Render Margin: the factor is the level; the default size follows, the gain stays") {
        const ViewFov view50 = Symmetric(50);
        const HeadLocked off = SolveHeadLocked(Placement{}, 40.0f, kAspect3ds, view50);
        REQUIRE(off.factor == Approx(kFactorMin));
        Options wider;
        wider.wider = WiderFactor(2);
        REQUIRE(wider.wider == Approx(1.2f));
        // AzaharFrame immersive2: the game draws 1.2x wider into a screen 1.2x as wide (in
        // tangents), so the picture keeps its scale and only more shows around it.
        const HeadLocked stepped =
            SolveHeadLocked(Placement{}, 40.0f, kAspect3ds, view50, 0.0f, wider);
        REQUIRE(stepped.factor == Approx(1.2f));
        REQUIRE(stepped.gain == Approx(off.gain));
        REQUIRE(stepped.tan_half_y == Approx(off.tan_half_y * 1.2f));
        // At a size of its own the screen keeps that size relative to the default.
        Placement p;
        p.size = 0.8f;
        REQUIRE(SolveHeadLocked(p, 40.0f, kAspect3ds, view50, 0.0f, wider).tan_half_y ==
                Approx(stepped.tan_half_y * 0.8f));
        // Any level past the last is the last; the factor stays within CitraVR's 1.4.
        REQUIRE(WiderFactor(9) == Approx(kFactorMax));
        Options too_wide;
        too_wide.wider = 3.0f;
        REQUIRE(SolveHeadLocked(p, 40.0f, kAspect3ds, view50, 0.0f, too_wide).factor ==
                Approx(kFactorMax));
    }
    SECTION("Render Margin: the toast and the old switch") {
        REQUIRE(WiderToast(0) == "Render Margin: Off"); // AzaharFrame kiu: was Wider View
        REQUIRE(WiderToast(2) == "Render Margin: 1.2x");
        REQUIRE(WiderToast(4) == "Render Margin: 1.4x");
        // The old switch let the factor follow the size (about 0.78 x size): the nearest level.
        REQUIRE(LevelForOldWiderView(std::nullopt) == kMaxWiderLevel);
        REQUIRE(LevelForOldWiderView(1.0f) == 0u);
        REQUIRE(LevelForOldWiderView(1.5f) == 2u);
        REQUIRE(LevelForOldWiderView(2.5f) == kMaxWiderLevel);
    }
    SECTION("distance keeps the angle") {
        Placement near_p, far_p;
        near_p.distance = 0.5f;
        far_p.distance = 2.0f;
        const HeadLocked a = SolveHeadLocked(near_p, 60.0f, kAspect3ds, Symmetric(45));
        const HeadLocked b = SolveHeadLocked(far_p, 60.0f, kAspect3ds, Symmetric(45));
        REQUIRE(a.height() / a.distance == Approx(b.height() / b.distance));
        REQUIRE(a.width() / a.distance == Approx(b.width() / b.distance));
        REQUIRE(a.factor == Approx(b.factor));
        REQUIRE(a.gain == Approx(b.gain));
    }
    SECTION("the default is centred on the middle of the view") {
        ViewFov v = Symmetric(45);
        v.down = Rad(-55);
        const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, v);
        REQUIRE(s.pitch_rad == Approx(ViewMiddlePitch(v) + Rad(kDefaultPitchDeg)));
        REQUIRE(s.pitch_rad == Approx(Rad(-5)));
    }
}

TEST_CASE("FrameXr immersive: the old Narrow Screen setting (now Horizontal Scale)", "[frame_xr]") {
    // AzaharFrame immersive2: percent on the 5 % grid; 1 to 3 are the steps of earlier builds.
    REQUIRE(NarrowPercent(0) == 0u);
    REQUIRE(NarrowPercent(1) == 10u);
    REQUIRE(NarrowPercent(2) == 20u);
    REQUIRE(NarrowPercent(3) == 30u);
    REQUIRE(NarrowPercent(5) == 5u);
    REQUIRE(NarrowPercent(15) == 15u);
    REQUIRE(NarrowPercent(7) == 5u);
    REQUIRE(NarrowPercent(8) == 10u);
    REQUIRE(NarrowPercent(30) == 30u);
    REQUIRE(NarrowPercent(99) == 30u);
    REQUIRE(SqueezeFor(0) == 0.0f);
    REQUIRE(SqueezeFor(1) == Approx(0.1f));
    REQUIRE(SqueezeFor(3) == Approx(0.3f));
    REQUIRE(SqueezeFor(15) == Approx(0.15f));
    // AzaharFrame kiu: the steps and toasts are Horizontal and Vertical Scale's now
    // (immersive_scale.cpp).
}

TEST_CASE("FrameXr immersive: the old Narrow Height setting (now Vertical Scale)", "[frame_xr]") {
    // AzaharFrame immersive2: the same as Narrow Screen, up and down.
    const ViewFov roomy = Symmetric(70);
    const HeadLocked plain = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, roomy);
    Options o;
    o.squeeze_y = SqueezeFor(20);
    const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, roomy, 0.0f, o);
    REQUIRE(s.width() == Approx(plain.width()));
    REQUIRE(s.height() == Approx(plain.height() * 0.8f));
    REQUIRE(s.gain_x == Approx(plain.gain_x));
    REQUIRE(s.gain == Approx(plain.gain / 0.8f));
    REQUIRE(s.factor_y == Approx(plain.factor_y)); // the game draws the same
    const float t = std::tan(Rad(30));
    REQUIRE(s.factor_y * t / s.tan_half_y == Approx(s.gain));
    REQUIRE(s.factor * t * kAspect3ds / (s.tan_half_y * s.aspect) == Approx(s.gain_x));
    // Both at once: each axis its own.
    o.squeeze = SqueezeFor(10);
    const HeadLocked both = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, roomy, 0.0f, o);
    REQUIRE(both.width() == Approx(plain.width() * 0.9f));
    REQUIRE(both.height() == Approx(plain.height() * 0.8f));
    REQUIRE(both.gain_x == Approx(plain.gain / 0.9f));
}

TEST_CASE("FrameXr immersive: Squeeze Width", "[frame_xr]") {
    const ViewFov roomy = Symmetric(70);
    const HeadLocked plain = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, roomy);
    Options o;
    o.squeeze = SqueezeFor(2);
    const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, roomy, 0.0f, o);
    // Same height and pitch gain, 20 % narrower, the yaw gain makes up for it.
    REQUIRE(s.tan_half_y == Approx(plain.tan_half_y));
    REQUIRE(s.gain == Approx(plain.gain));
    REQUIRE(s.width() == Approx(plain.width() * 0.8f));
    REQUIRE(s.gain_x == Approx(plain.gain / 0.8f));
    // The yaw gain is the game's horizontal angle over the screen's (tangents), as the pitch gain
    // is for the vertical: the middle stays locked both ways.
    const float t = std::tan(Rad(30));
    REQUIRE(s.factor * t * kAspect3ds / (s.tan_half_y * s.aspect) == Approx(s.gain_x));
    REQUIRE(s.factor_y * t / s.tan_half_y == Approx(s.gain));
    REQUIRE(plain.gain_x == Approx(plain.gain));
}

TEST_CASE("FrameXr immersive: Squeeze Width 30%", "[frame_xr]") {
    // A 75 degree game at the default size (gain 1): the yaw gain stays under the limit,
    // so the middle stays locked both ways.
    Options o;
    o.squeeze = SqueezeFor(3);
    const HeadLocked plain = SolveHeadLocked(Placement{}, 75.0f, kAspect3ds, Symmetric(50));
    const HeadLocked s = SolveHeadLocked(Placement{}, 75.0f, kAspect3ds, Symmetric(50), 0.0f, o);
    REQUIRE(s.gain == Approx(plain.gain));
    REQUIRE(s.width() == Approx(plain.width() * 0.7f));
    REQUIRE(s.gain < 1.4f);
    REQUIRE(s.gain_x == Approx(s.gain / 0.7f));
    REQUIRE(s.gain_x < kMaxGain);
    // A smaller screen (bigger gain): the yaw gain stops at the limit, never past it.
    Placement small;
    small.size = 0.6f;
    const HeadLocked t = SolveHeadLocked(small, 75.0f, kAspect3ds, Symmetric(50), 0.0f, o);
    REQUIRE(t.gain > 1.4f);
    REQUIRE(t.gain_x == Approx(kMaxGain));
    REQUIRE(std::isfinite(t.tan_half_y));
}

TEST_CASE("FrameXr immersive: the rotation gain", "[frame_xr]") {
    const Quatf q = ScaleRotation(YawRotation(Rad(20)), 1.5f);
    const Vec3f f = Forward(q);
    REQUIRE(std::atan2(-f.x, -f.z) == Approx(Rad(30)));
    const Quatf same = ScaleRotation(YawRotation(Rad(20)), 1.0f);
    REQUIRE(same.y == Approx(YawRotation(Rad(20)).y));
    const Quatf none = ScaleRotation(Quatf{}, 1.5f);
    REQUIRE(none.w == Approx(1.0f));
    // A pitch is scaled about its own axis.
    const Vec3f up = Forward(ScaleRotation(PitchRotation(Rad(10)), 0.5f));
    REQUIRE(Elevation(up) == Approx(Rad(5)));
}

TEST_CASE("FrameXr immersive: separate yaw and pitch gains", "[frame_xr]") {
    const Quatf roll{0.0f, 0.0f, std::sin(Rad(8) / 2), std::cos(Rad(8) / 2)};
    for (const float yaw : {0.0f, 25.0f, -60.0f}) {
        for (const float pitch : {0.0f, 15.0f, -30.0f}) {
            const Quatf head = In::Normalized(In::Multiply(
                In::Multiply(YawRotation(Rad(yaw)), PitchRotation(Rad(pitch))), roll));
            const Quatf q = ScaleYawPitch(head, 1.5f, 1.2f);
            const Vec3f f = Forward(q);
            REQUIRE(std::atan2(-f.x, -f.z) == Approx(Rad(yaw * 1.5f)).margin(1e-4));
            REQUIRE(Elevation(f) == Approx(Rad(pitch * 1.2f)).margin(1e-4));
            // The roll is kept: the camera's up, seen from its forward, is turned by 8 degrees.
            const Quatf aim = In::Multiply(YawRotation(Rad(yaw * 1.5f)),
                                           PitchRotation(Rad(pitch * 1.2f)));
            const Vec3f up = In::Rotate(In::Conjugate(aim), In::Rotate(q, Vec3f{0, 1, 0}));
            REQUIRE(std::atan2(-up.x, up.y) == Approx(Rad(8)).margin(1e-4));
        }
    }
    // Both gains 1: unchanged; equal gains: the same forward as the isotropic gain.
    const Quatf h = In::Normalized(In::Multiply(YawRotation(Rad(20)), PitchRotation(Rad(-10))));
    REQUIRE(ScaleYawPitch(h, 1.0f, 1.0f).x == h.x);
    const Vec3f a = Forward(ScaleYawPitch(YawRotation(Rad(20)), 1.3f, 1.3f));
    const Vec3f b = Forward(ScaleRotation(YawRotation(Rad(20)), 1.3f));
    REQUIRE(a.x == Approx(b.x).margin(1e-5));
    REQUIRE(a.z == Approx(b.z).margin(1e-5));
}

TEST_CASE("FrameXr immersive: the middle stays locked with a gain", "[frame_xr]") {
    // A small screen (gain about 1.7) and a big one (magnified, gain under 1), squeezed: a small
    // head turn moves the game camera by gain times as much, which moves the picture back by the
    // turn at the screen's middle.
    Options o;
    o.squeeze = SqueezeFor(1);
    Placement small;
    small.size = 0.6f;
    Placement big;
    big.size = 1.5f;
    for (const Placement& place : {small, big}) {
    const HeadLocked s = SolveHeadLocked(place, 60.0f, kAspect3ds, Symmetric(50), 0.0f, o);
    REQUIRE(std::abs(s.gain - 1.0f) > 0.1f);
    const float turn = Rad(0.5f);
    // Yaw: the camera turns by gain_x x turn; the game's straight-ahead point is then that far to
    // the side in the picture, which is (tan ratio gain_x) turn on the screen.
    const Vec3f cy = Forward(GameCameraRotation(YawRotation(turn), s));
    const float game_yaw = std::atan2(-cy.x, -cy.z);
    REQUIRE(game_yaw / s.gain_x == Approx(turn).epsilon(2e-2));
    const Vec3f cp = Forward(GameCameraRotation(PitchRotation(turn), s));
    REQUIRE((Elevation(cp) - s.pitch_rad) / s.gain == Approx(turn).epsilon(2e-2));
    }
}

TEST_CASE("FrameXr immersive: mode 1 bottom screen sits in front of the wide screen", "[frame_xr]") {
    const Posef rest{YawRotation(Rad(40)), Vec3f{0.1f, 1.6f, 0.2f}};
    const float pitch = Rad(-5);
    const float d = kDefaultDistance;
    const Posef wide = WideQuadPose(rest, pitch, d);
    const BottomQuad b = WideBottomQuad(rest, pitch, d);
    // Nearer and lower.
    REQUIRE(In::Length(b.pose.position - rest.position) < In::Length(wide.position - rest.position));
    const Posef to_rest = In::Inverse(rest);
    REQUIRE(In::Transform(to_rest, b.pose.position).y < In::Transform(to_rest, wide.position).y);
    // Never through the wide screen: every corner of the bottom screen is on the viewer's side of
    // the wide screen's plane.
    const Vec3f n = In::Rotate(wide.orientation, Vec3f{0.0f, 0.0f, 1.0f}); // toward the viewer
    for (const Vec3f& c : Corners(b.pose, b.width, b.height)) {
        REQUIRE(In::Dot(c - wide.position, n) > 0.05f);
    }
}

TEST_CASE("FrameXr immersive: which screens show", "[frame_xr]") {
    ScreenState s;
    s.live = true;
    s.view_valid = true;
    SECTION("normal: both screens") {
        const ScreenDecision d = Decide(s);
        REQUIRE_FALSE(d.immersive);
        REQUIRE(d.show_bottom);
    }
    SECTION("Hide Bottom Screen: hidden until the chord") {
        s.hide_bottom = true;
        REQUIRE_FALSE(Decide(s).show_bottom);
        s.bottom_toggled = true;
        REQUIRE(Decide(s).show_bottom);
    }
    SECTION("mode 3 (a racing game that starts with the bottom screen shown): hidden, immersive") {
        s.mode = 3;
        ScreenDecision d = Decide(s);
        REQUIRE(d.immersive);
        REQUIRE_FALSE(d.show_bottom);
        // The chord shows it and pauses immersive mode: never both at once.
        s.bottom_toggled = true;
        d = Decide(s);
        REQUIRE(d.show_bottom);
        REQUIRE_FALSE(d.immersive);
        // Hidden again: immersive again.
        s.bottom_toggled = false;
        REQUIRE(Decide(s).immersive);
    }
    SECTION("modes 2 and 3 pause for the in-game menu and without a head pose") {
        s.mode = 2;
        s.menu_open = true;
        REQUIRE_FALSE(Decide(s).immersive);
        REQUIRE_FALSE(Decide(s).show_bottom);
        s.menu_open = false;
        s.view_valid = false;
        REQUIRE_FALSE(Decide(s).immersive);
    }
    SECTION("the bottom screen in focus pauses every mode and shows it") {
        for (const uint32_t mode : {1u, 2u, 3u}) {
            s.mode = mode;
            s.bottom_focused = true;
            const ScreenDecision d = Decide(s);
            REQUIRE_FALSE(d.immersive);
            REQUIRE(d.show_bottom);
            s.bottom_focused = false;
            REQUIRE(Decide(s).immersive);
            REQUIRE_FALSE(Decide(s).show_bottom);
        }
    }
    SECTION("mode 1 keeps going with the bottom screen shown") {
        s.mode = 1;
        s.bottom_toggled = true;
        const ScreenDecision d = Decide(s);
        REQUIRE(d.immersive);
        REQUIRE(d.show_bottom);
    }
    SECTION("no game on view: nothing immersive") {
        s.mode = 2;
        s.live = false;
        REQUIRE_FALSE(Decide(s).immersive);
    }
}

TEST_CASE("FrameXr immersive: the grip gesture", "[frame_xr]") {
    Adjuster adj;
    Placement p;
    p.distance = kDefaultDistance; // room to go farther
    const Posef head{};
    // Squeezed while aiming straight ahead (from the eye, so the angles are exact).
    Adjuster::Result r = adj.Update(Hands(1.0f, AimPitched(0)), head, 0.011f, true, p);
    REQUIRE(r.active);
    REQUIRE(r.hand == 1);
    SECTION("the screen follows the hand up and down") {
        r = adj.Update(Hands(1.0f, AimPitched(-10)), head, 0.011f, true, p);
        REQUIRE(r.changed);
        REQUIRE(p.pitch_deg == Approx(kDefaultPitchDeg - 10.0f).margin(1e-3));
        r = adj.Update(Hands(1.0f, AimPitched(4)), head, 0.011f, true, p);
        REQUIRE(p.pitch_deg == Approx(kDefaultPitchDeg + 4.0f).margin(1e-3));
    }
    SECTION("stick up: farther, stick right: bigger; under half way: nothing") {
        adj.Update(Hands(1.0f, AimPitched(0), {0.0f, 1.0f}), head, 0.1f, true, p);
        REQUIRE(p.distance == Approx(kDefaultDistance * std::exp(kDistanceRate * 0.1f)));
        adj.Update(Hands(1.0f, AimPitched(0), {1.0f, 0.0f}), head, 0.1f, true, p);
        REQUIRE(p.size == Approx(std::exp(kSizeRate * 0.1f)));
        const Placement before = p;
        adj.Update(Hands(1.0f, AimPitched(0), {0.4f, -0.4f}), head, 0.1f, true, p);
        REQUIRE(p == before);
    }
    SECTION("changing the distance does not move it, with the hand below the eye") {
        Adjuster a;
        Placement q;
        q.distance = kDefaultDistance;
        const Posef low{PitchRotation(Rad(-10)), Vec3f{0.25f, -0.4f, -0.35f}};
        a.Update(Hands(1.0f, {Quatf{}, low.position}), head, 0.011f, true, q);
        a.Update(Hands(1.0f, low), head, 0.011f, true, q);
        const float pitch = q.pitch_deg;
        REQUIRE(pitch < kDefaultPitchDeg); // the hand aimed lower: the screen went down
        for (int i = 0; i < 10; i++) {
            a.Update(Hands(1.0f, low, {0.0f, 1.0f}), head, 0.05f, true, q);
        }
        REQUIRE(q.distance > kDefaultDistance * 1.5f);
        REQUIRE(q.pitch_deg == Approx(pitch).margin(1e-3));
    }
    SECTION("limits") {
        for (int i = 0; i < 200; i++) {
            adj.Update(Hands(1.0f, AimPitched(-89), {-1.0f, -1.0f}), head, 0.1f, true, p);
        }
        REQUIRE(p.pitch_deg == Approx(kMinPitchDeg));
        REQUIRE(p.size == Approx(kMinSize));
        REQUIRE(p.distance == Approx(kMinDistance));
        // Coming back follows the hand at once, from the limit.
        adj.Update(Hands(1.0f, AimPitched(-79), {}), head, 0.1f, true, p);
        REQUIRE(p.pitch_deg == Approx(kMinPitchDeg + 10.0f).margin(1e-3));
    }
    SECTION("let go: released once, then idle") {
        r = adj.Update(Hands(0.3f, AimPitched(0)), head, 0.011f, true, p);
        REQUIRE(r.released);
        REQUIRE_FALSE(r.active);
        r = adj.Update(Hands(0.3f, AimPitched(0)), head, 0.011f, true, p);
        REQUIRE_FALSE(r.released);
    }
    SECTION("a grip already held does not start it") {
        Adjuster other;
        Placement q;
        other.Update(Hands(1.0f, AimPitched(0)), head, 0.011f, false, q); // held, disabled
        r = other.Update(Hands(1.0f, AimPitched(0)), head, 0.011f, true, q);
        REQUIRE_FALSE(r.active);
    }
    SECTION("disabled (immersive mode paused) cancels without saving") {
        r = adj.Update(Hands(1.0f, AimPitched(-10)), head, 0.011f, false, p);
        REQUIRE_FALSE(r.active);
        REQUIRE_FALSE(r.released);
        REQUIRE_FALSE(adj.Active());
    }
}

TEST_CASE("FrameXr immersive: aim elevation on the screen's sphere", "[frame_xr]") {
    // A hand below the eye aiming at a point 10 degrees below straight ahead, 0.75 m away.
    const float d = 0.75f;
    const Vec3f target{0.0f, -d * std::sin(Rad(10)), -d * std::cos(Rad(10))};
    const Vec3f hand{0.15f, -0.4f, -0.3f};
    const Posef aim{In::FaceViewer(target, hand, Vec3f{0.0f, 1.0f, 0.0f}), hand};
    REQUIRE(AimElevation(Posef{}, aim, d) == Approx(Rad(-10)).margin(1e-4));
}

TEST_CASE("FrameXr immersive: the per-game file", "[frame_xr]") {
    Store store;
    Placement p;
    p.pitch_deg = -9.5f;
    p.distance = 1.25f;
    p.size = 0.9f;
    store.Set(0x0004000000033500ULL, false, p);
    store.Set(0x0004000000033500ULL, true, Placement{});
    Store again;
    again.Parse(store.Format());
    REQUIRE(again.Size() == 2);
    REQUIRE(again.Get(0x0004000000033500ULL, false).value() == p);
    REQUIRE_FALSE(again.Get(0x0004000000030800ULL, false).has_value());

    SECTION("bad lines are skipped, values clamped") {
        Store s;
        s.Parse("# comment\n"
                "0004000000030800 head -90 9 5\n"
                "zzzz head 1 1 1\n"
                "0004000000030800 sideways 1 1 1\n"
                "0004000000030800 wide 1 1\n"
                "0004000000054000 wide -3 0.8 1.1 # trailing\n");
        REQUIRE(s.Size() == 2);
        const Placement c = s.Get(0x0004000000030800ULL, false).value();
        REQUIRE(c.pitch_deg == Approx(kMinPitchDeg));
        REQUIRE(c.distance == Approx(kMaxDistance));
        REQUIRE(c.size == Approx(kMaxSize));
        REQUIRE(s.Get(0x0004000000054000ULL, true)->size == Approx(1.1f));
    }
    SECTION("new lines are world lines") {
        REQUIRE(store.Format().find("0004000000033500 world -9.50 1.250 0.900") !=
                std::string::npos);
        REQUIRE_FALSE(again.OldSize(0x0004000000033500ULL));
    }
    SECTION("erase") {
        again.Erase(0x0004000000033500ULL, false);
        REQUIRE_FALSE(again.Get(0x0004000000033500ULL, false).has_value());
        REQUIRE(again.Get(0x0004000000033500ULL, true).has_value());
    }
}

TEST_CASE("FrameXr immersive: files of earlier builds", "[frame_xr]") {
    // AzaharFrame immersive: "head" lines (1.0, 1.1) and "view" lines (1.2.0 test builds, the
    // owner's Mario Kart 7) had sizes against smaller defaults: their size is not used (the game's
    // default is), the pitch and distance are.
    const std::string old_file = "# Azahar for Frame: immersive screen placement per game\n"
                                 "0004000000033500 head -4.30 0.811 0.960\n"
                                 "0004000000030800 view 1.12 0.750 1.454\n";
    Store s;
    s.Parse(old_file);
    REQUIRE(s.Size() == 2);
    REQUIRE(s.OldSize(0x0004000000033500ULL));
    REQUIRE(s.OldSize(0x0004000000030800ULL));
    REQUIRE(s.Get(0x0004000000030800ULL, false)->pitch_deg == Approx(1.12f));
    REQUIRE(s.Get(0x0004000000030800ULL, false)->distance == Approx(0.75f));
    // Kept as old lines until replaced.
    Store kept;
    kept.Parse(s.Format());
    REQUIRE(kept.OldSize(0x0004000000033500ULL));
    REQUIRE(kept.Get(0x0004000000033500ULL, false)->distance == Approx(0.811f));
    // Replaced: a world line.
    Placement now = s.Get(0x0004000000030800ULL, false).value();
    now.size = 1.1f;
    s.Set(0x0004000000030800ULL, false, now);
    REQUIRE_FALSE(s.OldSize(0x0004000000030800ULL));
    REQUIRE(s.Format().find("0004000000030800 world 1.12 0.750 1.100") != std::string::npos);

    SECTION("the view-filling build's screen lines: pitch and distance kept, size not") {
        // AzaharFrame immersive2: the owner's Majora's Mask 3D, saved by the 1.2.0 test build.
        Store m;
        m.Parse("0004000000125600 screen -9.21 3.000 0.600\n");
        REQUIRE(m.OldSize(0x0004000000125600ULL));
        REQUIRE(m.Get(0x0004000000125600ULL, false)->pitch_deg == Approx(-9.21f));
        REQUIRE(m.Get(0x0004000000125600ULL, false)->distance == Approx(3.0f));
    }
    SECTION("a world line wins over an old line, in either order") {
        for (const char* text :
             {"0004000000054000 view -6 0.75 1.5\n0004000000054000 world -2 1 1.2\n",
              "0004000000054000 world -2 1 1.2\n0004000000054000 head -6 0.75 1.5\n",
              "0004000000054000 screen -6 0.75 0.6\n0004000000054000 world -2 1 1.2\n"}) {
            Store m;
            m.Parse(text);
            REQUIRE(m.Size() == 1);
            REQUIRE_FALSE(m.OldSize(0x0004000000054000ULL));
            REQUIRE(m.Get(0x0004000000054000ULL, false)->size == Approx(1.2f));
        }
    }
    SECTION("wide lines are unchanged") {
        Store w;
        w.Parse("0004000000033500 wide -3 0.8 1.1\n");
        REQUIRE(w.Get(0x0004000000033500ULL, true)->size == Approx(1.1f));
        REQUIRE(w.Format().find("0004000000033500 wide -3.00 0.800 1.100") != std::string::npos);
    }
}

TEST_CASE("FrameXr immersive: mode 3 patches only world cameras", "[frame_xr]") {
    using VideoCore::Immersive::LooksLikeWorldView;
    // A game camera (Mario Kart 7's, from a mock run).
    const float camera[3][3] = {{0.89f, 0.0f, -0.45f}, {-0.30f, 0.75f, -0.60f}, {0.33f, 0.67f, 0.67f}};
    REQUIRE(LooksLikeWorldView(camera));
    const float race[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 0.98f, -0.18f}, {0.0f, 0.18f, 0.98f}};
    REQUIRE(LooksLikeWorldView(race));
    // A 2D layout camera straight down -Z.
    const float layout[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    REQUIRE_FALSE(LooksLikeWorldView(layout));
    // Other data in the register (a menu layer's).
    const float junk[3][3] = {{0.19f, 0.93f, 0.97f}, {21.0f, 44.0f, -25.0f}, {0.19f, 0.84f, 0.97f}};
    REQUIRE_FALSE(LooksLikeWorldView(junk));
    const float scaled[3][3] = {{2.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}, {0.0f, 0.0f, 2.0f}};
    REQUIRE_FALSE(LooksLikeWorldView(scaled));
    const float skew[3][3] = {{1.0f, 0.0f, 0.0f}, {0.6f, 0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    REQUIRE_FALSE(LooksLikeWorldView(skew));
}

// AzaharFrame depth
TEST_CASE("FrameXr immersive: a known register still searches for the eye indicator",
          "[frame_xr]") {
    using VideoCore::Immersive::RegisterSearchFor;
    // Super Mario 3D Land's and Mario Kart 7's preset register (90), no eye indicator set:
    // the eye indicator is still searched for, or both eyes get the same picture.
    const auto preset = RegisterSearchFor(90, false);
    REQUIRE_FALSE(preset.view_matrix);
    REQUIRE(preset.eye_indicator);
    REQUIRE(preset.Any());
    // Auto: both, as CitraVR did.
    const auto automatic = RegisterSearchFor(-1, false);
    REQUIRE(automatic.view_matrix);
    REQUIRE(automatic.eye_indicator);
    // Both set: nothing to search for.
    REQUIRE_FALSE(RegisterSearchFor(90, true).Any());
    // Only the eye indicator set: the register is searched for.
    const auto eye_set = RegisterSearchFor(-1, true);
    REQUIRE(eye_set.view_matrix);
    REQUIRE_FALSE(eye_set.eye_indicator);
}

// ---- AzaharFrame vrsize: the Immersive Offset ----

TEST_CASE("FrameXr immersive: the offset's steps and limits", "[frame_xr]") {
    REQUIRE(ClampedOffset({0.26f, -0.74f}) == LensOffset{0.5f, -0.5f});
    REQUIRE(ClampedOffset({25.0f, -25.0f}) == LensOffset{kMaxOffsetDeg, -kMaxOffsetDeg});
    REQUIRE(ClampedOffset({NAN, INFINITY}) == LensOffset{});
    // -0.1 rounds to 0, never -0 (the toast and the file would show "-0.0").
    REQUIRE(!std::signbit(ClampedOffset({-0.1f, -0.1f}).right_deg));
    LensOffset o;
    o = NudgedOffset(o, 1, 0);
    o = NudgedOffset(o, 1, 0);
    o = NudgedOffset(o, 1, -1);
    REQUIRE(o == LensOffset{1.5f, -0.5f});
    // 100 presses right stop at the limit.
    REQUIRE(NudgedOffset(o, 100, 0).right_deg == Approx(kMaxOffsetDeg));
    REQUIRE(NudgedOffset(LensOffset{kMaxOffsetDeg, 0}, 1, 0).right_deg == Approx(kMaxOffsetDeg));
}

TEST_CASE("FrameXr immersive: the offset's toast", "[frame_xr]") {
    REQUIRE(OffsetToast({1.5f, 0.0f}, true) == "Offset Right 1.5\xC2\xB0");
    REQUIRE(OffsetToast({-0.5f, 0.0f}, true) == "Offset Left 0.5\xC2\xB0");
    REQUIRE(OffsetToast({0.0f, 2.0f}, false) == "Offset Up 2.0\xC2\xB0");
    REQUIRE(OffsetToast({3.0f, -10.0f}, false) == "Offset Down 10.0\xC2\xB0");
    REQUIRE(OffsetToast({0.0f, 1.0f}, true) == "Offset Centred");
}

TEST_CASE("FrameXr immersive: the offset turns the screen and the camera together",
          "[frame_xr]") {
    const ViewFov view = Symmetric(50);
    const HeadLocked base = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, view, 0.0f);
    const LensOffset offset{2.0f, 1.5f};
    const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, view, 0.0f, {}, offset);
    // Only turned: the same size, factor and gains.
    REQUIRE(s.tan_half_y == Approx(base.tan_half_y));
    REQUIRE(s.aspect == Approx(base.aspect));
    REQUIRE(s.gain == Approx(base.gain));
    REQUIRE(s.gain_x == Approx(base.gain_x));
    REQUIRE(s.distance == Approx(base.distance));
    REQUIRE(s.pitch_rad == Approx(base.pitch_rad + Rad(1.5f)));
    REQUIRE(s.yaw_rad == Approx(-Rad(2.0f)));
    // The same angles at every distance: the quad's centre is 2 degrees to the right and its
    // elevation 1.5 above the base's, wherever it is.
    for (const float d : {kMinDistance, kDefaultDistance, kMaxDistance}) {
        Placement p;
        p.distance = d;
        const HeadLocked sd = SolveHeadLocked(p, 60.0f, kAspect3ds, view, 0.0f, {}, offset);
        const HeadLocked bd = SolveHeadLocked(p, 60.0f, kAspect3ds, view, 0.0f);
        const Vec3f c = HeadLockedQuadPose(sd, 0.0f).position;
        const Vec3f cb = HeadLockedQuadPose(bd, 0.0f).position;
        REQUIRE(In::Length(c) == Approx(d));
        REQUIRE(std::atan2(c.x, -c.z) == Approx(Rad(2.0f)).epsilon(1e-3));
        REQUIRE(Elevation(c) - Elevation(cb) == Approx(Rad(1.5f)).epsilon(1e-3));
    }
    // The quad faces the eye, upright (no roll).
    const Posef pose = HeadLockedQuadPose(s, 0.0f);
    const Vec3f n = In::Rotate(pose.orientation, Vec3f{0.0f, 0.0f, 1.0f});
    REQUIRE(In::Dot(n, In::Normalized(pose.position * -1.0f)) == Approx(1.0f));
    REQUIRE(In::Rotate(pose.orientation, Vec3f{1.0f, 0.0f, 0.0f}).y == Approx(0.0f).margin(1e-6));
    // The world stays where it is: with the head still, the game camera looks where the quad's
    // centre is, so only the window onto the world moved.
    const Vec3f cam = Forward(GameCameraRotation(Quatf{}, s));
    REQUIRE(In::Dot(cam, In::Normalized(pose.position)) == Approx(1.0f));
    // Mode 3: each eye's quad is the same, moved by the eye.
    const Posef left = HeadLockedQuadPose(s, -0.032f);
    REQUIRE(left.position.x - pose.position.x == Approx(-0.032f));
    // Head turns still turn the camera by the gain on top of the offset.
    const Vec3f turned = Forward(GameCameraRotation(YawRotation(Rad(1.0f)), s));
    REQUIRE(std::atan2(-turned.x, -turned.z) - std::atan2(-cam.x, -cam.z) ==
            Approx(Rad(1.0f) * s.gain_x).epsilon(3e-2));
}

TEST_CASE("FrameXr immersive: the offset turns the wide screen too", "[frame_xr]") {
    const Posef rest{YawRotation(Rad(30)), Vec3f{0.0f, 1.6f, 0.0f}};
    const Posef plain = WideQuadPose(rest, 0.0f, 1.0f);
    const Posef turned = WideQuadPose(rest, 0.0f, 1.0f, -Rad(3.0f)); // 3 degrees right
    const Posef to_rest = In::Inverse(rest);
    const Vec3f a = In::Transform(to_rest, plain.position);
    const Vec3f b = In::Transform(to_rest, turned.position);
    REQUIRE(std::atan2(b.x, -b.z) - std::atan2(a.x, -a.z) == Approx(Rad(3.0f)).epsilon(1e-3));
    REQUIRE(In::Length(b) == Approx(1.0f));
    // Its bottom screen follows.
    const BottomQuad bp = WideBottomQuad(rest, 0.0f, 1.0f);
    const BottomQuad bt = WideBottomQuad(rest, 0.0f, 1.0f, -Rad(3.0f));
    const Vec3f pa = In::Transform(to_rest, bp.pose.position);
    const Vec3f pb = In::Transform(to_rest, bt.pose.position);
    REQUIRE(std::atan2(pb.x, -pb.z) - std::atan2(pa.x, -pa.z) == Approx(Rad(3.0f)).epsilon(1e-3));
}

TEST_CASE("FrameXr immersive: the offset setting is global and in range", "[frame_xr]") {
    auto& x = Settings::values.vr_immersive_offset_x;
    auto& y = Settings::values.vr_immersive_offset_y;
    REQUIRE(x.GetDefault() == 0.0f);
    REQUIRE(x.GetLabel() == "vr_immersive_offset_x");
    REQUIRE(y.GetLabel() == "vr_immersive_offset_y");
    x.SetValue(25.0f);
    y.SetValue(-25.0f);
    REQUIRE(x.GetValue() == Approx(kMaxOffsetDeg));
    REQUIRE(y.GetValue() == Approx(-kMaxOffsetDeg));
    // After a game it is the global value again (and the Qt config only reads and writes it in
    // the global file).
    x.SetGlobal(false);
    Settings::RestoreGlobalState(false);
    REQUIRE(x.UsingGlobal());
    REQUIRE(y.UsingGlobal());
    REQUIRE(x.GetValue() == Approx(kMaxOffsetDeg));
    x.SetValue(0.0f);
    y.SetValue(0.0f);
}

// AzaharFrame depth: the game camera's angle and shape from its projection registers.
TEST_CASE("FrameXr immersive: the projection gives the game camera's angle and shape",
          "[frame_xr]") {
    using VideoCore::Immersive::Projection;
    using VideoCore::Immersive::ProjectionFromRows;
    // Super Mario 3D Land's title camera (registers 86 to 89, mock run): the top screen turned
    // by 90 degrees, so the first row scales x; 45 degrees high, 5:3, with the 3D slider's shear.
    const float sy = 1.0f / std::tan(22.5f * 3.14159265f / 180.0f);
    const float sm3dl[4][4] = {{-sy / (5.0f / 3.0f), 0, 0.05229f, 0},
                               {0, sy, 0, 0},
                               {0, 0, 1.001f, 100.1f},
                               {0, 0, -1, 0}};
    Projection p;
    REQUIRE(ProjectionFromRows(sm3dl, p));
    REQUIRE(2.0f * std::atan(p.tan_half_y) * 180.0f / 3.14159265f == Approx(45.0f).margin(0.05));
    REQUIRE(p.aspect == Approx(5.0f / 3.0f).epsilon(0.001));
    // The rows the other way round, 16:9 (Ocarina of Time 3D with a widescreen mod).
    const float wide[4][4] = {{0, -sy, 0, 0}, {sy / (16.0f / 9.0f), 0, 0, 0}, {0, 0, 1, 7}, {0, 0, 1, 0}};
    REQUIRE(ProjectionFromRows(wide, p));
    REQUIRE(p.aspect == Approx(16.0f / 9.0f).epsilon(0.001));
    // Not a perspective matrix: an orthographic 2D camera, a view matrix, a silly angle.
    const float ortho[4][4] = {{0, 0.008f, 0, 0}, {0.005f, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    REQUIRE_FALSE(ProjectionFromRows(ortho, p));
    const float view[4][4] = {{1, 0, 0, 5}, {0, 1, 0, 2}, {0, 0, 1, -300}, {0, 0, 0, 1}};
    REQUIRE_FALSE(ProjectionFromRows(view, p));
    const float narrow[4][4] = {{-60, 0, 0, 0}, {0, 100, 0, 0}, {0, 0, 1, 1}, {0, 0, -1, 0}};
    REQUIRE_FALSE(ProjectionFromRows(narrow, p)); // about 1 degree high
    // Taller than wide: another matrix in the same registers (Ocarina of Time 3D has some).
    const float portrait[4][4] = {{-sy / 0.52f, 0, 0, 0}, {0, sy, 0, 0}, {0, 0, 1, 1}, {0, 0, -1, 0}};
    REQUIRE_FALSE(ProjectionFromRows(portrait, p));
}

TEST_CASE("FrameXr immersive: the projection most draws use wins each window", "[frame_xr]") {
    using VideoCore::Immersive::Projection;
    using VideoCore::Immersive::ProjectionTracker;
    ProjectionTracker t;
    Projection w;
    const Projection world{0.4142f, 5.0f / 3.0f}, hud{0.2f, 1.0f};
    int64_t ms = 1000;
    for (int i = 0; i < 30; i++) {
        REQUIRE_FALSE(t.Add(i % 3 == 0 ? hud : world, ms + i, w));
    }
    REQUIRE(t.Add(world, ms + ProjectionTracker::kWindowMs, w));
    REQUIRE(w.tan_half_y == Approx(world.tan_half_y));
    REQUIRE(w.aspect == Approx(world.aspect));
    // The next window starts afresh: a cut to another camera shows a quarter second later.
    ms += 1000;
    for (int i = 0; i < 10; i++) {
        REQUIRE_FALSE(t.Add(hud, ms + i, w));
    }
    REQUIRE(t.Add(hud, ms + ProjectionTracker::kWindowMs, w));
    REQUIRE(w.tan_half_y == Approx(hud.tan_half_y));
}

// AzaharFrame depth: Match Game Camera. Off leaves Immersive Game FOV and Screen Shape as they are
// and the emulation thread reads nothing; on, the draws are measured and the measured camera wins.
TEST_CASE("FrameXr immersive: Match Game Camera off measures nothing, on measures",
          "[frame_xr]") {
    using namespace FrameXr::Immersive::Layout;
    using VideoCore::Immersive::MeasureDraw;
    using VideoCore::Immersive::MeasuredProjection;
    using VideoCore::Immersive::ProjectionTracker;

    // The setting: Always by default (the earlier "on"), global and per game (AzaharFrame
    // fovonce: Off, Set Once, Always).
    REQUIRE(Settings::values.vr_immersive_match_camera_mode.GetDefault() == 2);

    // XR side: the settings untouched when off (also with a measurement in hand), the measured
    // camera when on, the settings again on while nothing was measured.
    CameraShape c = CameraInUse(false, 45.0f, 16.0f / 9.0f, 60.0f, 5.0f / 3.0f);
    REQUIRE(c.fov_y_deg == 60.0f);
    REQUIRE(c.aspect == 5.0f / 3.0f);
    c = CameraInUse(true, 45.0f, 16.0f / 9.0f, 60.0f, 5.0f / 3.0f);
    REQUIRE(c.fov_y_deg == 45.0f);
    REQUIRE(c.aspect == 16.0f / 9.0f);
    c = CameraInUse(true, 0.0f, 0.0f, 72.0f, 4.0f / 3.0f);
    REQUIRE(c.fov_y_deg == 72.0f);
    REQUIRE(c.aspect == 4.0f / 3.0f);

    // Emulation side: registers 86 to 89 hold Super Mario 3D Land's projection (45 degrees, 5:3),
    // 90 the view matrix. A register that counts reads, so "off" can be seen to read none.
    struct Reg {
        const float* v;
        int* reads;
        float operator[](int j) const {
            ++*reads;
            return v[j];
        }
    };
    const float sy = 1.0f / std::tan(22.5f * 3.14159265f / 180.0f);
    static float data[96][4] = {};
    data[86][0] = -sy / (5.0f / 3.0f);
    data[87][1] = sy;
    data[88][2] = 1.001f;
    data[88][3] = 100.1f;
    data[89][2] = -1.0f;
    int reads = 0;
    std::array<Reg, 96> regs;
    for (std::size_t i = 0; i < regs.size(); i++) {
        regs[i] = {data[i], &reads};
    }
    int clock_calls = 0;
    int64_t now = 5000;
    const auto clock = [&] {
        clock_calls++;
        return now;
    };
    ProjectionTracker tracker;
    MeasuredProjection out;
    float tan_half_y = 0.0f, aspect = 0.0f;
    int64_t stamp = 0;
    for (int i = 0; i < 100; i++, now += 5) {
        REQUIRE_FALSE(MeasureDraw(false, regs, regs.size(), 90, 3, tracker, out, clock));
    }
    REQUIRE(reads == 0);
    REQUIRE(clock_calls == 0);
    REQUIRE_FALSE(out.Read(0, tan_half_y, aspect, stamp));

    // On: a quarter second of draws publishes the camera.
    for (int i = 0; i < 60; i++, now += 5) {
        REQUIRE(MeasureDraw(true, regs, regs.size(), 90, 3, tracker, out, clock));
    }
    REQUIRE(reads > 0);
    REQUIRE(out.Read(0, tan_half_y, aspect, stamp));
    REQUIRE(2.0f * std::atan(tan_half_y) * 180.0f / 3.14159265f == Approx(45.0f).margin(0.05));
    REQUIRE(aspect == Approx(5.0f / 3.0f).epsilon(0.001));
    // Not a Full mode, or no room for the projection: nothing either.
    reads = 0;
    REQUIRE_FALSE(MeasureDraw(true, regs, regs.size(), 90, 1, tracker, out, clock));
    REQUIRE_FALSE(MeasureDraw(true, regs, regs.size(), 3, 2, tracker, out, clock));
    REQUIRE(reads == 0);
}

// AzaharFrame immersive: the measured camera split into the screen's camera and the live angle.
TEST_CASE("FrameXr immersive: a zoom changes the live angle, never the screen's camera",
          "[frame_xr]") {
    const auto tan_of = [](float fov_deg) { return std::tan(Rad(fov_deg / 2.0f)); };
    CameraFilter f;
    REQUIRE(f.ScreenFovDeg() == 0.0f);
    // The first camera: taken once two measurements agree, by the screen and the live angle.
    REQUIRE(f.Add(tan_of(55.0f), kAspect3ds) == CameraFilter::None);
    REQUIRE(f.Add(tan_of(55.0f), kAspect3ds) == (CameraFilter::Live | CameraFilter::Screen));
    REQUIRE(f.ScreenFovDeg() == Approx(55.0f));
    REQUIRE(f.LiveFovDeg() == Approx(55.0f));
    REQUIRE(f.ScreenAspect() == Approx(kAspect3ds));
    SECTION("an aim zooms to 40 degrees (and a different shape) for a second") {
        unsigned seen = 0;
        for (int i = 0; i < 4; i++) {
            seen |= f.Add(tan_of(40.0f), 1.9f);
        }
        REQUIRE(seen == CameraFilter::Live);
        REQUIRE(f.LiveFovDeg() == Approx(40.0f));
        REQUIRE(f.ScreenFovDeg() == Approx(55.0f));
        REQUIRE(f.ScreenAspect() == Approx(kAspect3ds));
        f.Add(tan_of(55.0f), kAspect3ds);
        REQUIRE(f.Add(tan_of(55.0f), kAspect3ds) == CameraFilter::Live);
        REQUIRE(f.LiveFovDeg() == Approx(55.0f));
    }
    SECTION("a single odd measurement is dropped") {
        REQUIRE(f.Add(tan_of(70.0f), kAspect3ds) == CameraFilter::None);
        REQUIRE(f.Add(tan_of(55.0f), kAspect3ds) == CameraFilter::None);
        REQUIRE(f.LiveFovDeg() == Approx(55.0f));
    }
    SECTION("a widescreen mod: the same height, wider: the screen's shape follows") {
        REQUIRE(f.Add(tan_of(54.7f), 2.22f) == CameraFilter::None);
        REQUIRE(f.Add(tan_of(54.7f), 2.22f) == CameraFilter::Screen);
        REQUIRE(f.ScreenAspect() == Approx(2.22f));
    }
    SECTION("a new scene: the screen's angle follows after kSettleCount measurements") {
        for (int i = 0; i < CameraFilter::kSettleCount - 1; i++) {
            REQUIRE((f.Add(tan_of(45.0f), kAspect3ds) & CameraFilter::Screen) == 0u);
        }
        REQUIRE(f.ScreenFovDeg() == Approx(55.0f));
        REQUIRE((f.Add(tan_of(45.0f), kAspect3ds) & CameraFilter::Screen) != 0u);
        REQUIRE(f.ScreenFovDeg() == Approx(45.0f));
        REQUIRE(f.ScreenAspect() == Approx(kAspect3ds));
    }
    SECTION("a wobble within 1.5 degrees changes nothing; angles are clamped") {
        REQUIRE(f.Add(tan_of(55.8f), kAspect3ds) == CameraFilter::None);
        REQUIRE(f.Add(tan_of(54.4f), kAspect3ds) == CameraFilter::None);
        CameraFilter c;
        c.Add(tan_of(10.0f), kAspect3ds);
        c.Add(tan_of(10.0f), kAspect3ds);
        REQUIRE(c.ScreenFovDeg() == Approx(30.0f));
        REQUIRE(c.Add(std::nanf(""), kAspect3ds) == CameraFilter::None);
    }
    SECTION("reset forgets it") {
        f.Reset();
        REQUIRE(f.ScreenFovDeg() == 0.0f);
        REQUIRE(f.LiveFovDeg() == 0.0f);
    }
}

// AzaharFrame immersive: mode 2 patches only draws through a perspective camera.
TEST_CASE("FrameXr immersive: mode 2 leaves screen-space passes alone", "[frame_xr]") {
    using VideoCore::Immersive::HasPerspectiveProjection;
    const float sy = 1.0f / std::tan(Rad(27.5f));
    // Ocarina of Time 3D's camera (registers 0 to 3): 55 degrees, 5:3.
    const float world[4][4] = {{0, -sy, 0, 0}, {sy / kAspect3ds, 0, 0, 0}, {0, 0, 1.0f, 7.0f},
                               {0, 0, -1, 0}};
    REQUIRE(HasPerspectiveProjection(world));
    // Its HUD (the value CitraVR skipped, f[0][3] = 1) and a full-screen pass in clip space.
    const float hud[4][4] = {{0, -0.008f, 0, 1}, {0.005f, 0, 0, -1}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    REQUIRE_FALSE(HasPerspectiveProjection(hud));
    const float pass[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    REQUIRE_FALSE(HasPerspectiveProjection(pass));
    const float broken[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, std::nanf(""), 0}};
    REQUIRE_FALSE(HasPerspectiveProjection(broken));
}

// AzaharFrame immersive: one head pose per game frame (both eyes).
TEST_CASE("FrameXr immersive: the head pose changes only between game frames", "[frame_xr]") {
    using VideoCore::Immersive::FrameLatch;
    FrameLatch latch;
    // A 30 frames a second game: a swap every other VBlank. The VBlank between two swaps (the
    // middle of a frame) never changes the pose.
    for (int frame = 0; frame < 10; frame++) {
        REQUIRE(latch.OnTopSwap());
        REQUIRE_FALSE(latch.OnVBlank());
        REQUIRE_FALSE(latch.OnVBlank());
    }
    // No swap at all (a loading screen): every kMaxVBlanks VBlanks.
    int applied = 0;
    for (int v = 0; v < 4 * FrameLatch::kMaxVBlanks; v++) {
        applied += latch.OnVBlank() ? 1 : 0;
    }
    REQUIRE(applied == 4);
}

// AzaharFrame doubling: the head pose a picture was drawn with goes through the mailbox, and the
// picture on the screens after a latch is the game frame that just ended, drawn with the pose
// applied at the latch before (not the one applied now, which the next frame is drawn with).
TEST_CASE("FrameXr immersive: the picture's head pose follows the game frame", "[frame_xr]") {
    using VideoCore::XrContentPose;
    using VideoCore::Immersive::ContentTrack;
    using VideoCore::Immersive::Mailbox;
    using VideoCore::Immersive::VRData;
    const auto pose = [](int64_t time, float yaw_w) {
        XrContentPose p;
        p.valid = true;
        p.orientation[1] = std::sqrt(1.0f - yaw_w * yaw_w);
        p.orientation[3] = yaw_w;
        p.position[0] = 0.25f;
        p.time = time;
        return p;
    };
    Mailbox box;
    VRData d;
    d.mode = 2;
    d.pose = pose(1000, 0.9f);
    box.Publish(d);
    VRData out;
    u32 serial = 0;
    REQUIRE(box.Read(out, serial));
    REQUIRE(out.pose.valid);
    REQUIRE(out.pose.time == 1000);
    REQUIRE(out.pose.orientation[3] == 0.9f);
    REQUIRE(out.pose.orientation[1] == d.pose.orientation[1]);
    REQUIRE(out.pose.position[0] == 0.25f);
    VRData off; // not immersive: no pose
    box.Publish(off);
    REQUIRE(box.Read(out, serial));
    REQUIRE_FALSE(out.pose.valid);

    ContentTrack track;
    REQUIRE_FALSE(track.Drawn().valid);
    track.Applied(pose(1, 1.0f)); // frame 1 is drawn with pose 1
    track.Latch();                // frame 1 ends: on the screens now
    track.Applied(pose(2, 0.99f)); // frame 2 gets pose 2
    REQUIRE(track.Drawn().time == 1);
    // A 30 frames a second game: the VBlank in the middle of frame 2 is no latch; the screens
    // still show frame 1.
    REQUIRE(track.Drawn().time == 1);
    track.Latch();
    track.Applied(pose(3, 0.98f));
    REQUIRE(track.Drawn().time == 2);
    // No new snapshot at a latch (the mailbox was busy): the next frame keeps pose 3.
    track.Latch();
    track.Latch();
    REQUIRE(track.Drawn().time == 3);
}

// AzaharFrame immersive: the owner's report: after a resolution change (Internal Resolution,
// Dynamic Resolution, a game's own option) the character was off to one side and the camera
// swung round it. Nothing about the render size goes into the rest pose, the screen or the
// camera; the rest pose is taken again only at the start, on resuming after a pause (where such
// changes are made) and on a recenter.
TEST_CASE("FrameXr immersive: a resolution change keeps the centre", "[frame_xr]") {
    RestPose rest;
    REQUIRE_FALSE(rest.Update(true, false, 1)); // no head pose yet
    REQUIRE(rest.Update(true, true, 2));
    REQUIRE(rest.Have());
    // Resolution changes while immersive mode stays on: the rest pose stays.
    for (int64_t t = 3; t < 300; ++t) {
        REQUIRE_FALSE(rest.Update(true, true, t));
    }
    // The screen, its pose and the game camera for one head pose at every render scale: the
    // same inputs (no render size among them) give the same centre, offset and turn.
    const ViewFov view = Symmetric(50);
    const LensOffset offset{6.0f, 0.0f};
    const HeadLocked base = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, view, 0.0f, {}, offset);
    const Quatf head = YawRotation(Rad(10.0f));
    const Vec3f cam = Forward(GameCameraRotation(head, base));
    const Vec3f centre = HeadLockedQuadPose(base, 0.0f).position;
    for (uint32_t scale = 1; scale <= 10; ++scale) {
        const HeadLocked s =
            SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, view, 0.0f, {}, offset);
        REQUIRE(s.yaw_rad == base.yaw_rad);
        REQUIRE(s.tan_half_y == base.tan_half_y);
        REQUIRE(s.gain_x == base.gain_x);
        REQUIRE(In::Dot(Forward(GameCameraRotation(head, s)), cam) == Approx(1.0f));
        REQUIRE(In::Length(HeadLockedQuadPose(s, 0.0f).position - centre) == Approx(0.0f));
    }
    SECTION("resuming after a pause re-centres on where the head is now") {
        REQUIRE_FALSE(rest.Update(false, true, 300)); // in-game menu, bottom screen
        REQUIRE_FALSE(rest.Update(false, true, 301));
        REQUIRE(rest.Update(true, true, 302));
        REQUIRE_FALSE(rest.Update(true, true, 303));
    }
    SECTION("a recentre takes the first head pose at or after its time") {
        rest.Recentre(400);
        REQUIRE_FALSE(rest.Update(true, true, 399));
        REQUIRE(rest.Update(true, true, 400));
        REQUIRE_FALSE(rest.Update(true, true, 401));
    }
    SECTION("a recentre in a pause is taken once immersive mode is on again") {
        REQUIRE_FALSE(rest.Update(false, true, 300));
        rest.Recentre(0);
        REQUIRE_FALSE(rest.Update(false, true, 301));
        REQUIRE(rest.Update(true, true, 302));
    }
    SECTION("the next game takes its own") {
        rest.Forget();
        REQUIRE_FALSE(rest.Have());
        REQUIRE(rest.Update(true, true, 500));
    }
}

// AzaharFrame jump: the Frame's eye views as located in the owner's Mario Kart 7 launches of
// 2026-11-12 switched between two shapes (the right eye 1.9 degrees up or down against the left:
// the both-eyes view cut at the top or the bottom), and the screen refitted each frame jumped
// up and down with them.
namespace {
ViewFov FrameView(float right_eye_pitch_deg) {
    const EyeFov l{Rad(-59.1f), Rad(50.4f), Rad(49.5f), Rad(-60.0f)};
    EyeFov r{Rad(-50.4f), Rad(59.1f), Rad(49.5f), Rad(-59.6f)};
    r.pitch = Rad(right_eye_pitch_deg);
    return BothEyes(l, r);
}
HeadLocked SolveMk7(const ViewFov& view) {
    Placement p;
    p.pitch_deg = -6.0f;
    p.distance = 0.371f;
    p.size = 2.022f;
    Options o;
    o.wider = 1.3f;
    o.squeeze = 0.30f;
    o.squeeze_y = 0.15f;
    return SolveHeadLocked(p, 40.0f, kAspect3ds, view, 0.0f, o, LensOffset{0.5f, 0.0f});
}
} // namespace

TEST_CASE("FrameXr immersive: views that change each frame refit the screen each frame",
          "[frame_xr]") {
    // What happened without ViewSettle: the screen's pitch and the game's up-and-down factor
    // follow each frame's view (the log's "-10.1 / -11.9 degrees from straight ahead" and
    // "1.64 / 1.55 up and down" for one placement).
    const HeadLocked up = SolveMk7(FrameView(1.9f));
    const HeadLocked down = SolveMk7(FrameView(-1.9f));
    REQUIRE(std::abs(up.pitch_rad - down.pitch_rad) > Rad(1.5f));
    // AzaharFrame oot: the game's up-and-down factor is Render Margin's own now, whatever the view.
    REQUIRE(up.factor_y == Approx(down.factor_y));
    REQUIRE(up.factor_y == Approx(up.factor));
}

TEST_CASE("FrameXr immersive: the screen is fitted to a settled view", "[frame_xr]") {
    ViewSettle settle;
    REQUIRE_FALSE(settle.Has());
    // The first frame: something to fit to at once.
    REQUIRE(settle.Add(FrameView(1.9f), 0.032f));
    REQUIRE(settle.Has());
    int changes = 0;
    for (int i = 1; i < kViewSettleFirstFrames; i++) {
        changes += settle.Add(FrameView(i % 2 ? -1.9f : 1.9f), 0.032f) ? 1 : 0;
    }
    // The first mean (half of each shape) replaces the first frame's view.
    REQUIRE(changes == 1);
    const ViewFov fitted = settle.View();
    const HeadLocked first = SolveMk7(fitted);
    // From then on the alternating views never move the screen.
    for (int i = 0; i < 20 * kViewSettleBlockFrames; i++) {
        REQUIRE_FALSE(settle.Add(FrameView(i % 2 ? 1.9f : -1.9f), 0.032f));
    }
    REQUIRE(settle.Distance(fitted) == 0.0f);
    const HeadLocked later = SolveMk7(settle.View());
    REQUIRE(later.pitch_rad == first.pitch_rad);
    REQUIRE(later.factor_y == first.factor_y);
    // Between the two shapes, about where a still view puts it.
    const HeadLocked still = SolveMk7(FrameView(0.0f));
    REQUIRE(std::abs(first.pitch_rad - still.pitch_rad) < Rad(0.3f));
    REQUIRE(settle.HalfIpd() == Approx(0.032f));
}

TEST_CASE("FrameXr immersive: a lasting view change refits the screen once", "[frame_xr]") {
    ViewSettle settle;
    for (int i = 0; i < kViewSettleFirstFrames; i++) {
        settle.Add(FrameView(0.0f), 0.032f);
    }
    const ViewFov before = settle.View();
    // A short change (well under a block) does nothing.
    int changes = 0;
    for (int i = 0; i < 20; i++) {
        changes += settle.Add(FrameView(4.0f), 0.032f) ? 1 : 0;
    }
    for (int i = 20; i < kViewSettleBlockFrames; i++) {
        changes += settle.Add(FrameView(0.0f), 0.032f) ? 1 : 0;
    }
    REQUIRE(changes == 0);
    REQUIRE(settle.Distance(before) == 0.0f);
    // A change held for a whole block (the IPD wheel): taken at the block's end, once.
    for (int i = 0; i < 3 * kViewSettleBlockFrames; i++) {
        changes += settle.Add(FrameView(0.0f), 0.034f) ? 1 : 0;
    }
    REQUIRE(changes == 1);
    REQUIRE(settle.HalfIpd() == Approx(0.034f));
    // Nonsense from the runtime is left out.
    REQUIRE_FALSE(settle.Add(FrameView(0.0f), std::nanf("")));
}

// AzaharFrame turns: the anchored screen about the eye where it is now.
TEST_CASE("FrameXr immersive: the anchored screen keeps its scenery still when the eyes move",
          "[frame_xr]") {
    const ViewFov view = Symmetric(45.0f);
    Placement p;
    p.distance = 0.37f; // Mario Kart 7's
    const HeadLocked s = SolveHeadLocked(p, 52.0f, kAspect3ds, view, 0.0f);
    const float neck = 0.09f, turn = Rad(5.0f);
    // The picture drawn with the head straight ahead; the head now turned 5 degrees left about a
    // neck 9 cm behind the eyes, so the eyes moved too.
    const Posef picture{Quatf{}, Vec3f{0.0f, 1.6f, 0.0f}};
    const Posef now{YawRotation(turn), Vec3f{-neck * std::sin(turn), 1.6f,
                                             neck - neck * std::cos(turn)}};
    for (const float x : {-0.032f, 0.0f, 0.032f}) {
        const Posef in_view = HeadLockedQuadPose(s, x);
        const Vec3f drawn = In::Normalized(In::Rotate(picture.orientation, in_view.position -
                                                                               Vec3f{x, 0, 0}));
        const Vec3f eye_now = now.position + In::Rotate(now.orientation, Vec3f{x, 0.0f, 0.0f});
        // About the eye now: the screen's middle is seen exactly where its picture was drawn.
        const Posef follow = AnchoredQuadPose(picture, now, in_view, x, true);
        REQUIRE(In::Dot(In::Normalized(follow.position - eye_now), drawn) ==
                Approx(1.0f).margin(1e-6));
        REQUIRE(In::Dot(In::Rotate(follow.orientation, Vec3f{0, 0, 1}),
                        In::Rotate(picture.orientation, In::Rotate(in_view.orientation,
                                                                   Vec3f{0, 0, 1}))) ==
                Approx(1.0f));
        // At the picture's head pose: off by about neck / distance of the turn (23 %).
        const Posef fixed = AnchoredQuadPose(picture, now, in_view, x, false);
        const float off = std::acos(std::min(
            1.0f, In::Dot(In::Normalized(fixed.position - eye_now), drawn)));
        REQUIRE(off / turn == Approx(neck / 0.37f).epsilon(0.15));
    }
    // The head where the picture was drawn: both are the old placement.
    const Posef in_view = HeadLockedQuadPose(s, 0.032f);
    const Posef a = AnchoredQuadPose(picture, picture, in_view, 0.032f, true);
    const Posef b = AnchoredQuadPose(picture, picture, in_view, 0.032f, false);
    REQUIRE(In::Length(a.position - b.position) == Approx(0.0f).margin(1e-6));
}

// AzaharFrame dbl: the anchored screen keeps the scenery of any one depth still while the eyes
// move (AZAHARFRAME_IMMERSIVE_ANCHOR_DEPTH): following the eyes' move x (1 - distance / z), every
// point of the screen is seen from the eye now in the direction of the point z / distance as far
// along its ray from the eye then (the scenery at z the picture shows there). 1 and 0 are the
// two placements above.
TEST_CASE("FrameXr immersive: the anchored screen keeps the scenery at a chosen depth still",
          "[frame_xr]") {
    std::mt19937 rng(20261120);
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    const ViewFov view = Symmetric(45.0f);
    for (int run = 0; run < 2000; ++run) {
        Placement p;
        p.distance = uni(0.3f, 3.0f);
        const HeadLocked s = SolveHeadLocked(p, uni(30.0f, 70.0f), kAspect3ds, view, 0.0f);
        const float turn = Rad(uni(-8.0f, 8.0f)), neck = uni(0.0f, 0.12f);
        const Posef picture{YawRotation(Rad(uni(-30.0f, 30.0f))),
                            Vec3f{uni(-0.2f, 0.2f), 1.6f, uni(-0.2f, 0.2f)}};
        const Posef now{In::Multiply(picture.orientation, YawRotation(turn)),
                        picture.position + In::Rotate(picture.orientation,
                                                      Vec3f{-neck * std::sin(turn) + uni(-0.01f, 0.01f),
                                                            uni(-0.01f, 0.01f),
                                                            neck - neck * std::cos(turn)})};
        const float z = uni(0.1f, 50.0f);
        const float x = uni(-0.035f, 0.035f);
        INFO("run " << run << " distance " << p.distance << " z " << z);
        const Posef in_view = HeadLockedQuadPose(s, x);
        const float follow = 1.0f - p.distance / z;
        const Posef placed = AnchoredQuadPose(picture, now, in_view, x, follow);
        const Vec3f eye_then = picture.position + In::Rotate(picture.orientation, Vec3f{x, 0, 0});
        const Vec3f eye_now = now.position + In::Rotate(now.orientation, Vec3f{x, 0, 0});
        for (const Vec3f corner : {Vec3f{0, 0, 0}, Vec3f{0.5f, 0.5f, 0}, Vec3f{-0.5f, 0.3f, 0}}) {
            const Vec3f local{corner.x * s.width(), corner.y * s.height(), 0.0f};
            // The point on the screen as placed, and where the picture drew it (from the eye then).
            const Vec3f on = placed.position + In::Rotate(placed.orientation, local);
            const Vec3f drawn = picture.position +
                                In::Rotate(picture.orientation,
                                           in_view.position + In::Rotate(in_view.orientation, local));
            const Vec3f scenery = eye_then + (drawn - eye_then) * (z / p.distance);
            REQUIRE(In::Dot(In::Normalized(on - eye_now), In::Normalized(scenery - eye_now)) ==
                    Approx(1.0f).margin(2e-6));
        }
        // 1: as following the eye; 0: as at the picture's head pose.
        const Posef far = AnchoredQuadPose(picture, now, in_view, x, 1.0f);
        const Posef near = AnchoredQuadPose(picture, now, in_view, x, 0.0f);
        REQUIRE(In::Length(far.position - AnchoredQuadPose(picture, now, in_view, x, true).position) ==
                Approx(0.0f).margin(1e-5));
        REQUIRE(In::Length(near.position -
                           AnchoredQuadPose(picture, now, in_view, x, false).position) ==
                Approx(0.0f).margin(1e-5));
    }
    // The follow for the game's plane: Mario Kart 7 (45 units ahead, head motion x90, yaw gain
    // 0.72) on a 3 m screen: as 0.36 m away. Not known: 1 (far, as before).
    CHECK(AnchorFollow(3.0f, 90.0f, 45.0f, 0.72f) == Approx(1.0f - 3.0f * 90.0f / 45.0f / 0.72f).epsilon(1e-5));
    CHECK(AnchorFollow(3.0f, 90.0f, -1.0f, 0.72f) == 1.0f);
    CHECK(AnchorFollow(3.0f, 0.0f, 45.0f, 0.72f) == 1.0f);
    CHECK(AnchorFollow(3.0f, 90.0f, 0.01f, 0.72f) == -20.0f);
}

// AzaharFrame turns2: the anchored screen for any settings, not only a preset's. Random but valid
// settings (mode, distance, size, pitch, Immersive Game FOV and Screen Shape or a measured camera,
// Render Margin, Horizontal and Vertical Scale, a zoom, the Immersive Offset, the view and the eyes) go
// through SolveHeadLocked and the quad Immersive::Screens shows (AnchoredFullQuad); the head turns
// (yaw, pitch, roll) about a neck behind and below the eyes and leans. Each eye must see every
// point of the screen (its middle and corners) in the same direction as when the picture was drawn:
// far scenery stays where the picture put it. With the head where the picture was drawn, the
// placement is the one before the anchoring followed the eye.
TEST_CASE("FrameXr immersive: the anchored screen keeps far scenery still for any settings",
          "[frame_xr]") {
    std::mt19937 rng(20261113);
    const auto uni = [&rng](float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng);
    };
    const auto pick = [&rng](uint32_t n) { return static_cast<uint32_t>(rng() % n); };
    const auto compose = [](const Posef& a, const Posef& b) {
        return Posef{In::Normalized(In::Multiply(a.orientation, b.orientation)),
                     a.position + In::Rotate(a.orientation, b.position)};
    };
    const auto angle = [](const Vec3f& a, const Vec3f& b) { // degrees, accurate near 0
        const double ax = a.x, ay = a.y, az = a.z, bx = b.x, by = b.y, bz = b.z;
        const double cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
        return std::atan2(std::sqrt(cx * cx + cy * cy + cz * cz), ax * bx + ay * by + az * bz) *
               180.0 / 3.14159265358979;
    };
    const auto ypr = [](float yaw, float pitch, float roll) {
        const Quatf r{0.0f, 0.0f, std::sin(roll / 2), std::cos(roll / 2)};
        return In::Normalized(In::Multiply(In::Multiply(YawRotation(yaw), PitchRotation(pitch)), r));
    };
    const float shapes[] = {400.0f / 240.0f, 16.0f / 9.0f, 21.0f / 9.0f,
                            4.0f / 3.0f,     3.0f / 2.0f,  16.0f / 10.0f};
    constexpr int kCombinations = 600, kTurns = 4;
    double worst = 0.0, worst_rest_pos = 0.0, worst_rest_deg = 0.0, worst_camera = 0.0;
    double worst_fixed = 0.0, worst_head_pivot = 0.0, worst_reported = 0.0;
    int placements = 0;
    for (int c = 0; c < kCombinations; c++) {
        const uint32_t mode = 2 + pick(2);
        const Placement p{uni(kMinPitchDeg, kMaxPitchDeg), uni(kMinDistance, kMaxDistance),
                          uni(kMinSize, kMaxSize)};
        // Two eyes with their own (asymmetric, canted) views.
        const EyeFov left{-Rad(uni(40, 65)), Rad(uni(30, 55)), Rad(uni(35, 60)), -Rad(uni(35, 60)),
                          Rad(uni(0, 10)), Rad(uni(-4, 4))};
        const EyeFov right{-Rad(uni(30, 55)), Rad(uni(40, 65)), Rad(uni(35, 60)),
                           -Rad(uni(35, 60)), -Rad(uni(0, 10)), Rad(uni(-4, 4))};
        const ViewFov view = BothEyes(left, right);
        const float half_ipd = uni(0.027f, 0.040f);
        Options o;
        o.wider = WiderFactor(pick(kMaxWiderLevel + 1));
        o.squeeze = SqueezeFor(pick(kMaxNarrowPercent + 1));
        o.squeeze_y = SqueezeFor(pick(kMaxNarrowPercent + 1));
        o.live_fov_y_deg = pick(3) == 0 ? 0.0f : uni(10, 150);
        const LensOffset offset{uni(-kMaxOffsetDeg, kMaxOffsetDeg), uni(-kMaxOffsetDeg, kMaxOffsetDeg)};
        // Match Game Camera with a measurement, or the settings (Immersive Game FOV, Screen
        // Shape); a game's own FOV sizes the screen (immersive.cpp OnFrame).
        const float setting_fov = uni(30, 110);
        const CameraShape cam = CameraInUse(pick(2) == 1, pick(3) ? uni(10, 150) : 0.0f,
                                            uni(1.2f, 3.0f), setting_fov, shapes[pick(6)]);
        const float fov = pick(2) ? setting_fov : cam.fov_y_deg;
        const HeadLocked s = SolveHeadLocked(p, fov, cam.aspect, view,
                                             SharedQuadEyeShift(mode, p, half_ipd), o, offset);
        REQUIRE(std::isfinite(s.tan_half_y));
        REQUIRE(s.width() > 0.0f);
        REQUIRE(s.height() > 0.0f);
        for (int t = 0; t < kTurns; t++) {
            const Posef picture{ypr(Rad(uni(-180, 180)), Rad(uni(-50, 50)), Rad(uni(-20, 20))),
                                Vec3f{uni(-1, 1), uni(1, 2), uni(-1, 1)}};
            // The head turned (in its own frame) about a neck `back` behind and `down` below the
            // eyes, and leaned a little.
            const Quatf turn = ypr(Rad(uni(-40, 40)), Rad(uni(-25, 25)), Rad(uni(-15, 15)));
            const Vec3f neck_in_head{0.0f, -uni(0, 0.1f), uni(0, 0.12f)};
            const Vec3f neck = picture.position + In::Rotate(picture.orientation, neck_in_head);
            Posef now;
            now.orientation = In::Normalized(In::Multiply(picture.orientation, turn));
            now.position = neck - In::Rotate(now.orientation, neck_in_head) +
                           Vec3f{uni(-0.05f, 0.05f), uni(-0.05f, 0.05f), uni(-0.05f, 0.05f)};
            for (int eye = 0; eye < 2; eye++) {
                const float x = EyeX(eye, half_ipd);
                const Posef in_view = FullQuadInView(s, mode, eye, half_ipd);
                const Posef drawn = compose(picture, in_view); // where the picture put it
                const Vec3f eye_then = picture.position + In::Rotate(picture.orientation, Vec3f{x, 0, 0});
                const Vec3f eye_now = now.position + In::Rotate(now.orientation, Vec3f{x, 0, 0});
                const Posef placed = AnchoredFullQuad(s, mode, eye, half_ipd, picture, now, true);
                const Posef fixed = AnchoredFullQuad(s, mode, eye, half_ipd, picture, now, false);
                const Posef head_pivot = AnchoredQuadPose(picture, now, in_view, 0.0f, true);
                for (const float sx : {0.0f, -0.5f, 0.5f}) {
                    for (const float sy : {0.0f, -0.5f, 0.5f}) {
                        const Vec3f u{sx * s.width(), sy * s.height(), 0.0f};
                        const auto at = [&u](const Posef& q) {
                            return q.position + In::Rotate(q.orientation, u);
                        };
                        const Vec3f then = at(drawn) - eye_then;
                        worst = std::max(worst, angle(at(placed) - eye_now, then));
                        worst_fixed = std::max(worst_fixed, angle(at(fixed) - eye_now, then));
                        if (mode == 2) {
                            worst_head_pivot =
                                std::max(worst_head_pivot, angle(at(head_pivot) - eye_now, then));
                        }
                    }
                }
                // What the per-second log line reports for it.
                const In::Vec2f e = FarSceneryError(picture, in_view, x, placed, eye_now);
                worst_reported = std::max<double>(worst_reported, std::hypot(e.x, e.y) / kDegToRad);
                // The head where the picture was drawn: the placement before the anchoring
                // followed the eye.
                const Posef rest = AnchoredFullQuad(s, mode, eye, half_ipd, picture, picture, true);
                worst_rest_pos = std::max<double>(worst_rest_pos, In::Length(rest.position - drawn.position));
                worst_rest_deg = std::max(worst_rest_deg, angle(In::Rotate(rest.orientation, Vec3f{0, 0, -1}),
                                                                In::Rotate(drawn.orientation, Vec3f{0, 0, -1})));
                worst_rest_deg = std::max(worst_rest_deg, angle(In::Rotate(rest.orientation, Vec3f{1, 0, 0}),
                                                                In::Rotate(drawn.orientation, Vec3f{1, 0, 0})));
                // There, the screen's middle is where the game camera looked (at the rest pose):
                // from the eye in mode 3, from the middle of the head in mode 2 (one picture).
                const Vec3f from = mode == 3 ? eye_then : picture.position;
                const Vec3f looked = In::Rotate(
                    picture.orientation, Forward(GameCameraRotation(Quatf{}, s)));
                worst_camera = std::max(worst_camera, angle(drawn.position - from, looked));
                placements++;
            }
        }
    }
    std::printf("anchored screen sweep: %d settings, %d eye placements; worst far scenery error "
                "%.5f degrees (reported %.5f), at the picture's pose %.2g m and %.5f degrees from "
                "the old placement, screen's middle %.5f degrees from the game camera; without "
                "following the eye %.2f degrees, mode 2 about the middle of the head %.3f degrees\n",
                kCombinations, placements, worst, worst_reported, worst_rest_pos, worst_rest_deg,
                worst_camera, worst_fixed, worst_head_pivot);
    REQUIRE(placements == kCombinations * kTurns * 2);
    REQUIRE(worst < 0.002);
    REQUIRE(worst_reported < 0.002);
    REQUIRE(worst_rest_pos < 2e-5);
    REQUIRE(worst_rest_deg < 0.002);
    REQUIRE(worst_camera < 0.002);
    // The sweep can see a slip: the old placement and mode 2 about the head are off.
    REQUIRE(worst_fixed > 1.0);
    REQUIRE(worst_head_pivot > 0.1);
}

// ---- AzaharFrame fovonce: Match Game Camera's Set Once (FovEstimate) and Always (FovFollow,
// FovEase), on made-up measurement sequences, four a second as ProjectionTracker gives them.
namespace {
using FrameXr::Immersive::Layout::FovEase;
using FrameXr::Immersive::Layout::FovEstimate;
using FrameXr::Immersive::Layout::FovFollow;
using FrameXr::Immersive::Layout::FovSlew;

// A little measurement noise, the same every run (+-0.4 degrees).
struct Noise {
    uint32_t s = 12345;
    float operator()() {
        s = s * 1664525u + 1013904223u;
        return (static_cast<float>(s >> 8) / 16777216.0f - 0.5f) * 0.8f;
    }
};

// Feeds `deg(t)` (0 or less: no camera) every 250 ms until the estimate stops or `max_ms`.
template <typename F>
int64_t RunEstimate(FovEstimate& e, F&& deg, int64_t max_ms = 90000) {
    int64_t t = 0;
    for (; t < max_ms && e.GetState() == FovEstimate::State::Measuring; t += 250) {
        e.Tick(250);
        const float d = deg(t);
        if (d > 0.0f) {
            e.Add(t, d);
        }
    }
    return t;
}
} // namespace

TEST_CASE("FrameXr immersive: Set Once finds a steady camera through cuts and cutscenes",
          "[frame_xr][fovonce]") {
    Noise noise;
    FovEstimate e;
    // Majora's Mask 3D-like: 52 degrees in play; every 5 s a cut, Z-targeting or flash to 30 or
    // 80 degrees for 1.5 s; and from 8 s a 5 s cutscene at 80 degrees (it holds, and counts).
    const int64_t took = RunEstimate(e, [&](int64_t t) {
        if (t >= 8000 && t < 13000) {
            return 80.0f + noise();
        }
        const int64_t in = t % 5000;
        if (in >= 3500) {
            return ((t / 5000) % 2 ? 30.0f : 80.0f) + noise();
        }
        return 52.0f + noise();
    });
    REQUIRE(e.GetState() == FovEstimate::State::Done);
    CHECK(e.Value() == 52);
    CHECK(took <= 60000);
    INFO("took " << took << " ms, counted " << e.CountedMs() << " ms, share " << e.Share());
    CHECK(e.CountedMs() >= FovEstimate::kEnoughMs);

    // Steady play only: done after about kEnoughMs of it (plus the first run's kHoldMs).
    FovEstimate steady;
    const int64_t quick = RunEstimate(steady, [&](int64_t) { return 47.3f + noise(); });
    REQUIRE(steady.GetState() == FovEstimate::State::Done);
    CHECK(steady.Value() == 47);
    CHECK(quick >= 10000);
    CHECK(quick <= 20000);

    // Spikes shorter than kHoldMs never count, however often: only the steady angle does.
    FovEstimate spiky;
    RunEstimate(spiky, [&](int64_t t) {
        return (t % 4000) < 1750 ? (t / 4000 % 2 ? 31.0f : 79.0f) : 60.0f + noise();
    });
    REQUIRE(spiky.GetState() == FovEstimate::State::Done);
    CHECK(spiky.Value() == 60);
}

TEST_CASE("FrameXr immersive: Set Once on a slow drift takes the angle it held most",
          "[frame_xr][fovonce]") {
    Noise noise;
    FovEstimate e;
    // 50 to 56 degrees over 30 s (a camera slowly pulling back), then 56.
    RunEstimate(e, [&](int64_t t) {
        return 50.0f + 6.0f * std::min(static_cast<float>(t) / 30000.0f, 1.0f) + noise();
    });
    REQUIRE(e.GetState() == FovEstimate::State::Done);
    CHECK(e.Value() >= 50);
    CHECK(e.Value() <= 56);
}

TEST_CASE("FrameXr immersive: Set Once with no camera, or none steady, gives up",
          "[frame_xr][fovonce]") {
    // No camera at all (a 2D game, the projection never found): 60 s, then nothing.
    FovEstimate none;
    const int64_t t = RunEstimate(none, [](int64_t) { return 0.0f; });
    CHECK(none.GetState() == FovEstimate::State::Failed);
    CHECK(t == FovEstimate::kGiveUpMs);
    CHECK(none.CountedMs() == 0);
    // A camera that never holds an angle for 2 s.
    FovEstimate restless;
    RunEstimate(restless, [](int64_t t) { return 35.0f + static_cast<float>((t / 1000) % 9) * 7.0f; });
    CHECK(restless.GetState() == FovEstimate::State::Failed);
    // Out of the setting's range (a 10 degree close-up): never counts.
    FovEstimate close;
    RunEstimate(close, [](int64_t) { return 12.0f; });
    CHECK(close.GetState() == FovEstimate::State::Failed);
    // Measuring time only counts while a game is on view (the caller ticks): measurements alone
    // never give up, and a Reset starts over.
    FovEstimate paused;
    paused.Tick(59000);
    CHECK(paused.GetState() == FovEstimate::State::Measuring);
    paused.Reset();
    CHECK(paused.MeasuringMs() == 0);
}

TEST_CASE("FrameXr immersive: Always ignores a reading that is back within half a second",
          "[frame_xr][fovonce]") {
    Noise noise;
    FovFollow f;
    FovSlew screen;
    std::vector<float> shown;
    int64_t t = 0;
    // 52 degrees for 5 s; a flash to 80 for two measurements (5.00 and 5.25 s), 52 again; at
    // 7 s a single reading at 30.
    for (; t < 10000; t += 250) {
        const bool flash = t >= 5000 && t < 5500;
        f.Add(t, flash ? 80.0f : t == 7000 ? 30.0f : 52.0f + noise() * 0.1f);
        CHECK(std::abs(f.Target() - 52.0f) < 0.1f);
        for (int64_t frame = t; frame < t + 250; frame += 8) { // 120 Hz frames in between
            shown.push_back(screen.Update(f.Target(), frame * 1'000'000));
        }
    }
    for (float v : shown) {
        CHECK(std::abs(v - 52.0f) < 0.1f);
    }
}

namespace {
// A cut from `from` to `to` (held), frame by frame at `hz` (each frame's length jittered by up to
// `jitter` of it, the same every run) with measurements every 250 ms through FovFollow and
// FovSlew. Checks every step and every change of speed against the limits; returns the seconds
// from the cut until the value settled on `to`.
double SlewedCut(float from, float to, double hz = 120.0, double jitter = 0.0) {
    FovFollow f;
    FovSlew slew;
    uint32_t rng = 777;
    float last = 0.0f, last_speed = 0.0f;
    double settled = -1.0;
    int64_t ns = 0, last_ns = 0, next_measure = 0;
    const int64_t cut_ns = 5'000'000'000;
    const double frame_ns = 1e9 / hz;
    for (int frame = 0; ns < 20'000'000'000; frame++) {
        if (ns >= next_measure) { // every 250 ms
            f.Add(ns / 1'000'000, ns >= cut_ns ? to : from);
            next_measure += 250'000'000;
        }
        const float v = slew.Update(f.Target(), ns);
        if (frame > 0) {
            const float dt = static_cast<float>(ns - last_ns) / 1e9f;
            const float step = v - last;
            INFO(hz << " Hz, frame " << frame << " value " << v << " step " << step);
            CHECK(std::abs(step) <= FovSlew::kMaxStepDeg + 1e-5f);
            CHECK(std::abs(step) <= FovSlew::kMaxSpeedDegS * dt + 1e-4f);
            CHECK(std::abs(slew.Speed() - last_speed) <= FovSlew::kAccelDegS2 * dt + 1e-3f);
            // never past the target (by more than a rounding when the frame length varies)
            const float slack = jitter > 0.0 ? 0.01f : 2e-4f;
            CHECK(std::min(from, to) - slack <= v);
            CHECK(v <= std::max(from, to) + slack);
            last_speed = slew.Speed();
        }
        last = v;
        if (ns > cut_ns && settled < 0.0 && v == to && !slew.Moving()) {
            settled = static_cast<double>(ns - cut_ns) / 1e9;
        }
        last_ns = ns;
        rng = rng * 1664525u + 1013904223u;
        const double j = (static_cast<double>(rng >> 8) / 16777216.0 - 0.5) * 2.0 * jitter;
        ns += static_cast<int64_t>(frame_ns * (1.0 + j));
    }
    CHECK(last == to);
    return settled;
}
} // namespace

TEST_CASE("FrameXr immersive: Always slews a cut at most 12 degrees a second, ramped both ends",
          "[frame_xr][fovonce]") {
    for (const double hz : {72.0, 90.0, 120.0, 144.0}) {
        for (const float size : {5.0f, 10.0f, 40.0f}) {
            for (const float sign : {1.0f, -1.0f}) {
                const float from = 55.0f, to = 55.0f + sign * size;
                INFO(hz << " Hz, cut " << from << " to " << to);
                const double secs = SlewedCut(from, to, hz);
                // Half a second (a measurement or so) to win, then size / 12 s plus the ramps.
                const double slew = secs - 0.5;
                CHECK(slew >= size / FovSlew::kMaxSpeedDegS - 0.05);
                CHECK(slew <= size / FovSlew::kMaxSpeedDegS + 3.0 * FovSlew::kRampS + 0.3);
            }
        }
    }
    // The same time at every refresh rate (AzaharFrame integ8: it was per frame, so 90 Hz took a
    // third longer than 120 Hz), and when the frames' lengths vary.
    const double at120 = SlewedCut(52.0f, 62.0f, 120.0);
    for (const double hz : {72.0, 90.0, 144.0}) {
        INFO(hz << " Hz against 120 Hz: " << SlewedCut(52.0f, 62.0f, hz) << " s, " << at120);
        CHECK(std::abs(SlewedCut(52.0f, 62.0f, hz) - at120) < 0.05);
    }
    // Frames 30 % longer or shorter at random: within a tenth of a second.
    CHECK(std::abs(SlewedCut(52.0f, 62.0f, 120.0, 0.3) - at120) < 0.1);
    CHECK(std::abs(SlewedCut(62.0f, 52.0f, 90.0, 0.3) - at120) < 0.1);
    // A 10 degree cut takes about a second once it won: 0.83 s at 12 degrees a second, plus the
    // ramps.
    CHECK(at120 - 0.5 <= 10.0 / FovSlew::kMaxSpeedDegS + 2.0 * FovSlew::kRampS + 0.3);

    // Slowly back when the camera returns half way: it slows down, turns and comes back, with
    // the same limits.
    FovSlew slew;
    constexpr int64_t kFrame = 1'000'000'000 / 120;
    slew.Update(50.0f, 0);
    float last = 50.0f, last_speed = 0.0f, highest = 50.0f;
    for (int frame = 1; frame < 600; frame++) {
        const float v = slew.Update(frame < 60 ? 70.0f : 50.0f, frame * kFrame);
        const float step = v - last;
        CHECK(std::abs(step) <= FovSlew::kMaxStepDeg + 1e-5f);
        CHECK(std::abs(slew.Speed() - last_speed) <=
              FovSlew::kAccelDegS2 * static_cast<float>(kFrame) / 1e9f + 1e-3f);
        highest = std::max(highest, v);
        last = v;
        last_speed = slew.Speed();
    }
    CHECK(highest < 70.0f);
    CHECK(last == 50.0f);
    CHECK(!slew.Moving());
}

TEST_CASE("FrameXr immersive: Always's slew: no time, nothing moves; a hitch moves one capped step",
          "[frame_xr][fovonce]") {
    FovSlew slew;
    slew.Update(50.0f, 1'000'000'000);
    // The same display time again, or an earlier one: no step.
    CHECK(slew.Update(70.0f, 1'000'000'000) == 50.0f);
    CHECK(slew.Update(70.0f, 900'000'000) == 50.0f);
    // At full speed, a frame of 100 ms (a hitch) moves no more than the per-frame cap.
    int64_t ns = 1'000'000'000;
    for (int i = 0; i < 120; i++) {
        ns += 1'000'000'000 / 120;
        slew.Update(70.0f, ns);
    }
    CHECK(slew.Speed() == Catch::Approx(FovSlew::kMaxSpeedDegS));
    const float before = slew.Value();
    ns += 100'000'000;
    const float after = slew.Update(70.0f, ns);
    CHECK(after - before > 0.0f);
    CHECK(after - before <= FovSlew::kMaxStepDeg + 1e-5f);
}

TEST_CASE("FrameXr immersive: Always keeps up with a gradual change",
          "[frame_xr][fovonce]") {
    // Mario Kart 7 widening with speed: 52 to 62 degrees over 3 s (3.3 degrees a second), then
    // back to 52 over 3 s, measured every 250 ms, shown at 120 Hz.
    FovFollow f;
    FovSlew live;
    float worst = 0.0f, last = 0.0f;
    const auto measured = [](int64_t ms) {
        const float up = std::clamp((ms - 1000) / 3000.0f, 0.0f, 1.0f);
        const float down = std::clamp((ms - 6000) / 3000.0f, 0.0f, 1.0f);
        return 52.0f + 10.0f * (up - down);
    };
    for (int frame = 0; frame < 12 * 120; frame++) {
        const int64_t ms = static_cast<int64_t>(frame) * 1000 / 120;
        if (frame % 30 == 0) {
            f.Add(ms, measured(ms));
        }
        const float v = live.Update(f.Target(), static_cast<int64_t>(frame) * 1'000'000'000 / 120);
        if (frame > 0) {
            CHECK(std::abs(v - last) <= FovSlew::kMaxStepDeg + 1e-5f);
        }
        last = v;
        worst = std::max(worst, std::abs(v - measured(ms)));
    }
    INFO("worst lag " << worst << " degrees");
    CHECK(worst < 3.0f); // about 0.6 s behind at 3.3 degrees a second
    CHECK(last == Catch::Approx(52.0f).margin(0.01f));
}

TEST_CASE("FrameXr immersive: Off and Set Once hand the angles over as before",
          "[frame_xr][fovonce]") {
    using FrameXr::Immersive::Layout::CameraAngles;
    // Off: exactly as given, every frame, also after a change.
    CameraAngles off;
    for (int64_t ms = 0; ms < 2000; ms += 8) {
        const float screen = ms < 1000 ? 55.0f : 40.0f, live = ms < 1000 ? 50.0f : 45.0f;
        const auto out = off.Update(0, screen, live, ms, ms * 1'000'000);
        CHECK(out.screen_deg == screen);
        CHECK(out.live_deg == live);
    }
    // Set Once: the value it wrote is eased in (FovEase), the same as before.
    CameraAngles once;
    FovEase ref_screen, ref_live;
    for (int64_t ms = 0; ms < 3000; ms += 8) {
        const float screen = ms < 1000 ? 50.0f : 40.0f;
        const auto out = once.Update(1, screen, screen, ms, ms * 1'000'000);
        CHECK(out.screen_deg == ref_screen.Update(screen, ms));
        CHECK(out.live_deg == ref_live.Update(screen, ms));
    }
    CHECK(once.Update(1, 40.0f, 40.0f, 3000, 3'000'000'000).screen_deg == 40.0f);
    // Always: slewed.
    CameraAngles always;
    always.Update(2, 50.0f, 50.0f, 0, 0);
    const auto a = always.Update(2, 40.0f, 40.0f, 8, 8'333'333);
    CHECK(a.screen_deg < 50.0f);
    CHECK(a.screen_deg >= 50.0f - FovSlew::kMaxStepDeg);
}

// AzaharFrame oot: the owner's headset test of Ocarina of Time 3D with a single-screen mod ("it
// says 5:3, but it is not 5:3", the item screen off kilter): at Render Margin Off the game still
// drew 1.25 times its height on the Frame, and more up and down than across at every level.
// Render Margin (was Wider View) now widens the game's own picture evenly. For any settings
// (Horizontal and Vertical Scale of either sign too): Off is exactly the
// game's frame, every level pads both ways by the same factor, the screen has the picture's own
// shape (over the scales), the scale is the same both ways and the gains are the padded
// game tangent over the screen's on each axis. Nothing here is fitted to a game or a preset.
TEST_CASE("FrameXr immersive: Render Margin pads the game's own shape evenly for any settings",
          "[frame_xr]") {
    std::mt19937 rng(20261113);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    const auto lerp = [&](float a, float b) { return a + (b - a) * u(rng); };
    int checked = 0;
    for (int i = 0; i < 2000; ++i) {
        ViewFov v;
        v.left = Rad(-lerp(35.0f, 60.0f));
        v.right = Rad(lerp(35.0f, 60.0f));
        v.down = Rad(-lerp(35.0f, 65.0f));
        v.up = Rad(lerp(30.0f, 60.0f));
        v.outer_left = v.left - Rad(lerp(0.0f, 10.0f));
        v.outer_right = v.right + Rad(lerp(0.0f, 10.0f));
        v.outer_down = v.down - Rad(lerp(0.0f, 2.0f));
        v.outer_up = v.up + Rad(lerp(0.0f, 2.0f));
        const uint32_t level = static_cast<uint32_t>(i % 5);
        const float aspect = lerp(1.2f, 2.4f);
        const float fov = lerp(30.0f, 110.0f);
        Placement p;
        p.size = lerp(kMinSize, kMaxSize);
        p.pitch_deg = lerp(-20.0f, 10.0f);
        p.distance = lerp(kMinDistance, kMaxDistance);
        Options o;
        o.wider = WiderFactor(level);
        // Horizontal and Vertical Scale, -30 % to +30 % (AzaharFrame kiu: larger above 0).
        o.squeeze = SqueezeForScale(5 * (i % 13) - kMaxScalePercent);
        o.squeeze_y = SqueezeForScale(5 * ((i / 13) % 13) - kMaxScalePercent);
        if (i % 3 == 0) {
            o.live_fov_y_deg = fov * lerp(0.7f, 1.2f); // a zoom
        }
        const float shift = (i % 2) ? lerp(0.0f, 0.035f / kMinDistance) : 0.0f;
        const HeadLocked s = SolveHeadLocked(p, fov, aspect, v, shift, o);
        const float t = std::tan(Rad(fov / 2.0f));
        const float live = o.live_fov_y_deg > 0.0f ? std::tan(Rad(o.live_fov_y_deg / 2.0f)) : t;
        // Even padding; Off is the game's own frame.
        REQUIRE(s.factor == Approx(WiderFactor(level)));
        REQUIRE(s.factor_y == Approx(s.factor));
        if (level == 0) {
            REQUIRE(s.factor == 1.0f);
            REQUIRE(s.factor_y == Approx(1.0f));
        }
        // The screen's shape: the picture's, times the scales.
        REQUIRE(s.aspect == Approx(aspect * (1.0f - o.squeeze) / (1.0f - o.squeeze_y)));
        // The same scale both ways before Horizontal and Vertical Scale: the screen's tangents over the game's.
        const float qx = s.tan_half_y * s.aspect / (1.0f - o.squeeze);
        const float qy = s.tan_half_y / (1.0f - o.squeeze_y);
        REQUIRE(qx / (s.factor * aspect * t) == Approx(qy / (s.factor_y * t)));
        // A point of the game's frame (its corner, at w = 1 inside the view) lands where the
        // frame gives it: the frame is 1 / factor of the screen both ways.
        REQUIRE((qx / s.factor) / (qy / s.factor_y) == Approx(aspect));
        // The gains: the padded game tangent over the screen's, per axis, within the limits.
        const float base = s.factor_y * live / qy;
        REQUIRE(s.gain == Approx(std::clamp(base / (1.0f - o.squeeze_y), kMinGain, kMaxGain)));
        REQUIRE(s.gain_x == Approx(std::clamp(base / (1.0f - o.squeeze), kMinGain, kMaxGain)));
        ++checked;
    }
    REQUIRE(checked == 2000);
}

// AzaharFrame oot: the picture is tagged with the pose its world was drawn with, followed through
// the copies into the framebuffer the game shows, not with the pose applied at the latch before.
namespace {
VideoCore::XrContentPose YawPose(int64_t time, float yaw_deg) {
    VideoCore::XrContentPose p;
    p.valid = true;
    const float h = yaw_deg * 3.14159265f / 360.0f;
    p.orientation[1] = std::sin(h);
    p.orientation[3] = std::cos(h);
    p.time = time;
    return p;
}
} // namespace

TEST_CASE("FrameXr immersive: the picture's pose is the one its world was drawn with",
          "[frame_xr]") {
    using VideoCore::Immersive::ContentTrack;
    using Kind = ContentTrack::Kind;
    constexpr u32 A = 0x18000000, B = 0x18046500;
    SECTION("a game drawing its world every frame: as before") {
        ContentTrack track, old(false);
        for (int f = 1; f < 40; ++f) {
            const u32 fb = f % 2 ? A : B;
            for (ContentTrack* t : {&track, &old}) {
                t->Applied(YawPose(f, 2.0f * f));
                t->WorldDraw();
                t->Transfer(fb);
                t->Latch(fb);
            }
            REQUIRE(track.Drawn().time == f);
            REQUIRE(track.Drawn().time == old.Drawn().time);
            REQUIRE(track.LastKind() == Kind::Same);
        }
    }
    SECTION("the world every other frame, the HUD in between (30 world pictures in 60 swaps)") {
        ContentTrack track, old(false);
        float worst_new = 0.0f, worst_old = 0.0f;
        int world_frame = 0;
        for (int f = 1; f < 60; ++f) {
            const u32 fb = f % 2 ? A : B;
            for (ContentTrack* t : {&track, &old}) {
                t->Applied(YawPose(f, 3.0f * f)); // a steady head turn, 3 degrees a frame
                if (f % 2) {
                    t->WorldDraw();
                }
                t->Transfer(fb); // the HUD composited over the last world, copied in
                t->Latch(fb);
            }
            if (f % 2) {
                world_frame = f;
            }
            const VideoCore::XrContentPose truth = YawPose(world_frame, 3.0f * world_frame);
            worst_new = std::max(worst_new, ContentTrack::TurnDeg(track.Drawn(), truth));
            worst_old = std::max(worst_old, ContentTrack::TurnDeg(old.Drawn(), truth));
            REQUIRE(track.Drawn().time == world_frame);
            REQUIRE(track.LastKind() == (f % 2 ? Kind::Same : Kind::Earlier));
            if (!(f % 2)) {
                REQUIRE(track.LastTurnDeg() == Approx(3.0f).margin(0.01f));
            }
        }
        REQUIRE(worst_new == Approx(0.0f).margin(0.01f));
        REQUIRE(worst_old == Approx(3.0f).margin(0.01f)); // a frame's turn: the double image
    }
    SECTION("the picture copied in before the next frame's world is drawn") {
        ContentTrack track;
        track.Applied(YawPose(1, 0.0f));
        track.WorldDraw();
        for (int f = 2; f < 20; ++f) {
            track.Applied(YawPose(f, 2.0f * f));
            track.Transfer(A); // frame f - 1's world into the framebuffer
            track.WorldDraw(); // then frame f's world
            track.Latch(A);
            REQUIRE(track.Drawn().time == f - 1);
            REQUIRE(track.LastKind() == Kind::Earlier);
        }
    }
    SECTION("nothing copied into the framebuffer shown lately: the earlier rule") {
        ContentTrack track;
        track.Applied(YawPose(1, 0.0f));
        track.WorldDraw();
        track.Transfer(A);
        track.Latch(A);
        for (int f = 2; f < 10; ++f) {
            track.Applied(YawPose(f, 1.0f * f));
            track.WorldDraw();
            track.Latch(B); // drawn straight into B
            REQUIRE(track.Drawn().time == f);
            REQUIRE(track.LastKind() == Kind::Untraced);
        }
        // An old copy into A no longer counts after kTransferLatches latches.
        track.Applied(YawPose(10, 10.0f));
        track.Latch(A);
        REQUIRE(track.Drawn().time == 10);
        // The VBlank fallback (no swap): the earlier rule.
        track.Applied(YawPose(11, 11.0f));
        track.Transfer(A);
        track.Latch();
        REQUIRE(track.Drawn().time == 11);
    }
    SECTION("outside the Full modes: no pose, nothing kept") {
        ContentTrack track;
        track.Applied(YawPose(1, 0.0f));
        track.WorldDraw();
        track.Transfer(A);
        track.Latch(A);
        track.Applied(VideoCore::XrContentPose{}); // immersive mode off
        track.Transfer(A);
        track.Latch(A);
        REQUIRE_FALSE(track.Drawn().valid);
    }
    SECTION("any turn rate and swap pattern: the tag is the world's pose") {
        std::mt19937 rng(7);
        for (int run = 0; run < 200; ++run) {
            ContentTrack track;
            const int every = 1 + static_cast<int>(rng() % 4); // world every 1 to 4 swaps
            const bool copy_first = rng() % 2;
            const float rate = static_cast<float>(rng() % 600) / 100.0f; // up to 6 deg a frame
            int world_frame = -1, copied_frame = -1;
            for (int f = 0; f < 80; ++f) {
                const u32 fb = (f % 2) ? A : B;
                track.Applied(YawPose(f, rate * f));
                const bool world = f % every == 0;
                if (copy_first) {
                    track.Transfer(fb);
                    copied_frame = world_frame;
                }
                if (world) {
                    track.WorldDraw();
                    world_frame = f;
                }
                if (!copy_first) {
                    track.Transfer(fb);
                    copied_frame = world_frame;
                }
                track.Latch(fb);
                if (copied_frame < 0) {
                    continue;
                }
                const VideoCore::XrContentPose truth = YawPose(copied_frame, rate * copied_frame);
                REQUIRE(ContentTrack::TurnDeg(track.Drawn(), truth) <= 0.01f);
            }
        }
    }
}

// AzaharFrame oot: Render Margin Off asks every game for exactly its own picture: no padding either
// way, the screen in the picture's shape, for every game shape, both Full modes (mode 2's shared
// quad with each eye's shift, mode 3's quad on each eye) and any view, the Frame's included.
TEST_CASE("FrameXr immersive: Render Margin Off is exactly the game's picture", "[frame_xr]") {
    ViewFov frame;
    frame.left = Rad(-50.3f);
    frame.right = Rad(50.4f);
    frame.down = Rad(-59.6f);
    frame.up = Rad(49.5f);
    frame.outer_left = Rad(-59.1f);
    frame.outer_right = Rad(59.2f);
    frame.outer_down = Rad(-60.0f);
    frame.outer_up = Rad(49.9f);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    std::vector<ViewFov> views{frame};
    for (int i = 0; i < 40; ++i) {
        ViewFov v;
        v.left = Rad(-(30.0f + 35.0f * u(rng)));
        v.right = Rad(30.0f + 35.0f * u(rng));
        v.down = Rad(-(30.0f + 35.0f * u(rng)));
        v.up = Rad(30.0f + 35.0f * u(rng));
        v.outer_left = v.left - Rad(10.0f * u(rng));
        v.outer_right = v.right + Rad(10.0f * u(rng));
        v.outer_down = v.down;
        v.outer_up = v.up;
        views.push_back(v);
    }
    std::vector<float> shapes{4.0f / 3.0f, kAspect3ds, 1.6f, 16.0f / 9.0f, 21.0f / 9.0f};
    for (int i = 0; i < 10; ++i) {
        shapes.push_back(1.1f + 1.5f * u(rng));
    }
    int checked = 0;
    for (const ViewFov& v : views) {
        for (const float a : shapes) {
            for (const float shift : {0.0f, 0.032f / 3.0f, 0.035f / kMinDistance}) {
                for (const float fov : {35.0f, 55.0f, 75.0f, 100.0f}) {
                    for (const float size : {kMinSize, 1.0f, kMaxSize}) {
                        Placement p;
                        p.size = size;
                        Options o; // Off: o.wider is kFactorMin
                        REQUIRE(o.wider == WiderFactor(0));
                        const HeadLocked s = SolveHeadLocked(p, fov, a, v, shift, o);
                        REQUIRE(s.factor == 1.0f);
                        REQUIRE(s.factor_y == Approx(1.0f).epsilon(1e-5));
                        REQUIRE(s.aspect == Approx(a).epsilon(1e-5));
                        ++checked;
                    }
                }
            }
        }
    }
    REQUIRE(checked == 41 * 15 * 3 * 4 * 3);
}
