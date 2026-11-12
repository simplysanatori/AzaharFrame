// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texui: the Mods tab's Mods and Textures segments (mods_segments.h), each drawn in
// the area under the segmented button (Material 3 lists).
//
// Mods, the loaded list:
//   [Install Mods]                                                 tonal button
//   No Bloom (v)                                    Remove  ( o)    managed: switch and Remove
//   simplysanatori · 2026-11-01
//   code.ips                                        Remove          your own files: Remove only
// Mods, Install Mods:
//   <-  Install Mods
//   Ocarina ReFramed (Single Screen)                       [Install]
//   OTPR26
// Textures:
//   Custom Textures                                          ( o)
//   ---------------------------------------------------------------
//   (o) Off
//   ( ) 4K           Henriko Magnifico · 6.0.0 · ASTC/KTX · 2.1 GB
//   ---------------------------------------------------------------
//   Preload                                                  (o )
// Both: [ Restart the Game to Apply                      Restart ]   snackbar after a change
// No sections and no subheaders. A question (a conflict, deleting your own files) is a Material
// basic dialog over the whole panel.

#include "frame_xr/menu/mods_segments.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;

namespace {

constexpr float kPad = 24.0f;     ///< inside the pane, as the other tabs
constexpr float kTop = 8.0f;      ///< under the segmented button's gap
constexpr float kRow1 = 48.0f;    ///< a one-line row
constexpr float kRow2 = 64.0f;    ///< a row with a supporting line
/// A mod's row with "Created by <author>": its name in the top 32 dp, the link's 48 dp target
/// in the lower 48, so pointing at the name never opens the link.
constexpr float kRowLink = 80.0f;
constexpr float kLinkNameDy = -5.0f;  ///< the name 16 dp over the middle (RowText puts it 11 up)
constexpr float kLinkLineDy = 3.0f;   ///< "Created by" 16 dp under the middle (CreatedBy: 13 down)
constexpr float kDividerH = 17.0f; ///< a 1 dp divider and 8 dp each side
constexpr float kBarH = 48.0f;    ///< a top bar's buttons
constexpr float kButtonH = 40.0f; ///< a button in a row (its target is 48)
constexpr float kSwitchW = 52.0f;
constexpr float kLeading = 40.0f; ///< a leading radio button (24) and its gap (16)
constexpr float kTrailGap = 16.0f;
constexpr float kNameGap = 24.0f;
constexpr float kBadge = 20.0f;   ///< the verified badge after a name
constexpr float kSmall = 14.0f;   ///< Body Small, one step up as the menu's type scale
constexpr float kSnackH = 48.0f;  ///< Material's one-line snackbar
constexpr float kSnackGap = 16.0f;
constexpr float kPi = 3.14159265f;
/// Material's inverse primary (the snackbar's action), SchemeTonalSpot 2021, dark, seed #9CC9FF.
constexpr uint32_t kInversePrimary = 0x35618E;
constexpr const char* kRestartText = "Restart the Game to Apply";

/// The tab's scroll (the three segments share it).
float ScrollWanted(const MenuView& view) {
    return view.scroll[static_cast<std::size_t>(view.tab)];
}

/// A list scrolled to item edges, as the Cheats segment: nothing shows cut at the top, and at
/// the end the last item shows whole.
struct ListScroll {
    std::vector<float> tops;
    float content_h = 0.0f;
    float max_scroll = 0.0f;
    float scroll = 0.0f;
    float step = kRow1; ///< the top item's height: the stick's step (MenuLayout::scroll_step)
};

ListScroll Scroll(const std::vector<float>& heights, float list_h, float want) {
    ListScroll s;
    for (const float h : heights) {
        s.tops.push_back(s.content_h);
        s.content_h += h;
    }
    const float raw_max = std::max(0.0f, s.content_h - list_h);
    if (raw_max > 0.0f) {
        s.max_scroll = s.content_h;
        for (const float t : s.tops) {
            if (t >= raw_max) {
                s.max_scroll = t;
                break;
            }
        }
    }
    const float w = std::clamp(want, 0.0f, s.max_scroll);
    for (std::size_t i = 0; i < s.tops.size(); i++) {
        if (s.tops[i] <= w + 0.01f) {
            s.scroll = s.tops[i];
            s.step = heights[i];
        }
    }
    return s;
}

/// The scroll indicator, as the other long tabs.
void ScrollIndicator(Kit& kit, const Rect& list, float right, const ListScroll& s) {
    if (s.max_scroll <= 0.0f) {
        return;
    }
    const float track_h = list.H() - 16.0f;
    const float thumb_h = std::max(40.0f, track_h * list.H() / s.content_h);
    const float ty = list.y0 + 8.0f + (track_h - thumb_h) * (s.scroll / s.max_scroll);
    kit.Fill({right - 10.0f, ty, right - 6.0f, ty + thumb_h}, 2.0f, Tok::kOnSurfaceVariant, 0.38f);
}

/// An arc (radians, 0 = up, clockwise) as short strokes.
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

/// Material 3 indeterminate circular progress of diameter `d` (48 or 24 dp).
void Ring(Kit& kit, float cx, float cy, float d, int spin) {
    const float stroke = d >= 40.0f ? 4.0f : 3.0f, r = d * 0.5f - stroke * 0.5f;
    const float gap = (stroke + 4.0f) / r;
    const float t = static_cast<float>(spin) / 15.0f;
    const float start = std::fmod(t * 1.6f * kPi, 2.0f * kPi);
    const float len = kPi * (0.35f + 0.55f * (0.5f - 0.5f * std::cos(t * 2.0f * kPi / 1.5f)));
    Arc(kit, cx, cy, r, start, start + len, stroke, Tok::kPrimary);
    Arc(kit, cx, cy, r, start + len + gap, start + 2.0f * kPi - gap, stroke,
        Tok::kSecondaryContainer);
}

/// Material 3 radio button: a 20 dp ring, 2 dp, and a 10 dp dot when selected.
void Radio(Kit& kit, float cx, float cy, bool selected, bool enabled) {
    const uint32_t ink =
        !enabled ? Tok::kOnSurface : selected ? Tok::kPrimary : Tok::kOnSurfaceVariant;
    const float op = enabled ? 1.0f : kDisabledContent;
    kit.Stroke({cx - 10.0f, cy - 10.0f, cx + 10.0f, cy + 10.0f}, 10.0f, 2.0f, ink, op);
    if (selected) {
        kit.Dot(cx, cy, 5.0f, ink, op);
    }
}

float ButtonWidth(const Kit& kit, const std::string& label, bool icon, float side) {
    return kit.TextWidth(Type::Label, label) + (icon ? 28.0f : 0.0f) + side * 2.0f;
}

/// A row's headline (with the verified badge after it) and its supporting line; one line when
/// `detail` is empty. `right` is where the text must end.
void RowText(Kit& kit, float x, float right, float cy, const std::string& name, bool verified,
             const std::string& detail, bool detail_error, float op, bool two_line = false) {
    const float nw = right - x - (verified ? kBadge + 8.0f : 0.0f);
    const float ny = detail.empty() && !two_line ? cy : cy - 11.0f;
    const float drawn =
        kit.TextMid(x, ny, Type::Body, Tok::kOnSurface, kit.Fit(Type::Body, name, nw), 0, op);
    if (verified) {
        // Material's verified badge after the name: made for Azahar for Frame.
        kit.Icon(x + drawn + 6.0f + kBadge * 0.5f, ny, kBadge, Sym::kVerified, true, Tok::kPrimary,
                 op);
    }
    if (!detail.empty()) {
        kit.TextSized(x, cy + 13.0f + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall, false,
                      detail_error ? Tok::kError : Tok::kOnSurfaceVariant,
                      kit.Fit(Type::Tooltip, detail, right - x), 0, op);
    }
}

/// A centred empty (or waiting) state: the icon or a progress ring, a title, a smaller line.
/// A mod's supporting line: "Created by <author>" with the author as a link (the primary colour,
/// underlined while the beam is on it), then " · <rest>". Returns the link's rectangle (empty
/// when there is no link).
Rect CreatedBy(Kit& kit, float x, float right, float cy, const std::string& author, bool link,
               State link_state, const std::string& rest, bool rest_error, float op) {
    const float base = cy + 13.0f + kit.CapHeightSized(kSmall, false) * 0.5f;
    float at = x;
    Rect r{};
    if (!author.empty()) {
        const std::string by = "Created by ";
        at += kit.TextSized(at, base, kSmall, false, Tok::kOnSurfaceVariant, by, 0, op);
        const std::string name = kit.Fit(Type::Tooltip, author, std::max(24.0f, right - at));
        const float w = kit.TextSized(at, base, kSmall, false,
                                      link ? Tok::kPrimary : Tok::kOnSurfaceVariant, name, 0, op);
        if (link) {
            if (link_state == State::Hover || link_state == State::Pressed) {
                kit.Fill({at, base + 2.0f, at + w, base + 3.0f}, 0.0f, Tok::kPrimary, op);
            }
            r = {at, base - 14.0f, at + w, base + 4.0f};
        }
        at += w;
    }
    if (!rest.empty() && right - at > 24.0f) {
        const std::string text = (at > x ? " \u00b7 " : "") + rest;
        kit.TextSized(at, base, kSmall, false, rest_error ? Tok::kError : Tok::kOnSurfaceVariant,
                      kit.Fit(Type::Tooltip, text, right - at), 0, op);
    }
    return r;
}

/// The link's target: the text's width and more (at least 48) and the row's lower 48 dp, under
/// the name line (rows of kRowLink).
Rect LinkTarget(const Rect& link, const Rect& row) {
    const float pad = std::max(8.0f, (48.0f - link.W()) * 0.5f);
    return {link.x0 - pad, row.y1 - 48.0f, link.x1 + pad, row.y1};
}

void EmptyState(Kit& kit, const Rect& area, char32_t icon, bool ring, int spin,
                const std::string& title, const std::string& line) {
    const float icon_h = 48.0f, title_h = 24.0f, small_h = 20.0f;
    const float h = icon_h + 16.0f + title_h + (line.empty() ? 0.0f : 8.0f + small_h);
    float y = area.Cy() - h * 0.5f;
    if (ring) {
        Ring(kit, area.Cx(), y + icon_h * 0.5f, 48.0f, spin);
    } else {
        kit.Icon(area.Cx(), y + icon_h * 0.5f, icon_h, icon, false, Tok::kOnSurfaceVariant);
    }
    y += icon_h + 16.0f;
    kit.TextMid(area.Cx(), y + title_h * 0.5f, Type::Title, Tok::kOnSurface, title, 1);
    y += title_h;
    if (!line.empty()) {
        y += 8.0f;
        kit.TextSized(area.Cx(), y + 15.0f, kSmall, false, Tok::kOnSurfaceVariant,
                      kit.Fit(Type::Tooltip, line, area.W()), 1);
    }
}

/// Words of `s` in lines at most `width` wide (at most `max_lines`, the last one cut to fit).
std::vector<std::string> Wrap(const Kit& kit, const std::string& s, float size, bool medium,
                              float width, int max_lines) {
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
        if (!line.empty() && kit.TextWidthSized(size, medium, next) > width) {
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
        for (std::size_t i = static_cast<std::size_t>(max_lines); i < lines.size(); i++) {
            last += " " + lines[i];
        }
        lines.resize(static_cast<std::size_t>(max_lines));
        // Fit cuts with an ellipsis; Tooltip is the 14 dp style, so cut by the ratio of sizes.
        while (last.size() > 1 && kit.TextWidthSized(size, medium, last + "…") > width) {
            last.pop_back();
        }
        lines.back() = last + "…";
    }
    return lines;
}

/// The dialog to show: the model's (the Qt side's question), else "Open GitHub?" for a link.
ModsDialog DialogFor(const ModsListModel& m, const MenuView& view) {
    if (m.dialog.kind != ModsDialogKind::None || view.open_link.empty()) {
        return m.dialog;
    }
    return {ModsDialogKind::OpenLink, "", GithubLinkText(view.open_link)};
}

} // namespace

