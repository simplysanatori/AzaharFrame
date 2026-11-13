// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame scroll: how the in-game menu's lists scroll, the same in every list (Settings,
// Cheats, Install Mods, Download Packs, Homebrew and the rest). Pure arithmetic, no drawing and
// no OpenXR, so the unit tests can drive it with made-up sticks and beams:
//
// - StickScroll: a stick scrolls at a speed that follows how far it is pushed. Held far over, it
//   speeds up smoothly from about a row every 0.2 s to a screen every 0.25 s after about a
//   second. Let go, it stops where it is. A short tap moves exactly one row (a wheel notch too).
// - Scrollbar: where the thumb and the track are, a beam's hit strip wider than the slim bar,
//   the thumb dragged 1:1 along the track, and a click on the track paging by one screen.
// - DragScroll: a press that moves more than kDragSlop scrolls the list (touch style); let go
//   while moving, the list coasts and slows down (a gentle fling).

#pragma once

#include <array>

#include "frame_xr/menu/md3.h"

namespace FrameXr::Menu::Scrolling {

// ---- The stick ----
constexpr float kStickDead = 0.12f;       ///< below this the stick is at rest
constexpr float kStickCurve = 1.6f;       ///< speed ~ push^1.6: half a push is about a third
constexpr float kStartSpeed = 300.0f;     ///< dp/s at a full push, before it speeds up
constexpr float kScreensPerSecond = 4.0f; ///< the top speed: a screen every 0.25 s
constexpr float kAccelPush = 0.75f;       ///< pushed at least this far, the speed builds up
constexpr float kAccelDelay = 0.2f;       ///< s held before it starts to speed up
constexpr float kAccelFull = 1.0f;        ///< s held to reach the top speed
constexpr float kRiseTau = 0.05f;         ///< s: speeding up eases in; slowing down does not
constexpr float kTapTime = 0.2f;          ///< a push let go sooner is a tap: one row
constexpr float kTapPush = 0.5f;          ///< how far a tap must push

/// A stick's push (-1 to 1, as the stick's y: up is positive) as a speed in dp/s down the list,
/// without the build-up: 0 inside the dead zone, then curved.
float StickSpeed(float stick_y, float speed);
/// The speed the list scrolls at a full push after `held` seconds pushed far over (kAccelPush),
/// for a list `view_h` dp tall: kStartSpeed, eased up to kScreensPerSecond screens per second.
float HeldSpeed(float held, float view_h);

/// The stick's scrolling of one list (XR thread). Update each frame; it returns the new scroll.
class StickScroll {
public:
    /// `stick_y`: the stick (up is positive); `scroll`, `max_scroll`: the list's now (dp);
    /// `view_h`: the list's visible height; `step`: one row (a tap's move).
    float Update(float stick_y, float dt, float scroll, float max_scroll, float view_h,
                 float step);
    /// Forget a push (another tab, a dialog): the next push starts slow again.
    void Reset() {
        *this = {};
    }
    bool Active() const {
        return dir != 0;
    }
    /// The speed now (dp/s, signed: positive is down the list).
    float Velocity() const {
        return static_cast<float>(dir) * speed;
    }

private:
    int dir = 0;          ///< +1 down the list (stick down), -1 up, 0 at rest
    float held = 0.0f;    ///< s pushed this way
    float accel = 0.0f;   ///< s pushed far over (the build-up)
    float speed = 0.0f;   ///< dp/s now, eased towards the stick's
    float peak = 0.0f;    ///< the push's furthest (for a tap)
    float start = 0.0f;   ///< the scroll when the push started
};

// ---- The scrollbar ----
constexpr float kTrackInset = 8.0f;   ///< the track's ends inside the list
constexpr float kThumbMin = 40.0f;    ///< the shortest thumb (dp)
constexpr float kBarRight = 6.0f;     ///< the bar's right edge, in from the list's right
constexpr float kBarW = 4.0f;         ///< the bar at rest
constexpr float kBarHoverW = 8.0f;    ///< the bar with a beam on it or held
constexpr float kHitLeft = 24.0f;     ///< the beam's strip: this far into the list...
constexpr float kHitRight = 12.0f;    ///< ...and this far past it (the pane's margin)
constexpr float kPageRepeatDelay = 0.4f; ///< s a press on the track holds before paging again
constexpr float kPageRepeat = 0.12f;     ///< s between pages after that

/// One list's scrollbar as drawn (MenuLayout::bar). `list` is the visible list, `right` the
/// x the bar hangs off (the pane's right edge), `content_h` the whole list's height.
struct Bar {
    Rect list;
    float right = 0.0f;
    float content_h = 0.0f;
    float max_scroll = 0.0f;
    bool Valid() const {
        return max_scroll > 0.0f && content_h > 0.0f && list.H() > 2.0f * kTrackInset;
    }
};

float TrackY0(const Bar& b);
float TrackY1(const Bar& b);
float ThumbH(const Bar& b);
/// The thumb's top at `scroll`.
float ThumbY(const Bar& b, float scroll);
/// Where a beam takes the bar: the whole track's height, wider than the bar.
Rect HitRect(const Bar& b);
/// The bar as drawn: slim at rest, wider with a beam on it (`wide`).
Rect ThumbRect(const Bar& b, float scroll, bool wide);
Rect TrackRect(const Bar& b, bool wide);
/// The scroll that puts the thumb's top at `thumb_y` (clamped to the list's range).
float ScrollForThumb(const Bar& b, float thumb_y);
/// -1: `y` is above the thumb, +1 below it, 0 on it.
int TrackSide(const Bar& b, float scroll, float y);
/// One screen up (-1) or down (+1) from `scroll`.
float PageFrom(const Bar& b, float scroll, int dir);

/// A press on the scrollbar (XR thread): the thumb held, or the track paging while held.
class BarPress {
public:
    /// The press starts at `y`; returns the scroll after it (a track press pages at once).
    float Begin(const Bar& b, float scroll, float y);
    /// Each frame the press holds, the beam at `y`; returns the new scroll.
    float Update(const Bar& b, float scroll, float y, float dt);
    void End() {
        mode = Mode::None;
    }
    bool Active() const {
        return mode != Mode::None;
    }
    bool Dragging() const {
        return mode == Mode::Thumb;
    }

private:
    enum class Mode { None, Thumb, Track } mode = Mode::None;
    float grab = 0.0f;   ///< the beam's y less the thumb's top when it took the thumb
    int dir = 0;         ///< the track's paging direction
    float wait = 0.0f;   ///< s to the next page
};

// ---- Dragging the list itself ----
constexpr float kDragSlop = 12.0f;     ///< dp a press moves before it scrolls (not a tap)
constexpr float kFlingMin = 250.0f;    ///< dp/s: let go slower, the list stays put
constexpr float kFlingMax = 4000.0f;   ///< dp/s
constexpr float kFlingTau = 0.3f;      ///< s: a fling slows by e every 0.3 s (coasts v * 0.3)
constexpr float kFlingStop = 20.0f;    ///< dp/s: a fling this slow stops
constexpr float kVelocityWindow = 0.1f; ///< s of the beam's path a fling's speed is taken over

/// A press on the list that may become a drag, and the fling after it (XR thread).
class DragScroll {
public:
    /// A press starts at `y` (dp) at time `t` (s); a fling stops.
    void Press(float y, float scroll, float t);
    /// The press moved to `y`. Returns true while it drags the list (past kDragSlop); `scroll`
    /// is then where the list follows the beam (1:1).
    bool Move(float y, float t, float& scroll, float max_scroll);
    /// The press ended; a drag that was still moving flings.
    void Release(float t);
    /// Each frame: a fling's scroll (returns `scroll` unchanged when none).
    float Coast(float scroll, float max_scroll, float dt);
    void Stop() {
        fling = 0.0f;
    }
    void Cancel() {
        pressed = dragging = false;
        fling = 0.0f;
    }
    bool Dragging() const {
        return dragging;
    }
    bool Flinging() const {
        return fling != 0.0f;
    }
    float FlingVelocity() const {
        return fling;
    }
    /// The beam's speed over the last kVelocityWindow (dp/s down the list; scroll's direction).
    float Velocity() const;

private:
    bool pressed = false, dragging = false;
    float y0 = 0.0f, scroll0 = 0.0f;
    float fling = 0.0f;
    struct Sample {
        float t = 0.0f, scroll = 0.0f;
    };
    std::array<Sample, 16> samples{};
    int count = 0, head = 0;
    void Add(float t, float scroll);
};

} // namespace FrameXr::Menu::Scrolling
