// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/motion.h"

// AzaharFrame
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

HandInput MakeHand(const Quatf& orientation) {
    HandInput hand;
    hand.active = true;
    hand.aim.orientation = orientation;
    return hand;
}

constexpr int64_t kStepNs = 11'111'111; // 90 Hz

} // namespace

TEST_CASE("FrameXr motion: gravity in the 3DS axes", "[frame_xr]") {
    SECTION("the ray pointing level, upright, is a 3DS held up facing you: gravity is -z") {
        const Vec3f a = AccelFromOrientation(Quatf{});
        REQUIRE(a.x == Approx(0.0f).margin(1e-5f));
        REQUIRE(a.y == Approx(0.0f).margin(1e-5f));
        REQUIRE(a.z == Approx(-1.0f));
    }
    SECTION("the ray pointing straight down is a 3DS lying flat, screen up: (0, -1, 0)") {
        const Vec3f a = AccelFromOrientation(AxisAngle({1, 0, 0}, -90.0f));
        REQUIRE(a.x == Approx(0.0f).margin(1e-5f));
        REQUIRE(a.y == Approx(-1.0f));
        REQUIRE(a.z == Approx(0.0f).margin(1e-5f));
    }
    SECTION("the ray pointing straight up is a 3DS lying flat, screen down: (0, 1, 0)") {
        const Vec3f a = AccelFromOrientation(AxisAngle({1, 0, 0}, 90.0f));
        REQUIRE(a.y == Approx(1.0f));
    }
    SECTION("rolled 90 degrees right (clockwise seen from behind): gravity is toward 3DS x+") {
        // Rolling the right side down puts gravity on the controller's +x, which is the 3DS's -x
        // (3DS x points to its left).
        const Vec3f a = AccelFromOrientation(AxisAngle({0, 0, -1}, 90.0f));
        REQUIRE(a.x == Approx(-1.0f));
        REQUIRE(a.y == Approx(0.0f).margin(1e-5f));
        REQUIRE(a.z == Approx(0.0f).margin(1e-5f));
    }
    SECTION("always unit length") {
        const Vec3f a = AccelFromOrientation(Normalized(Quatf{0.3f, -0.5f, 0.2f, 0.8f}));
        REQUIRE(Length(a) == Approx(1.0f));
    }
}

