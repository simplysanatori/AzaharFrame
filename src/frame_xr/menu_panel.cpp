// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <utility>

#include <fmt/format.h>
#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu_panel.h"
#include "frame_xr/session.h"
#include "frame_xr/vis/controller_models.h"
#include "frame_xr/vis/help_content.h"
#include "frame_xr/vis/menu_diagram.h"
#include "frame_xr/vis/visuals.h"

namespace FrameXr {

namespace {

using Menu::Id;
using Menu::Lock;
using Menu::Target;

// 1.75 pixels a dp: 1470 x 980, under the 1000 rows past which SteamVR's foveated rendering
// starts to soften a quad on the Frame.
constexpr float kPxPerDp = 1.75f;
constexpr int kTexW = static_cast<int>(Menu::kPanelW * kPxPerDp + 0.5f);
constexpr int kTexH = static_cast<int>(Menu::kPanelH * kPxPerDp + 0.5f);
constexpr float kPress = 0.55f, kLetGo = 0.40f; ///< trigger hysteresis
constexpr float kTooltipRest = 0.6f;            ///< seconds (owner option A)
constexpr float kStickOn = 0.6f;
constexpr float kStickFirstRepeat = 0.4f, kStickRepeat = 0.2f;
constexpr std::array<uint32_t, 7> kCurveSteps{0, 15, 30, 45, 60, 75, 90};
constexpr std::array<uint32_t, 3> kScaleSteps{100, 150, 200};

Input::Posef ToInput(const XrPosef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef ToXr(const Input::Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

template <std::size_t N>
int NearestStep(const std::array<uint32_t, N>& steps, uint32_t value) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(N); i++) {
        const auto d = [&](int k) {
            return std::abs(static_cast<int>(steps[static_cast<std::size_t>(k)]) -
                            static_cast<int>(value));
        };
        if (d(i) < d(best)) {
            best = i;
        }
    }
    return best;
}

std::string OffsetText(float x, float y) {
    std::string s;
    const auto part = [&s](float v, const char* plus, const char* minus) {
        if (std::fabs(v) < 0.05f) {
            return;
        }
        if (!s.empty()) {
            s += ", ";
        }
        s += fmt::format("{} {:.1f}°", v > 0 ? plus : minus, std::fabs(v));
    };
    part(x, "Right", "Left");
    part(y, "Up", "Down");
    return s.empty() ? std::string{"None"} : s;
}

/// The release's VERSION file beside the program, or "" for a build from source.
std::string ReadVersion() {
    std::error_code ec;
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
    std::ifstream f(exe.parent_path() / "VERSION");
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}

template <typename T, bool R>
void Apply(Settings::SwitchableSetting<T, R>& s, T value, bool per_game) {
    if (per_game) {
        s.SetGlobal(false);
    }
    s.SetValue(value);
}

/// Per-game settings (Azahar's Properties have them too): locked in Global while the game has
/// its own value.
bool PerGameSetting(Id id) {
    switch (id) {
    case Id::HideBottomScreen:
    case Id::ScreenShape:
    case Id::ImmersiveMode:
    case Id::WiderView:
    case Id::GameFov:
    case Id::MatchCamera: // AzaharFrame depth
    case Id::NarrowScreen:
    case Id::DepthFactor:
    case Id::Depth3d: // AzaharFrame depth3d
        return true;
    default:
        return false;
    }
}

bool UsesGlobal(Id id) {
    auto& v = Settings::values;
    switch (id) {
    case Id::HideBottomScreen:
        return v.vr_hide_bottom_screen.UsingGlobal();
    case Id::ScreenShape:
        return v.vr_top_aspect.UsingGlobal();
    case Id::ImmersiveMode:
        return v.vr_immersive_mode.UsingGlobal();
    case Id::WiderView:
        return v.vr_immersive_wider_view_level.UsingGlobal();
    case Id::GameFov:
        return v.vr_immersive_game_fov.UsingGlobal();
    case Id::MatchCamera:
        return v.vr_immersive_match_camera.UsingGlobal();
    case Id::NarrowScreen:
        return v.vr_immersive_squeeze.UsingGlobal();
    case Id::DepthFactor:
        return v.vr_immersive_positional_game_scaler.UsingGlobal();
    case Id::Depth3d:
        return v.factor_3d.UsingGlobal();
    default:
        return true;
    }
}

// AzaharFrame cheatstab: the Mods tab's values from the Qt side (SetInGameMenuCheats and so on).
std::mutex g_mods_mutex;
Menu::ModsTabModel g_mods;
constexpr float kDragSlop = 12.0f; ///< dp the beam moves before a press scrolls the list

} // namespace

void SetInGameMenuCheats(const Menu::CheatsModel& cheats) {
    std::scoped_lock lock{g_mods_mutex};
    g_mods.cheats = cheats;
}

void SetInGameMenuModsList(const Menu::ModsListModel& mods) {
    std::scoped_lock lock{g_mods_mutex};
    g_mods.mods = mods;
}

void SetInGameMenuTextures(const Menu::TexturesModel& textures) {
    std::scoped_lock lock{g_mods_mutex};
    g_mods.textures = textures;
}

struct MenuPanel::Shared {
    std::mutex mutex;
    std::condition_variable cv;
    bool quit = false;
    // A redraw asked for.
    bool job = false;
    uint64_t job_seq = 0;
    Menu::MenuModel job_model;
    Menu::MenuView job_view;
    // The newest drawing.
    uint64_t done_seq = 0;
    std::vector<uint32_t> pixels;
    Menu::MenuLayout layout;
    // The game's title and slots (the Qt side).
    std::string title;
    std::vector<MenuSaveSlot> slots;
    bool per_game = false;
    // The controller drawings of the Controls tab (made on the drawing thread once).
    std::atomic<bool> art_ready{false};
};

MenuPanel::MenuPanel() : shared(std::make_unique<Shared>()) {
    model.version = ReadVersion();
}

MenuPanel::~MenuPanel() {
    DestroyXr();
}

void MenuPanel::Create(Runtime& rt) {
    chain.Create(rt.session, rt.vulkan->Device(), rt.vulkan->PhysicalDevice(),
                 rt.vulkan->QueueFamily(), rt.queue_lock, rt.swapchain_format, kTexW, kTexH);
    posted_any = false;
    uploaded = 0;
    if (!worker.joinable()) {
        {
            std::scoped_lock lock{shared->mutex};
            shared->quit = false;
        }
        worker = std::thread([this] { Worker(); });
    }
    LOG_INFO(Frontend, "xr: in-game menu: {}x{} swapchain", kTexW, kTexH);
}

void MenuPanel::DestroyXr() {
    if (worker.joinable()) {
        {
            std::scoped_lock lock{shared->mutex};
            shared->quit = true;
        }
        shared->cv.notify_all();
        worker.join();
    }
    chain.Destroy();
    open = false;
}

void MenuPanel::SetInfo(const std::string& title, const std::vector<MenuSaveSlot>& slots,
                        bool per_game) {
    std::scoped_lock lock{shared->mutex};
    shared->title = title;
    shared->slots = slots;
    shared->per_game = per_game;
}

void MenuPanel::Open(const XrPosef& at, float width, std::vector<std::pair<std::string, std::string>> rows) {
    pose = at;
    width_m = width;
    stats = std::move(rows);
    open = true;
    view.tab = Menu::Tab::Main;
    view.page = Menu::Page::Tabs;
    view.hover = view.pressed = {};
    view.open_menu = view.tooltip = view.dragging = Id::None;
    Resume();
}

void MenuPanel::Resume() {
    open = true;
    press_hand = -1;
    rest_id = Id::None;
    view.pressed = {};
    // Buttons held from opening (Menu, a trigger, B in the window) click nothing until let go.
    a_down = b_down = true;
    trigger_down = {true, true};
    LOG_INFO(Frontend, "xr: in-game menu: the tab menu shows");
}

void MenuPanel::Close() {
    if (!open) {
        return;
    }
    open = false;
    view.open_menu = view.dragging = Id::None;
    press_hand = -1;
}

void MenuPanel::BuildModel(const Host& host) {
    auto& v = Settings::values;
    Menu::MenuModel m;
    bool per_game_ok;
    {
        std::scoped_lock lock{shared->mutex};
        m.game_title = shared->title;
        per_game_ok = shared->per_game;
        m.quick_load_available = false;
        for (const MenuSaveSlot& s : shared->slots) {
            if (s.slot == 0) {
                m.quick_load_available = true;
            } else {
                m.slots.push_back({s.slot, s.time});
            }
        }
    }
    if (!per_game_ok) {
        scope = 0;
    }
    { // AzaharFrame cheatstab
        std::scoped_lock lock{g_mods_mutex};
        m.mods = g_mods;
    }
    if (m.mods.cheats.download == Menu::DownloadState::Downloading && view.tab == Menu::Tab::Mods &&
        view.mods_segment == Menu::ModsSegment::Cheats) {
        // The progress indicators turn, 15 steps a second (a redraw each step) while they show.
        m.mods.cheats.spin =
            static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now().time_since_epoch())
                                 .count() /
                             66);
    }
    m.hide_bottom_screen = v.vr_hide_bottom_screen.GetValue();
    m.screen_shape = static_cast<int>(std::min<u32>(v.vr_top_aspect.GetValue(), 5));
    m.screen_curve = NearestStep(kCurveSteps, v.vr_screen_curve.GetValue());
    m.screen_layout = static_cast<int>(std::min<u32>(v.vr_screen_layout.GetValue(), 1));
    m.move_screens = static_cast<int>(std::min<u32>(v.vr_move_screens.GetValue(), 2)); // menu
    m.touchpad_touch = static_cast<int>(std::min<u32>(v.vr_touchpad_touch.GetValue(), 2));
    const float ox = v.vr_immersive_offset_x.GetValue(), oy = v.vr_immersive_offset_y.GetValue();
    m.immersive_offset = OffsetText(ox, oy);
    m.immersive_offset_set = std::fabs(ox) >= 0.05f || std::fabs(oy) >= 0.05f;
    m.show_stats = v.vr_show_stats.GetValue();
    m.stats_position = static_cast<int>(std::min<u32>(v.vr_stats_position.GetValue(), 1));
    // The rows of the stats panel: the game's five, the VR app's two, the system's.
    for (std::size_t i = 0; i < stats.size(); i++) {
        (i < 5 ? m.stats_game : i < 7 ? m.stats_vr : m.stats_system).push_back(stats[i]);
    }
    m.version = model.version;
    m.controls_page = controls_page;
    m.scope = scope;
    m.background = static_cast<int>(std::min<u32>(v.vr_background.GetValue(), 1));
    m.face_buttons = static_cast<int>(std::min<u32>(v.vr_face_buttons.GetValue(), 1));
    m.motion_controls = static_cast<int>(std::min<u32>(v.vr_motion_controls.GetValue(), 3));
    m.show_controllers = v.vr_show_controllers.GetValue();
    m.window_scale = NearestStep(kScaleSteps, v.vr_window_scale.GetValue());
    m.depth_3d = static_cast<int>(std::min<u32>(v.factor_3d.GetValue(), 100)); // depth3d
    // AzaharFrame texui: the two switches straight from the settings (the rest is the Qt side's)
    m.mods.textures.custom_textures = v.custom_textures.GetValue();
    m.mods.textures.preload = v.preload_textures.GetValue();
    bool spinning = m.mods.mods.catalog == Menu::CatalogState::Loading;
    for (const auto& o : m.mods.mods.available) {
        spinning |= o.state == Menu::OfferState::Installing;
    }
    if (spinning && view.tab == Menu::Tab::Mods && view.mods_segment == Menu::ModsSegment::Mods) {
        // The progress rings turn, 15 steps a second (a redraw each step) while they show.
        m.mods.mods.spin = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::steady_clock::now().time_since_epoch())
                                                .count() /
                                            66);
    }
    if (m.mods.textures.downloading && view.tab == Menu::Tab::Mods &&
        view.mods_segment == Menu::ModsSegment::Textures &&
        view.textures_page == Menu::TexturesPage::Download) {
        m.mods.textures.spin = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count() /
            66);
    }
    m.immersive_mode = static_cast<int>(std::min<u32>(v.vr_immersive_mode.GetValue(), 3));
    m.wider_view = static_cast<int>(std::min<u32>(v.vr_immersive_wider_view_level.GetValue(), 4));
    m.game_fov = v.vr_immersive_game_fov.GetValue();
    m.match_camera = v.vr_immersive_match_camera.GetValue(); // AzaharFrame depth
    m.narrow_screen = static_cast<int>(std::min<u32>(v.vr_immersive_squeeze.GetValue(), 3));
    m.depth_factor =
        static_cast<int>(std::min<u32>(v.vr_immersive_positional_game_scaler.GetValue(), 2));

    const auto lock = [&m](Id id, Lock l) { m.locked[static_cast<std::size_t>(id)] = l; };
    for (int i = 0; i < Menu::kIdCount; i++) {
        const Id id = static_cast<Id>(i);
        if (PerGameSetting(id) && scope == 0 && !UsesGlobal(id)) {
            lock(id, Lock::GameValue);
        }
    }
    // Settings that are the same in every game stay usable in Per-Game and change the global
    // value (SetSetting); only the per-game ones follow the choice.
    if (!host.curve_ok) {
        lock(Id::ScreenCurve, Lock::Unavailable);
    }
    if (!host.passthrough_ok && m.background == 0) {
        lock(Id::Background, Lock::Unavailable);
    }
    if (!per_game_ok) {
        lock(Id::Scope, Lock::Unavailable);
    }
    model = std::move(m);
}

