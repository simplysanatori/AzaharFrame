// AzaharFrame pacing + ffaudio
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// The DSP's output callback with both of its speed changes: the VR frame pacing lock's rate trim
// (only at 1x, audio_core/rate_trim.h) and Fast Forward Audio's Normal Pitch (only above 100%).

#include <cmath>
#include <functional>
#include <memory>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "audio_core/audio_types.h"
#include "audio_core/dsp_interface.h"
#include "audio_core/sink.h"
#include "common/settings.h"
#include "common/xr_pacer.h"
#include "core/core.h"

namespace {

using Mode = Settings::FastForwardAudio;

/// A sink the test calls: it keeps the DSP's output callback.
class TestSink final : public AudioCore::Sink {
public:
    unsigned int GetNativeSampleRate() const override {
        return AudioCore::native_sample_rate;
    }
    void SetCallback(std::function<void(s16*, std::size_t)> cb) override {
        callback = std::move(cb);
    }
    std::function<void(s16*, std::size_t)> callback;
};

/// The DSP's output half; the rest of the interface is not used.
class TestDsp final : public AudioCore::DspInterface {
public:
    explicit TestDsp(Core::System& system) : DspInterface(system) {}
    u16 RecvData(u32) override {
        return 0;
    }
    bool RecvDataIsReady(u32) const override {
        return false;
    }
    void SetSemaphore(u16) override {}
    std::vector<u8> PipeRead(AudioCore::DspPipe, std::size_t) override {
        return {};
    }
    std::size_t GetPipeReadableSize(AudioCore::DspPipe) const override {
        return 0;
    }
    void PipeWrite(AudioCore::DspPipe, std::span<const u8>) override {}
    void SetInterruptHandler(
        std::function<void(Service::DSP::InterruptType, AudioCore::DspPipe)>) override {}
    void LoadComponent(std::span<const u8>) override {}
    void UnloadComponent() override {}
    using DspInterface::OutputFrame;
};

constexpr std::size_t kChunk = 1024; // sink callback size
constexpr double kRate = AudioCore::native_sample_rate;
constexpr int kRamp = 30000; // the right channel counts 0..29999 and wraps

/// What one stretch of play gave: per callback, the right channel's steps (a ramp the game makes),
/// and per second the left channel's tone (440 Hz as the game makes it).
struct Played {
    std::vector<double> freq;    ///< Hz of each whole second of output
    std::size_t steps = 0;       ///< right-channel steps inside callbacks
    std::size_t step_one = 0;    ///< ...of exactly 1 (read as it is)
    std::size_t step_two = 0;    ///< ...of 2 (the trim reading 0.28 % fast, rounded)
    std::size_t repeats = 0;     ///< ...of 0 (an underrun holding the last frame)
    std::size_t step_other = 0;  ///< ...anything else (a cut, a stretched sound)
    std::size_t edge_jumps = 0;  ///< steps between callbacks that are not 1 or 2
};

/// The game makes `speed` x real time of sound in 160-frame DSP frames, the sink takes 1024-frame
/// callbacks. Returns what came out of callbacks from `skip_s` seconds on.
struct Game {
    TestDsp& dsp;
    TestSink& sink;
    double phase = 0;
    int ramp = 0;
    double owed = 480; // a starting backlog of three DSP frames, as the sink's start leaves
    s16 last_right = -1;

