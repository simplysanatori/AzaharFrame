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
//   <-  Install Mods                                 [Install from Zip]
//   Ocarina ReFramed (Single Screen)                       [Install]
//   OTPR26
// Textures:
//   [Download Packs]                                               tonal button
//   Custom Textures                                          ( o)
//   ---------------------------------------------------------------
//   (o) Off
//   ( ) 4K           Henriko Magnifico · 6.0.0 · ASTC/KTX · 2.1 GB
//   ---------------------------------------------------------------
//   Preload                                                  (o )
// Textures, Download Packs (AzaharFrame ui2: the same page as Install Mods):
//   <-  Download Packs                               [Install from Zip]
//   4K                                                    [Download]
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
/// AzaharFrame compat: a catalog row (Install Mods, Download Packs): the name and "Created by
/// <author> · v172 · 10K+ Downloads · Updated Mar 2026" (its link's 48 dp target round that
/// line). AzaharFrame ui3: one line when it fits (kRowCatalog1, the name and that line at
/// kCatalog1NameY and kCatalog1ByY), else it goes on at kCatalogWrapY (kRowCatalog).
constexpr float kRowCatalog = 96.0f;
constexpr float kCatalogNameY = 22.0f, kCatalogByY = 50.0f, kCatalogWrapY = 76.0f;
constexpr float kRowCatalog1 = 80.0f;
constexpr float kCatalog1NameY = 26.0f, kCatalog1ByY = 54.0f;
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

/// Material 3 determinate circular progress: `frac` of the ring, the track after a gap.
void RingValue(Kit& kit, float cx, float cy, float d, float frac) {
    const float stroke = d >= 40.0f ? 4.0f : 3.0f, r = d * 0.5f - stroke * 0.5f;
    const float gap = (stroke + 4.0f) / r;
    const float len = 2.0f * kPi * std::clamp(frac, 0.0f, 1.0f);
    Arc(kit, cx, cy, r, 0.0f, len, stroke, Tok::kPrimary);
    Arc(kit, cx, cy, r, len + (len > 0.0f ? gap : 0.0f), 2.0f * kPi - (len > 0.0f ? gap : 0.0f),
        stroke, Tok::kSecondaryContainer);
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
               State link_state, const std::string& rest, bool rest_error, float op,
               bool created_by = true) {
    const float base = cy + 13.0f + kit.CapHeightSized(kSmall, false) * 0.5f;
    float at = x;
    Rect r{};
    if (!author.empty()) {
        if (created_by) { // AzaharFrame zipinstall: Download Packs shows the author alone
            at += kit.TextSized(at, base, kSmall, false, Tok::kOnSurfaceVariant, "Created by ", 0, op);
        }
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

/// AzaharFrame ui3: a byline, "Created by <author> · a · b · c", measured: the author cut to the
/// width and how many parts (the ones not empty) stay on the first line (BylineSplit).
struct BylineFit {
    bool created_by = true;
    std::string author;          ///< cut to fit ("" none)
    std::vector<std::string> parts;
    std::vector<std::size_t> index; ///< each part's index in the parts given (the error one)
    std::size_t first = 0;       ///< parts on the first line
    bool Wraps() const {
        return first < parts.size();
    }
};

BylineFit FitByline(const Kit& kit, float width, const std::string& author, bool created_by,
                    const std::vector<std::string>& parts) {
    BylineFit f;
    f.created_by = created_by;
    float lead_w = 0.0f;
    if (!author.empty()) {
        const float cb = created_by ? kit.TextWidthSized(kSmall, false, "Created by ") : 0.0f;
        f.author = kit.Fit(Type::Tooltip, author, std::max(24.0f, width - cb));
        lead_w = cb + kit.TextWidthSized(kSmall, false, f.author);
    }
    std::vector<float> widths;
    for (std::size_t i = 0; i < parts.size(); i++) {
        if (!parts[i].empty()) {
            f.parts.push_back(parts[i]);
            f.index.push_back(i);
            widths.push_back(kit.TextWidthSized(kSmall, false, parts[i]));
        }
    }
    f.first = BylineSplit(lead_w, widths, kit.TextWidthSized(kSmall, false, " \u00b7 "), width);
    return f;
}

/// Draws a BylineFit: the first line centred on `y1`, the rest (if any) on `y2`, the author a link
/// (the primary colour, underlined while the beam is on it) and the part given as `error_part`
/// in the error colour. Returns the link's rectangle (empty when there is no link).
Rect DrawByline(Kit& kit, float x, float right, float y1, float y2, const BylineFit& f, bool link,
                State link_state, std::size_t error_part, float op) {
    const float cap = kit.CapHeightSized(kSmall, false) * 0.5f;
    float base = y1 + cap;
    float at = x;
    Rect r{};
    if (!f.author.empty()) {
        if (f.created_by) {
            at += kit.TextSized(at, base, kSmall, false, Tok::kOnSurfaceVariant, "Created by ", 0, op);
        }
        const float w = kit.TextSized(at, base, kSmall, false,
                                      link ? Tok::kPrimary : Tok::kOnSurfaceVariant, f.author, 0, op);
        if (link) {
            if (link_state == State::Hover || link_state == State::Pressed) {
                kit.Fill({at, base + 2.0f, at + w, base + 3.0f}, 0.0f, Tok::kPrimary, op);
            }
            r = {at, base - 14.0f, at + w, base + 4.0f};
        }
        at += w;
    }
    for (std::size_t i = 0; i < f.parts.size(); i++) {
        if (i == f.first) { // the second line
            base = y2 + cap;
            at = x;
        }
        const std::string text = (at > x ? " \u00b7 " : "") + f.parts[i];
        if (right - at < 24.0f) {
            continue; // no room left on this line
        }
        at += kit.TextSized(at, base, kSmall, false,
                            f.index[i] == error_part ? Tok::kError : Tok::kOnSurfaceVariant,
                            kit.Fit(Type::Tooltip, text, right - at), 0, op);
    }
    return r;
}

/// AzaharFrame ui3: a catalog row's height: one byline line or two.
float CatalogRowH(const Kit& kit, float width, const std::string& author,
                  const std::vector<std::string>& parts) {
    return FitByline(kit, width, author, true, parts).Wraps() ? kRowCatalog : kRowCatalog1;
}

/// AzaharFrame ui3: the width of the "Not for Your Version" and region chips (CompatChips).
float CompatChipsWidth(const Kit& kit, const std::string& region) {
    float w = kit.TextWidth(Type::Label, kNotForYourVersion) + 24.0f;
    if (!region.empty()) {
        w += kit.TextWidth(Type::Label, region) + 24.0f + 8.0f;
    }
    return w;
}

/// AzaharFrame modcat2: where a row's notes tooltip goes. AzaharFrame compat: wholly above the row
/// (under it when there is no room above in the list), never over its own band, and no wider
/// than the names' column (`name_x1`: where the leftmost chip or button of the rows shown
/// starts), so it covers no chip. Drawn after the list, wrapped.
struct NoteTip {
    float cx = 0.0f, y = 0.0f;
    bool above = true;
    float max_w = 0.0f;
    std::string text;
};
NoteTip MakeNoteTip(const Kit& kit, const std::string& text, const Rect& row, const Rect& list,
                    const Rect& area, float name_x0, float name_x1) {
    NoteTip t;
    t.text = text;
    const float left = std::max(area.x0 + 4.0f, name_x0 - 12.0f);
    t.max_w = std::max(200.0f, std::min(name_x1 - left, 560.0f));
    const auto lines = kit.TooltipLines(text, t.max_w - 20.0f);
    float widest = 0.0f;
    for (const auto& l : lines) {
        widest = std::max(widest, kit.TextWidth(Type::Tooltip, l));
    }
    const float half = (widest + 20.0f) * 0.5f;
    t.cx = left + half; // from the names' left edge
    const float h = 20.0f * static_cast<float>(lines.size()) + 10.0f;
    // TooltipWrapped puts its box 4 dp from `y`: 2 dp clear of the row either way.
    t.above = row.y0 - h - 2.0f >= list.y0;
    t.y = t.above ? row.y0 + 2.0f : row.y1 - 2.0f;
    return t;
}

/// AzaharFrame compat: a chip's wrapped tooltip over (or under) the chip, inside the area.
NoteTip MakeChipTip(const Kit& kit, const std::string& text, const Rect& chip, const Rect& list,
                    const Rect& area) {
    NoteTip t;
    t.text = text;
    t.max_w = std::min(area.W() - 48.0f, 600.0f);
    const auto lines = kit.TooltipLines(text, t.max_w - 20.0f);
    float widest = 0.0f;
    for (const auto& l : lines) {
        widest = std::max(widest, kit.TextWidth(Type::Tooltip, l));
    }
    const float half = (widest + 20.0f) * 0.5f;
    t.cx = std::clamp(chip.Cx(), area.x0 + half + 4.0f, area.x1 - half - 4.0f);
    const float h = 20.0f * static_cast<float>(lines.size()) + 10.0f;
    t.above = chip.y0 - h - 8.0f >= list.y0;
    t.y = t.above ? chip.y0 - 4.0f : chip.y1 + 4.0f;
    return t;
}

/// AzaharFrame compat: "Not for Your Version" (filled, the error container) and the region chip
/// (outlined in the error colour, as the Cheats rows' version chip), right-aligned at `right`;
/// returns where they start (`right` when there is none) and sets `chips` to their extent.
float CompatChips(Kit& kit, float right, float cy, const std::string& region, Rect* chips) {
    constexpr float kChipH = 32.0f, kChipPad = 12.0f, kGap = 8.0f;
    float x = right;
    if (!region.empty()) {
        const float w = kit.TextWidth(Type::Label, region) + kChipPad * 2.0f;
        const Rect r{x - w, cy - kChipH * 0.5f, x, cy + kChipH * 0.5f};
        kit.Stroke(r, 8.0f, 1.0f, Tok::kError);
        kit.TextMid(r.Cx(), cy, Type::Label, Tok::kError, region, 1);
        x = r.x0 - kGap;
    }
    const float w = kit.TextWidth(Type::Label, kNotForYourVersion) + kChipPad * 2.0f;
    const Rect r{x - w, cy - kChipH * 0.5f, x, cy + kChipH * 0.5f};
    kit.Fill(r, 8.0f, Tok::kErrorContainer);
    kit.TextMid(r.Cx(), cy, Type::Label, Tok::kOnErrorContainer, kNotForYourVersion, 1);
    if (chips) {
        *chips = {r.x0, cy - kChipH * 0.5f, right, cy + kChipH * 0.5f};
    }
    return r.x0;
}

/// The row's tooltip: why it may not fit, then its notes.
std::string RowTip(bool mismatch, const std::string& reason, const std::string& notes) {
    if (!mismatch || reason.empty()) {
        return notes;
    }
    return notes.empty() ? reason : reason + " " + notes;
}

/// The link's target: the text's width and more (at least 48) and the row's lower 48 dp, under
/// the name line (rows of kRowLink).
Rect LinkTarget(const Rect& link, const Rect& row) {
    const float pad = std::max(8.0f, (48.0f - link.W()) * 0.5f);
    return {link.x0 - pad, row.y1 - 48.0f, link.x1 + pad, row.y1};
}

/// AzaharFrame compat: a catalog row's name and byline; returns the link's 48 dp target (empty: no
/// link). AzaharFrame ui3: "Created by <author> · a · b" on one line, going on to a second only
/// when it runs out of width (at a " · ", the author kept on the first); `parts` "" ones left
/// out, the one at `error_part` in the error colour (a failure; npos none).
Rect CatalogRowText(Kit& kit, float x, float right, const Rect& row, const std::string& name,
                    bool verified, const std::string& author, bool link, State link_state,
                    const std::vector<std::string>& parts, std::size_t error_part) {
    const BylineFit f = FitByline(kit, right - x, author, true, parts);
    const bool wraps = f.Wraps();
    const float name_y = row.y0 + (wraps ? kCatalogNameY : kCatalog1NameY);
    const float by_y = row.y0 + (wraps ? kCatalogByY : kCatalog1ByY);
    RowText(kit, x, right, name_y, name, verified, "", false, 1.0f);
    const Rect l = DrawByline(kit, x, right, by_y, row.y0 + kCatalogWrapY, f, link, link_state,
                              error_part, 1.0f);
    Rect target{};
    if (link && l.W() > 0.0f) {
        const float pad = std::max(8.0f, (48.0f - l.W()) * 0.5f);
        target = {l.x0 - pad, by_y - 24.0f, l.x1 + pad, by_y + 24.0f};
    }
    return target;
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

/// AzaharFrame compat: "This May Not Work" for the row the view asks about ("" kind None when
/// the row is gone or fits after all).
/// The dialog to show: the model's (the Qt side's question), else "This May Not Work" (AzaharFrame
/// compat), else "Open GitHub?" for a link.
ModsDialog DialogFor(const ModsDialog& dialog, const MenuView& view, const MenuModel& model) {
    if (dialog.kind != ModsDialogKind::None) {
        return dialog;
    }
    if (const ModsDialog c = CompatConfirmDialog(model, view); c.kind != ModsDialogKind::None) {
        return c;
    }
    if (view.open_link.empty()) {
        return dialog;
    }
    return {ModsDialogKind::OpenLink, LinkHost(view.open_link), LinkText(view.open_link)};
}

} // namespace

// AzaharFrame compat
namespace {
template <typename T>
const T* At(const std::vector<T>& list, int index) {
    return index >= 0 && index < static_cast<int>(list.size()) ? &list[static_cast<std::size_t>(index)]
                                                               : nullptr;
}

} // namespace

namespace {
/// The version reason, the scan, or both in one dialog (the version reason first).
template <typename Offer>
ModsDialog OfferQuestion(const Offer& o, const char* anyway) {
    if (o.mismatch && o.unscanned) {
        return {ModsDialogKind::Mismatch, o.mismatch_body + " " + kUnscannedAlso, anyway};
    }
    if (o.mismatch) {
        return {ModsDialogKind::Mismatch, o.mismatch_body, anyway};
    }
    if (o.unscanned) {
        return {ModsDialogKind::Unscanned, kUnscannedBody, "Download Anyway"};
    }
    return {};
}
template <typename Offer>
int OfferFlags(const Offer& o) {
    return (o.mismatch ? kAnywayOtherVersion : 0) | (o.unscanned ? kAnywayUnscanned : 0);
}
} // namespace

int CompatConfirmFlags(const MenuModel& model, const MenuView& view) {
    const MenuView::Confirm& c = view.confirm;
    if (c.id == Id::ModInstall) {
        const ModOffer* o = At(model.mods.mods.available, c.index);
        return o && o->id == c.key ? OfferFlags(*o) : 0;
    }
    if (c.id == Id::PackDownload) {
        const PackOffer* o = At(model.mods.textures.downloads, c.index);
        return o && o->id == c.key ? OfferFlags(*o) : 0;
    }
    return 0;
}

ModsDialog CompatConfirmDialog(const MenuModel& model, const MenuView& view) {
    const MenuView::Confirm& c = view.confirm;
    if (c.id == Id::ModInstall) {
        const ModOffer* o = At(model.mods.mods.available, c.index);
        if (o && o->id == c.key) {
            return OfferQuestion(*o, "Install Anyway");
        }
    } else if (c.id == Id::PackDownload) {
        const PackOffer* o = At(model.mods.textures.downloads, c.index);
        if (o && o->id == c.key) {
            return OfferQuestion(*o, "Download Anyway");
        }
    } else if (c.id == Id::Cheat) {
        const CheatItem* o = At(model.mods.cheats.cheats, c.index);
        if (o && o->name == c.key && !o->version_note.empty()) {
            return {ModsDialogKind::Mismatch, CheatMismatchBody(o->version_note), "Turn On Anyway"};
        }
    }
    return {};
}


std::string LinkText(const std::string& url) {
    static constexpr std::string_view kPrefix = "https://";
    if (url.rfind(kPrefix, 0) != 0 || url.size() > 300) {
        return {};
    }
    std::string_view rest = std::string_view(url).substr(kPrefix.size());
    const std::size_t slash = rest.find('/');
    const std::string_view host = rest.substr(0, slash);
    if (host.empty() || host.front() == '.' || host.back() == '.' ||
        host.find('.') == std::string_view::npos) {
        return {};
    }
    for (const char ch : host) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '.')) {
            return {}; // no user, port, upper case or other tricks in the host
        }
    }
    for (const char ch : rest) {
        if (ch <= ' ' || ch >= 0x7f || std::string_view("\"<>\\^`{|}").find(ch) !=
                                            std::string_view::npos) {
            return {};
        }
    }
    while (!rest.empty() && rest.back() == '/') {
        rest.remove_suffix(1);
    }
    return std::string(rest);
}