void MenuPanel::SetSetting(Id id, int value, Host& host) {
    auto& v = Settings::values;
    const bool per_game = scope == 1 && PerGameSetting(id);
    MenuRequest r;
    r.command = MenuCommand::SetSetting;
    r.per_game = per_game;
    r.value = value;
    const u32 u = static_cast<u32>(std::max(value, 0));
    switch (id) {
    case Id::HideBottomScreen:
        Apply(v.vr_hide_bottom_screen, value != 0, per_game);
        r.setting = MenuSetting::HideBottomScreen;
        ShowToastText("Hide Bottom Screen: From the Next Start");
        break;
    case Id::ScreenShape:
        Apply(v.vr_top_aspect, u, per_game);
        r.setting = MenuSetting::ScreenShape;
        break;
    case Id::ScreenCurve:
        r.value = static_cast<int>(kCurveSteps[std::min<std::size_t>(u, kCurveSteps.size() - 1)]);
        Apply(v.vr_screen_curve, static_cast<u32>(r.value), false);
        r.setting = MenuSetting::ScreenCurve;
        break;
    case Id::ScreenLayout: // AzaharFrame layout, global
        Apply(v.vr_screen_layout, std::min<u32>(u, 1), false);
        r.setting = MenuSetting::ScreenLayout;
        break;
    case Id::MoveScreens: // AzaharFrame menu, global
        Apply(v.vr_move_screens, std::min<u32>(u, 2), false);
        r.setting = MenuSetting::MoveScreens;
        ShowToastText(Menu::LabelFor(id) + ": " + Menu::OptionsFor(id)[std::min<u32>(u, 2)]);
        break;
    case Id::TouchpadTouch: // AzaharFrame menu, global
        Apply(v.vr_touchpad_touch, std::min<u32>(u, 2), false);
        r.setting = MenuSetting::TouchpadTouch;
        break;
    case Id::CustomTextures: // AzaharFrame texui: the game's own value when it has one
        r.per_game = !v.custom_textures.UsingGlobal();
        Apply(v.custom_textures, value != 0, r.per_game);
        r.setting = MenuSetting::CustomTextures;
        break;
    case Id::TexturePreload:
        r.per_game = !v.preload_textures.UsingGlobal();
        Apply(v.preload_textures, value != 0, r.per_game);
        r.setting = MenuSetting::PreloadTextures;
        break;
    case Id::ImmersiveOffsetReset:
        Apply(v.vr_immersive_offset_x, 0.0f, false);
        Apply(v.vr_immersive_offset_y, 0.0f, false);
        r.setting = MenuSetting::ImmersiveOffsetReset;
        break;
    case Id::ShowStats:
        Apply(v.vr_show_stats, value != 0, false);
        r.setting = MenuSetting::ShowStats;
        break;
    case Id::StatsPosition: // AzaharFrame statspos, global
        Apply(v.vr_stats_position, std::min<u32>(u, 1), false);
        r.setting = MenuSetting::StatsPosition;
        break;
    case Id::Background:
        Apply(v.vr_background, u, false);
        r.setting = MenuSetting::Background;
        break;
    case Id::FaceButtons:
        Apply(v.vr_face_buttons, u, false);
        r.setting = MenuSetting::FaceButtons;
        break;
    case Id::MotionControls:
        Apply(v.vr_motion_controls, u, false);
        r.setting = MenuSetting::MotionControls;
        break;
    case Id::ShowControllers:
        Apply(v.vr_show_controllers, value != 0, false);
        r.setting = MenuSetting::ShowControllers;
        break;
    case Id::WindowScale:
        r.value = static_cast<int>(kScaleSteps[std::min<std::size_t>(u, kScaleSteps.size() - 1)]);
        Apply(v.vr_window_scale, static_cast<u32>(r.value), false);
        r.setting = MenuSetting::WindowScale;
        break;
    case Id::ImmersiveMode:
        Apply(v.vr_immersive_mode, u, per_game);
        r.setting = MenuSetting::ImmersiveMode;
        break;
    case Id::WiderView:
        Apply(v.vr_immersive_wider_view_level, u, per_game);
        r.setting = MenuSetting::WiderView;
        break;
    case Id::GameFov:
        Apply(v.vr_immersive_game_fov, static_cast<s32>(value), per_game);
        r.setting = MenuSetting::GameFov;
        break;
    case Id::MatchCamera: // AzaharFrame depth: live
        Apply(v.vr_immersive_match_camera, value != 0, per_game);
        r.setting = MenuSetting::MatchCamera;
        break;
    case Id::NarrowScreen:
        Apply(v.vr_immersive_squeeze, u, per_game);
        r.setting = MenuSetting::NarrowScreen;
        ShowToastText("Narrow Screen: From the Next Start");
        break;
    case Id::DepthFactor:
        Apply(v.vr_immersive_positional_game_scaler, u, per_game);
        r.setting = MenuSetting::DepthFactor;
        break;
    case Id::Depth3d: // AzaharFrame depth3d: the HID's 3D slider reads it on every pad update
        Apply(v.factor_3d, std::min<u32>(u, 100), per_game);
        r.setting = MenuSetting::Depth3d;
        break;
    default:
        return;
    }
    LOG_INFO(Frontend, "xr: in-game menu: {} = {}{}", Menu::LabelFor(id), r.value,
             per_game ? " (this game)" : "");
    if (host.request) {
        host.request(r);
    }
}

