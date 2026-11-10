// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/menu/md3.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include "frame_xr/menu/md3_fonts.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;
namespace Emb = ::FrameXr::Menu::Fonts;

namespace {

struct FontSet {
    frame_xr::ui::TtfFont sans400, sans500, symbols, symbols_filled;
};

// The embedded fonts, decoded once. The glyph caches are not locked: draw from one thread.
FontSet& TheFonts() {
    static FontSet fonts;
    static std::once_flag once;
    std::call_once(once, [] {
        fonts.sans400.LoadBase64(Emb::kSans400);
        fonts.sans500.LoadBase64(Emb::kSans500);
        fonts.symbols.LoadBase64(Emb::kSymbols);
        fonts.symbols_filled.LoadBase64(Emb::kSymbolsFilled);
    });
    return fonts;
}

} // namespace

Kit::Kit(float px_per_dp) : s(px_per_dp) {
    TheFonts();
}

void Kit::Begin(Canvas& canvas) {
    cv = &canvas;
}

void Kit::Clip(const Rect& r) {
    cv->SetClip(D(r.x0), D(r.y0), D(r.x1), D(r.y1));
}

void Kit::ClearClip() {
    cv->ResetClip();
}

frame_xr::ui::TtfFont& Kit::FontFor(bool medium) const {
    return medium ? TheFonts().sans500 : TheFonts().sans400;
}

float Kit::SizeOf(Type type) const {
    switch (type) {
    case Type::RailLabel:
        return 14.0f;
    case Type::Label:
        return 16.0f;
    case Type::Body:
    case Type::Value:
        return 18.0f;
    case Type::Title:
        return 20.0f;
    case Type::Headline:
        return 26.0f;
    case Type::Tooltip:
        return 14.0f;
    }
    return 16.0f;
}

bool Kit::Medium(Type type) const {
    return type == Type::RailLabel || type == Type::Label || type == Type::Value ||
           type == Type::Title;
}

void Kit::Fill(const Rect& r, float radius, uint32_t rgb, float opacity) {
    cv->RoundRect(D(r.x0), D(r.y0), D(r.x1), D(r.y1), D(radius), Rgb(rgb, opacity));
}

void Kit::FillCorners(const Rect& r, float tl, float tr, float br, float bl, uint32_t rgb,
                      float opacity) {
    cv->RoundRectCorners(D(r.x0), D(r.y0), D(r.x1), D(r.y1), D(tl), D(tr), D(br), D(bl),
                         Rgb(rgb, opacity));
}

void Kit::Stroke(const Rect& r, float radius, float width, uint32_t rgb, float opacity) {
    // Inside the rectangle, as Material's outlines.
    const float h = width * 0.5f;
    cv->StrokeRoundRect(D(r.x0 + h), D(r.y0 + h), D(r.x1 - h), D(r.y1 - h), D(radius - h),
                        D(width), Rgb(rgb, opacity));
}

void Kit::Line(float x0, float y0, float x1, float y1, float width, uint32_t rgb, float opacity) {
    cv->Line(D(x0), D(y0), D(x1), D(y1), D(width), Rgb(rgb, opacity));
}

void Kit::Dot(float cx, float cy, float radius, uint32_t rgb, float opacity) {
    cv->Circle(D(cx), D(cy), D(radius), Rgb(rgb, opacity));
}

float Kit::Text(float x, float baseline, Type type, uint32_t rgb, const std::string& str,
                int align, float opacity) {
    return TextSized(x, baseline, SizeOf(type), Medium(type), rgb, str, align, opacity);
}

float Kit::TextMid(float x, float cy, Type type, uint32_t rgb, const std::string& str, int align,
                   float opacity) {
    const float cap = CapHeightSized(SizeOf(type), Medium(type));
    return Text(x, cy + cap * 0.5f, type, rgb, str, align, opacity);
}

float Kit::TextSized(float x, float baseline, float size_dp, bool medium, uint32_t rgb,
                     const std::string& str, int align, float opacity) {
    return FontFor(medium).Draw(*cv, D(x), D(baseline), D(size_dp), Rgb(rgb, opacity), str, align) /
           s;
}

float Kit::TextWidth(Type type, const std::string& str) const {
    return TextWidthSized(SizeOf(type), Medium(type), str);
}

