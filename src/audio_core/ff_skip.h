// AzaharFrame ffspeed
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <cstddef>
#include <vector>
#include "common/common_types.h"

namespace AudioCore {

/// How the skipper cuts the sound; the defaults are what the DSP uses (see the ffspeed report).
struct SkipTuning {
    double segment_min_ms = 80.0;  ///< shortest stretch played as it is
    double segment_max_ms = 250.0; ///< longest (also used at 1x)
    double lag_budget_ms = 300.0;  ///< the sound is at most about this far behind just before a jump
    double fade_ms = 15.0;         ///< crossfade length at each jump
    double search_ms = 6.0;        ///< a jump may move this much either way to land in phase
    double min_skip_ms = 10.0;     ///< no jump that would skip less than this
    double guard_ms = 4.0;         ///< sound kept past the jump so the next block is covered
    double max_queue_ms = 1000.0;  ///< a jump is forced when this much is waiting
    bool align = true;             ///< pick the landing point by cross-correlation
};

/**
 * Fast Forward Audio "Normal Speed": the game makes N times as much sound as real time. This plays
 * a stretch of it at 1x, then jumps to the newest sound and carries on, so the tempo and the pitch
 * stay as the game made them and about 1/N of the sound is heard. Each jump is a short equal-power
 * crossfade (a plain fade when the two sides are the same wave) that lands where the two are in
 * phase. The sound is at most `lag_budget_ms` behind the picture and 0 right after a jump.
 *
 * Frames are interleaved s16 stereo. Not thread safe: the audio thread owns it.
 */
class FastForwardSkipper {
public:
    explicit FastForwardSkipper(unsigned sample_rate, SkipTuning tuning = {});

    /// Appends sound the game made.
    void Feed(const s16* frames, std::size_t num_frames);
    /// Fills up to `num_frames` and returns how many it wrote (fewer only when it ran dry).
    std::size_t Render(s16* out, std::size_t num_frames);
    /// Fast forward ended: one last jump to the newest sound, then what is left is played out.
    void Finish();
    /// Nothing waits and no fade runs, so the plain path can take over.
    bool Idle() const {
        return !fading && Queued() == 0;
    }
    /// Frames waiting to be played (not the ones a jump will skip).
    std::size_t Queued() const {
        return buf.size() / 2 - (fading ? new_pos : pos);
    }
    bool Finishing() const {
        return finishing;
    }
    u64 Jumps() const {
        return jumps;
    }
    u64 SkippedFrames() const {
        return skipped;
    }
    void Reset();

private:
    std::size_t Frames() const {
        return buf.size() / 2;
    }
    bool StartJump(std::size_t guard);
    void NewSegment();
    void Compact();

    unsigned rate;
    SkipTuning t;
    std::size_t fade_len, search, min_skip, guard_extra, seg_min, seg_max, max_queue;
    std::vector<s16> buf;
    std::size_t pos = 0;        ///< next frame to play
    bool fading = false;
    std::size_t fade_i = 0, new_pos = 0;
    bool linear_fade = false;   ///< the two sides were alike: gains add to 1
    bool finishing = false;
    bool finish_done = false;   ///< the last jump was made (or there was nothing to skip)
    std::size_t seg_played = 0, seg_target = 0;
    u64 in_since = 0, out_since = 0;
    double speed_est = 1.0;
    u64 jumps = 0, skipped = 0;
};

} // namespace AudioCore
