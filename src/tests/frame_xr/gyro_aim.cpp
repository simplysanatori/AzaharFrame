// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/gyro_aim.h"
#include "frame_xr/kiu_scan.h"
#include "frame_xr/vis/beam.h"

// AzaharFrame kiugyro: Gyro Aim (frame_xr/input/gyro_aim.h) and the mod scan (frame_xr/kiu_scan.h).
using Catch::Approx;
using namespace FrameXr::Input;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;
constexpr int64_t kStepNs = 11'111'111; // 90 Hz

Quatf AxisAngle(Vec3f axis, float degrees) {
    const Vec3f n = Normalized(axis);
    const float half = degrees * kDeg * 0.5f;
    const float s = std::sin(half);
    return Quatf{n.x * s, n.y * s, n.z * s, std::cos(half)};
}

/// A controller aimed `right` degrees to the right and `up` degrees up, rolled `roll` degrees.
HandInput Aimed(float right, float up, float roll = 0.0f) {
    HandInput hand;
    hand.active = true;
    hand.aim.orientation = Multiply(Multiply(AxisAngle({0, 1, 0}, -right), AxisAngle({1, 0, 0}, up)),
                                    AxisAngle({0, 0, -1}, roll));
    return hand;
}

/// What the C-Stick & Gyro mod (v172) does with one HID gyroscope entry: z is the horizontal
/// rate and x the vertical one, each after Azahar's s16 truncation, the 0.25 deg/s dead zone and
/// the +-360 clamp (design.md facts 2 to 4). Positive moves the reticle right / down.
struct ModRead {
    float right = 0.0f;
    float down = 0.0f;
};
float ModAxis(float dps) {
    const float raw = std::trunc(dps * 14.375f); // HID: s16(gyro * coef)
    float v = raw / 14.375f;
    if (v > 0.25f) {
        v -= 0.25f;
    } else if (v < -0.25f) {
        v += 0.25f;
    } else {
        v = 0.0f;
    }
    return std::clamp(v, -360.0f, 360.0f);
}
ModRead ModReads(const Vec3f& gyro) {
    return {ModAxis(gyro.z), ModAxis(gyro.x)};
}

} // namespace

TEST_CASE("FrameXr Gyro Aim: the mod's axes follow the hand", "[frame_xr][kiugyro]") {
    GyroAimTracker tracker;
    const float roll = GENERATE(0.0f, 35.0f, -60.0f);
    INFO("roll " << roll);
    tracker.Update(Aimed(0, 0, roll), 0, 1.0f, false);
    SECTION("turning right moves the reticle right") {
        const auto out = tracker.Update(Aimed(1.0f, 0, roll), kStepNs, 1.0f, false);
        REQUIRE(out.right_dps == Approx(90.0f).epsilon(0.02));
        REQUIRE(out.down_dps == Approx(0.0f).margin(0.5f));
        const ModRead m = ModReads(ModGyro(out.right_dps, out.down_dps));
        REQUIRE(m.right > 85.0f);
        REQUIRE(m.down == 0.0f);
    }
    SECTION("aiming up moves it up (down negative)") {
        const auto out = tracker.Update(Aimed(0, 0.5f, roll), kStepNs, 1.0f, false);
        REQUIRE(out.down_dps == Approx(-45.0f).epsilon(0.02));
        REQUIRE(out.right_dps == Approx(0.0f).margin(0.5f));
        const ModRead m = ModReads(ModGyro(out.right_dps, out.down_dps));
        REQUIRE(m.down < -40.0f);
        REQUIRE(m.right == 0.0f);
    }
    SECTION("turning left and aiming down") {
        const auto out = tracker.Update(Aimed(-0.5f, -0.5f, roll), kStepNs, 1.0f, false);
        REQUIRE(out.right_dps < -40.0f);
        REQUIRE(out.down_dps > 40.0f);
    }
}

