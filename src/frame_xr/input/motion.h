// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include "frame_xr/input/input_frame.h"
#include "frame_xr/input/types.h"

// AzaharFrame
// A Steam Frame controller as the 3DS's gyroscope and accelerometer. Pure math (no OpenXR, Qt or
// emulator calls), so it is unit tested. The numbers go to the "frame_xr" motion device
// (input_common/frame_xr.h), which the HID service reads.
//
// The 3DS axes are the ones Azahar's motion devices use (see input_common/motion_emu.cpp and the
// Android sensor mapping in android/.../ndk_motion.cpp): x is the 3DS's left, y comes out of the
// screen, z points to the top of the 3DS (the hinge), right handed. The accelerometer reads the
// direction of gravity in those axes, in g: a 3DS lying flat, screen up, reads (0, -1, 0).
// The gyroscope reads the angular rate about those axes in degrees per second.
//
// The controller is held like a window: its pointing ray (-Z of the aim pose) goes into the
// screen, so the ray points where the 3DS's back would face. Controller axes (x right, y up, z
// toward the player when the ray points away) become 3DS axes with
//     3DS x = -controller x,  3DS y = controller z,  3DS z = controller y.
// Turning the ray left turns the 3DS about its vertical (z) axis, tilting it up turns it about x,
// as when you move a 3DS held in front of you.
namespace FrameXr::Input {

struct MotionSample {
    Vec3f accel{0.0f, -1.0f, 0.0f}; ///< gravity direction in the 3DS axes, in g
    Vec3f gyro;                     ///< angular rate in the 3DS axes, in degrees per second
};

/// The rest state: the 3DS flat on a table.
constexpr MotionSample kMotionRest{};

/// A vector in the controller's axes (aim pose) as seen in the 3DS axes (see above).
constexpr Vec3f ControllerToThreeDs(const Vec3f& v) {
    return {-v.x, v.z, v.y};
}

/// The angular rate that took the controller from orientation `from` to `to` in `dt_s` seconds,
/// as the gyroscope of a 3DS fixed to the controller would read it (degrees per second, 3DS axes).
/// Zero when dt_s <= 0.
Vec3f GyroFromOrientations(const Quatf& from, const Quatf& to, float dt_s);

/// Gravity (world -Y, the base space is gravity aligned) in the 3DS axes for a controller with
/// this orientation, in g. Unit length.
Vec3f AccelFromOrientation(const Quatf& orientation);

struct MotionConfig {
    /// Rates below this magnitude read as zero (hand tremor and pose noise).
    float gyro_deadzone_dps = 1.5f;
    /// The 3DS's gyroscope saturates at about +-2279 deg/s (s16 / 14.375).
    float gyro_max_dps = 2000.0f;
    /// A gap longer than this between two frames restarts the tracking instead of making a rate.
    int64_t max_gap_ns = 100'000'000;
};

/// Turns the poses of one hand, frame after frame, into motion samples. XR thread only.
class MotionTracker {
public:
    explicit MotionTracker(const MotionConfig& config = {}) : config_(config) {}

    /// `hand` is the controller that drives the motion; `enabled` false (or the controller not
    /// tracked) gives the rest state with the gyroscope still, and forgets the last pose.
    MotionSample Update(const HandInput& hand, int64_t time_ns, bool enabled);

    void Reset() {
        have_last_ = false;
    }

private:
    MotionConfig config_;
    bool have_last_ = false;
    Quatf last_orientation_;
    int64_t last_time_ns_ = 0;
    MotionSample last_sample_;
};

} // namespace FrameXr::Input
