// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash: Motion Clarity (Settings::values.vr_motion_clarity, per game and global).
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
//   - a picture late past its time (a dropped game frame, a stall, a loading screen) is shown
//     again from one refresh after the time the next one was due, and held while none comes: the
//     screen never goes dark for longer than one picture's time;
//   - held (no flashing) while the VR menu, the game's keyboard or the bottom screen in focus is
//     shown, or the game is paused, so menus are steady; and outside the Full modes, where the
//     picture does not follow the head (the setting's row is locked there).
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

/// Whether pictures are flashed at all for this frame: the override, else the setting while the
/// top screen is the head-driven immersive screen (Full modes, engaged: Immersive::ScreenLayers::
/// head_driven).
inline bool InEffect(bool setting, std::optional<bool> override_on, bool head_driven) {
    if (override_on) {
        return *override_on;
    }
    return setting && head_driven;
}

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
    Unsteady, ///< menu, keyboard, pause, the bottom screen in focus: shown
    Learning, ///< the picture rate is not known yet: shown
    Slow,     ///< under the full rate: shown (Slow::Hold)
    Fresh,    ///< the first refresh of a new picture: shown
    Second,   ///< Slow::Twice: the picture's second refresh: shown
    Late,     ///< no new picture past the time one was due: shown again (held)
    Dark,     ///< shown already: the backing
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
/// flash2: a gap without a game picture up to this long keeps the learnt rate: a swapchain made
/// anew (a Dynamic Resolution step) has no image for a refresh or two, and relearning the rate
/// after it held about 18 refreshes lit (150 ms), a bright flash at every step. A longer gap (a
/// game stopped, another starting) learns the next game's rate anew.
inline constexpr int64_t kGapKeepNs = 500'000'000;

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
            return {true, r.on ? Why::Unsteady : Why::Off};
        }
        last_picture_ns_ = r.time_ns;
        if (r.picture != picture_) {
            if (picture_ != 0 && clean_) {
                Push(age_ + 1);
            }
            picture_ = r.picture;
            age_ = 0;
            clean_ = r.steady; // an interval counts only when the whole of it was played
        } else {
            age_ = std::min<uint32_t>(age_ + 1, 1000);
            clean_ = clean_ && r.steady;
        }
        UpdateRate();
        if (!r.on) {
            return {true, Why::Off};
        }
        if (!r.steady) {
            return {true, Why::Unsteady};
        }
        if (filled_ < kHistory) {
            return {true, Why::Learning};
        }
        uint32_t lit = 1;
        if (!fast_) {
            if (slow_ == Slow::Hold || mean_ > kTwiceMax) {
                return {true, Why::Slow};
            }
            lit = 2;
        }
        if (age_ < lit) {
            return {true, age_ == 0 ? Why::Fresh : Why::Second};
        }
        if (age_ >= LateAfter()) {
            return {true, Why::Late};
        }
        return {false, Why::Dark};
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

private:
    /// flash2: no game picture this refresh. A short gap (kGapKeepNs since the last picture: a
    /// swapchain made anew) keeps the rate, and the next picture is a new one at once (the gap is
    /// not an interval), so flashing goes on with it; a longer one (no game, a new one starting),
    /// or one with no time, learns the next picture's rate anew.
    void Gap(int64_t now_ns) {
        if (now_ns != 0 && last_picture_ns_ != 0 && now_ns - last_picture_ns_ <= kGapKeepNs) {
            picture_ = 0;
            age_ = 0;
            clean_ = false;
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
    }
    void Push(uint32_t interval) {
        if (interval > kMaxInterval) {
            return; // a stall or a loading screen, not the game's rate
        }
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
};

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
/// flashing), and every kLogSeconds while it is not off.
class Log {
public:
    static constexpr uint32_t kLogSeconds = 10;
    enum class State : uint8_t { Off, Held, Flashing };

    void Note(Why w) {
        second_[static_cast<std::size_t>(w)]++;
        window_[static_cast<std::size_t>(w)]++;
    }
    /// The state of the last second: flashing when any refresh showed the backing, off when all
    /// were Off (or none counted), else held.
    static State StateOf(const std::array<uint32_t, kWhyCount>& n) {
        if (n[static_cast<std::size_t>(Why::Dark)] > 0) {
            return State::Flashing;
        }
        for (std::size_t i = 0; i < kWhyCount; i++) {
            if (i != static_cast<std::size_t>(Why::Off) && n[i] > 0) {
                return State::Held;
            }
        }
        return State::Off;
    }
    /// Once a second: the text to log (without "xr: "), "" for none. `mean`: Scheduler::Mean,
    /// `period_ns` the display period, `backing` whether the backing is drawn (else left out).
    std::string Second(float mean, int64_t period_ns, bool backing) {
        const State state = StateOf(second_);
        second_.fill(0);
        seconds_++;
        const bool changed = state != state_;
        state_ = state;
        if (!changed && (state == State::Off || seconds_ < kLogSeconds)) {
            if (state == State::Off) {
                window_.fill(0);
                seconds_ = 0;
            }
            return {};
        }
        const auto n = [&](Why w) { return window_[static_cast<std::size_t>(w)]; };
        const uint32_t dark = n(Why::Dark);
        uint32_t shown = 0;
        for (std::size_t i = 0; i < kWhyCount; i++) {
            if (i != static_cast<std::size_t>(Why::Dark)) {
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
        char buf[320];
        std::snprintf(buf, sizeof(buf),
                      "Motion Clarity %s: %s; over %u s, lit %u of %u refreshes (%.0f %%), %s %u; "
                      "held: late %u, menu or pause %u, slow game %u, learning %u, off %u",
                      state == State::Flashing ? "flashing"
                      : state == State::Held   ? "holding"
                                               : "off",
                      rate.c_str(), seconds_, shown, shown + dark, 100.0 * LitShare(shown, dark),
                      backing ? "black backing" : "screen left out", dark, n(Why::Late),
                      n(Why::Unsteady), n(Why::Slow), n(Why::Learning), n(Why::Off));
        window_.fill(0);
        seconds_ = 0;
        return buf;
    }

private:
    std::array<uint32_t, kWhyCount> second_{}, window_{};
    uint32_t seconds_ = 0;
    State state_ = State::Off;
};

} // namespace FrameXr::MotionClarity
