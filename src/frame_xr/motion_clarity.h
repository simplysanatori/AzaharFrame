// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash: Motion Clarity. AzaharFrame fixa: two settings, each read alone, per game and
// global: vr_motion_clarity_immersive (the Immersive section's row) in the Full modes, and
// vr_motion_clarity_normal (the Graphics section's row) in the Off and Wide modes. Only the 3DS's
// top screen ever flashes. The key of the development builds, vr_motion_clarity, is no longer read.
//
// In the Full immersive modes the game draws each top screen picture for the head pose of its
// moment. Shown on a second display refresh it is a picture of the past held in front of a head
// that has moved on, and no hold (rigid, or warped as the game would draw it now) makes it line
// up again: the owner's headset A/B of 2026-11-14 (handoff reports 51 and 54) found doubling or
// jitter with every hold, and none at all with each picture on one refresh only (the test switch
// AZAHARFRAME_XR_SINGLE_FLASH). This is that switch made a display mode:
//
//   - each new game picture is shown on the first refresh it is on view; on the refreshes after,
//     an opaque black quad of the screen's exact pose, size and shape (the backing, per eye)
//     takes the screen's place in the layer list, so nothing the compositor has behind the
//     app (its background, an earlier frame's panels) shows through, the layer list keeps its
//     shape from refresh to refresh, and the controllers and beams go over it as over the
//     screen;
//   - only at the game's full rate: the scheduler measures, from the pictures it sees, how many
//     refreshes each one is on view. At 60 pictures a second on 120 Hz that is 2 (lit 1 in 2).
//     A 30 a second game (4 refreshes each) would be lit 1 refresh in 4: 25 % of the light,
//     flickering at 30 Hz, so it is held as before (Slow::Hold). AZAHARFRAME_XR_FLASH_SLOW=twice
//     shows it on 2 refreshes of 4 instead (the second warped, as any held picture), for an A/B;
//   - flash2: the same share of light in every other state of a Full mode, so the brightness
//     never steps (the owner's headset test of 2026-11-14 saw 50 % -> 100 % lit for 1-9 s each
//     time the VR menu opened, report 64): with the VR menu, the game's keyboard, a pause or the
//     bottom screen in focus (immersive is paused then and the top screen is not head-driven; it
//     is flashed where it is, under the menu's own quads), while the rate is learnt, and for a
//     picture late past its time (a dropped game frame, a stall, a loading screen), the newest
//     picture is lit on a fixed beat at the game's rate (every other refresh at 60 pictures on
//     120 Hz; Scheduler::Beat), never on two refreshes in a row: a still picture flashed at
//     60 Hz looks as bright as one in play and cannot double. A late picture is shown again one
//     refresh after the next was due, as before, and then on the beat, so the screen is never
//     dark for longer than one picture's time; a new picture due before the beat waits for it;
//   - held (as without it) for a game under the full rate (above: a 60 <-> 30 change still steps
//     the brightness, with hysteresis);
//   - fixa: in the Off and Wide modes with their own setting: the top screen while it is the
//     Primary one; swapped (the bottom screen in focus) it is held (HeldForSwap), and the bottom
//     screen never flashes; with the screen's own shape (with Screen Curve on the backing is a
//     cylinder layer like the screen); left out while its layers don't fit the runtime's limit
//     (BackingRoom: held then). Its brightness is flash2's, as in the Full modes (no gain);
//   - fixa: Match Bottom Screen Brightness (vr_motion_clarity_match_bottom, off by default): while
//     the top screen flashes, the bottom screen is dimmed to the top one's time-averaged light
//     (BottomDim: the beat's share, half at 120 Hz), by the layer's colour scale when the runtime
//     has XR_KHR_composition_layer_color_scale_bias, else in the shader that draws its picture.
//     Dimming only: never a boost, so nothing clips.
//
// AZAHARFRAME_XR_SINGLE_FLASH keeps working as an override: 1 on (in every mode, as the test
// switch always was), 0 off, unset the setting. Pure (unit-tested: src/tests/frame_xr/
// motion_clarity.cpp).

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace FrameXr::MotionClarity {

/// AZAHARFRAME_XR_SINGLE_FLASH (live): "1" on in every mode, "0" off; anything else, or unset:
/// none (the setting holds).
inline std::optional<bool> OverrideFrom(const std::optional<std::string>& v) {
    if (v && *v == "1") {
        return true;
    }
    if (v && *v == "0") {
        return false;
    }
    return std::nullopt;
}

/// Whether pictures are flashed at all for this frame: the override, else the setting of the mode
/// the game on view is in. `in_full`: vr_motion_clarity_immersive, for the Full modes
/// (Immersive::ScreenLayers::full_mode: modes 2 and 3, engaged or paused for the VR menu, the
/// game's keyboard or the bottom screen); `in_normal`: vr_motion_clarity_normal, for the Off and
/// Wide modes (fixa). flash2: it was the setting while engaged only (head_driven), so opening the
/// menu turned it off: 100 % lit. projdef: the Full modes took vr_motion_clarity too (the RC's
/// master switch, no longer read).
inline bool InEffect(bool in_full, bool in_normal, std::optional<bool> override_on,
                     bool full_mode) {
    if (override_on) {
        return *override_on;
    }
    return full_mode ? in_full : in_normal;
}

/// AzaharFrame fixa: the Off and Wide modes hold the top screen while it is swapped out of the
/// Primary slot (the bottom screen in focus): Motion Clarity flashes only the 3DS's top screen, and
/// only where it is the Primary one. The Full modes keep flash2's rule: the top screen flashes on
/// the beat while the bottom one is in focus (immersive paused), so its brightness does not step.
inline bool HeldForSwap(bool full_mode, bool swapped) {
    return !full_mode && swapped;
}

/// AzaharFrame fixa: room for the backing's layers (one per eye for the top screen) under the
/// runtime's limit. The VR menu with both controllers pointing at it
/// carried 16 layers in the Full modes with the backing; the Off and Wide modes have one screen
/// layer more (the Secondary screen). Over the limit the last layers of the list (the beams and
/// pointer dots) would be cut, so the backing goes instead and the screen is held (lit every
/// refresh) until the backing fits again for kBackAfter refreshes in a row (no flicker of the
/// list's shape at the edge). `others`: the layers the frame carries besides the backing (the
/// screens, the projection layer, the slots the last frame had present).
class BackingRoom {
public:
    static constexpr uint32_t kBackAfter = 90; ///< 0.75 s at 120 Hz
    static bool Fits(std::size_t limit, std::size_t others, std::size_t backing) {
        return others + backing <= limit;
    }
    bool Next(bool fits) {
        if (!fits) {
            if (room_) {
                times_out_++;
            }
            room_ = false;
            streak_ = 0;
            return false;
        }
        if (!room_ && ++streak_ >= kBackAfter) {
            room_ = true;
        }
        return room_;
    }
    bool Room() const {
        return room_;
    }
    /// Times the backing was left out for room, since the start (for the log).
    uint32_t TimesOut() const {
        return times_out_;
    }

private:
    bool room_ = true;
    uint32_t streak_ = 0;
    uint32_t times_out_ = 0;
};

/// How a game under the full rate (30 or 20 pictures a second) is shown with Motion Clarity on.
enum class Slow : uint8_t {
    Hold,  ///< as without it: every refresh (the default)
    Twice, ///< on the first 2 refreshes of each picture (AZAHARFRAME_XR_FLASH_SLOW=twice)
};
/// AZAHARFRAME_XR_FLASH_SLOW (live): "twice", else Hold.
inline Slow SlowFrom(const std::optional<std::string>& v) {
    return v && *v == "twice" ? Slow::Twice : Slow::Hold;
}

/// Why a refresh shows the picture or the backing (counted for the log).
enum class Why : uint8_t {
    Off,      ///< Motion Clarity not in effect: shown (held as before)
    Unsteady, ///< menu, keyboard, pause, the bottom screen in focus: shown (flash2: on the beat)
    Learning, ///< the picture rate is not known yet: shown (flash2: on the beat)
    Slow,     ///< under the full rate: shown (Slow::Hold)
    Fresh,    ///< the first refresh of a new picture: shown
    Second,   ///< Slow::Twice: the picture's second refresh: shown
    Late,     ///< no new picture past the time one was due: shown again (flash2: on the beat)
    Dark,     ///< shown already, or not its beat yet: the backing
    Gap,      ///< flash2: no picture this refresh (a swapchain made anew): nothing drawn
    Count
};
inline constexpr std::size_t kWhyCount = static_cast<std::size_t>(Why::Count);

inline const char* WhyName(Why w) {
    switch (w) {
    case Why::Off:
        return "off";
    case Why::Unsteady:
        return "menu or pause";
    case Why::Learning:
        return "rate not known yet";
    case Why::Slow:
        return "slow game, held";
    case Why::Fresh:
        return "new picture";
    case Why::Second:
        return "second refresh";
    case Why::Late:
        return "late, held";
    case Why::Dark:
        return "backing";
    case Why::Gap:
        return "no picture";
    default:
        return "?";
    }
}

/// One refresh's facts (XR thread, while building the layers).
struct Refresh {
    bool on = false;      ///< InEffect
    bool steady = false;  ///< playing: no menu, keyboard, pause or bottom screen in focus
    uint64_t picture = 0; ///< the game picture on view this refresh (0: none)
    /// flash2: the refresh's display time (XrTime, ns), for how long a gap without a picture
    /// lasts (kGapKeepNs); 0 not known (a gap then forgets the rate).
    int64_t time_ns = 0;
    /// flash2: the display period (ns), for the beat (Scheduler::Beat: 60 pictures a second);
    /// 0 not known (every other refresh).
    int64_t period_ns = 0;
};

struct Decision {
    bool show = true; ///< false: the backing in the screen's place
    Why why = Why::Off;
};

/// The rate rules, in display refreshes per game picture (the mean over the last kHistory).
/// Full rate: 2 at 60 pictures on 120 Hz (1.5 on 90 Hz). Entered at or under kFastEnter (48 a
/// second and more on 120 Hz), left above kFastLeave (under 40), so a game that hovers in
/// between does not change brightness back and forth. Twice applies up to kTwiceMax (24 a second
/// and more on 120 Hz); slower games are held.
inline constexpr std::size_t kHistory = 8;
inline constexpr float kFastEnter = 2.5f;
inline constexpr float kFastLeave = 3.0f;
inline constexpr float kTwiceMax = 5.0f;
/// An interval longer than this many refreshes is not counted (a stall or a loading screen is not
/// the game's rate; 10 pictures a second at 120 Hz).
inline constexpr uint32_t kMaxInterval = 12;
/// flash3 (report 70, session 573): an interval of twice the full rate's or more (4 refreshes at
/// 60 pictures on 120 Hz) while the game is fast or the rate is being learnt is a hitch (a race
/// starting, a savestate loaded, a dropped frame), not the game's rate: it stays out of the
/// history and the late picture flashes on the beat (Why::Late). One hitch of 11 or more in the
/// history took the mean over kFastLeave and held the screen lit (Slow::Hold) until it left the
/// history again: a brightness step after Quick Load (19 refreshes lit) and at race starts.
/// A game that really runs slower is learnt from a run of kSlowRun such intervals in a row, each
/// within kSlowSpread of the run's first (8 of 4 +-1 at 30 pictures on 120 Hz): then the history
/// is that run and the rate rules decide (Hold) as before.
inline constexpr uint32_t kSlowRun = 8;
inline constexpr uint32_t kSlowSpread = 1;
/// flash2: a gap without a game picture up to this long keeps the learnt rate: a swapchain made
/// anew (a Dynamic Resolution step) has no image for a refresh or two, and relearning the rate
/// after it held about 18 refreshes lit (150 ms), a bright flash at every step. A longer gap (a
/// game stopped, another starting) learns the next game's rate anew.
inline constexpr int64_t kGapKeepNs = 500'000'000;
/// flash2: the beat of the states not played at a known rate (Scheduler::Beat): 60 pictures a
/// second, a game's full rate.
inline constexpr double kDefaultRate = 60.0;

class Scheduler {
public:
    explicit Scheduler(Slow slow = Slow::Hold) : slow_(slow) {}

    void SetSlow(Slow slow) {
        slow_ = slow;
    }

    /// The decision for this refresh. Follows the pictures whether or not it is on, so turning
    /// it on never hides a new picture and the rate is known at once.
    Decision Next(const Refresh& r) {
        if (r.picture == 0) {
            Gap(r.time_ns);
            // Nothing is drawn; the next picture is lit at once (flash2: flashing goes on).
            since_lit_ = kLongAgo;
            credit_ = 0.0f;
            return {true, r.on ? Why::Gap : Why::Off};
        }
        last_picture_ns_ = r.time_ns;
        if (r.picture != picture_) {
            if (picture_ != 0 && clean_) {
                Push(age_ + 1, r.period_ns);
            }
            picture_ = r.picture;
            age_ = 0;
            clean_ = r.steady; // an interval counts only when the whole of it was played
            lits_ = 0;
        } else {
            age_ = std::min<uint32_t>(age_ + 1, 1000);
            clean_ = clean_ && r.steady;
        }
        UpdateRate();
        since_lit_ = std::min<uint32_t>(since_lit_ + 1, kLongAgo);
        const Decision d = Decide(r);
        if (d.show) {
            lits_ = std::min<uint32_t>(lits_ + 1, 1000);
            since_lit_ = 0;
        }
        return d;
    }

    /// flash3: intervals taken for hitches (kept out of the history) since the start.
    uint32_t Hitches() const {
        return hitches_;
    }
    /// Refreshes per picture, the mean of the last kHistory (0: fewer known).
    float Mean() const {
        return filled_ < kHistory ? 0.0f : mean_;
    }
    /// Whether the game runs at the full rate (flashed once per picture).
    bool Fast() const {
        return filled_ >= kHistory && fast_;
    }
    /// The age (refreshes since a picture was first on view) from which it is shown again when
    /// no new one has come: one refresh after the next was due (the mean, rounded), so the screen
    /// is never dark longer than one picture's time. 3 at 60 pictures on 120 Hz, also with the
    /// odd dropped frame in the mean; 5 at 30 (Slow::Twice).
    uint32_t LateAfter() const {
        const float mean = filled_ < kHistory ? 2.0f : mean_;
        return std::max<uint32_t>(2, static_cast<uint32_t>(std::lround(mean))) + 1;
    }
    /// flash2: refreshes per lit refresh on the beat (the states that are not played at a known
    /// rate, and repeats of a late picture): the full rate, kDefaultRate pictures a second, at
    /// `period_ns` (2 at 120 Hz, the share of light of a 60 a second game in play, and of a 30
    /// one with Slow::Twice; 2.4 at 144 Hz), else 2. Not the learnt mean: a dropped picture in it
    /// would dim a menu or a stall. Never under 1.
    static float Beat(int64_t period_ns) {
        if (period_ns > 0) {
            return std::max(1.0f, static_cast<float>(1e9 / kDefaultRate /
                                                     static_cast<double>(period_ns)));
        }
        return 2.0f;
    }

private:
    static constexpr uint32_t kLongAgo = 1000;

    /// The rules, after the picture and the rate are followed. flash2: the share of light is
    /// the same in every state that is not held (Slow::Hold, off): a new picture is lit once
    /// the beat allows it (not on the refresh after a lit one at 60 pictures on 120 Hz, so the
    /// menu's beat hands over to the game's pictures with no two lit refreshes in a row); in play
    /// at a known rate a picture shown once waits for the next as before, until one refresh
    /// after it was due (LateAfter), and is shown again then; otherwise (the menu, a pause, the
    /// keyboard, the bottom screen in focus, learning, a picture shown again late) the newest
    /// picture is lit on the beat (Beat: every other refresh at 120 Hz).
    Decision Decide(const Refresh& r) {
        if (!r.on) {
            credit_ = 0.0f;
            return {true, Why::Off};
        }
        const bool known = filled_ >= kHistory;
        if (known && !fast_) {
            if (slow_ == Slow::Hold || mean_ > kTwiceMax) {
                credit_ = 0.0f;
                return {true, Why::Slow}; // held, as without it (also in the menu)
            }
            if (r.steady) { // Slow::Twice in play: the picture's first 2 refreshes
                credit_ = 0.0f;
                if (age_ < 2) {
                    return {true, age_ == 0 ? Why::Fresh : Why::Second};
                }
                if (age_ >= LateAfter()) {
                    return {true, Why::Late};
                }
                return {false, Why::Dark};
            }
        }
        const bool playing = r.steady && known; // at the full rate here
        const Why held = !r.steady ? Why::Unsteady : !known ? Why::Learning : Why::Late;
        const float beat = Beat(r.period_ns);
        if (lits_ == 0) {
            // A new picture: lit once half a refresh short of the beat has passed since the last
            // lit refresh (at 120 Hz: not right after one; at 90 Hz, 1.5 a picture: at once).
            if (static_cast<float>(since_lit_) + 0.5f >= beat) {
                credit_ = 0.0f;
                return {true, playing ? Why::Fresh : held};
            }
            credit_ += 1.0f / beat;
            return {false, Why::Dark};
        }
        if (playing && lits_ == 1) {
            // Shown once: the next picture is waited for until one refresh past its time.
            credit_ = 0.0f;
            if (since_lit_ < LateAfter()) {
                return {false, Why::Dark};
            }
            return {true, Why::Late};
        }
        // On the beat: the credit of 1 / beat a refresh since the last lit one reaches 1.
        credit_ += 1.0f / beat;
        if (credit_ >= 1.0f - 1e-4f) {
            credit_ = std::max(0.0f, credit_ - 1.0f);
            return {true, held};
        }
        return {false, Why::Dark};
    }

    /// flash2: no game picture this refresh. A short gap (kGapKeepNs since the last picture: a
    /// swapchain made anew) keeps the rate, and the next picture is a new one at once (the gap is
    /// not an interval), so flashing goes on with it; a longer one (no game, a new one starting),
    /// or one with no time, learns the next picture's rate anew.
    void Gap(int64_t now_ns) {
        if (now_ns != 0 && last_picture_ns_ != 0 && now_ns - last_picture_ns_ <= kGapKeepNs) {
            picture_ = 0;
            age_ = 0;
            clean_ = false;
            lits_ = 0;
            return;
        }
        Forget();
    }
    void Forget() {
        picture_ = 0;
        age_ = 0;
        clean_ = false;
        filled_ = 0;
        next_ = 0;
        fast_ = false;
        mean_ = 0.0f;
        last_picture_ns_ = 0;
        lits_ = 0;
        run_ = 0; // flash3
    }
    /// A played interval (refreshes a picture was on view). flash3: while the game is fast, or
    /// the rate is being learnt, one of twice the full rate's or more is a hitch (kSlowRun): kept
    /// out of the history unless it is part of a steady run of them (a slower game).
    void Push(uint32_t interval, int64_t period_ns) {
        if (interval > kMaxInterval) {
            run_ = 0;
            return; // a stall or a loading screen, not the game's rate
        }
        const bool known = filled_ >= kHistory;
        if (known && !fast_) {
            Add(interval); // a slow game's own rate (and its way back to the full one)
            return;
        }
        // The full rate's interval: the learnt mean, or the beat while learning (2 at 120 Hz).
        const float full = known ? mean_ : Beat(period_ns);
        const bool hitch = static_cast<float>(interval) >= std::round(2.0f * full);
        const uint32_t first = run_values_[0];
        const bool fits =
            run_ > 0 && (interval > first ? interval - first : first - interval) <= kSlowSpread;
        if (hitch || fits) {
            if (!fits) {
                run_ = 0; // a new run starts at this interval
            }
            run_values_[run_ % kSlowRun] = interval;
            run_++;
            if (run_ >= kSlowRun) {
                // A steady run: the game's rate now. The history becomes the run.
                for (std::size_t i = 0; i < kHistory; i++) {
                    history_[i] = run_values_[i % kSlowRun];
                }
                filled_ = kHistory;
                next_ = 0;
                run_ = 0;
                return;
            }
            if (hitch) {
                hitches_++;
                return; // a hitch: not the game's rate (yet)
            }
        } else {
            run_ = 0;
        }
        Add(interval);
    }
    void Add(uint32_t interval) {
        history_[next_] = interval;
        next_ = (next_ + 1) % kHistory;
        filled_ = std::min<std::size_t>(filled_ + 1, kHistory);
    }
    void UpdateRate() {
        if (filled_ < kHistory) {
            fast_ = false;
            return;
        }
        uint32_t sum = 0;
        for (const uint32_t h : history_) {
            sum += h;
        }
        mean_ = static_cast<float>(sum) / static_cast<float>(kHistory);
        fast_ = fast_ ? mean_ <= kFastLeave : mean_ <= kFastEnter;
    }

    Slow slow_;
    uint64_t picture_ = 0;
    uint32_t age_ = 0;   ///< refreshes since `picture_` was first on view
    bool clean_ = false; ///< `picture_` has been on view only while steady
    std::array<uint32_t, kHistory> history_{};
    std::size_t filled_ = 0, next_ = 0;
    bool fast_ = false;
    float mean_ = 0.0f;
    int64_t last_picture_ns_ = 0; ///< flash2: the time of the last refresh with a picture
    uint32_t lits_ = 0;              ///< flash2: refreshes `picture_` has been lit on
    uint32_t since_lit_ = kLongAgo;  ///< flash2: refreshes since the last lit one (1: the last)
    float credit_ = 0.0f;            ///< flash2: the beat's share since the last lit refresh
    /// flash3: the run of long intervals (Push): its length and values (run_values_[0] its first).
    uint32_t run_ = 0;
    std::array<uint32_t, kSlowRun> run_values_{};
    uint32_t hitches_ = 0;
};

/// AzaharFrame fixa: Match Bottom Screen Brightness. The bottom screen's colour scale while the top
/// screen flashes: the top one's time-averaged share of light at the beat (1 / Scheduler::Beat:
/// 0.5 at 120 Hz, 0.67 at 90 Hz), steady from refresh to refresh (the learnt LitShare would wander
/// with every late picture); 1 (as it is) when the setting is off or the top screen is not flashing
/// (off, held for a slow game, held for room or a swap). Never above 1.
inline float BottomDim(bool match, bool flashing, int64_t period_ns) {
    if (!match || !flashing) {
        return 1.0f;
    }
    return std::min(1.0f, 1.0f / Scheduler::Beat(period_ns));
}
/// How the bottom screen is dimmed: the layer's colour scale (XR_KHR_composition_layer_color_
/// scale_bias) when the runtime has it, else the shader that draws its picture into its swapchain.
enum class DimMethod : uint8_t { ColorScale, Shader };
inline DimMethod DimMethodFor(bool color_scale_bias) {
    return color_scale_bias ? DimMethod::ColorScale : DimMethod::Shader;
}
inline const char* DimMethodName(DimMethod m) {
    return m == DimMethod::ColorScale
               ? "the layer's colour scale (XR_KHR_composition_layer_color_scale_bias)"
               : "the bottom screen's shader (the runtime has no layer colour scale)";
}
/// The shader's push constant (the XR present shader's `layer`, which it does not use otherwise):
/// the dim in thousandths, 1 to 999; 0 for none.
inline int32_t DimPermille(float dim) {
    if (!(dim < 1.0f)) {
        return 0;
    }
    return std::clamp(static_cast<int32_t>(std::lround(dim * 1000.0f)), 1, 999);
}

/// The scheduler's picture id from what a release recorded: its game picture (ShownPicture::
/// picture: the top screen swaps + 1; 0 not known) or, without one, the release serial itself
/// (every release a new picture). Kept apart by the low bit; 0 only with neither.
inline uint64_t PictureId(uint64_t picture, uint64_t serial) {
    if (picture != 0) {
        return picture << 1;
    }
    return serial == 0 ? 0 : (serial << 1) | 1;
}

/// How the top screen's layers go out this refresh. SteamVR's OpenXR runtime makes each quad
/// layer the overlay of its position in the frame's list and sets that overlay's texture, bounds,
/// transform and size in separate calls the compositor applies on its own schedule (handoff report
/// 55/flash/overlay-path.md). So nothing may change an overlay's texture or the list's shape from
/// one refresh to the next: while Motion Clarity is in effect each eye's screen has an opaque
/// black backing under it, in a layer of its own (the same pose, size and shape), on every
/// refresh; on a dark refresh the screen keeps its layer, swapchain and rect and is only shrunk to
/// nothing (kCollapse), so the backing shows. Only the overlay's size changes.
struct TopLayers {
    bool backing = false;  ///< a backing layer under each eye's screen
    bool collapse = false; ///< the screen shrunk to nothing (a dark refresh)
    bool omit = false;     ///< the screen left out (AZAHARFRAME_XR_FLASH_BACKING=0, the A/B)
};
/// `on`: InEffect; `backing_switch`: AZAHARFRAME_XR_FLASH_BACKING (not 0); `backing_ready`: its
/// image exists; `dark`: the scheduler's decision (!Decision::show). Without the backing's image
/// nothing is hidden (held as before): a dark refresh with nothing of ours behind the screen
/// would show the compositor's background.
inline TopLayers TopLayersFor(bool on, bool backing_switch, bool backing_ready, bool dark) {
    TopLayers t;
    if (!on) {
        return t;
    }
    if (!backing_switch) {
        t.omit = dark;
        return t;
    }
    t.backing = backing_ready;
    t.collapse = backing_ready && dark;
    return t;
}
/// A collapsed screen's size, times its own: under 0.3 mm for the biggest immersive screen, far
/// under a pixel at any distance; never 0, which a runtime may refuse.
inline constexpr float kCollapse = 1e-5f;

/// The share of refreshes that show a picture (the rest show the backing), for the log and the
/// brightness the player sees: the time-averaged light of the screen is this share of what the
/// hold gives (Talbot-Plateau: a flicker above fusion looks as bright as its mean).
inline float LitShare(uint32_t shown, uint32_t dark) {
    const uint32_t all = shown + dark;
    return all == 0 ? 1.0f : static_cast<float>(shown) / static_cast<float>(all);
}

/// The log: each refresh's Why counted; once a second a line when the state changes (off, held,
/// flashing), and every kLogSeconds while it is not off. flash2: also the brightness steps (a
/// lit run of kStepRun refreshes or more after flashing, and flashing again after one) and the
/// longest lit run, while Motion Clarity is in effect.
class Log {
public:
    static constexpr uint32_t kLogSeconds = 10;
    /// flash2: lit this many refreshes in a row (50 ms at 120 Hz) is a held picture: the light
    /// steps up from the flashing share.
    static constexpr uint32_t kStepRun = 6;
    enum class State : uint8_t { Off, Held, Flashing };

    void Note(Why w) {
        second_[static_cast<std::size_t>(w)]++;
        window_[static_cast<std::size_t>(w)]++;
        // flash2: the steps. Off (not in effect) starts afresh; no picture (nothing drawn) is
        // neither lit nor dark.
        if (w == Why::Off) {
            level_ = Level::None;
            lit_run_ = 0;
        } else if (w == Why::Dark) {
            if (level_ == Level::Held) {
                steps_++;
            }
            level_ = Level::Flashing;
            lit_run_ = 0;
        } else if (w != Why::Gap) {
            lit_run_++;
            longest_run_ = std::max(longest_run_, lit_run_);
            if (lit_run_ >= kStepRun && level_ != Level::Held) {
                if (level_ == Level::Flashing) {
                    steps_++;
                }
                level_ = Level::Held;
            }
        }
    }
    /// flash2: brightness steps since the start (for the tests; the line gives the window's).
    uint32_t Steps() const {
        return steps_total_ + steps_;
    }
    /// The state of the last second: flashing when any refresh showed the backing, off when all
    /// were Off (or none counted), else held.
    static State StateOf(const std::array<uint32_t, kWhyCount>& n) {
        if (n[static_cast<std::size_t>(Why::Dark)] > 0) {
            return State::Flashing;
        }
        for (std::size_t i = 0; i < kWhyCount; i++) {
            if (i != static_cast<std::size_t>(Why::Off) && i != static_cast<std::size_t>(Why::Gap) &&
                n[i] > 0) {
                return State::Held;
            }
        }
        return State::Off;
    }
    /// Once a second: the text to log (without "xr: "), "" for none. `mean`: Scheduler::Mean,
    /// `period_ns` the display period, `backing` whether the backing is drawn (else left out),
    /// `hitches`: Scheduler::Hitches (flash3; the line gives the window's).
    std::string Second(float mean, int64_t period_ns, bool backing, uint32_t hitches = 0) {
        const State state = StateOf(second_);
        second_.fill(0);
        seconds_++;
        const bool changed = state != state_;
        state_ = state;
        if (!changed && (state == State::Off || seconds_ < kLogSeconds)) {
            if (state == State::Off) {
                window_.fill(0);
                seconds_ = 0;
                steps_total_ += steps_;
                steps_ = 0;
                longest_run_ = lit_run_;
                hitches_seen_ = hitches;
            }
            return {};
        }
        const auto n = [&](Why w) { return window_[static_cast<std::size_t>(w)]; };
        const uint32_t dark = n(Why::Dark);
        uint32_t shown = 0;
        for (std::size_t i = 0; i < kWhyCount; i++) {
            if (i != static_cast<std::size_t>(Why::Dark) && i != static_cast<std::size_t>(Why::Gap)) {
                shown += window_[i];
            }
        }
        const double hz = period_ns > 0 ? 1e9 / static_cast<double>(period_ns) : 0.0;
        std::string rate = "rate not known yet";
        if (mean > 0.0f) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "a game picture every %.2f refreshes (%.1f a second at %.0f Hz)",
                          mean, hz > 0.0 ? hz / mean : 0.0, hz);
            rate = buf;
        }
        char buf[512];
        std::snprintf(buf, sizeof(buf),
                      "Motion Clarity %s: %s; over %u s, lit %u of %u refreshes (%.0f %%), %s %u; "
                      "brightness steps %u, longest lit run %u; lit for: new picture %u, late %u, "
                      "menu or pause %u, learning %u; held: slow game %u, off %u; no picture %u; "
                      "hitches not taken for the rate %u",
                      state == State::Flashing ? "flashing"
                      : state == State::Held   ? "holding"
                                               : "off",
                      rate.c_str(), seconds_, shown, shown + dark, 100.0 * LitShare(shown, dark),
                      backing ? "black backing" : "screen left out", dark, steps_, longest_run_,
                      n(Why::Fresh) + n(Why::Second), n(Why::Late), n(Why::Unsteady),
                      n(Why::Learning), n(Why::Slow), n(Why::Off), n(Why::Gap),
                      hitches - std::min(hitches, hitches_seen_));
        hitches_seen_ = hitches;
        window_.fill(0);
        seconds_ = 0;
        steps_total_ += steps_;
        steps_ = 0;
        longest_run_ = lit_run_; // a run going on counts in the next window too
        return buf;
    }

private:
    enum class Level : uint8_t { None, Flashing, Held }; ///< flash2: for the steps
    std::array<uint32_t, kWhyCount> second_{}, window_{};
    uint32_t seconds_ = 0;
    State state_ = State::Off;
    Level level_ = Level::None;
    uint32_t lit_run_ = 0, longest_run_ = 0, steps_ = 0, steps_total_ = 0;
    uint32_t hitches_seen_ = 0; ///< flash3: Scheduler::Hitches at the last line
};

} // namespace FrameXr::MotionClarity