std::string LinkHost(const std::string& url) {
    const std::string text = LinkText(url);
    return text.substr(0, text.find('/'));
}

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

std::string DownloadsText(uint64_t count) {
    if (count < 100) {
        return "Under 100 Downloads";
    }
    if (count < 1000) {
        return "100+ Downloads";
    }
    if (count < 5000) {
        return "1K+ Downloads";
    }
    if (count < 10000) {
        return "5K+ Downloads";
    }
    if (count < 50000) {
        return "10K+ Downloads";
    }
    return "50K+ Downloads";
}

std::size_t BylineSplit(float lead_w, const std::vector<float>& part_w, float sep_w, float width) {
    float at = lead_w;
    std::size_t n = 0;
    for (const float w : part_w) {
        const float add = (at > 0.0f ? sep_w : 0.0f) + w;
        if (at + add > width) {
            if (lead_w <= 0.0f && n == 0) {
                n = 1; // nothing before it: the first part stays, cut to fit
            }
            break;
        }
        at += add;
        n++;
    }
    return n;
}

std::string CountWithCommas(uint64_t n) { // AzaharFrame ui3
    std::string s = std::to_string(n);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) {
        s.insert(static_cast<std::size_t>(i), ",");
    }
    return s;
}

std::string OptimizeNudgeText(const std::string& pack) {
    return "Optimize " + pack + " for Frame?";
}

OptimizeRowText OptimizeText(const PackOptimize& o) {
    OptimizeRowText t;
    const std::string size = o.size_text.empty() ? std::string{} : " \u00b7 " + o.size_text;
    t.title = "PNG" + size;
    switch (o.state) {
    case OptimizeState::None:
        return {};
    case OptimizeState::Png:
        t.line = kOptimizeLine;
        t.button = 1;
        break;
    case OptimizeState::Checking:
        t.line = kOptimizeLine;
        t.ring = true;
        break;
    case OptimizeState::Converting:
        t.bar = true;
        t.button = 2;
        if (o.total > 0 && o.permille >= 0) {
            t.line = CountWithCommas(static_cast<uint64_t>(o.done)) + " of " +
                     CountWithCommas(static_cast<uint64_t>(o.total)) + " Textures \u00b7 " +
                     std::to_string(o.permille / 10) + "%";
        } else {
            t.line = o.phase.empty() ? std::string("Starting") : o.phase;
        }
        break;
    case OptimizeState::Stopped:
        t.line = kOptimizeStopped;
        t.button = 1;
        break;
    case OptimizeState::Failed:
        t.line = "Not Optimized" + (o.error.empty() ? std::string{} : " \u00b7 " + o.error);
        t.line_error = true;
        t.button = 1;
        break;
    case OptimizeState::Optimized:
        t.title = "Optimized for Frame \u00b7 ASTC" + size;
        if (o.original_removed) { // AzaharFrame ui3: nothing to go back to
            t.title += " \u00b7 Original Removed";
            break;
        }
        t.button = 3;
        t.remove = o.can_remove;
        t.line = "Original PNG Pack" +
                 (o.original_size.empty() ? std::string{} : " \u00b7 " + o.original_size);
        break;
    }
    return t;
}

std::string RemoveOriginalBody(const std::string& size, bool catalog) {
    return (size.empty() ? std::string{} : "Frees " + size + ". ") +
           "The optimized pack keeps working. To go back to the original, you'd need to " +
           (catalog ? "download" : "add") + " it again.";
}

std::string ModVersionText(const std::string& version) { // AzaharFrame ui3
    if (!version.empty() && version.front() >= '0' && version.front() <= '9') {
        return "v" + version;
    }
    return version;
}

