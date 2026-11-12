// Adapted from Frametop's pointer helper (pointer/helper/ft-pointer.cpp, pointer/common/vec3.h,
// screens/posemath.h; https://github.com/Frametop/frametop) for Azahar for Frame: its 3D mouse's
// cursor model, driving our own ray instead of a SteamVR driver.
//
// MIT License
//
// Copyright (c) 2026 DeeJanuz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <algorithm>
#include <cmath>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/mouse_pointer.h"

// AzaharFrame padsui
namespace FrameXr::Input {

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kRad = kPi / 180.0f;

float YawOf(const Vec3f& d) {
    return std::atan2(-d.x, -d.z) / kRad;
}
float PitchOf(const Vec3f& d) {
    return std::asin(std::clamp(d.y, -1.0f, 1.0f)) / kRad;
}

Vec3f RotateAbout(const Vec3f& v, const Vec3f& axis, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    return v * c + Cross(axis, v) * s + axis * (Dot(axis, v) * (1.0f - c));
}

// Unit vector v, turned toward unit vector `center` until it is at most max_rad from it.
Vec3f PullWithin(const Vec3f& v, const Vec3f& center, float max_rad) {
    if (std::acos(std::clamp(Dot(v, center), -1.0f, 1.0f)) <= max_rad) {
        return v;
    }
    Vec3f axis = Cross(center, v);
    if (Length(axis) < 1e-6f) {
        axis = Cross(center, {0.0f, 1.0f, 0.0f});
    }
    return RotateAbout(center, Normalized(axis), max_rad);
}

// A unit direction with its pitch limited to +-max_deg.
Vec3f LimitPitch(const Vec3f& d, float max_deg) {
    return DirectionOf(YawOf(d), std::clamp(PitchOf(d), -max_deg, max_deg));
}

struct Basis {
    Vec3f x, y, z;
};
Basis AimBasis(const Vec3f& aim) {
    const Vec3f z = Normalized(aim * -1.0f);
    const Vec3f x = Normalized(Cross({0.0f, 1.0f, 0.0f}, z));
    return {x, Cross(z, x), z};
}
Vec3f ToBasis(const Basis& b, const Vec3f& v) {
    return {Dot(v, b.x), Dot(v, b.y), Dot(v, b.z)};
}
Vec3f FromBasisVec(const Basis& b, const Vec3f& v) {
    return b.x * v.x + b.y * v.y + b.z * v.z;
}

float Seconds(int64_t ns) {
    return static_cast<float>(ns) * 1e-9f;
}

} // namespace

Vec3f DirectionOf(float yaw_deg, float pitch_deg) {
    const float y = yaw_deg * kRad, p = pitch_deg * kRad;
    return {-std::sin(y) * std::cos(p), std::sin(p), -std::cos(y) * std::cos(p)};
}

Quatf AimOrientation(const Vec3f& dir) {
    const Basis b = AimBasis(dir);
    return FromBasis(b.x, b.y, b.z);
}

void MousePointer::Wake(int64_t now) {
    if (!active_) {
        active_ = true;
        recenter_ = true; // where you look
    }
    last_used_ = now;
}

