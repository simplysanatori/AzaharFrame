// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/mouse_pointer.h"
#include "frame_xr/controls_presets.h" // AzaharFrame fixes7
#include "frame_xr/input/reticle.h"
#include "frame_xr/input/touchpad_touch.h"

// AzaharFrame mouse: Bottom Screen Reticle.
using Catch::Approx;
using namespace FrameXr::Input;

namespace {

constexpr int64_t kMs = 1'000'000;
constexpr float kHalf = 15.0f; ///< the cone these tests were written for (30 degrees wide)
constexpr float kRadPerDeg = 3.14159265f / 180.0f;

HandInput Aim(float yaw, float pitch, float trigger = 0.0f, float grip = 0.0f) {
    HandInput h;
    h.active = true;
    h.aim.orientation = AimOrientation(DirectionOf(yaw, pitch));
    h.trigger = trigger;
    h.grip = grip;
    return h;
}

struct Rig {
    BottomScreenReticle r;
    int64_t now = 1'000 * kMs;
    ReticleFrame f;
    ReticleEdge edge = ReticleEdge::Follow;
    float half = kHalf;
    ReticleModes modes; ///< AzaharFrame fixes7
    Vec2f stick;        ///< AzaharFrame fixes7: the right stick (Right Stick Moves Touch)
    ReticleFrame& Step(const HandInput& h, int frames = 1) {
        for (int i = 0; i < frames; i++) {
            now += 11 * kMs;
            ReticleAim aim = ReticleAimOf(h);
            aim.stick = stick;
            f = r.Update(aim, true, half, now, edge, modes);
        }
        return f;
    }
};

} // namespace

TEST_CASE("Reticle: angle to point", "[frame_xr]") {
    // AzaharFrame input2: its own cone, not the mouse's Cursor Speed.
    CHECK(ReticleHalfWidthDeg(ReticleHand::Left) == Approx(20.0f));
    CHECK(ReticleHalfWidthDeg(ReticleHand::Right) == Approx(20.0f));
    CHECK(ReticleHalfWidthDeg(ReticleHand::Headset) == Approx(15.0f));
    CHECK(ReticleHalfWidthDeg(ReticleHand::Off) == Approx(0.0f));
    Vec2f p = ReticlePoint(0.0f, 0.0f, 15.0f);
    CHECK(p.x == Approx(0.5f));
    CHECK(p.y == Approx(0.5f));
    p = ReticlePoint(15.0f, 0.0f, 15.0f); // 15 degrees left: the left edge
    CHECK(p.x == Approx(0.0f));
    p = ReticlePoint(-7.5f, 5.625f, 15.0f); // right and up (the height is 3/4 of the width)
    CHECK(p.x == Approx(0.75f));
    CHECK(p.y == Approx(0.25f));
    p = ReticlePoint(-40.0f, -40.0f, 15.0f); // clamped
    CHECK(p.x == Approx(1.0f));
    CHECK(p.y == Approx(1.0f));
    CHECK(ReticleHandIndex(ReticleHand::Off) == -1);
    CHECK(ReticleHandIndex(ReticleHandFrom(1)) == HandIndex(Hand::Left));
    CHECK(ReticleHandIndex(ReticleHandFrom(2)) == HandIndex(Hand::Right));
    CHECK(ReticleHandFrom(3) == ReticleHand::Headset);
    CHECK(ReticleHandIndex(ReticleHand::Headset) == -1);
    CHECK(ReticleHandFrom(4) == ReticleHand::Off);
}

TEST_CASE("Reticle: absolute aim from where it starts, with edge drift", "[frame_xr]") {
    Rig t;
    // Wherever it points first is the middle.
    ReticleFrame f = t.Step(Aim(30.0f, 10.0f));
    REQUIRE(f.on);
    CHECK(f.point.x == Approx(0.5f));
    CHECK(f.point.y == Approx(0.5f));
    // 7.5 degrees right, 5.625 down: three quarters across, three quarters down.
    f = t.Step(Aim(22.5f, 4.375f), 90);
    CHECK(f.point.x == Approx(0.75f).margin(0.01));
    CHECK(f.point.y == Approx(0.75f).margin(0.01));
    // Back: the same direction is the same point.
    f = t.Step(Aim(30.0f, 10.0f), 90);
    CHECK(f.point.x == Approx(0.5f).margin(0.01));
    // 25 degrees left: on the left edge, the reference dragged about 10 degrees along (the yaw
    // across the cone is the turn times the cosine of the reference's pitch).
    const float ys = std::cos(10.0f * kRadPerDeg);
    f = t.Step(Aim(55.0f, 10.0f), 90);
    CHECK(f.point.x == Approx(0.0f).margin(0.01));
    CHECK(t.r.ref_yaw_deg() == Approx(55.0f - 15.0f / ys).margin(0.05));
    // Turning back moves the point at once: 10 degrees back is that far from the edge.
    f = t.Step(Aim(45.0f, 10.0f), 90);
    CHECK(f.point.x == Approx(10.0f * ys / 30.0f).margin(0.005));
}

TEST_CASE("Reticle: grip and Recentre re-zero, the trigger touches", "[frame_xr]") {
    Rig t;
    t.Step(Aim(0.0f, 0.0f));
    ReticleFrame f = t.Step(Aim(-10.0f, 0.0f), 90);
    CHECK(f.point.x > 0.8f);
    // A grip press: the middle is where it points now.
    f = t.Step(Aim(-10.0f, 0.0f, 0.0f, 0.9f));
    CHECK(f.rezeroed);
    CHECK(f.point.x == Approx(0.5f));
    f = t.Step(Aim(-10.0f, 0.0f, 0.0f, 0.9f), 10); // held: once
    CHECK_FALSE(f.rezeroed);
    CHECK(f.point.x == Approx(0.5f).margin(0.01));
    t.Step(Aim(-10.0f, 0.0f), 2);
    f = t.Step(Aim(-15.0f, 0.0f), 90);
    CHECK(f.point.x == Approx(0.5f + 5.0f / 30.0f).margin(0.01));
    t.r.Rezero(); // Recenter
    f = t.Step(Aim(-15.0f, 0.0f));
    CHECK(f.rezeroed);
    CHECK(f.point.x == Approx(0.5f));
    // The trigger, with the mapper's thresholds.
    CHECK(t.Step(Aim(-15.0f, 0.0f, 0.6f)).down);
    CHECK(t.Step(Aim(-15.0f, 0.0f, 0.45f)).down);
    CHECK_FALSE(t.Step(Aim(-15.0f, 0.0f, 0.3f)).down);
}

