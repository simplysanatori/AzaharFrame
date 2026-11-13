// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame pacing: the parity gate, the phase error, the bounded correction, the auto-off rules
// and the lock's speed (common/xr_pacer.h), with small simulations of a 59.83 Hz game on a 120 Hz
// headset.

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/xr_pacer.h"

namespace P = Common::XrPacer;
using Catch::Approx;

namespace {
constexpr double kPeriodUs = 1e6 / 120.0;
constexpr int64_t kPeriodNs = 8'333'333;
} // namespace

TEST_CASE("XrPacer: a game frame's phase error wraps to the window around its middle",
          "[xr_pacer]") {
    const double p = kPeriodUs;
    // Without the gate: against the last latch, to the window's middle.
    CHECK(P::PhaseErrorUs(p / 2, p) == Approx(0).margin(1e-6));
    CHECK(P::PhaseErrorUs(0, p) == Approx(-p / 2));
    CHECK(P::PhaseErrorUs(p - 1, p) == Approx(p / 2 - 1));
    CHECK(P::PhaseErrorUs(1.5 * p, p) == Approx(0).margin(1e-6));
    CHECK(P::PhaseErrorUs(-1, p) == Approx(p / 2 - 1));
    for (double since = -6 * p; since < 6 * p; since += 97.3) {
        const double e = P::PhaseErrorUs(since, p);
        REQUIRE(e >= -p / 2);
        REQUIRE(e < p / 2);
    }
    // A frame the gate holds: how far from the middle of the window it may go in, unwrapped.
    CHECK(P::SampleErrorUs(p - 1, 0, p) == Approx(p / 2 - 1));
    CHECK(P::SampleErrorUs(p - 1000, 1, p) == Approx(-1000 - p / 2)); // held 1 ms
    CHECK(P::SampleErrorUs(1000, 2, p) == Approx(-(2 * p - 1000) - p / 2));
    CHECK(P::SampleErrorUs(p + 10, 1, p) == Approx(-p / 2)); // the latch is late: no hold left
    CHECK(P::WrapUs(123, 0) == 0);
}

TEST_CASE("XrPacer: the phase filter follows the phase around the window", "[xr_pacer]") {
    const double p = kPeriodUs;
    P::PhaseFilter f;
    CHECK(f.MeanUs() == 0);
    for (int i = 0; i < 200; i++) {
        f.Add(1000, p);
    }
    CHECK(f.MeanUs() == Approx(1000).margin(1));
    CHECK(f.Concentration() == Approx(1).margin(1e-6));

    // Releases straddling a latch (just before, just after): at the latch, not the middle.
    P::PhaseFilter g;
    for (int i = 0; i < 200; i++) {
        g.Add(i % 2 ? p / 2 - 300 : -p / 2 + 300, p);
    }
    CHECK(std::abs(g.MeanUs()) > p / 2 - 400);
    CHECK(g.Concentration() > 0.9);

    // A correction moves the estimate at once, wrapping round the window.
    f.Shift(-300);
    CHECK(f.MeanUs() == Approx(700).margin(1));
    f.Shift(p - 400); // a whole window less 400: the same place as -400
    CHECK(f.MeanUs() == Approx(300).margin(1));
    f.Shift(4000); // past the window's end: just after its start
    CHECK(f.MeanUs() == Approx(4300 - p).margin(1));
    CHECK(P::WrapUs(p / 2, p) == Approx(-p / 2));
    CHECK(P::WrapUs(-p / 2, p) == Approx(-p / 2));
}

TEST_CASE("XrPacer: the gate opens after two ended frames", "[xr_pacer]") {
    P::Gate g;
    CHECK(g.Open(0)); // nothing released yet
    g.Latch(1);
    CHECK_FALSE(g.Open(1)); // on view once: the next would leave it for one frame
    g.Latch(1);
    CHECK(g.Open(1));
    CHECK_FALSE(g.Open(2)); // released, not yet on view
    g.Latch(2);
    g.Latch(2);
    g.Latch(2);
    g.Latch(3);
    g.Latch(0); // nothing shown (session paused): ends 3's run
    uint32_t runs[4];
    g.TakeRuns(runs);
    CHECK(runs[0] == 1); // 3
    CHECK(runs[1] == 1); // 1
    CHECK(runs[2] == 1); // 2
    CHECK(runs[3] == 0);
    g.TakeRuns(runs);
    CHECK(runs[0] + runs[1] + runs[2] + runs[3] == 0);
    CHECK(g.latches == 7);
}

