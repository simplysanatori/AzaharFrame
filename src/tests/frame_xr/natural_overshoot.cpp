// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <random>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/immersive_layout.h"
#include "video_core/vr_view_matrix.h"

// AzaharFrame overshoot: mode 2's natural slider with the game's far scenery as it really moves
// (frame_xr/immersive_layout.h FarMeasureHold, NaturalSlider, ParallelDistance). Mock logs of
// lane natural2 (f0bd422e0): Ocarina of Time 3D on a screen at 0.30 m went up to 0.25 degrees
// past parallel in steady play (its far point moves about 2 % by scene, the limit was made from
// each measure with a 0.05 degree margin) and the limit hunted several times a second; Majora's
// Mask 3D went 0.39 degrees past parallel for a moment after World Scale 25 -> 400 % (the screen
// moved on while the game still drew the slider of before).
//
// The whole path is simulated as Immersive runs it: the game draws each frame with the slider
// it was given a few frames before (its far scenery: its far width per slider unit, which
// moves with the scene, times that slider); FarPerSlider measures it (video_core, as the
// emulation thread does); the XR thread holds the measure (FarMeasureHold), makes the limit for
// the screen as it is now and where World Scale is taking it (StereoSliderLimit), sends 3D
// Depth x that (NaturalSlider), and moves the screen no farther than the frames being drawn
// allow (ParallelDistance on the ramp). What the eyes see is checked on every displayed frame.
using namespace FrameXr::Immersive::Layout;
namespace VI = VideoCore::Immersive;

namespace {

constexpr double kRadToDeg = 57.29577951308232;

HeadLocked Screen(float factor, float tan_half_y, float aspect, float distance) {
    HeadLocked s;
    s.factor = factor;
    s.factor_y = factor;
    s.tan_half_y = tan_half_y;
    s.aspect = aspect;
    s.distance = distance;
    return s;
}

// The game's far width per slider unit over time: a base, scene steps, a slow wander and a
// frame-to-frame jitter (all relative).
struct FarSeries {
    enum Kind { kSteady, kJitter, kWander, kSteps };
    Kind kind = kSteady;
    float base = 0.1f;
    float amount = 0.0f; ///< jitter or wander: +-amount; steps: up to +-amount each
    uint32_t seed = 1;

