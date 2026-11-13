// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu2: a toast always in front of every screen it is over (frame_xr/toast_depth.h),
// for any screen distance, size, curve and place.

#include <cmath>
#include <cstdint>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/toast_depth.h"
#include "frame_xr/toast_fit.h"

using Catch::Approx;
using namespace FrameXr;
using namespace FrameXr::Input;

namespace {
Quatf Yaw(float deg) {
    const float r = deg * 3.14159265f / 360.0f;
    return {0.0f, std::sin(r), 0.0f, std::cos(r)};
}
/// A screen facing the eye at the origin, `d` metres along `yaw_deg` (left positive), `up` high.
PanelQuad Screen(float d, float yaw_deg, float up, float w, float h, float curve = 0.0f) {
    PanelQuad q;
    const float r = yaw_deg * 3.14159265f / 180.0f;
    q.pose = {Yaw(yaw_deg), {-std::sin(r) * d, up, -std::cos(r) * d}};
    q.width_m = w;
    q.height_m = h;
    q.visible = true;
    q.curve_radius = curve;
    return q;
}
/// The toast as hud.cpp places it: 0.9 m ahead, 0.25 m below the eye, facing it.
Posef Toast() {
    return {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -0.25f, -0.9f}};
}
} // namespace

TEST_CASE("Toasts: in front of a nearer screen, at any distance", "[frame_xr][kiu2]") {
    const Vec3f eye{0.0f, 0.0f, 0.0f};
    const ToastFit::Box box;
    const Posef toast = Toast();
    // Mario Kart 7's immersive screen at 0.37 m, about 96 degrees wide: the toast comes nearer.
    {
        const std::vector<PanelQuad> s{Screen(0.37f, 0.0f, 0.0f, 0.8f, 0.6f)};
        const float k = ToastDepth::NearScale(eye, toast, box.width_m, box.height_m, s);
        REQUIRE(k < 1.0f);
        // Every corner of the toast is nearer than the screen along its own ray.
        const float zt = 0.9f * k; // the toast's plane
        CHECK(zt < 0.37f * ToastDepth::kMargin + 1e-4f);
        CHECK(box.width_m * k / zt == Approx(box.width_m / 0.9f)); // the same size in view
    }
    // A sweep of distances, sizes, curves and places: wherever a screen covers the toast, the
    // whole toast ends up nearer; where none does, it stays where it was.
    for (float d = 0.15f; d <= 3.0f; d += 0.05f) {
        for (float w : {0.3f, 0.8f, 2.0f, 4.0f}) {
            for (float yaw : {-60.0f, -15.0f, 0.0f, 10.0f, 45.0f}) {
                for (float curve : {0.0f, 1.0f, 2.5f}) {
                    for (float up : {-0.5f, -0.25f, 0.0f, 0.4f}) {
                        const PanelQuad q = Screen(d, yaw, up, w, w * 0.6f, curve);
                        const std::vector<PanelQuad> s{q};
                        const float k =
                            ToastDepth::NearScale(eye, toast, box.width_m, box.height_m, s);
                        CHECK(k > 0.0f);
                        CHECK(k <= 1.0f);
                        // Rays through the toast's corners and middle, scaled: before any hit.
                        for (float u : {-0.5f, 0.0f, 0.5f}) {
                            for (float v : {-0.5f, 0.0f, 0.5f}) {
                                const Vec3f p{u * box.width_m, -0.25f + v * box.height_m, -0.9f};
                                const float pd = Length(p);
                                const Ray ray{eye, p * (1.0f / pd)};
                                const QuadHit hit = IntersectQuad(ray, q);
                                if (hit.on_plane && hit.in_bounds && hit.t > 0.0f &&
                                    hit.t * ToastDepth::kMargin >= ToastDepth::kMinDistanceM * 1.1f) {
                                    CHECK(pd * k < hit.t);
                                }
                            }
                        }
                        // Farther than the toast everywhere: nothing changes.
                        if (d > 1.2f && curve == 0.0f && std::fabs(yaw) < 20.0f) {
                            CHECK(k == 1.0f);
                        }
                    }
                }
            }
        }
    }
    // No screens, a hidden one, or one off to the side: the toast's own distance.
    CHECK(ToastDepth::NearScale(eye, toast, box.width_m, box.height_m, {}) == 1.0f);
    PanelQuad hidden = Screen(0.3f, 0.0f, 0.0f, 2.0f, 1.2f);
    hidden.visible = false;
    CHECK(ToastDepth::NearScale(eye, toast, box.width_m, box.height_m, {hidden}) == 1.0f);
    CHECK(ToastDepth::NearScale(eye, toast, box.width_m, box.height_m,
                                {Screen(0.4f, 90.0f, 0.0f, 0.5f, 0.3f)}) == 1.0f);
    // Never nearer than kMinDistanceM (a screen almost on the face).
    const float k = ToastDepth::NearScale(eye, toast, box.width_m, box.height_m,
                                          {Screen(0.05f, 0.0f, 0.0f, 2.0f, 2.0f)});
    CHECK(k * Length(toast.position) == Approx(ToastDepth::kMinDistanceM));
}

