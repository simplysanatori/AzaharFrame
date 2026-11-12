// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texparts: the "Customize" mockup (texparts_mock.h). Drawn over the Mods tab's
// Textures segment with the Kit, using the segment's own measures (mods_segments.cpp), so the
// pictures show what the feature would look like on the panel. Fake data taken from real packs:
// Henriko Magnifico's Ocarina of Time 3D 4K (ASTC), Stella Glow HD, DeathWrench's MM3DHD 0.0.42b.
//
//   Textures:                                   Customize page:
//   ( ) 1080p               [Customize]         <-  4K                    All On  All Off
//   (o) 4K · 6 of 7 Parts   [Customize]         Created by Henriko Magnifico · 6.0.0 · ...
//                                               (i) Where Parts Overlap, the Lower One Wins
//                                               [x] [img] Characters
//                                                         291 Textures · 263 MB
//                                               [x] [img] Effects                  ^  v
//                                                         52 Textures · 73 MB · Overlaps ...

#include "frame_xr/menu/texparts_mock.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include <lodepng.h>

#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/mods_segments.h"
#include "frame_xr/menu/mods_tab.h"

namespace FrameXr::Menu::TexPartsMock {

namespace Sym = ::FrameXr::Menu::Icon;

namespace {

// The Textures segment's measures (mods_segments.cpp).
constexpr float kPad = 24.0f;
constexpr float kTop = 8.0f;
constexpr float kRow1 = 48.0f;
constexpr float kRow2 = 64.0f;
constexpr float kSubhead = 40.0f; ///< a group's name over its rows
constexpr float kDividerH = 17.0f;
constexpr float kBarH = 48.0f;
constexpr float kButtonH = 40.0f;
constexpr float kSwitchW = 52.0f;
constexpr float kLeading = 40.0f;
constexpr float kNameGap = 24.0f;
constexpr float kSmall = 14.0f;
constexpr float kSnackH = 48.0f;
constexpr float kSnackGap = 16.0f;
constexpr float kThumb = 40.0f;  ///< a part's preview
constexpr float kIndent = 32.0f; ///< a part inside a folder group
constexpr float kIconBtn = 48.0f;
constexpr uint32_t kInversePrimary = 0x35618E;

// The Mods tab's segment area: the pane (rail 104, margin 12) under the segmented button
// (pane padding 24, 48 high, gap 12), as mods_tab.cpp lays it out.
constexpr Rect kArea{104.0f, 96.0f, 828.0f, 548.0f};
constexpr float kPaneRadius = 24.0f;

float ButtonWidth(const Kit& kit, const std::string& label, bool icon, float side) {
    return kit.TextWidth(Type::Label, label) + (icon ? 28.0f : 0.0f) + side * 2.0f;
}

void Radio(Kit& kit, float cx, float cy, bool selected) {
    const uint32_t ink = selected ? Tok::kPrimary : Tok::kOnSurfaceVariant;
    kit.Stroke({cx - 10.0f, cy - 10.0f, cx + 10.0f, cy + 10.0f}, 10.0f, 2.0f, ink);
    if (selected) {
        kit.Dot(cx, cy, 5.0f, ink);
    }
}

/// Material 3 checkbox: an 18 dp box, 2 dp outline, 2 dp corners; checked or mixed (a group with
/// some parts on) filled with the primary colour, the mark in on-primary.
void Checkbox(Kit& kit, float cx, float cy, int value /* 0 off, 1 mixed, 2 on */) {
    const Rect b{cx - 9.0f, cy - 9.0f, cx + 9.0f, cy + 9.0f};
    if (value == 0) {
        kit.Stroke(b, 2.0f, 2.0f, Tok::kOnSurfaceVariant);
        return;
    }
    kit.Fill(b, 2.0f, Tok::kPrimary);
    if (value == 1) {
        kit.Line(cx - 5.0f, cy, cx + 5.0f, cy, 2.0f, Tok::kOnPrimary);
    } else {
        kit.Line(cx - 5.0f, cy + 0.5f, cx - 1.5f, cy + 4.0f, 2.0f, Tok::kOnPrimary);
        kit.Line(cx - 1.5f, cy + 4.0f, cx + 5.5f, cy - 3.5f, 2.0f, Tok::kOnPrimary);
    }
}

/// arrow_upward / arrow_downward in a standard icon button (the glyphs would be added to
/// md3_fonts.h; drawn with lines here).
void ArrowButton(Kit& kit, float cx, float cy, bool up, bool enabled, bool hover) {
    if (hover && enabled) {
        kit.Dot(cx, cy, 20.0f, Tok::kOnSurfaceVariant, kHoverLayer);
    }
    const float op = enabled ? 1.0f : kDisabledContent;
    const uint32_t ink = enabled ? Tok::kOnSurfaceVariant : Tok::kOnSurface;
    const float d = up ? -1.0f : 1.0f;
    kit.Line(cx, cy - 7.0f * d, cx, cy + 7.0f * d, 2.0f, ink, op);
    kit.Line(cx, cy + 7.5f * d, cx - 6.0f, cy + 1.5f * d, 2.0f, ink, op);
    kit.Line(cx, cy + 7.5f * d, cx + 6.0f, cy + 1.5f * d, 2.0f, ink, op);
}

/// The headline and the supporting line (mods_segments.cpp RowText, without the badge).
void RowText(Kit& kit, float x, float right, float cy, const std::string& name,
             const std::string& detail, float op = 1.0f) {
    const float ny = detail.empty() ? cy : cy - 11.0f;
    kit.TextMid(x, ny, Type::Body, Tok::kOnSurface, kit.Fit(Type::Body, name, right - x), 0, op);
    if (!detail.empty()) {
        kit.TextSized(x, cy + 13.0f + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall, false,
                      Tok::kOnSurfaceVariant, kit.Fit(Type::Tooltip, detail, right - x), 0, op);
    }
}

/// The restart snackbar (TabMenu::DrawRestartSnackbar).
float Snackbar(Kit& kit, bool hover) {
    const Rect c{kArea.x0 + kPad, kArea.y0, kArea.x1 - kPad, kArea.y1};
    const Rect bar{c.x0, kArea.y1 - kSnackGap - kSnackH, c.x1, kArea.y1 - kSnackGap};
    kit.Fill(bar, 4.0f, Tok::kInverseSurface);
    const std::string action = "Restart";
    const float aw = kit.TextWidth(Type::Label, action) + 24.0f;
    const Rect a{bar.x1 - 8.0f - aw, bar.Cy() - 20.0f, bar.x1 - 8.0f, bar.Cy() + 20.0f};
    kit.StateLayer(a, 20.0f, kInversePrimary, hover ? State::Hover : State::Rest);
    kit.TextMid(a.Cx(), bar.Cy(), Type::Label, kInversePrimary, action, 1);
    kit.TextSized(bar.x0 + 16.0f, bar.Cy() + kit.CapHeightSized(16.0f, false) * 0.5f, 16.0f, false,
                  Tok::kInverseOnSurface, "Restart the Game to Apply");
    return bar.y0 - 8.0f;
}

/// A tooltip's centre moved left as needed so it stays 8 dp inside the pane.
float InPane(const Kit& kit, float cx, const std::string& text) {
    const float half = (kit.TextWidth(Type::Tooltip, text) + 20.0f) * 0.5f;
    return std::min(cx, kArea.x1 - 8.0f - half);
}

/// Clears the segment area (the menu under it drew the empty Textures state).
void Erase(Kit& kit) {
    kit.FillCorners(kArea, 0.0f, 0.0f, kPaneRadius, kPaneRadius, Tok::kSurface);
}

std::string Count(uint64_t n, const char* one, const char* many) {
    std::string digits = std::to_string(n), out;
    for (std::size_t i = 0; i < digits.size(); i++) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            out += ',';
        }
        out += digits[i];
    }
    return out + " " + (n == 1 ? one : many);
}

