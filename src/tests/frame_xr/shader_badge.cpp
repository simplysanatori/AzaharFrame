// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame shaderbadge: the "Preparing Shaders" badge's place (inside the main screen's
// top-right corner, or head-locked at the top right of what is in view) and its fade.

#include <algorithm>
#include <cmath>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/screen_rig.h"
#include "frame_xr/vis/shader_badge.h"

using namespace FrameXr;
using namespace FrameXr::Vis::Badge;
using Catch::Approx;

namespace {

constexpr float kDeg = 3.14159265f / 180.0f;
constexpr float kAspect = 6.0f; // "Preparing Shaders 3 / 9": about 384 x 64 px

Input::Posef Top() {
    Input::Posef p;
    p.position = {0.0f, 0.0f, -2.0f};
    return p;
}

/// A point of the badge in its parent space (`sx`, `sy` from -0.5 to 0.5 of its size).
Input::Vec3f Corner(const Placed& b, float sx, float sy) {
    return Input::Transform(b.pose, {sx * b.width, sy * b.height, 0.0f});
}

/// A point's angles seen from the head at the origin looking down -Z (radians).
float Yaw(const Input::Vec3f& p) {
    return std::atan2(p.x, -p.z);
}
float Pitch(const Input::Vec3f& p) {
    return std::atan2(p.y, -p.z);
}

Angles View(float half_deg) {
    return {-half_deg * kDeg, half_deg * kDeg, half_deg * kDeg, -half_deg * kDeg};
}

} // namespace

TEST_CASE("Shader badge: just inside the normal top screen's top-right corner", "[frame_xr]") {
    const float w = 5.0f / 3.0f, h = 1.0f;
    const Placed b = OnScreen(Top(), w, h, 0.0f, kAspect);
    REQUIRE(b.height == Approx(h * kScreenHeight));
    REQUIRE(b.width == Approx(b.height * kAspect));
    // Its top-right corner kScreenInset of the height in from both edges, in the screen's plane
    // (a few millimetres in front of it).
    const Input::Vec3f tr = Corner(b, 0.5f, 0.5f);
    REQUIRE(tr.x == Approx(w / 2 - h * kScreenInset));
    REQUIRE(tr.y == Approx(h / 2 - h * kScreenInset));
    REQUIRE(tr.z > -2.0f);
    REQUIRE(tr.z < -1.99f);
    const Input::Vec3f bl = Corner(b, -0.5f, -0.5f);
    REQUIRE(bl.x > 0.0f);          // in the right half
    REQUIRE(bl.y > h / 2 - 0.15f); // and the top strip only
    // About 2.3 degrees high at 2 m.
    REQUIRE(2.0f * std::atan(b.height / 2.0f / 2.0f) / kDeg == Approx(2.3f).margin(0.1f));
}

TEST_CASE("Shader badge: moves and turns with the screen", "[frame_xr]") {
    const float w = 5.0f / 3.0f, h = 1.0f;
    const Placed at_rest = OnScreen(Top(), w, h, 0.0f, kAspect);
    Input::Posef moved;
    const float a = 30.0f * kDeg;
    moved.orientation = {0.0f, std::sin(a / 2), 0.0f, std::cos(a / 2)};
    moved.position = {-0.7f, 0.3f, -1.5f};
    const Placed b = OnScreen(moved, w * 1.4f, h * 1.4f, 0.0f, kAspect);
    // The same point of the screen: its local offset scales with the screen.
    const Input::Vec3f local = Input::Transform(Input::Inverse(moved), b.pose.position);
    const Input::Vec3f rest_local = Input::Transform(Input::Inverse(Top()), at_rest.pose.position);
    REQUIRE(local.x == Approx(rest_local.x * 1.4f));
    REQUIRE(local.y == Approx(rest_local.y * 1.4f));
    REQUIRE(b.height == Approx(at_rest.height * 1.4f));
    // Facing where the screen faces.
    const Input::Vec3f n = Input::Rotate(b.pose.orientation, {0, 0, 1});
    const Input::Vec3f sn = Input::Rotate(moved.orientation, {0, 0, 1});
    REQUIRE(Input::Dot(n, sn) == Approx(1.0f));
}

TEST_CASE("Shader badge: on a curved screen it sits on the curve", "[frame_xr]") {
    const float w = 5.0f / 3.0f, h = 1.0f;
    const float radius = Input::ScreenRig::CurveRadius(w, 40);
    REQUIRE(radius > 0.0f);
    const Placed b = OnScreen(Top(), w, h, radius, kAspect);
    const Input::ScreenRig::Cylinder c = Input::ScreenRig::CylinderFor(Top(), w, h, radius);
    // Its centre just inside the cylinder (towards its axis), its face turned to the axis.
    // AzaharFrame fixd: inside by the flat badge's sagitta and 2 mm (CurveLift).
    const Input::Vec3f d = b.pose.position - c.pose.position;
    REQUIRE(std::hypot(d.x, d.z) ==
            Approx(radius - b.width * b.width / (8.0f * radius) - 0.002f).margin(1e-4f));
    REQUIRE(std::hypot(d.x, d.z) < radius);
    const Input::Vec3f n = Input::Rotate(b.pose.orientation, {0, 0, 1});
    const Input::Vec3f to_axis = Input::Normalized(Input::Vec3f{-d.x, 0.0f, -d.z});
    REQUIRE(Input::Dot(n, to_axis) == Approx(1.0f).margin(1e-4f));
    // The same arc length from the middle as the flat badge's offset.
    const Placed flat = OnScreen(Top(), w, h, 0.0f, kAspect);
    REQUIRE(std::atan2(d.x, -d.z) * radius == Approx(flat.pose.position.x).margin(0.001f));
}

