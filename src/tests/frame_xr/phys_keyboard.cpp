// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame input2: a physical keyboard typing into the game's keyboard in VR
// (frame_xr/input/phys_keyboard.h): the US layout, Shift and the locks, the editing keys, and
// which input devices are keyboards.

#include <optional>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/phys_keyboard.h"
#include "frame_xr/menu/swkbd_model.h" // AzaharFrame integ3

using namespace FrameXr::Input;

namespace {

// Linux key codes.
enum : int {
    kEsc = 1,
    k1 = 2,
    k2 = 3,
    kMinus = 12,
    kBackspace = 14,
    kQ = 16,
    kEnter = 28,
    kLeftCtrl = 29,
    kA = 30,
    kSemicolon = 39,
    kApostrophe = 40,
    kLeftShift = 42,
    kBackslash = 43,
    kZ = 44,
    kSlash = 53,
    kRightShift = 54,
    kSpace = 57,
    kCapsLock = 58,
    kNumLock = 69,
    kKp7 = 71,
    kKp4 = 75,
    kKp1 = 79,
    kKpEnter = 96,
    kLeft = 105,
    kRight = 106,
    kDelete = 111,
};

/// Types `code` (down, up) and returns what it gave.
KeyAction Tap(KeyTranslator& t, int code) {
    const KeyAction a = t.Key(code, 1);
    t.Key(code, 0);
    return a;
}

std::string Text(KeyTranslator& t, std::initializer_list<int> codes) {
    std::string s;
    for (const int c : codes) {
        const KeyAction a = Tap(t, c);
        if (a.kind == KeyActionKind::Text) {
            s += a.text;
        }
    }
    return s;
}

} // namespace

TEST_CASE("Physical keyboard: letters, digits and symbols by the US layout", "[frame_xr]") {
    KeyTranslator t;
    CHECK(Text(t, {kA, kZ, kQ, k1, k2, kMinus, kSpace, kSemicolon, kApostrophe, kBackslash,
                   kSlash}) == "azq12- ;'\\/");
    t.Key(kLeftShift, 1);
    CHECK(Text(t, {kA, kZ, k1, k2, kMinus, kSemicolon, kApostrophe, kSlash}) == "AZ!@_:\"?");
    t.Key(kLeftShift, 0);
    t.Key(kRightShift, 1);
    CHECK(Text(t, {kQ}) == "Q");
    t.Key(kRightShift, 0);
    CHECK(Text(t, {kQ}) == "q");
}

TEST_CASE("Physical keyboard: Caps Lock turns letters only, Shift turns them back", "[frame_xr]") {
    KeyTranslator t;
    Tap(t, kCapsLock);
    CHECK(t.caps_lock());
    CHECK(Text(t, {kA, k1}) == "A1");
    t.Key(kLeftShift, 1);
    CHECK(Text(t, {kA, k1}) == "a!");
    t.Key(kLeftShift, 0);
    Tap(t, kCapsLock);
    CHECK(Text(t, {kA}) == "a");
    // As the keyboard's LED showed it when reading started.
    KeyTranslator led;
    led.SetLocks(true, true);
    CHECK(Text(led, {kZ}) == "Z");
}

TEST_CASE("Physical keyboard: the editing keys, Enter, Shift+Enter and Esc", "[frame_xr]") {
    KeyTranslator t;
    CHECK(Tap(t, kBackspace).kind == KeyActionKind::Backspace);
    CHECK(Tap(t, kDelete).kind == KeyActionKind::Delete);
    CHECK(Tap(t, kLeft).kind == KeyActionKind::Left);
    CHECK(Tap(t, kRight).kind == KeyActionKind::Right);
    CHECK(Tap(t, kEnter).kind == KeyActionKind::Enter);
    CHECK(Tap(t, kKpEnter).kind == KeyActionKind::Enter);
    CHECK(Tap(t, kEsc).kind == KeyActionKind::Escape);
    t.Key(kLeftShift, 1);
    CHECK(Tap(t, kEnter).kind == KeyActionKind::NewLine);
    t.Key(kLeftShift, 0);
    // Held: Backspace and the letters repeat, Enter and Esc answer once.
    CHECK(t.Key(kBackspace, 1).kind == KeyActionKind::Backspace);
    CHECK(t.Key(kBackspace, 2).kind == KeyActionKind::Backspace);
    t.Key(kBackspace, 0);
    CHECK(t.Key(kEnter, 1).kind == KeyActionKind::Enter);
    CHECK(t.Key(kEnter, 2).kind == KeyActionKind::None);
    t.Key(kEnter, 0);
    CHECK(t.Key(kA, 1).text == "a");
    CHECK(t.Key(kA, 2).text == "a");
    t.Key(kA, 0);
    // Releases give nothing.
    CHECK(t.Key(kA, 0).kind == KeyActionKind::None);
}

TEST_CASE("Physical keyboard: shortcuts type nothing, held keys from before never repeat",
          "[frame_xr]") {
    KeyTranslator t;
    t.Key(kLeftCtrl, 1);
    CHECK(Tap(t, kA).kind == KeyActionKind::None);
    CHECK(Tap(t, kBackspace).kind == KeyActionKind::Backspace); // editing still works
    t.Key(kLeftCtrl, 0);
    CHECK(Tap(t, kA).text == "a");
    // A key already held when reading started: its repeats are not typing.
    KeyTranslator late;
    CHECK(late.Key(kBackspace, 2).kind == KeyActionKind::None);
    CHECK(late.Key(kA, 2).kind == KeyActionKind::None);
    late.Key(kA, 0);
    CHECK(late.Key(kA, 1).text == "a");
    // Reset: everything let go.
    t.Key(kLeftShift, 1);
    t.Reset();
    CHECK(Tap(t, kA).text == "a");
}

