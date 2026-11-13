// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame pacing: while the VR frame pacing lock runs the game 0.28 % fast (common/xr_pacer.h)
// the DSP makes 0.28 % more audio than the output plays. Without audio stretching the backlog
// would grow 2.8 ms a second and DrainFifoBacklog would cut out about 10 ms at a time (a click).
// RateTrim reads the backlog at the game's speed instead: `ratio` input frames per output frame,
// interpolated between neighbours, so nothing is ever cut.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include "common/common_types.h"

namespace AudioCore {

class RateTrim {
public:
    using Frame = std::array<s16, 2>;

    /// True when nothing is held back (the plain path may take over).
    bool Idle() const {
        return held.empty();
    }

    /// Writes up to `frames` stereo frames to `out`, reading the input `ratio` frames per output
    /// frame. `pop(Frame* dst, std::size_t max)` takes up to `max` frames of input and returns how
    /// many it gave. Returns the frames written: fewer when the input ran dry.
    template <typename PopFn>
    std::size_t Process(double ratio, s16* out, std::size_t frames, PopFn&& pop) {
        std::size_t written = 0;
        while (written < frames) {
            const auto i = static_cast<std::size_t>(pos);
            if (i + 1 >= held.size()) {
                const auto want = static_cast<std::size_t>(
                                      std::ceil(static_cast<double>(frames - written) * ratio)) +
                                  2;
                const std::size_t have = held.size();
                held.resize(have + want);
                const std::size_t got = pop(held.data() + have, want);
                held.resize(have + got);
                if (got == 0) {
                    break;
                }
                continue;
            }
            const double f = pos - static_cast<double>(i);
            for (std::size_t c = 0; c < 2; c++) {
                const double a = held[i][c], b = held[i + 1][c];
                out[2 * written + c] = static_cast<s16>(std::lround(a + f * (b - a)));
            }
            written++;
            pos += ratio;
        }
        const std::size_t used = std::min(static_cast<std::size_t>(pos), held.size());
        held.erase(held.begin(), held.begin() + static_cast<std::ptrdiff_t>(used));
        pos -= static_cast<double>(used);
        return written;
    }

    /// The lock ended: writes what is held back as it is (at most `frames`) and forgets the rest
    /// of the fraction. Returns the frames written.
    std::size_t Flush(s16* out, std::size_t frames) {
        const auto start = std::min(static_cast<std::size_t>(pos), held.size());
        const std::size_t n = std::min(frames, held.size() - start);
        for (std::size_t k = 0; k < n; k++) {
            out[2 * k] = held[start + k][0];
            out[2 * k + 1] = held[start + k][1];
        }
        held.erase(held.begin(), held.begin() + static_cast<std::ptrdiff_t>(start + n));
        pos = 0;
        return n;
    }

private:
    std::vector<Frame> held; ///< input read from the backlog and not yet played past
    double pos = 0;          ///< where in `held` the next output frame is read
};

} // namespace AudioCore
