// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu2: a toast always in front of every game screen. The toast is placed 0.9 m
// ahead (hud.cpp), but an immersive screen can be much nearer (Mario Kart 7's sat at 0.37 m), and
// a toast farther than the screen it is drawn over cannot be read: the eyes meet on the screen
// and the toast's two images do not fuse. NearScale gives the factor about the eye that brings
// the toast's whole rectangle nearer than every screen its rays meet, by kMargin, computed each
// frame from the screens as they are drawn (pose, size, curve), so it holds for any distance,
// size or placement. Scaled about the eye, the toast keeps its direction and its angular size.
// Pure (unit-tested).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "frame_xr/input/geometry.h"

namespace FrameXr::ToastDepth {

/// The toast ends up this much of the way to the nearest screen it is over (15 % nearer).
constexpr float kMargin = 0.85f;
/// Never nearer than this to the eye (a screen nearer still: the toast is drawn last, on top).
constexpr float kMinDistanceM = 0.12f;
/// Rays through a grid over the toast's rectangle, its corners and edges included (between two
/// rays a screen's edge is covered by kMargin).
constexpr int kRaysAcross = 5;
constexpr int kRaysDown = 3;

/// 1 when no screen is nearer; else under 1. `toast` faces +Z (as Input::PanelQuad), `width_m`
/// by `height_m`; `screens` as drawn this frame, in the same space as `eye`.
inline float NearScale(const Input::Vec3f& eye, const Input::Posef& toast, float width_m,
                       float height_m, const std::vector<Input::PanelQuad>& screens) {
    using namespace Input;
    const float centre_d = Length(toast.position - eye);
    if (!(centre_d > 1e-4f) || screens.empty()) {
        return 1.0f;
    }
    float k = 1.0f;
    const Vec3f right = Rotate(toast.orientation, {1.0f, 0.0f, 0.0f});
    const Vec3f up = Rotate(toast.orientation, {0.0f, 1.0f, 0.0f});
    for (int i = 0; i < kRaysAcross; i++) {
        for (int j = 0; j < kRaysDown; j++) {
            const float u = (static_cast<float>(i) / (kRaysAcross - 1) - 0.5f) * width_m;
            const float v = (static_cast<float>(j) / (kRaysDown - 1) - 0.5f) * height_m;
            const Vec3f p = toast.position + right * u + up * v;
            const Vec3f to = p - eye;
            const float d = Length(to);
            if (!(d > 1e-4f)) {
                continue;
            }
            const Ray ray{eye, to * (1.0f / d)};
            for (const PanelQuad& q : screens) {
                if (!q.visible) {
                    continue;
                }
                const QuadHit hit = IntersectQuad(ray, q);
                if (hit.on_plane && hit.in_bounds && hit.t > 0.0f) {
                    k = std::min(k, hit.t * kMargin / d);
                }
            }
        }
    }
    return std::clamp(k, std::min(kMinDistanceM / centre_d, 1.0f), 1.0f);
}

/// AzaharFrame integ9: what the frame loop does with a screen this refresh (app.cpp add_screen).
/// The toast's inputs take every screen with a picture, whether or not its layer is drawn: with
/// AZAHARFRAME_XR_SINGLE_FLASH the top screen's layer is left out on every other refresh, and a
/// toast that saw it only on alternate refreshes jumped between its own distance and the near one.
struct ScreenUse {
    bool toast_input = false; ///< goes into the toast's screens (NearScale)
    bool layer = false;       ///< drawn as a layer this refresh
};
constexpr ScreenUse UseOf(bool has_picture, bool hidden_this_refresh) {
    return {has_picture, has_picture && !hidden_this_refresh};
}

/// AzaharFrame integ9: NearScale eased, so the toast never pops in depth as its rectangle crosses
/// a screen's edge (a world-fixed screen, a toast that stays where it was shown): in front of the
/// nearer screen quickly (kNearTauNs, so it is behind a screen for a moment at most), back out to
/// its own distance more slowly (kFarTauNs). A toast shown afresh (Reset) starts where it belongs.
/// The factor scales the toast about the eye: only its depth eases, its size in view stays.
class Ease {
public:
    /// Time constants (63 % of the way in this time): toward the eye, and back out.
    static constexpr int64_t kNearTauNs = 60'000'000;
    static constexpr int64_t kFarTauNs = 250'000'000;

    void Reset() {
        started_ = false;
    }
    /// `target`: this refresh's NearScale; `now_ns`: its display time. Returns the factor to use.
    float Step(float target, int64_t now_ns) {
        if (!std::isfinite(target)) {
            target = 1.0f;
        }
        if (!started_) {
            k_ = target;
        } else {
            const double dt = static_cast<double>(std::max<int64_t>(now_ns - last_ns_, 0));
            const double tau = static_cast<double>(target < k_ ? kNearTauNs : kFarTauNs);
            const float a = static_cast<float>(1.0 - std::exp(-dt / tau));
            k_ += (target - k_) * a;
            if (std::fabs(target - k_) < 1e-4f) {
                k_ = target;
            }
        }
        started_ = true;
        last_ns_ = now_ns;
        return k_;
    }
    float Value() const {
        return k_;
    }

private:
    bool started_ = false;
    float k_ = 1.0f;
    int64_t last_ns_ = 0;
};

} // namespace FrameXr::ToastDepth
