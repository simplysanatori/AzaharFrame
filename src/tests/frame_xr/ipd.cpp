// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <random>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/ipd_smoother.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/vr_immersive.h"

// AzaharFrame ipd: the headset's IPD, measured from the located eyes, for the game's eyes in
// mode 3 and the head-locked screens.

using Catch::Approx;
using FrameXr::Ipd::Smoother;

namespace {

constexpr int64_t kPeriodNs = 8'333'333; // 120 Hz

/// Feeds `seconds` of frames from frame `f` on, the IPD given by `ipd(frame)`; returns the next
/// frame. `each(frame, value)` sees every value handed out.
template <typename Ipd, typename Each>
int Run(Smoother& s, int f, double seconds, Ipd&& ipd, Each&& each, bool valid = true) {
    const int n = static_cast<int>(seconds * 120.0 + 0.5);
    for (int i = 0; i < n; i++, f++) {
        each(f, s.Add(f * kPeriodNs, ipd(f), valid));
    }
    return f;
}
template <typename Ipd>
int Run(Smoother& s, int f, double seconds, Ipd&& ipd, bool valid = true) {
    return Run(s, f, seconds, ipd, [](int, float) {}, valid);
}

} // namespace

TEST_CASE("FrameXr ipd: 65 mm until the headset's is known", "[frame_xr]") {
    Smoother s;
    float logged = 0.0f;
    REQUIRE(s.Value() == FrameXr::Ipd::kFallbackM);
    REQUIRE_FALSE(s.TakeLog(logged));
    // Untracked frames, and values that are no IPD, are not a value.
    int f = Run(s, 0, 1.0, [](int) { return 0.061f; }, false);
    f = Run(s, f, 1.0, [](int) { return 0.0f; });
    f = Run(s, f, 1.0, [](int) { return NAN; });
    f = Run(s, f, 1.0, [](int) { return 0.2f; });
    REQUIRE_FALSE(s.Known());
    REQUIRE(s.Value() == FrameXr::Ipd::kFallbackM);
    REQUIRE_FALSE(s.TakeLog(logged));
    // The first full window of tracked frames is the value, eased to from 65 mm, logged once.
    f = Run(s, f, FrameXr::Ipd::kWindow / 120.0, [](int) { return 0.0634f; });
    REQUIRE(s.Known());
    REQUIRE(s.Target() == Approx(0.0634f));
    f = Run(s, f, 0.5, [](int) { return 0.0634f; });
    REQUIRE(s.Value() == Approx(0.0634f));
    REQUIRE(s.TakeLog(logged));
    REQUIRE(logged == Approx(0.0634f));
    REQUIRE_FALSE(s.TakeLog(logged));
}

TEST_CASE("FrameXr ipd: a steady IPD with jitter holds still", "[frame_xr]") {
    Smoother s;
    float logged = 0.0f;
    int f = Run(s, 0, 1.5, [](int) { return 0.0634f; }); // past the close following at start
    REQUIRE(s.TakeLog(logged));
    const float settled = s.Value();
    // Ten seconds of noise of up to +-0.4 mm a frame, alternating +-0.25 mm, and a few wild
    // frames: the value never moves.
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> noise(-0.0004f, 0.0004f);
    int moved = 0;
    f = Run(
        s, f, 10.0,
        [&](int i) {
            if (i % 97 == 0) {
                return 0.080f; // a one-off outlier inside the plausible range
            }
            return 0.0634f + noise(rng) + ((i & 1) ? 0.00025f : -0.00025f);
        },
        [&](int, float v) { moved += v != settled; });
    REQUIRE(moved == 0);
    REQUIRE_FALSE(s.TakeLog(logged));
    // Jitter that alternates frame by frame (as MOCK_XR_IPD_JITTER makes) averages out: the
    // value is the middle, not one side.
    Smoother alt;
    Run(alt, 0, 2.0, [](int i) { return (i & 1) ? 0.0632f : 0.0628f; });
    CHECK(alt.Value() == Approx(0.063f).margin(1e-6));
    // Short tracking losses with nonsense in them change nothing either.
    f = Run(s, f, 0.3, [](int) { return 0.050f; }, false);
    f = Run(s, f, 1.0, [](int) { return 0.0634f; });
    REQUIRE(s.Value() == settled);
    REQUIRE_FALSE(s.TakeLog(logged));
}