TEST_CASE("Reticle: tremor stays put, slow moves go through", "[frame_xr]") {
    Rig t;
    t.Step(Aim(0.0f, 0.0f));
    ReticleFrame f = t.Step(Aim(-6.0f, 3.0f), 90);
    const Vec2f rest = f.point;
    // A hand's tremor: 0.05 degrees either way every frame.
    for (int i = 0; i < 120; i++) {
        const float j = (i % 2 == 0) ? 0.05f : -0.05f;
        f = t.Step(Aim(-6.0f + j, 3.0f - j));
        CHECK(f.point.x == Approx(rest.x).margin(0.0005));
        CHECK(f.point.y == Approx(rest.y).margin(0.0005));
    }
    // A slow, fine move (2 degrees a second for a second) is not lost.
    for (int i = 1; i <= 90; i++) {
        f = t.Step(Aim(-6.0f - 2.0f * static_cast<float>(i) / 90.0f, 3.0f));
    }
    f = t.Step(Aim(-8.0f, 3.0f), 60);
    CHECK(f.point.x == Approx(0.5f + 8.0f / 30.0f).margin(0.01));
}

TEST_CASE("Reticle: off, untracked and turning through behind you", "[frame_xr]") {
    Rig t;
    t.Step(Aim(0.0f, 0.0f));
    t.Step(Aim(-10.0f, 0.0f), 30);
    HandInput gone = Aim(-10.0f, 0.0f);
    gone.active = false;
    CHECK_FALSE(t.Step(gone).on);
    // Back: the middle where it points.
    ReticleFrame f = t.Step(Aim(-20.0f, 0.0f));
    CHECK(f.on);
    CHECK(f.point.x == Approx(0.5f));
    t.now += 11 * kMs;
    CHECK_FALSE(t.r.Update(ReticleAimOf(Aim(-20.0f, 0.0f)), false, kHalf, t.now).on);

    // From 179 degrees to -179 is 2 degrees further left, not 358 to the right.
    Rig w;
    w.Step(Aim(179.0f, 0.0f));
    f = w.Step(Aim(-179.0f, 0.0f), 90);
    CHECK(f.point.x == Approx(0.5f - 2.0f / 30.0f).margin(0.01));
}

TEST_CASE("Reticle: touches through the cursor pipeline, the bottom screen hidden too",
          "[frame_xr]") {
    TouchpadTouch t;
    TouchpadFrame fr;
    fr.time_ns = 1'000 * kMs;
    fr.bottom_visible = false;
    fr.reticle = {true, {0.25f, 0.5f}, false};
    TouchpadResult r = t.Update(fr);
    CHECK(r.event_count == 0);
    CHECK_FALSE(r.cursor.visible); // no dot on a hidden screen
    fr.reticle.down = true;
    fr.time_ns += 11 * kMs;
    r = t.Update(fr);
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Pressed);
    CHECK(r.events[0].x == Approx(80.0f));
    CHECK(r.events[0].y == Approx(120.0f));
    CHECK(t.owner() == TouchSource::Reticle);
    fr.reticle.point = {0.5f, 0.5f};
    fr.time_ns += 11 * kMs;
    r = t.Update(fr);
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Moved);
    CHECK(r.events[0].x == Approx(160.0f));
    // At the edge it stays down (no Recentre at Edge: the reticle's edges drift).
    fr.reticle.point = {0.0f, 0.5f};
    fr.time_ns += 11 * kMs;
    r = t.Update(fr);
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Moved);
    fr.time_ns += 11 * kMs;
    CHECK(t.Update(fr).event_count == 0);
    // Shown: the dot, pressed.
    fr.bottom_visible = true;
    fr.time_ns += 11 * kMs;
    r = t.Update(fr);
    CHECK(r.cursor.visible);
    CHECK(r.cursor.source == TouchSource::Reticle);
    CHECK(r.cursor.pressed);
    fr.reticle.down = false;
    fr.time_ns += 11 * kMs;
    r = t.Update(fr);
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Released);
}

TEST_CASE("Reticle: Snap to Middle lifts at the edge and touches again at the middle",
          "[frame_xr]") {
    CHECK(ReticleEdgeFrom(0) == ReticleEdge::Follow);
    CHECK(ReticleEdgeFrom(1) == ReticleEdge::SnapToMiddle);
    Rig t;
    t.edge = ReticleEdge::SnapToMiddle;
    TouchpadTouch touch;
    TouchpadFrame fr;
    fr.bottom_visible = true;
    // One frame of both: the reticle, then the touch pipeline as the bridge runs it.
    std::vector<TouchEvent> seen;
    const auto frame = [&](const HandInput& h) {
        const ReticleFrame& f = t.Step(h);
        fr.time_ns = t.now;
        fr.reticle = {f.on, f.point, f.down};
        const TouchpadResult r = touch.Update(fr);
        seen.assign(r.events.begin(), r.events.begin() + r.event_count);
        return f;
    };
    frame(Aim(0.0f, 0.0f));
    frame(Aim(0.0f, 0.0f, 1.0f));
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].type == TouchEventType::Pressed);
    CHECK(seen[0].x == Approx(160.0f));
    // A swipe right, 3 degrees a frame (about 270 degrees a second), the trigger held.
    float yaw = 0.0f;
    int snapped_at = -1;
    std::vector<std::vector<TouchEvent>> log;
    for (int i = 0; i < 40; i++) {
        yaw -= 3.0f;
        const ReticleFrame f = frame(Aim(yaw, 0.0f, 1.0f));
        log.push_back(seen);
        if (f.snapped && snapped_at < 0) {
            snapped_at = i;
        }
    }
    REQUIRE(snapped_at > 0);
    // Before the snap the touch dragged right toward the edge.
    bool moved_right = false;
    for (int i = 0; i < snapped_at; i++) {
        for (const TouchEvent& e : log[static_cast<std::size_t>(i)]) {
            CHECK(e.type == TouchEventType::Moved);
            moved_right = moved_right || e.x > 240.0f;
        }
    }
    CHECK(moved_right);
    // The snap frame: a release (the game sees the stylus lift)...
    const auto& at = log[static_cast<std::size_t>(snapped_at)];
    REQUIRE(at.size() == 1);
    CHECK(at[0].type == TouchEventType::Released);
    // ...then nothing for at least one frame, then a touch at the middle.
    int pressed_at = -1;
    for (int i = snapped_at + 1; i < 40 && pressed_at < 0; i++) {
        for (const TouchEvent& e : log[static_cast<std::size_t>(i)]) {
            if (e.type == TouchEventType::Pressed) {
                pressed_at = i;
                CHECK(e.x == Approx(160.0f).margin(1.0f));
                CHECK(e.y == Approx(120.0f).margin(1.0f));
            } else {
                FAIL("only a press may follow the lift");
            }
        }
    }
    REQUIRE(pressed_at > snapped_at + 1);
    CHECK((pressed_at - snapped_at) * 11 * kMs >= BottomScreenReticle::kLiftNs);
    // And the drag goes on from the middle, to the right.
    bool dragged_on = false;
    for (int i = pressed_at + 1; i < 40; i++) {
        for (const TouchEvent& e : log[static_cast<std::size_t>(i)]) {
            if (e.type == TouchEventType::Moved && e.x > 160.0f) {
                dragged_on = true;
            }
        }
    }
    CHECK(dragged_on);
}

