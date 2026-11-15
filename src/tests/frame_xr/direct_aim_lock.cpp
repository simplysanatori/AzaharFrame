// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuaim3: Direct Aim's Turn Zone From (Screen Edge, Aim Box), Keep Reticle on Screen
// (the edge point, rays pointing away, the resting guard) and Grip Frees Aim (Hold, Toggle), and
// the mapper leaving a locked controller's beam out of the bottom screen.

#include <cmath>
#include <optional>
#include <string>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/gyro_aim.h"
#include "frame_xr/input/mapper.h"
#include "tests/frame_xr/input_test_util.h"

using Catch::Approx;
using namespace FrameXr::Input;

namespace {

constexpr float kDeg = 3.14159265358979f / 180.0f;

/// The top screen 2 m wide (5:3, 1.2 m high), 2 m ahead, centred at eye height 1.2 m.
PanelQuad Top() {
    PanelQuad q;
    q.visible = true;
    q.width_m = 2.0f;
    q.height_m = 1.2f;
    q.pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
    q.pose.position = {0.0f, 1.2f, -2.0f};
    return q;
}

/// The bottom screen below it and nearer.
PanelQuad Bottom() {
    PanelQuad q = Top();
    q.width_m = 1.2f;
    q.height_m = 0.9f;
    q.pose.position = {0.0f, 0.2f, -1.8f};
    return q;
}

constexpr Vec3f kEye{0.0f, 1.2f, 0.0f};

/// A ray from the eye, `yaw` degrees right and `pitch` degrees up of straight ahead.
Ray RayAt(float yaw, float pitch, Vec3f origin = kEye) {
    Ray r;
    r.origin = origin;
    r.direction = {std::sin(yaw * kDeg) * std::cos(pitch * kDeg), std::sin(pitch * kDeg),
                   -std::cos(yaw * kDeg) * std::cos(pitch * kDeg)};
    return r;
}

Ray RayTo(Vec3f point, Vec3f origin = kEye) {
    return {origin, Normalized(point - origin)};
}

/// The ground reading of direct_aim.cpp: box x 140 to 260, y 32 to 112 (cy 80).
ReticleReading Ground() {
    ReticleReading r;
    r.valid = true;
    r.w = 400.0f;
    r.h = 240.0f;
    r.cx = 200.0f;
    r.cy = 80.0f;
    r.x = 200.0f;
    r.y = 80.0f;
    r.ground = true;
    r.box_top = -0.6f;
    r.box_bottom = 0.4f;
    return r;
}

LightGunTarget At(float u, float v) {
    LightGunTarget t;
    t.valid = true;
    t.u = u;
    t.v = v;
    return t;
}

} // namespace

// ---- Turn Zone From ----------------------------------------------------------------------------

TEST_CASE("Turn Zone From: Aim Box measures from the box's edge outward", "[frame_xr][kiuaim3]") {
    const ReticleReading r = Ground();
    const ReticleBox box = GroundBox(r);
    REQUIRE(box.x0 == Approx(140.0f));
    REQUIRE(box.x1 == Approx(260.0f));
    REQUIRE(box.y0 == Approx(32.0f));
    REQUIRE(box.y1 == Approx(112.0f));
    // 10 %: 40 px across, 24 px up and down. 0 at the edge, half way, full, and beyond.
    CHECK(AimBoxTurnDepth(r, {200.0f, 80.0f}, 0.1f).x == 0.0f);
    CHECK(AimBoxTurnDepth(r, {260.0f, 80.0f}, 0.1f).x == 0.0f);
    CHECK(AimBoxTurnDepth(r, {280.0f, 80.0f}, 0.1f).x == Approx(0.5f));
    CHECK(AimBoxTurnDepth(r, {300.0f, 80.0f}, 0.1f).x == Approx(1.0f));
    CHECK(AimBoxTurnDepth(r, {390.0f, 80.0f}, 0.1f).x == Approx(1.0f));
    CHECK(AimBoxTurnDepth(r, {120.0f, 80.0f}, 0.1f).x == Approx(-0.5f));
    CHECK(AimBoxTurnDepth(r, {200.0f, 124.0f}, 0.1f).y == Approx(0.5f));
    CHECK(AimBoxTurnDepth(r, {200.0f, 20.0f}, 0.1f).y == Approx(-0.5f));
    CHECK(AimBoxTurnDepth(r, {280.0f, 80.0f}, 0.0f).x == 0.0f); // 0 %: off
    // No dead band: just past the box it already turns, a little; deeper is more.
    float last = 0.0f;
    for (float x = 261.0f; x <= 300.0f; x += 1.0f) {
        const float d = AimBoxTurnDepth(r, {x, 80.0f}, 0.1f).x;
        CHECK(d > last);
        last = d;
    }
}

