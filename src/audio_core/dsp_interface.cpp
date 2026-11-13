// Copyright 2017-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include "audio_core/dsp_interface.h"
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/xr_pacer.h"
#include "core/core.h"
#include "core/dumping/backend.h"

namespace AudioCore {

namespace {
/// Amount of CSND frames to wait until they are outputted on their own
/// because DSP is not producing any output.
constexpr u32 CsndFramesBeforeDspIdle = 2;
/// Max amount of CSND pending samples.
constexpr std::size_t MaxCsndPendingSamples = samples_per_frame * 4;
} // namespace

DspInterface::DspInterface(Core::System& system_) : system(system_) {}

DspInterface::~DspInterface() {
    if (stat_file) {
        std::fclose(stat_file);
    }
    if (capture_file) {
        std::fclose(capture_file);
    }
    if (capture_in_file) {
        std::fclose(capture_in_file);
    }
}

void DspInterface::SetSink(AudioCore::SinkType sink_type, std::string_view audio_device) {
    // Dispose of the current sink first to avoid contention.
    sink.reset();

    SetSink(AudioCore::GetSinkDetails(sink_type).create_sink(audio_device));
}

void DspInterface::SetSink(std::unique_ptr<Sink> new_sink) {
    sink.reset();
    sink = std::move(new_sink);
    sink->SetCallback(
        [this](s16* buffer, std::size_t num_frames) { OutputCallback(buffer, num_frames); });
    time_stretcher.SetOutputSampleRate(sink->GetNativeSampleRate());
}

Sink& DspInterface::GetSink() {
    ASSERT(sink);
    return *sink.get();
}

void DspInterface::EnableStretching(bool enable) {
    enable_time_stretching = enable;
}

void DspInterface::OutputFrame(StereoFrame16 frame) {
    std::scoped_lock lock{csnd_mutex};
    MixCsnd(frame);
    PushSamples(frame);
}

void DspInterface::OutputSample(std::array<s16, 2> sample) {
    std::scoped_lock lock{csnd_mutex};
    MixCsnd({&sample, 1});
    PushSamples({&sample, 1});
}

void DspInterface::OutputCsndFrame(const StereoFrame16& frame) {
    std::scoped_lock lock{csnd_mutex};

    if (dsp_samples_since_csnd_frame == 0) {
        csnd_frames_without_dsp++;
    } else {
        csnd_frames_without_dsp = 0;
    }
    dsp_samples_since_csnd_frame = 0;

    csnd_pending.insert(csnd_pending.end(), frame.begin(), frame.end());

    if (csnd_frames_without_dsp >= CsndFramesBeforeDspIdle) {
        // The DSP is not running, CSND is the only output.
        const std::vector<std::array<s16, 2>> samples(csnd_pending.begin(), csnd_pending.end());
        csnd_pending.clear();
        PushSamples(samples);
        return;
    }

    // The two clocks may drift a little, don't let CSND fall too far behind the DSP output.
    while (csnd_pending.size() > MaxCsndPendingSamples) {
        csnd_pending.pop_front();
    }
}

void DspInterface::MixCsnd(std::span<std::array<s16, 2>> samples) {
    // TODO(PabloMK7): Implement SIMD mixing here if possible.
    dsp_samples_since_csnd_frame += samples.size();
    for (auto& sample : samples) {
        if (csnd_pending.empty()) {
            break;
        }
        const auto& csnd = csnd_pending.front();
        for (std::size_t i = 0; i < 2; i++) {
            sample[i] = static_cast<s16>(std::clamp(sample[i] + csnd[i], -0x8000, 0x7FFF));
        }
        csnd_pending.pop_front();
    }
}

void DspInterface::PushSamples(std::span<const std::array<s16, 2>> samples) {
    if (!sink) {
        return;
    }

    pushed_frames += samples.size();
    if (sink->ImmediateSubmission()) {
        sink->PushSamples(samples.data(), samples.size());
    } else {
        fifo.Push(samples.data(), samples.size());
    }

    auto video_dumper = system.GetVideoDumper();
    if (video_dumper && video_dumper->IsDumping()) {
        for (const auto& sample : samples) {
            video_dumper->AddAudioSample(sample);
        }
    }
}

void DspInterface::DrainFifoBacklog(std::size_t num_frames) {
    // The emulation thread produces a video frame of audio at a time, so the FIFO only needs enough
    // headroom to smooth those bursts. Audio that stays in the FIFO for a whole window without
    // being needed remains queued during a stall or at stream start. That extra data delays all
    // later audio by its length, for the entire session.
    constexpr std::size_t window_frames = native_sample_rate;
    constexpr std::size_t kept_frames = native_sample_rate / 100;

    fifo_low_fill = std::min(fifo_low_fill, fifo.Size());
    backlog_window_frames += num_frames;
    if (backlog_window_frames < window_frames) {
        return;
    }
    if (fifo_low_fill > 2 * kept_frames) {
        const std::size_t excess = fifo_low_fill - kept_frames;
        fifo.Pop(excess);
        // AzaharFrame pacing: counted for the VR pacing line; while the lock runs a drop means
        // the rate trim missed, worth seeing in the log.
        auto& pace = Common::XrPacer::Get();
        pace.audio_drops.fetch_add(1, std::memory_order_relaxed);
        pace.audio_dropped_frames.fetch_add(static_cast<u32>(excess), std::memory_order_relaxed);
        if (pace.audio_ratio.load(std::memory_order_relaxed) != 1.0) {
            LOG_INFO(Audio, "Dropped {} frames of audio backlog (VR frame pacing lock on)", excess);
        } else {
            LOG_DEBUG(Audio, "Dropped {} frames of audio backlog", excess);
        }
    }
    fifo_low_fill = std::numeric_limits<std::size_t>::max();
    backlog_window_frames = 0;
}

// AzaharFrame ffaudio: a developer aid for the mocks: AZAHARFRAME_AUDIO_STATS=FILE logs the queue
// once a second, AZAHARFRAME_AUDIO_CAPTURE=FILE keeps the raw output (s16 stereo at the sink rate).
void DspInterface::InitStatFiles() {
    if (stat_init) {
        return;
    }
    stat_init = true;
    if (const char* path = std::getenv("AZAHARFRAME_AUDIO_STATS"); path && *path) {
        stat_file = std::fopen(path, "w");
    }
    if (const char* path = std::getenv("AZAHARFRAME_AUDIO_CAPTURE"); path && *path) {
        capture_file = std::fopen(path, "wb");
    }
    // ffspeed: what the game made, as it reaches the skipper (to measure the tempo and the jumps)
    if (const char* path = std::getenv("AZAHARFRAME_AUDIO_CAPTURE_IN"); path && *path) {
        capture_in_file = std::fopen(path, "wb");
    }
}

void DspInterface::WriteAudioStats(const s16* buffer, std::size_t num_frames,
                                   std::size_t frames_written, bool stretched) {
    InitStatFiles();
    if (capture_file) {
        std::fwrite(buffer, sizeof(s16), num_frames * 2, capture_file);
    }
    if (!stat_file) {
        return;
    }
    stat_callbacks++;
    stat_frames += num_frames;
    if (frames_written < num_frames) {
        stat_underruns++;
    }
    const unsigned rate = sink ? sink->GetNativeSampleRate() : native_sample_rate;
    stat_window += num_frames;
    if (stat_window < rate) {
        return;
    }
    stat_window = 0;
    const double queue_ms =
        1000.0 * static_cast<double>(fifo.Size() + time_stretcher.PendingFrames() + skipper.Queued()) /
        native_sample_rate;
    std::fprintf(stat_file,
                 "t=%.2f limit=%.0f stretch=%d ratio=%.3f queue_ms=%.1f fifo=%zu pending=%zu out=%zu in=%zu "
                 "underruns=%llu callbacks=%llu pushed=%llu gain=%.2f mode=%u cpu_ns=%llu trim=%.4f "
                 "skips=%llu skipped_ms=%.0f\n",
                 static_cast<double>(stat_frames) / rate, Settings::GetFrameLimit(),
                 stretched ? 1 : 0, time_stretcher.GetStretchRatio(), queue_ms, fifo.Size(),
                 time_stretcher.PendingFrames(), time_stretcher.OutputQueued(),
                 time_stretcher.InputQueued(), static_cast<unsigned long long>(stat_underruns),
                 static_cast<unsigned long long>(stat_callbacks),
                 static_cast<unsigned long long>(pushed_frames.load()), ff_gain,
                 static_cast<unsigned>(Settings::values.fast_forward_audio.GetValue()),
                 static_cast<unsigned long long>(stat_cpu_ns),
                 Common::XrPacer::Get().audio_ratio.load(std::memory_order_relaxed), // pacing
                 static_cast<unsigned long long>(skipper.Jumps()),
                 1000.0 * static_cast<double>(skipper.SkippedFrames()) / native_sample_rate);
    std::fflush(stat_file);
}

namespace {
u64 ThreadCpuNs() {
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return static_cast<u64>(ts.tv_sec) * 1000000000ull + static_cast<u64>(ts.tv_nsec);
}
} // namespace

void DspInterface::OutputCallback(s16* buffer, std::size_t num_frames) {
    const u64 cpu_start = stat_file ? ThreadCpuNs() : 0;
    OutputCallbackImpl(buffer, num_frames);
    if (stat_file) {
        stat_cpu_ns += ThreadCpuNs() - cpu_start;
    }
}

void DspInterface::OutputCallbackImpl(s16* buffer, std::size_t num_frames) {
    // AzaharFrame ffaudio, below: Normal Pitch stretches while fast forwarding, Muted fades out.
    const double frame_limit = Settings::GetFrameLimit();
    const FastForwardAudioPlan plan =
        PlanFastForwardAudio(Settings::values.fast_forward_audio.GetValue(), frame_limit,
                             enable_time_stretching.load());
    bool want_stretching = plan.stretch;
    bool low_latency = plan.low_latency;
    if (plan.low_latency) {
        ff_settle_frames = 0;
        time_stretcher.SetDraining(false);
    } else if (performing_time_stretching && time_stretcher.IsLowLatency()) {
        // Fast forward just ended: carry on stretching until the tempo is back to 1 and the queue
        // is nearly empty (a few tenths of a second), so the switch back drops nothing and clicks not.
        const bool settled = std::abs(time_stretcher.GetStretchRatio() - 1.0) < 0.06 &&
                             time_stretcher.BacklogFullness() < 0.12;
        if (!settled && ff_settle_frames < native_sample_rate) {
            time_stretcher.SetDraining(true);
            ff_settle_frames += num_frames;
            want_stretching = true;
            low_latency = true;
        }
    }
    time_stretcher.SetLowLatency(low_latency);
    if (!plan.skip && (want_stretching || plan.mute)) {
        skipper.Reset(); // switched to another choice mid fast forward: what it held is dropped
    }

    // Determine if we should stretch based on the current emulation speed.
    // TODO: Only activate audio stretching when emulation speed goes below 95% threshold
    //       (see #2487) -OS
    if (performing_time_stretching && !want_stretching) {
        // If we just stopped stretching, flush the stretcher before returning to normal output.
        flushing_time_stretcher = true;
    }
    performing_time_stretching = want_stretching;

    std::size_t frames_written = 0;
    if (performing_time_stretching) {
        std::vector<s16> in{fifo.Pop()};
        // AzaharFrame pacing + ffaudio: a fast forward straight from the pacing lock: the two or
        // three frames the rate trim still holds go to the stretcher first, so none are left over.
        if (!rate_trim.Idle()) {
            std::vector<s16> held;
            std::array<s16, 2 * 16> part{};
            while (!rate_trim.Idle()) {
                const std::size_t n = rate_trim.Flush(part.data(), 16);
                held.insert(held.end(), part.begin(), part.begin() + static_cast<std::ptrdiff_t>(2 * n));
            }
            in.insert(in.begin(), held.begin(), held.end());
        }
        const std::size_t num_in{in.size() / 2};
        frames_written = time_stretcher.Process(in.data(), num_in, buffer, num_frames);
    } else {
        if (flushing_time_stretcher) {
            time_stretcher.Flush();
            frames_written = time_stretcher.Process(nullptr, 0, buffer, num_frames);
            flushing_time_stretcher = false;

            // Make sure any frames that did not fit are cleared from the time stretcher,
            // so that they do not bleed into the next time the stretcher is enabled.
            time_stretcher.Clear();
        }
        // AzaharFrame ffspeed: Normal Speed. The sound the game makes goes through the skipper
        // (stretches at 1x, jumps to the newest sound); when fast forward ends it makes one last
        // jump and plays out what is left, then the plain path below takes over.
        bool skipping = plan.skip || !skipper.Idle();
        if (skipping) {
            if (plan.skip && !skipper.Finishing()) {
                std::vector<s16> in{fifo.Pop()};
                if (!rate_trim.Idle()) {
                    std::vector<s16> held;
                    std::array<s16, 2 * 16> part{};
                    while (!rate_trim.Idle()) {
                        const std::size_t n = rate_trim.Flush(part.data(), 16);
                        held.insert(held.end(), part.begin(),
                                    part.begin() + static_cast<std::ptrdiff_t>(2 * n));
                    }
                    in.insert(in.begin(), held.begin(), held.end());
                }
                InitStatFiles();
                if (capture_in_file) {
                    std::fwrite(in.data(), sizeof(s16), in.size(), capture_in_file);
                }
                skipper.Feed(in.data(), in.size() / 2);
            } else {
                skipper.Finish();
            }
            frames_written += skipper.Render(buffer + 2 * frames_written, num_frames - frames_written);
            if (skipper.Idle() && !(plan.skip && !skipper.Finishing())) {
                skipper.Reset();
                skipping = false; // the rest of the block comes straight from the fifo
            }
        }
        if (!skipping) {
        // AzaharFrame pacing: while the VR frame pacing lock runs the game fast (x1.0028 at
        // 120 Hz) the backlog is read that much faster, so it does not grow (rate_trim.h). Never
        // while fast forwarding (PlainReadRatio): ffaudio's Sped Up and Muted play it as it is.
        const double ratio =
            PlainReadRatio(Common::XrPacer::Get().audio_ratio.load(std::memory_order_relaxed),
                           frame_limit);
        if (ratio != 1.0) {
            frames_written += rate_trim.Process(
                ratio, buffer + 2 * frames_written, num_frames - frames_written,
                [this](RateTrim::Frame* dst, std::size_t max) { return fifo.Pop(dst, max); });
        } else if (!rate_trim.Idle()) {
            frames_written +=
                rate_trim.Flush(buffer + 2 * frames_written, num_frames - frames_written);
        }
        frames_written += fifo.Pop(buffer + 2 * frames_written, num_frames - frames_written);
        if (plan.mute) { // AzaharFrame ffaudio
            fifo.Pop(fifo.Size()); // nothing of the fast forward is kept for after it
        }
        DrainFifoBacklog(num_frames);
        }
    }

    if (frames_written > 0) {
        std::memcpy(&last_frame[0], buffer + 2 * (frames_written - 1), 2 * sizeof(s16));
    }

    // Hold last emitted frame; this prevents popping.
    for (std::size_t i = frames_written; i < num_frames; i++) {
        std::memcpy(buffer + 2 * i, &last_frame[0], 2 * sizeof(s16));
    }

    // AzaharFrame ffaudio: the Muted fade (30 ms) so the start and the end of a fast forward do not click
    {
        const float target = plan.mute ? 0.0f : 1.0f;
        if (ff_gain != target || target != 1.0f) {
            const unsigned rate = sink ? sink->GetNativeSampleRate() : native_sample_rate;
            const float step = 1.0f / (0.03f * static_cast<float>(rate));
            for (std::size_t i = 0; i < num_frames; i++) {
                ff_gain = ff_gain < target ? std::min(target, ff_gain + step)
                                           : std::max(target, ff_gain - step);
                buffer[i * 2 + 0] = static_cast<s16>(buffer[i * 2 + 0] * ff_gain);
                buffer[i * 2 + 1] = static_cast<s16>(buffer[i * 2 + 1] * ff_gain);
            }
        }
    }

    WriteAudioStats(buffer, num_frames, frames_written, performing_time_stretching);

    // Implementation of the hardware volume slider
    // A cubic curve is used to approximate a linear change in human-perceived loudness
    const float linear_volume = std::clamp(Settings::Volume(), 0.0f, 1.0f);
    if (linear_volume != 1.0) {
        const float volume_scale_factor = linear_volume * linear_volume * linear_volume;
        for (std::size_t i = 0; i < num_frames; i++) {
            buffer[i * 2 + 0] = static_cast<s16>(buffer[i * 2 + 0] * volume_scale_factor);
            buffer[i * 2 + 1] = static_cast<s16>(buffer[i * 2 + 1] * volume_scale_factor);
        }
    }
}

} // namespace AudioCore
