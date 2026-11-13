// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame stall: "Preparing Shaders 12 / 40" while the game stands still because shaders and
// pipelines are being built (Common::StallTelemetry's build counters). A game's first run, or the
// first after the shader cache was made again, can stand 30 s on a load screen this way (Kid
// Icarus: Uprising, 2026-11-12); the notice says why. Pure logic: the caller polls it a few times
// a second and shows its state as the VR badge in the main screen's top-right corner
// (FrameXr::SetShaderBadge, vis/shader_badge.h) or in the status bar in flat.
//
// AzaharFrame shaderbadge: it used to be a centre toast that came back for a second at every
// stall; without async shaders that kept popping up in the middle of play. Now it is one steady
// state with no flicker:
//   - it shows only once the game has stood kStandUs with builds waiting or just made (brief
//     hitches show nothing);
//   - once shown it stays at least kMinShowUs, and until kBridgeUs after the last stall or build,
//     so a new stall within that time keeps the same badge and only its counts change;
//   - the disk shader cache's progress at boot ("Preparing Shaders 3 / 9", "Loading Vertex Shader
//     12 / 40", Loading) shows in the same badge.
// The fades (vis/shader_badge.h) are the XR thread's.

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
    /// Builds have stopped this long: the next one starts a new count ("0 / n").
    static constexpr std::int64_t kQuietUs = 1'000'000;
    /// Once shown, the badge stays at least this long.
    static constexpr std::int64_t kMinShowUs = 1'500'000;
    /// And until this long after the last stall, build or load step: a new stall within it keeps
    /// the badge (its counts change) instead of hiding and showing it again.
    static constexpr std::int64_t kBridgeUs = 2'000'000;
    /// A load step (Loading) counts as loading this long without the next one or LoadDone.
    static constexpr std::int64_t kLoadStaleUs = 3'000'000;

    struct State {
        bool visible = false;
        std::string text;      ///< "" while hidden
        float progress = 0.0f; ///< done / total, 0 to 1
        bool operator==(const State&) const = default;
    };

    /// `queued` and `done`: the cumulative build counters; `frame_end_us`: when the last emulated
    /// frame ended (0 none yet: the game stands); `running`: a game runs and is not paused.
    /// Paused or stopped hides it at once (no minimum, no bridge).
    const State& Update(std::int64_t now_us, std::uint64_t queued, std::uint64_t done,
                        std::int64_t frame_end_us, bool running) {
        const bool loading_now = loading && now_us - last_load_us < kLoadStaleUs;
        if (!running && !loading_now) {
            Reset(queued);
            return state;
        }
        if (!seen) { // the counts so far belong to no burst
            seen = true;
            last_queued = queued;
            base = done;
        }
        if (queued != last_queued) {
            // A burst starts after a quiet second: its counts start at the builds finished then.
            // Not while the badge shows a burst's counts (a bridged stall keeps adding to them).
            const bool quiet = last_activity_us == 0 || now_us - last_activity_us >= kQuietUs;
            if (fresh || (quiet && !(state.visible && !loading_now))) {
                base = std::min(done, last_queued);
                fresh = false;
            }
            last_activity_us = now_us;
            last_queued = queued;
        }
        const std::uint64_t backlog = queued > done ? queued - done : 0;
        const bool standing = frame_end_us == 0 || now_us - frame_end_us >= kStandUs;
        const bool recent = last_activity_us > 0 && now_us - last_activity_us < kQuietUs;
        const bool stall = standing && (backlog > 0 || recent);
        // Busy keeps a shown badge: a stall, a load step, or builds still waiting or just made.
        const bool busy =
            stall || loading_now || (state.visible && (backlog > 0 || last_activity_us == now_us));
        if (busy) {
            last_busy_us = now_us;
        }
        if (!state.visible) {
            if (!stall && !loading_now) {
                return state;
            }
            state.visible = true;
            shown_since_us = now_us;
        } else if (!busy && now_us - shown_since_us >= kMinShowUs &&
                   now_us - last_busy_us >= kBridgeUs) {
            Reset(queued);
            return state;
        }
        if (loading_now) {
            state.text = load_text;
            state.progress = Fraction(load_done, load_total);
        } else if (!fresh || state.text.empty()) { // after the loading: its line until a build
            const std::uint64_t total = queued > base ? queued - base : 0;
            const std::uint64_t finished = std::min(done > base ? done - base : 0, total);
            state.text = Text(finished, total);
            state.progress = Fraction(finished, total);
        }
        return state;
    }

    /// GUI thread, VR: a step of the disk shader cache's loading at boot (Azahar's loading screen
    /// line, `done` of `total`). The badge shows it at once.
    void Loading(std::int64_t now_us, const std::string& text, std::uint64_t done,
                 std::uint64_t total) {
        loading = true;
        last_load_us = now_us;
        load_text = text;
        load_done = done;
        load_total = total;
    }
    /// The loading is over (the badge then goes as after a stall). The next builds count anew.
    void LoadDone(std::int64_t now_us) {
        if (loading) {
            loading = false;
            last_busy_us = now_us;
            fresh = true; // the next build starts a new burst
        }
    }

    /// Azahar's loading screen string ("Preparing Shaders %1 / %2").
    static std::string Text(std::uint64_t finished, std::uint64_t total) {
        return fmt::format("Preparing Shaders {} / {}", finished, total);
    }

    const State& Current() const {
        return state;
    }
    bool Active() const {
        return state.visible;
    }

private:
    static float Fraction(std::uint64_t done, std::uint64_t total) {
        return total == 0
                   ? 0.0f
                   : std::clamp(static_cast<float>(done) / static_cast<float>(total), 0.0f, 1.0f);
    }

    void Reset(std::uint64_t queued) {
        state = {};
        last_queued = queued;
        loading = false;
        fresh = false;
    }

    State state;
    bool seen = false;
    std::uint64_t base = 0; ///< builds finished before this burst
    std::uint64_t last_queued = 0;
    std::int64_t last_activity_us = 0; ///< the last change of `queued`
    std::int64_t last_busy_us = 0;
    std::int64_t shown_since_us = 0;
    bool fresh = false; ///< the loading just ended: the next build starts a new burst
    bool loading = false;
    std::int64_t last_load_us = 0;
    std::string load_text;
    std::uint64_t load_done = 0, load_total = 0;
};

} // namespace FrameXr