TEST_CASE("FrameXr ipd: a dial turn mid-run settles within half a second, without a snap",
          "[frame_xr]") {
    // The owner turns the dial from 63 to 68 mm at 20 s, during play.
    Smoother s;
    float logged = 0.0f;
    int f = Run(s, 0, 20.0, [](int) { return 0.063f; });
    REQUIRE(s.Value() == Approx(0.063f));
    REQUIRE(s.TakeLog(logged));
    REQUIRE(logged == Approx(0.063f));
    const int step = f;
    float last = s.Value(), max_step = 0.0f;
    int settled_at = -1, moving_frames = 0;
    f = Run(
        s, f, 2.0, [](int) { return 0.068f; },
        [&](int i, float v) {
            REQUIRE(v >= last); // straight there, no overshoot or back
            REQUIRE(v <= 0.068f + 1e-6f);
            max_step = std::max(max_step, v - last);
            moving_frames += v != last;
            last = v;
            if (settled_at < 0 && std::abs(v - 0.068f) < 0.0001f) {
                settled_at = i;
            }
        });
    REQUIRE(settled_at >= 0);
    const double settle_s = (settled_at - step) / 120.0;
    INFO("settled in " << settle_s << " s, largest step " << max_step * 1000.0f << " mm");
    CHECK(settle_s < 0.5);
    // Eased over several frames: no frame takes more than half the 5 mm change.
    CHECK(max_step < 0.0025f);
    CHECK(moving_frames >= 8);
    REQUIRE(s.TakeLog(logged));
    REQUIRE(logged == Approx(0.068f));
    REQUIRE_FALSE(s.TakeLog(logged));
    // A turn back follows the same way.
    f = Run(s, f, 1.0, [](int) { return 0.0625f; });
    REQUIRE(s.Value() == Approx(0.0625f).margin(1e-5));
    REQUIRE(s.TakeLog(logged));
    REQUIRE(logged == Approx(0.0625f));
}

TEST_CASE("FrameXr ipd: the ease takes as long at 90 Hz, and a slow turn is followed",
          "[frame_xr]") {
    Smoother s;
    int64_t t = 0;
    for (int i = 0; i < 90; i++, t += 11'111'111) {
        s.Add(t, 0.063f, true);
    }
    const int64_t step = t;
    int64_t done = -1;
    for (int i = 0; i < 90; i++, t += 11'111'111) {
        if (std::abs(s.Add(t, 0.068f, true) - 0.068f) < 0.0001f && done < 0) {
            done = t;
        }
    }
    REQUIRE(done > 0);
    CHECK((done - step) / 1e9 < 0.5);

    // The dial turned slowly over two seconds (63 to 67 mm): the value follows during the turn,
    // and small changes under 0.5 mm are not logged one by one.
    Smoother slow;
    float logged = 0.0f;
    int f = Run(slow, 0, 1.0, [](int) { return 0.063f; });
    REQUIRE(slow.TakeLog(logged));
    int logs = 0;
    float mid_turn = 0.0f;
    f = Run(
        slow, f, 2.0, [&](int i) { return 0.063f + 0.004f * (i - 120) / 240.0f; },
        [&](int i, float v) {
            logs += slow.TakeLog(logged);
            if (i == 240) {
                mid_turn = v;
            }
        });
    f = Run(slow, f, 1.0, [](int) { return 0.067f; });
    logs += slow.TakeLog(logged);
    // The end of the turn is reached exactly (the close following after a change), then held.
    CHECK(mid_turn > 0.0642f); // half way through the turn it is already on its way
    CHECK(slow.Value() == Approx(0.067f).margin(0.00005));
    CHECK(logs >= 3);
    CHECK(logs <= 10);
}