std::string Detail(uint64_t textures, uint64_t bytes, const std::string& overlaps = "") {
    return JoinDetail({Count(textures, "Texture", "Textures"), SizeText(bytes),
                       overlaps.empty() ? std::string() : "Overlaps " + overlaps});
}

// ---------------------------------------------------------------------------------------------
// Previews: the part's first sizeable PNG, cropped square, shrunk, corners rounded.

struct Thumbs {
    std::string dir;
    int n = 100;          ///< pixels across
    float radius_px = 20; ///< corner radius in pixels
    std::map<std::string, std::vector<uint8_t>> cache;

    const std::vector<uint8_t>* Get(const std::string& key) {
        if (key.empty() || dir.empty()) {
            return nullptr;
        }
        auto it = cache.find(key);
        if (it == cache.end()) {
            it = cache.emplace(key, Load(dir + "/" + key + ".png")).first;
        }
        return it->second.empty() ? nullptr : &it->second;
    }

    std::vector<uint8_t> Load(const std::string& path) const {
        std::vector<unsigned char> img;
        unsigned w = 0, h = 0;
        if (lodepng::decode(img, w, h, path) != 0 || w == 0 || h == 0) {
            return {};
        }
        const double side = std::min(w, h);
        const double ox = (w - side) * 0.5, oy = (h - side) * 0.5;
        std::vector<uint8_t> out(static_cast<std::size_t>(n) * n * 4);
        for (int y = 0; y < n; y++) {
            for (int x = 0; x < n; x++) {
                const int sx0 = static_cast<int>(ox + side * x / n);
                const int sx1 = std::max(sx0 + 1, static_cast<int>(ox + side * (x + 1) / n));
                const int sy0 = static_cast<int>(oy + side * y / n);
                const int sy1 = std::max(sy0 + 1, static_cast<int>(oy + side * (y + 1) / n));
                double acc[4] = {0, 0, 0, 0};
                int cnt = 0;
                for (int sy = sy0; sy < sy1 && sy < static_cast<int>(h); sy++) {
                    for (int sx = sx0; sx < sx1 && sx < static_cast<int>(w); sx++) {
                        const unsigned char* p = &img[(static_cast<std::size_t>(sy) * w + sx) * 4];
                        const double a = p[3] / 255.0;
                        acc[0] += p[0] * a;
                        acc[1] += p[1] * a;
                        acc[2] += p[2] * a;
                        acc[3] += a;
                        cnt++;
                    }
                }
                uint8_t* q = &out[(static_cast<std::size_t>(y) * n + x) * 4];
                const double a = cnt ? acc[3] / cnt : 0.0;
                for (int i = 0; i < 3; i++) {
                    q[i] = static_cast<uint8_t>(acc[3] > 0 ? std::clamp(acc[i] / acc[3], 0.0, 255.0) : 0);
                }
                // Rounded corners: coverage of the pixel centre inside the rounded square.
                const float r = radius_px;
                const float px = x + 0.5f, py = y + 0.5f;
                const float cx = std::clamp(px, r, n - r), cy = std::clamp(py, r, n - r);
                const float dist = std::hypot(px - cx, py - cy);
                const float cover = std::clamp(r - dist + 0.5f, 0.0f, 1.0f);
                q[3] = static_cast<uint8_t>(std::clamp(a * cover * 255.0, 0.0, 255.0));
            }
        }
        return out;
    }
};

