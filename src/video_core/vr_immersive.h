// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: the hand-off for CitraVR's immersive mode between the frame_xr thread and the
// emulation thread.
//
// CitraVR called RasterizerInterface::SetVRData from its OpenXR thread while the emulation
// thread was drawing with the same fields (a data race). Here the XR thread only publishes a
// snapshot (a seqlock of atomics, one writer); the emulation thread takes the newest one once
// per guest frame (GPU::VBlankCallback, right after SwapBuffers) and calls SetVRData itself,
// so the rasterizer is only ever touched from the thread that draws.
//
// Header only: video_core does not link frame_xr; frame_xr links video_core and publishes.
// Nothing is published without an XR session, so a flat run behaves as upstream.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>

#include "common/common_types.h"
#include "common/settings.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/vr_view_matrix.h" // AzaharFrame depth

namespace VideoCore::Immersive {

/// The arguments of RasterizerInterface::SetVRData. The defaults are CitraVR's "off" values
/// (vr_main.cpp DisableImmersiveMode): mode 1 with factor 1 changes nothing.
struct VRData {
    s32 mode = 1;
    float factor = 1.0f;
    s32 uoffset = -1;
    float game_pos_scaler = 0.0f;
    std::array<float, 16> inv_view{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

/// One writer (the XR thread), any number of readers. A reader never blocks the writer; a
/// read that overlaps a write is retried a few times and otherwise skipped (the next guest
/// frame takes the next snapshot).
class Mailbox {
public:
    void Publish(const VRData& data) noexcept {
        const u32 s = seq.load(std::memory_order_relaxed);
        seq.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        mode.store(data.mode, std::memory_order_relaxed);
        factor.store(data.factor, std::memory_order_relaxed);
        uoffset.store(data.uoffset, std::memory_order_relaxed);
        game_pos_scaler.store(data.game_pos_scaler, std::memory_order_relaxed);
        for (std::size_t i = 0; i < 16; i++) {
            inv_view[i].store(data.inv_view[i], std::memory_order_relaxed);
        }
        seq.store(s + 2, std::memory_order_release);
    }

    /// False: nothing published yet, or a write was in progress every try.
    /// `serial` is a number that changes with every publish.
    bool Read(VRData& out, u32& serial) const noexcept {
        for (int attempt = 0; attempt < 4; attempt++) {
            const u32 s0 = seq.load(std::memory_order_acquire);
            if (s0 == 0) {
                return false;
            }
            if (s0 & 1) {
                continue;
            }
            VRData d;
            d.mode = mode.load(std::memory_order_relaxed);
            d.factor = factor.load(std::memory_order_relaxed);
            d.uoffset = uoffset.load(std::memory_order_relaxed);
            d.game_pos_scaler = game_pos_scaler.load(std::memory_order_relaxed);
            for (std::size_t i = 0; i < 16; i++) {
                d.inv_view[i] = inv_view[i].load(std::memory_order_relaxed);
            }
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq.load(std::memory_order_relaxed) == s0) {
                out = d;
                serial = s0;
                return true;
            }
        }
        return false;
    }

private:
    std::atomic<u32> seq{0};
    std::atomic<s32> mode{1};
    std::atomic<float> factor{1.0f};
    std::atomic<s32> uoffset{-1};
    std::atomic<float> game_pos_scaler{0.0f};
    std::array<std::atomic<float>, 16> inv_view{};
};

inline Mailbox g_mailbox;

/// True while frame_xr's immersive module is installed (an XR session exists).
inline std::atomic<bool> g_xr_active{false};

/// AzaharFrame depth: the game camera's projection as the emulation thread last measured it in
/// immersive modes 2 and 3 (vr_view_matrix.h ProjectionTracker), for the XR thread, which sizes
/// the head-locked screen with it. Both floats in one word, so a read is never torn; `ms` is
/// the steady clock in milliseconds when it was measured (0: never).
class MeasuredProjection {
public:
    void Publish(float tan_half_y, float aspect, int64_t now_ms) noexcept {
        u32 a, b;
        std::memcpy(&a, &tan_half_y, 4);
        std::memcpy(&b, &aspect, 4);
        packed.store((static_cast<u64>(a) << 32) | b, std::memory_order_relaxed);
        ms.store(now_ms, std::memory_order_release);
    }
    /// False when nothing was measured since `since_ms`; `stamp_ms` is when it was.
    bool Read(int64_t since_ms, float& tan_half_y, float& aspect,
              int64_t& stamp_ms) const noexcept {
        const int64_t t = ms.load(std::memory_order_acquire);
        if (t == 0 || t < since_ms) {
            return false;
        }
        stamp_ms = t;
        const u64 v = packed.load(std::memory_order_relaxed);
        const u32 a = static_cast<u32>(v >> 32), b = static_cast<u32>(v);
        std::memcpy(&tan_half_y, &a, 4);
        std::memcpy(&aspect, &b, 4);
        return true;
    }

private:
    std::atomic<u64> packed{0};
    std::atomic<int64_t> ms{0};
};

inline MeasuredProjection g_measured_projection;

/// The steady clock in milliseconds, the time base of g_measured_projection.
inline int64_t SteadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/// AzaharFrame depth: Match Game Camera, as the XR thread wants it now (the setting, not forced
/// off by AZAHARFRAME_IMMERSIVE_MEASURE=0 or AZAHARFRAME_IMMERSIVE_FOV, a game in a Full mode).
/// False: the emulation thread measures nothing.
inline std::atomic<bool> g_measure_camera{false};

/// AzaharFrame depth: one patched draw in a Full mode (`matrix_mode` 2 or 3) with the view
/// matrix at register `view_index` of `f` (`count` registers, rows of 4). When `enabled`, reads
/// the projection in the four registers before it into `tracker` and publishes the winner of
/// each window to `out`. When not, returns at once: no register read, no clock, no bookkeeping.
/// True when the registers were read.
template <typename Registers, typename Clock>
inline bool MeasureDraw(bool enabled, const Registers& f, std::size_t count, int view_index,
                        int matrix_mode, ProjectionTracker& tracker, MeasuredProjection& out,
                        Clock&& now_ms) {
    if (!enabled || view_index < 4 || count <= static_cast<std::size_t>(view_index) ||
        (matrix_mode != 2 && matrix_mode != 3)) {
        return false;
    }
    float rows[4][4];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            rows[i][j] = f[view_index - 4 + i][j];
        }
    }
    Projection p, winner;
    if (!ProjectionFromRows(rows, p)) {
        return true;
    }
    const int64_t now = now_ms();
    if (tracker.Add(p, now, winner)) {
        out.Publish(winner.tan_half_y, winner.aspect, now);
    }
    return true;
}