void MenuPanel::Click(Target t, Host& host) {
    const auto ask = [&host](MenuCommand c, int value = 0) {
        MenuRequest r;
        r.command = c;
        r.value = value;
        if (host.request) {
            host.request(r);
        }
    };
    const auto ask_text = [&host](MenuCommand c, const std::string& text, int value = 0) {
        MenuRequest r; // AzaharFrame texui: a pack's or a mod's id
        r.command = c;
        r.value = value;
        r.text = text;
        if (host.request) {
            host.request(r);
        }
    };
    // A click anywhere but on the open menu's items closes it (and does nothing else).
    if (view.open_menu != Id::None) {
        const Id menu = view.open_menu;
        view.open_menu = Id::None;
        if (t.id == menu && t.index >= 0) {
            SetSetting(menu, t.index, host);
        }
        return;
    }
    if (t.index == Menu::kLabelIndex || t.index == Menu::kMenuAreaIndex) {
        return;
    }
    static constexpr std::array<const char*, 6> kTabNames{"Main",     "Position", "Stats",
                                                          "Controls", "Mods",     "Settings"};
    static constexpr std::array<const char*, 3> kSegmentNames{"Cheats", "Mods", "Textures"};
    if (t.id == Id::TabButton) {
        LOG_INFO(Frontend, "xr: in-game menu: {} tab",
                 kTabNames[static_cast<std::size_t>(std::clamp(t.index, 0, 5))]);
    } else if (t.id == Id::ModsSegment) { // AzaharFrame cheatstab
        LOG_INFO(Frontend, "xr: in-game menu: Mods tab: {}",
                 kSegmentNames[static_cast<std::size_t>(std::clamp(t.index, 0, 2))]);
    } else {
        LOG_INFO(Frontend, "xr: in-game menu: {}{}", Menu::LabelFor(t.id),
                 t.index >= 0 ? fmt::format(" ({})", t.index) : std::string{});
    }
    switch (t.id) {
    case Id::TabButton:
        view.tab = static_cast<Menu::Tab>(t.index);
        view.page = Menu::Page::Tabs;
        break;
    case Id::Continue:
        ask(MenuCommand::Continue);
        break;
    case Id::QuickSave:
        ask(MenuCommand::QuickSave);
        break;
    case Id::QuickLoad:
        ask(MenuCommand::QuickLoad);
        break;
    case Id::SaveState:
        view.page = Menu::Page::SaveSlots;
        break;
    case Id::LoadState:
        view.page = Menu::Page::LoadSlots;
        break;
    case Id::SlotsBack:
        view.page = Menu::Page::Tabs;
        break;
    case Id::Slot:
        ask(view.page == Menu::Page::SaveSlots ? MenuCommand::SaveSlot : MenuCommand::LoadSlot,
            t.index);
        view.page = Menu::Page::Tabs;
        break;
    case Id::Screenshot:
        ask(MenuCommand::Screenshot);
        break;
    case Id::Restart:
        ask(MenuCommand::Restart);
        break;
    case Id::AzaharWindow:
        ask(MenuCommand::ShowWindow);
        break;
    case Id::SwitchToFlat:
        ask(MenuCommand::SwitchToFlat);
        break;
    case Id::CloseGame:
        ask(MenuCommand::CloseGame);
        break;
    case Id::MoreSettings:
        ask(MenuCommand::MoreSettings);
        break;
    // AzaharFrame cheatstab: the Mods tab and its Cheats segment
    case Id::ModsSegment:
        if (t.index >= 0 && t.index <= static_cast<int>(Menu::ModsSegment::Textures) &&
            static_cast<Menu::ModsSegment>(t.index) != view.mods_segment) {
            view.mods_segment = static_cast<Menu::ModsSegment>(t.index);
            view.scroll[static_cast<std::size_t>(Menu::Tab::Mods)] = 0.0f;
        }
        break;
    case Id::CheatsWindow:
        ask(MenuCommand::Cheats);
        break;
    case Id::DownloadCheats:
        ask(MenuCommand::DownloadCheats);
        break;
    case Id::Cheat:
        if (t.index >= 0) {
            ask(MenuCommand::ToggleCheat, t.index);
        }
        break;
    // AzaharFrame texui: the Mods tab's Mods and Textures segments (citra_qt/frame_mods.cpp)
    case Id::CustomTextures:
        SetSetting(t.id, model.mods.textures.custom_textures ? 0 : 1, host);
        break;
    case Id::TexturePreload:
        SetSetting(t.id, model.mods.textures.preload ? 0 : 1, host);
        break;
    case Id::TexturePack:
        if (t.index >= 0 && t.index <= static_cast<int>(model.mods.textures.packs.size())) {
            ask_text(MenuCommand::SelectTexturePack,
                     t.index > 0 ? model.mods.textures.packs[static_cast<std::size_t>(t.index - 1)].id
                                 : std::string{});
        }
        break;
    case Id::PackDelete: // "Delete <name>?" first (the Qt side asks)
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.packs.size())) {
            ask_text(MenuCommand::DeletePack,
                     model.mods.textures.packs[static_cast<std::size_t>(t.index)].id);
        }
        break;
    case Id::DownloadPacks:
        view.textures_page = Menu::TexturesPage::Download;
        view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
        ask(MenuCommand::LoadPackDownloads);
        break;
    case Id::TexturesBack:
        view.textures_page = Menu::TexturesPage::Packs;
        view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
        break;
    case Id::PackDownload:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.downloads.size())) {
            ask_text(MenuCommand::DownloadPack,
                     model.mods.textures.downloads[static_cast<std::size_t>(t.index)].id);
        }
        break;
    case Id::PackCancel:
        ask(MenuCommand::CancelPackDownload);
        break;
    case Id::PackLink: // "Open <host>?" first
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.downloads.size())) {
            const std::string& url =
                model.mods.textures.downloads[static_cast<std::size_t>(t.index)].url;
            if (!Menu::LinkText(url).empty()) {
                view.open_link = url;
            }
        }
        break;
    case Id::RestartGame:
        ask(MenuCommand::Restart);
        break;
    case Id::InstallMods:
        view.mods_page = Menu::ModsPage::Install;
        view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
        ask(MenuCommand::LoadModCatalog);
        break;
    case Id::ModsBack:
        view.mods_page = Menu::ModsPage::Loaded;
        view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
        break;
    case Id::Mod:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.mods.loaded.size())) {
            const auto& row = model.mods.mods.loaded[static_cast<std::size_t>(t.index)];
            ask_text(MenuCommand::SetModActive, row.id, row.active ? 0 : 1);
        }
        break;
    case Id::ModRemove:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.mods.loaded.size())) {
            ask_text(MenuCommand::RemoveMod,
                     model.mods.mods.loaded[static_cast<std::size_t>(t.index)].id);
        }
        break;
    case Id::ModInstall:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.mods.available.size())) {
            ask_text(MenuCommand::InstallMod,
                     model.mods.mods.available[static_cast<std::size_t>(t.index)].id);
        }
        break;
    case Id::ModLink: { // "Open GitHub?" first
        const auto& mm = model.mods.mods;
        const bool install = view.mods_page == Menu::ModsPage::Install;
        const std::size_t n = install ? mm.available.size() : mm.loaded.size();
        if (t.index >= 0 && static_cast<std::size_t>(t.index) < n) {
            const std::string& url = install ? mm.available[static_cast<std::size_t>(t.index)].url
                                             : mm.loaded[static_cast<std::size_t>(t.index)].url;
            if (!Menu::GithubLinkText(url).empty()) {
                view.open_link = url;
            }
        }
        break;
    }
    case Id::DialogConfirm:
    case Id::DialogCancel: {
        // The segment's own dialog (the Qt side's), else the link's (the view's).
        const bool textures = view.mods_segment == Menu::ModsSegment::Textures;
        const auto& dialog = textures ? model.mods.textures.dialog : model.mods.mods.dialog;
        const bool yes = t.id == Id::DialogConfirm;
        if (dialog.kind == Menu::ModsDialogKind::None && !view.open_link.empty()) {
            const std::string url = std::exchange(view.open_link, {});
            if (yes) {
                ask_text(MenuCommand::OpenUrl, url);
            }
        } else {
            ask(textures ? MenuCommand::TexturesDialog : MenuCommand::ModsDialog, yes ? 1 : 0);
        }
        break;
    }
    case Id::EditControls: // AzaharFrame menu: the remap lane's
        ask(MenuCommand::EditControls);
        break;
    case Id::ResetPosition:
        if (host.reset_position) {
            host.reset_position();
        }
        break;
    case Id::SwapScreens:
        if (host.swap_screens) {
            host.swap_screens();
        }
        break;
    case Id::HideBottomScreen:
        SetSetting(t.id, model.hide_bottom_screen ? 0 : 1, host);
        break;
    case Id::ShowStats:
        SetSetting(t.id, model.show_stats ? 0 : 1, host);
        break;
    case Id::ShowControllers:
        SetSetting(t.id, model.show_controllers ? 0 : 1, host);
        break;
    case Id::MatchCamera: // AzaharFrame depth
        SetSetting(t.id, model.match_camera ? 0 : 1, host);
        break;
    case Id::ImmersiveOffsetReset:
        SetSetting(t.id, 0, host);
        break;
    case Id::ScreenShape:
    case Id::ScreenCurve:
    case Id::ImmersiveMode:
        view.open_menu = t.id; // a menu button: its menu
        break;
    case Id::ControlsPage:
        controls_page = t.index;
        break;
    case Id::Scope:
        scope = t.index;
        break;
    case Id::ScreenLayout:
    case Id::MoveScreens:
    case Id::TouchpadTouch:
    case Id::StatsPosition:
    case Id::Background:
    case Id::FaceButtons:
    case Id::MotionControls:
    case Id::WindowScale:
    case Id::WiderView:
    case Id::NarrowScreen:
    case Id::DepthFactor:
        if (t.index >= 0) {
            SetSetting(t.id, t.index, host);
        }
        break;
    default:
        break;
    }
}

