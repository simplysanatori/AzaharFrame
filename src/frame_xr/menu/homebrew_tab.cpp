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
//   Pokémon Emerald 3DS               Uninstall  [Repair]                    broken (emhb)
//   ZallaxDev · Game Data Missing
//
// AzaharFrame emhb: Uninstall (a text button, last on the left) whenever anything is there, and
// its dialog over the whole menu (HomebrewTabModel::dialog). The dialog is drawn here, as the
// Mods tab's (mods_segments.cpp DrawModsDialog) with a checkbox row: switch to that one's once it
// has a checkbox of its own.

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
constexpr float kRowTwo = 120.0f; ///< AzaharFrame emhb: its buttons on two lines
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

/// AzaharFrame emhb: words of `s` in lines at most `width` wide (as mods_segments.cpp's Wrap).
std::vector<std::string> Wrap(const Kit& kit, const std::string& s, float size, float width,
                              std::size_t max_lines) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t at = 0;
    while (at < s.size()) {
        std::size_t end = s.find(' ', at);
        if (end == std::string::npos) {
            end = s.size();
        }
        const std::string word = s.substr(at, end - at);
        const std::string next = line.empty() ? word : line + " " + word;
        if (!line.empty() && kit.TextWidthSized(size, false, next) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = next;
        }
        at = end + 1;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    if (lines.size() > max_lines) {
        lines.resize(max_lines);
    }
    return lines;
}

/// AzaharFrame emhb: Material 3 checkbox, 18 dp (as mods_segments.cpp's).
void CheckBox(Kit& kit, float cx, float cy, bool checked) {
    const Rect b{cx - 9.0f, cy - 9.0f, cx + 9.0f, cy + 9.0f};
    if (!checked) {
        kit.Stroke(b, 2.0f, 2.0f, Tok::kOnSurfaceVariant, 1.0f);
        return;
    }
    kit.Fill(b, 2.0f, Tok::kPrimary);
    kit.Line(cx - 5.0f, cy + 0.5f, cx - 1.5f, cy + 4.0f, 2.0f, Tok::kOnPrimary);
    kit.Line(cx - 1.5f, cy + 4.0f, cx + 5.5f, cy - 3.5f, 2.0f, Tok::kOnPrimary);
}

} // namespace

std::vector<Id> HomebrewButtons(const HomebrewItem& item) {
    if (item.busy) {
        return {};
    }
    std::vector<Id> out;
    if (item.repair) { // AzaharFrame emhb: some of it is there, but it cannot be played
        out = {Id::HomebrewInstall};
    } else if (!item.installed) {
        out = {Id::HomebrewInstall};
    } else if (item.update) {
        out = {Id::HomebrewPlay, Id::HomebrewUpdate};
    } else {
        out = {Id::HomebrewPlay, Id::HomebrewCheck, Id::HomebrewReinstall};
    }
    if (item.can_uninstall) { // AzaharFrame emhb
        out.push_back(Id::HomebrewUninstall);
    }
    return out;
}

