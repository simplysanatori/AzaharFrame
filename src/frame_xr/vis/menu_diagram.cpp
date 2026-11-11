// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/vis/menu_diagram.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "frame_xr/vis/help_layout.h"

namespace FrameXr::Vis {

namespace {

using Menu::Rect;
namespace Tok = Menu::Tok;

// As panels.cpp MakeHelpArt (tabmenu phase 2: one copy of these).
constexpr DiagramAngle kTopAngle{0.0f, 90.0f};
constexpr DiagramAngle kFrontAngle{180.0f, 165.0f};
constexpr float kFrontScale = 0.9f;
constexpr int kFrontH = 440;

// Straight alpha from the drawings' premultiplied pixels.
void Unpremultiply(DiagramImage& img) {
    for (std::size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        const uint32_t a = img.rgba[i + 3];
        if (a > 0 && a < 255) {
            for (int c = 0; c < 3; c++) {
                img.rgba[i + c] =
                    static_cast<uint8_t>(std::min<uint32_t>(255, (img.rgba[i + c] * 255 + a / 2) / a));
            }
        }
    }
}

uint32_t Argb(uint32_t rgb) {
    return 0xFF000000u | rgb;
}

} // namespace

MenuDiagramArt MakeMenuDiagramArt(const ModelFile& left, const ModelFile& right, float k) {
    MenuDiagramArt art;
    art.k = k;
    DiagramStyle style;
    style.line = Argb(Tok::kOnSurfaceVariant);
    style.body = Argb(Tok::kSurfaceContainer);
    style.part = Argb(Tok::kSurfaceContainerHighest);
    style.line_width = 2.2f * k;
    const int top_w = static_cast<int>(std::lround(kDiagramTopW * k));
    const int top_h = static_cast<int>(std::lround(kDiagramTopH * k));
    const int front_w = static_cast<int>(std::lround(kDiagramFrontW * k));
    const int front_h = static_cast<int>(std::lround(kFrontH * k));
    const float top = DiagramScale({&left, &right}, kTopAngle, top_w, top_h);
    const float front = top * kFrontScale;
    const std::array<const ModelFile*, 2> models{&left, &right};
    for (int h = 0; h < 2; h++) {
        const DiagramAngle ta{h == 0 ? kTopAngle.roll : -kTopAngle.roll, kTopAngle.pitch};
        const DiagramAngle fa{h == 0 ? kFrontAngle.roll : -kFrontAngle.roll, kFrontAngle.pitch};
        art.top[h] = DrawDiagram(*models[h], ta, top, top_w, top_h, style);
        const DiagramImage full = DrawDiagram(*models[h], fa, front, front_w, front_h, style);
        int y0 = 0, y1 = 0;
        while (y0 < full.height) {
            bool any = false;
            for (int x = 0; x < full.width && !any; x++) {
                any = full.rgba[(static_cast<std::size_t>(y0) * full.width + x) * 4 + 3] > 0;
            }
            if (any) {
                break;
            }
            y0++;
        }
        for (const CtrlPart p : {CtrlPart::Bumper, CtrlPart::Trigger}) {
            if (full.anchor[static_cast<std::size_t>(p)]) {
                y1 = std::max(y1, static_cast<int>(full.bounds[static_cast<std::size_t>(p)][3]));
            }
        }
        const int extra = static_cast<int>(std::lround(40 * k));
        const int fade = static_cast<int>(std::lround(36 * k));
        art.front[h] = y1 > y0 ? CropDiagram(full, std::max(0, y0 - 2), y1 + extra, fade)
                               : CropDiagram(full, 0, full.height / 2, fade);
        Unpremultiply(art.top[h]);
        Unpremultiply(art.front[h]);
    }
    return art;
}

namespace {

// What a drawing of the diagram needs once the labels are placed: the area's mapping into help
// panel pixels, and the placed labels.
struct Placed {
    float sc = 1.0f, ox = 0.0f, oy = 0.0f;
    std::vector<DiagramPlaced> labels;
    float X(float x) const {
        return ox + x * sc;
    }
    float Y(float y) const {
        return oy + y * sc;
    }
};

// Draws the controller pictures and places the labels of `page` (as DrawMenuDiagram did).
Placed PlaceDiagram(Menu::Kit& kit, const Rect& area, const DiagramPage& page,
                    const MenuDiagramArt& art) {
    Placed out;
    const float k = art.k;
    // Help panel pixels (x 0..kDiagramPanelW, y kDiagramTop..kDiagramBottom) into the area.
    const float span_h = kDiagramBottom - kDiagramTop;
    const float sc = std::min(area.W() / kDiagramPanelW, area.H() / span_h);
    out.sc = sc;
    out.ox = area.Cx() - kDiagramPanelW * sc * 0.5f;
    out.oy = area.Cy() - span_h * sc * 0.5f - kDiagramTop * sc;
    const auto X = [&](float x) { return out.X(x); };
    const auto Y = [&](float y) { return out.Y(y); };

    // Where each part shows (help panel pixels), as HelpArtAnchors.
    DiagramAnchors an;
    for (int h = 0; h < 2; h++) {
        for (std::size_t p = 0; p < kCtrlParts; p++) {
            const bool front = DiagramPartInFront(static_cast<CtrlPart>(p));
            const DiagramImage& img = front ? art.front[h] : art.top[h];
            const float ix = front ? DiagramFrontX(h) : DiagramTopX(h);
            const float iy = front ? DiagramFrontY(static_cast<int>(std::lround(img.height / k)))
                                   : kDiagramTopY;
            if (const auto& at = img.anchor[p]) {
                an.at[h][p] = DiagramPoint{at->x / k + ix, at->y / k + iy};
            }
            const auto& b = img.bounds[p];
            an.bounds[h][p] = {b[0] / k + ix, b[1] / k + iy, b[2] / k + ix, b[3] / k + iy};
        }
    }
    for (int h = 0; h < 2; h++) {
        const DiagramImage& t = art.top[h];
        const float tx = DiagramTopX(h), ty = kDiagramTopY;
        kit.Image(t.rgba.data(), t.width, t.height,
                  {X(tx), Y(ty), X(tx + t.width / k), Y(ty + t.height / k)});
        const DiagramImage& f = art.front[h];
        const float fx = DiagramFrontX(h);
        const float fy = DiagramFrontY(static_cast<int>(std::lround(f.height / k)));
        kit.Image(f.rgba.data(), f.width, f.height,
                  {X(fx), Y(fy), X(fx + f.width / k), Y(fy + f.height / k)});
    }
    // Label text in help panel pixels for the layout; drawn at sc dp a pixel.
    const auto measure = [&](float size, bool bold, const std::string& text) {
        return kit.TextWidthSized(size * sc, bold, text) / sc;
    };
    out.labels = LayoutHelpDiagram(page, an, measure);
    return out;
}

void DrawLeaders(Menu::Kit& kit, const Placed& pl) {
    for (const DiagramPlaced& p : pl.labels) {
        for (const DiagramLeader& l : p.leaders) {
            kit.Line(pl.X(l.from.x), pl.Y(l.from.y), pl.X(l.to.x), pl.Y(l.to.y), 1.5f, Tok::kOutline);
            if (l.ring > 0.0f) {
                const float r = l.ring * pl.sc;
                kit.Stroke({pl.X(l.dot.x) - r, pl.Y(l.dot.y) - r, pl.X(l.dot.x) + r, pl.Y(l.dot.y) + r},
                           r, 1.5f, Tok::kPrimary);
            } else {
                kit.Dot(pl.X(l.dot.x), pl.Y(l.dot.y), 3.5f, Tok::kPrimary);
            }
        }
    }
}

// The 3DS button's glyph in a box (dp): the button as it looks on the 3DS, drawn with the kit's
// shapes so every button is its own picture.
void DrawGlyph(Menu::Kit& kit, const Rect& b, int button, uint32_t fg, uint32_t bg, float op) {
    namespace NB = Settings::NativeButton;
    const float cx = b.Cx(), cy = b.Cy(), r = std::min(b.W(), b.H()) * 0.5f;
    const auto letter = [&](const char* t, float size_scale) {
        kit.TextSized(cx, cy + kit.CapHeightSized(r * size_scale, true) * 0.5f, r * size_scale, true, fg,
                      t, 1, op);
    };
    const auto pill = [&](const char* t, float w_scale) {
        const Rect p{cx - r * w_scale, cy - r * 0.62f, cx + r * w_scale, cy + r * 0.62f};
        kit.Fill(p, r * 0.3f, bg, op);
        kit.Stroke(p, r * 0.3f, 1.2f, fg, op * 0.7f);
        kit.TextSized(cx, cy + kit.CapHeightSized(r * 0.95f, true) * 0.5f, r * 0.95f, true, fg, t, 1,
                      op);
    };
    switch (button) {
    case NB::A:
    case NB::B:
    case NB::X:
    case NB::Y: {
        kit.Dot(cx, cy, r, bg, op);
        kit.Stroke({cx - r, cy - r, cx + r, cy + r}, r, 1.2f, fg, op * 0.7f);
        static const char* const names[] = {"A", "B", "X", "Y"};
        letter(names[button == NB::A ? 0 : button == NB::B ? 1 : button == NB::X ? 2 : 3], 1.15f);
        break;
    }
    case NB::Up:
    case NB::Down:
    case NB::Left:
    case NB::Right: {
        // The cross, the pressed arm in the accent.
        const float arm = r * 0.36f;
        kit.Fill({cx - r, cy - arm, cx + r, cy + arm}, arm * 0.5f, bg, op);
        kit.Fill({cx - arm, cy - r, cx + arm, cy + r}, arm * 0.5f, bg, op);
        const float dx = button == NB::Left ? -1.0f : button == NB::Right ? 1.0f : 0.0f;
        const float dy = button == NB::Up ? -1.0f : button == NB::Down ? 1.0f : 0.0f;
        kit.Dot(cx + dx * r * 0.66f, cy + dy * r * 0.66f, r * 0.26f, fg, op);
        break;
    }
    case NB::L:
        pill("L", 1.0f);
        break;
    case NB::R:
        pill("R", 1.0f);
        break;
    case NB::ZL:
        pill("ZL", 1.0f);
        break;
    case NB::ZR:
        pill("ZR", 1.0f);
        break;
    case NB::Start:
    case NB::Select: {
        // The two small pills of the 3DS: Start and Select, one lit.
        const float w = r * 0.9f, h = r * 0.34f;
        const Rect top{cx - w, cy - r * 0.62f, cx + w, cy - r * 0.62f + h * 2.0f};
        const Rect low{cx - w, cy + r * 0.62f - h * 2.0f, cx + w, cy + r * 0.62f};
        const bool start = button == NB::Start;
        kit.Fill(top, h, start ? fg : bg, op);
        kit.Fill(low, h, start ? bg : fg, op);
        break;
    }
    case NB::Home:
        kit.Dot(cx, cy, r * 0.8f, bg, op);
        kit.Stroke({cx - r * 0.45f, cy - r * 0.45f, cx + r * 0.45f, cy + r * 0.45f}, r * 0.1f, 1.4f,
                   fg, op);
        break;
    default:
        kit.Dot(cx, cy, r * 0.7f, bg, op);
        letter("?", 1.0f);
        break;
    }
}

} // namespace

void DrawMenuDiagram(Menu::Kit& kit, const Rect& area, const HelpDiagram& d, DiagramPageId page,
                     const MenuDiagramArt& art) {
    if (art.top[0].width == 0) {
        return;
    }
    const Placed pl = PlaceDiagram(kit, area, d.pages[static_cast<std::size_t>(page)], art);
    DrawLeaders(kit, pl);
    const float sc = pl.sc;
    for (const DiagramPlaced& p : pl.labels) {
        float y = p.y0;
        const auto& lines = p.label->lines;
        for (std::size_t i = 0; i < lines.size(); i++) {
            const DiagramLine& l = lines[i];
            if (l.heading && i > 0) {
                y += kDiagramHeadingGap;
            }
            const float size = kDiagramLabelSize * sc;
            const float base =
                pl.Y(y + kDiagramLabelLine * 0.5f) + kit.CapHeightSized(size, l.title) * 0.5f;
            float x = pl.X(p.x0 + (l.sub ? kDiagramSubIndent : 0.0f));
            if (l.heading) {
                kit.TextSized(x, base, size, true, Tok::kPrimary, l.key);
            } else {
                if (!l.key.empty()) {
                    x += kit.TextSized(x, base, size, l.title, Tok::kOnSurfaceVariant, l.key + ": ");
                }
                kit.TextSized(x, base, size, l.title, Tok::kOnSurface, l.action);
            }
            y += kDiagramLabelLine;
        }
    }
}

// AzaharFrame remap: Edit Controls. An M3 assist chip per line: container, the glyph of the 3DS
// button (or a ring when the control has none), "Control: 3DS Button". The chip that listens is
// filled; a disabled chip (a gesture) is dimmed and no target.
void DrawMenuEditDiagram(Menu::Kit& kit, const Rect& area, const DiagramPage& page,
                         const MenuDiagramArt& art, const Menu::DiagramEdit& edit,
                         std::vector<Menu::DiagramChipHit>& chips) {
    if (art.top[0].width == 0) {
        return;
    }
    const Placed pl = PlaceDiagram(kit, area, page, art);
    DrawLeaders(kit, pl);
    const float sc = pl.sc;
    for (const DiagramPlaced& p : pl.labels) {
        float y = p.y0;
        for (const DiagramLine& l : p.label->lines) {
            const Rect r{pl.X(p.x0), pl.Y(y + (kChipLine - kChipHeight) * 0.5f),
                         pl.X(p.x1), pl.Y(y + (kChipLine + kChipHeight) * 0.5f)};
            y += kChipLine;
            const bool enabled = l.control >= 0;
            const bool listening = enabled && edit.listening == l.control;
            const bool hover = enabled && edit.hover == l.control;
            const bool pressed = enabled && edit.pressed == l.control;
            const float op = enabled ? 1.0f : Menu::kDisabledContent;
            const float rad = r.H() * 0.28f;
            uint32_t fg = Tok::kOnSurface, glyph_bg = Tok::kSecondaryContainer;
            if (listening) {
                kit.Fill(r, rad, Tok::kPrimaryContainer);
                kit.Stroke(r, rad, 2.0f, Tok::kPrimary);
                fg = Tok::kOnPrimaryContainer;
                glyph_bg = Tok::kPrimary;
            } else {
                kit.Fill(r, rad, enabled ? Tok::kSurfaceContainerHigh : Tok::kSurface, enabled ? 1.0f : 0.5f);
                kit.Stroke(r, rad, 1.2f, enabled ? Tok::kOutline : Tok::kOutlineVariant, enabled ? 1.0f : 0.6f);
                kit.StateLayer(r, rad, Tok::kOnSurface,
                               pressed ? Menu::State::Pressed : hover ? Menu::State::Hover : Menu::State::Rest);
            }
            const float pad = kChipPad * sc;
            float x = r.x0 + pad;
            if (enabled) {
                const float g = r.H() * 0.8f;
                const Rect gb{x - pad * 0.15f, r.Cy() - g * 0.5f, x - pad * 0.15f + g, r.Cy() + g * 0.5f};
                if (l.glyph >= 0) {
                    DrawGlyph(kit, gb, l.glyph, listening ? Tok::kOnPrimary : Tok::kPrimary, glyph_bg, 1.0f);
                } else {
                    kit.Stroke({gb.Cx() - g * 0.4f, gb.Cy() - g * 0.4f, gb.Cx() + g * 0.4f, gb.Cy() + g * 0.4f},
                               g * 0.4f, 1.3f, Tok::kOutline);
                }
                x = gb.x1 + pad * 0.6f;
            }
            const float size = kChipText * sc;
            const float base = r.Cy() + kit.CapHeightSized(size, true) * 0.5f;
            if (!l.action.empty()) {
                x += kit.TextSized(x, base, size, true, Tok::kOnSurfaceVariant, l.key + ": ", 0, op);
                kit.TextSized(x, base, size, true, fg, l.action, 0, op);
            } else {
                kit.TextSized(x, base, size, true, fg, l.key, 0, op);
            }
            if (enabled) {
                chips.push_back({r, l.control});
            }
        }
    }
}

} // namespace FrameXr::Vis
