// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/menu/ds_drawing.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/remap.h"

namespace FrameXr::Menu {

namespace {

// The console in millimetres (x right, y down), opened flat and seen from the front: the lid
// (0 to 93.5), the hinge's barrel, the base (from kBase). New 3DS XL: 160 x 93.5 mm each half,
// a 4.88" 5:3 top screen (106.3 x 63.8 mm), a 4.18" 4:3 touch screen (84.9 x 63.7 mm). Drawn
// as the Frame controllers' drawings on the Controls tab are: 1.5 dp outlines, finer detail
// lines, tonal fills a step or two over the pane.
constexpr float kLine = 1.5f;   // dp: outlines
constexpr float kDetail = 1.0f; // dp: bevels, frames, textures
constexpr uint32_t kInk = Tok::kOnSurfaceVariant;
constexpr uint32_t kShell = Tok::kSurfaceContainer;
constexpr uint32_t kWell = Tok::kSurfaceContainerLow;
constexpr uint32_t kPart = Tok::kSurfaceContainerHigh;
constexpr uint32_t kBezel = 0x15181C;
constexpr uint32_t kGlass = 0x0B0E12;

struct U {
    float x0, y0, x1, y1;
};
constexpr float kHalfW = 160.0f, kHalfH = 93.5f;
constexpr float kBase = 99.0f; ///< the base's back edge (the lid ends at kHalfH, the barrel between)
constexpr float kShellR = 9.5f;
constexpr U kLid{0, 0, kHalfW, kHalfH};
constexpr U kTopScreen{26.85f, 15.0f, 133.15f, 78.8f};
constexpr U kBaseShell{0, kBase, kHalfW, kBase + kHalfH};
constexpr U kBottomScreen{37.55f, kBase + 8.5f, 122.45f, kBase + 72.2f};
constexpr U kBarrel{36.0f, 92.6f, 124.0f, 99.6f};

constexpr DsPoint kCirclePad{19.5f, kBase + 24.0f};
constexpr float kCirclePadWell = 9.6f, kCirclePadCap = 7.4f;
constexpr DsPoint kDpad{19.5f, kBase + 54.5f};
constexpr float kDpadArm = 8.6f, kDpadHalf = 2.9f;
constexpr DsPoint kFace{140.5f, kBase + 40.0f};
constexpr float kFaceGap = 8.4f, kFaceR = 3.6f;
constexpr DsPoint kCStick{133.0f, kBase + 20.0f};
constexpr float kCStickR = 2.6f;
constexpr DsPoint kSelect{146.0f, kBase + 57.5f};
constexpr DsPoint kStart{146.0f, kBase + 64.5f};
constexpr float kSmallR = 2.2f;
constexpr DsPoint kHome{80.0f, kBase + 81.5f};

DsPoint UnitAnchor(int target) {
    switch (target) {
    case DsButton::A:
        return {kFace.x + kFaceGap, kFace.y};
    case DsButton::B:
        return {kFace.x, kFace.y + kFaceGap};
    case DsButton::X:
        return {kFace.x, kFace.y - kFaceGap};
    case DsButton::Y:
        return {kFace.x - kFaceGap, kFace.y};
    case DsButton::Up:
        return {kDpad.x, kDpad.y - kDpadArm + 3.0f};
    case DsButton::Down:
        return {kDpad.x, kDpad.y + kDpadArm - 3.0f};
    case DsButton::Left:
        return {kDpad.x - kDpadArm + 3.0f, kDpad.y};
    case DsButton::Right:
        return {kDpad.x + kDpadArm - 3.0f, kDpad.y};
    case DsButton::L:
        return {12.5f, 96.0f};
    case DsButton::R:
        return {kHalfW - 12.5f, 96.0f};
    case DsButton::ZL:
        return {30.0f, 96.5f};
    case DsButton::ZR:
        return {kHalfW - 30.0f, 96.5f};
    case DsButton::Start:
        return kStart;
    case DsButton::Select:
        return kSelect;
    case DsButton::Home:
        return kHome;
    case DsButton::CirclePad:
        return kCirclePad;
    case DsButton::CStick:
        return kCStick;
    default:
        return {80, 96};
    }
}

} // namespace

DsPoint DsDrawing::Anchor(int target) const {
    const DsPoint u = UnitAnchor(target);
    return {box.x0 + u.x * s, box.y0 + u.y * s};
}

float DsDrawing::Radius(int target) const {
    switch (target) {
    case DsButton::A:
    case DsButton::B:
    case DsButton::X:
    case DsButton::Y:
        return kFaceR * s;
    case DsButton::CirclePad:
        return kCirclePadWell * s;
    case DsButton::CStick:
        return kCStickR * s;
    case DsButton::Select:
    case DsButton::Start:
        return kSmallR * s;
    default:
        return 3.5f * s;
    }
}

Rect DsDrawing::BottomScreen() const {
    return {box.x0 + kBottomScreen.x0 * s, box.y0 + kBottomScreen.y0 * s,
            box.x0 + kBottomScreen.x1 * s, box.y0 + kBottomScreen.y1 * s};
}

DsDrawing PlaceDs(const Rect& area) {
    DsDrawing d;
    d.s = std::min(area.W() / kDsUnitsW, area.H() / kDsUnitsH);
    const float w = kDsUnitsW * d.s, h = kDsUnitsH * d.s;
    d.box = {area.Cx() - w * 0.5f, area.Cy() - h * 0.5f, area.Cx() + w * 0.5f, area.Cy() + h * 0.5f};
    return d;
}

void DrawDs(Kit& kit, const DsDrawing& d, int lit) {
    const float s = d.s;
    const auto X = [&](float x) { return d.box.x0 + x * s; };
    const auto Y = [&](float y) { return d.box.y0 + y * s; };
    const auto R = [&](const U& u) { return Rect{X(u.x0), Y(u.y0), X(u.x1), Y(u.y1)}; };
    const auto inset = [](const U& u, float m) { return U{u.x0 + m, u.y0 + m, u.x1 - m, u.y1 - m}; };
    const auto shape = [&](const U& u, float radius, uint32_t fill) {
        kit.Fill(R(u), radius * s, fill);
        kit.Stroke(R(u), radius * s, kLine, kInk);
    };
    const auto detail = [&](const U& u, float radius, float opacity) {
        kit.Stroke(R(u), radius * s, kDetail, kInk, opacity);
    };
    const auto circle = [&](DsPoint c, float r, uint32_t fill) {
        shape({c.x - r, c.y - r, c.x + r, c.y + r}, r, fill);
    };
    const auto ring = [&](DsPoint c, float r, float opacity) {
        detail({c.x - r, c.y - r, c.x + r, c.y + r}, r, opacity);
    };
    const auto dot = [&](float x, float y, float r, float opacity) {
        kit.Dot(X(x), Y(y), r * s, kInk, opacity);
    };

    // The shoulder buttons on the base's back corners, L and R outside, ZL and ZR in from them;
    // the base covers their lower part.
    shape({1.5f, 92.8f, 24.0f, kBase + 6.0f}, 5.0f, kPart);
    shape({26.0f, 94.0f, 34.0f, kBase + 6.0f}, 3.0f, kPart);
    shape({kHalfW - 24.0f, 92.8f, kHalfW - 1.5f, kBase + 6.0f}, 5.0f, kPart);
    shape({kHalfW - 34.0f, 94.0f, kHalfW - 26.0f, kBase + 6.0f}, 3.0f, kPart);

    // The lid: the shell and its bevel, the screen's black surround, the screen; the inner camera
    // and its light, the speakers, the 3D depth slider and its light at the screen's right.
    shape(kLid, kShellR, kShell);
    detail(inset(kLid, 2.2f), kShellR - 2.2f, 0.3f);
    kit.Fill(R(inset(kTopScreen, -4.5f)), 3.0f * s, kBezel);
    detail(inset(kTopScreen, -4.5f), 3.0f, 0.5f);
    kit.Fill(R(kTopScreen), 0.8f * s, kGlass);
    detail(kTopScreen, 0.8f, 0.55f);
    ring({80.0f, 7.4f}, 1.8f, 0.7f);
    dot(80.0f, 7.4f, 0.7f, 0.7f);
    dot(86.0f, 7.4f, 0.6f, 0.5f);
    for (int row = 0; row < 4; row++) {
        const float y = 50.0f + static_cast<float>(row) * 4.0f;
        for (const float x : {10.5f, 13.5f, kHalfW - 13.5f, kHalfW - 10.5f}) {
            dot(x, y, 0.65f, 0.6f);
        }
    }
    shape({140.3f, 19.0f, 143.1f, 42.0f}, 1.4f, kWell);
    kit.Fill(R({139.6f, 36.0f, 143.8f, 41.4f}), 1.2f * s, kPart);
    detail({139.6f, 36.0f, 143.8f, 41.4f}, 1.2f, 0.8f);
    dot(141.7f, 14.8f, 0.8f, 0.6f);

    // The base: the shell and its bevel, the touch screen in its frame.
    shape(kBaseShell, kShellR, kShell);
    detail(inset(kBaseShell, 2.2f), kShellR - 2.2f, 0.3f);
    kit.Fill(R(inset(kBottomScreen, -3.5f)), 2.5f * s, kBezel);
    detail(inset(kBottomScreen, -3.5f), 2.5f, 0.5f);
    kit.Fill(R(kBottomScreen), 0.8f * s, kGlass);
    detail(kBottomScreen, 0.8f, 0.55f);

    // The hinge: a slim barrel over the join, between the shoulder buttons.
    shape(kBarrel, 2.0f, kShell);
    kit.Line(X(kBarrel.x0 + 2.0f), Y(96.1f), X(kBarrel.x1 - 2.0f), Y(96.1f), kDetail, kInk, 0.25f);

    // Circle Pad: the well, the cap, the cap's grip ring.
    circle(kCirclePad, kCirclePadWell, kWell);
    circle(kCirclePad, kCirclePadCap, kPart);
    ring(kCirclePad, kCirclePadCap - 2.6f, 0.35f);
    // D-pad: the cross (its outline as twelve sides), the dimple in the middle.
    {
        const float a = kDpadArm, h = kDpadHalf, cx = kDpad.x, cy = kDpad.y;
        kit.Fill(R({cx - a, cy - h, cx + a, cy + h}), 0.8f * s, kPart);
        kit.Fill(R({cx - h, cy - a, cx + h, cy + a}), 0.8f * s, kPart);
        const std::array<DsPoint, 12> p{{{cx - h, cy - a},
                                         {cx + h, cy - a},
                                         {cx + h, cy - h},
                                         {cx + a, cy - h},
                                         {cx + a, cy + h},
                                         {cx + h, cy + h},
                                         {cx + h, cy + a},
                                         {cx - h, cy + a},
                                         {cx - h, cy + h},
                                         {cx - a, cy + h},
                                         {cx - a, cy - h},
                                         {cx - h, cy - h}}};
        for (std::size_t i = 0; i < p.size(); i++) {
            const DsPoint& q = p[i];
            const DsPoint& n = p[(i + 1) % p.size()];
            kit.Line(X(q.x), Y(q.y), X(n.x), Y(n.y), kLine, kInk);
        }
        ring(kDpad, 1.4f, 0.35f);
    }
    // A, B, X, Y; the C-Stick; Select over Start; Home under the touch screen.
    for (const int b : {DsButton::A, DsButton::B, DsButton::X, DsButton::Y}) {
        circle(UnitAnchor(b), kFaceR, kPart);
    }
    circle(kCStick, kCStickR, kPart);
    ring(kCStick, 1.0f, 0.4f);
    circle(kSelect, kSmallR, kPart);
    circle(kStart, kSmallR, kPart);
    shape({kHome.x - 5.25f, kHome.y - 2.3f, kHome.x + 5.25f, kHome.y + 2.3f}, 2.3f, kPart);

    // The chosen input: a ring in the primary colour.
    if (lit >= 0) {
        const DsPoint c = d.Anchor(lit);
        const float r = d.Radius(lit) + 4.0f;
        kit.Stroke({c.x - r, c.y - r, c.x + r, c.y + r}, r, 2.0f, Tok::kPrimary);
    }
}

void DrawDsDot(Kit& kit, const DsDrawing& d, int target, bool lit) {
    // Smaller on the small buttons, so they still show round it.
    const DsPoint c = d.Anchor(target);
    const float r = std::min(3.5f, d.Radius(target) * 0.6f);
    kit.Dot(c.x, c.y, lit ? r + 1.0f : r, Tok::kPrimary);
}

void DrawDsGlyph(Kit& kit, const Rect& b, int target, uint32_t fg, uint32_t bg) {
    const float cx = b.Cx(), cy = b.Cy(), r = std::min(b.W(), b.H()) * 0.5f;
    const auto letter = [&](const char* t, float size) {
        kit.TextSized(cx, cy + kit.CapHeightSized(size, true) * 0.5f, size, true, fg, t, 1);
    };
    const auto pill = [&](const char* t) {
        const Rect p{cx - r, cy - r * 0.62f, cx + r, cy + r * 0.62f};
        kit.Fill(p, r * 0.3f, bg);
        kit.Stroke(p, r * 0.3f, 1.2f, fg, 0.7f);
        letter(t, r * 0.95f);
    };
    switch (target) {
    case DsButton::A:
    case DsButton::B:
    case DsButton::X:
    case DsButton::Y: {
        kit.Dot(cx, cy, r, bg);
        kit.Stroke({cx - r, cy - r, cx + r, cy + r}, r, 1.2f, fg, 0.7f);
        static constexpr const char* kNames[] = {"A", "B", "X", "Y"};
        letter(kNames[target], r * 1.15f);
        break;
    }
    case DsButton::Up:
    case DsButton::Down:
    case DsButton::Left:
    case DsButton::Right: {
        // The cross, the pressed arm marked.
        const float arm = r * 0.36f;
        kit.Fill({cx - r, cy - arm, cx + r, cy + arm}, arm * 0.5f, bg);
        kit.Fill({cx - arm, cy - r, cx + arm, cy + r}, arm * 0.5f, bg);
        const float dx = target == DsButton::Left ? -1.0f : target == DsButton::Right ? 1.0f : 0.0f;
        const float dy = target == DsButton::Up ? -1.0f : target == DsButton::Down ? 1.0f : 0.0f;
        kit.Dot(cx + dx * r * 0.66f, cy + dy * r * 0.66f, r * 0.26f, fg);
        break;
    }
    case DsButton::L:
        pill("L");
        break;
    case DsButton::R:
        pill("R");
        break;
    case DsButton::ZL:
        pill("ZL");
        break;
    case DsButton::ZR:
        pill("ZR");
        break;
    case DsButton::Start:
    case DsButton::Select: {
        // The two small pills, one lit (Select over Start, as on the console).
        const float w = r * 0.9f, h = r * 0.34f;
        const Rect top{cx - w, cy - r * 0.62f, cx + w, cy - r * 0.62f + h * 2.0f};
        const Rect low{cx - w, cy + r * 0.62f - h * 2.0f, cx + w, cy + r * 0.62f};
        const bool start = target == DsButton::Start;
        kit.Fill(top, h, start ? bg : fg);
        kit.Fill(low, h, start ? fg : bg);
        break;
    }
    case DsButton::Home:
        kit.Dot(cx, cy, r, bg);
        kit.Icon(cx, cy, r * 1.3f, Icon::kHome, false, fg);
        break;
    case DsButton::CirclePad:
        kit.Dot(cx, cy, r, bg);
        kit.Stroke({cx - r, cy - r, cx + r, cy + r}, r, 1.2f, fg, 0.7f);
        kit.Stroke({cx - r * 0.55f, cy - r * 0.55f, cx + r * 0.55f, cy + r * 0.55f}, r * 0.55f, 1.4f, fg);
        break;
    case DsButton::CStick:
        kit.Dot(cx, cy, r * 0.8f, bg);
        kit.Dot(cx, cy, r * 0.32f, fg);
        break;
    default:
        kit.Dot(cx, cy, r * 0.7f, bg);
        break;
    }
}

} // namespace FrameXr::Menu
