// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctrlfront
// How the controller models (and a weapon in hand) go in front of the screens: "Controllers in
// Front of Screen" (Settings::values.vr_controllers_in_front): Off, Primary, Secondary, Both, as
// Detach Screens (Off, Secondary, Both) names the screens.
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
//   - before the models, each eye's pass writes the depth of the screens and panels it shows
//     (Occluder, flat or curved, no colour), so the parts of a controller behind one fail the
//     depth test and leave the image clear there: the screen shows through. A few quads a
//     frame into a depth buffer the pass clears anyway: next to no GPU time;
//   - a screen the choice leaves out (the Secondary Screen with Primary, the Primary Screen with
//     Secondary) is written at the nearest depth instead (Occluder::always): it covers the
//     controllers wherever they are, as before. The panels (menus, keyboard, stats, help) are
//     always by depth; the immersive top screen, when chosen, hides nothing (it is far away);
//   - Off: the projection layer as before (under every screen and panel).
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

/// The stored values (vr_controllers_in_front). Primary is the screen in the main slot (the top
/// screen, or the bottom one while it is swapped in), Secondary the other, as Detach Screens.
enum class ControllersInFront : uint32_t { Off = 0, Primary = 1, Secondary = 2, Both = 3 };

/// A value out of range (a hand-edited file) reads as the default, Both.
constexpr ControllersInFront ControllersInFrontFrom(uint32_t value) {
    return value <= 3 ? static_cast<ControllersInFront>(value) : ControllersInFront::Both;
}

/// The models go out as the eye quads (else as the projection layer, under everything).
constexpr bool ModelsAsQuads(ControllersInFront choice) {
    return choice != ControllersInFront::Off;
}

/// Whether the controllers may go in front of the Primary (`primary`) or the Secondary Screen;
/// when not, that screen covers them wherever they are.
constexpr bool InFrontOfScreen(ControllersInFront choice, bool primary) {
    return choice == ControllersInFront::Both ||
           choice == (primary ? ControllersInFront::Primary : ControllersInFront::Secondary);
}

/// A screen or panel that hides the parts of a controller behind it, in LOCAL
/// space: a rectangle facing +Z of `pose`, or, with `curve_radius` above 0, bent as a cylinder
/// layer (Input::PanelQuad's rule: the middle stays at the pose, the width is the arc's length,
/// the sides come toward the viewer).
struct Occluder {
    Input::Posef pose;
    float width_m = 0.0f;
    float height_m = 0.0f;
    float curve_radius = 0.0f;
    uint8_t eyes = 3; ///< bit 0 the left eye sees it, bit 1 the right eye
    /// Covers the controllers wherever they are (a screen the choice leaves out): written at
    /// the nearest depth.
    bool always = false;
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