float Kit::TextWidthSized(float size_dp, bool medium, const std::string& str) const {
    return FontFor(medium).Width(D(size_dp), str) / s;
}

float Kit::CapHeightSized(float size_dp, bool medium) const {
    return FontFor(medium).CapHeight(D(size_dp)) / s;
}

std::string Kit::Fit(Type type, const std::string& str, float width) const {
    return FontFor(Medium(type)).Fit(D(SizeOf(type)), str, D(width));
}

void Kit::Icon(float cx, float cy, float size, char32_t icon, bool filled, uint32_t rgb,
               float opacity) {
    if (!icon) {
        return;
    }
    (filled ? TheFonts().symbols_filled : TheFonts().symbols)
        .DrawCentred(*cv, D(cx), D(cy), D(size), icon, Rgb(rgb, opacity));
}

void Kit::Image(const uint8_t* rgba, int w, int h, const Rect& r) {
    cv->BlitRGBA(rgba, w, h, D(r.x0), D(r.y0), D(r.x1), D(r.y1), true);
}

void Kit::StateLayer(const Rect& r, float radius, uint32_t rgb, State state) {
    if (state == State::Hover) {
        Fill(r, radius, rgb, kHoverLayer);
    } else if (state == State::Pressed) {
        Fill(r, radius, rgb, kPressedLayer);
    }
}

void Kit::RailItem(const Rect& r, char32_t icon, const std::string& label, bool active,
                   State state) {
    // Material 3 navigation rail: a 56 x 32 indicator, the icon 24 in it, the label 4 under it.
    const float pill_w = 56.0f, pill_h = 32.0f, gap = 4.0f;
    const float label_h = SizeOf(Type::RailLabel) * 1.4f;
    const float top = r.Cy() - (pill_h + gap + label_h) * 0.5f;
    const Rect pill{r.Cx() - pill_w * 0.5f, top, r.Cx() + pill_w * 0.5f, top + pill_h};
    if (active) {
        Fill(pill, pill_h * 0.5f, Tok::kSecondaryContainer);
    }
    StateLayer(pill, pill_h * 0.5f, active ? Tok::kOnSecondaryContainer : Tok::kOnSurfaceVariant,
               state);
    Icon(pill.Cx(), pill.Cy(), 24.0f, icon, active,
         active ? Tok::kOnSecondaryContainer : Tok::kOnSurfaceVariant);
    TextMid(r.Cx(), pill.y1 + gap + label_h * 0.5f, Type::RailLabel,
            active ? Tok::kOnSurface : Tok::kOnSurfaceVariant, label, 1);
}

namespace {
struct ButtonInk {
    uint32_t container, content;
    bool has_container;
};
ButtonInk InkFor(ButtonStyle style) {
    switch (style) {
    case ButtonStyle::Filled:
        return {Tok::kPrimary, Tok::kOnPrimary, true};
    case ButtonStyle::Tonal:
        return {Tok::kSecondaryContainer, Tok::kOnSecondaryContainer, true};
    case ButtonStyle::Danger:
        return {Tok::kErrorContainer, Tok::kOnErrorContainer, true};
    case ButtonStyle::Text:
        return {0, Tok::kPrimary, false};
    }
    return {Tok::kSecondaryContainer, Tok::kOnSecondaryContainer, true};
}
} // namespace

void Kit::Button(const Rect& r, ButtonStyle style, char32_t icon, const std::string& label,
                 State state, Type type) {
    const ButtonInk ink = InkFor(style);
    const float radius = r.H() * 0.5f;
    const bool off = state == State::Disabled;
    if (ink.has_container) {
        Fill(r, radius, off ? Tok::kOnSurface : ink.container, off ? kDisabledContainer : 1.0f);
    }
    StateLayer(r, radius, ink.content, state);
    const uint32_t fg = off ? Tok::kOnSurface : ink.content;
    const float op = off ? kDisabledContent : 1.0f;
    const float icon_size = type == Type::Title ? 24.0f : 20.0f;
    const float gap = 8.0f;
    const float tw = TextWidth(type, label);
    const float total = tw + (icon ? icon_size + gap : 0.0f);
    float x = r.Cx() - total * 0.5f;
    if (icon) {
        Icon(x + icon_size * 0.5f, r.Cy(), icon_size, icon, true, fg, op);
        x += icon_size + gap;
    }
    TextMid(x, r.Cy(), type, fg, label, 0, op);
}

