// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "frame_xr/vis/help_layout.h"

namespace FrameXr::Vis {

namespace {

struct Widest {
    float control = 0.0f;
    float action = 0.0f;
};

Widest Measure(const std::vector<HelpRow>& rows, float size, const TextMeasure& measure) {
    Widest w;
    for (const HelpRow& r : rows) {
        if (r.kind != HelpRow::Kind::Row) {
            continue;
        }
        w.control = std::max(w.control, measure(size, true, r.control));
        w.action = std::max(w.action, measure(size, false, r.action));
    }
    return w;
}

bool Fits(const Widest& w, const HelpColumnSpan& span) {
    return w.control + kHelpColumnGap + w.action <= span.x1 - span.x0;
}

} // namespace

HelpTextLayout LayoutHelpText(const HelpPage& page, const HelpColumnSpan& game_span,
                              const HelpColumnSpan& window_span, float max_size,
                              const TextMeasure& measure) {
    HelpTextLayout out;
    float size = max_size;
    while (true) {
        const Widest g = Measure(page.game, size, measure);
        const Widest w = Measure(page.window, size, measure);
        const bool fits = Fits(g, game_span) && Fits(w, window_span);
        if (fits || size <= kHelpMinTextSize) {
            out.size = size;
            out.fits = fits;
            out.game.action_x = game_span.x0 + g.control + kHelpColumnGap;
            out.window.action_x = window_span.x0 + w.control + kHelpColumnGap;
            return out;
        }
        size = std::max(kHelpMinTextSize, size - 0.5f);
    }
}

} // namespace FrameXr::Vis
