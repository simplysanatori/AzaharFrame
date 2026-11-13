// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: the hand-off for CitraVR's immersive mode between the frame_xr thread and the
// emulation thread.
//
// CitraVR called RasterizerInterface::SetVRData from its OpenXR thread while the emulation
// thread was drawing with the same fields (a data race). Here the XR thread only publishes a
// snapshot (a seqlock of atomics, one writer); the emulation thread takes the newest one once
// per game frame (when the game swaps the top screen, FrameLatch) and calls SetVRData itself,
// so the rasterizer is only ever touched from the thread that draws.
//
// Header only: video_core does not link frame_xr; frame_xr links video_core and publishes.
// Nothing is published without an XR session, so a flat run behaves as upstream.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "common/common_types.h"
#include "common/logging/log.h" // AzaharFrame oot
#include "common/settings.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/vr_view_matrix.h" // AzaharFrame depth
#include "video_core/xr_content_pose.h" // AzaharFrame doubling

namespace VideoCore::Immersive {

/// The arguments of RasterizerInterface::SetVRData. The defaults are CitraVR's "off" values
/// (vr_main.cpp DisableImmersiveMode): mode 1 with factor 1 changes nothing.
struct VRData {
    s32 mode = 1;
    float factor = 1.0f;
    float factor_y = 1.0f; ///< AzaharFrame immersive: the factor up and down
    s32 uoffset = -1;
    float game_pos_scaler = 0.0f;
    std::array<float, 16> inv_view{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    /// AzaharFrame doubling: the head pose inv_view was made from (not for the rasterizer):
    /// the XR thread places the immersive screen where the head was when a picture was drawn.
    XrContentPose pose{};
    /// AzaharFrame ipd: the headset's IPD in metres (frame_xr/ipd_smoother.h), CitraVR's 0.065
    /// until it is known: mode 3 puts the game's eyes vr_game_pos_scaler x this apart.
    float ipd = 0.065f;
};

/// AzaharFrame ipd: the IPD the rasterizer uses: `ipd_m` when it is one, else `fallback_m`.
inline float UsableIpd(float ipd_m, float fallback_m) {
    return std::isfinite(ipd_m) && ipd_m >= 0.045f && ipd_m <= 0.085f ? ipd_m : fallback_m;
}

/// AzaharFrame ipd: mode 3's shift of the game camera for one eye (the view matrix's x
/// translation): the Depth Factor (game_pos_scaler) x half the IPD, left eye negative.
inline float GameEyeOffset(float game_pos_scaler, float ipd_m, bool left_eye) {
    return game_pos_scaler * (left_eye ? -1.0f : 1.0f) * (ipd_m / 2.0f);
}

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
        factor_y.store(data.factor_y, std::memory_order_relaxed);
        uoffset.store(data.uoffset, std::memory_order_relaxed);
        game_pos_scaler.store(data.game_pos_scaler, std::memory_order_relaxed);
        for (std::size_t i = 0; i < 16; i++) {
            inv_view[i].store(data.inv_view[i], std::memory_order_relaxed);
        }
        pose_valid.store(data.pose.valid, std::memory_order_relaxed);
        for (std::size_t i = 0; i < 4; i++) {
            pose_q[i].store(data.pose.orientation[i], std::memory_order_relaxed);
        }
        for (std::size_t i = 0; i < 3; i++) {
            pose_p[i].store(data.pose.position[i], std::memory_order_relaxed);
        }
        pose_time.store(data.pose.time, std::memory_order_relaxed);
        ipd.store(data.ipd, std::memory_order_relaxed);
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
            d.factor_y = factor_y.load(std::memory_order_relaxed);
            d.uoffset = uoffset.load(std::memory_order_relaxed);
            d.game_pos_scaler = game_pos_scaler.load(std::memory_order_relaxed);
            for (std::size_t i = 0; i < 16; i++) {
                d.inv_view[i] = inv_view[i].load(std::memory_order_relaxed);
            }
            d.pose.valid = pose_valid.load(std::memory_order_relaxed);
            for (std::size_t i = 0; i < 4; i++) {
                d.pose.orientation[i] = pose_q[i].load(std::memory_order_relaxed);
            }
            for (std::size_t i = 0; i < 3; i++) {
                d.pose.position[i] = pose_p[i].load(std::memory_order_relaxed);
            }
            d.pose.time = pose_time.load(std::memory_order_relaxed);
            d.ipd = ipd.load(std::memory_order_relaxed);
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
    std::atomic<float> factor_y{1.0f};
    std::atomic<s32> uoffset{-1};
    std::atomic<float> game_pos_scaler{0.0f};
    std::array<std::atomic<float>, 16> inv_view{};
    std::atomic<bool> pose_valid{false};
    std::array<std::atomic<float>, 4> pose_q{};
    std::array<std::atomic<float>, 3> pose_p{};
    std::atomic<int64_t> pose_time{0};
    std::atomic<float> ipd{0.065f};
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

/// AzaharFrame doubling: emulation thread only. `applied`: the head pose the rasterizer draws
/// with now (the last snapshot ApplyPending handed over); `drawn`: the one the picture the game
/// shows from the last latch on was drawn with (RendererVulkan::RenderToXr tags the XR images
/// with it).
///
/// AzaharFrame oot: the world in a picture is not always drawn in the game frame that shows it.
/// A game can draw its world every other frame and only its HUD in between (30 world pictures
/// in 60 swaps), or copy the picture into the framebuffer before it draws the next frame's world.
/// Tagging such a picture with the pose applied at the latch before gave it a pose its world was
/// not drawn with: on a head turn the anchored world stepped by the turn of one game frame and
/// back (a double image the far-scenery line cannot see, as it trusts the tag). So the track
/// follows the world itself: each draw through the game's patched world camera (WorldDraw) notes
/// the pose it used, each display transfer or texture copy (Transfer) notes which pose's world
/// it copied to its address, and the latch takes the pose of the newest copy into the
/// framebuffer the game shows. Where the game shows a framebuffer no copy went into lately (it
/// draws straight into it, or nothing was copied for a while), the earlier rule stays: the pose
/// applied at the latch before. Nothing in it depends on a game, a setting or a frame rate.
class ContentTrack {
public:
    /// Copies remembered (each framebuffer of a double- or triple-buffered game, both eyes).
    static constexpr int kTransfers = 8;
    /// A copy counts for this many latches (the latch it was made in and the ones after it, so
    /// a double-buffered game showing a picture copied one frame earlier is followed).
    static constexpr u32 kTransferLatches = 3;

