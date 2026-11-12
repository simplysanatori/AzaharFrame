// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

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

// ---- AzaharFrame ctrldiag -----------------------------------------------------------------------

float DiagramTopX(int hand) {
    return hand == 0 ? kDiagramPanelW * 0.5f - kDiagramTopGap * 0.5f - kDiagramTopW
                     : kDiagramPanelW * 0.5f + kDiagramTopGap * 0.5f;
}

float DiagramFrontX(int hand) {
    return DiagramTopX(hand) + (kDiagramTopW - kDiagramFrontW) * 0.5f;
}

float DiagramFrontY(int height) {
    return std::max(kDiagramTop, kDiagramTopY - 6.0f - static_cast<float>(height));
}

float DiagramLineWidth(const DiagramLine& l, const TextMeasure& measure) {
    const float indent = l.sub ? kDiagramSubIndent : 0.0f;
    if (l.heading) {
        return indent + measure(kDiagramLabelSize, true, l.key);
    }
    const std::string key = l.key.empty() ? std::string{} : l.key + ": ";
    return indent + measure(kDiagramLabelSize, l.title, key) +
           measure(kDiagramLabelSize, l.title, l.action);
}

namespace {

constexpr float kColumnGap = 18.0f; ///< between a column and the drawings
constexpr float kEdge = 36.0f;      ///< the nearest a label comes to the panel's side
constexpr float kBlockGap = 16.0f;
constexpr float kAttachGap = 10.0f; ///< between a leader's end and its label

struct Block {
    DiagramPlaced placed;
    float want_y = 0.0f; ///< where it wants the middle of its first line
};

// Where a label wants its first line. Most beside what they point at; View and Menu sit on the
// top of the face buttons, over the head (their lines then pass over the D-pad and Y), and the
// sticks, on the inner side, at the bottom of their column (their lines pass under the D-pad and
// the face buttons).
float WantY(CtrlPart part, float anchor_y) {
    switch (part) {
    case CtrlPart::View:
    case CtrlPart::Menu:
        return kDiagramTopY - 4.0f;
    case CtrlPart::Stick:
        return kDiagramTopY + kDiagramTopH - 24.0f;
    default:
        return anchor_y;
    }
}

// One column: in the order they want, each first line as near where it wants as the blocks
// above it allow. When that pushes the last one down, the column moves up into the room over
// its first one (as much as the last was pushed), then as far as needed to end over the bottom.
void Stack(std::vector<Block>& col) {
    std::stable_sort(col.begin(), col.end(),
                     [](const Block& p, const Block& q) { return p.want_y < q.want_y; });
    float next = kDiagramTop;
    for (Block& b : col) {
        const float h = b.placed.y1 - b.placed.y0;
        b.placed.y0 = std::max(next, b.want_y - kDiagramLabelLine * 0.5f);
        b.placed.y1 = b.placed.y0 + h;
        next = b.placed.y1 + kBlockGap;
    }
    if (!col.empty()) {
        const float pushed = col.back().placed.y0 - (col.back().want_y - kDiagramLabelLine * 0.5f);
        const float up = std::min(pushed, col.front().placed.y0 - kDiagramTop);
        for (Block& b : col) {
            b.placed.y0 -= std::max(0.0f, up);
            b.placed.y1 -= std::max(0.0f, up);
        }
    }
    float limit = kDiagramBottom;
    for (auto it = col.rbegin(); it != col.rend(); ++it) {
        const float h = it->placed.y1 - it->placed.y0;
        it->placed.y1 = std::min(it->placed.y1, limit);
        it->placed.y0 = it->placed.y1 - h;
        limit = it->placed.y0 - kBlockGap;
    }
}

// The middle of the parts of `hand` a label points at, if any of them shows.
std::optional<DiagramPoint> Centre(const DiagramAnchors& an, int hand,
                                   const std::vector<CtrlPart>& parts) {
    float x = 0.0f, y = 0.0f;
    int n = 0;
    for (const CtrlPart p : parts) {
        if (const auto& at = an.at[hand][static_cast<std::size_t>(p)]) {
            x += at->x;
            y += at->y;
            n++;
        }
    }
    if (n == 0) {
        return std::nullopt;
    }
    return DiagramPoint{x / n, y / n};
}

// For a group of parts (the face buttons): the radius of a ring round them, else 0.
float Ring(const DiagramAnchors& an, int hand, const std::vector<CtrlPart>& parts,
           const DiagramPoint& centre) {
    if (parts.size() < 2) {
        return 0.0f;
    }
    float r = 0.0f;
    for (const CtrlPart p : parts) {
        if (!an.at[hand][static_cast<std::size_t>(p)]) {
            continue;
        }
        const auto& b = an.bounds[hand][static_cast<std::size_t>(p)];
        for (const float bx : {b[0], b[2]}) {
            for (const float by : {b[1], b[3]}) {
                r = std::max(r, std::hypot(bx - centre.x, by - centre.y));
            }
        }
    }
    // The bounds' corners overstate a round button: a little inside them, a little out.
    return r * 0.92f + 4.0f;
}

// Where a leader from `p` meets the label: its side facing p, by the first line, or its top or
// bottom edge when p is above or below it.
DiagramPoint Attach(const DiagramPlaced& b, const DiagramPoint& p) {
    const float first = b.y0 + kDiagramLabelLine * 0.5f;
    if (p.x < b.x0 - kAttachGap) {
        return {b.x0 - kAttachGap, first};
    }
    if (p.x > b.x1 + kAttachGap) {
        return {b.x1 + kAttachGap, first};
    }
    const float x = std::clamp(p.x, b.x0 + 12.0f, b.x1 - 12.0f);
    return p.y < b.y0 ? DiagramPoint{x, b.y0 - kAttachGap * 0.5f}
                      : DiagramPoint{x, b.y1 + kAttachGap * 0.5f};
}

} // namespace

