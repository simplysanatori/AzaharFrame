// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame pacing: the game's pictures on a 120 Hz headset.
//
// The 3DS draws 59.83 pictures a second (4481136 ARM11 cycles a frame), the Frame shows 120. Left
// to run free, the moment a picture is released drifts through the headset's refresh and crosses
// the moment the frame loop latches the screens for xrEndFrame once every 2.94 s. With the release
// time's own jitter (emulation, then 4 to 7 ms of GPU) each crossing is about a second of pictures
// on view for 1 and 3 refreshes instead of 2 and 2: a double image on fast head turns.
//
// Two parts, both only at 120 Hz:
// - the gate (frame_xr/pacing.cpp, AZAHARFRAME_XR_PACE_GATE): the release thread holds a picture
//   until the one on view has been in two ended frames, so no picture is ever shown for one;
// - the lock (FrameLimiter, AZAHARFRAME_XR_PACE_LOCK): the game runs at exactly half the display
//   rate (60.000 at 120 Hz, 0.28 % fast) and the frame limiter nudges its sleeps so frames are
//   ready at a set place between two latches, measured against the frame loop's own latch times:
//   the middle without the gate, just after the window opens with it (kGatedTarget).
// The audio (audio_core/rate_trim.h) is read 0.28 % faster to match while the lock runs.
//
// This header holds the shared state (XR side to limiter and audio) and the arithmetic, so the
// tests can reach it without a session.

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace Common::XrPacer {

/// The 3DS's frame in microseconds: 4481136 ARM11 cycles at 268111856 Hz (59.83 Hz).
constexpr double kEmuFrameUs = 4481136.0 * 1e6 / 268111856.0;

/// A display period the game's 60 pictures a second divide evenly into two refreshes each:
/// 120 Hz (8.33 ms) give or take 2 %. Not 90 Hz (60 on 90 has no even cadence).
constexpr bool EvenCadencePeriod(int64_t period_ns) {
    return period_ns >= 8'166'000 && period_ns <= 8'500'000;
}

/// `x` wrapped to [-P/2, P/2).
inline double WrapUs(double x, double period_us) {
    if (period_us <= 0) {
        return 0;
    }
    x = std::fmod(x, period_us);
    if (x < -period_us / 2) {
        x += period_us;
    } else if (x >= period_us / 2) {
        x -= period_us;
    }
    return x;
}

/// Where a game frame was ready (to be released), `since_latch_us` after the last latch: its
/// distance from the middle of the window it goes up at the end of, wrapped to [-P/2, P/2).
/// Negative: early (just after a latch), positive: late (just before the next one). The time left
/// to the latch is P/2 - the result.
inline double PhaseErrorUs(double since_latch_us, double period_us) {
    return WrapUs(since_latch_us - period_us / 2, period_us);
}

/// What the lock is told about a game frame ready `since_latch_us` after the last latch: its
/// PhaseErrorUs when it goes up at the next latch. When the parity gate holds it instead (the one
/// before is on view once, or not yet: `latches_needed` more ended frames until it may go), the
/// game runs ahead of the cadence the gate keeps, and against the latches alone the frame could
/// look early or late. Then the error is how far it was from the middle of the window it may go
/// in: -(the hold + P/2), below -P/2, not wrapped. The lock moves the game that much later.
inline double SampleErrorUs(double since_latch_us, uint32_t latches_needed, double period_us) {
    if (latches_needed == 0) {
        return PhaseErrorUs(since_latch_us, period_us);
    }
    const double hold_us = std::max(0.0, latches_needed * period_us - since_latch_us);
    return -(hold_us + period_us / 2);
}

/// The game frames' phase errors, smoothed: an average that follows the phase around the window
/// (one refresh). Each error is taken relative to the average so far (wrapped to the nearest),
/// so errors straddling the window's ends (some just before, some just after) average to "at the
/// end", not to the middle as a plain average of the wrapped errors would.
class PhaseFilter {
public:
    static constexpr double kAlpha = 0.02; ///< about a second (50 game frames) of memory

    explicit PhaseFilter(double alpha = kAlpha) : alpha{alpha} {}

    void Add(double error_us, double period_us) {
        if (period_us <= 0) {
            return;
        }
        if (period_us != period) {
            Reset();
            period = period_us;
        }
        const double a = 2 * std::numbers::pi * error_us / period_us;
        if (samples == 0) {
            mean = error_us;
            c = std::cos(a);
            s = std::sin(a);
        } else {
            mean = WrapUs(mean + alpha * WrapUs(error_us - mean, period_us), period_us);
            c += alpha * (std::cos(a) - c);
            s += alpha * (std::sin(a) - s);
        }
        samples++;
    }
    /// The releases from now on come `us` later (a correction made after the samples so far).
    void Shift(double us) {
        if (samples) {
            mean = WrapUs(mean + us, period);
        }
    }
    /// The smoothed error in [-P/2, P/2) (0 with no samples).
    double MeanUs() const {
        return samples ? mean : 0.0;
    }
    /// 1: every release at one phase; near 0: spread over the whole window.
    double Concentration() const {
        return std::hypot(c, s);
    }
    uint32_t Samples() const {
        return samples;
    }
    void Reset() {
        mean = c = s = 0;
        samples = 0;
    }

private:
    double alpha;
    double period = 0;
    double mean = 0;
    double c = 0, s = 0;
    uint32_t samples = 0;
};

/// With the gate the errors are not wrapped (a held frame's says how far early it is, up to more
/// than a refresh): a plain average, kept in step with the corrections like PhaseFilter.
class LinearFilter {
public:
    explicit LinearFilter(double alpha = PhaseFilter::kAlpha) : alpha{alpha} {}
    void Add(double error_us) {
        mean = samples == 0 ? error_us : mean + alpha * (error_us - mean);
        samples++;
    }
    void Shift(double us) {
        if (samples) {
            mean += us;
        }
    }
    double MeanUs() const {
        return samples ? mean : 0.0;
    }
    uint32_t Samples() const {
        return samples;
    }
    void Reset() {
        mean = 0;
        samples = 0;
    }

private:
    double alpha;
    double mean = 0;
    uint32_t samples = 0;
};

/// The parity gate's bookkeeping. The XR loop calls Latch after each xrEndFrame with the game
/// frame (release serial) that frame showed; the release thread asks Open before releasing the
/// next one. Keyed on ended frames, not on the clock: once a late picture moves the cadence to
/// the other parity, the pictures after it wait for that parity too instead of flipping back
/// (3/1/3/1); the next one is let through only after the one on view was in two ended frames.
struct Gate {
    uint64_t latches = 0; ///< frames ended
    uint64_t shown = 0;   ///< the game frame in the last ended frame (0: none)
    uint32_t count = 0;   ///< how many ended frames in a row it was in
    uint32_t runs[4]{};   ///< pictures on view for 1, 2, 3, 4 or more frames (TakeRuns)

    void Latch(uint64_t serial) {
        latches++;
        if (serial != 0 && serial == shown) {
            count++;
            return;
        }
        if (shown != 0 && count > 0) {
            runs[std::min<uint32_t>(count, 4) - 1]++;
        }
        shown = serial;
        count = serial != 0 ? 1 : 0;
    }
    /// The game frame released last (`released`) has been on view in two ended frames: the next
    /// may replace it. Not yet on view (0 frames) or on view once: wait.
    bool Open(uint64_t released) const {
        return released == 0 || (released == shown && count >= 2) || released < shown;
    }
    void TakeRuns(uint32_t out[4]) {
        std::copy(runs, runs + 4, out);
        std::fill(runs, runs + 4, 0u);
    }
};

/// Why the lock is off (None: on).
enum class Off : uint8_t {
    None,
    Switch,       ///< AZAHARFRAME_XR_PACE_LOCK=0
    File,         ///< the user folder's xr_pace_off file
    NoSession,    ///< no VR session shows the game
    Period,       ///< not 120 Hz
    FrameLimit,   ///< the speed limit is not 100 %
    FrameAdvance, ///< frame advance
    Slow,         ///< the limiter has not slept for 30 frames: the game runs below full speed
};

inline const char* OffText(Off off) {
    switch (off) {
    case Off::None:
        return "on";
    case Off::Switch:
        return "AZAHARFRAME_XR_PACE_LOCK=0";
    case Off::File:
        return "xr_pace_off";
    case Off::NoSession:
        return "no session";
    case Off::Period:
        return "not 120 Hz";
    case Off::FrameLimit:
        return "speed limit not 100 %";
    case Off::FrameAdvance:
        return "frame advance";
    case Off::Slow:
        return "below full speed";
    }
    return "?";
}

struct LockInputs {
    bool switch_on = true;
    bool file_off = false;
    bool session = false;
    int64_t period_ns = 0;
    double frame_limit = 100;
    bool frame_advance = false;
    bool slow = false;
};

constexpr Off DecideLock(const LockInputs& in) {
    if (!in.switch_on) {
        return Off::Switch;
    }
    if (in.file_off) {
        return Off::File;
    }
    if (!in.session) {
        return Off::NoSession;
    }
    if (!EvenCadencePeriod(in.period_ns)) {
        return Off::Period;
    }
    if (in.frame_advance) {
        return Off::FrameAdvance;
    }
    if (in.frame_limit != 100) {
        return Off::FrameLimit;
    }
    if (in.slow) {
        return Off::Slow;
    }
    return Off::None;
}

/// The speed the lock runs the game at: one picture per two display periods (x1.00282 at
/// 120 Hz: 60.000 pictures a second).
inline double LockSpeed(int64_t period_ns) {
    return period_ns > 0 ? kEmuFrameUs / (2.0 * static_cast<double>(period_ns) / 1000.0) : 1.0;
}

/// With the gate the lock aims just after the window opens (0.1 P in, 0.4 P early of the middle):
/// a frame that comes early is only held (it still goes up at its latch), one that comes late
/// keeps the one before on view for three refreshes and moves the cadence a refresh later, which
/// the lock then has to make up. The mock's slow frames run 5 to 6 ms over the usual.
constexpr double kGatedTarget = -0.4;
constexpr double kGain = 0.05;            ///< of the smoothed phase error, per game frame
constexpr int64_t kMaxCorrectionUs = 400; ///< per game frame

/// The limiter's sleep change for a smoothed phase error: positive sleeps longer (the release
/// moves later), at most `limit` either way.
inline int64_t CorrectionUs(double mean_error_us, double gain = kGain,
                            int64_t limit = kMaxCorrectionUs) {
    const auto c = static_cast<int64_t>(std::llround(-gain * mean_error_us));
    return std::clamp<int64_t>(c, -limit, limit);
}

/// Below full speed: the limiter did not sleep for 30 frames in a row; full speed again once it
/// slept for 60 in a row.
class SlowDetector {
public:
    static constexpr uint32_t kOffAfter = 30, kOnAfter = 60;
    void Note(bool slept) {
        if (slept) {
            awake = 0;
            if (asleep < kOnAfter) {
                asleep++;
            }
            if (slow && asleep >= kOnAfter) {
                slow = false;
            }
        } else {
            asleep = 0;
            if (awake < kOffAfter) {
                awake++;
            }
            if (awake >= kOffAfter) {
                slow = true;
            }
        }
    }
    bool Slow() const {
        return slow;
    }

private:
    uint32_t awake = 0, asleep = 0;
    bool slow = false;
};

/// Shared between the XR side (writes the session, the period and the phase), the frame limiter
/// (the lock) and the audio output (the ratio). Process-wide.
struct Shared {
    // XR side.
    std::atomic<bool> lock_switch{true};  ///< AZAHARFRAME_XR_PACE_LOCK (not 0)
    std::atomic<bool> file_off{false};    ///< user/xr_pace_off exists
    std::atomic<bool> session{false};     ///< the last latch had the game's screens live
    std::atomic<int64_t> latch_ns{0};     ///< steady clock of the last latch
    std::atomic<int64_t> period_ns{0};    ///< the runtime's display period
    std::atomic<int64_t> phase_error_ns{0}; ///< the newest game frame's (SampleErrorUs)
    std::atomic<bool> phase_gated{false};   ///< it was measured with the gate on
    std::atomic<uint32_t> phase_seq{0};     ///< releases measured so far
    // Limiter.
    std::atomic<uint8_t> lock_off{static_cast<uint8_t>(Off::NoSession)};
    std::atomic<double> audio_ratio{1.0}; ///< the game's speed while locked, else 1
    std::atomic<int64_t> phase_mean_ns{0}; ///< the lock's smoothed phase error from its target
    std::atomic<int64_t> wake_ns{0};       ///< steady clock: the limiter last let the game go on
    // Per second (the XR side's log line takes and clears them).
    std::atomic<uint32_t> lock_frames{0};
    std::atomic<int64_t> correction_sum_us{0};
    std::atomic<uint32_t> correction_max_us{0}; ///< largest |correction|
    std::atomic<uint32_t> no_sleep_frames{0};
    std::atomic<uint32_t> audio_drops{0};
    std::atomic<uint32_t> audio_dropped_frames{0};
};

inline Shared& Get() {
    static Shared shared;
    return shared;
}

/// The frame limiter's half of the lock, once per game frame. The XR side publishes each game
/// frame's phase error (SampleErrorUs); the lock smooths them (PhaseFilter, about a second) and
/// moves the next wake-up by kGain of the smoothed error, at most kMaxCorrectionUs. With the gate
/// the errors are averaged as they are (a held frame's below -P/2), without it round the window. The game's speed is fed forward
/// (LockSpeed), so the corrections only hold the phase, they never carry the rate.
class LimiterLock {
public:
    struct Step {
        double speed = 1.0;        ///< multiplies the limiter's sleep scale
        int64_t correction_us = 0; ///< added to the limiter's sleep
        Off off = Off::NoSession;
    };

    /// `now_ns`: the steady clock. Without a latch for 100 ms there is no session.
    Step Frame(Shared& s, bool frame_advance, double frame_limit, int64_t now_ns) {
        LockInputs in;
        in.switch_on = s.lock_switch.load(std::memory_order_relaxed);
        in.file_off = s.file_off.load(std::memory_order_relaxed);
        in.session = s.session.load(std::memory_order_relaxed) &&
                     now_ns - s.latch_ns.load(std::memory_order_relaxed) < 100'000'000;
        in.period_ns = s.period_ns.load(std::memory_order_relaxed);
        in.frame_limit = frame_limit;
        in.frame_advance = frame_advance;
        in.slow = slow.Slow();
        Step step;
        step.off = DecideLock(in);
        s.lock_off.store(static_cast<uint8_t>(step.off), std::memory_order_relaxed);
        if (step.off != Off::None) {
            s.audio_ratio.store(1.0, std::memory_order_relaxed);
            circular.Reset();
            linear.Reset();
            last_correction_us = 0;
            last_seq = s.phase_seq.load(std::memory_order_relaxed); // a fresh start when back on
            return step;
        }
        step.speed = LockSpeed(in.period_ns);
        const uint32_t seq = s.phase_seq.load(std::memory_order_acquire);
        const double period_us = static_cast<double>(in.period_ns) / 1000.0;
        if (seq != last_seq) {
            last_seq = seq;
            const bool now_gated = s.phase_gated.load(std::memory_order_relaxed);
            if (now_gated != gated) {
                gated = now_gated;
                circular.Reset();
                linear.Reset();
            }
            // The newest frame was ready before the last correction took effect: as if after it.
            const double moved = s.phase_error_ns.load(std::memory_order_relaxed) / 1000.0 +
                                 static_cast<double>(last_correction_us);
            if (gated) {
                // A frame more than a refresh early counts as one: after a late frame the gate
                // holds the next ones by up to two refreshes, and the lock must not race.
                linear.Add(std::clamp(moved, -period_us, period_us / 2));
            } else {
                circular.Add(moved, period_us);
            }
        }
        const uint32_t samples = gated ? linear.Samples() : circular.Samples();
        const double mean = gated ? linear.MeanUs() : circular.MeanUs();
        const double target = gated ? kGatedTarget * period_us : 0.0;
        if (samples != 0) {
            step.correction_us = CorrectionUs(mean - target);
            // The frames from the next one on move by the correction: the estimate with them (no
            // waiting for the average to see it, which made the loop swing).
            linear.Shift(static_cast<double>(step.correction_us));
            circular.Shift(static_cast<double>(step.correction_us));
        }
        last_correction_us = step.correction_us;
        s.phase_mean_ns.store(
            static_cast<int64_t>(((gated ? linear.MeanUs() : circular.MeanUs()) - target) * 1000.0),
            std::memory_order_relaxed);
        s.audio_ratio.store(step.speed, std::memory_order_relaxed);
        s.lock_frames.fetch_add(1, std::memory_order_relaxed);
        s.correction_sum_us.fetch_add(step.correction_us, std::memory_order_relaxed);
        const auto mag = static_cast<uint32_t>(std::abs(step.correction_us));
        uint32_t prev = s.correction_max_us.load(std::memory_order_relaxed);
        while (mag > prev && !s.correction_max_us.compare_exchange_weak(prev, mag)) {
        }
        return step;
    }
    /// After the limiter's sleep (or none): the game goes on now (`now_ns`, the steady clock).
    void NoteWake(Shared& s, int64_t now_ns) {
        s.wake_ns.store(now_ns, std::memory_order_relaxed);
    }
    /// After the limiter's decision: whether it slept this frame.
    void NoteSleep(Shared& s, bool slept) {
        slow.Note(slept);
        if (!slept) {
            s.no_sleep_frames.fetch_add(1, std::memory_order_relaxed);
        }
    }

private:
    SlowDetector slow;
    PhaseFilter circular; ///< without the gate: any window will do
    LinearFilter linear;  ///< with it: the cadence's parity holds, held frames say how early
    bool gated = false;
    uint32_t last_seq = 0;
    int64_t last_correction_us = 0;
};

} // namespace Common::XrPacer
