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
#include "common/frame_log_gate.h" // AzaharFrame integ8
#include "common/frame_vr_depth.h" // AzaharFrame stereo
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
/// translation): the eye scale (game_pos_scaler: 3D Depth x the leaning, units per metre) x
/// half the IPD, left eye negative.
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

    /// AzaharFrame integ9: poses applied at the latches remembered for the tag offset.
    static constexpr int kLatched = 8;

    explicit ContentTrack(bool follow_world = true) noexcept : follow_world_(follow_world) {}

    /// A snapshot was handed to the rasterizer: the draws from now on use its pose.
    void Applied(const XrContentPose& pose) noexcept {
        applied_ = pose;
        // AzaharFrame integ9: tag offset +1 on the newest pose: the one applied after the latch.
        if (tag_next_ && pose.valid) {
            drawn_ = pose;
        }
        tag_next_ = false;
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
        drawn_ = follow_world_.load(std::memory_order_relaxed) && last_ == Kind::Earlier
                     ? copy->world
                     : by_latch;
        tag_next_ = false;
        Remember(by_latch);
        ShiftTag(tag_offset_.load(std::memory_order_relaxed));
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
        return follow_world_.load(std::memory_order_relaxed);
    }
    /// AzaharFrame dbl: the switch read again once the test-switch file is in the environment
    /// (g_content is made before main). AzaharFrame integ9: any thread, live.
    void SetFollowWorld(bool follow) noexcept {
        follow_world_.store(follow, std::memory_order_relaxed);
    }
    /// AzaharFrame integ9: AZAHARFRAME_IMMERSIVE_TAG_OFFSET (a test switch, any thread, live):
    /// each picture is tagged with the pose applied one latch earlier (-1) or later (+1) than
    /// the one the rules above give, as an A/B of the tag where it cannot be told (Mario Kart 7's
    /// pictures are all "not told"). The poses are the distinct ones applied at the latches;
    /// with no earlier one remembered the tag stays. +1 on the newest pose takes the one applied
    /// right after the latch (ApplyPending), the pose the game's next frame is drawn with.
    void SetTagOffset(int offset) noexcept {
        tag_offset_.store(std::clamp(offset, -1, 1), std::memory_order_relaxed);
    }
    int TagOffset() const noexcept {
        return tag_offset_.load(std::memory_order_relaxed);
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

    /// AzaharFrame integ9: a pose applied at a latch, if it differs from the last one remembered.
    void Remember(const XrContentPose& p) noexcept {
        if (!p.valid || (latched_count_ > 0 && latched_[(latched_next_ + kLatched - 1) %
                                                        kLatched].time == p.time)) {
            return;
        }
        latched_[latched_next_] = p;
        latched_next_ = (latched_next_ + 1) % kLatched;
        latched_count_ = std::min(latched_count_ + 1, kLatched);
    }
    /// AzaharFrame integ9: drawn_ moved by `offset` along the remembered poses (SetTagOffset).
    void ShiftTag(int offset) noexcept {
        if (offset == 0 || !drawn_.valid) {
            return;
        }
        // Age 0: the newest remembered, 1 the one before, ...
        for (int age = 0; age < latched_count_; ++age) {
            const XrContentPose& p = latched_[(latched_next_ + kLatched - 1 - age) % kLatched];
            if (p.time != drawn_.time) {
                continue;
            }
            if (offset < 0) {
                if (age + 1 < latched_count_) {
                    drawn_ = latched_[(latched_next_ + kLatched - 2 - age) % kLatched];
                }
            } else if (age > 0) {
                drawn_ = latched_[(latched_next_ + kLatched - age) % kLatched];
            } else {
                tag_next_ = true; // the newest: the pose applied after this latch
            }
            return;
        }
    }

    std::atomic<bool> follow_world_{true};
    std::atomic<int> tag_offset_{0};
    std::array<XrContentPose, kLatched> latched_{};
    int latched_next_ = 0;
    int latched_count_ = 0;
    bool tag_next_ = false;
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
/// AzaharFrame stereo: the game's far scenery, the two eyes' far points apart as a share of its
/// picture's width (StereoFrame), the median of the last second's frames with two eyes; below 0
/// when the last second had none. Written by the emulation thread once a second (FrameTrace),
/// read by the XR thread (frame_xr/immersive.cpp, its stereo line).
inline std::atomic<float> g_stereo_far_width{-1.0f};
/// AzaharFrame stereo: the game's far width per slider unit (FarPerSlider), every swap; below 0
/// until measured. The XR thread's stereo cap is made from it (frame_xr/immersive.cpp).
inline std::atomic<float> g_stereo_far_per_slider{-1.0f};
/// AzaharFrame dbl: the game's zero-parallax plane (ZeroParallaxMeasure), every swap: NDC per
/// unit of eye distance there, and its distance in the game's units; below 0 until measured.
/// The XR thread bounds mode 3's eye distance with it (frame_xr/immersive.cpp).
inline std::atomic<float> g_stereo_near_per_unit{-1.0f};
inline std::atomic<float> g_stereo_zero_distance{-1.0f};
/// AzaharFrame dbl: the share (0 to 1) of mode 3's eye distance (Depth Factor x IPD) the
/// rasterizer gives the game's two eyes, from the XR thread (StereoEyeLimit, EyeScale); the
/// head-motion scale never changes with it.
inline std::atomic<float> g_eye_scale{1.0f};
/// AzaharFrame natural: the game's eye separation per unit of its far width (EyePerFar: game
/// units per picture width), every swap; below 0 until measured. The XR thread's leaning scale
/// is made from it (frame_xr/immersive.cpp, UpdateNatural).
inline std::atomic<float> g_stereo_eye_per_far{-1.0f};
inline std::atomic<int> g_stereo_eye_samples{0}; ///< frames read so far (EyePerFar::Count)
/// AzaharFrame stereo: the XR thread adds one when another game starts (not a quick save loaded
/// in the same one), so the measures (and the menu rule's "two eyes earlier") start afresh.
inline std::atomic<u32> g_stereo_generation{0};
/// AzaharFrame oot: the game shows a screen of its own in front of its world (WorldOnceRun),
/// every swap. The XR thread's menu rule reads it (frame_xr/immersive.cpp).
inline std::atomic<bool> g_world_once{false};

/// AzaharFrame integ8: FrameTrace's two per-second lines in the Full modes (the game's eyes and
/// far scenery; the world pose) are written on a meaningful change, else every 10 s
/// (Common::FrameLog::ChangeGate). What they count is summed over the seconds since the line
/// before; what they last said is kept here. Kept outside FrameTrace, which resets itself each
/// second. The emulation thread only.
struct FrameTraceLog {
    // The stereo line: summed, and what it last said.
    std::array<u32, 3> eyes{};
    u32 shown_3d = 0, swaps = 0;
    u32 pairs = 0, pairs_apart = 0; ///< AzaharFrame natural: the eyes line, summed likewise
    int secs = 0;
    int said_main = -1;      ///< 0, 1 or 2: the game's eyes in most swaps
    bool said_3d = false;    ///< some swaps in the 3D format
    float said_far = -1.0f;  ///< far scenery, of the picture's width; below 0 not seen
    float said_per = -1.0f;  ///< far width per percent; below 0 not measured
    float said_slider = -1.0f;
    Common::FrameLog::ChangeGate stereo;
    // The world-pose line: summed, and what it last said.
    u32 w_swaps = 0, w_without_world = 0, w_without_copy = 0, w_same = 0, w_earlier = 0,
        w_untraced = 0;
    float w_turn_sum = 0.0f, w_turn_max = 0.0f;
    int w_secs = 0;
    bool said_earlier = false, said_without_world = false, said_follows = false;
    Common::FrameLog::ChangeGate world;

    void Reset() {
        *this = FrameTraceLog{};
    }
    /// The stereo line's values this second differ meaningfully from the ones last written:
    /// the game's eyes in most swaps, the 3D format, far scenery seen or 15 % apart (and 0.05 %
    /// of the width), the measure per percent 10 % apart, the slider shown half a percent.
    bool StereoChanged(int main, bool shown3d, float far, float per, float slider) const {
        using Common::FrameLog::Moved;
        return main != said_main || shown3d != said_3d || Moved(said_far, far, 0.15f, 0.0005f) ||
               Moved(said_per, per, 0.10f, 0.0f) || Moved(said_slider, slider, 0.0f, 0.005f);
    }
    /// The world-pose line's: pictures drawn with an earlier pose or none, swaps without a
    /// world draw or none, how pictures are tagged.
    bool WorldChanged(bool earlier, bool without_world, bool follows) const {
        return earlier != said_earlier || without_world != said_without_world ||
               follows != said_follows;
    }
};
inline FrameTraceLog g_frame_trace_log;

class FrameTrace {
public:
    static constexpr int kFarSamples = 64;
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
    /// AzaharFrame stereo: a world draw's far point across (FarAcrossFromRows).
    /// AzaharFrame dbl: and its near term and horizontal scale (NearTermFromRows; NaN: not read).
    /// AzaharFrame natural: and the eye's camera matrix before the patch (`m`, 3 rows), the
    /// projection's scale across (`p00`).
    void Far(float ndc, float k = std::nanf(""), float across = 0.0f,
             const float (*m)[4] = nullptr, float p00 = 0.0f) noexcept {
        stereo_.Draw(ndc, k, across, m, p00);
    }
    /// AzaharFrame oot (traced): one world upload's game uniforms, before the patch.
    void WorldUniforms(u64 hash) noexcept {
        frame_sum_ += hash;
        ++frame_uploads_;
    }
    /// A latch with a swap (`swap`) or the VBlank fallback; `track` after its Latch.
    /// `shown_3d`: the swap's framebuffer is in the 3DS's stereoscopic format.
    void Latch(bool swap, const ContentTrack& track, bool full_mode, int64_t now_ms,
               bool shown_3d = false) {
        if (swap) {
            ++swaps_;
            // AzaharFrame stereo: the game's eyes in this frame (its world draws' far points).
            const StereoFrame::Result st = stereo_.End();
            const u32 gen = g_stereo_generation.load(std::memory_order_relaxed);
            if (gen != generation_) {
                generation_ = gen;
                far_per_slider_.Reset();
                eye_per_far_.Reset();
                world_once_.Reset();
                zero_.Reset();
            }
            const float slider = Settings::frame_vr_slider_shown.load(std::memory_order_relaxed);
            far_per_slider_.Swap(st.eyes, st.far_ndc / 2.0f, slider);
            g_stereo_far_per_slider.store(far_per_slider_.Value(), std::memory_order_relaxed);
            // AzaharFrame dbl: where the game's own two eyes agree.
            zero_.Swap(st);
            g_stereo_near_per_unit.store(zero_.PerUnit(), std::memory_order_relaxed);
            g_stereo_zero_distance.store(zero_.Distance(), std::memory_order_relaxed);
            // AzaharFrame natural: the game's eyes apart, per its far width.
            eye_per_far_.Swap(st.eyes, st.far_ndc, st.eye_apart, st.p00, st.zero_distance);
            g_stereo_eye_per_far.store(eye_per_far_.Value(), std::memory_order_relaxed);
            g_stereo_eye_samples.store(eye_per_far_.Count(), std::memory_order_relaxed);
            if (st.have_m) {
                last_pair_ = st;
                ++pairs_;
                if (st.eye_apart > 0.0f) {
                    ++pairs_apart_;
                }
            }
            // AzaharFrame oot: a menu of the game's own (its world drawn once).
            g_world_once.store(world_once_.Swap(st.eyes, slider), std::memory_order_relaxed);
            ++eyes_[std::clamp(st.eyes, 0, 2)];
            if (shown_3d) {
                ++shown_3d_;
            }
            if (st.eyes == 2 && far_len_ < kFarSamples) {
                far_[far_len_++] = st.far_ndc / 2.0f;
            }
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
        float far_width = -1.0f;
        if (far_len_ > 0) {
            std::nth_element(far_.begin(), far_.begin() + far_len_ / 2, far_.begin() + far_len_);
            far_width = far_[far_len_ / 2];
        }
        g_stereo_far_width.store(far_width, std::memory_order_relaxed);
        if (full_mode && swaps_ > 0) {
            // AzaharFrame integ8: on a meaningful change, else every 10 s, summed since the last.
            FrameTraceLog& lg = g_frame_trace_log;
            if (generation_ != lg_generation_) {
                lg.Reset();
                lg_generation_ = generation_;
            }
            for (int e = 0; e < 3; ++e) {
                lg.eyes[e] += eyes_[e];
            }
            lg.shown_3d += shown_3d_;
            lg.pairs += pairs_;
            lg.pairs_apart += pairs_apart_;
            lg.swaps += swaps_;
            ++lg.secs;
            const int main = eyes_[2] >= eyes_[1] && eyes_[2] >= eyes_[0] ? 2
                             : eyes_[1] >= eyes_[0]                     ? 1
                                                                        : 0;
            const float per = far_per_slider_.Value();
            const float slider = Settings::frame_vr_slider_shown.load(std::memory_order_relaxed);
            const auto say = lg.stereo.Update(
                now_ms, lg.StereoChanged(main, shown_3d_ > 0, far_width, per, slider));
            if (say != Common::FrameLog::ChangeGate::Say::Nothing) {
                LOG_INFO(HW_GPU,
                         "xr: immersive stereo: {} top screen swaps with the game's two eyes, {} "
                         "with its world drawn once, {} with no world camera; {} in the 3D "
                         "format; far scenery {} of the picture's width apart ({} per percent of "
                         "the slider, shown {:.1f} %); the game's zero-parallax plane {} (over {} "
                         "s, {})",
                         lg.eyes[2], lg.eyes[1], lg.eyes[0], lg.shown_3d,
                         far_width >= 0.0f ? fmt::format("{:.2f} %", far_width * 100.0f)
                                           : std::string("not seen"),
                         per >= 0.0f ? fmt::format("{:.4f} %", per) : std::string("not measured"),
                         100.0f * slider,
                         zero_.Distance() > 0.0f
                             ? fmt::format("{:.1f} units ahead", zero_.Distance())
                             : std::string("not measured"),
                         lg.secs, Common::FrameLog::Why(say));
                // AzaharFrame natural: the game's eye separation (its camera matrices' difference).
                const auto& ma = last_pair_.ma;
                const auto& mb = last_pair_.mb;
                LOG_INFO(HW_GPU,
                         "xr: immersive eyes: {} of {} two-eye swaps read the eyes apart; last {} "
                         "units (far {:.5f} NDC, P00 {:.3f}); {} units per picture width, zero "
                         "parallax at {} units; eye matrices [{:.4f} {:.4f} {:.4f} {:.4f} | {:.4f} "
                         "{:.4f} {:.4f} {:.4f} | {:.4f} {:.4f} {:.4f} {:.4f}] and [{:.4f} {:.4f} "
                         "{:.4f} {:.4f} | {:.4f} {:.4f} {:.4f} {:.4f} | {:.4f} {:.4f} {:.4f} {:.4f}]",
                         lg.pairs_apart, lg.pairs,
                         last_pair_.eye_apart >= 0.0f ? fmt::format("{:.4f}", last_pair_.eye_apart)
                                                      : std::string("not"),
                         last_pair_.far_ndc, last_pair_.p00,
                         eye_per_far_.Value() >= 0.0f ? fmt::format("{:.2f}", eye_per_far_.Value())
                                                      : std::string("not measured"),
                         eye_per_far_.ZeroParallax() >= 0.0f
                             ? fmt::format("{:.1f}", eye_per_far_.ZeroParallax())
                             : std::string("not measured"),
                         ma[0][0], ma[0][1], ma[0][2], ma[0][3], ma[1][0], ma[1][1], ma[1][2],
                         ma[1][3], ma[2][0], ma[2][1], ma[2][2], ma[2][3], mb[0][0], mb[0][1],
                         mb[0][2], mb[0][3], mb[1][0], mb[1][1], mb[1][2], mb[1][3], mb[2][0],
                         mb[2][1], mb[2][2], mb[2][3]);
                lg.eyes = {};
                lg.shown_3d = lg.swaps = 0;
                lg.pairs = lg.pairs_apart = 0;
                lg.secs = 0;
                lg.said_main = main;
                lg.said_3d = shown_3d_ > 0;
                lg.said_far = far_width;
                lg.said_per = per;
                lg.said_slider = slider;
            }
            lg.w_swaps += swaps_;
            lg.w_without_world += swaps_without_world_;
            lg.w_without_copy += swaps_without_copy_;
            lg.w_same += same_;
            lg.w_earlier += earlier_;
            lg.w_untraced += untraced_;
            lg.w_turn_sum += turn_sum_;
            lg.w_turn_max = std::max(lg.w_turn_max, turn_max_);
            ++lg.w_secs;
            const bool follows = track.FollowsWorld();
            const auto wsay = lg.world.Update(
                now_ms, lg.WorldChanged(earlier_ > 0, swaps_without_world_ > 0, follows));
            if (wsay != Common::FrameLog::ChangeGate::Say::Nothing) {
                LOG_INFO(HW_GPU,
                         "xr: immersive: {} top screen swaps, {} without a world draw, {} "
                         "without a copy into a framebuffer; the picture's world drawn with the "
                         "latch's pose {}, an earlier one {} (the head {:.2f} degrees on, max "
                         "{:.2f}), not told {}; pictures tagged {} (over {} s, {})",
                         lg.w_swaps, lg.w_without_world, lg.w_without_copy, lg.w_same,
                         lg.w_earlier,
                         lg.w_earlier ? lg.w_turn_sum / static_cast<float>(lg.w_earlier) : 0.0f,
                         lg.w_turn_max, lg.w_untraced,
                         follows ? "with the world's pose" : "by the latch", lg.w_secs,
                         Common::FrameLog::Why(wsay));
                lg.w_swaps = lg.w_without_world = lg.w_without_copy = lg.w_same = lg.w_earlier =
                    lg.w_untraced = 0;
                lg.w_turn_sum = lg.w_turn_max = 0.0f;
                lg.w_secs = 0;
                lg.said_earlier = earlier_ > 0;
                lg.said_without_world = swaps_without_world_ > 0;
                lg.said_follows = follows;
            }
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
        if (!full_mode) {
            g_frame_trace_log.Reset(); // AzaharFrame integ8: the next Full mode logs at once
        }
        const u64 keep_sum = last_sum_;
        const bool keep_have = have_last_sum_;
        const StereoFrame keep_stereo = stereo_;
        const FarPerSlider keep_per_slider = far_per_slider_;
        const EyePerFar keep_eye_per_far = eye_per_far_;
        const WorldOnceRun keep_world_once = world_once_;
        const ZeroParallaxMeasure keep_zero = zero_;
        const u32 keep_generation = generation_;
        const u32 keep_lg_generation = lg_generation_;
        *this = FrameTrace{};
        lg_generation_ = keep_lg_generation;
        stereo_ = keep_stereo;
        far_per_slider_ = keep_per_slider;
        eye_per_far_ = keep_eye_per_far;
        world_once_ = keep_world_once;
        zero_ = keep_zero;
        generation_ = keep_generation;
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
    StereoFrame stereo_;               ///< AzaharFrame stereo: this frame's world far points
    FarPerSlider far_per_slider_;      ///< AzaharFrame stereo: the game's 3D per slider unit
    EyePerFar eye_per_far_;            ///< AzaharFrame natural: its eyes apart per far width
    StereoFrame::Result last_pair_;    ///< the last swap whose two eyes' matrices were read
    u32 pairs_ = 0, pairs_apart_ = 0;  ///< this second (reset with the rest each second, summed
                                       ///< into g_frame_trace_log): swaps with both eyes' matrices
                                       ///< read, and with the eyes apart
    WorldOnceRun world_once_;          ///< AzaharFrame oot: a menu of the game's own
    ZeroParallaxMeasure zero_;         ///< AzaharFrame dbl: where the game's two eyes agree
    u32 generation_ = 0;               ///< g_stereo_generation it was reset for
    u32 lg_generation_ = 0;            ///< AzaharFrame integ8: the one g_frame_trace_log is for
    std::array<u32, 3> eyes_{};        ///< swaps with 0, 1, 2 eyes this second
    u32 shown_3d_ = 0;                 ///< swaps in the 3D format this second
    std::array<float, kFarSamples> far_{};
    int far_len_ = 0;
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
/// AzaharFrame stereo: and where it puts the far scenery across (`far_valid`: read).
/// AzaharFrame dbl: and its near term and horizontal scale (NearTermFromRows; NaN: not read).
/// AzaharFrame natural: and the eye's camera before the patch (`eye_m`, 3 rows; null: not read)
/// and the projection's scale across (`p00`).
inline void OnWorldDraw(float tan_half_y = 0.0f, float aspect = 0.0f, bool far_valid = false,
                        float far_ndc = 0.0f, float near_k = std::nanf(""),
                        float across = 0.0f, const float (*eye_m)[4] = nullptr,
                        float p00 = 0.0f) {
    g_content.WorldDraw();
    g_frame_trace.WorldDraw(tan_half_y, aspect, tan_half_y > 0.0f);
    if (far_valid) {
        g_frame_trace.Far(far_ndc, near_k, across, eye_m, p00);
    }
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
/// `shown_3d`: that framebuffer is in the 3D (stereoscopic) format.
inline void LatchFrame(RasterizerInterface* rasterizer, u32 shown = 0, bool shown_3d = false) {
    g_content.Latch(shown);
    g_frame_trace.Latch(shown != 0, g_content, g_content.Drawn().valid, SteadyMs(), shown_3d);
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
inline void OnTopScreenSwap(RasterizerInterface* rasterizer, u32 shown = 0,
                            bool shown_3d = false) {
    if (g_frame_latch.OnTopSwap()) {
        LatchFrame(rasterizer, shown, shown_3d);
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
