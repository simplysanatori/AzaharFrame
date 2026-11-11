// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
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
    REQUIRE(StepCurve(60, 1) == 75);
    REQUIRE(StepCurve(75, 1) == 90);
    REQUIRE(StepCurve(90, 1) == 90); // the strongest step stays
    REQUIRE(StepCurve(90, -1) == 75);
    REQUIRE(StepCurve(60, -1) == 45);
    REQUIRE(StepCurve(15, -1) == 0);
    REQUIRE(StepCurve(0, -1) == 0);
    REQUIRE(StepCurve(30, 0) == 30);
    // A value typed into the config between two steps goes to the next one that way.
    REQUIRE(StepCurve(40, 1) == 45);
    REQUIRE(StepCurve(40, -1) == 30);
    REQUIRE(StepCurve(500, -1) == 90);
    // Up all the way and down all the way pass every step once.
    uint32_t deg = 0;
    int ups = 0;
    while (StepCurve(deg, 1) != deg) {
        deg = StepCurve(deg, 1);
        ups++;
    }
    REQUIRE(ups == static_cast<int>(kCurveStepsDeg.size()) - 1);
    REQUIRE(kCurveStepsDeg.size() == 7);
    REQUIRE(CurveToast(0) == "Screen Curve: Off");
    REQUIRE(CurveToast(15) == "Screen Curve: 15°");
    REQUIRE(CurveToast(30) == "Screen Curve: 30°");
    REQUIRE(CurveToast(45) == "Screen Curve: 45°");
    REQUIRE(CurveToast(60) == "Screen Curve: 60°"); 
    REQUIRE(CurveToast(75) == "Screen Curve: 75°");
    REQUIRE(CurveToast(90) == "Screen Curve: 90°");
    REQUIRE(std::string(CurveName(5)) == "15°"); // a typed value near Off is still curved
    REQUIRE(std::string(CurveName(40)) == "45°");
}