TEST_CASE("Turn Zone From: Screen Edge keeps its dead band, Aim Box has none",
          "[frame_xr][kiuaim3]") {
    const ReticleReading r = Ground();
    const LightGunTarget past_box = At(0.7f, 0.3f); // 280 px: past the box, far from the edge
    CHECK(TurnDepthFor(r, past_box, 0.1f, TurnZoneFrom::ScreenEdge).x == 0.0f);
    CHECK(TurnDepthFor(r, past_box, 0.1f, TurnZoneFrom::AimBox).x == Approx(0.5f));
    CHECK(TurnDepthFor(r, At(0.95f, 0.3f), 0.1f, TurnZoneFrom::ScreenEdge).x == Approx(0.5f));
    // The air: neither turns.
    ReticleReading air = r;
    air.ground = false;
    CHECK(TurnDepthFor(air, past_box, 0.1f, TurnZoneFrom::AimBox).x == 0.0f);
    CHECK(TurnDepthFor(air, At(1.0f, 0.3f), 0.1f, TurnZoneFrom::ScreenEdge).x == 0.0f);

    DirectAim aim;
    const DirectAim::Out out =
        aim.Update(1, r, past_box, DirectAim::Turn{0.1f, TurnZoneFrom::AimBox, 1.0f}, false);
    CHECK(out.write);
    CHECK(out.x == Approx(259.5f)); // at the box's edge, as the C-Stick leaves it
    CHECK(out.turning);
    CHECK(out.right_dps == Approx(0.5f * DirectAim::kTurnPushDps));
    DirectAim edge_basis;
    CHECK_FALSE(edge_basis
                    .Update(1, r, past_box, DirectAim::Turn{0.1f, TurnZoneFrom::ScreenEdge, 1.0f},
                            false)
                    .turning);
    // The older overload is Screen Edge.
    DirectAim old;
    CHECK_FALSE(old.Update(1, r, past_box, 0.1f, 1.0f, false).turning);
    CHECK(TurnZoneFromValue(0) == TurnZoneFrom::ScreenEdge);
    CHECK(TurnZoneFromValue(1) == TurnZoneFrom::AimBox);
    CHECK(TurnZoneFromValue(7) == TurnZoneFrom::AimBox);
    CHECK(kTurnZoneFromDefault == 1);
}

// ---- Keep Reticle on Screen --------------------------------------------------------------------