std::string GithubLinkText(const std::string& url) {
    static constexpr std::string_view kPrefix = "https://github.com/";
    if (url.rfind(kPrefix, 0) != 0) {
        return {};
    }
    std::string_view path = std::string_view(url).substr(kPrefix.size());
    if (!path.empty() && path.back() == '/') {
        path.remove_suffix(1);
    }
    const std::size_t slash = path.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 >= path.size() ||
        path.find('/', slash + 1) != std::string_view::npos) {
        return {}; // owner/repo, nothing more
    }
    for (const char ch : path) {
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '/';
        if (!ok) {
            return {};
        }
    }
    if (path.find("..") != std::string_view::npos) {
        return {};
    }
    return "github.com/" + std::string(path);
}

std::string JoinDetail(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& p : parts) {
        if (p.empty()) {
            continue;
        }
        if (!out.empty()) {
            out += " · ";
        }
        out += p;
    }
    return out;
}

std::string SizeText(uint64_t bytes) {
    static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes);
    int unit = 0;
    while (v >= 1000.0 && unit < 4) {
        v /= 1000.0;
        unit++;
    }
    char buf[32];
    if (unit == 0) {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    } else if (v < 10.0) {
        std::snprintf(buf, sizeof(buf), "%.1f %s", v, kUnits[unit]);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f %s", v, kUnits[unit]);
    }
    return buf;
}

