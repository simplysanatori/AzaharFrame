// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// AzaharFrame input2
// A physical keyboard (USB or Bluetooth, such as a Keychron Q1 Pro) typing into the game's
// keyboard in VR. In VR no window of the app holds the keyboard focus under gamescope (SteamVR
// is in front), so Qt never sees a key; the keyboards are read from their event nodes instead,
// as the mouse is (devices.cpp), and only while the game's keyboard is open. Nothing is grabbed:
// the desktop keeps the keyboard.
//
// KeyTranslator is the pure part (no evdev, tested anywhere): Linux key codes to typing, by the
// US layout (the keyboard's own layout setting is not known here), Shift and Caps Lock, the
// keypad with Num Lock. Ctrl, Alt and Meta held type nothing (shortcuts), but the editing keys
// still work. A key held from before reading started never repeats.
//   letters, digits, symbols, Space  text
//   Backspace, Delete, Left, Right, Home, End   editing (they repeat while held)
//   Enter, keypad Enter              the confirm button (OK); Shift+Enter a new line
//   Esc                              the game's cancel button
namespace FrameXr::Input {

enum class KeyActionKind : uint8_t {
    None,
    Text,
    Backspace,
    Delete,
    Left,
    Right,
    Home,
    End,
    Enter,
    NewLine,
    Escape,
};

struct KeyAction {
    KeyActionKind kind = KeyActionKind::None;
    std::string text; ///< UTF-8, for Text
    bool operator==(const KeyAction&) const = default;
};

class KeyTranslator {
public:
    /// One key event: `code` a Linux KEY_* code, `value` 0 up, 1 down, 2 the kernel's repeat.
    KeyAction Key(int code, int value);
    /// The lock keys' state as the keyboard's LEDs show it when reading starts.
    void SetLocks(bool caps_lock, bool num_lock);
    /// Everything let go (reading stopped).
    void Reset();

    bool caps_lock() const {
        return caps_;
    }

private:
    bool Shift() const;
    bool Command() const; ///< Ctrl, Alt or Meta held

    std::array<bool, 256> down_{};
    bool caps_ = false;
    bool num_ = true;
};

/// The character a key types by the US layout (`shift` the effective shift for letters already
/// includes Caps Lock), or 0.
char UsLayoutChar(int code, bool shift, bool caps_lock, bool num_lock);

/// The keyboards in /proc/bus/input/devices' text: nodes with an event handler and the keys of a
/// keyboard (A to Z, Space, Enter, Backspace), not Valve's (the Frame's and Steam's own: a
/// controller in its keyboard mode must not type). Their event nodes ("event5").
struct KeyboardNode {
    std::string name;
    std::string event;
};
std::vector<KeyboardNode> ParseKeyboards(const std::string& text);

/// The reader (Linux evdev; elsewhere it finds none). XR thread only.
class PhysicalKeyboards {
public:
    PhysicalKeyboards() = default;
    PhysicalKeyboards(const PhysicalKeyboards&) = delete;
    PhysicalKeyboards& operator=(const PhysicalKeyboards&) = delete;
    ~PhysicalKeyboards();

    /// Reads the keyboards from now on (looks for new ones every 2 s); the keys since then.
    std::vector<KeyAction> Poll(int64_t now_ns);
    /// Stops reading and closes every node.
    void Close();
    /// At least one keyboard is being read.
    bool Active() const;

private:
    struct Node {
        std::string event;
        std::string name;
        int fd = -1;
    };
    void Scan(int64_t now_ns);

    std::vector<Node> nodes_;
    std::vector<std::string> failed_;
    KeyTranslator translator_;
    bool open_ = false;
    int64_t scanned_ns_ = 0;
    bool rescan_ = false;
};

} // namespace FrameXr::Input
