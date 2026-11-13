// Copyright 2017-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#pragma once

#include <cstdio>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <boost/serialization/access.hpp>
#include "audio_core/audio_types.h"
#include "audio_core/rate_trim.h"
#include "audio_core/time_stretch.h"
#include "common/common_types.h"
#include "common/ring_buffer.h"
#include "common/settings.h"
#include "core/memory.h"

namespace Core {
class System;
} // namespace Core

namespace Service::DSP {
enum class InterruptType : u32;
} // namespace Service::DSP

namespace AudioCore {

class Sink;
enum class SinkType : u32;

/// AzaharFrame ffaudio: what the audio does for a fast forward audio mode and the frame limit.
struct FastForwardAudioPlan {
    bool stretch = false;     ///< run the time stretcher
    bool low_latency = false; ///< ...with the small queue of the fast forward profile
    bool mute = false;        ///< fade out and keep the fifo empty
};

/// A frame limit above 100% (or none, 0) is fast forwarding.
constexpr bool IsFastForwarding(double frame_limit) {
    return frame_limit == 0.0 || frame_limit > 100.0;
}

constexpr FastForwardAudioPlan PlanFastForwardAudio(Settings::FastForwardAudio mode,
                                                    double frame_limit, bool user_stretching) {
    FastForwardAudioPlan plan;
    plan.stretch = user_stretching;
    if (IsFastForwarding(frame_limit)) {
        if (mode == Settings::FastForwardAudio::NormalPitch) {
            plan.stretch = true;
            plan.low_latency = true;
        } else if (mode == Settings::FastForwardAudio::Muted) {
            plan.mute = true;
        }
    }
    return plan;
}

/// AzaharFrame pacing + ffaudio: the read speed of the plain (unstretched) output path. While the
/// VR frame pacing lock runs (only at a 100% speed limit) the backlog is read at the game's speed
/// (`pacer_ratio`, rate_trim.h); at any other limit (fast forward, a slowed game) it plays as it
/// is. The lock turns itself off at those limits; this also covers the callback or two between a
/// limit change and the limiter's next frame.
constexpr double PlainReadRatio(double pacer_ratio, double frame_limit) {
    return frame_limit == 100.0 ? pacer_ratio : 1.0;
}

class DspInterface {
public:
    DspInterface(Core::System& system_);
    virtual ~DspInterface();

    DspInterface(const DspInterface&) = delete;
    DspInterface(DspInterface&&) = delete;
    DspInterface& operator=(const DspInterface&) = delete;
    DspInterface& operator=(DspInterface&&) = delete;

    /**
     * Reads data from one of three DSP registers
     * @note this function blocks until the data is available
     * @param register_number the index of the register to read
     * @returns the value of the register
     */
    virtual u16 RecvData(u32 register_number) = 0;

    /**
     * Checks whether data is ready in one of three DSP registers
     * @param register_number the index of the register to check
     * @returns true if data is ready
     */
    virtual bool RecvDataIsReady(u32 register_number) const = 0;

    /**
     * Sets the DSP semaphore register
     * @param semaphore_value the value set to the semaphore register
     */
    virtual void SetSemaphore(u16 semaphore_value) = 0;

    /**
     * Reads `length` bytes from the DSP pipe identified with `pipe_number`.
     * @note Can read up to the maximum value of a u16 in bytes (65,535).
     * @note IF an error is encoutered with either an invalid `pipe_number` or `length` value, an
     * empty vector will be returned.
     * @note IF `length` is set to 0, an empty vector will be returned.
     * @note IF `length` is greater than the amount of data available, this function will only read
     * the available amount.
     * @param pipe_number a `DspPipe`
     * @param length the number of bytes to read. The max is 65,535 (max of u16).
     * @returns a vector of bytes from the specified pipe. On error, will be empty.
     */
    virtual std::vector<u8> PipeRead(DspPipe pipe_number, std::size_t length) = 0;

    /**
     * How much data is left in pipe
     * @param pipe_number The Pipe ID
     * @return The amount of data remaning in the pipe. This is the maximum length PipeRead will
     * return.
     */
    virtual std::size_t GetPipeReadableSize(DspPipe pipe_number) const = 0;

    /**
     * Write to a DSP pipe.
     * @param pipe_number The Pipe ID
     * @param buffer The data to write to the pipe.
     */
    virtual void PipeWrite(DspPipe pipe_number, std::span<const u8> buffer) = 0;

    /// Sets the handler for the interrupts we trigger
    virtual void SetInterruptHandler(
        std::function<void(Service::DSP::InterruptType type, DspPipe pipe)> handler) = 0;

    /// Loads the DSP program
    virtual void LoadComponent(std::span<const u8> buffer) = 0;

    /// Unloads the DSP program
    virtual void UnloadComponent() = 0;

    /// Select the sink to use based on sink type.
    void SetSink(SinkType sink_type, std::string_view audio_device);
    /// Uses this sink (AzaharFrame: the audio tests drive the output callback through their own).
    void SetSink(std::unique_ptr<Sink> new_sink);
    /// Get the current sink
    Sink& GetSink();
    /// Enable/Disable audio stretching.
    void EnableStretching(bool enable);

    /**
     * Adds one frame of CSND output. On hardware CSND and the DSP reach the codec chip on separate
     * I2S lines and the codec mixes them, here the frame is mixed to the DSP output.
     * Must be called once per audio frame period.
     */
    void OutputCsndFrame(const StereoFrame16& frame);

protected:
    void OutputFrame(StereoFrame16 frame);
    void OutputSample(std::array<s16, 2> sample);

private:
    void FlushResidualStretcherAudio();
    void WriteAudioStats(const s16* buffer, std::size_t num_frames, std::size_t frames_written,
                         bool stretched);
    void OutputCallback(s16* buffer, std::size_t num_frames);
    void OutputCallbackImpl(s16* buffer, std::size_t num_frames);
    void DrainFifoBacklog(std::size_t num_frames);
    void MixCsnd(std::span<std::array<s16, 2>> samples);
    void PushSamples(std::span<const std::array<s16, 2>> samples);

    Core::System& system;

    std::atomic<bool> enable_time_stretching = false;
    std::atomic<bool> performing_time_stretching = false;
    std::atomic<bool> flushing_time_stretcher = false;
    Common::RingBuffer<s16, 0x2000, 2> fifo;
    std::array<s16, 2> last_frame{};
    std::size_t fifo_low_fill = std::numeric_limits<std::size_t>::max();
    std::size_t backlog_window_frames = 0;
    TimeStretcher time_stretcher;
    // AzaharFrame ffaudio (the audio thread's, but stats' counters)
    std::size_t ff_settle_frames = 0; ///< output frames since fast forward ended, still stretching
    float ff_gain = 1.0f;             ///< the fast forward mute fade, 0 to 1
    std::atomic<u64> pushed_frames{0};
    u64 stat_cpu_ns = 0; ///< the audio callback's CPU time (stats only)
    u64 stat_callbacks = 0, stat_underruns = 0, stat_frames = 0, stat_window = 0;
    std::FILE* stat_file = nullptr;
    std::FILE* capture_file = nullptr;
    bool stat_init = false;
    RateTrim rate_trim; ///< AzaharFrame pacing: the backlog read at the game's speed while locked
    std::unique_ptr<Sink> sink;

    std::mutex csnd_mutex;
    StereoBuffer16 csnd_pending;
    std::size_t dsp_samples_since_csnd_frame = 0;
    u32 csnd_frames_without_dsp = 0;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {}
    friend class boost::serialization::access;
};

} // namespace AudioCore