std::vector<std::string> OfferParts(const ModOffer& offer) {
    if (!offer.note.empty()) {
        return {ModVersionText(offer.mod_version), offer.note};
    }
    return {ModVersionText(offer.mod_version), offer.version_match ? "Matches Your Game" : "",
            offer.downloads, offer.updated};
}

std::vector<std::string> PackOfferParts(const PackOffer& offer) {
    if (offer.state == PackOfferState::Failed || offer.state == PackOfferState::Downloading) {
        return {offer.note};
    }
    return {offer.downloads, offer.updated};
}

std::string BylineText(const std::string& author, const std::vector<std::string>& parts,
                       std::size_t first) {
    std::vector<std::string> one, two;
    for (const std::string& p : parts) {
        if (!p.empty()) {
            (one.size() < first ? one : two).push_back(p);
        }
    }
    std::string line = author.empty() ? JoinDetail(one) : JoinDetail({"Created by " + author, JoinDetail(one)});
    if (!two.empty()) {
        line += "\n" + JoinDetail(two);
    }
    return line;
}

namespace { // AzaharFrame ui3: the text's right edge of a catalog row (as drawn below)

float OfferTextRight(const Kit& kit, const ModOffer& o, float x1) {
    float right = x1;
    switch (o.state) {
    case OfferState::Install:
    case OfferState::Unavailable:
        right = x1 - ButtonWidth(kit, LabelFor(Id::ModInstall), false, 24.0f);
        break;
    case OfferState::Installing:
        right = x1 - ButtonWidth(kit, LabelFor(Id::ModCancel), false, 12.0f) - 32.0f;
        break;
    case OfferState::Installed:
        right = x1 - ButtonWidth(kit, "Installed", true, 12.0f) + 24.0f;
        break;
    }
    float name_right = right - kNameGap;
    if (!o.version.empty()) {
        name_right = right - 16.0f - (kit.TextWidth(Type::Label, o.version) + 24.0f) - kNameGap;
    }
    if (o.mismatch) {
        const float start = name_right + kNameGap - (o.version.empty() ? 16.0f : 8.0f);
        name_right = start - CompatChipsWidth(kit, o.region_chip) - kNameGap;
    }
    return name_right;
}

float PackTextRight(const Kit& kit, const PackOffer& o, float x1) {
    float right = x1;
    switch (o.state) {
    case PackOfferState::Download:
    case PackOfferState::Failed:
        right = x1 - ButtonWidth(kit,
                                 o.state == PackOfferState::Failed ? std::string("Try Again")
                                                                   : LabelFor(Id::PackDownload),
                                 false, 24.0f);
        break;
    case PackOfferState::Downloading: {
        float x = x1 - ButtonWidth(kit, LabelFor(Id::PackCancel), false, 12.0f) - 4.0f;
        if (o.percent >= 0) {
            x -= kit.TextWidth(Type::Label, "100%") + 12.0f;
        }
        right = x - 24.0f;
        break;
    }
    case PackOfferState::Installed:
        right = x1 - ButtonWidth(kit, "Installed", true, 12.0f) + 24.0f;
        break;
    }
    if (o.mismatch) {
        return right - 16.0f - CompatChipsWidth(kit, o.region_chip) - kNameGap;
    }
    return right - kNameGap;
}

} // namespace

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

float TabMenu::DrawRestartSnackbar(Kit& kit, const MenuView& view, const Rect& area, const std::string& text) {
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
                  false, Tok::kInverseOnSurface, text.empty() ? std::string(kRestartText) : text);
    return bar.y0 - 8.0f;
}

// AzaharFrame compat: "Turned Off <names>: The game closed unexpectedly after ..." with Turn
// Back On; Material's two-line snackbar (68 dp) when the text needs it.
std::string TurnedOffText(const std::vector<std::string>& names) {
    std::string joined;
    for (std::size_t i = 0; i < names.size(); i++) {
        if (i > 0) {
            joined += i + 1 == names.size() ? " and " : ", ";
        }
        joined += names[i];
    }
    return "Turned Off " + joined + ": The game closed unexpectedly after " +
           (names.size() > 1 ? "they were" : "it was") + " installed.";
}

float TabMenu::DrawTurnedOffSnackbar(Kit& kit, const MenuView& view, const Rect& area,
                                     const std::vector<std::string>& names, float bottom) {
    const Rect c{area.x0 + kPad, area.y0, area.x1 - kPad, area.y1};
    const std::string action = LabelFor(Id::TurnBackOn);
    const float aw = kit.TextWidth(Type::Label, action) + 24.0f;
    const float text_w = c.W() - 16.0f - 8.0f - aw - 8.0f;
    const auto lines = Wrap(kit, TurnedOffText(names), 16.0f, false, text_w, 2);
    const float h = lines.size() > 1 ? 68.0f : kSnackH;
    const float y1 = std::min(bottom, area.y1 - kSnackGap);
    const Rect bar{c.x0, y1 - h, c.x1, y1};
    kit.Fill(bar, 4.0f, Tok::kInverseSurface);
    const Rect a{bar.x1 - 8.0f - aw, bar.Cy() - 20.0f, bar.x1 - 8.0f, bar.Cy() + 20.0f};
    kit.StateLayer(a, 20.0f, kInversePrimary, StateOf(view, {Id::TurnBackOn}));
    kit.TextMid(a.Cx(), bar.Cy(), Type::Label, kInversePrimary, action, 1);
    AddHit({a.x0, bar.y0, a.x1, bar.y1}, {Id::TurnBackOn}, area);
    const float lh = 20.0f;
    float ty = bar.Cy() - lh * static_cast<float>(lines.size()) * 0.5f;
    for (const std::string& l : lines) {
        kit.TextSized(bar.x0 + 16.0f, ty + lh * 0.5f + kit.CapHeightSized(16.0f, false) * 0.5f, 16.0f,
                      false, Tok::kInverseOnSurface, l);
        ty += lh;
    }
    return bar.y0 - 8.0f;
}

// AzaharFrame ui3: Material's snackbar with one action (one line, two when the text needs it).
float TabMenu::DrawActionSnackbar(Kit& kit, const MenuView& view, const Rect& area,
                                  const std::string& text, Id action_id, float bottom) {
    const Rect c{area.x0 + kPad, area.y0, area.x1 - kPad, area.y1};
    const std::string action = LabelFor(action_id);
    const float aw = kit.TextWidth(Type::Label, action) + 24.0f;
    const float text_w = c.W() - 16.0f - 8.0f - aw - 8.0f;
    const auto lines = Wrap(kit, text, 16.0f, false, text_w, 2);
    const float h = lines.size() > 1 ? 68.0f : kSnackH;
    const float y1 = std::min(bottom, area.y1 - kSnackGap);
    const Rect bar{c.x0, y1 - h, c.x1, y1};
    kit.Fill(bar, 4.0f, Tok::kInverseSurface);
    const Rect a{bar.x1 - 8.0f - aw, bar.Cy() - 20.0f, bar.x1 - 8.0f, bar.Cy() + 20.0f};
    kit.StateLayer(a, 20.0f, kInversePrimary, StateOf(view, {action_id}));
    kit.TextMid(a.Cx(), bar.Cy(), Type::Label, kInversePrimary, action, 1);
    AddHit({a.x0, bar.y0, a.x1, bar.y1}, {action_id}, area);
    const float lh = 20.0f;
    float ty = bar.Cy() - lh * static_cast<float>(lines.size()) * 0.5f;
    for (const std::string& l : lines) {
        kit.TextSized(bar.x0 + 16.0f, ty + lh * 0.5f + kit.CapHeightSized(16.0f, false) * 0.5f, 16.0f,
                      false, Tok::kInverseOnSurface, l);
        ty += lh;
    }
    return bar.y0 - 8.0f;
}

