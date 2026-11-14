// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctrlglitch: the guard on the controllers' drawn poses (frame_xr/input/pose_guard.h):
// every movement a hand can make passes untouched (no lag), a spike of a refresh or two is never
// shown, a real move to a new place is eased, and the pose a refresh ahead rides on the guarded one.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/pose_guard.h"

using Catch::Approx;
using namespace FrameXr::Input;

namespace {

constexpr int64_t kPeriod = 8'333'333; // 120 Hz
constexpr uint8_t kAllTracked =
    kTrackPositionValid | kTrackOrientationValid | kTrackPositionTracked | kTrackOrientationTracked;

Quatf AxisAngle(Vec3f axis, float angle) {
    axis = Normalized(axis);
    const float s = std::sin(angle * 0.5f);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(angle * 0.5f)};
}

float Distance(const Posef& a, const Posef& b) {
    return Length(a.position - b.position);
}

/// A hand's path: circling, flicking and turning the wrist, all at once (seconds in).
Posef Path(double t) {
    const float ft = static_cast<float>(t);
    // A 0.3 m circle at 1.5 turns a second (2.8 m/s), plus a flick out and back of 0.2 m at
    // 31.6 rad/s (6.3 m/s at its fastest, 200 m/s^2), plus the wrist turning +-90 degrees at 3 Hz.
    const float a = 2.0f * 3.14159265f * 1.5f * ft;
    const Vec3f circle{0.3f * std::cos(a), 1.1f + 0.3f * std::sin(a), -0.4f};
    const Vec3f flick{0.2f * (1.0f - std::cos(31.6f * ft)), 0.0f, 0.0f};
    const Quatf wrist = AxisAngle({0.2f, 1.0f, 0.1f}, 1.5708f * std::sin(2.0f * 3.14159265f * 3.0f * ft));
    return {wrist, circle + flick};
}

PoseGuard::Out Step(PoseGuard& g, int64_t t, const Posef& raw, bool active = true,
                    uint8_t track = kAllTracked) {
    return g.Step(t, active, track, raw, raw, false);
}

} // namespace

TEST_CASE("Pose guard: a hand's real movements pass untouched", "[frame_xr][pose_guard]") {
    PoseGuard g;
    std::mt19937 rng{7};
    std::uniform_real_distribution<float> noise(-0.002f, 0.002f); // tracking noise, 2 mm
    std::uniform_real_distribution<float> turn_noise(-0.009f, 0.009f); // half a degree
    std::bernoulli_distribution missed(0.05);                          // a missed refresh
    int64_t t = 1'000'000'000;
    for (int i = 0; i < 3000; i++) {
        t += missed(rng) ? 2 * kPeriod : kPeriod;
        Posef raw = Path(static_cast<double>(t) * 1e-9);
        raw.position = raw.position + Vec3f{noise(rng), noise(rng), noise(rng)};
        raw.orientation = Normalized(
            Multiply(AxisAngle({turn_noise(rng), 1.0f, turn_noise(rng)}, turn_noise(rng)),
                     raw.orientation));
        const PoseGuard::Out out = Step(g, t, raw);
        REQUIRE(out.active);
        REQUIRE_FALSE(out.held);
        // Exactly the pose reported: no lag, no smoothing.
        REQUIRE(Distance(out.pose, raw) < 1e-6f);
        REQUIRE(QuatAngle(out.pose.orientation, raw.orientation) < 1e-3f);
    }
    CHECK(g.Stats().holds == 0);
    CHECK(g.Stats().jumps == 0);
    CHECK(g.Stats().tracked == 3000);
}