TEST_CASE("Toast depth: the screens go into the toast's inputs even when left out",
          "[frame_xr][toast]") {
    // AzaharFrame integ9: with SINGLE_FLASH the top screen's layer is left out every other
    // refresh, but the toast still sees it (in front of it, and shown).
    constexpr auto shown = ToastDepth::UseOf(true, false);
    constexpr auto hidden = ToastDepth::UseOf(true, true);
    constexpr auto none = ToastDepth::UseOf(false, false);
    CHECK(shown.toast_input);
    CHECK(shown.layer);
    CHECK(hidden.toast_input);
    CHECK_FALSE(hidden.layer);
    CHECK_FALSE(none.toast_input);
    CHECK_FALSE(none.layer);
}

TEST_CASE("Toast depth: eased in front of a nearer screen and back out", "[frame_xr][toast]") {
    using ToastDepth::Ease;
    constexpr int64_t ms = 1'000'000;
    constexpr int64_t refresh = 11 * ms; // 90 Hz
    Ease e;
    CHECK(e.Step(1.0f, 0) == 1.0f); // first: where it belongs, at once
    // Over a nearer screen: toward it, a part each refresh (no pop), there within a few tenths.
    float k = 1.0f, last = 1.0f;
    int64_t t = 0;
    for (int i = 0; i < 60; i++) {
        t += refresh;
        k = e.Step(0.4f, t);
        CHECK(k <= last);
        CHECK(k >= 0.4f);
        if (i == 0) {
            CHECK(k > 0.85f); // one refresh moves it a part of the way only
        }
        if (t >= 4 * Ease::kNearTauNs) {
            CHECK(k < 0.42f); // in front within a quarter second
        }
        last = k;
    }
    CHECK(k == Approx(0.4f).margin(1e-3f));
    // Off the screen: back out, more slowly, never past its own distance.
    last = k;
    for (int i = 0; i < 400; i++) {
        t += refresh;
        k = e.Step(1.0f, t);
        CHECK(k >= last);
        CHECK(k <= 1.0f);
        if (i == 0) {
            CHECK(k < 0.45f);
        }
        last = k;
    }
    CHECK(k == 1.0f);
    // A time constant in: about 63 % of the way, each way.
    Ease f;
    f.Step(0.5f, 0);
    CHECK(f.Step(1.0f, Ease::kFarTauNs) == Approx(0.5f + 0.5f * 0.632f).margin(0.01f));
    Ease n;
    n.Step(1.0f, 0);
    CHECK(n.Step(0.5f, Ease::kNearTauNs) == Approx(1.0f - 0.5f * 0.632f).margin(0.01f));
    CHECK(Ease::kNearTauNs < Ease::kFarTauNs);
    // Shown afresh (Reset): where it belongs, at once.
    f.Reset();
    CHECK(f.Step(0.3f, Ease::kFarTauNs + refresh) == 0.3f);
    // The same display time twice (no time passed): no move.
    CHECK(f.Step(1.0f, Ease::kFarTauNs + refresh) == 0.3f);
    // Nonsense in: its own distance.
    Ease g;
    CHECK(g.Step(std::nanf(""), 0) == 1.0f);
}