TEST_CASE("Keep Reticle on Screen: on the screen nothing changes, off it the nearest edge",
          "[frame_xr][kiuaim3]") {
    const PanelQuad top = Top();
    // On the screen: the same hit as without it.
    const Ray on = RayTo({0.5f, 1.5f, -2.0f});
    const LightGunTarget a = LockedTargetFor(on, top);
    const LightGunTarget b = LightGunTargetFor(on, top);
    REQUIRE(a.valid);
    CHECK_FALSE(a.edge);
    CHECK(a.u == Approx(b.u));
    CHECK(a.v == Approx(b.v));
    // Right of the screen, level with its middle: the right edge's middle.
    const LightGunTarget right = LockedTargetFor(RayTo({1.5f, 1.2f, -2.0f}), top);
    REQUIRE(right.valid);
    CHECK(right.edge);
    CHECK(right.u == Approx(1.0f));
    CHECK(right.v == Approx(0.5f));
    CHECK(right.past_u == 1);
    CHECK(right.past_v == 0);
    CHECK(right.point.x == Approx(1.0f));
    CHECK(right.past_deg == Approx(std::atan(0.75f) / kDeg - std::atan(0.5f) / kDeg).margin(0.1f));
    CHECK_FALSE(right.resting);
    // Above and left: the corner.
    const LightGunTarget corner = LockedTargetFor(RayTo({-1.6f, 2.4f, -2.0f}), top);
    CHECK(corner.u == 0.0f);
    CHECK(corner.v == 0.0f);
    CHECK(corner.past_u == -1);
    CHECK(corner.past_v == -1);
    // Without it the same ray is a target half a screen out, or none.
    CHECK(LightGunTargetFor(RayTo({1.5f, 1.2f, -2.0f}), top).u == Approx(1.25f));
    CHECK(LightGunTargetFor(RayTo({9.0f, 1.2f, -2.0f}), top).u == Approx(1.5f));
    // Not shown: nothing.
    PanelQuad hidden = top;
    hidden.visible = false;
    CHECK_FALSE(LockedTargetFor(on, hidden).valid);
    // The laser does not draw to an edge point.
    CHECK_FALSE(LaserSight(GyroAimMode::LightGun, true, right));
    CHECK(LaserSight(GyroAimMode::LightGun, true, a));
}

TEST_CASE("Keep Reticle on Screen: the bottom screen keeps the reticle at the edge",
          "[frame_xr][kiuaim3]") {
    const PanelQuad top = Top(), bottom = Bottom();
    const Ray on_bottom = RayTo({0.2f, 0.2f, -1.8f});
    REQUIRE(IntersectQuad(on_bottom, bottom).in_bounds);
    CHECK_FALSE(LightGunTargetFor(on_bottom, top, &bottom).valid); // free: a touch there
    const LightGunTarget locked = LockedTargetFor(on_bottom, top);
    REQUIRE(locked.valid);
    CHECK(locked.edge);
    CHECK(locked.v == 1.0f);
    CHECK(locked.past_v == 1);
    CHECK_FALSE(locked.resting); // just below the top screen: it turns down
}

TEST_CASE("Keep Reticle on Screen: rays pointing away clamp by their direction",
          "[frame_xr][kiuaim3]") {
    const PanelQuad top = Top();
    // Straight back: the bottom edge's middle (the arm lowered behind), resting.
    const LightGunTarget back = LockedTargetFor({kEye, {0.0f, 0.0f, 1.0f}}, top);
    REQUIRE(back.valid);
    CHECK(back.edge);
    CHECK(back.u == Approx(0.5f));
    CHECK(back.v == 1.0f);
    CHECK(back.resting);
    // Back and to the left: the left edge.
    const LightGunTarget back_left = LockedTargetFor({kEye, Normalized(Vec3f{-1.0f, 0.0f, 1.0f})}, top);
    CHECK(back_left.u == 0.0f);
    CHECK(back_left.v == Approx(0.5f));
    CHECK(back_left.past_u == -1);
    CHECK(back_left.resting);
    // Straight up (along the screen's plane): the top edge, resting.
    const LightGunTarget up = LockedTargetFor({kEye, {0.0f, 1.0f, 0.0f}}, top);
    CHECK(up.u == Approx(0.5f));
    CHECK(up.v == 0.0f);
    CHECK(up.past_v == -1);
    CHECK(up.past_deg == Approx(90.0f - std::atan(0.6f / 2.0f) / kDeg).margin(0.1f));
    CHECK(up.resting);
    // Right and slightly back: the right edge.
    const LightGunTarget right = LockedTargetFor({kEye, Normalized(Vec3f{1.0f, 0.1f, 0.2f})}, top);
    CHECK(right.u == 1.0f);
    CHECK(right.past_u == 1);
    // Everything finite.
    for (const LightGunTarget& t : {back, back_left, up, right}) {
        CHECK(std::isfinite(t.u));
        CHECK(std::isfinite(t.v));
        CHECK(std::isfinite(t.past_deg));
    }
}