    struct State {
        std::mt19937 rng;
        float wander_from = 1.0f, wander_to = 1.0f;
        int64_t wander_t0 = 0, wander_t1 = 0;
        float scene = 1.0f;
        int64_t next_step = 0;
    };
    State Start() const {
        State s{std::mt19937(seed)};
        return s;
    }
    float At(State& s, int64_t t_ms) const {
        auto uni = [&](float a, float b) {
            return std::uniform_real_distribution<float>(a, b)(s.rng);
        };
        switch (kind) {
        case kSteady:
            return base;
        case kJitter:
            return base * (1.0f + uni(-amount, amount));
        case kWander: {
            // A new point within +-amount every 0.5 to 2 s, linear in between (a camera
            // turning, the game's convergence following its target).
            if (t_ms >= s.wander_t1) {
                s.wander_from = s.wander_to;
                s.wander_to = 1.0f + uni(-amount, amount);
                s.wander_t0 = t_ms;
                s.wander_t1 = t_ms + static_cast<int64_t>(uni(500.0f, 2000.0f));
            }
            const float a = static_cast<float>(t_ms - s.wander_t0) /
                            static_cast<float>(s.wander_t1 - s.wander_t0);
            return base * (s.wander_from + (s.wander_to - s.wander_from) * a) *
                   (1.0f + uni(-0.005f, 0.005f));
        }
        case kSteps:
            // A new scene every 2 to 6 s, up or down by up to `amount` (within 0.7 to 1.4 of the
            // base), with 1 % jitter.
            if (t_ms >= s.next_step) {
                if (s.next_step > 0) {
                    s.scene = std::clamp(s.scene * (1.0f + uni(-amount, amount)), 0.7f, 1.4f);
                }
                s.next_step = t_ms + static_cast<int64_t>(uni(2000.0f, 6000.0f));
            }
            return base * s.scene * (1.0f + uni(-0.01f, 0.01f));
        }
        return base;
    }
};

struct Setup {
    float factor = 1.4f, tan_half_y = 0.8f, aspect = 1.67f;
    float h = 0.032f;                             ///< half the IPD (m)
    float f = 1.0f;                               ///< 3D Depth / 100
    float world_from = 100.0f, world_to = 100.0f; ///< World Scale (%), the step at kStepMs
    int game_ms = 33;                             ///< the game's frame time
    int lag = 2;      ///< the game's frames between a slider given and drawn with it
    bool old = false; ///< natural2 as it was: each measure, 0.05 degrees, no ceiling
    FarSeries far;
};

struct Result {
    double worst_deg = 1e9;         ///< the eyes inwards at the far scenery, worst displayed frame
    double worst_second_deg = 1e9;  ///< the median of each second's frames, worst (the log's)
    double worst_step_deg = 1e9;    ///< worst within 1 s after the World Scale step
    double depth_lost = 0.0;        ///< 1 - slider sent / the natural slider of the far scenery's
                                    ///< average, over the steady part
    double depth_lost_vs_max = 0.0; ///< 1 - slider sent / the natural slider of its highest
    int64_t longest_past_ms = 0;    ///< the longest the far scenery stayed past parallel
    int reversals = 0;              ///< the slider sent going up after down or down after up
    int changes = 0;                ///< changes of the slider sent, over the steady part
};

/// Past parallel: the eyes diverging by more than this (0.6 arc minutes; the code's own
/// kDivergeToleranceDeg, not counted or warned about, is 0.1 degrees).
constexpr double kPastDeg = 0.01;
constexpr int64_t kStart = 10000;
constexpr int64_t kSettleMs = 4000; ///< measured and held by then
constexpr int64_t kStepMs = 14000;  ///< World Scale's step (when there is one)
constexpr int64_t kEndMs = 22000;

Result Simulate(const Setup& u) {
    Result r;
    VI::FarPerSlider measure;
    FarMeasureHold hold;
    NaturalSlider natural;
    RatioRamp ramp{1.3862944f}; // a factor 4 a second, as Immersive's
    FarSeries::State fs = u.far.Start();
    std::deque<float> given; // the slider given to the game at each of its frames
    float drawn_far = -1.0f; // the frame on view
    float sent = -1.0f;
    float last_sent = -1.0f;
    int dir = 0;
    std::vector<double> second;
    double sum_sent = 0.0, sum_mean = 0.0, sum_max = 0.0;
    int steady_frames = 0;
    double per_sum = 0.0, per_max = 0.0;
    int per_count = 0;
    std::vector<float> step_per;
    int64_t next_game = kStart;
    int64_t past_since = -1;
    int64_t second_start = kStart + kSettleMs;
    const bool step = u.world_to != u.world_from;
    for (int64_t t = kStart; t < kStart + kEndMs; t += 8) {
        const int64_t rel = t - kStart;
        const float world = step && rel >= kStepMs ? u.world_to : u.world_from;
        const float target = FullDistance(world, 1.0f, true);
        const HeadLocked angle = Screen(u.factor, u.tan_half_y, u.aspect, 1.0f);
        const float ceiling =
            u.old ? std::numeric_limits<float>::infinity()
                  : ParallelDistance(angle, u.h, measure.Drawn() * (1.0f + hold.Margin()));
        const float d = ramp.Update(target, t, ceiling);
        const HeadLocked now = Screen(u.factor, u.tan_half_y, u.aspect, d);
        HeadLocked going = now;
        going.distance = std::max(d, target);
        // The game: a new frame drawn with the slider given `lag` frames ago.
        if (t >= next_game) {
            next_game += u.game_ms;
            const float per = u.far.At(fs, rel);
            given.push_back(std::max(sent, 0.0f));
            if (static_cast<int>(given.size()) > u.lag + 1) {
                given.pop_front();
            }
            const float drawn_slider = given.front();
            drawn_far = per * std::min(drawn_slider, 1.0f);
            // The emulation thread's measure, with the slider given now.
            measure.Swap(drawn_slider > 0.0f ? 2 : 1, drawn_far, given.back());
            if (rel >= kSettleMs && rel < (step ? kStepMs : kEndMs)) {
                per_sum += per;
                per_max = std::max<double>(per_max, per);
                ++per_count;
            }
        }
        // The XR thread.
        const float m = measure.Value();
        float limit;
        if (u.old) {
            limit = std::min(StereoSliderLimit(now, u.h, m), StereoSliderLimit(going, u.h, m));
        } else {
            const float held = hold.Update(measure.Recent(), measure.Taken(), t);
            limit = std::min(StereoSliderLimit(now, u.h, held, kNaturalMarginDeg),
                             StereoSliderLimit(going, u.h, held, kNaturalMarginDeg));
        }
        bool no_higher = false;
        if (!u.old) {
            const float drawn = measure.Drawn();
            no_higher = drawn > 0.0f && StereoFarOnQuad(now, 2, u.h, drawn).vergence_deg <= 0.0f;
        }
        sent = natural.Update(u.f, limit, t, no_higher);
        // What the eyes see.
        if (drawn_far >= 0.0f && rel >= kSettleMs) {
            const StereoFar st = StereoFarOnQuad(now, 2, u.h, drawn_far);
            r.worst_deg = std::min<double>(r.worst_deg, st.vergence_deg);
            if (st.vergence_deg < -kPastDeg) {
                past_since = past_since < 0 ? t : past_since;
                r.longest_past_ms = std::max(r.longest_past_ms, t + 8 - past_since);
            } else {
                past_since = -1;
            }
            second.push_back(st.vergence_deg);
            if (step && rel >= kStepMs && rel < kStepMs + 1000) {
                r.worst_step_deg = std::min<double>(r.worst_step_deg, st.vergence_deg);
            }
            if (t - second_start >= 1000) {
                std::nth_element(second.begin(), second.begin() + second.size() / 2, second.end());
                r.worst_second_deg = std::min(r.worst_second_deg, second[second.size() / 2]);
                second.clear();
                second_start = t;
            }
        }
        if (rel >= kSettleMs && rel < (step ? kStepMs : kEndMs)) {
            sum_sent += sent;
            ++steady_frames;
            if (last_sent >= 0.0f && std::abs(sent - last_sent) > 1e-7f) {
                ++r.changes;
                const int now_dir = sent > last_sent ? 1 : -1;
                if (dir != 0 && now_dir != dir) {
                    ++r.reversals;
                }
                dir = now_dir;
            }
            last_sent = sent;
        }
    }
    if (steady_frames > 0 && per_count > 0) {
        const HeadLocked s =
            Screen(u.factor, u.tan_half_y, u.aspect, FullDistance(u.world_from, 1.0f, true));
        const double mean_nat = std::min(
            1.0, static_cast<double>(
                     u.f * StereoSliderLimit(s, u.h, static_cast<float>(per_sum / per_count),
                                             kNaturalMarginDeg)));
        const double max_nat = std::min(
            1.0, static_cast<double>(u.f * StereoSliderLimit(s, u.h, static_cast<float>(per_max),
                                                             kNaturalMarginDeg)));
        const double avg = sum_sent / steady_frames;
        r.depth_lost = mean_nat > 0.0 ? 1.0 - avg / mean_nat : 0.0;
        r.depth_lost_vs_max = max_nat > 0.0 ? 1.0 - avg / max_nat : 0.0;
    }
    return r;
}

Setup RandomSetup(std::mt19937& rng) {
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    Setup u;
    u.factor = uni(1.0f, 1.5f);
    u.tan_half_y = uni(0.5f, 0.9f);
    u.aspect = uni(1.5f, 1.8f);
    u.h = uni(0.0275f, 0.0375f); // IPD 55 to 75 mm
    u.f = rng() % 3 == 0 ? 1.0f : uni(0.2f, 1.0f);
    u.world_from = u.world_to = std::exp(uni(std::log(10.0f), std::log(400.0f)));
    u.game_ms = rng() % 2 ? 33 : 17;
    u.lag = 1 + static_cast<int>(rng() % 3);
    u.far.base = std::exp(uni(std::log(0.02f), std::log(0.4f)));
    u.far.seed = rng();
    return u;
}

} // namespace

