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
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

/// The part of an eye's image the models can cover: the bounds of the spheres (centre LOCAL,
/// radius) seen from `eye` with that field of view, as {u0, v0, u1, v1}, 0 to 1 across the image
/// from its left and top edges; none when no sphere is in front of the eye. A sphere reaching the
/// eye's near plane (`near_m`) gives the whole image. The occluders are drawn only there: their
/// depth matters nowhere else, and so it costs next to nothing (on the Frame a screen filling the
/// view, written whole, cost about 0.2 ms a picture).
struct ModelSphere {
    Input::Vec3f centre;
    float radius = 0.0f;
};
constexpr float kWeaponSphere = 1.2f; ///< a weapon in hand round the aim pose, with the forearm
/// Room for a controller's moving parts (trigger, stick, buttons) past its model at rest.
constexpr float kMovingPartsMargin = 0.02f;

/// The sphere round a model's vertices (`stride` floats each, the position first): the middle of
/// their box, out to the farthest one, plus `margin`. A Frame controller's is about 9 cm round a
/// point 6 cm behind its aim pose (the files `azahar --frame-controller-models` makes).
inline ModelSphere ModelBoundingSphere(const std::vector<float>& vertices, uint32_t stride,
                                       float margin) {
    if (stride < 3 || vertices.size() < stride) {
        return {{0.0f, 0.0f, 0.0f}, 0.0f};
    }
    Input::Vec3f lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
    for (std::size_t i = 0; i + 2 < vertices.size(); i += stride) {
        lo = {std::min(lo.x, vertices[i]), std::min(lo.y, vertices[i + 1]),
              std::min(lo.z, vertices[i + 2])};
        hi = {std::max(hi.x, vertices[i]), std::max(hi.y, vertices[i + 1]),
              std::max(hi.z, vertices[i + 2])};
    }
    const Input::Vec3f c = (lo + hi) * 0.5f;
    float r = 0.0f;
    for (std::size_t i = 0; i + 2 < vertices.size(); i += stride) {
        r = std::max(r, Input::Length(Input::Vec3f{vertices[i], vertices[i + 1], vertices[i + 2]} - c));
    }
    return {c, r + margin};
}
inline std::optional<std::array<float, 4>> ModelsViewBounds(
    const Input::Posef& eye, float angle_left, float angle_right, float angle_up, float angle_down,
    const ModelSphere* spheres, std::size_t count, float near_m) {
    const float l = std::tan(angle_left), r = std::tan(angle_right);
    const float u = std::tan(angle_up), d = std::tan(angle_down);
    const Input::Posef to_eye = Input::Inverse(eye);
    std::optional<std::array<float, 4>> out;
    const auto add = [&](float u0, float v0, float u1, float v1) {
        if (!out) {
            out = std::array<float, 4>{u0, v0, u1, v1};
        } else {
            (*out)[0] = std::min((*out)[0], u0);
            (*out)[1] = std::min((*out)[1], v0);
            (*out)[2] = std::max((*out)[2], u1);
            (*out)[3] = std::max((*out)[3], v1);
        }
    };
    for (std::size_t i = 0; i < count; i++) {
        const Input::Vec3f c = Input::Transform(to_eye, spheres[i].centre);
        const float rad = spheres[i].radius;
        if (c.z - rad >= 0.0f) {
            continue; // all of it behind the eye
        }
        if (c.z + rad > -near_m) {
            add(0.0f, 0.0f, 1.0f, 1.0f); // at the eye: anywhere
            continue;
        }
        // The box round the sphere, its corners projected (all in front of the eye).
        float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
        for (int k = 0; k < 8; k++) {
            const float x = c.x + ((k & 1) ? rad : -rad);
            const float y = c.y + ((k & 2) ? rad : -rad);
            const float z = c.z + ((k & 4) ? rad : -rad);
            const float tx = x / -z, ty = y / -z;
            const float pu = (tx - l) / (r - l), pv = (u - ty) / (u - d);
            u0 = std::min(u0, pu);
            u1 = std::max(u1, pu);
            v0 = std::min(v0, pv);
            v1 = std::max(v1, pv);
        }
        if (u1 <= 0.0f || u0 >= 1.0f || v1 <= 0.0f || v0 >= 1.0f) {
            continue; // out of view
        }
        add(std::clamp(u0, 0.0f, 1.0f), std::clamp(v0, 0.0f, 1.0f), std::clamp(u1, 0.0f, 1.0f),
            std::clamp(v1, 0.0f, 1.0f));
    }
    return out;
}

