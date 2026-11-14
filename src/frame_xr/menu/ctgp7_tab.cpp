// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctgp7: the Mods tab's CTGP-7 segment (ctgp7_tab.h), Material 3: the About section
// (a title, two short paragraphs, Official Website as a text button with open_in_new), a divider,
// then one list item with its trailing buttons and, while an install runs, a linear progress
// indicator under it. AzaharFrame ctgp7b: then, after a divider, Mods for CTGP-7: the catalog's
// rows as Install Mods draws them (mods_segments.cpp DrawOfferList), scrolling under the rest.

#include <algorithm>
#include <cmath>

#include "frame_xr/menu/ctgp7_tab.h"
#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;

namespace {

constexpr float kPad = 24.0f;     ///< inside the pane, as the other segments
constexpr float kTop = 8.0f;      ///< under the segmented button's gap
constexpr float kBody = 16.0f;    ///< the About text (Body Medium, a step up)
constexpr float kBodyLine = 24.0f;
constexpr float kSmall = 14.0f;   ///< a list item's supporting text
constexpr float kButtonH = 40.0f; ///< a button (its target is 48)
constexpr float kStatusRowH = 72.0f; ///< a two-line list item
constexpr float kNameGap = 24.0f;

float ButtonWidth(const Kit& kit, const std::string& label, bool icon, float side) {
    return kit.TextWidth(Type::Label, label) + (icon ? 28.0f : 0.0f) + side * 2.0f;
}

/// Words of `s` in lines at most `width` wide (at most `max_lines`; the last one cut to fit).
std::vector<std::string> WrapText(const Kit& kit, const std::string& s, float size, float width,
                                  int max_lines) {
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
    if (static_cast<int>(lines.size()) > max_lines) {
        std::string last = lines[static_cast<std::size_t>(max_lines - 1)];
        lines.resize(static_cast<std::size_t>(max_lines));
        while (last.size() > 1 && kit.TextWidthSized(size, false, last + "…") > width) {
            last.pop_back();
        }
        lines.back() = last + "…";
    }
    return lines;
}

} // namespace

std::string Ctgp7Line(const Ctgp7Model& m, bool* error_colour) {
    bool error = false;
    std::string line;
    if (m.busy) {
        line = m.detail;
    } else if (!m.error.empty()) {
        line = m.error;
        error = true;
    } else if (!m.installed && !m.blocker.empty()) {
        line = m.blocker;
        error = true;
    } else if (m.installed) {
        line = m.version.empty() ? std::string("Installed") : "Version " + m.version;
    } else if (m.setup_unfinished) {
        line = kCtgp7SetupUnfinished;
    } else {
        line = kCtgp7NotInstalled;
    }
    if (error_colour != nullptr) {
        *error_colour = error;
    }
    return line;
}

std::vector<int> Ctgp7Buttons(const Ctgp7Model& m) {
    if (m.busy) {
        return m.can_cancel ? std::vector<int>{static_cast<int>(Id::Ctgp7Cancel)} : std::vector<int>{};
    }
    if (m.installed) {
        return {static_cast<int>(Id::Ctgp7Launch), static_cast<int>(Id::Ctgp7Uninstall)};
    }
    return {static_cast<int>(Id::Ctgp7Install)};
}

namespace {
bool ForCtgp7(const ModsListModel& m, const std::string& id) {
    for (const ModOffer& o : m.available) {
        if (o.id == id) {
            return o.requires_ctgp7;
        }
    }
    return false;
}
} // namespace

