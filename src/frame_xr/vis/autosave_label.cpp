// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>

#include "frame_xr/input/geometry.h"
#include "frame_xr/vis/autosave_label.h"

namespace FrameXr::Vis {

using Input::Posef;
using Input::Vec3f;

Posef LabelPose(const Posef& top, float top_w, float top_h) {
    // In the screen's own axes: right of centre by half the screen less half the label, up by half
    // the screen, the gap and half the label.
    const Vec3f local{top_w * 0.5f - kLabelW * 0.5f, top_h * 0.5f + kLabelGap + kLabelH * 0.5f,
                      0.0f};
    Posef p;
    p.orientation = top.orientation;
    p.position = Input::Transform(top, local);
    return p;
}

bool PointInView(const EyeFov& eye, const Vec3f& point, float margin_deg) {
    const Vec3f p = Input::Transform(Input::Inverse(eye.pose), point);
    if (p.z > -0.05f) {
        return false; // behind, or at the eye
    }
    const float h = std::atan2(p.x, -p.z);
    const float v = std::atan2(p.y, -p.z);
    const float m = margin_deg * 3.14159265f / 180.0f;
    return h >= eye.left + m && h <= eye.right - m && v >= eye.down + m && v <= eye.up - m;
}

bool LabelInView(const Posef& label, const EyeFov& left, const EyeFov& right, float margin_deg) {
    const float hw = kLabelW * 0.5f, hh = kLabelH * 0.5f;
    const std::array<Vec3f, 4> corners{{{-hw, -hh, 0}, {hw, -hh, 0}, {-hw, hh, 0}, {hw, hh, 0}}};
    for (const Vec3f& c : corners) {
        const Vec3f p = Input::Transform(label, c);
        if (!PointInView(left, p, margin_deg) || !PointInView(right, p, margin_deg)) {
            return false;
        }
    }
    return true;
}

float LabelOpacity(int64_t t_ns) {
    if (t_ns < 0 || t_ns >= kLabelNs) {
        return 0.0f;
    }
    const int64_t edge = std::min(t_ns, kLabelNs - t_ns);
    return std::clamp(static_cast<float>(edge) / static_cast<float>(kFadeNs), 0.0f, 1.0f);
}

int LabelLevel(float opacity) {
    if (opacity <= 0.0f) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::ceil(opacity * kLabelLevels - 1e-4f)), 1, kLabelLevels);
}

} // namespace FrameXr::Vis
