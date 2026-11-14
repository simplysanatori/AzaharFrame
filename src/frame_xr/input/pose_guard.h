// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctrlglitch
// A guard on a controller's aim pose as SteamVR reports it, for what is drawn of the controller
// (its model, the weapon in hand, its beam). It adds no lag: a pose that a hand could have
// reached is passed through untouched. Only a pose too far from where the hand was heading to be
// a real movement (a step of centimetres or tens of degrees between two refreshes, against an
// acceleration no hand makes) is held back:
//
//   - for up to kMaxHold refreshes the controller keeps to its last course (constant velocity
//     from the last two poses it took); a pose back on that course ends the hold, and the odd one
//     out is dropped (a tracking spike);
//   - poses that stay at the new place (kConfirm of them agreeing, or the hold running out) are
//     a real move (the tracking found the controller again somewhere else): the controller goes
//     there, eased from where it was shown over about kEaseNs instead of in one step;
//   - after the controller was lost (no valid pose), a pose anywhere is taken at once, eased from
//     where the controller was left.
//
// It also counts, for the log, how often the pose is tracked, only inferred (valid but not
// tracked: SteamVR's estimate while the cameras do not see the controller), or lost, and how far
// each pose lands from the course the last ones set: the evidence for "the controllers glitch"
// reports (handoff report 70).
//
// The pose a refresh later (SteamVR's own prediction for the next display time) rides on the
// guarded pose: the models' image is drawn on one refresh and shown from the next, so they are
// drawn where the controller will be then. No OpenXR here (unit tested).

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "frame_xr/input/geometry.h"

namespace FrameXr::Input {

/// HandInput::track: what SteamVR said of this frame's aim pose (XrSpaceLocationFlags).
enum TrackBits : uint8_t {
    kTrackPositionValid = 1,
    kTrackOrientationValid = 2,
    kTrackPositionTracked = 4,
    kTrackOrientationTracked = 8,
};

/// The angle (radians, 0 to pi) of the rotation from `a` to `b`.
inline float QuatAngle(const Quatf& a, const Quatf& b) {
    const float d = std::fabs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
    return 2.0f * std::acos(std::min(d, 1.0f));
}

/// The rotation `d` scaled to `t` of its angle about the same axis (t = 0: none, 1: `d`).
inline Quatf ScaleRotation(const Quatf& d, float t) {
    Quatf q = d.w < 0.0f ? Quatf{-d.x, -d.y, -d.z, -d.w} : d;
    const float s = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
    if (s < 1e-7f) {
        return {};
    }
    const float angle = 2.0f * std::atan2(s, q.w) * t;
    const float k = std::sin(angle * 0.5f) / s;
    return Normalized(Quatf{q.x * k, q.y * k, q.z * k, std::cos(angle * 0.5f)});
}

/// The world-frame rotation taking `from` to `to` (to = result * from).
inline Quatf RotationBetween(const Quatf& from, const Quatf& to) {
    return Normalized(Multiply(to, Conjugate(from)));
}

/// The counts behind the log line, since the last ResetStats.
struct PoseGuardStats {
    uint32_t frames = 0;   ///< refreshes seen
    uint32_t tracked = 0;  ///< position and orientation tracked
    uint32_t inferred = 0; ///< valid, but the position (or orientation) not tracked
    uint32_t lost = 0;     ///< no valid pose
    uint32_t holds = 0;    ///< a pose too far from the course: held back
    uint32_t dropped = 0;  ///< ...and the hold ended back on the course (a spike, never shown)
    uint32_t jumps = 0;    ///< ...or the new place was taken (eased)
    uint32_t held_frames = 0;
    /// How far each pose lands from the course of the last two (metres): under 0.5, 1, 2 and
    /// 4 cm, and more.
    std::array<uint32_t, 5> off_cm{};
    float off_max_m = 0.0f;
    float off_max_deg = 0.0f;
};

class PoseGuard {
public:
    /// What may separate a pose from the course of the last two, `dt` after the last: a base for
    /// tracking noise plus what an acceleration a hand never makes would add (half a t squared).
    static constexpr float kBaseM = 0.03f;
    static constexpr float kAccel = 200.0f; ///< m/s^2 (about 20 g)
    static constexpr float kBaseRad = 0.14f; ///< 8 degrees
    static constexpr float kAngAccel = 2000.0f; ///< rad/s^2
    /// The fastest a hand moves and turns (10 m/s, 2300 degrees a second): what may separate
    /// two poses when there is no course to go by.
    static constexpr float kMaxSpeed = 10.0f;
    static constexpr float kMaxTurn = 40.0f;
    static constexpr int kMaxHold = 4;  ///< refreshes a pose off the course is held back at most
    static constexpr int kConfirm = 3;  ///< poses agreeing on a new place: taken at once
    static constexpr float kEaseNs = 25e6f; ///< the ease's time constant (95 % in 75 ms)
    /// A gap with no valid pose longer than this forgets the course (the models are hidden by then:
    /// Vis::HandActivity::kHoldNs).
    static constexpr int64_t kForgetNs = 250'000'000;
    /// The prediction a refresh ahead is clamped to this (8 m/s, 60 degrees at 120 Hz).
    static constexpr float kMaxAheadM = 0.07f;
    static constexpr float kMaxAheadRad = 1.05f;

