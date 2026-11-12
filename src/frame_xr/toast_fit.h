// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: a toast's box sized to its text (hud.cpp), so no toast is cut. A row of the
// toast atlas is kAtlasW pixels wide; the box is as wide as the text and its padding, at least
// kMinBoxPx (the size every toast had), on one line in the large font. Text too long for a row
// goes on two lines in the small font. The quad grows with the box at the same scale up to
// kMaxWidthM; past that the whole toast is shown smaller. Pure (unit-tested).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace FrameXr {

/// AzaharFrame ui3: how long a toast stays, unless a newer toast replaces it. Short: the answer to
/// a press just made, whose effect shows (a value, a mode: "Mouse: Pointer", "Offset Right 1.5°").
/// Long: a one-shot message about something done in the background, applied later, or refused
/// ("<Mod> Downloaded. Restart to Enable.", "Texture Pack Not Changed", "Quick Saved"), which must
/// not be missed while playing.
enum class ToastTime : uint8_t { Short, Long };

} // namespace FrameXr

namespace FrameXr::ToastFit {

constexpr int64_t kShortNs = 2'000'000'000; ///< ToastTime::Short
constexpr int64_t kLongNs = 5'000'000'000;  ///< ToastTime::Long

constexpr int64_t DurationNs(ToastTime time) {
    return time == ToastTime::Long ? kLongNs : kShortNs;
}

constexpr int kAtlasW = 1024;
constexpr int kRowPx = 96;
constexpr int kMinBoxPx = 512;
constexpr float kPadPx = 40.0f;               ///< each side of the text
constexpr float kMetresPerPx = 0.45f / 512.0f; ///< the toasts' scale (0.45 m for 512 px)
constexpr float kMaxWidthM = 0.6f;

struct Box {
    int box_px = kMinBoxPx; ///< the box's width in the row (from its left edge)
    bool two_lines = false; ///< the small font, two lines
    float width_m = 0.45f;  ///< the quad
    float height_m = 0.45f * kRowPx / kMinBoxPx;
};

/// `one_line_px`: the text's width in the large font; `two_lines_px`: the wider of its two lines
/// in the small font (asked for only when one line does not fit).
template <typename TwoLines>
Box Fit(float one_line_px, TwoLines two_lines_px) {
    Box b;
    float need = one_line_px + 2.0f * kPadPx;
    if (need > static_cast<float>(kAtlasW)) {
        b.two_lines = true;
        need = static_cast<float>(two_lines_px()) + 2.0f * kPadPx;
    }
    b.box_px = std::clamp(static_cast<int>(std::ceil(need)), kMinBoxPx, kAtlasW);
    b.width_m = std::min(kMetresPerPx * static_cast<float>(b.box_px), kMaxWidthM);
    b.height_m = b.width_m * static_cast<float>(kRowPx) / static_cast<float>(b.box_px);
    return b;
}

} // namespace FrameXr::ToastFit
