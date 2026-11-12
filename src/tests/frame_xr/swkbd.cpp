// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: the game's keyboard in VR (frame_xr/menu/swkbd_model.h,
// frame_xr/swkbd_config.h).

#include <catch2/catch_test_macros.hpp>

#include "core/frontend/applets/swkbd.h"
#include "frame_xr/menu/swkbd_model.h"
#include "frame_xr/swkbd_config.h"

using namespace FrameXr;
using namespace FrameXr::Swkbd;

namespace {

/// The applet's own checks, as a frontend has them.
class TestKeyboard final : public Frontend::SoftwareKeyboard {
public:
    void ShowError(const std::string&) override {}
};

Frontend::KeyboardConfig Config(
    Frontend::ButtonConfig buttons, int max_length = 8,
    Frontend::AcceptedInput accept = Frontend::AcceptedInput::Anything) {
    Frontend::KeyboardConfig c;
    c.type = Frontend::KeyboardType::Normal;
    c.button_config = buttons;
    c.accept_mode = accept;
    c.multiline_mode = false;
    c.max_text_length = static_cast<u16>(max_length);
    c.max_digits = 0;
    c.hint_text = "Name";
    c.button_text = {"", "", ""};
    c.filters = {};
    return c;
}

int Find(const Model& m, KeyKind kind, char32_t ch = 0) {
    const auto& keys = m.GetLayout().keys;
    for (int i = 0; i < static_cast<int>(keys.size()); i++) {
        const Key& k = keys[static_cast<std::size_t>(i)];
        if (k.kind == kind && (kind != KeyKind::Char || k.ch == ch)) {
            return i;
        }
    }
    return -1;
}

void Tap(Model& m, KeyKind kind, char32_t ch = 0) {
    const int i = Find(m, kind, ch);
    REQUIRE(i >= 0);
    m.Press(i);
}

Spec Simple(int max_length = 8) {
    Spec s;
    s.max_length = max_length;
    s.buttons = {{"Cancel", 0, false}, {"OK", 1, true}};
    return s;
}

} // namespace

TEST_CASE("Swkbd: keys type at the caret, Shift gives one capital", "[frame_xr]") {
    Model m(Simple());
    REQUIRE(m.GetShift() == Shift::Once); // the first letter is a capital
    REQUIRE(Find(m, KeyKind::Char, U'L') >= 0);
    Tap(m, KeyKind::Char, U'L');
    REQUIRE(m.GetShift() == Shift::Off);
    Tap(m, KeyKind::Char, U'i');
    Tap(m, KeyKind::Char, U'n');
    Tap(m, KeyKind::Char, U'k');
    REQUIRE(m.TextUtf8() == "Link");
    REQUIRE(m.Caret() == 4);
    m.MoveCaret(-2);
    Tap(m, KeyKind::Space);
    REQUIRE(m.TextUtf8() == "Li nk");
    Tap(m, KeyKind::Backspace);
    REQUIRE(m.TextUtf8() == "Link");
    REQUIRE(m.Caret() == 2);
    m.CaretEnd();
    m.Backspace();
    REQUIRE(m.TextUtf8() == "Lin");
}

TEST_CASE("Swkbd: Shift taps go Once, Locked, Off", "[frame_xr]") {
    Model m(Simple());
    Tap(m, KeyKind::Shift); // Once -> Locked
    REQUIRE(m.GetShift() == Shift::Locked);
    Tap(m, KeyKind::Shift); // Locked -> Off
    m.InsertText("x");
    Tap(m, KeyKind::Shift); // Off -> Once
    Tap(m, KeyKind::Shift); // Once -> Locked
    REQUIRE(m.GetShift() == Shift::Locked);
    Tap(m, KeyKind::Char, U'A');
    Tap(m, KeyKind::Char, U'B');
    REQUIRE(m.GetShift() == Shift::Locked);
    REQUIRE(m.TextUtf8() == "xAB");
    Tap(m, KeyKind::Shift);
    REQUIRE(m.GetShift() == Shift::Off);
    REQUIRE(Find(m, KeyKind::Char, U'a') >= 0);
    REQUIRE(Find(m, KeyKind::Char, U'A') < 0);
    Tap(m, KeyKind::Shift);
    REQUIRE(m.GetShift() == Shift::Once);
    m.InsertText("1"); // anything typed ends a Shift for one
    REQUIRE(m.GetShift() == Shift::Off);
}

