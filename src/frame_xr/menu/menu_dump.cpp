// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: draws each tab of the in-game menu with the real menu code into PNG files,
// with sample values, for design reviews without a headset.
//
//   frame_xr_menu_dump OUT_DIR [--models DIR] [--px-per-dp N] [--texparts THUMBS_DIR]
//
// AzaharFrame texparts: --texparts draws only the texture pack Customize screens
// (texparts_dump.h), with the part previews from THUMBS_DIR/<key>.png.
//
// DIR holds frame_controller_left.model and frame_controller_right.model (the files the
// installer makes, in the user folder's frame-models/); without them the Controls tab is empty.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include <lodepng.h>

#include "frame_xr/input/controls.h"
#include "frame_xr/menu/edit_devices.h"
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/menu/texparts_dump.h" // AzaharFrame texparts
#include "frame_xr/input/mouse.h"   // AzaharFrame mouse: its one-time toast
#include "frame_xr/input/reticle.h" // AzaharFrame mouse
#include "frame_xr/toast_fit.h"     // AzaharFrame menuux
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

// AzaharFrame cheatstab: the Cheats segment's sample list (Stella Glow, USA), file order.
CheatsModel SampleCheats() {
    CheatsModel c;
    c.cheats = {
        {"No Bloom (USA)", CheatSource::Developer, true, ""},
        {"Infinite HP in Battle", CheatSource::Sharkive, true, ""},
        {"Max EXP After Battle", CheatSource::Sharkive, false, "for v1.0, this game is v1.1"},
        {"Max Money", CheatSource::Sharkive, false, ""},
        {"All Items x99", CheatSource::Sharkive, false, ""},
        {"Unlock All Songs", CheatSource::Sharkive, false, ""},
        {"Max Affinity With Everyone", CheatSource::Sharkive, false, ""},
        {"Walk Through Walls (Press L+R to Toggle, Hold to Keep It On)", CheatSource::Sharkive,
         false, ""},
        {"My Speed Hack", CheatSource::User, false, ""}};
    c.download = DownloadState::Done;
    c.status = "8 cheats added, 1 for another version";
    return c;
}

// AzaharFrame texui: the Mods tab's Mods and Textures segments, with the real catalog's and the
// owner's packs' names. Stella Glow (USA) with the developer's No Bloom loaded.
ModsListModel SampleModsStellaGlow() {
    ModsListModel mm;
    mm.loaded = {{"stella-glow-usa-no-bloom", "No Bloom", "simplysanatori", "https://github.com/simplysanatori/3ds-game-fixes", "", true, true, true}};
    return mm;
}

// Ocarina of Time 3D: the catalog's two Ocarina ReFramed builds.
ModsListModel SampleModsOot3d() {
    ModsListModel mm;
    mm.catalog = CatalogState::Ready;
    mm.available = {
        {"oot3d-ocarina-reframed-single", "Ocarina ReFramed (Single Screen)", "OTPR26", "https://github.com/OTPR26/OOT3DHud",
         false, OfferState::Install, "", false, "Updated Nov 2026"}, // AzaharFrame modcat
        {"oot3d-ocarina-reframed-dual", "Ocarina ReFramed (Dual Screen)", "OTPR26", "https://github.com/OTPR26/OOT3DHud",
         false, OfferState::Install, "", false, "Updated Oct 2026"}};
    return mm;
}

TexturesModel SampleTextures() {
    TexturesModel t;
    t.custom_textures = true;
    t.preload = false;
    t.packs = {{"oot3d-1080p", "1080p", "Henriko Magnifico \u00b7 3.2 \u00b7 ASTC/KTX \u00b7 640 MB", false},
               {"oot3d-4k", "4K", "Henriko Magnifico \u00b7 3.2 \u00b7 ASTC/KTX \u00b7 2.1 GB", true},
               {"oot3d-4k-png", "4K (PNG)", "PNG \u00b7 5.8 GB", false}};
    return t;
}

// Download Packs for Pokemon X: the catalog's pack for it, 1.8 GB (so it asks first).
TexturesModel SamplePokemonDownloads() {
    TexturesModel t;
    t.custom_textures = true;
    t.downloads = {{"pokemon-xy-hd", "Pokemon X and Y HD", "Ullr8, Gray-Rice",
                    "https://github.com/Gray-Rice/PokeTex-3DS", "1.8 GB", PackOfferState::Download,
                    -1, "", "Updated Feb 2025", false}};
    return t;
}

