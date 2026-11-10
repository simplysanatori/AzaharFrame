// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <string>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/screen_rig.h"

// AzaharFrame vrsize: the normal screens' size and the grip + D-pad nudge
// (frame_xr/input/screen_rig.h).
using Catch::Approx;
using namespace FrameXr::Input;
using namespace FrameXr::Input::ScreenRig;

namespace {

constexpr float kDeg = 3.14159265f / 180.0f;
constexpr float kAspect3ds = 400.0f / 240.0f;

Posef Ahead(float distance) {
    return {Quatf{}, Vec3f{0.0f, 0.0f, -distance}};
}

float Azimuth(const Vec3f& p) {
    return std::atan2(p.x, -p.z) / kDeg;
}

float Elevation(const Vec3f& p) {
    return std::asin(p.y / Length(p)) / kDeg;
}

// The top screen's full width in degrees, seen from the origin, at `distance` and `scale`.
float WidthDeg(float distance, float scale) {
    return 2.0f * std::atan(kAspect3ds * scale * kTopHalfHeight / distance) / kDeg;
}

} // namespace

TEST_CASE("FrameXr screen rig: the size limits let the top screen fill the view", "[frame_xr]") {
    REQUIRE(ClampScale(0.1f) == Approx(kMinScale));
    REQUIRE(ClampScale(9.0f) == Approx(kMaxScale));
    REQUIRE(ClampScale(NAN) == Approx(1.0f));
    // At the start distance the largest screen is wider than the Frame's view both eyes see
    // (about 100 degrees), at the nearest distance more again; scale 1 is the old 45 degrees.
    REQUIRE(WidthDeg(2.0f, 1.0f) == Approx(45.2f).margin(0.1f));
    REQUIRE(WidthDeg(2.0f, kMaxScale) > 100.0f);
    REQUIRE(WidthDeg(-kNearestTopZ, kMaxScale) > 130.0f);
    // The nearest the screens get stays sane: the top one 1 m, the bottom one 0.5 m nearer.
    const Posef bottom = BottomFromTop(Ahead(-kNearestTopZ), kMaxScale);
    REQUIRE(-bottom.position.z == Approx(0.5f));
    REQUIRE(Length(bottom.position) > 0.5f);
}

TEST_CASE("FrameXr screen rig: a bigger screen gets less 3D depth", "[frame_xr]") {
    REQUIRE(DepthScale(kMinScale) == Approx(1.0f));
    REQUIRE(DepthScale(1.0f) == Approx(1.0f));
    // The eyes' pictures of a far object keep their distance apart in metres: the offset on the
    // picture (a share of its width) x the depth x the width stays what it is at size 1.
    for (const float s : {1.5f, 2.0f, kMaxScale}) {
        REQUIRE(DepthScale(s) * s == Approx(1.0f));
    }
    REQUIRE(DepthScale(9.0f) == Approx(1.0f / kMaxScale));
}

TEST_CASE("FrameXr screen rig: the stick resizes", "[frame_xr]") {
    // Full stick for 1 s: e^0.5 bigger, in small frames.
    float scale = 1.0f;
    for (int i = 0; i < 90; i++) {
        scale = ScaleWithStick(scale, 1.0f, 1.0f / 90.0f, 0.5f);
    }
    REQUIRE(scale == Approx(std::exp(0.5f)).epsilon(0.001));
    REQUIRE(ScaleWithStick(1.0f, 0.4f, 1.0f, 0.5f) == Approx(1.0f));
    REQUIRE(ScaleWithStick(1.0f, -1.0f, 0.05f, 0.5f) < 1.0f);
    // A long frame counts as at most 0.1 s; the limits hold.
    REQUIRE(ScaleWithStick(1.0f, 1.0f, 5.0f, 0.5f) == Approx(std::exp(0.05f)));
    REQUIRE(ScaleWithStick(kMaxScale, 1.0f, 0.1f, 0.5f) == Approx(kMaxScale));
    REQUIRE(ScaleWithStick(kMinScale, -1.0f, 0.1f, 0.5f) == Approx(kMinScale));
}

