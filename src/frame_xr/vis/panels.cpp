// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>

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

} // namespace FrameXr::Vis
