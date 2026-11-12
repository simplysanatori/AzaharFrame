// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <random>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/mouse_pointer.h"
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
    ReticleFrame& Step(const HandInput& h, int frames = 1) {
        for (int i = 0; i < frames; i++) {
            now += 11 * kMs;
            f = r.Update(ReticleAimOf(h), true, half, now, edge);
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

TEST_CASE("Reticle: grip and Recenter re-zero, the trigger touches", "[frame_xr]") {
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
