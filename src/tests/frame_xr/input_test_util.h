// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include "frame_xr/input/geometry.h"
#include "frame_xr/input/mapper.h"

// AzaharFrame
// Shared fixtures for the frame_xr input tests.
namespace FrameXr::Input::TestUtil {

/// A pose at `origin` whose -Z (the pointing ray) goes toward `target`.
inline Posef AimAt(Vec3f origin, Vec3f target) {
    return Posef{FaceViewer(target, origin, Vec3f{0.0f, 1.0f, 0.0f}), origin};
}

/// Top screen 2 m ahead, 1.6 x 0.96 m; bottom screen below and nearer, 0.6 x 0.45 m (4:3), both
/// facing the viewer.
inline PanelGeometry StandardPanels() {
    PanelGeometry panels;
    panels.top.pose = Posef{Quatf{}, Vec3f{0.0f, 0.0f, -2.0f}};
    panels.top.width_m = 1.6f;
    panels.top.height_m = 0.96f;
    panels.top.visible = true;
    panels.bottom.pose = Posef{Quatf{}, Vec3f{0.0f, -0.8f, -1.5f}};
    panels.bottom.width_m = 0.6f;
    panels.bottom.height_m = 0.45f;
    panels.bottom.visible = true;
    return panels;
}

constexpr Vec3f kLeftOrigin{-0.2f, -0.4f, -0.2f};
constexpr Vec3f kRightOrigin{0.2f, -0.4f, -0.2f};
constexpr int64_t kFrameNs = 11'111'111; // 90 Hz

/// A frame with both controllers tracked and pointing sideways (+X), at nothing.
inline InputFrame IdleFrame(int64_t time_ns = 0) {
    InputFrame frame;
    frame.time_ns = time_ns;
    frame.hand[0].active = true;
    frame.hand[0].aim = AimAt(kLeftOrigin, Vec3f{5.0f, kLeftOrigin.y, kLeftOrigin.z});
    frame.hand[1].active = true;
    frame.hand[1].aim = AimAt(kRightOrigin, Vec3f{5.0f, kRightOrigin.y, kRightOrigin.z});
    return frame;
}

/// Points a hand at a point (in the panels' space).
inline void Point(InputFrame& frame, Hand hand, Vec3f target) {
    const Vec3f origin = hand == Hand::Left ? kLeftOrigin : kRightOrigin;
    frame.hand[static_cast<std::size_t>(HandIndex(hand))].aim = AimAt(origin, target);
}

} // namespace FrameXr::Input::TestUtil