TEST_CASE("Reticle: Snap to Middle waits before it snaps again", "[frame_xr]") {
    Rig t;
    t.edge = ReticleEdge::SnapToMiddle;
    t.Step(Aim(0.0f, 0.0f));
    // No touch: past the right edge (15 degrees) the point snaps to the middle at once.
    ReticleFrame f = t.Step(Aim(-16.0f, 0.0f));
    int frames = 1;
    while (!f.snapped && frames < 30) {
        f = t.Step(Aim(-16.0f, 0.0f));
        frames++;
    }
    REQUIRE(f.snapped);
    CHECK(f.point.x == Approx(0.5f));
    CHECK_FALSE(f.down);
    // Straight on past the next edge within the cooldown: no second snap, the edge drifts.
    f = t.Step(Aim(-40.0f, 0.0f));
    CHECK_FALSE(f.snapped);
    for (int i = 0; i < 10; i++) { // 110 ms, inside the 200 ms
        f = t.Step(Aim(-40.0f, 0.0f));
        CHECK_FALSE(f.snapped);
    }
    CHECK(f.point.x > 0.9f);
    // Back inside until the cooldown is over; then past the edge snaps again.
    for (int i = 0; i < 30; i++) {
        CHECK_FALSE(t.Step(Aim(-25.0f, 0.0f)).snapped);
    }
    bool again = false;
    for (int i = 0; i < 10 && !again; i++) {
        again = t.Step(Aim(-60.0f, 0.0f)).snapped;
    }
    CHECK(again);
    // Follow never snaps.
    Rig follow;
    follow.Step(Aim(0.0f, 0.0f));
    for (int i = 0; i < 60; i++) {
        CHECK_FALSE(follow.Step(Aim(-60.0f, 0.0f)).snapped);
    }
    CHECK(follow.f.point.x == Approx(1.0f).margin(0.01));
}

// AzaharFrame input2: the head aims, either trigger touches, the grips do nothing to it.
TEST_CASE("Reticle: Headset", "[frame_xr]") {
    HandInput left = Aim(50.0f, -30.0f);
    HandInput right = Aim(-50.0f, -30.0f);
    ReticleAim a = ReticleHeadAim(true, AimOrientation(DirectionOf(0.0f, 0.0f)), left, right);
    CHECK(a.active);
    CHECK(a.trigger == 0.0f);
    CHECK(a.grip == 0.0f);
    left.trigger = 0.7f;
    CHECK(ReticleHeadAim(true, a.orientation, left, right).trigger == Approx(0.7f));
    left.trigger = 0.0f;
    right.trigger = 0.9f;
    right.grip = 1.0f;
    a = ReticleHeadAim(true, a.orientation, left, right);
    CHECK(a.trigger == Approx(0.9f));
    CHECK(a.grip == 0.0f); // a grip grabs screens as before, it never re-zeroes the head
    CHECK_FALSE(ReticleHeadAim(false, a.orientation, left, right).active);

    BottomScreenReticle r;
    int64_t now = 1'000 * kMs;
    const float hw = ReticleHalfWidthDeg(ReticleHand::Headset);
    const auto head = [&](float yaw, float pitch, float trigger, int frames) {
        ReticleFrame f;
        for (int i = 0; i < frames; i++) {
            now += 11 * kMs;
            ReticleAim h{true, AimOrientation(DirectionOf(yaw, pitch)), trigger, 0.0f};
            f = r.Update(h, true, hw, now);
        }
        return f;
    };
    ReticleFrame f = head(0.0f, 0.0f, 0.0f, 1);
    CHECK(f.point.x == Approx(0.5f));
    // The head turned 7.5 degrees right: a quarter of the cone right of the middle.
    f = head(-7.5f, 0.0f, 0.0f, 90);
    CHECK(f.point.x == Approx(0.75f).margin(0.01));
    CHECK_FALSE(f.down);
    CHECK(head(-7.5f, 0.0f, 0.8f, 1).down);
    // Recenter: the middle is where the head points.
    r.Rezero();
    f = head(-7.5f, 0.0f, 0.0f, 1);
    CHECK(f.rezeroed);
    CHECK(f.point.x == Approx(0.5f));
}

// AzaharFrame input2: a controller held pointing down is as fast sideways as up and down.
TEST_CASE("Reticle: the yaw across the cone at the reference's pitch", "[frame_xr]") {
    Rig t;
    t.half = ReticleHalfWidthDeg(ReticleHand::Right);
    t.Step(Aim(0.0f, -40.0f));
    ReticleFrame f = t.Step(Aim(-10.0f, -40.0f), 120);
    const float across = 10.0f * std::cos(40.0f * kRadPerDeg); // about 7.7 degrees
    CHECK(f.point.x == Approx(0.5f + across / 40.0f).margin(0.003));
    // Up and down as before: 7.7 degrees up.
    t.Step(Aim(0.0f, -40.0f), 120);
    f = t.Step(Aim(0.0f, -40.0f + across), 120);
    CHECK(f.point.y == Approx(0.5f - across / 30.0f).margin(0.003));
}

// AzaharFrame input2: the point keeps up with a steady move (it trailed 0.6 degrees at 20
// degrees a second before), and a flick settles at once.
TEST_CASE("Reticle: little lag in a move", "[frame_xr]") {
    Rig t;
    t.half = ReticleHalfWidthDeg(ReticleHand::Right);
    std::mt19937 rng(7);
    std::normal_distribution<float> tremor(0.0f, 0.03f);
    t.Step(Aim(10.0f, 0.0f));
    // 20 degrees a second to the right, for 0.9 s (from 10 left to 8 right).
    float yaw = 10.0f;
    double lag = 0.0;
    int n = 0;
    for (int i = 0; i < 82; i++) {
        yaw -= 20.0f * 0.011f;
        const ReticleFrame& f = t.Step(Aim(yaw + tremor(rng), 0.0f));
        if (i >= 40) {
            const float shown = 10.0f - (f.point.x - 0.5f) * 40.0f; // the yaw the point shows
            lag += shown - yaw;
            n++;
        }
    }
    CHECK(lag / n < 0.4);
    CHECK(lag / n > 0.0);
    // A flick of 10 degrees in 0.1 s: within 0.2 degrees 50 ms after it stops.
    Rig k;
    k.half = 20.0f;
    k.Step(Aim(0.0f, 0.0f));
    for (int i = 1; i <= 9; i++) {
        k.Step(Aim(-10.0f * static_cast<float>(i) / 9.0f, 0.0f));
    }
    const ReticleFrame& f = k.Step(Aim(-10.0f, 0.0f), 5);
    CHECK((f.point.x - 0.5f) * 40.0f == Approx(10.0f).margin(0.2));
}

