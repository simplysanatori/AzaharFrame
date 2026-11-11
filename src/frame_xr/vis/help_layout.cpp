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

// ---- AzaharFrame remap: Edit Controls' layout ------------------------------------------------

float EditChipWidth(const DiagramLine& l, const TextMeasure& measure) {
    return kEditChipPadL + kEditGlyph + kEditChipGap +
           measure(kEditChipText, true, l.key + ": " + l.action) + kEditChipPadR;
}

namespace {

struct EditGroup {
    const DiagramLabel* label = nullptr;
    int hand = 0;
    DiagramPoint at;       ///< what it points at (help panel pixels)
    float want = 0.0f;     ///< where it wants its middle (dp)
    float top = 0.0f;      ///< where it goes (dp)
    float height = 0.0f;
};

float GroupHeight(const DiagramLabel& l) {
    const float n = static_cast<float>(l.lines.size());
    return n * kEditChipH + (n - 1.0f) * kEditChipGap;
}

// One column, top to bottom in the order the groups want: each as near its want as the ones
// above allow; then up, by half of what the last was pushed (as far as the room over the first
// allows), and up again to end over the bottom.
void StackEdit(std::vector<EditGroup>& col, float y0, float y1) {
    std::stable_sort(col.begin(), col.end(),
                     [](const EditGroup& p, const EditGroup& q) { return p.want < q.want; });
    float next = y0;
    for (EditGroup& g : col) {
        g.top = std::max(next, g.want - g.height * 0.5f);
        next = g.top + g.height + kEditGroupGap;
    }
    if (col.empty()) {
        return;
    }
    const float pushed = col.back().top - (col.back().want - col.back().height * 0.5f);
    const float over = col.back().top + col.back().height - y1;
    const float up = std::min(col.front().top - y0, std::max(pushed * 0.5f, over));
    for (EditGroup& g : col) {
        g.top -= std::max(0.0f, up);
    }
}

} // namespace

EditLayout LayoutEditDiagram(const DiagramPage& page, const DiagramAnchors& an, float art_y0,
                             float art_y1, const EditBox& area, const TextMeasure& measure) {
    EditLayout out;
    std::vector<EditGroup> cols[2];
    float widest = 0.0f;
    for (const DiagramLabel& label : page.labels) {
        const int hand = label.hand == 1 ? 1 : 0;
        const auto centre = label.parts.empty() ? std::nullopt : Centre(an, hand, label.parts);
        if (label.lines.empty() || !centre) {
            continue;
        }
        for (const DiagramLine& l : label.lines) {
            widest = std::max(widest, EditChipWidth(l, measure));
        }
        EditGroup g;
        g.label = &label;
        g.hand = hand;
        g.at = *centre;
        g.height = GroupHeight(label);
        cols[hand].push_back(g);
    }

    // The drawings: centred, as large as the columns (both as wide as the widest chip) and the
    // height allow.
    const float art_x0 = DiagramTopX(0), art_x1 = DiagramTopX(1) + kDiagramTopW;
    const float room_w = area.x1 - area.x0 - 2.0f * (widest + kEditLeaderMin);
    const float sc = std::max(0.1f, std::min({kEditMaxScale, room_w / (art_x1 - art_x0),
                                              (area.y1 - area.y0) / (art_y1 - art_y0)}));
    out.sc = sc;
    out.ox = (area.x0 + area.x1) * 0.5f - (art_x0 + art_x1) * 0.5f * sc;
    out.oy = (area.y0 + area.y1) * 0.5f - (art_y0 + art_y1) * 0.5f * sc;
    const auto P = [&out](const DiagramPoint& p) {
        return DiagramPoint{out.ox + p.x * out.sc, out.oy + p.y * out.sc};
    };
    // The columns' inner edges: kEditLeaderMin from the drawings.
    const float inner[2] = {out.ox + art_x0 * sc - kEditLeaderMin,
                            out.ox + art_x1 * sc + kEditLeaderMin};

    for (int h = 0; h < 2; h++) {
        for (EditGroup& g : cols[h]) {
            g.want = P(g.at).y;
        }
        StackEdit(cols[h], area.y0, area.y1);
        for (const EditGroup& g : cols[h]) {
            // The chips of a group as wide as its widest, so it reads as one set.
            float w = 0.0f;
            for (const DiagramLine& l : g.label->lines) {
                w = std::max(w, EditChipWidth(l, measure));
            }
            float y = g.top;
            for (const DiagramLine& l : g.label->lines) {
                EditChip c;
                c.r = h == 0 ? EditBox{inner[0] - w, y, inner[0], y + kEditChipH}
                             : EditBox{inner[1], y, inner[1] + w, y + kEditChipH};
                c.line = &l;
                out.chips.push_back(c);
                y += kEditChipH + kEditChipGap;
            }
            // The leader: from the chip's side, or from a bracket by a group's chips.
            EditLeader leader;
            const float side = h == 0 ? -1.0f : 1.0f; // towards the column
            const float first = g.top + kEditChipH * 0.5f;
            const float last = g.top + g.height - kEditChipH * 0.5f;
            DiagramPoint start{inner[h], first};
            if (g.label->lines.size() > 1) {
                const float bx = inner[h] - side * kEditBracket;
                for (float cy = first; cy <= last + 0.5f; cy += kEditChipH + kEditChipGap) {
                    leader.lines.push_back({DiagramPoint{inner[h] - side * 4.0f, cy}, DiagramPoint{bx, cy}});
                }
                leader.lines.push_back({DiagramPoint{bx, first}, DiagramPoint{bx, last}});
                start = {bx, (first + last) * 0.5f};
            } else {
                start.x -= side * 4.0f;
            }
            const DiagramPoint dot = P(g.at);
            leader.dot = dot;
            leader.ring = Ring(an, h, g.label->parts, g.at) * sc;
            DiagramPoint end = dot;
            if (leader.ring > 0.0f) {
                const float dx = start.x - dot.x, dy = start.y - dot.y;
                const float len = std::max(1.0f, std::hypot(dx, dy));
                end = {dot.x + dx / len * leader.ring, dot.y + dy / len * leader.ring};
            }
            leader.lines.push_back({start, end});
            out.leaders.push_back(std::move(leader));
        }
    }
    return out;
}

} // namespace FrameXr::Vis

