// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include <string>

// AzaharFrame keys
// A physical keyboard's keys as a game control device: by Linux key code (KEY_* of
// input-event-codes.h), not by layout, so a key keeps its binding whatever the keyboard's layout
// setting. The names follow the US key caps. Header only: the menu (Edit Controls) and the input
// code both use it, and the menu links no input sources.
namespace FrameXr::Input {

/// Key codes 1 to 127: every key of a full-size keyboard (F13 and up and the media keys are
/// above and are not used). The same numbers are a keyboard mapping's values.
constexpr int kNumKeys = 128;

/// Linux key codes the defaults and the tests use.
namespace Key {
enum : int {
    Esc = 1,
    Num1 = 2,
    Num2 = 3,
    Q = 16,
    W = 17,
    T = 20,
    I = 23,
    O = 24,
    P = 25,
    Enter = 28,
    A = 30,
    S = 31,
    D = 32,
    F = 33,
    G = 34,
    H = 35,
    J = 36,
    K = 37,
    L = 38,
    LeftShift = 42,
    Z = 44,
    X = 45,
    V = 47,
    B = 48,
    N = 49,
    M = 50,
    Space = 57,
    Up = 103,
    Left = 105,
    Right = 106,
    Down = 108,
};
} // namespace Key

/// The name of a key as Edit Controls shows it ("A", "Esc", "Left Shift", "Up Arrow", "Keypad 8"),
/// or "" for a code that is not a key that can be bound (0, the volume and power keys, codes
/// with no key).
inline const char* KeyName(int code) {
    static constexpr const char* kNames[] = {
        "", "Esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Backspace", "Tab",
        // 16
        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Left Ctrl", "A", "S",
        // 32
        "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", "Left Shift", "\\", "Z", "X", "C", "V",
        // 48
        "B", "N", "M", ",", ".", "/", "Right Shift", "Keypad *", "Left Alt", "Space", "Caps Lock",
        "F1", "F2", "F3", "F4", "F5",
        // 64
        "F6", "F7", "F8", "F9", "F10", "Num Lock", "Scroll Lock", "Keypad 7", "Keypad 8",
        "Keypad 9", "Keypad -", "Keypad 4", "Keypad 5", "Keypad 6", "Keypad +", "Keypad 1",
        // 80
        "Keypad 2", "Keypad 3", "Keypad 0", "Keypad .", "", "Hankaku", "Non-US \\", "F11", "F12",
        "Ro", "Katakana", "Hiragana", "Henkan", "Kana", "Muhenkan", "Keypad JP ,",
        // 96
        "Keypad Enter", "Right Ctrl", "Keypad /", "Print Screen", "Right Alt", "", "Home",
        "Up Arrow", "Page Up", "Left Arrow", "Right Arrow", "End", "Down Arrow", "Page Down",
        "Insert", "Delete",
        // 112: Macro, Mute, Volume Down, Volume Up, Power are the system's
        "", "", "", "", "", "Keypad =", "", "Pause", "", "Keypad ,", "Hangul", "Hanja", "Yen",
        "Left Meta", "Right Meta", "Menu Key"};
    static_assert(sizeof(kNames) / sizeof(kNames[0]) == kNumKeys);
    return code > 0 && code < kNumKeys ? kNames[code] : "";
}

/// A key a keyboard mapping may hold (it has a name).
inline bool BindableKey(int code) {
    return KeyName(code)[0] != '\0';
}

/// The four keys of a 3DS stick (up, down, left, right; -1 none) for a chip: "Arrow Keys" for
/// the arrows, else the keys in the order up, left, down, right ("W A S D", "I J K L"; names of
/// more than one character with commas), "–" for a direction with no key, "" for none at all.
inline std::string KeyStickName(const std::array<int8_t, 4>& keys) {
    if (keys[0] == Key::Up && keys[1] == Key::Down && keys[2] == Key::Left &&
        keys[3] == Key::Right) {
        return "Arrow Keys";
    }
    bool any = false;
    bool short_names = true;
    for (const int8_t k : keys) {
        if (BindableKey(k)) {
            any = true;
            short_names = short_names && KeyName(k)[1] == '\0';
        }
    }
    if (!any) {
        return {};
    }
    std::string out;
    for (const std::size_t i : {0u, 2u, 1u, 3u}) {
        if (!out.empty()) {
            out += short_names ? " " : ", ";
        }
        out += BindableKey(keys[i]) ? KeyName(keys[i]) : "\xE2\x80\x93";
    }
    return out;
}

} // namespace FrameXr::Input