TEST_CASE("FrameXr Gyro Aim: rolling the controller moves nothing", "[frame_xr][kiugyro]") {
    GyroAimTracker tracker;
    tracker.Update(Aimed(10, 5, 0), 0, 1.0f, false);
    const auto out = tracker.Update(Aimed(10, 5, 20), kStepNs, 1.0f, false);
    REQUIRE(out.right_dps == Approx(0.0f).margin(0.05f));
    REQUIRE(out.down_dps == Approx(0.0f).margin(0.05f));
}

TEST_CASE("FrameXr Gyro Aim: Gyro Sensitivity scales the rate", "[frame_xr][kiugyro]") {
    const float s = GENERATE(0.2f, 0.5f, 1.0f, 2.0f, 4.0f);
    GyroAimTracker one, scaled;
    one.Update(Aimed(0, 0), 0, 1.0f, false);
    scaled.Update(Aimed(0, 0), 0, s, false);
    const auto a = one.Update(Aimed(0.7f, -0.3f), kStepNs, 1.0f, false);
    const auto b = scaled.Update(Aimed(0.7f, -0.3f), kStepNs, s, false);
    REQUIRE(b.right_dps == Approx(a.right_dps * s).epsilon(1e-4));
    REQUIRE(b.down_dps == Approx(a.down_dps * s).epsilon(1e-4));
    SECTION("out of range is clamped, not a fault") {
        REQUIRE(ClampGyroSensitivity(0.0f) == kGyroSensitivityMin);
        REQUIRE(ClampGyroSensitivity(9.0f) == kGyroSensitivityMax);
        REQUIRE(ClampGyroSensitivity(std::nanf("")) == kGyroSensitivityDefault);
    }
}

TEST_CASE("FrameXr Gyro Aim: the grip pauses, and letting go does not jump", "[frame_xr][kiugyro]") {
    GyroAimTracker tracker;
    GripClutch clutch;
    int64_t t = 0;
    float yaw = 0.0f;
    float moved_while_free = 0.0f;
    // Turn right 30 degrees, hold the grip, swing back 30 degrees left, let go, turn right again.
    const auto step = [&](float d_yaw, float grip) {
        yaw += d_yaw;
        t += kStepNs;
        const bool held = clutch.Update(grip);
        const auto out = tracker.Update(Aimed(yaw, 0), t, 1.0f, held);
        return std::pair{out, held};
    };
    for (int i = 0; i < 30; i++) {
        moved_while_free += step(1.0f, 0.0f).first.right_dps * (kStepNs * 1e-9f);
    }
    REQUIRE(moved_while_free == Approx(29.0f).margin(1.1f)); // the first frame is a start
    for (int i = 0; i < 30; i++) {
        const auto [out, held] = step(-1.0f, 1.0f);
        REQUIRE(held);
        REQUIRE(out.right_dps == 0.0f);
        REQUIRE(out.down_dps == 0.0f);
    }
    // Let go: the hand is 30 degrees left of where it was, and the first frame sends nothing.
    const auto [first, held] = step(0.0f, 0.0f);
    REQUIRE(!held);
    REQUIRE(first.right_dps == 0.0f);
    // Then it carries on from the new place.
    const auto next = step(1.0f, 0.0f).first;
    REQUIRE(next.right_dps == Approx(90.0f).epsilon(0.02));
    SECTION("the grip's hysteresis") {
        GripClutch c;
        REQUIRE(!c.Update(0.55f));
        REQUIRE(c.Update(0.65f));
        REQUIRE(c.Update(0.45f));
        REQUIRE(!c.Update(0.35f));
    }
}

TEST_CASE("FrameXr Gyro Aim: ModRateFor gives the mod the rate asked for", "[frame_xr][kiugyro]") {
    for (float r = -400.0f; r <= 400.0f; r += 0.37f) {
        const float sent = ModRateFor(r);
        const float applied = ModAxis(sent);
        if (std::fabs(r) < 0.02f) {
            REQUIRE(applied == 0.0f);
        } else if (std::fabs(r) <= 359.0f) {
            REQUIRE(applied == Approx(r).margin(kHidGyroStepDps));
        } else {
            REQUIRE(std::fabs(applied) ==
                    Approx(std::min(std::fabs(r), 360.0f)).margin(kHidGyroStepDps));
        }
    }
    REQUIRE(ModRateFor(std::nanf("")) == 0.0f);
}

