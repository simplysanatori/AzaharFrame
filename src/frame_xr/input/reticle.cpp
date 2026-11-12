// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <cmath>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/reticle.h"

// AzaharFrame mouse
namespace FrameXr::Input {

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kRad = kPi / 180.0f;
constexpr float kHeightOverWidth = 240.0f / 320.0f;

float Wrap180(float deg) {
    while (deg > 180.0f) {
        deg -= 360.0f;
    }
    while (deg < -180.0f) {
        deg += 360.0f;
    }
    return deg;
}

float Alpha(float cutoff_hz, float dt) {
    const float tau = 1.0f / (2.0f * kPi * cutoff_hz);
    return 1.0f / (1.0f + tau / dt);
}

} // namespace

namespace {
std::atomic<bool> g_rezero{false};
}

void RequestReticleRezero() {
    g_rezero.store(true);
}

bool TakeReticleRezero() {
    return g_rezero.exchange(false);
}

// AzaharFrame input2
ReticleAim ReticleAimOf(const HandInput& hand) {
    return {hand.active, hand.aim.orientation, hand.trigger, hand.grip};
}

ReticleAim ReticleHeadAim(bool head_active, const Quatf& head, const HandInput& left,
                          const HandInput& right) {
    return {head_active, head, std::max(left.trigger, right.trigger), 0.0f};
}

int ReticleHandIndex(ReticleHand hand) {
    switch (hand) {
    case ReticleHand::Left:
        return HandIndex(Hand::Left);
    case ReticleHand::Right:
        return HandIndex(Hand::Right);
    default:
        return -1;
    }
}

int ReticleDrivingHand(ReticleHand source, bool game_frame, const InputFrame& in) {
    const int h = ReticleHandIndex(source); // AzaharFrame polish
    if (!game_frame || h < 0 || !in.hand[static_cast<std::size_t>(h)].active) {
        return -1;
    }
    return h;
}

// AzaharFrame reticle: Reticle Speed
float ClampReticleSpeed(float speed) {
    if (!(speed == speed)) { // NaN (a hand-edited config)
        return kReticleSpeedDefault;
    }
    return std::clamp(speed, kReticleSpeedMin, kReticleSpeedMax);
}

float ReticleHalfWidthDeg(ReticleHand source, float speed) {
    const float s = ClampReticleSpeed(speed);
    switch (source) {
    case ReticleHand::Left:
    case ReticleHand::Right:
        return kReticleControllerHalfWidthDeg / s;
    case ReticleHand::Headset:
        return kReticleHeadHalfWidthDeg / s;
    default:
        return 0.0f;
    }
}

Vec2f ReticlePoint(float yaw_deg, float pitch_deg, float half_width_deg) {
    const float hw = std::max(half_width_deg, 0.1f);
    const float hh = hw * kHeightOverWidth;
    return {std::clamp(0.5f - yaw_deg / (2.0f * hw), 0.0f, 1.0f),
            std::clamp(0.5f - pitch_deg / (2.0f * hh), 0.0f, 1.0f)};
}

float OneEuro::Filter(float x, float dt) {
    if (!started_ || dt <= 0.0f || dt > 0.1f) {
        started_ = true;
        x_ = x;
        dx_ = 0.0f;
        return x;
    }
    const float dx = (x - x_) / dt;
    dx_ += Alpha(d_cutoff_, dt) * (dx - dx_);
    const float cutoff = min_cutoff_ + beta_ * std::fabs(dx_);
    x_ += Alpha(cutoff, dt) * (x - x_);
    return x_;
}

void BottomScreenReticle::Reset() {
    started_ = false;
    rezero_ = false;
    out_ = {};
    yaw_filter_.Reset();
    pitch_filter_.Reset();
    trigger_ = false;
    grip_ = false;
    lifting_ = false;
    lift_until_ = 0;
    snap_ready_ = 0;
}