// ---- AzaharFrame ctrlglitch: the eye quads on a world-locked canvas ----
//
// SteamVR makes each quad layer an overlay and sets its texture and its place in separate calls
// that the compositor takes up on its own schedule (handoff report 55/flash/overlay-path.md), so
// for a refresh a quad can show one image at the place meant for another. The eye quads were
// each eye's view at the time their image was drawn: a new place and a new image every refresh
// (the log's "1200 pictures in 10 s"), so a refresh that paired an image with the place of the
// refresh before or after showed the controllers off by the head's turn in one refresh (1.25
// degrees at 150 degrees a second), left or right as the head turned: the reported glitching.
//
// Now each eye's quad is a canvas that stays where it was put in the room, square to the eye's
// view when put there and a little larger (kCanvasMargin), and every image is drawn onto it
// from wherever the eye is (CanvasView: the eye looking square at the canvas, its frustum through
// the canvas's edges; an image drawn so looks right from that eye whatever way the head turns).
// The place only changes when the canvas stops serving the view (CanvasServes): the models'
// part of the view leaves it, the head turns well away from it (kCanvasMaxTilt), or the eye
// comes too near. A new image paired with the canvas's place is right; an older one is only the
// older moment of the controllers, never the head's turn.
//
// Moving the canvas itself goes through CanvasHandover, so its texture and its place never show
// mismatched either: the quads are shrunk to nothing first, moved once the shrink has had time
// to take, and grown back once images for the new place have gone out for as long. Meanwhile
// the image released last also goes out as the projection layer (image and views in one
// submit, which SteamVR gets right), so the controllers stay on view; they only go behind a
// screen for those few refreshes. No OpenXR or Vulkan here (unit tested).

/// A quad not shown is this size times its own (LayerSlots::kCollapse): nothing is drawn of it,
/// and its texture and bounds stay as they are.
constexpr float kCanvasShrink = 1e-5f;
/// The canvas reaches this far round the eye's view (radians, about 4 degrees each side).
constexpr float kCanvasMargin = 0.07f;
/// It is moved when the models' part of the view, this much larger (about 2 degrees), leaves it.
constexpr float kCanvasGuard = 0.035f;
/// ... or the eye looks this far off its square (45 degrees) ...
constexpr float kCanvasMaxTilt = 0.785f;
/// The models' part of the view only counts this far in from the view's edges (about 6
/// degrees): controllers held low, across the edge of the view, would otherwise move the
/// canvas at every nod. Past it, at the very edge of the lenses, the quads may cut them.
constexpr float kCanvasEdge = 0.105f;
/// ... or the eye is this near it (metres).
constexpr float kCanvasNearest = 0.08f;

/// An eye's canvas when put down now: its view at `distance`, kCanvasMargin larger each side.
inline EyePlane PlaceCanvas(const Input::Posef& eye, float angle_left, float angle_right,
                            float angle_up, float angle_down, float distance,
                            float margin = kCanvasMargin) {
    constexpr float kMaxAngle = 1.45f; // 83 degrees: the tangent stays finite
    return PlaneForEye(eye, std::max(angle_left - margin, -kMaxAngle),
                       std::min(angle_right + margin, kMaxAngle),
                       std::min(angle_up + margin, kMaxAngle),
                       std::max(angle_down - margin, -kMaxAngle), distance);
}

