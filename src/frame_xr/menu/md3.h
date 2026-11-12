// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: Material 3 for the in-VR menu, drawn by the project's CPU canvas
// (frame_xr/ui). A dark Material 3 colour scheme (tonal spot, from the port's accent #9CC9FF),
// Google Sans Flex text and Material Symbols Rounded icons (both embedded, md3_fonts.h), and the
// components the menu uses: navigation rail items, filled and tonal buttons, segmented buttons,
// switches, sliders, menu buttons and menus, list rows, plain tooltips, each with Material's
// state layers (hover 8 %, pressed 10 %, disabled 38 %).
//
// Everything is laid out in dp; the kit turns dp into canvas pixels (px_per_dp). On the panel,
// 1 dp is 1.25 mm at the menu's 1.2 m (about 1 mm at 1 m, 0.06 degrees).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "frame_xr/ui/ui_canvas.h"
#include "frame_xr/ui/ui_ttf.h"

namespace FrameXr::Menu {

using frame_xr::ui::Canvas;
using frame_xr::ui::Color;

/// Material 3 dark scheme tokens, 0xRRGGBB (steam-frame/dev/make-md3-fonts.py notes how they
/// were made: material-color-utilities, SchemeTonalSpot, 2021 spec, dark, seed #9CC9FF).
namespace Tok {
constexpr uint32_t kPrimary = 0xA0CAFD;
constexpr uint32_t kOnPrimary = 0x003258;
constexpr uint32_t kPrimaryContainer = 0x194975;
constexpr uint32_t kOnPrimaryContainer = 0xD1E4FF;
constexpr uint32_t kSecondary = 0xBBC7DB;
constexpr uint32_t kSecondaryContainer = 0x3B4858;
constexpr uint32_t kOnSecondaryContainer = 0xD6E3F7;
constexpr uint32_t kSurface = 0x111418;
constexpr uint32_t kSurfaceContainerLow = 0x191C20;
constexpr uint32_t kSurfaceContainer = 0x1D2024;
constexpr uint32_t kSurfaceContainerHigh = 0x272A2F;
constexpr uint32_t kSurfaceContainerHighest = 0x32353A;
constexpr uint32_t kOnSurface = 0xE1E2E8;
constexpr uint32_t kOnSurfaceVariant = 0xC3C7CF;
constexpr uint32_t kOutline = 0x8D9199;
constexpr uint32_t kOutlineVariant = 0x42474E;
constexpr uint32_t kInverseSurface = 0xE1E2E8;
constexpr uint32_t kInverseOnSurface = 0x2E3135;
constexpr uint32_t kInversePrimary = 0x35618E; ///< AzaharFrame remap: snackbar actions (primary tone 40)
constexpr uint32_t kError = 0xFFB4AB;
constexpr uint32_t kErrorContainer = 0x93000A;
constexpr uint32_t kOnErrorContainer = 0xFFDAD6;
} // namespace Tok

/// Material state layer opacities.
constexpr float kHoverLayer = 0.08f;
constexpr float kPressedLayer = 0.10f;
constexpr float kDisabledContent = 0.38f;
constexpr float kDisabledContainer = 0.12f;

enum class State { Rest, Hover, Pressed, Disabled };

/// The type styles of the menu: Material's type scale, one step larger for the panel's angular
/// size (sizes in dp; Medium = weight 500).
enum class Type {
    RailLabel, ///< Label Large: 14, Medium
    Label,     ///< buttons, segments: 16, Medium
    Body,      ///< list row labels: 18
    Value,     ///< values (stats, sliders): 18, Medium
    Title,     ///< the filled button: 20, Medium
    Headline,  ///< the game's title: 26
    Tooltip,   ///< plain tooltips: 14
};

struct Rect {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float W() const {
        return x1 - x0;
    }
    float H() const {
        return y1 - y0;
    }
    float Cx() const {
        return (x0 + x1) * 0.5f;
    }
    float Cy() const {
        return (y0 + y1) * 0.5f;
    }
    bool Contains(float x, float y) const {
        return x >= x0 && x < x1 && y >= y0 && y < y1;
    }
};

enum class ButtonStyle { Filled, Tonal, Danger, Text };

class Kit {
public:
    explicit Kit(float px_per_dp);

    float Px() const {
        return s;
    }
    /// The canvas to draw on (dp coordinates from its top left).
    void Begin(Canvas& canvas);
    /// Drawing outside `r` is dropped (a scrolled list); ClearClip ends it.
    void Clip(const Rect& r);
    void ClearClip();

