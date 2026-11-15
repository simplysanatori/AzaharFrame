// Copyright Citra Emulator Project / CitraVR
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib> // AzaharFrame shaderbadge: the test delay
#include <thread>

namespace Common {

/**
 * Process-wide, lock-free counters used to diagnose frame stalls. Written from
 * the emu/GPU thread (core frame timing, draw-time shader compiles) and the VR
 * frame thread (compositor deadline misses), read from the stats panel and the
 * user stall-marker logger.
 *
 * All counters are cumulative since process start; readers wanting
 * per-interval values should diff successive reads. "Worst" values are
 * all-time maxima.
 */
struct StallTelemetry {
    // Draw-time shader compiles on the emu/GPU thread (GLES compile+link on a
    // shader-cache miss, including the disk-cache write). The dominant cause
    // of one-off stalls in Citra-family emulators.
    std::atomic<std::uint32_t> shader_compile_count{0};
    std::atomic<std::uint64_t> shader_compile_time_us{0};
    // Steady-clock timestamp (us since epoch of steady_clock) of the most
    // recent draw-time compile; 0 if none has happened yet.
    std::atomic<std::int64_t> last_shader_compile_steady_us{0};

    // Core emulation frames that blew their vblank budget (see perf_stats.cpp
    // for thresholds).
    std::atomic<std::uint32_t> emu_slow_frame_count{0};
    std::atomic<std::uint32_t> emu_stall_frame_count{0};
    std::atomic<std::uint64_t> emu_worst_frame_time_us{0};

    // VR frame loop (see vr_main.cpp). A "missed" frame is a gap between
    // successive xrEndFrame completions exceeding 1.5x the display period.
    std::atomic<std::uint32_t> vr_missed_frame_count{0};
    std::atomic<std::uint64_t> vr_worst_frame_gap_us{0};
    std::atomic<std::uint64_t> vr_worst_wait_frame_us{0};

    // User-initiated stall markers (thumbstick click in VR).
    std::atomic<std::uint32_t> user_marker_count{0};

    // AzaharFrame: the renderer's queue. Interval maxima, taken (reset to 0) by the XR
    // thread's per-second line: the longest vkQueueSubmit under the scheduler's submit
    // mutex, and the longest the scheduler waited for that mutex.
    std::atomic<std::uint64_t> queue_submit_max_us{0};
    std::atomic<std::uint64_t> queue_submit_wait_max_us{0};
    // The desktop window's submit and vkQueuePresentKHR under that mutex (interval max).
    std::atomic<std::uint64_t> window_present_max_us{0};
    // AzaharFrame: custom texture uploads on the emu thread (RasterizerCache::TickFrame),
    // cumulative, and the longest image creation of one upload (interval max).
    std::atomic<std::uint64_t> custom_upload_us{0};
    std::atomic<std::uint64_t> custom_upload_count{0};
    std::atomic<std::uint64_t> custom_upload_bytes{0};
    std::atomic<std::uint64_t> custom_image_us{0};  ///< of custom_upload_us: making the images
    std::atomic<std::uint64_t> staging_wait_us{0}; ///< a stream buffer full: waited for the GPU
    std::atomic<std::uint64_t> surface_gc_us{0};   ///< freeing replaced surfaces (TickFrame)
    // AzaharFrame: vkAllocateMemory (any thread): cumulative, and interval maxima taken by the
    // XR thread's per-second line.
    std::atomic<std::uint64_t> device_alloc_us{0};
    std::atomic<std::uint64_t> device_alloc_count{0};
    std::atomic<std::uint64_t> device_alloc_max_us{0};
    std::atomic<std::uint64_t> device_alloc_max_bytes{0};

    // AzaharFrame stall: shaders and pipelines built while a game runs (Vulkan: the shader and
    // pipeline workers), queued and finished, cumulative; the difference is the backlog. And the
    // steady-clock time (us) the last emulated frame ended, so a reader can tell the game stands.
    std::atomic<std::uint64_t> shader_builds_queued{0};
    std::atomic<std::uint64_t> shader_builds_done{0};
    std::atomic<std::int64_t> emu_frame_end_steady_us{0};
    // AzaharFrame flash3: draws left out because their pipeline was still being built (Vulkan,
    // async shaders), cumulative; the XR thread's per-second line gives the second's.
    std::atomic<std::uint64_t> draws_skipped{0};
    // AzaharFrame projdef: the GPU's busy time on the renderer's command buffers (timestamps,
    // video_core/renderer_vulkan/vk_gpu_busy.h) and on the XR thread's controller and projection
    // pass (frame_xr/vis/controller_models.cpp), cumulative in ns; the XR thread's per-second line
    // takes (exchanges) them.
    std::atomic<std::uint64_t> gpu_busy_ns{0};
    std::atomic<std::uint64_t> gpu_busy_buffers{0};
    std::atomic<std::uint64_t> xr_pass_gpu_ns{0};
    std::atomic<std::uint64_t> xr_pass_gpu_n{0};

    static StallTelemetry& Get() {
        static StallTelemetry instance;
        return instance;
    }
};

/// AzaharFrame stall: a shader or pipeline build queued at run time, and one finished.
inline void NoteShaderBuildQueued() {
    StallTelemetry::Get().shader_builds_queued.fetch_add(1, std::memory_order_relaxed);
    // AzaharFrame shaderbadge: AZAHARFRAME_TEST_SHADER_BUILD_MS=N holds the thread that asks for
    // a build N ms (the emulation thread: the game stands with the build waiting), for mock runs
    // on a GPU that builds too fast to make the game stand. Never set otherwise.
    static const int test_ms = [] {
        const char* v = std::getenv("AZAHARFRAME_TEST_SHADER_BUILD_MS");
        return v ? std::atoi(v) : 0;
    }();
    if (test_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(test_ms));
    }
}
inline void NoteShaderBuildDone() {
    StallTelemetry::Get().shader_builds_done.fetch_add(1, std::memory_order_relaxed);
}

inline void UpdateMax(std::atomic<std::uint64_t>& target, std::uint64_t value) {
    std::uint64_t current = target.load(std::memory_order_relaxed);
    while (current < value &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

inline std::int64_t SteadyNowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace Common
