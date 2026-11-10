// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/geometry.h"
#include "tests/frame_xr/input_test_util.h"

// AzaharFrame
using Catch::Approx;
using namespace FrameXr::Input;
using namespace FrameXr::Input::TestUtil;

namespace {
PanelQuad TopQuad() {
    return StandardPanels().top;
}
} // namespace

TEST_CASE("FrameXr geometry: pointing ray", "[frame_xr]") {
    SECTION("identity pose points down -Z") {
        const Ray ray = RayFromPose(Posef{});
        REQUIRE(ray.direction.x == Approx(0.0f).margin(1e-6));
        REQUIRE(ray.direction.y == Approx(0.0f).margin(1e-6));
        REQUIRE(ray.direction.z == Approx(-1.0f));
    }
    SECTION("a quarter turn about Y points down -X") {
        const float s = std::sqrt(0.5f);
        const Ray ray = RayFromPose(Posef{Quatf{0.0f, s, 0.0f, s}, Vec3f{}});
        REQUIRE(ray.direction.x == Approx(-1.0f));
        REQUIRE(ray.direction.z == Approx(0.0f).margin(1e-6));
    }
    SECTION("AimAt helper aims where it says") {
        const Ray ray = RayFromPose(AimAt(Vec3f{1.0f, 1.0f, 0.0f}, Vec3f{1.0f, 1.0f, -4.0f}));
        REQUIRE(ray.direction.z == Approx(-1.0f));
        REQUIRE(ray.origin.x == Approx(1.0f));
    }
}

TEST_CASE("FrameXr geometry: ray and quad", "[frame_xr]") {
    const PanelQuad quad = TopQuad(); // z = -2, 1.6 x 0.96

    SECTION("straight on hits the centre") {
        const QuadHit hit = IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{0, 0, -1}}, quad);
        REQUIRE(hit.on_plane);
        REQUIRE(hit.in_bounds);
        REQUIRE(hit.t == Approx(2.0f));
        REQUIRE(hit.u == Approx(0.5f));
        REQUIRE(hit.v == Approx(0.5f));
        REQUIRE(hit.point_world.z == Approx(-2.0f));
    }
    SECTION("u runs left to right and v top to bottom") {
        // 0.4 right of the centre of a 1.6 wide quad is u = 0.75; 0.24 above the centre of a 0.96
        // tall quad is v = 0.25.
        const QuadHit hit = IntersectQuad(Ray{Vec3f{0.4f, 0.24f, 0}, Vec3f{0, 0, -1}}, quad);
        REQUIRE(hit.in_bounds);
        REQUIRE(hit.u == Approx(0.75f));
        REQUIRE(hit.v == Approx(0.25f));
    }
    SECTION("outside the rectangle is on the plane but not in bounds") {
        const QuadHit hit = IntersectQuad(Ray{Vec3f{1.0f, 0, 0}, Vec3f{0, 0, -1}}, quad);
        REQUIRE(hit.on_plane);
        REQUIRE_FALSE(hit.in_bounds);
        REQUIRE(hit.u > 1.0f);
    }
    SECTION("a ray pointing away misses") {
        REQUIRE_FALSE(IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{0, 0, 1}}, quad).on_plane);
    }
    SECTION("a ray from behind the quad misses") {
        REQUIRE_FALSE(IntersectQuad(Ray{Vec3f{0, 0, -3.0f}, Vec3f{0, 0, 1}}, quad).on_plane);
    }
    SECTION("a ray parallel to the quad misses") {
        REQUIRE_FALSE(IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{1, 0, 0}}, quad).on_plane);
    }
    SECTION("an invisible or empty quad is never hit") {
        PanelQuad hidden = quad;
        hidden.visible = false;
        REQUIRE_FALSE(IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{0, 0, -1}}, hidden).on_plane);
        PanelQuad empty = quad;
        empty.width_m = 0.0f;
        REQUIRE_FALSE(IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{0, 0, -1}}, empty).on_plane);
    }
    SECTION("a rotated quad is hit in its own space") {
        // Quad 2 m to the left facing +X (a quarter turn about Y turns +Z into +X).
        const float s = std::sqrt(0.5f);
        PanelQuad left = quad;
        left.pose = Posef{Quatf{0.0f, s, 0.0f, s}, Vec3f{-2.0f, 0.0f, 0.0f}};
        const QuadHit hit = IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{-1, 0, 0}}, left);
        REQUIRE(hit.in_bounds);
        REQUIRE(hit.t == Approx(2.0f));
        REQUIRE(hit.u == Approx(0.5f));
    }
}

TEST_CASE("FrameXr geometry: pixels and poses", "[frame_xr]") {
    SECTION("the centre of the bottom screen is pixel 160,120") {
        QuadHit hit;
        hit.u = 0.5f;
        hit.v = 0.5f;
        const Vec2f px = HitToPixels(hit, kBottomScreenWidthPx, kBottomScreenHeightPx, true);
        REQUIRE(px.x == Approx(160.0f));
        REQUIRE(px.y == Approx(120.0f));
    }
    SECTION("clamping keeps a point on the screen") {
        QuadHit hit;
        hit.u = 1.4f;
        hit.v = -0.3f;
        const Vec2f px = HitToPixels(hit, kBottomScreenWidthPx, kBottomScreenHeightPx, true);
        REQUIRE(px.x < 320.0f);
        REQUIRE(px.x > 319.0f);
        REQUIRE(px.y == Approx(0.0f));
    }
    SECTION("inverse undoes the transform") {
        const Posef pose{Quatf{0.0f, std::sqrt(0.5f), 0.0f, std::sqrt(0.5f)}, Vec3f{1, 2, 3}};
        const Vec3f p = Transform(Inverse(pose), Transform(pose, Vec3f{0.3f, -0.2f, 0.9f}));
        REQUIRE(p.x == Approx(0.3f).margin(1e-5));
        REQUIRE(p.y == Approx(-0.2f).margin(1e-5));
        REQUIRE(p.z == Approx(0.9f).margin(1e-5));
    }
}