TEST_CASE("FrameXr Gyro Aim: Light Gun's target and Turn Zone", "[frame_xr][kiugyro]") {
    PanelQuad top;
    top.visible = true;
    top.width_m = 1.6f;
    top.height_m = 0.96f;
    top.pose.position = {0.0f, 0.0f, -2.0f};
    SECTION("the middle, the corners") {
        const LightGunTarget mid = LightGunTargetFor(Aimed(0, 0), top);
        REQUIRE(mid.valid);
        REQUIRE(mid.u == Approx(0.5f).margin(1e-4f));
        REQUIRE(mid.v == Approx(0.5f).margin(1e-4f));
        const float right = std::atan2(0.8f, 2.0f) / kDeg;
        const float up = std::atan2(0.48f, 2.0f) / kDeg;
        const LightGunTarget tr = LightGunTargetFor(Aimed(right * 0.999f, 0), top);
        REQUIRE(tr.u == Approx(1.0f).margin(2e-3f));
        HandInput h = Aimed(0, 0);
        h.aim.orientation = AxisAngle({1, 0, 0}, up);
        const LightGunTarget t = LightGunTargetFor(h, top);
        REQUIRE(t.v == Approx(0.0f).margin(2e-3f));
    }
    SECTION("not shown, not tracked, pointing away") {
        PanelQuad hidden = top;
        hidden.visible = false;
        REQUIRE(!LightGunTargetFor(Aimed(0, 0), hidden).valid);
        HandInput lost = Aimed(0, 0);
        lost.active = false;
        REQUIRE(!LightGunTargetFor(lost, top).valid);
        REQUIRE(!LightGunTargetFor(Aimed(180, 0), top).valid);
    }
    SECTION("depth in the zone") {
        const float zone = 0.1f;
        REQUIRE(TurnZoneDepth({true, 0.5f, 0.5f}, zone).x == 0.0f);
        REQUIRE(TurnZoneDepth({true, 0.95f, 0.5f}, zone).x == Approx(0.5f));
        REQUIRE(TurnZoneDepth({true, 1.2f, 0.5f}, zone).x == 1.0f);
        REQUIRE(TurnZoneDepth({true, 0.02f, 0.5f}, zone).x == Approx(-0.8f));
        REQUIRE(TurnZoneDepth({true, 0.5f, 0.0f}, zone).y == -1.0f);
        REQUIRE(TurnZoneDepth({true, 0.95f, 0.5f}, 0.0f).x == 0.0f); // Turn Zone 0%: off
        REQUIRE(TurnZoneDepth({false, 0.95f, 0.5f}, zone).x == 0.0f);
    }
}

namespace {

/// The game with the mod, as far as Light Gun sees it: the HID gyroscope sampled at 101 Hz,
/// the game reading the latest sample once per game frame and moving the reticle by
/// gain x rate x frame (on the ground, clamped to the box: the overshoot turns the camera), the
/// move visible to the probe `delay` after the frame starts.
struct Game {
    float frame_s = 1.0f / 60.0f;
    float delay_s = 0.3f / 60.0f;
    float gain_x = 4.0f; ///< px per degree (the game's sensitivity; negative: inverted)
    float gain_y = 4.0f;
    bool ground = true;
    float w = 400.0f, h = 240.0f;
    float x = 200.0f, y = 120.0f; ///< the reticle as moved
    float seen_x = 200.0f, seen_y = 120.0f;
    double turned_x = 0.0, turned_y = 0.0; ///< overshoot past the box, px
    // internal
    ModRead hid;
    double next_hid = 0.0, next_frame = 0.0;
    struct Pending {
        double at;
        ModRead rate;
    };
    std::vector<Pending> pending;

    ReticleReading Reading() const {
        ReticleReading r;
        r.valid = true;
        r.x = seen_x;
        r.y = seen_y;
        r.w = w;
        r.h = h;
        r.cx = w * 0.5f;
        r.cy = h * 0.5f;
        r.ground = ground;
        return r;
    }