TEST_CASE("FrameXr motion: angular rate in the 3DS axes", "[frame_xr]") {
    const Quatf start{};

    SECTION("no time, no rate") {
        REQUIRE(Length(GyroFromOrientations(start, AxisAngle({0, 1, 0}, 10.0f), 0.0f)) == 0.0f);
    }
    SECTION("no turn, no rate") {
        REQUIRE(Length(GyroFromOrientations(start, start, 0.01f)) == 0.0f);
    }
    SECTION("turning the ray left turns the 3DS about its z axis (the way up) the same way") {
        const Vec3f g = GyroFromOrientations(start, AxisAngle({0, 1, 0}, 9.0f), 0.1f);
        REQUIRE(g.x == Approx(0.0f).margin(1e-3f));
        REQUIRE(g.y == Approx(0.0f).margin(1e-3f));
        REQUIRE(g.z == Approx(90.0f));
    }
    SECTION("tilting the ray up turns the 3DS about x, negative") {
        const Vec3f g = GyroFromOrientations(start, AxisAngle({1, 0, 0}, 6.0f), 0.1f);
        REQUIRE(g.x == Approx(-60.0f));
        REQUIRE(g.y == Approx(0.0f).margin(1e-3f));
        REQUIRE(g.z == Approx(0.0f).margin(1e-3f));
    }
    SECTION("rolling the controller about its ray turns the 3DS about its screen normal") {
        // Controller +z is the 3DS's +y; a turn about +z is a turn about 3DS y.
        const Vec3f g = GyroFromOrientations(start, AxisAngle({0, 0, 1}, 4.5f), 0.1f);
        REQUIRE(g.y == Approx(45.0f));
        REQUIRE(g.x == Approx(0.0f).margin(1e-3f));
        REQUIRE(g.z == Approx(0.0f).margin(1e-3f));
    }
    SECTION("the rate is in the controller's own axes, not the world's") {
        // Ray pointing left (yawed 90 degrees), then rolled about the ray: still 3DS y.
        const Quatf yawed = AxisAngle({0, 1, 0}, 90.0f);
        const Quatf rolled = Multiply(yawed, AxisAngle({0, 0, 1}, 3.0f));
        const Vec3f g = GyroFromOrientations(yawed, rolled, 0.1f);
        REQUIRE(g.y == Approx(30.0f));
        REQUIRE(g.x == Approx(0.0f).margin(1e-3f));
        REQUIRE(g.z == Approx(0.0f).margin(1e-3f));
    }
    SECTION("the short way round, and a negative double cover quaternion, give the same rate") {
        const Quatf to = AxisAngle({0, 1, 0}, 9.0f);
        const Quatf negated{-to.x, -to.y, -to.z, -to.w};
        const Vec3f a = GyroFromOrientations(start, to, 0.1f);
        const Vec3f b = GyroFromOrientations(start, negated, 0.1f);
        REQUIRE(b.z == Approx(a.z));
    }
    SECTION("gyro and accelerometer agree: gravity turns by -w x g in the 3DS's own axes") {
        // The same convention test one would run on a real sensor: a body turning at w sees a
        // fixed world vector rotate as d(g)/dt = -w x g.
        const Quatf q1 = Normalized(Quatf{0.2f, 0.4f, -0.3f, 0.8f});
        const Quatf q2 = Multiply(q1, AxisAngle({0.3f, 0.5f, -0.7f}, 0.5f));
        const float dt = 0.01f;
        const Vec3f g1 = AccelFromOrientation(q1);
        const Vec3f g2 = AccelFromOrientation(q2);
        const Vec3f w = GyroFromOrientations(q1, q2, dt) * kDeg; // rad/s
        const Vec3f expected = Cross(w, g1) * (-dt);
        const Vec3f actual = g2 - g1;
        REQUIRE(actual.x == Approx(expected.x).margin(2e-4f));
        REQUIRE(actual.y == Approx(expected.y).margin(2e-4f));
        REQUIRE(actual.z == Approx(expected.z).margin(2e-4f));
    }
}