namespace {

struct CadenceResult {
    uint32_t runs[4]{};
    uint32_t waits = 0, timeouts = 0;
};

/// A free-running game (pictures every `frame_us`, ready at a jittered time) against latches every
/// period, with or without the gate (a release waits for Open, at most P + 2 ms). The release
/// thread is one thread: a release never overtakes the one before.
CadenceResult SimulateCadence(bool gated, double frame_us, double jitter_us, int frames,
                              unsigned seed) {
    std::mt19937 rng{seed};
    std::uniform_real_distribution<double> jitter{-jitter_us, jitter_us};
    P::Gate gate;
    CadenceResult out;
    double next_latch = kPeriodUs;
    double last_release = 0;
    uint64_t released = 0; // the newest released game frame
    const auto latch_until = [&](double t) {
        while (next_latch <= t) {
            gate.Latch(released);
            next_latch += kPeriodUs;
        }
    };
    for (int n = 1; n <= frames; n++) {
        double ready = std::max(last_release, n * frame_us + 4000 + jitter(rng));
        latch_until(ready);
        if (gated && !gate.Open(released)) {
            out.waits++;
            const double deadline = ready + kPeriodUs + 2000;
            while (!gate.Open(released) && next_latch <= deadline) {
                latch_until(next_latch);
            }
            if (!gate.Open(released)) {
                out.timeouts++;
                latch_until(deadline);
                ready = deadline;
            } else {
                ready = next_latch - kPeriodUs + 1; // just after the latch that opened it
            }
        }
        released = static_cast<uint64_t>(n);
        last_release = ready;
    }
    latch_until(last_release + 3 * kPeriodUs);
    gate.TakeRuns(out.runs);
    return out;
}

} // namespace

TEST_CASE("XrPacer: free running at 59.83 Hz shows pictures for one frame; the gate stops it",
          "[xr_pacer]") {
    const int frames = 60 * 30; // 30 s: about ten 2.94 s beats
    const CadenceResult free = SimulateCadence(false, P::kEmuFrameUs, 3000, frames, 1);
    INFO("free 1/2/3/4+ " << free.runs[0] << "/" << free.runs[1] << "/" << free.runs[2] << "/"
                          << free.runs[3]);
    CHECK(free.runs[0] > 50); // the doubling the owner saw (6/48/6, 15/29/16 a second)
    const CadenceResult gated = SimulateCadence(true, P::kEmuFrameUs, 3000, frames, 1);
    INFO("gated 1/2/3/4+ " << gated.runs[0] << "/" << gated.runs[1] << "/" << gated.runs[2] << "/"
                           << gated.runs[3] << ", waits " << gated.waits);
    CHECK(gated.runs[0] == 0);
    CHECK(gated.timeouts == 0);
    CHECK(gated.runs[1] > gated.runs[2] * 8); // mostly 2s, a 3 each beat
}

TEST_CASE("XrPacer: the auto-off rules", "[xr_pacer]") {
    P::LockInputs in;
    in.session = true;
    in.period_ns = kPeriodNs;
    CHECK(P::DecideLock(in) == P::Off::None);
    {
        auto x = in;
        x.switch_on = false;
        CHECK(P::DecideLock(x) == P::Off::Switch);
    }
    {
        auto x = in;
        x.file_off = true;
        CHECK(P::DecideLock(x) == P::Off::File);
    }
    {
        auto x = in;
        x.session = false;
        CHECK(P::DecideLock(x) == P::Off::NoSession);
    }
    {
        auto x = in;
        x.period_ns = 11'111'111; // 90 Hz
        CHECK(P::DecideLock(x) == P::Off::Period);
        x.period_ns = 0;
        CHECK(P::DecideLock(x) == P::Off::Period);
    }
    {
        auto x = in;
        x.frame_limit = 200;
        CHECK(P::DecideLock(x) == P::Off::FrameLimit);
        x.frame_limit = 0; // unlimited
        CHECK(P::DecideLock(x) == P::Off::FrameLimit);
    }
    {
        auto x = in;
        x.frame_advance = true;
        CHECK(P::DecideLock(x) == P::Off::FrameAdvance);
    }
    {
        auto x = in;
        x.slow = true;
        CHECK(P::DecideLock(x) == P::Off::Slow);
    }
    for (int i = 0; i <= static_cast<int>(P::Off::Slow); i++) {
        CHECK(std::string(P::OffText(static_cast<P::Off>(i))) != "?");
    }
    CHECK(P::EvenCadencePeriod(kPeriodNs));
    CHECK(P::EvenCadencePeriod(8'340'000));
    CHECK_FALSE(P::EvenCadencePeriod(11'111'111));
    CHECK_FALSE(P::EvenCadencePeriod(6'944'444)); // 144 Hz
}

