// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "frame_xr/input/mouse.h"

// AzaharFrame padsui
namespace FrameXr::Input {

const char* MouseInputName(int input) {
    switch (static_cast<MouseInput>(input)) {
    case MouseInput::LeftClick:
        return "Left Click";
    case MouseInput::RightClick:
        return "Right Click";
    case MouseInput::MiddleClick:
        return "Middle Click";
    case MouseInput::Back:
        return "Back";
    case MouseInput::Forward:
        return "Forward";
    case MouseInput::WheelUp:
        return "Wheel Up";
    case MouseInput::WheelDown:
        return "Wheel Down";
    default:
        return "";
    }
}

float MouseScreenGain(int speed) {
    // A mouse of 800 to 1600 counts per inch: an inch or so across the screen at Normal.
    return speed <= 0 ? 0.15f : speed == 1 ? 0.3f : 0.6f;
}

float MousePointerGain(int speed) {
    return speed <= 0 ? 0.015f : speed == 1 ? 0.03f : 0.06f;
}

// ---- MouseLogic -----------------------------------------------------------------------------

bool MouseLogic::Step(Notches& n, int64_t added, int64_t now) {
    n.queued = std::min<int64_t>(n.queued + std::max<int64_t>(added, 0), kMaxNotches);
    if (n.on && now >= n.until) {
        n.on = false;
        n.until = now + kNotchOffNs;
    }
    if (!n.on && now >= n.until && n.queued > 0) {
        n.queued--;
        n.on = true;
        n.until = now + kNotchOnNs;
    }
    return n.on;
}

MouseFrame MouseLogic::Update(const MouseRaw& raw, int open_menu_input, int64_t now) {
    MouseFrame out;
    out.present = raw.present;
    if (!seen_) {
        // The first sample (or a mouse that came back): its totals are the start, its held
        // buttons no press.
        seen_ = true;
        last_ = raw;
        swallowed_ = raw.buttons;
    }
    out.motion = {static_cast<float>(raw.x - last_.x), static_cast<float>(raw.y - last_.y)};
    const int64_t notches_up = raw.wheel_up - last_.wheel_up;
    const int64_t notches_down = raw.wheel_down - last_.wheel_down;
    const std::array<bool, kNumMouseButtons> prev = last_.buttons;
    last_ = raw;
    if (!raw.present) {
        seen_ = false;
    }

    std::array<bool, kNumMouseButtons> down = raw.buttons;
    bool edge_any = false;
    for (int i = 0; i < kNumMouseButtons; i++) {
        const std::size_t k = static_cast<std::size_t>(i);
        edge_any = edge_any || (down[k] && !prev[k]);
    }

    // Left and Right: the chord, or a click a moment late.
    for (int i = 0; i < 2; i++) {
        const std::size_t k = static_cast<std::size_t>(i);
        const std::size_t j = 1 - k;
        const bool edge = down[k] && !prev[k];
        if (edge && !swallowed_[k]) {
            if (pending_since_[j] != 0 && now - pending_since_[j] <= kChordWindowNs) {
                // The other one is waiting: the chord. Neither reaches anything.
                chord_ = true;
                chord_fired_ = false;
                chord_since_ = now;
                pending_since_ = {};
                tap_until_ = {};
                swallowed_[0] = swallowed_[1] = true;
            } else if (down[j] && prev[j] && !swallowed_[j] && pending_since_[j] == 0) {
                // The other one is a press already going on: a plain press, at once.
            } else {
                pending_since_[k] = now;
            }
        }
    }
    for (int i = 0; i < 2; i++) {
        const std::size_t k = static_cast<std::size_t>(i);
        if (pending_since_[k] == 0) {
            continue;
        }
        if (!down[k]) {
            pending_since_[k] = 0; // let go while it waited: a short press all the same
            tap_until_[k] = now + kTapNs;
        } else if (now - pending_since_[k] > kChordWindowNs) {
            pending_since_[k] = 0; // no chord: the click goes on
        }
    }
    if (chord_) {
        if (down[0] && down[1]) {
            if (!chord_fired_) {
                const int64_t held = now - chord_since_;
                out.chord = std::clamp(static_cast<float>(held) / static_cast<float>(kChordHoldNs),
                                       0.0f, 1.0f);
                if (held >= kChordHoldNs) {
                    chord_fired_ = true;
                    out.switch_mode = true;
                    out.chord = 0.0f;
                }
            }
        } else {
            chord_ = false; // one let go: over (the other stays away until let go too)
        }
    }

    for (int i = 0; i < kNumMouseButtons; i++) {
        const std::size_t k = static_cast<std::size_t>(i);
        if (!down[k]) {
            swallowed_[k] = false;
        }
        bool held = down[k] && !swallowed_[k];
        if (i < 2) {
            held = (held && pending_since_[k] == 0) || now < tap_until_[k];
        }
        out.held[k] = held;
    }
    if (release_wheel_) {
        release_wheel_ = false;
        up_ = {};
        down_ = {};
    } else {
        out.held[static_cast<std::size_t>(MouseInput::WheelUp)] = Step(up_, notches_up, now);
        out.held[static_cast<std::size_t>(MouseInput::WheelDown)] = Step(down_, notches_down, now);
    }

    const bool menu_now = open_menu_input >= 0 && open_menu_input < kNumMouseInputs &&
                          out.held[static_cast<std::size_t>(open_menu_input)];
    out.open_menu = menu_now && !open_menu_prev_;
    open_menu_prev_ = menu_now;
    out.used = out.motion.x != 0.0f || out.motion.y != 0.0f || edge_any || notches_up > 0 ||
               notches_down > 0;
    return out;
}

void MouseLogic::Release() {
    swallowed_ = last_.buttons;
    pending_since_ = {};
    tap_until_ = {};
    chord_ = false;
    release_wheel_ = true;
}

void MouseLogic::Swallow(int input) {
    if (input >= 0 && input < kNumMouseButtons) {
        swallowed_[static_cast<std::size_t>(input)] = true;
        if (input < 2) {
            pending_since_[static_cast<std::size_t>(input)] = 0;
            tap_until_[static_cast<std::size_t>(input)] = 0;
        }
    } else if (input == static_cast<int>(MouseInput::WheelUp) ||
               input == static_cast<int>(MouseInput::WheelDown)) {
        release_wheel_ = true;
    }
}

void ApplyMousePointer(InputFrame& in, int hand, const Posef& aim, bool trigger, float stick_y) {
    if (hand < 0 || hand >= kNumHands) {
        return;
    }
    // The aim and the trigger are the mouse's; the controller's other inputs, if it is in a
    // hand, stay its own.
    HandInput& h = in.hand[static_cast<std::size_t>(hand)];
    if (!h.active) {
        h = HandInput{};
    }
    h.active = true;
    h.aim = aim;
    h.trigger = trigger ? 1.0f : 0.0f;
    if (stick_y != 0.0f) {
        h.stick = {0.0f, stick_y};
    }
    h.mouse = true;
}

} // namespace FrameXr::Input