/// AzaharFrame zipinstall: Henriko Magnifico's verified packs, the longest author line.
TexturesModel SampleOot3dDownloads() {
    TexturesModel t;
    t.custom_textures = true;
    t.downloads = {{"oot3d-4k-henriko", "Ocarina of Time 3D 4K", "Henriko Magnifico",
                    "", "", PackOfferState::Download, -1, "",
                    "Updated Sep 2026", true},
                   {"oot3d-1080p-henriko", "Ocarina of Time 3D 1080p", "Henriko Magnifico",
                    "", "", PackOfferState::Downloading, 42,
                    "Downloading", "Updated Sep 2026", true},
                   {"mm3d-hd-deathwrench", "Majora's Mask 3D HD", "DeathWrench",
                    "https://github.com/DeathWrench/MM3DHD", "", PackOfferState::Installed, -1, "",
                    "Updated May 2021", false}};
    t.downloading = true;
    return t;
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
    std::string texparts; // AzaharFrame texparts
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
        } else if (a == "--texparts") {
            texparts = argv[i + 1];
        }
        if (a == "--models" || a == "--px-per-dp" || a == "--texparts") {
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
    if (!texparts.empty()) { // AzaharFrame texparts
        const auto draw = [&](const std::string& name, const MenuModel& model, MenuView view) {
            return shot(name, model, view);
        };
        return TexPartsDump::Run(texparts == "-" ? std::string() : texparts, m, draw) ? 0 : 1;
    }
    bool ok = true;

    // AzaharFrame mouse + menuux: toasts as hud.cpp draws them (a box sized by toast_fit.h, the
    // text centred; one line in the large font, else two in the small one), one row each.
    // AzaharFrame ui3: a list of toasts to one file (the Homebrew results have their own)
    const auto toast_shot = [&out](const std::string& file, const std::vector<std::string>& toasts) {
        namespace ui = frame_xr::ui;
        ui::Canvas cv;
        cv.Init(ToastFit::kAtlasW, ToastFit::kRowPx * static_cast<int>(toasts.size()));
        cv.Clear(Rgb(kBackdrop));
        const ui::Color white = ui::rgb(240, 240, 245);
        for (std::size_t i = 0; i < toasts.size(); i++) {
            const std::string& t = toasts[i];
            std::vector<std::string> lines;
            const ToastFit::Box box = ToastFit::Fit(ui::TextWidth(ui::kFontLarge, t.c_str()), [&] {
                lines = ui::WrapText(ui::kFontSmall, t,
                                     ToastFit::kAtlasW - 2.0f * ToastFit::kPadPx, 2);
                float w = 0.0f;
                for (const auto& l : lines) {
                    w = std::max(w, ui::TextWidth(ui::kFontSmall, l.c_str()));
                }
                return w;
            });
            const float top = static_cast<float>(i * ToastFit::kRowPx);
            const float w = static_cast<float>(box.box_px);
            cv.RoundRect(2, top + 2, w - 2, top + ToastFit::kRowPx - 2, 36,
                         ui::rgb(28, 28, 34, 0.92f));
            if (!box.two_lines) {
                cv.Text(w / 2.0f, top + 62, ui::kFontLarge, t.c_str(), white, 1);
            } else {
                for (std::size_t l = 0; l < lines.size() && l < 2; l++) {
                    cv.Text(w / 2.0f, top + (lines.size() == 1 ? 58 : 40 + 38 * l),
                            ui::kFontSmall, lines[l].c_str(), white, 1);
                }
            }
            std::printf("toast %zu: %d px%s: %s\n", i, box.box_px, box.two_lines ? ", two lines" : "",
                        t.c_str());
        }
        return WritePng(out + "/" + file, cv);
    };
    {
        frame_xr::ui::InitFonts();
        ok &= toast_shot("toasts.png",
                         {Input::kReticleHint, Input::kMouseHint,
                          "Async Shader Compilation: From the Next Start",
                          "Hide Bottom Screen: From the Next Start",
                          "Bottom Screen Reticle: Right Controller", "At Edge: Snap to Middle",
                          "Preparing Shaders 3 / 9", // AzaharFrame integ3: the stall lane's shader notice
                          "C-Stick & Gyro Downloaded. Restart to Enable."}); // AzaharFrame ui3: five seconds
        // AzaharFrame ui3: Check for Updates' results (five seconds)
        ok &= toast_shot("homebrew-no-updates-toast.png", {"No Updates Available"});
        ok &= toast_shot("homebrew-update-available-toast.png", {"Update Available"});
        ok &= toast_shot("homebrew-check-failed-toast.png", {"Couldn't Check for Updates"});
    }
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
    // AzaharFrame mouse: Bottom Screen Reticle on (At Edge under it), Click for Input.
    {
        MenuModel mr = m;
        mr.touchpad_touch = 1;
        mr.bottom_screen_reticle = 3; // AzaharFrame input2: Headset, the widest row
        mr.reticle_at_edge = 1;
        v = {};
        v.tab = Tab::Controls;
        v.hover = {Id::ReticleAtEdge, 1};
        ok &= shot("4c-controls-reticle", mr, v);
    }

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

    // AzaharFrame menuux: the renderer's rows, the Internal Resolution menu, Dynamic Resolution
    // at 1x, and the Position tab's longest offset.
    MenuModel rm = m;
    rm.internal_resolution = 3;
    v = {};
    v.tab = Tab::Settings;
    v.scroll[static_cast<int>(Tab::Settings)] = 5.0f * 64.0f;
    v.hover = {Id::DynamicResolution};
    ok &= shot("5f-settings-renderer", rm, v);
    v.open_menu = Id::InternalResolution;
    v.hover = {Id::InternalResolution, 3};
    ok &= shot("5g-settings-resolution-menu", rm, v);
    v.open_menu = Id::None;
    rm.internal_resolution = 1;
    v.hover = {Id::DynamicResolution, kLabelIndex};
    v.tooltip = Id::DynamicResolution;
    ok &= shot("5h-settings-dynamic-at-1x", rm, v);
    MenuModel om = m;
    om.immersive_offset = "Right 3.5°, Down 2.5°";
    om.immersive_offset_set = true;
    v = {};
    v.tab = Tab::Position;
    ok &= shot("2d-position-long-offset", om, v);

    // AzaharFrame cheatstab: the Mods tab's Cheats segment and each state.
    MenuModel cm = m;
    cm.mods.cheats = SampleCheats();
    v = {};
    v.tab = Tab::Mods;
    v.hover = {Id::Cheat, 2};
    ok &= shot("cheats", cm, v);
    v.hover = {};
    MenuModel dl = cm; // downloading again over a list
    dl.mods.cheats.download = DownloadState::Downloading;
    dl.mods.cheats.spin = 10;
    dl.mods.cheats.status.clear();
    ok &= shot("cheats-downloading-list", dl, v);
    MenuModel fl = cm; // a failed download over a list
    fl.mods.cheats.download = DownloadState::Failed;
    fl.mods.cheats.status = "Could not establish connection";
    ok &= shot("cheats-error-list", fl, v);
    v.scroll[static_cast<int>(Tab::Mods)] = -1.0f; // the end of the list
    ok &= shot("cheats-scrolled", cm, v);
    v = {};
    v.tab = Tab::Mods;
    MenuModel em = m;
    v.hover = {Id::DownloadCheats};
    ok &= shot("cheats-empty", em, v);
    v.hover = {};
    em.mods.cheats.download = DownloadState::Downloading;
    em.mods.cheats.spin = 10;
    ok &= shot("cheats-downloading", em, v);
    em.mods.cheats.download = DownloadState::Failed;
    em.mods.cheats.status = "Download failed: Could not establish connection";
    ok &= shot("cheats-error", em, v);
    em.mods.cheats.download = DownloadState::Done;
    em.mods.cheats.status = "No cheats for this game";
    ok &= shot("cheats-none-found", em, v);
    // AzaharFrame ui2: Install from File's browser over the Cheats segment
    {
        MenuModel cf = cm;
        ZipModel& z = cf.mods.zip;
        z.open = true;
        z.target = ZipTarget::Cheats;
        z.browse = true;
        z.folder = "/home/user/Downloads";
        z.folder_text = "~/Downloads";
        z.can_go_up = true;
        z.chips = {{"Downloads", "/home/user/Downloads"},
                   {"Documents", "/home/user/Documents"},
                   {"Home", "/home/user"}};
        z.entries = {{"Cheats", true, ""},
                     {"0004000000033500.txt", false, "4 KB"},
                     {"OoT3D Codes.zip", false, "12 KB"}};
        v.hover = {Id::ZipEntry, 1};
        ok &= shot("cheats-file-browse", cf, v);
        z.file = "readme.txt";
        z.error = "No Cheats in This File";
        v.hover = {};
        ok &= shot("cheats-file-error", cf, v);
        v.hover = {Id::AddCheat};
        ok &= shot("cheats-add-hover", cm, v);
        v.hover = {};
    }
    // AzaharFrame texui: the Mods and Textures segments.
    const Tab mods_tab = Tab::Mods;
    {
        MenuModel mm = m;
        mm.game_title = "Stella Glow";
        mm.mods.mods = SampleModsStellaGlow();
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Mods;
        ok &= shot("mods", mm, v);
        MenuModel me = m;
        v.hover = {Id::InstallMods};
        ok &= shot("mods-empty", me, v);
        MenuModel mi = m;
        mi.mods.mods = SampleModsOot3d();
        v.mods_page = ModsPage::Install;
        v.hover = {Id::ModInstall, 0};
        ok &= shot("mods-install", mi, v);
        { // AzaharFrame integ3: a homebrew game (no title id): No Mods for This Game at once
            MenuModel mh = m;
            mh.game_title = "Emerald 3DS";
            mh.mods.mods = {};
            mh.mods.mods.catalog = CatalogState::Ready;
            MenuView hv = v;
            hv.hover = {};
            ok &= shot("mods-install-homebrew", mh, hv);
        }
        { // AzaharFrame ui3: Kid Icarus: Uprising's MEGA patches, the version by the author, the date
            MenuModel mk = m;
            mk.game_title = "Kid Icarus: Uprising";
            mk.mods.mods.catalog = CatalogState::Ready;
            const char* kUrl = "https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/";
            for (const char* name : {"C-Stick & Gyro", "Widescreen 16:9", "Ultrawide 21:9"}) {
                ModOffer o;
                o.id = name;
                o.name = name;
                o.author = "Darkshade";
                o.url = kUrl;
                o.mod_version = "v172";
                o.updated = "Updated Oct 2026";
                mk.mods.mods.available.push_back(o);
            }
            ModOffer cd;
            cd.id = "kiu-cd-quality-soundtrack";
            cd.name = "CD Quality Soundtrack";
            cd.author = "Zara Nerd";
            cd.url = "https://gamebanana.com/members/6074804";
            cd.mod_version = "v1.0.0";
            cd.downloads = "Under 100 Downloads";
            cd.updated = "Updated Oct 2026";
            mk.mods.mods.available.push_back(cd);
            MenuView kv = v;
            kv.hover = {Id::ModInstall, 0};
            ok &= shot("mods-install-kiu", mk, kv);
            // AzaharFrame ui3: a GameBanana row with all four parts and a long author name: the
            // byline goes on to a second line at a " · "
            MenuModel ml = mk;
            ModOffer lg;
            lg.id = "kiu-long";
            lg.name = "Restored Japanese Voices";
            lg.author = "The Kid Icarus Uprising Restoration Collective";
            lg.url = "https://gamebanana.com/members/1";
            lg.mod_version = "v2.4.1";
            lg.version = "v3.2.0";
            lg.version_match = true;
            lg.downloads = "10K+ Downloads";
            lg.updated = "Updated Sep 2026";
            ml.mods.mods.available.insert(ml.mods.mods.available.begin(), lg);
            kv.hover = {Id::ModLink, 0};
            ok &= shot("mods-install-long", ml, kv);
        }
        v.hover = {Id::InstallFromZip, 1}; // AzaharFrame zipinstall
        ok &= shot("zip-mods-button", mi, v);
        MenuModel mz = mi;
        mz.mods.zip.open = true;
        mz.mods.zip.target = ZipTarget::Mods;
        mz.mods.zip.browse = true;
        mz.mods.zip.folder = "/home/user/Downloads/Mods";
        mz.mods.zip.folder_text = "~/Downloads/Mods";
        mz.mods.zip.can_go_up = true;
        mz.mods.zip.chips = {{"Downloads", "/home/user/Downloads"}, {"Home", "/home/user"}};
        mz.mods.zip.entries = {{"Ocarina-ReFramed-v1.5.9.zip", false, "3.4 MB"}};
        v.hover = {Id::ZipUp, -1};
        ok &= shot("zip-mods-browse", mz, v);
        MenuModel mp = mi; // downloading the first, the second waits
        mp.mods.mods.busy = true;
        mp.mods.mods.available[0].state = OfferState::Installing;
        mp.mods.mods.available[0].note = "Downloading 42%"; // AzaharFrame mega + integ3: Cancel
        mp.mods.mods.spin = 9;
        v.hover = {Id::ModCancel, 0};
        ok &= shot("mods-installing", mp, v);
        MenuModel mc = mi; // Dual Screen in, Single Screen asked for
        mc.mods.mods.available[1].state = OfferState::Installed;
        mc.mods.mods.dialog = {ModsDialogKind::Replace, "Ocarina ReFramed (Single Screen)",
                               "Ocarina ReFramed (Dual Screen)"};
        v.hover = {Id::DialogConfirm};
        ok &= shot("mods-conflict", mc, v);
        v.hover = {Id::ModLink, 0}; // the author's link, underlined under the beam
        ok &= shot("mods-link", mi, v);
        v.open_link = "https://github.com/OTPR26/OOT3DHud";
        v.hover = {Id::DialogConfirm};
        ok &= shot("mods-open-github", mi, v);
        // AzaharFrame modcat: Project Restoration's three builds as chips, the game's filled first
        MenuModel mv = mi;
        mv.mods.mods.available = {
            {"v110", "Project Restoration", "leoetlino", "https://github.com/leoetlino/project-restoration",
             false, OfferState::Install, "", false, "Updated Jan 2022", "Card 1.1",
             "For game cards with 1.1 built in.", true},
            {"v100", "Project Restoration", "leoetlino", "https://github.com/leoetlino/project-restoration",
             false, OfferState::Install, "", false, "Updated Jan 2022", "1.0",
             "For the original 1.0 game, or 1.0 with the 1.1 update.", false},
            {"v101", "Project Restoration", "leoetlino", "https://github.com/leoetlino/project-restoration",
             false, OfferState::Install, "", false, "Updated Jan 2022", "Fallback",
             "Try this if the first one doesn't work.", false}};
        v.open_link.clear();
        v.hover = {Id::ModVersion, 1};
        v.tooltip = Id::ModVersion;
        v.tooltip_index = 1;
        ok &= shot("mods-version-chips", mv, v);
        // AzaharFrame modcat2: the notes tooltip over a row's name (three sentences, wrapped)
        mv.mods.mods.available[1].notes =
            "For the original 1.0 game, or 1.0 with the 1.1 update. The Korean release is not supported. "
            "It conflicts with the other Project Restoration builds.";
        v.hover = {Id::ModNote, 1};
        v.tooltip = Id::ModNote;
        v.tooltip_index = 1;
        ok &= shot("mods-notes-tooltip", mv, v);
        v.tooltip = Id::None;
        v.tooltip_index = -1;
        v.open_link = "https://github.com/OTPR26/OOT3DHud";
        v.open_link.clear();
        MenuModel ml = m; // loaded, waiting for a restart
        ml.mods.mods.loaded = {
            {"oot3d-ocarina-reframed-single", "Ocarina ReFramed (Single Screen)",
             "OTPR26", "https://github.com/OTPR26/OOT3DHud", "v1.5.9", false, true, true},
            {"oot3d-ocarina-reframed-dual", "Ocarina ReFramed (Dual Screen)",
             "OTPR26", "https://github.com/OTPR26/OOT3DHud", "v1.5.9", false, true, false},
            {"unmanaged:romfs", "romfs", "", "", "214 Files", false, false, true}};
        ml.mods.mods.needs_restart = true;
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Mods;
        v.hover = {Id::ModRemove, 2};
        ok &= shot("mods-restart", ml, v);

        // AzaharFrame ui3: the Homebrew tab (a segment of the Mods tab until now), each state
        {
            MenuModel hb = m;
            hb.game_title = "Pokémon Emerald 3DS";
            MenuView hv;
            hv.tab = Tab::Homebrew;
            ok &= shot("homebrew-empty", hb, hv);
            HomebrewItem it;
            it.id = "emerald3ds";
            it.title = "Pokémon Emerald 3DS";
            it.author = "ZallaxDev";
            hb.homebrew.items = {it};
            hv.hover = {Id::HomebrewInstall, 0};
            ok &= shot("homebrew-install", hb, hv);
            hb.homebrew.items[0].busy = true;
            hb.homebrew.items[0].status = "Downloading";
            hb.homebrew.items[0].progress = 420;
            hv.hover = {};
            ok &= shot("homebrew-installing", hb, hv);
            hb.homebrew.items[0].progress = -1;
            hb.homebrew.items[0].status = "Building";
            hb.homebrew.spin = 7;
            ok &= shot("homebrew-building", hb, hv);
            hb.homebrew.items[0] = it;
            hb.homebrew.items[0].installed = true;
            hb.homebrew.items[0].author = "ZallaxDev \u00b7 English";
            hb.homebrew.items[0].version = "v0.5.0";
            hv.hover = {Id::HomebrewCheck, 0};
            ok &= shot("homebrew-installed", hb, hv);
            hb.homebrew.items[0].checking = true; // AzaharFrame ui3: Check for Updates' states
            hb.homebrew.spin = 7;
            hv.hover = {};
            ok &= shot("homebrew-checking", hb, hv);
            hb.homebrew.items[0].checking = false;
            hb.homebrew.items[0].checked = "Checked Just Now";
            ok &= shot("homebrew-no-updates", hb, hv);
            hv.hover = {Id::HomebrewReinstall, 0};
            ok &= shot("homebrew-reinstall", hb, hv);
            hv.hover = {};
            hb.homebrew.items[0].checked = "Checked 2 Hours Ago"; // failed: the row as it was
            ok &= shot("homebrew-check-failed", hb, hv);
            hb.homebrew.items[0].checked = "Checked Just Now";
            hb.homebrew.items[0].update = true;
            hb.homebrew.items[0].latest = "v0.6.0";
            hv.hover = {Id::HomebrewUpdate, 0};
            ok &= shot("homebrew-update", hb, hv);
            hb.homebrew.items[0].update = false;
            hb.homebrew.items[0].latest.clear();
            hb.homebrew.items[0].checked.clear();
            hb.homebrew.items[0].status = "Could not connect";
            hv.hover = {};
            ok &= shot("homebrew-error", hb, hv);
        }

        MenuModel tm = m;
        tm.mods.textures = SampleTextures();
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Textures;
        v.hover = {Id::TexturePack, 1};
        ok &= shot("textures", tm, v);
        { // AzaharFrame ui3: Optimize for Frame on a PNG pack's page, each state, and its snackbar
            MenuModel om = m;
            om.mods.textures = SampleTextures();
            TexturePartsModel& pp = om.mods.textures.parts;
            pp.pack_id = "oot3d-4k-png";
            pp.title = "4K (PNG)";
            pp.author = "Henriko Magnifico";
            pp.rest = "3.2 \u00b7 2,034 Textures On \u00b7 4.7 GB";
            pp.all_buttons = true;
            pp.rows = {{PartRowKind::Check, "Characters", "291 Textures \u00b7 263 MB", nullptr, 0, 2},
                       {PartRowKind::Check, "Effects", "52 Textures \u00b7 73 MB", nullptr, 0, 2},
                       {PartRowKind::Check, "Environment", "1,434 Textures \u00b7 4.2 GB", nullptr, 0, 2},
                       {PartRowKind::Check, "UI", "257 Textures \u00b7 160 MB", nullptr, 0, 0}};
            pp.optimize.state = OptimizeState::Png;
            pp.optimize.size_text = "4.7 GB";
            MenuView ov;
            ov.tab = mods_tab;
            ov.mods_segment = ModsSegment::Textures;
            ov.textures_page = TexturesPage::Parts;
            ov.hover = {Id::OptimizePack};
            ok &= shot("optimize-png", om, ov);
            om.mods.textures.dialog = {ModsDialogKind::Optimize, "2,308", "About 4.7 GB"};
            ov.hover = {Id::DialogConfirm};
            ok &= shot("optimize-confirm", om, ov);
            om.mods.textures.dialog = {};
            pp.optimize.state = OptimizeState::Converting;
            pp.optimize.done = 1234;
            pp.optimize.total = 2308;
            pp.optimize.permille = 534;
            ov.hover = {};
            ok &= shot("optimize-converting", om, ov);
            pp.optimize = {OptimizeState::Stopped, "4.7 GB"};
            ok &= shot("optimize-stopped", om, ov);
            pp.optimize = {OptimizeState::Failed, "4.7 GB", 0, 0, -1, {}, "Not Enough Space"};
            ok &= shot("optimize-failed", om, ov);
            pp.optimize = {OptimizeState::Optimized, "2.1 GB"};
            om.mods.textures.needs_restart = true;
            ok &= shot("optimize-optimized", om, ov);
            MenuModel nm = m;
            nm.mods.textures = SampleTextures();
            nm.mods.textures.packs[2].active = true;
            nm.mods.textures.packs[1].active = false;
            nm.mods.textures.packs[2].customize = true;
            nm.mods.textures.nudge_pack = "oot3d-4k-png";
            nm.mods.textures.nudge_name = "4K (PNG)";
            nm.mods.textures.needs_restart = true;
            MenuView nv;
            nv.tab = mods_tab;
            nv.mods_segment = ModsSegment::Textures;
            nv.hover = {Id::OptimizeNudge};
            ok &= shot("optimize-nudge", nm, nv);
        }
        MenuModel tr = tm;
        for (auto& p : tr.mods.textures.packs) {
            p.active = p.id == "oot3d-1080p";
        }
        tr.mods.textures.needs_restart = true;
        v.hover = {Id::RestartGame};
        ok &= shot("textures-restart", tr, v);
        v.hover = {};
        v.scroll[static_cast<int>(mods_tab)] = -1.0f;
        ok &= shot("textures-restart-end", tr, v);
        MenuModel te = m;
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Textures;
        ok &= shot("textures-empty", te, v);
        MenuModel to = tm;
        to.mods.textures.custom_textures = false;
        v.hover = {Id::CustomTextures};
        ok &= shot("textures-off", to, v);

        // Download Packs
        MenuModel dm = m;
        dm.game_title = "Pokemon X";
        dm.mods.textures = SamplePokemonDownloads();
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Textures;
        v.hover = {Id::DownloadPacks};
        ok &= shot("textures-empty-download", dm, v);
        v.textures_page = TexturesPage::Download;
        v.hover = {Id::PackDownload, 0};
        ok &= shot("download-list", dm, v);
        MenuModel dh = dm; // AzaharFrame zipinstall: dates and the verified badge
        dh.mods.textures = SampleOot3dDownloads();
        v.hover = {Id::PackLink, 0};
        ok &= shot("download-list-dates", dh, v);
        MenuModel db = dm;
        db.mods.textures.dialog = {ModsDialogKind::BigDownload, "Pokemon X and Y HD", "1.8 GB"};
        v.hover = {Id::DialogConfirm};
        ok &= shot("download-confirm", db, v);
        MenuModel dd = dm;
        dd.mods.textures.downloading = true;
        dd.mods.textures.downloads[0].state = PackOfferState::Downloading;
        dd.mods.textures.downloads[0].percent = 42;
        dd.mods.textures.downloads[0].note = "Downloading";
        v.hover = {Id::PackCancel, 0};
        ok &= shot("downloading", dd, v);
        MenuModel df = dm;
        df.mods.textures.downloads[0].state = PackOfferState::Failed;
        df.mods.textures.downloads[0].note = "Not enough free space: 4.6 GB needed, 1.1 GB free";
        v.hover = {Id::PackDownload, 0};
        ok &= shot("failed", df, v);
        v.open_link = "https://github.com/Gray-Rice/PokeTex-3DS";
        v.hover = {Id::DialogConfirm};
        ok &= shot("download-open-link", dm, v);
        v.open_link.clear();
        MenuModel di = dm;
        di.mods.textures.downloads[0].state = PackOfferState::Installed;
        v.hover = {};
        ok &= shot("download-installed", di, v);
        MenuModel dp = di; // the downloaded pack in the list, with Delete
        dp.mods.textures.packs = {{"/load/texture-packs/0004000000055D00/PokeTex-3DS",
                                   "Pokemon X and Y HD", "Ullr8, Gray-Rice · PNG · 1.8 GB",
                                   false, true}};
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Textures;
        v.hover = {Id::PackDelete, 0};
        ok &= shot("textures-downloaded", dp, v);
        dp.mods.textures.dialog = {ModsDialogKind::DeletePack, "Pokemon X and Y HD", ""};
        v.hover = {Id::DialogConfirm};
        ok &= shot("textures-delete", dp, v);
        // AzaharFrame zipinstall: Install from Zip; AzaharFrame ui2: on Download Packs
        v.textures_page = TexturesPage::Download;
        v.hover = {Id::InstallFromZip, 0};
        dp.mods.textures.dialog = {};
        ok &= shot("zip-textures-button", dm, v);
        MenuModel zm = dp;
        ZipModel& z = zm.mods.zip;
        z.open = true;
        z.target = ZipTarget::Textures;
        z.browse = true;
        z.folder = "/home/user/Downloads";
        z.folder_text = "~/Downloads";
        z.can_go_up = true;
        z.chips = {{"Downloads", "/home/user/Downloads"},
                   {"Documents", "/home/user/Documents"},
                   {"Home", "/home/user"}};
        z.entries = {{"3DS Packs", true, ""},
                     {"Screenshots", true, ""},
                     {"Henriko Magnifico Ocarina of Time 3D 4K Texture Pack v6.0.0.zip", false, "2.1 GB"},
                     {"No Bloom.7z", false, "12 KB"},
                     {"PokeTex-3DS.zip", false, "1.8 GB"}};
        v.hover = {Id::ZipEntry, 2};
        ok &= shot("zip-browse", zm, v);
        MenuModel ze = zm;
        ze.mods.zip.file = "photos.zip";
        ze.mods.zip.error = "Not a Texture Pack or Mod";
        v.hover = {Id::ZipChip, 1};
        ok &= shot("zip-error", ze, v);
        MenuModel zw = zm;
        zw.mods.zip.working = true;
        zw.mods.zip.phase = "Unpacking";
        zw.mods.zip.file = "PokeTex-3DS.zip";
        v.hover = {};
        ok &= shot("zip-working", zw, v);
        MenuModel zc = zm;
        zc.mods.zip.choices = {"0004000000033500", "0004000000033600"};
        v.hover = {Id::ZipChoice, 0};
        ok &= shot("zip-choose", zc, v);
        MenuModel zr = zm;
        zr.mods.zip.dialog = {ModsDialogKind::Replace, "PokeTex-3DS", "Pokemon X and Y HD"};
        v.hover = {Id::DialogConfirm};
        ok &= shot("zip-replace", zr, v);
        MenuModel zf = dm;
        zf.mods.zip = z;
        zf.mods.zip.browse = false;
        zf.mods.zip.file = "photos.zip";
        zf.mods.zip.error = "Not a Texture Pack or Mod";
        v.hover = {};
        ok &= shot("zip-flat-error", zf, v);
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Textures;
        v.textures_page = TexturesPage::Download;
        v.hover = {Id::InstallFromZip, 0};
        MenuModel dz = dm;
        dz.mods.textures.downloads.clear();
        ok &= shot("zip-textures-empty", dz, v);
    }
    // AzaharFrame compat: entries for another version of the game, with their reasons; the catalog
    // rows' "Created by" and bulleted lines, sorted by downloads; This May Not Work; Turned Off.
    {
        const Tab mods_tab = Tab::Mods;
        // Majora's Mask 3D, Europe, code matching Card 1.1: the game's own first (most downloaded
        // first), then the other builds and the USA-only mods.
        const auto offer = [](const char* id, const char* name, const char* author, const char* updated,
                              const char* downloads) {
            ModOffer o;
            o.id = id;
            o.name = name;
            o.author = author;
            o.url = "https://gamebanana.com/members/1";
            o.updated = updated;
            o.downloads = downloads;
            return o;
        };
        MenuModel cm = m;
        cm.game_title = "The Legend of Zelda: Majora's Mask 3D";
        ModsListModel& mm = cm.mods.mods;
        mm.catalog = CatalogState::Ready;
        ModOffer pr110 = offer("mm3d-project-restoration-v110", "Project Restoration", "leoetlino",
                               "Updated Jan 2022", "50K+ Downloads");
        pr110.version = "Card 1.1";
        pr110.version_tip = "For game cards with 1.1 built in.";
        pr110.version_match = true;
        ModOffer inn = offer("mm3d-dirtier-stock-pot-inn", "Dirtier Stock Pot Inn", "Takeshre",
                             "Updated Mar 2026", "1K+ Downloads");
        ModOffer pr100 = offer("mm3d-project-restoration-v100", "Project Restoration", "leoetlino",
                               "Updated Jan 2022", "10K+ Downloads");
        pr100.version = "1.0";
        pr100.version_tip = "For the original 1.0 game, or 1.0 with the 1.1 update.";
        pr100.mismatch = true;
        pr100.mismatch_reason = "Made for another build of the game. Yours matches Card 1.1. It may not work.";
        pr100.mismatch_body = "This mod was made for another build of the game, and yours matches Card 1.1. "
                              "It may not work, or the game may not start.";
        ModOffer lillie = offer("mm3d-legend-of-lillie", "The Legend of Lillie", "Ellie", "", "100+ Downloads");
        lillie.mismatch = true;
        lillie.region_chip = "USA Only";
        lillie.mismatch_reason = "Made for the USA version. Yours is the European version. It may not work.";
        lillie.mismatch_body = "This mod was made for the USA version of the game, and yours is the European "
                               "version. It may not work, or the game may not start.";
        lillie.notes = "Replaces Link and his four masked forms with Lillie from Pokemon Sun and Moon.";
        mm.available = {pr110, inn, pr100, lillie};
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Mods;
        v.mods_page = ModsPage::Install;
        ok &= shot("compat-mods", cm, v);
        v.scroll[static_cast<int>(mods_tab)] = -1.0f; // the last rows
        v.hover = {Id::ModCompat, 3};
        v.tooltip = Id::ModCompat;
        v.tooltip_index = 3;
        ok &= shot("compat-mods-chip-tooltip", cm, v);
        v.hover = {Id::ModNote, 3};
        v.tooltip = Id::ModNote;
        v.tooltip_index = 3;
        ok &= shot("compat-mods-row-tooltip", cm, v);
        v.scroll[static_cast<int>(mods_tab)] = 0.0f;
        v.hover = {Id::ModNote, 2}; // a row's notes stay clear of every chip, its own and others'
        v.tooltip_index = 2;
        cm.mods.mods.available[2].notes =
            "For the original 1.0 game, or 1.0 with the 1.1 update. The Korean release is not supported. "
            "It conflicts with the other Project Restoration builds.";
        ok &= shot("compat-mods-notes-tooltip", cm, v);
        v.tooltip = Id::None;
        v.tooltip_index = -1;
        v.scroll[static_cast<int>(mods_tab)] = -1.0f;
        v.confirm = {Id::ModInstall, 3, "mm3d-legend-of-lillie"};
        v.hover = {Id::DialogConfirm};
        ok &= shot("compat-mods-dialog", cm, v);

        // Download Packs: Ocarina of Time 3D, Japan: the packs for every region first, sorted
        TexturesModel& t = cm.mods.textures;
        t.custom_textures = true;
        PackOffer p4k{"oot3d-4k-henriko", "Ocarina of Time 3D 4K", "Henriko Magnifico", "", "", PackOfferState::Download,
                      -1, "", "Updated Sep 2026", true};
        p4k.downloads = "50K+ Downloads";
        PackOffer p1080{"oot3d-1080p-henriko", "Ocarina of Time 3D 1080p", "Henriko Magnifico", "", "",
                        PackOfferState::Download, -1, "", "Updated Sep 2026", true};
        p1080.downloads = "5K+ Downloads";
        PackOffer touch{"oot3d-touchless-layout-vanilla", "Touchless Layout", "Linkus", "", "",
                        PackOfferState::Download, -1, "", "", false};
        touch.downloads = "Under 100 Downloads";
        PackOffer npc{"oot3d-1998-npc-overhaul", "1998 NPC Overhaul", "Hylian Modding", "", "",
                      PackOfferState::Download, -1, "", "Updated Mar 2026", false};
        npc.mismatch = true;
        npc.region_chip = "USA and Europe";
        npc.mismatch_reason = "Made for the USA and European versions. Yours is the Japanese version. It may not work.";
        npc.mismatch_body = "This texture pack was made for the USA and European versions of the game, and yours "
                            "is the Japanese version. It may not work, or the game may not start.";
        PackOffer skin{"oot3d-link-skin-pack", "Link Skin Pack", "Nintendo Fan", "", "", PackOfferState::Download,
                       -1, "", "", false};
        skin.mismatch = true;
        skin.region_chip = "USA Only";
        skin.mismatch_reason = "Made for the USA version. Yours is the Japanese version. It may not work.";
        t.downloads = {p4k, p1080, touch, npc, skin};
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Textures;
        v.textures_page = TexturesPage::Download;
        ok &= shot("compat-packs-sorted", cm, v);
        v.scroll[static_cast<int>(mods_tab)] = -1.0f;
        v.hover = {Id::PackCompat, 3};
        v.tooltip = Id::PackCompat;
        v.tooltip_index = 3;
        ok &= shot("compat-packs-chip-tooltip", cm, v);
        v.tooltip = Id::None;
        v.tooltip_index = -1;
        v.confirm = {Id::PackDownload, 3, "oot3d-1998-npc-overhaul"};
        v.hover = {Id::DialogConfirm};
        ok &= shot("compat-packs-dialog", cm, v);
        // GameBanana has not scanned the file: the question after the refusal (the scan alone) ...
        v.confirm = {};
        t.dialog = {ModsDialogKind::Unscanned, t.downloads[3].name, "Download Anyway"};
        ok &= shot("compat-unscanned-dialog", cm, v);
        t.dialog = {};
        // ... and, known before the click, one dialog with both reasons, the version first
        t.downloads[3].unscanned = true;
        v.confirm = {Id::PackDownload, 3, "oot3d-1998-npc-overhaul"};
        ok &= shot("compat-unscanned-version-dialog", cm, v);
        t.downloads[3].unscanned = false;

        // A cheat for 1.1 on a 1.0 game
        MenuModel ch = m;
        ch.mods.cheats = SampleCheats();
        ch.mods.cheats.cheats[2].version_note = "for v1.1, installed v1.0";
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Cheats;
        v.confirm = {Id::Cheat, 2, ch.mods.cheats.cheats[2].name};
        v.hover = {Id::DialogConfirm};
        ok &= shot("compat-cheat-dialog", ch, v);

        // Turned off before this start: the snackbar on the Mods segment (one, then two names)
        MenuModel to = m;
        to.mods.mods.loaded = {
            {"oot3d-ocarina-reframed-single", "Ocarina ReFramed (Single Screen)", "OTPR26",
             "https://github.com/OTPR26/OOT3DHud", "v1.5.9", false, true, false},
            {"unmanaged:romfs", "romfs", "", "", "214 Files", false, false, true}};
        to.mods.mods.turned_off = {"Ocarina ReFramed (Single Screen)"};
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Mods;
        v.hover = {Id::TurnBackOn};
        ok &= shot("compat-turned-off", to, v);
        to.mods.mods.turned_off = {"Ocarina ReFramed (Single Screen)", "Ocarina of Time 3D 4K"};
        v.hover = {};
        ok &= shot("compat-turned-off-two", to, v);
    }
    v = {};
    v.tab = Tab::Main;
    ok &= shot("main", m, v);
    return ok ? 0 : 1;
}