    /// Runs to `until_s`; each HID sample asks the loop.
    template <typename Ask>
    void Run(double& now_s, double until_s, Ask&& ask) {
        while (now_s < until_s) {
            now_s += 0.0005;
            if (now_s >= next_hid) {
                next_hid += 1.0 / 101.0;
                hid = ModReads(ask(now_s, Reading()));
            }
            if (now_s >= next_frame) {
                next_frame += frame_s;
                pending.push_back({now_s + delay_s, hid});
            }
            while (!pending.empty() && now_s >= pending.front().at) {
                const ModRead rate = pending.front().rate;
                pending.erase(pending.begin());
                x += gain_x * rate.right * frame_s;
                y += gain_y * rate.down * frame_s;
                float x0 = 0, y0 = 0, x1 = w, y1 = h;
                if (ground) {
                    ReticleReading r = Reading();
                    const ReticleBox b = GroundBox(r);
                    x0 = b.x0, y0 = b.y0, x1 = b.x1, y1 = b.y1;
                }
                if (x < x0) {
                    turned_x += x - x0;
                    x = x0;
                } else if (x > x1) {
                    turned_x += x - x1;
                    x = x1;
                }
                if (y < y0) {
                    turned_y += y - y0;
                    y = y0;
                } else if (y > y1) {
                    turned_y += y - y1;
                    y = y1;
                }
                seen_x = x;
                seen_y = y;
            }
        }
    }
};

struct Session {
    Game game;
    LightGunLoop loop;
    double now = 0.0;
    LightGunTarget target{true, 0.5f, 0.5f};
    float zone = 0.1f;
    float speed = 1.0f;
    bool clutch = false;
    std::vector<float> xs; ///< the reticle seen at each game frame

    void Run(double seconds) {
        const double until = now + seconds;
        double next_log = now;
        game.Run(now, until, [&](double t, const ReticleReading& r) {
            const auto out = loop.Update(static_cast<int64_t>(t * 1e9), r, target, zone, speed,
                                         clutch);
            return ModGyro(ModRateFor(out.right_dps), ModRateFor(out.down_dps));
        });
        (void)next_log;
    }
    /// Moves the target there, then counts game frames until the reticle stays within 1 px of
    /// it; `overshoot` gets how far past the target it went (px, in the move's direction).
    int Step(float u, float v, float& overshoot, double max_s = 2.0) {
        const float from_x = game.seen_x, from_y = game.seen_y;
        target = {true, u, v};
        const float tx = u * game.w, ty = v * game.h;
        const float dir_x = tx >= from_x ? 1.0f : -1.0f;
        const float dir_y = ty >= from_y ? 1.0f : -1.0f;
        overshoot = 0.0f;
        int frames = -1;
        int settled = 0;
        const int max_frames = static_cast<int>(max_s / game.frame_s);
        for (int f = 0; f < max_frames; f++) {
            Run(game.frame_s);
            overshoot = std::max(overshoot, (game.seen_x - tx) * dir_x);
            overshoot = std::max(overshoot, (game.seen_y - ty) * dir_y);
            const bool in = std::fabs(game.seen_x - tx) <= 1.0f && std::fabs(game.seen_y - ty) <= 1.0f;
            settled = in ? settled + 1 : 0;
            if (settled == 1) {
                frames = f + 1;
            }
            if (settled >= 20) {
                return frames;
            }
        }
        return -1;
    }
};

} // namespace

