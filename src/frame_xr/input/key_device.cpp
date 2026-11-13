// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/input/key_device.h"

// AzaharFrame keys
namespace FrameXr::Input {

KeyboardFrame KeyboardLogic::Update(const KeyboardRaw& raw, int open_menu_key) {
    KeyboardFrame out;
    out.present = raw.present;
    if (!seen_) {
        // The first sample (or a keyboard that came back): its counts are the start, its held
        // keys no press.
        seen_ = true;
        last_ = raw;
        swallowed_ = raw.down;
    }
    for (std::size_t k = 1; k < static_cast<std::size_t>(kNumKeys); k++) {
        const bool pressed = raw.presses[k] != last_.presses[k];
        out.used = out.used || pressed;
        if (raw.down[k]) {
            out.held[k] = !swallowed_[k];
        } else {
            // Up now: a press since the last frame (a quick tap) still counts once.
            out.held[k] = pressed && !swallowed_[k];
            swallowed_[k] = false;
        }
    }
    last_ = raw;
    if (!raw.present) {
        seen_ = false;
    }
    const bool menu_now = out.Held(open_menu_key);
    out.open_menu = menu_now && !open_menu_prev_;
    open_menu_prev_ = menu_now;
    return out;
}

void KeyboardLogic::Release() {
    swallowed_ = last_.down;
    open_menu_prev_ = false;
}

void KeyboardLogic::Swallow(int key) {
    if (key > 0 && key < kNumKeys) {
        swallowed_[static_cast<std::size_t>(key)] = true;
    }
}

Vec2f KeyStickValue(const std::array<bool, kNumKeys>& held, const std::array<int8_t, 4>& keys,
                    bool modifier) {
    const auto on = [&held](int8_t key) {
        return key > 0 && key < kNumKeys && held[static_cast<std::size_t>(key)];
    };
    Vec2f v{(on(keys[3]) ? 1.0f : 0.0f) - (on(keys[2]) ? 1.0f : 0.0f),
            (on(keys[0]) ? 1.0f : 0.0f) - (on(keys[1]) ? 1.0f : 0.0f)};
    if (v.x != 0.0f && v.y != 0.0f) {
        constexpr float kSqrtHalf = 0.70710678f;
        v.x *= kSqrtHalf;
        v.y *= kSqrtHalf;
    }
    if (modifier) {
        v.x *= kKeyModifierScale;
        v.y *= kKeyModifierScale;
    }
    return v;
}

} // namespace FrameXr::Input