void DrawThumb(Kit& kit, Thumbs& thumbs, const Rect& r, const std::string& key, char32_t fallback) {
    kit.Fill(r, 8.0f, Tok::kSurfaceContainerHighest);
    if (const auto* px = thumbs.Get(key)) {
        kit.Image(px->data(), thumbs.n, thumbs.n, r);
    } else {
        kit.Icon(r.Cx(), r.Cy(), 24.0f, fallback, false, Tok::kOnSurfaceVariant);
    }
}

// ---------------------------------------------------------------------------------------------
// The Customize page

enum class Kind {
    Check,      ///< a part: checkbox, preview, name, counts
    Radio,      ///< one variant of a group: radio button, preview, name, counts
    RadioOff,   ///< a group's "Off"
    GroupCheck, ///< a folder of parts: a checkbox for all of them (mixed when some are on)
    GroupRadio, ///< a group of variants: its name only
};

struct Row {
    Kind kind = Kind::Check;
    std::string name;
    std::string detail;
    std::string thumb;
    int value = 0;      ///< Check/GroupCheck: 0 off, 1 mixed, 2 on; Radio: 2 chosen
    bool child = false; ///< inside a folder group: indented
    bool arrows = false; ///< its order decides an overlap: Move Up, Move Down
    bool up = true, down = true;
};

enum class What { None, Row, Up, Down, AllOn, AllOff, Back, Restart };
struct Hover {
    What what = What::None;
    int row = -1;
};

struct Page {
    std::string title;
    std::string author;
    std::string rest; ///< after the author
    bool all_buttons = true;
    bool note = false; ///< some parts overlap
    std::vector<Row> rows;
    float scroll = 0.0f; ///< -1: the end
    bool restart = false;
    Hover hover;
};