void Kit::Tile(const Rect& r, ButtonStyle style, char32_t icon, const std::string& label,
               State state) {
    const ButtonInk ink = InkFor(style);
    const float radius = 28.0f;
    const bool off = state == State::Disabled;
    Fill(r, radius, off ? Tok::kOnSurface : ink.container, off ? kDisabledContainer : 1.0f);
    StateLayer(r, radius, ink.content, state);
    const uint32_t fg = off ? Tok::kOnSurface : ink.content;
    const float op = off ? kDisabledContent : 1.0f;
    const float icon_size = 32.0f, gap = 10.0f;
    const float label_h = CapHeightSized(SizeOf(Type::Label), true);
    const float top = r.Cy() - (icon_size + gap + label_h) * 0.5f;
    Icon(r.Cx(), top + icon_size * 0.5f, icon_size, icon, false, fg, op);
    Text(r.Cx(), top + icon_size + gap + label_h, Type::Label, fg,
         Fit(Type::Label, label, r.W() - 16.0f), 1, op);
}

namespace {
constexpr float kSegPad = 24.0f;   ///< 12 dp each side of a segment's label
constexpr float kSegCheck = 26.0f; ///< the selected segment's check (18) and its gap (8)
} // namespace

float Kit::SegmentedWidth(const std::vector<std::string>& options, float max_width) const {
    float widest = 0.0f, sum = 0.0f;
    for (const auto& o : options) {
        const float w = TextWidth(Type::Label, o);
        widest = std::max(widest, w);
        sum += w + kSegPad;
    }
    const float n = static_cast<float>(options.size());
    // Material's equal segments with the check; when they do not fit, segments as wide as
    // their labels (with the check, then without it).
    const float equal = (widest + kSegCheck + kSegPad) * n;
    if (equal <= max_width) {
        return equal;
    }
    return std::min(max_width, sum + kSegCheck);
}

void Kit::Segmented(const Rect& r, const std::vector<std::string>& options, int selected,
                    int hover, int pressed, bool disabled) {
    const int n = static_cast<int>(options.size());
    if (n == 0) {
        return;
    }
    const std::vector<float> edges = SegmentEdges(r, options);
    const float radius = r.H() * 0.5f;
    const float op = disabled ? kDisabledContent : 1.0f;
    for (int i = 0; i < n; i++) {
        const Rect sr{edges[static_cast<std::size_t>(i)], r.y0, edges[static_cast<std::size_t>(i + 1)],
                      r.y1};
        const float l = i == 0 ? radius : 0.0f, rr = i == n - 1 ? radius : 0.0f;
        const bool on = i == selected;
        if (on) {
            FillCorners(sr, l, rr, rr, l, disabled ? Tok::kOnSurface : Tok::kSecondaryContainer,
                        disabled ? kDisabledContainer : 1.0f);
        }
        if (!disabled && (i == hover || i == pressed)) {
            FillCorners(sr, l, rr, rr, l, on ? Tok::kOnSecondaryContainer : Tok::kOnSurface,
                        i == pressed ? kPressedLayer : kHoverLayer);
        }
        const uint32_t fg = disabled ? Tok::kOnSurface
                            : on     ? Tok::kOnSecondaryContainer
                                     : Tok::kOnSurface;
        const std::string& text = options[static_cast<std::size_t>(i)];
        const bool check = on && TextWidth(Type::Label, text) + kSegCheck <= sr.W() - kSegPad;
        const std::string label =
            Fit(Type::Label, text, sr.W() - kSegPad - (check ? kSegCheck : 0.0f));
        const float tw = TextWidth(Type::Label, label);
        if (check) {
            const float x = sr.Cx() - (tw + kSegCheck) * 0.5f;
            Icon(x + 9.0f, sr.Cy(), 18.0f, Sym::kCheck, false, fg, op);
            TextMid(x + kSegCheck, sr.Cy(), Type::Label, fg, label, 0, op);
        } else {
            TextMid(sr.Cx(), sr.Cy(), Type::Label, fg, label, 1, op);
        }
        if (i > 0) {
            Line(sr.x0, r.y0, sr.x0, r.y1, 1.0f, disabled ? Tok::kOnSurface : Tok::kOutline,
                 disabled ? kDisabledContainer : 1.0f);
        }
    }
    Stroke(r, radius, 1.0f, disabled ? Tok::kOnSurface : Tok::kOutline,
           disabled ? kDisabledContainer : 1.0f);
}

