// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: TrueType text and icons for the canvas (ui_canvas.h) at any size, read with
// the vendored stb_truetype (keyboard/stb_truetype.h). The same drawing as the vis panels'
// (vis/panel_art.cpp): quarter-pixel pen positions, glyphs cached per size, coverage a little
// heavier. Fonts come from memory (the menu's embedded ones, menu/md3_fonts.h) or a file.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "frame_xr/ui/ui_canvas.h"

namespace frame_xr::ui {

class TtfFont {
public:
    TtfFont();
    ~TtfFont();
    TtfFont(const TtfFont&) = delete;
    TtfFont& operator=(const TtfFont&) = delete;

    /// The font file's bytes, base64-encoded (whitespace ignored). False when it is no font.
    bool LoadBase64(const char* base64);
    bool LoadFile(const std::string& path);
    bool Ok() const;

    /// Width of UTF-8 `s` at `px` pixels per em, with kerning.
    float Width(float px, const std::string& s) const;
    /// Height of a capital H at `px` (to centre a line on a point: baseline = y + CapHeight / 2).
    float CapHeight(float px) const;
    /// `s` cut to `width` with "..." when it is wider.
    std::string Fit(float px, const std::string& s, float width) const;
    /// UTF-8 text on a baseline; align 0 left of x, 1 centred on it, 2 right of it. Returns the
    /// width drawn.
    float Draw(Canvas& cv, float x, float baseline, float px, const Color& c, const std::string& s,
               int align = 0);
    /// One code point (an icon) centred on (cx, cy), its em `px` pixels square.
    void DrawCentred(Canvas& cv, float cx, float cy, float px, char32_t code, const Color& c);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// The next code point of UTF-8 `s` from byte `i` (advanced past it).
char32_t NextUtf8(const std::string& s, std::size_t& i);

} // namespace frame_xr::ui
