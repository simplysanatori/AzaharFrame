// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame stall: "Preparing Shaders n / m" only while the game stands for its builds.
// AzaharFrame shaderbadge: shown at least 1.5 s, short gaps bridged (no flicker), the disk
// cache's loading in the same badge.

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/shader_notice.h"

using FrameXr::ShaderNotice;

namespace {
constexpr std::int64_t kMs = 1000;

/// The GUI's poll (every 100 ms) over a game: frames flow or stand, builds are queued and done.
struct Game {
    ShaderNotice n;
    std::int64_t t = 1000 * kMs;
    std::uint64_t queued = 0, done = 0;
    std::int64_t frame_end = 1000 * kMs;
    bool running = true;
    int shows = 0, hides = 0;
    bool was = false;

    const ShaderNotice::State& Poll() {
        const auto& s = n.Update(t, queued, done, frame_end, running);
        if (s.visible && !was) {
            shows++;
        }
        if (!s.visible && was) {
            hides++;
        }
        was = s.visible;
        return s;
    }
    /// `ms` of play: frames every poll (`frames`) or none, `per_poll` builds queued and done.
    void Play(int ms, bool frames, int per_poll = 0) {
        for (int i = 0; i < ms / 100; i++) {
            t += 100 * kMs;
            if (frames) {
                frame_end = t - 8 * kMs;
            }
            queued += per_poll;
            done += per_poll;
            Poll();
        }
    }
};
} // namespace

TEST_CASE("ShaderNotice: builds while frames flow say nothing", "[frame_xr]") {
    Game g;
    g.Play(3000, true, 2);
    REQUIRE(g.shows == 0);
    REQUIRE_FALSE(g.n.Active());
}

TEST_CASE("ShaderNotice: a hitch under 400 ms shows nothing", "[frame_xr]") {
    Game g;
    g.Play(1000, true);
    g.queued += 20; // a stall of 300 ms while 20 are built
    g.Play(300, false);
    g.done += 20;
    g.Play(2000, true);
    REQUIRE(g.shows == 0);
}

TEST_CASE("ShaderNotice: a game standing for its builds, then going on", "[frame_xr]") {
    ShaderNotice n;
    const std::int64_t frame_end = 1000 * kMs; // the last frame, then the load screen stands
    std::int64_t t = 1000 * kMs;
    REQUIRE_FALSE(n.Update(t, 10, 10, frame_end, true).visible); // ten earlier builds, all done
    t += 200 * kMs;
    REQUIRE_FALSE(n.Update(t, 50, 12, frame_end, true).visible); // not standing long enough yet
    t += 300 * kMs;
    auto s = n.Update(t, 60, 20, frame_end, true);
    REQUIRE(s.visible);
    REQUIRE(s.text == "Preparing Shaders 10 / 50");
    REQUIRE(s.progress == 0.2f);
    t += 200 * kMs;
    s = n.Update(t, 60, 40, frame_end, true);
    REQUIRE(s.text == "Preparing Shaders 30 / 50");
    const std::int64_t last_stall = t;
    t += 200 * kMs;
    s = n.Update(t, 60, 60, t - 10 * kMs, true); // all built, frames again: it stays a while
    REQUIRE(s.visible);
    REQUIRE(s.text == "Preparing Shaders 50 / 50");
    REQUIRE(s.progress == 1.0f);
    t = last_stall + ShaderNotice::kBridgeUs - 100 * kMs;
    REQUIRE(n.Update(t, 60, 60, t - 10 * kMs, true).visible);
    t += 100 * kMs; // the bridge is over
    REQUIRE_FALSE(n.Update(t, 60, 60, t - 10 * kMs, true).visible);
    REQUIRE(n.Current().text.empty());
    t += 2000 * kMs;
    REQUIRE_FALSE(n.Update(t, 60, 60, frame_end, true).visible); // standing, nothing building
}