float RowHeight(const Row& r) {
    switch (r.kind) {
    case Kind::GroupRadio:
        return kSubhead;
    case Kind::RadioOff:
        return kRow1;
    default:
        return r.detail.empty() ? kRow1 : kRow2;
    }
}

void DrawPage(Kit& kit, Thumbs& thumbs, const Page& p) {
    Erase(kit);
    const Rect c{kArea.x0 + kPad, kArea.y0 + kTop, kArea.x1 - kPad, kArea.y1 - kPad};
    float bottom = kArea.y1 - 8.0f;
    if (p.restart) {
        bottom = Snackbar(kit, p.hover.what == What::Restart);
    }
    // Top bar: back, the pack's name, All On and All Off.
    const Rect back{c.x0 - 12.0f, c.y0, c.x0 + 36.0f, c.y0 + kBarH};
    kit.StateLayer(back, 24.0f, Tok::kOnSurfaceVariant,
                   p.hover.what == What::Back ? State::Hover : State::Rest);
    kit.Icon(back.Cx(), back.Cy(), 24.0f, Sym::kArrowBack, false, Tok::kOnSurface);
    float title_right = c.x1;
    if (p.all_buttons) {
        float right = c.x1 + 12.0f; // a text button's padding lines its label up with the edge
        const std::pair<What, const char*> buttons[] = {{What::AllOff, "All Off"},
                                                        {What::AllOn, "All On"}};
        for (const auto& [what, label] : buttons) {
            const float w = ButtonWidth(kit, label, false, 12.0f);
            const Rect b{right - w, back.Cy() - kButtonH * 0.5f, right, back.Cy() + kButtonH * 0.5f};
            kit.Button(b, ButtonStyle::Text, 0, label,
                       p.hover.what == what ? State::Hover : State::Rest);
            right = b.x0 - 8.0f;
        }
        title_right = right - 8.0f;
    }
    kit.TextMid(back.x1 + 12.0f, back.Cy(), Type::Title, Tok::kOnSurface,
                kit.Fit(Type::Title, p.title, title_right - back.x1 - 12.0f));
    // "Created by <author> · <version> · <n> Textures On · <size>"
    float y = c.y0 + kBarH;
    {
        const float base = y + 20.0f;
        float at = c.x0;
        at += kit.TextSized(at, base, kSmall, false, Tok::kOnSurfaceVariant, "Created by ");
        at += kit.TextSized(at, base, kSmall, false, Tok::kPrimary, p.author);
        kit.TextSized(at, base, kSmall, false, Tok::kOnSurfaceVariant,
                      kit.Fit(Type::Tooltip, " · " + p.rest, c.x1 - at));
        y += 32.0f;
    }
    if (p.note) {
        kit.Icon(c.x0 + 10.0f, y + 14.0f, 20.0f, Sym::kInfo, false, Tok::kOnSurfaceVariant);
        kit.TextSized(c.x0 + 30.0f, y + 14.0f + kit.CapHeightSized(kSmall, false) * 0.5f, kSmall,
                      false, Tok::kOnSurfaceVariant, "Where Parts Overlap, the Lower One Wins");
        y += 32.0f;
    }
    const float top = y + 4.0f;
    const Rect list{kArea.x0, top, kArea.x1, bottom};

    // The list, scrolled to whole rows (as the segment's lists).
    std::vector<float> tops;
    float content = 0.0f;
    for (const Row& r : p.rows) {
        tops.push_back(content);
        content += RowHeight(r);
    }
    float max_scroll = 0.0f;
    const float raw_max = std::max(0.0f, content - list.H());
    if (raw_max > 0.0f) {
        max_scroll = content;
        for (const float t : tops) {
            if (t >= raw_max) {
                max_scroll = t;
                break;
            }
        }
    }
    const float want = p.scroll < 0.0f ? max_scroll : std::clamp(p.scroll, 0.0f, max_scroll);
    float scroll = 0.0f;
    for (const float t : tops) {
        if (t <= want + 0.01f) {
            scroll = t;
        }
    }
    struct Tip {
        float cx = 0, y = 0;
        std::string text;
        bool above = true;
    } tip;
    kit.Clip(list);
    for (std::size_t i = 0; i < p.rows.size(); i++) {
        const Row& r = p.rows[i];
        const float h = RowHeight(r);
        const float ry = top + tops[i] - scroll;
        if (ry < list.y0 - 0.01f || ry + h > list.y1 + 0.01f) {
            continue; // whole rows only
        }
        const int idx = static_cast<int>(i);
        const Rect row{c.x0 - 12.0f, ry, c.x1 + 12.0f, ry + h};
        const float cy = row.Cy();
        if (r.kind == Kind::GroupRadio) {
            // Material list subheader: the group's name.
            kit.TextSized(c.x0, ry + h - 12.0f, kSmall, true, Tok::kPrimary, r.name);
            continue;
        }
        if (p.hover.what == What::Row && p.hover.row == idx) {
            kit.StateLayer(row, 16.0f, Tok::kOnSurface, State::Hover);
        }
        const float indent = r.child ? kIndent : 0.0f;
        const float lead = c.x0 + 12.0f + indent;
        if (r.kind == Kind::Radio || r.kind == Kind::RadioOff) {
            Radio(kit, lead, cy, r.value == 2);
        } else {
            Checkbox(kit, lead, cy, r.value);
        }
        const Rect th{c.x0 + kLeading + indent, cy - kThumb * 0.5f, c.x0 + kLeading + indent + kThumb,
                      cy + kThumb * 0.5f};
        if (r.kind == Kind::GroupCheck) {
            kit.Fill(th, 8.0f, Tok::kSurfaceContainerHighest);
            kit.Icon(th.Cx(), th.Cy(), 24.0f, Sym::kFolderOpen, false, Tok::kOnSurfaceVariant);
        } else if (r.kind != Kind::RadioOff) {
            DrawThumb(kit, thumbs, th, r.thumb, Sym::kTexture);
        }
        float right = c.x1;
        if (r.arrows) {
            const float down_cx = c.x1 - kIconBtn * 0.5f + 12.0f;
            const float up_cx = down_cx - kIconBtn;
            const bool hu = p.hover.row == idx && p.hover.what == What::Up;
            const bool hd = p.hover.row == idx && p.hover.what == What::Down;
            ArrowButton(kit, up_cx, cy, true, r.up, hu);
            ArrowButton(kit, down_cx, cy, false, r.down, hd);
            if (hu || hd) {
                // Above, unless that covers the row above's buttons or leaves the list: then
                // below (never over another row's order buttons).
                const bool prev_arrows = i > 0 && p.rows[i - 1].arrows;
                const bool first = ry - kRow1 < list.y0;
                const bool next_arrows = i + 1 < p.rows.size() && p.rows[i + 1].arrows;
                const bool above = !(prev_arrows || first) || next_arrows;
                tip = {hu ? up_cx : down_cx, above ? row.y0 + 4.0f : row.y1 - 4.0f,
                       hu ? "Move Up" : "Move Down", above};
            }
            right = up_cx - kIconBtn * 0.5f - 8.0f;
        }
        RowText(kit, th.x1 + 16.0f, right, cy, r.name, r.detail);
    }
    kit.ClearClip();
    if (max_scroll > 0.0f) {
        const float track_h = list.H() - 16.0f;
        const float thumb_h = std::max(40.0f, track_h * list.H() / content);
        const float ty = list.y0 + 8.0f + (track_h - thumb_h) * (scroll / max_scroll);
        kit.Fill({kArea.x1 - 10.0f, ty, kArea.x1 - 6.0f, ty + thumb_h}, 2.0f,
                 Tok::kOnSurfaceVariant, 0.38f);
    }
    if (!tip.text.empty()) {
        kit.Tooltip(InPane(kit, tip.cx, tip.text), tip.y, tip.text, tip.above);
    }
}