// AzaharFrame ui3: Optimize for Frame on a pack's page:
//
//   PNG · 4.7 GB                                          [Optimize for Frame]
//   Converts the pack to a format the GPU reads directly. HD textures show
//   sooner and use less memory. The original is kept.
//
//   PNG · 4.7 GB                                                      Stop
//   ===========--------------------------
//   1,234 of 2,308 Textures · 42%
float TabMenu::DrawOptimizeRow(Kit& kit, const PackOptimize& o, const MenuView& view,
                               const Rect& area, float x0, float x1, float y) {
    const OptimizeRowText t = OptimizeText(o);
    if (o.state == OptimizeState::None) {
        return y;
    }
    constexpr float kTitleH = 48.0f, kLineH = 20.0f;
    const float cy = y + kTitleH * 0.5f;
    float right = x1;
    if (t.button != 0) {
        const Id id = t.button == 1 ? Id::OptimizePack : t.button == 2 ? Id::OptimizeStop : Id::UseOriginal;
        const bool tonal = t.button == 1;
        const std::string label = LabelFor(id);
        const float w = ButtonWidth(kit, label, false, tonal ? 24.0f : 12.0f);
        // a text button's own padding lines its label up with the edge
        const float edge = tonal ? x1 : x1 + 12.0f;
        const Rect b{edge - w, cy - kButtonH * 0.5f, edge, cy + kButtonH * 0.5f};
        kit.Button(b, tonal ? ButtonStyle::Tonal : ButtonStyle::Text, 0, label, StateOf(view, {id}));
        AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {id}, area);
        right = b.x0 - kNameGap;
        if (t.remove) { // AzaharFrame ui3: Remove Original, left of Use Original
            const std::string rl = LabelFor(Id::RemoveOriginal);
            const float rw = ButtonWidth(kit, rl, false, 12.0f);
            const Rect rb{b.x0 - 8.0f - rw, b.y0, b.x0 - 8.0f, b.y1};
            kit.Button(rb, ButtonStyle::Text, 0, rl, StateOf(view, {Id::RemoveOriginal}));
            AddHit({rb.x0, cy - 24.0f, rb.x1, cy + 24.0f}, {Id::RemoveOriginal}, area);
            right = rb.x0 - kNameGap;
        }
    } else if (t.ring) {
        Ring(kit, x1 - 12.0f, cy, 24.0f, o.spin);
        right = x1 - 24.0f - kNameGap;
    }
    kit.TextMid(x0, cy, Type::Body, Tok::kOnSurface, kit.Fit(Type::Body, t.title, right - x0));
    float yy = y + kTitleH;
    if (t.bar) { // Material 3 linear progress: the indicator, a 4 dp gap, the track, the stop dot
        constexpr float kH = 4.0f;
        const float w = x1 - x0;
        const float f = o.permille < 0 ? 0.0f : std::clamp(o.permille / 1000.0f, 0.0f, 1.0f);
        const float split = x0 + w * f;
        if (f > 0.0f) {
            kit.Fill({x0, yy, split, yy + kH}, kH * 0.5f, Tok::kPrimary);
        }
        const float t0 = f > 0.0f ? split + 4.0f : x0;
        if (t0 < x1) {
            kit.Fill({t0, yy, x1, yy + kH}, kH * 0.5f, Tok::kSecondaryContainer);
            kit.Fill({x1 - kH, yy, x1, yy + kH}, kH * 0.5f, Tok::kPrimary);
        }
        yy += kH + 8.0f;
    }
    if (!t.line.empty()) {
        const auto lines = Wrap(kit, t.line, kSmall, false, x1 - x0, 2);
        for (const std::string& l : lines) {
            kit.TextSized(x0, yy + kLineH * 0.5f + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall,
                          false, t.line_error ? Tok::kError : Tok::kOnSurfaceVariant, l);
            yy += kLineH;
        }
    }
    yy += 12.0f;
    kit.Line(x0, yy, x1, yy, 1.0f, Tok::kOutlineVariant);
    return yy + 8.0f;
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
        headline = d.mod == "github.com" ? std::string("Open GitHub?") : "Open " + d.mod + "?";
        body = "This opens " + d.other + " in Steam's browser.";
        confirm = "Open";
        break;
    case ModsDialogKind::BigDownload:
        headline = "Download " + d.other + "?";
        body = d.mod;
        confirm = "Download";
        break;
    case ModsDialogKind::DeletePack:
        headline = "Delete " + d.mod + "?";
        body = d.other.empty() ? std::string("You can download it again.") : d.other; // zipinstall
        confirm = "Delete";
        break;
    case ModsDialogKind::Mismatch: // AzaharFrame compat
        headline = kMayNotWorkTitle;
        body = d.mod;
        confirm = d.other;
        break;
    case ModsDialogKind::Unscanned: // AzaharFrame compat
        headline = kUnscannedTitle;
        body = kUnscannedBody;
        confirm = "Download Anyway";
        break;
    case ModsDialogKind::Preload: // AzaharFrame memguard
        headline = d.title;
        body = d.mod;
        confirm = "Turn On";
        break;
    case ModsDialogKind::Optimize: // AzaharFrame ui3
        headline = "Optimize Texture Pack?";
        body = kOptimizeBody;
        confirm = "Optimize";
        break;
    case ModsDialogKind::RemoveOriginal: // AzaharFrame ui3
        headline = kRemoveOriginalTitle;
        body = d.mod;
        confirm = "Remove";
        break;
    case ModsDialogKind::Notice: // AzaharFrame savestate
        headline = d.mod;
        body = d.other;
        confirm = d.confirm;
        break;
    case ModsDialogKind::None:
        break;
    }
    // AzaharFrame memguard: Preload is wider, with a list of numbers between two dividers and a
    // status row (an icon and one line) under the supporting text
    const bool numbers = d.kind == ModsDialogKind::Preload;
    // AzaharFrame ui3: label and value rows under the supporting text (no "Label: value").
    std::vector<std::pair<std::string, std::string>> info_rows;
    if (d.kind == ModsDialogKind::Optimize) {
        info_rows = {{"Textures", d.mod}, {"Size", d.other}};
    }
    constexpr float kInfoRowH = 28.0f;
    const float rows_h =
        info_rows.empty() ? 0.0f : 8.0f + kInfoRowH * static_cast<float>(info_rows.size());
    const float kW = numbers ? 520.0f : 440.0f;
    constexpr float kP = 24.0f, kHead = 26.0f, kBody = 16.0f;
    const float tw = kW - kP * 2.0f;
    const auto head_lines = Wrap(kit, headline, kHead, false, tw, 3);
    const auto body_lines = Wrap(kit, body, kBody, false, tw, 7); // AzaharFrame compat: 7 lines
    const float head_lh = 32.0f, body_lh = 24.0f;
    constexpr float kRowH = 40.0f; // a one-line list item, dense
    const float numbers_h = numbers ? 16.0f + 1.0f + kRowH * static_cast<float>(d.rows.size()) +
                                          1.0f + (d.status != 0 ? 8.0f + kRowH : 0.0f)
                                    : 0.0f;
    const float h = kP + head_lh * static_cast<float>(head_lines.size()) + 16.0f +
                    body_lh * static_cast<float>(body_lines.size()) + numbers_h + rows_h + 24.0f +
                    kButtonH + kP;
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
    if (numbers) { // AzaharFrame memguard: the list (label left, value right), the status row
        const auto divider = [&](float at) {
            kit.Fill({box.x0 + kP, at, box.x1 - kP, at + 1.0f}, 0.0f, Tok::kOutlineVariant);
        };
        y += 16.0f;
        divider(y);
        y += 1.0f;
        const float cap = kit.CapHeightSized(kBody, false) * 0.5f;
        for (const auto& [label, value] : d.rows) {
            const float base = y + kRowH * 0.5f + cap;
            kit.TextSized(box.x0 + kP, base, kBody, false, Tok::kOnSurfaceVariant, label);
            kit.TextSized(box.x1 - kP - kit.TextWidthSized(kBody, true, value), base, kBody, true,
                          Tok::kOnSurface, value);
            y += kRowH;
        }
        divider(y);
        y += 1.0f;
        if (d.status != 0) {
            y += 8.0f;
            const float cy = y + kRowH * 0.5f;
            const float ix = box.x0 + kP + 12.0f;
            // The font has no check_circle or warning: a filled circle with check in it, and
            // info filled in the error colour
            if (d.status == 1) {
                kit.Fill({ix - 11.0f, cy - 11.0f, ix + 11.0f, cy + 11.0f}, 11.0f, Tok::kPrimary);
                kit.Icon(ix, cy, 18.0f, Sym::kCheck, false, Tok::kOnPrimary);
            } else {
                kit.Icon(ix, cy, 24.0f, Sym::kInfo, true, Tok::kError);
            }
            kit.TextSized(ix + 24.0f, cy + kit.CapHeightSized(kBody, true) * 0.5f, kBody, true,
                          d.status == 1 ? Tok::kOnSurface : Tok::kError, d.other);
            y += kRowH;
        }
    }
    if (!info_rows.empty()) { // AzaharFrame ui3: the label left, the value right
        y += 8.0f;
        for (const auto& [label, value] : info_rows) {
            const float base = y + kInfoRowH * 0.5f + kit.CapHeightSized(kBody, false) * 0.5f;
            kit.TextSized(box.x0 + kP, base, kBody, false, Tok::kOnSurfaceVariant, label);
            kit.TextSized(box.x1 - kP - kit.TextWidthSized(kBody, true, value), base, kBody, true,
                          Tok::kOnSurface, value);
            y += kInfoRowH;
        }
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
    } else if (d.kind == ModsDialogKind::Notice) { // AzaharFrame savestate
        action(Id::DialogConfirm, d.confirm);
        if (!d.cancel.empty()) {
            action(Id::DialogCancel, d.cancel);
        }
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
    if (!m.turned_off.empty()) { // AzaharFrame compat
        bottom = DrawTurnedOffSnackbar(kit, view, area, m.turned_off, area.y1);
    }
    if (m.needs_restart && m.turned_off.empty()) { // one snackbar at a time: Turned Off first
        bottom = DrawRestartSnackbar(kit, view, area, m.notice); // AzaharFrame catalog3
    }

    if (view.mods_page == ModsPage::Install && model.mods.zip.open &&
        model.mods.zip.target == ZipTarget::Mods) {
        DrawZip(kit, model.mods.zip, view, area, bottom); // AzaharFrame zipinstall
        return;
    }
    if (view.mods_page == ModsPage::Install) {
        // ---- Install Mods: the back button and the title, then the title's mods ----
        const Rect back{c.x0 - 12.0f, c.y0, c.x0 + 36.0f, c.y0 + kBarH};
        kit.StateLayer(back, 24.0f, Tok::kOnSurfaceVariant, StateOf(view, {Id::ModsBack}));
        kit.Icon(back.Cx(), back.Cy(), 24.0f, Sym::kArrowBack, false, Tok::kOnSurface);
        AddHit(back, {Id::ModsBack}, area);
        kit.TextMid(back.x1 + 12.0f, back.Cy(), Type::Title, Tok::kOnSurface,
                    LabelFor(Id::InstallMods));
        DrawZipButton(kit, view, area, ZipTarget::Mods, !m.busy); // AzaharFrame zipinstall
        const float top = c.y0 + kBarH + 8.0f;
        const Rect list{area.x0, top, area.x1, bottom};
        if (m.catalog != CatalogState::Ready) {
            EmptyState(kit, {c.x0, top, c.x1, bottom}, 0, true, m.spin, "Loading Mods", "");
        } else if (m.available.empty()) {
            EmptyState(kit, {c.x0, top, c.x1, bottom}, Sym::kExtension, false, 0,
                       "No Mods for This Game", "");
        } else {
            // AzaharFrame ui3: a row is taller only when its byline goes on to a second line
            std::vector<float> heights;
            for (const ModOffer& o : m.available) {
                heights.push_back(CatalogRowH(kit, OfferTextRight(kit, o, c.x1) - c.x0, o.author,
                                              OfferParts(o)));
            }
            const ListScroll s = Scroll(heights, list.H(), ScrollWanted(view));
            layout.max_scroll = s.max_scroll;
            layout.scroll_step = s.step;
            kit.Clip(list);
            // AzaharFrame modcat: the version chip's tooltip, drawn after the list
            struct ChipTip {
                float cx = 0.0f, y = 0.0f;
                bool above = true;
                std::string text;
            } chip_tip;
            NoteTip note_tip; // AzaharFrame modcat2
            // AzaharFrame compat: the row whose notes show and where the names' column ends
            const ModOffer* tip_row = nullptr;
            Rect tip_rect{};
            float names_right = c.x1;
            NoteTip compat_tip;
            for (std::size_t i = 0; i < m.available.size(); i++) {
                const float y = top + s.tops[i] - s.scroll;
                const float rh = heights[i];
                if (y + rh <= list.y0 || y + rh > list.y1 + 0.01f) {
                    continue; // whole rows only
                }
                const ModOffer& o = m.available[i];
                const int idx = static_cast<int>(i);
                const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + rh};
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
                    // AzaharFrame integ3: Cancel where the Install button was (as Download Packs'
                    // rows), the ring left of it; the row's line says "Downloading 42%".
                    const std::string label = LabelFor(Id::ModCancel);
                    const float w = ButtonWidth(kit, label, false, 12.0f);
                    const Rect b{c.x1 - w, cy - kButtonH * 0.5f, c.x1, cy + kButtonH * 0.5f};
                    kit.Button(b, ButtonStyle::Text, 0, label, StateOf(view, {Id::ModCancel, idx}));
                    AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {Id::ModCancel, idx}, list);
                    Ring(kit, b.x0 - 16.0f, cy, 24.0f, m.spin);
                    right = b.x0 - 32.0f;
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
                // AzaharFrame modcat: the version chip, left of the button (the Cheats rows' chip:
                // outlined; filled for the build that matches the game); hover shows its tooltip
                float name_right = right - kNameGap;
                if (!o.version.empty()) {
                    constexpr float kChipH = 32.0f, kChipPad = 12.0f;
                    const float cw = kit.TextWidth(Type::Label, o.version) + kChipPad * 2.0f;
                    const Rect chip{right - 16.0f - cw, cy - kChipH * 0.5f, right - 16.0f,
                                    cy + kChipH * 0.5f};
                    if (o.version_match) {
                        kit.Fill(chip, 8.0f, Tok::kSecondaryContainer);
                        kit.TextMid(chip.Cx(), cy, Type::Label, Tok::kOnSecondaryContainer, o.version, 1);
                    } else {
                        kit.Stroke(chip, 8.0f, 1.0f, Tok::kOutline);
                        kit.TextMid(chip.Cx(), cy, Type::Label, Tok::kOnSurfaceVariant, o.version, 1);
                    }
                    // The touch target 48 dp round the 32 dp chip, as the Install button's.
                    const float hw = std::max(chip.W(), 48.0f) * 0.5f;
                    AddHit({chip.Cx() - hw, cy - 24.0f, chip.Cx() + hw, cy + 24.0f},
                           {Id::ModVersion, idx}, list);
                    if (view.tooltip == Id::ModVersion && view.tooltip_index == idx &&
                        !o.version_tip.empty()) {
                        const float half = kit.TextWidth(Type::Tooltip, o.version_tip) * 0.5f + 10.0f;
                        chip_tip.cx = std::clamp(chip.Cx(), area.x0 + half + 4.0f, area.x1 - half - 4.0f);
                        // above the chip; under it for the first row shown
                        chip_tip.above = chip.y0 - 40.0f > list.y0;
                        chip_tip.y = chip_tip.above ? chip.y0 : chip.y1;
                        chip_tip.text = o.version_tip;
                    }
                    name_right = chip.x0 - kNameGap;
                }
                // AzaharFrame compat: made for another version: the red chip and the region's
                if (o.mismatch) {
                    Rect chips{};
                    const float x0 = CompatChips(kit, name_right + kNameGap - (o.version.empty() ? 16.0f : 8.0f), cy,
                                                 o.region_chip, &chips);
                    AddHit({chips.x0, cy - 24.0f, chips.x1, cy + 24.0f}, {Id::ModCompat, idx}, list);
                    if (view.tooltip == Id::ModCompat && view.tooltip_index == idx &&
                        !o.mismatch_reason.empty()) {
                        compat_tip = MakeChipTip(kit, o.mismatch_reason, chips, list, area);
                    }
                    name_right = x0 - kNameGap;
                }
                names_right = std::min(names_right, name_right);
                AddHit({row.x0, y, name_right, y + rh}, {Id::ModNote, idx}, list);
                if (view.tooltip == Id::ModNote && view.tooltip_index == idx) {
                    tip_row = &o;
                    tip_rect = row;
                }
                // AzaharFrame compat: "Created by <author>" (any https page), AzaharFrame ui3: then
                // " · v172 · Matches Your Game · 10K+ Downloads · Updated Sep 2026" (the phase or the
                // failure while it installs or when it failed): OfferParts
                const bool link = !LinkText(o.url).empty();
                const Rect l = CatalogRowText(kit, c.x0, name_right, row, o.name, o.verified, o.author,
                                              link, StateOf(view, {Id::ModLink, idx}), OfferParts(o),
                                              !o.note.empty() && o.error ? 1 : std::string::npos);
                if (l.W() > 0.0f) {
                    AddHit(l, {Id::ModLink, idx}, list);
                }
            }
            kit.ClearClip();
            ScrollIndicator(kit, list, area.x1, s);
            if (tip_row != nullptr) {
                const std::string text = RowTip(tip_row->mismatch, tip_row->mismatch_reason, tip_row->notes);
                if (!text.empty()) {
                    note_tip = MakeNoteTip(kit, text, tip_rect, list, area, c.x0, names_right);
                }
            }
            if (!note_tip.text.empty()) {
                kit.TooltipWrapped(note_tip.cx, note_tip.y, note_tip.text, note_tip.above, note_tip.max_w);
            }
            if (!chip_tip.text.empty()) {
                kit.Tooltip(chip_tip.cx, chip_tip.y, chip_tip.text, chip_tip.above);
            }
            if (!compat_tip.text.empty()) {
                kit.TooltipWrapped(compat_tip.cx, compat_tip.y, compat_tip.text, compat_tip.above,
                                   compat_tip.max_w);
            }
        }
        DrawModsDialog(kit, DialogFor(m.dialog, view, model), view);
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
        DrawModsDialog(kit, DialogFor(m.dialog, view, model), view);
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
    const std::string remove = LabelFor(Id::ModRemove);
    const float remove_w = ButtonWidth(kit, remove, false, 12.0f);
    // AzaharFrame ui3: the text's right edge, left of Remove (as below); a row whose byline goes on
    // to a second line is a catalog row's height
    const float loaded_right = c.x1 - kSwitchW - kTrailGap + 12.0f - remove_w + 12.0f - kNameGap;
    std::vector<float> heights;
    for (const ModRow& r : m.loaded) {
        heights.push_back(!r.author.empty()
                              ? (FitByline(kit, loaded_right - c.x0, r.author, true, {r.detail}).Wraps()
                                     ? kRowCatalog
                                     : kRowLink)
                              : r.detail.empty() ? kRow1 : kRow2);
    }
    const ListScroll s = Scroll(heights, list.H(), ScrollWanted(view));
    layout.max_scroll = s.max_scroll;
    layout.scroll_step = s.step;
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
            const bool link = !LinkText(r.url).empty(); // AzaharFrame modcat: GameBanana pages too
            // AzaharFrame ui3: "Created by <author> · v1.5.9", going on to a second line when it
            // runs out of width (as the catalog rows)
            const BylineFit f = FitByline(kit, text_right - c.x0, r.author, true, {r.detail});
            const bool wraps = f.Wraps();
            const float by_y = wraps ? row.y0 + kCatalogByY : cy + kLinkLineDy + 13.0f;
            if (wraps) {
                RowText(kit, c.x0, text_right, row.y0 + kCatalogNameY, r.name, r.verified, "", false, op);
            } else {
                RowText(kit, c.x0, text_right, cy + kLinkNameDy, r.name, r.verified, "", false, op, true);
            }
            const Rect l = DrawByline(kit, c.x0, text_right, by_y, row.y0 + kCatalogWrapY, f, link,
                                      StateOf(view, {Id::ModLink, idx}, enabled), std::string::npos, op);
            if (link && enabled && l.W() > 0.0f) {
                const float pad = std::max(8.0f, (48.0f - l.W()) * 0.5f);
                AddHit(wraps ? Rect{l.x0 - pad, by_y - 24.0f, l.x1 + pad, by_y + 24.0f} : LinkTarget(l, row),
                       {Id::ModLink, idx}, list);
            }
        }
    }
    kit.ClearClip();
    ScrollIndicator(kit, list, area.x1, s);
    DrawModsDialog(kit, DialogFor(m.dialog, view, model), view);
}