    explicit ContentTrack(bool follow_world = true) noexcept : follow_world_(follow_world) {}

    /// A snapshot was handed to the rasterizer: the draws from now on use its pose.
    void Applied(const XrContentPose& pose) noexcept {
        applied_ = pose;
    }
    /// AzaharFrame oot: a draw through the game's world camera, patched with the applied pose.
    void WorldDraw() noexcept {
        world_ = applied_;
        have_world_ = true;
    }
    /// AzaharFrame oot: a display transfer or texture copy into `out` (a physical address): the
    /// picture there now holds the world of the newest world draw.
    void Transfer(u32 out) noexcept {
        if (out == 0) {
            return;
        }
        Copy& c = copies_[next_];
        next_ = (next_ + 1) % kTransfers;
        c = {out, latches_, have_world_, world_};
    }
    /// A game frame ended (FrameLatch). `shown`: the physical address of the top framebuffer
    /// the game shows from now on (0: no swap, the VBlank fallback; the earlier rule).
    void Latch(u32 shown = 0) noexcept {
        const XrContentPose by_latch = applied_; // the rule up to 1.2: the pose applied last
        const Copy* copy = shown != 0 ? NewestCopy(shown) : nullptr;
        last_ = Kind::Untraced;
        last_turn_deg_ = 0.0f;
        if (copy && copy->have_world && copy->world.valid && by_latch.valid) {
            if (copy->world.time == by_latch.time) {
                last_ = Kind::Same;
            } else {
                last_ = Kind::Earlier;
                last_turn_deg_ = TurnDeg(copy->world, by_latch);
            }
        }
        drawn_ = follow_world_ && last_ == Kind::Earlier ? copy->world : by_latch;
        ++latches_;
    }
    const XrContentPose& Drawn() const noexcept {
        return drawn_;
    }

