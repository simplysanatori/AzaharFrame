// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuaim2: Direct Aim, absolute (frame_xr/input/gyro_aim.h): the one ray, its pixel on
// the top screen, the reticle written into the game, the pauses and the Turn Zone, the samples
// between the XR thread and the emulation thread, and Check Aim.

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/gyro_aim.h"
#include "frame_xr/vis/beam.h"

using Catch::Approx;
using namespace FrameXr::Input;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

Quatf AxisAngle(Vec3f axis, float degrees) {
    const Vec3f n = Normalized(axis);
    const float half = degrees * kDeg * 0.5f;
    const float s = std::sin(half);
    return Quatf{n.x * s, n.y * s, n.z * s, std::cos(half)};
}

/// An orientation whose -Z is `dir`, rolled `roll` degrees about it.
Quatf Pointing(Vec3f dir, float roll) {
    const Vec3f d = Normalized(dir);
    const float yaw = std::atan2(-d.x, -d.z) / kDeg;
    const float pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f)) / kDeg;
    return Multiply(Multiply(AxisAngle({0, 1, 0}, yaw), AxisAngle({1, 0, 0}, pitch)),
                    AxisAngle({0, 0, -1}, roll));
}

/// The top screen as drawn: `width` metres wide, `aspect` (w/h), `distance` ahead, centred at
/// eye height 1.2 m; `curve` > 0 bends it.
PanelQuad Screen(float width, float aspect = 5.0f / 3.0f, float distance = 2.0f,
                 float curve = 0.0f) {
    PanelQuad q;
    q.visible = true;
    q.width_m = width;
    q.height_m = width / aspect;
    q.pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
    q.pose.position = {0.0f, 1.2f, -distance};
    q.curve_radius = curve;
    return q;
}

ReticleReading Reading(bool ground) {
    ReticleReading r;
    r.valid = true;
    r.w = 400.0f;
    r.h = 240.0f;
    r.cx = 200.0f;
    r.cy = 80.0f; // the game's rest point is above the middle (the Frame mock: 200, ~99)
    r.x = 200.0f;
    r.y = 80.0f;
    r.ground = ground;
    r.box_top = -0.6f;
    r.box_bottom = 0.4f;
    return r;
}

/// The pixel the ray from `origin` through the screen's point (u, v) gives.
Vec2f PixelVia(const Ray& ray, const PanelQuad& top) {
    const LightGunTarget t = LightGunTargetFor(ray, top);
    REQUIRE(t.valid);
    return ReticlePixel(Reading(false), t.u, t.v);
}

} // namespace

// ---- The ray and its pixel -------------------------------------------------------------------

TEST_CASE("Direct Aim: the ray's pixel at the corners, centre and edges", "[frame_xr][kiuaim2]") {
    // World Scale changes the screen's size and distance together (the angles stay); the screen
    // shape (5:3, or 16:9 and 21:9 for the mod's wide builds) stretches the picture, its pixels
    // with it. Every one maps the same share of the screen to the same game pixel.
    struct Case {
        float width, aspect, distance, curve;
    };
    const std::vector<Case> cases{
        {1.6f, 5.0f / 3.0f, 2.0f, 0.0f},  {0.8f, 5.0f / 3.0f, 1.0f, 0.0f},
        {3.2f, 5.0f / 3.0f, 4.0f, 0.0f},  {1.6f, 16.0f / 9.0f, 2.0f, 0.0f},
        {2.1f, 21.0f / 9.0f, 2.0f, 0.0f}, {1.6f, 5.0f / 3.0f, 2.0f, 1.5f},
    };
    const std::vector<Vec2f> points{{0.0f, 0.0f},  {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f},
                                    {0.5f, 0.5f},  {0.5f, 0.0f}, {0.5f, 1.0f}, {0.0f, 0.5f},
                                    {1.0f, 0.5f},  {0.25f, 0.75f}};
    const Vec3f eye{0.15f, 1.0f, 0.1f}; // the hand: below and beside the screen's middle
    for (const Case& c : cases) {
        const PanelQuad top = Screen(c.width, c.aspect, c.distance, c.curve);
        for (const Vec2f& p : points) {
            const Vec3f at = PointOnQuad(top, p.x, p.y);
            const Ray ray{eye, Normalized(at - eye)};
            const Vec2f px = PixelVia(ray, top);
            CHECK(px.x == Approx(p.x * 400.0f).margin(0.05f));
            CHECK(px.y == Approx(p.y * 240.0f).margin(0.05f));
            // The laser ends where the reticle goes.
            const LightGunTarget t = LightGunTargetFor(ray, top);
            CHECK(Length(t.point - at) < 1e-4f);
        }
    }
}

