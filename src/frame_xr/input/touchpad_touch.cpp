// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "frame_xr/input/touchpad_touch.h"

// AzaharFrame touchpad
namespace FrameXr::Input {

namespace {

float Clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

// Stay inside the last pixel: 320.0 itself is off the screen.
float ToPx(float unit, float size) {
    return std::clamp(unit * size, 0.0f, std::nextafter(size, 0.0f));
}

} // namespace

void TouchpadTouch::Emit(TouchpadResult& out, TouchEventType type, float px, float py) {
    if (out.event_count < static_cast<int>(out.events.size())) {
        out.events[static_cast<std::size_t>(out.event_count++)] = {type, px, py};
    }
}

void TouchpadTouch::Take(TouchpadResult& out, TouchSource who, float px, float py) {
    // The last source to become active wins; a touch in progress just moves to its point.
    Emit(out, touching_ ? TouchEventType::Moved : TouchEventType::Pressed, px, py);
    touching_ = true;
    owner_ = who;
    sent_px_ = px;
    sent_py_ = py;
}

void TouchpadTouch::Drop(TouchpadResult& out, TouchSource who) {
    if (owner_ == who && touching_) {
        Emit(out, TouchEventType::Released, 0.0f, 0.0f);
        touching_ = false;
    }
    if (owner_ == who) {
        owner_ = TouchSource::None;
    }
}

void TouchpadTouch::Follow(TouchpadResult& out, TouchSource who, float px, float py) {
    if (owner_ != who) {
        return;
    }
    if (std::abs(px - sent_px_) >= kMoveEpsilonPx || std::abs(py - sent_py_) >= kMoveEpsilonPx) {
        Emit(out, TouchEventType::Moved, px, py);
        sent_px_ = px;
        sent_py_ = py;
    }
}

// True when the source engages with an edge it may use: a source that was down at the start
// waits for a release first.
bool TouchpadTouch::Engage(Source& s, bool engaged) {
    if (!engaged) {
        s.stale = false;
    }
    const bool edge = engaged && !s.engaged_prev && !s.stale;
    s.engaged_prev = engaged;
    return edge;
}

// AzaharFrame padsui: the shared cursor pipeline.
void TouchpadTouch::CursorStep(TouchpadResult& out, CursorTrack& t, const CursorInput& in,
                               TouchSource who, int64_t now) {
    const float max_x = std::nextafter(kBottomScreenWidthPx, 0.0f);
    const float max_y = std::nextafter(kBottomScreenHeightPx, 0.0f);
    if (!t.started) {
        t.src.px = kBottomScreenWidthPx * 0.5f;
        t.src.py = kBottomScreenHeightPx * 0.5f;
        t.started = true;
    }
    const bool engaged = in.live && in.down;
    if (!engaged) {
        t.offset = {};
        t.gap = false;
    }
    // Where it is, and whether it ran into an edge this frame.
    bool hit_edge = false;
    bool at_edge = false;
    if (in.absolute) {
        if (in.has_point) {
            const float ux = in.point.x + t.offset.x, uy = in.point.y + t.offset.y;
            const float nx = ToPx(Clamp01(ux), kBottomScreenWidthPx);
            const float ny = ToPx(Clamp01(uy), kBottomScreenHeightPx);
            if (nx != t.src.px || ny != t.src.py) {
                t.move_ns = now;
            }
            t.src.px = nx;
            t.src.py = ny;
            at_edge = ux <= kPadEdge || ux >= 1.0f - kPadEdge || uy <= kPadEdge ||
                      uy >= 1.0f - kPadEdge;
            hit_edge = at_edge && !t.at_edge;
        }
    } else if (in.live && (in.delta.x != 0.0f || in.delta.y != 0.0f)) {
        const float nx = t.src.px + in.delta.x;
        const float ny = t.src.py + in.delta.y;
        hit_edge = nx < 0.0f || nx > max_x || ny < 0.0f || ny > max_y;
        t.src.px = std::clamp(nx, 0.0f, max_x);
        t.src.py = std::clamp(ny, 0.0f, max_y);
        t.move_ns = now;
    }
    const bool edge = Engage(t.src, engaged);
    if (!engaged) {
        Drop(out, who);
    } else if (t.gap) {
        if (now >= t.gap_until) {
            t.gap = false;
            Take(out, who, t.src.px, t.src.py);
        }
    } else if (edge) {
        Take(out, who, t.src.px, t.src.py);
    } else if (in.recentre && hit_edge && owner_ == who && touching_) {
        // Recentre at Edge: lift, and touch again at the middle; an absolute source goes on
        // from there by its motion (an offset) until it lets go.
        Drop(out, who);
        if (in.absolute) {
            t.offset = {0.5f - in.point.x, 0.5f - in.point.y};
        }
        t.src.px = kBottomScreenWidthPx * 0.5f;
        t.src.py = kBottomScreenHeightPx * 0.5f;
        t.gap = true;
        t.gap_until = now + kRecentreGapNs;
    } else {
        Follow(out, who, t.src.px, t.src.py);
    }
    t.at_edge = engaged && at_edge && !t.gap;
}

TouchpadResult TouchpadTouch::Update(const TouchpadFrame& frame) {
    TouchpadResult out;

    // The beam, in order, as the mapper made them.
    for (int i = 0; i < frame.beam_count; ++i) {
        const TouchEvent& e = frame.beam[i];
        switch (e.type) {
        case TouchEventType::Pressed:
            Take(out, TouchSource::Beam, e.x, e.y);
            break;
        case TouchEventType::Moved:
            if (owner_ == TouchSource::Beam) {
                Emit(out, TouchEventType::Moved, e.x, e.y);
                sent_px_ = e.x;
                sent_py_ = e.y;
            }
            break;
        case TouchEventType::Released:
            Drop(out, TouchSource::Beam);
            break;
        }
    }

    const bool on = frame.mode != TouchpadMode::Off && frame.bottom_visible;
    const bool cursor_on = frame.bottom_visible && frame.cursor.on; // AzaharFrame padsui

    // The pad. Touch: finger contact is the touch. Click: the click is, contact only aims.
    // AzaharFrame padsui: the pad and the mouse's cursor share one pipeline (CursorStep): the pad
    // gives its point, the mouse its counts.
    {
        const PadSample& pad = frame.pad;
        CursorInput in;
        in.live = on && pad.present;
        in.absolute = true;
        in.has_point = pad.present && (pad.contact || pad.click);
        in.point = {pad.x, pad.y};
        in.down = frame.mode == TouchpadMode::Touch ? pad.contact : pad.click && in.has_point;
        in.recentre = frame.pad_recentre;
        CursorStep(out, pad_, in, TouchSource::Pad, frame.time_ns);
    }

    // AzaharFrame padsui: the mouse in Bottom Screen mode: its counts, scaled by Cursor Speed.
    bool cursor_hover = false;
    {
        const CursorSample& c = frame.cursor;
        CursorInput in;
        in.live = cursor_on;
        in.delta = {c.motion.x * c.gain, c.motion.y * c.gain};
        in.down = c.down;
        in.recentre = c.recentre;
        CursorStep(out, cursor_, in, TouchSource::Cursor, frame.time_ns);
        cursor_hover =
            cursor_on && (frame.time_ns - cursor_.move_ns < kMouseIdleNs || (in.live && in.down));
    }

    // The mouse: the left button touches in both modes (a hover is not a contact).
    bool mouse_hover = false;
    {
        const MouseSample& m = frame.mouse;
        const bool inside = on && m.inside;
        const bool engaged = inside && m.down;
        if (m.inside) {
            mouse_.px = ToPx(Clamp01(m.x), kBottomScreenWidthPx);
            mouse_.py = ToPx(Clamp01(m.y), kBottomScreenHeightPx);
            if (!mouse_seen_) {
                // First seen (the game just started, or the menu closed): not a move.
                mouse_move_ns_ = frame.time_ns - kMouseIdleNs;
            } else if (m.x != mouse_last_x_ || m.y != mouse_last_y_) {
                mouse_move_ns_ = frame.time_ns;
            }
            mouse_last_x_ = m.x;
            mouse_last_y_ = m.y;
            mouse_seen_ = true;
        }
        mouse_hover = inside && frame.time_ns - mouse_move_ns_ < kMouseIdleNs;
        if (Engage(mouse_, engaged)) {
            Take(out, TouchSource::Mouse, mouse_.px, mouse_.py);
        } else if (engaged) {
            Follow(out, TouchSource::Mouse, mouse_.px, mouse_.py);
        }
        if (!engaged) {
            Drop(out, TouchSource::Mouse);
        }
    }

    // The dot: where the touch is, or where it would be (pad contact in Click mode, the pointer).
    TouchpadCursor& c = out.cursor;
    if ((on || cursor_on) && owner_ != TouchSource::Beam) {
        const bool pad_hover = frame.pad.present && (frame.pad.contact || frame.pad.click);
        TouchSource src = TouchSource::None;
        if (owner_ == TouchSource::Pad || owner_ == TouchSource::Mouse ||
            owner_ == TouchSource::Cursor) {
            src = owner_;
        } else if (pad_hover) {
            src = TouchSource::Pad;
        } else if (cursor_hover) {
            src = TouchSource::Cursor;
        } else if (mouse_hover) {
            src = TouchSource::Mouse;
        }
        if (src != TouchSource::None) {
            const Source& s = src == TouchSource::Pad     ? pad_.src
                              : src == TouchSource::Mouse ? mouse_
                                                          : cursor_.src;
            c.visible = true;
            c.source = src;
            c.px = s.px;
            c.py = s.py;
            c.u = s.px / kBottomScreenWidthPx;
            c.v = s.py / kBottomScreenHeightPx;
            c.pressed = owner_ == src;
        }
    }
    return out;
}

bool TouchpadTouch::Reset() {
    const bool was = touching_;
    owner_ = TouchSource::None;
    touching_ = false;
    pad_ = CursorTrack{};
    mouse_ = Source{};
    mouse_seen_ = false;
    cursor_ = CursorTrack{};
    return was;
}

DotPose DotOnQuad(const PanelQuad& quad, float u, float v, const Vec3f& eye) {
    DotPose dot;
    float lx = (u - 0.5f) * quad.width_m;
    float lz = 0.0f;
    Quatf turn;
    if (quad.curve_radius > 0.0f) {
        // The quad bent about the line x = 0, z = R (geometry.cpp, IntersectQuad).
        const float r = quad.curve_radius;
        const float angle = lx / r;
        lx = r * std::sin(angle);
        lz = r * (1.0f - std::cos(angle));
        turn = {0.0f, std::sin(-angle * 0.5f), 0.0f, std::cos(-angle * 0.5f)};
    }
    const Vec3f local{lx, (0.5f - v) * quad.height_m, lz};
    dot.position = Transform(quad.pose, local);
    dot.orientation = Normalized(Multiply(quad.pose.orientation, turn));
    dot.size_m = 0.01f + 0.003f * Length(dot.position - eye);
    return dot;
}

} // namespace FrameXr::Input
