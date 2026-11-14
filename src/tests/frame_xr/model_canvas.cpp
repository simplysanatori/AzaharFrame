// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctrlglitch: the controllers' eye quads on a world-locked canvas
// (frame_xr/vis/model_plane.h). An image drawn onto the canvas from where the eye is shows
// every point where the eye sees it; the canvas is moved only when it stops serving the view;
// and, against a model of SteamVR's overlays (texture, place and size each taken up to two
// refreshes late, independently), a quad is never on view with an image drawn for another
// place, where the old per-refresh eye quads were, by the head's turn in a refresh.

#include <array>
#include <cmath>
#include <optional>
#include <random>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/pose_guard.h" // QuatAngle
#include "frame_xr/vis/model_plane.h"

using Catch::Approx;
using namespace FrameXr;
using namespace FrameXr::Vis;

namespace {

// The Frame's left eye, as the log reads it: -59.1/50.4 across, -60.0/49.5 up (degrees).
constexpr float kL = -1.0315f, kR = 0.8796f, kU = 0.8639f, kD = -1.0472f;

Input::Quatf Yaw(float angle) {
    return {0.0f, std::sin(angle * 0.5f), 0.0f, std::cos(angle * 0.5f)};
}
Input::Quatf Pitch(float angle) {
    return {std::sin(angle * 0.5f), 0.0f, 0.0f, std::cos(angle * 0.5f)};
}

/// Where a point lands in an image drawn with `ce` (u from the left, v from the top), as the
/// models' pass projects it (Projection of the angles, the inverse of the pose).
std::optional<std::array<float, 2>> ImagePosition(const CanvasEye& ce, const Input::Vec3f& p) {
    const Input::Vec3f l = Input::Transform(Input::Inverse(ce.pose), p);
    if (l.z >= 0.0f) {
        return std::nullopt;
    }
    const float tl = std::tan(ce.angle_left), tr = std::tan(ce.angle_right);
    const float tu = std::tan(ce.angle_up), td = std::tan(ce.angle_down);
    const float tx = l.x / -l.z, ty = l.y / -l.z;
    return std::array<float, 2>{(tx - tl) / (tr - tl), (tu - ty) / (tu - td)};
}

} // namespace

TEST_CASE("Canvas: an image drawn onto it from the eye shows every point where the eye sees it",
          "[frame_xr][ctrlglitch]") {
    std::mt19937 rng{3};
    std::uniform_real_distribution<float> turn(-0.5f, 0.5f), shift(-0.08f, 0.08f),
        spread(-0.25f, 0.25f), depth(0.2f, 1.2f);
    for (int n = 0; n < 200; n++) {
        const Input::Posef placed{Input::Normalized(Input::Multiply(Yaw(turn(rng)), Pitch(turn(rng)))),
                                  {0.0f, 1.6f, 0.0f}};
        const EyePlane canvas = PlaceCanvas(placed, kL, kR, kU, kD, depth(rng));
        // The eye since: turned and moved a little, within what the canvas serves.
        const Input::Posef eye{
            Input::Normalized(Input::Multiply(Yaw(turn(rng) * 0.3f), placed.orientation)),
            placed.position + Input::Vec3f{shift(rng), shift(rng), shift(rng)}};
        const auto ce = CanvasView(canvas, eye.position);
        REQUIRE(ce);
        // Square to the canvas, at the eye.
        CHECK(Input::QuatAngle(ce->pose.orientation, canvas.pose.orientation) < 1e-4f);
        CHECK(Input::Length(ce->pose.position - eye.position) < 1e-6f);
        const Input::PanelQuad quad{canvas.pose, canvas.width_m, canvas.height_m, true};
        for (int k = 0; k < 20; k++) {
            // A point the eye sees, at any distance (nearer than the canvas or past it).
            const Input::Vec3f dir = Input::Normalized(
                Input::Rotate(eye.orientation, {spread(rng), spread(rng), -1.0f}));
            const Input::Vec3f p = eye.position + dir * depth(rng);
            const Input::QuadHit hit = Input::IntersectQuad({eye.position, dir}, quad);
            if (!hit.in_bounds) {
                continue;
            }
            const auto at = ImagePosition(*ce, p);
            REQUIRE(at);
            CHECK((*at)[0] == Approx(hit.u).margin(1e-3));
            CHECK((*at)[1] == Approx(hit.v).margin(1e-3));
        }
    }
    // Not in front of it: no view.
    const Input::Posef eye{{}, {0.0f, 1.6f, 0.0f}};
    const EyePlane canvas = PlaceCanvas(eye, kL, kR, kU, kD, 0.4f);
    CHECK_FALSE(CanvasView(canvas, eye.position + Input::Vec3f{0.0f, 0.0f, -0.39f}));
    CHECK_FALSE(CanvasView(canvas, eye.position + Input::Vec3f{0.0f, 0.0f, -0.5f}));
}