TEST_CASE("Physical keyboard: the keypad with and without Num Lock", "[frame_xr]") {
    KeyTranslator t;
    t.SetLocks(false, true);
    CHECK(Text(t, {kKp7, kKp4, kKp1}) == "741");
    Tap(t, kNumLock);
    CHECK(Tap(t, kKp4).kind == KeyActionKind::Left);
    CHECK(Tap(t, kKp7).kind == KeyActionKind::Home);
    CHECK(Tap(t, kKp1).kind == KeyActionKind::End);
}

TEST_CASE("Physical keyboard: which input devices are keyboards", "[frame_xr]") {
    // A Bluetooth keyboard (letters, LEDs), a mouse's keyboard node (letters, no LEDs), a power
    // key (no letters), a mouse, and Valve's own (the Frame's or Steam's, in keyboard mode).
    const std::string text =
        "I: Bus=0005 Vendor=1234 Product=0610 Version=0132\n"
        "N: Name=\"BT Keyboard\"\n"
        "H: Handlers=sysrq kbd leds event5\n"
        "B: EV=12001f\n"
        "B: KEY=3f00733fff 0 0 483ffff17aff32d bfd4444600000000 1 130ff38b17d007 "
        "ffff7bfad9415fff ffbeffdfffefffff fffffffffffffffe\n"
        "\n"
        "I: Bus=0005 Vendor=046d Product=b023 Version=0015\n"
        "N: Name=\"Wireless Mouse Keyboard\"\n"
        "H: Handlers=sysrq kbd event7\n"
        "B: EV=100013\n"
        "B: KEY=1000000000007 ff800000000007ff febeffdfffefffff fffffffffffffffe\n"
        "\n"
        "I: Bus=0000 Vendor=0000 Product=0000 Version=0000\n"
        "N: Name=\"pmic_pwrkey\"\n"
        "H: Handlers=kbd event0\n"
        "B: EV=3\n"
        "B: KEY=10000000000000 0\n"
        "\n"
        "I: Bus=0003 Vendor=046d Product=c077 Version=0111\n"
        "N: Name=\"USB Optical Mouse\"\n"
        "H: Handlers=mouse0 event8\n"
        "B: EV=17\n"
        "B: KEY=70000 0 0 0 0\n"
        "B: REL=903\n"
        "\n"
        "I: Bus=0003 Vendor=28de Product=1205 Version=0111\n"
        "N: Name=\"Valve Software Steam Controller\"\n"
        "H: Handlers=sysrq kbd event9\n"
        "B: EV=12001f\n"
        "B: KEY=ffff7bfad9415fff ffbeffdfffefffff fffffffffffffffe\n";
    const std::vector<KeyboardNode> k = ParseKeyboards(text);
    REQUIRE(k.size() == 2);
    CHECK(k[0].name == "BT Keyboard");
    CHECK(k[0].event == "event5");
    CHECK(k[1].event == "event7");
    CHECK(ParseKeyboards("").empty());
}

// AzaharFrame integ3: the menu's own text boxes (ui2's Cheat Name and Code, Spec::over_menu) are
// the game's keyboard too, so a physical keyboard types into them; Shift+Enter is a new line in the
// multi-line Code box and Enter its confirm button (Add).
TEST_CASE("Physical keyboard: the in-game menu's multi-line Code box", "[frame_xr]") {
    using namespace FrameXr::Swkbd;
    Spec spec;
    spec.hint = "Code (XXXXXXXX YYYYYYYY)";
    spec.max_length = 4000;
    spec.multiline = true;
    spec.buttons = {{"Cancel", 0, false}, {"Add", 1, true}};
    spec.over_menu = true;
    Model m(spec);
    KeyTranslator t;
    // As the keyboard panel applies them (swkbd_panel.cpp, FromKeyAction).
    const auto apply = [&m](const KeyAction& a) -> std::optional<Result> {
        switch (a.kind) {
        case KeyActionKind::Text:
            m.InsertText(a.text);
            return std::nullopt;
        case KeyActionKind::NewLine:
            m.NewLine();
            return std::nullopt;
        case KeyActionKind::Backspace:
            m.Backspace();
            return std::nullopt;
        case KeyActionKind::Enter:
            return m.Enter();
        case KeyActionKind::Escape:
            return m.Escape();
        default:
            return std::nullopt;
        }
    };
    for (const int c : {k1, k2, kSpace, k2, k1}) {
        CHECK_FALSE(apply(Tap(t, c)));
    }
    t.Key(kLeftShift, 1);
    const KeyAction shift_enter = t.Key(kEnter, 1);
    CHECK(shift_enter.kind == KeyActionKind::NewLine);
    CHECK_FALSE(apply(shift_enter));
    t.Key(kEnter, 0);
    t.Key(kLeftShift, 0);
    CHECK_FALSE(apply(Tap(t, kA)));
    const std::optional<Result> r = apply(Tap(t, kEnter));
    REQUIRE(r);
    CHECK(r->button == 1);
    CHECK(r->text == "12 21\na");
}
