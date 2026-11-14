// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctrlfront
// How the controller models (and a weapon in hand) go over the screens: "Controllers Over
// Screens" (Settings::values.vr_controllers_over).
//
// SteamVR draws every quad and cylinder layer over every projection layer, whatever the order
// they are submitted in (Moonlight XR's finding from SteamVR screenshots, and what the owner saw
// here: a hand held up in front of a screen went behind it). The models were a projection layer
// of their own, so the screens and panels always covered them. The fix needs no new image and no
// help from the compositor:
//
//   - the models' image is submitted as two quad layers instead, one per eye, each the plane of
//     that eye's view at the controllers' distance (PlaneForEye: an image drawn for an eye is a
//     picture of that eye's view on any plane square to it, so it looks the same, and SteamVR
//     moves it with the head as it does the screens). They go after the screens and panels and
//     under the beams and dots, so they draw over the screens and panels;
//   - When in Front: before the models, each eye's pass writes the depth of the screens and
//     panels it shows (Occluder, flat or curved, no colour), so the parts of a controller behind
//     a screen fail the depth test and leave the image clear there: the screen shows through.
//     A few quads a frame into a depth buffer the pass clears anyway: next to no GPU time;
//   - Never: the projection layer as before (under everything);
//   - Always: the quads with no occluders.
//
// XR_KHR_composition_layer_depth would not do it: SteamVR lists it but uses the depth only to
// reproject, it never depth-tests one layer against another, and quads still go over the
// projection layers. No OpenXR or Vulkan here (unit tested).

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#include "frame_xr/input/geometry.h"

namespace FrameXr::Vis {

enum class ControllersOver : uint32_t { Never = 0, WhenInFront = 1, Always = 2 };

constexpr ControllersOver ControllersOverFrom(uint32_t value) {
    return value == 0 ? ControllersOver::Never
                      : value == 2 ? ControllersOver::Always : ControllersOver::WhenInFront;
}

/// The models go out as the eye quads (else as the projection layer, under everything).
constexpr bool ModelsAsQuads(ControllersOver over) {
    return over != ControllersOver::Never;
}

/// A screen or panel that hides the parts of a controller behind it (When in Front), in LOCAL
/// space: a rectangle facing +Z of `pose`, or, with `curve_radius` above 0, bent as a cylinder
/// layer (Input::PanelQuad's rule: the middle stays at the pose, the width is the arc's length,
/// the sides come toward the viewer).
struct Occluder {
    Input::Posef pose;
    float width_m = 0.0f;
    float height_m = 0.0f;
    float curve_radius = 0.0f;
    uint8_t eyes = 3; ///< bit 0 the left eye sees it, bit 1 the right eye
};

/// The segments a curved occluder is drawn with (a flat one is one).
constexpr int kOccluderArcSegments = 24;
constexpr int OccluderSegments(const Occluder& o) {
    return o.curve_radius > 0.0f ? kOccluderArcSegments : 1;
}

/// Vertex `i` of an occluder's triangle strip, 2 x (segments + 1) of them, in the occluder's own
/// space (the vertex shader's kOccluderVert does the same): along the width, bottom then top.
inline Input::Vec3f OccluderVertex(float width, float height, float radius, int segments, int i) {
    const float u = static_cast<float>(i / 2) / static_cast<float>(segments);
    const float y = (i % 2 == 0 ? -0.5f : 0.5f) * height;
    if (radius <= 0.0f) {
        return {(u - 0.5f) * width, y, 0.0f};
    }
    const float a = (u - 0.5f) * width / radius;
    return {radius * std::sin(a), y, radius * (1.0f - std::cos(a))};
}

/// The plane an eye's image of the models is shown on: square to the eye's view at `distance`,
/// exactly as large as its field of view there (the image's edges are the view's edges).
struct EyePlane {
    Input::Posef pose;
    float width_m = 0.0f;
    float height_m = 0.0f;
};

/// `eye` LOCAL, the field of view's angles in radians as XrFovf (left and down negative).
inline EyePlane PlaneForEye(const Input::Posef& eye, float angle_left, float angle_right,
                            float angle_up, float angle_down, float distance) {
    const float l = std::tan(angle_left), r = std::tan(angle_right);
    const float u = std::tan(angle_up), d = std::tan(angle_down);
    const Input::Vec3f centre{distance * 0.5f * (l + r), distance * 0.5f * (u + d), -distance};
    return {{eye.orientation, Input::Transform(eye, centre)},
            distance * (r - l),
            distance * (u - d)};
}

/// The plane's distance: the nearer shown controller's depth in front of the head, so where the
/// controllers are the picture is right for any head movement before it is shown (a plane at
/// the wrong depth would swim against the hand when the head moves), else kModelPlaneDefault.
constexpr float kModelPlaneDefault = 0.5f;
constexpr float kModelPlaneMin = 0.15f;
constexpr float kModelPlaneMax = 2.0f;
inline float ModelPlaneDistance(const Input::Posef& head,
                                const std::array<std::optional<Input::Vec3f>, 2>& hands) {
    const Input::Vec3f forward = Input::Rotate(head.orientation, {0.0f, 0.0f, -1.0f});
    std::optional<float> nearest;
    for (const auto& h : hands) {
        if (!h) {
            continue;
        }
        const float z = Input::Dot(*h - head.position, forward);
        if (z > 0.0f && (!nearest || z < *nearest)) {
            nearest = z;
        }
    }
    return std::clamp(nearest.value_or(kModelPlaneDefault), kModelPlaneMin, kModelPlaneMax);
}

} // namespace FrameXr::Vis
