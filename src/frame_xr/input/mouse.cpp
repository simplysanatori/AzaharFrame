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

float CursorSpeedScale(int speed) {
    static constexpr float kScale[kNumCursorSpeeds] = {0.5f, 0.7f, 1.0f, 1.4f, 2.0f};
    return kScale[std::clamp(speed, 0, kNumCursorSpeeds - 1)];
}

float MouseScreenGain(int speed) {
    // A mouse of 800 to 1600 counts per inch: an inch or so across the screen at Normal.
    return 0.3f * CursorSpeedScale(speed);
}

float MousePointerGain(int speed) {
    return 0.03f * CursorSpeedScale(speed);
}

int MousePointerHand(const InputFrame& in) {
    const int right = HandIndex(Hand::Right);
    const int left = HandIndex(Hand::Left);
    if (!in.hand[static_cast<std::size_t>(right)].active) {
        return right;
    }
    return in.hand[static_cast<std::size_t>(left)].active ? right : left;
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

    // Left and Right go to the game at once. The other one going down within kChordWindowNs is
    // the chord: both are let go for the game there and then, and the ring starts.
    for (int i = 0; i < 2; i++) {
        const std::size_t k = static_cast<std::size_t>(i);
        const std::size_t j = 1 - k;
        if (!down[k]) {
            down_since_[k] = 0;
            continue;
        }
        if (prev[k] || swallowed_[k]) {
            continue;
        }
        if (down_since_[j] != 0 && now - down_since_[j] <= kChordWindowNs) {
            chord_ = true;
            chord_fired_ = false;
            chord_since_ = now;
            down_since_ = {};
            swallowed_[0] = swallowed_[1] = true;
        } else {
            down_since_[k] = now;
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
        out.held[k] = down[k] && !swallowed_[k];
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
    down_since_ = {};
    chord_ = false;
    release_wheel_ = true;
}

void MouseLogic::Swallow(int input) {
    if (input >= 0 && input < kNumMouseButtons) {
        swallowed_[static_cast<std::size_t>(input)] = true;
        if (input < 2) {
            down_since_[static_cast<std::size_t>(input)] = 0;
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