TEST_CASE("Canvas: it serves the view until the models' part of it leaves it",
          "[frame_xr][ctrlglitch]") {
    const Input::Posef eye{{}, {0.0f, 1.6f, 0.0f}};
    for (const float distance : {0.2f, 0.4f, 1.5f}) {
        const EyePlane canvas = PlaceCanvas(eye, kL, kR, kU, kD, distance);
        // Just put down: it serves any part of the view.
        for (const auto& part : {std::array<float, 4>{0.0f, 0.0f, 1.0f, 1.0f},
                                 std::array<float, 4>{0.4f, 0.6f, 0.6f, 0.9f}}) {
            CHECK(CanvasServes(canvas, eye, kL, kR, kU, kD, part));
        }
        CHECK(CanvasServes(canvas, eye, kL, kR, kU, kD, std::nullopt));
        // The models toward the right: a turn right moves it once their right side, widened
        // by the guard, passes the canvas's edge (turning right is a negative yaw).
        const std::array<float, 4> right_edge{0.80f, 0.4f, 0.88f, 0.6f};
        const auto turned = [&](float yaw) {
            return Input::Posef{Yaw(yaw), eye.position};
        };
        const float right =
            std::atan(std::tan(kL) + (std::tan(kR) - std::tan(kL)) * right_edge[2]);
        const float slack = (kR + kCanvasMargin) - (right + kCanvasGuard);
        CHECK(slack > 0.15f);
        CHECK(CanvasServes(canvas, turned(-slack * 0.9f), kL, kR, kU, kD, right_edge));
        CHECK_FALSE(CanvasServes(canvas, turned(-slack * 1.1f), kL, kR, kU, kD, right_edge));
        // Held low, across the bottom edge of the view: nods within kCanvasEdge plus the margin
        // keep it (only the part kCanvasEdge in from the edge counts).
        const std::array<float, 4> low{0.4f, 0.9f, 0.6f, 1.0f};
        const auto nodded = [&](float pitch) {
            return Input::Posef{Pitch(pitch), eye.position};
        };
        const float nod = kCanvasEdge + kCanvasMargin;
        CHECK(CanvasServes(canvas, nodded(-nod * 0.8f), kL, kR, kU, kD, low));
        CHECK_FALSE(CanvasServes(canvas, nodded(-nod * 1.2f), kL, kR, kU, kD, low));
        // Only in the band at the very edge: nothing to serve.
        CHECK(CanvasServes(canvas, nodded(-nod * 1.2f), kL, kR, kU, kD,
                           std::array<float, 4>{0.4f, 0.97f, 0.6f, 1.0f}));
        // A turn the other way keeps it, up to the tilt limit.
        CHECK(CanvasServes(canvas, turned(0.3f), kL, kR, kU, kD, right_edge));
        // The models straight ahead (the view is wider to the left): a long turn before the
        // canvas runs out under them.
        const std::array<float, 4> middle{0.55f, 0.45f, 0.61f, 0.55f};
        CHECK(CanvasServes(canvas, turned(kCanvasMaxTilt * 0.95f), kL, kR, kU, kD, middle));
        // Past the tilt limit, whatever the models.
        CHECK_FALSE(CanvasServes(canvas, turned(kCanvasMaxTilt * 1.05f), kL, kR, kU, kD, middle));
        CHECK_FALSE(CanvasServes(canvas, turned(kCanvasMaxTilt * 1.05f), kL, kR, kU, kD,
                                 std::nullopt));
    }
    // The eye too near: moved.
    const EyePlane canvas = PlaceCanvas(eye, kL, kR, kU, kD, 0.3f);
    const Input::Posef near{{}, eye.position + Input::Vec3f{0.0f, 0.0f, -0.25f}};
    CHECK_FALSE(CanvasServes(canvas, near, kL, kR, kU, kD, std::nullopt));
}