std::vector<DiagramPlaced> LayoutHelpDiagram(const DiagramPage& page, const DiagramAnchors& an,
                                             const TextMeasure& measure) {
    std::vector<Block> left, right, under;
    for (const DiagramLabel& label : page.labels) {
        const int hand = label.hand == 2 ? 0 : label.hand;
        const auto centre = label.parts.empty() ? std::nullopt : Centre(an, hand, label.parts);
        if (label.lines.empty() || !centre) {
            continue;
        }
        float width = 0.0f, height = 0.0f;
        for (std::size_t i = 0; i < label.lines.size(); i++) {
            width = std::max(width, DiagramLineWidth(label.lines[i], measure));
            height += kDiagramLabelLine + (label.lines[i].heading && i > 0 ? kDiagramHeadingGap : 0.0f);
        }
        Block b;
        b.placed.label = &label;
        b.want_y = WantY(label.parts[0], centre->y);
        if (label.hand == 2) {
            b.placed.x0 = kDiagramPanelW * 0.5f - width * 0.5f;
            b.placed.y0 = kDiagramTopY + kDiagramTopH + 10.0f;
        } else if (label.hand == 0) {
            // A label too wide for its column keeps to the panel and comes nearer the drawing.
            b.placed.x0 = std::max(kEdge, DiagramTopX(0) - kColumnGap - width);
        } else {
            b.placed.x0 = std::min(kDiagramPanelW - kEdge - width,
                                   DiagramTopX(1) + kDiagramTopW + kColumnGap);
        }
        b.placed.x1 = b.placed.x0 + width;
        b.placed.y1 = b.placed.y0 + height;
        (label.hand == 2 ? under : label.hand == 0 ? left : right).push_back(std::move(b));
    }
    Stack(left);
    Stack(right);

    const auto lead = [&an](DiagramPlaced& p) {
        p.leaders.clear();
        for (int h = 0; h < 2; h++) {
            if (p.label->hand != 2 && p.label->hand != h) {
                continue;
            }
            const auto centre = Centre(an, h, p.label->parts);
            if (!centre) {
                continue;
            }
            DiagramLeader l;
            l.dot = *centre;
            l.ring = Ring(an, h, p.label->parts, *centre);
            l.to = Attach(p, *centre);
            l.from = *centre;
            if (l.ring > 0.0f) {
                const float dx = l.to.x - centre->x, dy = l.to.y - centre->y;
                const float len = std::max(1.0f, std::hypot(dx, dy));
                l.from = {centre->x + dx / len * l.ring, centre->y + dy / len * l.ring};
            }
            p.leaders.push_back(l);
        }
    };
    std::vector<DiagramPlaced> out;
    for (auto* col : {&left, &right}) {
        for (Block& b : *col) {
            lead(b.placed);
            out.push_back(std::move(b.placed));
        }
    }
    // The grips' label: as high under the drawings as the columns' lines let it be (a column
    // pushed down by a long label sends its last line under the drawings).
    const auto clear = [&out](const DiagramPlaced& p) {
        for (const DiagramPlaced& q : out) {
            if (p.x0 < q.x1 + kBlockGap && q.x0 < p.x1 + kBlockGap && p.y0 < q.y1 + kBlockGap &&
                q.y0 < p.y1 + kBlockGap) {
                return false;
            }
            for (const DiagramLeader& l : q.leaders) {
                const float x0 = p.x0 - 8.0f, x1 = p.x1 + 8.0f, y0 = p.y0 - 8.0f, y1 = p.y1;
                for (int i = 0; i <= 64; i++) {
                    const float t = static_cast<float>(i) / 64.0f;
                    const float x = l.from.x + (l.to.x - l.from.x) * t;
                    const float y = l.from.y + (l.to.y - l.from.y) * t;
                    if (x > x0 && x < x1 && y > y0 && y < y1) {
                        return false;
                    }
                }
            }
        }
        return true;
    };
    for (Block& b : under) {
        // Down as far as the footer, and a little to either side, the nearest place that is clear.
        const DiagramPlaced base = b.placed;
        DiagramPlaced& p = b.placed;
        bool found = false;
        for (float dy = 0.0f; !found && base.y1 + dy <= kDiagramBottom; dy += 2.0f) {
            for (const float dx : {0.0f, 12.0f, -12.0f, 24.0f, -24.0f, 36.0f, -36.0f, 48.0f, -48.0f}) {
                p = base;
                p.x0 += dx;
                p.x1 += dx;
                p.y0 += dy;
                p.y1 += dy;
                if (clear(p)) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            p = base;
        }
        lead(p);
        out.push_back(std::move(p));
    }
    return out;
}

} // namespace FrameXr::Vis

