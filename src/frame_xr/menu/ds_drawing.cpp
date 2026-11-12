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

// The console in its own units (x right, y down; 160 across, about a millimetre), opened flat
// and seen from the front, laid out after a front photo of an open New 3DS XL: the lid (well
// rounded top corners, a screw mark in each corner, the camera and its light between two round
// dots on the top edge, a five-dot speaker grille each side of the screen, the volume slider on
// the left edge and the 3D slider on the right), the hinge (a long barrel between two end caps),
// the base (the Circle Pad in its ring over the D-pad; the C-Stick by the touch screen's corner,
// A B X Y under it, Start over Select under those; the touch screen in a raised surround that runs
// to the bottom edge, Home on it under the screen) with the shoulder buttons along its top edge.
// Drawn as the Frame controllers' drawings on the Controls tab are: 1.5 dp outlines, finer detail
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
constexpr float kMid = 80.0f; ///< the console's centre line
constexpr U Mirror(const U& u) {
    return {2.0f * kMid - u.x1, u.y0, 2.0f * kMid - u.x0, u.y1};
}
constexpr DsPoint Centre(const U& u) {
    return {(u.x0 + u.x1) * 0.5f, (u.y0 + u.y1) * 0.5f};
}

// The lid runs on under the hinge (its lower corners hidden there).
constexpr U kLid{0.0f, 0.0f, 160.0f, 86.0f};
constexpr float kLidR = 10.0f;
constexpr U kTopBezel{23.0f, 8.3f, 137.0f, 79.0f};
constexpr U kTopScreen{26.0f, 11.4f, 134.0f, 75.6f};
constexpr float kGrilleY = 35.5f, kGrilleX = 11.6f, kGrilleArm = 4.0f;
constexpr U kVolSlot{-0.8f, 50.0f, 1.8f, 66.0f};
// The hinge: the end caps the halves' full depth, the barrel between them slimmer.
constexpr float kHingeY0 = 79.5f, kHingeY1 = 89.5f;
constexpr U kHingeCap{0.0f, kHingeY0, 15.6f, kHingeY1};
constexpr U kHingeBarrel{15.0f, kHingeY0 + 1.0f, 145.0f, kHingeY1 - 1.0f};
// The base, and the shoulder buttons along its top edge (the hinge hides their upper part).
constexpr DsPoint Shoulder(const U& u) {
    return {(u.x0 + u.x1) * 0.5f, (89.5f + u.y1) * 0.5f};
}
constexpr U kBaseShell{0.0f, kHingeY1, 160.0f, 174.0f};
constexpr float kBaseR = 12.0f;
constexpr U kShoulderL{1.5f, 85.0f, 15.5f, 94.5f};
constexpr U kShoulderZL{17.5f, 85.0f, 26.5f, 93.5f};
constexpr U kSurround{34.0f, 90.6f, 126.0f, 174.0f};
constexpr U kBottomScreen{37.0f, 94.0f, 123.0f, 159.0f};
constexpr U kHomeKey{74.5f, 163.7f, 85.5f, 168.7f};

