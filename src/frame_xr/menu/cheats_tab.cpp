// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the Mods tab's Cheats segment (cheats_tab.h), Material 3 on the pane:
//
//   [Download]                                     Azahar's Window     top bar
//   12 cheats added, 3 for another version                             status (after a download)
//   No Bloom (USA) (v)                                       ( o)      one flat list, 48 dp rows
//   Infinite HP in Battle                                    (o )
//   Max EXP After Battle                              [v1.0] (o )      made for another version
//   Max Money                                                (o )      the player's own
//
// With nothing in the list the segment shows one centred state instead: No Cheats (with
// Download), Downloading (a progress ring) or Download Failed (the reason, Try Again).

#include "frame_xr/menu/cheats_tab.h"

#include <algorithm>
#include <cmath>

#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;

namespace {

constexpr float kPad = 24.0f;     ///< inside the pane, as the other tabs
constexpr float kBarH = 48.0f;    ///< the top bar's buttons
constexpr float kStatusH = 24.0f; ///< the status line under the bar
constexpr float kItemH = 48.0f;   ///< a list item (48 dp, the minimum touch target)
constexpr float kSwitchW = 52.0f;
constexpr float kTrailGap = 16.0f; ///< between the trailing chip and the switch
constexpr float kNameGap = 24.0f;  ///< between the name and what trails it
constexpr float kChipH = 32.0f;
constexpr float kChipPad = 12.0f;
constexpr float kBadge = 20.0f; ///< the verified badge after a developer name
constexpr float kSmall = 14.0f; ///< Body Small, one step up as the menu's type scale
constexpr float kPi = 3.14159265f;

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

/// Material 3 (2024) indeterminate circular progress, 48 dp with a 4 dp stroke: an arc that
/// turns and grows and shrinks over the track, with gaps. `spin` steps 15 a second.
void ProgressRing(Kit& kit, float cx, float cy, int spin) {
    const float stroke = 4.0f, r = 22.0f;
    const float gap = (stroke + 4.0f) / r; // the gap and the caps, in radians
    const float t = static_cast<float>(spin) / 15.0f;
    const float start = std::fmod(t * 1.6f * kPi, 2.0f * kPi);
    const float len = kPi * (0.35f + 0.55f * (0.5f - 0.5f * std::cos(t * 2.0f * kPi / 1.5f)));
    Arc(kit, cx, cy, r, start, start + len, stroke, Tok::kPrimary);
    Arc(kit, cx, cy, r, start + len + gap, start + 2.0f * kPi - gap, stroke,
        Tok::kSecondaryContainer);
}

/// Material 3 (2024) indeterminate linear progress: a 4 dp track and an active segment that
/// runs along it, a 4 dp gap each side.
void ProgressBar(Kit& kit, float x0, float x1, float cy, int spin) {
    const float h = 4.0f, gap = 4.0f, w = x1 - x0;
    const float t = std::fmod(static_cast<float>(spin) / 15.0f / 1.8f, 1.0f); // 1.8 s a sweep
    const float a0 = x0 + w * std::clamp(t * 1.4f - 0.4f, 0.0f, 1.0f);
    const float a1 = x0 + w * std::clamp(t * 1.4f, 0.0f, 1.0f);
    const auto track = [&](float s0, float s1) {
        if (s1 - s0 > h) {
            kit.Fill({s0, cy - h * 0.5f, s1, cy + h * 0.5f}, h * 0.5f, Tok::kSecondaryContainer);
        }
    };
    track(x0, a0 - gap);
    if (a1 - a0 > h) {
        kit.Fill({a0, cy - h * 0.5f, a1, cy + h * 0.5f}, h * 0.5f, Tok::kPrimary);
    }
    track(a1 + gap, x1);
}

/// A button's width for its label and icon (Material: 24 dp each side, icon 20 and gap 8).
float ButtonWidth(const Kit& kit, const std::string& label, bool icon, float side = 24.0f) {
    return kit.TextWidth(Type::Label, label) + (icon ? 28.0f : 0.0f) + side * 2.0f;
}

} // namespace