/// The view an image of the canvas is drawn with, from `eye_position` (LOCAL): the eye where it
/// is, turned square to the canvas, the frustum through the canvas's edges (angles as XrFovf,
/// left and down negative). None when the eye is not in front of it.
struct CanvasEye {
    Input::Posef pose;
    float angle_left = 0.0f, angle_right = 0.0f, angle_up = 0.0f, angle_down = 0.0f;
};
inline std::optional<CanvasEye> CanvasView(const EyePlane& canvas,
                                           const Input::Vec3f& eye_position) {
    const Input::Vec3f e = Input::Transform(Input::Inverse(canvas.pose), eye_position);
    if (e.z < kCanvasNearest * 0.5f) {
        return std::nullopt;
    }
    const float hw = canvas.width_m * 0.5f, hh = canvas.height_m * 0.5f;
    return CanvasEye{{canvas.pose.orientation, eye_position},
                     std::atan((-hw - e.x) / e.z),
                     std::atan((hw - e.x) / e.z),
                     std::atan((hh - e.y) / e.z),
                     std::atan((-hh - e.y) / e.z)};
}

/// Whether `canvas` still serves `eye` (LOCAL, its field of view's angles), with the models in
/// the part `models` of its view ({u0, v0, u1, v1} as ModelsViewBounds gives, none when they are
/// not in view).
inline bool CanvasServes(const EyePlane& canvas, const Input::Posef& eye, float angle_left,
                         float angle_right, float angle_up, float angle_down,
                         const std::optional<std::array<float, 4>>& models) {
    const Input::Posef to_canvas = Input::Inverse(canvas.pose);
    const Input::Vec3f e = Input::Transform(to_canvas, eye.position);
    if (e.z < kCanvasNearest) {
        return false;
    }
    const Input::Vec3f look = Input::Rotate(eye.orientation, {0.0f, 0.0f, -1.0f});
    const Input::Vec3f square = Input::Rotate(canvas.pose.orientation, {0.0f, 0.0f, -1.0f});
    if (Input::Dot(look, square) < std::cos(kCanvasMaxTilt)) {
        return false;
    }
    if (!models) {
        return true;
    }
    const float tl = std::tan(angle_left), tr = std::tan(angle_right);
    const float tu = std::tan(angle_up), td = std::tan(angle_down);
    const auto widen = [](float t, float by) {
        return std::tan(std::clamp(std::atan(t) + by, -1.5f, 1.5f));
    };
    // Widened by the guard, then kept kCanvasEdge in from the view's edges.
    const float x0 = std::max(widen(tl + (tr - tl) * (*models)[0], -kCanvasGuard),
                              std::tan(angle_left + kCanvasEdge));
    const float x1 = std::min(widen(tl + (tr - tl) * (*models)[2], kCanvasGuard),
                              std::tan(angle_right - kCanvasEdge));
    const float y1 = std::min(widen(tu - (tu - td) * (*models)[1], kCanvasGuard),
                              std::tan(angle_up - kCanvasEdge));
    const float y0 = std::max(widen(tu - (tu - td) * (*models)[3], -kCanvasGuard),
                              std::tan(angle_down + kCanvasEdge));
    if (x0 > x1 || y0 > y1) {
        return true; // only at the very edge of the view
    }
    const float hw = canvas.width_m * 0.5f, hh = canvas.height_m * 0.5f;
    for (const float x : {x0, x1}) {
        for (const float y : {y0, y1}) {
            const Input::Vec3f d =
                Input::Rotate(to_canvas.orientation, Input::Rotate(eye.orientation, {x, y, -1.0f}));
            if (d.z >= -1e-6f) {
                return false; // the ray never reaches it
            }
            const float t = -e.z / d.z;
            const float hx = e.x + d.x * t, hy = e.y + d.y * t;
            if (std::fabs(hx) > hw || std::fabs(hy) > hh) {
                return false;
            }
        }
    }
    return true;
}

/// What the eye quads and the models' layer do this refresh.
struct CanvasFrame {
    uint32_t canvas = 0;       ///< the canvas the quads carry and the next image is drawn for (0 none)
    bool quads_shown = false;  ///< the quads at full size (else shrunk to nothing, in place)
    bool projection = false;   ///< the image released last also goes out as the projection layer
};