TEST_CASE("ShaderNotice: once shown it stays at least 1.5 s and 2 s past the stall", "[frame_xr]") {
    Game g;
    g.Play(1000, true);
    g.queued += 5;
    g.Play(500, false); // stands 0.5 s: shows at 0.4 s
    REQUIRE(g.shows == 1);
    const std::int64_t shown_at = g.t - 100 * kMs;
    g.done += 5;
    // Frames again at once: on view for the bridge after the last busy poll, then gone.
    std::int64_t gone_at = 0;
    for (int i = 0; i < 40 && !gone_at; i++) {
        g.Play(100, true);
        if (!g.n.Active()) {
            gone_at = g.t;
        }
    }
    REQUIRE(gone_at - shown_at >= ShaderNotice::kMinShowUs);
    REQUIRE(gone_at - shown_at >= ShaderNotice::kBridgeUs);
    REQUIRE(gone_at - shown_at <= ShaderNotice::kBridgeUs + 300 * kMs);
    REQUIRE(g.hides == 1);
}

TEST_CASE("ShaderNotice: stalls within 2 s keep one badge, the counts go on", "[frame_xr]") {
    Game g;
    g.Play(1000, true);
    // Three stalls of 0.6 s, 1.5 s of play between them (no async shaders: each area builds).
    for (int k = 0; k < 3; k++) {
        g.queued += 10;
        g.Play(300, false);
        g.done += 5;
        g.Play(300, false);
        g.done += 5;
        g.Play(1500, true);
    }
    REQUIRE(g.shows == 1); // one badge throughout, never hidden and shown again
    REQUIRE(g.hides == 0);
    REQUIRE(g.n.Current().text == "Preparing Shaders 30 / 30"); // one count for all three
    g.Play(2500, true);
    REQUIRE(g.hides == 1);
    // A stall long after starts a new count.
    g.queued += 4;
    g.Play(500, false);
    REQUIRE(g.shows == 2);
    REQUIRE(g.n.Current().text == "Preparing Shaders 0 / 4");
}

TEST_CASE("ShaderNotice: paused or stopped takes it back at once", "[frame_xr]") {
    ShaderNotice n;
    REQUIRE(n.Update(1000 * kMs, 5, 0, 0, true).text == "Preparing Shaders 0 / 5"); // no frame yet
    const auto& s = n.Update(1100 * kMs, 6, 1, 0, false);
    REQUIRE_FALSE(s.visible);
    REQUIRE(s.text.empty());
    REQUIRE_FALSE(n.Active());
}

TEST_CASE("ShaderNotice: the disk cache's loading shows in the same badge", "[frame_xr]") {
    Game g;
    g.frame_end = 0; // no frame yet: the game boots
    g.n.Loading(g.t, "Preparing Shaders 3 / 9", 3, 9);
    auto s = g.Poll();
    REQUIRE(s.visible); // at once, no 400 ms wait
    REQUIRE(s.text == "Preparing Shaders 3 / 9");
    REQUIRE(s.progress == 3.0f / 9.0f);
    g.t += 100 * kMs;
    g.n.Loading(g.t, "Loading Vertex Shader 12 / 40", 12, 40);
    REQUIRE(g.Poll().text == "Loading Vertex Shader 12 / 40");
    g.n.LoadDone(g.t);
    g.Play(1000, true);
    REQUIRE(g.n.Active()); // bridged
    REQUIRE(g.n.Current().text == "Loading Vertex Shader 12 / 40");
    // The game's first stall right after: the same badge, a fresh count.
    g.queued += 7;
    g.Play(500, false);
    REQUIRE(g.shows == 1);
    REQUIRE(g.n.Current().text == "Preparing Shaders 0 / 7");
    g.done += 7;
    g.Play(3000, true);
    REQUIRE_FALSE(g.n.Active());
    REQUIRE(g.shows == 1);
}

TEST_CASE("ShaderNotice: a load step counts while the game is not running yet", "[frame_xr]") {
    ShaderNotice n;
    n.Loading(1000 * kMs, "Preparing Shaders 1 / 2", 1, 2);
    REQUIRE(n.Update(1000 * kMs, 0, 0, 0, false).visible);
    // Never finished (the boot was cancelled): it goes once the step is stale.
    REQUIRE_FALSE(n.Update(1000 * kMs + ShaderNotice::kLoadStaleUs, 0, 0, 0, false).visible);
}