TEST_CASE("Overshoot: the held measure, steady and noisy", "[frame_xr][natural][overshoot]") {
    FarMeasureHold h;
    int64_t t = 1000;
    uint32_t taken = 0;
    const float floor = 0.1f * (1.0f + FarMeasureHold::kMarginFloor);
    CHECK(h.Update(-1.0f, taken, t) < 0.0f);
    // The first measure at once, with the floor margin only.
    CHECK(h.Update(0.1f, ++taken, t += 8) == Catch::Approx(floor));
    // Steady: nothing moves, ever.
    for (int i = 0; i < 2000; ++i) {
        REQUIRE(h.Update(0.1f, ++taken, t += 8) == Catch::Approx(floor));
    }
    // A higher measure counts at once (the limit lower at once).
    CHECK(h.Update(0.11f, ++taken, t += 8) >= 0.11f);
    // A lower one waits: held while the higher one is in the window...
    const float high = h.Value();
    for (int i = 0; i < 200; ++i) {
        REQUIRE(h.Update(0.1f, ++taken, t += 8) == high);
    }
    // ...also while the measure waits (no new sample: the slider moving), however long...
    for (int i = 0; i < 2000; ++i) {
        REQUIRE(h.Update(0.1f, taken, t += 8) == high);
    }
    // ...and goes back down once measured lower for the window (the slider eases up).
    for (int i = 0; i < 2000; ++i) {
        h.Update(0.1f, ++taken, t += 8);
    }
    CHECK(h.Value() < high);
    // The jump measured counts as noise: a margin for as long as it fades.
    CHECK(h.Value() > floor);
    for (int i = 0; i < 20000; ++i) {
        h.Update(0.1f, ++taken, t += 8);
    }
    CHECK(h.Value() == Catch::Approx(floor).epsilon(NaturalSlider::kRise));
    // Unknown for a while (a menu): the last holds.
    const float last = h.Value();
    for (int i = 0; i < 2000; ++i) {
        REQUIRE(h.Update(-1.0f, ++taken, t += 8) == last);
    }
    // A noisy measure gets a margin from its own noise, never more than kMarginMax, and stops
    // moving once the noise is known.
    FarMeasureHold n;
    std::mt19937 rng(5);
    std::vector<float> in_use;
    for (int i = 0; i < 4000; ++i) {
        in_use.push_back(
            n.Update(0.1f * (1.0f + std::uniform_real_distribution<float>(-0.03f, 0.03f)(rng)),
                     ++taken, t += 8));
    }
    CHECK(n.Margin() <= FarMeasureHold::kMarginMax);
    CHECK(n.Value() >= 0.103f);
    CHECK(std::count_if(in_use.begin() + 1000, in_use.end() - 1,
                        [&](const float& v) { return (&v)[1] != v; }) <= 1);
}