// AzaharFrame input2: at an edge, tremor moves the reference at most by its own size; it does
// not creep a little further every time it crosses.
TEST_CASE("Reticle: tremor at an edge does not creep the reference", "[frame_xr]") {
    Rig t;
    t.half = 20.0f;
    t.Step(Aim(0.0f, 0.0f));
    t.Step(Aim(-20.0f, 0.0f), 120); // on the right edge
    const float ref0 = t.r.ref_yaw_deg();
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> tremor(-0.15f, 0.15f);
    for (int i = 0; i < 900; i++) { // 10 s
        t.Step(Aim(-20.0f + tremor(rng), tremor(rng)));
    }
    CHECK(std::fabs(t.r.ref_yaw_deg() - ref0) < 0.15f);
    CHECK(std::fabs(t.r.ref_pitch_deg()) < 0.15f);
    // Back to the middle: the same direction is the same point as before.
    const ReticleFrame& f = t.Step(Aim(0.0f, 0.0f), 120);
    CHECK(f.point.x == Approx(0.5f).margin(0.008));
}

// AzaharFrame polish: which controller drives the reticle (no beam, no pointer) this frame:
// only the chosen one, only in a game's frame, only while tracked. Everywhere else every
// controller is a normal pointer.
TEST_CASE("Reticle: the controller that drives it", "[frame_xr]") {
    InputFrame in;
    in.hand[0].active = true;
    in.hand[1].active = true;

    SECTION("in a game: the chosen controller") {
        CHECK(ReticleDrivingHand(ReticleHand::Right, true, in) == 1);
        CHECK(ReticleDrivingHand(ReticleHand::Left, true, in) == 0);
    }
    SECTION("Off and Headset take no controller") {
        CHECK(ReticleDrivingHand(ReticleHand::Off, true, in) == -1);
        CHECK(ReticleDrivingHand(ReticleHand::Headset, true, in) == -1);
        CHECK(ReticleDrivingHand(ReticleHandFrom(7), true, in) == -1);
    }
    SECTION("not a game's frame (in-game menu, Azahar's window, keyboard): a normal pointer") {
        for (const ReticleHand hand : {ReticleHand::Left, ReticleHand::Right}) {
            CHECK(ReticleDrivingHand(hand, false, in) == -1);
        }
    }
    SECTION("not tracked: nothing to hide") {
        in.hand[1].active = false;
        CHECK(ReticleDrivingHand(ReticleHand::Right, true, in) == -1);
        CHECK(ReticleDrivingHand(ReticleHand::Left, true, in) == 0);
    }
    SECTION("the setting's values") {
        CHECK(ReticleDrivingHand(ReticleHandFrom(1), true, in) == 0);
        CHECK(ReticleDrivingHand(ReticleHandFrom(2), true, in) == 1);
        CHECK(ReticleDrivingHand(ReticleHandFrom(3), true, in) == -1);
        CHECK(ReticleDrivingHand(ReticleHandFrom(0), true, in) == -1);
    }
}

// AzaharFrame reticle: Reticle Speed scales the turn from where aiming started (the cone is the
// 1x cone over the speed), one value for the head and the controllers.
TEST_CASE("Reticle: Reticle Speed", "[frame_xr][reticle]") {
    CHECK(kReticleSpeedDefault == 2.0f);
    CHECK(ClampReticleSpeed(0.1f) == Approx(0.5f));
    CHECK(ClampReticleSpeed(9.0f) == Approx(4.0f));
    CHECK(ClampReticleSpeed(2.5f) == Approx(2.5f));
    CHECK(ClampReticleSpeed(std::nanf("")) == Approx(kReticleSpeedDefault));
    CHECK(ReticleHalfWidthDeg(ReticleHand::Headset, 1.0f) == Approx(15.0f));
    CHECK(ReticleHalfWidthDeg(ReticleHand::Headset, 2.0f) == Approx(7.5f));
    CHECK(ReticleHalfWidthDeg(ReticleHand::Right, 2.0f) == Approx(10.0f));
    CHECK(ReticleHalfWidthDeg(ReticleHand::Left, 0.5f) == Approx(40.0f));
    CHECK(ReticleHalfWidthDeg(ReticleHand::Off, 2.0f) == Approx(0.0f));
    CHECK(ReticleHalfWidthDeg(ReticleHand::Headset, 99.0f) == Approx(15.0f / 4.0f)); // clamped

    // The same 5 degree head turn right: a sixth of the width at 1x, a third at 2x.
    for (const float speed : {1.0f, 2.0f, 3.0f}) {
        INFO("speed " << speed);
        BottomScreenReticle r;
        int64_t now = 1'000 * kMs;
        const float hw = ReticleHalfWidthDeg(ReticleHand::Headset, speed);
        const auto head = [&](float yaw, float pitch, int frames) {
            ReticleFrame f;
            for (int i = 0; i < frames; i++) {
                now += 11 * kMs;
                ReticleAim h{true, AimOrientation(DirectionOf(yaw, pitch)), 0.0f, 0.0f};
                f = r.Update(h, true, hw, now);
            }
            return f;
        };
        // The dead zone (kDeadzoneDeg of turn) moves the point that much less, at the speed.
        const float dx = BottomScreenReticle::kDeadzoneDeg * speed / 30.0f + 0.002f;
        const float dy = BottomScreenReticle::kDeadzoneDeg * speed / 22.5f + 0.002f;
        head(0.0f, 0.0f, 1);
        ReticleFrame f = head(-5.0f, 0.0f, 120);
        CHECK(f.point.x == Approx(std::min(1.0f, 0.5f + 5.0f * speed / 30.0f)).margin(dx));
        f = head(0.0f, 3.0f, 120); // 3 degrees up
        CHECK(f.point.y == Approx(0.5f - 3.0f * speed / 22.5f).margin(dy));
        // Clamped to the edges, and back at once (the edge drift): 20 degrees right is past
        // the edge at any speed here.
        f = head(-20.0f, 0.0f, 120);
        CHECK(f.point.x == Approx(1.0f).margin(dx));
        f = head(-20.0f + 15.0f / speed, 0.0f, 120); // turned back half the screen
        CHECK(f.point.x == Approx(0.5f).margin(dx));
        // Recentrable: Recenter puts the middle where the head points
        r.Rezero();
        f = head(-30.0f, 10.0f, 1);
        CHECK(f.rezeroed);
        CHECK(f.point.x == Approx(0.5f));
        CHECK(f.point.y == Approx(0.5f));
    }
}