TEST_CASE("FrameXr Light Gun: the reticle goes under the pointer, 30 and 60 fps",
          "[frame_xr][kiugyro]") {
    const float fps = GENERATE(30.0f, 60.0f);
    const float gain = GENERATE(3.0f, 4.0f, 9.0f, 25.0f);
    const float extra_frames = GENERATE(0.0f, 1.0f); // the game applies input a frame later
    INFO("fps " << fps << " gain " << gain << " extra latency frames " << extra_frames);
    Session s;
    s.game.frame_s = 1.0f / fps;
    s.game.delay_s = (0.3f + extra_frames) / fps;
    s.game.gain_x = gain;
    s.game.gain_y = gain;
    s.game.ground = false; // the whole screen: no box
    // Warm up with a few moves (the gain is learnt from them, from 12 px per degree).
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> pos(0.1f, 0.9f);
    float over = 0.0f;
    for (int i = 0; i < 6; i++) {
        REQUIRE(s.Step(pos(rng), pos(rng), over, 4.0) > 0);
    }
    CHECK(std::fabs(s.loop.gain_x()) == Approx(gain).epsilon(0.35));
    CHECK(s.loop.frame_s() > 0.8f / fps);
    CHECK(s.loop.frame_s() < 1.5f / fps);
    SECTION("long moves: as fast as the mod's 360 deg/s cap lets it, no overshoot") {
        int worst = 0;
        float worst_over = 0.0f;
        for (int i = 0; i < 12; i++) {
            const int frames = s.Step(pos(rng), pos(rng), over);
            REQUIRE(frames > 0);
            worst = std::max(worst, frames);
            worst_over = std::max(worst_over, over);
        }
        INFO("worst frames " << worst << ", worst overshoot " << worst_over << " px");
        CHECK(worst <= 28);
        // A frame of latency more than the loop assumes costs a few pixels more.
        CHECK(worst_over <= 3.0f + 4.0f * extra_frames);
    }
    SECTION("small corrections settle within 1 px in a few frames") {
        std::uniform_real_distribution<float> nudge(-0.05f, 0.05f);
        int worst = 0;
        float worst_over = 0.0f;
        for (int i = 0; i < 12; i++) {
            const float u = std::clamp(s.target.u + nudge(rng), 0.1f, 0.9f);
            const float v = std::clamp(s.target.v + nudge(rng), 0.1f, 0.9f);
            const int frames = s.Step(u, v, over);
            REQUIRE(frames > 0);
            worst = std::max(worst, frames);
            worst_over = std::max(worst_over, over);
        }
        INFO("worst frames " << worst << ", worst overshoot " << worst_over << " px");
        CHECK(worst <= 16);
        CHECK(worst_over <= 3.0f + 4.0f * extra_frames);
    }
    SECTION("a sweep is followed a few frames behind") {
        REQUIRE(s.Step(0.2f, 0.5f, over) > 0);
        float u = 0.2f;
        float lag = 0.0f;
        const float speed = 150.0f; // px a second
        for (int f = 0; f < static_cast<int>(fps); f++) {
            u += speed / s.game.w / fps;
            s.target = {true, u, 0.5f};
            s.Run(1.0 / fps);
            if (f > static_cast<int>(0.4f * fps)) {
                lag = std::max(lag, std::fabs(u * s.game.w - s.game.seen_x));
            }
        }
        INFO("lag " << lag << " px");
        CHECK(lag <= speed * (4.0f + extra_frames) / fps);
    }
    SECTION("at rest: still (no dither)") {
        const float x = s.game.seen_x, y = s.game.seen_y;
        s.Run(1.0);
        CHECK(s.game.seen_x == x);
        CHECK(s.game.seen_y == y);
    }
}