constexpr DsPoint kCirclePad{16.2f, 107.5f};
constexpr float kCirclePadRing = 10.8f, kCirclePadCap = 7.6f;
constexpr DsPoint kDpad{15.4f, 134.2f};
constexpr float kDpadArm = 9.4f, kDpadHalf = 3.0f;
constexpr DsPoint kFace{140.8f, 112.5f};
constexpr float kFaceGap = 7.6f, kFaceR = 3.5f;
constexpr DsPoint kCStick{132.0f, 98.6f};
constexpr float kCStickR = 3.0f;
constexpr DsPoint kStart{136.5f, 138.5f};
constexpr DsPoint kSelect{136.5f, 148.5f};
constexpr float kSmallR = 2.4f;
constexpr DsPoint kHome = Centre(kHomeKey);

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
        return Shoulder(kShoulderL);
    case DsButton::R:
        return Shoulder(Mirror(kShoulderL));
    case DsButton::ZL:
        return Shoulder(kShoulderZL);
    case DsButton::ZR:
        return Shoulder(Mirror(kShoulderZL));
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
        return {kMid, kHingeY1};
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
        return kCirclePadRing * s;
    case DsButton::CStick:
        return kCStickR * s;
    case DsButton::Select:
    case DsButton::Start:
    case DsButton::Home:
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

    // The lid: the shell and its bevel, the screw marks in its corners, the screen's black
    // surround, the screen; on the top edge the camera with its light between two round dots; a
    // five-dot speaker grille each side; the volume slider on the left edge, the 3D slider on the
    // right (at Off), each with its wedge beside it.
    shape(kLid, kLidR, kShell);
    detail(inset(kLid, 2.0f), kLidR - 2.0f, 0.3f);
    for (const float x : {11.8f, 2.0f * kMid - 11.8f}) {
        for (const float y : {9.0f, 75.5f}) {
            detail({x - 1.2f, y - 1.2f, x + 1.2f, y + 1.2f}, 0.5f, 0.45f);
        }
    }
    kit.Fill(R(kTopBezel), 3.0f * s, kBezel);
    detail(kTopBezel, 3.0f, 0.5f);
    kit.Fill(R(kTopScreen), 0.8f * s, kGlass);
    detail(kTopScreen, 0.8f, 0.55f);
    circle({kMid, 3.6f}, 2.3f, kWell);
    dot(kMid, 3.6f, 0.9f, 0.7f);
    dot(kMid + 4.6f, 3.6f, 0.7f, 0.6f);
    circle({42.6f, 3.4f}, 1.6f, kPart);
    circle({2.0f * kMid - 42.6f, 3.4f}, 1.6f, kPart);
    for (const float cx : {kGrilleX, 2.0f * kMid - kGrilleX}) {
        for (const DsPoint o : {DsPoint{0, 0}, DsPoint{0, -kGrilleArm}, DsPoint{0, kGrilleArm},
                                DsPoint{-kGrilleArm, 0}, DsPoint{kGrilleArm, 0}}) {
            dot(cx + o.x, kGrilleY + o.y, 0.6f, 0.65f);
        }
    }
    const auto slider = [&](const U& slot, float knob_y, float wedge_x, float inward) {
        shape(slot, 1.3f, kWell);
        const U knob{slot.x0 - 0.4f, knob_y, slot.x1 + 0.4f, knob_y + 4.5f};
        kit.Fill(R(knob), 1.2f * s, kPart);
        detail(knob, 1.2f, 0.8f);
        kit.Line(X(wedge_x), Y(slot.y0 + 1.5f), X(wedge_x), Y(slot.y1 - 2.0f), kDetail, kInk, 0.5f);
        kit.Line(X(wedge_x + inward * 2.8f), Y(slot.y0 + 1.5f), X(wedge_x), Y(slot.y1 - 2.0f), kDetail, kInk,
                 0.5f);
    };
    slider(kVolSlot, kVolSlot.y0 + 1.0f, 4.2f, 1.0f);
    slider(Mirror(kVolSlot), kVolSlot.y1 - 5.5f, 2.0f * kMid - 4.2f, -1.0f);

    // The base: the shell and its bevel, the
    // raised surround (to the bottom edge) with the touch screen and Home.
    shape(kBaseShell, kBaseR, kShell);
    detail(inset(kBaseShell, 2.0f), kBaseR - 2.0f, 0.3f);
    // The shoulder buttons' faces along its top edge, under the hinge.
    for (const U& u : {kShoulderL, Mirror(kShoulderL), kShoulderZL, Mirror(kShoulderZL)}) {
        const float r = u.x1 - u.x0 > 10.0f ? 3.0f : 2.0f;
        shape({u.x0, kHingeY1 - 1.0f, u.x1, u.y1}, std::min(r, (u.y1 - kHingeY1) * 0.5f), kPart);
    }
    shape(kSurround, 4.0f, kWell);
    kit.Fill(R(kBottomScreen), 0.8f * s, kGlass);
    detail(kBottomScreen, 0.8f, 0.55f);
    shape(kHomeKey, 2.5f, kPart);
    {
        // The house on Home.
        const float hx = kHome.x, hy = kHome.y;
        kit.Line(X(hx - 1.4f), Y(hy - 0.1f), X(hx), Y(hy - 1.5f), kDetail, kInk, 0.7f);
        kit.Line(X(hx), Y(hy - 1.5f), X(hx + 1.4f), Y(hy - 0.1f), kDetail, kInk, 0.7f);
        kit.Line(X(hx - 1.0f), Y(hy - 0.4f), X(hx - 1.0f), Y(hy + 1.3f), kDetail, kInk, 0.7f);
        kit.Line(X(hx + 1.0f), Y(hy - 0.4f), X(hx + 1.0f), Y(hy + 1.3f), kDetail, kInk, 0.7f);
        kit.Line(X(hx - 1.0f), Y(hy + 1.3f), X(hx + 1.0f), Y(hy + 1.3f), kDetail, kInk, 0.7f);
    }

    // The hinge: the long barrel, the two end caps over its ends.
    shape(kHingeBarrel, (kHingeBarrel.y1 - kHingeBarrel.y0) * 0.5f, kShell);
    kit.Line(X(kHingeBarrel.x0 + 2.0f), Y((kHingeY0 + kHingeY1) * 0.5f), X(kHingeBarrel.x1 - 2.0f),
             Y((kHingeY0 + kHingeY1) * 0.5f), kDetail, kInk, 0.2f);
    shape(kHingeCap, 4.5f, kShell);
    shape(Mirror(kHingeCap), 4.5f, kShell);

    // Circle Pad: the recessed ring, the cap, the cap's grip ring.
    circle(kCirclePad, kCirclePadRing, kWell);
    ring(kCirclePad, kCirclePadRing - 1.6f, 0.3f);
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
    // A, B, X, Y; the C-Stick above them; Start over Select below them.
    for (const int b : {DsButton::A, DsButton::B, DsButton::X, DsButton::Y}) {
        circle(UnitAnchor(b), kFaceR, kPart);
    }
    circle(kCStick, kCStickR, kPart);
    ring(kCStick, 1.2f, 0.4f);
    circle(kStart, kSmallR, kPart);
    circle(kSelect, kSmallR, kPart);

    // The chosen input: a ring in the primary colour.
    if (lit >= 0 && !Remap::IsExtra(lit)) { // padsui: the mouse's own targets are not on the 3DS
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
        // The two small round buttons, Start over Select, the one meant lit.
        const float br = r * 0.38f, dy = r * 0.5f;
        const bool start = target == DsButton::Start;
        kit.Dot(cx, cy - dy, br, start ? fg : bg);
        kit.Dot(cx, cy + dy, br, start ? bg : fg);
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
    case DsButton::Touch: {
        // AzaharFrame padsui: a small screen with a touch on it.
        const Rect sc{cx - r * 0.9f, cy - r * 0.68f, cx + r * 0.9f, cy + r * 0.68f};
        kit.Fill(sc, r * 0.18f, bg);
        kit.Stroke(sc, r * 0.18f, 1.2f, fg, 0.7f);
        kit.Dot(cx + r * 0.2f, cy + r * 0.1f, r * 0.26f, fg);
        break;
    }
    case DsButton::OpenMenu: {
        // AzaharFrame padsui: three lines, a menu.
        kit.Dot(cx, cy, r, bg);
        const float w = r * 0.56f, h = r * 0.09f;
        for (int i = -1; i <= 1; i++) {
            const float y = cy + static_cast<float>(i) * r * 0.32f;
            kit.Fill({cx - w, y - h, cx + w, y + h}, h, fg);
        }
        break;
    }
    default:
        kit.Dot(cx, cy, r * 0.7f, bg);
        break;
    }
}

} // namespace FrameXr::Menu