TEST_CASE("Direct Aim: yaw and pitch sweeps follow the screen's own geometry",
          "[frame_xr][kiuaim2]") {
    const PanelQuad top = Screen(1.6f);
    const Vec3f eye{0.0f, 1.2f, 0.0f};
    float last = -1.0f;
    for (float yaw = -20.0f; yaw <= 20.0f; yaw += 2.5f) {
        HandInput h;
        h.active = true;
        h.aim.position = eye;
        h.aim.orientation = AxisAngle({0, 1, 0}, -yaw);
        const LightGunTarget t = LightGunTargetFor(h, top);
        REQUIRE(t.valid);
        const float x = ReticlePixel(Reading(false), t.u, t.v).x;
        const float want = (0.5f + 2.0f * std::tan(yaw * kDeg) / 1.6f) * 400.0f;
        CHECK(x == Approx(want).margin(0.05f));
        CHECK(t.v == Approx(0.5f).margin(1e-4f));
        CHECK(x > last);
        last = x;
    }
    last = -1.0f;
    for (float pitch = 12.0f; pitch >= -12.0f; pitch -= 2.0f) {
        HandInput h;
        h.active = true;
        h.aim.position = eye;
        h.aim.orientation = AxisAngle({1, 0, 0}, pitch);
        const LightGunTarget t = LightGunTargetFor(h, top);
        REQUIRE(t.valid);
        const float y = ReticlePixel(Reading(false), t.u, t.v).y;
        const float want = (0.5f - 2.0f * std::tan(pitch * kDeg) / 0.96f) * 240.0f;
        CHECK(y == Approx(want).margin(0.05f));
        CHECK(y > last);
        last = y;
    }
}

TEST_CASE("Direct Aim: rolling the controller moves nothing but the beam's own hit",
          "[frame_xr][kiuaim2]") {
    const PanelQuad top = Screen(1.6f);
    const Vec3f hand{0.2f, 1.0f, -0.3f};
    for (const Vec2f& p : std::vector<Vec2f>{{0.5f, 0.5f}, {0.1f, 0.15f}, {0.92f, 0.8f}}) {
        const Vec3f at = PointOnQuad(top, p.x, p.y);
        SECTION("from the controller's aim pose: the same ray at any roll") {
            for (const float roll : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
                const Posef aim{Pointing(at - hand, roll), hand};
                const Vec2f px = PixelVia(AimRay(aim, std::nullopt), top);
                CHECK(px.x == Approx(p.x * 400.0f).margin(1.0f));
                CHECK(px.y == Approx(p.y * 240.0f).margin(1.0f));
            }
        }
        SECTION("from the weapon's muzzle: the beam held on the point gives that pixel") {
            const Vec3f muzzle{0.03f, 0.05f, -0.25f}; // above and beside the hand, ahead
            for (const float roll : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
                // The player keeps the beam on the point: the pose turns until the muzzle's ray
                // meets it (a few steps converge: the muzzle is close to the hand).
                Posef aim{Pointing(at - hand, roll), hand};
                for (int i = 0; i < 8; i++) {
                    const Ray ray = AimRay(aim, muzzle);
                    aim.orientation = Pointing(at - ray.origin, roll);
                }
                const Ray ray = AimRay(aim, muzzle);
                const LightGunTarget t = LightGunTargetFor(ray, top);
                const Vec2f px = ReticlePixel(Reading(false), t.u, t.v);
                CHECK(px.x == Approx(p.x * 400.0f).margin(1.0f));
                CHECK(px.y == Approx(p.y * 240.0f).margin(1.0f));
                // The beam is that ray: it starts at the muzzle and ends at the reticle's point.
                CHECK(Length(ray.origin - (hand + Rotate(aim.orientation, muzzle))) < 1e-5f);
                CHECK(Length(t.point - at) < 2e-3f);
            }
        }
        SECTION("from the muzzle with the pose held: the hit moves with the muzzle, and the "
                "reticle with it") {
            const Vec3f muzzle{0.04f, 0.0f, 0.0f};
            const Posef base{Pointing(at - hand, 0.0f), hand};
            for (const float roll : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
                Posef aim = base;
                aim.orientation = Pointing(at - hand, roll);
                const Ray ray = AimRay(aim, muzzle);
                const LightGunTarget t = LightGunTargetFor(ray, top);
                REQUIRE(t.valid);
                // About the muzzle's offset (4 cm = 10 px on this screen), more only as the
                // screen is met at a slant.
                CHECK(Length(t.point - at) <= 0.05f);
                // And the reticle is exactly where this beam ends.
                CHECK(Length(PointOnQuad(top, t.u, t.v) - t.point) < 1e-4f);
            }
        }
    }
}