TEST_CASE("XrPacer: below full speed after 30 frames without a sleep", "[xr_pacer]") {
    P::SlowDetector d;
    for (int i = 0; i < 29; i++) {
        d.Note(false);
    }
    CHECK_FALSE(d.Slow());
    d.Note(true); // one sleep resets the count
    for (int i = 0; i < 29; i++) {
        d.Note(false);
    }
    CHECK_FALSE(d.Slow());
    d.Note(false);
    CHECK(d.Slow());
    for (int i = 0; i < 59; i++) {
        d.Note(true);
    }
    CHECK(d.Slow());
    d.Note(true);
    CHECK_FALSE(d.Slow());
}

TEST_CASE("XrPacer: the lock's speed and its bounded correction", "[xr_pacer]") {
    CHECK(P::LockSpeed(kPeriodNs) == Approx(1.0028209).epsilon(1e-6));
    CHECK(P::kEmuFrameUs / P::LockSpeed(kPeriodNs) == Approx(2 * kPeriodUs).epsilon(1e-6));
    CHECK(P::LockSpeed(0) == 1.0);
    CHECK(P::CorrectionUs(0) == 0);
    CHECK(P::CorrectionUs(1000) == -50);  // late in the window: sleep less
    CHECK(P::CorrectionUs(-1000) == 50);  // early: sleep more
    CHECK(P::CorrectionUs(4166) == -208);
    CHECK(P::CorrectionUs(1e6) == -P::kMaxCorrectionUs);
    CHECK(P::CorrectionUs(-1e6) == P::kMaxCorrectionUs);
    CHECK(P::CorrectionUs(1e6, 0.05, 300) == -300);
}

TEST_CASE("XrPacer: the limiter lock turns on and off with the session", "[xr_pacer]") {
    P::Shared s;
    P::LimiterLock lock;
    const int64_t now = 50'000'000'000;
    auto step = lock.Frame(s, false, 100, now);
    CHECK(step.off == P::Off::NoSession);
    CHECK(step.speed == 1.0);
    CHECK(s.audio_ratio.load() == 1.0);

    s.session = true;
    s.period_ns = kPeriodNs;
    s.latch_ns = now - 5'000'000;
    step = lock.Frame(s, false, 100, now);
    CHECK(step.off == P::Off::None);
    CHECK(step.speed == Approx(1.0028209).epsilon(1e-6));
    CHECK(s.audio_ratio.load() == step.speed);
    CHECK(step.correction_us == 0); // no phase sample yet

    s.phase_error_ns = 2'000'000; // 2 ms late in the window
    s.phase_seq++;
    step = lock.Frame(s, false, 100, now);
    CHECK(step.correction_us == -100);
    CHECK(s.phase_mean_ns.load() == 1'900'000); // the correction is counted in at once
    step = lock.Frame(s, false, 100, now);
    CHECK(step.correction_us == -95); // the estimate keeps moving with the corrections
    CHECK(s.lock_frames.load() == 3);
    CHECK(s.correction_max_us.load() == 100);
    CHECK(s.correction_sum_us.load() == -195);

    step = lock.Frame(s, false, 100, now + 200'000'000); // no latch for 200 ms: the session ended
    CHECK(step.off == P::Off::NoSession);
    CHECK(s.audio_ratio.load() == 1.0);

    step = lock.Frame(s, true, 100, now);
    CHECK(step.off == P::Off::FrameAdvance);
    step = lock.Frame(s, false, 50, now);
    CHECK(step.off == P::Off::FrameLimit);
    CHECK(static_cast<P::Off>(s.lock_off.load()) == P::Off::FrameLimit);

    for (int i = 0; i < 30; i++) {
        lock.NoteSleep(s, false);
    }
    CHECK(s.no_sleep_frames.load() == 30);
    step = lock.Frame(s, false, 100, now);
    CHECK(step.off == P::Off::Slow);
}

