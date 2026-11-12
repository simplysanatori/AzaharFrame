// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: the game's keyboard's content and layout (swkbd_model.h).

#include "frame_xr/menu/swkbd_model.h"

#include <algorithm>
#include <utility>

namespace FrameXr::Swkbd {

namespace {

/// One key of a row: its weight (1 = a letter key), or a gap of that weight (no key).
struct Cell {
    float weight = 1.0f;
    KeyKind kind = KeyKind::Char;
    char32_t ch = 0;
    const char* label = nullptr; ///< for the keys that do not type themselves
    bool spacer = false;
};

Cell Ch(char32_t c, float w = 1.0f) {
    return {w, KeyKind::Char, c, nullptr, false};
}
Cell Fn(KeyKind kind, const char* label, float w) {
    return {w, kind, 0, label, false};
}
Cell Gap(float w) {
    return {w, KeyKind::Char, 0, nullptr, true};
}

bool IsLower(char32_t c) {
    return c >= U'a' && c <= U'z';
}

char32_t Upper(char32_t c) {
    return IsLower(c) ? c - U'a' + U'A' : c;
}

std::vector<Cell> Chars(const char* s) {
    std::vector<Cell> out;
    for (const char* p = s; *p; ++p) {
        out.push_back(Ch(static_cast<unsigned char>(*p)));
    }
    return out;
}

std::vector<Cell> Join(std::initializer_list<std::vector<Cell>> parts) {
    std::vector<Cell> out;
    for (const auto& p : parts) {
        out.insert(out.end(), p.begin(), p.end());
    }
    return out;
}

void AddRow(Layout& l, const std::vector<Cell>& row, float x0, float x1, float y0, float y1,
            bool upper) {
    float weights = 0.0f;
    for (const Cell& c : row) {
        weights += c.weight;
    }
    const float gaps = kKeyGap * static_cast<float>(row.size() - 1);
    const float unit = (x1 - x0 - gaps) / weights;
    float x = x0;
    for (const Cell& c : row) {
        const float w = unit * c.weight;
        if (!c.spacer) {
            Key k;
            k.kind = c.kind;
            k.r = {x, y0, x + w, y1};
            if (c.kind == KeyKind::Char) {
                k.ch = upper ? Upper(c.ch) : c.ch;
                k.label = ToUtf8(std::u32string(1, k.ch));
            } else {
                k.label = c.label;
            }
            l.keys.push_back(std::move(k));
        }
        x += w + kKeyGap;
    }
}

} // namespace

std::u32string FromUtf8(const std::string& s) {
    std::u32string out;
    for (std::size_t i = 0; i < s.size();) {
        const auto b = static_cast<unsigned char>(s[i]);
        char32_t c;
        int n;
        if (b < 0x80) {
            c = b;
            n = 1;
        } else if ((b >> 5) == 0x6) {
            c = b & 0x1F;
            n = 2;
        } else if ((b >> 4) == 0xE) {
            c = b & 0x0F;
            n = 3;
        } else if ((b >> 3) == 0x1E) {
            c = b & 0x07;
            n = 4;
        } else {
            i++; // a stray byte
            continue;
        }
        if (i + static_cast<std::size_t>(n) > s.size()) {
            break;
        }
        for (int k = 1; k < n; k++) {
            c = (c << 6) | (static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]) & 0x3F);
        }
        out.push_back(c);
        i += static_cast<std::size_t>(n);
    }
    return out;
}

std::string ToUtf8(const std::u32string& s) {
    std::string out;
    for (const char32_t c : s) {
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (c >> 12)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (c >> 18)));
            out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

int Utf16Length(const std::u32string& s) {
    int n = 0;
    for (const char32_t c : s) {
        n += c >= 0x10000 ? 2 : 1;
    }
    return n;
}

