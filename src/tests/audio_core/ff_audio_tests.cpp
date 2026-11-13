// AzaharFrame ffaudio
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <cmath>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "audio_core/audio_types.h"
#include "audio_core/dsp_interface.h"
#include "audio_core/time_stretch.h"

using AudioCore::PlanFastForwardAudio;
using Mode = Settings::FastForwardAudio;

TEST_CASE("Fast forward audio: which mode does what", "[audio_core][ffaudio]") {
    // Normal speed: nothing changes in any mode
    for (Mode m : {Mode::SpedUp, Mode::NormalPitch, Mode::Muted}) {
        const auto p = PlanFastForwardAudio(m, 100.0, false);
        CHECK(!p.stretch);
        CHECK(!p.mute);
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