void MousePointer::HeadFollow(const Posef& head, int64_t now) {
    // Past the leash (for the delay), ease the reference to the head's facing and turn the
    // cursor with it.
    const Vec3f hf = Rotate(head.orientation, {0.0f, 0.0f, -1.0f});
    const Vec3f facing = LimitPitch(hf, kPitchClampDeg);
    if (follow_reset_) {
        follow_ref_ = facing;
        follow_lag_ = 0.0f;
        leash_out_since_ = 0;
        follow_reset_ = following_ = false;
    }
    const float dt = std::min(0.1f, follow_at_ ? Seconds(now - follow_at_) : 0.0f);
    const float leash = settings_.leash_deg * kRad;
    const float lag = std::acos(std::clamp(Input::Dot(follow_ref_, facing), -1.0f, 1.0f));
    float keep = lag;
    if (leash <= 0.0f) {
        keep = 0.0f;
    } else if (following_) {
        keep = lag * (settings_.leash_return > 0.0f ? std::exp(-dt / settings_.leash_return) : 0.0f);
        keep = std::min(keep, std::max(leash, follow_lag_));
        if (keep < 0.05f * kRad) {
            keep = 0.0f;
            following_ = false;
        }
    } else if (lag > leash) {
        if (leash_out_since_ == 0) {
            leash_out_since_ = now;
        }
        if (Seconds(now - leash_out_since_) >= settings_.leash_delay) {
            following_ = true;
            leash_out_since_ = 0;
        }
    } else {
        leash_out_since_ = 0; // back inside before the delay: a glance
    }
    follow_lag_ = keep;
    const Vec3f ref = LimitPitch(PullWithin(follow_ref_, facing, keep), kPitchClampDeg);
    // The cursor keeps its offset from the reference.
    Vec3f d = FromBasisVec(AimBasis(ref), ToBasis(AimBasis(follow_ref_), DirectionOf(yaw_, pitch_)));
    d = PullWithin(Normalized(d), ref, settings_.follow_reach * kRad);
    yaw_ = YawOf(d);
    pitch_ = std::clamp(PitchOf(d), -kPitchClampDeg, kPitchClampDeg);
    // The ray's origin closes in on the eye as the reference does on the facing.
    const float t = leash <= 0.0f ? 0.0f : lag > 1e-6f ? keep / lag : following_ ? 0.0f : 1.0f;
    anchor_ = head.position + (anchor_ - head.position) * t;
    follow_ref_ = ref;
}

MousePointer::Result MousePointer::Update(const MouseFrame& mouse, bool pointer,
                                          const HandInput& real, const Posef& head,
                                          bool head_valid, int64_t now) {
    Result out;
    if (!pointer || !mouse.present) {
        if (active_) {
            Reset();
        }
        follow_at_ = now;
        return out;
    }
    const bool was_active = active_;
    // Waking, and taking the ray back after a pause, needs deliberate movement, so a mouse
    // resting on a desk cannot steal the ray from a controller; a click wakes it at once.
    bool pressed_edge = false;
    for (const bool h : mouse.held) {
        pressed_edge = pressed_edge || h;
    }
    pressed_edge = pressed_edge && mouse.used;
    bool moves = mouse.motion.x != 0.0f || mouse.motion.y != 0.0f;
    const bool dormant = !active_ || now - last_used_ > kResumePauseNs;
    if (moves && dormant) {
        if (now - pending_since_ > kWakeWindowNs) {
            pending_ = 0;
            pending_since_ = now;
        }
        pending_ += static_cast<int64_t>(std::fabs(mouse.motion.x) + std::fabs(mouse.motion.y));
        if (pending_ < settings_.wake_counts) {
            moves = false; // not yet deliberate
        } else {
            pending_ = 0;
        }
    }
    if (moves || pressed_edge) {
        Wake(now);
    }

    if (active_ && recenter_ && head_valid) {
        anchor_ = head.position;
        const Vec3f f = Rotate(head.orientation, {0.0f, 0.0f, -1.0f});
        yaw_ = YawOf(f);
        pitch_ = std::clamp(PitchOf(f), -kPitchClampDeg, kPitchClampDeg);
        anchored_ = true;
        recenter_ = false;
        follow_reset_ = true;
    }
    if (active_ && moves && anchored_) {
        // Mouse right turns the ray right (yaw down); mouse down tilts it down.
        const float g = MousePointerGain(mouse.speed) * (settings_.deg_per_count / 0.03f);
        yaw_ += -mouse.motion.x * g;
        while (yaw_ > 180.0f) {
            yaw_ -= 360.0f;
        }
        while (yaw_ < -180.0f) {
            yaw_ += 360.0f;
        }
        pitch_ = std::clamp(pitch_ - mouse.motion.y * g, -kPitchClampDeg, kPitchClampDeg);
    }

    // Last used wins: the right controller moved on purpose has its ray back.
    if (real.active && real_prev_valid_ && now > real_prev_ns_) {
        const float dt = Seconds(now - real_prev_ns_);
        const float speed = Length(real.aim.position - real_prev_.position) / dt;
        const Vec3f a = Rotate(real_prev_.orientation, {0.0f, 0.0f, -1.0f});
        const Vec3f b = Rotate(real.aim.orientation, {0.0f, 0.0f, -1.0f});
        const float spin = std::acos(std::clamp(Input::Dot(a, b), -1.0f, 1.0f)) / dt;
        const float scale = settings_.pickup_scale;
        if (active_ && now - last_used_ > kPickupQuietNs &&
            (speed > 0.35f * scale || spin > 2.0f * scale)) {
            if (moving_since_ == 0) {
                moving_since_ = now;
            }
            if (now - moving_since_ >= kPickupForNs) {
                active_ = false;
                out.gave_back = true;
            }
        } else {
            moving_since_ = 0;
        }
    } else {
        moving_since_ = 0;
    }
    real_prev_valid_ = real.active;
    real_prev_ = real.aim;
    real_prev_ns_ = now;
    if (active_ && now - last_used_ > settings_.idle_ns) {
        active_ = false;
        out.gave_back = true;
    }

    if (active_ && anchored_ && settings_.follow && head_valid) {
        HeadFollow(head, now);
    }
    follow_at_ = now;
    if (!active_ || !anchored_) {
        return out;
    }
    out.active = true;
    out.took = !was_active;
    out.dir = DirectionOf(yaw_, pitch_);
    out.aim = {AimOrientation(out.dir), anchor_};
    return out;
}