// AzaharFrame reticle: the turn that covers the whole bottom screen, edge to edge, measured by a
// slow sweep from the middle: at the default half as much as at 1x, for the head (30 x 22.5
// degrees to 15 x 11.25) and a controller (40 x 30 to 20 x 15).
TEST_CASE("Reticle: the default speed halves the turn across the screen", "[frame_xr][reticle]") {
    // The turn from the middle at which the point reaches the right (or top) edge: where the
    // edge drift starts to move the reference along.
    const auto to_edge = [](ReticleHand source, float speed, bool vertical) {
        BottomScreenReticle r;
        int64_t now = 1'000 * kMs;
        const float hw = ReticleHalfWidthDeg(source, speed);
        for (int step = 0; step <= 2000; step++) {
            const float a = 0.02f * static_cast<float>(step); // 0.02 degrees each 22 ms
            ReticleFrame f;
            for (int i = 0; i < 2; i++) {
                now += 11 * kMs;
                ReticleAim h{true,
                             AimOrientation(vertical ? DirectionOf(0.0f, a) : DirectionOf(-a, 0.0f)),
                             0.0f, 0.0f};
                f = r.Update(h, true, hw, now);
            }
            (void)f;
            if (std::fabs(vertical ? r.ref_pitch_deg() : r.ref_yaw_deg()) > 0.001f) {
                return a;
            }
        }
        return -1.0f;
    };
    // Edge to edge is twice the turn from the middle (the cone is symmetric).
    CHECK(2.0f * to_edge(ReticleHand::Headset, 1.0f, false) == Approx(30.0f).margin(0.4));
    CHECK(2.0f * to_edge(ReticleHand::Headset, kReticleSpeedDefault, false) ==
          Approx(15.0f).margin(0.4));
    CHECK(2.0f * to_edge(ReticleHand::Headset, 1.0f, true) == Approx(22.5f).margin(0.4));
    CHECK(2.0f * to_edge(ReticleHand::Headset, kReticleSpeedDefault, true) ==
          Approx(11.25f).margin(0.4));
    CHECK(2.0f * to_edge(ReticleHand::Right, 1.0f, false) == Approx(40.0f).margin(0.4));
    CHECK(2.0f * to_edge(ReticleHand::Right, kReticleSpeedDefault, false) ==
          Approx(20.0f).margin(0.4));
}

// AzaharFrame fixes7: Aim Only While Pressed: in the middle until the trigger touches; while it is
// held the aim moves it from the middle; let go, it is back in the middle.
TEST_CASE("Reticle: Aim Only While Pressed", "[frame_xr][reticle]") {
    Rig t;
    t.modes.center_until_pressed = true;
    ReticleFrame f = t.Step(Aim(0.0f, 0.0f));
    CHECK(f.point.x == Approx(0.5f));
    // Turning without the trigger: still the middle, not touching.
    f = t.Step(Aim(-10.0f, 5.0f), 90);
    CHECK(f.point.x == Approx(0.5f));
    CHECK(f.point.y == Approx(0.5f));
    CHECK_FALSE(f.down);
    // The trigger goes down: the touch starts in the middle, whatever it aimed at before.
    f = t.Step(Aim(-10.0f, 5.0f, 0.9f));
    CHECK(f.down);
    CHECK(f.point.x == Approx(0.5f));
    CHECK(f.point.y == Approx(0.5f));
    // Held, 7.5 degrees right: three quarters across.
    f = t.Step(Aim(-17.5f, 5.0f, 0.9f), 90);
    CHECK(f.down);
    CHECK(f.point.x == Approx(0.75f).margin(0.01));
    // Let go: back in the middle at once.
    f = t.Step(Aim(-17.5f, 5.0f, 0.0f));
    CHECK_FALSE(f.down);
    CHECK(f.point.x == Approx(0.5f));
    CHECK(f.point.y == Approx(0.5f));
    // Off: the aim moves it as before (absolute from where it was re-centred).
    t.modes.center_until_pressed = false;
    f = t.Step(Aim(-25.0f, 5.0f), 90);
    CHECK(f.point.x == Approx(0.75f).margin(0.01));
}

// AzaharFrame fixes7: Right Stick Moves Touch (was Stick Moves Reticle): the right stick moves the
// point (with the aim, during a touch too); full tilt crosses the screen in a second.
TEST_CASE("Reticle: Right Stick Moves Touch", "[frame_xr][reticle]") {
    Rig t;
    t.modes.stick_moves = true;
    t.Step(Aim(0.0f, 0.0f));
    // Inside the dead zone: nothing.
    t.stick = {0.1f, 0.1f};
    ReticleFrame f = t.Step(Aim(0.0f, 0.0f), 50);
    CHECK(f.point.x == Approx(0.5f).margin(0.002));
    // Full tilt right for a quarter of a second: a quarter of the way across.
    t.stick = {1.0f, 0.0f};
    f = t.Step(Aim(0.0f, 0.0f, 0.9f), 23); // 253 ms
    CHECK(f.down);
    CHECK(f.point.x == Approx(0.75f).margin(0.02));
    CHECK(f.point.y == Approx(0.5f).margin(0.002));
    // Up moves it up; the head's turn adds to the stick's.
    t.stick = {0.0f, 1.0f};
    f = t.Step(Aim(0.0f, 0.0f, 0.9f), 9); // 99 ms: a tenth of the height
    CHECK(f.point.y == Approx(0.4f).margin(0.02));
    t.stick = {};
    const float x_before = f.point.x;
    f = t.Step(Aim(-3.0f, 0.0f, 0.9f), 90); // 3 degrees right: a tenth of the 30 degree cone
    CHECK(f.point.x == Approx(x_before + 0.1f).margin(0.01));
    // Into the edge: it stays on the edge (the edges drift as with the aim).
    t.stick = {1.0f, 0.0f};
    f = t.Step(Aim(-3.0f, 0.0f, 0.9f), 120);
    CHECK(f.point.x == Approx(1.0f).margin(0.005)); // the dead zone trails by 0.08 degrees
    t.stick = {-1.0f, 0.0f};
    f = t.Step(Aim(-3.0f, 0.0f, 0.9f), 23);
    CHECK(f.point.x == Approx(0.75f).margin(0.02));
    // Off: the stick does nothing.
    t.modes.stick_moves = false;
    const float x_off = f.point.x;
    f = t.Step(Aim(-3.0f, 0.0f, 0.9f), 50);
    CHECK(f.point.x == Approx(x_off).margin(0.002));
}

// AzaharFrame fixes7: both on: the stick does nothing while the reticle waits in the middle,
// and moves it during a touch.
TEST_CASE("Reticle: Aim Only While Pressed with Right Stick Moves Touch", "[frame_xr][reticle]") {
    Rig t;
    t.modes = {true, true};
    t.stick = {1.0f, 0.0f};
    ReticleFrame f = t.Step(Aim(0.0f, 0.0f), 30);
    CHECK(f.point.x == Approx(0.5f));
    f = t.Step(Aim(0.0f, 0.0f, 0.9f), 23);
    CHECK(f.point.x == Approx(0.75f).margin(0.03));
    f = t.Step(Aim(0.0f, 0.0f, 0.0f));
    CHECK(f.point.x == Approx(0.5f));
}