    /// What the last latch found (for the log and the tests): the world of the picture shown was
    /// drawn with the pose the earlier rule gives (Same), with an earlier one (Earlier; the
    /// head turned LastTurnDeg between them), or it could not be told (Untraced).
    enum class Kind { Untraced, Same, Earlier };
    Kind LastKind() const noexcept {
        return last_;
    }
    float LastTurnDeg() const noexcept {
        return last_turn_deg_;
    }
    bool FollowsWorld() const noexcept {
        return follow_world_;
    }

    /// The angle between two head orientations, in degrees.
    static float TurnDeg(const XrContentPose& a, const XrContentPose& b) noexcept {
        // conj(a) x b, in doubles; atan2 keeps small angles exact (acos near 1 does not).
        const double ax = -a.orientation[0], ay = -a.orientation[1], az = -a.orientation[2],
                     aw = a.orientation[3];
        const double bx = b.orientation[0], by = b.orientation[1], bz = b.orientation[2],
                     bw = b.orientation[3];
        const double x = aw * bx + ax * bw + ay * bz - az * by;
        const double y = aw * by - ax * bz + ay * bw + az * bx;
        const double z = aw * bz + ax * by - ay * bx + az * bw;
        const double w = aw * bw - ax * bx - ay * by - az * bz;
        return static_cast<float>(2.0 * std::atan2(std::sqrt(x * x + y * y + z * z), std::abs(w)) *
                                  57.29577951308232);
    }

private:
    struct Copy {
        u32 out = 0;
        u32 latch = 0;
        bool have_world = false;
        XrContentPose world{};
    };
    const Copy* NewestCopy(u32 shown) const noexcept {
        for (int i = 1; i <= kTransfers; ++i) {
            const Copy& c = copies_[(next_ + kTransfers - i) % kTransfers];
            if (c.out == shown && latches_ - c.latch < kTransferLatches) {
                return &c;
            }
        }
        return nullptr;
    }

    bool follow_world_ = true;
    XrContentPose applied_{};
    XrContentPose drawn_{};
    XrContentPose world_{};
    bool have_world_ = false;
    std::array<Copy, kTransfers> copies_{};
    int next_ = 0;
    u32 latches_ = 0;
    Kind last_ = Kind::Untraced;
    float last_turn_deg_ = 0.0f;
};
/// AzaharFrame oot: AZAHARFRAME_IMMERSIVE_WORLD_TAG=0 tags a picture with the pose applied at
/// the latch before, as up to 1.2 (A/B runs); the default follows the world (ContentTrack).
inline bool FollowWorldFromEnv() {
    const char* v = std::getenv("AZAHARFRAME_IMMERSIVE_WORLD_TAG");
    return !(v && v[0] == '0');
}
inline ContentTrack g_content{FollowWorldFromEnv()};

/// AzaharFrame oot: what the latches found, once a second in the log while a Full mode is on
/// (emulation thread). Per top-screen swap: how many draws went through the patched world camera
/// and how many copies into a framebuffer were made, and whether the world of the picture shown
/// was drawn with the latch's pose, an earlier one (and how far the head had turned between
/// them), or could not be told. With AZAHARFRAME_IMMERSIVE_FRAME_TRACE=1 also the world draws of
/// each swap in order (the first 24 of each second) and the distinct world projections.
class FrameTrace {
public:
    static constexpr int kSequence = 24;
    static constexpr int kProjections = 6;

