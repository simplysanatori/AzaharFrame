// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// The headset keyboard: a keyboard in VR under Azahar's window (the home panel and the in-game
// menu), the Compact keyboard of Moonlight XR (same author). Y on the right controller shows or
// hides it while the window shows. The XR side (keyboard_panel.cpp) draws it and turns presses
// into the events below; the Qt side (citra_qt/frame_mirror.cpp) types them into the focused
// widget. No Qt and no OpenXR in this header.

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace FrameXr {

/// One key typed on the headset keyboard: pressed and let go.
struct KeyboardEvent {
    enum class Key : uint8_t {
        Character, ///< `character` (letters, digits, punctuation, space)
        Backspace,
        Tab,
        Enter,
        Escape,
        Insert,
        Delete,
        Home,
        End,
        PageUp,
        PageDown,
        Left,
        Up,
        Right,
        Down,
        F1, ///< F1 .. F12 follow in order
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
    };
    /// Modifier bits held with the key. Shift is set for a capital and for a character typed
    /// with Shift on a US keyboard ("!", "?"), so shortcuts see what a real keyboard sends.
    static constexpr uint8_t kShift = 1;
    static constexpr uint8_t kCtrl = 2;
    static constexpr uint8_t kAlt = 4;
    static constexpr uint8_t kMeta = 8; ///< the keyboard's Win key

    Key key = Key::Character;
    char32_t character = 0; ///< Character: what it types (ASCII, US layout)
    uint8_t modifiers = 0;
    std::string text; ///< UTF-8 text the key types, as a real keyboard sends it ("" for none)
};

/// Where typed keys go (called on the XR thread: queue to the GUI thread). Empty: no headset
/// keyboard (Y does nothing). Any thread.
void SetKeyboardHandler(std::function<void(const KeyboardEvent&)> on_key);

/// A click put the keys in a text field: the keyboard opens if it is not open. Any thread.
void OpenKeyboardForTextField();
/// No text field has the keys any more: the keyboard closes if OpenKeyboardForTextField opened
/// it (not if Y did). Any thread.
void TextFieldFocusLost();

} // namespace FrameXr