TEST_CASE("FrameXr motion: tracker", "[frame_xr]") {
    MotionTracker tracker;

    SECTION("off, or an untracked controller, reads as a 3DS lying flat and still") {
        HandInput lost = MakeHand(AxisAngle({0, 1, 0}, 30.0f));
        lost.active = false;
        for (const MotionSample& s : {tracker.Update(MakeHand(Quatf{}), 0, false),
                                      tracker.Update(lost, kStepNs, true)}) {
            REQUIRE(s.accel.y == Approx(-1.0f));
            REQUIRE(s.accel.x == 0.0f);
            REQUIRE(s.accel.z == 0.0f);
            REQUIRE(Length(s.gyro) == 0.0f);
        }
    }
    SECTION("the first frame has no rate, the next ones do") {
        MotionSample s = tracker.Update(MakeHand(Quatf{}), 0, true);
        REQUIRE(Length(s.gyro) == 0.0f);
        // 1 degree per frame at 90 Hz is 90 deg/s.
        s = tracker.Update(MakeHand(AxisAngle({0, 1, 0}, 1.0f)), 1 * kStepNs, true);
        REQUIRE(s.gyro.z == Approx(90.0f).epsilon(0.01));
        s = tracker.Update(MakeHand(AxisAngle({0, 1, 0}, 2.0f)), 2 * kStepNs, true);
        REQUIRE(s.gyro.z == Approx(90.0f).epsilon(0.01));
    }
    SECTION("a standing still hand reads zero, whatever its tremor below the dead zone") {
        tracker.Update(MakeHand(Quatf{}), 0, true);
        // 0.01 degrees per frame is about 0.9 deg/s.
        const MotionSample s = tracker.Update(MakeHand(AxisAngle({0, 1, 0}, 0.01f)), kStepNs, true);
        REQUIRE(Length(s.gyro) == 0.0f);
    }
    SECTION("the rate is capped") {
        tracker.Update(MakeHand(Quatf{}), 0, true);
        const MotionSample s = tracker.Update(MakeHand(AxisAngle({0, 1, 0}, 90.0f)), kStepNs, true);
        REQUIRE(Length(s.gyro) == Approx(2000.0f));
    }
    SECTION("a long gap restarts the tracking instead of making a huge rate") {
        tracker.Update(MakeHand(Quatf{}), 0, true);
        const MotionSample s =
            tracker.Update(MakeHand(AxisAngle({0, 1, 0}, 90.0f)), 500'000'000, true);
        REQUIRE(Length(s.gyro) == 0.0f);
        const MotionSample next =
            tracker.Update(MakeHand(AxisAngle({0, 1, 0}, 91.0f)), 500'000'000 + kStepNs, true);
        REQUIRE(next.gyro.z == Approx(90.0f).epsilon(0.01));
    }
    SECTION("the same display time twice repeats the last sample") {
        tracker.Update(MakeHand(Quatf{}), 0, true);
        const MotionSample a = tracker.Update(MakeHand(AxisAngle({0, 1, 0}, 1.0f)), kStepNs, true);
        const MotionSample b = tracker.Update(MakeHand(AxisAngle({0, 1, 0}, 1.0f)), kStepNs, true);
        REQUIRE(b.gyro.z == Approx(a.gyro.z));
    }
    SECTION("turning it off and on again forgets the pose") {
        tracker.Update(MakeHand(Quatf{}), 0, true);
        tracker.Update(MakeHand(Quatf{}), kStepNs, false);
        const MotionSample s =
            tracker.Update(MakeHand(AxisAngle({0, 1, 0}, 40.0f)), 2 * kStepNs, true);
        REQUIRE(Length(s.gyro) == 0.0f);
    }
}

TEST_CASE("FrameXr motion: the headset as the motion source", "[frame_xr]") {
    InputFrame frame;
    frame.head_active = true;

    SECTION("looking straight ahead, level, is a 3DS held upright in front of you") {
        const MotionSample s = MotionTracker{}.Update(HeadAsMotionSource(frame), 0, true);
        REQUIRE(s.accel.x == Approx(0.0f).margin(1e-5f));
        REQUIRE(s.accel.y == Approx(0.0f).margin(1e-5f));
        REQUIRE(s.accel.z == Approx(-1.0f));
    }
    SECTION("the head pose gives the same numbers as a controller with that aim pose") {
        frame.head.orientation = AxisAngle({1, 0, 0}, -90.0f); // looking straight down
        const MotionSample s = MotionTracker{}.Update(HeadAsMotionSource(frame), 0, true);
        REQUIRE(s.accel.y == Approx(-1.0f)); // a 3DS lying flat
        REQUIRE(s.accel.z == Approx(0.0f).margin(1e-5f));
    }
    SECTION("turning the head left reads as the 3DS turning about its vertical axis") {
        MotionTracker tracker;
        frame.head.orientation = Quatf{};
        tracker.Update(HeadAsMotionSource(frame), 0, true);
        frame.head.orientation = AxisAngle({0, 1, 0}, 1.0f); // 1 degree per frame at 90 Hz
        const MotionSample s = tracker.Update(HeadAsMotionSource(frame), kStepNs, true);
        REQUIRE(s.gyro.z == Approx(90.0f).epsilon(0.01));
    }
    SECTION("a head that is not located reads as the rest state") {
        frame.head_active = false;
        frame.head.orientation = AxisAngle({0, 1, 0}, 30.0f);
        const MotionSample s = MotionTracker{}.Update(HeadAsMotionSource(frame), 0, true);
        REQUIRE(s.accel.y == Approx(-1.0f));
        REQUIRE(Length(s.gyro) == 0.0f);
    }
}
