// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include "frame_xr/input/types.h"

// AzaharFrame
// Ray / quad intersection and the panel drag math (CitraVR's sphere drag), with no OpenXR calls.
namespace FrameXr::Input {

// ---- Small vector and quaternion helpers ---------------------------------------------------

Vec3f operator+(const Vec3f& a, const Vec3f& b);
Vec3f operator-(const Vec3f& a, const Vec3f& b);
Vec3f operator*(const Vec3f& a, float s);
float Dot(const Vec3f& a, const Vec3f& b);
Vec3f Cross(const Vec3f& a, const Vec3f& b);
float Length(const Vec3f& a);
Vec3f Normalized(const Vec3f& a);

Quatf Normalized(const Quatf& q);
Quatf Conjugate(const Quatf& q);
Quatf Multiply(const Quatf& a, const Quatf& b);
Vec3f Rotate(const Quatf& q, const Vec3f& v);
/// Rotation from a right handed basis (columns: x axis, y axis, z axis).
Quatf FromBasis(const Vec3f& x_axis, const Vec3f& y_axis, const Vec3f& z_axis);

Posef Inverse(const Posef& pose);
Vec3f Transform(const Posef& pose, const Vec3f& point);

// ---- Rays and quads --------------------------------------------------------------------------

struct Ray {
    Vec3f origin;
    Vec3f direction; ///< unit length
};

/// The pointing ray of an aim pose: starts at the pose, goes along its -Z axis.
Ray RayFromPose(const Posef& aim);

/// A flat rectangle in space, facing +Z of its pose. width/height are in metres.
struct PanelQuad {
    Posef pose;
    float width_m = 0.0f;
    float height_m = 0.0f;
    /// Not visible quads are never hit.
    bool visible = false;
    /// AzaharFrame curve: above 0, the rectangle is bent about the vertical line (its +Y) this
    /// far in front of it (+Z), as a cylinder layer: its middle stays at the pose, its width is
    /// the arc's length, and the sides come towards the viewer (screen_rig.h).
    float curve_radius = 0.0f;
};

struct QuadHit {
    /// The ray reaches the quad's plane from the front at t >= 0.
    bool on_plane = false;
    /// on_plane and the point is inside the rectangle.
    bool in_bounds = false;
    /// Distance along the ray (metres).
    float t = 0.0f;
    Vec3f point_world;
    /// Position in the quad, 0..1 inside, u from the left edge, v from the TOP edge. Not clamped,
    /// so a point outside the rectangle has values below 0 or above 1.
    float u = 0.0f;
    float v = 0.0f;
};

/// Intersects the ray with the quad plane. A ray from behind the quad, parallel to it, or
/// pointing away is not a hit (on_plane false). A curved quad (curve_radius) is hit on the half
/// of its cylinder in front of the axis, from the inside, with u along the arc; a ray that
/// misses that half gets the flat plane's answer, so a drag or a touch carries on off the edge.
QuadHit IntersectQuad(const Ray& ray, const PanelQuad& quad);

/// Pixel position in a screen of the given size for a hit. clamp keeps the result on the screen.
Vec2f HitToPixels(const QuadHit& hit, float screen_w_px, float screen_h_px, bool clamp);

// ---- Panel dragging (CitraVR GameSurfaceLayer::SetTopPanelFromController / FromThumbstick) -----

/// Panel orientation that faces the viewer: +Z toward the viewer, +Y as close to up as possible.
Quatf FaceViewer(const Vec3f& panel_position, const Vec3f& viewer, const Vec3f& up);

struct DragLimits {
    /// A drag target whose z is at or above this (closer than 0.5 m, or behind) is refused.
    float max_z = -0.5f;
    /// The panel must stay at least this far from the other (bottom) panel; 0 disables the check.
    float min_distance_from_other = 0.0f;
};

/// Moves a panel on the sphere around the viewer that it is on (radius = its current distance) to
/// the direction of `target_point`, turned to face the viewer. Returns nothing (the panel stays)
/// when the move is refused by the limits.
std::optional<Posef> SolveSphereDrag(const Posef& current, const Vec3f& target_point,
                                     const Vec3f& viewer, const Vec3f* other_panel_position,
                                     const DragLimits& limits);

/// New z for a panel pushed with the thumbstick: z + delta clamped to [max_depth_z, nearest_z].
/// (CitraVR: further than -10 m is refused, and never nearer than the bottom panel's z.)
float ApplyDepth(float current_z, float delta_z, float nearest_z, float max_depth_z = -10.0f);

} // namespace FrameXr::Input