// ---------------------------------------------------------------------------------------------
// Textures

void TabMenu::DrawTextures(Kit& kit, const MenuModel& model, const MenuView& view,
                           const Rect& area) {
    const TexturesModel& m = model.mods.textures;
    const Rect c{area.x0 + kPad, area.y0 + kTop, area.x1 - kPad, area.y1 - kPad};
    layout.max_scroll = 0.0f;
    float bottom = area.y1 - 8.0f;
    const std::vector<std::string>& turned_off = model.mods.mods.turned_off; // AzaharFrame compat
    if (!turned_off.empty()) {
        bottom = DrawTurnedOffSnackbar(kit, view, area, turned_off, area.y1);
    }
    if (m.needs_restart && turned_off.empty()) {
        bottom = DrawRestartSnackbar(kit, view, area);
    }
    // AzaharFrame ui3: "Optimize <Pack> for Frame?" over the pack list (once a pack)
    if (!m.nudge_pack.empty() && view.textures_page == TexturesPage::Packs) {
        bottom = DrawActionSnackbar(kit, view, area, OptimizeNudgeText(m.nudge_name), Id::OptimizeNudge,
                                    bottom);
    }
    // AzaharFrame ui2: Install from Zip is on Download Packs, as it is on Install Mods.
    if (view.textures_page == TexturesPage::Download && model.mods.zip.open &&
        model.mods.zip.target == ZipTarget::Textures) {
        DrawZip(kit, model.mods.zip, view, area, bottom); // AzaharFrame zipinstall
        return;
    }
    if (view.textures_page == TexturesPage::Download) {
        DrawPackDownloads(kit, m, view, area, bottom);
        DrawModsDialog(kit, DialogFor(m.dialog, view, model), view);
        return;
    }
    if (view.textures_page == TexturesPage::Parts) { // AzaharFrame texparts
        DrawTextureParts(kit, m.parts, view, area, bottom);
        DrawModsDialog(kit, DialogFor(m.dialog, view, model), view); // AzaharFrame ui3: Optimize
        return;
    }
    // AzaharFrame ui2: the same as the Mods segment: one button to the page of this game's packs
    // (with Install from Zip there), and with nothing installed one centred state.
    const std::string download = LabelFor(Id::DownloadPacks);
    if (m.packs.empty()) {
        const Rect a{c.x0, c.y0, c.x1, bottom};
        const float h = 48.0f + 16.0f + 24.0f + 24.0f + kBarH;
        float y = a.Cy() - h * 0.5f;
        kit.Icon(a.Cx(), y + 24.0f, 48.0f, Sym::kTexture, false, Tok::kOnSurfaceVariant);
        y += 48.0f + 16.0f;
        kit.TextMid(a.Cx(), y + 12.0f, Type::Title, Tok::kOnSurface, "No Texture Packs", 1);
        y += 24.0f + 24.0f;
        const float w = ButtonWidth(kit, download, true, 24.0f);
        const Rect b{a.Cx() - w * 0.5f, y, a.Cx() + w * 0.5f, y + kBarH};
        kit.Button(b, ButtonStyle::Filled, Sym::kDownload, download,
                   StateOf(view, {Id::DownloadPacks}));
        AddHit(b, {Id::DownloadPacks}, area);
        DrawModsDialog(kit, DialogFor(m.dialog, view, model), view);
        return;
    }
    {
        const Rect b{c.x0, c.y0, c.x0 + ButtonWidth(kit, download, true, 24.0f), c.y0 + kBarH};
        kit.Button(b, ButtonStyle::Tonal, Sym::kDownload, download,
                   StateOf(view, {Id::DownloadPacks}));
        AddHit(b, {Id::DownloadPacks}, area);
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

    const float top = c.y0 + kBarH + 8.0f;
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
            float right = c.x1;
            float trail = c.x1; // AzaharFrame texparts: where the next button ends
            if (p.downloaded) {
                // Delete, for the packs Download Packs brought (they can come again).
                const std::string label = LabelFor(Id::PackDelete);
                const float w = ButtonWidth(kit, label, false, 12.0f);
                const Rect b{c.x1 - w, cy - kButtonH * 0.5f, c.x1, cy + kButtonH * 0.5f}; // integ
                const bool can = !m.busy && !m.downloading;
                const Target dt{Id::PackDelete, it.index - 1};
                kit.Button(b, ButtonStyle::Text, 0, label, StateOf(view, dt, can));
                if (can) {
                    AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, dt, list);
                }
                right = b.x0 + 12.0f - kNameGap;
                trail = b.x0 - 8.0f;
            }
            if (p.customize) {
                // AzaharFrame texparts: Customize, a page of the pack's parts
                const std::string label = LabelFor(Id::PackCustomize);
                const float w = ButtonWidth(kit, label, true, 12.0f);
                const Rect b{trail - w, cy - kButtonH * 0.5f, trail, cy + kButtonH * 0.5f};
                const Target ct{Id::PackCustomize, it.index - 1};
                kit.Button(b, ButtonStyle::Text, Sym::kTune, label, StateOf(view, ct, !m.busy));
                if (!m.busy) {
                    AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, ct, list);
                }
                right = b.x0 + 12.0f - kNameGap;
            }
            RowText(kit, tx, right, cy, p.name, false, p.detail, false, op);
        }
    }
    kit.ClearClip();
    ScrollIndicator(kit, list, area.x1, s);
    DrawModsDialog(kit, DialogFor(m.dialog, view, model), view);
}

