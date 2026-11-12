// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

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
constexpr int kNormal = 2;

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
    ReticleFrame& Step(const HandInput& h, int frames = 1, int speed = kNormal) {
        for (int i = 0; i < frames; i++) {
            now += 11 * kMs;
            f = r.Update(h, true, speed, now);
        }
        return f;
    }
};

} // namespace

TEST_CASE("Reticle: angle to point", "[frame_xr]") {
    CHECK(ReticleHalfWidthDeg(kNormal) == Approx(15.0f));
    CHECK(ReticleHalfWidthDeg(4) == Approx(7.5f));  // faster: a narrower cone
    CHECK(ReticleHalfWidthDeg(0) == Approx(30.0f)); // slower: a wider one
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
    // 25 degrees left: on the left edge, the reference dragged 10 degrees along.
    f = t.Step(Aim(55.0f, 10.0f), 90);
    CHECK(f.point.x == Approx(0.0f).margin(0.01));
    CHECK(t.r.ref_yaw_deg() == Approx(40.0f).margin(0.2));
    // Turning back moves the point at once: 15 degrees left is now 5 from the reference.
    f = t.Step(Aim(45.0f, 10.0f), 90);
    CHECK(f.point.x == Approx(0.5f - 5.0f / 30.0f).margin(0.01));
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
    CHECK_FALSE(t.r.Update(Aim(-20.0f, 0.0f), false, kNormal, t.now).on);

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