TEST_CASE("Swkbd: ?123 and ABC switch pages", "[frame_xr]") {
    Model m(Simple());
    REQUIRE(m.GetPage() == Page::Letters);
    REQUIRE(Find(m, KeyKind::Char, U'1') < 0);
    Tap(m, KeyKind::PageSwitch);
    REQUIRE(m.GetPage() == Page::Symbols);
    REQUIRE(Find(m, KeyKind::Shift) < 0);
    Tap(m, KeyKind::Char, U'1');
    Tap(m, KeyKind::Char, U'#');
    REQUIRE(m.TextUtf8() == "1#");
    const int abc = Find(m, KeyKind::PageSwitch);
    REQUIRE(m.GetLayout().keys[static_cast<std::size_t>(abc)].label == "ABC");
    m.Press(abc);
    REQUIRE(m.GetPage() == Page::Letters);
    // Every printable ASCII character a name needs is on one of the two pages.
    for (const char32_t c : std::u32string(U"0123456789@#$%&*-+()=_\"':;!?,./")) {
        Model s(Simple(1));
        s.SetPage(Page::Symbols);
        INFO(static_cast<int>(c));
        REQUIRE(Find(s, KeyKind::Char, c) >= 0);
    }
}

TEST_CASE("Swkbd: the number pad types digits only", "[frame_xr]") {
    Spec s = Simple(4);
    s.numpad = true;
    Model m(s);
    REQUIRE(m.GetPage() == Page::Digits);
    REQUIRE(Find(m, KeyKind::PageSwitch) < 0);
    REQUIRE(Find(m, KeyKind::Shift) < 0);
    for (char32_t c = U'0'; c <= U'9'; c++) {
        REQUIRE(Find(m, KeyKind::Char, c) >= 0);
    }
    REQUIRE_FALSE(m.InsertText("a"));
    REQUIRE(m.InsertText("12x3"));
    REQUIRE(m.TextUtf8() == "123");
    m.SetPage(Page::Letters);
    REQUIRE(m.GetPage() == Page::Digits);
}

TEST_CASE("Swkbd: the maximum length, in UTF-16 units", "[frame_xr]") {
    Model m(Simple(3));
    REQUIRE(m.InsertText("abcd"));
    REQUIRE(m.TextUtf8() == "abc");
    REQUIRE(m.Counter() == "3 / 3");
    REQUIRE_FALSE(m.Insert(U'e'));
    REQUIRE(m.Error().empty()); // the count says why
    Model wide(Simple(2));
    REQUIRE(wide.InsertText("a\xF0\x9F\x98\x80")); // a fits, the emoji (2 units) does not
    REQUIRE(wide.TextUtf8() == "a");
    REQUIRE(wide.Length() == 1);
}

TEST_CASE("Swkbd: single and multi-line", "[frame_xr]") {
    Model one(Simple(20));
    REQUIRE(Find(one, KeyKind::Enter) < 0);
    one.InsertText("a\nb");
    REQUIRE(one.TextUtf8() == "ab");
    Spec s = Simple(20);
    s.multiline = true;
    Model multi(s);
    REQUIRE(Find(multi, KeyKind::Enter) >= 0);
    multi.InsertText("a");
    Tap(multi, KeyKind::Enter);
    multi.InsertText("b");
    REQUIRE(multi.TextUtf8() == "a\nb");
    REQUIRE_FALSE(multi.Enter()); // a physical Enter is a new line too
    REQUIRE(multi.TextUtf8() == "a\nb\n");
    REQUIRE(multi.GetLayout().field.H() > one.GetLayout().field.H());
}