TEST_CASE("Direct Aim: on the bottom screen, or away, there is no target", "[frame_xr][kiuaim2]") {
    const PanelQuad top = Screen(1.6f);
    PanelQuad bottom = Screen(0.8f, 4.0f / 3.0f, 1.2f);
    bottom.pose.position = {0.0f, 0.55f, -1.2f};
    const Vec3f hand{0.0f, 1.0f, 0.0f};
    const Vec3f on_bottom = PointOnQuad(bottom, 0.5f, 0.5f);
    CHECK_FALSE(LightGunTargetFor(Ray{hand, Normalized(on_bottom - hand)}, top, &bottom).valid);
    CHECK(LightGunTargetFor(Ray{hand, Normalized(on_bottom - hand)}, top).valid); // no bottom: v > 1
    CHECK_FALSE(LightGunTargetFor(Ray{hand, {0.0f, 0.0f, 1.0f}}, top, &bottom).valid);
    PanelQuad hidden = top;
    hidden.visible = false;
    CHECK_FALSE(LightGunTargetFor(Ray{hand, {0.0f, 0.0f, -1.0f}}, hidden, &bottom).valid);
}

// ---- The reticle's pixels and box ------------------------------------------------------------

TEST_CASE("Direct Aim: the reticle's coordinates and the box", "[frame_xr][kiuaim2]") {
    const ReticleReading ground = Reading(true);
    SECTION("pixels and back") {
        const Vec2f px = ReticlePixel(ground, 0.25f, 0.75f);
        CHECK(px.x == 100.0f);
        CHECK(px.y == 180.0f);
        const Vec2f uv = ReticleUv(ground, px.x, px.y);
        CHECK(uv.x == Approx(0.25f));
        CHECK(uv.y == Approx(0.75f));
    }
    SECTION("the ground box from the game's own factors, just inside") {
        const ReticleBox b = ReachableBox(ground);
        CHECK(b.x0 == Approx(140.5f));
        CHECK(b.x1 == Approx(259.5f));
        CHECK(b.y0 == Approx(32.5f));
        CHECK(b.y1 == Approx(111.5f)); // 1.4 x 80: the mock's "15 px low" was a fixed 1.25 x 80
        ReticleReading tall = ground;
        tall.box_bottom = 0.8f;
        CHECK(ReachableBox(tall).y1 == Approx(143.5f));
    }
    SECTION("the air: the whole screen") {
        const ReticleBox b = ReachableBox(Reading(false));
        CHECK(b.x0 == Approx(0.5f));
        CHECK(b.x1 == Approx(399.5f));
        CHECK(b.y0 == Approx(0.5f));
        CHECK(b.y1 == Approx(239.5f));
        const AimRegion region = ReachableRegion(Reading(false));
        CHECK(region.u0 == Approx(0.5f / 400.0f));
        CHECK(region.v1 == Approx(239.5f / 240.0f));
    }
}

TEST_CASE("Direct Aim: the reticle written is the ray's pixel", "[frame_xr][kiuaim2]") {
    DirectAim aim;
    SECTION("in the air, anywhere on the screen, at once") {
        const ReticleReading r = Reading(false);
        for (const Vec2f& p : std::vector<Vec2f>{{0.02f, 0.03f}, {0.5f, 0.5f}, {0.97f, 0.98f}}) {
            const auto out = aim.Update(1, r, {true, p.x, p.y}, 0.1f, 1.0f, false);
            REQUIRE(out.write);
            CHECK(out.x == Approx(std::clamp(p.x * 400.0f, 0.5f, 399.5f)));
            CHECK(out.y == Approx(std::clamp(p.y * 240.0f, 0.5f, 239.5f)));
            CHECK_FALSE(out.turning); // no turn in the air
        }
    }
    SECTION("on the ground, kept in the box") {
        const ReticleReading r = Reading(true);
        auto out = aim.Update(1, r, {true, 0.55f, 0.3f}, 0.1f, 1.0f, false);
        CHECK(out.x == Approx(220.0f));
        CHECK(out.y == Approx(72.0f));
        out = aim.Update(2, r, {true, 0.8f, 0.6f}, 0.1f, 1.0f, false);
        CHECK(out.x == Approx(259.5f));
        CHECK(out.y == Approx(111.5f));
        CHECK_FALSE(out.turning); // between the box and the Turn Zone: waits at the edge
    }
    SECTION("nothing while the game places it, with no reading, or off the top screen") {
        ReticleReading r = Reading(false);
        r.game_places = true;
        CHECK_FALSE(aim.Update(1, r, {true, 0.5f, 0.5f}, 0.1f, 1.0f, false).write);
        CHECK_FALSE(aim.Update(2, ReticleReading{}, {true, 0.5f, 0.5f}, 0.1f, 1.0f, false).write);
        CHECK_FALSE(aim.Update(3, Reading(false), LightGunTarget{}, 0.1f, 1.0f, false).write);
    }
}