namespace {

struct LockResult {
    double mean_error_us = 0;
    double max_abs_error_us = 0;
    double max_error_us = -1e9; ///< the latest a frame came
    int crossings = 0; ///< releases in a different latch window than the cadence wants
    int holds = 0;     ///< frames the gate held
    int64_t max_correction_us = 0;
};

/// The closed loop: the limiter wakes every game frame (the 3DS frame / speed + its correction),
/// the frame is ready `work_us` later with jitter and goes up at the first latch after that (with
/// the gate: not before the one before was in two), and its phase reaches the lock `delay` frames
/// later (0: before the next limiter call, 1: after it).
LockResult SimulateLock(double start_us, double jitter_us, int frames, unsigned seed, int settle,
                        int delay, bool gated, bool off_parity = false) {
    std::mt19937 rng{seed};
    std::uniform_real_distribution<double> jitter{-jitter_us, jitter_us};
    P::Shared s;
    s.session = true;
    s.period_ns = kPeriodNs;
    s.phase_gated = gated;
    P::LimiterLock lock;
    const double work_us = 9000;
    double wake = start_us - work_us + 100 * kPeriodUs;
    std::vector<double> pending;
    LockResult r;
    double sum = 0;
    int n = 0;
    double last_up = -1;
    for (int i = 0; i < frames; i++) {
        if (static_cast<int>(pending.size()) > delay) {
            s.phase_error_ns = static_cast<int64_t>(pending.front() * 1000);
            s.phase_seq++;
            pending.erase(pending.begin());
        }
        s.latch_ns = 1'000'000'000;
        const auto step = lock.Frame(s, false, 100, 1'000'000'000);
        REQUIRE(step.off == P::Off::None);
        const double ready = wake + work_us + jitter(rng);
        double up = std::ceil(ready / kPeriodUs) * kPeriodUs; // the latch it goes up at
        bool held = false;
        uint32_t needed = 0;
        const double last_latch = std::floor(ready / kPeriodUs) * kPeriodUs;
        if (gated && last_up >= 0 && up < last_up + 2 * kPeriodUs - 1) {
            up = last_up + 2 * kPeriodUs; // the gate: not before the one before was in two
            held = true;
            needed = static_cast<uint32_t>(std::lround((last_up + kPeriodUs - last_latch) / kPeriodUs));
        }
        if (last_up >= 0) {
            const double e = P::SampleErrorUs(ready - last_latch, needed, kPeriodUs);
            pending.push_back(e);
            if (i >= settle) {
                sum += e;
                n++;
                r.max_abs_error_us = std::max(r.max_abs_error_us, std::abs(e));
                r.max_error_us = std::max(r.max_error_us, e);
                r.max_correction_us =
                    std::max<int64_t>(r.max_correction_us, std::abs(step.correction_us));
                if (std::abs(up - last_up - 2 * kPeriodUs) > 1) {
                    r.crossings++; // the one before on view for 1 or 3 frames
                }
                r.holds += held;
            }
        }
        // off_parity: the first frame went up a refresh late (a hold before the lock started),
        // so the gate holds every frame after it until the lock moves the game later.
        last_up = up + (off_parity && i == 0 ? kPeriodUs : 0);
        wake += P::kEmuFrameUs / step.speed + static_cast<double>(step.correction_us);
    }
    r.mean_error_us = n ? sum / n : 0;
    return r;
}

} // namespace

TEST_CASE("XrPacer: the lock holds releases in the middle of the window", "[xr_pacer]") {
    // From anywhere (on a latch is the worst place), with +-3 ms of jitter (the owner's log) and
    // the phase reaching the lock before or after its next call: no picture on view for 1 or 3
    // frames once settled (5 s), and the corrections stay within their bound.
    for (const bool gated : {true, false}) {
        for (const double start : {50.0, kPeriodUs / 2, kPeriodUs - 50, 2000.0, 1.3 * kPeriodUs}) {
            for (int delay = 0; delay <= 1; delay++) {
                for (unsigned seed = 1; seed <= 3; seed++) {
                    const LockResult r =
                        SimulateLock(start, 3000, 60 * 120, seed, 60 * 5, delay, gated);
                    INFO("gated " << gated << " start " << start << " delay " << delay << " seed "
                                  << seed << ": mean " << r.mean_error_us << " us, max "
                                  << r.max_abs_error_us << " us, crossings " << r.crossings);
                    const double target = gated ? P::kGatedTarget * kPeriodUs : 0.0;
                    CHECK(std::abs(r.mean_error_us - target) < 150);
                    CHECK(r.max_error_us < kPeriodUs / 2 - 300); // never near late
                    CHECK(r.crossings == 0);
                    if (!gated) {
                        CHECK(r.max_abs_error_us < kPeriodUs / 2 - 300);
                    }
                    CHECK(r.max_correction_us <= P::kMaxCorrectionUs);
                }
            }
        }
    }
    // Held on every frame (the cadence the gate keeps is a refresh behind the game): the lock
    // moves the game later until it is at its target.
    for (unsigned seed = 1; seed <= 3; seed++) {
        for (const double start : {kPeriodUs * 0.2, kPeriodUs * 0.5, kPeriodUs * 0.8}) {
            const LockResult r = SimulateLock(start, 1000, 60 * 30, seed, 60 * 5, 1, true, true);
            INFO("off parity, start " << start << " seed " << seed << ": holds " << r.holds
                                      << ", crossings " << r.crossings << ", mean "
                                      << r.mean_error_us);
            // Aimed just after the window opens, the early half of the jitter is still held a
            // little (harmless: those go up at their latch all the same).
            CHECK(r.holds < 60 * 25 / 4);
            CHECK(r.crossings == 0);
            CHECK(std::abs(r.mean_error_us - P::kGatedTarget * kPeriodUs) < 600);
        }
    }
    // Without jitter it sits in the middle.
    const LockResult still = SimulateLock(100, 0, 60 * 20, 1, 60 * 10, 1, false);
    CHECK(still.max_abs_error_us < 100);
    const LockResult still_gated = SimulateLock(100, 0, 60 * 20, 1, 60 * 10, 1, true);
    CHECK(std::abs(still_gated.mean_error_us - P::kGatedTarget * kPeriodUs) < 100);
    CHECK(still_gated.holds == 0);
}

// AzaharFrame pacing2: games at 30 pictures a second (every other release the same picture again).

TEST_CASE("XrPacer: the gate counts game pictures; the same picture again goes at once",
          "[xr_pacer]") {
    P::Gate g;
    CHECK(g.PictureNeeded(0, true, 1) == 0); // nothing released yet
    g.Latch(1, 1);
    CHECK(g.PictureNeeded(1, true, 1) == 1); // on view once
    CHECK(g.PictureNeeded(1, false, 1) == 0); // a repeat of it never waits
    CHECK(g.PictureNeeded(1, true, 2) == 3);  // at 30 a second it wants 4
    g.Latch(2, 1); // the repeat went up: the same picture, still counting
    CHECK(g.PictureNeeded(1, true, 1) == 0);
    CHECK(g.PictureNeeded(1, true, 2) == 2);
    CHECK(g.PictureNeeded(2, true, 2) == 4); // not yet on view: all four
    g.Latch(2, 1);
    g.Latch(2, 1);
    CHECK(g.PictureNeeded(1, true, 2) == 0);
    g.Latch(3, 2);
    g.Latch(3, 2);
    g.Latch(0, 0); // nothing shown: ends 2's run
    uint32_t pics[6], rel[4];
    g.TakePictureRuns(pics);
    g.TakeRuns(rel);
    CHECK(pics[3] == 1); // picture 1: four frames over two releases
    CHECK(pics[1] == 1); // picture 2: two
    CHECK(rel[0] == 1);  // release 1 on view once, release 2 three times, release 3 twice
    CHECK(rel[1] == 1);
    CHECK(rel[2] == 1);
    // Each release its own picture (the one-argument Latch): the same as the release counts.
    P::Gate h;
    h.Latch(1);
    h.Latch(1);
    CHECK(h.PictureNeeded(1, true, 1) == 0);
    CHECK(h.Open(1));
}

TEST_CASE("XrPacer: the picture cadence from the game's swaps", "[xr_pacer]") {
    const auto feed = [](P::PictureCadence& c, int every, int releases) {
        for (int i = 0; i < releases; i++) {
            c.Note(i % every == 0);
        }
    };
    P::PictureCadence c60, c30, c20, none;
    feed(c60, 1, 40);
    feed(c30, 2, 40);
    feed(c20, 3, 40);
    for (int i = 0; i < 40; i++) {
        none.Note(false);
    }
    CHECK(c60.Cadence() == 1);
    CHECK(c30.Cadence() == 2);
    CHECK(c20.Cadence() == 3);
    CHECK(c30.Swapping());
    CHECK_FALSE(none.Swapping()); // never swaps: every release is a new picture
    CHECK(none.Cadence() == 1);
    // One picture that took three VBlanks (a slow game frame) keeps 30; a run of 60s ends it.
    c30.Note(true);
    c30.Note(false);
    c30.Note(false);
    feed(c30, 2, 6);
    CHECK(c30.Cadence() == 2);
    feed(c30, 1, 6);
    CHECK(c30.Cadence() == 1);
    feed(c30, 2, 16);
    CHECK(c30.Cadence() == 2);
    // A still screen (no swap for kNoSwap releases) counts every release as new again.
    for (uint32_t i = 0; i < P::PictureCadence::kNoSwap; i++) {
        c30.Note(false);
    }
    CHECK_FALSE(c30.Swapping());
    CHECK(c30.Cadence() == 1);
}

namespace {

struct PictureResult {
    uint32_t pictures[6]{}; ///< game pictures on view 1 .. 6+ frames
    uint32_t releases[4]{}; ///< releases on view 1 .. 4+ frames
    uint32_t waits = 0, timeouts = 0;
};

/// A game at 30 pictures a second, run at the lock's rate: a new picture every four display
/// periods (two 60.000 Hz frames), ready `new_us` into its
/// pair with jitter (and every `late_every`-th one `late_us` -3..+5 ms later still: a 4K texture
/// pack's slow frame), the same picture again `repeat_gap_us` after its usual time. Latches every period. `by_picture`:
/// the pacing2 gate (a repeat goes at once, a new picture waits for 2 x cadence frames of the one
/// on view, PictureCadence from the swaps); else the release gate it replaces (each release waits
/// for two frames of the one before, at most P + 2 ms). One release thread: none overtakes.
PictureResult SimulatePictures(bool by_picture, double new_us, double repeat_gap_us,
                               double jitter_us, int pairs, unsigned seed, int late_every = 0,
                               double late_us = 0) {
    std::mt19937 rng{seed};
    std::uniform_real_distribution<double> jitter{-jitter_us, jitter_us};
    std::uniform_real_distribution<double> late{-3000, 5000};
    P::Gate gate;
    P::PictureCadence cadence;
    PictureResult out;
    double next_latch = kPeriodUs;
    double last_release = 0;
    uint64_t released = 0, released_picture = 0;
    const auto latch_until = [&](double t) {
        while (next_latch <= t) {
            gate.Latch(released, released_picture);
            next_latch += kPeriodUs;
        }
    };
    uint64_t picture = 0;
    for (int k = 0; k < pairs; k++) {
        const double on_time = 4 * k * kPeriodUs + new_us + jitter(rng);
        double new_ready = on_time;
        if (late_every && k % late_every == late_every - 1) {
            new_ready += late_us + late(rng); // the repeat after it comes on time
        }
        for (int r = 0; r < 2; r++) {
            const bool is_new = r == 0;
            cadence.Note(is_new);
            if (is_new) {
                picture++;
            }
            double ready = std::max(last_release, is_new ? new_ready : on_time + repeat_gap_us);
            latch_until(ready);
            const uint32_t c = cadence.Cadence();
            const auto open = [&] {
                return by_picture ? gate.PictureNeeded(released_picture, is_new, c) == 0
                                  : gate.Open(released);
            };
            if (!open()) {
                out.waits++;
                const uint32_t needed = gate.PictureNeeded(released_picture, is_new, c);
                const double hold =
                    by_picture && c > 1 ? needed * kPeriodUs + 2000 : kPeriodUs + 2000;
                const double deadline = ready + hold;
                while (!open() && next_latch <= deadline) {
                    latch_until(next_latch);
                }
                if (!open()) {
                    out.timeouts++;
                    latch_until(deadline);
                    ready = deadline;
                } else {
                    ready = next_latch - kPeriodUs + 1; // just after the latch that opened it
                }
            }
            released++;
            released_picture = picture;
            last_release = ready;
        }
    }
    latch_until(last_release + 6 * kPeriodUs);
    gate.TakePictureRuns(out.pictures);
    gate.TakeRuns(out.releases);
    return out;
}

} // namespace

TEST_CASE("XrPacer: a 30-a-second game's repeat right after its picture never times out",
          "[xr_pacer]") {
    // Kid Icarus Uprising in the owner's log: the same picture again 0 to 1.5 ms after the new
    // one, both 3 to 6 ms before a latch. The release gate held the repeat for two frames of
    // the new picture, timed out at P + 2 ms, and left releases on view 1/3/1/3 (29/2/29 a
    // second, timeouts 29); the picture gate lets the repeat go and keeps each picture four.
    for (unsigned seed = 1; seed <= 3; seed++) {
        const double new_us = 2 * kPeriodUs - 4500; // 4.5 ms before a latch
        const PictureResult before = SimulatePictures(false, new_us, 800, 600, 60 * 30, seed);
        const PictureResult after = SimulatePictures(true, new_us, 800, 600, 60 * 30, seed);
        INFO("seed " << seed << ": before releases 1/2/3/4+ " << before.releases[0] << "/"
                     << before.releases[1] << "/" << before.releases[2] << "/"
                     << before.releases[3] << " timeouts " << before.timeouts
                     << "; after pictures 1..6+ " << after.pictures[0] << "/"
                     << after.pictures[1] << "/" << after.pictures[2] << "/"
                     << after.pictures[3] << "/" << after.pictures[4] << "/"
                     << after.pictures[5] << " timeouts " << after.timeouts);
        CHECK(before.timeouts > 60 * 25); // nearly every pair
        CHECK(before.releases[0] > 60 * 25);
        CHECK(after.timeouts == 0);
        CHECK(after.pictures[3] >= 60 * 30 - 2); // every picture on view four frames
        CHECK(after.pictures[0] + after.pictures[1] + after.pictures[2] == 0);
    }
}

TEST_CASE("XrPacer: a 30-a-second game's slow pictures keep four frames each", "[xr_pacer]") {
    // Majora's Mask 3D with a 4K pack: the new picture usually just after a latch, the same
    // picture again a 3DS frame later, and every 7th new picture 7.5 to 15.5 ms late (GPU
    // behind up to 11 ms, texture loads). The release gate protects the repeat and lets the late
    // picture end up on view for 3 (or less); the picture gate keeps every picture at four or
    // more (the one before a late one five).
    for (unsigned seed = 1; seed <= 3; seed++) {
        const double new_us = 2 * kPeriodUs + 1500;
        const PictureResult before =
            SimulatePictures(false, new_us, 2 * kPeriodUs, 1500, 60 * 30, seed, 7, 10500);
        const PictureResult after =
            SimulatePictures(true, new_us, 2 * kPeriodUs, 1500, 60 * 30, seed, 7, 10500);
        const auto off = [](const PictureResult& r) {
            return r.pictures[0] + r.pictures[1] + r.pictures[2] + r.pictures[4] + r.pictures[5];
        };
        INFO("seed " << seed << ": pictures 1..6+ before " << before.pictures[0] << "/"
                     << before.pictures[1] << "/" << before.pictures[2] << "/"
                     << before.pictures[3] << "/" << before.pictures[4] << "/"
                     << before.pictures[5] << ", after " << after.pictures[0] << "/"
                     << after.pictures[1] << "/" << after.pictures[2] << "/"
                     << after.pictures[3] << "/" << after.pictures[4] << "/"
                     << after.pictures[5]);
        // None short once the cadence is known (the first eight pictures are taken as 60 a
        // second, and the 7th of them is a late one), and far fewer off four than before.
        CHECK(after.pictures[0] + after.pictures[1] + after.pictures[2] <= 1);
        CHECK(before.pictures[2] >= 8);
        CHECK(off(after) * 4 < off(before));
    }
}
