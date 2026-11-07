// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// The headset keyboard's look, drawn on the CPU the way Moonlight XR draws its Compact keyboard
// (ui/panel_keyboard.c, ui/canvas.c, ui/text.c; same author): a dark rounded sheet, translucent
// rounded keys with a thin rim, Noto Sans Regular labels, Ctrl/Alt/Win lit white, locked ones
// blue with an underline and a dot, and the modifiers held shown on the space bar. Noto Sans is
// SteamOS's own (/usr/share/fonts/noto, SIL OFL 1.1) and is read at run time with stb_truetype;
// without it the labels fall back to the picker's baked font. Plus the two small pieces of art
// Moonlight XR shows with it as quads of their own: the ring over the key under the ray, and the
// move bar's pill.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace FrameXr::Keyboard {

/// What the keyboard shows.
struct View {
    int sheet = 0;  ///< Sheet
    int mods = 0;   ///< kMod bits lit (Ctrl, Alt, Win)
    int locked = 0; ///< kMod bits locked (Shift included)
    int hover = -1; ///< the key under the ray, or -1
    bool operator==(const View&) const = default;
};

class Art {
public:
    Art();
    ~Art();
    Art(const Art&) = delete;
    Art& operator=(const Art&) = delete;

    /// Draws the keyboard for `view` (kTexWidth x kTexHeight). Only the two keys whose hover
    /// changed are drawn again when nothing else did. False when nothing changed.
    bool Draw(const View& view);
    /// Premultiplied RGBA8, sRGB values, rows top down.
    const uint8_t* Pixels() const;

    /// The ring over the key under the ray: a rounded rectangle's rim, white, kRingSize square.
    static constexpr int kRingSize = 128;
    static std::vector<uint8_t> RingArt();
    /// The move bar's pill: a rounded bar, light grey, kPillWidth x kPillHeight.
    static constexpr int kPillWidth = 256;
    static constexpr int kPillHeight = 24;
    static std::vector<uint8_t> PillArt();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace FrameXr::Keyboard