TEST_CASE("Overshoot: a screen no farther than the game's far scenery allows",
          "[frame_xr][natural][overshoot]") {
    std::mt19937 rng(31);
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    for (int i = 0; i < 500; ++i) {
        const float h = uni(0.0225f, 0.0425f);
        const float far = uni(0.001f, 0.2f);
        HeadLocked s = Screen(uni(1.0f, 1.5f), uni(0.4f, 1.0f), uni(1.3f, 2.0f), 1.0f);
        s.distance = ParallelDistance(s, h, far);
        CHECK(std::abs(StereoFarOnQuad(s, 2, h, far).vergence_deg) < 1e-3f);
        s.distance *= 0.99f;
        CHECK(StereoFarOnQuad(s, 2, h, far).vergence_deg > 0.0f);
    }
    CHECK(std::isinf(ParallelDistance(Screen(1.4f, 0.8f, 1.67f, 3.0f), 0.032f, 0.0f)));
    CHECK(std::isinf(ParallelDistance(Screen(1.4f, 0.8f, 1.67f, 3.0f), 0.032f, -1.0f)));
    // The ramp waits at its ceiling (or where it is, when that is higher), then goes on eased.
    RatioRamp d{1.3862944f};
    int64_t t = 0;
    CHECK(d.Update(1.0f, t) == 1.0f);
    for (int k = 0; k < 100; ++k) {
        REQUIRE(d.Update(12.0f, t += 8, 0.5f) == 1.0f);
    }
    float prev = d.Value();
    for (int k = 0; k < 100; ++k) {
        const float v = d.Update(12.0f, t += 8, 2.0f);
        REQUIRE(v <= 2.0f);
        REQUIRE(v >= prev);
        REQUIRE(v <= prev * std::exp(1.3862944f * 0.008f) + 1e-5f);
        prev = v;
    }
    CHECK(prev == Catch::Approx(2.0f));
    // Nearer is never held.
    CHECK(d.Update(0.3f, t += 8, 0.1f) < 2.0f);
}