// ---------------------------------------------------------------------------------------------
// Shared: the snackbar and the dialog

float TabMenu::DrawRestartSnackbar(Kit& kit, const MenuView& view, const Rect& area) {
    const Rect c{area.x0 + kPad, area.y0, area.x1 - kPad, area.y1};
    const Rect bar{c.x0, area.y1 - kSnackGap - kSnackH, c.x1, area.y1 - kSnackGap};
    kit.Fill(bar, 4.0f, Tok::kInverseSurface); // Material: extra-small corners
    const std::string action = LabelFor(Id::RestartGame);
    const float aw = kit.TextWidth(Type::Label, action) + 24.0f;
    const Rect a{bar.x1 - 8.0f - aw, bar.Cy() - 20.0f, bar.x1 - 8.0f, bar.Cy() + 20.0f};
    kit.StateLayer(a, 20.0f, kInversePrimary, StateOf(view, {Id::RestartGame}));
    kit.TextMid(a.Cx(), bar.Cy(), Type::Label, kInversePrimary, action, 1);
    AddHit({a.x0, bar.y0, a.x1, bar.y1}, {Id::RestartGame}, area);
    kit.TextSized(bar.x0 + 16.0f, bar.Cy() + kit.CapHeightSized(16.0f, false) * 0.5f, 16.0f,
                  false, Tok::kInverseOnSurface, kRestartText);
    return bar.y0 - 8.0f;
}

