// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame padsui: Edit Controls' drawing of a 3DS: an open New 3DS XL from the front, both
// screens, in line art made of the menu kit's shapes (our own drawing, no logos), with a dot on
// every input Edit Controls shows. The drawing is laid out in its own units (160 across, about a
// millimetre; 174 tall open) and scaled into the space it gets.

#pragma once

#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/remap.h"

namespace FrameXr::Menu {

/// The drawing's inputs: the 3DS buttons (DsButton) and the two sticks.
constexpr int kDsCirclePad = DsButton::CirclePad;
constexpr int kDsCStick = DsButton::CStick;

constexpr float kDsUnitsW = 160.0f;
constexpr float kDsUnitsH = 174.0f;

struct DsPoint {
    float x = 0, y = 0;
};

struct DsDrawing {
    Rect box;          ///< where it is (dp)
    float s = 1.0f;    ///< dp per unit
    /// Where the input `target` is (dp): its dot.
    DsPoint Anchor(int target) const;
    /// The radius of the input's own outline (dp), for the listening ring.
    float Radius(int target) const;
    /// The bottom screen's picture (dp), for the Pointer Test.
    Rect BottomScreen() const;
};

/// The largest drawing that fits `area`, centred.
DsDrawing PlaceDs(const Rect& area);

/// Draws the console; `lit` (an input, or -1) gets a ring in the primary colour.
void DrawDs(Kit& kit, const DsDrawing& d, int lit);
/// The dot on an input (larger when lit), at the end of its leader line.
void DrawDsDot(Kit& kit, const DsDrawing& d, int target, bool lit);

/// A 3DS input's glyph in `box` (dp): the button as it looks on the console.
void DrawDsGlyph(Kit& kit, const Rect& box, int target, uint32_t fg, uint32_t bg);

} // namespace FrameXr::Menu