std::vector<float> Kit::SegmentEdges(const Rect& r, const std::vector<std::string>& options) const {
    const std::size_t n = options.size();
    std::vector<float> edges(n + 1, r.x0);
    float widest = 0.0f, sum = 0.0f;
    std::vector<float> w(n);
    for (std::size_t i = 0; i < n; i++) {
        w[i] = TextWidth(Type::Label, options[i]) + kSegPad;
        widest = std::max(widest, w[i]);
        sum += w[i];
    }
    const bool equal = widest * static_cast<float>(n) <= r.W() + 0.5f;
    for (std::size_t i = 0; i < n; i++) {
        const float share = equal ? r.W() / static_cast<float>(n) : r.W() * w[i] / sum;
        edges[i + 1] = edges[i] + share;
    }
    edges[n] = r.x1;
    return edges;
}

void Kit::Switch(const Rect& r, bool on, State state) {
    const float tw = 52.0f, th = 32.0f;
    const Rect track{r.x1 - tw, r.Cy() - th * 0.5f, r.x1, r.Cy() + th * 0.5f};
    const bool off = state == State::Disabled;
    if (on) {
        Fill(track, th * 0.5f, off ? Tok::kOnSurface : Tok::kPrimary, off ? kDisabledContainer : 1.0f);
    } else {
        Fill(track, th * 0.5f, Tok::kSurfaceContainerHighest, off ? kDisabledContainer : 1.0f);
        Stroke(track, th * 0.5f, 2.0f, off ? Tok::kOnSurface : Tok::kOutline,
               off ? kDisabledContainer : 1.0f);
    }
    const float thumb = state == State::Pressed ? 28.0f : on ? 24.0f : 16.0f;
    const float cx = on ? track.x1 - th * 0.5f : track.x0 + th * 0.5f;
    if (state == State::Hover || state == State::Pressed) {
        Dot(cx, track.Cy(), 20.0f, on ? Tok::kPrimary : Tok::kOnSurface,
            state == State::Pressed ? kPressedLayer : kHoverLayer);
    }
    const uint32_t thumb_ink = off ? (on ? Tok::kSurface : Tok::kOnSurface)
                               : on ? Tok::kOnPrimary
                                    : Tok::kOutline;
    Dot(cx, track.Cy(), thumb * 0.5f, thumb_ink, off && !on ? kDisabledContent : 1.0f);
    if (on) {
        Icon(cx, track.Cy(), 16.0f, Sym::kCheck, false,
             off ? Tok::kOnSurface : Tok::kOnPrimaryContainer, off ? kDisabledContent : 1.0f);
    }
}

void Kit::Slider(const Rect& r, float t, const std::string& value, State state, int stops) {
    // Material 3 (2024) slider: a 16 dp track, a 4 x 44 dp handle with a 6 dp gap each side,
    // a stop dot at the inactive end. The value at the right, in its own column.
    const float value_w = 64.0f;
    const float x0 = r.x0, x1 = r.x1 - value_w - 16.0f;
    const float cy = r.Cy(), th = 16.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const bool off = state == State::Disabled;
    const float handle_w = state == State::Pressed ? 2.0f : 4.0f, gap = 6.0f;
    const float hx = x0 + handle_w * 0.5f + t * (x1 - x0 - handle_w);
    const uint32_t active = off ? Tok::kOnSurface : Tok::kPrimary;
    const uint32_t inactive = off ? Tok::kOnSurface : Tok::kSecondaryContainer;
    const float aop = off ? kDisabledContent : 1.0f, iop = off ? kDisabledContainer : 1.0f;
    if (hx - gap - handle_w * 0.5f > x0 + 2.0f) {
        FillCorners({x0, cy - th * 0.5f, hx - handle_w * 0.5f - gap, cy + th * 0.5f}, 8.0f, 2.0f,
                    2.0f, 8.0f, active, aop);
    }
    if (hx + gap + handle_w * 0.5f < x1 - 2.0f) {
        FillCorners({hx + handle_w * 0.5f + gap, cy - th * 0.5f, x1, cy + th * 0.5f}, 2.0f, 8.0f,
                    8.0f, 2.0f, inactive, iop);
    }
    // Stops (discrete values) on the track, and the end stop.
    for (int i = 1; i < stops; i++) {
        const float sx = x0 + 8.0f + (x1 - x0 - 16.0f) * static_cast<float>(i) / static_cast<float>(stops);
        if (std::fabs(sx - hx) > gap + 4.0f) {
            Dot(sx, cy, 2.0f, sx < hx ? Tok::kOnPrimary : Tok::kOnSecondaryContainer, aop);
        }
    }
    if (x1 - 8.0f - hx > gap + 4.0f) {
        Dot(x1 - 8.0f, cy, 2.0f, active, aop);
    }
    if (state == State::Hover || state == State::Pressed) {
        Fill({hx - 12.0f, cy - 26.0f, hx + 12.0f, cy + 26.0f}, 12.0f, Tok::kPrimary,
             state == State::Pressed ? kPressedLayer : kHoverLayer);
    }
    Fill({hx - handle_w * 0.5f, cy - 22.0f, hx + handle_w * 0.5f, cy + 22.0f}, handle_w * 0.5f,
         active, aop);
    TextMid(r.x1, cy, Type::Value, off ? Tok::kOnSurface : Tok::kOnSurface, value, 2, aop);
}