// AzaharFrame fixes7, kiu: Kid Icarus: Uprising's Controls preset (every region): Headset,
// C-Stick at Edge on, Motion Controls Off; Right Stick Moves Touch is not given (the global
// default, off).
TEST_CASE("Reticle: Kid Icarus: Uprising's Controls preset", "[frame_xr][reticle]") {
    for (const uint64_t id : {0x0004000000030000ULL, 0x0004000000030100ULL, 0x0004000000030200ULL,
                              0x0004000000030300ULL}) { // kiu: 30000, Japan in the mod's list
        const FrameXr::ControlsPreset* p = FrameXr::FindControlsPreset(id);
        REQUIRE(p != nullptr);
        CHECK(p->bottom_screen_reticle == 3);
        CHECK(p->edge_cstick);
        CHECK(p->motion_controls == 0);
    }
    CHECK(FrameXr::FindControlsPreset(0x0004000000033500ULL) == nullptr);
}

// AzaharFrame kiu: Right Stick Moves Touch off never swallows the C-Stick; on, the stick is the
// reticle's and the game gets none of it while the reticle is live.
TEST_CASE("Reticle: where the right stick goes", "[frame_xr][reticle][kiu]") {
    const Vec2f stick{0.6f, -0.3f};
    for (const bool live : {false, true}) {
        const ReticleStickSplit off = SplitReticleStick(false, live, stick);
        CHECK(off.to_reticle.x == 0.0f);
        CHECK(off.to_reticle.y == 0.0f);
        CHECK(off.to_game.x == stick.x); // the C-Stick as it is
        CHECK(off.to_game.y == stick.y);
    }
    const ReticleStickSplit on = SplitReticleStick(true, true, stick);
    CHECK(on.to_reticle.x == stick.x);
    CHECK(on.to_reticle.y == stick.y);
    CHECK(on.to_game.x == 0.0f);
    CHECK(on.to_game.y == 0.0f);
    // On, but the reticle not live (the controller untracked): the game keeps its C-Stick.
    const ReticleStickSplit idle = SplitReticleStick(true, false, stick);
    CHECK(idle.to_game.x == stick.x);
    CHECK(idle.to_game.y == stick.y);
}

// AzaharFrame kiu: C-Stick at Edge: the band follows the cone (any source, any Reticle Speed),
// the push grows from 0 at the band's start to full tilt at the edge, and there is none outside
// the band, lifted, or switched off.
TEST_CASE("Reticle: C-Stick at Edge", "[frame_xr][reticle][kiu]") {
    // The band is kEdgeBandDeg of aim at every speed and source, within its clamp.
    for (const ReticleHand src : {ReticleHand::Left, ReticleHand::Right, ReticleHand::Headset}) {
        for (float speed = kReticleSpeedMin; speed <= kReticleSpeedMax + 0.001f; speed += 0.1f) {
            const float hw = ReticleHalfWidthDeg(src, speed);
            const ReticleEdgeBand b = ReticleEdgeBandFor(hw);
            REQUIRE(b.On());
            CHECK(b.x >= kEdgeBandMin);
            CHECK(b.x <= kEdgeBandMax);
            CHECK(b.y >= kEdgeBandMin);
            CHECK(b.y <= kEdgeBandMax);
            const float want_x = kEdgeBandDeg / (2.0f * hw);
            if (want_x > kEdgeBandMin && want_x < kEdgeBandMax) {
                CHECK(b.x * 2.0f * hw == Approx(kEdgeBandDeg)); // the same turn enters it
            }
            CHECK(b.y >= b.x); // the screen is shorter: a larger share of its height
        }
    }
    CHECK_FALSE(ReticleEdgeBandFor(0.0f).On()); // Off
    const ReticleEdgeBand band = ReticleEdgeBandFor(ReticleHalfWidthDeg(ReticleHand::Right, 2.0f));
    CHECK(band.x == Approx(0.15f)); // a controller at 2x: 3 of its 20 degrees
    CHECK(band.y == Approx(0.2f));
    // The middle and anywhere inside the band's inner edge: nothing.
    for (const Vec2f p : {Vec2f{0.5f, 0.5f}, Vec2f{band.x + 0.001f, 0.5f},
                          Vec2f{1.0f - band.x - 0.001f, 0.5f}, Vec2f{0.5f, band.y + 0.001f}}) {
        const Vec2f s = ReticleEdgeStick(p, true, band);
        CHECK(s.x == 0.0f);
        CHECK(s.y == 0.0f);
    }
    // Into the band: outward, by how far in (linear), full tilt at the edge.
    for (int i = 0; i <= 10; i++) {
        const float t = static_cast<float>(i) / 10.0f;
        const Vec2f right = ReticleEdgeStick({1.0f - band.x + t * band.x, 0.5f}, true, band);
        const Vec2f left = ReticleEdgeStick({band.x - t * band.x, 0.5f}, true, band);
        const Vec2f top = ReticleEdgeStick({0.5f, band.y - t * band.y}, true, band);
        const Vec2f bottom = ReticleEdgeStick({0.5f, 1.0f - band.y + t * band.y}, true, band);
        CHECK(right.x == Approx(t).margin(1e-4));
        CHECK(left.x == Approx(-t).margin(1e-4));
        CHECK(top.y == Approx(t).margin(1e-4)); // the top is up on the stick
        CHECK(bottom.y == Approx(-t).margin(1e-4));
        CHECK(right.y == 0.0f);
        CHECK(top.x == 0.0f);
    }
    // A corner pushes both ways, no further than full tilt.
    const Vec2f corner = ReticleEdgeStick({1.0f, 0.0f}, true, band);
    CHECK(corner.x > 0.0f);
    CHECK(corner.y > 0.0f);
    CHECK(std::sqrt(corner.x * corner.x + corner.y * corner.y) == Approx(1.0f));
    // Lifted or off: nothing, at the edge too.
    const Vec2f lifted = ReticleEdgeStick({1.0f, 0.5f}, false, band);
    CHECK(lifted.x == 0.0f);
    const Vec2f off = ReticleEdgeStick({1.0f, 0.5f}, true, ReticleEdgeBand{});
    CHECK(off.x == 0.0f);
    CHECK(off.y == 0.0f);
    // Added to the stick: capped at full tilt.
    const Vec2f sum = AddStick({0.8f, 0.0f}, {0.8f, 0.0f});
    CHECK(sum.x == Approx(1.0f));
    const Vec2f small = AddStick({0.2f, 0.1f}, {0.1f, 0.0f});
    CHECK(small.x == Approx(0.3f));
    CHECK(small.y == Approx(0.1f));
}