    void WorldDraw(float tan_half_y, float aspect, bool traced) noexcept {
        ++world_draws_;
        if (!traced) {
            return;
        }
        for (int i = 0; i < projections_; ++i) {
            Proj& pr = proj_[i];
            if (std::abs(pr.tan_half_y - tan_half_y) <= 0.002f * pr.tan_half_y &&
                std::abs(pr.aspect - aspect) <= 0.002f * pr.aspect) {
                ++pr.draws;
                return;
            }
        }
        if (projections_ < kProjections) {
            proj_[projections_++] = {tan_half_y, aspect, 1};
        } else {
            ++other_proj_;
        }
    }
    void Transfer() noexcept {
        ++copies_;
    }
    /// AzaharFrame oot (traced): one world upload's game uniforms, before the patch.
    void WorldUniforms(u64 hash) noexcept {
        frame_sum_ += hash;
        ++frame_uploads_;
    }
    /// A latch with a swap (`swap`) or the VBlank fallback; `track` after its Latch.
    void Latch(bool swap, const ContentTrack& track, bool full_mode, int64_t now_ms) {
        if (swap) {
            ++swaps_;
            if (world_draws_ == 0) {
                ++swaps_without_world_;
            }
            if (copies_ == 0) {
                ++swaps_without_copy_;
            }
            if (sequence_len_ < kSequence) {
                sequence_[sequence_len_++] = world_draws_;
            }
            if (frame_uploads_ > 0) {
                ++game_frames_;
                if (have_last_sum_ && frame_sum_ == last_sum_) {
                    ++game_repeats_;
                }
                last_sum_ = frame_sum_;
                have_last_sum_ = true;
            }
            frame_sum_ = 0;
            frame_uploads_ = 0;
            switch (track.LastKind()) {
            case ContentTrack::Kind::Same:
                ++same_;
                break;
            case ContentTrack::Kind::Earlier:
                ++earlier_;
                turn_sum_ += track.LastTurnDeg();
                turn_max_ = std::max(turn_max_, track.LastTurnDeg());
                break;
            default:
                ++untraced_;
            }
        }
        world_draws_ = 0;
        copies_ = 0;
        if (start_ms_ == 0) {
            start_ms_ = now_ms;
        }
        if (now_ms - start_ms_ < 1000) {
            return;
        }
        if (full_mode && swaps_ > 0) {
            LOG_INFO(HW_GPU,
                     "xr: immersive: {} top screen swaps, {} without a world draw, {} without a "
                     "copy into a framebuffer; the picture's world drawn with the latch's pose "
                     "{}, an earlier one {} (the head {:.2f} degrees on, max {:.2f}), not told "
                     "{}; pictures tagged {} (AZAHARFRAME_IMMERSIVE_WORLD_TAG)",
                     swaps_, swaps_without_world_, swaps_without_copy_, same_, earlier_,
                     earlier_ ? turn_sum_ / static_cast<float>(earlier_) : 0.0f, turn_max_,
                     untraced_, track.FollowsWorld() ? "with the world's pose" : "by the latch");
            if (Traced()) {
                std::string seq, proj;
                for (int i = 0; i < sequence_len_; ++i) {
                    seq += (i ? "," : "") + std::to_string(sequence_[i]);
                }
                for (int i = 0; i < projections_; ++i) {
                    char b[64];
                    std::snprintf(b, sizeof(b), "%s%.1f deg x %.3f (%u)", i ? ", " : "",
                                  2.0f * std::atan(proj_[i].tan_half_y) * 57.29578f,
                                  proj_[i].aspect, proj_[i].draws);
                    proj += b;
                }
                LOG_INFO(HW_GPU,
                         "xr: immersive frame trace: world draws per swap {}; world "
                         "projections {}{}; game uniforms the same as the swap before in {} of "
                         "{} swaps with world uploads",
                         seq, proj.empty() ? "none" : proj,
                         other_proj_ ? " and " + std::to_string(other_proj_) + " more" : "",
                         game_repeats_, game_frames_);
            }
        }
        const u64 keep_sum = last_sum_;
        const bool keep_have = have_last_sum_;
        *this = FrameTrace{};
        start_ms_ = now_ms;
        last_sum_ = keep_sum;
        have_last_sum_ = keep_have;
    }
    static bool Traced() {
        static const bool on = [] {
            const char* v = std::getenv("AZAHARFRAME_IMMERSIVE_FRAME_TRACE");
            return v && v[0] == '1';
        }();
        return on;
    }

private:
    struct Proj {
        float tan_half_y = 0.0f;
        float aspect = 0.0f;
        u32 draws = 0;
    };
    u32 world_draws_ = 0, copies_ = 0;
    u32 swaps_ = 0, swaps_without_world_ = 0, swaps_without_copy_ = 0;
    u32 same_ = 0, earlier_ = 0, untraced_ = 0;
    float turn_sum_ = 0.0f, turn_max_ = 0.0f;
    std::array<u32, kSequence> sequence_{};
    int sequence_len_ = 0;
    std::array<Proj, kProjections> proj_{};
    int projections_ = 0;
    u32 other_proj_ = 0;
    u64 frame_sum_ = 0, last_sum_ = 0;
    u32 frame_uploads_ = 0, game_frames_ = 0, game_repeats_ = 0;
    bool have_last_sum_ = false;
    int64_t start_ms_ = 0;
};

/// AzaharFrame oot: FNV-1a of `size` bytes (the frame trace).
inline u64 HashBytes(const void* data, std::size_t size) noexcept {
    const auto* p = static_cast<const unsigned char*>(data);
    u64 h = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i) {
        h = (h ^ p[i]) * 1099511628211ull;
    }
    return h;
}
inline FrameTrace g_frame_trace;

/// AzaharFrame oot: a draw through the game's patched world camera (emulation thread), with the
/// projection in the four registers before the view matrix (0: not read).
inline void OnWorldDraw(float tan_half_y = 0.0f, float aspect = 0.0f) {
    g_content.WorldDraw();
    g_frame_trace.WorldDraw(tan_half_y, aspect, tan_half_y > 0.0f);
}

/// AzaharFrame oot: GPU::MemoryTransfer copied a picture to `out` (physical; emulation thread).
inline void OnTransfer(u32 out) {
    g_content.Transfer(out);
    g_frame_trace.Transfer();
}

/// Emulation thread, once per game frame (FrameLatch): hands the newest snapshot to the
/// rasterizer (again for a new rasterizer, as every game boot makes one).
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
    rasterizer->SetVRFactorY(d.factor_y);
    rasterizer->SetVRIpd(d.ipd);
    g_content.Applied(d.pose);
}