TEST_CASE("FrameXr screen rig: the bottom screen follows the bigger top screen", "[frame_xr]") {
    // Scale 1 is CitraVR's: 0.75 m lower, 0.5 m nearer, tilted back 45 degrees.
    const Posef b1 = BottomFromTop(Ahead(2.0f), 1.0f);
    REQUIRE(b1.position.x == Approx(0.0f).margin(1e-5));
    REQUIRE(b1.position.y == Approx(-0.75f));
    REQUIRE(b1.position.z == Approx(-1.5f));
    const Vec3f normal = Rotate(b1.orientation, {0.0f, 0.0f, 1.0f});
    REQUIRE(std::atan2(normal.y, normal.z) / kDeg == Approx(45.0f).margin(0.01f));
    // At any scale its centre stays 0.25 m under the top screen's bottom edge, so the top screen
    // never covers it, and it stays 0.5 m nearer.
    for (const float s : {kMinScale, 1.0f, 1.7f, kMaxScale}) {
        const Vec3f off = BottomOffset(s);
        REQUIRE(off.y == Approx(-(kTopHalfHeight * s + 0.25f)));
        REQUIRE(off.z == Approx(kBottomNearer));
    }
    // It turns with the top screen.
    const Posef top{FaceViewer({1.0f, 0.5f, -2.0f}, {}, {0, 1, 0}), {1.0f, 0.5f, -2.0f}};
    const Posef b = BottomFromTop(top, 2.0f);
    const Vec3f rel = Rotate(Conjugate(top.orientation), b.position - top.position);
    REQUIRE(rel.x == Approx(0.0f).margin(1e-5));
    REQUIRE(rel.y == Approx(-1.25f));
    REQUIRE(rel.z == Approx(0.5f));
}

TEST_CASE("FrameXr screen rig: the D-pad turns the screens about the head", "[frame_xr]") {
    const Posef start = Ahead(2.0f);
    SECTION("one step right keeps the distance and faces the viewer") {
        const auto moved = NudgeTop(start, 1, 0);
        REQUIRE(moved);
        REQUIRE(Length(moved->position) == Approx(2.0f));
        REQUIRE(Azimuth(moved->position) == Approx(kNudgeStepDeg));
        REQUIRE(Elevation(moved->position) == Approx(0.0f).margin(1e-4));
        const Vec3f facing = Rotate(moved->orientation, {0.0f, 0.0f, 1.0f});
        const Vec3f to_viewer = Normalized(moved->position * -1.0f);
        REQUIRE(Dot(facing, to_viewer) == Approx(1.0f));
    }
    SECTION("up, down, left") {
        REQUIRE(Elevation(NudgeTop(start, 0, 3)->position) == Approx(3 * kNudgeStepDeg));
        REQUIRE(Elevation(NudgeTop(start, 0, -1)->position) == Approx(-kNudgeStepDeg));
        REQUIRE(Azimuth(NudgeTop(start, -2, 0)->position) == Approx(-2 * kNudgeStepDeg));
    }
    SECTION("no step past the limit, but back towards the middle") {
        Posef p = start;
        int steps = 0;
        while (const auto next = NudgeTop(p, 1, 0)) {
            p = *next;
            REQUIRE(++steps < 1000);
        }
        REQUIRE(Azimuth(p.position) <= kMaxNudgeAngleDeg + 1e-3f);
        REQUIRE(Azimuth(p.position) > kMaxNudgeAngleDeg - kNudgeStepDeg - 1e-3f);
        REQUIRE(NudgeTop(p, -1, 0));
    }
    SECTION("no nearer than 1 m ahead, as a drag") {
        REQUIRE_FALSE(NudgeTop(Ahead(-kNearestTopZ), 1, 0));
        REQUIRE(NudgeTop(Ahead(1.2f), 1, 0));
    }
    SECTION("the toasts") {
        REQUIRE(std::string{MovedToast(1, 0)} == "Moved Right");
        REQUIRE(std::string{MovedToast(-1, 0)} == "Moved Left");
        REQUIRE(std::string{MovedToast(0, 1)} == "Moved Up");
        REQUIRE(std::string{MovedToast(0, -1)} == "Moved Down");
    }
}