void TabMenu::DrawPackDownloads(Kit& kit, const TexturesModel& m, const MenuView& view,
                                const Rect& area, float bottom) {
    // ---- Download Packs: the back button and the title, then the title's packs ----
    const Rect c{area.x0 + kPad, area.y0 + kTop, area.x1 - kPad, area.y1 - kPad};
    const Rect back{c.x0 - 12.0f, c.y0, c.x0 + 36.0f, c.y0 + kBarH};
    kit.StateLayer(back, 24.0f, Tok::kOnSurfaceVariant, StateOf(view, {Id::TexturesBack}));
    kit.Icon(back.Cx(), back.Cy(), 24.0f, Sym::kArrowBack, false, Tok::kOnSurface);
    AddHit(back, {Id::TexturesBack}, area);
    kit.TextMid(back.x1 + 12.0f, back.Cy(), Type::Title, Tok::kOnSurface,
                LabelFor(Id::DownloadPacks));
    DrawZipButton(kit, view, area, ZipTarget::Textures, true); // AzaharFrame ui2
    const float top = c.y0 + kBarH + 8.0f;
    const Rect list{area.x0, top, area.x1, bottom};
    if (m.downloads.empty()) {
        EmptyState(kit, {c.x0, top, c.x1, bottom}, Sym::kTexture, false, 0,
                   "No Packs for This Game", "");
        return;
    }
    std::vector<float> heights; // AzaharFrame ui3: as Install Mods
    for (const PackOffer& o : m.downloads) {
        heights.push_back(CatalogRowH(kit, PackTextRight(kit, o, c.x1) - c.x0, o.author,
                                      PackOfferParts(o)));
    }
    const ListScroll s = Scroll(heights, list.H(), ScrollWanted(view));
    layout.max_scroll = s.max_scroll;
    layout.scroll_step = s.step;
    kit.Clip(list);
    NoteTip note_tip; // AzaharFrame modcat2
    const PackOffer* tip_row = nullptr; // AzaharFrame compat: as Install Mods
    Rect tip_rect{};
    float names_right = c.x1;
    NoteTip compat_tip;
    for (std::size_t i = 0; i < m.downloads.size(); i++) {
        const float y = top + s.tops[i] - s.scroll;
        const float rh = heights[i];
        if (y + rh <= list.y0 || y + rh > list.y1 + 0.01f) {
            continue; // whole rows only
        }
        const PackOffer& o = m.downloads[i];
        const int idx = static_cast<int>(i);
        const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + rh};
        const float cy = row.Cy();
        AddHit(row, {Id::PackDownload, kLabelIndex}, list);
        float right = c.x1;
        switch (o.state) {
        case PackOfferState::Download:
        case PackOfferState::Failed: {
            const std::string label = o.state == PackOfferState::Failed ? std::string("Try Again")
                                                                        : LabelFor(Id::PackDownload);
            const float w = ButtonWidth(kit, label, false, 24.0f);
            const Rect b{c.x1 - w, cy - kButtonH * 0.5f, c.x1, cy + kButtonH * 0.5f};
            const bool on = !m.downloading;
            kit.Button(b, ButtonStyle::Tonal, 0, label, StateOf(view, {Id::PackDownload, idx}, on));
            if (on) {
                AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {Id::PackDownload, idx}, list);
            }
            right = b.x0;
            break;
        }
        case PackOfferState::Downloading: {
            // The ring, the percent, Cancel.
            const std::string label = LabelFor(Id::PackCancel);
            const float w = ButtonWidth(kit, label, false, 12.0f);
            // AzaharFrame integ: inside the content edge (its hover container too), the ring and
            // percent left of it.
            const Rect b{c.x1 - w, cy - kButtonH * 0.5f, c.x1, cy + kButtonH * 0.5f};
            kit.Button(b, ButtonStyle::Text, 0, label, StateOf(view, {Id::PackCancel, idx}));
            AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, {Id::PackCancel, idx}, list);
            float x = b.x0 - 4.0f;
            if (o.percent >= 0) {
                const std::string pct = std::to_string(std::min(o.percent, 100)) + "%";
                const float pw = kit.TextWidth(Type::Label, "100%");
                kit.TextMid(x, cy, Type::Label, Tok::kOnSurface, pct, 2);
                x -= pw + 12.0f;
            }
            if (o.percent >= 0) {
                RingValue(kit, x - 12.0f, cy, 24.0f, static_cast<float>(o.percent) / 100.0f);
            } else {
                Ring(kit, x - 12.0f, cy, 24.0f, m.spin); // connecting, checking, unpacking
            }
            right = x - 24.0f;
            break;
        }
        case PackOfferState::Installed: {
            const std::string label = "Installed";
            const float w = ButtonWidth(kit, label, true, 12.0f);
            const Rect b{c.x1 - w + 12.0f, cy - kButtonH * 0.5f, c.x1 + 12.0f, cy + kButtonH * 0.5f};
            kit.Button(b, ButtonStyle::Text, Sym::kCheck, label, State::Disabled);
            right = b.x0 + 12.0f;
            break;
        }
        }
        float text_right = right - kNameGap;
        if (o.mismatch) { // AzaharFrame compat: made for another version
            Rect chips{};
            const float x0 = CompatChips(kit, right - 16.0f, cy, o.region_chip, &chips);
            AddHit({chips.x0, cy - 24.0f, chips.x1, cy + 24.0f}, {Id::PackCompat, idx}, list);
            if (view.tooltip == Id::PackCompat && view.tooltip_index == idx && !o.mismatch_reason.empty()) {
                compat_tip = MakeChipTip(kit, o.mismatch_reason, chips, list, area);
            }
            text_right = x0 - kNameGap;
        }
        names_right = std::min(names_right, text_right);
        // AzaharFrame zipinstall: the verified badge; "<author> · Updated Sep 2026" (the phase
        // while it downloads, the error when it failed)
        AddHit({row.x0, y, text_right, y + rh}, {Id::PackNote, idx}, list); // AzaharFrame modcat2
        if (view.tooltip == Id::PackNote && view.tooltip_index == idx) {
            tip_row = &o;
            tip_rect = row;
        }
        // AzaharFrame compat: "Created by <author>", AzaharFrame ui3: then " · 10K+ Downloads ·
        // Updated Sep 2026" (the phase while it downloads, the error when it failed)
        const bool failed = o.state == PackOfferState::Failed;
        const bool link = !LinkText(o.url).empty();
        const Rect l = CatalogRowText(kit, c.x0, text_right, row, o.name, o.verified, o.author, link,
                                      StateOf(view, {Id::PackLink, idx}), PackOfferParts(o),
                                      failed ? 0 : std::string::npos);
        if (l.W() > 0.0f) {
            AddHit(l, {Id::PackLink, idx}, list);
        }
    }
    kit.ClearClip();
    ScrollIndicator(kit, list, area.x1, s);
    if (tip_row != nullptr) {
        const std::string text = RowTip(tip_row->mismatch, tip_row->mismatch_reason, tip_row->notes);
        if (!text.empty()) {
            note_tip = MakeNoteTip(kit, text, tip_rect, list, area, c.x0, names_right);
        }
    }
    if (!note_tip.text.empty()) {
        kit.TooltipWrapped(note_tip.cx, note_tip.y, note_tip.text, note_tip.above, note_tip.max_w);
    }
    if (!compat_tip.text.empty()) {
        kit.TooltipWrapped(compat_tip.cx, compat_tip.y, compat_tip.text, compat_tip.above,
                           compat_tip.max_w);
    }
}

