// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <optional>

#include "frame_xr/vis/beam.h"
#include "frame_xr/vis/help_layout.h"
#include "frame_xr/vis/panels.h"

namespace FrameXr::Vis {

namespace {

constexpr uint32_t kAction = 0xFFDCDCE2u;

// One column of the help: headings and rows from `y`, `row_h` apart.
void Column(PanelArt& a, const std::vector<HelpRow>& rows, float x0, float x1, float action_x,
            float y, float row_h, float size) {
    for (const HelpRow& r : rows) {
        if (r.kind == HelpRow::Kind::Heading) {
            y += row_h * 0.35f;
            const float base = y + row_h * 0.75f;
            a.Text(x0, base, size + 3.0f, true, r.now ? Ink::kAccent : Ink::kDim, r.control);
            if (r.now) {
                const float tw = a.TextWidth(size + 3.0f, true, r.control);
                const float px0 = x0 + tw + 14.0f, pw = a.TextWidth(size - 4.0f, true, "Now") + 22.0f;
                const float py0 = base - a.CapHeight(size + 3.0f) - 4.0f, py1 = base + 5.0f;
                a.FillRRect(px0, py0, px0 + pw, py1, (py1 - py0) * 0.5f, Ink::kKeyOn);
                a.Text(px0 + pw * 0.5f, base, size - 4.0f, true, Ink::kInkOnLight, "Now", 1);
            }
            a.FillRRect(x0, base + 9.0f, x1, base + 11.0f, 1.0f, Ink::kRule);
            y += row_h + 6.0f;
            continue;
        }
        const float base = y + row_h * 0.72f;
        a.Text(x0, base, size, true, Ink::kText, a.Fit(size, true, r.control, action_x - x0 - 12.0f));
        a.Text(action_x, base, size, false, kAction, a.Fit(size, false, r.action, x1 - action_x));
        y += row_h;
    }
}

// The tallest rows that let the column fit in `height` (a heading is 1.35 rows and 6 pixels).
float RowHeightFor(const std::vector<HelpRow>& rows, float height) {
    float units = 0.0f, fixed = 0.0f;
    for (const HelpRow& r : rows) {
        units += r.kind == HelpRow::Kind::Heading ? 1.35f : 1.0f;
        fixed += r.kind == HelpRow::Kind::Heading ? 6.0f : 0.0f;
    }
    return units > 0.0f ? (height - fixed) / units : height;
}

} // namespace

void DrawHelpClose(PanelArt& a, bool close_hot) {
    const float x = static_cast<float>(kHelpRect.x), y = static_cast<float>(kHelpRect.y);
    // The button's box lies inside the sheet: the sheet again, then the button.
    const int bx = static_cast<int>(x + kCloseX - kCloseR - 2), by = static_cast<int>(y + kCloseY - kCloseR - 2);
    const int bs = static_cast<int>(2 * kCloseR + 4);
    a.ClearRect(bx, by, bs, bs);
    a.FillRRect(static_cast<float>(bx), static_cast<float>(by), static_cast<float>(bx + bs),
                static_cast<float>(by + bs), 0.0f, Ink::kSheet);
    a.FillRRect(x + kCloseX - kCloseR, y + kCloseY - kCloseR, x + kCloseX + kCloseR,
                y + kCloseY + kCloseR, kCloseR, close_hot ? Ink::kKeyHot : Ink::kKey);
    a.StrokeRRect(x + kCloseX - kCloseR, y + kCloseY - kCloseR, x + kCloseX + kCloseR,
                  y + kCloseY + kCloseR, kCloseR, 2.0f, close_hot ? Ink::kKeyRimHot : Ink::kKeyRim);
    const float c = 10.0f;
    a.Line(x + kCloseX - c, y + kCloseY - c, x + kCloseX + c, y + kCloseY + c, 3.5f, Ink::kText);
    a.Line(x + kCloseX - c, y + kCloseY + c, x + kCloseX + c, y + kCloseY - c, 3.5f, Ink::kText);
}

void DrawHelp(PanelArt& a, const HelpPage& page) {
    const AtlasRect R = kHelpRect;
    a.ClearRect(R.x, R.y, R.w, R.h);
    const float x = static_cast<float>(R.x), y = static_cast<float>(R.y);
    a.FillRRect(x + 2, y + 2, x + R.w - 2, y + R.h - 2, 36.0f, Ink::kSheet);
    a.Text(x + 44, y + 76, 40.0f, true, Ink::kText, "Controls");
    DrawHelpClose(a, false);

    // Two columns between the title and the footer; rows as tall as fit, at most 30 pixels.
    const float top = y + 104.0f, bottom = y + R.h - 64.0f;
    const float row_h = std::min({30.0f, RowHeightFor(page.game, bottom - top),
                                  RowHeightFor(page.window, bottom - top)});
    // One text size for both columns, the largest that cuts no label, and the actions start after
    // each column's widest control label (AzaharFrame screenswap: "Hold View + Right Bumper" used
    // to be cut).
    const float max_size = std::clamp(row_h * 0.68f, 15.0f, 20.0f);
    const HelpColumnSpan game_span{x + 44, x + 640}, window_span{x + 684, x + R.w - 44};
    const HelpTextLayout fit = LayoutHelpText(
        page, game_span, window_span, max_size,
        [&a](float size, bool bold, const std::string& text) { return a.TextWidth(size, bold, text); });
    Column(a, page.game, game_span.x0, game_span.x1, fit.game.action_x, top, row_h, fit.size);
    Column(a, page.window, window_span.x0, window_span.x1, fit.window.action_x, top, row_h,
           fit.size);
    a.FillRRect(x + 44, y + R.h - 58, x + R.w - 44, y + R.h - 56, 1.0f, Ink::kRule);
    a.Text(x + 44, y + R.h - 24, 18.0f, false, Ink::kDim, page.footer);
}

void DrawStrip(PanelArt& a, const StripView& v) {
    const AtlasRect R = kStripRect;
    a.ClearRect(R.x, R.y, R.w, R.h);
    const float y0 = static_cast<float>(R.y) + 8.0f, y1 = static_cast<float>(R.y + R.h) - 8.0f;
    const struct {
        float x0, x1;
        const char* label;
        bool on;
        bool hot;
    } buttons[] = {
        {kStripControls0, kStripControls1, "Controls", v.help_on, v.hover == 0},
        {kStripStats0, kStripStats1, "Stats", v.stats_on, v.hover == 1},
    };
    for (const auto& b : buttons) {
        const float x0 = static_cast<float>(R.x) + b.x0, x1 = static_cast<float>(R.x) + b.x1;
        const float r = (y1 - y0) * 0.5f;
        a.FillRRect(x0 - 2, y0 - 2, x1 + 2, y1 + 2, r + 2, 0xD0141416u); // readable over anything
        a.FillRRect(x0, y0, x1, y1, r, b.on ? Ink::kKeyOn : b.hot ? Ink::kKeyHot : Ink::kKey);
        a.StrokeRRect(x0, y0, x1, y1, r, 2.0f,
                      b.on ? Ink::kKeyOnRim : b.hot ? Ink::kKeyRimHot : Ink::kKeyRim);
        a.Text((x0 + x1) * 0.5f, (y0 + y1) * 0.5f + a.CapHeight(26.0f) * 0.5f, 26.0f, true,
               b.on ? Ink::kInkOnLight : Ink::kText, b.label, 1);
    }
}

void DrawBeam(PanelArt& a) {
    const std::vector<uint8_t> beam = BeamArt();
    a.Put(kBeamRect.x, kBeamRect.y, beam.data(), kBeamRect.w, kBeamRect.h);
}

void DrawStats(PanelArt& a, const std::vector<StatsRow>& rows) {
    a.Clear(0);
    const float w = static_cast<float>(a.Width()), h = static_cast<float>(a.Height());
    a.FillRRect(2, 2, w - 2, h - 2, 24.0f, Ink::kSheet);
    a.Text(28, 46, 26.0f, true, Ink::kText, "Stats");
    const float top = 66.0f;
    const float row_h = std::min(30.0f, (h - top - 14.0f) / std::max<float>(1.0f, rows.size()));
    float y = top;
    for (const StatsRow& r : rows) {
        const float base = y + row_h * 0.72f;
        a.Text(28, base, 20.0f, false, Ink::kDim, r.label);
        a.Text(w - 28, base, 20.0f, true, Ink::kText, r.value, 2);
        y += row_h;
    }
}

// ---- The diagram (AzaharFrame ctrldiag) ---------------------------------------------------------

static_assert(kHelpRect.w == static_cast<int>(kDiagramPanelW) &&
              kHelpRect.h == static_cast<int>(kDiagramPanelH));

namespace {

// The angles of the drawings (controller_diagram.h), and the front one's size: kFrontScale of
// the top one's, drawn kFrontH tall and cut short under the trigger.
constexpr DiagramAngle kTopAngle{0.0f, 90.0f};
constexpr DiagramAngle kFrontAngle{180.0f, 165.0f};
constexpr float kFrontScale = 0.9f;
constexpr int kFrontH = 440;
constexpr uint32_t kLeader = 0xFF8C8C96u;

// The tabs, in the title row: [x0, x1) and the height of the row.
constexpr float kTabY0 = 38.0f, kTabY1 = 84.0f;
constexpr float kTabX[2][2] = {{262.0f, 404.0f}, {416.0f, 636.0f}};

void DrawLabel(PanelArt& a, const DiagramPlaced& p, float ox, float oy) {
    float y = oy + p.y0;
    const auto& lines = p.label->lines;
    for (std::size_t i = 0; i < lines.size(); i++) {
        const DiagramLine& l = lines[i];
        if (l.heading && i > 0) {
            y += kDiagramHeadingGap;
        }
        const float size = kDiagramLabelSize;
        const float base = y + kDiagramLabelLine * 0.5f + a.CapHeight(size) * 0.5f;
        float x = ox + p.x0 + (l.sub ? kDiagramSubIndent : 0.0f);
        if (l.heading) {
            a.Text(x, base, size, true, Ink::kAccent, l.key);
        } else {
            if (!l.key.empty()) {
                const std::string key = l.key + ": ";
                a.Text(x, base, size, l.title, Ink::kDim, key);
                x += a.TextWidth(size, l.title, key);
            }
            a.Text(x, base, size, l.title, Ink::kText, l.action);
        }
        y += kDiagramLabelLine;
    }
}

void PutOver(PanelArt& a, float x, float y, const DiagramImage& img) {
    a.PutOver(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)), img.rgba.data(),
              img.width, img.height);
}

} // namespace

