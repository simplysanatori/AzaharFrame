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
    // AzaharFrame padsui: with Recentre at Edge, a drag that reaches an edge lifts and touches
    // again at the middle; from there the pad moves the touch by its motion (an offset) until
    // the finger lets go.
    {
        const PadSample& pad = frame.pad;
        const bool has_point = pad.present && (pad.contact || pad.click);
        const bool engaged =
            on && pad.present &&
            (frame.mode == TouchpadMode::Touch ? pad.contact : pad.click && has_point);
        if (!engaged) {
            pad_offset_ = {};
            pad_gap_ = false;
        }
        if (has_point) {
            pad_.px = ToPx(Clamp01(pad.x + pad_offset_.x), kBottomScreenWidthPx);
            pad_.py = ToPx(Clamp01(pad.y + pad_offset_.y), kBottomScreenHeightPx);
        }
        const float ux = pad.x + pad_offset_.x, uy = pad.y + pad_offset_.y;
        const bool at_edge = ux <= kPadEdge || ux >= 1.0f - kPadEdge || uy <= kPadEdge ||
                             uy >= 1.0f - kPadEdge;
        const bool edge = Engage(pad_, engaged);
        if (!engaged) {
            Drop(out, TouchSource::Pad);
        } else if (pad_gap_) {
            if (frame.time_ns >= pad_gap_until_) {
                pad_gap_ = false;
                Take(out, TouchSource::Pad, pad_.px, pad_.py);
            }
        } else if (edge) {
            Take(out, TouchSource::Pad, pad_.px, pad_.py);
        } else if (frame.pad_recentre && owner_ == TouchSource::Pad && touching_ && at_edge &&
                   !pad_at_edge_) {
            Drop(out, TouchSource::Pad);
            pad_offset_ = {0.5f - pad.x, 0.5f - pad.y};
            pad_.px = ToPx(0.5f, kBottomScreenWidthPx);
            pad_.py = ToPx(0.5f, kBottomScreenHeightPx);
            pad_gap_ = true;
            pad_gap_until_ = frame.time_ns + kRecentreGapNs;
        } else {
            Follow(out, TouchSource::Pad, pad_.px, pad_.py);
        }
        pad_at_edge_ = engaged && at_edge && !pad_gap_;
    }

    // AzaharFrame padsui: the mouse in Bottom Screen mode: its own cursor, moved by the counts
    // and kept on the screen; the touch input touches there, held it drags. With Recentre at
    // Edge, a drag pushed past an edge lifts and touches again at the middle.
    bool cursor_hover = false;
    {
        const CursorSample& c = frame.cursor;
        const bool live = cursor_on;
        if (!cursor_started_) {
            cursor_.px = kBottomScreenWidthPx * 0.5f;
            cursor_.py = kBottomScreenHeightPx * 0.5f;
            cursor_started_ = true;
        }
        bool pushed_out = false;
        if (live && (c.motion.x != 0.0f || c.motion.y != 0.0f)) {
            const float nx = cursor_.px + c.motion.x * c.gain;
            const float ny = cursor_.py + c.motion.y * c.gain;
            const float max_x = std::nextafter(kBottomScreenWidthPx, 0.0f);
            const float max_y = std::nextafter(kBottomScreenHeightPx, 0.0f);
            pushed_out = nx < 0.0f || nx > max_x || ny < 0.0f || ny > max_y;
            cursor_.px = std::clamp(nx, 0.0f, max_x);
            cursor_.py = std::clamp(ny, 0.0f, max_y);
            cursor_move_ns_ = frame.time_ns;
        }
        const bool engaged = live && c.down;
        const bool edge = Engage(cursor_, engaged);
        if (!engaged) {
            Drop(out, TouchSource::Cursor);
            cursor_gap_ = false;
        } else if (cursor_gap_) {
            if (frame.time_ns >= cursor_gap_until_) {
                cursor_gap_ = false;
                Take(out, TouchSource::Cursor, cursor_.px, cursor_.py);
            }
        } else if (edge) {
            Take(out, TouchSource::Cursor, cursor_.px, cursor_.py);
        } else if (c.recentre && pushed_out && owner_ == TouchSource::Cursor && touching_) {
            Drop(out, TouchSource::Cursor);
            cursor_.px = kBottomScreenWidthPx * 0.5f;
            cursor_.py = kBottomScreenHeightPx * 0.5f;
            cursor_gap_ = true;
            cursor_gap_until_ = frame.time_ns + kRecentreGapNs;
        } else {
            Follow(out, TouchSource::Cursor, cursor_.px, cursor_.py);
        }
        cursor_hover = live && (frame.time_ns - cursor_move_ns_ < kMouseIdleNs || engaged);
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
            const Source& s = src == TouchSource::Pad     ? pad_
                              : src == TouchSource::Mouse ? mouse_
                                                          : cursor_;
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
    pad_ = Source{};
    mouse_ = Source{};
    mouse_seen_ = false;
    cursor_ = Source{};
    cursor_started_ = false;
    cursor_gap_ = false;
    pad_gap_ = false;
    pad_offset_ = {};
    pad_at_edge_ = false;
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