// ---------------------------------------------------------------------------------------------
// The Textures segment with Customize on each pack

struct PackRow {
    std::string name, detail;
    bool active = false;
};

enum class CustomizeStyle { TextButton, IconButton };

void DrawPackList(Kit& kit, const std::vector<PackRow>& packs, CustomizeStyle style,
                  int hover_customize) {
    Erase(kit);
    const Rect c{kArea.x0 + kPad, kArea.y0 + kTop, kArea.x1 - kPad, kArea.y1 - kPad};
    {
        const std::string download = "Download Packs";
        const Rect b{c.x0, c.y0, c.x0 + ButtonWidth(kit, download, true, 24.0f), c.y0 + kBarH};
        kit.Button(b, ButtonStyle::Tonal, Sym::kDownload, download, State::Rest);
    }
    float y = c.y0 + kBarH + 8.0f;
    const auto switch_row = [&](const char* label, bool on) {
        const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + kRow1};
        kit.Switch({c.x1 - kSwitchW, row.y0, c.x1, row.y1}, on, State::Rest);
        kit.TextMid(c.x0, row.Cy(), Type::Body, Tok::kOnSurface, label);
        y += kRow1;
    };
    const auto divider = [&] {
        kit.Fill({c.x0, y + 8.0f, c.x1, y + 9.0f}, 0.0f, Tok::kOutlineVariant);
        y += kDividerH;
    };
    switch_row("Custom Textures", true);
    divider();
    {
        const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + kRow1};
        Radio(kit, c.x0 + 12.0f, row.Cy(), false);
        kit.TextMid(c.x0 + kLeading, row.Cy(), Type::Body, Tok::kOnSurface, "Off");
        y += kRow1;
    }
    struct Tip {
        float cx = 0, y = 0;
    } tip{-1.0f, 0.0f};
    for (std::size_t i = 0; i < packs.size(); i++) {
        const PackRow& p = packs[i];
        const Rect row{c.x0 - 12.0f, y, c.x1 + 12.0f, y + kRow2};
        const float cy = row.Cy();
        const bool hover = hover_customize == static_cast<int>(i);
        Radio(kit, c.x0 + 12.0f, cy, p.active);
        float right = c.x1;
        if (style == CustomizeStyle::TextButton) {
            const std::string label = "Customize";
            const float w = ButtonWidth(kit, label, true, 12.0f);
            const Rect b{c.x1 - w, cy - kButtonH * 0.5f, c.x1, cy + kButtonH * 0.5f};
            kit.Button(b, ButtonStyle::Text, Sym::kTune, label, hover ? State::Hover : State::Rest);
            right = b.x0 + 12.0f - kNameGap;
        } else {
            const float cx = c.x1 - 12.0f;
            if (hover) {
                kit.Dot(cx, cy, 20.0f, Tok::kOnSurfaceVariant, kHoverLayer);
                // Left of the button, on its own row: above or below it would cover the
                // neighbouring rows' buttons.
                const float half = (kit.TextWidth(Type::Tooltip, "Customize") + 20.0f) * 0.5f;
                tip = {cx - 24.0f - 8.0f - half, cy - 19.0f};
            }
            kit.Icon(cx, cy, 24.0f, Sym::kTune, false, Tok::kOnSurfaceVariant);
            right = cx - 24.0f - kNameGap;
        }
        RowText(kit, c.x0 + kLeading, right, cy, p.name, p.detail);
        y += kRow2;
    }
    divider();
    switch_row("Preload", false);
    if (tip.cx >= 0.0f) {
        kit.Tooltip(tip.cx, tip.y, "Customize", false);
    }
}

