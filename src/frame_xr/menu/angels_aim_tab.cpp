// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiufix: the Mods tab's Angel's Aim segment (angels_aim_tab.h), drawn as the CTGP-7
// segment is (ctgp7_tab.cpp): one page that scrolls as a whole; the Required section's catalog
// rows are Install Mods' own (DrawOfferList).

#include <algorithm>
#include <cmath>

#include "frame_xr/menu/angels_aim_tab.h"
#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;

namespace {

constexpr float kPad = 24.0f;
constexpr float kTop = 8.0f;
constexpr float kBody = 16.0f;
constexpr float kBodyLine = 24.0f;
constexpr float kSmall = 14.0f;
constexpr float kButtonH = 40.0f;
constexpr float kItemH = 56.0f;  ///< a one-line list item with its trailing text
constexpr float kTitleH = 40.0f; ///< a section's title
constexpr float kDividerH = 16.0f;


std::vector<std::string> Wrap(const Kit& kit, const std::string& s, float size, float width) {
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
    return lines;
}

std::string Percent(const std::string& s) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char ch : s) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '.' || ch == '_' || ch == '~') {
            out += static_cast<char>(ch);
        } else {
            out += '%';
            out += kHex[ch >> 4];
            out += kHex[ch & 15];
        }
    }
    return out;
}

} // namespace

std::string AngelsAimReportUrl(const std::string& app_version, const std::string& game,
                               bool mod_installed) {
    const auto clip = [](const std::string& s) { return s.substr(0, 40); };
    const std::string body = "Azahar for Frame " + clip(app_version.empty() ? "?" : app_version) +
                             "\nGame: " + clip(game.empty() ? "?" : game) +
                             "\nC-Stick & Gyro mod: " + (mod_installed ? "installed" : "not installed") +
                             "\n\n";
    return std::string(kAngelsAimIssuesUrl) + "&body=" + Percent(body);
}

bool IsAngelsAimReportUrl(const std::string& url) {
    return url.rfind(kAngelsAimIssuesUrl, 0) == 0;
}

std::vector<int> AngelsAimModRows(const ModsListModel& m) {
    std::vector<int> out;
    for (std::size_t i = 0; i < m.available.size(); i++) {
        for (const char* id : kAngelsAimModIds) {
            if (m.available[i].id == id) {
                out.push_back(static_cast<int>(i));
                break;
            }
        }
    }
    return out;
}

