// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: Dynamic Resolution's hand-off. The frame_xr thread decides how many steps
// under the internal resolution the renderer draws (frame_xr/dynamic_resolution.h); the renderer
// applies it where it reads the factor (RendererBase::GetResolutionScaleFactor) and says what
// the factor was before it. The rasterizer cache sees the new factor at its next frame and starts
// again at it, as it does for the setting itself. Header only: video_core does not link
// frame_xr. Zero steps (the default, and always without VR) changes nothing.

#pragma once

#include <atomic>
#include <chrono>

#include "common/common_types.h"

namespace VideoCore::DynamicResolution {

/// Steps under the factor (frame_xr's thread writes, the renderer reads).
inline std::atomic<u32> g_drop{0};
/// The factor before the steps, as the renderer last worked it out (0: not yet).
inline std::atomic<u32> g_base{0};

/// The factor to draw at for `base`, at least 1.
inline u32 Apply(u32 base) {
    g_base.store(base, std::memory_order_relaxed);
    const u32 drop = g_drop.load(std::memory_order_relaxed);
    return base > drop ? base - drop : 1;
}

// How far behind the GPU is: the XR target notes when a frame's last work went to the queue
// (NoteSubmit, after the flush in RenderToXr) and when its fence signalled (NoteGpuDone, on the
// handoff thread that waits for it). A GPU that keeps up finishes that last bit in a few ms; one
// that cannot is still on earlier work and takes about a game frame or more.
inline std::atomic<s64> g_submit_ns{0};
inline std::atomic<u32> g_gpu_frames{0};
inline std::atomic<u64> g_gpu_us_sum{0};
inline std::atomic<u32> g_gpu_us_max{0};

inline s64 NowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

inline void NoteSubmit() {
    g_submit_ns.store(NowNs(), std::memory_order_relaxed);
}

inline void NoteGpuDone() {
    const s64 at = g_submit_ns.exchange(0, std::memory_order_relaxed);
    if (at == 0) {
        return;
    }
    const s64 us = (NowNs() - at) / 1000;
    if (us < 0 || us > 1'000'000) {
        return;
    }
    g_gpu_frames.fetch_add(1, std::memory_order_relaxed);
    g_gpu_us_sum.fetch_add(static_cast<u64>(us), std::memory_order_relaxed);
    u32 prev = g_gpu_us_max.load(std::memory_order_relaxed);
    while (static_cast<u32>(us) > prev &&
           !g_gpu_us_max.compare_exchange_weak(prev, static_cast<u32>(us))) {
    }
}

struct GpuSecond {
    u32 frames = 0;
    double avg_ms = 0.0;
    double max_ms = 0.0;
};

/// The frames noted since the last call (the frame_xr thread, once a second).
inline GpuSecond TakeGpuSecond() {
    GpuSecond g;
    g.frames = g_gpu_frames.exchange(0, std::memory_order_relaxed);
    const u64 sum = g_gpu_us_sum.exchange(0, std::memory_order_relaxed);
    g.max_ms = g_gpu_us_max.exchange(0, std::memory_order_relaxed) / 1000.0;
    g.avg_ms = g.frames ? static_cast<double>(sum) / 1000.0 / g.frames : 0.0;
    return g;
}

} // namespace VideoCore::DynamicResolution