void TabMenu::DrawModsDialog(Kit& kit, const ModsDialog& d, const MenuView& view) {
    if (d.kind == ModsDialogKind::None) {
        return;
    }
    // Material 3 basic dialog over a scrim on the whole panel; nothing under it is hit.
    const Rect panel{0.0f, 0.0f, kPanelW, kPanelH};
    kit.Fill(panel, kPanelRadius, 0x000000, 0.32f);
    AddHit(panel, {Id::None, kMenuAreaIndex}, panel);
    layout.max_scroll = 0.0f; // the list under it stays put
    std::string headline, body, confirm;
    switch (d.kind) {
    case ModsDialogKind::Replace:
        headline = "Install " + d.mod + "?";
        body = "Replaces " + d.other;
        confirm = "Replace";
        break;
    case ModsDialogKind::Remove:
        headline = "Remove " + d.mod + "?";
        body = d.other; // the files, which only you have
        confirm = "Remove";
        break;
    case ModsDialogKind::Blocked:
        headline = "Can't Install " + d.mod;
        body = "Remove " + d.other + " First";
        break;
    case ModsDialogKind::OpenLink:
        headline = "Open GitHub?";
        body = "This opens " + d.other + " in Steam's browser.";
        confirm = "Open";
        break;
    case ModsDialogKind::None:
        break;
    }
    constexpr float kW = 440.0f, kP = 24.0f, kHead = 26.0f, kBody = 16.0f;
    const float tw = kW - kP * 2.0f;
    const auto head_lines = Wrap(kit, headline, kHead, false, tw, 3);
    const auto body_lines = Wrap(kit, body, kBody, false, tw, 3);
    const float head_lh = 32.0f, body_lh = 24.0f;
    const float h = kP + head_lh * static_cast<float>(head_lines.size()) + 16.0f +
                    body_lh * static_cast<float>(body_lines.size()) + 24.0f + kButtonH + kP;
    const Rect box{(kPanelW - kW) * 0.5f, (kPanelH - h) * 0.5f, (kPanelW + kW) * 0.5f,
                   (kPanelH + h) * 0.5f};
    kit.Fill(box, 28.0f, Tok::kSurfaceContainerHigh);
    float y = box.y0 + kP;
    for (const std::string& l : head_lines) {
        kit.TextSized(box.x0 + kP, y + head_lh * 0.5f + kit.CapHeightSized(kHead, false) * 0.5f,
                      kHead, false, Tok::kOnSurface, l);
        y += head_lh;
    }
    y += 16.0f;
    for (const std::string& l : body_lines) {
        kit.TextSized(box.x0 + kP, y + body_lh * 0.5f + kit.CapHeightSized(kBody, false) * 0.5f,
                      kBody, false, Tok::kOnSurfaceVariant, l);
        y += body_lh;
    }
    // The actions: text buttons at the end, the confirming one last.
    float right = box.x1 - kP + 12.0f; // a text button's own padding lines its label up
    const float by = box.y1 - kP - kButtonH;
    const auto action = [&](Id id, const std::string& label) {
        const float w = ButtonWidth(kit, label, false, 12.0f);
        const Rect b{right - w, by, right, by + kButtonH};
        kit.Button(b, ButtonStyle::Text, 0, label, StateOf(view, {id}));
        AddHit({b.x0, b.Cy() - 24.0f, b.x1, b.Cy() + 24.0f}, {id}, panel);
        right = b.x0 - 8.0f;
    };
    if (d.kind == ModsDialogKind::Blocked) {
        action(Id::DialogCancel, "OK");
    } else {
        action(Id::DialogConfirm, confirm);
        action(Id::DialogCancel, "Cancel");
    }
}