void MenuPanel::Frame(const FrameInfo& info, const Input::InputFrame& in, bool have_input,
                      Host& host, std::vector<Quad>& layers, std::optional<XrPosef>& cursor) {
    if (!open) {
        return;
    }
    const XrTime now = info.display_time;
    const float dt =
        last_time ? std::min(0.1f, static_cast<float>(now - last_time) * 1e-9f) : 0.0f;
    last_time = now;
    chain.Poll();
    BuildModel(host);

    // ---- The beams ----
    const Input::PanelQuad quad{ToInput(pose), width_m, Height(), true};
    std::array<Input::QuadHit, 2> hit{};
    std::array<bool, 2> edge{};
    for (int h = 0; h < Input::kNumHands; h++) {
        const auto& hand = in.hand[static_cast<std::size_t>(h)];
        const bool down = have_input && hand.trigger > (trigger_down[h] ? kLetGo : kPress);
        edge[h] = down && !trigger_down[h];
        trigger_down[h] = down;
        if (have_input && hand.active) {
            hit[h] = Input::IntersectQuad(Input::RayFromPose(hand.aim), quad);
        }
        if (edge[h] && hit[h].in_bounds) {
            active_hand = h;
        }
    }
    int p = -1;
    if (press_hand >= 0) {
        p = press_hand;
    } else if (hit[active_hand].in_bounds) {
        p = active_hand;
    } else if (hit[1 - active_hand].in_bounds) {
        p = 1 - active_hand;
    }
    Target hover{};
    float x = 0.0f, y = 0.0f;
    if (p >= 0 && hit[p].on_plane) {
        x = hit[p].u * Menu::kPanelW;
        y = hit[p].v * Menu::kPanelH;
        if (hit[p].in_bounds) {
            hover = layout.HitTest(x, y);
            Input::Posef dot = ToInput(pose);
            dot.position = hit[p].point_world;
            cursor = ToXr(dot);
        }
    }
    // A (right controller) clicks too, for the hand that points.
    const bool a = have_input && in.a;
    const bool a_edge = a && !a_down;
    a_down = a;
    const bool b = have_input && in.b;
    const bool b_edge = b && !b_down;
    b_down = b;

    // ---- Press, drag, release ----
    if (press_hand < 0 && p >= 0 && (edge[p] || a_edge) && hover.id != Id::None) {
        press_hand = p;
        view.pressed = hover;
        drag_y0 = y; // AzaharFrame cheatstab: a press that moves scrolls the list
        drag_scroll0 = view.scroll[static_cast<std::size_t>(view.tab)];
        beam_drag = false;
        if (Menu::SliderFor(hover.id).max > 1.0f && hover.index != Menu::kLabelIndex &&
            view.open_menu == Id::None) {
            view.dragging = hover.id;
        }
    }
    if (press_hand >= 0) {
        if (view.dragging != Id::None) {
            const float t = layout.SliderAt(view.dragging, x);
            if (t >= 0.0f) {
                view.drag_t = t;
            }
        } else if (view.open_menu == Id::None && layout.max_scroll > 0.0f && p == press_hand &&
                   hit[p].on_plane && (beam_drag || std::fabs(y - drag_y0) > kDragSlop)) {
            // AzaharFrame cheatstab: dragged, the list follows the beam and nothing is clicked.
            beam_drag = true;
            view.pressed = {};
            view.scroll[static_cast<std::size_t>(view.tab)] =
                std::clamp(drag_scroll0 - (y - drag_y0), 0.0f, layout.max_scroll);
        }
        const bool let_go = !trigger_down[press_hand] && !a_down; // both let go
        if (let_go) {
            if (beam_drag) {
                // Settles on a whole row, as the stick scrolls.
                beam_drag = false;
                float& s = view.scroll[static_cast<std::size_t>(view.tab)];
                const float step = std::max(1.0f, layout.scroll_step);
                s = std::clamp(std::round(s / step) * step, 0.0f, layout.max_scroll);
            } else if (view.dragging != Id::None) {
                const Menu::SliderRange r = Menu::SliderFor(view.dragging);
                const float value =
                    r.min + std::round(view.drag_t * (r.max - r.min) / r.step) * r.step;
                SetSetting(view.dragging, static_cast<int>(value), host);
                view.dragging = Id::None;
            } else if (hover == view.pressed) {
                Click(hover, host);
            } else if (view.open_menu != Id::None && hover.id == Id::None) {
                view.open_menu = Id::None; // pressed off the menu
            }
            view.pressed = {};
            press_hand = -1;
        }
    } else if (p < 0 && (edge[0] || edge[1] || a_edge) && view.open_menu != Id::None) {
        view.open_menu = Id::None; // a click beside the panel closes a drop-down menu
    }
    view.hover = hover;

    // ---- B: back ----
    if (b_edge) {
        // AzaharFrame texui: the Mods segment's dialog (Cancel), then Install Mods (back).
        // The Textures segment's the same way: its dialog, then Download Packs.
        const bool mods_shown = view.tab == Menu::Tab::Mods &&
                                view.mods_segment == Menu::ModsSegment::Mods;
        const bool textures_shown = view.tab == Menu::Tab::Mods &&
                                    view.mods_segment == Menu::ModsSegment::Textures;
        if (view.open_menu != Id::None) {
            view.open_menu = Id::None;
        } else if (mods_shown && (model.mods.mods.dialog.kind != Menu::ModsDialogKind::None ||
                                  !view.open_link.empty())) {
            Click({Id::DialogCancel}, host);
        } else if (textures_shown &&
                   (model.mods.textures.dialog.kind != Menu::ModsDialogKind::None ||
                    !view.open_link.empty())) {
            Click({Id::DialogCancel}, host);
        } else if (mods_shown && view.mods_page == Menu::ModsPage::Install) {
            Click({Id::ModsBack}, host);
        } else if (textures_shown && view.textures_page == Menu::TexturesPage::Download) {
            Click({Id::TexturesBack}, host);
        } else if (view.page != Menu::Page::Tabs) {
            view.page = Menu::Page::Tabs;
        } else {
            LOG_INFO(Frontend, "xr: in-game menu: B continues the game");
            Click({Id::Continue}, host);
        }
    }

    // ---- Sticks: scroll a row at a time ----
    float sy = 0.0f;
    if (have_input) {
        for (const auto& hand : in.hand) {
            if (std::fabs(hand.stick.y) > std::fabs(sy)) {
                sy = hand.stick.y;
            }
        }
    }
    const int dir = sy > kStickOn ? -1 : sy < -kStickOn ? 1 : 0; // stick up: towards the top
    if (dir != stick_dir) {
        stick_dir = dir;
        stick_hold = 0.0f;
        stick_repeat = 0.0f;
        if (dir != 0) {
            stick_repeat = kStickFirstRepeat;
            float& s = view.scroll[static_cast<std::size_t>(view.tab)];
            s = std::clamp(s + layout.scroll_step * static_cast<float>(dir), 0.0f,
                           layout.max_scroll);
        }
    } else if (dir != 0) {
        stick_hold += dt;
        if (stick_hold >= stick_repeat) {
            stick_repeat += kStickRepeat;
            float& s = view.scroll[static_cast<std::size_t>(view.tab)];
            s = std::clamp(s + layout.scroll_step * static_cast<float>(dir), 0.0f,
                           layout.max_scroll);
        }
    }
    {
        float& s = view.scroll[static_cast<std::size_t>(view.tab)];
        s = std::clamp(s, 0.0f, layout.max_scroll);
    }

    // ---- The tooltip: a beam resting on a setting ----
    const Id rest = hover.id;
    if (rest != rest_id) {
        rest_id = rest;
        rest_s = 0.0f;
        view.tooltip = Id::None;
    } else if (rest != Id::None && view.pressed.id == Id::None) {
        rest_s += dt;
        const bool has_text =
            !Menu::ExplainFor(rest).empty() ||
            model.locked[static_cast<std::size_t>(rest)] != Lock::None;
        if (rest_s >= kTooltipRest && has_text) {
            view.tooltip = rest;
        }
    }
    if (view.pressed.id != Id::None) {
        view.tooltip = Id::None;
    }

    // ---- A redraw when anything changed; the newest drawing to the swapchain ----
    if (chain.Valid() && (!posted_any || !(model == posted_model) || !(view == posted_view) ||
                          (view.tab == Menu::Tab::Controls && shared->art_ready.exchange(false)))) {
        posted_any = true;
        posted_model = model;
        posted_view = view;
        {
            std::scoped_lock lock{shared->mutex};
            shared->job = true;
            shared->job_seq = ++posted;
            shared->job_model = model;
            shared->job_view = view;
        }
        shared->cv.notify_one();
    }
    {
        std::scoped_lock lock{shared->mutex};
        if (shared->done_seq != uploaded && !shared->pixels.empty() && chain.Valid()) {
            uploaded = shared->done_seq;
            chain.Upload(reinterpret_cast<const uint8_t*>(shared->pixels.data()));
            layout = shared->layout;
            stale_uploaded = true; // AzaharFrame kbd
        }
    }
    if (stale && stale_uploaded && !chain.Busy()) {
        stale = false; // AzaharFrame kbd: the keyboard's pixels are gone
    }
    if (chain.HasImage() && info.should_render && !stale) {
        Quad q;
        q.swapchain = chain.Handle();
        q.image_rect = {{0, 0}, {kTexW, kTexH}};
        q.pose = pose;
        q.size = {width_m, Height()};
        q.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        layers.push_back(q);
    }
}