ReticleFrame BottomScreenReticle::Update(const ReticleAim& hand, bool live, float half_width_deg,
                                         int64_t now, ReticleEdge edge) {
    ReticleFrame f;
    if (!live || !hand.active) {
        Reset();
        return f;
    }
    // Where the controller (or the head) points, as yaw (left positive) and pitch (up
    // positive), the yaw unwrapped so a turn through behind you does not jump.
    const Vec3f d = Rotate(hand.orientation, {0.0f, 0.0f, -1.0f});
    const float yaw = std::atan2(-d.x, -d.z) / kRad;
    const float pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f)) / kRad;
    raw_yaw_ = started_ ? raw_yaw_ + Wrap180(yaw - Wrap180(raw_yaw_)) : yaw;
    const float dt = started_ ? static_cast<float>(now - last_ns_) * 1e-9f : 0.0f;
    last_ns_ = now;
    const float fy = yaw_filter_.Filter(raw_yaw_, dt);
    const float fp = pitch_filter_.Filter(pitch, dt);

    // A grip press: the reference to where it points.
    const bool grip = hand.grip > (grip_ ? kGripRelease : kGripPress);
    if (grip && !grip_) {
        rezero_ = true;
    }
    grip_ = grip;
    if (!started_ || rezero_) {
        f.rezeroed = started_;
        ref_yaw_ = fy;
        ref_pitch_ = fp;
        out_ = {};
        started_ = true;
        rezero_ = false;
    }

    const float hw = std::max(half_width_deg, 0.1f);
    const float hh = hw * kHeightOverWidth;
    // AzaharFrame input2: the yaw across the cone, at the reference's pitch (a yaw of 10 degrees
    // while pointing 40 degrees down turns the aim by about 7.7). `ys` is fixed for the frame, so
    // the edge drift below stays exact.
    const float ys = std::max(std::cos(ref_pitch_ * kRad), kMinYawScale);
    float dy = (fy - ref_yaw_) * ys;
    float dp = fp - ref_pitch_;
    trigger_ = hand.trigger > (trigger_ ? kTriggerRelease : kTriggerPress);
    // Snap to Middle: past an edge (and not just after the last snap), the middle is where it
    // points now; a touch held lifts first.
    if (edge == ReticleEdge::SnapToMiddle && !lifting_ && now >= snap_ready_ &&
        (std::fabs(dy) > hw || std::fabs(dp) > hh)) {
        f.snapped = true;
        ref_yaw_ = fy;
        ref_pitch_ = fp;
        out_ = {};
        dy = dp = 0.0f;
        if (trigger_) {
            lifting_ = true;
            lift_until_ = now + kLiftNs;
        } else {
            snap_ready_ = now + kSnapCooldownNs;
        }
    } else if (lifting_) {
        // Lifted: the middle stays where it points, so the touch comes back at the middle
        // however far the swipe goes meanwhile.
        ref_yaw_ = fy;
        ref_pitch_ = fp;
        out_ = {};
        dy = dp = 0.0f;
        if (now >= lift_until_ || !trigger_) {
            lifting_ = false; // this frame touches again (if still held)
            snap_ready_ = now + kSnapCooldownNs;
        }
    }
    // Edge drift (Follow, and Snap to Middle while it waits): past an edge the reference comes
    // along, the point stays on the edge.
    if (dy > hw) {
        ref_yaw_ += (dy - hw) / ys;
        dy = hw;
    } else if (dy < -hw) {
        ref_yaw_ += (dy + hw) / ys;
        dy = -hw;
    }
    if (dp > hh) {
        ref_pitch_ += dp - hh;
        dp = hh;
    } else if (dp < -hh) {
        ref_pitch_ += dp + hh;
        dp = -hh;
    }

    // The dead zone drags the point: it moves once the aim is kDeadzoneDeg away, and then
    // follows at that distance (no steps).
    const float ex = dy - out_.x, ey = dp - out_.y;
    const float e = std::sqrt(ex * ex + ey * ey);
    if (e > kDeadzoneDeg) {
        const float k = (e - kDeadzoneDeg) / e;
        out_.x += ex * k;
        out_.y += ey * k;
    }

    f.on = true;
    f.point = ReticlePoint(out_.x, out_.y, hw);
    f.down = trigger_ && !lifting_;
    return f;
}

} // namespace FrameXr::Input