// ---------------------------------------------------------------------------------------------
// Sample data

constexpr uint64_t kMB = 1000ull * 1000ull;

std::vector<PackRow> OotPacks() {
    return {{"1080p", "Henriko Magnifico · 6.0.0 · ASTC/KTX · 2.3 GB", false},
            {"4K", "Henriko Magnifico · 6.0.0 · ASTC/KTX · 6.0 GB · 6 of 7 Parts", true},
            {"4K (PNG)", "Henriko Magnifico · 6.0.0 · PNG · 4.7 GB", false}};
}

/// Henriko Magnifico's OoT3D 4K, ASTC build (sizes and counts of the installed pack); UI off.
/// EFFECTS and ENVIRONMENT share one texture hash.
Page OotPage() {
    Page p;
    p.title = "4K";
    p.author = "Henriko Magnifico";
    p.rest = "6.0.0 · 2,034 Textures On · 5.0 GB";
    p.note = true;
    p.rows = {
        {Kind::Check, "Characters", Detail(291, 262608424), "oot-CHARACTERS", 2},
        {Kind::Check, "Creatures", Detail(81, 72280948), "oot-CREATURES", 2},
        {Kind::Check, "Effects", Detail(52, 73494804, "Environment"), "oot-EFFECTS", 2, false, true,
         true, true},
        {Kind::Check, "Environment", Detail(1434, 4496484836ull, "Effects"), "oot-ENVIRONMENT", 2,
         false, true, true, true},
        {Kind::Check, "Link", Detail(96, 79833336), "oot-LINK", 2},
        {Kind::Check, "Maps", Detail(80, 42565292), "oot-MAPS", 2},
        {Kind::Check, "UI", Detail(274, 1007027932), "oot-UI", 0},
    };
    return p;
}