    Played Play(double speed, double seconds, double skip_s = 0.0) {
        Played p;
        std::vector<s16> out(kChunk * 2);
        const auto callbacks = static_cast<std::size_t>(seconds * kRate / kChunk);
        std::size_t out_frames = 0, crossings = 0;
        s16 prev_left = 0;
        for (std::size_t c = 0; c < callbacks; c++) {
            owed += speed * kChunk;
            while (owed >= AudioCore::samples_per_frame) {
                AudioCore::StereoFrame16 frame;
                for (auto& f : frame) {
                    f[0] = static_cast<s16>(12000.0 * std::sin(phase));
                    phase += 2.0 * M_PI * 440.0 / kRate;
                    f[1] = static_cast<s16>(ramp);
                    ramp = (ramp + 1) % kRamp;
                }
                dsp.OutputFrame(frame);
                owed -= AudioCore::samples_per_frame;
            }
            sink.callback(out.data(), kChunk);
            const bool counted = static_cast<double>(c * kChunk) >= skip_s * kRate;
            for (std::size_t i = 0; i < kChunk; i++) {
                const s16 r = out[2 * i + 1];
                // Steps next to the ramp's wrap are left out: read between frames (the trim
                // interpolates), 29999 then 0 gives any value in between.
                const bool near_wrap = r < 100 || r > kRamp - 100 || last_right < 100 ||
                                       last_right > kRamp - 100;
                if (counted && !near_wrap && (i > 0 || last_right >= 0)) {
                    const int step = ((r - last_right) % kRamp + kRamp) % kRamp;
                    if (i == 0) {
                        p.edge_jumps += (step == 1 || step == 2) ? 0 : 1;
                    } else {
                        p.steps++;
                        p.step_one += step == 1;
                        p.step_two += step == 2;
                        p.repeats += step == 0;
                        p.step_other += (step > 2);
                    }
                }
                last_right = r;
                if (counted) {
                    const s16 l = out[2 * i];
                    crossings += (prev_left < 0 && l >= 0) ? 1 : 0;
                    prev_left = l;
                }
            }
            if (counted) {
                out_frames += kChunk;
                if (out_frames >= kRate) {
                    p.freq.push_back(static_cast<double>(crossings) * kRate /
                                     static_cast<double>(out_frames));
                    out_frames = crossings = 0;
                }
            }
        }
        return p;
    }
};

/// Puts the settings and the pacer back as they were.
struct Restore {
    double limit = Settings::values.frame_limit.GetValue();
    Mode mode = Settings::values.fast_forward_audio.GetValue();
    ~Restore() {
        Settings::values.frame_limit.SetValue(limit);
        Settings::values.fast_forward_audio.SetValue(mode);
        Common::XrPacer::Get().audio_ratio.store(1.0);
    }
};

} // namespace

TEST_CASE("DSP output: the plain path's read speed", "[audio_core][pacing][ffaudio]") {
    CHECK(AudioCore::PlainReadRatio(1.0028, 100.0) == 1.0028); // locked at 1x: the trim
    CHECK(AudioCore::PlainReadRatio(1.0, 100.0) == 1.0);       // not locked
    for (double limit : {300.0, 0.0, 150.0, 50.0}) {           // fast forward, unlimited, slowed
        CHECK(AudioCore::PlainReadRatio(1.0028, limit) == 1.0);
    }
}

