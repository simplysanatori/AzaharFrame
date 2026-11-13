// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fixes7: Screen Shape: Automatic. The emulation thread samples the vertex shader's
// float uniforms for the game camera's projection in any view (not only the immersive Full modes,
// whose register is known: vr_immersive.h MeasureDraw) and publishes the projection most uploads
// used in each quarter second; the XR thread picks the screen's shape from it
// (frame_xr/auto_shape.h).
//
// Cheap by design: only while Automatic is in use (g_measure_shape), at most one sample every
// kSampleUs, and a register block that is not a perspective matrix is rejected by its last row
// (two compares) before anything else is read. A game that uploads its projection already
// multiplied with the view (no separate perspective matrix) is never measured: the shape is kept.

#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "video_core/vr_immersive.h"
#include "video_core/vr_view_matrix.h"

namespace VideoCore::Immersive {

/// Set by the XR thread while a game runs with Screen Shape: Automatic.
inline std::atomic<bool> g_measure_shape{false};
/// The newest quarter second's winner (SteadyMs time base).
inline MeasuredProjection g_measured_shape;

/// The first perspective matrix in the uniforms (rows of 4, `count` registers), lowest register
/// first: a game's world camera is uploaded with a draw's own matrices, so the lowest perspective
/// block is the one the draw projects with. False when there is none.
template <typename Registers>
inline bool FindProjection(const Registers& f, std::size_t count, Projection& out) {
    for (std::size_t r = 0; r + 3 < count; ++r) {
        // The last row of a perspective matrix: (0, 0, +-1, 0). Most blocks fail here.
        const float w = f[r + 3][3];
        const float z = f[r + 3][2];
        if (!(std::abs(w) <= 1e-3f) || !(std::abs(std::abs(z) - 1.0f) <= 1e-3f)) {
            continue;
        }
        float rows[4][4];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                rows[i][j] = f[r + static_cast<std::size_t>(i)][j];
            }
        }
        if (ProjectionFromRows(rows, out)) {
            return true;
        }
    }
    return false;
}

/// Emulation thread: one vertex uniform upload. Samples at most every kSampleUs; publishes each
/// window's winner (ProjectionTracker) to `out`. True when the uniforms were read.
class ShapeSampler {
public:
    static constexpr int64_t kSampleUs = 2000;

    template <typename Registers>
    bool Sample(const Registers& f, std::size_t count, int64_t now_us, MeasuredProjection& out) {
        if (last_us_ != 0 && now_us - last_us_ < kSampleUs) {
            return false;
        }
        last_us_ = now_us;
        Projection p, winner;
        if (FindProjection(f, count, p) && tracker_.Add(p, now_us / 1000, winner)) {
            out.Publish(winner.tan_half_y, winner.aspect, now_us / 1000);
        }
        return true;
    }

private:
    int64_t last_us_ = 0;
    ProjectionTracker tracker_;
};

inline int64_t SteadyUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace VideoCore::Immersive