/// Stella Glow HD (the owner's pack): no overlaps, so no order controls; Characters and UI hold
/// no textures of their own, so their folders are the parts, under a checkbox for the folder.
Page StellaPage() {
    Page p;
    p.title = "Stella Glow HD";
    p.author = "simplysanatori";
    p.rest = "1.0.0 · 5,013 Textures On · 3.2 GB";
    const auto part = [](const char* name, uint64_t n, uint64_t bytes, const char* thumb, int v,
                         bool child = false) {
        Row r{Kind::Check, name, Detail(n, bytes), thumb, v};
        r.child = child;
        return r;
    };
    Row characters{Kind::GroupCheck, "Characters", Detail(10914, 2311374131ull), "", 1};
    Row ui{Kind::GroupCheck, "UI", Detail(1451, 333022144), "", 2};
    p.rows = {
        part("Backgrounds", 806, 1323126008, "sg-backgrounds", 2),
        characters,
        part("3D Models", 279, 178555557, "sg-3d", 2, true),
        part("Battle Portraits", 136, 113749005, "sg-battle-portraits", 2, true),
        part("Portraits", 795, 204743216, "sg-portraits", 2, true),
        part("Sprites", 9704, 1814326353, "sg-sprites", 0, true),
        part("Effects", 535, 110989425, "sg-effects", 2),
        part("Event CGs", 178, 551156940, "sg-cgs", 2),
        part("Maps", 662, 349513655, "sg-maps", 2),
        part("Misc", 3, 15407, "sg-misc", 2),
        ui,
        part("Battle", 624, 59914387, "sg-ui-battle", 2, true),
        part("Common", 451, 181668333, "sg-ui-common", 2, true),
        part("Dialogue Boxes", 30, 920238, "sg-ui-dialogue", 2, true),
        part("Menus", 181, 22980112, "sg-ui-menus", 2, true),
        part("Staff Roll", 54, 5406107, "sg-ui-staff", 2, true),
        part("System", 43, 2155531, "sg-ui-system", 2, true),
        part("Titles & Chapter Cards", 68, 59977436, "sg-ui-titles", 2, true),
        part("Weapons", 168, 33656722, "sg-weapons", 2),
    };
    return p;
}

/// DeathWrench's MM3DHD 0.0.42b as one pack: [Main], two variant groups ([Fonts], [Link's Tunic],
/// one choice each), [Link] Nerrel Style, and the "AI Upscaled or WIP" folder. [Link] Nerrel
/// Style and AI Upscaled or WIP/Other Link Textures share 10 hashes.
Page MmPage() {
    Page p;
    p.title = "MM3D HD";
    p.author = "DeathWrench";
    p.rest = "0.0.42b · 209 Textures On · 73 MB";
    p.note = true;
    Row link{Kind::Check, "Link: Nerrel Style", Detail(10, 252138, "Other Link Textures"),
             "mm-link-nerrel", 2};
    link.arrows = true;
    Row ai{Kind::GroupCheck, "AI Upscaled or WIP", Detail(119, 30973689, "Link: Nerrel Style"), "", 1};
    ai.arrows = true;
    ai.down = false;
    Row other{Kind::Check, "Other Link Textures", Detail(78, 23172816, "Link: Nerrel Style"),
              "mm-ai-link", 2};
    other.child = true;
    Row map{Kind::Check, "Map", Detail(37, 3355982), "mm-ai-map", 2};
    map.child = true;
    Row wip{Kind::Check, "wip", Detail(4, 4444891), "mm-ai-wip", 0};
    wip.child = true;
    p.rows = {
        {Kind::Check, "Main", Detail(80, 45393424), "mm-main", 2},
        {Kind::GroupRadio, "Fonts"},
        {Kind::RadioOff, "Off"},
        {Kind::Radio, "Original N64", Detail(1, 177980), "mm-font-n64", 2},
        {Kind::Radio, "Original Pizza", Detail(1, 117365), "mm-font-pizza", 0},
        {Kind::Radio, "Original WW (resembles 3DS)", Detail(1, 123279), "mm-font-ww", 0},
        {Kind::GroupRadio, "Link's Tunic"},
        {Kind::RadioOff, "Off"},
        {Kind::Radio, "N64 Inspired", Detail(3, 770125), "mm-tunic-n64", 0},
        {Kind::Radio, "Nerrel Style", Detail(3, 809766), "mm-tunic-nerrel", 2},
        link,
        ai,
        map,
        other,
        wip,
    };
    return p;
}