// ---- A game with the mod, frame by frame -----------------------------------------------------

namespace {

/// What the game does each frame (design.md, 0x6D2B5C, 0x6D2C08): the reticle plus the drag (the
/// C-Stick's, and the mod's gyro at `gain` px a degree), clamped to the box (the ground's) or the
/// screen, what is clamped off turning the camera. Direct Aim writes between frames, at the pad
/// updates (234 a second) and the VBlanks (60).
struct Sim {
    ReticleReading r = Reading(true);
    DirectAim aim;
    double t = 0.0;
    double frame_s = 1.0 / 30.0;
    double next_frame = 0.0, next_pad = 0.0, next_vblank = 0.0;
    float gain = 7.0f;   ///< px per degree of the mod's gyro (any: Direct Aim never needs it)
    float stick_px_s = 0.0f;
    float push_right = 0.0f, push_down = 0.0f;
    double turned_x = 0.0, turned_y = 0.0;
    std::vector<Vec2f> frames; ///< the reticle at each game frame (as the game used it)

    void Run(double seconds, const LightGunTarget& target, bool pause, float zone = 0.1f,
             float speed = 1.0f) {
        const double until = t + seconds;
        while (t < until) {
            t += 0.0002;
            const auto poll = [&] {
                const auto out = aim.Update(static_cast<int64_t>(t * 1e9), r, target, zone,
                                            speed, pause);
                if (out.write) {
                    r.x = out.x;
                    r.y = out.y;
                }
                push_right = out.right_dps;
                push_down = out.down_dps;
            };
            if (t >= next_pad) {
                next_pad += 1.0 / 234.0;
                poll();
            }
            if (t >= next_vblank) {
                next_vblank += 1.0 / 59.83;
                poll();
            }
            if (t >= next_frame) {
                next_frame += frame_s;
                // The mod's gyro (its dead zone and clamp), then the stick, then the clamp.
                const auto mod = [](float dps) {
                    const float v = ModRateFor(dps);
                    return v > kModDeadZoneDps    ? std::min(v - kModDeadZoneDps, kModMaxDps)
                           : v < -kModDeadZoneDps ? std::max(v + kModDeadZoneDps, -kModMaxDps)
                                                  : 0.0f;
                };
                r.x += gain * mod(push_right) * static_cast<float>(frame_s) +
                       stick_px_s * static_cast<float>(frame_s);
                r.y += gain * mod(push_down) * static_cast<float>(frame_s);
                const ReticleBox b = r.ground ? GroundBox(r) : ReticleBox{0, 0, r.w, r.h};
                const float cx = std::clamp(r.x, b.x0, b.x1), cy = std::clamp(r.y, b.y0, b.y1);
                turned_x += r.x - cx;
                turned_y += r.y - cy;
                r.x = cx;
                r.y = cy;
                frames.push_back({r.x, r.y});
            }
        }
    }
};

} // namespace

TEST_CASE("Direct Aim: every game frame starts at the ray's pixel, no turn of its own",
          "[frame_xr][kiuaim2]") {
    for (const double fps : {30.0, 60.0}) {
        for (const bool ground : {true, false}) {
            Sim s;
            s.frame_s = 1.0 / fps;
            s.r = Reading(ground);
            const LightGunTarget target{true, 0.52f, 0.31f};
            s.Run(0.5, target, false);
            const ReticleBox b = ReachableBox(s.r);
            for (std::size_t i = 1; i < s.frames.size(); i++) {
                CHECK(s.frames[i].x == Approx(std::clamp(208.0f, b.x0, b.x1)).margin(0.01f));
                CHECK(s.frames[i].y == Approx(std::clamp(74.4f, b.y0, b.y1)).margin(0.01f));
            }
            CHECK(s.turned_x == 0.0);
            CHECK(s.turned_y == 0.0);
        }
    }
}

TEST_CASE("Direct Aim: the C-Stick takes over, then Direct Aim eases back in",
          "[frame_xr][kiuaim2]") {
    Sim s;
    const LightGunTarget target{true, 0.5f, 0.33f};
    s.Run(0.2, target, false);
    REQUIRE(s.r.x == Approx(200.0f));
    // The stick pushed: Direct Aim lets it move the reticle (to the box's edge, then it turns).
    s.stick_px_s = 300.0f;
    s.Run(0.5, target, CStickTakesAim({0.6f, 0.0f}));
    CHECK(s.r.x == Approx(GroundBox(s.r).x1));
    CHECK(s.turned_x > 50.0); // the stick's own turn, not fought
    // Let go: from where it is, back under the ray over 300 ms, no jump.
    s.stick_px_s = 0.0f;
    s.frames.clear();
    const float start = s.r.x;
    s.Run(0.5, target, false);
    REQUIRE(s.frames.size() > 10);
    const float gap = start - 200.0f;
    float prev = start;
    for (const Vec2f& f : s.frames) {
        CHECK(std::fabs(f.x - prev) <= gap * 0.3f); // eased: no frame jumps most of the way
        prev = f.x;
    }
    CHECK(s.frames[1].x > 200.0f + gap * 0.5f); // still on its way after two frames
    CHECK(s.r.x == Approx(200.0f).margin(0.01f)); // and exactly there after
}

