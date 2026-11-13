// AzaharFrame ffaudio
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cmath>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "audio_core/audio_types.h"
#include "audio_core/dsp_interface.h"
#include "audio_core/ff_skip.h"
#include "audio_core/time_stretch.h"

using AudioCore::PlanFastForwardAudio;
using Mode = Settings::FastForwardAudio;

TEST_CASE("Fast forward audio: which mode does what", "[audio_core][ffaudio]") {
    // Normal speed: nothing changes in any mode
    for (Mode m : {Mode::SpedUp, Mode::NormalPitch, Mode::NormalSpeed, Mode::Muted}) {
        const auto p = PlanFastForwardAudio(m, 100.0, false);
        CHECK(!p.stretch);
        CHECK(!p.mute);
        CHECK(!p.skip);
    }
    CHECK(PlanFastForwardAudio(Mode::NormalPitch, 100.0, true).stretch); // the old switch stays
    CHECK(!PlanFastForwardAudio(Mode::NormalPitch, 100.0, true).low_latency);
    // Fast forward (above 100%, or no limit)
    for (double limit : {150.0, 200.0, 1000.0, 0.0}) {
        const auto sped = PlanFastForwardAudio(Mode::SpedUp, limit, false);
        CHECK(!sped.stretch);
        CHECK(!sped.mute);
        const auto normal = PlanFastForwardAudio(Mode::NormalPitch, limit, false);
        CHECK(normal.stretch);
        CHECK(normal.low_latency);
        CHECK(!normal.mute);
        const auto muted = PlanFastForwardAudio(Mode::Muted, limit, false);
        CHECK(muted.mute);
        CHECK(!muted.stretch);
        const auto speed = PlanFastForwardAudio(Mode::NormalSpeed, limit, true); // ffspeed
        CHECK(speed.skip);
        CHECK(!speed.stretch); // never both
        CHECK(!speed.mute);
        CHECK(!sped.skip);
        CHECK(!normal.skip);
    }
    // Below 100% (a slowed game) is not fast forwarding
    CHECK(!AudioCore::IsFastForwarding(50.0));
    CHECK(!PlanFastForwardAudio(Mode::Muted, 50.0, false).mute);
}

namespace {

struct Run {
    std::vector<double> freq; ///< Hz of each full second of output
    std::size_t max_pending = 0;
    std::size_t underruns = 0;
    double end_ratio = 0;
};

/// Feeds a 440 Hz tone that the game makes `speed` times as fast as real time, in sink callbacks
/// of 1024 frames, and measures the tone that comes out.
Run Feed(AudioCore::TimeStretcher& st, double speed, double seconds, double& phase) {
    constexpr std::size_t chunk = 1024;
    const double rate = AudioCore::native_sample_rate;
    Run run;
    std::vector<s16> out(chunk * 2);
    std::vector<s16> in;
    std::size_t out_frames = 0, crossings = 0;
    s16 prev = 0;
    double carry = 0;
    const std::size_t callbacks = static_cast<std::size_t>(seconds * rate / chunk);
    for (std::size_t c = 0; c < callbacks; c++) {
        carry += speed * chunk;
        const std::size_t n = static_cast<std::size_t>(carry);
        carry -= static_cast<double>(n);
        in.resize(n * 2);
        for (std::size_t i = 0; i < n; i++) {
            const auto v = static_cast<s16>(12000.0 * std::sin(phase));
            phase += 2.0 * M_PI * 440.0 / rate;
            in[2 * i] = in[2 * i + 1] = v;
        }
        const std::size_t got = st.Process(in.data(), n, out.data(), chunk);
        run.underruns += got < chunk ? 1 : 0;
        if (c * chunk >= rate) { // the first second is the tempo finding its way
            run.max_pending = std::max(run.max_pending, st.PendingFrames());
        }
        for (std::size_t i = 0; i < got; i++) {
            const s16 v = out[2 * i];
            crossings += (prev < 0 && v >= 0) ? 1 : 0;
            prev = v;
        }
        out_frames += got;
        if (out_frames >= rate) {
            run.freq.push_back(static_cast<double>(crossings) * rate / static_cast<double>(out_frames));
            out_frames = crossings = 0;
        }
    }
    run.end_ratio = st.GetStretchRatio();
    return run;
}

} // namespace

