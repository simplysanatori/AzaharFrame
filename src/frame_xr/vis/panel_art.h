// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// Drawing for the panels of frame_xr/vis (controls help, stats, the Controls and Stats buttons):
// premultiplied RGBA8 pixels, sRGB values, rows top down; anti-aliased rounded rectangles and
// UTF-8 text in Noto Sans (Regular and Bold, SteamOS's own fonts, read with the stb_truetype the
// keyboard already builds), in the look of the headset keyboard (keyboard/art.cpp): a dark sheet,
// white text, light blue for what is lit. Without Noto Sans the text falls back to the picker's
// baked font (frame_xr/ui).

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "frame_xr/ui/ui_canvas.h"

namespace FrameXr::Vis {

/// Colours as 0xAARRGGBB, straight alpha.
namespace Ink {
constexpr uint32_t kSheet = 0xF0141416u;     ///< the keyboard's sheet
constexpr uint32_t kText = 0xFFFFFFFFu;
constexpr uint32_t kDim = 0xFFA8A8B0u;       ///< secondary text
constexpr uint32_t kAccent = 0xFF9CC9FFu;    ///< the keyboard's "locked" blue
constexpr uint32_t kRule = 0x30FFFFFFu;      ///< thin separators
constexpr uint32_t kKey = 0x28FFFFFFu;       ///< a button at rest (the keyboard's key)
constexpr uint32_t kKeyHot = 0x44FFFFFFu;    ///< under the ray
constexpr uint32_t kKeyRim = 0x50FFFFFFu;
constexpr uint32_t kKeyRimHot = 0x90FFFFFFu;
constexpr uint32_t kKeyOn = 0xEE9CC9FFu;     ///< switched on
constexpr uint32_t kKeyOnRim = 0xFFCFE4FFu;
constexpr uint32_t kInkOnLight = 0xFF141416u;
} // namespace Ink

class PanelArt {
public:
    PanelArt();
    ~PanelArt();
    PanelArt(const PanelArt&) = delete;
    PanelArt& operator=(const PanelArt&) = delete;

    void Resize(int width, int height);
    int Width() const {
        return w;
    }
    int Height() const {
        return h;
    }
    /// Premultiplied RGBA8, rows top down.
    const uint8_t* Pixels() const {
        return reinterpret_cast<const uint8_t*>(store.px.data());
    }

    void Clear(uint32_t argb = 0);
    /// Clears a rectangle to nothing (before a part of an atlas is drawn again).
    void ClearRect(int x, int y, int width, int height);
    /// Copies premultiplied RGBA8 pixels (rows top down) to x, y.
    void Put(int x, int y, const uint8_t* rgba, int width, int height);
    void FillRRect(float x0, float y0, float x1, float y1, float radius, uint32_t argb);
    void StrokeRRect(float x0, float y0, float x1, float y1, float radius, float width,
                     uint32_t argb);
    void Line(float x0, float y0, float x1, float y1, float width, uint32_t argb);

    /// UTF-8 text on a baseline; align 0 left of x, 1 centred on it, 2 right of it.
    void Text(float x, float baseline, float size, bool bold, uint32_t argb, const std::string& s,
              int align = 0);
    float TextWidth(float size, bool bold, const std::string& s) const;
    /// The text cut to `width` with "..." when it is wider.
    std::string Fit(float size, bool bold, const std::string& s, float width) const;
    /// The height of a capital at `size` (for centring a line on a point).
    float CapHeight(float size) const;

private:
    void Over(int x, int y, uint32_t argb, float coverage);

    struct Fonts;
    std::unique_ptr<Fonts> fonts;
    int w = 0, h = 0;
    frame_xr::ui::Canvas store; ///< the pixels (R in the low byte); its baked font is the fallback
};

} // namespace FrameXr::Vis