TEST_CASE("Direct Aim: the grip pauses, letting go eases back", "[frame_xr][kiuaim2]") {
    Sim s;
    s.r = Reading(false);
    s.Run(0.2, {true, 0.2f, 0.2f}, false);
    REQUIRE(s.r.x == Approx(80.0f));
    // Held: the reticle stays where it is while the hand moves.
    s.Run(0.4, {true, 0.8f, 0.7f}, true);
    CHECK(s.r.x == Approx(80.0f));
    CHECK(s.r.y == Approx(48.0f));
    CHECK(s.turned_x == 0.0);
    s.frames.clear();
    s.Run(0.5, {true, 0.8f, 0.7f}, false);
    float prev = 80.0f;
    for (const Vec2f& f : s.frames) {
        CHECK(std::fabs(f.x - prev) <= 240.0f * 0.3f);
        prev = f.x;
    }
    CHECK(s.r.x == Approx(320.0f).margin(0.01f));
    CHECK(s.r.y == Approx(168.0f).margin(0.01f));
}

TEST_CASE("Direct Aim: the Turn Zone turns on the ground, deeper is faster", "[frame_xr][kiuaim2]") {
    const auto turn = [](float u, float v, bool ground, float zone, float speed) {
        Sim s;
        s.r = Reading(ground);
        s.Run(1.0, {true, u, v}, false, zone, speed);
        return Vec2f{static_cast<float>(s.turned_x), static_cast<float>(s.turned_y)};
    };
    const Vec2f mid = turn(0.85f, 0.4f, true, 0.1f, 1.0f); // past the box, short of the zone
    CHECK(mid.x == 0.0f);
    const Vec2f half = turn(0.95f, 0.4f, true, 0.1f, 1.0f);
    const Vec2f edge = turn(1.0f, 0.4f, true, 0.1f, 1.0f);
    const Vec2f beyond = turn(1.3f, 0.4f, true, 0.1f, 1.0f);
    CHECK(half.x > 0.0f);
    CHECK(edge.x > half.x * 1.6f);
    CHECK(beyond.x == Approx(edge.x).epsilon(0.02)); // 1 at the edge and beyond
    CHECK(turn(0.0f, 0.4f, true, 0.1f, 1.0f).x < -0.9f * edge.x); // left: the other way
    CHECK(turn(0.5f, 0.02f, true, 0.1f, 1.0f).y < 0.0f);          // top: up
    CHECK(turn(1.0f, 0.4f, true, 0.1f, 2.0f).x > 1.8f * edge.x);  // Turn Speed
    CHECK(turn(1.0f, 0.4f, true, 0.0f, 1.0f).x == 0.0f);          // Turn Zone 0 %: off
    CHECK(turn(1.0f, 0.4f, false, 0.1f, 1.0f).x == 0.0f);         // the air: no turn
    // At the edge of the screen the push is the mod's gyro at kTurnPushDps x Turn Speed.
    DirectAim aim;
    const auto out = aim.Update(1, Reading(true), {true, 1.0f, 0.4f}, 0.1f, 1.5f, false);
    CHECK(out.turning);
    CHECK(out.right_dps == Approx(1.5f * DirectAim::kTurnPushDps));
    CHECK(out.x == Approx(259.5f)); // written at the box's edge
}

// ---- The samples, XR frame by XR frame -------------------------------------------------------