// AzaharFrame ui2: Install from Zip at the end of a download page's bar (Install Mods, Download
// Packs), in the same place with the same words on both.
void TabMenu::DrawZipButton(Kit& kit, const MenuView& view, const Rect& area, ZipTarget target,
                            bool enabled) {
    const Rect c{area.x0 + kPad, area.y0 + kTop, area.x1 - kPad, area.y1 - kPad};
    const std::string zip = LabelFor(Id::InstallFromZip);
    const Target zt{Id::InstallFromZip, static_cast<int>(target)};
    const Rect b{c.x1 - ButtonWidth(kit, zip, true, 24.0f), c.y0 + (kBarH - kButtonH) * 0.5f, c.x1,
                 c.y0 + (kBarH + kButtonH) * 0.5f};
    kit.Button(b, ButtonStyle::Tonal, Sym::kFolderOpen, zip, StateOf(view, zt, enabled));
    if (enabled) {
        AddHit({b.x0, c.y0, b.x1, c.y0 + kBarH}, zt, area);
    }
}

// ---------------------------------------------------------------------------------------------
// AzaharFrame zipinstall: Install from Zip
//   <-  Install from Zip
//   [v Downloads] [Documents] [Home]                       filter chips (VR)
//   ~/Downloads                                            the folder, or why the last one failed
//   [] ..
//   [] Packs
//   [] Ocarina 4K.zip                               1.2 GB
// While it works: a progress ring, the phase and the file. Several games in the archive: "Choose
// the Game" and their title IDs.

void TabMenu::DrawZip(Kit& kit, const ZipModel& m, const MenuView& view, const Rect& area,
                      float bottom) {
    const Rect c{area.x0 + kPad, area.y0 + kTop, area.x1 - kPad, area.y1 - kPad};
    const bool choosing = !m.choices.empty() && !m.working;
    const Rect back{c.x0 - 12.0f, c.y0, c.x0 + 36.0f, c.y0 + kBarH};
    if (!m.working) {
        kit.StateLayer(back, 24.0f, Tok::kOnSurfaceVariant, StateOf(view, {Id::ZipBack}));
        kit.Icon(back.Cx(), back.Cy(), 24.0f, Sym::kArrowBack, false, Tok::kOnSurface);
        AddHit(back, {Id::ZipBack}, area);
    } else {
        kit.Icon(back.Cx(), back.Cy(), 24.0f, Sym::kArrowBack, false, Tok::kOnSurface,
                 kDisabledContent);
    }
    // AzaharFrame ui2: a cheat file's page has the Cheats segment's button's name
    const std::string title =
        LabelFor(m.target == ZipTarget::Cheats ? Id::InstallCheatFile : Id::InstallFromZip);
    kit.TextMid(back.x1 + 12.0f, back.Cy(), Type::Title, Tok::kOnSurface,
                choosing ? std::string("Choose the Game") : title);
    float top = c.y0 + kBarH + 8.0f;
    const Rect rest{c.x0, top, c.x1, bottom};
    if (m.working) {
        EmptyState(kit, rest, 0, true, m.spin, m.phase.empty() ? std::string("Installing") : m.phase,
                   m.file);
        DrawModsDialog(kit, m.dialog, view);
        return;
    }

    struct Row {
        Id id;
        int index;
        std::string name;
        std::string detail;
        char32_t icon;
    };
    std::vector<Row> rows;
    if (choosing) {
        for (std::size_t i = 0; i < m.choices.size(); i++) {
            rows.push_back({Id::ZipChoice, static_cast<int>(i), m.choices[i], "", 0});
        }
    } else if (m.browse) {
        // The quick folders: Material filter chips (the remap row's look)
        constexpr float kChipH = 32.0f, kChipR = 8.0f, kCheckW = 18.0f + 8.0f;
        float x = c.x0;
        for (std::size_t i = 0; i < m.chips.size(); i++) {
            const ZipChip& chip = m.chips[i];
            const bool chosen = chip.path == m.folder;
            const float w = (chosen ? 8.0f + kCheckW : 16.0f) + kit.TextWidth(Type::Label, chip.label) + 16.0f;
            const Rect r{x, top + 8.0f, x + w, top + 8.0f + kChipH};
            const Target tg{Id::ZipChip, static_cast<int>(i)};
            if (chosen) {
                kit.Fill(r, kChipR, Tok::kSecondaryContainer);
            } else {
                kit.Stroke(r, kChipR, 1.0f, Tok::kOutline);
            }
            const uint32_t fg = chosen ? Tok::kOnSecondaryContainer : Tok::kOnSurfaceVariant;
            kit.StateLayer(r, kChipR, fg, StateOf(view, tg));
            float tx = r.x0 + 16.0f;
            if (chosen) {
                kit.Icon(r.x0 + 8.0f + 9.0f, r.Cy(), 18.0f, Sym::kCheck, false, fg);
                tx = r.x0 + 8.0f + kCheckW;
            }
            kit.TextMid(tx, r.Cy(), Type::Label, fg, chip.label);
            AddHit({r.x0, top, r.x1, top + kRow1}, tg, area);
            x += w + 8.0f;
        }
        top += kRow1;
        // The folder, or why the last archive was not installed
        const bool failed = !m.error.empty();
        const std::string line = failed ? (m.file.empty() ? m.error : m.file + ": " + m.error)
                                        : m.folder_text;
        kit.TextSized(c.x0, top + 14.0f, kSmall, false, failed ? Tok::kError : Tok::kOnSurfaceVariant,
                      kit.Fit(Type::Tooltip, line, c.W()));
        top += 24.0f;
        if (m.can_go_up && !m.loading) {
            rows.push_back({Id::ZipUp, -1, "..", "", Sym::kFolderOpen});
        }
        if (!m.loading) {
            const char32_t archive = m.target == ZipTarget::Mods     ? Sym::kExtension
                                     : m.target == ZipTarget::Cheats ? Sym::kCode // AzaharFrame ui2
                                                                     : Sym::kTexture;
            for (std::size_t i = 0; i < m.entries.size(); i++) {
                const ZipEntry& e = m.entries[i];
                rows.push_back({Id::ZipEntry, static_cast<int>(i), e.name, e.detail,
                                e.folder ? Sym::kFolderOpen : archive});
            }
        }
    }
    const Rect list{area.x0, top, area.x1, bottom};
    if (!choosing && !m.browse) {
        // Flat: Qt's dialog picked the file; only a failure is left to show
        EmptyState(kit, {c.x0, top, c.x1, bottom}, Sym::kInfo, false, 0,
                   m.error.empty() ? title : m.error, m.file);
    } else if (m.browse && m.loading) {
        EmptyState(kit, {c.x0, top, c.x1, bottom}, 0, true, m.spin, "Loading", "");
    } else if (m.browse && rows.empty()) {
        EmptyState(kit, {c.x0, top, c.x1, bottom}, Sym::kFolderOpen, false, 0,
                   m.target == ZipTarget::Cheats ? "No Cheat or Zip Files" : "No Zip Files", "");
    } else {
        std::vector<float> heights(rows.size(), kRow1);
        const ListScroll s = Scroll(heights, list.H(), ScrollWanted(view));
        layout.max_scroll = s.max_scroll;
        layout.scroll_step = s.step;
        kit.Clip(list);
        for (std::size_t i = 0; i < rows.size(); i++) {
            const float y = top + s.tops[i] - s.scroll;
            if (y + kRow1 <= list.y0 || y + kRow1 > list.y1 + 0.01f) {
                continue; // whole rows only
            }
            const Row& r = rows[i];
            const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + kRow1};
            const Target t{r.id, r.index};
            kit.StateLayer(row, 16.0f, Tok::kOnSurface, StateOf(view, t));
            AddHit(row, t, list);
            float tx = c.x0;
            if (r.icon != 0) {
                kit.Icon(c.x0 + 12.0f, row.Cy(), 24.0f, r.icon, false, Tok::kOnSurfaceVariant);
                tx = c.x0 + kLeading;
            }
            float right = c.x1;
            if (!r.detail.empty()) {
                const float dw = kit.TextWidthSized(kSmall, false, r.detail);
                kit.TextSized(c.x1, row.Cy() + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall, false,
                              Tok::kOnSurfaceVariant, r.detail, 2);
                right = c.x1 - dw - kNameGap;
            }
            kit.TextMid(tx, row.Cy(), Type::Body, Tok::kOnSurface, kit.Fit(Type::Body, r.name, right - tx));
        }
        kit.ClearClip();
        ScrollIndicator(kit, list, area.x1, s);
    }
    DrawModsDialog(kit, m.dialog, view);
}

// ---------------------------------------------------------------------------------------------
// AzaharFrame texparts: a pack's Customize page
//   <-  4K                                                 All On  All Off
//   Created by Henriko Magnifico · 6.0.0 · 2,034 Textures On · 5.0 GB
//   (i) Texture Conflicts: Parts Higher in the List Take Priority
//   [x] [img] Effects                                           ^   v
//             52 Textures · 73 MB · Conflicts with Environment
//   Fonts                                                  (a choice's title)
//   ( ) Off
//   (o) [img] Original N64