// AzaharFrame fixd (session 651, item 12): the flat badge on a curved screen. Its ends stood
// w^2/(8r) behind the cylinder (1.5 to 2.5 cm at 60 to 90 degrees), so the screen covered them.
TEST_CASE("Shader badge: all of it in front of a curved screen", "[frame_xr][fixd]") {
    // The lift: 3 mm flat; the sagitta and 2 mm curved.
    CHECK(CurveLift(0.48f, 0.0f) == Approx(0.003f));
    CHECK(CurveLift(0.48f, 1.2f) == Approx(0.48f * 0.48f / (8.0f * 1.2f) + 0.002f));
    CHECK(CurveLift(0.48f, 1.2f) == Approx(0.026f));
    // A bigger curve (smaller radius) lifts it more.
    CHECK(CurveLift(0.48f, 1.0f) > CurveLift(0.48f, 2.0f));
    for (const float w : {5.0f / 3.0f, 2.5f, 1.2f}) {
        for (const uint32_t deg : {20u, 40u, 60u, 90u, 120u}) {
            const float h = w * 0.6f;
            const float radius = Input::ScreenRig::CurveRadius(w, deg);
            REQUIRE(radius > 0.0f);
            const Placed b = OnScreen(Top(), w, h, radius, kAspect);
            const Input::ScreenRig::Cylinder c = Input::ScreenRig::CylinderFor(Top(), w, h, radius);
            INFO("width " << w << ", curve " << deg << " degrees, radius " << radius);
            // Every point along the badge, its ends too, is nearer the axis than the cylinder
            // (in front of it, as the head sees it), by at most the lift and at least 1.4 mm up to
            // 90 degrees (w^2/(8r) is the sagitta to first order: at 120 degrees the ends keep
            // 0.6 mm or more).
            float least = 1e9f, most = -1e9f;
            for (int i = 0; i <= 20; i++) {
                const float sx = -0.5f + static_cast<float>(i) / 20.0f;
                const Input::Vec3f p = Corner(b, sx, 0.5f) - c.pose.position;
                const float clear = radius - std::hypot(p.x, p.z);
                least = std::min(least, clear);
                most = std::max(most, clear);
            }
            CHECK(least >= (deg <= 90u ? 0.0014f : 0.0006f));
            CHECK(most <= CurveLift(b.width, radius) + 1e-4f);
            // The old 3 mm left the ends behind the curve.
            const float half = b.width * 0.5f, r_old = radius - 0.003f;
            CHECK(std::hypot(r_old, half) > radius);
        }
    }
}

TEST_CASE("Shader badge: shrunk to fit a very small screen", "[frame_xr]") {
    const Placed b = OnScreen(Top(), 0.3f, 1.0f, 0.0f, kAspect);
    REQUIRE(b.width <= 0.3f - 2.0f * kScreenInset + 1e-5f);
    REQUIRE(b.width / b.height == Approx(kAspect));
}

TEST_CASE("Shader badge: head-locked at the top right of the view", "[frame_xr]") {
    const Placed b = HeadLocked(View(45.0f), std::nullopt, kAspect);
    const Input::Vec3f tr = Corner(b, 0.5f, 0.5f);
    REQUIRE(Yaw(tr) / kDeg == Approx(45.0f - kHeadMarginDeg).margin(0.05f));
    REQUIRE(Pitch(tr) / kDeg == Approx(45.0f - kHeadMarginDeg).margin(0.05f));
    REQUIRE(Input::Length(b.pose.position) == Approx(kHeadDistance));
    // Facing the head, upright, kHeadHeightDeg high.
    const Input::Vec3f n = Input::Rotate(b.pose.orientation, {0, 0, 1});
    REQUIRE(Input::Dot(n, Input::Normalized(b.pose.position * -1.0f)) ==
            Approx(1.0f).margin(1e-4f));
    REQUIRE(Input::Rotate(b.pose.orientation, {1, 0, 0}).y == Approx(0.0f).margin(1e-5f));
    REQUIRE(2.0f * std::atan(b.height / 2.0f / kHeadDistance) / kDeg == Approx(kHeadHeightDeg));
}