/// Moving the canvas without the quads ever showing an image at a place it was not drawn for,
/// whatever order SteamVR takes the calls up in (up to kSettle refreshes late each).
class CanvasHandover {
public:
    static constexpr int kSettle = 2;

    /// A new canvas (its number, never 0) wanted: the quads shrink now and move to it once they
    /// have gone out shrunk (or not at all) for kSettle refreshes; at once when they already
    /// have. Ignored while a move waits for that (Busy).
    void Request(uint32_t canvas) {
        if (pending_ == 0) {
            pending_ = canvas;
        }
    }
    bool Busy() const {
        return pending_ != 0;
    }
    bool Shown() const {
        return shown_;
    }
    uint32_t Canvas() const {
        return quads_;
    }

    /// Each refresh the quads go out. `image`: an image has been released (it can go out as the
    /// projection layer); `released`: the canvas it was drawn for (0: drawn for the eyes' own
    /// views).
    CanvasFrame Step(bool image, uint32_t released) {
        if (!image) {
            released = 0;
        }
        if (pending_ != 0 && hidden_ >= kSettle) {
            quads_ = pending_;
            pending_ = 0;
            pose_age_ = 0;
            matched_ = 0;
        }
        CanvasFrame f;
        f.canvas = quads_;
        f.quads_shown = pending_ == 0 && quads_ != 0 && released == quads_ &&
                        pose_age_ >= kSettle && matched_ >= kSettle;
        f.projection = image && !(f.quads_shown && shown_age_ >= kSettle);
        shown_ = f.quads_shown;
        if (f.quads_shown) {
            shown_age_++;
            hidden_ = 0;
        } else {
            shown_age_ = 0;
            hidden_++;
        }
        pose_age_++;
        matched_ = released == quads_ ? matched_ + 1 : 0;
        return f;
    }

    /// Each refresh the quads do not go out (no models, or Off): the layer slots shrink them in
    /// place (layer_slots.h), which counts as hidden.
    void Absent() {
        shown_ = false;
        shown_age_ = 0;
        hidden_++;
        matched_ = 0;
    }

    /// Each refresh the quads go out but not through here (AZAHARFRAME_XR_CANVAS=0): on view at
    /// some other place, so a move back starts with the shrink.
    void Elsewhere() {
        shown_ = true;
        shown_age_ = 0;
        hidden_ = 0;
        matched_ = 0;
    }

    /// AzaharFrame ctrl2: takes over quads already on `canvas` (CanvasDouble's side on view when
    /// the two canvases stop: AZAHARFRAME_XR_CANVAS_DOUBLE=0, or no room for four quads), its
    /// place and its images long settled; `shown`: at full size now.
    void Adopt(uint32_t canvas, bool shown) {
        Reset();
        quads_ = canvas;
        pose_age_ = kSettle;
        if (shown) {
            shown_ = true;
            shown_age_ = kSettle;
            matched_ = kSettle;
        }
    }

    void Reset() {
        *this = {};
    }

private:
    uint32_t quads_ = 0;
    uint32_t pending_ = 0;
    bool shown_ = false;
    int hidden_ = 0;     ///< refreshes in a row the quads went out shrunk (or not at all)
    int shown_age_ = 0;  ///< refreshes in a row they went out at full size
    int pose_age_ = 0;   ///< refreshes they have carried this canvas's place
    int matched_ = 0;    ///< refreshes in a row the image released was drawn for it
};

// ---- AzaharFrame ctrl2: two canvases, the next one made ready while the one on view stays ----
//
// 1.2.0 RC2 (the owner): the controllers and the weapon blinked out at each canvas move (the
// handover above hides them for its few refreshes; up to 22 moves in 10 s), and the 10 degree
// margin that made moves rarer made them soft. Now each canvas has its own pair of eye quads with
// its own swapchain, so its own overlays (layer_slots.h): the pair on view keeps its place and
// its swapchain while the other pair is put at the new place, shrunk, and images for the new
// place go into its swapchain. Once that pair's place and images have gone out for kSettle
// refreshes it grows; the old pair stays at full size until the new one has been for kSettle
// refreshes (both show the models where they are, each drawn for its own place), then shrinks
// in place and waits there for the next move. Whatever order SteamVR takes the calls up in, a
// quad at full size only ever shows an image drawn for its place, and one of the two always
// does. One image is drawn a refresh, as before: during a move it goes to the two swapchains in
// turn. No OpenXR or Vulkan here (unit tested).

