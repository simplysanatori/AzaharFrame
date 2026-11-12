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

// The console in units (x right, y down). Lines in the outline colour of the Frame controllers'
// drawings on the Controls tab, the shells a step lighter than the pane.
constexpr float kLine = 1.5f; // dp
constexpr uint32_t kInk = Tok::kOnSurfaceVariant;
constexpr uint32_t kShell = Tok::kSurfaceContainerLow;
constexpr uint32_t kPart = Tok::kSurfaceContainerHigh;
constexpr uint32_t kGlass = 0x0B0E12;

struct U {
    float x0, y0, x1, y1;
};
constexpr U kTopHalf{0, 0, 160, 92};
constexpr U kTopScreen{27, 15, 133, 79};
constexpr U kBottomHalf{0, 99, 160, 192};
constexpr U kBottomScreen{37, 106, 123, 170};
constexpr U kHinge{38, 88, 122, 102};
constexpr float kShellR = 11.0f;

constexpr DsPoint kCirclePad{21, 122};
constexpr float kCirclePadR = 9.5f;
constexpr DsPoint kDpad{21, 152};
constexpr float kDpadArm = 9.5f, kDpadHalf = 3.4f;
constexpr DsPoint kFace{140, 137};
constexpr float kFaceGap = 9.5f, kFaceR = 4.6f;
constexpr DsPoint kCStick{139, 112};
constexpr float kCStickR = 3.6f;
constexpr DsPoint kSelect{148, 165};
constexpr DsPoint kStart{148, 176};
constexpr DsPoint kHome{80, 181};

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
        return {13, 95};
    case DsButton::R:
        return {147, 95};
    case DsButton::ZL:
        return {31, 96};
    case DsButton::ZR:
        return {129, 96};
    case DsButton::Start:
        return kStart;
    case DsButton::Select:
        return kSelect;
    case DsButton::Home:
        return kHome;
    case kDsCirclePad:
        return kCirclePad;
    case kDsCStick:
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
    case kDsCirclePad:
        return kCirclePadR * s;
    case kDsCStick:
        return kCStickR * s;
    default:
        return 4.0f * s;
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
    const auto shape = [&](const Rect& r, float radius, uint32_t fill) {
        kit.Fill(r, radius, fill);
        kit.Stroke(r, radius, kLine, kInk);
    };
    const auto circle = [&](DsPoint c, float r, uint32_t fill) {
        shape({X(c.x - r), Y(c.y - r), X(c.x + r), Y(c.y + r)}, r * s, fill);
    };
    const auto pill = [&](DsPoint c, float hw, float hh) {
        shape({X(c.x - hw), Y(c.y - hh), X(c.x + hw), Y(c.y + hh)}, hh * s, kPart);
    };

    // The lid: shell, the screen's bezel, the screen, camera, speakers, the 3D slider.
    shape(R(kTopHalf), kShellR * s, kShell);
    kit.Stroke(R({kTopScreen.x0 - 5, kTopScreen.y0 - 5, kTopScreen.x1 + 5, kTopScreen.y1 + 5}), 5 * s,
               1.0f, kInk, 0.45f);
    shape(R(kTopScreen), 1.5f * s, kGlass);
    kit.Dot(X(80), Y(7), 1.6f * s, kInk, 0.8f);
    for (int i = 0; i < 4; i++) {
        const float y = 38.0f + static_cast<float>(i) * 5.0f;
        kit.Dot(X(13), Y(y), 0.9f * s, kInk, 0.7f);
        kit.Dot(X(147), Y(y), 0.9f * s, kInk, 0.7f);
    }
    shape(R({138.5f, 60, 141.5f, 75}), 1.5f * s, kPart);

    // The shoulder buttons, behind the base's back edge.
    shape(R({2, 92.5f, 25, 103}), 4.5f * s, kPart);
    shape(R({27, 94, 36, 103}), 3.0f * s, kPart);
    shape(R({135, 92.5f, 158, 103}), 4.5f * s, kPart);
    shape(R({124, 94, 133, 103}), 3.0f * s, kPart);

    // The base: shell, the screen's frame, the touch screen, the hinge over the join.
    shape(R(kBottomHalf), kShellR * s, kShell);
    kit.Stroke(R({kBottomScreen.x0 - 4, kBottomScreen.y0 - 4, kBottomScreen.x1 + 4, kBottomScreen.y1 + 4}),
               4 * s, 1.0f, kInk, 0.45f);
    shape(R(kBottomScreen), 1.5f * s, kGlass);
    shape(R(kHinge), (kHinge.y1 - kHinge.y0) * 0.5f * s, kShell);
    for (const float x : {52.0f, 108.0f}) {
        kit.Line(X(x), Y(kHinge.y0 + 2), X(x), Y(kHinge.y1 - 2), 1.0f, kInk, 0.6f);
    }

    // Circle Pad: the well and the cap.
    circle(kCirclePad, kCirclePadR + 1.8f, kShell);
    circle(kCirclePad, kCirclePadR - 1.8f, kPart);
    // D-pad: the cross, its outline as twelve sides.
    {
        const float a = kDpadArm, h = kDpadHalf, cx = kDpad.x, cy = kDpad.y;
        kit.Fill(R({cx - a, cy - h, cx + a, cy + h}), 1.0f * s, kPart);
        kit.Fill(R({cx - h, cy - a, cx + h, cy + a}), 1.0f * s, kPart);
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
    }
    // A, B, X, Y; the C-Stick nub; Select, Start; Home; Power.
    for (const int b : {DsButton::A, DsButton::B, DsButton::X, DsButton::Y}) {
        circle(UnitAnchor(b), kFaceR, kPart);
    }
    circle(kCStick, kCStickR, kPart);
    pill(kSelect, 4.5f, 2.2f);
    pill(kStart, 4.5f, 2.2f);
    pill(kHome, 6.0f, 3.0f);
    circle({21, 181}, 2.4f, kShell);

    // The selected input: a ring in the primary colour.
    if (lit >= 0) {
        const DsPoint c = d.Anchor(lit);
        const float r = d.Radius(lit) + 4.0f;
        kit.Stroke({c.x - r, c.y - r, c.x + r, c.y + r}, r, 2.0f, Tok::kPrimary);
    }
}

void DrawDsDot(Kit& kit, const DsDrawing& d, int target, bool lit) {
    const DsPoint c = d.Anchor(target);
    kit.Dot(c.x, c.y, lit ? 4.5f : 3.5f, Tok::kPrimary);
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
    case kDsCirclePad:
        kit.Dot(cx, cy, r, bg);
        kit.Stroke({cx - r, cy - r, cx + r, cy + r}, r, 1.2f, fg, 0.7f);
        kit.Stroke({cx - r * 0.55f, cy - r * 0.55f, cx + r * 0.55f, cy + r * 0.55f}, r * 0.55f, 1.4f, fg);
        break;
    case kDsCStick:
        kit.Dot(cx, cy, r * 0.8f, bg);
        kit.Dot(cx, cy, r * 0.32f, fg);
        break;
    default:
        kit.Dot(cx, cy, r * 0.7f, bg);
        break;
    }
}

} // namespace FrameXr::Menu