TEST_CASE("Keep Reticle on Screen: off the screen it turns all the way, resting it does not",
          "[frame_xr][kiuaim3]") {
    const PanelQuad top = Top();
    const ReticleReading r = Ground();
    const float edge_yaw = std::atan(0.5f) / kDeg; // the right edge's middle, from the eye
    const auto run = [&](float yaw, float pitch, TurnZoneFrom from) {
        DirectAim aim;
        return aim.Update(1, r, LockedTargetFor(RayAt(yaw, pitch), top),
                          DirectAim::Turn{0.1f, from, 1.0f}, false);
    };
    for (const TurnZoneFrom from : {TurnZoneFrom::AimBox, TurnZoneFrom::ScreenEdge}) {
        // 20 degrees past the right edge: the box's edge, full speed right.
        const DirectAim::Out past = run(edge_yaw + 20.0f, 0.0f, from);
        CHECK(past.write);
        CHECK(past.x == Approx(259.5f));
        CHECK(past.turning);
        CHECK(past.right_dps == Approx(DirectAim::kTurnPushDps));
        CHECK_FALSE(past.resting);
        // 50 degrees past a side edge: still a turn (1.2.0 RC2: the reticle at an edge turns,
        // however far past; a cannon's barrel points far past the side).
        const DirectAim::Out far = run(edge_yaw + 50.0f, 0.0f, from);
        CHECK(far.write);
        CHECK(far.x == Approx(259.5f));
        CHECK(far.turning);
        CHECK(far.right_dps == Approx(DirectAim::kTurnPushDps));
        CHECK_FALSE(far.resting);
    }
    // The threshold.
    CHECK(kRestingPastDeg == 45.0f);
    // Past the bottom edge (the arm lowered) only.
    const float bottom_pitch = -std::atan(0.3f) / kDeg; // the bottom edge's middle, from the eye
    CHECK_FALSE(LockedTargetFor(RayAt(0.0f, bottom_pitch - 30.0f), top).resting); // aiming low
    CHECK_FALSE(LockedTargetFor(RayAt(0.0f, bottom_pitch - (kRestingPastDeg - 0.5f)), top).resting);
    CHECK(LockedTargetFor(RayAt(0.0f, bottom_pitch - (kRestingPastDeg + 0.5f)), top).resting);
    CHECK_FALSE(LockedTargetFor(RayAt(edge_yaw + 50.0f, 0.0f), top).resting);
    CHECK_FALSE(LockedTargetFor(RayAt(-edge_yaw - 60.0f, 0.0f), top).resting);
    CHECK(LockedTargetFor(RayAt(edge_yaw + 20.0f, 0.0f), top).past_deg ==
          Approx(20.0f).margin(0.05f));
    // The arm lowered to the side: resting, the reticle at the bottom of the box.
    const DirectAim::Out lowered = run(10.0f, -80.0f, TurnZoneFrom::AimBox);
    CHECK(lowered.write);
    CHECK(lowered.y == Approx(111.5f));
    CHECK(lowered.resting);
    CHECK_FALSE(lowered.turning);
    // In the air: at the screen's edge, never a turn.
    ReticleReading air = r;
    air.ground = false;
    DirectAim aim;
    const DirectAim::Out out =
        aim.Update(1, air, LockedTargetFor(RayAt(edge_yaw + 10.0f, 0.0f), top),
                   DirectAim::Turn{0.1f, TurnZoneFrom::AimBox, 1.0f}, false);
    CHECK(out.x == Approx(399.5f));
    CHECK_FALSE(out.turning);
}

