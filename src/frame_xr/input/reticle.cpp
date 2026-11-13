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

// AzaharFrame kiu
ReticleStickSplit SplitReticleStick(bool stick_moves, bool reticle_live, Vec2f stick) {
    ReticleStickSplit split;
    split.to_reticle = stick_moves ? stick : Vec2f{};
    split.to_game = stick_moves && reticle_live ? Vec2f{} : stick;
    return split;
}

ReticleEdgeBand ReticleEdgeBandFor(float half_width_deg) {
    if (!(half_width_deg > 0.0f)) {
        return {};
    }
    const float hh = half_width_deg * kHeightOverWidth;
    return {std::clamp(kEdgeBandDeg / (2.0f * half_width_deg), kEdgeBandMin, kEdgeBandMax),
            std::clamp(kEdgeBandDeg / (2.0f * hh), kEdgeBandMin, kEdgeBandMax)};
}

namespace {
/// How far into the band at the low (negative) or the high (positive) end of 0..1.
float EdgeDepth(float v, float band) {
    v = std::clamp(v, 0.0f, 1.0f);
    if (v < band) {
        return -std::min((band - v) / band, 1.0f);
    }
    if (v > 1.0f - band) {
        return std::min((v - (1.0f - band)) / band, 1.0f);
    }
    return 0.0f;
}
} // namespace

Vec2f ReticleEdgeStick(Vec2f point, bool touching, const ReticleEdgeBand& band) {
    if (!touching || !band.On() || !(point.x == point.x) || !(point.y == point.y)) {
        return {};
    }
    // The point's y grows downward, the stick's up.
    return AddStick({EdgeDepth(point.x, band.x), -EdgeDepth(point.y, band.y)}, {});
}

namespace {
/// -1 at the low end of 0..1, +1 at the high end, 0 outside a band of `band`; `depth` how far in
/// (0 at the band's start, 1 at the edge).
int EdgeSide(float v, float band, float& depth) {
    v = std::clamp(v, 0.0f, 1.0f);
    depth = 0.0f;
    if (v < band) {
        depth = std::min((band - v) / band, 1.0f);
        return -1;
    }
    if (v > 1.0f - band) {
        depth = std::min((v - (1.0f - band)) / band, 1.0f);
        return 1;
    }
    return 0;
}
} // namespace

ReticleEdgeHandoff::Out ReticleEdgeHandoff::Update(Vec2f point, bool down,
                                                   const ReticleEdgeBand& band, int64_t now) {
    Out out;
    const State before = state_;
    const bool valid = point.x == point.x && point.y == point.y;
    if (!down || !band.On() || !valid) {
        state_ = State::Touch;
        out.changed = state_ != before;
        return out;
    }
    float dx = 0.0f, dy = 0.0f;
    const int sx = EdgeSide(point.x, band.x, dx);
    const int sy = EdgeSide(point.y, band.y, dy);
    const bool in_band = sx != 0 || sy != 0;
    // The band and its hysteresis strip.
    float wx = 0.0f, wy = 0.0f;
    const int hx = EdgeSide(point.x, std::min(band.x * (1.0f + kHandoffHysteresis), 0.5f), wx);
    const int hy = EdgeSide(point.y, std::min(band.y * (1.0f + kHandoffHysteresis), 0.5f), wy);
    const bool near = hx != 0 || hy != 0;
    switch (state_) {
    case State::Touch:
        if (in_band) {
            state_ = State::Lead;
            since_ns_ = now;
        }
        break;
    case State::Lead:
        if (!in_band) {
            state_ = State::Touch;
        } else if (now - since_ns_ >= kHandoffLeadNs) {
            state_ = State::Stick;
            since_ns_ = now;
        }
        break;
    case State::Stick:
        if (!near) {
            state_ = State::Return;
            since_ns_ = now;
        }
        break;
    case State::Return:
        if (in_band) {
            state_ = State::Stick; // back into the band before the stick let go
            since_ns_ = now;
        } else if (now - since_ns_ >= kHandoffLagNs) {
            state_ = State::Touch;
        }
        break;
    }
    out.changed = state_ != before;
    out.touch = state_ != State::Stick;
    if (state_ != State::Touch) {
        // Each axis in its band or strip pushes toward its edge, at least the floor; the point's
        // y grows downward, the stick's up. In Return (out of the strip) the axis that was
        // pushing keeps the floor until the touch is back.
        const auto axis = [](int side_band, float depth, int side_near) {
            const int side = side_band != 0 ? side_band : side_near;
            return side == 0 ? 0.0f
                             : static_cast<float>(side) *
                                   (kEdgeStickFloor + (1.0f - kEdgeStickFloor) * depth);
        };
        Vec2f s{axis(sx, dx, hx), -axis(sy, dy, hy)};
        if (state_ == State::Return && s.x == 0.0f && s.y == 0.0f) {
            s = last_push_;
        }
        out.stick = AddStick(s, {});
        last_push_ = {out.stick.x == 0.0f ? 0.0f : std::copysign(kEdgeStickFloor, out.stick.x),
                      out.stick.y == 0.0f ? 0.0f : std::copysign(kEdgeStickFloor, out.stick.y)};
    } else {
        last_push_ = {};
    }
    return out;
}

Vec2f AddStick(Vec2f a, Vec2f b) {
    Vec2f s{a.x + b.x, a.y + b.y};
    const float len = std::sqrt(s.x * s.x + s.y * s.y);
    if (len > 1.0f) {
        s.x /= len;
        s.y /= len;
    }
    return s;
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
                                         int64_t now, ReticleEdge edge, ReticleModes modes) {
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
    trigger_ = hand.trigger > (trigger_ ? kTriggerRelease : kTriggerPress);
    if (modes.center_until_pressed && !trigger_) {
        // AzaharFrame fixes7: Aim Only While Pressed: not touching, the middle is where it aims.
        ref_yaw_ = fy;
        ref_pitch_ = fp;
        out_ = {};
        lifting_ = false;
    } else if (modes.stick_moves) {
        // AzaharFrame fixes7: Right Stick Moves Touch: the stick turns the reference the other way,
        // so the point moves with the stick (right is right, up is up).
        const float len = std::sqrt(hand.stick.x * hand.stick.x + hand.stick.y * hand.stick.y);
        if (len > kStickDeadzone) {
            const float t = std::min((len - kStickDeadzone) / (1.0f - kStickDeadzone), 1.0f);
            const float k = t * t / len;
            const float step = std::clamp(dt, 0.0f, 0.1f) * kStickScreensPerSec * 2.0f;
            ref_yaw_ += hand.stick.x * k * step * hw / ys;
            ref_pitch_ -= hand.stick.y * k * step * hh;
        }
    }
    float dy = (fy - ref_yaw_) * ys;
    float dp = fp - ref_pitch_;
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
