// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: the game's keyboard in Material 3 (swkbd_view.h).

#include "frame_xr/menu/swkbd_view.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "frame_xr/menu/md3_fonts.h"

namespace FrameXr::Swkbd {

namespace {

using Menu::ButtonStyle;
using Menu::Kit;
using Menu::State;
using Menu::Type;
namespace Tok = Menu::Tok;

constexpr float kKeyRadius = 12.0f;
constexpr float kTextSize = 26.0f; ///< Type::Headline: the typed text and the letter keys
constexpr float kLineH = 32.0f;    ///< a line of typed text
constexpr float kFieldInset = 16.0f;
constexpr float kIconSize = 30.0f; ///< Shift and Backspace (Material Symbols, as Gboard)

Menu::Rect R(const Rect& r) {
    return {r.x0, r.y0, r.x1, r.y1};
}

State StateOf(int index, int hover, int pressed) {
    if (index == pressed) {
        return State::Pressed;
    }
    return index == hover ? State::Hover : State::Rest;
}

/// The typed text in lines: a new line, or wrapping (by character) at `width` dp.
struct Line {
    int start = 0, end = 0; ///< [start, end) in the text
};

std::vector<Line> Lines(const Kit& kit, const std::u32string& text, float width, bool wrap) {
    std::vector<Line> lines;
    Line cur;
    for (int i = 0; i < static_cast<int>(text.size()); i++) {
        const char32_t c = text[static_cast<std::size_t>(i)];
        if (c == U'\n') {
            cur.end = i;
            lines.push_back(cur);
            cur = {i + 1, i + 1};
            continue;
        }
        if (wrap && i > cur.start) {
            const std::u32string part = text.substr(static_cast<std::size_t>(cur.start),
                                                    static_cast<std::size_t>(i + 1 - cur.start));
            if (kit.TextWidth(Type::Headline, ToUtf8(part)) > width) {
                cur.end = i;
                lines.push_back(cur);
                cur = {i, i};
            }
        }
    }
    cur.end = static_cast<int>(text.size());
    lines.push_back(cur);
    return lines;
}

void DrawField(Kit& kit, const Model& m) {
    const Layout& l = m.GetLayout();
    const Spec& spec = m.GetSpec();
    const bool error = !m.Error().empty();
    const Menu::Rect f = R(l.field);
    // Material 3 filled text field: the container with its top corners round, the active
    // indicator under it, the label (the game's hint) at the top.
    kit.FillCorners(f, 4.0f, 4.0f, 0.0f, 0.0f, Tok::kSurfaceContainerHighest);
    kit.Fill({f.x0, f.y1 - 2.0f, f.x1, f.y1}, 0.0f, error ? Tok::kError : Tok::kPrimary);
    const float inner_w = f.W() - 2.0f * kFieldInset;
    const bool has_label = !spec.hint.empty();
    if (has_label) {
        kit.Text(f.x0 + kFieldInset, f.y0 + 24.0f, Type::Tooltip,
                 error ? Tok::kError : Tok::kPrimary, kit.Fit(Type::Tooltip, spec.hint, inner_w));
    }

    const std::u32string& text = m.Text();
    const std::vector<Line> lines = Lines(kit, text, inner_w, spec.multiline);
    int caret_line = 0;
    for (int i = 0; i < static_cast<int>(lines.size()); i++) {
        const Line& ln = lines[static_cast<std::size_t>(i)];
        if (m.Caret() >= ln.start && m.Caret() <= ln.end) {
            caret_line = i; // the later line when the caret is on a wrap
        }
    }
    const float first_base =
        has_label ? f.y0 + 58.0f : (spec.multiline ? f.y0 + 40.0f : f.Cy() + 9.5f);
    const int fits =
        std::max(1, static_cast<int>((f.y1 - 8.0f - (first_base - kTextSize)) / kLineH));
    const int first = std::max(0, caret_line - (fits - 1));

    kit.Clip({f.x0 + kFieldInset - 2.0f, f.y0, f.x1 - kFieldInset + 2.0f, f.y1 - 2.0f});
    for (int i = first; i < static_cast<int>(lines.size()) && i < first + fits; i++) {
        const Line& ln = lines[static_cast<std::size_t>(i)];
        const float base = first_base + static_cast<float>(i - first) * kLineH;
        const std::string s = ToUtf8(text.substr(static_cast<std::size_t>(ln.start),
                                                 static_cast<std::size_t>(ln.end - ln.start)));
        // One line longer than the field scrolls so the caret stays in view.
        float shift = 0.0f;
        float caret_x = -1.0f;
        if (i == caret_line) {
            const std::string before =
                ToUtf8(text.substr(static_cast<std::size_t>(ln.start),
                                   static_cast<std::size_t>(m.Caret() - ln.start)));
            caret_x = kit.TextWidth(Type::Headline, before);
            shift = std::max(0.0f, caret_x - inner_w + 4.0f);
        }
        const float x = f.x0 + kFieldInset - shift;
        kit.Text(x, base, Type::Headline, Tok::kOnSurface, s);
        if (caret_x >= 0.0f) {
            kit.Fill({x + caret_x, base - 24.0f, x + caret_x + 2.0f, base + 6.0f}, 0.0f,
                     error ? Tok::kError : Tok::kPrimary);
        }
    }
    kit.ClearClip();

    // Supporting text: why the text was refused (left), the count (right).
    const Menu::Rect s = R(l.supporting);
    const float base = s.y0 + 20.0f;
    const std::string count = m.Counter();
    const float cw = kit.Text(s.x1, base, Type::Tooltip, Tok::kOnSurfaceVariant, count, 2);
    if (error) {
        kit.Text(s.x0, base, Type::Tooltip, Tok::kError,
                 kit.Fit(Type::Tooltip, m.Error(), s.W() - cw - 24.0f));
    }
}

void DrawKey(Kit& kit, const Model& m, const Key& k, State state) {
    const Menu::Rect r = R(k.r);
    uint32_t container = Tok::kSurfaceContainerHighest, content = Tok::kOnSurface;
    Type type = Type::Label;
    char32_t icon = 0;
    bool filled = false;
    switch (k.kind) {
    case KeyKind::Char:
        type = Type::Headline;
        break;
    case KeyKind::Space:
        break;
    case KeyKind::Shift:
        icon = Menu::Icon::kShift;
        if (m.GetShift() == Shift::Locked) {
            container = Tok::kPrimary;
            content = Tok::kOnPrimary;
            icon = Menu::Icon::kShiftLock;
            filled = true;
        } else if (m.GetShift() == Shift::Once) {
            container = Tok::kPrimaryContainer;
            content = Tok::kOnPrimaryContainer;
            filled = true;
        } else {
            container = Tok::kSecondaryContainer;
            content = Tok::kOnSecondaryContainer;
        }
        break;
    case KeyKind::Backspace:
        container = Tok::kSecondaryContainer;
        content = Tok::kOnSecondaryContainer;
        icon = Menu::Icon::kBackspace;
        break;
    case KeyKind::Enter:
    case KeyKind::PageSwitch:
        container = Tok::kSecondaryContainer;
        content = Tok::kOnSecondaryContainer;
        break;
    case KeyKind::Button: {
        const Button& b = m.GetSpec().buttons[static_cast<std::size_t>(k.button)];
        kit.Button(r, b.confirm ? ButtonStyle::Filled : ButtonStyle::Tonal, 0,
                   kit.Fit(Type::Label, k.label, r.W() - 32.0f), state);
        return;
    }
    }
    kit.Fill(r, kKeyRadius, container);
    kit.StateLayer(r, kKeyRadius, content, state);
    if (icon) {
        kit.Icon(r.Cx(), r.Cy(), kIconSize, icon, filled, content);
    } else if (type == Type::Headline) {
        kit.TextMid(r.Cx(), r.Cy(), type, content, k.label, 1);
    } else {
        kit.TextMid(r.Cx(), r.Cy(), type, content, kit.Fit(type, k.label, r.W() - 12.0f), 1);
    }
}

} // namespace

void Draw(Kit& kit, const Model& m, int hover, int pressed) {
    kit.Fill({0, 0, kPanelW, kPanelH}, kPanelRadius, Tok::kSurfaceContainer);
    DrawField(kit, m);
    const auto& keys = m.GetLayout().keys;
    for (int i = 0; i < static_cast<int>(keys.size()); i++) {
        DrawKey(kit, m, keys[static_cast<std::size_t>(i)], StateOf(i, hover, pressed));
    }
}

} // namespace FrameXr::Swkbd
