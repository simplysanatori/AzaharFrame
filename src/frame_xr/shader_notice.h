// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame stall: "Preparing Shaders 12 / 40" while the game stands still because shaders and
// pipelines are being built (Common::StallTelemetry's build counters). A game's first run, or the
// first after the shader cache was made again, can stand 30 s on a load screen this way (Kid
// Icarus: Uprising, 2026-11-12); the notice says why. Pure logic: the caller shows the text as a
// VR toast or in the status bar, and polls a few times a second.

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include <fmt/format.h>

namespace FrameXr {

class ShaderNotice {
public:
    /// The game stands this long without an emulated frame before a build is worth a notice.
    static constexpr std::int64_t kStandUs = 400'000;
    /// Builds have stopped this long (and none waits): the notice goes.
    static constexpr std::int64_t kQuietUs = 1'000'000;
    /// The same text is shown again this often (a toast fades after 2 s).
    static constexpr std::int64_t kRefreshUs = 1'000'000;

    struct Out {
        std::string show; ///< show this text now ("" nothing new)
        std::string hide; ///< take this text back ("" nothing)
    };

    /// `queued` and `done`: the cumulative build counters; `frame_end_us`: when the last emulated
    /// frame ended (0 none yet: the game stands); `running`: a game runs and is not paused.
    Out Update(std::int64_t now_us, std::uint64_t queued, std::uint64_t done,
               std::int64_t frame_end_us, bool running) {
        Out out;
        if (!running) {
            if (active) {
                out.hide = shown;
            }
            Reset(queued);
            return out;
        }
        if (!seen) { // the counts so far belong to no burst
            seen = true;
            last_queued = queued;
            base = done;
        }
        if (queued != last_queued) {
            // A burst starts after a quiet second: its counts start at the builds finished then.
            if (last_activity_us == 0 || now_us - last_activity_us >= kQuietUs) {
                base = std::min(done, last_queued);
            }
            last_activity_us = now_us;
            last_queued = queued;
        }
        const std::uint64_t backlog = queued > done ? queued - done : 0;
        const bool standing = frame_end_us == 0 || now_us - frame_end_us >= kStandUs;
        const bool recent = last_activity_us > 0 && now_us - last_activity_us < kQuietUs;
        if (!active) {
            if (!standing || (backlog == 0 && !recent)) {
                return out;
            }
            active = true;
            last_shown_us = 0;
        } else if (backlog == 0 && (!standing || !recent)) {
            out.hide = shown;
            Reset(queued);
            return out;
        }
        const std::string text = Text(queued, done);
        if (text != shown || now_us - last_shown_us >= kRefreshUs) {
            shown = text;
            last_shown_us = now_us;
            out.show = text;
        }
        return out;
    }

    /// Azahar's loading screen string ("Preparing Shaders %1 / %2"), with this burst's counts.
    std::string Text(std::uint64_t queued, std::uint64_t done) const {
        const std::uint64_t total = queued > base ? queued - base : 0;
        const std::uint64_t finished = done > base ? done - base : 0;
        return fmt::format("Preparing Shaders {} / {}", std::min(finished, total), total);
    }

    bool Active() const {
        return active;
    }

private:
    void Reset(std::uint64_t queued) {
        active = false;
        shown.clear();
        last_queued = queued;
    }

    bool seen = false;
    bool active = false;
    std::uint64_t base = 0;          ///< builds finished before this burst
    std::uint64_t last_queued = 0;
    std::int64_t last_activity_us = 0;
    std::int64_t last_shown_us = 0;
    std::string shown;
};

} // namespace FrameXr