TEST_CASE("FrameXr Light Gun: any game speed, gain, sign and latency (sweep)",
          "[frame_xr][kiugyro]") {
    std::mt19937 rng(20261114);
    std::uniform_real_distribution<float> fps_d(20.0f, 60.0f), gain_d(2.5f, 40.0f),
        late_d(0.0f, 1.2f), pos(0.1f, 0.9f), coin(0.0f, 1.0f);
    int over_4 = 0;
    for (int run = 0; run < 40; run++) {
        Session s;
        const float fps = fps_d(rng);
        s.game.frame_s = 1.0f / fps;
        s.game.delay_s = (0.2f + late_d(rng)) / fps;
        s.game.gain_x = gain_d(rng) * (coin(rng) < 0.2f ? -1.0f : 1.0f);
        s.game.gain_y = gain_d(rng) * (coin(rng) < 0.3f ? -1.0f : 1.0f);
        s.game.ground = false;
        INFO("run " << run << ": fps " << fps << ", gains " << s.game.gain_x << ", "
                    << s.game.gain_y << ", delay " << s.game.delay_s * fps << " frames");
        float over = 0.0f;
        for (int i = 0; i < 4; i++) {
            REQUIRE(s.Step(pos(rng), pos(rng), over, 6.0) > 0);
        }
        float worst_over = 0.0f;
        for (int i = 0; i < 6; i++) {
            REQUIRE(s.Step(pos(rng), pos(rng), over, 3.0) > 0);
            worst_over = std::max(worst_over, over);
        }
        // Past the target by a few pixels at most; a little more now and then where the game's
        // latency is far from the model's one frame.
        CHECK(worst_over <= 30.0f);
        over_4 += worst_over > 4.0f ? 1 : 0;
    }
    CHECK(over_4 <= 4);
}

TEST_CASE("FrameXr Light Gun: an inverted axis is learnt, not fought", "[frame_xr][kiugyro]") {
    Session s;
    s.game.ground = false;
    s.game.gain_y = -5.0f; // the game's Invert option
    s.game.gain_x = 6.0f;
    float over = 0.0f;
    REQUIRE(s.Step(0.3f, 0.7f, over, 6.0) > 0);
    REQUIRE(s.loop.gain_y() < 0.0f);
    REQUIRE(s.Step(0.8f, 0.2f, over, 2.0) > 0);
    CHECK(over <= 3.0f);
}

TEST_CASE("FrameXr Light Gun: the ground box and the Turn Zone", "[frame_xr][kiugyro]") {
    Session s;
    s.game.ground = true;
    float over = 0.0f;
    // Inside the box: under the pointer.
    REQUIRE(s.Step(0.45f, 0.45f, over, 4.0) > 0);
    REQUIRE(s.Step(0.6f, 0.55f, over, 4.0) > 0);
    SECTION("past the box but outside the zone: at the box's edge, no turn") {
        s.game.turned_x = 0.0;
        s.target = {true, 0.8f, 0.5f};
        s.Run(1.0);
        CHECK(s.game.seen_x == Approx(0.65f * s.game.w).margin(1.0f));
        CHECK(std::fabs(s.game.turned_x) < 2.0);
    }
    SECTION("in the zone: turns, faster deeper, and stops when the pointer comes back") {
        s.game.turned_x = 0.0;
        s.target = {true, 0.95f, 0.5f}; // half way into a 10 % zone
        s.Run(1.0);
        const double half = s.game.turned_x;
        CHECK(half == Approx(0.5 * s.game.w).epsilon(0.25)); // half a screen width a second
        s.game.turned_x = 0.0;
        s.target = {true, 1.1f, 0.5f}; // past the edge: full depth
        s.Run(1.0);
        CHECK(s.game.turned_x == Approx(2.0 * half).epsilon(0.2));
        s.speed = 2.0f;
        s.game.turned_x = 0.0;
        s.Run(1.0);
        CHECK(s.game.turned_x == Approx(4.0 * half).epsilon(0.2));
        s.target = {true, 0.5f, 0.5f};
        s.Run(0.2);
        s.game.turned_x = 0.0;
        s.Run(1.0);
        CHECK(std::fabs(s.game.turned_x) < 1.0);
        CHECK(s.game.seen_x == Approx(0.5f * s.game.w).margin(1.0f));
    }
    SECTION("left and up turn the other way") {
        s.game.turned_x = s.game.turned_y = 0.0;
        s.target = {true, -0.2f, -0.2f};
        s.Run(0.5);
        CHECK(s.game.turned_x < -50.0);
        CHECK(s.game.turned_y < -20.0);
    }
    SECTION("in the air the zone pushes nothing past the edge") {
        s.game.ground = false;
        s.target = {true, 1.2f, 0.5f};
        s.Run(1.0);
        CHECK(s.game.seen_x == Approx(s.game.w).margin(1.0f));
        CHECK(std::fabs(s.game.turned_x) < 2.0);
    }
}