TEST_CASE("Keep Reticle on Screen: the controller's roll changes nothing", "[frame_xr][kiuaim3]") {
    const PanelQuad top = Top();
    const Vec3f hand{0.2f, 1.0f, -0.3f};
    const LightGunTarget base =
        LockedTargetFor(AimRay(Posef{FaceViewer({3.0f, 1.0f, -2.0f}, hand, {0, 1, 0}), hand},
                               std::nullopt),
                        top);
    REQUIRE(base.edge);
    for (const float roll : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
        // Rolled about its own pointing axis (-Z).
        Posef pose{FaceViewer({3.0f, 1.0f, -2.0f}, hand, {0, 1, 0}), hand};
        const float h = roll * kDeg * 0.5f;
        pose.orientation = Normalized(
            Multiply(pose.orientation, Quatf{0.0f, 0.0f, -std::sin(h), std::cos(h)}));
        const LightGunTarget t = LockedTargetFor(AimRay(pose, std::nullopt), top);
        CHECK(t.u == Approx(base.u).margin(1e-4f));
        CHECK(t.v == Approx(base.v).margin(1e-4f));
        CHECK(t.past_deg == Approx(base.past_deg).margin(0.01f));
        CHECK(t.resting == base.resting);
    }
}

TEST_CASE("Keep Reticle on Screen: the sampler does not carry an edge point along",
          "[frame_xr][kiuaim3]") {
    AimSampler s;
    LightGunTarget a = At(0.9f, 0.5f);
    LightGunTarget b = At(1.0f, 0.5f);
    b.edge = true;
    b.past_u = 1;
    s.Push(0, a);
    s.Push(8'000'000, b);
    const LightGunTarget later = s.At(14'000'000);
    CHECK(later.u == 1.0f);
    CHECK(later.edge);
}

// ---- Grip Frees Aim ----------------------------------------------------------------------------

TEST_CASE("Grip Frees Aim: Hold frees while held", "[frame_xr][kiuaim3]") {
    AimGrip g;
    CHECK_FALSE(g.Update(0.0f, false, AimGripMode::Hold, false).free);
    CHECK(g.Update(0.7f, false, AimGripMode::Hold, false).free);
    CHECK(g.Update(0.5f, false, AimGripMode::Hold, false).free); // the hysteresis
    const AimGrip::Out let_go = g.Update(0.3f, false, AimGripMode::Hold, false);
    CHECK_FALSE(let_go.free);
    CHECK_FALSE(let_go.toggled); // no toasts in Hold
    // Both grips: free too (the immersive screen's shape is the other grip's business).
    CHECK(g.Update(0.9f, true, AimGripMode::Hold, false).free);
    CHECK(AimGripModeFrom(0) == AimGripMode::Hold);
    CHECK(AimGripModeFrom(1) == AimGripMode::Toggle);
    CHECK(AimGripModeFrom(5) == AimGripMode::Hold);
}

