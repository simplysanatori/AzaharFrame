// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the in-game menu's Cheats tab (Material 3 list on the pane).
//
//   [Download]                                     [Azahar's Window]   top bar
//   12 cheats added, 3 for another version                             status (after a download)
//   Made for Azahar for Frame                                          subheader
//   * No Bloom                                           [Install]     a game fix
//   * No Bloom (USA)                                         (o )      a cheat
//   Sharkive
//     Max EXP                                         [v1.0] (o )      made for another version
//   Your Cheats
//     ...
//
// With nothing in the list the pane shows one centred state instead: No Cheats (with Download),
// Downloading (a progress ring) or Download Failed (with Try Again).

#include "frame_xr/menu/cheats_tab.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;

namespace {

constexpr float kPad = 24.0f;       ///< inside the pane, as the other tabs
constexpr float kBarH = 48.0f;      ///< the top bar's buttons
constexpr float kStatusH = 24.0f;   ///< the status line under the bar
constexpr float kItemH = 56.0f;     ///< a list item: Material's one-line item
constexpr float kSubheadH = 40.0f;  ///< a section's subheader
constexpr float kSwitchW = 52.0f;
constexpr float kTrailGap = 16.0f;  ///< between the trailing chip and the switch
constexpr float kNameGap = 24.0f;   ///< between the name and what trails it
constexpr float kChipH = 32.0f;
constexpr float kChipPad = 12.0f;
constexpr float kFixH = 40.0f;      ///< the Install button (48 dp to the beam)
constexpr float kSmall = 14.0f;     ///< Body Small, one step up as the menu's type scale
constexpr float kLeading = 40.0f;   ///< a leading icon (24) and its gap (16)
constexpr float kPi = 3.14159265f;

std::string SectionTitle(CheatSource s, CheatBadge badge) {
    switch (s) {
    case CheatSource::Developer:
        return badge == CheatBadge::Star ? "Made for Azahar for Frame" : "Azahar for Frame";
    case CheatSource::Sharkive:
        return "Sharkive";
    case CheatSource::User:
        return "Your Cheats";
    }
    return {};
}

struct Item {
    enum class Kind { Subhead, Fix, Cheat } kind;
    CheatSource source;
    int index = -1;
};

std::vector<Item> ItemsFor(const CheatsModel& m) {
    std::vector<Item> items;
    for (const CheatSource s : {CheatSource::Developer, CheatSource::Sharkive, CheatSource::User}) {
        const std::size_t first = items.size();
        items.push_back({Item::Kind::Subhead, s});
        if (s == CheatSource::Developer && m.fix != FixState::None) {
            items.push_back({Item::Kind::Fix, s});
        }
        for (int i = 0; i < static_cast<int>(m.cheats.size()); i++) {
            if (m.cheats[static_cast<std::size_t>(i)].source == s) {
                items.push_back({Item::Kind::Cheat, s, i});
            }
        }
        if (items.size() == first + 1) {
            items.pop_back(); // an empty section has no subheader
        }
    }
    return items;
}

float HeightOf(const Item& item) {
    return item.kind == Item::Kind::Subhead ? kSubheadH : kItemH;
}

/// The developer's badge, centred on (cx, cy).
void Badge(Kit& kit, float cx, float cy, CheatBadge badge, float size) {
    kit.Icon(cx, cy, size, badge == CheatBadge::Star ? Sym::kStar : Sym::kVerified, true,
             Tok::kPrimary);
}

/// An arc of a circle (angles in radians, 0 = up, clockwise) as short round-capped strokes.
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

/// Material 3 (2024) determinate circular progress: 48 dp, a 4 dp stroke, the active arc from
/// the top, a gap, the track.
void ProgressRing(Kit& kit, float cx, float cy, float t) {
    const float stroke = 4.0f, r = 22.0f;
    const float gap = (stroke + 4.0f) / r; // the gap and the caps, in radians
    const float a = std::clamp(t, 0.0f, 1.0f) * 2.0f * kPi;
    if (a > 0.0f) {
        Arc(kit, cx, cy, r, 0.0f, a, stroke, Tok::kPrimary);
    }
    const float t0 = a > 0.0f ? a + gap : 0.0f;
    const float t1 = 2.0f * kPi - (a > 0.0f ? gap : 0.0f);
    Arc(kit, cx, cy, r, t0, t1, stroke, Tok::kSecondaryContainer);
}

/// Material 3 (2024) determinate linear progress: a 4 dp track, the active part, a 4 dp gap,
/// the rest of the track with a stop dot at its end.
void ProgressBar(Kit& kit, float x0, float x1, float cy, float t) {
    const float h = 4.0f, gap = 4.0f;
    const float ax = x0 + (x1 - x0) * std::clamp(t, 0.0f, 1.0f);
    if (ax > x0 + h) {
        kit.Fill({x0, cy - h * 0.5f, ax, cy + h * 0.5f}, h * 0.5f, Tok::kPrimary);
    }
    if (x1 - ax > gap + h) {
        kit.Fill({ax + gap, cy - h * 0.5f, x1, cy + h * 0.5f}, h * 0.5f, Tok::kSecondaryContainer);
        kit.Dot(x1 - h * 0.5f, cy, h * 0.5f, Tok::kPrimary);
    }
}

/// A button's width for its label and icon (Material: 24 dp each side, icon 20 and gap 8).
float ButtonWidth(const Kit& kit, const std::string& label, bool icon, float side = 24.0f) {
    return kit.TextWidth(Type::Label, label) + (icon ? 28.0f : 0.0f) + side * 2.0f;
}

} // namespace

