// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// See layout.h. The tables, the geometry and the rules are Moonlight XR's (ui/panel_keyboard.c,
// xr/core/xr_keys.c), QWERTY only.

#include "frame_xr/keyboard/layout.h"

#include <cstring>

namespace FrameXr::Keyboard {

namespace {

// Five rows: the widths of their keys in key units, and how far each row is indented.
constexpr float kRowWidths[5][10] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {1.5f, 1, 1, 1, 1, 1, 1, 1, 1.5f, 0},
    {1.5f, 1, 4, 1, 1.5f, 1, 0, 0, 0, 0},
};
constexpr int kRowKeys[5] = {10, 10, 9, 9, 6};
constexpr float kRowIndent[5] = {0.0f, 0.0f, 0.5f, 0.0f, 0.0f};
constexpr float kRowUnits = 10.0f;
constexpr float kPadU = 0.012f;
constexpr float kPadV = 0.030f;
constexpr float kGapU = 0.005f;
constexpr float kGapV = 0.014f;

constexpr int Vk(int vk) {
    return kCodeVk + vk;
}

struct Key {
    const char* label;
    int code;
};

constexpr Key kLower[kKeyCount] = {
    {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'}, {"5", '5'},
    {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'}, {"0", '0'},
    {"q", 'q'}, {"w", 'w'}, {"e", 'e'}, {"r", 'r'}, {"t", 't'},
    {"y", 'y'}, {"u", 'u'}, {"i", 'i'}, {"o", 'o'}, {"p", 'p'},
    {"a", 'a'}, {"s", 's'}, {"d", 'd'}, {"f", 'f'}, {"g", 'g'},
    {"h", 'h'}, {"j", 'j'}, {"k", 'k'}, {"l", 'l'},
    {"@shift", kCodeShift}, {"z", 'z'}, {"x", 'x'}, {"c", 'c'}, {"v", 'v'},
    {"b", 'b'}, {"n", 'n'}, {"m", 'm'}, {"Del", 8},
    {"?123", kCodeSymbols}, {",", ','}, {"@space", 32}, {".", '.'}, {"@enter", 13},
    {"@hide", kCodeHide},
};
constexpr Key kUpper[kKeyCount] = {
    {"!", '!'}, {"@", '@'}, {"#", '#'}, {"$", '$'}, {"%", '%'},
    {"^", '^'}, {"&", '&'}, {"*", '*'}, {"(", '('}, {")", ')'},
    {"Q", 'Q'}, {"W", 'W'}, {"E", 'E'}, {"R", 'R'}, {"T", 'T'},
    {"Y", 'Y'}, {"U", 'U'}, {"I", 'I'}, {"O", 'O'}, {"P", 'P'},
    {"A", 'A'}, {"S", 'S'}, {"D", 'D'}, {"F", 'F'}, {"G", 'G'},
    {"H", 'H'}, {"J", 'J'}, {"K", 'K'}, {"L", 'L'},
    {"@shift", kCodeShift}, {"Z", 'Z'}, {"X", 'X'}, {"C", 'C'}, {"V", 'V'},
    {"B", 'B'}, {"N", 'N'}, {"M", 'M'}, {"Del", 8},
    {"?123", kCodeSymbols}, {",", ','}, {"@space", 32}, {".", '.'}, {"@enter", 13},
    {"@hide", kCodeHide},
};
constexpr Key kSymbols[kKeyCount] = {
    {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'}, {"5", '5'},
    {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'}, {"0", '0'},
    {"@", '@'}, {"#", '#'}, {"$", '$'}, {"%", '%'}, {"&", '&'},
    {"*", '*'}, {"-", '-'}, {"+", '+'}, {"(", '('}, {")", ')'},
    {"!", '!'}, {"\"", '"'}, {"'", '\''}, {":", ':'}, {";", ';'},
    {"/", '/'}, {"?", '?'}, {"_", '_'}, {"=", '='},
    {"Fn", kCodeFn}, {"<", '<'}, {">", '>'}, {"[", '['}, {"]", ']'},
    {"{", '{'}, {"}", '}'}, {"\\", '\\'}, {"Del", 8},
    {"ABC", kCodeSymbols}, {",", ','}, {"@space", 32}, {".", '.'}, {"@enter", 13},
    {"@hide", kCodeHide},
};
// The keys that type nothing. Arrows are drawn as shapes: "@up" "@down" "@left" "@right".
constexpr Key kFn[kKeyCount] = {
    {"Esc", Vk(0x1B)}, {"F1", Vk(0x70)}, {"F2", Vk(0x71)}, {"F3", Vk(0x72)}, {"F4", Vk(0x73)},
    {"F5", Vk(0x74)}, {"F6", Vk(0x75)}, {"Ins", Vk(0x2D)}, {"Home", Vk(0x24)}, {"PgUp", Vk(0x21)},
    {"Tab", 9}, {"F7", Vk(0x76)}, {"F8", Vk(0x77)}, {"F9", Vk(0x78)}, {"F10", Vk(0x79)},
    {"F11", Vk(0x7A)}, {"F12", Vk(0x7B)}, {"Del", Vk(0x2E)}, {"End", Vk(0x23)}, {"PgDn", Vk(0x22)},
    {nullptr, 0}, {nullptr, 0}, {nullptr, 0}, {nullptr, 0}, {nullptr, 0},
    {nullptr, 0}, {"@up", Vk(0x26)}, {nullptr, 0}, {nullptr, 0},
    {"?123", kCodeFn}, {"@ctrl", kCodeCtrl}, {"@win", kCodeWin}, {"@alt", kCodeAlt}, {nullptr, 0},
    {"@left", Vk(0x25)}, {"@down", Vk(0x28)}, {"@right", Vk(0x27)}, {"Bksp", 8},
    {"ABC", kCodeSymbols}, {",", ','}, {"@space", 32}, {".", '.'}, {"@enter", 13},
    {"@hide", kCodeHide},
};

constexpr const Key* kSheets[kSheetCount] = {kLower, kUpper, kSymbols, kFn};

const Key* KeyOf(int sheet, int index) {
    if (sheet < 0 || sheet >= kSheetCount || index < 0 || index >= kKeyCount) {
        return nullptr;
    }
    return &kSheets[sheet][index];
}

// The characters a US keyboard types with Shift held.
bool ShiftedOnUs(char32_t c) {
    if (c >= 'A' && c <= 'Z') {
        return true;
    }
    return c < 128 && c != 0 && std::strchr("~!@#$%^&*()_+{}|:\"<>?", static_cast<int>(c)) != nullptr;
}

} // namespace

Rect KeyRect(int index) {
    Rect r;
    if (index < 0 || index >= kKeyCount) {
        return r;
    }
    int row = 0;
    int k = index;
    while (row < 5 && k >= kRowKeys[row]) {
        k -= kRowKeys[row];
        row++;
    }
    const float unit = (1.0f - 2.0f * kPadU) / kRowUnits;
    const float rowh = (1.0f - 2.0f * kPadV) / 5.0f;
    float x = kRowIndent[row];
    for (int i = 0; i < k; i++) {
        x += kRowWidths[row][i];
    }
    const float w = kRowWidths[row][k];
    r.x0 = kPadU + x * unit + kGapU * 0.5f;
    r.y0 = kPadV + static_cast<float>(row) * rowh + kGapV * 0.5f;
    r.x1 = kPadU + (x + w) * unit - kGapU * 0.5f;
    r.y1 = kPadV + static_cast<float>(row + 1) * rowh - kGapV * 0.5f;
    return r;
}

int KeyCode(int sheet, int index) {
    const Key* k = KeyOf(sheet, index);
    return k ? k->code : 0;
}

const char* KeyLabel(int sheet, int index) {
    const Key* k = KeyOf(sheet, index);
    return k ? k->label : nullptr;
}

int KeyAt(int sheet, float u, float v) {
    int found = -1;
    for (int i = 0; i < kKeyCount; i++) {
        if (KeyCode(sheet, i) == 0) {
            continue;
        }
        const Rect r = KeyRect(i);
        if (u >= r.x0 && u <= r.x1 && v >= r.y0 && v <= r.y1) {
            found = i;
        }
    }
    return found;
}

int ModifierBit(int code) {
    switch (code) {
    case kCodeCtrl:
        return kModCtrl;
    case kCodeAlt:
        return kModAlt;
    case kCodeWin:
        return kModWin;
    default:
        return 0;
    }
}

Press ApplyPress(int sheet, int mods, int code) {
    Press p;
    p.sheet = sheet;
    p.mods = mods;
    const int bit = ModifierBit(code);
    if (bit != 0) {
        p.mods = mods ^ bit;
    } else if (code == kCodeShift) {
        // Shift off the symbols page goes to the capitals rather than back where it came from.
        p.sheet = sheet == kSheetUpper ? kSheetLower : kSheetUpper;
    } else if (code == kCodeSymbols) {
        // The ABC key on the symbols and the Fn sheet, ?123 on the letters.
        p.sheet = sheet == kSheetSymbols || sheet == kSheetFn ? kSheetLower : kSheetSymbols;
    } else if (code == kCodeFn) {
        // Fn on the symbols, and ?123 in the same place on the Fn sheet.
        p.sheet = sheet == kSheetFn ? kSheetSymbols : kSheetFn;
    } else if (code == kCodeHide) {
        p.hide = true;
        p.mods = 0;
    } else if (code > 0) {
        p.code = code;
        p.code_mods = mods;
        p.mods = 0;
        // Shift is one shot over the letters, the way a phone keyboard behaves, and sticky over
        // the punctuation row above them.
        if (sheet == kSheetUpper && code >= 'A' && code <= 'Z') {
            p.sheet = kSheetLower;
        }
    }
    return p;
}

int StickyBit(int code) {
    if (code == kCodeShift) {
        return kModShift;
    }
    return ModifierBit(code);
}

void Sticky::Reset() {
    locked = 0;
    hold_bit = 0;
    hold_start_ns = 0;
}

bool Sticky::OnPress(int code, int64_t now_ns) {
    const int bit = StickyBit(code);
    hold_bit = 0;
    if (bit == 0) {
        return false;
    }
    if (locked & bit) {
        locked &= ~bit;
        return true;
    }
    hold_bit = bit;
    hold_start_ns = now_ns;
    return false;
}

int Sticky::Tick(bool down, int64_t now_ns, int64_t hold_ns) {
    if (hold_bit == 0) {
        return 0;
    }
    if (!down) {
        hold_bit = 0;
        return 0;
    }
    if (now_ns - hold_start_ns < hold_ns) {
        return 0;
    }
    const int bit = hold_bit;
    locked |= bit;
    hold_bit = 0;
    return bit;
}

int Sticky::Mods(int mods) const {
    return mods | (locked & ~kModShift);
}

int Sticky::SheetFor(int sheet) const {
    if ((locked & kModShift) && sheet == kSheetLower) {
        return kSheetUpper;
    }
    return sheet;
}

bool MakeEvent(int code, int mods, KeyboardEvent& out) {
    using Key = KeyboardEvent::Key;
    out = KeyboardEvent{};
    out.modifiers = static_cast<uint8_t>(mods & (kModCtrl | kModAlt | kModWin));
    switch (code) {
    case 8:
        out.key = Key::Backspace;
        out.text = "\b";
        return true;
    case 9:
        out.key = Key::Tab;
        out.text = "\t";
        return true;
    case 13:
        out.key = Key::Enter;
        out.text = "\r";
        return true;
    default:
        break;
    }
    if (code >= 32 && code <= 126) {
        out.key = Key::Character;
        out.character = static_cast<char32_t>(code);
        out.text = std::string(1, static_cast<char>(code));
        if (ShiftedOnUs(out.character)) {
            out.modifiers |= KeyboardEvent::kShift;
        }
        return true;
    }
    if (code < kCodeVk) {
        return false;
    }
    const int vk = code - kCodeVk;
    if (vk >= 0x70 && vk <= 0x7B) {
        out.key = static_cast<Key>(static_cast<int>(Key::F1) + (vk - 0x70));
        return true;
    }
    switch (vk) {
    case 0x1B:
        out.key = Key::Escape;
        out.text = "\x1b";
        return true;
    case 0x2D:
        out.key = Key::Insert;
        return true;
    case 0x2E:
        out.key = Key::Delete;
        return true;
    case 0x24:
        out.key = Key::Home;
        return true;
    case 0x23:
        out.key = Key::End;
        return true;
    case 0x21:
        out.key = Key::PageUp;
        return true;
    case 0x22:
        out.key = Key::PageDown;
        return true;
    case 0x25:
        out.key = Key::Left;
        return true;
    case 0x26:
        out.key = Key::Up;
        return true;
    case 0x27:
        out.key = Key::Right;
        return true;
    case 0x28:
        out.key = Key::Down;
        return true;
    default:
        return false;
    }
}

} // namespace FrameXr::Keyboard