std::vector<int> InstallModsRows(const ModsListModel& m, bool ctgp7_shown) {
    std::vector<int> out;
    for (std::size_t i = 0; i < m.available.size(); i++) {
        if (!ctgp7_shown || !m.available[i].requires_ctgp7) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

std::vector<int> Ctgp7ModRows(const ModsListModel& m, bool ctgp7_shown) {
    std::vector<int> out;
    for (std::size_t i = 0; ctgp7_shown && i < m.available.size(); i++) {
        if (m.available[i].requires_ctgp7) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

std::vector<int> LoadedRows(const ModsListModel& m, bool ctgp7_shown) {
    std::vector<int> out;
    for (std::size_t i = 0; i < m.loaded.size(); i++) {
        // one the catalog no longer lists stays here (the CTGP-7 segment lists the catalog's)
        if (!ctgp7_shown || !ForCtgp7(m, m.loaded[i].id)) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

bool Ctgp7ModsInstallable(const Ctgp7Model& m) {
    return m.installed && !m.busy;
}

std::vector<std::string> ModsSegmentLabels(bool ctgp7_shown) {
    std::vector<std::string> out = OptionsFor(Id::ModsSegment);
    if (ctgp7_shown) {
        out.emplace_back(kCtgp7Segment);
    }
    return out;
}

void TabMenu::DrawCtgp7(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area) {
    const Ctgp7Model& m = model.mods.ctgp7;
    const Rect c{area.x0 + kPad, area.y0 + kTop, area.x1 - kPad, area.y1 - kPad};
    // AzaharFrame ctgp7b: one page that scrolls as a whole (with Mods for CTGP-7 it outgrows the
    // pane); `shown` is what of it shows
    const Rect shown{area.x0, area.y0, area.x1, area.y1 - 8.0f};
    const std::vector<int> rows = Ctgp7ModRows(model.mods.mods, m.shown);
    OfferListOptions opt;
    opt.install_allowed = Ctgp7ModsInstallable(m);
    opt.manage = true;
    opt.in_page = true;
    float content = 40.0f;
    for (const char* para : {kCtgp7About1, kCtgp7About2}) {
        content += kBodyLine * static_cast<float>(WrapText(kit, para, kBody, c.W(), 3).size()) + 8.0f;
    }
    content += kButtonH + 16.0f + 8.0f + kStatusRowH + (m.busy ? 4.0f : 0.0f);
    if (!rows.empty()) {
        content += 8.0f + 8.0f + 40.0f + (m.installed ? 0.0f : 24.0f) +
                   OfferListHeight(kit, model.mods.mods, c, rows, opt);
    }
    const float max_scroll = std::max(0.0f, content - (shown.y1 - c.y0));
    const float scroll = std::clamp(view.scroll[static_cast<std::size_t>(view.tab)], 0.0f, max_scroll);
    layout.max_scroll = max_scroll;
    layout.scroll_step = 48.0f;
    float y = c.y0 - scroll;
    kit.Clip(shown);

    // ---- About CTGP-7 ----
    kit.TextMid(c.x0, y + 16.0f, Type::Title, Tok::kOnSurface, kCtgp7AboutTitle);
    y += 40.0f;
    for (const char* para : {kCtgp7About1, kCtgp7About2}) {
        for (const std::string& l : WrapText(kit, para, kBody, c.W(), 3)) {
            kit.TextSized(c.x0, y + kBodyLine * 0.5f + kit.CapHeightSized(kBody, false) * 0.5f, kBody,
                          false, Tok::kOnSurfaceVariant, l);
            y += kBodyLine;
        }
        y += 8.0f;
    }
    { // Official Website: a text button (its own 12 dp padding lines its icon up with the text)
        const std::string label = LabelFor(Id::Ctgp7Website);
        const float w = ButtonWidth(kit, label, true, 12.0f);
        const Rect b{c.x0 - 12.0f, y, c.x0 - 12.0f + w, y + kButtonH};
        kit.Button(b, ButtonStyle::Text, Sym::kOpenInNew, label, StateOf(view, {Id::Ctgp7Website}));
        AddHit({b.x0, b.Cy() - 24.0f, b.x1, b.Cy() + 24.0f}, {Id::Ctgp7Website}, shown);
        y += kButtonH + 16.0f;
    }
    kit.Line(c.x0, y, c.x1, y, 1.0f, Tok::kOutlineVariant);
    y += 8.0f;

    // ---- The status row ----
    const float cy = y + kStatusRowH * 0.5f;
    float right = c.x1;
    for (const int raw : Ctgp7Buttons(m)) {
        const Id id = static_cast<Id>(raw);
        const std::string label =
            id == Id::Ctgp7Install && m.setup_unfinished ? std::string(kCtgp7FinishSetup) : LabelFor(id);
        const bool text = id == Id::Ctgp7Cancel || id == Id::Ctgp7Uninstall;
        const char32_t icon = id == Id::Ctgp7Install ? Sym::kDownload
                              : id == Id::Ctgp7Launch ? Sym::kPlayArrow
                                                      : char32_t{0};
        const float w = ButtonWidth(kit, label, icon != 0, text ? 12.0f : 24.0f);
        // a text button's own 12 dp padding lines its label up with the pane's edge; next to
        // another button its container keeps the 8 dp gap
        const float edge = text && right == c.x1 ? right + 12.0f : right;
        const Rect b{edge - w, cy - kButtonH * 0.5f, edge, cy + kButtonH * 0.5f};
        kit.Button(b, text ? ButtonStyle::Text : ButtonStyle::Filled, icon, label, StateOf(view, {id}));
        AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {id}, shown);
        right = b.x0 - 8.0f; // M3: 8 dp between buttons
    }
    right -= kNameGap - 8.0f; // the text ends 24 dp before the first button
    const std::string headline = m.busy && !m.phase.empty() ? m.phase : std::string(kCtgp7Segment);
    kit.TextMid(c.x0, cy - 11.0f, Type::Body, Tok::kOnSurface,
                kit.Fit(Type::Body, headline, right - c.x0));
    bool error = false;
    const std::string line = Ctgp7Line(m, &error);
    kit.TextSized(c.x0, cy + 13.0f + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall, false,
                  error ? Tok::kError : Tok::kOnSurfaceVariant, kit.Fit(Type::Tooltip, line, right - c.x0));
    y += kStatusRowH;
    const float bar_y = y;
    if (m.busy) { // Material 3 linear progress: the indicator, a 4 dp gap, the track, the stop dot
        y += 4.0f;
        constexpr float kH = 4.0f;
        const float x0 = c.x0, x1 = c.x1, w = x1 - x0;
        if (m.permille >= 0) {
            const float f = std::clamp(m.permille / 1000.0f, 0.0f, 1.0f);
            const float split = x0 + w * f;
            if (f > 0.0f) {
                kit.Fill({x0, bar_y, split, bar_y + kH}, kH * 0.5f, Tok::kPrimary);
            }
            const float t0 = f > 0.0f ? split + 4.0f : x0;
            if (t0 < x1) {
                kit.Fill({t0, bar_y, x1, bar_y + kH}, kH * 0.5f, Tok::kSecondaryContainer);
                kit.Fill({x1 - kH, bar_y, x1, bar_y + kH}, kH * 0.5f, Tok::kPrimary);
            }
        } else { // indeterminate: a third of the track moving across it
            const float t = std::fmod(static_cast<float>(m.spin) / 22.0f, 1.0f);
            const float a = x0 + (w * 1.33f) * t - w * 0.33f;
            const float s0 = std::max(x0, a), s1 = std::min(x1, a + w * 0.33f);
            kit.Fill({x0, bar_y, x1, bar_y + kH}, kH * 0.5f, Tok::kSecondaryContainer);
            if (s1 > s0) {
                kit.Fill({s0, bar_y, s1, bar_y + kH}, kH * 0.5f, Tok::kPrimary);
            }
        }
    }

    // ---- AzaharFrame ctgp7b: Mods for CTGP-7, the catalog's (not in the Mods segment) ----
    if (!rows.empty()) {
        y += 8.0f;
        kit.Line(c.x0, y, c.x1, y, 1.0f, Tok::kOutlineVariant);
        y += 8.0f;
        kit.TextMid(c.x0, y + 16.0f, Type::Title, Tok::kOnSurface, kCtgp7ModsTitle);
        y += 40.0f;
        if (!m.installed) { // their Install waits for CTGP-7
            kit.TextSized(c.x0, y + 10.0f + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall, false,
                          Tok::kOnSurfaceVariant, kCtgp7ModsFirst);
            y += 24.0f;
        }
        opt.page_top = y;
        DrawOfferList(kit, model, view, area, c, shown, rows, opt);
    }
    kit.ClearClip();
    DrawScrollbar(kit, view, shown, area.x1, content, max_scroll, scroll);

    // ---- A question: the Qt side's, else the Mods list's (its mods: Replace, This May Not Work,
    // ...) or a link's (Official Website, a mod's page) ----
    if (m.dialog.kind != ModsDialogKind::None) {
        DrawModsDialog(kit, m.dialog, view);
    } else {
        DrawModsDialog(kit, ModsDialogFor(model, view), view);
    }
}

} // namespace FrameXr::Menu
