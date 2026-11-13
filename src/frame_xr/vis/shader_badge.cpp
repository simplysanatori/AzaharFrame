// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>

#include "frame_xr/input/geometry.h"
#include "frame_xr/vis/shader_badge.h"

namespace FrameXr::Vis::Badge {

using Input::Posef;
using Input::Quatf;
using Input::Vec3f;

namespace {
constexpr float kDegToRad = 3.14159265f / 180.0f;
/// The badge this far in front of the screen's surface (metres), so it is never behind it.
constexpr float kLift = 0.003f;
} // namespace

Placed OnScreen(const Posef& screen, float width, float height, float curve_radius, float aspect) {
    Placed out;
    const float inset = height * kScreenInset;
    out.height = height * kScreenHeight;
    out.width = out.height * aspect;
    const float room = width - 2.0f * inset;
    if (out.width > room && room > 0.0f) {
        const float s = room / out.width;
        out.width *= s;
        out.height *= s;
    }
    // In the screen's own axes, from its centre.
    const float x = width * 0.5f - inset - out.width * 0.5f;
    const float y = height * 0.5f - inset - out.height * 0.5f;
    Vec3f local{x, y, kLift};
    Quatf turn{};
    if (curve_radius > 0.0f) {
        // On the cylinder (screen_rig.cpp CylinderFor): its axis the radius in front of the
        // screen's centre, the arc as long as the screen is wide; the badge faces the axis.
        const float phi = x / curve_radius;
        const float r = curve_radius - kLift;
        local = {r * std::sin(phi), y, curve_radius - r * std::cos(phi)};
        turn = {0.0f, std::sin(-phi * 0.5f), 0.0f, std::cos(-phi * 0.5f)};
    }
    out.pose.orientation = Input::Normalized(Input::Multiply(screen.orientation, turn));
    out.pose.position = Input::Transform(screen, local);
    return out;
}

std::optional<Angles> ScreenAngles(const Posef& screen, float width, float height) {
    const float hw = width * 0.5f, hh = height * 0.5f;
    const std::array<Vec3f, 4> corners{{{-hw, -hh, 0}, {hw, -hh, 0}, {-hw, hh, 0}, {hw, hh, 0}}};
    Angles a{1e9f, -1e9f, -1e9f, 1e9f};
    for (const Vec3f& c : corners) {
        const Vec3f p = Input::Transform(screen, c);
        if (p.z > -0.01f) {
            return std::nullopt; // behind the head, or at it
        }
        const float h = std::atan2(p.x, -p.z);
        const float v = std::atan2(p.y, -p.z);
        a.left = std::min(a.left, h);
        a.right = std::max(a.right, h);
        a.down = std::min(a.down, v);
        a.up = std::max(a.up, v);
    }
    return a;
}

Placed HeadLocked(const Angles& view, const std::optional<Angles>& screen, float aspect) {
    Placed out;
    const float half_h = kHeadHeightDeg * 0.5f * kDegToRad;
    out.height = 2.0f * kHeadDistance * std::tan(half_h);
    out.width = out.height * aspect;
    const float half_w = std::atan(out.width * 0.5f / kHeadDistance);
    const float margin = kHeadMarginDeg * kDegToRad;
    float right = view.right - margin;
    float up = view.up - margin;
    if (screen) {
        const float inset = kHeadInsetDeg * kDegToRad;
        right = std::min(right, screen->right - inset);
        up = std::min(up, screen->up - inset);
    }
    // Never past the view's other edges either (a screen off to the side, a tiny view).
    right = std::max(right, view.left + margin + 2.0f * half_w);
    up = std::max(up, view.down + margin + 2.0f * half_h);
    // Facing the head, upright, its centre at (yaw, pitch); a few corrections put its top-right
    // corner at (right, up) exactly (a quad facing the head off to the side is turned).
    const auto place = [&](float yaw, float pitch) {
        const Vec3f dir = Input::Normalized(Vec3f{std::tan(yaw), std::tan(pitch), -1.0f});
        out.pose.position = dir * kHeadDistance;
        const Vec3f z = dir * -1.0f;
        const Vec3f x = Input::Normalized(Input::Cross(Vec3f{0.0f, 1.0f, 0.0f}, z));
        const Vec3f y = Input::Cross(z, x);
        out.pose.orientation = Input::Normalized(Input::FromBasis(x, y, z));
    };
    float yaw = right - half_w, pitch = up - half_h;
    for (int i = 0; i < 4; i++) {
        place(yaw, pitch);
        const Vec3f c = Input::Transform(out.pose, {out.width * 0.5f, out.height * 0.5f, 0.0f});
        yaw += right - std::atan2(c.x, -c.z);
        pitch += up - std::atan2(c.y, -c.z);
    }
    place(yaw, pitch);
    return out;
}

float Fade(float opacity, bool shown, int64_t dt_ns) {
    const float step =
        static_cast<float>(std::max<int64_t>(dt_ns, 0)) / static_cast<float>(kFadeNs);
    return std::clamp(opacity + (shown ? step : -step), 0.0f, 1.0f);
}

int Level(float opacity) {
    if (opacity <= 0.0f) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::ceil(opacity * kLevels - 1e-4f)), 1, kLevels);
}

} // namespace FrameXr::Vis::Badge