Layout MakeLayout(const Spec& spec, Page page, Shift shift) {
    Layout l;
    const float field_h = spec.multiline ? kFieldMultiH : kFieldH;
    l.field = {kPad, kPad, kPanelW - kPad, kPad + field_h};
    l.supporting = {kPad + 16.0f, l.field.y1, kPanelW - kPad - 16.0f, l.field.y1 + kSupportingH};

    const float top = l.supporting.y1 + 8.0f;
    const float bottom = kPanelH - kPad - kButtonH - 16.0f;
    const float row_h = (bottom - top - 3.0f * kKeyGap) / 4.0f;
    const auto row_y = [&](int i) {
        const float y = top + static_cast<float>(i) * (row_h + kKeyGap);
        return std::pair{y, y + row_h};
    };
    const float x0 = kPad, x1 = kPanelW - kPad;
    const bool upper = shift != Shift::Off;
    const bool multi = spec.multiline;

    std::vector<std::vector<Cell>> rows;
    float rx0 = x0, rx1 = x1;
    if (page == Page::Digits) {
        // A number pad in the middle.
        const float w = 3.0f * 160.0f + 2.0f * kKeyGap;
        rx0 = (kPanelW - w) * 0.5f;
        rx1 = rx0 + w;
        rows = {Chars("123"),
                Chars("456"),
                Chars("789"),
                {Gap(1.0f), Ch(U'0'), Fn(KeyKind::Backspace, "Backspace", 1.0f)}};
    } else if (page == Page::Letters) {
        rows = {Chars("qwertyuiop"), Join({{Gap(0.5f)}, Chars("asdfghjkl"), {Gap(0.5f)}}),
                Join({{Fn(KeyKind::Shift, "Shift", 1.5f)},
                      Chars("zxcvbnm"),
                      {Fn(KeyKind::Backspace, "Backspace", 1.5f)}}),
                multi ? std::vector<Cell>{Fn(KeyKind::PageSwitch, "?123", 1.5f), Ch(U','),
                                          Fn(KeyKind::Space, "Space", 5.0f), Ch(U'.'),
                                          Fn(KeyKind::Enter, "Enter", 1.5f)}
                      : std::vector<Cell>{Fn(KeyKind::PageSwitch, "?123", 1.5f), Ch(U','),
                                          Fn(KeyKind::Space, "Space", 6.5f), Ch(U'.')}};
    } else {
        rows = {Chars("1234567890"), Chars("@#$%&*-+()"),
                Join({Chars("=_\"':;!?"), {Fn(KeyKind::Backspace, "Backspace", 2.0f)}}),
                multi ? std::vector<Cell>{Fn(KeyKind::PageSwitch, "ABC", 1.5f), Ch(U','), Ch(U'/'),
                                          Fn(KeyKind::Space, "Space", 4.0f), Ch(U'.'),
                                          Fn(KeyKind::Enter, "Enter", 1.5f)}
                      : std::vector<Cell>{Fn(KeyKind::PageSwitch, "ABC", 1.5f), Ch(U','), Ch(U'/'),
                                          Fn(KeyKind::Space, "Space", 5.5f), Ch(U'.')}};
    }
    for (int i = 0; i < static_cast<int>(rows.size()); i++) {
        const auto [y0, y1] = row_y(i);
        AddRow(l, rows[static_cast<std::size_t>(i)], rx0, rx1, y0, y1,
               upper && page == Page::Letters);
    }

    // The game's buttons, at the bottom right, left to right as on the 3DS.
    const int n = static_cast<int>(spec.buttons.size());
    float x = kPanelW - kPad - static_cast<float>(n) * kButtonW -
              static_cast<float>(std::max(n - 1, 0)) * kButtonGap;
    const float by1 = kPanelH - kPad, by0 = by1 - kButtonH;
    for (int i = 0; i < n; i++) {
        Key k;
        k.kind = KeyKind::Button;
        k.button = i;
        k.r = {x, by0, x + kButtonW, by1};
        k.label = spec.buttons[static_cast<std::size_t>(i)].label;
        l.keys.push_back(std::move(k));
        x += kButtonW + kButtonGap;
    }
    return l;
}

Model::Model(Spec s) : spec(std::move(s)) {
    error = spec.error;
    if (spec.numpad) {
        page = Page::Digits;
        shift = Shift::Off;
    }
    Relayout();
}

std::string Model::TextUtf8() const {
    return ToUtf8(text);
}

int Model::Length() const {
    return Utf16Length(text);
}

std::string Model::Counter() const {
    return std::to_string(Length()) + " / " + std::to_string(spec.max_length);
}

void Model::Changed() {
    revision++;
}

void Model::Relayout() {
    layout = MakeLayout(spec, page, shift);
    Changed();
}

int Model::KeyAt(float x, float y) const {
    for (int i = 0; i < static_cast<int>(layout.keys.size()); i++) {
        if (layout.keys[static_cast<std::size_t>(i)].r.Contains(x, y)) {
            return i;
        }
    }
    return -1;
}

