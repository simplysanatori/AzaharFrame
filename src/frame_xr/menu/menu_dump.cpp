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

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <lodepng.h>

#include "frame_xr/input/controls.h"
#include "frame_xr/input/key_names.h" // AzaharFrame keys
#include "frame_xr/input/mouse.h"   // AzaharFrame mouse: its one-time toast
#include "frame_xr/input/reticle.h" // AzaharFrame mouse
#include "frame_xr/mem_watch.h"     // AzaharFrame memwarn
#include "frame_xr/menu/edit_devices.h"
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/md3_fonts.h" // AzaharFrame useglobal: the option B mock's icon
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/menu/texparts_dump.h" // AzaharFrame texparts
#include "frame_xr/toast_fit.h"          // AzaharFrame menuux
#include "frame_xr/immersive_names.h"    // AzaharFrame integ8
#include "frame_xr/vis/help_content.h"
#include "frame_xr/vis/help_layout.h"
#include "frame_xr/vis/menu_diagram.h"
#include "frame_xr/vis/model_file.h"
#include "video_core/custom_textures/preload_budget.h" // AzaharFrame memguard

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
    m.scale_x = 0;
    m.slots = {{1, "2026-11-08 21:14"}, {2, "2026-11-09 19:02"}, {5, "2026-11-10 08:47"}};
    m.world_scale_stop = 20;
    m.follow_screen = true;
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
    std::string autosave_thumbs; // AzaharFrame autosave: real top screens for the card
    std::string only; // AzaharFrame useglobal: --only PREFIX: only the shots named so
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
        } else if (a == "--autosave-thumbs") {
            autosave_thumbs = argv[i + 1];
        } else if (a == "--only") {
            only = argv[i + 1];
        }
        if (a == "--models" || a == "--px-per-dp" || a == "--texparts" ||
            a == "--autosave-thumbs" || a == "--only") {
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
                          const MouseOverlay& mouse = {},
                          const std::function<void(Kit&)>& over = {}) {
        if (!only.empty() && name.rfind(only, 0) != 0) {
            return true; // AzaharFrame useglobal: --only
        }
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
        if (over) {
            over(kit); // AzaharFrame useglobal: a mock drawn over the menu (another design)
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
                          VideoCore::PreloadBudget::kTooLargeToast, // AzaharFrame memguard
                          VideoCore::PreloadBudget::kStoppedToast, "Stopping...",
                          "C-Stick & Gyro Downloaded. Restart to Enable."}); // AzaharFrame ui3: five seconds
        // AzaharFrame ui3: Check for Updates' results (five seconds)
        ok &= toast_shot("homebrew-no-updates-toast.png", {"No Updates Available"});
        ok &= toast_shot("homebrew-update-available-toast.png", {"Update Available"});
        ok &= toast_shot("homebrew-check-failed-toast.png", {"Couldn't Check for Updates"});
        // AzaharFrame natural: the grip's World Scale, a resize with Follow Screen Size on, the
        // 3D Factor shortcuts in the Full modes (the stereo cap's "Limited to" toast is gone)
        ok &= toast_shot("natural-toasts.png",
                         {Immersive::Names::WorldScaleToast(45.0f),
                          Immersive::Names::WorldScaleToast(100.0f),
                          Immersive::Names::kWorldScaleFollowsToast,
                          Immersive::Names::DepthToast(45)});
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
    { // AzaharFrame autosave: Load State's Autosave card (owner's Option B)
        std::vector<std::shared_ptr<const AutosavePicture>> pics;
        std::error_code ec;
        std::vector<std::string> files;
        for (const auto& e : std::filesystem::directory_iterator(autosave_thumbs, ec)) {
            if (e.path().extension() == ".png") {
                files.push_back(e.path().string());
            }
        }
        std::sort(files.rbegin(), files.rend()); // newest first (the names are their times)
        for (const std::string& f : files) {
            std::vector<unsigned char> rgba;
            unsigned pw = 0, ph = 0;
            if (lodepng::decode(rgba, pw, ph, f) == 0 && pw > 0) {
                auto p = std::make_shared<AutosavePicture>();
                p->w = static_cast<int>(pw);
                p->h = static_cast<int>(ph);
                p->rgba = std::move(rgba);
                pics.push_back(std::move(p));
            }
        }
        const auto pic = [&pics](std::size_t i) {
            return pics.empty() ? nullptr : pics[i % pics.size()];
        };
        MenuModel am = m;
        am.autosave = true;
        am.autosave_keep = 3;
        am.autosaves = {{"Just Now", "2026-11-12 10:26", "10:26", false, pic(0)},
                        {"4 Minutes Ago", "2026-11-12 10:22", "10:22", false, pic(1)},
                        {"9 Minutes Ago", "2026-11-12 10:17", "10:17", false, pic(2)}};
        MenuView av;
        av.page = Page::LoadSlots;
        av.hover = {Id::AutosaveCard};
        ok &= shot("1i-load-autosave-card", am, av);
        av.open_menu = Id::AutosaveCard;
        av.hover = {Id::AutosaveEntry, 1};
        ok &= shot("1j-load-autosave-list", am, av);
        MenuModel ten = am; // Keep 10: two columns
        ten.autosave_keep = 10;
        const char* agos[] = {"Just Now",       "4 Minutes Ago",  "9 Minutes Ago",
                              "14 Minutes Ago", "19 Minutes Ago", "24 Minutes Ago",
                              "29 Minutes Ago", "34 Minutes Ago", "39 Minutes Ago",
                              "1 Hour Ago"};
        ten.autosaves.clear();
        for (int i = 0; i < 10; i++) {
            const int minute = 26 - i * 5;
            const int h = minute < 0 ? 9 : 10, mm = minute < 0 ? minute + 60 : minute;
            char clock[8];
            std::snprintf(clock, sizeof(clock), "%02d:%02d", h, mm);
            ten.autosaves.push_back({agos[i], std::string("2026-11-12 ") + clock, clock, false,
                                     pic(static_cast<std::size_t>(i))});
        }
        av.hover = {Id::AutosaveEntry, 7};
        ok &= shot("1j2-load-autosave-list-keep10", ten, av);
        // AzaharFrame advfix: the menu-open autosave in the list as the newest, beyond Keep
        MenuModel mo = am;
        mo.autosaves.insert(mo.autosaves.begin(),
                            {"Just Now", "2026-11-12 10:31", "10:31", false, pic(3)});
        mo.autosaves[1].ago = "5 Minutes Ago";
        av.hover = {Id::AutosaveEntry, 3};
        ok &= shot("1j3-load-autosave-list-menu-open", mo, av);
        MenuModel ten_mo = ten;
        ten_mo.autosaves.insert(ten_mo.autosaves.begin(),
                                {"Just Now", "2026-11-12 10:31", "10:31", false, pic(5)});
        av.hover = {Id::AutosaveEntry, 10};
        ok &= shot("1j4-load-autosave-list-keep10-menu-open", ten_mo, av);
        av.open_menu = Id::None;
        av.hover = {};
        MenuModel one = am; // Keep 1: no arrow; tapping the card loads it
        one.autosave_keep = 1;
        one.autosaves.resize(1);
        ok &= shot("1k-load-autosave-keep1", one, av);
        MenuModel aoff = m;
        aoff.autosave = false;
        av.hover = {Id::AutosaveSettings};
        ok &= shot("1l-load-autosave-off", aoff, av);
        MenuModel none = am; // on, none made yet
        none.autosaves.clear();
        av.hover = {};
        ok &= shot("1m-load-autosave-none-yet", none, av);
        MenuModel undo = am; // after an autosave was loaded
        undo.undo_load = "Autosave Loaded: 10:22";
        av.hover = {Id::UndoLoad};
        ok &= shot("1n-load-autosave-undo", undo, av);
        undo.undo_load = "Slot 3 Loaded: 10:22"; // AzaharFrame advfix: Undo Load for slots
        ok &= shot("1n2-load-slot-undo", undo, av);
        undo.undo_load = "Quick Save Loaded: 10:22";
        ok &= shot("1n3-quick-load-undo", undo, av);
        MenuModel other = am; // the newest from another build
        other.autosaves[0].other = true;
        other.autosaves[0].time = other.autosaves[0].clock = "From 1.1.1";
        av.hover = {};
        ok &= shot("1o-load-autosave-other-build", other, av);
        // The "Autosaved" label as hud.cpp draws it into the toast atlas (480 x 96 px), and the
        // toast it falls back to
        frame_xr::ui::Canvas lc;
        lc.Init(480, 96);
        lc.Clear(Rgb(kBackdrop));
        Kit lk(1.0f);
        lk.Begin(lc);
        DrawAutosavedLabel(lk, {2.0f, 2.0f, 478.0f, 94.0f});
        ok &= WritePng(out + "/autosaved-label.png", lc);
        ok &= toast_shot("autosaved-toast.png", {"Autosaved"});
        // AzaharFrame advfix: the Per-Game scope's toasts and the failures'
        ok &= toast_shot("advfix-toasts.png",
                         {"Autosave On for Ocarina of Time 3D",
                          "Autosave Off for The Legend of Zelda: Maj...", "Couldn't Autosave",
                          "Couldn't Autosave: Not Enough Space",
                          "Couldn't Autosave: Couldn't Write the File"});
        // AzaharFrame utf8: titles and text with accents and typographic punctuation
        // AzaharFrame fovonce: Detect Game FOV's two toasts (the HUD draws "°" as a ring; the
        // middle dot is the font's)
        ok &= toast_shot("fo-toasts.png", {"Game FOV Detected: 40", "Couldn't Detect Game FOV \u00b7 Kept 50"});
        ok &= toast_shot("utf8-toasts.png",
                         {"Autosave Off for Pokémon X", "Autosave On for Pokémon Y",
                          "Super Mario 3D Land – München ñandú",
                          "Kid Icarus’s “Uprising” — Łódź…"});
    }
    { // AzaharFrame savestate: slots from another build, the mismatch and About notices
        MenuModel sm = m;
        sm.slots = {{1, "From 1.1.1", true}, {2, "2026-11-09 19:02"}, {5, "From Another Version", true}};
        MenuView sv;
        sv.page = Page::LoadSlots;
        sv.hover = {Id::Slot, 1};
        ok &= shot("1c-load-slots-other-build", sm, sv);
        sv.hover = {};
        sm.notice.kind = ModsDialogKind::Notice;
        sm.notice.mod = "This Save State Is From an Older Version";
        sm.notice.other = "It was made with Azahar for Frame 1.1.1. It may not load, and it could "
                          "corrupt this game's progress. Save states are NOT a replacement for "
                          "saving in the game.";
        sm.notice.confirm = "Load Anyway";
        sm.notice.cancel = "Cancel";
        ok &= shot("1d-load-anyway-dialog", sm, sv);
        sm.notice.mod = "This Save State Is From Another Version";
        sm.notice.other = "It was made with another version of Azahar. It may not load, and it "
                          "could corrupt this game's progress. Save states are NOT a replacement "
                          "for saving in the game.";
        ok &= shot("1e-load-anyway-dialog-other", sm, sv);
        sm.notice.mod = "About Save States";
        sm.notice.other = "Save states are NOT a replacement for saving in the game. They only load "
                          "in the version of Azahar for Frame that made them, so after an update "
                          "older ones may not load. Save in the game before you update.";
        sm.notice.confirm = "Got It";
        sm.notice.cancel.clear();
        sv.page = Page::SaveSlots;
        ok &= shot("1f-about-save-states", sm, sv);
        // AzaharFrame advfix: the backup's time, and the current save data backed up first
        sm.notice.mod = "Restore Save Data From 10:26?";
        sm.notice.other = "A save state from another version was loaded last time, and the game "
                          "didn't close normally. Restore puts back this game's save data from "
                          "before that load. Your current save data is backed up first.";
        sm.notice.confirm = "Restore";
        sm.notice.cancel = "Keep Current";
        sv.page = Page::Tabs;
        ok &= shot("1g-restore-save-data", sm, sv);
        sm.notice.mod = "This Save State Couldn't Be Loaded";
        sm.notice.other = "The game had to stop. Its save data wasn't changed.";
        sm.notice.confirm = "OK";
        sm.notice.cancel.clear();
        ok &= shot("1h-save-state-not-loaded", sm, sv);
        // AzaharFrame advfix: a load that failed before restoring (a same-version state cut short)
        sm.notice.other = "The game and its save data weren't changed.";
        sv.page = Page::LoadSlots;
        ok &= shot("1h2-save-state-not-loaded-unchanged", sm, sv);
        // AzaharFrame advfix: a save that failed, with its reason, and with none
        sm.notice.mod = "Couldn't Save State";
        sm.notice.other = "Not Enough Space\nThe game is still running. Try another slot, or "
                          "free up space on the Frame.";
        sv.page = Page::SaveSlots;
        ok &= shot("1h3-save-state-not-saved-no-space", sm, sv);
        sm.notice.other = "The game is still running. Try another slot, or free up space on the "
                          "Frame.";
        ok &= shot("1h4-save-state-not-saved", sm, sv);
    }
    { // AzaharFrame memwarn: Level 1's toast and Level 2's dialog (custom textures on, then off)
        namespace MW = FrameXr::MemWatch;
        constexpr std::uint64_t kMiB = 1024 * 1024;
        const std::uint64_t max = 8396 * kMiB; // 8.2 GB
        ok &= toast_shot("memwarn-toast.png", {MW::WarnText(7270 * kMiB, max)});
        MenuModel mm = m;
        MenuView mv;
        mv.page = Page::Tabs;
        const auto dialog = [&](const std::string& name, bool textures, Id hover) {
            const MW::PauseText t = MW::MakePauseText(7782 * kMiB, max, textures);
            mm.notice = {};
            mm.notice.kind = ModsDialogKind::Notice;
            mm.notice.mod = t.title;
            mm.notice.other = t.body;
            mm.notice.confirm = textures ? t.textures : t.resume;
            mm.notice.cancel = textures ? t.resume : std::string{};
            mm.notice.extra = t.stop;
            mv.hover = {hover};
            return shot(name, mm, mv);
        };
        ok &= dialog("memwarn-dialog", true, Id::None);
        ok &= dialog("memwarn-dialog-hover-textures", true, Id::DialogConfirm);
        ok &= dialog("memwarn-dialog-hover-stop", true, Id::DialogExtra);
        ok &= dialog("memwarn-dialog-no-textures", false, Id::None);
    }

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
    { // AzaharFrame fixes7: Screen Shape: Automatic chosen, its menu open, and the tooltip
        MenuModel ma = m;
        ma.screen_shape = 6;
        v.hover = {Id::ScreenShape, 6};
        ok &= shot("2d-position-shape-automatic-menu", ma, v);
        v.open_menu = Id::None;
        v.hover = {Id::ScreenShape, kLabelIndex};
        v.tooltip = Id::ScreenShape;
        ok &= shot("2e-position-shape-automatic-tooltip", ma, v);
        v.tooltip = Id::None;
    }

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
        // AzaharFrame reticle: Reticle Speed held at 3.0x, and its tooltip
        v.hover = {};
        v.dragging = Id::ReticleSpeed;
        v.drag_t = (30.0f - 5.0f) / 35.0f;
        ok &= shot("4d-controls-reticle-speed-drag", mr, v);
        v.dragging = Id::None;
        v.hover = {Id::ReticleSpeed, kLabelIndex};
        v.tooltip = Id::ReticleSpeed;
        ok &= shot("4e-controls-reticle-speed-tooltip", mr, v);
        // AzaharFrame fixes7: Aim Only While Pressed on, Right Stick Moves Touch off; each
        // tooltip. (AzaharFrame kiugyro: C-Stick at Edge has no row now.)
        mr.reticle_center = true;
        v = {};
        v.tab = Tab::Controls;
        ok &= shot("4f-controls-reticle-switches", mr, v);
        v.hover = {Id::ReticleCenter, -1};
        v.tooltip = Id::ReticleCenter;
        ok &= shot("4g-controls-aim-only-while-pressed-tooltip", mr, v);
        v.hover = {Id::ReticleStick, -1};
        v.tooltip = Id::ReticleStick;
        ok &= shot("4h-controls-right-stick-moves-touch-tooltip", mr, v);
        MenuModel mk = mr; // the Right Controller, as the owner plays Kid Icarus: Uprising
        mk.bottom_screen_reticle = 2;
        mk.reticle_center = false;
        mk.reticle_stick = true;
        v = {};
        v.tab = Tab::Controls;
        ok &= shot("4i-controls-reticle-right-stick-on", mk, v);
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

        // AzaharFrame keys: a keyboard, its default (Azahar's desktop keys, gamepads.cpp's
        // DefaultKeyboardMapping), then listening, then a WASD Circle Pad.
        namespace K = Input::Key;
        EditDevice kb;
        kb.key = "keyboard";
        kb.name = "Keyboard";
        kb.kind = DeviceKind::Keyboard;
        kb.names = ButtonNames::Keyboard;
        kb.sticks = {-1, -1};
        const std::array<std::pair<int, int>, 15> kd{{{DsButton::A, K::A},
                                                      {DsButton::B, K::S},
                                                      {DsButton::X, K::Z},
                                                      {DsButton::Y, K::X},
                                                      {DsButton::Up, K::T},
                                                      {DsButton::Down, K::G},
                                                      {DsButton::Left, K::F},
                                                      {DsButton::Right, K::H},
                                                      {DsButton::L, K::Q},
                                                      {DsButton::R, K::W},
                                                      {DsButton::Start, K::M},
                                                      {DsButton::Select, K::N},
                                                      {DsButton::ZL, K::Num1},
                                                      {DsButton::ZR, K::Num2},
                                                      {DsButton::Home, K::B}}};
        for (const auto& [b, k] : kd) {
            kb.mapping[static_cast<std::size_t>(b)] = static_cast<int8_t>(k);
        }
        kb.extras = {-1, K::Esc};
        kb.keys = {K::Up, K::Down, K::Left, K::Right, K::I, K::K, K::J, K::L, K::D};
        MenuModel mk = me;
        mk.devices.list = {frame, ds4, mouse, kb};
        mk.devices.selected = 3;
        mk.devices.pointer = {};
        v = {};
        v.tab = Tab::Controls;
        v.remap.editing = true;
        ok &= shot("edit-3ds-keyboard", mk, v);
        v.remap.listening = DsButton::CirclePad;
        v.remap.step = 1;
        v.hover = {Id::RemapCancel};
        ok &= shot("edit-3ds-keyboard-stick", mk, v);
        v.remap.step = 0;
        v.remap.listening = DsButton::Modifier;
        ok &= shot("edit-3ds-keyboard-listening", mk, v);
        v.remap.listening = -1;
        v.hover = {};
        mk.devices.list[3].keys = {K::W, K::S, K::A, K::D, K::I, K::K, K::J, K::L, K::LeftShift};
        auto& custom = mk.devices.list[3].mapping;
        custom[DsButton::A] = K::Space;
        custom[DsButton::B] = K::Enter;
        custom[DsButton::Up] = K::Up;
        custom[DsButton::Down] = K::Down;
        custom[DsButton::Left] = K::Left;
        custom[DsButton::Right] = K::Right;
        ok &= shot("edit-3ds-keyboard-custom", mk, v);
    }

    // AzaharFrame autosave: Settings in three sections (General · Graphics · Immersive), the scope
    // menu button at the section row's end, Per-Game as the menu opens; the Saves group at
    // General's end, its rows per game.
    const auto settings = [](SettingsSection section) {
        MenuView sv;
        sv.tab = Tab::Settings;
        sv.section = section;
        return sv;
    };
    MenuModel pg = m; // Per-Game: as the menu opens while a game runs
    pg.scope = 1;
    pg.autosave = true;
    pg.autosave_every = 2;
    pg.autosave_keep = 3;
    v = settings(SettingsSection::General);
    v.hover = {Id::ShowControllers};
    ok &= shot("5-settings", pg, v);
    {
        // AzaharFrame ctrlfront: Controllers in Front of Screen under Show Controllers, its
        // Primary choice's tooltip; then Show Controllers off, the row gone.
        MenuView cv = settings(SettingsSection::General);
        cv.hover = {Id::ControllersInFront, 1};
        ok &= shot("5c-settings-controllers-in-front", pg, cv);
        MenuModel off = pg;
        off.show_controllers = false;
        cv.hover = {Id::ShowControllers};
        ok &= shot("5d-settings-controllers-off", off, cv);
    }
    v.hover = {Id::AutosaveKeep};
    v.scroll[static_cast<int>(Tab::Settings)] = -1.0f;
    ok &= shot("5a-settings-general-end-saves", pg, v);
    {
        // AzaharFrame kiu: Motion Controls is per game too: Kid Icarus: Uprising's Off, in
        // Per-Game, then greyed in Global with its tooltip.
        MenuModel mo = pg;
        mo.motion_controls = 0;
        mo.game_value[static_cast<std::size_t>(Id::MotionControls)] = true;
        MenuView mv = settings(SettingsSection::General);
        ok &= shot("5m-settings-motion-controls-per-game", mo, mv);
        mo.scope = 0;
        mo.locked[static_cast<std::size_t>(Id::MotionControls)] = Lock::GameValue;
        mv.hover = {Id::MotionControls, kLabelIndex};
        mv.tooltip = Id::MotionControls;
        ok &= shot("5n-settings-motion-controls-global-locked", mo, mv);
    }
    {
        // AzaharFrame kiugyro: Aiming: Direct Aim (stored 2; Kid Icarus: Uprising's preset with
        // the C-Stick & Gyro mod) shows Turn Zone and Turn Speed under Gyro Sensitivity; then
        // Gyro Aim (stored 1), with Aiming's tooltip.
        MenuModel go = pg;
        go.motion_controls = 1;
        go.gyro_aim = 2;
        go.gyro_sensitivity = 10;
        go.turn_zone = 10;
        go.turn_speed = 10;
        go.game_value[static_cast<std::size_t>(Id::GyroAim)] = true;
        go.game_value[static_cast<std::size_t>(Id::MotionControls)] = true;
        MenuView gv = settings(SettingsSection::General);
        gv.hover = {Id::TurnZone};
        ok &= shot("5g-settings-aiming-direct-aim", go, gv);
        go.gyro_aim = 1;
        go.gyro_sensitivity = 15;
        gv.hover = {Id::GyroAim, kLabelIndex};
        gv.tooltip = Id::GyroAim;
        ok &= shot("5h-settings-aiming-gyro-aim-tooltip", go, gv);
        // AzaharFrame kiuguns: Direct Aim with Recoil off, Weapon in Hand on and Laser Sight on,
        // Laser Sight's tooltip shown.
        go.gyro_aim = 2;
        go.gyro_sensitivity = 10;
        go.recoil = false;
        go.weapon_in_hand = true;
        go.laser_sight = true;
        go.game_value[static_cast<std::size_t>(Id::Recoil)] = true;
        go.game_value[static_cast<std::size_t>(Id::LaserSight)] = true;
        gv.hover = {Id::LaserSight, kLabelIndex};
        gv.tooltip = Id::LaserSight;
        ok &= shot("5h2-settings-aiming-laser-sight-recoil", go, gv);
        gv.hover = {Id::WeaponInHand, kLabelIndex};
        gv.tooltip = Id::WeaponInHand;
        ok &= shot("5h3-settings-aiming-weapon-in-hand-tooltip", go, gv);
        // AzaharFrame kiuflash: Muzzle Flash under Weapon in Hand, off for this game, its tooltip.
        go.muzzle_flash = false;
        go.game_value[static_cast<std::size_t>(Id::MuzzleFlash)] = true;
        gv.hover = {Id::MuzzleFlash, kLabelIndex};
        gv.tooltip = Id::MuzzleFlash;
        {
            // Scrolled down to Muzzle Flash (low in General).
            frame_xr::ui::Canvas scratch;
            scratch.Init(static_cast<int>(kPanelW * px_per_dp + 0.5f),
                         static_cast<int>(kPanelH * px_per_dp + 0.5f));
            const auto on_view = [&](Id id) {
                for (const auto& hit : menu.Hits()) {
                    if (hit.t.id == id && hit.t.index == -1) {
                        return true; // a row the list's edge cuts is a target too
                    }
                }
                return false;
            };
            for (float s = 0.0f; s <= 3000.0f; s += 20.0f) {
                gv.scroll[static_cast<int>(Tab::Settings)] = s;
                kit.Begin(scratch);
                menu.Draw(kit, go, gv);
                if (on_view(Id::MuzzleFlash) || s >= menu.MaxScroll()) {
                    // Two rows more, so it sits above the list's edge with its tooltip.
                    gv.scroll[static_cast<int>(Tab::Settings)] =
                        std::min(s + 180.0f, menu.MaxScroll());
                    break;
                }
            }
        }
        ok &= shot("5h4-settings-aiming-muzzle-flash-tooltip", go, gv);
        gv.scroll[static_cast<int>(Tab::Settings)] = 0.0f;
    }
    {
        MenuModel so = pg; // Autosave off: Every and Keep greyed, with their tooltip
        so.autosave = false;
        so.locked[static_cast<std::size_t>(Id::AutosaveEvery)] = Lock::AutosaveOff;
        so.locked[static_cast<std::size_t>(Id::AutosaveKeep)] = Lock::AutosaveOff;
        v.hover = {Id::AutosaveEvery, kLabelIndex};
        v.tooltip = Id::AutosaveEvery;
        ok &= shot("5a2-settings-autosave-off", so, v);
        v.tooltip = Id::None;
        v.hover = {};
        v.dragging = Id::AutosaveEvery; // Every held at 10 Min: the stops' minutes under it
        v.drag_t = 0.75f;
        ok &= shot("5a3-settings-every-drag", pg, v);
        v.dragging = Id::None;
        MenuModel gs = pg; // Global chosen, the game has its own Autosave
        gs.scope = 0;
        for (Id id : {Id::Autosave, Id::AutosaveEvery, Id::AutosaveKeep}) {
            gs.locked[static_cast<std::size_t>(id)] = Lock::GameValue;
        }
        v.hover = {Id::Autosave, kLabelIndex};
        v.tooltip = Id::Autosave;
        ok &= shot("5a4-settings-saves-global-game-value", gs, v);
        v = settings(SettingsSection::General);
        v.hover = {Id::Scope, 0};
        ok &= shot("5a5-settings-general-scope-hover", pg, v);
        // AzaharFrame crashreport: the Help group at General's end
        v = settings(SettingsSection::General);
        v.scroll[static_cast<int>(Tab::Settings)] = -1.0f;
        v.hover = {Id::ReportProblem};
        ok &= shot("5a6-settings-general-help", pg, v);
        // AzaharFrame ffspeed: the Fast Forward Audio row (four choices) with its tooltip
        for (int pick : {1, 2, 3}) {
            MenuModel fa = pg;
            fa.fast_forward_audio = pick;
            v = settings(SettingsSection::General);
            v.scroll[static_cast<int>(Tab::Settings)] = 300.0f;
            if (pick == 2) {
                v.hover = {Id::FastForwardAudio, kLabelIndex};
                v.tooltip = Id::FastForwardAudio;
            }
            ok &= shot(pick == 2 ? "5a7-ff-audio-normal-speed-tooltip"
                                 : (pick == 1 ? "5a7-ff-audio-normal-pitch" : "5a7-ff-audio-muted"),
                       fa, v);
            if (pick == 2) { // AzaharFrame kiu2: its menu open
                v.hover = {Id::FastForwardAudio, 2};
                v.tooltip = Id::None;
                v.open_menu = Id::FastForwardAudio;
                ok &= shot("5a7-ff-audio-menu-open", fa, v);
            }
        }
    }
    v = settings(SettingsSection::Immersive);
    v.hover = {Id::GameFov};
    ok &= shot("5b-settings-immersive", pg, v);
    v.hover = {Id::ScaleX, kLabelIndex};
    v.tooltip = Id::ScaleX;
    ok &= shot("5c-settings-tooltip-option", pg, v);
    {
        // AzaharFrame kiu: Horizontal and Vertical Scale, both signs and 0; Render Margin's tip
        MenuModel sc = pg;
        sc.scale_x = -10;
        sc.scale_y = 15;
        v = settings(SettingsSection::Immersive);
        v.scroll[static_cast<int>(Tab::Settings)] = -1.0f;
        ok &= shot("5c2-settings-scales", sc, v);
        v.hover = {Id::ScaleY, kLabelIndex};
        v.tooltip = Id::ScaleY;
        ok &= shot("5c3-settings-vertical-scale-tooltip", sc, v);
        sc.scale_x = 30;
        sc.scale_y = 0;
        v.scroll[static_cast<int>(Tab::Settings)] = 0.0f;
        v.hover = {Id::WiderView, kLabelIndex};
        v.tooltip = Id::WiderView;
        ok &= shot("5c4-settings-render-margin-tooltip", sc, v);
        // AzaharFrame fovscale: Scale Mode, Stretch | Field of View, each picked with its
        // tooltip under the beam, and the label's.
        sc.scale_x = -30;
        sc.scale_y = -15;
        v = settings(SettingsSection::Immersive);
        v.scroll[static_cast<int>(Tab::Settings)] = -1.0f;
        static constexpr const char* kModes[] = {"stretch", "fov"};
        for (int i = 0; i < 2; i++) {
            sc.scale_mode = i;
            v.hover = {Id::ScaleMode, i};
            v.tooltip = Id::ScaleMode;
            v.tooltip_index = i;
            ok &= shot(std::string("fs-scale-mode-tip-") + kModes[i], sc, v);
        }
        v.hover = {Id::ScaleMode, kLabelIndex};
        v.tooltip_index = kLabelIndex;
        ok &= shot("fs-scale-mode-tip-label", sc, v);
    }
    MenuModel gv = m; // Global chosen, the game has its own Immersive Mode
    gv.locked[static_cast<std::size_t>(Id::ImmersiveMode)] = Lock::GameValue;
    v = settings(SettingsSection::Immersive);
    v.hover = {Id::ImmersiveMode, kLabelIndex};
    v.tooltip = Id::ImmersiveMode;
    ok &= shot("5e-settings-game-value-locked", gv, v);
    MenuModel off = pg;
    off.immersive_mode = 0;
    v = settings(SettingsSection::Immersive);
    v.open_menu = Id::ImmersiveMode;
    v.hover = {Id::ImmersiveMode, 2};
    ok &= shot("5d-settings-immersive-menu", off, v);

    // AzaharFrame menuux: the renderer's rows, the Internal Resolution menu, Dynamic Resolution
    // at 1x, and the Position tab's longest offset. AzaharFrame autosave: the Graphics section.
    MenuModel rm = pg;
    rm.internal_resolution = 3;
    v = settings(SettingsSection::Graphics);
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
    // AzaharFrame xfergpu: Accurate Image Copies, Off | Automatic | On, each choice's tooltip
    // with the beam on it, and the label's.
    {
        MenuModel ic = pg;
        ic.internal_resolution = 3;
        ic.accurate_image_copies = 1;
        v = settings(SettingsSection::Graphics);
        static constexpr const char* kNames[] = {"off", "automatic", "on"};
        for (int i = 0; i < 3; i++) {
            v.hover = {Id::AccurateImageCopies, i};
            v.tooltip = Id::AccurateImageCopies;
            v.tooltip_index = i;
            ok &= shot(std::string("xg-image-copies-tip-") + kNames[i], ic, v);
        }
        v.hover = {Id::AccurateImageCopies, kLabelIndex};
        v.tooltip_index = kLabelIndex;
        ok &= shot("xg-image-copies-tip-label", ic, v);
        v = settings(SettingsSection::Graphics);
    }
    // AzaharFrame fovonce: Detect Game FOV, Off | Once | Always, each choice picked with its
    // tooltip under the beam, and the label's.
    {
        MenuModel mc = pg;
        mc.game_fov = 52;
        v = settings(SettingsSection::Immersive);
        static constexpr const char* kNames[] = {"off", "once", "always"};
        for (int i = 0; i < 3; i++) {
            mc.match_camera = i;
            v.hover = {Id::MatchCamera, i};
            v.tooltip = Id::MatchCamera;
            v.tooltip_index = i;
            ok &= shot(std::string("fo-detect-fov-tip-") + kNames[i], mc, v);
        }
        v.hover = {Id::MatchCamera, kLabelIndex};
        v.tooltip_index = kLabelIndex;
        ok &= shot("fo-detect-fov-tip-label", mc, v);
        v = settings(SettingsSection::Immersive);
        // AzaharFrame kiu: each choice picked, nothing under the beam: the check mark on each.
        for (int i = 0; i < 3; i++) {
            mc.match_camera = i;
            ok &= shot(std::string("fo-detect-fov-selected-") + kNames[i], mc, v);
        }
    }
    // AzaharFrame reticle: the scope control (Global | Per-Game, ⋮, Per-Game's count tooltip) on the
    // Settings, Controls and Position tabs, in Per-Game and Global, the ⋮ menu open, and no game.
    {
        const auto own = [](MenuModel mm, std::initializer_list<Id> ids, int scope) {
            mm.scope = scope;
            for (Id id : ids) {
                mm.game_value[static_cast<std::size_t>(id)] = true;
                if (scope == 0) {
                    mm.locked[static_cast<std::size_t>(id)] = Lock::GameValue;
                }
            }
            return mm;
        };
        MenuModel ug = pg;
        ug.internal_resolution = 3;
        ug.depth_3d = 80;
        ug.async_shaders = true;
        const std::initializer_list<Id> gfx{Id::Depth3d, Id::InternalResolution, Id::AsyncShaders};
        v = settings(SettingsSection::Graphics);
        ok &= shot("sc0-settings-graphics-per-game-none", own(ug, {}, 1), v);
        ok &= shot("sc1-settings-graphics-per-game", own(ug, gfx, 1), v);
        // Pointing at Per-Game: the count, under the toggle (none, and three)
        v.hover = {Id::Scope, 1};
        v.tooltip = Id::Scope;
        v.tooltip_index = 1;
        ok &= shot("sc0a-settings-per-game-tooltip-none", own(ug, {}, 1), v);
        ok &= shot("sc1a-settings-per-game-tooltip", own(ug, gfx, 1), v);
        // AzaharFrame integ8: 3D Depth's tooltip, with the stereo cap's line
        v = settings(SettingsSection::Graphics);
        v.hover = {Id::Depth3d, kLabelIndex};
        v.tooltip = Id::Depth3d;
        ok &= shot("sc1b-settings-3d-depth-tooltip", own(ug, gfx, 1), v);
        v = settings(SettingsSection::Graphics);
        ok &= shot("sc2-settings-graphics-global", own(ug, gfx, 0), v);
        v.hover = {Id::Depth3d, kLabelIndex};
        v.tooltip = Id::Depth3d;
        ok &= shot("sc2a-settings-graphics-global-locked-tooltip", own(ug, gfx, 0), v);
        v = settings(SettingsSection::Graphics);
        v.open_menu = Id::ScopeMore;
        v.hover = {Id::ScopeMore, 0};
        ok &= shot("sc3-settings-scope-more-menu", own(ug, gfx, 1), v);
        v = settings(SettingsSection::Graphics);
        v.hover = {Id::ScopeMore, kLabelIndex};
        v.tooltip = Id::ScopeMore;
        ok &= shot("sc3a-settings-scope-more-disabled", own(ug, {}, 1), v);
        MenuModel ugs = pg;
        ugs.autosave_keep = 8;
        v = settings(SettingsSection::General);
        ok &= shot("sc4-settings-general-per-game", own(ugs, {Id::Autosave, Id::AutosaveKeep}, 1), v);
        MenuModel ugi = pg;
        ugi.game_fov = 85;
        ugi.wider_view = 3;
        v = settings(SettingsSection::Immersive);
        ok &= shot("sc5-settings-immersive-per-game",
                   own(ugi, {Id::WiderView, Id::GameFov, Id::MatchCamera}, 1), v);
        // Position: Hide Bottom Screen and Screen Shape follow the toggle too
        MenuModel pos = pg;
        pos.screen_shape = 1;
        v = {};
        v.tab = Tab::Position;
        ok &= shot("sc6-position-per-game", own(pos, {Id::HideBottomScreen, Id::ScreenShape}, 1), v);
        v.hover = {Id::Scope, 1};
        v.tooltip = Id::Scope;
        v.tooltip_index = 1;
        ok &= shot("sc6a-position-per-game-tooltip",
                   own(pos, {Id::HideBottomScreen, Id::ScreenShape}, 1), v);
        v = {};
        v.tab = Tab::Position;
        ok &= shot("sc7-position-global", own(pos, {Id::HideBottomScreen, Id::ScreenShape}, 0), v);
        v.open_menu = Id::ScopeMore;
        v.hover = {Id::ScopeMore, 0};
        ok &= shot("sc8-position-scope-more-menu",
                   own(pos, {Id::HideBottomScreen, Id::ScreenShape}, 1), v);
        // Controls: Kid Icarus: Uprising with its own Bottom Screen Reticle and Reticle Speed
        MenuModel ctl = pg;
        ctl.game_title = "Kid Icarus: Uprising";
        ctl.bottom_screen_reticle = 3;
        ctl.reticle_speed = 25;
        const std::initializer_list<Id> cids{Id::BottomScreenReticle, Id::ReticleSpeed};
        v = {};
        v.tab = Tab::Controls;
        ok &= shot("sc9-controls-per-game", own(ctl, cids, 1), v);
        v.hover = {Id::Scope, 1};
        v.tooltip = Id::Scope;
        v.tooltip_index = 1;
        ok &= shot("sc9a-controls-per-game-tooltip", own(ctl, cids, 1), v);
        v = {};
        v.tab = Tab::Controls;
        ok &= shot("sc10-controls-global", own(ctl, cids, 0), v);
        v.hover = {Id::BottomScreenReticle, kLabelIndex};
        v.tooltip = Id::BottomScreenReticle;
        ok &= shot("sc10a-controls-global-locked-tooltip", own(ctl, cids, 0), v);
        v = {};
        v.tab = Tab::Controls;
        v.open_menu = Id::ScopeMore;
        v.hover = {Id::ScopeMore, 0};
        ok &= shot("sc11-controls-scope-more-menu", own(ctl, cids, 1), v);
        MenuModel ctlw = own(ctl, cids, 1);
        ctlw.controls_page = 1;
        v = {};
        v.tab = Tab::Controls;
        ok &= shot("sc12-controls-window-page-per-game", ctlw, v);
        // No game that can have its own values: Global alone, ⋮ greyed
        for (const Tab t : {Tab::Settings, Tab::Controls, Tab::Position}) {
            MenuModel ng = m;
            ng.scope = 0;
            ng.locked[static_cast<std::size_t>(Id::Scope)] = Lock::NoGame;
            v = {};
            v.tab = t;
            if (t == Tab::Settings) {
                v.section = SettingsSection::General;
            }
            v.hover = {Id::Scope, kLabelIndex};
            v.tooltip = Id::Scope;
            ok &= shot(std::string("sc13-no-game-") +
                           (t == Tab::Settings ? "settings" : t == Tab::Controls ? "controls" : "position"),
                       ng, v);
        }
    }

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
    {
        // AzaharFrame scroll: a long Cheats list (Pokémon X has 125) scrolled part of the way, its
        // scrollbar at rest, with the beam on it (wider, the track shows) and held (dragged on);
        // the beam's dot where it points.
        static const char* const kNames[] = {
            "Refill AP 4th ATK 4th PKMN (Press LEFT+L)", "Max AP 4th ATK 4th PKMN (Press LEFT+R)",
            "Lvl 100 aft battle 5th PKMN (Press Y)", "1st ATK 5th PKMN Modifier",
            "2nd ATK 5th PKMN Modifier", "Refill HP 5th PKMN (Press L)", "Infinite Money",
            "Max Battle Points", "Walk Through Walls (Hold R)", "Shiny Wild Encounters"};
        MenuModel lm = m;
        lm.mods.cheats = SampleCheats();
        lm.mods.cheats.cheats.clear();
        for (int i = 0; i < 125; i++) {
            lm.mods.cheats.cheats.push_back(
                {std::string(kNames[i % 10]) + " " + std::to_string(i / 10 + 1),
                 CheatSource::Sharkive, i % 9 == 4,
                 i % 6 == 0 ? "for v1.5, this game is v1.0" : ""});
        }
        lm.mods.cheats.status = "125 cheats added, 21 for another version";
        const auto dot = [](float x, float y) {
            return [x, y](Kit& k) {
                k.Fill({x - 8.0f, y - 8.0f, x + 8.0f, y + 8.0f}, 8.0f, 0x000000, 0.45f);
                k.Fill({x - 6.0f, y - 6.0f, x + 6.0f, y + 6.0f}, 6.0f, 0xFFFFFF, 0.95f);
            };
        };
        MenuView sv;
        sv.tab = Tab::Mods;
        sv.cheats_other_open = true; // the whole list
        sv.scroll[static_cast<int>(Tab::Mods)] = 1234.0f; // mid-list: a part row at the top
        ok &= shot("scroll-cheats-idle", lm, sv);
        const Scrolling::Bar bar = menu.Layout().bar;
        const float hx = Scrolling::HitRect(bar).Cx();
        std::printf("scroll-cheats: list %.1f..%.1f, %.1f..%.1f dp, track %.1f..%.1f, thumb %.1f\n",
                    bar.list.x0, bar.list.x1, bar.list.y0, bar.list.y1, Scrolling::TrackY0(bar),
                    Scrolling::TrackY1(bar), Scrolling::ThumbH(bar));
        sv.hover = {Id::Scrollbar};
        ok &= shot("scroll-cheats-hover", lm, sv, {},
                   dot(hx, Scrolling::ThumbRect(bar, 1234.0f, true).Cy()));
        sv.pressed = {Id::Scrollbar};
        sv.scroll[static_cast<int>(Tab::Mods)] = 3700.0f; // dragged on down the track
        ok &= shot("scroll-cheats-dragged", lm, sv, {},
                   dot(hx + 10.0f, Scrolling::ThumbRect(bar, 3700.0f, true).Cy() + 6.0f));
        // The cheats for another version in their own section, as Pokémon X 1.0 with Sharkive's
        // 125 (119 for v1.5, two of them on): closed, with the beam on it; open, scrolled to it;
        // and every cheat for another version (the note over the section).
        MenuModel xm = lm;
        for (int i = 0; i < 125; i++) {
            CheatItem& ch = xm.mods.cheats.cheats[static_cast<std::size_t>(i)];
            ch.version_note = i < 6 ? "" : "for v1.5, this game is v1.0";
            ch.enabled = i == 2 || i == 40 || i == 77;
        }
        xm.mods.cheats.status = "125 cheats added, 119 for another version";
        MenuView xv;
        xv.tab = Tab::Mods;
        xv.hover = {Id::CheatsOtherVersion};
        ok &= shot("cheats-other-closed", xm, xv);
        xv.hover = {};
        xv.cheats_other_open = true;
        xv.scroll[static_cast<int>(Tab::Mods)] = 4.0f * 48.0f;
        ok &= shot("cheats-other-open", xm, xv);
        MenuModel am = xm;
        for (CheatItem& ch : am.mods.cheats.cheats) {
            ch.version_note = "for v1.5, this game is v1.0";
        }
        am.mods.cheats.status = "125 cheats added, 125 for another version";
        xv = {};
        xv.tab = Tab::Mods;
        ok &= shot("cheats-other-all", am, xv);
        // The same bar on the Settings tab (a short overflow: a long thumb).
        MenuView gv;
        gv.tab = Tab::Settings;
        gv.section = SettingsSection::Graphics;
        gv.scroll[static_cast<int>(Tab::Settings)] = 100.0f;
        ok &= shot("scroll-settings-idle", m, gv);
        const Scrolling::Bar gbar = menu.Layout().bar;
        gv.hover = {Id::Scrollbar};
        ok &= shot("scroll-settings-hover", m, gv, {},
                   dot(Scrolling::HitRect(gbar).Cx(), Scrolling::ThumbRect(gbar, 100.0f, true).Cy()));
    }
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
        { // AzaharFrame jsoncrash: the list still being made, and the list that could not be made
            MenuModel ml = m;
            ml.game_title = "Pokemon X";
            ml.mods.mods = {};
            ml.mods.mods.catalog = CatalogState::Loading;
            MenuView lv = v;
            lv.hover = {};
            ok &= shot("mods-install-loading", ml, lv);
            ml.mods.mods.catalog = CatalogState::Failed;
            lv.hover = {Id::InstallMods};
            ok &= shot("mods-install-failed", ml, lv);
        }
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
        { // AzaharFrame fixes7: Install Mods: the recommended badge (first), the size while it
          // downloads, Resume after a Cancel, a rate limit
            MenuModel mr = mi;
            auto& list = mr.mods.mods.available;
            list[0].recommended = true;
            MenuView rv = v;
            rv.hover = {};
            ok &= shot("fixes7-mods-install-recommended", mr, rv);
            mr.mods.mods.busy = true;
            list[0].state = OfferState::Installing;
            list[0].note = "Downloading 40% \u00b7 " + ProgressSizeText(12ull << 20, 30ull << 20);
            mr.mods.mods.spin = 9;
            rv.hover = {Id::ModCancel, 0};
            ok &= shot("fixes7-mods-install-progress", mr, rv);
            mr.mods.mods.busy = false;
            list[0].state = OfferState::Install;
            list[0].note.clear();
            list[0].resume = ResumeText(12ull << 20, 30ull << 20); // "12 of 30 MB Downloaded"
            rv.hover = {Id::ModInstall, 0};
            ok &= shot("fixes7-mods-install-resume", mr, rv);
            list[0].resume.clear();
            list[0].note = "GameBanana is limiting downloads from this network. Try again later.";
            list[0].error = true;
            ok &= shot("fixes7-mods-install-rate-limited", mr, rv);
        }
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

            // AzaharFrame emhb: Uninstall, a copy from another computer, Repair, and the dialog
            HomebrewItem in = it;
            in.installed = true;
            in.can_uninstall = true;
            in.author = "ZallaxDev \u00b7 English";
            in.version = "v0.5.0";
            in.checked = "Checked Just Now";
            hb.homebrew.items = {in};
            hv.hover = {Id::HomebrewUninstall, 0};
            ok &= shot("homebrew-uninstall", hb, hv);
            hb.homebrew.items[0].author = "ZallaxDev"; // a copy: no release named in its files
            hb.homebrew.items[0].version.clear();
            hb.homebrew.items[0].update = true;
            hb.homebrew.items[0].latest = "v0.6.0";
            hv.hover = {};
            ok &= shot("homebrew-imported", hb, hv);
            HomebrewItem br = it;
            br.repair = true;
            br.can_uninstall = true;
            br.author = "ZallaxDev \u00b7 Game Data Missing";
            hb.homebrew.items = {br};
            hv.hover = {Id::HomebrewInstall, 0};
            ok &= shot("homebrew-repair", hb, hv);
            hb.homebrew.items = {in};
            HomebrewDialog& dl = hb.homebrew.dialog;
            dl.id = "emerald3ds";
            dl.title = "Uninstall Pok\u00e9mon Emerald 3DS?";
            dl.body = "Removes the game and its build files. Your ROM and save data are kept.";
            dl.check = "Also Delete Save Data";
            dl.confirm = "Uninstall";
            dl.cancel = "Cancel";
            hv.hover = {Id::HomebrewDialogCancel};
            ok &= shot("homebrew-uninstall-dialog", hb, hv);
            dl.checked = true;
            dl.body = "Removes the game, its build files and your save data. Your ROM is kept.";
            hv.hover = {Id::HomebrewDialogCheck};
            ok &= shot("homebrew-uninstall-dialog-save", hb, hv);
            dl.checked = false;
            dl.check.clear();
            dl.body = "Removes the game and its build files. Your ROM is kept.";
            hv.hover = {Id::HomebrewDialogConfirm};
            ok &= shot("homebrew-uninstall-dialog-nosave", hb, hv);
            dl = {};
            dl.id = "emerald3ds";
            dl.title = "Stop the Game First";
            dl.body = "Pok\u00e9mon Emerald 3DS is running. Stop it, then uninstall it from the "
                      "Homebrew menu.";
            dl.confirm = "OK";
            hv.hover = {};
            ok &= shot("homebrew-uninstall-running", hb, hv);
            dl = {};
            hb.homebrew.items[0].busy = true;
            hb.homebrew.items[0].status = "Uninstalling";
            hb.homebrew.spin = 7;
            ok &= shot("homebrew-uninstalling", hb, hv);
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
            // AzaharFrame fixes7: the line by size, the percent of the same (the copy's estimate)
            pp.optimize.done = 137;
            pp.optimize.total = 1624;
            pp.optimize.permille = 491;
            pp.optimize.done_bytes = 3ull << 30;
            pp.optimize.total_bytes = (61ull << 30) / 10;
            ok &= shot("fixes7-optimize-progress-size", om, ov);
            pp.optimize = {OptimizeState::Stopped, "4.7 GB"};
            ok &= shot("optimize-stopped", om, ov);
            pp.optimize = {OptimizeState::Failed, "4.7 GB", 0, 0, -1, {}, "Not Enough Space"};
            ok &= shot("optimize-failed", om, ov);
            pp.optimize = {OptimizeState::Optimized, "2.1 GB"};
            pp.optimize.original_size = "4.7 GB";
            om.mods.textures.needs_restart = true;
            ok &= shot("optimize-optimized", om, ov);
            // AzaharFrame ui3: Remove Original (the copy in use), its dialog, and after
            om.mods.textures.needs_restart = false;
            pp.optimize.can_remove = true;
            ov.hover = {Id::RemoveOriginal};
            ok &= shot("optimize-remove", om, ov);
            om.mods.textures.dialog = {ModsDialogKind::RemoveOriginal,
                                       RemoveOriginalBody("4.7 GB", true), ""};
            ov.hover = {Id::DialogConfirm};
            ok &= shot("optimize-remove-confirm", om, ov);
            om.mods.textures.dialog = {};
            pp.optimize.original_removed = true;
            pp.optimize.can_remove = false;
            pp.title = "4K"; // the page is the copy's now
            ov.hover = {};
            ok &= shot("optimize-removed", om, ov);
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
        { // AzaharFrame fixes7: Majora's Mask 3D 4K (3.4 GB, a Drive folder): Preparing while the
          // file is found, then the size moving with the percent, and Resume after a Cancel
            MenuModel mm = dm;
            mm.game_title = "The Legend of Zelda: Majora's Mask 3D";
            PackOffer o;
            o.id = "mm3d-4k-henriko";
            o.name = "Majora's Mask 3D 4K";
            o.author = "Henriko Magnifico";
            o.url = "https://www.henrikomagnifico.com/zelda-majoras-mask-3d-4k";
            o.size_text = "3.4 GB";
            o.updated = "Updated Jan 2026";
            o.recommended = true;
            PackOffer o2 = o;
            o2.id = "mm3d-1080p-henriko";
            o2.name = "Majora's Mask 3D 1080p";
            o2.size_text = "1.1 GB";
            mm.mods.textures.downloads = {o, o2};
            mm.mods.textures.downloading = true;
            mm.mods.textures.spin = 9;
            PackOffer& row = mm.mods.textures.downloads[0];
            row.state = PackOfferState::Downloading;
            row.percent = -1;
            row.note = "Preparing";
            v.hover = {Id::PackCancel, 0};
            ok &= shot("fixes7-download-preparing", mm, v);
            row.percent = 3;
            row.note = ProgressSizeText(120ull << 20, (34ull << 30) / 10); // "120 MB of 3.4 GB"
            ok &= shot("fixes7-download-progress", mm, v);
            mm.mods.textures.downloading = false;
            row.state = PackOfferState::Download;
            row.percent = -1;
            row.note.clear();
            row.resume = ResumeText((12ull << 30) / 10, (34ull << 30) / 10); // "1.2 of 3.4 GB Downloaded"
            v.hover = {Id::PackDownload, 0};
            ok &= shot("fixes7-download-resume", mm, v);
            // the badge: recommended packs first (no row is busy)
            row.resume.clear();
            v.hover = {};
            ok &= shot("fixes7-download-recommended", mm, v);
            // a site that limits downloads
            MenuModel rl = dm;
            rl.mods.textures.downloads[0].state = PackOfferState::Failed;
            rl.mods.textures.downloads[0].note =
                "GameBanana is limiting downloads from this network. Try again in 10 minutes.";
            v.hover = {Id::PackDownload, 0};
            ok &= shot("fixes7-download-rate-limited", rl, v);
        }
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
        // AzaharFrame memguard: Preload Textures? with this machine's memory now (run the dump
        // inside the launcher's scope to see Azahar's Limit): OoT3D 4K as ASTC (6034295947
        // bytes) and a 0.9 GB pack, or AZAHARFRAME_DUMP_PRELOAD_BIG / _SMALL bytes
        {
            namespace PB = VideoCore::PreloadBudget;
            const auto bytes = [](const char* env, std::uint64_t fallback) {
                const char* e = std::getenv(env);
                return e && *e ? std::strtoull(e, nullptr, 10) : fallback;
            };
            const std::pair<const char*, std::uint64_t> cases[] = {
                {"preload-too-large", bytes("AZAHARFRAME_DUMP_PRELOAD_BIG", 6034295947ULL)},
                {"preload-fits", bytes("AZAHARFRAME_DUMP_PRELOAD_SMALL", 900'000'000ULL)},
            };
            for (const auto& [name, need] : cases) {
                const PB::ConfirmText t =
                    PB::MakeConfirmText(PB::Summarize(need, PB::ReadRoom(), PB::ReserveFromEnv()));
                MenuModel pm = dp;
                ModsDialog& d = pm.mods.textures.dialog;
                d = {ModsDialogKind::Preload, t.body, t.status_text};
                d.rows = t.rows;
                d.status = static_cast<int>(t.status);
                d.title = t.title;
                v.hover = {Id::DialogConfirm};
                ok &= shot(name, pm, v);
            }
        }
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
    // AzaharFrame ctgp7: the Mods tab's CTGP-7 segment (shown while Mario Kart 7 is open), each
    // state, and "Requires CTGP-7" over Install Mods
    {
        MenuModel ct = m;
        ct.game_title = "Mario Kart 7";
        ct.mods.ctgp7.shown = true;
        MenuView cv;
        cv.tab = Tab::Mods;
        cv.mods_segment = ModsSegment::Ctgp7;
        cv.hover = {Id::Ctgp7Install};
        ok &= shot("ctgp7-install", ct, cv);
        cv.hover = {Id::ModsSegment, 3};
        ok &= shot("ctgp7-segment-hover", ct, cv);
        cv.hover = {};
        MenuModel blocked = ct;
        blocked.mods.ctgp7.blocker = "Needs Mario Kart 7 Version 1.2";
        ok &= shot("ctgp7-needs-update", blocked, cv);
        blocked.mods.ctgp7.dialog.kind = ModsDialogKind::Notice;
        blocked.mods.ctgp7.dialog.mod = "Update Needed";
        blocked.mods.ctgp7.dialog.other =
            "CTGP-7 needs Mario Kart 7 version 1.2. Dump the 1.2 update from your 3DS and install "
            "it with File > Install CIA in Azahar's window.";
        blocked.mods.ctgp7.dialog.confirm = "OK";
        ok &= shot("ctgp7-update-dialog", blocked, cv);
        MenuModel save = ct;
        save.mods.ctgp7.blocker = "Play Mario Kart 7 Once First";
        save.mods.ctgp7.dialog.kind = ModsDialogKind::Notice;
        save.mods.ctgp7.dialog.mod = "Play Mario Kart 7 First";
        save.mods.ctgp7.dialog.other =
            "CTGP-7 needs Mario Kart 7's save data. Play until the main menu, then try again.";
        save.mods.ctgp7.dialog.confirm = "OK";
        ok &= shot("ctgp7-save-dialog", save, cv);
        MenuModel choose = ct;
        choose.mods.ctgp7.dialog.kind = ModsDialogKind::Notice;
        choose.mods.ctgp7.dialog.mod = "Choose Your Mario Kart 7";
        choose.mods.ctgp7.dialog.other = "This Copy Is Encrypted. CTGP-7 needs a decrypted .3ds, "
                                         ".cci or .cia of your own Mario Kart 7.";
        choose.mods.ctgp7.dialog.confirm = "Choose";
        choose.mods.ctgp7.dialog.cancel = "Cancel";
        ok &= shot("ctgp7-choose-dialog", choose, cv);
        MenuModel busy = ct;
        busy.mods.ctgp7.busy = true;
        busy.mods.ctgp7.phase = "Installing Mario Kart 7";
        busy.mods.ctgp7.detail = "620 MB of 1.1 GB";
        busy.mods.ctgp7.permille = 320;
        ok &= shot("ctgp7-installing-game", busy, cv);
        busy.mods.ctgp7.phase = "Downloading CTGP-7";
        busy.mods.ctgp7.detail = "120 MB of 767 MB";
        busy.mods.ctgp7.permille = 156;
        cv.hover = {Id::Ctgp7Cancel};
        ok &= shot("ctgp7-downloading", busy, cv);
        cv.hover = {};
        busy.mods.ctgp7.phase = "Unpacking CTGP-7";
        busy.mods.ctgp7.detail.clear();
        busy.mods.ctgp7.permille = -1;
        busy.mods.ctgp7.spin = 7;
        ok &= shot("ctgp7-unpacking", busy, cv);
        busy.mods.ctgp7.phase = "Installing CTGP-7";
        busy.mods.ctgp7.detail = "This Takes About 5 Minutes";
        busy.mods.ctgp7.permille = 430;
        ok &= shot("ctgp7-running-installer", busy, cv);
        MenuModel failed = ct;
        failed.mods.ctgp7.error = "Install Failed: No Internet Connection";
        ok &= shot("ctgp7-failed", failed, cv);
        failed.mods.ctgp7.error = "Install Failed: Not Enough Space: Needs 3.6 GB Free";
        ok &= shot("ctgp7-no-space", failed, cv);
        // AzaharFrame ctgp7b: GameBanana's errors (the owner's wording)
        failed.mods.ctgp7.error = "Install Failed: The CTGP-7 Installer Isn't on GameBanana";
        ok &= shot("ctgp7-failed-not-on-gamebanana", failed, cv);
        failed.mods.ctgp7.error = "Install Failed: GameBanana Error 503";
        ok &= shot("ctgp7-failed-gamebanana-error", failed, cv);
        MenuModel done = ct;
        done.mods.ctgp7.installed = true;
        done.mods.ctgp7.version = "1.6.30";
        cv.hover = {Id::Ctgp7Launch};
        ok &= shot("ctgp7-installed", done, cv);
        cv.hover = {Id::Ctgp7Website};
        ok &= shot("ctgp7-website-hover", done, cv);
        cv.hover = {};
        cv.open_link = "https://ctgp-7.github.io/";
        ok &= shot("ctgp7-website-dialog", done, cv);
        cv.open_link.clear();
        cv.hover = {Id::Ctgp7Uninstall};
        ok &= shot("ctgp7-uninstall-hover", done, cv);
        cv.hover = {};
        MenuModel un = done;
        un.mods.ctgp7.dialog.kind = ModsDialogKind::Notice;
        un.mods.ctgp7.dialog.mod = "Uninstall CTGP-7?";
        un.mods.ctgp7.dialog.other = "This removes CTGP-7, its launcher and the mods installed for "
                                     "it. Mario Kart 7, its save data and CTGP-7's progress stay.";
        un.mods.ctgp7.dialog.confirm = "Uninstall";
        un.mods.ctgp7.dialog.cancel = "Cancel";
        un.mods.ctgp7.dialog.check = "Also Delete CTGP-7's Progress";
        ok &= shot("ctgp7-uninstall-dialog", un, cv);
        cv.dialog_check = true;
        cv.hover = {Id::DialogCheck};
        ok &= shot("ctgp7-uninstall-dialog-checked", un, cv);
        cv.dialog_check = false;
        cv.hover = {};
        un.mods.ctgp7.dialog.other =
            "This removes CTGP-7, its launcher and the mods installed for it. Uninstall Both also "
            "removes the Mario Kart 7 it installed. Your game file, its save data and CTGP-7's "
            "progress stay.";
        un.mods.ctgp7.dialog.extra = "Uninstall Both";
        ok &= shot("ctgp7-uninstall-dialog-both", un, cv);
        MenuModel uning = ct;
        uning.mods.ctgp7.busy = true;
        uning.mods.ctgp7.can_cancel = false;
        uning.mods.ctgp7.phase = "Uninstalling CTGP-7";
        uning.mods.ctgp7.spin = 3;
        ok &= shot("ctgp7-uninstalling", uning, cv);
        MenuModel unfinished = ct;
        unfinished.mods.ctgp7.setup_unfinished = true;
        cv.hover = {Id::Ctgp7Install};
        ok &= shot("ctgp7-setup-unfinished", unfinished, cv);
        cv.hover = {};
        // Not Mario Kart 7: three segments, as before
        MenuModel other = m;
        MenuView ov;
        ov.tab = Tab::Mods;
        ov.mods_segment = ModsSegment::Cheats;
        ok &= shot("ctgp7-hidden-other-game", other, ov);
        // AzaharFrame ctgp7b: the catalog's mods for CTGP-7 (the eight of its ctgp7 branch) in the
        // segment, under the status row; Install Mods and the loaded list keep the others
        MenuModel req = ct;
        const auto offer = [](const char* id, const char* name, const char* author, const char* url,
                              bool ctgp7) {
            ModOffer o;
            o.id = id;
            o.name = name;
            o.author = author;
            o.url = url;
            o.notes = ctgp7 ? "Requires CTGP-7." : "";
            o.requires_ctgp7 = ctgp7;
            return o;
        };
        auto& rm = req.mods.mods;
        rm.available = {
            offer("mk7-sonic", "Sonic", "Blurro", "https://gamebanana.com/members/1", false),
            offer("mk7-sm64-mario", "SM64 Mario", "NintendoGameTuber", "https://gamebanana.com/members/2", false),
            offer("mk7-hatsune-miku", "Hatsune Miku", "Sedral", "https://gamebanana.com/members/2124152", true),
            offer("mk7-pikmin-red", "Pikmin Red", "Yasioen", "https://gamebanana.com/members/3", true),
            offer("mk7-pikmin-blue", "Pikmin Blue", "Yasioen", "https://gamebanana.com/members/3", true),
            offer("mk7-pikmin-yellow", "Pikmin Yellow", "Yasioen", "https://gamebanana.com/members/3", true),
            offer("mk7-pikmin-purple", "Pikmin Purple", "Yasioen", "https://gamebanana.com/members/3", true),
            offer("mk7-pikmin-white", "Pikmin White", "Yasioen", "https://gamebanana.com/members/3", true),
            offer("mk7-pikmin-mushroom", "Pikmin Mushroom (Puffmin)", "Yasioen", "https://gamebanana.com/members/3", true),
            offer("mk7-midna", "Midna", "TheHumanBanana", "https://gamebanana.com/members/4", true)};
        rm.available[2].downloads = "10K+ Downloads";
        rm.available[2].updated = "Updated May 2024";
        rm.catalog = CatalogState::Ready;
        MenuView rv;
        rv.tab = Tab::Mods;
        rv.mods_segment = ModsSegment::Ctgp7;
        ok &= shot("ctgp7-mods-not-installed", req, rv);
        rv.scroll[static_cast<int>(Tab::Mods)] = 10000.0f;
        ok &= shot("ctgp7-mods-not-installed-end", req, rv);
        rv.scroll[static_cast<int>(Tab::Mods)] = 0.0f;
        req.mods.ctgp7.installed = true;
        req.mods.ctgp7.version = "1.6.30";
        ModRow sonic_row, miku_row;
        sonic_row.id = "mk7-sonic";
        sonic_row.name = "Sonic";
        sonic_row.author = "Blurro";
        sonic_row.url = "https://gamebanana.com/members/1";
        sonic_row.managed = true;
        miku_row.id = "mk7-hatsune-miku";
        miku_row.name = "Hatsune Miku";
        miku_row.author = "Sedral";
        miku_row.url = "https://gamebanana.com/members/2124152";
        miku_row.managed = true;
        rm.loaded = {sonic_row, miku_row};
        rm.available[0].state = OfferState::Installed;
        rm.available[2].state = OfferState::Installed;
        rv.hover = {Id::ModInstall, 3};
        ok &= shot("ctgp7-mods-installed", req, rv);
        rm.available[3].state = OfferState::Installing;
        rm.available[3].note = "Downloading 42%";
        rm.busy = true;
        rm.spin = 5;
        rv.hover = {};
        ok &= shot("ctgp7-mods-installing", req, rv);
        rm.available[3].state = OfferState::Install;
        rm.available[3].note.clear();
        rm.busy = false;
        rv.mods_segment = ModsSegment::Mods;
        ok &= shot("ctgp7-mods-loaded-list", req, rv);
        rv.mods_page = ModsPage::Install;
        ok &= shot("ctgp7-mods-install-mods", req, rv);
    }
    v = {};
    v.tab = Tab::Main;
    ok &= shot("main", m, v);
    return ok ? 0 : 1;
}
