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

// ---- AzaharFrame remap: Edit Controls' layout ----
// In dp, the menu's units: Material 3 chips one step up for the panel (32 -> 40 dp tall, 8 dp
// corners, a 24 dp leading slot for the 3DS button's glyph, Label Large text one step up), in a
// column on the outer side of each hand, the drawings between them as large as the columns
// leave room for. A group of chips (the D-pad, the face buttons) has one leader, from a bracket.

constexpr float kEditChipH = 40.0f;
constexpr float kEditChipRadius = 8.0f;
constexpr float kEditChipText = 16.0f;
constexpr float kEditGlyph = 24.0f;
constexpr float kEditChipPadL = 8.0f;  ///< before the glyph
constexpr float kEditChipGap = 8.0f;   ///< glyph to text, and between the chips of a group
constexpr float kEditChipPadR = 16.0f; ///< after the text
constexpr float kEditGroupGap = 16.0f; ///< between groups
constexpr float kEditLeaderMin = 28.0f; ///< the columns' inner edges to the drawings' sides
constexpr float kEditBracket = 12.0f;   ///< a group's bracket from its chips' inner edge
constexpr float kEditMaxScale = 0.75f;  ///< dp per help panel pixel, at most

struct EditBox {
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
};

struct EditChip {
    EditBox r;
    const DiagramLine* line = nullptr;
};

struct EditLeader {
    std::vector<std::array<DiagramPoint, 2>> lines; ///< the bracket's, then the leader
    DiagramPoint dot;  ///< the part's mark, or the middle of the ring
    float ring = 0.0f; ///< the radius of a ring round a group of parts (dp), or 0
};

struct EditLayout {
    float sc = 1.0f, ox = 0.0f, oy = 0.0f; ///< help panel pixels to dp: ox + x * sc
    std::vector<EditChip> chips;
    std::vector<EditLeader> leaders;
};

/// The width of a chip of `line` (dp).
float EditChipWidth(const DiagramLine& line, const TextMeasure& measure);

/// Lays out `page` (BuildEditPage) in `area` (dp): `an` in help panel pixels, the drawings from
/// `art_y0` to `art_y1` (help panel pixels) between kDiagramTopX(0) and its right edge.
EditLayout LayoutEditDiagram(const DiagramPage& page, const DiagramAnchors& an, float art_y0,
                             float art_y1, const EditBox& area, const TextMeasure& measure);

} // namespace FrameXr::Vis