TEST_CASE("Grip Frees Aim: Toggle frees on one press, locks on the next", "[frame_xr][kiuaim3]") {
    AimGrip g;
    const auto press = [&](bool other = false, bool suspend = false) {
        g.Update(0.9f, other, AimGripMode::Toggle, suspend);
        return g.Update(0.0f, false, AimGripMode::Toggle, suspend);
    };
    CHECK_FALSE(g.Update(0.0f, false, AimGripMode::Toggle, false).free);
    // Held: nothing yet (it toggles when let go).
    const AimGrip::Out held = g.Update(0.9f, false, AimGripMode::Toggle, false);
    CHECK_FALSE(held.free);
    CHECK_FALSE(held.toggled);
    const AimGrip::Out freed = g.Update(0.0f, false, AimGripMode::Toggle, false);
    CHECK(freed.free);
    CHECK(freed.toggled);
    CHECK_FALSE(g.Update(0.0f, false, AimGripMode::Toggle, false).toggled); // once
    const AimGrip::Out locked = press();
    CHECK_FALSE(locked.free);
    CHECK(locked.toggled);
    // With the other grip down (both grips: the immersive screen's shape): no toggle.
    CHECK_FALSE(press(true).free);
    // The other grip joining half way through: no toggle either.
    g.Update(0.9f, false, AimGripMode::Toggle, false);
    g.Update(0.9f, true, AimGripMode::Toggle, false);
    CHECK_FALSE(g.Update(0.0f, false, AimGripMode::Toggle, false).free);
    // Check Aim: locked, presses ignored.
    CHECK(press().free);
    const AimGrip::Out checking = g.Update(0.0f, false, AimGripMode::Toggle, true);
    CHECK_FALSE(checking.free);
    CHECK(checking.toggled);
    CHECK_FALSE(press(false, true).free);
    // Back to Hold while free: locked.
    CHECK(press().free);
    CHECK_FALSE(g.Update(0.0f, false, AimGripMode::Hold, false).free);
    CHECK(std::string(kAimUnlockedToast) == "Aim Unlocked");
    CHECK(std::string(kAimLockedToast) == "Aim Locked");
}

// ---- The mapper ------------------------------------------------------------------------------

TEST_CASE("Keep Reticle on Screen: a locked controller does not touch the bottom screen",
          "[frame_xr][kiuaim3]") {
    using namespace FrameXr::Input::TestUtil;
    const PanelGeometry panels = StandardPanels();
    const Vec3f on_bottom{0.0f, -0.8f, -1.5f};
    for (const bool locked : {false, true}) {
        Mapper mapper{MapperConfig{}};
        InputFrame frame = IdleFrame();
        Point(frame, Hand::Right, on_bottom);
        frame.hand[1].aim_locked = locked;
        frame.time_ns = 0;
        MapperOutput out = mapper.Update(frame, panels);
        CHECK(out.cursor.visible == !locked);
        frame.hand[1].trigger = 1.0f;
        frame.time_ns = kFrameNs;
        out = mapper.Update(frame, panels);
        if (locked) {
            CHECK(out.touch_event_count == 0);
            CHECK(out.controls.Get(Control::TriggerR)); // fires
        } else {
            REQUIRE(out.touch_event_count == 1);
            CHECK(out.touch_events[0].type == TouchEventType::Pressed);
            CHECK_FALSE(out.controls.Get(Control::TriggerR));
        }
    }
}

// AzaharFrame fixc: Grip Frees Aim and Keep Reticle on Screen are the aiming controller's alone
// (owner, 2026-11-15): the other controller points, touches the Secondary Screen, grabs and fires
// with no grip held, whichever hand aims (Motion Controls: right, or left for the left-handed).
TEST_CASE("Aim lock: only the aiming controller's grip is taken and only it is locked",
          "[frame_xr][fixc]") {
    using namespace FrameXr::Input::TestUtil;
    for (const int aim : {0, 1}) {
        InputFrame frame = IdleFrame();
        frame.hand[0].grip = 0.9f;
        frame.hand[1].grip = 0.7f;
        const float taken = TakeAimGrip(frame, aim);
        CHECK(taken == (aim == 0 ? 0.9f : 0.7f));
        CHECK(frame.hand[static_cast<std::size_t>(aim)].grip == 0.0f);
        CHECK(frame.hand[static_cast<std::size_t>(1 - aim)].grip == (aim == 0 ? 0.7f : 0.9f));
        LockAim(frame, aim, true);
        CHECK(frame.hand[static_cast<std::size_t>(aim)].aim_locked);
        CHECK_FALSE(frame.hand[static_cast<std::size_t>(1 - aim)].aim_locked);
        LockAim(frame, aim, false); // freed by its grip, or Keep Reticle on Screen off
        CHECK_FALSE(frame.hand[0].aim_locked);
        CHECK_FALSE(frame.hand[1].aim_locked);        // 1.2.0 RC2: the other controller's grip held (moving a screen): read, but left.
        InputFrame both = IdleFrame();
        both.hand[0].grip = 0.9f;
        both.hand[1].grip = 0.7f;
        CHECK(TakeAimGrip(both, aim, true) == (aim == 0 ? 0.9f : 0.7f));
        CHECK(both.hand[0].grip == 0.9f);
        CHECK(both.hand[1].grip == 0.7f);
    }
    // Headset: no controller aims, nothing is taken.
    InputFrame frame = IdleFrame();
    frame.hand[0].grip = frame.hand[1].grip = 0.8f;
    CHECK(TakeAimGrip(frame, -1) == 0.0f);
    LockAim(frame, -1, true);
    for (const HandInput& h : frame.hand) {
        CHECK(h.grip == 0.8f);
        CHECK_FALSE(h.aim_locked);
    }
}

