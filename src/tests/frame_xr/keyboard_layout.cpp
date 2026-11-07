// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/keyboard/layout.h"

// AzaharFrame
using namespace FrameXr;
using namespace FrameXr::Keyboard;

namespace {

constexpr int64_t kMs = 1'000'000;

// The keyboard as keyboard_panel.cpp drives it: a press, then once a frame the hold clock.
struct Rig {
    int sheet = kSheetLower;
    int mods = 0;
    Sticky sticky;
    Press last;
    int64_t now = 0;

    void Down(int code) {
        sticky.OnPress(code, now);
        last = ApplyPress(sheet, mods, code);
        sheet = last.sheet;
        mods = last.mods;
        Frame(true);
    }
    void Frame(bool down) {
        sticky.Tick(down, now, kDefaultHoldNs);
        mods = sticky.Mods(mods);
        sheet = sticky.SheetFor(sheet);
    }
    // Held for `ms`, a frame every 11 ms, then let go.
    void Hold(int code, int64_t ms) {
        Down(code);
        for (int64_t t = 0; t < ms * kMs; t += 11 * kMs) {
            now += 11 * kMs;
            Frame(true);
        }
        now += 11 * kMs;
        Frame(false);
    }
    void Tap(int code) {
        Hold(code, 60);
    }
};

int Find(int sheet, int code) {
    for (int i = 0; i < kKeyCount; i++) {
        if (KeyCode(sheet, i) == code) {
            return i;
        }
    }
    return -1;
}

} // namespace

TEST_CASE("FrameXr keyboard: key rectangles", "[frame_xr]") {
    for (int i = 0; i < kKeyCount; i++) {
        const Rect a = KeyRect(i);
        REQUIRE(a.x0 >= 0.0f);
        REQUIRE(a.y0 >= 0.0f);
        REQUIRE(a.x1 <= 1.0f);
        REQUIRE(a.y1 <= 1.0f);
        REQUIRE(a.x1 > a.x0);
        REQUIRE(a.y1 > a.y0);
        for (int j = i + 1; j < kKeyCount; j++) {
            const Rect b = KeyRect(j);
            REQUIRE((a.x1 <= b.x0 || b.x1 <= a.x0 || a.y1 <= b.y0 || b.y1 <= a.y0));
        }
    }
    // Five rows: ten digits, ten letters, nine, nine (Shift and Del wider), six (the space bar).
    REQUIRE(KeyRect(0).y0 == KeyRect(9).y0);
    REQUIRE(KeyRect(10).y0 > KeyRect(0).y0);
    const Rect space = KeyRect(Find(kSheetLower, 32));
    const Rect q = KeyRect(Find(kSheetLower, 'q'));
    REQUIRE((space.x1 - space.x0) > 3.5f * (q.x1 - q.x0));
    // The home row is half a key in.
    REQUIRE(KeyRect(Find(kSheetLower, 'a')).x0 > q.x0);
}

