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

void DrawMenuDiagram(Menu::Kit& kit, const Rect& area, const HelpDiagram& d, DiagramPageId page,
                     const MenuDiagramArt& art) {
    if (art.top[0].width == 0) {
        return;
    }
    const float k = art.k;
    // Help panel pixels (x 0..kDiagramPanelW, y kDiagramTop..kDiagramBottom) into the area.
    const float span_h = kDiagramBottom - kDiagramTop;
    const float sc = std::min(area.W() / kDiagramPanelW, area.H() / span_h);
    const float ox = area.Cx() - kDiagramPanelW * sc * 0.5f;
    const float oy = area.Cy() - span_h * sc * 0.5f - kDiagramTop * sc;
    const auto X = [&](float x) { return ox + x * sc; };
    const auto Y = [&](float y) { return oy + y * sc; };

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
    const auto placed =
        LayoutHelpDiagram(d.pages[static_cast<std::size_t>(page)], an, measure);
    for (const DiagramPlaced& p : placed) {
        for (const DiagramLeader& l : p.leaders) {
            kit.Line(X(l.from.x), Y(l.from.y), X(l.to.x), Y(l.to.y), 1.5f, Tok::kOutline);
            if (l.ring > 0.0f) {
                const float r = l.ring * sc;
                kit.Stroke({X(l.dot.x) - r, Y(l.dot.y) - r, X(l.dot.x) + r, Y(l.dot.y) + r}, r, 1.5f,
                           Tok::kPrimary);
            } else {
                kit.Dot(X(l.dot.x), Y(l.dot.y), 3.5f, Tok::kPrimary);
            }
        }
        float y = p.y0;
        const auto& lines = p.label->lines;
        for (std::size_t i = 0; i < lines.size(); i++) {
            const DiagramLine& l = lines[i];
            if (l.heading && i > 0) {
                y += kDiagramHeadingGap;
            }
            const float size = kDiagramLabelSize * sc;
            const float base =
                Y(y + kDiagramLabelLine * 0.5f) + kit.CapHeightSized(size, l.title) * 0.5f;
            float x = X(p.x0 + (l.sub ? kDiagramSubIndent : 0.0f));
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

} // namespace FrameXr::Vis