std::string HomebrewButtonLabel(const HomebrewItem& item, Id id) { // AzaharFrame emhb
    if (id == Id::HomebrewInstall && item.repair) {
        return "Repair";
    }
    if (id == Id::HomebrewUninstall) {
        return "Uninstall";
    }
    return LabelFor(id);
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
    // AzaharFrame emhb: a row whose title would not fit beside all its buttons (Play, Check for
    // Updates, Reinstall and Uninstall) has the text buttons on a second line, under the others
    const auto width_of = [&kit](const HomebrewItem& it, Id id) {
        if (id == Id::HomebrewCheck) {
            return std::max(ButtonWidth(kit, LabelFor(Id::HomebrewCheck), false),
                            ButtonWidth(kit, HomebrewCheckLabel(it), true));
        }
        const bool text = id == Id::HomebrewReinstall || id == Id::HomebrewUninstall;
        const bool icon = id == Id::HomebrewPlay || (id == Id::HomebrewInstall && !it.repair);
        return ButtonWidth(kit, HomebrewButtonLabel(it, id), icon) - (text ? 24.0f : 0.0f);
    };
    const auto is_text = [](Id id) {
        return id == Id::HomebrewReinstall || id == Id::HomebrewUninstall;
    };
    std::vector<bool> two_lines(m.items.size(), false);
    float list_h = 0.0f;
    for (std::size_t i = 0; i < m.items.size(); i++) {
        const HomebrewItem& it = m.items[i];
        float all = 0.0f;
        bool text = false, other = false;
        for (const Id id : HomebrewButtons(it)) {
            all += width_of(it, id) + kGap;
            (is_text(id) ? text : other) = true;
        }
        two_lines[i] = text && other &&
                       c.W() - all + kGap - kNameGap < kit.TextWidth(Type::Body, it.title);
        list_h += two_lines[i] ? kRowTwo : kRow;
    }
    layout.max_scroll = std::max(0.0f, std::ceil((list_h - c.H()) / kRow) * kRow);
    const float scroll =
        std::clamp(view.scroll[static_cast<int>(Tab::Homebrew)], 0.0f, layout.max_scroll);
    // AzaharFrame scroll: the list between the paddings; a part row shows cut at its edges
    const Rect list{area.x0, c.y0, area.x1, c.y1};
    kit.Clip(list);
    float y = c.y0 - scroll;
    for (std::size_t i = 0; i < m.items.size(); i++) {
        const HomebrewItem& it = m.items[i];
        const int idx = static_cast<int>(i);
        const float row_h = two_lines[i] ? kRowTwo : kRow;
        const float row_y = y;
        y += row_h;
        if (row_y + row_h <= list.y0 || row_y >= list.y1) {
            continue; // out of view
        }
        // the text's centre line as in a one-line row; the buttons on it, or (two lines) the
        // first ones level with the title and the text buttons under them
        const float text_cy = row_y + kRow * 0.5f;
        float cy = two_lines[i] ? row_y + kRow * 0.5f - 12.0f : text_cy;
        if (i > 0) {
            kit.Line(c.x0, row_y, c.x1, row_y, 1.0f, Tok::kOutlineVariant);
        }
        float right = c.x1;
        const auto button = [&](Id id, ButtonStyle style, char32_t icon) {
            const std::string label = HomebrewButtonLabel(it, id); // AzaharFrame emhb
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
            const float w = width_of(it, Id::HomebrewCheck);
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
        float title_right = c.x1 + kGap; // where the buttons beside the title begin
        for (const Id id : HomebrewButtons(it)) {
            if (two_lines[i] && is_text(id) && cy < text_cy) {
                title_right = right + kGap; // the second line of buttons
                cy = row_y + kRowTwo - 12.0f - kButtonH * 0.5f;
                right = c.x1;
            }
            switch (id) {
            case Id::HomebrewInstall: // AzaharFrame emhb: Repair has no icon (it rebuilds, no download only)
                button(id, ButtonStyle::Filled, it.repair ? 0 : Sym::kDownload);
                break;
            case Id::HomebrewPlay:
                button(id, ButtonStyle::Filled, Sym::kPlayArrow);
                break;
            case Id::HomebrewCheck: // AzaharFrame ui3
                check();
                break;
            case Id::HomebrewReinstall: // AzaharFrame ui3: quieter, a repair
            case Id::HomebrewUninstall: // AzaharFrame emhb
                button(id, ButtonStyle::Text, 0);
                break;
            default: // Update
                button(id, ButtonStyle::Tonal, 0);
                break;
            }
        }
        if (!two_lines[i]) {
            title_right = right + kGap;
        }
        const float title_w = title_right - kNameGap - c.x0;
        const float line_w = right + kGap - kNameGap - c.x0; // two lines: beside the text buttons
        kit.TextMid(c.x0, text_cy - 12.0f, Type::Body, Tok::kOnSurface,
                    kit.Fit(Type::Body, it.title, title_w));
        const std::string line = HomebrewLine(it);
        const bool error = !it.busy && !it.status.empty();
        kit.TextSized(c.x0, text_cy + 22.0f, kSmall, false, error ? Tok::kError : Tok::kOnSurfaceVariant,
                      kit.Fit(Type::Tooltip, line, two_lines[i] ? line_w : title_w));
    }
    kit.ClearClip();
    DrawScrollbar(kit, view, list, area.x1, list_h, layout.max_scroll, scroll); // AzaharFrame scroll
    DrawHomebrewDialog(kit, m.dialog, view); // AzaharFrame emhb
}

/// AzaharFrame emhb: Uninstall's dialog: Material 3 basic dialog over a scrim on the whole panel
/// (nothing under it is hit): headline, supporting text, the checkbox row, then text buttons at
/// the end, the confirming one last.
void TabMenu::DrawHomebrewDialog(Kit& kit, const HomebrewDialog& d, const MenuView& view) {
    if (d.id.empty()) {
        return;
    }
    const Rect panel{0.0f, 0.0f, kPanelW, kPanelH};
    kit.Fill(panel, kPanelRadius, 0x000000, 0.32f);
    AddHit(panel, {Id::None, kMenuAreaIndex}, panel);
    layout.scroll_locked = true;
    constexpr float kW = 440.0f, kP = 24.0f, kHead = 26.0f, kBody = 16.0f;
    constexpr float kHeadLh = 32.0f, kBodyLh = 24.0f, kCheckRow = 48.0f;
    const float tw = kW - kP * 2.0f;
    const auto head = Wrap(kit, d.title, kHead, tw, 3);
    const auto body = Wrap(kit, d.body, kBody, tw, 4);
    const bool check = !d.check.empty();
    const float h = kP + kHeadLh * static_cast<float>(head.size()) + 16.0f +
                    kBodyLh * static_cast<float>(body.size()) + (check ? 8.0f + kCheckRow : 0.0f) +
                    24.0f + kButtonH + kP;
    const Rect box{(kPanelW - kW) * 0.5f, (kPanelH - h) * 0.5f, (kPanelW + kW) * 0.5f,
                   (kPanelH + h) * 0.5f};
    kit.Fill(box, 28.0f, Tok::kSurfaceContainerHigh);
    float y = box.y0 + kP;
    for (const std::string& l : head) {
        kit.TextSized(box.x0 + kP, y + kHeadLh * 0.5f + kit.CapHeightSized(kHead, false) * 0.5f,
                      kHead, false, Tok::kOnSurface, l);
        y += kHeadLh;
    }
    y += 16.0f;
    for (const std::string& l : body) {
        kit.TextSized(box.x0 + kP, y + kBodyLh * 0.5f + kit.CapHeightSized(kBody, false) * 0.5f,
                      kBody, false, Tok::kOnSurfaceVariant, l);
        y += kBodyLh;
    }
    if (check) {
        // The checkbox and its label are one target (48 dp high), the state layer on the box
        y += 8.0f;
        const float cy = y + kCheckRow * 0.5f;
        const float cx = box.x0 + kP + 9.0f;
        const Target t{Id::HomebrewDialogCheck};
        const State st = StateOf(view, t);
        if (st != State::Rest) {
            kit.Dot(cx, cy, 20.0f, d.checked ? Tok::kPrimary : Tok::kOnSurface,
                    st == State::Pressed ? kPressedLayer : kHoverLayer);
        }
        CheckBox(kit, cx, cy, d.checked);
        const float lx = cx + 9.0f + 16.0f;
        kit.TextSized(lx, cy + kit.CapHeightSized(kBody, false) * 0.5f, kBody, false,
                      Tok::kOnSurface, d.check);
        AddHit({box.x0 + kP - 11.0f, y, lx + kit.TextWidthSized(kBody, false, d.check) + 8.0f,
                y + kCheckRow},
               t, panel);
    }
    float right = box.x1 - kP + 12.0f; // a text button's own padding lines its label up
    const float by = box.y1 - kP - kButtonH;
    const auto action = [&](Id id, const std::string& label) {
        const float w = kit.TextWidth(Type::Label, label) + 24.0f;
        const Rect b{right - w, by, right, by + kButtonH};
        kit.Button(b, ButtonStyle::Text, 0, label, StateOf(view, {id}));
        AddHit({b.x0, b.Cy() - 24.0f, b.x1, b.Cy() + 24.0f}, {id}, panel);
        right = b.x0 - 8.0f;
    };
    action(Id::HomebrewDialogConfirm, d.confirm);
    if (!d.cancel.empty()) {
        action(Id::HomebrewDialogCancel, d.cancel);
    }
}

} // namespace FrameXr::Menu