std::string VersionChip(const std::string& note) {
    // "for v1.0, this game is v1.1" / "for Rev0, this cartridge is Rev1": the word after "for".
    std::size_t at = note.rfind("for ", 0) == 0 ? 4 : 0;
    const std::size_t end = note.find_first_of(", ", at);
    std::string v = note.substr(at, end == std::string::npos ? std::string::npos : end - at);
    if (v.size() > 8) {
        v = v.substr(0, 8); // a chip, not a sentence
    }
    return v;
}

void TabMenu::DrawCheats(Kit& kit, const MenuModel& model, const MenuView& view,
                         const Rect& pane) {
    const CheatsModel& m = model.cheats;
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    const std::vector<Item> items = ItemsFor(m);
    const bool downloading = m.download == DownloadState::Downloading;
    const bool failed = m.download == DownloadState::Failed;

    // ---- Top bar: Download on the left (with a list), Azahar's Window on the right ----
    {
        const std::string label = LabelFor(Id::CheatsWindow);
        const float w = ButtonWidth(kit, label, true, 16.0f);
        const Rect win{c.x1 - w, c.y0, c.x1, c.y0 + kBarH};
        kit.Button(win, ButtonStyle::Text, Sym::kOpenInNew, label, StateOf(view, {Id::CheatsWindow}));
        AddHit(win, {Id::CheatsWindow}, pane);
    }
    if (items.empty()) {
        // ---- One centred state: No Cheats, Downloading or Download Failed ----
        const Rect area{c.x0, c.y0 + kBarH, c.x1, c.y1};
        const float icon = 48.0f, title_h = 24.0f, small_h = 20.0f, button_h = 48.0f;
        const bool with_status = !downloading && !m.status.empty();
        const bool with_button = !downloading;
        const float h = icon + 16.0f + title_h + (with_status ? 8.0f + small_h : 0.0f) +
                        (with_button ? 24.0f + button_h : 0.0f);
        float y = area.Cy() - h * 0.5f;
        if (downloading) {
            ProgressRing(kit, area.Cx(), y + icon * 0.5f, static_cast<float>(m.progress) / 100.0f);
        } else {
            kit.Icon(area.Cx(), y + icon * 0.5f, icon, failed ? Sym::kCloudOff : Sym::kCode, false,
                     failed ? Tok::kError : Tok::kOnSurfaceVariant);
        }
        y += icon + 16.0f;
        const char* title = downloading ? "Downloading Cheats" : failed ? "Download Failed" : "No Cheats";
        kit.TextMid(area.Cx(), y + title_h * 0.5f, Type::Title, Tok::kOnSurface, title, 1);
        y += title_h;
        if (with_status) {
            y += 8.0f;
            kit.TextSized(area.Cx(), y + 15.0f, kSmall, false, Tok::kOnSurfaceVariant,
                          kit.Fit(Type::Tooltip, m.status, area.W()), 1);
            y += small_h;
        }
        if (with_button) {
            y += 24.0f;
            const std::string label = failed ? "Try Again" : LabelFor(Id::DownloadCheats);
            const float w = ButtonWidth(kit, label, true);
            const Rect b{area.Cx() - w * 0.5f, y, area.Cx() + w * 0.5f, y + button_h};
            kit.Button(b, ButtonStyle::Filled, failed ? Sym::kRefresh : Sym::kCloudDownload, label,
                       StateOf(view, {Id::DownloadCheats}));
            AddHit(b, {Id::DownloadCheats}, pane);
        }
        return;
    }
    {
        const std::string label = LabelFor(Id::DownloadCheats);
        const Rect dl{c.x0, c.y0, c.x0 + ButtonWidth(kit, label, true), c.y0 + kBarH};
        kit.Button(dl, ButtonStyle::Tonal, Sym::kCloudDownload, label,
                   StateOf(view, {Id::DownloadCheats}, !downloading));
        if (!downloading) {
            AddHit(dl, {Id::DownloadCheats}, pane);
        }
    }
    float top = c.y0 + kBarH + 8.0f;
    if (downloading || !m.status.empty()) {
        // ---- The status line: progress while downloading, then what the download did ----
        const float cy = top + kStatusH * 0.5f;
        if (downloading) {
            ProgressBar(kit, c.x0, c.x1, cy, static_cast<float>(m.progress) / 100.0f);
        } else {
            kit.TextSized(c.x0, cy + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall, false,
                          failed ? Tok::kError : Tok::kOnSurfaceVariant,
                          kit.Fit(Type::Tooltip, m.status, c.W()));
        }
        top += kStatusH;
    }

    // ---- The list ----
    const Rect list{pane.x0, top, pane.x1, pane.y1 - 8.0f};
    // Scrolled to item edges, as the Settings tab to whole rows: nothing shows cut at the top,
    // and at the end the last item shows whole.
    std::vector<float> tops;
    float content_h = 0.0f;
    for (const Item& it : items) {
        tops.push_back(content_h);
        content_h += HeightOf(it);
    }
    const float raw_max = std::max(0.0f, content_h - list.H());
    float max_scroll = 0.0f;
    if (raw_max > 0.0f) {
        max_scroll = content_h;
        for (const float t : tops) {
            if (t >= raw_max) {
                max_scroll = t;
                break;
            }
        }
    }
    layout.max_scroll = max_scroll;
    float scroll = 0.0f;
    const float want = std::clamp(view.scroll[static_cast<int>(Tab::Cheats)], 0.0f, max_scroll);
    for (const float t : tops) {
        if (t <= want + 0.01f) {
            scroll = t;
        }
    }
    kit.Clip(list);
    float y = top - scroll;
    for (const Item& it : items) {
        const float ih = HeightOf(it);
        if (y + ih <= list.y0 || y >= list.y1) {
            y += ih;
            continue;
        }
        const bool dev = it.source == CheatSource::Developer;
        const bool star = dev && m.badge == CheatBadge::Star;
        const bool verified = dev && m.badge == CheatBadge::Verified;
        if (it.kind == Item::Kind::Subhead) {
            // Title Small in the primary colour, as the Stats tab's groups, near its rows.
            const float cy = y + ih - 14.0f;
            kit.TextMid(c.x0, cy, Type::Label, Tok::kPrimary, SectionTitle(it.source, m.badge));
            y += ih;
            continue;
        }
        const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + ih};
        const float cy = row.Cy();
        const float tx = star ? c.x0 + kLeading : c.x0;
        if (star) {
            Badge(kit, c.x0 + 12.0f, cy, m.badge, 24.0f);
        }
        float right; // where the name must end
        if (it.kind == Item::Kind::Fix) {
            const Target t{Id::CheatFix};
            const bool installed = m.fix == FixState::Installed;
            const bool other = m.fix == FixState::OtherPatch; // the player's own code.ips
            AddHit(row, {Id::CheatFix, kLabelIndex}, list);
            // A toggle button: Install (tonal), Installed (filled, selected); one width for both.
            // With another code.ips in the mods folder it is disabled and says so.
            const char* label = installed ? "Installed" : other ? "Other Patch" : "Install";
            const float w = std::max({ButtonWidth(kit, "Install", true),
                                      ButtonWidth(kit, "Installed", true),
                                      ButtonWidth(kit, "Other Patch", false)});
            const Rect b{c.x1 - w, cy - kFixH * 0.5f, c.x1, cy + kFixH * 0.5f};
            kit.Button(b, installed ? ButtonStyle::Filled : ButtonStyle::Tonal,
                       installed ? Sym::kCheck : other ? 0 : Sym::kDownload, label,
                       StateOf(view, t, !other));
            if (!other) {
                AddHit({b.x0, cy - 24.0f, b.x1, cy + 24.0f}, t, list);
            }
            right = b.x0 - kNameGap;
            const float nw = right - tx - (verified ? 28.0f : 0.0f);
            const float drawn = kit.TextMid(tx, cy, Type::Body, Tok::kOnSurface,
                                            kit.Fit(Type::Body, "Game Fix", nw));
            if (verified) {
                Badge(kit, tx + drawn + 6.0f + 10.0f, cy, m.badge, 20.0f);
            }
        } else {
            const CheatItem& cheat = m.cheats[static_cast<std::size_t>(it.index)];
            const Target t{Id::Cheat, it.index};
            const State st = StateOf(view, t);
            kit.StateLayer(row, 16.0f, Tok::kOnSurface, st);
            AddHit(row, t, list);
            kit.Switch({c.x1 - kSwitchW, row.y0, c.x1, row.y1}, cheat.enabled, st);
            right = c.x1 - kSwitchW - kNameGap;
            const std::string version = VersionChip(cheat.version_note);
            if (!version.empty()) {
                // An assist chip (Material's outlined style) in the error colour: made for
                // another version of the game.
                const float cw = kit.TextWidth(Type::Label, version) + kChipPad * 2.0f;
                const Rect chip{c.x1 - kSwitchW - kTrailGap - cw, cy - kChipH * 0.5f,
                                c.x1 - kSwitchW - kTrailGap, cy + kChipH * 0.5f};
                kit.Stroke(chip, 8.0f, 1.0f, Tok::kError);
                kit.TextMid(chip.Cx(), cy, Type::Label, Tok::kError, version, 1);
                right = chip.x0 - kNameGap;
            }
            const float nw = right - tx - (verified ? 28.0f : 0.0f);
            const float drawn = kit.TextMid(tx, cy, Type::Body, Tok::kOnSurface,
                                            kit.Fit(Type::Body, cheat.name, nw));
            if (verified) {
                Badge(kit, tx + drawn + 6.0f + 10.0f, cy, m.badge, 20.0f);
            }
        }
        y += ih;
    }
    kit.ClearClip();
    if (max_scroll > 0.0f) {
        // The scroll indicator, as the other long tabs.
        const float track_h = list.H() - 16.0f;
        const float thumb_h = std::max(40.0f, track_h * list.H() / content_h);
        const float ty = list.y0 + 8.0f + (track_h - thumb_h) * (scroll / max_scroll);
        kit.Fill({pane.x1 - 10.0f, ty, pane.x1 - 6.0f, ty + thumb_h}, 2.0f, Tok::kOnSurfaceVariant,
                 0.38f);
    }
}

} // namespace FrameXr::Menu