TEST_CASE("Fast forward audio: Normal Pitch keeps the pitch and a short queue", "[audio_core][ffaudio]") {
    for (double speed : {2.0, 4.0}) {
        AudioCore::TimeStretcher st;
        st.SetLowLatency(true);
        double phase = 0;
        const Run ff = Feed(st, speed, 8.0, phase);
        REQUIRE(ff.freq.size() >= 6);
        for (std::size_t i = 3; i < ff.freq.size(); i++) { // after the first seconds
            CHECK(ff.freq[i] > 440.0 * 0.95);
            CHECK(ff.freq[i] < 440.0 * 1.05);
        }
        CHECK(ff.max_pending < AudioCore::native_sample_rate / 8); // under 125 ms of waiting sound
        CHECK(std::abs(ff.end_ratio - speed) < 0.1 * speed);
        // Back to normal speed: the tempo returns to 1 within a second
        st.SetDraining(true); // as the DSP does while it waits to switch back
        const Run back = Feed(st, 1.0, 2.0, phase);
        CHECK(std::abs(back.end_ratio - 1.0) < 0.06);
        CHECK(back.max_pending < AudioCore::native_sample_rate / 8);
        CHECK(st.BacklogFullness() < 0.5);
    }
}

// AzaharFrame ffspeed: Normal Speed
TEST_CASE("Fast forward audio: the choices keep their stored values and menu order",
          "[audio_core][ffspeed]") {
    using Settings::FastForwardAudioFromIndex;
    using Settings::FastForwardAudioIndex;
    CHECK(static_cast<u32>(Mode::Muted) == 2); // configs written before Normal Speed
    CHECK(FastForwardAudioFromIndex(0) == Mode::SpedUp);
    CHECK(FastForwardAudioFromIndex(1) == Mode::NormalPitch);
    CHECK(FastForwardAudioFromIndex(2) == Mode::NormalSpeed);
    CHECK(FastForwardAudioFromIndex(3) == Mode::Muted);
    for (u32 i = 0; i < 4; i++) {
        CHECK(FastForwardAudioIndex(FastForwardAudioFromIndex(i)) == i);
    }
    CHECK(FastForwardAudioFromIndex(9) == Mode::NormalPitch);
}

namespace {

constexpr double kTone = 440.0;
constexpr double kAmp = 12000.0;
constexpr double kNaturalStep = kAmp * 2.0 * M_PI * kTone / AudioCore::native_sample_rate;

struct SkipRun {
    std::vector<s16> out;            ///< left channel (the tone) of everything played
    std::vector<s16> counter;        ///< right channel: the game's frame counter, wrapped at 2^15
    std::size_t max_queue = 0;       ///< frames waiting after a block, from 1 s on
    std::size_t underruns = 0;       ///< blocks that came up short, from 1 s on
    double max_lag_ms = 0;           ///< the game's newest sound minus what plays, from 1 s on
    double min_lag_ms = 1e9;
};

/// A game that makes `speed` x real time of a 440 Hz tone (left) and a frame counter (right).
struct SkipGame {
    AudioCore::FastForwardSkipper skip;
    double speed;
    double carry = 0;
    std::size_t made = 0, blocks = 0;
    bool coarse = false; ///< the counter is made / 64: no wrap in 2 M frames, for the lag
    explicit SkipGame(double speed_, bool coarse_ = false, AudioCore::SkipTuning t = {})
        : skip(AudioCore::native_sample_rate, t), speed(speed_), coarse(coarse_) {}

