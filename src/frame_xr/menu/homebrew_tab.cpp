// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: the Homebrew tab (homebrew_tab.h; a segment of the Mods tab until ui3),
// Material 3:
//
//   Pokémon Emerald 3DS                                 [Install]           not installed
//   ZallaxDev
//   Pokémon Emerald 3DS                       [Update]  [> Play]            a newer release seen
//   ZallaxDev · English
//   Pokémon Emerald 3DS     Reinstall  [Check for Updates]  [> Play]       the latest installed
//   ZallaxDev · English · v0.5.0 · Checked Just Now                          (ui3)
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

/// Material 3 circular progress, `size` dp (32; 20 inside a button): `frac` of the ring, or
/// turning with `spin` when < 0.
void Ring(Kit& kit, float cx, float cy, float frac, int spin, float size = 32.0f) {
    const float stroke = size >= 32.0f ? 4.0f : 3.0f, r = size * 0.5f - stroke * 0.5f;
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

std::vector<Id> HomebrewButtons(const HomebrewItem& item) {
    if (item.busy) {
        return {};
    }
    if (!item.installed) {
        return {Id::HomebrewInstall};
    }
    if (item.update) {
        return {Id::HomebrewPlay, Id::HomebrewUpdate};
    }
    return {Id::HomebrewPlay, Id::HomebrewCheck, Id::HomebrewReinstall};
}

std::string HomebrewLine(const HomebrewItem& item) {
    if (!item.status.empty()) {
        std::string line = item.status;
        if (item.busy && item.progress >= 0) {
            line += " " + std::to_string(std::clamp(item.progress, 0, 1000) / 10) + "%";
        }
        return line;
    }
    std::string line = item.author;
    const auto add = [&line](const std::string& part) {
        if (!part.empty()) {
            line += (line.empty() ? "" : " \u00b7 ") + part;
        }
    };
    if (item.installed) {
        add(item.version);
        add(item.update ? (item.latest.empty() ? std::string{} : item.latest + " Available")
                        : item.checked);
    }
    return line;
}

std::string HomebrewCheckLabel(const HomebrewItem& item) {
    return item.checking ? "Checking" : LabelFor(Id::HomebrewCheck);
}

void TabMenu::DrawHomebrew(Kit& kit, const MenuModel& model, const MenuView& view,
                           const Rect& pane) {
    const HomebrewTabModel& m = model.homebrew;
    // AzaharFrame ui3: its own tab now: the whole pane, padded as the other tabs
    const Rect area = pane;
    const Rect c{area.x0 + kPad, area.y0 + kPad, area.x1 - kPad, area.y1 - kPad};
    layout.max_scroll = 0.0f;
    layout.scroll_step = kRow;
    if (m.items.empty()) {
        kit.Icon(c.Cx(), c.Cy() - 24.0f, 48.0f, Sym::kDeployedCode, false, Tok::kOnSurfaceVariant);
        kit.TextMid(c.Cx(), c.Cy() + 28.0f, Type::Title, Tok::kOnSurface, "No Homebrew", 1);
        return;
    }
    const float list_h = kRow * static_cast<float>(m.items.size());
    layout.max_scroll = std::max(0.0f, std::ceil((list_h - c.H()) / kRow) * kRow);
    const float scroll =
        std::clamp(view.scroll[static_cast<int>(Tab::Homebrew)], 0.0f, layout.max_scroll);
    // AzaharFrame scroll: the list between the paddings; a part row shows cut at its edges
    const Rect list{area.x0, c.y0, area.x1, c.y1};
    kit.Clip(list);
    for (std::size_t i = 0; i < m.items.size(); i++) {
        const HomebrewItem& it = m.items[i];
        const int idx = static_cast<int>(i);
        const float y = c.y0 + kRow * static_cast<float>(i) - scroll;
        if (y + kRow <= list.y0 || y >= list.y1) {
            continue; // out of view
        }
        const float cy = y + kRow * 0.5f;
        if (i > 0) {
            kit.Line(c.x0, y, c.x1, y, 1.0f, Tok::kOutlineVariant);
        }
        float right = c.x1;
        const auto button = [&](Id id, ButtonStyle style, char32_t icon) {
            const std::string label = LabelFor(id);
            // AzaharFrame ui3: a text button's container is drawn 12 dp out, as M3's
            const float w = ButtonWidth(kit, label, icon != 0) - (style == ButtonStyle::Text ? 24.0f : 0.0f);
            const Rect b{right - w, cy - kButtonH * 0.5f, right, cy + kButtonH * 0.5f};
            kit.Button(b, style, icon, label, StateOf(view, {id, idx}));
            AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {id, idx}, list);
            right = b.x0 - kGap;
        };
        // AzaharFrame ui3: Check for Updates; while it asks, "Checking" with a small ring in the
        // icon's place, not pressable
        const auto check = [&] {
            const std::string label = HomebrewCheckLabel(it);
            // as wide either way, so Reinstall does not move while it checks
            const float w = std::max(ButtonWidth(kit, LabelFor(Id::HomebrewCheck), false),
                                     ButtonWidth(kit, label, true));
            const Rect b{right - w, cy - kButtonH * 0.5f, right, cy + kButtonH * 0.5f};
            if (it.checking) {
                kit.Button(b, ButtonStyle::Tonal, 0, "", State::Disabled);
                const float lw = kit.TextWidth(Type::Label, label);
                const float x0 = b.Cx() - (20.0f + 8.0f + lw) * 0.5f;
                Ring(kit, x0 + 10.0f, cy, -1.0f, m.spin, 20.0f);
                kit.TextMid(x0 + 28.0f, cy, Type::Label, Tok::kOnSurface, label, 0);
            } else {
                kit.Button(b, ButtonStyle::Tonal, 0, label, StateOf(view, {Id::HomebrewCheck, idx}));
                AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {Id::HomebrewCheck, idx}, list);
            }
            right = b.x0 - kGap;
        };
        if (it.busy) {
            Ring(kit, right - 16.0f, cy, it.progress < 0 ? -1.0f : it.progress / 1000.0f, m.spin);
            right -= 32.0f + kGap;
        }
        for (const Id id : HomebrewButtons(it)) {
            switch (id) {
            case Id::HomebrewInstall:
                button(id, ButtonStyle::Filled, Sym::kDownload);
                break;
            case Id::HomebrewPlay:
                button(id, ButtonStyle::Filled, Sym::kPlayArrow);
                break;
            case Id::HomebrewCheck: // AzaharFrame ui3
                check();
                break;
            case Id::HomebrewReinstall: // AzaharFrame ui3: quieter, a repair
                button(id, ButtonStyle::Text, 0);
                break;
            default: // Update
                button(id, ButtonStyle::Tonal, 0);
                break;
            }
        }
        const float text_w = right + kGap - kNameGap - c.x0;
        kit.TextMid(c.x0, cy - 12.0f, Type::Body, Tok::kOnSurface,
                    kit.Fit(Type::Body, it.title, text_w));
        const std::string line = HomebrewLine(it);
        const bool error = !it.busy && !it.status.empty();
        kit.TextSized(c.x0, cy + 22.0f, kSmall, false, error ? Tok::kError : Tok::kOnSurfaceVariant,
                      kit.Fit(Type::Tooltip, line, text_w));
    }
    kit.ClearClip();
    DrawScrollbar(kit, view, list, area.x1, list_h, layout.max_scroll, scroll); // AzaharFrame scroll
}

} // namespace FrameXr::Menu