    static constexpr float LimitM(float dt) {
        return kBaseM + 0.5f * kAccel * dt * dt;
    }
    static constexpr float LimitRad(float dt) {
        return kBaseRad + 0.5f * kAngAccel * dt * dt;
    }

    struct Out {
        bool active = false; ///< a pose to draw at (else the controller is lost this frame)
        Posef pose;          ///< the guarded pose at the frame's display time
        Posef ahead;         ///< the guarded pose a refresh later
        bool held = false;   ///< this frame's pose was held back
    };

    /// Each frame. `active`: the pose is valid (both position and orientation); `track` the
    /// TrackBits; `ahead`: SteamVR's pose for the next display time (`ahead_valid`).
    Out Step(int64_t t_ns, bool active, uint8_t track, const Posef& raw, const Posef& ahead,
             bool ahead_valid) {
        stats_.frames++;
        Out out;
        if (!active) {
            stats_.lost++;
            lost_frames_++;
            if (have_ && t_ns - last_t_ > kForgetNs) {
                have_ = false;
            }
            out.pose = out_;
            out.ahead = out_;
            return out;
        }
        constexpr uint8_t kTracked = kTrackPositionTracked | kTrackOrientationTracked;
        if ((track & kTracked) == kTracked) {
            stats_.tracked++;
        } else {
            stats_.inferred++;
        }
        out.active = true;
        if (!have_) {
            Take(t_ns, raw, false);
            ease_ = false;
            out_ = raw;
            lost_frames_ = 0;
            return Finish(out, raw, ahead, ahead_valid, t_ns);
        }
        const float dt = std::max(static_cast<float>(t_ns - last_t_) * 1e-9f, 1e-4f);
        const Posef course = Course(dt);
        const float off_m = Length(raw.position - course.position);
        const float off_rad = QuatAngle(raw.orientation, course.orientation);
        if (has_prev_ && held_ == 0 && lost_frames_ == 0) {
            NoteOff(off_m, off_rad);
        }
        if (lost_frames_ > 0) {
            // Found again after a loss: from where it was shown (frozen meanwhile), a step past
            // the noise is eased, wherever it lands.
            lost_frames_ = 0;
            held_ = 0;
            if (Length(raw.position - out_.position) > kBaseM ||
                QuatAngle(raw.orientation, out_.orientation) > kBaseRad) {
                Jump(t_ns, raw);
            } else {
                Take(t_ns, raw, false);
            }
            return Finish(out, raw, ahead, ahead_valid, t_ns);
        }
        // Without a course yet (just started, or a new place just taken) any speed a hand
        // reaches is allowed too.
        const float slack_m = has_prev_ ? 0.0f : kMaxSpeed * dt;
        const float slack_rad = has_prev_ ? 0.0f : kMaxTurn * dt;
        const bool plausible =
            off_m <= LimitM(dt) + slack_m && off_rad <= LimitRad(dt) + slack_rad;
        if (plausible) {
            if (held_ > 0) {
                stats_.dropped++; // the held pose was a spike: never shown
            }
            held_ = 0;
            Take(t_ns, raw, true);
            return Finish(out, raw, ahead, ahead_valid, t_ns);
        }
        // Too far from the course: held back, or a new place confirmed.
        const float since = std::max(static_cast<float>(t_ns - pending_t_) * 1e-9f, 1e-4f);
        if (held_ == 0) {
            stats_.holds++;
            pending_n_ = 1;
        } else if (Length(raw.position - pending_.position) <= LimitM(since) + kMaxSpeed * since &&
                   QuatAngle(raw.orientation, pending_.orientation) <=
                       LimitRad(since) + kMaxTurn * since) {
            pending_n_++; // agrees with the last one held: the same new place
        } else {
            pending_n_ = 1;
        }
        pending_ = raw;
        pending_t_ = t_ns;
        held_++;
        if (pending_n_ >= kConfirm || held_ > kMaxHold) {
            held_ = 0;
            Jump(t_ns, raw);
            return Finish(out, raw, ahead, ahead_valid, t_ns);
        }
        stats_.held_frames++;
        // Keep to the course (eased toward it if an ease is running), no prediction ahead.
        out_ = Eased(course, t_ns);
        out.pose = out_;
        out.ahead = out_;
        out.held = true;
        return out;
    }