    SkipRun Run(double seconds, std::size_t chunk = 1024, bool feed = true) {
        SkipRun r;
        std::vector<s16> in, out(chunk * 2);
        const auto n_blocks = static_cast<std::size_t>(seconds * AudioCore::native_sample_rate / chunk);
        for (std::size_t b = 0; b < n_blocks; b++, blocks++) {
            if (feed) {
                carry += speed * static_cast<double>(chunk);
                const auto n = static_cast<std::size_t>(carry);
                carry -= static_cast<double>(n);
                in.resize(2 * n);
                for (std::size_t i = 0; i < n; i++, made++) {
                    in[2 * i] = static_cast<s16>(
                        kAmp * std::sin(2.0 * M_PI * kTone * static_cast<double>(made) /
                                        AudioCore::native_sample_rate));
                    in[2 * i + 1] = static_cast<s16>(coarse ? made >> 6 : made % 32768);
                }
                skip.Feed(in.data(), n);
            }
            const std::size_t got = skip.Render(out.data(), chunk);
            const bool settled = blocks * chunk >= AudioCore::native_sample_rate;
            if (settled) {
                r.underruns += got < chunk ? 1 : 0;
                r.max_queue = std::max(r.max_queue, skip.Queued());
                if (feed && got > 0) {
                    // lag at the block's last frame: the game's newest sound minus the played one
                    // (the right channel counts the game's frames in 64s)
                    const double ms = 1000.0 *
                                      (static_cast<double>(made - 1) -
                                       64.0 * static_cast<double>(out[2 * (got - 1) + 1])) /
                                      AudioCore::native_sample_rate;
                    r.max_lag_ms = std::max(r.max_lag_ms, ms);
                    r.min_lag_ms = std::min(r.min_lag_ms, ms);
                }
            }
            for (std::size_t i = 0; i < got; i++) {
                r.out.push_back(out[2 * i]);
                r.counter.push_back(out[2 * i + 1]);
            }
        }
        return r;
    }
};

double MaxStep(const std::vector<s16>& v, std::size_t from = 0) {
    double m = 0;
    for (std::size_t i = std::max<std::size_t>(from, 1); i < v.size(); i++) {
        m = std::max(m, std::abs(static_cast<double>(v[i]) - v[i - 1]));
    }
    return m;
}

double ToneHz(const std::vector<s16>& v, std::size_t from, std::size_t to) {
    std::size_t crossings = 0;
    for (std::size_t i = from + 1; i < to; i++) {
        crossings += (v[i - 1] < 0 && v[i] >= 0) ? 1 : 0;
    }
    return static_cast<double>(crossings) * AudioCore::native_sample_rate /
           static_cast<double>(to - from);
}

/// Share of output steps of the frame counter that are exactly 1: the tempo is 1.0 there.
double RealTimeShare(const std::vector<s16>& counter, std::size_t from) {
    std::size_t one = 0, all = 0;
    for (std::size_t i = from + 1; i < counter.size(); i++) {
        const int step = ((counter[i] - counter[i - 1]) % 32768 + 32768) % 32768;
        all++;
        one += step == 1 ? 1 : 0;
    }
    return all ? static_cast<double>(one) / static_cast<double>(all) : 0.0;
}

} // namespace

TEST_CASE("Normal Speed: tempo 1.0 and pitch 1.0 at 2x, 3x and 4x", "[audio_core][ffspeed]") {
    for (double speed : {2.0, 3.0, 4.0}) {
        SkipGame game(speed);
        const SkipRun r = game.Run(8.0);
        const std::size_t from = AudioCore::native_sample_rate; // after the first second
        REQUIRE(r.out.size() > 6 * AudioCore::native_sample_rate);
        // tempo: the counter advances one per output frame in at least 80 % of them (the rest
        // are the 15 ms crossfades), so the sound plays at exactly real time
        CHECK(RealTimeShare(r.counter, from) > 0.80);
        // pitch: the tone is as the game made it (Sped Up would be 440 * speed)
        const double hz = ToneHz(r.out, from, r.out.size());
        CHECK(hz > kTone * 0.985);
        CHECK(hz < kTone * 1.015);
        // about 1/speed of the sound is heard
        CHECK(game.skip.Jumps() > 20);
        CHECK(r.underruns == 0);
    }
}