// The sweep: screen distance 0.3 to 12 m (World Scale 10 to 400 %), screens, IPDs, 3D Depth,
// game frame times and lags, and the game's far scenery steady, jittering, wandering or
// stepping by scene. A steady or jittering game: never past parallel, no hunting, little
// depth lost. A game whose far scenery moves on (wander, a new scene): past parallel only for
// the frames the game is ahead of any measure (it draws the new scene before it can be read),
// never as the eyes see it over a second, and the slider moves only as the game does.
TEST_CASE("Overshoot: any settings, the far scenery never past parallel",
          "[frame_xr][natural][overshoot]") {
    std::mt19937 rng(2026);
    struct Worst {
        double deg = 1e9, second = 1e9, lost = 0.0, lost_vs_max = 0.0, lost_sum = 0.0;
        double lost_vs_max_sum = 0.0;
        int64_t longest_past_ms = 0;
        int reversals = 0, runs = 0;
        void Add(const Result& r) {
            deg = std::min(deg, r.worst_deg);
            second = std::min(second, r.worst_second_deg);
            lost = std::max(lost, r.depth_lost);
            lost_vs_max = std::max(lost_vs_max, r.depth_lost_vs_max);
            lost_sum += r.depth_lost;
            lost_vs_max_sum += r.depth_lost_vs_max;
            longest_past_ms = std::max(longest_past_ms, r.longest_past_ms);
            reversals = std::max(reversals, r.reversals);
            ++runs;
        }
    };
    Worst worst[4], old_worst[4];
    const FarSeries::Kind kinds[] = {FarSeries::kSteady, FarSeries::kJitter, FarSeries::kWander,
                                     FarSeries::kSteps};
    for (int run = 0; run < 240; ++run) {
        Setup u = RandomSetup(rng);
        const int k = run % 4;
        u.far.kind = kinds[k];
        u.far.amount = k == 3 ? std::uniform_real_distribution<float>(0.05f, 0.15f)(rng)
                              : std::uniform_real_distribution<float>(0.02f, 0.05f)(rng);
        const Result r = Simulate(u);
        Setup o = u;
        o.old = true;
        const Result ro = Simulate(o);
        INFO("run " << run << " kind " << k << " amount " << u.far.amount << " world "
                    << u.world_from << " % f " << u.f << " h " << u.h << " per " << u.far.base
                    << " game " << u.game_ms << " ms lag " << u.lag);
        worst[k].Add(r);
        old_worst[k].Add(ro);
        // As the eyes see it over a second: never past parallel, whatever the game does.
        CHECK(r.worst_second_deg >= -kPastDeg);
        if (k == 0) {
            // A steady game: the slider set once, never moved again; exactly the floor margin.
            CHECK(r.worst_deg >= -1e-3);
            CHECK(r.changes == 0);
            CHECK(r.depth_lost <= FarMeasureHold::kMarginFloor * 1.01 + 1e-4);
        } else if (k == 1) {
            // Jitter 2 to 5 % frame to frame around a steady value: no hunting; past parallel
            // only for a frame now and then (a frame's own jitter beyond those measured before
            // it) and by hundredths of a degree; a few per cent of depth below the natural
            // slider of the far scenery's highest (the most any slider that never goes past
            // parallel could keep).
            CHECK(r.reversals <= 1);
            CHECK(r.worst_deg >= -0.05);
            CHECK(r.longest_past_ms <= u.game_ms + 16);
            CHECK(r.depth_lost_vs_max <= 0.05);
        } else {
            // Wandering or stepping: past parallel no longer than the game is ahead of any
            // measure (the frames it draws with the slider of before, the kStableSwaps the
            // measure waits after the slider moved, the kRecent it is made of, and the XR frame
            // after); the slider turns at most once a second (the game's own moves).
            const int64_t react_ms =
                (u.lag + VI::FarPerSlider::kStableSwaps + VI::FarPerSlider::kRecent + 1) *
                    u.game_ms +
                16;
            CHECK(r.longest_past_ms <= react_ms);
            CHECK(r.reversals <= 10);
            CHECK(r.depth_lost_vs_max <= 0.05);
        }
    }
    const char* names[] = {"steady", "jitter 2-5 %", "wander 2-5 %", "scene steps 5-15 %"};
    for (int k = 0; k < 4; ++k) {
        const Worst& w = worst[k];
        const Worst& o = old_worst[k];
        std::printf("overshoot sweep, %-18s: worst %+.3f deg, longest past %lld ms, worst second "
                    "%+.3f deg; depth lost mean %.2f %% (worst %.2f %%), vs its highest mean "
                    "%.2f %% (worst %.2f %%); reversals <= %d | natural2: worst %+.3f deg, longest "
                    "past %lld ms, worst second %+.3f deg, lost mean %.2f %%, reversals <= %d\n",
                    names[k], w.deg, static_cast<long long>(w.longest_past_ms), w.second,
                    100.0 * w.lost_sum / w.runs, 100.0 * w.lost, 100.0 * w.lost_vs_max_sum / w.runs,
                    100.0 * w.lost_vs_max, w.reversals, o.deg,
                    static_cast<long long>(o.longest_past_ms), o.second,
                    100.0 * o.lost_sum / o.runs, o.reversals);
    }
    // natural2's own rule does go past parallel and hunt with the same games.
    CHECK(old_worst[1].deg < -0.05);
    CHECK(old_worst[1].reversals > 10);
}