TEST_CASE("FrameXr keyboard: hit testing", "[frame_xr]") {
    for (int sheet = 0; sheet < kSheetCount; sheet++) {
        for (int i = 0; i < kKeyCount; i++) {
            const Rect r = KeyRect(i);
            const int at = KeyAt(sheet, (r.x0 + r.x1) * 0.5f, (r.y0 + r.y1) * 0.5f);
            if (KeyCode(sheet, i) == 0) {
                REQUIRE(at == -1); // a blank is no key
                REQUIRE(KeyLabel(sheet, i) == nullptr);
            } else {
                REQUIRE(at == i);
                REQUIRE(KeyLabel(sheet, i) != nullptr);
            }
        }
    }
    // The gap between two keys, the margin and outside are nothing.
    const Rect a = KeyRect(0), b = KeyRect(1);
    REQUIRE(KeyAt(kSheetLower, (a.x1 + b.x0) * 0.5f, (a.y0 + a.y1) * 0.5f) == -1);
    REQUIRE(KeyAt(kSheetLower, 0.001f, 0.5f) == -1);
    REQUIRE(KeyAt(kSheetLower, 0.5f, 1.2f) == -1);
    // The same place is another key on another sheet.
    const Rect r = KeyRect(10);
    REQUIRE(KeyCode(kSheetLower, KeyAt(kSheetLower, (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2)) == 'q');
    REQUIRE(KeyCode(kSheetUpper, KeyAt(kSheetUpper, (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2)) == 'Q');
    REQUIRE(KeyCode(kSheetSymbols, KeyAt(kSheetSymbols, (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2)) ==
            '@');
    REQUIRE(KeyCode(kSheetFn, KeyAt(kSheetFn, (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2)) == 9);
}

TEST_CASE("FrameXr keyboard: sheets", "[frame_xr]") {
    Rig k;
    k.Tap(kCodeShift);
    REQUIRE(k.sheet == kSheetUpper);
    k.Tap('Q');
    REQUIRE(k.last.code == 'Q');
    REQUIRE(k.sheet == kSheetLower); // Shift is one shot over the letters
    k.Tap(kCodeShift);
    k.Tap('!');
    REQUIRE(k.last.code == '!');
    REQUIRE(k.sheet == kSheetUpper); // and sticky over the punctuation row
    k.Tap(kCodeShift);
    REQUIRE(k.sheet == kSheetLower);
    k.Tap(kCodeSymbols);
    REQUIRE(k.sheet == kSheetSymbols);
    k.Tap(kCodeShift); // Shift off the symbols goes to the capitals
    REQUIRE(k.sheet == kSheetUpper);
    k.Tap(kCodeSymbols);
    k.Tap(kCodeFn);
    REQUIRE(k.sheet == kSheetFn);
    k.Tap(kCodeFn); // ?123 in the same place
    REQUIRE(k.sheet == kSheetSymbols);
    k.Tap(kCodeFn);
    k.Tap(kCodeSymbols); // ABC
    REQUIRE(k.sheet == kSheetLower);
    REQUIRE(KeyCode(kSheetFn, Find(kSheetFn, kCodeSymbols)) == kCodeSymbols);
}

TEST_CASE("FrameXr keyboard: modifiers for the next key", "[frame_xr]") {
    Rig k;
    k.sheet = kSheetFn;
    k.Tap(kCodeCtrl);
    REQUIRE(k.mods == kModCtrl);
    k.Tap(kCodeSymbols); // to the letters: Ctrl still lit
    REQUIRE(k.sheet == kSheetLower);
    REQUIRE(k.mods == kModCtrl);
    k.Tap('a');
    REQUIRE(k.last.code == 'a');
    REQUIRE(k.last.code_mods == kModCtrl);
    REQUIRE(k.mods == 0);
    // A second tap puts it out; Hide lets go of everything.
    k.sheet = kSheetFn;
    k.Tap(kCodeAlt);
    k.Tap(kCodeAlt);
    REQUIRE(k.mods == 0);
    k.Tap(kCodeWin);
    k.Tap(kCodeHide);
    REQUIRE(k.last.hide);
    REQUIRE(k.mods == 0);
}

TEST_CASE("FrameXr keyboard: hold to lock", "[frame_xr]") {
    SECTION("Ctrl held locks, stays lit over keys, a tap lets it go") {
        Rig k;
        k.sheet = kSheetFn;
        k.Hold(kCodeCtrl, 600);
        REQUIRE(k.sticky.locked == kModCtrl);
        REQUIRE(k.mods == kModCtrl);
        k.Tap(kCodeSymbols);
        k.Tap('c');
        REQUIRE(k.last.code_mods == kModCtrl);
        k.Tap('v');
        REQUIRE(k.last.code_mods == kModCtrl);
        REQUIRE(k.mods == kModCtrl);
        k.Tap(kCodeSymbols);
        k.Tap(kCodeFn);
        k.Tap(kCodeCtrl);
        REQUIRE(k.sticky.locked == 0);
        REQUIRE(k.mods == 0);
    }
    SECTION("just short of the hold is a tap") {
        Rig k;
        k.sheet = kSheetFn;
        k.Hold(kCodeAlt, 470);
        REQUIRE(k.sticky.locked == 0);
        REQUIRE(k.mods == kModAlt);
    }
    SECTION("a locked Shift keeps the capitals") {
        Rig k;
        k.Hold(kCodeShift, 600);
        REQUIRE(k.sticky.locked == kModShift);
        REQUIRE(k.sheet == kSheetUpper);
        k.Tap('A');
        REQUIRE(k.sheet == kSheetUpper);
        k.Tap('B');
        REQUIRE(k.last.code == 'B');
        REQUIRE(k.mods == 0); // Shift is a sheet, never a lit modifier
        k.Tap(kCodeShift);
        REQUIRE(k.sticky.locked == 0);
        REQUIRE(k.sheet == kSheetLower);
    }
}

TEST_CASE("FrameXr keyboard: key events", "[frame_xr]") {
    using Key = KeyboardEvent::Key;
    KeyboardEvent e;
    REQUIRE(MakeEvent('a', 0, e));
    REQUIRE(e.key == Key::Character);
    REQUIRE(static_cast<int>(e.character) == 'a');
    REQUIRE(e.text == "a");
    REQUIRE(e.modifiers == 0);
    REQUIRE(MakeEvent('A', 0, e));
    REQUIRE(e.modifiers == KeyboardEvent::kShift);
    REQUIRE(MakeEvent('?', kModCtrl, e));
    REQUIRE(e.modifiers == (KeyboardEvent::kShift | KeyboardEvent::kCtrl));
    REQUIRE(MakeEvent('/', 0, e));
    REQUIRE(e.modifiers == 0);
    REQUIRE(MakeEvent(' ', 0, e));
    REQUIRE(e.text == " ");
    REQUIRE(MakeEvent(8, 0, e));
    REQUIRE(e.key == Key::Backspace);
    REQUIRE(MakeEvent(13, 0, e));
    REQUIRE(e.key == Key::Enter);
    REQUIRE(MakeEvent(9, 0, e));
    REQUIRE(e.key == Key::Tab);
    // Every key on the Fn sheet that types makes an event.
    for (int i = 0; i < kKeyCount; i++) {
        const int code = KeyCode(kSheetFn, i);
        if (code > 0) {
            REQUIRE(MakeEvent(code, 0, e));
        }
    }
    REQUIRE(MakeEvent(kCodeVk + 0x70, 0, e));
    REQUIRE(e.key == Key::F1);
    REQUIRE(MakeEvent(kCodeVk + 0x7B, 0, e));
    REQUIRE(e.key == Key::F12);
    REQUIRE(MakeEvent(kCodeVk + 0x2E, 0, e));
    REQUIRE(e.key == Key::Delete);
    REQUIRE(MakeEvent(kCodeVk + 0x26, kModAlt, e));
    REQUIRE(e.key == Key::Up);
    REQUIRE(e.modifiers == KeyboardEvent::kAlt);
    REQUIRE(MakeEvent(kCodeVk + 0x1B, 0, e));
    REQUIRE(e.key == Key::Escape);
    REQUIRE_FALSE(MakeEvent(kCodeShift, 0, e));
    REQUIRE_FALSE(MakeEvent(0, 0, e));
}