TEST_CASE("Normal Speed: the queue and the lag stay bounded, the sound is never ahead",
          "[audio_core][ffspeed]") {
    for (double speed : {1.5, 2.0, 3.0, 4.0, 8.0}) {
        for (std::size_t chunk : {512u, 1024u, 2048u}) {
            SkipGame game(speed, true);
            const SkipRun r = game.Run(6.0, chunk);
            CAPTURE(speed, chunk, r.max_lag_ms, r.min_lag_ms);
            // 300 ms at the speeds of the menu (to 4x); at 8x the shortest stretch sets the lag
            const AudioCore::SkipTuning tune;
            const double budget = std::max(tune.lag_budget_ms, (speed - 1.0) * tune.segment_min_ms);
            // waiting sound: the segment's growth plus a block and the jump's search and guard
            const double queue_ms = 1000.0 * static_cast<double>(r.max_queue) / AudioCore::native_sample_rate;
            const double block_ms = 1000.0 * static_cast<double>(chunk) / AudioCore::native_sample_rate;
            CHECK(queue_ms < budget + 3.0 * block_ms + 60.0);
            CHECK(r.max_lag_ms < budget + 3.0 * block_ms + 120.0);
            CHECK(r.min_lag_ms >= -3.0); // 64 frames of resolution
            CHECK(r.underruns == 0);
        }
    }
}

TEST_CASE("Normal Speed: the jumps are smooth", "[audio_core][ffspeed]") {
    for (double speed : {2.0, 4.0}) {
        SkipGame game(speed);
        const SkipRun r = game.Run(8.0);
        REQUIRE(game.skip.Jumps() > 20);
        // no sample step over 1.5x what the tone itself does: no click at any seam
        CHECK(MaxStep(r.out, 1024) < 1.5 * kNaturalStep);
    }
    // ...also for noise-like sound the fade is the equal-power one: the level stays even
    AudioCore::FastForwardSkipper skip(AudioCore::native_sample_rate);
    std::vector<s16> in(2 * 4096), out(2 * 1024);
    unsigned seed = 1;
    double e_in = 0, e_out = 0;
    std::size_t n_in = 0, n_out = 0;
    for (int b = 0; b < 200; b++) {
        for (auto& v : in) {
            seed = seed * 1664525u + 1013904223u;
            v = static_cast<s16>(static_cast<int>(seed >> 16) % 8000 - 4000);
            e_in += static_cast<double>(v) * v;
            n_in++;
        }
        skip.Feed(in.data(), 4096);
        const auto got = skip.Render(out.data(), 1024);
        for (std::size_t i = 0; i < 2 * got; i++) {
            e_out += static_cast<double>(out[i]) * out[i];
            n_out++;
        }
    }
    CHECK(skip.Jumps() > 10);
    // the level stays as the sound's (within 0.5 dB): the fades do not dip or bump
    const double db = 10.0 * std::log10((e_out / static_cast<double>(n_out)) /
                                        (e_in / static_cast<double>(n_in)));
    CHECK(std::abs(db) < 0.5);
}

TEST_CASE("Normal Speed: at 1x nothing is skipped", "[audio_core][ffspeed]") {
    SkipGame game(1.0);
    const SkipRun r = game.Run(6.0);
    CHECK(game.skip.Jumps() == 0);
    CHECK(RealTimeShare(r.counter, 1024) > 0.999);
    CHECK(r.underruns == 0);
}

TEST_CASE("Normal Speed: the return to 1x is clean", "[audio_core][ffspeed]") {
    for (double speed : {2.0, 4.0}) {
        SkipGame game(speed);
        SkipRun r = game.Run(5.0);
        const std::size_t ff_end = r.out.size();
        game.skip.Finish();
        // the DSP stops feeding the skipper: what is left plays out, then the fifo (1x) follows.
        // Here the plain path is a 1x tone continuing from the game's newest sound.
        std::vector<s16> tail(2 * 1024);
        std::size_t played_after = 0;
        std::vector<s16> left;
        while (!game.skip.Idle() && played_after < AudioCore::native_sample_rate) {
            const auto got = game.skip.Render(tail.data(), 1024);
            for (std::size_t i = 0; i < got; i++) {
                left.push_back(tail[2 * i]);
            }
            played_after += got;
            if (got == 0) {
                break;
            }
        }
        CHECK(game.skip.Idle());
        CHECK(played_after < AudioCore::native_sample_rate / 5); // out within 200 ms
        CHECK(MaxStep(left) < 1.5 * kNaturalStep);
        // nothing waits for the plain path to carry as lag: it was the newest sound
        CHECK(game.skip.Queued() == 0);
        CHECK(ff_end > 0);
        // Reset and back to 1x: starts clean
        game.skip.Reset();
        CHECK(game.skip.Idle());
        CHECK(game.skip.Jumps() > 0);
    }
}