/// A pack that is only variants: HardDanX's Touchless Layout UI, one controller style. The
/// style names follow the catalog's notes; the counts are illustrative (the archive was not
/// listed). No PNG previews in the mock: the placeholder shows.
Page TouchlessPage() {
    Page p;
    p.title = "Touchless Layout UI";
    p.author = "HardDanX";
    p.rest = "146 Textures On · 13 MB";
    p.all_buttons = false;
    p.rows = {
        {Kind::GroupRadio, "Controller"},
        {Kind::RadioOff, "Off"},
        {Kind::Radio, "Xbox", Detail(146, 13 * kMB), "", 2},
        {Kind::Radio, "DualShock", Detail(146, 13 * kMB), "", 0},
        {Kind::Radio, "Nintendo", Detail(146, 13 * kMB), "", 0},
        {Kind::Radio, "SNES Colours", Detail(146, 14 * kMB), "", 0},
        {Kind::Radio, "Unedited Textures", Detail(146, 9 * kMB), "", 0},
    };
    return p;
}

} // namespace

bool Run(const std::string& thumbs_dir, const MenuModel& base, const Shot& shot) {
    Thumbs thumbs;
    thumbs.dir = thumbs_dir;
    MenuView v;
    v.tab = Tab::Mods;
    v.mods_segment = ModsSegment::Textures;
    MenuModel oot = base;
    oot.game_title = "The Legend of Zelda: Ocarina of Time 3D";
    oot.mods.textures.custom_textures = true;
    bool ok = true;
    const auto page = [&](const char* name, const MenuModel& m, Page p) {
        return shot(name, m, v, [&thumbs, p](Kit& kit) {
            thumbs.n = static_cast<int>(std::lround(kThumb * kit.Px()));
            thumbs.radius_px = 8.0f * kit.Px();
            DrawPage(kit, thumbs, p);
        });
    };

    // a. The Textures segment: Customize on each pack (a text button, or an icon button).
    ok &= shot("a1-textures-customize", oot, v, [](Kit& kit) {
        DrawPackList(kit, OotPacks(), CustomizeStyle::TextButton, 1);
    });
    ok &= shot("a2-textures-customize-icon", oot, v, [](Kit& kit) {
        DrawPackList(kit, OotPacks(), CustomizeStyle::IconButton, 1);
    });

    // b. A pack's Customize page.
    Page b = OotPage();
    b.hover = {What::Down, 2};
    ok &= page("b1-customize-oot3d-4k", oot, b);
    b.hover = {What::Row, 6};
    b.scroll = -1.0f;
    b.restart = true;
    ok &= page("b2-customize-oot3d-4k-restart", oot, b);
    MenuModel sg = base;
    sg.game_title = "Stella Glow";
    Page s = StellaPage();
    s.hover = {What::Row, 5};
    ok &= page("b3-customize-stella-glow", sg, s);
    s.hover = {};
    s.scroll = -1.0f;
    ok &= page("b4-customize-stella-glow-end", sg, s);

    // c. Variants: radio groups in a pack; a pack that is only variants.
    MenuModel mm = base;
    mm.game_title = "The Legend of Zelda: Majora's Mask 3D";
    Page c = MmPage();
    c.hover = {What::Row, 3};
    ok &= page("c1-variants-mm3dhd", mm, c);
    c.hover = {What::Up, 11};
    c.scroll = -1.0f;
    ok &= page("c2-variants-mm3dhd-end", mm, c);
    Page t = TouchlessPage();
    t.hover = {What::Row, 3};
    ok &= page("c3-variants-touchless-layout", oot, t);
    return ok;
}

} // namespace FrameXr::Menu::TexPartsMock
