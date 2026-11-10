// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <optional>
#include <cmath>
#include <string>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/immersive_layout.h"
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
    SECTION("every corner of the default screen is inside the view, margin kept") {
        ViewFov v;
        v.left = Rad(-42);
        v.right = Rad(42);
        v.up = Rad(40);
        v.down = Rad(-50);
        for (const float fov : {50.0f, 60.0f, 80.0f}) {
            const HeadLocked s = SolveHeadLocked(Placement{}, fov, kAspect3ds, v);
            const Posef pose = HeadLockedQuadPose(s, 0.0f);
            for (const Vec3f& c : Corners(pose, s.width(), s.height())) {
                const float h = std::atan2(c.x, -c.z);
                const float e = std::atan2(c.y, -c.z);
                REQUIRE(h >= v.left + Rad(kFitMarginDeg) - 1e-3f);
                REQUIRE(h <= v.right - Rad(kFitMarginDeg) + 1e-3f);
                REQUIRE(e >= v.down + Rad(kFitMarginDeg) - 1e-3f);
                REQUIRE(e <= v.up - Rad(kFitMarginDeg) + 1e-3f);
            }
        }
    }
}

TEST_CASE("FrameXr immersive: the default fits a comfortable width", "[frame_xr]") {
    // The Frame's view (a headset log): both eyes see -50.3 to 50.4 across, -59.6 to 49.5 up.
    ViewFov frame;
    frame.left = Rad(-50.3f);
    frame.right = Rad(50.4f);
    frame.down = Rad(-59.6f);
    frame.up = Rad(49.5f);
    for (const float fov : {60.0f, 75.0f}) {
        for (const float aspect : {kAspect3ds, 16.0f / 9.0f}) {
            const HeadLocked s = SolveHeadLocked(Placement{}, fov, aspect, frame);
            const Posef pose = HeadLockedQuadPose(s, 0.0f);
            for (const Vec3f& c : Corners(pose, s.width(), s.height())) {
                const float h = std::atan2(c.x, -c.z);
                const float e = std::atan2(c.y, -c.z);
                REQUIRE(std::abs(h) < frame.right - Rad(kFitMarginDeg));
                REQUIRE(e >= frame.down + Rad(kFitMarginDeg) - 1e-3f);
                REQUIRE(e <= frame.up - Rad(kFitMarginDeg) + 1e-3f);
            }
            // The HUD at the sides ends at the comfortable limit, not at the lens edge.
            REQUIRE(std::atan(s.tan_half_y * s.aspect) == Approx(Rad(kComfortHalfWidthDeg)));
            REQUIRE(s.factor == Approx(kFactorMin));
            REQUIRE(s.gain > 1.0f);
            REQUIRE(s.tan_half_y * s.gain == Approx(std::tan(Rad(fov / 2)))); // world locked
        }
    }
    SECTION("a game frame already inside the limit is shown at its own angle") {
        const HeadLocked s = SolveHeadLocked(Placement{}, 40.0f, kAspect3ds, frame);
        REQUIRE(s.factor == Approx(kFactorMin));
        REQUIRE(s.gain == Approx(1.0f));
        REQUIRE(s.tan_half_y == Approx(std::tan(Rad(20))));
    }
    SECTION("a narrow view still fits the whole screen") {
        const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, Symmetric(30));
        REQUIRE(std::atan(s.tan_half_y * s.aspect) < Rad(30 - kFitMarginDeg) + 1e-3f);
    }
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
                REQUIRE(s.tan_half_y * s.gain == Approx(t));
            }
            REQUIRE(s.gain >= kMinGain);
            REQUIRE(s.gain <= kMaxGain);
        }
    }
    SECTION("the gain range covers the sizes of a 60 degree game") {
        Placement big;
        big.size = kMaxSize;
        REQUIRE(SolveHeadLocked(big, 60.0f, kAspect3ds, roomy).gain > kMinGain);
        Placement small;
        small.size = 0.8f;
        REQUIRE(SolveHeadLocked(small, 60.0f, kAspect3ds, roomy).gain < kMaxGain);
        REQUIRE(kMaxSize >= 2.0f);
    }
    SECTION("Wider View: the factor is the level; a step scales the size, so the gain stays") {
        const HeadLocked off = SolveHeadLocked(Placement{}, 40.0f, kAspect3ds, roomy);
        REQUIRE(off.factor == Approx(kFactorMin));
        Options wider;
        wider.wider = WiderFactor(2);
        REQUIRE(wider.wider == Approx(1.2f));
        // At the same size the game draws 1.2x wider into the same screen: the gain grows.
        const HeadLocked same = SolveHeadLocked(Placement{}, 40.0f, kAspect3ds, roomy, 0.0f, wider);
        REQUIRE(same.factor == Approx(1.2f));
        REQUIRE(same.gain == Approx(off.gain * 1.2f));
        // The D-pad step also scales the size: the picture keeps its scale, only more shows.
        Placement p;
        p.size = SizeForWider(1.0f, WiderFactor(0), WiderFactor(2));
        REQUIRE(p.size == Approx(1.2f));
        const HeadLocked stepped = SolveHeadLocked(p, 40.0f, kAspect3ds, roomy, 0.0f, wider);
        REQUIRE(stepped.gain == Approx(off.gain));
        REQUIRE(stepped.tan_half_y == Approx(off.tan_half_y * 1.2f));
        // Back down to Off: the old size again.
        REQUIRE(SizeForWider(p.size, WiderFactor(2), WiderFactor(0)) == Approx(1.0f));
        // At the size limit the factor still changes.
        REQUIRE(SizeForWider(2.4f, WiderFactor(0), WiderFactor(4)) == Approx(kMaxSize));
        // Any level past the last is the last; the factor stays within CitraVR's 1.4.
        REQUIRE(WiderFactor(9) == Approx(kFactorMax));
        Options too_wide;
        too_wide.wider = 3.0f;
        REQUIRE(SolveHeadLocked(p, 40.0f, kAspect3ds, roomy, 0.0f, too_wide).factor ==
                Approx(kFactorMax));
    }
    SECTION("Wider View: the toast and the old switch") {
        REQUIRE(WiderToast(0) == "Wider View: Off");
        REQUIRE(WiderToast(2) == "Wider View: 1.2x");
        REQUIRE(WiderToast(4) == "Wider View: 1.4x");
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
    SECTION("the default is a few degrees below the middle of the view") {
        ViewFov v = Symmetric(45);
        v.down = Rad(-55);
        const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, v);
        REQUIRE(s.pitch_rad == Approx(ViewMiddlePitch(v) + Rad(kDefaultPitchDeg)));
        REQUIRE(s.pitch_rad < 0.0f);
    }
}

