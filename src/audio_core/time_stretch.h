// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include "common/common_types.h"

namespace soundtouch {
class SoundTouch;
}

namespace AudioCore {

class TimeStretcher {
public:
    TimeStretcher();
    ~TimeStretcher();

    void SetOutputSampleRate(unsigned int sample_rate);

    /// @param in       Input sample buffer
    /// @param num_in   Number of input frames in `in`
    /// @param out      Output sample buffer
    /// @param num_out  Desired number of output frames in `out`
    /// @returns Actual number of frames written to `out`
    std::size_t Process(const s16* in, std::size_t num_in, s16* out, std::size_t num_out);

    void Clear();

    void Flush();

    // AzaharFrame ffaudio from here to the private part
    /// A profile for fast forwarding: a small queue (about 60 ms), a
    /// faster ratio filter and shorter SoundTouch windows, so the audio stays with the picture.
    void SetLowLatency(bool enable);

    /// With the low latency profile: aim for an empty queue (the end of a fast forward) rather
    /// than a part-full one, so switching back to plain output drops next to nothing.
    void SetDraining(bool enable) {
        draining = enable;
    }

    /// Sets the tempo at once (the end of a fast forward is known, so the filter need not find it).
    void SnapRatio(double ratio) {
        stretch_ratio = ratio;
    }

    /// The tempo now in use (input frames per output frame).
    [[nodiscard]] double GetStretchRatio() const {
        return stretch_ratio;
    }

    [[nodiscard]] bool IsLowLatency() const {
        return low_latency;
    }

    /// Frames processed and waiting / frames still to be processed (input time), for the logs.
    [[nodiscard]] std::size_t OutputQueued() const;
    [[nodiscard]] std::size_t InputQueued() const;

    /// Frames inside the stretcher in output time: not yet processed plus processed and waiting.
    [[nodiscard]] std::size_t PendingFrames() const;

    /// The queue's frames (processed, waiting) as a part of the profile's full size.
    [[nodiscard]] double BacklogFullness() const;

private:
    bool low_latency = false;
    bool draining = false;
    std::unique_ptr<soundtouch::SoundTouch> sound_touch;
    double stretch_ratio = 1.0;
};

} // namespace AudioCore