void MenuPanel::Worker() {
    Menu::Kit kit(kPxPerDp);
    Menu::TabMenu menu;
    Vis::MenuDiagramArt art;
    bool art_tried = false;
    Vis::HelpBindings bindings_drawn;
    bool face_drawn = true;
    Vis::HelpDiagram diagram;
    bool have_diagram = false;
    menu.SetDiagram([&](Menu::Kit& k, const Menu::Rect& area, int page) {
        if (have_diagram) {
            Vis::DrawMenuDiagram(k, area, diagram, static_cast<Vis::DiagramPageId>(page), art);
        }
    });
    frame_xr::ui::Canvas canvas;
    while (true) {
        Menu::MenuModel m;
        Menu::MenuView v;
        uint64_t seq;
        {
            std::unique_lock lock{shared->mutex};
            shared->cv.wait(lock, [this] { return shared->quit || shared->job; });
            if (shared->quit) {
                return;
            }
            shared->job = false;
            seq = shared->job_seq;
            m = shared->job_model;
            v = shared->job_view;
        }
        if (v.tab == Menu::Tab::Controls) {
            // The diagram's labels follow the controls profile and Face Buttons.
            const Vis::HelpBindings bindings = Vis::CurrentHelpBindings();
            const bool face = m.face_buttons == 0;
            const bool same = bindings.buttons == bindings_drawn.buttons &&
                              bindings.analogs == bindings_drawn.analogs &&
                              bindings.valid == bindings_drawn.valid;
            if (!have_diagram || !same || face != face_drawn) {
                diagram = Vis::BuildHelpDiagram(
                    bindings, {.face_by_position = face, .in_menu = true, .game_running = true});
                bindings_drawn = bindings;
                face_drawn = face;
                have_diagram = true;
            }
        }
        const auto t0 = std::chrono::steady_clock::now();
        canvas.Init(kTexW, kTexH);
        kit.Begin(canvas);
        menu.Draw(kit, m, v);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
        {
            std::scoped_lock lock{shared->mutex};
            shared->pixels.swap(canvas.px);
            shared->layout = menu.Layout();
            shared->done_seq = seq;
        }
        static bool logged = false;
        if (!logged) {
            logged = true;
            LOG_INFO(Frontend, "xr: in-game menu: first drawing in {} ms", ms);
        }
        if (!art_tried) {
            // The Controls tab's drawings, once, after the first drawing (a few hundred ms).
            art_tried = true;
            std::array<Vis::ModelFile, 2> files;
            std::string error;
            if (Vis::ReadControllerModels(files, error)) {
                art = Vis::MakeMenuDiagramArt(files[0], files[1], kPxPerDp * 0.6f);
                shared->art_ready = true;
            } else {
                LOG_WARNING(Frontend, "xr: in-game menu: no controller drawings: {}", error);
            }
        }
    }
}

} // namespace FrameXr