TEST_CASE("Shader badge: head-locked inside the part of the screen in view", "[frame_xr]") {
    const Angles view = View(45.0f);
    SECTION("an immersive screen bigger than the view: the view's corner") {
        const Angles screen{-60 * kDeg, 60 * kDeg, 55 * kDeg, -55 * kDeg};
        const Placed b = HeadLocked(view, screen, kAspect);
        const Input::Vec3f tr = Corner(b, 0.5f, 0.5f);
        REQUIRE(Yaw(tr) / kDeg == Approx(45.0f - kHeadMarginDeg).margin(0.05f));
        REQUIRE(Pitch(tr) / kDeg == Approx(45.0f - kHeadMarginDeg).margin(0.05f));
    }
    SECTION("a screen smaller than the view: its own corner") {
        const Angles screen{-20 * kDeg, 20 * kDeg, 12 * kDeg, -12 * kDeg};
        const Placed b = HeadLocked(view, screen, kAspect);
        const Input::Vec3f tr = Corner(b, 0.5f, 0.5f);
        REQUIRE(Yaw(tr) / kDeg == Approx(20.0f - kHeadInsetDeg).margin(0.05f));
        REQUIRE(Pitch(tr) / kDeg == Approx(12.0f - kHeadInsetDeg).margin(0.05f));
    }
    SECTION("a screen off to the left: still all in view") {
        const Angles screen{-80 * kDeg, -40 * kDeg, 10 * kDeg, -10 * kDeg};
        const Placed b = HeadLocked(view, screen, kAspect);
        for (const float sx : {-0.5f, 0.5f}) {
            for (const float sy : {-0.5f, 0.5f}) {
                const Input::Vec3f p = Corner(b, sx, sy);
                REQUIRE(std::abs(Yaw(p)) < 45.0f * kDeg);
                REQUIRE(std::abs(Pitch(p)) < 45.0f * kDeg);
            }
        }
    }
}

TEST_CASE("Shader badge: a screen's angles from the head", "[frame_xr]") {
    // The immersive screen as immersive.cpp places it: 3 m ahead, 2 x 3 m.
    Input::Posef s;
    s.position = {0.0f, 0.0f, -3.0f};
    const auto a = ScreenAngles(s, 3.0f, 2.0f);
    REQUIRE(a.has_value());
    REQUIRE(a->right / kDeg == Approx(std::atan2(1.5f, 3.0f) / kDeg));
    REQUIRE(a->up / kDeg == Approx(std::atan2(1.0f, 3.0f) / kDeg));
    REQUIRE(a->left == Approx(-a->right));
    s.position = {0.0f, 0.0f, 1.0f}; // behind the head
    REQUIRE_FALSE(ScreenAngles(s, 3.0f, 2.0f).has_value());
}

TEST_CASE("Shader badge: fades in and out over 150 ms in four steps", "[frame_xr]") {
    constexpr int64_t kFrame = 8'333'333; // 120 Hz
    float o = 0.0f;
    int frames = 0;
    int last_level = 0;
    while (o < 1.0f) {
        o = Fade(o, true, kFrame);
        frames++;
        REQUIRE(Level(o) >= last_level); // never back
        last_level = Level(o);
    }
    REQUIRE(frames >= 18); // 150 ms at 120 Hz (a last step for the rounding)
    REQUIRE(frames <= 19);
    REQUIRE(Level(o) == kLevels);
    o = Fade(o, false, kFadeNs / 2);
    REQUIRE(o == Approx(0.5f));
    REQUIRE(Level(o) == 2);
    o = Fade(o, false, kFadeNs);
    REQUIRE(o == 0.0f);
    REQUIRE(Level(o) == 0);
    REQUIRE(Level(0.01f) == 1);
    REQUIRE(Fade(0.3f, true, -5) == 0.3f); // a clock going back changes nothing
}

// AzaharFrame fixes7: not on a black view at boot; only kGraceNs after the game screen shows.
TEST_CASE("Badge: waits for the game screen and a grace", "[frame_xr][shaderbadge]") {
    Gate gate;
    constexpr int64_t s = 1'000'000'000;
    // Booting: no screens yet, then screens live but no picture.
    REQUIRE_FALSE(gate.Allow(false, false, 1 * s));
    REQUIRE_FALSE(gate.Allow(true, false, 2 * s));
    REQUIRE_FALSE(gate.Allow(true, false, 9 * s));
    // The screen shows at 10 s: hidden until 13 s.
    REQUIRE_FALSE(gate.Allow(true, true, 10 * s));
    REQUIRE_FALSE(gate.Allow(true, true, 12 * s));
    REQUIRE_FALSE(gate.Allow(true, true, 10 * s + kGraceNs - 1));
    REQUIRE(gate.Allow(true, true, 10 * s + kGraceNs));
    // Hidden while the screen is not on view; back at once (no new grace) while the game runs.
    REQUIRE_FALSE(gate.Allow(true, false, 20 * s));
    REQUIRE(gate.Allow(true, true, 21 * s));
    // The next game waits again.
    REQUIRE_FALSE(gate.Allow(false, false, 30 * s));
    REQUIRE_FALSE(gate.Allow(true, true, 31 * s));
    REQUIRE(gate.Allow(true, true, 31 * s + kGraceNs));
}