// ---------------------------------------------------------------------------------------------
// Mods

void TabMenu::DrawModsList(Kit& kit, const MenuModel& model, const MenuView& view,
                           const Rect& area) {
    const ModsListModel& m = model.mods.mods;
    const Rect c{area.x0 + kPad, area.y0 + kTop, area.x1 - kPad, area.y1 - kPad};
    layout.max_scroll = 0.0f;
    float bottom = area.y1 - 8.0f;
    if (m.needs_restart) {
        bottom = DrawRestartSnackbar(kit, view, area);
    }

    if (view.mods_page == ModsPage::Install) {
        // ---- Install Mods: the back button and the title, then the title's mods ----
        const Rect back{c.x0 - 12.0f, c.y0, c.x0 + 36.0f, c.y0 + kBarH};
        kit.StateLayer(back, 24.0f, Tok::kOnSurfaceVariant, StateOf(view, {Id::ModsBack}));
        kit.Icon(back.Cx(), back.Cy(), 24.0f, Sym::kArrowBack, false, Tok::kOnSurface);
        AddHit(back, {Id::ModsBack}, area);
        kit.TextMid(back.x1 + 12.0f, back.Cy(), Type::Title, Tok::kOnSurface,
                    LabelFor(Id::InstallMods));
        const float top = c.y0 + kBarH + 8.0f;
        const Rect list{area.x0, top, area.x1, bottom};
        if (m.catalog != CatalogState::Ready) {
            EmptyState(kit, {c.x0, top, c.x1, bottom}, 0, true, m.spin, "Loading Mods", "");
        } else if (m.available.empty()) {
            EmptyState(kit, {c.x0, top, c.x1, bottom}, Sym::kExtension, false, 0,
                       "No Mods for This Game", "");
        } else {
            std::vector<float> heights(m.available.size(), kRowLink);
            const ListScroll s = Scroll(heights, list.H(), ScrollWanted(view));
            layout.max_scroll = s.max_scroll;
            layout.scroll_step = s.step;
            kit.Clip(list);
            for (std::size_t i = 0; i < m.available.size(); i++) {
                const float y = top + s.tops[i] - s.scroll;
                if (y + kRowLink <= list.y0 || y + kRowLink > list.y1 + 0.01f) {
                    continue; // whole rows only
                }
                const ModOffer& o = m.available[i];
                const int idx = static_cast<int>(i);
                const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + kRowLink};
                const float cy = row.Cy();
                AddHit(row, {Id::ModInstall, kLabelIndex}, list);
                float right = c.x1;
                switch (o.state) {
                case OfferState::Install:
                case OfferState::Unavailable: {
                    const std::string label = LabelFor(Id::ModInstall);
                    const float w = ButtonWidth(kit, label, false, 24.0f);
                    const Rect b{c.x1 - w, cy - kButtonH * 0.5f, c.x1, cy + kButtonH * 0.5f};
                    const bool on = o.state == OfferState::Install && !m.busy;
                    kit.Button(b, ButtonStyle::Tonal, 0, label,
                               StateOf(view, {Id::ModInstall, idx}, on));
                    if (on) {
                        AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {Id::ModInstall, idx}, list);
                    }
                    right = b.x0;
                    break;
                }
                case OfferState::Installing: {
                    // Where the Install button was, at its centre.
                    const float w = ButtonWidth(kit, LabelFor(Id::ModInstall), false, 24.0f);
                    Ring(kit, c.x1 - w * 0.5f, cy, 24.0f, m.spin);
                    right = c.x1 - w;
                    break;
                }
                case OfferState::Installed: {
                    const std::string label = "Installed";
                    const float w = ButtonWidth(kit, label, true, 12.0f);
                    const Rect b{c.x1 - w + 12.0f, cy - kButtonH * 0.5f, c.x1 + 12.0f,
                                 cy + kButtonH * 0.5f};
                    kit.Button(b, ButtonStyle::Text, Sym::kCheck, label, State::Disabled);
                    right = b.x0 + 12.0f;
                    break;
                }
                }
                RowText(kit, c.x0, right - kNameGap, cy + kLinkNameDy, o.name, o.verified, "", false,
                        1.0f, true);
                const bool link = !GithubLinkText(o.url).empty();
                const Rect l = CreatedBy(kit, c.x0, right - kNameGap, cy + kLinkLineDy, o.author, link,
                                         StateOf(view, {Id::ModLink, idx}), o.note, o.error, 1.0f);
                if (link && l.W() > 0.0f) {
                    AddHit(LinkTarget(l, row), {Id::ModLink, idx}, list);
                }
            }
            kit.ClearClip();
            ScrollIndicator(kit, list, area.x1, s);
        }
        DrawModsDialog(kit, DialogFor(m, view), view);
        return;
    }

    // ---- What is loaded for the game ----
    const std::string install = LabelFor(Id::InstallMods);
    if (m.loaded.empty()) {
        const Rect a{c.x0, c.y0, c.x1, bottom};
        const float h = 48.0f + 16.0f + 24.0f + 24.0f + kBarH;
        float y = a.Cy() - h * 0.5f;
        kit.Icon(a.Cx(), y + 24.0f, 48.0f, Sym::kExtension, false, Tok::kOnSurfaceVariant);
        y += 48.0f + 16.0f;
        kit.TextMid(a.Cx(), y + 12.0f, Type::Title, Tok::kOnSurface, "No Mods", 1);
        y += 24.0f + 24.0f;
        const float w = ButtonWidth(kit, install, true, 24.0f);
        const Rect b{a.Cx() - w * 0.5f, y, a.Cx() + w * 0.5f, y + kBarH};
        kit.Button(b, ButtonStyle::Filled, Sym::kDownload, install,
                   StateOf(view, {Id::InstallMods}));
        AddHit(b, {Id::InstallMods}, area);
        DrawModsDialog(kit, DialogFor(m, view), view);
        return;
    }
    {
        const Rect b{c.x0, c.y0, c.x0 + ButtonWidth(kit, install, true, 24.0f), c.y0 + kBarH};
        kit.Button(b, ButtonStyle::Tonal, Sym::kDownload, install,
                   StateOf(view, {Id::InstallMods}));
        AddHit(b, {Id::InstallMods}, area);
    }
    const float top = c.y0 + kBarH + 8.0f;
    const Rect list{area.x0, top, area.x1, bottom};
    std::vector<float> heights;
    for (const ModRow& r : m.loaded) {
        heights.push_back(!r.author.empty() ? kRowLink : r.detail.empty() ? kRow1 : kRow2);
    }
    const ListScroll s = Scroll(heights, list.H(), ScrollWanted(view));
    layout.max_scroll = s.max_scroll;
    layout.scroll_step = s.step;
    const std::string remove = LabelFor(Id::ModRemove);
    const float remove_w = ButtonWidth(kit, remove, false, 12.0f);
    kit.Clip(list);
    for (std::size_t i = 0; i < m.loaded.size(); i++) {
        const float ih = heights[i];
        const float y = top + s.tops[i] - s.scroll;
        if (y + ih <= list.y0 || y + ih > list.y1 + 0.01f) {
            continue;
        }
        const ModRow& r = m.loaded[i];
        const int idx = static_cast<int>(i);
        const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + ih};
        const float cy = row.Cy();
        const bool enabled = !m.busy;
        // The row turns a managed mod on or off, as its switch; your own files have no switch.
        if (r.managed) {
            const State st = StateOf(view, {Id::Mod, idx}, enabled);
            kit.StateLayer(row, 16.0f, Tok::kOnSurface, st);
            kit.Switch({c.x1 - kSwitchW, row.y0, c.x1, row.y1}, r.active, st);
            AddHit(row, enabled ? Target{Id::Mod, idx} : Target{Id::Mod, kLabelIndex}, list);
        } else {
            AddHit(row, {Id::Mod, kLabelIndex}, list);
        }
        // Remove, in a column left of the switches.
        const float rx1 = c.x1 - kSwitchW - kTrailGap + 12.0f;
        const Rect b{rx1 - remove_w, cy - kButtonH * 0.5f, rx1, cy + kButtonH * 0.5f};
        kit.Button(b, ButtonStyle::Text, 0, remove, StateOf(view, {Id::ModRemove, idx}, enabled));
        if (enabled) {
            AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {Id::ModRemove, idx}, list);
        }
        const float op = enabled ? 1.0f : kDisabledContent;
        const float text_right = b.x0 + 12.0f - kNameGap;
        if (r.author.empty()) {
            RowText(kit, c.x0, text_right, cy, r.name, r.verified, r.detail, false, op);
        } else {
            RowText(kit, c.x0, text_right, cy + kLinkNameDy, r.name, r.verified, "", false, op, true);
            const bool link = !GithubLinkText(r.url).empty();
            const Rect l = CreatedBy(kit, c.x0, text_right, cy + kLinkLineDy, r.author, link,
                                     StateOf(view, {Id::ModLink, idx}, enabled), r.detail, false, op);
            if (link && enabled && l.W() > 0.0f) {
                AddHit(LinkTarget(l, row), {Id::ModLink, idx}, list);
            }
        }
    }
    kit.ClearClip();
    ScrollIndicator(kit, list, area.x1, s);
    DrawModsDialog(kit, DialogFor(m, view), view);
}

