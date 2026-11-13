// AzaharFrame ffspeed
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cmath>
#include "audio_core/ff_skip.h"

namespace AudioCore {

namespace {
std::size_t Ms(double ms, unsigned rate) {
    return static_cast<std::size_t>(ms * rate / 1000.0 + 0.5);
}
} // namespace

FastForwardSkipper::FastForwardSkipper(unsigned sample_rate, SkipTuning tuning)
    : rate(sample_rate), t(tuning) {
    fade_len = std::max<std::size_t>(Ms(t.fade_ms, rate), 8);
    search = Ms(t.search_ms, rate);
    min_skip = Ms(t.min_skip_ms, rate);
    guard_extra = Ms(t.guard_ms, rate);
    seg_min = Ms(t.segment_min_ms, rate);
    seg_max = std::max(seg_min, Ms(t.segment_max_ms, rate));
    max_queue = Ms(t.max_queue_ms, rate);
    seg_target = seg_max;
}

void FastForwardSkipper::Reset() {
    buf.clear();
    pos = new_pos = fade_i = 0;
    fading = finishing = finish_done = linear_fade = false;
    seg_played = 0;
    seg_target = seg_max;
    in_since = out_since = 0;
    speed_est = 1.0;
}

void FastForwardSkipper::Compact() {
    if (pos < 16384 || fading) {
        return;
    }
    buf.erase(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(2 * pos));
    pos = 0;
}

void FastForwardSkipper::Feed(const s16* frames, std::size_t num_frames) {
    if (finishing || num_frames == 0) {
        return;
    }
    Compact();
    buf.insert(buf.end(), frames, frames + 2 * num_frames);
    in_since += num_frames;
}

void FastForwardSkipper::Finish() {
    finishing = true;
}

void FastForwardSkipper::NewSegment() {
    // How many times faster the game runs than the sound plays, over the last segment.
    if (out_since >= rate / 20) {
        const double n = static_cast<double>(in_since) / static_cast<double>(out_since);
        speed_est = 0.5 * speed_est + 0.5 * n;
    }
    in_since = out_since = 0;
    seg_played = 0;
    if (speed_est <= 1.1) {
        seg_target = seg_max;
    } else {
        const double ms = t.lag_budget_ms / (speed_est - 1.0);
        seg_target = std::clamp(Ms(ms, rate), seg_min, seg_max);
    }
}

bool FastForwardSkipper::StartJump(std::size_t guard) {
    const std::size_t size = Frames();
    const std::size_t L = fade_len;
    // The new side must start after the old side's fade and leave `guard` frames after it.
    if (size < pos + 2 * L + guard + min_skip) {
        return false;
    }
    const std::size_t hi = size - guard - L; // the newest landing point
    const std::size_t lo = pos + L + min_skip;
    if (hi < lo) {
        return false;
    }
    std::size_t best = hi;
    double rho = 0.0;
    const auto mono = [this](std::size_t i) {
        return static_cast<double>(buf[2 * i]) + static_cast<double>(buf[2 * i + 1]);
    };
    if (t.align && search > 0) {
        const std::size_t centre = hi > search ? hi - search : lo;
        const std::size_t from = std::max(lo, centre > search ? centre - search : lo);
        const std::size_t to = std::min(hi, centre + search);
        double e_old = 0;
        for (std::size_t i = 0; i < L; i++) {
            const double a = mono(pos + i);
            e_old += a * a;
        }
        double best_score = -2.0;
        for (std::size_t c = from; c <= to; c++) {
            double dot = 0, e_new = 0;
            for (std::size_t i = 0; i < L; i++) {
                const double b = mono(c + i);
                dot += mono(pos + i) * b;
                e_new += b * b;
            }
            const double score = (e_old > 0 && e_new > 0) ? dot / std::sqrt(e_old * e_new) : 0.0;
            if (score > best_score) {
                best_score = score;
                best = c;
            }
        }
        rho = best_score;
    }
    new_pos = best;
    linear_fade = rho > 0.7;
    fading = true;
    fade_i = 0;
    jumps++;
    skipped += new_pos - pos; // where the old side would be at the end of the fade, plus the fade
    return true;
}

std::size_t FastForwardSkipper::Render(s16* out, std::size_t num_frames) {
    const std::size_t guard = num_frames + guard_extra;
    std::size_t w = 0;
    while (w < num_frames) {
        if (fading) {
            const std::size_t k = std::min(num_frames - w, fade_len - fade_i);
            for (std::size_t j = 0; j < k; j++) {
                const double x = (static_cast<double>(fade_i) + 0.5) / static_cast<double>(fade_len);
                double go, gn;
                if (linear_fade) {
                    gn = 0.5 - 0.5 * std::cos(M_PI * x);
                    go = 1.0 - gn;
                } else {
                    go = std::cos(0.5 * M_PI * x);
                    gn = std::sin(0.5 * M_PI * x);
                }
                for (int ch = 0; ch < 2; ch++) {
                    const double v = go * buf[2 * pos + ch] + gn * buf[2 * new_pos + ch];
                    out[2 * (w + j) + ch] = static_cast<s16>(std::clamp(v, -32768.0, 32767.0));
                }
                pos++;
                new_pos++;
                fade_i++;
            }
            w += k;
            if (fade_i >= fade_len) {
                fading = false;
                pos = new_pos;
                if (!finishing) {
                    NewSegment();
                    seg_played = fade_len;
                }
            }
            continue;
        }
        const std::size_t avail = Frames() - pos;
        if (avail == 0) {
            break;
        }
        const bool due = finishing ? !finish_done
                                   : (seg_played >= seg_target || avail > max_queue);
        if (due) {
            if (StartJump(guard)) {
                finish_done = finishing;
                continue;
            }
            finish_done = finishing; // too little waiting to skip: play out what there is
        }
        std::size_t n = std::min(num_frames - w, avail);
        if (!due && !finishing) {
            n = std::min(n, seg_target - seg_played);
        }
        std::copy_n(buf.begin() + static_cast<std::ptrdiff_t>(2 * pos), 2 * n, out + 2 * w);
        pos += n;
        w += n;
        seg_played += n;
    }
    out_since += w;
    return w;
}

} // namespace AudioCore
