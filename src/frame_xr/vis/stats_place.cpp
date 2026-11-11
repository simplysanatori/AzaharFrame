// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <limits>

#include "frame_xr/vis/stats_place.h"

// AzaharFrame statspos
namespace FrameXr::Vis {

using Input::Posef;
using Input::Quatf;
using Input::Vec3f;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;
/// A held panel's middle stays this far from straight up or down.
constexpr float kMaxGrabPitch = 80.0f * kDeg;

float WrapAngle(float a) {
    while (a > kPi) {
        a -= 2.0f * kPi;
    }
    while (a < -kPi) {
        a += 2.0f * kPi;
    }
    return a;
}

} // namespace

float YawOf(const Vec3f& dir) {
    return std::atan2(-dir.x, -dir.z);
}

float PitchOf(const Vec3f& dir) {
    const float len = Input::Length(dir);
    return len > 0.0f ? std::asin(std::clamp(dir.y / len, -1.0f, 1.0f)) : 0.0f;
}

Vec3f DirectionFrom(float yaw, float pitch) {
    const float c = std::cos(pitch);
    return {-std::sin(yaw) * c, std::sin(pitch), -std::cos(yaw) * c};
}

Quatf StatsFacing(const Vec3f& centre, const Vec3f& eye) {
    const Vec3f v = eye - centre;
    const float a = (std::abs(v.x) + std::abs(v.z)) > 1e-6f ? std::atan2(v.x, v.z) : 0.0f;
    return {0.0f, std::sin(a * 0.5f), 0.0f, std::cos(a * 0.5f)};
}

Posef StatsPoseAt(const Vec3f& eye, const Vec3f& dir, float distance) {
    const Vec3f centre = eye + dir * distance;
    return {StatsFacing(centre, eye), centre};
}

float StatsClearDistance(const Vec3f& eye, const Vec3f& dir, float wanted, float w, float h,
                         const std::vector<Input::PanelQuad>& obstacles) {
    float limit = std::clamp(wanted, kStatsMinDistance, kStatsMaxDistance);
    // Twice: a panel moved nearer covers a wider angle, which may take in another screen.
    for (int pass = 0; pass < 2; pass++) {
        const Posef pose = StatsPoseAt(eye, dir, limit);
        const Vec3f right = Input::Rotate(pose.orientation, {1, 0, 0});
        const Vec3f up = Input::Rotate(pose.orientation, {0, 1, 0});
        float nearest = std::numeric_limits<float>::infinity();
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                const Vec3f p = pose.position + right * (0.5f * w * static_cast<float>(i)) +
                                up * (0.5f * h * static_cast<float>(j));
                const Input::Ray ray{eye, Input::Normalized(p - eye)};
                for (const Input::PanelQuad& q : obstacles) {
                    if (!q.visible) {
                        continue;
                    }
                    const Input::QuadHit hit = Input::IntersectQuad(ray, q);
                    if (hit.in_bounds) {
                        nearest = std::min(nearest, hit.t * Input::Dot(ray.direction, dir));
                    }
                }
            }
        }
        if (!(nearest - kStatsScreenGap < limit)) {
            break; // nothing nearer: the distance stands
        }
        limit = std::clamp(nearest - kStatsScreenGap, kStatsMinDistance, kStatsMaxDistance);
    }
    return limit;
}

Vec3f StatsFrontDirection(const Posef& head) {
    const Vec3f look = Input::Rotate(head.orientation, {0, 0, -1});
    const float pitch = std::clamp(PitchOf(look), -kStatsFrontMaxPitchDeg * kDeg,
                                   kStatsFrontMaxPitchDeg * kDeg);
    return DirectionFrom(YawOf(look), pitch - kStatsFrontBelowDeg * kDeg);
}

void StatsGrab::Begin(const Vec3f& eye, const Posef& panel, const Vec3f& hit) {
    const Vec3f to_centre = panel.position - eye;
    const Vec3f to_hit = hit - eye;
    yaw_offset = WrapAngle(YawOf(to_centre) - YawOf(to_hit));
    pitch_offset = PitchOf(to_centre) - PitchOf(to_hit);
}

Vec3f StatsGrab::Direction(const Vec3f& eye, const Input::Ray& ray, float distance) const {
    // Where the beam meets the sphere round the eyes the panel is on (the far side: the hand is
    // inside it), else the point that far along the beam.
    const Vec3f m = ray.origin - eye;
    const float b = Input::Dot(m, ray.direction);
    const float c = Input::Dot(m, m) - distance * distance;
    const float disc = b * b - c;
    float t = distance;
    if (disc >= 0.0f) {
        const float far = -b + std::sqrt(disc);
        if (far > 0.0f) {
            t = far;
        }
    }
    const Vec3f point = ray.origin + ray.direction * t;
    const Vec3f to = point - eye;
    const float pitch = std::clamp(PitchOf(to) + pitch_offset, -kMaxGrabPitch, kMaxGrabPitch);
    return DirectionFrom(YawOf(to) + yaw_offset, pitch);
}

// ---- On Wrist --------------------------------------------------------------------------------
//
// From the Frame controller model (frame_controller_left.model, in the aim space: -Z where the
// beam goes, +Y the top with the stick and D-pad, +X the inner side of the left controller, where
// the palm is): the handle runs back and down from the head at about 37 degrees, along
// (0, -0.6, 0.8), and ends at z 0.117, its end's middle at x -0.0135, y -0.058, its inner face at
// x -0.001. The wrist is past that end. The panel lies along the forearm on the inner side of the
// wrist: its +X (the way the text reads) toward the hand, its +Y toward the controller's top, its
// middle 4.5 cm past the handle's end along the handle's line and 3.5 cm inward of its middle
// (2.3 cm clear of the controller), its front facing inward, turned 10 degrees up. Held to play
// (pointing ahead, below the head), it faces away from the eyes (about 50 degrees off); turn the
// palm toward your face, as to read the inside of a watch, and it reads level.

Posef WristLocalPose() {
    const Vec3f along = Input::Normalized(Vec3f{0.0f, -0.6f, 0.8f}); // toward the wrist
    const Vec3f top = Input::Normalized(Vec3f{0.0f, 0.8f, 0.6f});    // off the handle's top
    const float tilt = 10.0f * kDeg;
    const Vec3f z = Input::Normalized(Vec3f{1.0f, 0.0f, 0.0f} * std::cos(tilt) + top * std::sin(tilt));
    const Vec3f x = along * -1.0f;
    const Vec3f y = Input::Cross(z, x);
    return {Input::Normalized(Input::FromBasis(x, y, z)), {0.022f, -0.085f, 0.148f}};
}

Posef WristStatsPose(const Posef& left_aim) {
    static const Posef local = WristLocalPose();
    return {Input::Normalized(Input::Multiply(left_aim.orientation, local.orientation)),
            Input::Transform(left_aim, local.position)};
}

float WristFacingDeg(const Posef& panel, const Vec3f& eye) {
    const Vec3f front = Input::Rotate(panel.orientation, {0, 0, 1});
    const Vec3f to = eye - panel.position;
    if (Input::Length(to) < 1e-5f) {
        return 180.0f;
    }
    const float c = std::clamp(Input::Dot(front, Input::Normalized(to)), -1.0f, 1.0f);
    return std::acos(c) / kDeg;
}

bool WristShown(float facing_deg, bool shown) {
    return facing_deg < (shown ? kWristHideDeg : kWristShowDeg);
}

} // namespace FrameXr::Vis
