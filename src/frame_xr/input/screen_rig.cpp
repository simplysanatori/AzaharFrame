// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame vrsize: see screen_rig.h.

#include <algorithm>
#include <cmath>
#include <iterator>

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

const char* LayoutName(uint32_t layout) {
    return layout == kLayoutSideBySide ? "Side by Side" : "Stacked";
}

std::string LayoutToast(uint32_t layout) {
    return std::string("Screen Layout: ") + LayoutName(layout);
}

uint32_t NextLayout(uint32_t layout) {
    return layout == kLayoutSideBySide ? kLayoutStacked : kLayoutSideBySide;
}

Posef SecondFromMain(const Posef& main, float scale, float main_width, float second_width,
                     uint32_t layout) {
    if (layout != kLayoutSideBySide) {
        return BottomFromTop(main, scale);
    }
    const float horizontal = std::max(std::hypot(main.position.x, main.position.z), 0.1f);
    // The arc between the two middles, on the circle at the main screen's horizontal distance.
    const float angle = (main_width * 0.5f + kSideGap + second_width * 0.5f) / horizontal;
    // Turned to the right about +Y: a negative angle (right-handed, Y up, -Z ahead).
    const Quatf turn{0.0f, std::sin(-angle / 2), 0.0f, std::cos(-angle / 2)};
    Posef s;
    s.position = Rotate(turn, main.position);
    s.orientation = Normalized(Multiply(turn, main.orientation));
    return s;
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

uint32_t StepCurve(uint32_t deg, int dir) {
    deg = std::min(deg, kMaxCurveDeg);
    if (dir > 0) {
        for (const uint32_t step : kCurveStepsDeg) {
            if (step > deg) {
                return step;
            }
        }
    } else if (dir < 0) {
        for (auto it = kCurveStepsDeg.rbegin(); it != kCurveStepsDeg.rend(); ++it) {
            if (*it < deg) {
                return *it;
            }
        }
    }
    return deg;
}

const char* CurveName(uint32_t deg) {
    static constexpr const char* kNames[] = {"Off", "Gentle", "Medium", "Strong", "Max"};
    static_assert(std::size(kNames) == kCurveStepsDeg.size());
    std::size_t best = 0;
    for (std::size_t i = 1; i < kCurveStepsDeg.size(); i++) {
        const auto off = [deg](uint32_t step) { return deg > step ? deg - step : step - deg; };
        if (off(kCurveStepsDeg[i]) < off(kCurveStepsDeg[best])) {
            best = i;
        }
    }
    return deg == 0 ? kNames[0] : kNames[std::max<std::size_t>(best, 1)];
}

std::string CurveToast(uint32_t deg) {
    return std::string("Screen Curve: ") + CurveName(deg);
}

float CurveRadius(float width, uint32_t deg) {
    deg = std::min(deg, kMaxCurveDeg);
    if (deg == 0 || !(width > 0.0f)) {
        return 0.0f;
    }
    return width / (static_cast<float>(deg) * kDegToRad);
}

void ApplyCurve(PanelQuad& main, PanelQuad& secondary, uint32_t deg) {
    main.curve_radius = CurveRadius(main.width_m, deg);
    secondary.curve_radius = 0.0f; // the second slot's screen is always a flat quad
}

Cylinder CylinderFor(const Posef& screen, float width, float height, float radius) {
    Cylinder c;
    c.radius = radius;
    c.pose.orientation = screen.orientation;
    c.pose.position =
        screen.position + Rotate(screen.orientation, Vec3f{0.0f, 0.0f, radius});
    c.central_angle = radius > 0.0f ? width / radius : 0.0f;
    c.aspect_ratio = height > 0.0f ? width / height : 1.0f;
    return c;
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
            if (edge) {
                steps.up_presses += kUp[i];
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
