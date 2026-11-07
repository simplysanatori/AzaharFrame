// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame vrsize: see screen_rig.h.

#include <algorithm>
#include <cmath>

#include "frame_xr/input/geometry.h"
#include "frame_xr/input/screen_rig.h"

namespace FrameXr::Input::ScreenRig {

namespace {
constexpr float kDegToRad = 3.14159265f / 180.0f;
}

float ClampScale(float scale) {
    return std::isfinite(scale) ? std::clamp(scale, kMinScale, kMaxScale) : 1.0f;
}

float ScaleWithStick(float scale, float stick_x, float dt, float threshold) {
    if (!(std::abs(stick_x) > threshold)) {
        return ClampScale(scale);
    }
    return ClampScale(scale * std::exp(stick_x * kScaleRate * std::clamp(dt, 0.0f, 0.1f)));
}

float DepthScale(float scale) {
    return 1.0f / std::max(1.0f, ClampScale(scale));
}

Vec3f BottomOffset(float scale) {
    const float s = ClampScale(scale);
    // At scale 1: 0.75 m below the centre, 0.25 m below the top screen's bottom edge.
    return {0.0f, -(kTopHalfHeight * s + (kBottomBelow - kTopHalfHeight)), kBottomNearer};
}

Posef BottomFromTop(const Posef& top, float scale) {
    const Quatf pitch{std::sin(kBottomPitch / 2), 0.0f, 0.0f, std::cos(kBottomPitch / 2)};
    Posef b;
    b.position = top.position + Rotate(top.orientation, BottomOffset(scale));
    b.orientation = Normalized(Multiply(top.orientation, pitch));
    return b;
}

std::optional<Posef> NudgeTop(const Posef& top, int right_steps, int up_steps) {
    const Vec3f p = top.position;
    const float r = Length(p);
    if (r < 1e-3f || (right_steps == 0 && up_steps == 0)) {
        return std::nullopt;
    }
    const float limit = kMaxNudgeAngleDeg * kDegToRad;
    const float azimuth = std::atan2(p.x, -p.z); // positive = right
    const float elevation = std::asin(std::clamp(p.y / r, -1.0f, 1.0f));
    const float step = kNudgeStepDeg * kDegToRad;
    const float a = azimuth + static_cast<float>(right_steps) * step;
    const float e = elevation + static_cast<float>(up_steps) * step;
    // Past the limit only back towards the middle.
    if ((std::abs(a) > limit && std::abs(a) > std::abs(azimuth)) ||
        (std::abs(e) > limit && std::abs(e) > std::abs(elevation))) {
        return std::nullopt;
    }
    const Vec3f moved{r * std::cos(e) * std::sin(a), r * std::sin(e), -r * std::cos(e) * std::cos(a)};
    if (moved.z > kNearestTopZ && moved.z > p.z) {
        return std::nullopt; // as a drag: no nearer than 1 m ahead
    }
    return Posef{FaceViewer(moved, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}), moved};
}

const char* MovedToast(int right_steps, int up_steps) {
    if (right_steps != 0) {
        return right_steps > 0 ? "Moved Right" : "Moved Left";
    }
    return up_steps > 0 ? "Moved Up" : "Moved Down";
}

DpadNudger::Steps DpadNudger::Filter(InputFrame& in, bool holding) {
    bool* const pressed[4] = {&in.dpad_right, &in.dpad_left, &in.dpad_up, &in.dpad_down};
    static constexpr int kRight[4] = {1, -1, 0, 0};
    static constexpr int kUp[4] = {0, 0, 1, -1};
    Steps steps;
    for (std::size_t i = 0; i < dirs_.size(); i++) {
        Dir& d = dirs_[i];
        const bool now = *pressed[i];
        const bool edge = now && !d.down;
        d.down = now;
        if (!now) {
            d.swallowed = false;
            continue;
        }
        if (holding) {
            d.swallowed = true;
            bool step = false;
            if (edge) {
                step = true;
                d.next_ns = in.time_ns + kRepeatDelayNs;
            } else if (in.time_ns >= d.next_ns && d.next_ns != 0) {
                step = true;
                d.next_ns = in.time_ns + kRepeatNs;
            }
            if (step) {
                steps.right += kRight[i];
                steps.up += kUp[i];
            }
        } else {
            d.next_ns = 0; // a held direction repeats again only after a new press while holding
        }
        if (d.swallowed) {
            *pressed[i] = false;
        }
    }
    return steps;
}

void DpadNudger::Reset() {
    dirs_ = {};
}

} // namespace FrameXr::Input::ScreenRig
