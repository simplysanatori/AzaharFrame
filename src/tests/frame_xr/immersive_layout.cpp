// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cmath>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/immersive_layout.h"

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

TEST_CASE("FrameXr immersive: size, factor and gain", "[frame_xr]") {
    const float t = std::tan(Rad(30)); // a game camera 60 degrees high
    SECTION("a roomy view: CitraVR's 1.4 at the game's angle, no gain") {
        const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, Symmetric(70));
        REQUIRE(s.factor == Approx(kFactorMax));
        REQUIRE(s.gain == Approx(1.0f));
        REQUIRE(s.tan_half_y == Approx(kFactorMax * t));
    }
    SECTION("smaller: the factor goes down first, the world stays locked") {
        Placement p;
        p.size = 0.85f;
        const HeadLocked s = SolveHeadLocked(p, 60.0f, kAspect3ds, Symmetric(70));
        REQUIRE(s.factor == Approx(kFactorMax * 0.85f));
        REQUIRE(s.gain == Approx(1.0f));
    }
    SECTION("bigger than 1.4: scaled, with the gain that keeps the middle locked") {
        Placement p;
        p.size = 1.2f;
        const HeadLocked s = SolveHeadLocked(p, 60.0f, kAspect3ds, Symmetric(70));
        REQUIRE(s.factor == Approx(kFactorMax));
        REQUIRE(s.tan_half_y == Approx(kFactorMax * t * 1.2f));
        REQUIRE(s.gain == Approx(1.0f / 1.2f));
    }
    SECTION("a view smaller than the game's own frame: factor 1, gain above 1") {
        const HeadLocked s = SolveHeadLocked(Placement{}, 60.0f, kAspect3ds, Symmetric(42));
        REQUIRE(s.factor == Approx(kFactorMin));
        REQUIRE(s.gain > 1.0f);
        REQUIRE(s.tan_half_y * s.gain == Approx(t)); // screen x gain = the game's angle
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

TEST_CASE("FrameXr immersive: the world stays put when the screen is moved", "[frame_xr]") {
    Placement p;
    p.pitch_deg = -12.0f;
    const HeadLocked s = SolveHeadLocked(p, 60.0f, kAspect3ds, Symmetric(70));
    REQUIRE(s.gain == Approx(1.0f));
    // The game camera's forward is drawn at the screen's middle. For the world to stay put, that
    // middle must point (in the room) where the camera looks in the game, for any head pose.
    for (const float yaw : {0.0f, 30.0f, -75.0f}) {
        for (const float pitch : {0.0f, 20.0f, -35.0f}) {
            const Quatf head = In::Normalized(
                In::Multiply(YawRotation(Rad(yaw)), PitchRotation(Rad(pitch))));
            const Vec3f camera = Forward(GameCameraRotation(head, s));
            const Posef quad = HeadLockedQuadPose(s, 0.0f);
            const Vec3f middle = In::Rotate(head, In::Normalized(quad.position));
            REQUIRE(camera.x == Approx(middle.x).margin(1e-4));
            REQUIRE(camera.y == Approx(middle.y).margin(1e-4));
            REQUIRE(camera.z == Approx(middle.z).margin(1e-4));
            // and the quad faces the eye
            const Vec3f normal = In::Rotate(quad.orientation, Vec3f{0.0f, 0.0f, 1.0f});
            REQUIRE(In::Dot(normal, In::Normalized(quad.position)) == Approx(-1.0f));
        }
    }
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
    SECTION("erase") {
        again.Erase(0x0004000000033500ULL, false);
        REQUIRE_FALSE(again.Get(0x0004000000033500ULL, false).has_value());
        REQUIRE(again.Get(0x0004000000033500ULL, true).has_value());
    }
}
