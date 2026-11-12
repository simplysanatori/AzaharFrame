// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame screenswap
// The sizes of the controls help's two columns, worked out from the text so no label is cut: one
// text size for both columns (the largest that fits, at most `max_size`), and in each column the
// action text starts after the widest control label. The measure is a callback, so the logic is
// unit tested without a font (src/tests/frame_xr/vis.cpp).

#pragma once

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "frame_xr/vis/help_content.h"

namespace FrameXr::Vis {

/// The width in pixels of `text` at `size`, bold or not.
using TextMeasure = std::function<float(float size, bool bold, const std::string& text)>;

struct HelpColumnSpan {
    float x0 = 0.0f; ///< left edge of the column
    float x1 = 0.0f; ///< right edge
};

struct HelpColumnLayout {
    float action_x = 0.0f; ///< where the action text starts
};

struct HelpTextLayout {
    float size = 0.0f; ///< text size of the rows
    HelpColumnLayout game;
    HelpColumnLayout window;
    bool fits = true;  ///< false: even the smallest size cuts something
};

constexpr float kHelpMinTextSize = 11.0f;
constexpr float kHelpColumnGap = 20.0f; ///< between a control label and its action

HelpTextLayout LayoutHelpText(const HelpPage& page, const HelpColumnSpan& game_span,
                              const HelpColumnSpan& window_span, float max_size,
                              const TextMeasure& measure);

// ---- AzaharFrame ctrldiag: the diagram's layout ----
// Panel pixels (the help panel, kHelpRect in panels.h). The drawings: each hand from above in a
// kDiagramTopW x kDiagramTopH box, the two side by side in the middle like the halves of a
// gamepad, and over each, the same hand from the front, cut short under the trigger. The labels:
// a column on the outer side of each hand, the grips' label under the drawings.

constexpr float kDiagramPanelW = 1240.0f, kDiagramPanelH = 900.0f;
constexpr int kDiagramTopW = 200, kDiagramTopH = 340;
constexpr float kDiagramTopY = 300.0f;
constexpr float kDiagramTopGap = 16.0f; ///< between the two boxes
constexpr int kDiagramFrontW = 200;
constexpr float kDiagramTop = 104.0f;    ///< under the title row
constexpr float kDiagramBottom = 830.0f; ///< over the footer
constexpr float kDiagramLabelSize = 20.0f;
constexpr float kDiagramLabelLine = 27.0f;
constexpr float kDiagramSubIndent = 18.0f;
constexpr float kDiagramHeadingGap = 5.0f;

float DiagramTopX(int hand);
float DiagramFrontX(int hand);
/// The front drawing's top: just over the top one, `height` pixels tall.
float DiagramFrontY(int height);

/// Where each part of each hand shows, in panel pixels (the top drawings; the bumper and the
/// trigger in the front ones), and its bounds (x0, y0, x1, y1).
struct DiagramAnchors {
    std::array<std::array<std::optional<DiagramPoint>, kCtrlParts>, 2> at{};
    std::array<std::array<std::array<float, 4>, kCtrlParts>, 2> bounds{};
};

/// The bumper and the trigger are pointed at in the front drawing, the rest in the top one.
constexpr bool DiagramPartInFront(CtrlPart part) {
    return part == CtrlPart::Bumper || part == CtrlPart::Trigger;
}

/// A leader line: from a part (or a ring round a group of parts) to its label.
struct DiagramLeader {
    DiagramPoint from, to;
    DiagramPoint dot;  ///< the part's mark, or the middle of the ring
    float ring = 0.0f; ///< the radius of a ring round a group of parts, or 0
};

struct DiagramPlaced {
    const DiagramLabel* label = nullptr;
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    std::vector<DiagramLeader> leaders; ///< one, or one a hand (the grips)
};

/// The width of one label line.
float DiagramLineWidth(const DiagramLine& line, const TextMeasure& measure);

/// Where the labels of `page` go and their leader lines. A label whose parts show nowhere is
/// left out.
std::vector<DiagramPlaced> LayoutHelpDiagram(const DiagramPage& page, const DiagramAnchors& anchors,
                                             const TextMeasure& measure);

} // namespace FrameXr::Vis