HelpArt MakeHelpArt(const ModelFile& left, const ModelFile& right) {
    HelpArt art;
    const float top = DiagramScale({&left, &right}, kTopAngle, kDiagramTopW, kDiagramTopH);
    const float front = top * kFrontScale;
    const std::array<const ModelFile*, 2> models{&left, &right};
    for (int h = 0; h < 2; h++) {
        // The right hand is drawn as the left one, rolled the other way.
        const DiagramAngle ta{h == 0 ? kTopAngle.roll : -kTopAngle.roll, kTopAngle.pitch};
        const DiagramAngle fa{h == 0 ? kFrontAngle.roll : -kFrontAngle.roll, kFrontAngle.pitch};
        art.top[h] = DrawDiagram(*models[h], ta, top, kDiagramTopW, kDiagramTopH);
        const DiagramImage full = DrawDiagram(*models[h], fa, front, kDiagramFrontW, kFrontH);
        // From the top of the drawing to a little under the trigger and the bumper.
        int y0 = 0, y1 = 0;
        while (y0 < full.height) {
            bool any = false;
            for (int x = 0; x < full.width && !any; x++) {
                any = full.rgba[(static_cast<std::size_t>(y0) * full.width + x) * 4 + 3] > 0;
            }
            if (any) {
                break;
            }
            y0++;
        }
        for (const CtrlPart p : {CtrlPart::Bumper, CtrlPart::Trigger}) {
            if (full.anchor[static_cast<std::size_t>(p)]) {
                y1 = std::max(y1, static_cast<int>(full.bounds[static_cast<std::size_t>(p)][3]));
            }
        }
        art.front[h] = y1 > y0 ? CropDiagram(full, std::max(0, y0 - 2), y1 + 40, 36)
                               : CropDiagram(full, 0, full.height / 2, 36);
    }
    return art;
}