TEST_CASE("Direct Aim: the latest XR sample, carried to the moment it is written",
          "[frame_xr][kiuaim2]") {
    AimSampler s;
    CHECK_FALSE(s.At(0).valid);
    constexpr int64_t kXr = 8'333'333; // 120 Hz
    const auto ray_u = [](int64_t ns) { return 0.3f + 0.4f * static_cast<float>(ns) * 1e-9f; };
    SECTION("each write uses the newest sample, along its motion") {
        int64_t xr = 0;
        float worst = 0.0f;
        for (int64_t now = 0; now < 1'000'000'000; now += 4'273'504) { // 234 Hz writes
            while (xr <= now) {
                s.Push(xr, {true, ray_u(xr), 0.5f});
                xr += kXr;
            }
            const LightGunTarget t = s.At(now);
            REQUIRE(t.valid);
            if (now > 2 * kXr) {
                worst = std::max(worst, std::fabs(t.u - ray_u(now)) * 400.0f);
            }
        }
        CHECK(worst < 0.01f); // a steady sweep (160 px a second): on it, not a frame behind
    }
    SECTION("never more than kMaxLeadNs ahead, nothing across a gap, nothing invalid") {
        s.Push(0, {true, 0.5f, 0.5f});
        s.Push(kXr, {true, 0.51f, 0.5f});
        CHECK(s.At(kXr + 4'000'000).u == Approx(0.51f + 0.01f * 4.0f / 8.333f).margin(1e-4f));
        CHECK(s.At(kXr + 100'000'000).u == Approx(0.51f + 0.01f * 12.0f / 8.333f).margin(1e-4f));
        s.Push(kXr + 60'000'000, {true, 0.6f, 0.5f}); // 60 ms later: no motion to carry
        CHECK(s.At(kXr + 70'000'000).u == Approx(0.6f));
        s.Push(kXr + 68'000'000, {false, 0.5f, 0.5f});
        CHECK_FALSE(s.At(kXr + 70'000'000).valid);
    }
}

TEST_CASE("Direct Aim: from the XR frame to the game's frame, a sweep and a stop",
          "[frame_xr][kiuaim2]") {
    // The XR thread at 120 Hz; the emulation thread writing at its pad updates and VBlanks; the
    // game reading once a frame at 30. The reticle each game frame against the ray then.
    Sim s;
    s.r = Reading(false);
    AimSampler sampler;
    const auto ray_u = [](double t) { return t < 0.5 ? 0.2 + 0.5 * t : 0.45; }; // 200 px/s, stop
    double xr = 0.0;
    double worst_sweep = 0.0, worst_still = 0.0;
    std::size_t seen = 0;
    while (s.t < 1.0) {
        while (xr <= s.t) {
            sampler.Push(static_cast<int64_t>(xr * 1e9),
                         {true, static_cast<float>(ray_u(xr)), 0.5f});
            xr += 1.0 / 120.0;
        }
        s.Run(0.0002, sampler.At(static_cast<int64_t>(s.t * 1e9)), false);
        if (s.frames.size() > seen) {
            seen = s.frames.size();
            const double err = std::fabs(s.frames.back().x - ray_u(s.t) * 400.0);
            if (s.t > 0.05 && s.t < 0.5) {
                worst_sweep = std::max(worst_sweep, err);
            } else if (s.t > 0.55) {
                worst_still = std::max(worst_still, err);
            }
        }
    }
    CHECK(worst_sweep < 1.5); // within the 4.3 ms between two writes at 200 px/s
    CHECK(worst_still < 0.01);
}

// ---- Check Aim -------------------------------------------------------------------------------

namespace {
/// Where a game whose reticle sprite shows `dx`, `dy` pixels off (and scaled `s`) shows it.
Vec2f ShownBy(Vec2f written, float dx, float dy, float s = 1.0f) {
    return {0.5f + (written.x - 0.5f) * s + dx / 400.0f, 0.5f + (written.y - 0.5f) * s + dy / 240.0f};
}
/// The pull the player makes to put that reticle on `target`, `slip` pixels off.
LightGunTarget PullFor(Vec2f target, float dx, float dy, float s = 1.0f, Vec2f slip = {}) {
    // shown(written) = target  =>  written = (target - 0.5 - d) / s + 0.5
    return {true, (target.x - 0.5f - dx / 400.0f) / s + 0.5f + slip.x / 400.0f,
            (target.y - 0.5f - dy / 240.0f) / s + 0.5f + slip.y / 240.0f};
}
} // namespace

TEST_CASE("Check Aim: seven targets, at the corners, centre and side middles",
          "[frame_xr][kiuaim2]") {
    const auto t = AimTargets({0.0f, 0.0f, 1.0f, 1.0f});
    CHECK(t[0].x == Approx(0.5f));
    CHECK(t[0].y == Approx(0.5f));
    int corners = 0, sides = 0;
    for (const Vec2f& p : t) {
        const bool cu = std::fabs(p.x - 0.08f) < 1e-5f || std::fabs(p.x - 0.92f) < 1e-5f;
        const bool cv = std::fabs(p.y - 0.08f) < 1e-5f || std::fabs(p.y - 0.92f) < 1e-5f;
        corners += cu && cv ? 1 : 0;
        sides += cu && std::fabs(p.y - 0.5f) < 1e-5f ? 1 : 0;
    }
    CHECK(corners == 4);
    CHECK(sides == 2);
    // On the ground: the box's corners (where the reticle can go).
    const AimRegion box = ReachableRegion(Reading(true));
    for (const Vec2f& p : AimTargets(box)) {
        CHECK(p.x > box.u0);
        CHECK(p.x < box.u1);
        CHECK(p.y > box.v0);
        CHECK(p.y < box.v1);
    }
}