namespace {
struct FakeRasterizer final : VideoCore::RasterizerInterface {
    void AddTriangle(const Pica::OutputVertex&, const Pica::OutputVertex&,
                     const Pica::OutputVertex&) override {}
    void DrawTriangles() override {}
    void FlushAll() override {}
    void FlushRegion(PAddr, u32) override {}
    void InvalidateRegion(PAddr, u32) override {}
    void FlushAndInvalidateRegion(PAddr, u32) override {}
    void ClearAll(bool) override {}
    void SetVRData(const int32_t& mode_, const float&, int, const float& scaler,
                   const float*) override {
        mode = mode_;
        game_pos_scaler = scaler;
    }
    void SetVRIpd(float ipd_m) override {
        ipd = ipd_m;
        ipd_calls++;
    }
    int mode = 0;
    float game_pos_scaler = 0.0f;
    float ipd = 0.0f;
    int ipd_calls = 0;
};
} // namespace

TEST_CASE("FrameXr ipd: the rasterizer gets the measured IPD", "[frame_xr]") {
    using namespace VideoCore::Immersive;
    // What the XR thread publishes each frame: the smoother's value.
    Smoother s;
    Run(s, 0, 1.0, [](int) { return 0.0612f; });
    VRData d;
    REQUIRE(d.ipd == FrameXr::Ipd::kFallbackM); // nothing measured yet: 65 mm
    d.mode = 3;
    d.game_pos_scaler = -10.0f;
    d.ipd = s.Value();
    Publish(d);
    FakeRasterizer r;
    LatchFrame(&r); // the emulation thread's hand-off at a game frame's end
    REQUIRE(r.ipd_calls == 1);
    REQUIRE(r.mode == 3);
    REQUIRE(r.ipd == Approx(0.0612f));
    // The game's eyes: the eye scale x half the measured IPD each way.
    CHECK(GameEyeOffset(r.game_pos_scaler, r.ipd, true) == Approx(10.0f * 0.0306f));
    CHECK(GameEyeOffset(r.game_pos_scaler, r.ipd, false) == Approx(-10.0f * 0.0306f));
    // A dial change reaches it at the next latch.
    d.ipd = 0.0671f;
    Publish(d);
    LatchFrame(&r);
    REQUIRE(r.ipd == Approx(0.0671f));
    // The rasterizer keeps 65 mm for anything that is no IPD.
    CHECK(UsableIpd(0.0671f, 0.065f) == 0.0671f);
    CHECK(UsableIpd(0.0f, 0.065f) == 0.065f);
    CHECK(UsableIpd(NAN, 0.065f) == 0.065f);
    CHECK(UsableIpd(0.5f, 0.065f) == 0.065f);
}

TEST_CASE("FrameXr ipd: the saved IPD is the fallback at start", "[frame_xr]") {
    using FrameXr::Ipd::SavedM;
    CHECK(SavedM(0.0f) == 0.0f); // never saved
    CHECK(SavedM(63.4f) == Approx(0.0634f));
    CHECK(SavedM(200.0f) == 0.0f);
    CHECK(SavedM(NAN) == 0.0f);
    Smoother s;
    s.SetFallback(SavedM(63.4f));
    CHECK(s.Value() == Approx(0.0634f)); // before anything is read
    CHECK_FALSE(s.Known());
    Run(s, 0, 2.0, [](int) { return 0.0f; }, false); // no eyes: still the saved value
    CHECK(s.Value() == Approx(0.0634f));
    Run(s, 240, 1.0, [](int) { return 0.0660f; }); // the headset's replaces it
    CHECK(s.Value() == Approx(0.0660f));
    s.SetFallback(0.060f); // ignored once known
    CHECK(s.Value() == Approx(0.0660f));
    Smoother none;
    none.SetFallback(SavedM(0.0f)); // nothing saved: 65 mm
    CHECK(none.Value() == FrameXr::Ipd::kFallbackM);
}