TEST_CASE("DSP output: 1x locked, fast forward with Normal Pitch, back to 1x",
          "[audio_core][pacing][ffaudio]") {
    Restore restore;
    Core::System system;
    TestDsp dsp(system);
    auto sink_owner = std::make_unique<TestSink>();
    TestSink& sink = *sink_owner;
    dsp.SetSink(std::move(sink_owner));
    REQUIRE(sink.callback);
    auto& pace = Common::XrPacer::Get();
    const double lock = Common::XrPacer::LockSpeed(8'333'333); // 60.000 Hz at 120 Hz
    REQUIRE(lock > 1.002);
    REQUIRE(lock < 1.004);
    Settings::values.fast_forward_audio.SetValue(Mode::NormalPitch);
    Settings::values.frame_limit.SetValue(100.0);
    pace.audio_ratio.store(lock);
    Game game{dsp, sink};

    // 1x, locked: the game runs `lock` fast and the backlog is read as fast, unstretched: every
    // step of the ramp is 1 or (0.28 % of the time) 2, nothing is cut and the backlog holds.
    const u32 drops0 = pace.audio_drops.load();
    const Played locked = game.Play(lock, 6.0, 1.0);
    CHECK(locked.step_other == 0);
    CHECK(locked.repeats == 0);
    CHECK(locked.edge_jumps == 0);
    CHECK(locked.step_two > 0);
    const double fast = static_cast<double>(locked.step_two) / static_cast<double>(locked.steps);
    CHECK(fast > 0.5 * (lock - 1.0));
    CHECK(fast < 2.0 * (lock - 1.0));
    CHECK(pace.audio_drops.load() == drops0); // no backlog was dropped

    // Fast forward at 300% with Normal Pitch: stretched, so the tone keeps its pitch (Sped Up
    // would be 1320 Hz). The pacer's ratio is still set here (the limiter resets it a frame later):
    // the trim must not join in.
    Settings::values.frame_limit.SetValue(300.0);
    const Played ff = game.Play(3.0, 6.0);
    REQUIRE(ff.freq.size() >= 5);
    for (std::size_t i = 2; i < ff.freq.size(); i++) {
        CHECK(ff.freq[i] > 440.0 * 0.95);
        CHECK(ff.freq[i] < 440.0 * 1.05);
    }
    pace.audio_ratio.store(1.0); // the limiter's next frame: lock off

    // Back to 1x and locked again: after the stretcher settles (at most a second) the output is
    // the plain path with the trim once more.
    Settings::values.frame_limit.SetValue(100.0);
    pace.audio_ratio.store(lock);
    const u32 drops1 = pace.audio_drops.load();
    const Played back = game.Play(lock, 6.0, 2.0);
    CHECK(back.step_other == 0);
    CHECK(back.repeats == 0);
    CHECK(back.edge_jumps == 0);
    CHECK(back.step_two > 0);
    for (double f : back.freq) {
        CHECK(f > 440.0 * 0.97);
        CHECK(f < 440.0 * 1.03);
    }
    // A backlog left from the fast forward may be dropped once while settling, not after.
    CHECK(pace.audio_drops.load() - drops1 <= 1);
}

TEST_CASE("DSP output: fast forward with Sped Up plays the backlog as it is",
          "[audio_core][pacing][ffaudio]") {
    Restore restore;
    Core::System system;
    TestDsp dsp(system);
    auto sink_owner = std::make_unique<TestSink>();
    TestSink& sink = *sink_owner;
    dsp.SetSink(std::move(sink_owner));
    Settings::values.fast_forward_audio.SetValue(Mode::SpedUp);
    Settings::values.frame_limit.SetValue(200.0);
    // A stale lock ratio (the frame before the limiter turns the lock off) changes nothing.
    Common::XrPacer::Get().audio_ratio.store(Common::XrPacer::LockSpeed(8'333'333));
    Game game{dsp, sink};
    // A game that cannot reach the fast forward speed (1x of sound at a 200% limit), so the
    // backlog does not overflow and every step shows: read 1:1, never trimmed.
    const Played sped = game.Play(1.0, 3.0, 0.5);
    CHECK(sped.steps > 0);
    CHECK(sped.step_one == sped.steps);
    CHECK(sped.edge_jumps == 0);
}

TEST_CASE("DSP output: 1x unlocked is the plain path", "[audio_core][pacing][ffaudio]") {
    Restore restore;
    Core::System system;
    TestDsp dsp(system);
    auto sink_owner = std::make_unique<TestSink>();
    TestSink& sink = *sink_owner;
    dsp.SetSink(std::move(sink_owner));
    Settings::values.fast_forward_audio.SetValue(Mode::NormalPitch);
    Settings::values.frame_limit.SetValue(100.0);
    Common::XrPacer::Get().audio_ratio.store(1.0);
    Game game{dsp, sink};
    const Played plain = game.Play(1.0, 3.0, 0.5);
    CHECK(plain.step_one == plain.steps);
    CHECK(plain.edge_jumps == 0);
}