TEST_CASE("Canvas handover: shrink, move, show, in that order", "[frame_xr][ctrlglitch]") {
    CanvasHandover h;
    constexpr int S = CanvasHandover::kSettle;
    std::vector<CanvasFrame> f;
    uint32_t drawn = 0; // the image drawn on a refresh is released on the next
    bool image = false;
    const auto step = [&] {
        const CanvasFrame s = h.Step(image, drawn);
        image = true;
        drawn = s.canvas; // drawn for the canvas the quads carry
        f.push_back(s);
        return s;
    };
    h.Request(1);
    for (int i = 0; i < 12; i++) {
        step();
    }
    // Nothing on view for S refreshes, then canvas 1; shown once its place and images have each
    // gone out S refreshes; the projection layer until S refreshes after that.
    for (int i = 0; i < S; i++) {
        CHECK(f[static_cast<std::size_t>(i)].canvas == 0);
    }
    int first_shown = -1;
    for (int i = 0; i < 12; i++) {
        if (f[static_cast<std::size_t>(i)].quads_shown) {
            first_shown = i;
            break;
        }
    }
    REQUIRE(first_shown > S);
    CHECK(f[static_cast<std::size_t>(first_shown)].canvas == 1);
    for (int i = 1; i < first_shown + S; i++) {
        CHECK(f[static_cast<std::size_t>(i)].projection); // something on view throughout
    }
    CHECK_FALSE(f.back().projection);
    // A move: shrunk now, still drawn for canvas 1 for S refreshes, then canvas 2.
    h.Request(2);
    CHECK(h.Busy());
    const CanvasFrame a = step();
    CHECK_FALSE(a.quads_shown);
    CHECK(a.projection);
    CHECK(a.canvas == 1);
    for (int i = 1; i < S; i++) {
        CHECK(step().canvas == 1);
    }
    CHECK(step().canvas == 2);
    CHECK_FALSE(h.Busy());
    int shown_after = 0;
    while (!step().quads_shown) {
        shown_after++;
        REQUIRE(shown_after < 10);
    }
    CHECK(shown_after >= S);
    // Absent for a while (no models): a move then goes at once.
    for (int i = 0; i < S; i++) {
        h.Absent();
    }
    h.Request(3);
    CHECK(step().canvas == 3);
}