    /// Forgets everything but the counts (a new session).
    void Reset() {
        have_ = false;
        has_prev_ = false;
        held_ = 0;
        pending_n_ = 0;
        lost_frames_ = 0;
        ease_ = false;
    }
    const PoseGuardStats& Stats() const {
        return stats_;
    }
    void ResetStats() {
        stats_ = {};
    }

private:
    /// Where the last two poses taken were heading, `dt` after the last.
    Posef Course(float dt) const {
        if (!has_prev_) {
            return last_;
        }
        const float span = std::max(static_cast<float>(last_t_ - prev_t_) * 1e-9f, 1e-4f);
        const float k = dt / span;
        const Vec3f v = last_.position - prev_.position;
        const Quatf w = ScaleRotation(RotationBetween(prev_.orientation, last_.orientation), k);
        return {Normalized(Multiply(w, last_.orientation)), last_.position + v * k};
    }

    void Take(int64_t t_ns, const Posef& raw, bool keep_course) {
        if (keep_course && have_) {
            prev_ = last_;
            prev_t_ = last_t_;
            has_prev_ = true;
        } else {
            has_prev_ = false;
        }
        last_ = raw;
        last_t_ = t_ns;
        have_ = true;
    }

    /// A new place taken: the course starts there, and the controller is eased to it from where
    /// it was shown.
    void Jump(int64_t t_ns, const Posef& raw) {
        stats_.jumps++;
        const Posef from = out_;
        Take(t_ns, raw, false);
        ease_dp_ = from.position - raw.position;
        ease_dq_ = RotationBetween(raw.orientation, from.orientation);
        ease_t_ = t_ns;
        ease_ = true;
    }

    /// `p` with what is left of the ease added.
    Posef Eased(const Posef& p, int64_t t_ns) {
        if (!ease_) {
            return p;
        }
        const float s = std::exp(-static_cast<float>(t_ns - ease_t_) / kEaseNs);
        if (s < 0.01f) {
            ease_ = false;
            return p;
        }
        return {Normalized(Multiply(ScaleRotation(ease_dq_, s), p.orientation)),
                p.position + ease_dp_ * s};
    }

    Out& Finish(Out& out, const Posef& raw, const Posef& ahead, bool ahead_valid, int64_t t_ns) {
        out_ = Eased(raw, t_ns);
        out.pose = out_;
        out.ahead = out_;
        if (ahead_valid) {
            Vec3f dp = ahead.position - raw.position;
            const float len = Length(dp);
            if (len > kMaxAheadM) {
                dp = dp * (kMaxAheadM / len);
            }
            Quatf dq = RotationBetween(raw.orientation, ahead.orientation);
            const float angle = QuatAngle(raw.orientation, ahead.orientation);
            if (angle > kMaxAheadRad) {
                dq = ScaleRotation(dq, kMaxAheadRad / angle);
            }
            out.ahead = {Normalized(Multiply(dq, out_.orientation)), out_.position + dp};
        }
        return out;
    }

    void NoteOff(float off_m, float off_rad) {
        const float cm = off_m * 100.0f;
        const std::size_t bin = cm < 0.5f ? 0 : cm < 1.0f ? 1 : cm < 2.0f ? 2 : cm < 4.0f ? 3 : 4;
        stats_.off_cm[bin]++;
        stats_.off_max_m = std::max(stats_.off_max_m, off_m);
        stats_.off_max_deg = std::max(stats_.off_max_deg, off_rad * 57.29578f);
    }

    bool have_ = false;
    bool has_prev_ = false;
    Posef last_{}, prev_{};
    int64_t last_t_ = 0, prev_t_ = 0;
    Posef out_{};
    int held_ = 0;
    Posef pending_{};
    int64_t pending_t_ = 0;
    int pending_n_ = 0;
    int lost_frames_ = 0;
    bool ease_ = false;
    Vec3f ease_dp_{};
    Quatf ease_dq_{};
    int64_t ease_t_ = 0;
    PoseGuardStats stats_;
};

} // namespace FrameXr::Input