std::string VersionChip(const std::string& note) {
    // "for v1.0, this game is v1.1" / "for Rev0, this cartridge is Rev1": the word after "for".
    const std::size_t at = note.rfind("for ", 0) == 0 ? 4 : 0;
    const std::size_t end = note.find_first_of(", ", at);
    std::string v = note.substr(at, end == std::string::npos ? std::string::npos : end - at);
    if (v.size() > 8) {
        v = v.substr(0, 8); // a chip, not a sentence
    }
    return v;
}

std::string FailureReason(const std::string& error) {
    // "Download failed: No reply" gives "No reply", then a capital first letter as a sentence.
    std::string why = error;
    for (const char* prefix : {"Download failed: ", "Download Failed: "}) {
        if (why.rfind(prefix, 0) == 0) {
            why = why.substr(std::char_traits<char>::length(prefix));
        }
    }
    if (!why.empty() && why[0] >= 'a' && why[0] <= 'z') {
        why[0] = static_cast<char>(why[0] - 'a' + 'A');
    }
    return why;
}

void TabMenu::DrawCheats(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area) {
    const CheatsModel& m = model.mods.cheats;
    const Rect c{area.x0 + kPad, area.y0, area.x1 - kPad, area.y1 - kPad};
    const bool downloading = m.download == DownloadState::Downloading;
    const bool failed = m.download == DownloadState::Failed;

    // ---- Top bar: Download on the left (with a list), Azahar's Window on the right ----
    {
        const std::string label = LabelFor(Id::CheatsWindow);
        const float w = ButtonWidth(kit, label, true, 16.0f);
        const Rect win{c.x1 - w, c.y0, c.x1, c.y0 + kBarH};
        kit.Button(win, ButtonStyle::Text, Sym::kOpenInNew, label,
                   StateOf(view, {Id::CheatsWindow}));
        AddHit(win, {Id::CheatsWindow}, area);
    }
    if (m.cheats.empty()) {
        // ---- One centred state: No Cheats, Downloading or Download Failed ----
        const Rect mid{c.x0, c.y0 + kBarH, c.x1, c.y1};
        const float icon = 48.0f, title_h = 24.0f, small_h = 20.0f, button_h = 48.0f;
        // After a download that found nothing the title says so; a failure says why under it.
        const bool none_found = m.download == DownloadState::Done;
        const bool with_status = failed && !m.status.empty();
        const bool with_button = !downloading;
        const float h = icon + 16.0f + title_h + (with_status ? 8.0f + small_h : 0.0f) +
                        (with_button ? 24.0f + button_h : 0.0f);
        float y = mid.Cy() - h * 0.5f;
        if (downloading) {
            ProgressRing(kit, mid.Cx(), y + icon * 0.5f, m.spin);
        } else {
            kit.Icon(mid.Cx(), y + icon * 0.5f, icon, failed ? Sym::kCloudOff : Sym::kCode, false,
                     failed ? Tok::kError : Tok::kOnSurfaceVariant);
        }
        y += icon + 16.0f;
        const char* title = downloading  ? "Downloading Cheats"
                            : failed     ? "Download Failed"
                            : none_found ? "No Cheats for This Game"
                                         : "No Cheats";
        kit.TextMid(mid.Cx(), y + title_h * 0.5f, Type::Title, Tok::kOnSurface, title, 1);
        y += title_h;
        if (with_status) {
            y += 8.0f;
            kit.TextSized(mid.Cx(), y + 15.0f, kSmall, false, Tok::kOnSurfaceVariant,
                          kit.Fit(Type::Tooltip, FailureReason(m.status), mid.W()), 1);
            y += small_h;
        }
        if (with_button) {
            y += 24.0f;
            const std::string label = failed ? "Try Again" : LabelFor(Id::DownloadCheats);
            const float w = ButtonWidth(kit, label, true);
            const Rect b{mid.Cx() - w * 0.5f, y, mid.Cx() + w * 0.5f, y + button_h};
            kit.Button(b, ButtonStyle::Filled, failed ? Sym::kRefresh : Sym::kCloudDownload, label,
                       StateOf(view, {Id::DownloadCheats}));
            AddHit(b, {Id::DownloadCheats}, area);
        }
        layout.max_scroll = 0.0f;
        return;
    }
    {
        const std::string label = LabelFor(Id::DownloadCheats);
        const Rect dl{c.x0, c.y0, c.x0 + ButtonWidth(kit, label, true), c.y0 + kBarH};
        kit.Button(dl, ButtonStyle::Tonal, Sym::kCloudDownload, label,
                   StateOf(view, {Id::DownloadCheats}, !downloading));
        if (!downloading) {
            AddHit(dl, {Id::DownloadCheats}, area);
        }
    }
    float top = c.y0 + kBarH + 8.0f;
    if (downloading || !m.status.empty()) {
        // ---- The status line: progress while downloading, then what the download did ----
        const float cy = top + kStatusH * 0.5f;
        if (downloading) {
            ProgressBar(kit, c.x0, c.x1, cy, m.spin);
        } else {
            const std::string text =
                failed ? "Download Failed: " + FailureReason(m.status) : m.status;
            kit.TextSized(c.x0, cy + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall, false,
                          failed ? Tok::kError : Tok::kOnSurfaceVariant,
                          kit.Fit(Type::Tooltip, text, c.W()));
        }
        top += kStatusH + 4.0f;
    }

    // ---- The list: one flat list in file order, scrolled by the beam or the stick ----
    const Rect list{area.x0, top, area.x1, area.y1 - 8.0f};
    const float content_h = kItemH * static_cast<float>(m.cheats.size());
    // In whole rows, as the Settings tab: at the end the top row shows whole.
    const float max_scroll = std::max(0.0f, std::ceil((content_h - list.H()) / kItemH) * kItemH);
    layout.max_scroll = max_scroll;
    layout.scroll_step = kItemH;
    const float scroll = std::clamp(view.scroll[static_cast<int>(Tab::Mods)], 0.0f, max_scroll);
    kit.Clip(list);
    for (int i = 0; i < static_cast<int>(m.cheats.size()); i++) {
        const float y = list.y0 - scroll + kItemH * static_cast<float>(i);
        if (y + kItemH <= list.y0 || y >= list.y1) {
            continue;
        }
        const CheatItem& cheat = m.cheats[static_cast<std::size_t>(i)];
        const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + kItemH};
        const float cy = row.Cy();
        const Target t{Id::Cheat, i};
        const State st = StateOf(view, t);
        kit.StateLayer(row, 12.0f, Tok::kOnSurface, st);
        AddHit(row, t, list);
        kit.Switch({c.x1 - kSwitchW, row.y0, c.x1, row.y1}, cheat.enabled, st);
        float right = c.x1 - kSwitchW - kNameGap; // where the name must end
        const std::string version = VersionChip(cheat.version_note);
        if (!version.empty()) {
            // Material's outlined chip in the error colour: made for another version of the game.
            const float cw = kit.TextWidth(Type::Label, version) + kChipPad * 2.0f;
            const Rect chip{c.x1 - kSwitchW - kTrailGap - cw, cy - kChipH * 0.5f,
                            c.x1 - kSwitchW - kTrailGap, cy + kChipH * 0.5f};
            kit.Stroke(chip, 8.0f, 1.0f, Tok::kError);
            kit.TextMid(chip.Cx(), cy, Type::Label, Tok::kError, version, 1);
            right = chip.x0 - kNameGap;
        }
        const bool dev = cheat.source == CheatSource::Developer;
        const float nw = right - c.x0 - (dev ? kBadge + 6.0f : 0.0f);
        const float drawn =
            kit.TextMid(c.x0, cy, Type::Body, Tok::kOnSurface, kit.Fit(Type::Body, cheat.name, nw));
        if (dev) {
            // Material's verified badge right after the name: made for Azahar for Frame.
            kit.Icon(c.x0 + drawn + 6.0f + kBadge * 0.5f, cy, kBadge, Sym::kVerified, true,
                     Tok::kPrimary);
        }
    }
    kit.ClearClip();
    if (max_scroll > 0.0f) {
        // The scroll indicator, as the other long tabs.
        const float track_h = list.H() - 16.0f;
        const float thumb_h = std::max(40.0f, track_h * list.H() / content_h);
        const float ty = list.y0 + 8.0f + (track_h - thumb_h) * (scroll / max_scroll);
        kit.Fill({area.x1 - 10.0f, ty, area.x1 - 6.0f, ty + thumb_h}, 2.0f, Tok::kOnSurfaceVariant,
                 0.38f);
    }
}

} // namespace FrameXr::Menu