TEST_CASE("Pose guard: a spike of one or two refreshes is never shown", "[frame_xr][pose_guard]") {
    for (const int length : {1, 2}) {
        for (const bool turn : {false, true}) {
            INFO("spike of " << length << " refreshes, " << (turn ? "a turn" : "a step"));
            PoseGuard g;
            int64_t t = 0;
            for (int i = 0; i < 60; i++) {
                t += kPeriod;
                Step(g, t, Path(t * 1e-9));
            }
            for (int i = 0; i < length; i++) {
                t += kPeriod;
                const Posef truth = Path(t * 1e-9);
                Posef spike = truth;
                if (turn) {
                    spike.orientation =
                        Normalized(Multiply(AxisAngle({0, 1, 0}, 0.6f), truth.orientation));
                } else {
                    spike.position = truth.position + Vec3f{0.10f, -0.03f, 0.02f};
                }
                const PoseGuard::Out out = Step(g, t, spike);
                CHECK(out.held);
                // Where the hand was heading: near the truth, nowhere near the spike.
                if (!turn) {
                    CHECK(Distance(out.pose, truth) < 0.5f * Distance(spike, truth));
                }
                CHECK(QuatAngle(out.pose.orientation, truth.orientation) <
                      0.5f * std::max(QuatAngle(spike.orientation, truth.orientation), 0.3f));
                CHECK(out.ahead.position.x == Approx(out.pose.position.x));
            }
            t += kPeriod;
            const Posef back = Path(t * 1e-9);
            const PoseGuard::Out out = Step(g, t, back);
            CHECK_FALSE(out.held);
            CHECK(Distance(out.pose, back) < 1e-6f);
            CHECK(g.Stats().holds == 1);
            CHECK(g.Stats().dropped == 1);
            CHECK(g.Stats().jumps == 0);
            CHECK(g.Stats().held_frames == static_cast<uint32_t>(length));
        }
    }
}

TEST_CASE("Pose guard: a real move to a new place is taken, eased", "[frame_xr][pose_guard]") {
    PoseGuard g;
    int64_t t = 0;
    const Posef still{AxisAngle({1, 0, 0}, -0.3f), {0.1f, 1.0f, -0.35f}};
    Posef shown = still;
    for (int i = 0; i < 30; i++) {
        t += kPeriod;
        shown = Step(g, t, still).pose;
    }
    // Found again 15 cm to the left and turned: the tracking's own correction.
    const Posef moved{Normalized(Multiply(AxisAngle({0, 0, 1}, 0.4f), still.orientation)),
                      still.position + Vec3f{-0.15f, 0.0f, 0.0f}};
    int held = 0;
    float max_step = 0.0f;
    bool reached = false;
    for (int i = 0; i < 30; i++) {
        t += kPeriod;
        const PoseGuard::Out out = Step(g, t, moved);
        held += out.held ? 1 : 0;
        max_step = std::max(max_step, Distance(out.pose, shown));
        shown = out.pose;
        if (!reached && Distance(out.pose, moved) < 0.002f &&
            QuatAngle(out.pose.orientation, moved.orientation) < 0.01f) {
            reached = true;
            // Within about 3 time constants of the place being taken.
            CHECK(i <= PoseGuard::kConfirm + 16);
        }
    }
    CHECK(held == PoseGuard::kConfirm - 1); // the third agreeing pose takes it
    CHECK(reached);
    CHECK(max_step < 0.15f * 0.35f); // never in one step
    CHECK(g.Stats().jumps == 1);
    CHECK(g.Stats().dropped == 0);
}