// SteamVR's overlays: each refresh the compositor takes up a quad's place, its size and its
// texture from what xrEndFrame sent up to `lag` refreshes earlier, each on its own (never going
// back). The eye quads before this lane went to each image's eye views every refresh; on the
// canvas they move only through the handover.
TEST_CASE("Canvas: a quad on view always pairs its image with the place it was drawn for",
          "[frame_xr][ctrlglitch]") {
    struct Motion {
        const char* name;
        float yaw, yaw_hz, pitch, pitch_hz; ///< amplitudes (radians) and rates
        int min_shown_pct, max_moves_per_s;
    };
    // Looking round a screen; and turning +-70 degrees at 0.4 Hz (176 degrees a second at the
    // middle) while nodding, the models going in and out of view, the worst case.
    for (const Motion& m : {Motion{"looking round", 0.26f, 0.3f, 0.12f, 0.2f, 97, 1},
                            Motion{"turning hard", 1.22f, 0.4f, 0.3f, 0.23f, 70, 6}}) {
        for (const int lag : {1, 2}) {
            INFO(m.name << ", taken up to " << lag << " refreshes late");
            std::mt19937 rng{static_cast<unsigned>(11 + lag)};
            std::uniform_int_distribution<int> late(0, lag);
            constexpr int kFrames = 12000;
            constexpr double kRefresh = 1.0 / 120.0;
            const auto head = [&](int i) {
                const double t = i * kRefresh;
                const float yaw = m.yaw * static_cast<float>(std::sin(2.0 * 3.14159265 * m.yaw_hz * t));
                const float pitch =
                    m.pitch * static_cast<float>(std::sin(2.0 * 3.14159265 * m.pitch_hz * t));
                return Input::Posef{Input::Normalized(Input::Multiply(Yaw(yaw), Pitch(pitch))),
                                    {0.0f, 1.6f, 0.0f}};
            };
            // The controllers held still in front, low.
            const ModelSphere models{{0.05f, 1.25f, -0.38f}, 0.09f};

            struct Sent {
                uint32_t place = 0;   ///< the canvas the quad's place is
                bool full = false;    ///< its size
                uint32_t texture = 0; ///< the canvas the image released was drawn for
            };
            std::vector<Sent> sent(kFrames);
            std::array<int, 3> taken{}; // place, size, texture: the frame each was last taken from
            CanvasHandover handover;
            std::vector<EyePlane> canvases(1);
            uint32_t drawn = 0;
            bool image = false;
            int mismatched = 0, shown = 0, moves = 0, dark = 0;
            for (int i = 0; i < kFrames; i++) {
                const Input::Posef eye = head(i);
                const auto part = ModelsViewBounds(eye, kL, kR, kU, kD, &models, 1, 0.05f);
                const uint32_t current = handover.Canvas();
                if (current == 0 || (!handover.Busy() &&
                                     !CanvasServes(canvases[current], eye, kL, kR, kU, kD, part))) {
                    canvases.push_back(PlaceCanvas(eye, kL, kR, kU, kD, 0.4f));
                    handover.Request(static_cast<uint32_t>(canvases.size() - 1));
                    moves++;
                }
                const CanvasFrame s = handover.Step(image, drawn);
                sent[static_cast<std::size_t>(i)] = {s.canvas, s.quads_shown, drawn};
                if (!s.quads_shown && !s.projection && image) {
                    dark++;
                }
                image = true;
                drawn = s.canvas;
                // The compositor's refresh.
                for (int& k : taken) {
                    k = std::max(k, i - late(rng));
                }
                const Sent& place = sent[static_cast<std::size_t>(taken[0])];
                const Sent& size = sent[static_cast<std::size_t>(taken[1])];
                const Sent& texture = sent[static_cast<std::size_t>(taken[2])];
                if (size.full) {
                    shown++;
                    if (texture.texture != place.place) {
                        mismatched++;
                    }
                }
            }
            INFO("moves " << moves << ", shown " << shown << " of " << kFrames);
            CHECK(mismatched == 0);
            CHECK(dark == 0); // the projection layer fills every gap
            CHECK(shown * 100 > kFrames * m.min_shown_pct);
            CHECK(moves <= m.max_moves_per_s * kFrames / 120 + 1);
        }
    }
}

TEST_CASE("Canvas: the old eye quads paired images with another refresh's place",
          "[frame_xr][ctrlglitch]") {
    // Before: each refresh, the quad went to the eye's view the released image was drawn for (one
    // refresh old), its texture that image. Taken up to a refresh late each, the image shows at
    // another refresh's place: the controllers off by the head's turn in between.
    std::mt19937 rng{5};
    std::uniform_int_distribution<int> late(0, 1);
    constexpr int kFrames = 2000;
    const float rate = 150.0f * 3.14159265f / 180.0f / 120.0f; // 150 degrees a second
    std::array<int, 2> taken{};                                  // place, texture
    int off = 0;
    float worst = 0.0f;
    for (int i = 2; i < kFrames; i++) {
        for (int& k : taken) {
            k = std::max(k, i - late(rng));
        }
        // Sent on refresh k: the place and the image of the image drawn on k - 1, the head's yaw
        // then (k - 1) * rate.
        const float err = std::fabs(static_cast<float>(taken[0] - taken[1])) * rate;
        if (err > 0.0f) {
            off++;
        }
        worst = std::max(worst, err);
    }
    CHECK(off > kFrames / 10);
    CHECK(worst * 180.0f / 3.14159265f == Approx(1.25f).margin(0.01f));
}