// World Scale steps up and down (the screen 0.3 to 12 m), the game a few frames behind: the
// screen waits until the game draws with the lower slider, so not even one frame goes past
// parallel. natural2 (the limit at the distance it is going to, no ceiling) did, within the lag.
TEST_CASE("Overshoot: World Scale steps, not even within the game's lag",
          "[frame_xr][natural][overshoot]") {
    std::mt19937 rng(4004);
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    double worst = 1e9, old_worst = 1e9;
    int old_diverged = 0;
    for (int run = 0; run < 120; ++run) {
        Setup u = RandomSetup(rng);
        const bool up = run % 3 != 0;
        const float a = std::exp(uni(std::log(10.0f), std::log(60.0f)));
        const float b = std::exp(uni(std::log(150.0f), std::log(400.0f)));
        u.world_from = up ? a : b;
        u.world_to = up ? b : a;
        u.far.kind = run % 2 ? FarSeries::kJitter : FarSeries::kSteady;
        u.far.amount = 0.03f;
        const Result r = Simulate(u);
        Setup o = u;
        o.old = true;
        const Result ro = Simulate(o);
        INFO("run " << run << " World Scale " << u.world_from << " -> " << u.world_to << " lag "
                    << u.lag << " game " << u.game_ms << " ms");
        // As in the sweep (steady or jittering): never past parallel but for a frame's own
        // jitter now and then, by hundredths of a degree; and not even within the game's lag at
        // the step.
        CHECK(r.worst_deg >= -0.05);
        CHECK(r.longest_past_ms <= u.game_ms + 16);
        worst = std::min(worst, r.worst_deg);
        old_worst = std::min(old_worst, ro.worst_step_deg);
        old_diverged += ro.worst_step_deg < -0.1;
    }
    std::printf("overshoot World Scale steps: worst %+.3f deg | natural2 within the lag: worst "
                "%+.3f deg, %d of 120 runs past 0.1 deg\n",
                worst, old_worst, old_diverged);
    CHECK(old_diverged > 10);
}