/// What the two pairs of eye quads do this refresh.
struct CanvasPairFrame {
    std::array<uint32_t, 2> canvas{}; ///< the canvas each side's quads carry (0: none yet)
    std::array<bool, 2> shown{};      ///< each side's quads at full size (else shrunk, in place)
    int front = -1; ///< the side whose canvas serves the view (-1: none yet)
    int draw = -1;  ///< the side the next image is drawn into, for its canvas (-1: none)
};

class CanvasDouble {
public:
    static constexpr int kSettle = CanvasHandover::kSettle;

    /// A new canvas (its number, never 0) wanted: it goes to the side not on view once that side
    /// has gone out shrunk (or not at all) for kSettle refreshes. Ignored while one is on its way.
    void Request(uint32_t canvas) {
        if (!Busy()) {
            pending_ = canvas;
        }
    }
    bool Busy() const {
        return pending_ != 0 || back_ >= 0;
    }
    /// The newest canvas: the one asked for or being made ready, else the one on view (0: none).
    uint32_t Canvas() const {
        if (pending_ != 0) {
            return pending_;
        }
        if (back_ >= 0) {
            return sides_[static_cast<std::size_t>(back_)].canvas;
        }
        return front_ >= 0 ? sides_[static_cast<std::size_t>(front_)].canvas : 0;
    }
    int Front() const {
        return front_;
    }
    /// The canvas of the side on view, and whether it went out at full size on the last Step.
    uint32_t FrontCanvas() const {
        return front_ >= 0 ? sides_[static_cast<std::size_t>(front_)].canvas : 0;
    }
    bool FrontShown() const {
        return front_ >= 0 && sides_[static_cast<std::size_t>(front_)].shown;
    }
    bool Shown() const {
        return sides_[0].shown || sides_[1].shown;
    }

    /// Each refresh the quads go out. `released[s]`: the canvas the image released last into
    /// side s was drawn for, 0 when none has been since the models last went away.
    CanvasPairFrame Step(const std::array<uint32_t, 2>& released) {
        if (pending_ != 0 && back_ < 0 && retiring_ < 0) {
            const int s = front_ < 0 ? 0 : 1 - front_;
            Side& b = sides_[static_cast<std::size_t>(s)];
            if (b.hidden >= kSettle) {
                b.canvas = pending_;
                b.pose_age = 0;
                b.matched = 0;
                back_ = s;
                pending_ = 0;
            }
        }
        const auto ready = [&](int s) {
            const Side& d = sides_[static_cast<std::size_t>(s)];
            return d.canvas != 0 && released[static_cast<std::size_t>(s)] == d.canvas &&
                   d.pose_age >= kSettle && d.matched >= kSettle;
        };
        // The new pair ready: on view from now, the old one until the new one has been for kSettle.
        if (back_ >= 0 && ready(back_)) {
            if (front_ >= 0 && sides_[static_cast<std::size_t>(front_)].shown) {
                retiring_ = front_;
            }
            front_ = back_;
            back_ = -1;
        }
        CanvasPairFrame f;
        f.front = front_;
        for (std::size_t s = 0; s < 2; s++) {
            f.canvas[s] = sides_[s].canvas;
        }
        if (front_ >= 0) {
            f.shown[static_cast<std::size_t>(front_)] = ready(front_);
        }
        if (retiring_ >= 0) {
            const Side& fr = sides_[static_cast<std::size_t>(front_)];
            if (f.shown[static_cast<std::size_t>(front_)] && fr.shown && fr.shown_age >= kSettle) {
                retiring_ = -1; // shrunk from now, in place
            } else {
                f.shown[static_cast<std::size_t>(retiring_)] = true;
            }
        }
        // One image a refresh: the new pair's first, then the two pairs on view (or being made
        // ready) in turn, so neither shows the controllers more than a refresh or two old.
        if (back_ >= 0) {
            const bool front_on = front_ >= 0 && f.shown[static_cast<std::size_t>(front_)];
            const std::size_t b = static_cast<std::size_t>(back_);
            const bool back_has = released[b] == sides_[b].canvas;
            f.draw = !front_on || !back_has || (tick_ & 1) ? back_ : front_;
        } else if (retiring_ >= 0) {
            f.draw = (tick_ & 1) ? retiring_ : front_;
        } else {
            f.draw = front_;
        }
        tick_++;
        for (std::size_t s = 0; s < 2; s++) {
            Side& d = sides_[s];
            if (f.shown[s]) {
                d.shown_age++;
                d.hidden = 0;
            } else {
                d.shown_age = 0;
                d.hidden++;
            }
            d.shown = f.shown[s];
            d.pose_age++;
            d.matched = d.canvas != 0 && released[s] == d.canvas ? d.matched + 1 : 0;
        }
        return f;
    }