TEST_CASE("FrameXr screen rig: grip + D-pad", "[frame_xr]") {
    DpadNudger nudger;
    InputFrame in;
    const auto frame = [&](int64_t ms, bool right, bool holding) {
        in = {};
        in.time_ns = ms * 1'000'000;
        in.dpad_right = right;
        return nudger.Filter(in, holding);
    };
    SECTION("without a grip the D-pad goes to the game") {
        REQUIRE_FALSE(frame(0, true, false).Any());
        REQUIRE(in.dpad_right);
    }
    SECTION("a press is a step, held it repeats, and the game sees none of it") {
        REQUIRE(frame(0, true, true).right == 1);
        REQUIRE_FALSE(in.dpad_right);
        REQUIRE(frame(100, true, true).right == 0);
        REQUIRE(frame(399, true, true).right == 0);
        REQUIRE(frame(400, true, true).right == 1);
        REQUIRE(frame(450, true, true).right == 0);
        REQUIRE(frame(500, true, true).right == 1);
        REQUIRE_FALSE(in.dpad_right);
        // The grip let go first: the direction stays away from the game until it is let go.
        REQUIRE_FALSE(frame(600, true, false).Any());
        REQUIRE_FALSE(in.dpad_right);
        frame(700, false, false);
        frame(800, true, false);
        REQUIRE(in.dpad_right);
    }
    SECTION("a direction held before the grip is kept from the game, with no step") {
        frame(0, true, false);
        REQUIRE(in.dpad_right);
        REQUIRE_FALSE(frame(10, true, true).Any());
        REQUIRE_FALSE(in.dpad_right);
        REQUIRE_FALSE(frame(2000, true, true).Any());
    }
    SECTION("each direction") {
        in = {};
        in.dpad_left = true;
        REQUIRE(nudger.Filter(in, true).right == -1);
        in = {};
        in.time_ns = 1;
        in.dpad_up = true;
        REQUIRE(nudger.Filter(in, true).up == 1);
        in = {};
        in.time_ns = 2;
        in.dpad_down = true;
        REQUIRE(nudger.Filter(in, true).up == -1);
        REQUIRE_FALSE(in.dpad_down);
    }
}

// AzaharFrame curve: Screen Curve (frame_xr/input/screen_rig.h).
TEST_CASE("FrameXr screen rig: the curve steps", "[frame_xr]") {
    REQUIRE(StepCurve(0, 1) == 15);
    REQUIRE(StepCurve(15, 1) == 30);
    REQUIRE(StepCurve(45, 1) == 60);
    REQUIRE(StepCurve(60, 1) == 60); // the strongest step stays
    REQUIRE(StepCurve(60, -1) == 45);
    REQUIRE(StepCurve(15, -1) == 0);
    REQUIRE(StepCurve(0, -1) == 0);
    REQUIRE(StepCurve(30, 0) == 30);
    // A value typed into the config between two steps goes to the next one that way.
    REQUIRE(StepCurve(40, 1) == 45);
    REQUIRE(StepCurve(40, -1) == 30);
    REQUIRE(StepCurve(500, -1) == 60);
    // Up all the way and down all the way pass every step once.
    uint32_t deg = 0;
    int ups = 0;
    while (StepCurve(deg, 1) != deg) {
        deg = StepCurve(deg, 1);
        ups++;
    }
    REQUIRE(ups == static_cast<int>(kCurveStepsDeg.size()) - 1);
    REQUIRE(CurveToast(0) == "Screen Curve: Off");
    REQUIRE(CurveToast(15) == "Screen Curve: Gentle");
    REQUIRE(CurveToast(30) == "Screen Curve: Medium");
    REQUIRE(CurveToast(45) == "Screen Curve: Strong");
    REQUIRE(CurveToast(60) == "Screen Curve: Max");
    REQUIRE(std::string(CurveName(5)) == "Gentle"); // a typed value near Off is still curved
    REQUIRE(std::string(CurveName(40)) == "Strong");
}