MousePointer::Dot MousePointer::PlaceDot(float hit_t, const Vec3f& hit_normal, bool dragging,
                                         const Vec3f& eye) {
    Dot dot;
    if (!active_ || !anchored_) {
        return dot;
    }
    const Vec3f dir = DirectionOf(yaw_, pitch_);
    if (dragging && !was_dragging_) {
        drag_distance_ = last_distance_; // a drag keeps the distance its press had
    }
    was_dragging_ = dragging;
    float distance = settings_.free_distance;
    bool on_panel = false;
    if (dragging) {
        distance = drag_distance_;
        on_panel = true;
    } else if (hit_t > 0.0f) {
        distance = hit_t;
        on_panel = true;
        edge_known_ = true;
        edge_point_ = anchor_ + dir * hit_t;
        edge_normal_ = Normalized(hit_normal);
        edge_last_ = edge_point_;
    } else if (edge_known_) {
        // Just off a panel: stay on its plane, to reach its edges and grab bars.
        const float denom = Input::Dot(dir, edge_normal_);
        if (std::fabs(denom) > 1e-4f) {
            const float along = Input::Dot(edge_point_ - anchor_, edge_normal_) / denom;
            const Vec3f at = anchor_ + dir * along;
            if (along > 0.05f && Length(at - edge_last_) <= settings_.edge_reach) {
                distance = along;
                on_panel = true;
            }
        }
    }
    last_distance_ = distance;
    Vec3f point = anchor_ + dir * distance;
    if (on_panel) {
        point = point + Normalized(eye - point) * 0.005f; // over the panel
    }
    const float from_eye = Length(point - eye);
    dot.shown = true;
    dot.position = point;
    dot.size_m = 2.0f * from_eye * std::tan(settings_.cursor_deg * kRad * 0.5f);
    dot.on_panel = on_panel;
    return dot;
}

void MousePointer::Reset() {
    active_ = false;
    recenter_ = false;
    anchored_ = false;
    pending_ = 0;
    moving_since_ = 0;
    real_prev_valid_ = false;
    edge_known_ = false;
    was_dragging_ = false;
    follow_reset_ = true;
}

} // namespace FrameXr::Input