TEST_CASE("Swkbd: every key and button lies on the panel, none overlap", "[frame_xr]") {
    for (const bool multi : {false, true}) {
        for (const Page page : {Page::Letters, Page::Symbols, Page::Digits}) {
            Spec s = Simple();
            s.multiline = multi;
            s.buttons = {{"Cancel", 0, false}, {"I Forgot", 1, false}, {"Ok", 2, true}};
            const Layout l = MakeLayout(s, page, Shift::Off);
            for (std::size_t i = 0; i < l.keys.size(); i++) {
                const Rect& a = l.keys[i].r;
                REQUIRE(a.x0 >= kPad - 0.01f);
                REQUIRE(a.x1 <= kPanelW - kPad + 0.01f);
                REQUIRE(a.y0 >= l.supporting.y1);
                REQUIRE(a.y1 <= kPanelH - kPad + 0.01f);
                REQUIRE(a.H() >= 48.0f); // Material's smallest target
                for (std::size_t j = i + 1; j < l.keys.size(); j++) {
                    const Rect& b = l.keys[j].r;
                    const bool apart = a.x1 <= b.x0 || b.x1 <= a.x0 || a.y1 <= b.y0 || b.y1 <= a.y0;
                    REQUIRE(apart);
                }
            }
        }
    }
    // KeyAt finds a key by its middle.
    Model m(Simple());
    const auto& keys = m.GetLayout().keys;
    for (int i = 0; i < static_cast<int>(keys.size()); i++) {
        REQUIRE(m.KeyAt(keys[static_cast<std::size_t>(i)].r.Cx(),
                        keys[static_cast<std::size_t>(i)].r.Cy()) == i);
    }
    REQUIRE(m.KeyAt(m.GetLayout().field.Cx(), m.GetLayout().field.Cy()) == -1);
}

TEST_CASE("Swkbd: the game's buttons, labels and ids as Azahar's dialog", "[frame_xr]") {
    TestKeyboard kb;
    using Frontend::ButtonConfig;
    {
        const auto c = Config(ButtonConfig::Single);
        kb.Execute(c);
        const Spec s = MakeSwkbdSpec(kb, c);
        REQUIRE(s.buttons.size() == 1);
        REQUIRE(s.buttons[0].label == "Ok");
        REQUIRE(s.buttons[0].id == 0);
        REQUIRE(s.buttons[0].confirm);
    }
    {
        auto c = Config(ButtonConfig::Dual);
        c.button_text = {"Back", "", "Done"};
        kb.Execute(c);
        const Spec s = MakeSwkbdSpec(kb, c);
        REQUIRE(s.buttons.size() == 2);
        REQUIRE(s.buttons[0].label == "Back");
        REQUIRE(s.buttons[0].id == 0);
        REQUIRE_FALSE(s.buttons[0].confirm);
        REQUIRE(s.buttons[1].label == "Done");
        REQUIRE(s.buttons[1].id == 1);
        REQUIRE(s.buttons[1].confirm);
        Model m(s);
        REQUIRE(m.CancelId() == 0);
    }
    {
        const auto c = Config(ButtonConfig::Triple);
        kb.Execute(c);
        const Spec s = MakeSwkbdSpec(kb, c);
        REQUIRE(s.buttons.size() == 3);
        REQUIRE(s.buttons[0].label == "Cancel");
        REQUIRE(s.buttons[1].label == "I Forgot");
        REQUIRE(s.buttons[2].label == "Ok");
        REQUIRE(s.buttons[2].id == 2);
        REQUIRE(s.buttons[2].confirm);
    }
    {
        const auto c = Config(ButtonConfig::None);
        kb.Execute(c);
        const Spec s = MakeSwkbdSpec(kb, c);
        REQUIRE(s.buttons.size() == 1); // one, so the keyboard can close
    }
}

TEST_CASE("Swkbd: pressing the game's buttons", "[frame_xr]") {
    TestKeyboard kb;
    auto c =
        Config(Frontend::ButtonConfig::Triple, 8, Frontend::AcceptedInput::NotEmptyAndNotBlank);
    kb.Execute(c);
    Model m(MakeSwkbdSpec(kb, c));
    m.InsertText("Kafei");
    const auto cancel = m.PressButton(0);
    REQUIRE(cancel);
    REQUIRE(cancel->button == 0);
    REQUIRE(cancel->text.empty()); // as Azahar's dialog: no text with Cancel
    const auto forgot = m.PressButton(1);
    REQUIRE(forgot);
    REQUIRE(forgot->button == 1);
    const auto ok = m.PressButton(2);
    REQUIRE(ok);
    REQUIRE(ok->button == 2);
    REQUIRE(ok->text == "Kafei");
    // The applet takes what the keyboard gives.
    REQUIRE(kb.Finalize(ok->text, ok->button) == Frontend::ValidationError::None);
    REQUIRE(kb.DataReady());
    // Through the layout's own button key.
    const auto& keys = m.GetLayout().keys;
    int last = -1;
    for (int i = 0; i < static_cast<int>(keys.size()); i++) {
        if (keys[static_cast<std::size_t>(i)].kind == KeyKind::Button) {
            last = i;
        }
    }
    const auto pressed = m.Press(last);
    REQUIRE(pressed);
    REQUIRE(pressed->button == 2);
    // A physical Enter is the confirm button.
    const auto enter = m.Enter();
    REQUIRE(enter);
    REQUIRE(enter->button == 2);
}

