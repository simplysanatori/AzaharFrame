// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: draws each tab of the in-game menu with the real menu code into PNG files,
// with sample values, for design reviews without a headset.
//
//   frame_xr_menu_dump OUT_DIR [--models DIR] [--px-per-dp N]
//
// DIR holds frame_controller_left.model and frame_controller_right.model (the files the
// installer makes, in the user folder's frame-models/); without them the Controls tab is empty.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <lodepng.h>

#include "frame_xr/input/controls.h"
#include "frame_xr/menu/edit_devices.h"
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/vis/help_content.h"
#include "frame_xr/vis/help_layout.h"
#include "frame_xr/vis/menu_diagram.h"
#include "frame_xr/vis/model_file.h"

using namespace FrameXr;
using namespace FrameXr::Menu;

namespace {

constexpr float kMargin = 24.0f; // dp of backdrop round the panel
constexpr uint32_t kBackdrop = 0x2A2D31;

Menu::MenuModel SampleModel() {
    Menu::MenuModel m;
    m.game_title = "The Legend of Zelda: Ocarina of Time 3D";
    m.hide_bottom_screen = false;
    m.screen_shape = 0;
    m.screen_curve = 4; // Strong
    m.immersive_offset = "Right 1.5°, Up 0.5°";
    m.immersive_offset_set = true;
    m.show_stats = true;
    m.stats_game = {{"App FPS", "30"},
                    {"Speed", "100%"},
                    {"Frame Time", "4.82 ms"},
                    {"Peak Frame Time", "11.30 ms"},
                    {"Slow Frames", "2"}};
    m.stats_vr = {{"Display", "90 Hz"}, {"Late Frames", "0"}};
    m.stats_system = {{"CPU", "38%"}, {"GPU Clock", "587 MHz"}};
    m.version = "1.1.0";
    m.controls_page = 0;
    m.move_screens = 1;
    m.scope = 0;
    m.background = 0;
    m.face_buttons = 0;
    m.motion_controls = 1;
    m.show_controllers = true;
    m.window_scale = 1;
    m.immersive_mode = 2;
    m.wider_view = 2;
    m.game_fov = 60;
    m.narrow_screen = 0;
    m.slots = {{1, "2026-11-08 21:14"}, {2, "2026-11-09 19:02"}, {5, "2026-11-10 08:47"}};
    m.depth_factor = 0;
    return m;
}

bool WritePng(const std::string& path, const frame_xr::ui::Canvas& cv,
              bool keep_alpha = false) {
    // The canvas is premultiplied, R in the low byte: over the opaque backdrop it is plain RGB.
    std::vector<unsigned char> rgba(static_cast<std::size_t>(cv.w) * cv.h * 4);
    for (std::size_t i = 0; i < cv.px.size(); i++) {
        const uint32_t p = cv.px[i];
        const uint32_t a = keep_alpha ? (p >> 24) : 255u;
        // Un-premultiply when the alpha is kept (rounded corners).
        const auto un = [&](uint32_t c) {
            return static_cast<unsigned char>(keep_alpha && a ? std::min(255u, (c * 255u + a / 2) / a) : c);
        };
        rgba[i * 4 + 0] = un(p & 255);
        rgba[i * 4 + 1] = un((p >> 8) & 255);
        rgba[i * 4 + 2] = un((p >> 16) & 255);
        rgba[i * 4 + 3] = static_cast<unsigned char>(a);
    }
    const unsigned err = lodepng::encode(path, rgba, static_cast<unsigned>(cv.w),
                                         static_cast<unsigned>(cv.h));
    if (err) {
        std::fprintf(stderr, "%s: %s\n", path.c_str(), lodepng_error_text(err));
        return false;
    }
    std::printf("%s\n", path.c_str());
    return true;
}

// AzaharFrame padsui: the mouse's pointer over the panel, as VR draws it (controls.cpp's MakeDot
// and MakeRingAtlas, the same formulas): the dot and, while Left and Right Click are held, the
// ring round it, `fill` of the way round.
struct MouseOverlay {
    bool shown = false;
    float x = 0.0f, y = 0.0f; ///< dp on the panel
    float ring = 0.0f;        ///< 0 none, else how far round
};

void DrawMouseOverlay(frame_xr::ui::Canvas& cv, const MouseOverlay& o, float px_per_dp, float ox) {
    if (!o.shown) {
        return;
    }
    const float cx = ox + o.x * px_per_dp, cy = ox + o.y * px_per_dp;
    // The dot: about 12 dp across on a 1 m menu at arm's length; the ring three times that.
    const float dot_r = 6.0f * px_per_dp;
    const float ring_outer = 18.0f * px_per_dp - 2.0f * px_per_dp * 18.0f / 32.0f;
    const float ring_w = 7.0f * 18.0f / 32.0f * px_per_dp;
    const int reach = static_cast<int>(18.0f * px_per_dp) + 2;
    for (int y = static_cast<int>(cy) - reach; y <= static_cast<int>(cy) + reach; y++) {
        for (int x = static_cast<int>(cx) - reach; x <= static_cast<int>(cx) + reach; x++) {
            if (x < 0 || y < 0 || x >= cv.w || y >= cv.h) {
                continue;
            }
            const float dx = static_cast<float>(x) + 0.5f - cx, dy = static_cast<float>(y) + 0.5f - cy;
            const float d = std::hypot(dx, dy);
            if (o.ring > 0.0f) {
                const float a = std::clamp(ring_outer - d, 0.0f, 1.0f) *
                                std::clamp(d - (ring_outer - ring_w), 0.0f, 1.0f);
                if (a > 0.0f) {
                    float turn = std::atan2(dx, -dy) / 6.2831853f;
                    if (turn < 0.0f) {
                        turn += 1.0f;
                    }
                    const bool done = turn <= o.ring;
                    cv.Blend(x, y,
                             done ? frame_xr::ui::Color{168.0f, 199.0f, 250.0f, 1.0f}
                                  : frame_xr::ui::Color{255.0f, 255.0f, 255.0f, 1.0f},
                             a * (done ? 1.0f : 0.35f));
                }
            }
            const float alpha = std::clamp(dot_r - d, 0.0f, 1.0f);
            if (alpha > 0.0f) {
                const float white = std::clamp(dot_r * 0.72f - d, 0.0f, 1.0f);
                const float v = white * 255.0f + (1.0f - white) * 40.0f;
                cv.Blend(x, y, {v, v, v, 1.0f}, alpha);
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s OUT_DIR [--models DIR] [--px-per-dp N] [--diagram-only]\n", argv[0]);
        return 2;
    }
    const std::string out = argv[1];
    std::string models;
    float px_per_dp = 2.5f;
    bool diagram_only = false;
    for (int i = 2; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--diagram-only") {
            diagram_only = true;
        } else if (i + 1 >= argc) {
            break;
        } else if (a == "--models") {
            models = argv[i + 1];
        } else if (a == "--px-per-dp") {
            px_per_dp = static_cast<float>(std::atof(argv[i + 1]));
        }
        if (a == "--models" || a == "--px-per-dp") {
            i++;
        }
    }

    Kit kit(px_per_dp);
    TabMenu menu;

    // The Controls tab: the default profile's diagram, from the installed model files.
    Vis::MenuDiagramArt art;
    const Vis::HelpDiagram diagram = Vis::BuildHelpDiagram(
        Vis::DefaultHelpBindings(), {.face_by_position = true, .in_menu = true, .game_running = true});
    if (!models.empty()) {
        Vis::ModelFile left, right;
        std::string why;
        if (Vis::LoadModelFile(models + "/frame_controller_left.model", left, why) &&
            Vis::LoadModelFile(models + "/frame_controller_right.model", right, why)) {
            art = Vis::MakeMenuDiagramArt(left, right, px_per_dp * 0.6f);
        } else {
            std::fprintf(stderr, "models: %s\n", why.c_str());
        }
    }
    menu.SetDiagram([&](Kit& k, const Rect& area, int page) {
        Vis::DrawMenuDiagram(k, area, diagram, static_cast<Vis::DiagramPageId>(page), art);
    });

    if (diagram_only) {
        // Just the In Game diagram on the menu's pane colour, for the README picture.
        constexpr float kPadDp = 12.0f;
        const float dw = 496.0f + kPadDp * 2.0f;
        const float dh = 496.0f * (Vis::kDiagramBottom - Vis::kDiagramTop) / Vis::kDiagramPanelW +
                         kPadDp * 2.0f;
        frame_xr::ui::Canvas cv;
        cv.Init(static_cast<int>(dw * px_per_dp + 0.5f), static_cast<int>(dh * px_per_dp + 0.5f));
        kit.Begin(cv);
        kit.Fill({0, 0, dw, dh}, 24.0f, Tok::kSurface);
        Vis::DrawMenuDiagram(kit, {kPadDp, kPadDp, dw - kPadDp, dh - kPadDp}, diagram,
                             Vis::DiagramPageId::Game, art);
        return WritePng(out + "/controls.png", cv, true) ? 0 : 1;
    }

    const int w = static_cast<int>((kPanelW + kMargin * 2.0f) * px_per_dp + 0.5f);
    const int h = static_cast<int>((kPanelH + kMargin * 2.0f) * px_per_dp + 0.5f);
    const auto shot = [&](const std::string& name, const MenuModel& m, MenuView v,
                          const MouseOverlay& mouse = {}) {
        frame_xr::ui::Canvas cv;
        cv.Init(w, h);
        cv.Clear(Rgb(kBackdrop));
        // The panel is drawn at its own origin: a second canvas, then copied in at the margin.
        frame_xr::ui::Canvas panel;
        panel.Init(static_cast<int>(kPanelW * px_per_dp + 0.5f),
                   static_cast<int>(kPanelH * px_per_dp + 0.5f));
        kit.Begin(panel);
        menu.Draw(kit, m, v);
        if (v.scroll[static_cast<int>(v.tab)] < 0.0f) { // -1: scrolled to the end
            v.scroll[static_cast<int>(v.tab)] = menu.MaxScroll();
            panel.Init(panel.w, panel.h);
            menu.Draw(kit, m, v);
        }
        const int ox = static_cast<int>(kMargin * px_per_dp + 0.5f);
        for (int y = 0; y < panel.h; y++) {
            for (int x = 0; x < panel.w; x++) {
                const uint32_t p = panel.px[static_cast<std::size_t>(y) * panel.w + x];
                const float a = static_cast<float>(p >> 24) / 255.0f;
                if (a <= 0.0f) {
                    continue;
                }
                // Premultiplied over: un-premultiply into a colour and blend.
                cv.Blend(x + ox, y + ox,
                         {static_cast<float>(p & 255) / a, static_cast<float>((p >> 8) & 255) / a,
                          static_cast<float>((p >> 16) & 255) / a, 1.0f},
                         a);
            }
        }
        DrawMouseOverlay(cv, mouse, px_per_dp, static_cast<float>(ox));
        return WritePng(out + "/" + name + ".png", cv);
    };

    const MenuModel m = SampleModel();
    bool ok = true;
    MenuView v;
    v.tab = Tab::Main;
    v.hover = {Id::QuickSave};
    ok &= shot("1-main", m, v);
    // AzaharFrame padsui: the mouse's pointer on Quick Save.
    {
        MouseOverlay at;
        for (const auto& hit : menu.Hits()) {
            if (hit.t.id == Id::QuickSave) {
                at = {true, hit.r.Cx() + 120.0f, hit.r.Cy() - 22.0f};
            }
        }
        ok &= shot("1-main-mouse-pointer", m, v, at);
        at.ring = 0.6f;
        ok &= shot("1-main-mouse-chord", m, v, at);
    }
    v.page = Page::LoadSlots;
    v.hover = {Id::Slot, 2};
    ok &= shot("1b-main-load-slots", m, v);

    v = {};
    v.tab = Tab::Position;
    v.hover = {Id::ScreenCurve, kLabelIndex};
    ok &= shot("2-position", m, v);

    v.hover = {Id::ScreenCurve, 5};
    v.open_menu = Id::ScreenCurve;
    ok &= shot("2c-position-curve-menu", m, v);
    v.open_menu = Id::None;

    v.hover = {Id::ScreenShape, 3};
    v.open_menu = Id::ScreenShape;
    ok &= shot("2b-position-shape-menu", m, v);

    v = {};
    v.tab = Tab::Stats;
    ok &= shot("3-stats", m, v);

    v = {};
    v.tab = Tab::Controls;
    v.hover = {Id::ControlsPage, 1};
    ok &= shot("4-controls-in-game", m, v);
    MenuModel mw = m;
    mw.controls_page = 1;
    v.hover = {};
    ok &= shot("4b-controls-window", mw, v);

    // AzaharFrame padsui: Edit Controls with sample devices: the Frame controllers, a
    // DualShock 4 (a touchpad), a mouse.
    {
        using Input::Control;
        MenuModel me = m;
        EditDevice frame;
        frame.key = "frame";
        frame.name = "Frame Controllers";
        RemapTable ft;
        ft.fill(-1);
        const std::array<Control, 12> fc{Control::A,      Control::B,        Control::X,
                                         Control::Y,      Control::DpadUp,   Control::DpadDown,
                                         Control::DpadLeft, Control::DpadRight, Control::BumperL,
                                         Control::BumperR, Control::Menu,     Control::View};
        for (std::size_t i = 0; i < fc.size(); i++) {
            ft[i] = static_cast<int8_t>(fc[i]);
        }
        ft[DsButton::ZL] = static_cast<int8_t>(Control::TriggerL);
        ft[DsButton::ZR] = static_cast<int8_t>(Control::TriggerR);
        frame.mapping = ft;
        EditDevice ds4;
        ds4.key = "050000004c050000cc09000000810000";
        ds4.name = "DualShock 4";
        ds4.kind = DeviceKind::Gamepad;
        ds4.names = NamesFor(4, false);
        ds4.has_touchpad = true;
        ds4.mapping = PositionalPadTable();
        ds4.sticks = {0, Input::EncodeStickRoute({1, false, false, true})}; // C-Stick Y turned round
        EditDevice mouse; // AzaharFrame padsui: its default mapping
        mouse.key = "mouse";
        mouse.name = "Mouse";
        mouse.kind = DeviceKind::Mouse;
        mouse.names = ButtonNames::Mouse;
        mouse.sticks = {-1, -1};
        mouse.mapping[DsButton::A] = 1; // Right Click
        mouse.mapping[DsButton::L] = 3; // Back
        mouse.mapping[DsButton::R] = 4; // Forward
        mouse.extras = {0, 2};          // Left Click touches, Middle Click opens the menu
        ds4.enabled = true;
        me.devices.list = {frame, ds4, mouse};

        v = {};
        v.tab = Tab::Controls;
        v.remap.editing = true;
        v.hover = {Id::RemapChip, DsButton::R};
        ok &= shot("edit-3ds-frame", me, v);
        v.remap.listening = DsButton::X;
        v.hover = {Id::RemapCancel};
        ok &= shot("edit-3ds-listening", me, v);
        v.remap.listening = DsButton::CirclePad;
        v.remap.step = 1;
        ok &= shot("edit-3ds-stick", me, v);
        v.remap.step = 0;

        v.remap.listening = -1;
        v.hover = {Id::EditDevice, 2};
        me.devices.selected = 1;
        me.touchpad_touch = 1;
        me.devices.pointer = {.shown = true, .down = true, .x = 0.64f, .y = 0.38f};
        ok &= shot("edit-3ds-gamepad", me, v);
        // AzaharFrame padsui: the DualShock 4 not used (its dot hollow, the switch off).
        {
            MenuModel off = me;
            off.devices.list[1].enabled = false;
            v.hover = {Id::UseDevice};
            ok &= shot("edit-3ds-gamepad-off", off, v);
            v.hover = {Id::EditDevice, 2};
        }

        v.hover = {};
        me.devices.selected = 2;
        me.touchpad_touch = 2;
        me.devices.pointer = {.shown = true, .down = false, .x = 0.3f, .y = 0.62f};
        ok &= shot("edit-3ds-mouse", me, v);
        // AzaharFrame padsui: A (Right Click) listening; Mouse Mode: Pointer, the pointer on it;
        // the chord's ring half way.
        v.remap.listening = DsButton::A;
        v.hover = {Id::RemapCancel};
        ok &= shot("edit-3ds-mouse-listening", me, v);
        v.remap.listening = -1;
        MenuModel mp = me;
        mp.devices.list[2].mouse_mode = 1;
        mp.devices.pointer = {};
        v.hover = {Id::MouseMode, 1};
        const auto& hits = menu.Hits();
        MouseOverlay at;
        for (const auto& hit : hits) {
            if (hit.t.id == Id::MouseMode && hit.t.index == 1) {
                at = {true, hit.r.Cx() + 30.0f, hit.r.Cy() + 15.0f};
            }
        }
        ok &= shot("edit-3ds-mouse-pointer", mp, v, at);
        at.ring = 0.5f;
        ok &= shot("edit-3ds-mouse-chord", mp, v, at);
    }

    v = {};
    v.tab = Tab::Settings;
    v.hover = {Id::ShowControllers};
    ok &= shot("5-settings", m, v);
    v.hover = {Id::GameFov};
    v.scroll[static_cast<int>(Tab::Settings)] = -1.0f;
    ok &= shot("5b-settings-end", m, v);
    v.hover = {Id::NarrowScreen, kLabelIndex};
    v.tooltip = Id::NarrowScreen;
    ok &= shot("5c-settings-tooltip-option", m, v);
    MenuModel gv = m; // Global chosen, the game has its own Immersive Mode
    gv.locked[static_cast<std::size_t>(Id::ImmersiveMode)] = Lock::GameValue;
    v = {};
    v.tab = Tab::Settings;
    v.hover = {Id::ImmersiveMode, kLabelIndex};
    v.tooltip = Id::ImmersiveMode;
    ok &= shot("5e-settings-game-value-locked", gv, v);
    MenuModel off = m;
    off.immersive_mode = 0;
    v = {};
    v.tab = Tab::Settings;
    v.scroll[static_cast<int>(Tab::Settings)] = -1.0f;
    v.open_menu = Id::ImmersiveMode;
    v.hover = {Id::ImmersiveMode, 2};
    ok &= shot("5d-settings-immersive-menu", off, v);
    return ok ? 0 : 1;
}
