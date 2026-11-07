// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame screenswap
// The sizes of the controls help's two columns, worked out from the text so no label is cut: one
// text size for both columns (the largest that fits, at most `max_size`), and in each column the
// action text starts after the widest control label. The measure is a callback, so the logic is
// unit tested without a font (src/tests/frame_xr/vis.cpp).

#pragma once

#include <functional>
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

} // namespace FrameXr::Vis
