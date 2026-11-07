// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/motion.h"

// AzaharFrame
namespace FrameXr::Input {

namespace {
constexpr float kRadToDeg = 57.29577951308232f;
}

Vec3f GyroFromOrientations(const Quatf& from, const Quatf& to, float dt_s) {
    if (!(dt_s > 0.0f)) {
        return {};
    }
    // The turn in the controller's own axes: to = from * delta.
    Quatf delta = Multiply(Conjugate(Normalized(from)), Normalized(to));
    if (delta.w < 0.0f) { // take the short way round
        delta = {-delta.x, -delta.y, -delta.z, -delta.w};
    }
    const float sin_half = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (sin_half < 1e-7f) {
        return {};
    }
    const float angle = 2.0f * std::atan2(sin_half, delta.w);
    const float scale = angle / sin_half / dt_s * kRadToDeg;
    return ControllerToThreeDs(Vec3f{delta.x * scale, delta.y * scale, delta.z * scale});
}

Vec3f AccelFromOrientation(const Quatf& orientation) {
    // World down, seen from the controller, then in the 3DS axes.
    const Vec3f down_in_controller = Rotate(Conjugate(Normalized(orientation)), Vec3f{0, -1, 0});
    return ControllerToThreeDs(down_in_controller);
}

MotionSample MotionTracker::Update(const HandInput& hand, int64_t time_ns, bool enabled) {
    if (!enabled || !hand.active) {
        have_last_ = false;
        last_sample_ = kMotionRest;
        return last_sample_;
    }
    const Quatf orientation = Normalized(hand.aim.orientation);
    MotionSample sample;
    sample.accel = AccelFromOrientation(orientation);
    if (have_last_ && time_ns > last_time_ns_ && time_ns - last_time_ns_ <= config_.max_gap_ns) {
        const float dt_s = static_cast<float>(time_ns - last_time_ns_) * 1e-9f;
        Vec3f gyro = GyroFromOrientations(last_orientation_, orientation, dt_s);
        const float speed = Length(gyro);
        if (speed < config_.gyro_deadzone_dps) {
            gyro = {};
        } else if (speed > config_.gyro_max_dps) {
            gyro = gyro * (config_.gyro_max_dps / speed);
        }
        sample.gyro = gyro;
    } else if (have_last_ && time_ns == last_time_ns_) {
        // The same display time twice: nothing new to say.
        return last_sample_;
    }
    have_last_ = true;
    last_orientation_ = orientation;
    last_time_ns_ = time_ns;
    last_sample_ = sample;
    return sample;
}

} // namespace FrameXr::Input