namespace {

constexpr float kSubhead = 40.0f; ///< a choice's title over its rows
constexpr float kThumb = 40.0f;   ///< a part's preview
constexpr float kIndent = 32.0f;  ///< a part in a folder
constexpr float kIconBtn = 48.0f;

/// Material 3 checkbox: an 18 dp box, 2 dp outline, 2 dp corners; checked or mixed (a folder with
/// some parts on) filled with the primary colour, the mark in on-primary.
void Checkbox(Kit& kit, float cx, float cy, int value, bool enabled) {
    const Rect b{cx - 9.0f, cy - 9.0f, cx + 9.0f, cy + 9.0f};
    const float op = enabled ? 1.0f : kDisabledContent;
    if (value == 0) {
        kit.Stroke(b, 2.0f, 2.0f, enabled ? Tok::kOnSurfaceVariant : Tok::kOnSurface, op);
        return;
    }
    kit.Fill(b, 2.0f, enabled ? Tok::kPrimary : Tok::kOnSurface, op);
    const uint32_t mark = enabled ? Tok::kOnPrimary : Tok::kSurface;
    if (value == 1) {
        kit.Line(cx - 5.0f, cy, cx + 5.0f, cy, 2.0f, mark);
    } else {
        kit.Line(cx - 5.0f, cy + 0.5f, cx - 1.5f, cy + 4.0f, 2.0f, mark);
        kit.Line(cx - 1.5f, cy + 4.0f, cx + 5.5f, cy - 3.5f, 2.0f, mark);
    }
}

/// arrow_upward / arrow_downward (not in the menu's icon font) in a standard icon button.
void ArrowButton(Kit& kit, float cx, float cy, bool up, bool enabled, State st) {
    if (enabled && st != State::Rest) {
        kit.Dot(cx, cy, 20.0f, Tok::kOnSurfaceVariant,
                st == State::Pressed ? kPressedLayer : kHoverLayer);
    }
    const float op = enabled ? 1.0f : kDisabledContent;
    const uint32_t ink = enabled ? Tok::kOnSurfaceVariant : Tok::kOnSurface;
    const float d = up ? -1.0f : 1.0f;
    kit.Line(cx, cy - 7.0f * d, cx, cy + 7.0f * d, 2.0f, ink, op);
    kit.Line(cx, cy + 7.5f * d, cx - 6.0f, cy + 1.5f * d, 2.0f, ink, op);
    kit.Line(cx, cy + 7.5f * d, cx + 6.0f, cy + 1.5f * d, 2.0f, ink, op);
}

float PartRowHeight(const TexturePartRow& r) {
    switch (r.kind) {
    case PartRowKind::ChoiceTitle:
        return r.arrows ? kRow1 : kSubhead; // the order buttons' 48 dp targets
    case PartRowKind::ChoiceOff:
        return kRow1;
    default:
        return r.detail.empty() ? kRow1 : kRow2;
    }
}

} // namespace

void TabMenu::DrawTextureParts(Kit& kit, const TexturePartsModel& m, const MenuView& view,
                               const Rect& area, float bottom) {
    const Rect c{area.x0 + kPad, area.y0 + kTop, area.x1 - kPad, area.y1 - kPad};
    // The top bar: back, the pack's name, All On and All Off.
    const Rect back{c.x0 - 12.0f, c.y0, c.x0 + 36.0f, c.y0 + kBarH};
    kit.StateLayer(back, 24.0f, Tok::kOnSurfaceVariant, StateOf(view, {Id::PartsBack}));
    kit.Icon(back.Cx(), back.Cy(), 24.0f, Sym::kArrowBack, false, Tok::kOnSurface);
    AddHit(back, {Id::PartsBack}, area);
    float title_right = c.x1;
    if (m.all_buttons && !m.loading) {
        float right = c.x1 + 12.0f; // a text button's padding lines its label up with the edge
        for (const Id id : {Id::PartsAllOff, Id::PartsAllOn}) {
            const std::string label = LabelFor(id);
            const float w = ButtonWidth(kit, label, false, 12.0f);
            const Rect b{right - w, back.Cy() - kButtonH * 0.5f, right,
                         back.Cy() + kButtonH * 0.5f};
            kit.Button(b, ButtonStyle::Text, 0, label, StateOf(view, {id}));
            AddHit({b.x0, back.y0, b.x1, back.y1}, {id}, area);
            right = b.x0 - 8.0f;
        }
        title_right = right - 8.0f;
    }
    kit.TextMid(back.x1 + 12.0f, back.Cy(), Type::Title, Tok::kOnSurface,
                kit.Fit(Type::Title, m.title, title_right - back.x1 - 12.0f));
    float y = c.y0 + kBarH;
    if (!m.author.empty() || !m.rest.empty()) {
        CreatedBy(kit, c.x0, c.x1, y + 20.0f - 13.0f - kit.CapHeightSized(kSmall, false) * 0.5f,
                  m.author, false, State::Rest, m.rest, false, 1.0f);
        y += 32.0f;
    }
    y = DrawOptimizeRow(kit, m.optimize, view, area, c.x0, c.x1, y); // AzaharFrame ui3
    if (m.loading) {
        EmptyState(kit, {c.x0, y, c.x1, bottom}, Sym::kTexture, true, 0, "Reading the Pack", "");
        return;
    }
    if (m.note) {
        kit.Icon(c.x0 + 10.0f, y + 14.0f, 20.0f, Sym::kInfo, false, Tok::kOnSurfaceVariant);
        const std::string text = "Texture Conflicts: Parts Higher in the List Take Priority";
        kit.TextSized(c.x0 + 30.0f, y + 14.0f + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall,
                      false, Tok::kOnSurfaceVariant, kit.Fit(Type::Tooltip, text, c.W() - 30.0f));
        y += 32.0f;
    }
    const float top = y + 4.0f;
    const Rect list{area.x0, top, area.x1, bottom};
    std::vector<float> heights;
    for (const TexturePartRow& r : m.rows) {
        heights.push_back(PartRowHeight(r));
    }
    const ListScroll s = Scroll(heights, list.H(), ScrollWanted(view));
    layout.max_scroll = s.max_scroll;
    layout.scroll_step = s.step;
    struct Tip {
        float cx = 0.0f, y = 0.0f;
        std::string text;
        bool above = true;
    } tip;
    kit.Clip(list);
    for (std::size_t i = 0; i < m.rows.size(); i++) {
        const TexturePartRow& r = m.rows[i];
        const float h = heights[i];
        const float ry = top + s.tops[i] - s.scroll;
        if (ry < list.y0 - 0.01f || ry + h > list.y1 + 0.01f) {
            continue; // whole rows only
        }
        const int idx = static_cast<int>(i);
        const Rect row{c.x0 - 12.0f, ry, c.x1 + 12.0f, ry + h};
        const float cy = row.Cy();
        const bool title = r.kind == PartRowKind::ChoiceTitle;
        if (title) {
            // Material list subheader: the choice's name.
            const float w = c.W() - (r.arrows ? 2.0f * kIconBtn : 0.0f);
            kit.TextSized(c.x0, ry + h - 12.0f, kSmall, true, Tok::kPrimary,
                          kit.Fit(Type::Tooltip, r.name, w));
            if (!r.arrows) {
                continue;
            }
        } else {
            const Target t{Id::TexturePart, idx};
            const State st = StateOf(view, t);
            kit.StateLayer(row, 16.0f, Tok::kOnSurface, st);
            AddHit(row, t, list);
        }
        const float indent = r.child ? kIndent : 0.0f;
        const float lead = c.x0 + 12.0f + indent;
        const bool radio = r.kind == PartRowKind::Choice || r.kind == PartRowKind::ChoiceOff;
        if (radio) {
            Radio(kit, lead, cy, r.value == 2, true);
        } else if (!title) {
            Checkbox(kit, lead, cy, r.value, true);
        }
        float tx = c.x0 + kLeading + indent;
        if (!title && r.kind != PartRowKind::ChoiceOff) {
            const Rect th{tx, cy - kThumb * 0.5f, tx + kThumb, cy + kThumb * 0.5f};
            kit.Fill(th, 8.0f, Tok::kSurfaceContainerHighest);
            if (r.preview && r.preview_px > 0 &&
                r.preview->size() >= static_cast<std::size_t>(r.preview_px) * r.preview_px * 4) {
                kit.Image(r.preview->data(), r.preview_px, r.preview_px, th);
            } else {
                kit.Icon(th.Cx(), th.Cy(), 24.0f,
                         r.kind == PartRowKind::Folder ? Sym::kFolderOpen : Sym::kTexture, false,
                         Tok::kOnSurfaceVariant);
            }
            tx = th.x1 + 16.0f;
        }
        float right = c.x1;
        if (r.arrows) {
            const float down_cx = c.x1 - kIconBtn * 0.5f + 12.0f;
            const float up_cx = down_cx - kIconBtn;
            for (const bool up : {true, false}) {
                const float bx = up ? up_cx : down_cx;
                const Target at{up ? Id::PartUp : Id::PartDown, idx};
                const bool can = up ? r.up : r.down;
                ArrowButton(kit, bx, cy, up, can, StateOf(view, at, can));
                if (can) {
                    AddHit({bx - kIconBtn * 0.5f, cy - 24.0f, bx + kIconBtn * 0.5f, cy + 24.0f}, at,
                           list);
                }
                if (view.tooltip == at.id && view.tooltip_index == idx) {
                    // Above, unless that covers the row above's buttons or leaves the list: then
                    // below (never over another row's order buttons).
                    const bool prev_arrows = i > 0 && m.rows[i - 1].arrows;
                    const bool first = ry - kRow1 < list.y0;
                    const bool next_arrows = i + 1 < m.rows.size() && m.rows[i + 1].arrows;
                    const bool above = !(prev_arrows || first) || next_arrows;
                    tip = {bx, above ? row.y0 + 4.0f : row.y1 - 4.0f,
                           up ? "Move Up" : "Move Down", above};
                }
            }
            right = up_cx - kIconBtn * 0.5f - 8.0f;
        }
        if (!title) {
            RowText(kit, tx, right, cy, r.name, false, r.detail, false, 1.0f);
        }
    }
    kit.ClearClip();
    ScrollIndicator(kit, list, area.x1, s);
    if (!tip.text.empty()) {
        const float half = (kit.TextWidth(Type::Tooltip, tip.text) + 20.0f) * 0.5f;
        kit.Tooltip(std::min(tip.cx, area.x1 - 8.0f - half), tip.y, tip.text, tip.above);
    }
}

} // namespace FrameXr::Menu
