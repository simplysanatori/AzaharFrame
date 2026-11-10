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

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <lodepng.h>

#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/vis/help_content.h"
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
    m.screen_curve = 1;
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
    m.scope = 0;
    m.background = 0;
    m.face_buttons = 0;
    m.motion_controls = 1;
    m.show_controllers = true;
    m.window_scale = 1;
    m.immersive_mode = 2;
    m.wider_view = 1.2f;
    m.game_fov = 60;
    m.squeeze_width = 0;
    m.depth_factor = 0;
    return m;
}

bool WritePng(const std::string& path, const frame_xr::ui::Canvas& cv) {
    // The canvas is premultiplied, R in the low byte: over the opaque backdrop it is plain RGB.
    std::vector<unsigned char> rgba(static_cast<std::size_t>(cv.w) * cv.h * 4);
    for (std::size_t i = 0; i < cv.px.size(); i++) {
        const uint32_t p = cv.px[i];
        rgba[i * 4 + 0] = static_cast<unsigned char>(p & 255);
        rgba[i * 4 + 1] = static_cast<unsigned char>((p >> 8) & 255);
        rgba[i * 4 + 2] = static_cast<unsigned char>((p >> 16) & 255);
        rgba[i * 4 + 3] = 255;
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

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s OUT_DIR [--models DIR] [--px-per-dp N]\n", argv[0]);
        return 2;
    }
    const std::string out = argv[1];
    std::string models;
    float px_per_dp = 2.5f;
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::string a = argv[i];
        if (a == "--models") {
            models = argv[i + 1];
        } else if (a == "--px-per-dp") {
            px_per_dp = static_cast<float>(std::atof(argv[i + 1]));
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

    const int w = static_cast<int>((kPanelW + kMargin * 2.0f) * px_per_dp + 0.5f);
    const int h = static_cast<int>((kPanelH + kMargin * 2.0f) * px_per_dp + 0.5f);
    const auto shot = [&](const std::string& name, const MenuModel& m, MenuView v) {
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
        return WritePng(out + "/" + name + ".png", cv);
    };

    const MenuModel m = SampleModel();
    bool ok = true;
    MenuView v;
    v.tab = Tab::Main;
    v.hover = {Id::QuickSave};
    ok &= shot("1-main", m, v);

    v = {};
    v.tab = Tab::Position;
    v.hover = {Id::ScreenCurve, 2};
    ok &= shot("2-position", m, v);

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

    v = {};
    v.tab = Tab::Settings;
    v.hover = {Id::ShowControllers};
    ok &= shot("5-settings", m, v);
    v.hover = {Id::GameFov};
    v.scroll[static_cast<int>(Tab::Settings)] = -1.0f;
    ok &= shot("5b-settings-end", m, v);
    v.hover = {Id::SqueezeWidth, -1};
    v.tooltip = Id::SqueezeWidth;
    ok &= shot("5c-settings-tooltip-option", m, v);
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
