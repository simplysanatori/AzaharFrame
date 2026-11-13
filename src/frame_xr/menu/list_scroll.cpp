// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame scroll: list_scroll.h.

#include "frame_xr/menu/list_scroll.h"

#include <algorithm>
#include <cmath>

namespace FrameXr::Menu::Scrolling {

namespace {
constexpr float kStickFull = 0.95f; ///< a Frame stick pushed all the way reads about this
} // namespace

// ---- The stick ----

float StickSpeed(float stick_y, float speed) {
    const float a = std::fabs(stick_y);
    if (a <= kStickDead) {
        return 0.0f;
    }
    const float m = std::clamp((a - kStickDead) / (kStickFull - kStickDead), 0.0f, 1.0f);
    return std::pow(m, kStickCurve) * speed;
}

float HeldSpeed(float held, float view_h) {
    const float r = std::clamp((held - kAccelDelay) / (kAccelFull - kAccelDelay), 0.0f, 1.0f);
    const float ease = r * r * (3.0f - 2.0f * r); // smoothstep: no jolt at either end
    const float top = std::max(kStartSpeed, kScreensPerSecond * view_h);
    return kStartSpeed + (top - kStartSpeed) * ease;
}

float StickScroll::Update(float stick_y, float dt, float scroll, float max_scroll, float view_h,
                          float step) {
    max_scroll = std::max(0.0f, max_scroll);
    const float a = std::fabs(stick_y);
    const int want = a <= kStickDead ? 0 : stick_y > 0.0f ? -1 : 1; // stick up: towards the top
    if (want != dir) {
        // A push ended (or turned round). A short tap moves one whole row, as a wheel notch.
        if (dir != 0 && held < kTapTime && peak >= kTapPush && step > 0.0f) {
            const float moved = (scroll - start) * static_cast<float>(dir);
            if (moved < step) {
                scroll = start + step * static_cast<float>(dir);
            }
        }
        dir = want;
        held = accel = speed = peak = 0.0f;
        start = scroll;
        if (dir == 0) {
            return std::clamp(scroll, 0.0f, max_scroll);
        }
    }
    held += dt;
    peak = std::max(peak, a);
    // The build-up only while pushed far over: a gentle push reads at a gentle speed for good.
    accel = a >= kAccelPush ? accel + dt : std::max(0.0f, accel - 2.0f * dt);
    const float target = StickSpeed(stick_y, HeldSpeed(accel, view_h));
    if (target <= speed || dt <= 0.0f) {
        speed = target; // easing off the stick slows at once: nothing drifts on
    } else {
        speed += (target - speed) * (1.0f - std::exp(-dt / kRiseTau));
    }
    return std::clamp(scroll + static_cast<float>(dir) * speed * dt, 0.0f, max_scroll);
}

// ---- The scrollbar ----

float TrackY0(const Bar& b) {
    return b.list.y0 + kTrackInset;
}

float TrackY1(const Bar& b) {
    return b.list.y1 - kTrackInset;
}

float ThumbH(const Bar& b) {
    const float track = std::max(0.0f, TrackY1(b) - TrackY0(b));
    if (b.content_h <= 0.0f) {
        return track;
    }
    return std::clamp(track * b.list.H() / b.content_h, std::min(kThumbMin, track), track);
}

float ThumbY(const Bar& b, float scroll) {
    const float room = TrackY1(b) - TrackY0(b) - ThumbH(b);
    const float t = b.max_scroll > 0.0f ? std::clamp(scroll / b.max_scroll, 0.0f, 1.0f) : 0.0f;
    return TrackY0(b) + std::max(0.0f, room) * t;
}

Rect HitRect(const Bar& b) {
    return {b.right - kHitLeft, b.list.y0, b.right + kHitRight, b.list.y1};
}

Rect ThumbRect(const Bar& b, float scroll, bool wide) {
    const float w = wide ? kBarHoverW : kBarW;
    const float y = ThumbY(b, scroll);
    return {b.right - kBarRight - w, y, b.right - kBarRight, y + ThumbH(b)};
}

Rect TrackRect(const Bar& b, bool wide) {
    const float w = wide ? kBarHoverW : kBarW;
    return {b.right - kBarRight - w, TrackY0(b), b.right - kBarRight, TrackY1(b)};
}

float ScrollForThumb(const Bar& b, float thumb_y) {
    const float room = TrackY1(b) - TrackY0(b) - ThumbH(b);
    if (room <= 0.0f || b.max_scroll <= 0.0f) {
        return 0.0f;
    }
    return std::clamp((thumb_y - TrackY0(b)) / room, 0.0f, 1.0f) * b.max_scroll;
}

int TrackSide(const Bar& b, float scroll, float y) {
    const float y0 = ThumbY(b, scroll);
    if (y < y0) {
        return -1;
    }
    return y >= y0 + ThumbH(b) ? 1 : 0;
}

float PageFrom(const Bar& b, float scroll, int dir) {
    return std::clamp(scroll + b.list.H() * static_cast<float>(dir), 0.0f,
                      std::max(0.0f, b.max_scroll));
}

float BarPress::Begin(const Bar& b, float scroll, float y) {
    const int side = TrackSide(b, scroll, y);
    if (side == 0) {
        mode = Mode::Thumb;
        grab = y - ThumbY(b, scroll);
        return scroll;
    }
    mode = Mode::Track;
    dir = side;
    wait = kPageRepeatDelay;
    return PageFrom(b, scroll, dir);
}

float BarPress::Update(const Bar& b, float scroll, float y, float dt) {
    switch (mode) {
    case Mode::Thumb:
        return ScrollForThumb(b, y - grab);
    case Mode::Track:
        // Held on the track, it pages on until the thumb comes under the beam.
        wait -= dt;
        if (wait <= 0.0f) {
            wait = kPageRepeat;
            if (TrackSide(b, scroll, y) == dir) {
                return PageFrom(b, scroll, dir);
            }
        }
        return scroll;
    case Mode::None:
        break;
    }
    return scroll;
}

// ---- Dragging the list itself ----

void DragScroll::Press(float y, float scroll, float t) {
    pressed = true;
    dragging = false;
    y0 = y;
    scroll0 = scroll;
    fling = 0.0f;
    count = 0;
    head = 0;
    (void)t;
}

void DragScroll::Add(float t, float scroll) {
    samples[static_cast<std::size_t>(head)] = {t, scroll};
    head = (head + 1) % static_cast<int>(samples.size());
    count = std::min(count + 1, static_cast<int>(samples.size()));
}

bool DragScroll::Move(float y, float t, float& scroll, float max_scroll) {
    if (!pressed) {
        return false;
    }
    if (!dragging) {
        if (std::fabs(y - y0) <= kDragSlop) {
            return false;
        }
        // Past the slop: from here the list follows the beam, without a jump.
        dragging = true;
        y0 = y;
        scroll0 = scroll;
    }
    scroll = std::clamp(scroll0 - (y - y0), 0.0f, std::max(0.0f, max_scroll));
    Add(t, scroll);
    return true;
}

float DragScroll::Velocity() const {
    if (count < 2) {
        return 0.0f;
    }
    const int n = static_cast<int>(samples.size());
    const Sample& last = samples[static_cast<std::size_t>((head - 1 + n) % n)];
    const Sample* first = &last;
    for (int i = 2; i <= count; i++) {
        const Sample& s = samples[static_cast<std::size_t>((head - i + n) % n)];
        if (last.t - s.t > kVelocityWindow) {
            break;
        }
        first = &s;
    }
    const float span = last.t - first->t;
    return span > 0.02f ? (last.scroll - first->scroll) / span : 0.0f;
}

void DragScroll::Release(float t) {
    (void)t;
    if (dragging) {
        const float v = Velocity();
        fling = std::fabs(v) >= kFlingMin ? std::clamp(v, -kFlingMax, kFlingMax) : 0.0f;
    }
    pressed = dragging = false;
}

float DragScroll::Coast(float scroll, float max_scroll, float dt) {
    if (fling == 0.0f || dt <= 0.0f) {
        return scroll;
    }
    max_scroll = std::max(0.0f, max_scroll);
    const float next = scroll + fling * dt;
    fling *= std::exp(-dt / kFlingTau);
    if (std::fabs(fling) < kFlingStop || next <= 0.0f || next >= max_scroll) {
        fling = 0.0f; // slowed down, or at an end: it stops there, no bounce
    }
    return std::clamp(next, 0.0f, max_scroll);
}

} // namespace FrameXr::Menu::Scrolling