TEST_CASE("Swkbd: the applet's checks show under the field", "[frame_xr]") {
    TestKeyboard kb;
    using Frontend::AcceptedInput;
    using Frontend::ButtonConfig;
    {
        const auto c = Config(ButtonConfig::Dual, 8, AcceptedInput::NotEmptyAndNotBlank);
        kb.Execute(c);
        Model m(MakeSwkbdSpec(kb, c));
        REQUIRE_FALSE(m.PressButton(1)); // empty: blank, refused
        REQUIRE(m.Error() == "Blank input is not allowed");
        m.InsertText("  ");
        REQUIRE(m.Error().empty()); // typing clears it
        REQUIRE_FALSE(m.PressButton(1));
        REQUIRE(m.Error() == "Blank input is not allowed");
        REQUIRE(m.PressButton(0)); // Cancel is never checked
    }
    {
        const auto c = Config(ButtonConfig::Single, 4, AcceptedInput::FixedLength);
        kb.Execute(c);
        Model m(MakeSwkbdSpec(kb, c));
        m.InsertText("abc");
        REQUIRE_FALSE(m.PressButton(0));
        REQUIRE(m.Error() == "Text length is not correct (should be 4 characters)");
        m.InsertText("d");
        const auto r = m.PressButton(0);
        REQUIRE(r);
        REQUIRE(r->text == "abcd");
    }
    {
        const auto c = Config(ButtonConfig::Single, 8, AcceptedInput::NotEmpty);
        kb.Execute(c);
        Model m(MakeSwkbdSpec(kb, c));
        REQUIRE_FALSE(m.PressButton(0));
        REQUIRE(m.Error() == "Empty input is not allowed");
    }
    {
        // The filters refuse a key as it is typed.
        auto c = Config(ButtonConfig::Single, 16);
        c.filters.prevent_at = true;
        c.filters.prevent_percent = true;
        c.filters.prevent_backslash = true;
        c.filters.prevent_digit = true;
        c.max_digits = 2;
        kb.Execute(c);
        Model m(MakeSwkbdSpec(kb, c));
        REQUIRE(m.InsertText("a1"));
        REQUIRE_FALSE(m.Insert(U'@'));
        REQUIRE(m.Error() == "@ is not allowed");
        REQUIRE_FALSE(m.Insert(U'%'));
        REQUIRE(m.Error() == "% is not allowed");
        REQUIRE_FALSE(m.Insert(U'\\'));
        REQUIRE(m.Error() == "\\ is not allowed");
        REQUIRE(m.Insert(U'2'));
        REQUIRE(m.Error().empty());
        REQUIRE_FALSE(m.Insert(U'3'));
        REQUIRE(m.Error() == "Too many digits (should be no more than 2)");
        REQUIRE(m.TextUtf8() == "a12");
    }
    {
        // The game's own message from its check shows at the start.
        Spec s = Simple();
        s.error = "That name cannot be used.";
        Model m(s);
        REQUIRE(m.Error() == "That name cannot be used.");
        m.InsertText("a");
        REQUIRE(m.Error().empty());
    }
}

TEST_CASE("Swkbd: UTF-8 round trip", "[frame_xr]") {
    const std::string s = "Ab\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    const std::u32string u = FromUtf8(s);
    REQUIRE(u.size() == 5);
    REQUIRE(ToUtf8(u) == s);
    REQUIRE(Utf16Length(u) == 6);
}