TEST_CASE("Pose guard: found again after a loss", "[frame_xr][pose_guard]") {
    const Posef before{{}, {0.2f, 1.0f, -0.4f}};
    const Posef after{{}, {-0.1f, 0.9f, -0.3f}};
    SECTION("a short loss: taken at once, eased from where it was shown") {
        PoseGuard g;
        int64_t t = 0;
        for (int i = 0; i < 10; i++) {
            Step(g, t += kPeriod, before);
        }
        for (int i = 0; i < 10; i++) {
            const PoseGuard::Out out = Step(g, t += kPeriod, before, false, 0);
            CHECK_FALSE(out.active);
        }
        const PoseGuard::Out out = Step(g, t += kPeriod, after);
        CHECK_FALSE(out.held);
        CHECK(Distance(out.pose, before) < 1e-5f); // continues from the frozen place
        PoseGuard::Out last{};
        for (int i = 0; i < 20; i++) {
            last = Step(g, t += kPeriod, after);
        }
        CHECK(Distance(last.pose, after) < 0.002f);
        CHECK(g.Stats().lost == 10);
        CHECK(g.Stats().jumps == 1);
        CHECK(g.Stats().holds == 0);
    }
    SECTION("a long loss: the controller was hidden; taken as it is") {
        PoseGuard g;
        int64_t t = 0;
        for (int i = 0; i < 10; i++) {
            Step(g, t += kPeriod, before);
        }
        t += PoseGuard::kForgetNs + kPeriod;
        Step(g, t, before, false, 0);
        const PoseGuard::Out out = Step(g, t += kPeriod, after);
        CHECK(Distance(out.pose, after) < 1e-6f);
        CHECK(g.Stats().jumps == 0);
    }
}

TEST_CASE("Pose guard: the pose a refresh ahead rides on the guarded one",
          "[frame_xr][pose_guard]") {
    PoseGuard g;
    int64_t t = 0;
    const Posef raw{{}, {0.0f, 1.0f, -0.4f}};
    g.Step(t += kPeriod, true, kAllTracked, raw, raw, false);
    // SteamVR's own prediction a refresh on: 2 cm and 5 degrees further.
    const Posef ahead{AxisAngle({0, 1, 0}, 0.087f), raw.position + Vec3f{0.02f, 0.0f, 0.0f}};
    PoseGuard::Out out = g.Step(t += kPeriod, true, kAllTracked, raw, ahead, true);
    CHECK(out.ahead.position.x == Approx(0.02f).margin(1e-5));
    CHECK(QuatAngle(out.ahead.orientation, ahead.orientation) < 1e-3f);
    // Not reported: the guarded pose itself.
    out = g.Step(t += kPeriod, true, kAllTracked, raw, ahead, false);
    CHECK(Distance(out.ahead, out.pose) < 1e-6f);
    // A wild prediction is clamped.
    const Posef wild{AxisAngle({0, 1, 0}, 2.5f), raw.position + Vec3f{0.0f, 0.5f, 0.0f}};
    out = g.Step(t += kPeriod, true, kAllTracked, raw, wild, true);
    CHECK(Distance(out.ahead, out.pose) == Approx(PoseGuard::kMaxAheadM).margin(1e-4));
    CHECK(QuatAngle(out.ahead.orientation, out.pose.orientation) ==
          Approx(PoseGuard::kMaxAheadRad).margin(1e-3));
}

TEST_CASE("Pose guard: the counts for the log", "[frame_xr][pose_guard]") {
    PoseGuard g;
    int64_t t = 0;
    const Posef raw{{}, {0.0f, 1.0f, -0.4f}};
    Step(g, t += kPeriod, raw);
    Step(g, t += kPeriod, raw, true, kTrackPositionValid | kTrackOrientationValid);
    Step(g, t += kPeriod, raw, true, kTrackPositionValid | kTrackOrientationValid |
                                         kTrackOrientationTracked);
    Step(g, t += kPeriod, raw, false, 0);
    // 1.5 cm off a still course.
    Step(g, t += kPeriod, raw);
    Step(g, t += kPeriod, raw);
    Step(g, t += kPeriod, {{}, raw.position + Vec3f{0.015f, 0.0f, 0.0f}});
    const PoseGuardStats& s = g.Stats();
    CHECK(s.frames == 7);
    CHECK(s.tracked == 4);
    CHECK(s.inferred == 2);
    CHECK(s.lost == 1);
    CHECK(s.off_cm[2] == 1);
    CHECK(s.off_max_m == Approx(0.015f).margin(1e-4));
    g.ResetStats();
    CHECK(g.Stats().frames == 0);
}