void TabMenu::DrawAngelsAim(Kit& kit, const MenuModel& model, const MenuView& view,
                            const Rect& area) {
    const AngelsAimModel& m = model.mods.angels_aim;
    const Rect c{area.x0 + kPad, area.y0 + kTop, area.x1 - kPad, area.y1 - kPad};
    const Rect shown{area.x0, area.y0, area.x1, area.y1 - 8.0f};
    const std::vector<int> rows = AngelsAimModRows(model.mods.mods);
    OfferListOptions opt;
    opt.manage = true;
    opt.in_page = true;
    const auto paras = [&] {
        std::vector<std::vector<std::string>> out;
        for (const char* p : {kAngelsAimAbout1, kAngelsAimAbout2}) {
            out.push_back(Wrap(kit, p, kBody, c.W()));
        }
        return out;
    }();
    float content = kTitleH + 28.0f;
    for (const auto& p : paras) {
        content += kBodyLine * static_cast<float>(p.size()) + 8.0f;
    }
    content += kButtonH + 24.0f + kDividerH;                    // Report, its line
    content += kTitleH + kItemH + 24.0f;                        // Required, the mod, reminder
    if (!rows.empty()) {
        content += OfferListHeight(kit, model.mods.mods, c, rows, opt);
    }
    content += kDividerH + kTitleH + 2.0f * kItemH;              // Suggested
    content += kDividerH + kTitleH + kButtonH + 16.0f;           // Settings
    const float max_scroll = std::max(0.0f, content - (shown.y1 - c.y0));
    const float scroll =
        std::clamp(view.scroll[static_cast<std::size_t>(view.tab)], 0.0f, max_scroll);
    layout.max_scroll = max_scroll;
    layout.scroll_step = 48.0f;
    float y = c.y0 - scroll;
    kit.Clip(shown);

    const auto title = [&](const char* text) {
        kit.TextMid(c.x0, y + 16.0f, Type::Title, Tok::kOnSurface, text);
        y += kTitleH;
    };
    const auto divider = [&] {
        kit.Line(c.x0, y + 8.0f, c.x1, y + 8.0f, 1.0f, Tok::kOutlineVariant);
        y += kDividerH;
    };
    // A one-line item: its name, and Installed / Not Installed at the end.
    const auto status_item = [&](const char* name, bool installed) {
        const float cy = y + kItemH * 0.5f;
        const char* state = installed ? kAngelsAimInstalled : kAngelsAimNotInstalled;
        const float sw = kit.TextWidth(Type::Body, state);
        if (installed) {
            kit.Icon(c.x1 - sw - 16.0f, cy, 20.0f, Sym::kCheck, false, Tok::kPrimary);
        }
        kit.TextMid(c.x1 - sw, cy, Type::Body, installed ? Tok::kPrimary : Tok::kOnSurfaceVariant,
                    state);
        kit.TextMid(c.x0, cy, Type::Body, Tok::kOnSurface,
                    kit.Fit(Type::Body, name, c.W() - sw - 48.0f));
        y += kItemH;
    };

    // ---- About Angel's Aim, its Alpha chip (as the Mods list's version chips) ----
    kit.TextMid(c.x0, y + 16.0f, Type::Title, Tok::kOnSurface, kAngelsAimTitle);
    {
        constexpr float kChipH = 32.0f, kChipPad = 12.0f;
        const float x = c.x0 + kit.TextWidth(Type::Title, kAngelsAimTitle) + 16.0f;
        const float cw = kit.TextWidth(Type::Label, kAngelsAimAlpha) + kChipPad * 2.0f;
        const Rect chip{x, y + 16.0f - kChipH * 0.5f, x + cw, y + 16.0f + kChipH * 0.5f};
        kit.Fill(chip, 8.0f, Tok::kSecondaryContainer);
        kit.TextMid(chip.Cx(), chip.Cy(), Type::Label, Tok::kOnSecondaryContainer, kAngelsAimAlpha, 1);
    }
    y += kTitleH;
    kit.TextSized(c.x0, y + 10.0f + kit.CapHeightSized(kSmall, true) * 0.5f, kSmall, true,
                  Tok::kPrimary, kAngelsAimStatus);
    y += 28.0f;
    for (const auto& p : paras) {
        for (const std::string& l : p) {
            kit.TextSized(c.x0, y + kBodyLine * 0.5f + kit.CapHeightSized(kBody, false) * 0.5f,
                          kBody, false, Tok::kOnSurfaceVariant, l);
            y += kBodyLine;
        }
        y += 8.0f;
    }
    { // Send Report (to the developer, Qt's dialog) and Report an Issue (GitHub), then their line
        const std::string send = LabelFor(Id::AngelsAimSend);
        const float sw = kit.TextWidth(Type::Label, send) + 48.0f;
        const Rect sb{c.x0, y, c.x0 + sw, y + kButtonH};
        kit.Button(sb, ButtonStyle::Tonal, 0, send, StateOf(view, {Id::AngelsAimSend}));
        AddHit({sb.x0, sb.Cy() - 24.0f, sb.x1, sb.Cy() + 24.0f}, {Id::AngelsAimSend}, shown);
        const std::string label = LabelFor(Id::AngelsAimReport);
        const float w = kit.TextWidth(Type::Label, label) + 28.0f + 24.0f;
        const Rect b{sb.x1 + 8.0f, y, sb.x1 + 8.0f + w, y + kButtonH};
        const bool can = !m.report_url.empty();
        kit.Button(b, ButtonStyle::Text, Sym::kOpenInNew, label,
                   StateOf(view, {Id::AngelsAimReport}, can));
        if (can) {
            AddHit({b.x0, b.Cy() - 24.0f, b.x1, b.Cy() + 24.0f}, {Id::AngelsAimReport}, shown);
        }
        y += kButtonH;
        kit.TextSized(c.x0, y + 10.0f + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall, false,
                      Tok::kOnSurfaceVariant, kAngelsAimReportLine);
        y += 24.0f;
    }
    divider();

    // ---- Required: the mod, the game's own controls, the catalog's rows ----
    title(kAngelsAimRequired);
    status_item(kAngelsAimModName, m.mod_installed);
    kit.TextSized(c.x0, y + 4.0f + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall, false,
                  Tok::kOnSurfaceVariant, kAngelsAimGyroReminder);
    y += 24.0f;
    if (!rows.empty()) {
        opt.page_top = y;
        DrawOfferList(kit, model, view, area, c, shown, rows, opt);
        y += OfferListHeight(kit, model.mods.mods, c, rows, opt);
    }
    divider();

    // ---- Suggested: the texture packs (no Verified badge until the owner confirms them) ----
    title(kAngelsAimSuggested);
    status_item(kAngelsAimWeaponsPack, m.weapons_pack);
    status_item(kAngelsAimEffectsPack, m.effects_pack);
    divider();

    // ---- Settings: the Controls tab at Aiming (the rows live there only) ----
    title(kAngelsAimSettings);
    {
        const std::string label = LabelFor(Id::AngelsAimSettings);
        const float w = kit.TextWidth(Type::Label, label) + 28.0f + 48.0f;
        const Rect b{c.x0, y, c.x0 + w, y + kButtonH};
        kit.Button(b, ButtonStyle::Tonal, Sym::kTune, label, StateOf(view, {Id::AngelsAimSettings}));
        AddHit({b.x0, b.Cy() - 24.0f, b.x1, b.Cy() + 24.0f}, {Id::AngelsAimSettings}, shown);
        y += kButtonH + 16.0f;
    }
    kit.ClearClip();
    DrawScrollbar(kit, view, shown, area.x1, content, max_scroll, scroll);
    DrawModsDialog(kit, ModsDialogFor(model, view), view);
}

} // namespace FrameXr::Menu