TEST_CASE("FrameXr screen rig: a curved screen keeps its size and shape", "[frame_xr]") {
    const float width = 1.0f * kAspect3ds; // the top screen at size 1
    const float height = 1.0f;
    REQUIRE(CurveRadius(width, 0) == 0.0f);
    REQUIRE(CurveRadius(0.0f, 30) == 0.0f);
    for (const uint32_t deg : {15u, 30u, 45u, 60u}) {
        const float r = CurveRadius(width, deg);
        const Cylinder c = CylinderFor(Ahead(2.0f), width, height, r);
        // The arc is as long as the flat screen was wide, at the angle of the step.
        REQUIRE(c.central_angle == Approx(deg * kDeg));
        REQUIRE(c.radius * c.central_angle == Approx(width));
        // The cylinder layer's height (radius x angle / aspect) is the flat screen's.
        REQUIRE(c.radius * c.central_angle / c.aspect_ratio == Approx(height));
        // The middle of the arc stays where the flat screen's middle was: the pose is `radius`
        // nearer the viewer, and the arc's middle is at the pose's -Z.
        const Vec3f middle = c.pose.position + Rotate(c.pose.orientation, Vec3f{0, 0, -c.radius});
        REQUIRE(middle.z == Approx(-2.0f));
        REQUIRE(middle.x == Approx(0.0f).margin(1e-5f));
    }
    // At the step whose radius is the distance, the centre is the viewer: 1.667 m / 2 m = 47.7
    // degrees, between the 45 and 60 degree steps.
    const float r45 = CurveRadius(width, 45);
    const float r60 = CurveRadius(width, 60);
    REQUIRE(r45 > 2.0f);
    REQUIRE(r60 < 2.0f);
    // A wider (bigger or wider-shaped) screen at the same step has a bigger radius.
    REQUIRE(CurveRadius(2.0f * width, 30) == Approx(2.0f * CurveRadius(width, 30)));
}

TEST_CASE("FrameXr screen rig: both screens bend about the same point", "[frame_xr]") {
    const float scale = 1.0f;
    const Posef top = Ahead(2.0f);
    const Posef bottom = BottomFromTop(top, scale);
    const float bottom_w = 0.75f;
    for (const uint32_t deg : {15u, 45u, 60u}) {
        PanelQuad main{top, kAspect3ds, 1.0f, true};
        PanelQuad second{bottom, bottom_w, 0.5625f, true};
        ApplyCurve(main, second, deg);
        REQUIRE(main.curve_radius == Approx(CurveRadius(kAspect3ds, deg)));
        REQUIRE(second.curve_radius > 0.0f);
        // The bottom screen's axis (through its cylinder pose, along its +Y) passes through the
        // top screen's centre of curvature.
        const Vec3f centre = CylinderFor(top, kAspect3ds, 1.0f, main.curve_radius).pose.position;
        const Cylinder b = CylinderFor(bottom, bottom_w, 0.5625f, second.curve_radius);
        const Vec3f axis = Rotate(b.pose.orientation, Vec3f{0, 1, 0});
        const Vec3f to = centre - b.pose.position;
        const Vec3f off_axis = to - axis * Dot(to, axis);
        REQUIRE(Length(off_axis) == Approx(0.0f).margin(1e-4f));
    }
    // Nothing bends at 0.
    PanelQuad main{top, kAspect3ds, 1.0f, true};
    PanelQuad second{bottom, bottom_w, 0.5625f, true};
    ApplyCurve(main, second, 0);
    REQUIRE(main.curve_radius == 0.0f);
    REQUIRE(second.curve_radius == 0.0f);
    // A secondary screen whose shared point would be behind it takes the main radius.
    REQUIRE(SecondaryCurveRadius(top, 1.0f, Ahead(-1.0f), 0.5f) == Approx(1.0f));
}

TEST_CASE("FrameXr screen rig: D-pad up and down presses for the curve", "[frame_xr]") {
    DpadNudger nudger;
    InputFrame in;
    const auto frame = [&](int64_t ms, bool up, bool down) {
        in = {};
        in.time_ns = ms * 1'000'000;
        in.dpad_up = up;
        in.dpad_down = down;
        return nudger.Filter(in, true);
    };
    REQUIRE(frame(0, true, false).up_presses == 1);
    // Held, it repeats as a nudge but is one press only.
    REQUIRE(frame(400, true, false).up == 1);
    REQUIRE(frame(400, true, false).up_presses == 0);
    REQUIRE(frame(500, true, false).up_presses == 0);
    frame(600, false, false);
    REQUIRE(frame(700, false, true).up_presses == -1);
    REQUIRE_FALSE(in.dpad_down);
}