bool Model::Accept(const std::u32string& next) {
    if (Utf16Length(next) > spec.max_length) {
        return false; // the counter shows why
    }
    if (spec.filter) {
        std::string why = spec.filter(ToUtf8(next));
        if (!why.empty()) {
            error = std::move(why);
            Changed();
            return false;
        }
    }
    return true;
}

bool Model::Insert(char32_t c) {
    if (c == U'\r') {
        c = U'\n';
    }
    if ((c < 0x20 && c != U'\n') || c == 0x7F) {
        return false;
    }
    if (c == U'\n' && !spec.multiline) {
        return false;
    }
    if (spec.numpad && (c < U'0' || c > U'9')) {
        return false;
    }
    std::u32string next = text;
    next.insert(next.begin() + caret, c);
    if (!Accept(next)) {
        return false;
    }
    text = std::move(next);
    caret++;
    error.clear();
    if (shift == Shift::Once) {
        shift = Shift::Off; // one capital, or none once something else was typed
        Relayout();
    }
    Changed();
    return true;
}

bool Model::InsertText(const std::string& utf8) {
    bool any = false;
    for (const char32_t c : FromUtf8(utf8)) {
        any = Insert(c) || any;
    }
    return any;
}

bool Model::Backspace() {
    if (caret <= 0) {
        return false;
    }
    text.erase(text.begin() + caret - 1);
    caret--;
    error.clear();
    Changed();
    return true;
}

bool Model::DeleteForward() {
    if (caret >= static_cast<int>(text.size())) {
        return false;
    }
    text.erase(text.begin() + caret);
    error.clear();
    Changed();
    return true;
}

void Model::MoveCaret(int delta) {
    const int next = std::clamp(caret + delta, 0, static_cast<int>(text.size()));
    if (next != caret) {
        caret = next;
        Changed();
    }
}

void Model::CaretHome() {
    MoveCaret(-caret);
}

void Model::CaretEnd() {
    MoveCaret(static_cast<int>(text.size()) - caret);
}

void Model::ToggleShift() {
    if (page != Page::Letters) {
        return;
    }
    shift = shift == Shift::Off ? Shift::Once : shift == Shift::Once ? Shift::Locked : Shift::Off;
    Relayout();
}

void Model::SetPage(Page p) {
    if (spec.numpad || p == Page::Digits || p == page) {
        return;
    }
    page = p;
    Relayout();
}

std::optional<Result> Model::PressButton(int index) {
    if (index < 0 || index >= static_cast<int>(spec.buttons.size())) {
        return std::nullopt;
    }
    const Button& b = spec.buttons[static_cast<std::size_t>(index)];
    if (!b.confirm) {
        return Result{{}, b.id}; // as Azahar's dialog: no text with Cancel or I Forgot
    }
    if (spec.validate) {
        std::string why = spec.validate(TextUtf8());
        if (!why.empty()) {
            error = std::move(why);
            Changed();
            return std::nullopt;
        }
    }
    return Result{TextUtf8(), b.id};
}

std::optional<Result> Model::Enter() {
    if (spec.multiline) {
        Insert(U'\n');
        return std::nullopt;
    }
    for (int i = 0; i < static_cast<int>(spec.buttons.size()); i++) {
        if (spec.buttons[static_cast<std::size_t>(i)].confirm) {
            return PressButton(i);
        }
    }
    return std::nullopt;
}

uint8_t Model::CancelId() const {
    if (spec.buttons.size() >= 2) {
        return spec.buttons.front().id;
    }
    for (const Button& b : spec.buttons) {
        if (b.confirm) {
            return b.id;
        }
    }
    return 0;
}

std::optional<Result> Model::Press(int index) {
    if (index < 0 || index >= static_cast<int>(layout.keys.size())) {
        return std::nullopt;
    }
    const Key key = layout.keys[static_cast<std::size_t>(index)];
    switch (key.kind) {
    case KeyKind::Char:
        Insert(key.ch);
        break;
    case KeyKind::Shift:
        ToggleShift();
        break;
    case KeyKind::Backspace:
        Backspace();
        break;
    case KeyKind::Space:
        Insert(U' ');
        break;
    case KeyKind::Enter:
        Insert(U'\n');
        break;
    case KeyKind::PageSwitch:
        SetPage(page == Page::Letters ? Page::Symbols : Page::Letters);
        break;
    case KeyKind::Button:
        return PressButton(key.button);
    }
    return std::nullopt;
}

} // namespace FrameXr::Swkbd
