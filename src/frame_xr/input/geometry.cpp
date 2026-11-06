// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "frame_xr/input/geometry.h"

// AzaharFrame
namespace FrameXr::Input {

Vec3f operator+(const Vec3f& a, const Vec3f& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3f operator-(const Vec3f& a, const Vec3f& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3f operator*(const Vec3f& a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}

float Dot(const Vec3f& a, const Vec3f& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3f Cross(const Vec3f& a, const Vec3f& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float Length(const Vec3f& a) {
    return std::sqrt(Dot(a, a));
}

Vec3f Normalized(const Vec3f& a) {
    const float len = Length(a);
    if (len < 1e-9f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return a * (1.0f / len);
}

Quatf Normalized(const Quatf& q) {
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len < 1e-9f) {
        return Quatf{};
    }
    const float inv = 1.0f / len;
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

Quatf Conjugate(const Quatf& q) {
    return {-q.x, -q.y, -q.z, q.w};
}

Quatf Multiply(const Quatf& a, const Quatf& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

Vec3f Rotate(const Quatf& q, const Vec3f& v) {
    // v' = v + 2w (u x v) + 2 u x (u x v), with u the vector part.
    const Vec3f u{q.x, q.y, q.z};
    const Vec3f uv = Cross(u, v);
    const Vec3f uuv = Cross(u, uv);
    return v + (uv * q.w + uuv) * 2.0f;
}

Quatf FromBasis(const Vec3f& xa, const Vec3f& ya, const Vec3f& za) {
    // Rotation matrix with columns xa, ya, za.
    const float m00 = xa.x, m01 = ya.x, m02 = za.x;
    const float m10 = xa.y, m11 = ya.y, m12 = za.y;
    const float m20 = xa.z, m21 = ya.z, m22 = za.z;
    Quatf q;
    const float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return Normalized(q);
}

Posef Inverse(const Posef& pose) {
    const Quatf inv = Conjugate(Normalized(pose.orientation));
    return {inv, Rotate(inv, pose.position * -1.0f)};
}

Vec3f Transform(const Posef& pose, const Vec3f& point) {
    return Rotate(pose.orientation, point) + pose.position;
}

Ray RayFromPose(const Posef& aim) {
    return {aim.position, Normalized(Rotate(aim.orientation, Vec3f{0.0f, 0.0f, -1.0f}))};
}

QuadHit IntersectQuad(const Ray& ray, const PanelQuad& quad) {
    QuadHit hit;
    if (!quad.visible || quad.width_m <= 0.0f || quad.height_m <= 0.0f) {
        return hit;
    }
    // Solve in the quad's space, where the quad is the rectangle z = 0 and faces +Z.
    const Posef quad_from_world = Inverse(quad.pose);
    const Vec3f o = Transform(quad_from_world, ray.origin);
    const Vec3f d = Rotate(quad_from_world.orientation, ray.direction);
    constexpr float kEpsilon = 1e-6f;
    // Must start in front of the quad and travel toward it.
    if (o.z < 0.0f || d.z > -kEpsilon) {
        return hit;
    }
    const float t = -o.z / d.z;
    if (t < 0.0f) {
        return hit;
    }
    const float lx = o.x + d.x * t;
    const float ly = o.y + d.y * t;
    hit.on_plane = true;
    hit.t = t;
    hit.point_world = ray.origin + ray.direction * t;
    hit.u = lx / quad.width_m + 0.5f;
    hit.v = 0.5f - ly / quad.height_m;
    hit.in_bounds = hit.u >= 0.0f && hit.u < 1.0f && hit.v >= 0.0f && hit.v < 1.0f;
    return hit;
}

Vec2f HitToPixels(const QuadHit& hit, float w, float h, bool clamp) {
    float x = hit.u * w;
    float y = hit.v * h;
    if (clamp) {
        // Stay inside the last pixel: 320.0 itself is off the screen.
        x = std::clamp(x, 0.0f, std::nextafter(w, 0.0f));
        y = std::clamp(y, 0.0f, std::nextafter(h, 0.0f));
    }
    return {x, y};
}

Quatf FaceViewer(const Vec3f& panel_position, const Vec3f& viewer, const Vec3f& up) {
    const Vec3f forward = Normalized(viewer - panel_position); // panel +Z
    Vec3f right = Normalized(Cross(up, forward));
    if (Length(right) < 1e-6f) {
        // Looking straight along the up axis: any right will do.
        right = {1.0f, 0.0f, 0.0f};
    }
    const Vec3f new_up = Cross(forward, right);
    return FromBasis(right, new_up, forward);
}

std::optional<Posef> SolveSphereDrag(const Posef& current, const Vec3f& target_point,
                                     const Vec3f& viewer, const Vec3f* other_panel_position,
                                     const DragLimits& limits) {
    const float radius = Length(current.position - viewer);
    const Vec3f to_target = target_point - viewer;
    // As documented: a target closer than max_z (or behind the viewer) is refused. (The
    // old check tested the normalized direction's length, which is always 1.)
    if (to_target.z >= limits.max_z || Length(to_target) < 1e-4f) {
        return std::nullopt;
    }
    const Vec3f direction = Normalized(to_target);
    const Vec3f position = viewer + direction * radius;
    if (position.z >= limits.max_z) {
        return std::nullopt;
    }
    if (other_panel_position != nullptr && limits.min_distance_from_other > 0.0f) {
        if (Length(position - *other_panel_position) < limits.min_distance_from_other) {
            return std::nullopt;
        }
    }
    return Posef{FaceViewer(position, viewer, Vec3f{0.0f, 1.0f, 0.0f}), position};
}

float ApplyDepth(float current_z, float delta_z, float nearest_z, float max_depth_z) {
    float z = current_z + delta_z;
    z = std::min(z, nearest_z);
    z = std::max(z, max_depth_z);
    return z;
}

} // namespace FrameXr::Input