    /// Each refresh the quads do not go out (no models, or Off): both pairs shrink in place
    /// (layer_slots.h); a pair goes out again once images drawn since have been for kSettle.
    void Absent() {
        for (Side& d : sides_) {
            d.shown = false;
            d.shown_age = 0;
            d.hidden++;
            d.matched = 0;
        }
        retiring_ = -1;
    }

    /// Takes over the one pair the single handover had on `canvas` (side `side`), as
    /// CanvasHandover::Adopt; the other pair waits kSettle refreshes before it is used.
    void Adopt(int side, uint32_t canvas, bool shown) {
        Reset();
        front_ = side;
        Side& d = sides_[static_cast<std::size_t>(side)];
        d.canvas = canvas;
        d.pose_age = kSettle;
        if (shown) {
            d.shown = true;
            d.shown_age = kSettle;
            d.matched = kSettle;
        }
    }

    void Reset() {
        *this = {};
    }

private:
    struct Side {
        uint32_t canvas = 0;
        bool shown = false;
        int hidden = 0;    ///< refreshes in a row it went out shrunk (or not at all)
        int shown_age = 0; ///< refreshes in a row it went out at full size
        int pose_age = 0;  ///< refreshes it has carried its canvas's place
        int matched = 0;   ///< refreshes in a row the image released into it was drawn for it
    };
    std::array<Side, 2> sides_{};
    int front_ = -1;    ///< the side on view (or the first canvas's)
    int back_ = -1;     ///< the side being made ready for a new canvas
    int retiring_ = -1; ///< the side still at full size after a switch
    uint32_t pending_ = 0;
    uint32_t tick_ = 0;
};

/// AzaharFrame fixc: why a refresh that showed a controller (or had just lost one) went out
/// without the controllers' layer: the hunt for the controllers' flicker. Counted per second
/// (LayerGapLog), each refresh's state judged by LayerGapFor once the refresh is done.
enum class LayerGap : uint8_t {
    Rebuilt,      ///< the pass made again at a path change (projection screens on or off)
    SettingUp,    ///< the files read and the pass set up (the first time)
    Failed,       ///< the pass could not be set up
    NoImage,      ///< no image released yet
    Returning,    ///< the controllers came back: a new image first
    CanvasMove,   ///< the eye quads shrunk while their canvas moves, and no projection layer
    NoCanvas,     ///< the eye quads' canvas not found
    NoRoom,       ///< no room among the layers (the runtime's limit)
    NoScreens,    ///< projection screens: not drawn with the screens this refresh
    TrackingLost, ///< no controller shown: one shown within kLayerGapLostNs lost its tracking
    Count,
};

/// A controller that showed this long ago or less and is no longer tracked counts as lost (a
/// longer loss, such as the SteamVR dashboard's, is not counted).
inline constexpr int64_t kLayerGapLostNs = 1'000'000'000;