void Kit::MenuButton(const Rect& r, const std::string& value, bool open, State state) {
    const float radius = r.H() * 0.5f;
    const bool off = state == State::Disabled;
    Fill(r, radius, off ? Tok::kOnSurface : Tok::kSecondaryContainer, off ? kDisabledContainer : 1.0f);
    StateLayer(r, radius, Tok::kOnSecondaryContainer, open ? State::Pressed : state);
    const uint32_t fg = off ? Tok::kOnSurface : Tok::kOnSecondaryContainer;
    const float op = off ? kDisabledContent : 1.0f;
    TextMid(r.x0 + 20.0f, r.Cy(), Type::Label, fg, Fit(Type::Label, value, r.W() - 20.0f - 44.0f),
            0, op);
    Icon(r.x1 - 26.0f, r.Cy(), 24.0f, Sym::kArrowDropDown, false, fg, op);
}

Rect Kit::Menu(const Rect& r, const std::vector<std::string>& options, int selected, int hover) {
    const float item = MenuItemHeight(), pad = 8.0f;
    const Rect box{r.x0, r.y0, r.x1, r.y0 + pad * 2.0f + item * static_cast<float>(options.size())};
    Fill(box, 16.0f, Tok::kSurfaceContainerHigh);
    for (std::size_t i = 0; i < options.size(); i++) {
        const Rect ir{box.x0 + 4.0f, box.y0 + pad + item * static_cast<float>(i), box.x1 - 4.0f,
                      box.y0 + pad + item * static_cast<float>(i + 1)};
        const bool on = static_cast<int>(i) == selected;
        if (on) {
            Fill(ir, 12.0f, Tok::kSecondaryContainer);
        }
        if (static_cast<int>(i) == hover) {
            Fill(ir, 12.0f, on ? Tok::kOnSecondaryContainer : Tok::kOnSurface, kHoverLayer);
        }
        const uint32_t fg = on ? Tok::kOnSecondaryContainer : Tok::kOnSurface;
        if (on) {
            Icon(ir.x0 + 24.0f, ir.Cy(), 20.0f, Sym::kCheck, false, fg);
        }
        TextMid(ir.x0 + 48.0f, ir.Cy(), Type::Label, fg,
                Fit(Type::Label, options[i], ir.W() - 64.0f));
    }
    return box;
}

void Kit::Tooltip(float cx, float y, const std::string& text, bool above) {
    const float pad = 10.0f, h = 30.0f;
    const float w = TextWidth(Type::Tooltip, text) + pad * 2.0f;
    const float y0 = above ? y - h - 4.0f : y + 4.0f;
    const Rect r{cx - w * 0.5f, y0, cx + w * 0.5f, y0 + h};
    Fill(r, 6.0f, Tok::kInverseSurface);
    TextMid(r.Cx(), r.Cy(), Type::Tooltip, Tok::kInverseOnSurface, text, 1);
}

} // namespace FrameXr::Menu