TEST_CASE("Check Aim: an exact game is identity, a shifted sprite is corrected",
          "[frame_xr][kiuaim2]") {
    const auto run = [](float dx, float dy, float s, std::vector<Vec2f> slips = {}) {
        AimCheckFlow flow;
        flow.Start({0.0f, 0.0f, 1.0f, 1.0f}, 400.0f, 240.0f);
        AimCheckFlow::Out out;
        int i = 0;
        while (flow.active()) {
            const Vec2f slip = i < static_cast<int>(slips.size()) ? slips[i] : Vec2f{};
            out = flow.Update(true, PullFor(flow.targets()[flow.step()], dx, dy, s, slip));
            i++;
            REQUIRE(i < 40);
        }
        REQUIRE(out.done);
        return out.result;
    };
    SECTION("exact, with the hand's own half-pixel wobble: nothing to correct") {
        const auto m = run(0.0f, 0.0f, 1.0f,
                           {{0.4f, -0.3f}, {-0.5f, 0.2f}, {0.3f, 0.4f}, {0.0f, -0.4f},
                            {-0.2f, 0.1f}, {0.5f, 0.0f}, {-0.3f, -0.2f}});
        REQUIRE(m);
        CHECK(m->set);
        CHECK(m->u[0] == 0.0f);
        CHECK(m->u[1] == 1.0f);
        CHECK(m->error_px < 1.0f);
        CHECK(AimCheckedToast(m->error_px) == "Aim Checked \xC2\xB7 Within 1 px");
    }
    SECTION("a sprite 3 px right and 2 px down, 2 % large: the reticle lands on the ray after") {
        const auto m = run(3.0f, 2.0f, 1.02f);
        REQUIRE(m);
        CHECK(m->error_px < 0.1f);
        for (const Vec2f& want : std::vector<Vec2f>{{0.1f, 0.1f}, {0.5f, 0.5f}, {0.9f, 0.85f}}) {
            const LightGunTarget written = ApplyAimMap(*m, {true, want.x, want.y});
            const Vec2f shown = ShownBy({written.u, written.v}, 3.0f, 2.0f, 1.02f);
            CHECK(shown.x * 400.0f == Approx(want.x * 400.0f).margin(0.05f));
            CHECK(shown.y * 240.0f == Approx(want.y * 240.0f).margin(0.05f));
        }
    }
}

TEST_CASE("Check Aim: a slip is asked again, three in a row start over", "[frame_xr][kiuaim2]") {
    AimCheckFlow flow;
    flow.Start({0.0f, 0.0f, 1.0f, 1.0f}, 400.0f, 240.0f);
    // Nothing happens without a pull.
    CHECK_FALSE(flow.Update(false, {true, 0.5f, 0.5f}).changed);
    // Far off the target: asked again.
    auto out = flow.Update(true, PullFor(flow.targets()[0], 0, 0, 1, {40.0f, 0.0f}));
    CHECK(out.retry);
    CHECK(flow.step() == 0);
    out = flow.Update(true, PullFor(flow.targets()[0], 0, 0));
    CHECK(out.changed);
    out = flow.Update(true, PullFor(flow.targets()[1], 0, 0));
    CHECK(flow.step() == 2);
    // Off the line the first two give by 10 px (within kAimCheckMaxPx of the target): a slip.
    out = flow.Update(true, PullFor(flow.targets()[2], 0, 0, 1, {10.0f, 0.0f}));
    CHECK(out.retry);
    CHECK(flow.step() == 2);
    out = flow.Update(true, PullFor(flow.targets()[2], 0, 0, 1, {0.0f, 10.0f}));
    CHECK(out.retry);
    out = flow.Update(true, PullFor(flow.targets()[2], 0, 0, 1, {-10.0f, 0.0f}));
    CHECK(out.restart);
    CHECK(flow.step() == 0);
    // Not on the top screen: asked again.
    CHECK(flow.Update(true, LightGunTarget{}).retry);
}