TEST_CASE("FrameXr immersive: Squeeze Width", "[frame_xr]") {
    const ViewFov roomy = Symmetric(70);
    REQUIRE(SqueezeFor(0) == 0.0f);
    REQUIRE(SqueezeFor(1) == Approx(0.1f));
    REQUIRE(SqueezeFor(2) == Approx(0.2f));
    REQUIRE(SqueezeFor(3) == Approx(0.3f));
    REQUIRE(SqueezeFor(7) == 0.0f);
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
    REQUIRE(s.factor * t / s.tan_half_y == Approx(s.gain));
    REQUIRE(plain.gain_x == Approx(plain.gain));
}

TEST_CASE("FrameXr immersive: Squeeze Width 30%", "[frame_xr]") {
    // The Frame's default for a 60 degree game (gain about 1.3): the yaw gain stays under the
    // limit, so the middle stays locked both ways.
    Options o;
    o.squeeze = SqueezeFor(3);
    const HeadLocked plain = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, Symmetric(50));
    const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, Symmetric(50), 0.0f, o);
    REQUIRE(s.gain == Approx(plain.gain));
    REQUIRE(s.width() == Approx(plain.width() * 0.7f));
    REQUIRE(s.gain < 1.4f);
    REQUIRE(s.gain_x == Approx(s.gain / 0.7f));
    REQUIRE(s.gain_x < kMaxGain);
    // A smaller screen (bigger gain): the yaw gain stops at the limit, never past it.
    Placement small;
    small.size = 0.6f;
    const HeadLocked t = SolveHeadLocked(small, 60.0f, kAspect3ds, Symmetric(50), 0.0f, o);
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
    // The default on the Frame (gain about 1.3), squeezed: a small head turn moves the game camera
    // by gain times as much, which moves the picture back by the turn at the screen's middle.
    Options o;
    o.squeeze = SqueezeFor(1);
    const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, Symmetric(50), 0.0f, o);
    REQUIRE(s.gain > 1.1f);
    const float turn = Rad(0.5f);
    // Yaw: the camera turns by gain_x x turn; the game's straight-ahead point is then that far to
    // the side in the picture, which is (tan ratio gain_x) turn on the screen.
    const Vec3f cy = Forward(GameCameraRotation(YawRotation(turn), s));
    const float game_yaw = std::atan2(-cy.x, -cy.z);
    REQUIRE(game_yaw / s.gain_x == Approx(turn).epsilon(2e-2));
    const Vec3f cp = Forward(GameCameraRotation(PitchRotation(turn), s));
    REQUIRE((Elevation(cp) - s.pitch_rad) / s.gain == Approx(turn).epsilon(2e-2));
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
    SECTION("new lines are view lines") {
        REQUIRE(store.Format().find("0004000000033500 view -9.50 1.250 0.900") !=
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
    // The owner's file before the comfortable default (sizes relative to the old default).
    const std::string old_file = "# Azahar for Frame: immersive screen placement per game\n"
                                 "0004000000030800 head -3.37 2.982 1.500\n"
                                 "0004000000033500 head -4.30 0.811 0.960\n"
                                 "0004000000054000 head -6.79 0.750 1.500\n";
    Store s;
    s.Parse(old_file);
    REQUIRE(s.Size() == 3);
    REQUIRE(s.OldSize(0x0004000000054000ULL));
    const Placement old = s.Get(0x0004000000054000ULL, false).value();
    REQUIRE(old.size == Approx(1.5f));
    REQUIRE(old.distance == Approx(0.75f));
    // Kept as old lines until converted.
    Store kept;
    kept.Parse(s.Format());
    REQUIRE(kept.OldSize(0x0004000000030800ULL));
    REQUIRE(kept.Get(0x0004000000030800ULL, false)->distance == Approx(2.982f));

    SECTION("converting keeps the screen's angle") {
        ViewFov frame;
        frame.left = Rad(-50.3f);
        frame.right = Rad(50.4f);
        frame.down = Rad(-59.6f);
        frame.up = Rad(49.5f);
        Placement now = old;
        now.size = SizeFromOldDefault(old.size, old, 60.0f, kAspect3ds, frame, 0.0f);
        REQUIRE(now.size > 1.5f); // the default is smaller now
        // The old screen: the whole view at most 1.4x the game's angle, times the old size.
        const float pitch = ViewMiddlePitch(frame) + Rad(old.pitch_deg);
        const float old_tan =
            std::min(FitTanHalfY(frame, kAspect3ds, pitch, Rad(kFitMarginDeg)),
                     kFactorMax * std::tan(Rad(30))) *
            old.size;
        REQUIRE(SolveHeadLocked(now, 60.0f, kAspect3ds, frame).tan_half_y ==
                Approx(old_tan).epsilon(1e-3));
        s.Set(0x0004000000054000ULL, false, now);
        REQUIRE_FALSE(s.OldSize(0x0004000000054000ULL));
        REQUIRE(s.Format().find("0004000000054000 view") != std::string::npos);
        REQUIRE(s.Format().find("0004000000030800 head") != std::string::npos);
    }
    SECTION("a view line wins over an old line, in either order") {
        for (const char* text : {"0004000000054000 head -6 0.75 1.5\n0004000000054000 view -2 1 1.2\n",
                                 "0004000000054000 view -2 1 1.2\n0004000000054000 head -6 0.75 1.5\n"}) {
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