TEST_CASE("Aim lock: the other controller points and touches while the aiming one fires",
          "[frame_xr][fixc]") {
    using namespace FrameXr::Input::TestUtil;
    const PanelGeometry panels = StandardPanels();
    const Vec3f on_bottom{0.0f, -0.8f, -1.5f};
    const Vec3f on_top{0.0f, 0.0f, -2.0f};
    for (const Hand aim : {Hand::Right, Hand::Left}) {
        const Hand other = OtherHand(aim);
        const auto ai = static_cast<std::size_t>(HandIndex(aim));
        const auto oi = static_cast<std::size_t>(HandIndex(other));
        Mapper mapper{MapperConfig{}};
        int64_t t = 0;
        InputFrame frame = IdleFrame();
        LockAim(frame, HandIndex(aim), true);
        // The aiming controller fires, held, at the bottom screen too (it is the hand that
        // pulled last), and the other one hovers the bottom screen: its pointer shows.
        Point(frame, aim, on_bottom);
        Point(frame, other, on_bottom + Vec3f{0.1f, 0.05f, 0.0f});
        frame.hand[ai].trigger = 1.0f;
        frame.time_ns = t;
        MapperOutput out = mapper.Update(frame, panels);
        CHECK(out.touch_event_count == 0);
        CHECK(out.controls.Get(aim == Hand::Left ? Control::TriggerL : Control::TriggerR));
        REQUIRE(out.cursor.visible);
        CHECK(out.cursor.hand == other);
        // Its trigger touches, with no grip held.
        frame.hand[oi].trigger = 1.0f;
        frame.time_ns = t += kFrameNs;
        out = mapper.Update(frame, panels);
        REQUIRE(out.touch_event_count == 1);
        CHECK(out.touch_events[0].type == TouchEventType::Pressed);
        CHECK(out.cursor.hand == other);
        CHECK_FALSE(out.controls.Get(other == Hand::Left ? Control::TriggerL : Control::TriggerR));
        frame.hand[oi].trigger = 0.0f;
        frame.time_ns = t += kFrameNs;
        out = mapper.Update(frame, panels);
        REQUIRE(out.touch_event_count == 1);
        CHECK(out.touch_events[0].type == TouchEventType::Released);
        // Over the top screen its trigger is ZL/ZR (it fires too, as before).
        Point(frame, other, on_top);
        frame.hand[oi].trigger = 1.0f;
        frame.time_ns = t += kFrameNs;
        out = mapper.Update(frame, panels);
        CHECK(out.touch_event_count == 0);
        CHECK(out.controls.Get(other == Hand::Left ? Control::TriggerL : Control::TriggerR));
        frame.hand[oi].trigger = 0.0f;
        // Its grip grabs a screen as ever (the aiming grip is taken: TakeAimGrip).
        frame.hand[oi].grip = 1.0f;
        frame.time_ns = t += kFrameNs;
        out = mapper.Update(frame, panels);
        CHECK(out.commands.top_drag_active);
        CHECK(out.commands.drag_hand == HandIndex(other));
    }
}
