// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame projscreen (a spike behind the live test switch AZAHARFRAME_XR_SCREENS_IN_PROJECTION=1)
//
// SteamVR 2.19.3 turns every OpenXR quad layer into an IVROverlay overlay whose texture, transform
// and visibility are separate calls with no display time and no fence (reports/55/flash/
// overlay-path.md). So a quad's image and its pose need not reach the screen together, and a
// picture held over two refreshes can be shown with the pose of the picture before or after it:
// the doubling. A projection layer is one submit, image and views together.
//
// With the switch on, the top game screen is not sent as a quad: the XR thread draws it into the
// controller models' projection layer (controller_models.h) every refresh, as a textured quad (or
// a section of a cylinder) at exactly the pose and size the quad would have had (Immersive::Screens
// and ScreenDraw, the same per-eye quads, HeldQuad warp included), seen through that refresh's own
// eye poses. The controller models are drawn after it with the depth test, so a controller nearer
// than the screen is in front of it. The bottom screen, panels and menus stay quads. Pure parts
// here (unit-tested), the GPU side in controller_models.cpp.

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>


namespace FrameXr::Vis {

/// The live test switch (test_switches.h kLive).
inline constexpr const char* kScreensInProjectionSwitch = "AZAHARFRAME_XR_SCREENS_IN_PROJECTION";
/// The eye buffer's size, a factor of the runtime's recommended size (live: the pass is made again).
inline constexpr const char* kProjScaleSwitch = "AZAHARFRAME_XR_PROJ_SCALE";
/// Test marks for the Frame mock (xrmock MOCK_XR_PROJ_SCAN, hold.py --proj): magenta Gaussian dots
/// on black discs at fixed points of the picture, and the picture's serial as a strip of cells in
/// each eye's corner.
inline constexpr const char* kProjMarksSwitch = "AZAHARFRAME_XR_PROJ_MARKS";
/// Test only: the top screen's quads are sent as well (over the projection layer), so a mock trace
/// has both for the same refresh (hold.py --proj-vs-quad).
inline constexpr const char* kProjAlsoQuadsSwitch = "AZAHARFRAME_XR_PROJ_ALSO_QUADS";

// ---- Pure helpers ----

// Templates over any quaternion (x, y, z, w) and vector (x, y, z) types with those members, so
// XrPosef here and plain structs in the tests use the same code.
template <typename Q>
Q QuatMul(const Q& a, const Q& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

template <typename Q, typename V>
V QuatRotate(const Q& q, const V& v) {
    const float tx = 2.0f * (q.y * v.z - q.z * v.y);
    const float ty = 2.0f * (q.z * v.x - q.x * v.z);
    const float tz = 2.0f * (q.x * v.y - q.y * v.x);
    return {v.x + q.w * tx + (q.y * tz - q.z * ty), v.y + q.w * ty + (q.z * tx - q.x * tz),
            v.z + q.w * tz + (q.x * ty - q.y * tx)};
}

/// `inner` (relative to `outer`) in outer's space: a VIEW-space quad in LOCAL through the head.
template <typename Pose>
Pose ComposePose(const Pose& outer, const Pose& inner) {
    const auto p = QuatRotate(outer.orientation, inner.position);
    Pose out = outer;
    out.orientation = QuatMul(outer.orientation, inner.orientation);
    out.position.x = outer.position.x + p.x;
    out.position.y = outer.position.y + p.y;
    out.position.z = outer.position.z + p.z;
    return out;
}

/// A point of the screen in the quad's own space, from screen coordinates u, v in -1..1 (u to the
/// right, v up), for a screen `width` x `height`: flat, or on the cylinder MakeCylinder sends
/// (input/screen_rig CylinderFor: the axis `radius` behind the middle, the arc the screen's width
/// long). The vertex shader does the same.
inline std::array<float, 3> ScreenPoint(float u, float v, float width, float height, float radius) {
    const float y = v * height * 0.5f;
    if (radius <= 0.0f) {
        return {u * width * 0.5f, y, 0.0f};
    }
    const float phi = u * width * 0.5f / radius;
    return {radius * std::sin(phi), y, radius - radius * std::cos(phi)};
}

/// The eye buffer's scale from AZAHARFRAME_XR_PROJ_SCALE: 0.5 to 2 (unset or not a number: 1).
inline float ProjScaleFrom(const std::optional<std::string>& v) {
    if (!v || v->empty()) {
        return 1.0f;
    }
    char* end = nullptr;
    const float f = std::strtof(v->c_str(), &end);
    if (end == v->c_str() || !std::isfinite(f)) {
        return 1.0f;
    }
    return f < 0.5f ? 0.5f : f > 2.0f ? 2.0f : f;
}

/// The test marks: the picture points (u, v in -1..1) the magenta dots sit on, row by row from
/// the top, left to right. hold.py --proj reads them in this order.
inline constexpr std::array<std::array<float, 2>, 15> kProjMarks{{
    {-0.5f, 0.5f}, {-0.25f, 0.5f}, {0.0f, 0.5f}, {0.25f, 0.5f}, {0.5f, 0.5f},
    {-0.5f, 0.0f}, {-0.25f, 0.0f}, {0.0f, 0.0f}, {0.25f, 0.0f}, {0.5f, 0.0f},
    {-0.5f, -0.5f}, {-0.25f, -0.5f}, {0.0f, -0.5f}, {0.25f, -0.5f}, {0.5f, -0.5f},
}};
/// The serial strip: kProjCodeBits cells of kProjCodeCell pixels in the top left corner of each
/// eye's part of the image, white for a 1 (least significant first), then a white and a black
/// cell (the strip's end, so a reader knows it is there).
inline constexpr uint32_t kProjCodeBits = 16;
inline constexpr uint32_t kProjCodeCell = 4;

namespace Shaders {

// The screen: a grid of `columns` x 1 cells (2 triangles each, no vertex buffer), flat or bent on
// a cylinder as ScreenPoint, the picture's sub-rectangle on it (OpenXR: the image's top row at the
// quad's top). Fragment: the picture filtered over each pixel's footprint (up to 4 x 4 bilinear
// taps where the picture is minified; the swapchain has no mips), opaque, plus the test marks.
inline constexpr const char* kScreenVert = R"(
layout(push_constant) uniform PC {
    mat4 mvp;     // proj * view * quad pose (OpenGL clip space, turned into Vulkan's below)
    vec4 shape;   // x, y: half width and height (m), z: cylinder radius (0: flat), w: columns
    vec4 rect;    // the eye's part of the image: offset (x, y) and size (z, w), 0..1
    vec4 tex;     // x, y: the image's size in texels, z: flags (1: test marks)
    vec4 extra;
} pc;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec2 v_screen;

void main() {
    int cell = gl_VertexIndex / 6;
    int corner = gl_VertexIndex % 6;
    // two triangles: (0,0) (1,0) (1,1) / (0,0) (1,1) (0,1)
    float cx = (corner == 1 || corner == 2 || corner == 4) ? 1.0 : 0.0;
    float cy = (corner == 2 || corner == 4 || corner == 5) ? 1.0 : 0.0;
    float columns = max(pc.shape.w, 1.0);
    float s = (float(cell) + cx) / columns; // 0..1 across
    float u = s * 2.0 - 1.0;
    float v = cy * 2.0 - 1.0;
    vec3 p;
    if (pc.shape.z > 0.0) {
        float phi = u * pc.shape.x / pc.shape.z;
        p = vec3(pc.shape.z * sin(phi), v * pc.shape.y, pc.shape.z - pc.shape.z * cos(phi));
    } else {
        p = vec3(u * pc.shape.x, v * pc.shape.y, 0.0);
    }
    v_uv = pc.rect.xy + vec2(s, 1.0 - cy) * pc.rect.zw;
    v_screen = vec2(u, v);
    vec4 c = pc.mvp * vec4(p, 1.0);
    gl_Position = vec4(c.x, -c.y, 0.5 * (c.z + c.w), c.w);
}
)";

inline constexpr const char* kScreenFrag = R"(
layout(push_constant) uniform PC {
    mat4 mvp;
    vec4 shape;
    vec4 rect;
    vec4 tex;
    vec4 extra;
} pc;

layout(set = 0, binding = 0) uniform sampler2D u_picture;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec2 v_screen;
layout(location = 0) out vec4 fragColor;

const vec2 MARKS[15] = vec2[15](
    vec2(-0.5, 0.5), vec2(-0.25, 0.5), vec2(0.0, 0.5), vec2(0.25, 0.5), vec2(0.5, 0.5),
    vec2(-0.5, 0.0), vec2(-0.25, 0.0), vec2(0.0, 0.0), vec2(0.25, 0.0), vec2(0.5, 0.0),
    vec2(-0.5, -0.5), vec2(-0.25, -0.5), vec2(0.0, -0.5), vec2(0.25, -0.5), vec2(0.5, -0.5));

void main() {
    // The pixel's footprint on the picture, in texels along the picture's axes.
    vec2 dx = dFdx(v_uv), dy = dFdy(v_uv);
    vec2 fx = dx * pc.tex.xy, fy = dy * pc.tex.xy;
    vec2 foot = vec2(length(vec2(fx.x, fy.x)), length(vec2(fx.y, fy.y)));
    ivec2 n = ivec2(clamp(ceil(foot), vec2(1.0), vec2(4.0)));
    vec3 sum = vec3(0.0);
    for (int j = 0; j < 4; j++) {
        if (j >= n.y) break;
        for (int i = 0; i < 4; i++) {
            if (i >= n.x) break;
            float ox = (float(i) + 0.5) / float(n.x) - 0.5;
            float oy = (float(j) + 0.5) / float(n.y) - 0.5;
            sum += texture(u_picture, v_uv + ox * dx + oy * dy).rgb;
        }
    }
    vec3 colour = sum / float(n.x * n.y);
    if (mod(pc.tex.z, 2.0) > 0.5) {
        // Test marks: on each mark's picture point a magenta Gaussian dot (sigma 1.2 pixels) on
        // a black disc 5.5 pixels round, so a reader can take its weighted centre to a hundredth
        // of a pixel (xrmock MOCK_XR_PROJ_SCAN).
        vec2 px = max(fwidth(v_screen), vec2(1e-6));
        for (int k = 0; k < 15; k++) {
            vec2 d = (v_screen - MARKS[k]) / px;
            float r2 = dot(d, d);
            if (r2 < 30.25) {
                float w = exp(-r2 / (2.0 * 1.44));
                colour = vec3(w, 0.0, w);
            }
        }
    }
    fragColor = vec4(colour, 1.0);
}
)";

} // namespace Shaders
} // namespace FrameXr::Vis