/// AzaharFrame doubling: a game frame ended (a latch): its draws used the pose applied so far.
/// `shown`: the top framebuffer the game shows from now on (0: no swap, the VBlank fallback).
inline void LatchFrame(RasterizerInterface* rasterizer, u32 shown = 0) {
    g_content.Latch(shown);
    g_frame_trace.Latch(shown != 0, g_content, g_content.Drawn().valid, SteadyMs());
    ApplyPending(rasterizer);
}

/// AzaharFrame doubling: the head pose the picture on the screens now was drawn with
/// (emulation thread, when the renderer presents). Invalid outside immersive modes 2 and 3.
inline XrContentPose DrawnContentPose() {
    return g_content.Drawn();
}

/// AzaharFrame immersive: when the emulation thread may give the rasterizer a new head pose.
///
/// Every draw of one game frame, both eyes, has to use the same pose. The pose used to change
/// at every VBlank (60 a second), but a game that runs at 30 frames a second (Ocarina of Time
/// 3D, Majora's Mask 3D) draws one frame across two VBlanks: the draws after the VBlank got a
/// newer pose than the ones before, so the two eyes, and parts of one eye's picture, were drawn
/// from different head poses (doubled objects whenever the head moved). Now a new pose is taken
/// only when the game hands over a finished top screen (its framebuffer swap, the end of a
/// frame), or after kMaxVBlanks VBlanks without one (a game that does not swap, a loading
/// screen), so a paused swap never leaves the pose stale for long. Emulation thread only.
class FrameLatch {
public:
    static constexpr int kMaxVBlanks = 4;

    /// The game swapped the top screen's framebuffer: a frame ended. True: apply the pose now.
    bool OnTopSwap() noexcept {
        vblanks_ = 0;
        return true;
    }
    /// A VBlank. True: no swap for kMaxVBlanks VBlanks, apply the pose now.
    bool OnVBlank() noexcept {
        if (++vblanks_ < kMaxVBlanks) {
            return false;
        }
        vblanks_ = 0;
        return true;
    }

private:
    int vblanks_ = 0;
};

inline FrameLatch g_frame_latch;

/// GPU::SetBufferSwap for the top screen (emulation thread): a game frame ended.
/// `shown`: the physical address of the left framebuffer the game shows from now on.
inline void OnTopScreenSwap(RasterizerInterface* rasterizer, u32 shown = 0) {
    if (g_frame_latch.OnTopSwap()) {
        LatchFrame(rasterizer, shown);
    }
}

/// GPU::VBlankCallback (emulation thread).
inline void OnVBlank(RasterizerInterface* rasterizer) {
    if (g_frame_latch.OnVBlank()) {
        LatchFrame(rasterizer);
    }
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