TEST_CASE("FrameXr ipd: the toasts, only the first time ever", "[frame_xr]") {
    using FrameXr::Ipd::Notice;
    const auto toasts = [](Smoother& s, Notice& n, bool saved, int from, double secs,
                           float ipd, bool valid, std::vector<std::string>& out) {
        return Run(
            s, from, secs, [&](int) { return ipd; },
            [&](int f, float) {
                const std::string t = n.Update(f * kPeriodNs, s, saved);
                if (!t.empty()) {
                    out.push_back(t + " @" + std::to_string(f));
                }
            },
            valid);
    };
    SECTION("read normally: nothing") {
        Smoother s;
        Notice n;
        std::vector<std::string> shown;
        int f = toasts(s, n, false, 0, 30.0, 0.0634f, true, shown);
        // Tracking lost for a while later on: still nothing.
        f = toasts(s, n, false, f, 20.0, 0.0f, false, shown);
        CHECK(shown.empty());
    }
    SECTION("read late, first time: the warning at 10 s, then the value once") {
        Smoother s;
        Notice n;
        std::vector<std::string> shown;
        int f = toasts(s, n, false, 0, 15.0, 0.0f, false, shown);
        REQUIRE(shown.size() == 1);
        // 10 s after the first frame (frame 1201 at 120 Hz).
        CHECK(shown[0] == std::string(FrameXr::Ipd::kNotReadText) + " @1201");
        CHECK(s.Value() == FrameXr::Ipd::kFallbackM); // 65 mm meanwhile
        f = toasts(s, n, false, f, 5.0, 0.0634f, true, shown);
        f = toasts(s, n, false, f, 5.0, 0.0680f, true, shown); // the dial: no more toasts
        REQUIRE(shown.size() == 2);
        CHECK(shown[1].rfind("IPD Set: 63 mm @", 0) == 0);
    }
    SECTION("an IPD saved before: never a toast") {
        Smoother s;
        s.SetFallback(0.0634f);
        Notice n;
        std::vector<std::string> shown;
        int f = toasts(s, n, true, 0, 30.0, 0.0f, false, shown);
        f = toasts(s, n, true, f, 5.0, 0.0634f, true, shown);
        CHECK(shown.empty());
    }
}

TEST_CASE("FrameXr ipd: the IPD is saved when it changes, not every frame", "[frame_xr]") {
    using FrameXr::Ipd::Saver;
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> noise(-0.0004f, 0.0004f);
    const auto saves = [&](Smoother& s, Saver& v, int& f, double secs, float ipd,
                           std::vector<float>& out) {
        f = Run(
            s, f, secs, [&](int) { return ipd + noise(rng); },
            [&](int, float) {
                float m = 0.0f;
                if (v.Want(s, m)) {
                    out.push_back(m);
                }
            });
    };
    SECTION("first run: saved once when read, then on each dial change over 0.5 mm") {
        Smoother s;
        Saver v; // nothing saved
        std::vector<float> saved;
        int f = 0;
        saves(s, v, f, 20.0, 0.0634f, saved); // 2400 frames of jitter: one write
        REQUIRE(saved.size() == 1);
        CHECK(saved[0] == Approx(0.0634f).margin(0.0003));
        saves(s, v, f, 5.0, 0.0636f, saved); // 0.2 mm: not saved
        CHECK(saved.size() == 1);
        saves(s, v, f, 5.0, 0.0680f, saved); // the dial
        REQUIRE(saved.size() == 2);
        CHECK(saved[1] == Approx(0.0680f).margin(0.0003));
    }
    SECTION("a later run reading the saved IPD again writes nothing") {
        Smoother s;
        s.SetFallback(0.0634f);
        Saver v(0.0634f);
        std::vector<float> saved;
        int f = 0;
        saves(s, v, f, 20.0, 0.0634f, saved);
        CHECK(saved.empty());
    }
}