// AzaharFrame kiu2: C-Stick at Edge's hand-over (ReticleEdgeHandoff, reports/30/kiuadv/plan.md):
// Touch -> Lead -> Stick in the band, Stick -> Return -> Touch out of it past the margin.
namespace {
using HState = ReticleEdgeHandoff::State;
struct EdgeCase {
    Vec2f at;    ///< the point on the edge
    Vec2f in;    ///< a unit step toward the middle
    Vec2f sign;  ///< the push's direction there
    bool on_x;   ///< the edge is across (the band's x)
};
constexpr EdgeCase kEdges[] = {{{1.0f, 0.5f}, {-1.0f, 0.0f}, {1.0f, 0.0f}, true},
                               {{0.0f, 0.5f}, {1.0f, 0.0f}, {-1.0f, 0.0f}, true},
                               {{0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, false},
                               {{0.5f, 1.0f}, {0.0f, -1.0f}, {0.0f, -1.0f}, false}};
Vec2f Into(const EdgeCase& e, float from_edge) {
    return {e.at.x + e.in.x * from_edge, e.at.y + e.in.y * from_edge};
}
} // namespace

TEST_CASE("Reticle: C-Stick at Edge hands the touch to the C-Stick", "[frame_xr][reticle][kiu]") {
    const int64_t lead = ReticleEdgeHandoff::kHandoffLeadNs;
    const int64_t lag = ReticleEdgeHandoff::kHandoffLagNs;
    const float floor = ReticleEdgeHandoff::kEdgeStickFloor;
    const float hys = ReticleEdgeHandoff::kHandoffHysteresis;
    for (const ReticleHand src : {ReticleHand::Left, ReticleHand::Right, ReticleHand::Headset}) {
        for (float speed = kReticleSpeedMin; speed <= kReticleSpeedMax + 0.001f; speed += 0.1f) {
            const ReticleEdgeBand band = ReticleEdgeBandFor(ReticleHalfWidthDeg(src, speed));
            REQUIRE(band.On());
            // The middle, held: the touch, no stick.
            ReticleEdgeHandoff m;
            for (int i = 0; i < 20; i++) {
                const auto o = m.Update({0.5f, 0.5f}, true, band, i * 11 * kMs);
                CHECK(o.touch);
                CHECK(o.stick.x == 0.0f);
                CHECK(o.stick.y == 0.0f);
            }
            for (const EdgeCase& e : kEdges) {
                const float b = e.on_x ? band.x : band.y;
                ReticleEdgeHandoff h;
                int64_t now = 0;
                h.Update({0.5f, 0.5f}, true, band, now);
                // Into the band: the stick at once, at least the floor; the touch for the lead.
                now += 11 * kMs;
                auto o = h.Update(Into(e, b * 0.5f), true, band, now);
                CHECK(h.state() == HState::Lead);
                CHECK(o.touch);
                const float want = floor + (1.0f - floor) * 0.5f;
                CHECK(o.stick.x == Approx(e.sign.x * want).margin(1e-4));
                CHECK(o.stick.y == Approx(e.sign.y * want).margin(1e-4));
                const int64_t entered = now;
                while (now - entered < lead) {
                    now += 11 * kMs;
                    o = h.Update(e.at, true, band, now);
                    CHECK(o.touch == (now - entered < lead));
                }
                CHECK(h.state() == HState::Stick);
                CHECK_FALSE(o.touch);
                CHECK(o.stick.x == Approx(e.sign.x).margin(1e-4)); // on the edge: full tilt
                CHECK(o.stick.y == Approx(e.sign.y).margin(1e-4));
                // Wobbling across the band's inner line, within the margin: no change.
                for (int i = 0; i < 20; i++) {
                    now += 11 * kMs;
                    const float f = (i % 2) ? b * 0.98f : b * (1.0f + hys * 0.8f);
                    o = h.Update(Into(e, f), true, band, now);
                    CHECK_FALSE(o.touch);
                    CHECK(std::fabs(o.stick.x + o.stick.y) >= floor - 1e-4f); // the floor holds
                }
                // Out past the margin: the touch first, the stick a lag later.
                now += 11 * kMs;
                o = h.Update(Into(e, b * (1.0f + hys * 1.5f)), true, band, now);
                CHECK(h.state() == HState::Return);
                CHECK(o.touch);
                CHECK(std::fabs(o.stick.x + o.stick.y) == Approx(floor).margin(1e-4));
                const int64_t left = now;
                while (h.state() == HState::Return) {
                    now += 11 * kMs;
                    o = h.Update({0.5f, 0.5f}, true, band, now);
                    CHECK(o.touch);
                }
                CHECK(now - left >= lag);
                CHECK(o.stick.x == 0.0f);
                CHECK(o.stick.y == 0.0f);
                // Let go in Stick: no stick and the touch allowed in the same call; band off too.
                for (int how = 0; how < 2; how++) {
                    ReticleEdgeHandoff g;
                    g.Update(e.at, true, band, 0);
                    g.Update(e.at, true, band, lead);
                    REQUIRE(g.state() == HState::Stick);
                    o = how == 0 ? g.Update(e.at, false, band, lead + 11 * kMs)
                                 : g.Update(e.at, true, ReticleEdgeBand{}, lead + 11 * kMs);
                    CHECK(o.touch);
                    CHECK(o.stick.x == 0.0f);
                    CHECK(o.stick.y == 0.0f);
                    CHECK(g.state() == HState::Touch);
                }
                // Out of the band during the lead: back to the touch, never lifted.
                ReticleEdgeHandoff t;
                t.Update(e.at, true, band, 0);
                o = t.Update({0.5f, 0.5f}, true, band, 11 * kMs);
                CHECK(t.state() == HState::Touch);
                CHECK(o.touch);
                CHECK(o.stick.x == 0.0f);
            }
            // Depth 0 to 1 gives the floor to full tilt, rising.
            float last = 0.0f;
            for (int i = 0; i <= 20; i++) {
                ReticleEdgeHandoff h;
                const float d = static_cast<float>(i) / 20.0f;
                const auto o = h.Update({1.0f - band.x + band.x * d + 1e-6f, 0.5f}, true, band, 0);
                CHECK(o.stick.x >= floor - 1e-4f);
                CHECK(o.stick.x >= last - 1e-5f);
                CHECK(o.stick.x <= 1.0f + 1e-5f);
                last = o.stick.x;
            }
            CHECK(last == Approx(1.0f));
            // Corners: both ways, never past full tilt.
            for (const Vec2f c : {Vec2f{0.0f, 0.0f}, Vec2f{1.0f, 0.0f}, Vec2f{0.0f, 1.0f},
                                  Vec2f{1.0f, 1.0f}}) {
                ReticleEdgeHandoff h;
                const auto o = h.Update(c, true, band, 0);
                CHECK(std::sqrt(o.stick.x * o.stick.x + o.stick.y * o.stick.y) <= 1.0f + 1e-5f);
                CHECK(o.stick.x != 0.0f);
                CHECK(o.stick.y != 0.0f);
            }
        }
    }
    // A NaN point: no stick, the touch allowed.
    ReticleEdgeHandoff n;
    const ReticleEdgeBand band = ReticleEdgeBandFor(ReticleHalfWidthDeg(ReticleHand::Right, 2.0f));
    const auto o = n.Update({std::nanf(""), 0.5f}, true, band, 0);
    CHECK(o.touch);
    CHECK(o.stick.x == 0.0f);
    CHECK(o.stick.y == 0.0f);
}

// AzaharFrame kiu2: through TouchpadTouch: a drag into the band gives Pressed, Moved..., one
// Released after the lead; back out, one Pressed where the point is (no Moved first). With the
// band off the events are as before; another source's touch gets no stick.
TEST_CASE("Reticle: C-Stick at Edge through the touch pipeline", "[frame_xr][reticle][kiu]") {
    const ReticleEdgeBand band = ReticleEdgeBandFor(ReticleHalfWidthDeg(ReticleHand::Right, 2.0f));
    const auto run = [&](const ReticleEdgeBand& b, std::vector<TouchEvent>& all,
                         std::vector<Vec2f>* sticks) {
        TouchpadTouch t;
        TouchpadFrame fr;
        fr.bottom_visible = true;
        fr.time_ns = 1'000 * kMs;
        const auto step = [&](Vec2f p, bool down) {
            fr.time_ns += 11 * kMs;
            fr.reticle = {true, p, down, b};
            const TouchpadResult r = t.Update(fr);
            all.insert(all.end(), r.events.begin(), r.events.begin() + r.event_count);
            if (sticks) {
                sticks->push_back(r.edge_stick);
            }
        };
        step({0.5f, 0.5f}, false);
        step({0.5f, 0.5f}, true);
        for (int i = 1; i <= 25; i++) {
            step({0.5f + 0.02f * static_cast<float>(i), 0.5f}, true); // to the edge, then held
        }
        for (int i = 0; i < 20; i++) {
            step({1.0f, 0.5f}, true);
        }
        all.push_back({TouchEventType::Moved, -1.0f, -1.0f}); // a marker: back out from here
        step({0.6f, 0.5f}, true);
        for (int i = 0; i < 10; i++) {
            step({0.6f - 0.01f * static_cast<float>(i), 0.5f}, true);
        }
        step({0.5f, 0.5f}, false);
    };
    std::vector<TouchEvent> with, without;
    std::vector<Vec2f> sticks;
    run(band, with, &sticks);
    run(ReticleEdgeBand{}, without, nullptr);
    // Without the band: one press, moves, one release at the end (as before kiu2).
    int presses = 0, releases = 0;
    for (const TouchEvent& e : without) {
        presses += e.type == TouchEventType::Pressed;
        releases += e.type == TouchEventType::Released;
    }
    CHECK(presses == 1);
    CHECK(releases == 1);
    // With it: Pressed, Moved..., Released (in the band), then after the marker Pressed at the
    // point, Moved..., Released.
    std::vector<TouchEventType> kinds;
    std::size_t marker = 0;
    for (std::size_t i = 0; i < with.size(); i++) {
        if (with[i].x < 0.0f) {
            marker = i;
            continue;
        }
        if (kinds.empty() || kinds.back() != with[i].type || with[i].type != TouchEventType::Moved) {
            kinds.push_back(with[i].type);
        }
    }
    const std::vector<TouchEventType> want = {
        TouchEventType::Pressed, TouchEventType::Moved,    TouchEventType::Released,
        TouchEventType::Pressed, TouchEventType::Moved,    TouchEventType::Released};
    CHECK(kinds == want);
    REQUIRE(marker + 1 < with.size());
    CHECK(with[marker + 1].type == TouchEventType::Pressed); // no Moved jump before it
    CHECK(with[marker + 1].x == Approx(0.6f * 320.0f).margin(1.0f));
    // The release came while the point was in the band, after the stick started.
    bool pushed = false;
    for (const Vec2f& s : sticks) {
        pushed = pushed || s.x >= ReticleEdgeHandoff::kEdgeStickFloor;
    }
    CHECK(pushed);
    CHECK(sticks.back().x == 0.0f);
    // Another source (the mouse) holds the touch: the reticle at the edge gets no stick.
    TouchpadTouch t;
    TouchpadFrame fr;
    fr.mode = TouchpadMode::Touch;
    fr.bottom_visible = true;
    fr.time_ns = 1'000 * kMs;
    fr.mouse = {true, false, 0.5f, 0.5f};
    t.Update(fr);
    fr.time_ns += 11 * kMs;
    fr.mouse.down = true;
    t.Update(fr);
    REQUIRE(t.owner() == TouchSource::Mouse);
    for (int i = 0; i < 10; i++) {
        fr.time_ns += 11 * kMs;
        fr.reticle = {true, {1.0f, 0.5f}, false, band};
        const TouchpadResult r = t.Update(fr);
        CHECK(r.edge_stick.x == 0.0f);
    }
}

// AzaharFrame kiu2: as controls.cpp runs it with the switch on: Follow whatever At Edge says, so
// the point stays on the edge (Snap to Middle never takes it out of the band), and the C-Stick
// pushes there for as long as it is held.
TEST_CASE("Reticle: C-Stick at Edge holds the edge with Snap to Middle stored",
          "[frame_xr][reticle][kiu]") {
    for (const bool center : {false, true}) {
        Rig t;
        t.edge = ReticleEdge::Follow; // what controls.cpp passes while the switch is on
        t.half = ReticleHalfWidthDeg(ReticleHand::Right, 2.0f);
        t.modes.center_until_pressed = center;
        const ReticleEdgeBand band = ReticleEdgeBandFor(t.half);
        ReticleEdgeHandoff h;
        t.Step(Aim(0.0f, 0.0f));
        float yaw = 0.0f;
        int lifted = 0;
        ReticleEdgeHandoff::Out o;
        for (int i = 0; i < 200; i++) {
            yaw = std::max(yaw - 1.0f, -t.half - 8.0f);
            const ReticleFrame& f = t.Step(Aim(yaw, 0.0f, 1.0f));
            CHECK_FALSE(f.snapped);
            o = h.Update(f.point, f.down, band, t.now);
            lifted += o.touch ? 0 : 1;
        }
        CHECK(lifted > 150);
        CHECK(o.stick.x > 0.95f); // the dead zone holds the point a hair short of the edge
    }
}
