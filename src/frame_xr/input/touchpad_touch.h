// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/mapper.h"
#include "frame_xr/input/types.h"

// AzaharFrame touchpad
// Touchpad Touch: a gamepad touchpad (or the mouse a touchpad is mapped to) touches the 3DS
// bottom screen, with the pad mapped absolutely onto it, and a dot on the screen where the touch
// is. Pure logic, no OpenXR, Qt or SDL: the bridge feeds it the samples and the touch events the
// controller beam made, and sends what it returns to the touch sink.
//
// Sources: the beam (the mapper's events), the pad and the mouse. The last source to become
// active owns the touch; a release by the owner ends the touch, and nothing takes over from a
// source that stayed down (it must let go and engage again). A hidden bottom screen has no pad
// or mouse touch.
namespace FrameXr::Input {

/// Settings::values.vr_touchpad_touch.
enum class TouchpadMode : int { Off = 0, Touch = 1, Click = 2 };

constexpr TouchpadMode TouchpadModeFrom(uint32_t value) {
    return value == 1 ? TouchpadMode::Touch : value == 2 ? TouchpadMode::Click : TouchpadMode::Off;
}

/// A gamepad touchpad. x and y are 0..1 from the top left (SDL's).
struct PadSample {
    bool present = false; ///< a controller with a touchpad is connected
    bool contact = false; ///< a finger is on the pad
    bool click = false;   ///< the pad is pressed
    float x = 0.0f;
    float y = 0.0f;
};

/// The pointer over the (invisible) render window. x and y are 0..1 over the window.
struct MouseSample {
    bool inside = false;
    bool down = false; ///< left button
    float x = 0.0f;
    float y = 0.0f;
};

/// AzaharFrame padsui: the mouse in Mouse Mode: Bottom Screen: relative, a cursor of its own.
struct CursorSample {
    bool on = false;   ///< a mouse in Bottom Screen mode, a game running, the menu closed
    Vec2f motion;      ///< counts this frame, right and down
    float gain = 0.3f; ///< 3DS pixels per count
    bool down = false; ///< the touch input held
    bool recentre = true;
};

enum class TouchSource : int { None = 0, Beam, Pad, Mouse, Cursor };

struct TouchpadFrame {
    TouchpadMode mode = TouchpadMode::Off;
    PadSample pad;
    MouseSample mouse;
    CursorSample cursor; ///< AzaharFrame padsui
    bool pad_recentre = false; ///< AzaharFrame padsui: the pad's Recentre at Edge
    bool bottom_visible = false;
    int64_t time_ns = 0;
    /// The mapper's touch events of this frame (the controller beam).
    const TouchEvent* beam = nullptr;
    int beam_count = 0;
};

struct TouchpadCursor {
    bool visible = false;
    TouchSource source = TouchSource::None;
    float px = 0.0f; ///< 3DS pixels
    float py = 0.0f;
    float u = 0.0f; ///< 0..1 from the left / top edge of the screen
    float v = 0.0f;
    bool pressed = false;
};

struct TouchpadResult {
    std::array<TouchEvent, 8> events{};
    int event_count = 0;
    /// The dot of the pad or the mouse; never shown while the beam owns the touch (the beam has
    /// its own dot).
    TouchpadCursor cursor;
};

class TouchpadTouch {
public:
    /// The mouse dot hovers this long after the pointer last moved.
    static constexpr int64_t kMouseIdleNs = 2'000'000'000;
    /// A touch only moves when it changed by at least this many 3DS pixels (as the mapper's).
    static constexpr float kMoveEpsilonPx = 0.25f;
    /// AzaharFrame padsui: Recentre at Edge: a drag that runs into an edge lifts for this long
    /// (so the game sees the lift) and touches again at the middle.
    static constexpr int64_t kRecentreGapNs = 50'000'000;
    /// A touchpad's point this near an edge (a fraction of the screen) is at it.
    static constexpr float kPadEdge = 0.02f;

    TouchpadResult Update(const TouchpadFrame& frame);

    /// Forgets everything. Returns true when a touch was in progress: the caller sends the
    /// release. A source already down afterwards must let go before it can touch.
    bool Reset();

    TouchSource owner() const {
        return owner_;
    }
    bool touching() const {
        return touching_;
    }

    /// AzaharFrame padsui: what a cursor source gives the shared cursor pipeline (CursorStep): a
    /// touchpad its finger's point (absolute), the mouse its counts (relative).
    struct CursorInput {
        bool live = false;     ///< the source counts at all now
        bool absolute = false; ///< `point` (0..1) is where it is; else `delta` moves it
        bool has_point = false;
        Vec2f point;
        Vec2f delta;           ///< 3DS pixels this frame (counts times the speed's gain)
        bool down = false;     ///< touching (click = touch, held = drag)
        bool recentre = false; ///< Recentre at Edge
    };

private:
    struct Source {
        bool engaged_prev = false;
        bool stale = true; ///< down when the arbiter (re)started: needs a release first
        float px = 0.0f;
        float py = 0.0f;
    };
    /// AzaharFrame padsui: one cursor of the shared pipeline: its point, clamped to the screen,
    /// and Recentre at Edge's state.
    struct CursorTrack {
        Source src;
        bool started = false;
        Vec2f offset;          ///< absolute: added to the point after a recentre
        bool gap = false;      ///< lifted by Recentre at Edge, touching again at gap_until
        int64_t gap_until = 0;
        bool at_edge = false;  ///< absolute: was at an edge last frame
        int64_t move_ns = 0;   ///< last moved
    };
    /// The pipeline: moves the cursor, clamps it, touches with `down` (and drags while held),
    /// and with Recentre at Edge lifts a drag that runs into an edge and touches again at the
    /// middle after kRecentreGapNs.
    void CursorStep(TouchpadResult& out, CursorTrack& t, const CursorInput& in, TouchSource who,
                    int64_t now);
    void Emit(TouchpadResult& out, TouchEventType type, float px, float py);
    void Take(TouchpadResult& out, TouchSource who, float px, float py);
    void Drop(TouchpadResult& out, TouchSource who);
    void Follow(TouchpadResult& out, TouchSource who, float px, float py);
    bool Engage(Source& s, bool engaged);

    TouchSource owner_ = TouchSource::None;
    bool touching_ = false; ///< the sink has been told of a touch
    float sent_px_ = 0.0f;
    float sent_py_ = 0.0f;
    // AzaharFrame padsui: the touchpad's and the mouse's cursors, one pipeline.
    CursorTrack pad_;
    CursorTrack cursor_;
    Source mouse_;
    bool mouse_seen_ = false;
    float mouse_last_x_ = 0.0f;
    float mouse_last_y_ = 0.0f;
    int64_t mouse_move_ns_ = 0;
};

/// Where the dot goes for a point (u, v) on the screen: on the quad, bent as the quad is, a hair
/// out of it; `eye` sets its size as the beam's dot grows with distance.
struct DotPose {
    Vec3f position;
    Quatf orientation;
    float size_m = 0.01f;
};
DotPose DotOnQuad(const PanelQuad& quad, float u, float v, const Vec3f& eye);

} // namespace FrameXr::Input