/// One refresh's state, as the models left it.
struct LayerGapState {
    bool wanted = false;   ///< a controller shown (on a refresh the runtime draws)
    bool lost = false;     ///< none shown: one shown within kLayerGapLostNs is not tracked
    bool rebuilt = false;  ///< the pass was made again at a path change, nothing shown since
    bool failed = false;
    bool ready = false;    ///< the files read and the pass set up (and uploaded)
    bool has_image = false;
    bool fresh = false;    ///< an image released since the controllers last went away
    bool screens = false;  ///< projection screens (the layer comes from FrameScreens)
    bool screens_layer = false; ///< FrameScreens gave the layer
    bool as_quads = false;      ///< the image goes out as the eye quads
    bool canvas_on = false;     ///< on the quads' canvas (model_plane.h)
    bool canvas_found = false;
    bool quads_shown = false;   ///< the quads at full size (CanvasFrame)
    bool projection = false;    ///< the projection layer goes too (CanvasFrame)
    bool quads_dropped = false; ///< the quads did not fit among the layers
    bool layer_dropped = false; ///< the projection layer did not fit
};

/// The reason the controllers were not on view that refresh; none when they were (or none was
/// wanted).
inline std::optional<LayerGap> LayerGapFor(const LayerGapState& s) {
    if (!s.wanted) {
        return s.lost ? std::optional{LayerGap::TrackingLost} : std::nullopt;
    }
    if (s.failed) {
        return LayerGap::Failed;
    }
    if (!s.ready) {
        return s.rebuilt ? LayerGap::Rebuilt : LayerGap::SettingUp;
    }
    if (!s.has_image) {
        return s.rebuilt ? LayerGap::Rebuilt : LayerGap::NoImage;
    }
    if (!s.fresh) {
        return LayerGap::Returning;
    }
    if (s.screens) {
        return s.screens_layer ? std::nullopt : std::optional{LayerGap::NoScreens};
    }
    const bool quads = s.as_quads && (!s.canvas_on || (s.canvas_found && s.quads_shown));
    const bool layer = !s.as_quads || (s.canvas_on && s.projection);
    if (!quads && !layer) {
        return s.canvas_on && !s.canvas_found ? LayerGap::NoCanvas : LayerGap::CanvasMove;
    }
    if ((quads && !s.quads_dropped) || (layer && !s.layer_dropped)) {
        return std::nullopt;
    }
    return LayerGap::NoRoom;
}

/// The refreshes without the controllers' layer, by reason: a line each second there were any.
class LayerGapLog {
public:
    static constexpr int64_t kPeriodNs = 1'000'000'000;

    void Gap(LayerGap why) {
        if (why < LayerGap::Count) {
            gaps_[static_cast<std::size_t>(why)]++;
        }
    }
    /// The line to log at `now_ns`, every kPeriodNs, when there were gaps in that time
    /// (cleared then); none otherwise.
    std::optional<std::string> Take(int64_t now_ns) {
        if (start_ns_ < 0 || now_ns < start_ns_) {
            start_ns_ = now_ns;
        }
        if (now_ns - start_ns_ < kPeriodNs) {
            return std::nullopt;
        }
        start_ns_ = now_ns;
        static constexpr const char* kWhy[] = {
            "the pass made again (path change)", "the pass being set up", "the pass failed",
            "no image yet", "a new image after they came back", "the quads' canvas moving",
            "no canvas", "no room among the layers", "not drawn with the screens",
            "tracking lost"};
        static_assert(std::size(kWhy) == static_cast<std::size_t>(LayerGap::Count));
        uint32_t total = 0;
        std::string why;
        for (std::size_t k = 0; k < gaps_.size(); k++) {
            if (gaps_[k]) {
                total += gaps_[k];
                why += (why.empty() ? "" : ", ") + std::to_string(gaps_[k]) + " " + kWhy[k];
            }
        }
        gaps_ = {};
        if (total == 0) {
            return std::nullopt;
        }
        return "xr: controllers: " + std::to_string(total) +
               (total == 1 ? " refresh" : " refreshes") + " without their layer: " + why;
    }

private:
    int64_t start_ns_ = -1;
    std::array<uint32_t, static_cast<std::size_t>(LayerGap::Count)> gaps_{};
};

} // namespace FrameXr::Vis