DiagramAnchors HelpArtAnchors(const HelpArt& art) {
    DiagramAnchors an;
    for (int h = 0; h < 2; h++) {
        for (std::size_t p = 0; p < kCtrlParts; p++) {
            const bool front = DiagramPartInFront(static_cast<CtrlPart>(p));
            const DiagramImage& img = front ? art.front[h] : art.top[h];
            const float ox = front ? DiagramFrontX(h) : DiagramTopX(h);
            const float oy = front ? DiagramFrontY(img.height) : kDiagramTopY;
            if (const auto& at = img.anchor[p]) {
                an.at[h][p] = DiagramPoint{at->x + ox, at->y + oy};
            }
            const auto& b = img.bounds[p];
            an.bounds[h][p] = {b[0] + ox, b[1] + oy, b[2] + ox, b[3] + oy};
        }
    }
    return an;
}

int HelpTabAt(float px, float py) {
    if (py < kTabY0 || py >= kTabY1) {
        return -1;
    }
    for (int t = 0; t < 2; t++) {
        if (px >= kTabX[t][0] && px < kTabX[t][1]) {
            return t;
        }
    }
    return -1;
}

void DrawHelpTabs(PanelArt& a, const HelpDiagram& d, DiagramPageId page, int hot) {
    const float x = static_cast<float>(kHelpRect.x), y = static_cast<float>(kHelpRect.y);
    for (int t = 0; t < 2; t++) {
        const float x0 = x + kTabX[t][0], x1 = x + kTabX[t][1];
        const float y0 = y + kTabY0, y1 = y + kTabY1;
        const bool on = static_cast<int>(page) == t;
        const float r = (y1 - y0) * 0.5f;
        // The tab's box lies inside the sheet: the sheet again, then the tab.
        a.ClearRect(static_cast<int>(x0) - 3, static_cast<int>(y0) - 3,
                    static_cast<int>(x1 - x0) + 6, static_cast<int>(y1 - y0) + 6);
        a.FillRRect(x0 - 3, y0 - 3, x1 + 3, y1 + 3, 0.0f, Ink::kSheet);
        a.FillRRect(x0, y0, x1, y1, r, on ? Ink::kKeyOn : hot == t ? Ink::kKeyHot : Ink::kKey);
        a.StrokeRRect(x0, y0, x1, y1, r, 2.0f,
                      on ? Ink::kKeyOnRim : hot == t ? Ink::kKeyRimHot : Ink::kKeyRim);
        const std::string& name = d.pages[static_cast<std::size_t>(t)].name;
        a.Text((x0 + x1) * 0.5f, (y0 + y1) * 0.5f + a.CapHeight(21.0f) * 0.5f, 21.0f, true,
               on ? Ink::kInkOnLight : Ink::kText, a.Fit(21.0f, true, name, x1 - x0 - 24.0f), 1);
    }
}

