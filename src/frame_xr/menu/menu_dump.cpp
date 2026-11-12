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
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <lodepng.h>

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
         false, OfferState::Install, "", false},
        {"oot3d-ocarina-reframed-dual", "Ocarina ReFramed (Dual Screen)", "OTPR26", "https://github.com/OTPR26/OOT3DHud",
         false, OfferState::Install, "", false}};
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
                    -1, ""}};
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
        MenuModel mp = mi; // downloading the first, the second waits
        mp.mods.mods.busy = true;
        mp.mods.mods.available[0].state = OfferState::Installing;
        mp.mods.mods.available[0].note = "Downloading";
        mp.mods.mods.spin = 9;
        v.hover = {};
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

        MenuModel tm = m;
        tm.mods.textures = SampleTextures();
        v = {};
        v.tab = mods_tab;
        v.mods_segment = ModsSegment::Textures;
        v.hover = {Id::TexturePack, 1};
        ok &= shot("textures", tm, v);
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
    }
    v = {};
    v.tab = Tab::Main;
    ok &= shot("main", m, v);
    return ok ? 0 : 1;
}
