// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// The headset keyboard's layout and rules, ported from Moonlight XR's Compact keyboard (same
// author): four sheets (lower case, capitals, symbols, Fn) sharing one set of 44 key rectangles,
// which press leaves which sheet showing, the Ctrl, Alt and Win lit for the next key, and the
// hold-to-lock of Shift, Ctrl, Alt and Win. US QWERTY. Plain arithmetic, no XR, Vulkan or Qt, so
// the tests reach all of it.

#pragma once

#include <cstdint>

#include "frame_xr/keyboard/keyboard.h"

namespace FrameXr::Keyboard {

/// The keyboard's art, in pixels: the key rectangles below are fractions of it.
constexpr int kTexWidth = 1120;
constexpr int kTexHeight = 448;
/// Keys on every sheet (a blank on a sheet is no key there).
constexpr int kKeyCount = 44;

enum Sheet : int { kSheetLower = 0, kSheetUpper, kSheetSymbols, kSheetFn, kSheetCount };

// What a key does. Codes above zero type: 8 Backspace, 9 Tab, 13 Enter, 32 to 126 the ASCII
// character, kCodeVk + a Windows virtual key the named keys (Esc, F1 to F12, the arrows and the
// rest). Codes below zero change the keyboard instead; 0 is a blank.
constexpr int kCodeShift = -2;
constexpr int kCodeSymbols = -3;
constexpr int kCodeHide = -4;
constexpr int kCodeFn = -5;
constexpr int kCodeCtrl = -6;
constexpr int kCodeAlt = -7;
constexpr int kCodeWin = -8;
constexpr int kCodeVk = 65536;

// Modifier bits (KeyboardEvent's): Ctrl, Alt and Win light for the next key; Shift is a sheet,
// and its bit only marks a locked Shift (Sticky) and a capital going out.
constexpr int kModShift = KeyboardEvent::kShift;
constexpr int kModCtrl = KeyboardEvent::kCtrl;
constexpr int kModAlt = KeyboardEvent::kAlt;
constexpr int kModWin = KeyboardEvent::kMeta;

/// 0..1 across the panel from its top left corner (y down).
struct Rect {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};

/// Where key `index` (0 .. kKeyCount - 1, row by row) is, on every sheet.
Rect KeyRect(int index);
/// What it does on `sheet`, 0 for a blank (and out of range).
int KeyCode(int sheet, int index);
/// What it says: the characters, or a token ("@shift", "@space", "@enter", "@hide", "@ctrl",
/// "@alt", "@win", and the arrows "@up", "@down", "@left", "@right", which are drawn as shapes).
/// nullptr for a blank.
const char* KeyLabel(int sheet, int index);
/// The key under a point of the panel on `sheet`, or -1 (a blank, a gap, outside).
int KeyAt(int sheet, float u, float v);

/// The modifier a key lights, a kMod bit (Ctrl, Alt, Win), or 0.
int ModifierBit(int code);

/// What one press does.
struct Press {
    int sheet = kSheetLower; ///< the sheet showing after it
    int mods = 0;            ///< the modifiers lit after it
    int code = -1;           ///< what to type, or -1 for nothing
    int code_mods = 0;       ///< the modifiers held while it goes
    bool hide = false;       ///< it put the keyboard away
};

/// A press on a key with this code, on this sheet, with these modifiers lit. A modifier key
/// lights or puts out its own bit. Any other key that types goes with whatever is lit and puts
/// them all out. The sheet keys leave the modifiers alone, so a Ctrl lit on the Fn sheet still
/// holds over the letters, and hiding the keyboard lets go of them. Shift is one shot over the
/// capitals and sticky over the punctuation row above them.
Press ApplyPress(int sheet, int mods, int code);

/// Hold to lock: a tap lights a modifier for the next key (ApplyPress; a second tap puts it out),
/// and holding it for the hold time locks it until it is tapped again. Ctrl, Alt and Win lock
/// their kMod bit; Shift, which is a sheet here, locks the capitals (kModShift).
struct Sticky {
    int locked = 0;       ///< kMod bits locked
    int hold_bit = 0;     ///< the bit of the key under the press, or 0
    int64_t hold_start_ns = 0;

    void Reset();
    /// A press on a key with this code. A tap on a locked modifier lets it go and returns true
    /// (ApplyPress then puts its light out); a press on one that is not locked starts its hold
    /// clock. Other keys do nothing.
    bool OnPress(int code, int64_t now_ns);
    /// Once a frame while the keyboard is up; `down`: the press is still held. Returns the bit
    /// that just locked, or 0.
    int Tick(bool down, int64_t now_ns, int64_t hold_ns);
    /// The lit modifiers once a press has been applied: those locked stay lit.
    int Mods(int mods) const;
    /// The sheet once a press has been applied: a locked Shift keeps the capitals.
    int SheetFor(int sheet) const;
};

/// The bit a key code locks, or 0.
int StickyBit(int code);

/// Moonlight XR's default: half a second.
constexpr int64_t kDefaultHoldNs = 500'000'000;

/// The key event a typing code makes with these modifiers lit (ApplyPress's code and
/// code_mods). False for a code that types nothing.
bool MakeEvent(int code, int mods, KeyboardEvent& out);

} // namespace FrameXr::Keyboard