void DrawHelpDiagram(PanelArt& a, const HelpDiagram& d, DiagramPageId page, const HelpArt& art) {
    const AtlasRect R = kHelpRect;
    a.ClearRect(R.x, R.y, R.w, R.h);
    const float x = static_cast<float>(R.x), y = static_cast<float>(R.y);
    a.FillRRect(x + 2, y + 2, x + R.w - 2, y + R.h - 2, 36.0f, Ink::kSheet);
    a.Text(x + 44, y + 76, 40.0f, true, Ink::kText, "Controls");
    DrawHelpClose(a, false);
    DrawHelpTabs(a, d, page, -1);
    for (int h = 0; h < 2; h++) {
        PutOver(a, x + DiagramTopX(h), y + kDiagramTopY, art.top[h]);
        PutOver(a, x + DiagramFrontX(h), y + DiagramFrontY(art.front[h].height), art.front[h]);
    }
    const auto placed = LayoutHelpDiagram(
        d.pages[static_cast<std::size_t>(page)], HelpArtAnchors(art),
        [&a](float size, bool bold, const std::string& text) { return a.TextWidth(size, bold, text); });
    for (const DiagramPlaced& p : placed) {
        for (const DiagramLeader& l : p.leaders) {
            a.Line(x + l.from.x, y + l.from.y, x + l.to.x, y + l.to.y, 2.0f, kLeader);
            const float cx = x + l.dot.x, cy = y + l.dot.y;
            if (l.ring > 0.0f) {
                a.StrokeRRect(cx - l.ring, cy - l.ring, cx + l.ring, cy + l.ring, l.ring, 2.0f,
                              Ink::kAccent);
            } else {
                a.FillRRect(cx - 5.0f, cy - 5.0f, cx + 5.0f, cy + 5.0f, 5.0f, Ink::kAccent);
            }
        }
        DrawLabel(a, p, x, y);
    }
    a.FillRRect(x + 44, y + R.h - 58, x + R.w - 44, y + R.h - 56, 1.0f, Ink::kRule);
    a.Text(x + 44, y + R.h - 24, 18.0f, false, Ink::kDim, d.footer);
}

} // namespace FrameXr::Vis
