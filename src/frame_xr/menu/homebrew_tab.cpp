// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: the Mods tab's Homebrew segment (homebrew_tab.h), Material 3 under the
// segmented button:
//
//   Pokémon Emerald 3DS                                 [Install]           not installed
//   ZallaxDev
//   Pokémon Emerald 3DS                       [Update]  [> Play]            installed
//   ZallaxDev
//   Pokémon Emerald 3DS                                    (o)              installing
//   Downloading 42%

#include <algorithm>
#include <cmath>

#include "frame_xr/menu/homebrew_tab.h"
#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;

namespace {

constexpr float kPad = 24.0f; ///< inside the pane, as the other tabs
constexpr float kRow = 88.0f; ///< a two-line row with its buttons
constexpr float kButtonH = 40.0f;
constexpr float kGap = 8.0f;      ///< between two buttons
constexpr float kNameGap = 24.0f; ///< between the text and the buttons
constexpr float kSmall = 14.0f;   ///< the supporting line (Body Small, a step up)
constexpr float kPi = 3.14159265f;

float ButtonWidth(const Kit& kit, const std::string& label, bool icon) {
    return kit.TextWidth(Type::Label, label) + (icon ? 28.0f : 0.0f) + 48.0f;
}

void Arc(Kit& kit, float cx, float cy, float r, float a0, float a1, float width, uint32_t rgb) {
    if (a1 <= a0) {
        return;
    }
    const int n = std::max(1, static_cast<int>(std::ceil((a1 - a0) / (kPi / 30.0f))));
    float px = cx + r * std::sin(a0), py = cy - r * std::cos(a0);
    for (int i = 1; i <= n; i++) {
        const float a = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(n);
        const float x = cx + r * std::sin(a), y = cy - r * std::cos(a);
        kit.Line(px, py, x, y, width, rgb);
        px = x;
        py = y;
    }
}

/// Material 3 circular progress, 32 dp: `frac` of the ring, or turning with `spin` when < 0.
void Ring(Kit& kit, float cx, float cy, float frac, int spin) {
    constexpr float stroke = 4.0f, r = 16.0f - stroke * 0.5f;
    const float gap = (stroke + 4.0f) / r;
    float start = 0.0f, len;
    if (frac < 0.0f) {
        const float t = static_cast<float>(spin) / 15.0f;
        start = std::fmod(t * 1.6f * kPi, 2.0f * kPi);
        len = kPi * (0.35f + 0.55f * (0.5f - 0.5f * std::cos(t * 2.0f * kPi / 1.5f)));
    } else {
        len = 2.0f * kPi * std::clamp(frac, 0.0f, 1.0f);
    }
    Arc(kit, cx, cy, r, start, start + len, stroke, Tok::kPrimary);
    const float g = len > 0.0f ? gap : 0.0f;
    Arc(kit, cx, cy, r, start + len + g, start + 2.0f * kPi - g, stroke, Tok::kSecondaryContainer);
}

} // namespace

void TabMenu::DrawHomebrew(Kit& kit, const MenuModel& model, const MenuView& view,
                           const Rect& area) {
    const HomebrewTabModel& m = model.homebrew;
    const Rect c{area.x0 + kPad, area.y0, area.x1 - kPad, area.y1 - kPad};
    layout.max_scroll = 0.0f;
    layout.scroll_step = kRow;
    if (m.items.empty()) {
        kit.Icon(c.Cx(), c.Cy() - 24.0f, 48.0f, Sym::kCode, false, Tok::kOnSurfaceVariant);
        kit.TextMid(c.Cx(), c.Cy() + 28.0f, Type::Title, Tok::kOnSurface, "No Homebrew", 1);
        return;
    }
    const float list_h = kRow * static_cast<float>(m.items.size());
    layout.max_scroll = std::max(0.0f, std::ceil((list_h - c.H()) / kRow) * kRow);
    const float scroll =
        std::clamp(view.scroll[static_cast<int>(Tab::Mods)], 0.0f, layout.max_scroll);
    kit.Clip(area);
    for (std::size_t i = 0; i < m.items.size(); i++) {
        const HomebrewItem& it = m.items[i];
        const int idx = static_cast<int>(i);
        const float y = c.y0 + kRow * static_cast<float>(i) - scroll;
        if (y < c.y0 - 0.01f || y + kRow > c.y1 + kPad + 0.01f) {
            continue; // whole rows only
        }
        const float cy = y + kRow * 0.5f;
        if (i > 0) {
            kit.Line(c.x0, y, c.x1, y, 1.0f, Tok::kOutlineVariant);
        }
        float right = c.x1;
        const auto button = [&](Id id, ButtonStyle style, char32_t icon) {
            const std::string label = LabelFor(id);
            const float w = ButtonWidth(kit, label, icon != 0);
            const Rect b{right - w, cy - kButtonH * 0.5f, right, cy + kButtonH * 0.5f};
            kit.Button(b, style, icon, label, StateOf(view, {id, idx}));
            AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {id, idx}, area);
            right = b.x0 - kGap;
        };
        if (it.busy) {
            Ring(kit, right - 16.0f, cy, it.progress < 0 ? -1.0f : it.progress / 1000.0f, m.spin);
            right -= 32.0f + kGap;
        } else if (!it.installed) {
            button(Id::HomebrewInstall, ButtonStyle::Filled, Sym::kDownload);
        } else {
            button(Id::HomebrewPlay, ButtonStyle::Filled, Sym::kPlayArrow);
            if (it.update) {
                button(Id::HomebrewUpdate, ButtonStyle::Tonal, 0);
            }
        }
        const float text_w = right + kGap - kNameGap - c.x0;
        kit.TextMid(c.x0, cy - 12.0f, Type::Body, Tok::kOnSurface,
                    kit.Fit(Type::Body, it.title, text_w));
        std::string line = it.status.empty() ? it.author : it.status;
        if (it.busy && it.progress >= 0) {
            line += " " + std::to_string(std::clamp(it.progress, 0, 1000) / 10) + "%";
        }
        const bool error = !it.busy && !it.status.empty();
        kit.TextSized(c.x0, cy + 22.0f, kSmall, false, error ? Tok::kError : Tok::kOnSurfaceVariant,
                      kit.Fit(Type::Tooltip, line, text_w));
    }
    kit.ClearClip();
}

} // namespace FrameXr::Menu
