// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include "frame_xr/input/key_names.h"
#include "frame_xr/input/types.h"

// AzaharFrame keys
// A physical keyboard (USB or Bluetooth) as a game control device in VR, pure logic (no evdev,
// OpenXR or settings): the source reads every keyboard's keys (KeyboardRaw: what is down, and a
// running count of presses so a tap shorter than a frame still counts), KeyboardLogic turns them
// into one frame (KeyboardFrame: the keys held for the game, Open Menu). The hub (gamepads.h)
// gives the game the 3DS buttons and sticks of the keyboard's mapping, ORed with every other
// device's.
//
// Typing and playing: while the game's keyboard is open the keys type into it (phys_keyboard.h)
// and none reaches the game or opens the menu; what is held when typing ends stays away from the
// game until let go. Nothing is grabbed: the desktop keeps the keyboard.
namespace FrameXr::Input {

/// The keyboards' keys as running totals, so each reader takes its own presses.
struct KeyboardRaw {
    bool present = false; ///< a keyboard is connected (and counts)
    std::array<bool, kNumKeys> down{};
    std::array<uint32_t, kNumKeys> presses{}; ///< key downs since the source started
};

/// One display frame of the keyboard.
struct KeyboardFrame {
    bool present = false;
    /// The keys held for the game, after a release (what is held then stays away until let go);
    /// a key pressed and let go between two frames is held for one.
    std::array<bool, kNumKeys> held{};
    bool open_menu = false; ///< the Open Menu key went down
    bool used = false;      ///< a key went down this frame
    bool disabled = false;  ///< a keyboard is there, with Use This Device off
    bool Held(int key) const {
        return key > 0 && key < kNumKeys && held[static_cast<std::size_t>(key)];
    }
};

class KeyboardLogic {
public:
    /// `open_menu_key` is the mapping's Open Menu key (-1 none).
    KeyboardFrame Update(const KeyboardRaw& raw, int open_menu_key);
    /// Everything held now stays away until let go (the menu closed, typing, a key just learnt
    /// in Edit Controls).
    void Release();
    /// One key stays away until let go.
    void Swallow(int key);

private:
    bool seen_ = false;
    KeyboardRaw last_;
    std::array<bool, kNumKeys> swallowed_{};
    bool open_menu_prev_ = false;
};

/// Azahar's analog from buttons: a stick from its four keys (up, down, left, right; -1 none),
/// a diagonal at full tilt along each axis times sqrt(1/2), all of it times kKeyModifierScale
/// while the modifier key is held. Up and right positive.
constexpr float kKeyModifierScale = 0.5f;
Vec2f KeyStickValue(const std::array<bool, kNumKeys>& held, const std::array<int8_t, 4>& keys,
                    bool modifier);

} // namespace FrameXr::Input