TEST_CASE("FrameXr Light Gun: the grip holds the reticle, letting go converges without a jump",
          "[frame_xr][kiugyro]") {
    Session s;
    s.game.ground = false;
    float over = 0.0f;
    REQUIRE(s.Step(0.3f, 0.4f, over, 4.0) > 0);
    s.clutch = true;
    const float x = s.game.seen_x;
    s.target = {true, 0.8f, 0.6f};
    s.Run(0.5);
    CHECK(s.game.seen_x == x);
    s.clutch = false;
    // Let go: it moves to the pointer over a few frames, never further than it.
    REQUIRE(s.Step(0.8f, 0.6f, over, 2.0) > 0);
    CHECK(over <= 3.0f);
}

TEST_CASE("FrameXr Light Gun: no reading, no rate", "[frame_xr][kiugyro]") {
    LightGunLoop loop;
    ReticleReading bad;
    const auto out = loop.Update(1'000'000, bad, {true, 0.9f, 0.9f}, 0.1f, 1.0f, false);
    REQUIRE(out.right_dps == 0.0f);
    REQUIRE(out.down_dps == 0.0f);
}

// ---- The mod scan (kiu_scan.h) -------------------------------------------------------------

namespace {
void Put(std::vector<uint8_t>& b, std::size_t at, std::initializer_list<uint32_t> words) {
    for (uint32_t w : words) {
        std::memcpy(b.data() + at, &w, 4);
        at += 4;
    }
}
/// A data segment with the three v172 patterns at their places (base 0xD32000), and the
/// literals they load.
std::vector<uint8_t> FakeModCode() {
    constexpr uint32_t base = 0xD32000;
    std::vector<uint8_t> b(0x22D000, 0);
    // Options check at 0xD33978: ldr ip,[pc,#0x60] -> literal 0xD339E0 = 0xD636C4.
    Put(b, 0xD33978 - base, {0xE59FC060, 0xE59CC000, 0xE35C0000, 0x0A000006, 0xE28CCA16,
                             0xE59CC3E8, 0xE20CC60E, 0xE35C0602});
    Put(b, 0xD339E0 - base, {0x00D636C4});
    // State hook at 0xF45398: push; ldr r4,[pc,#0xAF4] -> 0xF45E98 = 0xF35D40; sub; ldr; cmp.
    Put(b, 0xF45398 - base, {0xE92D4070, 0xE59F4AF4, 0xE2405C01, 0xE594601C, 0xE1510006});
    Put(b, 0xF45E98 - base, {0x00F35D40});
    // The box hook's reads at +0xA4 and +0xA8 (0xF453E8).
    Put(b, 0xF453E8 - base, {0xE1A04000, 0xE1A05001, 0xE1A06002, 0xE1A07003, 0xE59D8020,
                             0xE59DC024, 0xE59490A4, 0xE594A0A8});
    return b;
}
} // namespace

TEST_CASE("FrameXr Light Gun: the mod's addresses are found by their code", "[frame_xr][kiugyro]") {
    using namespace FrameXr::Kiu;
    std::vector<uint8_t> b = FakeModCode();
    SECTION("found") {
        const ModAddresses a = ScanForMod(b.data(), b.size(), 0xD32000);
        REQUIRE(a.found);
        REQUIRE(a.state == 0xF35D40u);
        REQUIRE(a.options_ptr == 0xD636C4u);
    }
    SECTION("the game without the mod, or a build that changed: not found") {
        std::vector<uint8_t> none(b.size(), 0);
        REQUIRE(!ScanForMod(none.data(), none.size(), 0xD32000).found);
        b[0xF453E8 - 0xD32000 + 24] = 0xA0; // +0xA0 instead of +0xA4: another layout
        REQUIRE(!ScanForMod(b.data(), b.size(), 0xD32000).found);
    }
    SECTION("twice is ambiguous: not found") {
        std::memcpy(b.data() + 0x100, b.data() + (0xF45398 - 0xD32000), 20);
        REQUIRE(!ScanForMod(b.data(), b.size(), 0xD32000).found);
    }
    SECTION("the scheme bits and the title ids") {
        REQUIRE(SchemeOf(0x00200000u) == kSchemeGyro);
        REQUIRE(SchemeOf(0x00600000u) == 3u);
        REQUIRE(SchemeOf(0x001FFFFFu) == 0u);
        REQUIRE(IsKidIcarus(0x0004000000030200ull));
        REQUIRE(!IsKidIcarus(0x0004000000030600ull));
    }
}

