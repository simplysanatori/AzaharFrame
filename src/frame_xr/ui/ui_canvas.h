// Copyright 2026 CitraFrame contributors
// Licensed under GPLv3 or any later version
//
// A CPU canvas for CitraFrame's own panels (the in-VR application picker):
// anti-aliased rounded shapes, scaled image blits and text from the baked
// glyph atlas in ui_font.h, into premultiplied RGBA8 pixels (R in the low
// byte, sRGB-encoded colours), rows top down.
//
// Adapted from GalaxyFrame's ui_canvas (same author), with fill, outline,
// image blit and text-fitting helpers added.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace frame_xr::ui {

enum { kFontLarge, kFontSmall };

struct Color {
    float r, g, b, a; // 0..255, alpha 0..1
};
inline Color rgb(int r, int g, int b, float a = 1.0f) {
    return {static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), a};
}

/// Decodes the glyph atlases; call once before drawing or measuring text.
void InitFonts();
/// Width of `s` in pixels. InitFonts() must have been called.
float TextWidth(int font, const char* s);

/// Breaks `s` at spaces into at most `max_lines` lines no wider than `width`.
/// Words wider than a line are split by character. If the text does not fit,
/// the last line is shortened and ends in "...". Returns no lines for "".
std::vector<std::string> WrapText(int font, const std::string& s, float width, int max_lines);

struct Canvas {
    int w = 0, h = 0;
    std::vector<uint32_t> px;

    void Init(int width, int height);
    void Clear(const Color& c);
    void Blend(int x, int y, const Color& c, float coverage);
    void FillRect(float x0, float y0, float x1, float y1, const Color& c);
    void RoundRect(float x0, float y0, float x1, float y1, float radius, const Color& c);
    /// Outline of a rounded rectangle, `thickness` wide, centred on the edge.
    void StrokeRoundRect(float x0, float y0, float x1, float y1, float radius, float thickness,
                         const Color& c);
    void Circle(float cx, float cy, float r, const Color& c);
    /// Draws straight (non-premultiplied) RGBA8 pixels `src` (sw x sh, rows top down)
    /// scaled into the destination rectangle, bilinear when `smooth`, else nearest.
    /// `radius` rounds the destination corners; `alpha` multiplies the image alpha.
    void BlitRGBA(const uint8_t* src, int sw, int sh, float x0, float y0, float x1, float y1,
                  bool smooth = true, float radius = 0.0f, float alpha = 1.0f);
    /// Text on a baseline; align 0 = left of x, 1 = centred on it, 2 = right.
    void Text(float x, float baseline, int font, const char* s, const Color& c, int align = 0);
    /// Text broken into lines at spaces to fit `width`, from the baseline
    /// `baseline` down in steps of `line_height`; returns the baseline after
    /// the last line.
    float TextWrapped(float x, float baseline, float width, float line_height, int font,
                      const char* s, const Color& c);
};

} // namespace frame_xr::ui