/// XR thread: the newest VR data for the emulation thread.
inline void Publish(const VRData& data) {
    g_mailbox.Publish(data);
}

/// Emulation thread, once per guest frame: hands the newest snapshot to the rasterizer (again
/// for a new rasterizer, as every game boot makes one).
inline void ApplyPending(RasterizerInterface* rasterizer) {
    static u32 last_serial = 0;
    static const RasterizerInterface* last_rasterizer = nullptr;
    if (!rasterizer) {
        return;
    }
    VRData d;
    u32 serial = 0;
    if (!g_mailbox.Read(d, serial)) {
        return;
    }
    if (serial == last_serial && rasterizer == last_rasterizer) {
        return;
    }
    last_serial = serial;
    last_rasterizer = rasterizer;
    rasterizer->SetVRData(d.mode, d.factor, d.uoffset, d.game_pos_scaler, d.inv_view.data());
}

/// CitraVR (vr_main.cpp:402) renders two resolution steps higher while immersive mode is set,
/// as the top screen covers much more of the view. Capped at 8x (SteamVR on the Frame scales
/// down layers wider than 3458 px, see renderer_vulkan_xr.cpp), never below the base.
inline u32 AdjustResolution(u32 base) {
    constexpr u32 kSteps = 2;
    constexpr u32 kMaxScale = 8;
    if (!g_xr_active.load(std::memory_order_relaxed) ||
        Settings::values.vr_immersive_mode.GetValue() == 0) {
        return base;
    }
    return std::min(base + kSteps, std::max(base, kMaxScale));
}

} // namespace VideoCore::Immersive