    // Primitives (dp).
    void Fill(const Rect& r, float radius, uint32_t rgb, float opacity = 1.0f);
    void FillCorners(const Rect& r, float tl, float tr, float br, float bl, uint32_t rgb,
                     float opacity = 1.0f);
    void Stroke(const Rect& r, float radius, float width, uint32_t rgb, float opacity = 1.0f);
    void Line(float x0, float y0, float x1, float y1, float width, uint32_t rgb,
              float opacity = 1.0f);
    void Dot(float cx, float cy, float radius, uint32_t rgb, float opacity = 1.0f);
    /// Text on a baseline (align 0 left, 1 centre, 2 right); returns its width.
    float Text(float x, float baseline, Type type, uint32_t rgb, const std::string& s,
               int align = 0, float opacity = 1.0f);
    /// Text centred vertically on `cy` (by its capital height).
    float TextMid(float x, float cy, Type type, uint32_t rgb, const std::string& s, int align = 0,
                  float opacity = 1.0f);
    /// Text at any size (dp), for the controller diagram's labels.
    float TextSized(float x, float baseline, float size_dp, bool medium, uint32_t rgb,
                    const std::string& s, int align = 0, float opacity = 1.0f);
    float TextWidth(Type type, const std::string& s) const;
    float TextWidthSized(float size_dp, bool medium, const std::string& s) const;
    float CapHeightSized(float size_dp, bool medium) const;
    std::string Fit(Type type, const std::string& s, float width) const;
    void Icon(float cx, float cy, float size, char32_t icon, bool filled, uint32_t rgb,
              float opacity = 1.0f);
    /// Straight-alpha RGBA8 pixels drawn into `r`.
    void Image(const uint8_t* rgba, int w, int h, const Rect& r);
    /// A state layer of `rgb` over `r` for `state` (nothing at rest or disabled).
    void StateLayer(const Rect& r, float radius, uint32_t rgb, State state);

    // Components (dp). Hit targets are the rectangles given; each is at least 48 dp tall.
    /// A navigation rail item: the indicator pill (56 x 32) with the icon, the label under it.
    void RailItem(const Rect& r, char32_t icon, const std::string& label, bool active, State state);
    /// A common button: the icon (may be 0) before the label, a full pill.
    void Button(const Rect& r, ButtonStyle style, char32_t icon, const std::string& label,
                State state, Type type = Type::Label);
    /// A large button with the icon over the label (the Main tab's tiles).
    void Tile(const Rect& r, ButtonStyle style, char32_t icon, const std::string& label,
              State state);
    /// Segmented buttons: equal segments, the selected one filled with a check. `hover` and
    /// `pressed` are segment indices or -1.
    void Segmented(const Rect& r, const std::vector<std::string>& options, int selected, int hover,
                   int pressed, bool disabled = false);
    /// A switch centred vertically in `r`, at its right end (52 x 32).
    void Switch(const Rect& r, bool on, State state);
    /// A slider in `r`: `t` 0..1, the value text at the right.
    void Slider(const Rect& r, float t, const std::string& value, State state, int stops = 0,
                float value_w = 64.0f);
    /// A tonal menu button showing `value` with a drop-down arrow.
    void MenuButton(const Rect& r, const std::string& value, bool open, State state);
    /// An open menu of `options` at `r` (its top left and width; the height follows).
    Rect Menu(const Rect& r, const std::vector<std::string>& options, int selected, int hover);
    static float MenuItemHeight() {
        return 48.0f;
    }
    /// A plain tooltip above (or below) the point.
    void Tooltip(float cx, float y, const std::string& text, bool above = true);

    /// The width a segmented button wants for `options` within `max_width`: equal segments as
    /// wide as the widest label, or, when those do not fit, each as wide as its own label.
    float SegmentedWidth(const std::vector<std::string>& options, float max_width = 1e9f) const;
    /// The segments' edges in `r` (count + 1 values), as Segmented draws them (hit testing).
    std::vector<float> SegmentEdges(const Rect& r, const std::vector<std::string>& options) const;

private:
    float D(float dp) const {
        return dp * s;
    }
    frame_xr::ui::TtfFont& FontFor(bool medium) const;
    float SizeOf(Type type) const;
    bool Medium(Type type) const;

    Canvas* cv = nullptr;
    float s;
};

/// Opaque colour from 0xRRGGBB with an opacity.
inline Color Rgb(uint32_t rgb, float opacity = 1.0f) {
    return frame_xr::ui::rgb(static_cast<int>((rgb >> 16) & 255), static_cast<int>((rgb >> 8) & 255),
                             static_cast<int>(rgb & 255), opacity);
}

} // namespace FrameXr::Menu