// ---------------------------------------------------------------------------------------------
// Textures

void TabMenu::DrawTextures(Kit& kit, const MenuModel& model, const MenuView& view,
                           const Rect& area) {
    const TexturesModel& m = model.mods.textures;
    const Rect c{area.x0 + kPad, area.y0 + kTop, area.x1 - kPad, area.y1 - kPad};
    layout.max_scroll = 0.0f;
    float bottom = area.y1 - 8.0f;
    if (m.needs_restart) {
        bottom = DrawRestartSnackbar(kit, view, area);
    }
    if (m.packs.empty()) {
        EmptyState(kit, {c.x0, c.y0, c.x1, bottom}, Sym::kTexture, false, 0, "No Texture Packs",
                   "Add packs in Azahar's Window");
        return;
    }

    struct Item {
        enum class Kind { Switch, Divider, Pack } kind;
        Id id = Id::None;
        int index = -1; ///< Pack: 0 Off, i + 1 packs[i]
        float h = kRow1;
    };
    std::vector<Item> items{{Item::Kind::Switch, Id::CustomTextures},
                            {Item::Kind::Divider, Id::None, -1, kDividerH}};
    for (int i = 0; i <= static_cast<int>(m.packs.size()); i++) {
        const bool two = i > 0 && !m.packs[static_cast<std::size_t>(i - 1)].detail.empty();
        items.push_back({Item::Kind::Pack, Id::TexturePack, i, two ? kRow2 : kRow1});
    }
    items.push_back({Item::Kind::Divider, Id::None, -1, kDividerH});
    items.push_back({Item::Kind::Switch, Id::TexturePreload});
    int active = 0; // Off
    for (std::size_t i = 0; i < m.packs.size(); i++) {
        if (m.packs[i].active) {
            active = static_cast<int>(i) + 1;
            break;
        }
    }

    const float top = c.y0;
    const Rect list{area.x0, top, area.x1, bottom};
    std::vector<float> heights;
    for (const Item& it : items) {
        heights.push_back(it.h);
    }
    const ListScroll s = Scroll(heights, list.H(), ScrollWanted(view));
    layout.max_scroll = s.max_scroll;
    layout.scroll_step = s.step;
    const bool on = m.custom_textures;
    kit.Clip(list);
    for (std::size_t n = 0; n < items.size(); n++) {
        const Item& it = items[n];
        const float y = top + s.tops[n] - s.scroll;
        if (y + it.h <= list.y0 || y + it.h > list.y1 + 0.01f) {
            continue;
        }
        if (it.kind == Item::Kind::Divider) {
            kit.Fill({c.x0, y + 8.0f, c.x1, y + 9.0f}, 0.0f, Tok::kOutlineVariant);
            continue;
        }
        const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + it.h};
        const float cy = row.Cy();
        const Target t{it.id, it.kind == Item::Kind::Pack ? it.index : -1};
        const bool enabled = !m.busy && (it.id == Id::CustomTextures || on);
        const State st = StateOf(view, t, enabled);
        const float op = enabled ? 1.0f : kDisabledContent;
        kit.StateLayer(row, 16.0f, Tok::kOnSurface, st);
        AddHit(row, enabled ? t : Target{it.id, kLabelIndex}, list);
        if (it.kind == Item::Kind::Switch) {
            const bool value = it.id == Id::CustomTextures ? m.custom_textures : m.preload;
            kit.Switch({c.x1 - kSwitchW, row.y0, c.x1, row.y1}, value, st);
            kit.TextMid(c.x0, cy, Type::Body, Tok::kOnSurface,
                        kit.Fit(Type::Body, LabelFor(it.id), c.W() - kSwitchW - kNameGap), 0, op);
            continue;
        }
        Radio(kit, c.x0 + 12.0f, cy, it.index == active, enabled);
        const float tx = c.x0 + kLeading;
        if (it.index == 0) {
            kit.TextMid(tx, cy, Type::Body, Tok::kOnSurface, "Off", 0, op);
        } else {
            const TexturePackItem& p = m.packs[static_cast<std::size_t>(it.index - 1)];
            RowText(kit, tx, c.x1, cy, p.name, false, p.detail, false, op);
        }
    }
    kit.ClearClip();
    ScrollIndicator(kit, list, area.x1, s);
}

} // namespace FrameXr::Menu