TEST_CASE("FrameXr screen rig: a curved screen keeps its size and shape", "[frame_xr]") {
    const float width = 1.0f * kAspect3ds; // the top screen at size 1
    const float height = 1.0f;
    REQUIRE(CurveRadius(width, 0) == 0.0f);
    REQUIRE(CurveRadius(0.0f, 30) == 0.0f);
    for (const uint32_t deg : {15u, 30u, 45u, 60u, 75u, 90u}) {
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

TEST_CASE("FrameXr screen rig: only the main slot's screen bends", "[frame_xr]") {
    const Posef top = Ahead(2.0f);
    const Posef bottom = BottomFromTop(top, 1.0f);
    const float bottom_w = 0.75f;
    for (const uint32_t deg : {15u, 45u, 60u, 90u}) {
        // The top screen in the main slot bends, the bottom one in the second slot stays flat.
        PanelQuad main{top, kAspect3ds, 1.0f, true};
        PanelQuad second{bottom, bottom_w, 0.5625f, true};
        second.curve_radius = 1.0f; // whatever it was before
        ApplyCurve(main, second, deg);
        REQUIRE(main.curve_radius == Approx(CurveRadius(kAspect3ds, deg)));
        REQUIRE(second.curve_radius == 0.0f);
        // The beam hits the curved main screen on its arc and the flat second one on its plane:
        // at the middles both are where the flat screens were, and off the middle of the second
        // screen the hit is the flat one.
        const QuadHit mid = IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{0, 0, -1}}, main);
        REQUIRE(mid.in_bounds);
        REQUIRE(mid.t == Approx(2.0f));
        REQUIRE(mid.u == Approx(0.5f));
        const Vec3f off = bottom.position + Rotate(bottom.orientation, Vec3f{0.3f, 0.0f, 0.0f});
        const QuadHit low = IntersectQuad(Ray{Vec3f{0, 0, 0}, Normalized(off)}, second);
        REQUIRE(low.in_bounds);
        REQUIRE(low.u == Approx(0.5f + 0.3f / bottom_w).margin(1e-4f));
        REQUIRE(low.v == Approx(0.5f).margin(1e-4f));
        REQUIRE(low.t == Approx(Length(off)).margin(1e-4f));
        // Swapped: the bottom screen in the main slot bends by its own width, the top one stays
        // flat.
        PanelQuad swapped_main{top, bottom_w, 0.5625f, true};
        PanelQuad swapped_second{bottom, kAspect3ds, 1.0f, true};
        ApplyCurve(swapped_main, swapped_second, deg);
        REQUIRE(swapped_main.curve_radius == Approx(CurveRadius(bottom_w, deg)));
        REQUIRE(swapped_second.curve_radius == 0.0f);
    }
    // Nothing bends at 0.
    PanelQuad main{top, kAspect3ds, 1.0f, true};
    PanelQuad second{bottom, bottom_w, 0.5625f, true};
    ApplyCurve(main, second, 0);
    REQUIRE(main.curve_radius == 0.0f);
    REQUIRE(second.curve_radius == 0.0f);
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

// AzaharFrame layout: where the second slot goes.
TEST_CASE("FrameXr screen rig: Stacked and Side by Side", "[frame_xr]") {
    const Posef top = Ahead(2.0f);
    const float main_w = kAspect3ds, second_w = 0.75f;
    // Stacked is the bottom screen under the top one, as before.
    const Posef stacked = SecondFromMain(top, 1.0f, main_w, second_w, kLayoutStacked);
    const Posef below = BottomFromTop(top, 1.0f);
    REQUIRE(Length(stacked.position - below.position) == Approx(0.0f).margin(1e-6f));
    // Side by Side: to the right, same distance and height, facing you, not tilted back.
    for (const Posef& main : {top, NudgeTop(top, -20, 6).value()}) {
        const Posef s = SecondFromMain(main, 1.0f, main_w, second_w, kLayoutSideBySide);
        REQUIRE(Length(s.position) == Approx(Length(main.position)));
        REQUIRE(s.position.y == Approx(main.position.y).margin(1e-5f));
        // Its normal points at the viewer as the main one's does.
        const Vec3f n = Rotate(s.orientation, Vec3f{0, 0, 1});
        const Vec3f to_viewer = Normalized(Vec3f{0, 0, 0} - s.position);
        const Vec3f n_main = Rotate(main.orientation, Vec3f{0, 0, 1});
        const Vec3f to_viewer_main = Normalized(Vec3f{0, 0, 0} - main.position);
        REQUIRE(Dot(n, to_viewer) == Approx(Dot(n_main, to_viewer_main)).margin(1e-4f));
        // To the right of the main one, in its own frame, with the gap between the edges.
        const Vec3f right = Rotate(main.orientation, Vec3f{1, 0, 0});
        REQUIRE(Dot(s.position - main.position, right) > 0.0f);
        const float horizontal = std::hypot(main.position.x, main.position.z);
        const float arc = std::acos(std::clamp(
                              Dot(Normalized(Vec3f{main.position.x, 0, main.position.z}),
                                  Normalized(Vec3f{s.position.x, 0, s.position.z})),
                              -1.0f, 1.0f)) *
                          horizontal;
        REQUIRE(arc == Approx(main_w / 2 + kSideGap + second_w / 2).margin(1e-3f));
    }
    REQUIRE(NextLayout(kLayoutStacked) == kLayoutSideBySide);
    REQUIRE(NextLayout(kLayoutSideBySide) == kLayoutStacked);
    REQUIRE(LayoutToast(kLayoutSideBySide) == "Screen Layout: Side by Side");
    REQUIRE(std::string(LayoutName(kLayoutStacked)) == "Stacked");
}

// ---- AzaharFrame move: Detach Screens -----------------------------------------------------------

namespace {

// The beam from `from` towards `at`.
Ray RayTo(const Vec3f& from, const Vec3f& at) {
    return {from, Normalized(at - from)};
}

// The screen's front (+Z) points at the eye.
float FacingDot(const Posef& p, const Vec3f& eye) {
    return Dot(Rotate(p.orientation, Vec3f{0, 0, 1}), Normalized(eye - p.position));
}

} // namespace

TEST_CASE("FrameXr screen rig: Detach Screens modes", "[frame_xr]") {
    REQUIRE(std::string(DetachName(kDetachOff)) == "Off");
    REQUIRE(std::string(DetachName(kDetachOther)) == "Other");
    REQUIRE(std::string(DetachName(kDetachBoth)) == "Both");
    REQUIRE(DetachToast(kDetachOther) == "Detach Screens: Other");
    REQUIRE(ClampDetach(7) == kDetachBoth);
    // Off: neither slot is free; Other: the second one; Both: both.
    REQUIRE_FALSE(SlotFree(kDetachOff, true));
    REQUIRE_FALSE(SlotFree(kDetachOff, false));
    REQUIRE_FALSE(SlotFree(kDetachOther, true));
    REQUIRE(SlotFree(kDetachOther, false));
    REQUIRE(SlotFree(kDetachBoth, true));
    REQUIRE(SlotFree(kDetachBoth, false));
}

TEST_CASE("FrameXr screen rig: detach, relink and reset of the slots", "[frame_xr]") {
    const Slots reset;
    REQUIRE(reset.main.position.z == Approx(-2.0f));
    REQUIRE(reset.main_scale == 1.0f);
    REQUIRE_FALSE(reset.second_free);
    REQUIRE(reset.second_scale == 1.0f);

    const Posef linked = BottomFromTop(reset.main, 1.0f);
    SECTION("a grab in Off leaves the second slot linked") {
        Slots s;
        REQUIRE_FALSE(Detach(s, kDetachOff, linked));
        REQUIRE_FALSE(s.second_free);
    }
    SECTION("a grab in Other or Both keeps the second slot where it is drawn, at 1.0x") {
        for (const uint32_t mode : {kDetachOther, kDetachBoth}) {
            Slots s;
            REQUIRE(Detach(s, mode, linked));
            REQUIRE(s.second_free);
            REQUIRE(Length(s.second.position - linked.position) == Approx(0.0f).margin(1e-6f));
            REQUIRE(s.second_scale == 1.0f);
            // Once free it is not moved again by the next grab.
            s.second.position.x = 0.7f;
            REQUIRE_FALSE(Detach(s, mode, linked));
            REQUIRE(s.second.position.x == Approx(0.7f));
        }
    }
    SECTION("Relink puts the second slot back on the main one at 1.0x, the main slot stays") {
        Slots s;
        s.main.position = {0.3f, 0.2f, -2.5f};
        s.main_scale = 1.8f;
        Detach(s, kDetachOther, linked);
        s.second_scale = 2.4f;
        Relink(s);
        REQUIRE_FALSE(s.second_free);
        REQUIRE(s.second_scale == 1.0f);
        REQUIRE(s.main.position.x == Approx(0.3f));
        REQUIRE(s.main_scale == Approx(1.8f));
    }
    SECTION("going to Off links the second slot again; Other and Both keep it") {
        Slots s;
        Detach(s, kDetachBoth, {Quatf{}, Vec3f{1.0f, -0.5f, -1.0f}});
        s.second_scale = 2.0f;
        REQUIRE(ForMode(s, kDetachOther).second_free);
        REQUIRE(ForMode(s, kDetachBoth).second_scale == Approx(2.0f));
        const Slots off = ForMode(s, kDetachOff);
        REQUIRE_FALSE(off.second_free);
        REQUIRE(off.second_scale == 1.0f);
    }
    SECTION("leaving Both, a main slot the Off grip cannot move goes back in front") {
        Slots s;
        s.main = FreePose({0, 0, 0}, {1.0f, 0.0f, 0.0f}, 1.5f); // moved to your right
        s.main_scale = 2.0f;
        REQUIRE(ForMode(s, kDetachBoth).main.position.x == Approx(1.5f));
        const Slots other = ForMode(s, kDetachOther);
        REQUIRE(other.main.position.z == Approx(-2.0f));
        REQUIRE(other.main.position.x == Approx(0.0f));
        REQUIRE(other.main_scale == Approx(2.0f)); // its size stays
        // One that is still ahead stays where it is.
        s.main = NudgeTop(Ahead(2.0f), 10, 0).value();
        REQUIRE(ForMode(s, kDetachOff).main.position.x == Approx(s.main.position.x));
    }
    SECTION("scales out of range are clamped") {
        Slots s;
        s.main_scale = 9.0f;
        s.second_scale = 0.1f;
        s.second_free = true;
        const Slots c = ForMode(s, kDetachBoth);
        REQUIRE(c.main_scale == Approx(kMaxScale));
        REQUIRE(c.second_scale == Approx(kMinScale));
    }
}

TEST_CASE("FrameXr screen rig: a free slot follows the beam round you, facing you", "[frame_xr]") {
    const Vec3f eye{0.0f, 1.6f, 0.0f};
    const Vec3f hand{0.2f, 1.2f, -0.3f};
    const Posef slot = FreePose(eye, {0.0f, -0.3f, -1.0f}, 1.5f);
    REQUIRE(Length(slot.position - eye) == Approx(1.5f));
    REQUIRE(FacingDot(slot, eye) == Approx(1.0f).margin(1e-4f));

    FreeGrab grab;
    // Grabbed off its middle: the slot does not jump.
    const Ray at_grab = RayTo(hand, slot.position + Vec3f{0.2f, 0.1f, 0.0f});
    grab.Begin(eye, slot, at_grab);
    const Posef same = FreePose(eye, grab.Direction(eye, at_grab, 1.5f), 1.5f);
    REQUIRE(Length(same.position - slot.position) == Approx(0.0f).margin(1e-3f));

    // The beam swings far to the left and behind: the slot goes there, still facing you.
    const Ray left = RayTo(hand, eye + Vec3f{-3.0f, 0.0f, 1.0f});
    const Posef moved = FreePose(eye, grab.Direction(eye, left, 1.5f), 1.5f);
    REQUIRE(moved.position.x < -1.0f);
    REQUIRE(Length(moved.position - eye) == Approx(1.5f));
    REQUIRE(FacingDot(moved, eye) == Approx(1.0f).margin(1e-4f));

    // Straight up is kept short of the pole.
    const Ray up = RayTo(hand, eye + Vec3f{0.0f, 10.0f, -0.01f});
    const Vec3f dir = grab.Direction(eye, up, 1.5f);
    REQUIRE(std::asin(dir.y) / kDeg <= Approx(kFreeMaxPitchDeg + 0.01f));
}

TEST_CASE("FrameXr screen rig: a free slot's distance and nudge", "[frame_xr]") {
    // Stick up (negative delta z) takes it further; kept within the limits.
    REQUIRE(FreeDistance(1.5f, -0.1f) == Approx(1.6f));
    REQUIRE(FreeDistance(1.5f, 0.1f) == Approx(1.4f));
    REQUIRE(FreeDistance(0.55f, 0.5f) == Approx(kFreeMinDistance));
    REQUIRE(FreeDistance(9.9f, -1.0f) == Approx(kFreeMaxDistance));
    REQUIRE(FreeDistance(NAN, 0.0f) == Approx(kFreeMinDistance));

    const Vec3f eye{0.1f, 1.5f, 0.2f};
    const Posef slot = FreePose(eye, {0.0f, -0.5f, -1.0f}, 1.2f);
    const Posef right = NudgeFree(slot, eye, 2);
    REQUIRE(Length(right.position - eye) == Approx(1.2f));
    REQUIRE(right.position.y == Approx(slot.position.y));
    REQUIRE(right.position.x > slot.position.x);
    REQUIRE(FacingDot(right, eye) == Approx(1.0f).margin(1e-4f));
    const float turned = std::acos(std::clamp(
        Dot(Normalized(Vec3f{right.position.x - eye.x, 0, right.position.z - eye.z}),
            Normalized(Vec3f{slot.position.x - eye.x, 0, slot.position.z - eye.z})),
        -1.0f, 1.0f));
    REQUIRE(turned / kDeg == Approx(2 * kNudgeStepDeg).margin(1e-2f));
    const Posef back = NudgeFree(right, eye, -2);
    REQUIRE(Length(back.position - slot.position) == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("FrameXr screen rig: the per-game screen placement file", "[frame_xr]") {
    Slots s;
    s.main = NudgeTop(Ahead(2.5f), 6, 0).value();
    s.main_scale = 1.5f;
    PlacementFile file;
    file.Set(0x0004000000030800ull, s); // linked: no second line
    Slots d = s;
    d.second_free = true;
    d.second = FreePose({0, 0, 0}, {0.5f, -0.6f, -1.0f}, 0.9f);
    d.second_scale = 2.25f;
    file.Set(0x00040000001B4F00ull, d);
    const std::string text = file.Format();
    REQUIRE(text.find("0004000000030800 main") != std::string::npos);
    REQUIRE(text.find("0004000000030800 second") == std::string::npos);
    REQUIRE(text.find("00040000001b4f00 second") != std::string::npos);

    PlacementFile read;
    read.Parse(text);
    REQUIRE(read.Size() == 2);
    const auto a = read.Get(0x0004000000030800ull);
    REQUIRE(a);
    REQUIRE_FALSE(a->second_free);
    REQUIRE(a->main_scale == Approx(1.5f));
    REQUIRE(Length(a->main.position - s.main.position) == Approx(0.0f).margin(2e-3f));
    REQUIRE(std::abs(Dot(Rotate(a->main.orientation, Vec3f{0, 0, 1}),
                         Rotate(s.main.orientation, Vec3f{0, 0, 1}))) == Approx(1.0f).margin(1e-3f));
    const auto b = read.Get(0x00040000001B4F00ull);
    REQUIRE(b);
    REQUIRE(b->second_free);
    REQUIRE(b->second_scale == Approx(2.25f));
    REQUIRE(Length(b->second.position - d.second.position) == Approx(0.0f).margin(2e-3f));

    SECTION("a game with no entry has none; an erased one neither") {
        REQUIRE_FALSE(read.Get(0x0004000000099999ull));
        read.Erase(0x0004000000030800ull);
        REQUIRE_FALSE(read.Get(0x0004000000030800ull));
        REQUIRE(read.Size() == 1);
    }
    SECTION("broken lines are kept out, a second slot alone is no entry") {
        PlacementFile f;
        f.Parse("# comment\n"
                "0004000000030800 main 0 0 -2 0 0 0 1 1 # trailing comment\n"
                "000400000003080 main 0 0 -2 0 0 0 1 1\n"        // 15 digits
                "000400000003080G main 0 0 -2 0 0 0 1 1\n"       // not hex
                "0004000000040000 middle 0 0 -2 0 0 0 1 1\n"     // unknown kind
                "0004000000050000 main 0 0 -2 0 0 0 1\n"         // too few numbers
                "0004000000060000 main 0 0 -2 0 0 0 0 1\n"       // no rotation
                "0004000000070000 main 0 0 -90 0 0 0 1 1\n"      // far away
                "0004000000080000 main 0 0 -2 0 0 0 1 nan\n"     // not a number
                "0004000000090000 second 0 0 -2 0 0 0 1 1\n"     // no main line
                "00040000000A0000 main 0 0 -2 0 0 0 2 7\n");     // unnormalised, too big
        REQUIRE(f.Get(0x0004000000030800ull));
        for (const uint64_t id : {0x0004000000040000ull, 0x0004000000050000ull,
                                  0x0004000000060000ull, 0x0004000000070000ull,
                                  0x0004000000080000ull, 0x0004000000090000ull}) {
            REQUIRE_FALSE(f.Get(id));
        }
        const auto big = f.Get(0x00040000000A0000ull);
        REQUIRE(big);
        REQUIRE(big->main.orientation.w == Approx(1.0f));
        REQUIRE(big->main_scale == Approx(kMaxScale));
        // The lone second line is dropped when saving.
        REQUIRE(f.Format().find("0004000000090000") == std::string::npos);
    }
}