TEST_CASE("FrameXr geometry: dragging a panel", "[frame_xr]") {
    const Vec3f viewer{0, 0, 0};
    const Posef current{Quatf{}, Vec3f{0, 0, -2.0f}};

    SECTION("facing the viewer from straight ahead is no turn at all") {
        const Quatf q = FaceViewer(Vec3f{0, 0, -2.0f}, viewer, Vec3f{0, 1, 0});
        REQUIRE(std::abs(q.w) == Approx(1.0f));
    }
    SECTION("the panel stays on its sphere and turns to face the viewer") {
        const auto moved = SolveSphereDrag(current, Vec3f{1.0f, 0, -1.0f}, viewer, nullptr, {});
        REQUIRE(moved.has_value());
        REQUIRE(Length(moved->position) == Approx(2.0f));
        REQUIRE(moved->position.x == Approx(std::sqrt(2.0f)).epsilon(1e-4));
        const Vec3f facing = Rotate(moved->orientation, Vec3f{0, 0, 1});
        const Vec3f to_viewer = Normalized(viewer - moved->position);
        REQUIRE(Dot(facing, to_viewer) == Approx(1.0f).epsilon(1e-4));
    }
    SECTION("a target closer than 0.5 m (or behind) is refused") {
        REQUIRE_FALSE(SolveSphereDrag(current, Vec3f{0, 0, -0.2f}, viewer, nullptr, {}));
        REQUIRE_FALSE(SolveSphereDrag(current, Vec3f{0, 0, 1.0f}, viewer, nullptr, {}));
    }
    SECTION("a panel may not be dragged onto the other one") {
        const Vec3f bottom{0.0f, -0.75f, -1.5f};
        DragLimits limits;
        limits.min_distance_from_other = 0.9f;
        REQUIRE_FALSE(SolveSphereDrag(current, Vec3f{0.0f, -0.5f, -2.0f}, viewer, &bottom,
                                      limits));
        REQUIRE(SolveSphereDrag(current, Vec3f{0.0f, 0.5f, -2.0f}, viewer, &bottom, limits));
    }
    SECTION("depth is clamped between the far limit and the bottom panel") {
        REQUIRE(ApplyDepth(-9.9f, -1.0f, -1.5f) == Approx(-10.0f));
        REQUIRE(ApplyDepth(-1.8f, 5.0f, -1.5f) == Approx(-1.5f));
        REQUIRE(ApplyDepth(-2.0f, -0.5f, -1.5f) == Approx(-2.5f));
    }
}

// AzaharFrame curve: a curved screen is hit on its arc (geometry.h, screen_rig.h).
TEST_CASE("FrameXr geometry: curved screens", "[frame_xr]") {
    PanelQuad quad = TopQuad(); // 1.6 x 0.96 m, 2 m ahead
    const float r = 1.6f / (60.0f * 3.14159265f / 180.0f); // 60 degrees across
    quad.curve_radius = r;
    SECTION("straight ahead: the middle, at the flat screen's distance") {
        const QuadHit hit = IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{0, 0, -1}}, quad);
        REQUIRE(hit.in_bounds);
        REQUIRE(hit.u == Approx(0.5f));
        REQUIRE(hit.v == Approx(0.5f));
        REQUIRE(hit.t == Approx(2.0f));
    }
    SECTION("u runs along the arc: the edge is 30 degrees round the axis") {
        // A point 27 degrees about the axis (at z = -2 + r), 9/10 of the way to the right edge.
        const float a = 30.0f * 3.14159265f / 180.0f;
        const Vec3f target{r * std::sin(a * 0.9f), 0.2f, -2.0f + r - r * std::cos(a * 0.9f)};
        const QuadHit hit = IntersectQuad(Ray{Vec3f{0, 0, 0}, Normalized(target)}, quad);
        REQUIRE(hit.in_bounds);
        REQUIRE(hit.u == Approx(0.5f + 0.9f * 0.5f).margin(1e-3f));
        REQUIRE(hit.v == Approx(0.5f - 0.2f / 0.96f).margin(1e-3f));
        REQUIRE(hit.point_world.x == Approx(target.x).margin(1e-3f));
        REQUIRE(hit.point_world.z == Approx(target.z).margin(1e-3f));
    }
    SECTION("a hand outside the cylinder still hits the arc from the inside") {
        PanelQuad tight = quad;
        tight.curve_radius = 0.6f; // the axis 1.4 m ahead, the origin 1.4 m from it
        const QuadHit hit = IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{0, 0, -1}}, tight);
        REQUIRE(hit.on_plane);
        REQUIRE(hit.t == Approx(2.0f));
        REQUIRE(hit.u == Approx(0.5f));
    }
    SECTION("past the edge it is still a hit, out of bounds, so a drag carries on") {
        const QuadHit hit = IntersectQuad(Ray{Vec3f{0, 0, 0}, Normalized(Vec3f{3.0f, 0, -1.0f})}, quad);
        REQUIRE(hit.on_plane);
        REQUIRE_FALSE(hit.in_bounds);
        REQUIRE(hit.u > 1.0f);
    }
    SECTION("from behind or pointing away is no hit") {
        REQUIRE_FALSE(IntersectQuad(Ray{Vec3f{0, 0, 0}, Vec3f{0, 0, 1}}, quad).on_plane);
    }
}