// ---- Light Gun's laser sight (vis/beam.h) --------------------------------------------------

TEST_CASE("FrameXr Light Gun: the laser sight shows only while Light Gun has the reticle",
          "[frame_xr][kiugyro]") {
    const LightGunTarget on{true, 0.4f, 0.6f};
    SECTION("locked, on the top screen, in a game: red") {
        CHECK(LaserSight(GyroAimMode::LightGun, true, true, on));
        CHECK(LaserSight(GyroAimMode::LightGun, true, true, {true, 0.0f, 1.0f})); // its edge
    }
    SECTION("fallen back to Gyro (no reading): the usual beam") {
        CHECK(!LaserSight(GyroAimMode::LightGun, true, false, on));
    }
    SECTION("off the top screen, or pointing away: the usual beam") {
        CHECK(!LaserSight(GyroAimMode::LightGun, true, true, {true, 1.05f, 0.5f}));
        CHECK(!LaserSight(GyroAimMode::LightGun, true, true, {true, 0.5f, -0.1f}));
        CHECK(!LaserSight(GyroAimMode::LightGun, true, true, {false, 0.5f, 0.5f}));
    }
    SECTION("menus, Gyro, Off: the usual beam") {
        CHECK(!LaserSight(GyroAimMode::LightGun, false, true, on));
        CHECK(!LaserSight(GyroAimMode::Gyro, true, true, on));
        CHECK(!LaserSight(GyroAimMode::Off, true, true, on));
    }
}

TEST_CASE("FrameXr Light Gun: the laser sight's art", "[frame_xr][kiugyro]") {
    using namespace FrameXr::Vis;
    SECTION("the red beam is the white one's shape, in red") {
        const std::vector<uint8_t> white = BeamArt();
        const std::vector<uint8_t> red = BeamArt(kLaserRgb);
        REQUIRE(white.size() == red.size());
        bool some = false;
        for (std::size_t i = 0; i < red.size(); i += 4) {
            REQUIRE(red[i + 3] == white[i + 3]); // the same alpha everywhere
            REQUIRE(red[i] <= red[i + 3]);        // premultiplied
            REQUIRE(red[i + 1] <= red[i]);        // red over green and blue
            REQUIRE(red[i + 2] <= red[i]);
            some = some || red[i + 3] > 200;
        }
        REQUIRE(some);
    }
    SECTION("the dot: opaque red in the middle, clear at the corners, small") {
        const std::vector<uint8_t> dot = LaserDotArt();
        REQUIRE(dot.size() == static_cast<std::size_t>(kLaserDotPixels * kLaserDotPixels * 4));
        const auto at = [&](int x, int y) { return &dot[(y * kLaserDotPixels + x) * 4]; };
        const uint8_t* mid = at(kLaserDotPixels / 2, kLaserDotPixels / 2);
        CHECK(mid[3] == 255);
        CHECK(mid[0] == 255);
        const uint8_t* rim = at(kLaserDotPixels / 2, kLaserDotPixels / 2 - 13);
        CHECK(rim[0] > 2 * rim[1]); // red toward the rim
        CHECK(at(0, 0)[3] == 0);
        CHECK(at(kLaserDotPixels - 1, kLaserDotPixels - 1)[3] == 0);
        // Half the pointer dot's size: about 1 cm at 3 m, a few game pixels on a 1.6 m screen.
        CHECK(LaserDotSize(3.0f) == Approx(0.0095f));
        CHECK(LaserDotSize(1.0f) < 0.01f + 0.003f * 1.0f);
    }
}
