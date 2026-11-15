// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame projdef: which path the top screen takes, quads or the projection layer.
//
// The owner's A/B (1.1.1-590, reports/76/session590): in the Full immersive modes the projection
// layer (proj_screen.h) is the cleaner picture: no overlay race, no doubling from a quad shown
// with another picture's pose, the controllers in the same image. So it is the default there. Motion
// Clarity's flashing needs the quads (a dark refresh is a quad shrunk to nothing), so with Motion
// Clarity on in the Full modes ("In Immersive Modes") the screens stay quads and flash, as before.
// The normal and the wide view (modes 0 and 1) stay quads: a screen fixed in the room is not exposed
// to the overlay race, and quads are sharper and cheaper there (Kid Icarus: Uprising, A vs B).
//
// The choice follows the settings, never a frame's state: a pause, the VR menu or the bottom screen
// in focus keeps it (a change makes the models' pass again). With no game on view (a Quick Load,
// a boot, the session hidden) the last choice holds for kNoGameHoldNs, then the quads (the home
// view). The test switch AZAHARFRAME_XR_SCREENS_IN_PROJECTION overrides it for an A/B: 1 forces the
// projection layer (anywhere, as the test builds did), 0 the quads. Pure (unit-tested:
// src/tests/frame_xr/proj_path.cpp).

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace FrameXr::Vis {

/// How long the last choice holds with no game on view.
inline constexpr int64_t kNoGameHoldNs = 10'000'000'000;

/// AZAHARFRAME_XR_SCREENS_IN_PROJECTION: "1" forces the projection layer, "0" the quads, anything
/// else (or unset) leaves the choice to the settings.
inline std::optional<bool> ProjForcedFrom(const std::optional<std::string>& v) {
    if (v && *v == "1") {
        return true;
    }
    if (v && *v == "0") {
        return false;
    }
    return std::nullopt;
}

struct PathInputs {
    std::optional<bool> forced; ///< the test switch
    bool game_on_view = false;  ///< Immersive::ScreensPathState
    bool full_mode = false;     ///< the game's Immersive Mode is 2 or 3
    /// Motion Clarity wanted in the Full modes (MotionClarity::WantedInFull: the settings)
    bool clarity = false;
    int64_t now_ns = 0;
};

enum class PathWhy : uint8_t {
    Start,      ///< nothing chosen yet: the quads
    Forced,     ///< the test switch
    Full,       ///< a Full mode, Motion Clarity off: the projection layer
    Clarity,    ///< a Full mode with Motion Clarity: the quads, flashed
    NotFull,    ///< the normal or the wide view: the quads
    Held,       ///< no game on view: the last choice, for a while
    NoGame,     ///< no game on view for kNoGameHoldNs: the quads
};

inline const char* PathWhyText(PathWhy w) {
    switch (w) {
    case PathWhy::Start:
        return "at start";
    case PathWhy::Forced:
        return "the test switch";
    case PathWhy::Full:
        return "a Full mode, Motion Clarity off";
    case PathWhy::Clarity:
        return "a Full mode with Motion Clarity";
    case PathWhy::NotFull:
        return "not a Full mode";
    case PathWhy::Held:
        return "no game on view, kept";
    case PathWhy::NoGame:
        return "no game on view";
    }
    return "";
}

class PathChoice {
public:
    /// Once a frame: true for the projection layer.
    bool Next(const PathInputs& in) {
        if (in.forced) {
            Set(*in.forced, PathWhy::Forced);
            off_since_ = -1;
            return projection_;
        }
        if (in.game_on_view) {
            off_since_ = -1;
            if (!in.full_mode) {
                Set(false, PathWhy::NotFull);
            } else if (in.clarity) {
                Set(false, PathWhy::Clarity);
            } else {
                Set(true, PathWhy::Full);
            }
            return projection_;
        }
        if (off_since_ < 0) {
            off_since_ = in.now_ns;
        }
        // The settings' projection layer holds a while (not one the switch forced: lifted with no
        // game on view, the quads at once).
        if (projection_ && why_ != PathWhy::Forced && in.now_ns - off_since_ < kNoGameHoldNs) {
            why_ = PathWhy::Held;
            return projection_;
        }
        Set(false, why_ == PathWhy::Start ? PathWhy::Start : PathWhy::NoGame);
        return projection_;
    }

    bool Projection() const {
        return projection_;
    }
    PathWhy Why() const {
        return why_;
    }
    /// True once after each change of path (for the log).
    bool TakeChanged() {
        const bool c = changed_;
        changed_ = false;
        return c;
    }

private:
    void Set(bool projection, PathWhy why) {
        if (projection != projection_) {
            changed_ = true;
        }
        projection_ = projection;
        why_ = why;
    }

    bool projection_ = false;
    PathWhy why_ = PathWhy::Start;
    int64_t off_since_ = -1;
    bool changed_ = false;
};

} // namespace FrameXr::Vis