TEST_CASE("Check Aim: the targets taken at any roll give the same map", "[frame_xr][kiuaim2]") {
    const PanelQuad top = Screen(1.6f);
    const Vec3f hand{0.1f, 1.0f, -0.2f};
    std::optional<AimMap> first;
    for (const float roll : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
        AimCheckFlow flow;
        flow.Start({0.0f, 0.0f, 1.0f, 1.0f}, 400.0f, 240.0f);
        AimCheckFlow::Out out;
        while (flow.active()) {
            // The player puts the reticle (2 px right of where it is written) on the target.
            const LightGunTarget want = PullFor(flow.targets()[flow.step()], 2.0f, 0.0f);
            const Vec3f at = PointOnQuad(top, want.u, want.v);
            const Posef aim{Pointing(at - hand, roll), hand};
            out = flow.Update(true, LightGunTargetFor(AimRay(aim, std::nullopt), top));
        }
        REQUIRE(out.result);
        if (!first) {
            first = out.result;
        }
        CHECK(out.result->u[0] == Approx(first->u[0]).margin(1e-4f));
        CHECK(out.result->u[1] == Approx(first->u[1]).margin(1e-4f));
        CHECK(out.result->v[0] == Approx(first->v[0]).margin(1e-4f));
        CHECK(out.result->u[0] * 400.0f == Approx(2.0f).margin(0.05f));
    }
}

TEST_CASE("Check Aim: the file keeps an older build's line, and is checked when read",
          "[frame_xr][kiuaim2]") {
    AimMap m;
    m.set = true;
    m.u[0] = 0.005f;
    m.u[1] = 1.01f;
    m.v[0] = -0.004f;
    m.error_px = 0.8f;
    m.points = 7;
    SECTION("written and read back") {
        const std::string text = AimFileText("", m);
        const AimMap back = AimMapFromFile(text);
        REQUIRE(back.set);
        CHECK(back.u[0] == Approx(0.005f));
        CHECK(back.u[1] == Approx(1.01f));
        CHECK(back.points == 7);
    }
    SECTION("an older build's line stays first (it reads only that one), ours after") {
        const std::string old = "v1 0.01000 1.00000 -0.02000 1.00000 0\n";
        const std::string text = AimFileText(old, m);
        CHECK(text.rfind("v1 0.01000", 0) == 0);
        bool had_old = false;
        CHECK(AimMapFromFile(text, &had_old).set);
        CHECK(had_old);
        // Reset: our line goes, the older build's stays.
        CHECK(AimFileText(text, AimMap{}) == old);
        // That older line alone is not used here.
        CHECK_FALSE(AimMapFromFile(old).set);
        // A file of ours alone: an older build reads its first line as "v1" and finds none.
        float a, b, c, d;
        int e;
        CHECK(std::sscanf(AimFileText("", m).c_str(), "v1 %f %f %f %f %d", &a, &b, &c, &d, &e) !=
              5);
        CHECK(AimFileText("", AimMap{}).empty()); // nothing to keep: no file
    }
    SECTION("out of the limits, or garbled: not used") {
        bool bad = false;
        CHECK_FALSE(AimMapFromFile("v2 0.3 1 0 0 1 0 0.5 7\n", nullptr, &bad).set); // 30 % off
        CHECK(bad);
        CHECK_FALSE(AimMapFrom("v2 0 1.4 0 0 1 0 0.5 7").set);  // scale
        CHECK_FALSE(AimMapFrom("v2 0 1 0.2 0 1 0 0.5 7").set);  // skew
        CHECK_FALSE(AimMapFrom("v2 0 1 0 0 1 0 nan 7").set);
        CHECK_FALSE(AimMapFrom("v2 0 1 0").set);
        CHECK_FALSE(AimMapFrom("").set);
        // Not set: the ray's own point is written.
        const LightGunTarget t = ApplyAimMap(AimMap{}, {true, 0.3f, 0.6f});
        CHECK(t.u == 0.3f);
        CHECK(t.v == 0.6f);
    }
}

TEST_CASE("Check Aim: its prompts", "[frame_xr][kiuaim2]") {
    CHECK(AimCheckStepToast(1) == "Check Aim: Target 2 of 7");
    CHECK(AimCheckedToast(0.2f) == "Aim Checked \xC2\xB7 Within 1 px");
    CHECK(AimCheckedToast(2.4f) == "Aim Checked \xC2\xB7 Within 3 px");
}

TEST_CASE("Check Aim: its target's art", "[frame_xr][kiuaim2]") {
    using namespace FrameXr::Vis;
    const std::vector<uint8_t> art = AimTargetArt();
    REQUIRE(art.size() == static_cast<std::size_t>(kAimTargetPixels * kAimTargetPixels * 4));
    const auto at = [&](int x, int y) { return &art[(static_cast<std::size_t>(y) * kAimTargetPixels + x) * 4]; };
    const int c = kAimTargetPixels / 2;
    CHECK(at(c, c)[3] == 255);            // the middle dot
    CHECK(at(c, c)[0] > at(c, c)[2]);     // amber
    CHECK(at(c + c - 5, c)[3] == 255);    // the ring
    CHECK(at(c + 9, c)[3] == 0);          // clear between them: the reticle shows through
    CHECK(at(0, 0)[3] == 0);              // the corners
    CHECK(AimTargetSize(1.6f) == Approx(0.08f));
}
