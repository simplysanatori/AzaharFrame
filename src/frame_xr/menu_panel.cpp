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
#include "common/param_package.h"
#include "common/settings.h"
#include "frame_xr/dynamic_resolution.h" // AzaharFrame menuux
#include "frame_xr/immersive.h"          // AzaharFrame fovonce: MeasureCameraAgain
#include "frame_xr/immersive_layout.h"   // AzaharFrame kiu: Horizontal and Vertical Scale
#include "frame_xr/input/default_bindings.h"
#include "frame_xr/input/devices.h" // AzaharFrame pads
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/gyro_aim.h" // AzaharFrame kiugyro
#include "frame_xr/input/reticle.h" // AzaharFrame reticle: Reticle Speed
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu_panel.h"
#include "frame_xr/per_game.h" // AzaharFrame useglobal
#include "frame_xr/session.h"
#include "frame_xr/ui_sound.h" // AzaharFrame menuux: Edit Controls' click
#include "frame_xr/vis/controller_models.h"
#include "frame_xr/vis/help_content.h"
#include "frame_xr/vis/menu_diagram.h"
#include "frame_xr/vis/visuals.h"
#include "input_common/frame_xr.h" // AzaharFrame pads: the mouse for the Pointer Test

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

// AzaharFrame padsui: the menu's 3DS button indices are Settings::NativeButton's.
static_assert(Menu::DsButton::A == Settings::NativeButton::A &&
              Menu::DsButton::Select == Settings::NativeButton::Select &&
              Menu::DsButton::ZL == Settings::NativeButton::ZL &&
              Menu::DsButton::ZR == Settings::NativeButton::ZR &&
              Menu::DsButton::Home == Settings::NativeButton::Home &&
              Menu::kRemapButtons == Settings::NativeButton::NumButtons);

// AzaharFrame remap: the controls profile's 3DS buttons as a table (the Frame control driving
// each), and back. Only buttons bound to the frame_xr engine are in the table; the others are left
// as they are.
// AzaharFrame padsui: the mouse's mapping holds MouseInputs, as the menu's table does.
Menu::RemapTable MouseTableOf(const Input::GamepadMapping& m) {
    Menu::RemapTable t = Menu::kNoMapping;
    for (std::size_t i = 0; i < t.size() && i < m.buttons.size(); i++) {
        t[i] = m.buttons[i];
    }
    return t;
}

// AzaharFrame keys: the keyboard's stick keys and modifier in the menu's terms, and back.
Menu::RemapKeys KeysOf(const Input::GamepadMapping& m) {
    Menu::RemapKeys k = Menu::kNoKeys;
    for (std::size_t i = 0; i < m.stick_keys.size(); i++) {
        k[i] = m.stick_keys[i];
    }
    k[Menu::kModifierSlot] = m.modifier;
    return k;
}

void SetKeys(Input::GamepadMapping& m, const Menu::RemapKeys& k) {
    for (std::size_t i = 0; i < m.stick_keys.size(); i++) {
        m.stick_keys[i] = k[i];
    }
    m.modifier = k[Menu::kModifierSlot];
}

Menu::RemapTable TableOf(const std::array<std::string, Menu::kRemapButtons>& buttons) {
    Menu::RemapTable t;
    t.fill(-1);
    for (std::size_t i = 0; i < buttons.size(); i++) {
        const Common::ParamPackage p(buttons[i]);
        if (p.Get("engine", "") == "frame_xr" && p.Has("button")) {
            t[i] = static_cast<int8_t>(p.Get("button", -1));
        }
    }
    return t;
}

void ApplyTable(std::array<std::string, Menu::kRemapButtons>& buttons, const Menu::RemapTable& now,
                const Menu::RemapTable& before) {
    for (std::size_t i = 0; i < buttons.size(); i++) {
        if (now[i] == before[i]) {
            continue;
        }
        if (now[i] < 0) {
            buttons[i].clear();
            continue;
        }
        Common::ParamPackage p(buttons[i]);
        if (p.Get("engine", "") != "frame_xr") {
            p = Common::ParamPackage{};
            p.Set("engine", "frame_xr");
        }
        p.Set("button", static_cast<int>(now[i]));
        buttons[i] = p.Serialize();
    }
}

// AzaharFrame padsui: the profile's Circle Pad and C-Stick as stick routes (stick_route.h), and
// back. A stick bound to another engine is -1 and left as it is.
Menu::RemapSticks SticksOf(const std::array<std::string, 2>& analogs) {
    Menu::RemapSticks t{-1, -1};
    for (std::size_t i = 0; i < analogs.size(); i++) {
        const Common::ParamPackage p(analogs[i]);
        if (p.Get("engine", "") == "frame_xr" && p.Has("analog")) {
            Input::StickRoute r;
            r.stick = p.Get("analog", -1);
            r.swap_xy = p.Get("swap_xy", 0) != 0;
            r.invert_x = p.Get("invert_x", 0) != 0;
            r.invert_y = p.Get("invert_y", 0) != 0;
            t[i] = Input::EncodeStickRoute(r);
        }
    }
    return t;
}

void ApplySticks(std::array<std::string, 2>& analogs, const Menu::RemapSticks& now,
                 const Menu::RemapSticks& before) {
    for (std::size_t i = 0; i < analogs.size(); i++) {
        if (now[i] == before[i] || now[i] < 0) {
            continue;
        }
        const Input::StickRoute r = Input::DecodeStickRoute(now[i]);
        Common::ParamPackage p;
        p.Set("engine", "frame_xr");
        p.Set("analog", r.stick);
        if (r.swap_xy) {
            p.Set("swap_xy", 1);
        }
        if (r.invert_x) {
            p.Set("invert_x", 1);
        }
        if (r.invert_y) {
            p.Set("invert_y", 1);
        }
        analogs[i] = p.Serialize();
    }
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

// AzaharFrame useglobal: the per-game rows (Menu::PerGameSetting) and their values against the
// settings (per_game.h).
using Menu::PerGameSetting;
using PerGame::UsesGlobal;

/// AzaharFrame useglobal: the Qt side's name for a per-game row's setting.
MenuSetting SettingOf(Id id) {
    switch (id) {
    case Id::HideBottomScreen:
        return MenuSetting::HideBottomScreen;
    case Id::ScreenShape:
        return MenuSetting::ScreenShape;
    case Id::ImmersiveMode:
        return MenuSetting::ImmersiveMode;
    case Id::WiderView:
        return MenuSetting::WiderView;
    case Id::GameFov:
        return MenuSetting::GameFov;
    case Id::MatchCamera:
        return MenuSetting::MatchCamera;
    case Id::ScaleX:
        return MenuSetting::ScaleX;
    case Id::ScaleY:
        return MenuSetting::ScaleY;
    case Id::ScaleMode: // AzaharFrame fovscale
        return MenuSetting::ScaleMode;
    case Id::WorldScale:
        return MenuSetting::WorldScale;
    case Id::FollowScreen: // AzaharFrame natural
        return MenuSetting::FollowScreen;
    case Id::Depth3d:
        return MenuSetting::Depth3d;
    case Id::InternalResolution:
        return MenuSetting::InternalResolution;
    case Id::TextureFilter:
        return MenuSetting::TextureFilter;
    case Id::AsyncShaders:
        return MenuSetting::AsyncShaders;
    case Id::AccurateImageCopies: // AzaharFrame accxfer
        return MenuSetting::AccurateImageCopies;
    case Id::FastForwardAudio: // AzaharFrame ffaudio
        return MenuSetting::FastForwardAudio;
    case Id::Autosave:
        return MenuSetting::Autosave;
    case Id::AutosaveEvery:
        return MenuSetting::AutosaveEvery;
    case Id::TouchpadTouch: // AzaharFrame reticle: the Controls tab's rows
        return MenuSetting::TouchpadTouch;
    case Id::BottomScreenReticle:
        return MenuSetting::BottomScreenReticle;
    case Id::ReticleAtEdge:
        return MenuSetting::ReticleAtEdge;
    case Id::ReticleSpeed:
        return MenuSetting::ReticleSpeed;
    case Id::ReticleCenter: // AzaharFrame fixes7
        return MenuSetting::ReticleCenter;
    case Id::ReticleStick:
        return MenuSetting::ReticleStick;
    case Id::ReticleEdgeCStick: // AzaharFrame kiu
        return MenuSetting::ReticleEdgeCStick;
    case Id::MotionControls:
        return MenuSetting::MotionControls;
    case Id::GyroAim: // AzaharFrame kiugyro
        return MenuSetting::GyroAim;
    case Id::GyroSensitivity:
        return MenuSetting::GyroSensitivity;
    case Id::TurnZone:
        return MenuSetting::TurnZone;
    case Id::TurnSpeed:
        return MenuSetting::TurnSpeed;
    case Id::Recoil:
        return MenuSetting::Recoil;
    case Id::AutosaveKeep:
    default: // UseGlobal checks PerGameSetting first
        return MenuSetting::AutosaveKeep;
    }
}

/// AzaharFrame zipinstall: Install from Zip's page shows (over the segment and page it came from).
bool ZipShown(const Menu::MenuModel& model, const Menu::MenuView& view) {
    const auto& zip = model.mods.zip;
    if (!zip.open || view.tab != Menu::Tab::Mods) {
        return false;
    }
    if (zip.target == Menu::ZipTarget::Cheats) { // AzaharFrame ui2
        return view.mods_segment == Menu::ModsSegment::Cheats;
    }
    if (zip.target == Menu::ZipTarget::Textures) {
        return view.mods_segment == Menu::ModsSegment::Textures && // AzaharFrame ui2
               view.textures_page == Menu::TexturesPage::Download;
    }
    return view.mods_segment == Menu::ModsSegment::Mods && view.mods_page == Menu::ModsPage::Install;
}

// AzaharFrame cheatstab: the Mods tab's values from the Qt side (SetInGameMenuCheats and so on).
std::mutex g_mods_mutex;
Menu::ModsTabModel g_mods;
Menu::HomebrewTabModel g_homebrew; // AzaharFrame emerald, under g_mods_mutex

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

void SetInGameMenuZip(const Menu::ZipModel& zip) { // AzaharFrame zipinstall
    std::scoped_lock lock{g_mods_mutex};
    g_mods.zip = zip;
}

void SetInGameMenuCtgp7(const Menu::Ctgp7Model& ctgp7) { // AzaharFrame ctgp7
    std::scoped_lock lock{g_mods_mutex};
    g_mods.ctgp7 = ctgp7;
}

Menu::ModsDialog g_notice; // AzaharFrame savestate, under g_mods_mutex
// AzaharFrame autosave: Load State's card and its list, and the Undo Load snackbar's text
std::vector<Menu::AutosaveCard> g_autosaves; // under g_mods_mutex
std::string g_undo_load;

void SetInGameMenuAutosaves(const std::vector<Menu::AutosaveCard>& autosaves,
                            const std::string& undo_load) {
    std::scoped_lock lock{g_mods_mutex};
    g_autosaves = autosaves;
    g_undo_load = undo_load;
}

void SetInGameMenuNotice(const std::string& title, const std::string& body,
                         const std::string& confirm, const std::string& cancel,
                         const std::string& extra) {
    std::scoped_lock lock{g_mods_mutex};
    g_notice = {};
    if (!title.empty()) {
        g_notice.kind = Menu::ModsDialogKind::Notice;
        g_notice.mod = title;
        g_notice.other = body;
        g_notice.confirm = confirm;
        g_notice.cancel = cancel;
        g_notice.extra = extra; // AzaharFrame memwarn
    }
}

void SetInGameMenuHomebrew(const Menu::HomebrewTabModel& homebrew) { // AzaharFrame emerald
    std::scoped_lock lock{g_mods_mutex};
    g_homebrew = homebrew;
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
    // AzaharFrame autosave: Per-Game every time the menu opens (owner rule): a change to a
    // per-game row is for the game being played, unless Global is picked from the scope's menu.
    // BuildModel makes it Global when the game cannot have settings of its own.
    scope = Menu::OpeningScope(true);
    view.hover = view.pressed = {};
    view.open_menu = view.tooltip = view.dragging = Id::None;
    view.tooltip_index = -1;
    Resume();
}

void MenuPanel::Resume() {
    open = true;
    press_hand = -1;
    rest_id = Id::None;
    view.pressed = {};
    bar_press.End(); // AzaharFrame scroll
    list_drag.Cancel();
    stick_scroll.Reset();
    // Buttons held from opening (Menu, a trigger, B in the window) click nothing until let go.
    a_down = b_down = true;
    trigger_down = {true, true};
    LOG_INFO(Frontend, "xr: in-game menu: the tab menu shows");
}

void MenuPanel::BeginEdit() {
    const Vis::HelpBindings given = Vis::CurrentHelpBindings();
    const Vis::HelpBindings b = given.valid ? given : Vis::DefaultHelpBindings();
    for (std::size_t i = 0; i < edit_buttons.size(); i++) {
        edit_buttons[i] = b.buttons[i];
    }
    edit_table = TableOf(edit_buttons);
    for (std::size_t i = 0; i < edit_analogs.size(); i++) {
        edit_analogs[i] = b.analogs[i];
    }
    edit_sticks = SticksOf(edit_analogs);
    std::array<std::string, Menu::kRemapButtons> defaults;
    const Vis::HelpBindings d = Vis::DefaultHelpBindings();
    for (std::size_t i = 0; i < defaults.size(); i++) {
        defaults[i] = d.buttons[i];
    }
    remap.Begin(edit_table, TableOf(defaults), Menu::RemapSource::Frame, edit_sticks,
                SticksOf({d.analogs[0], d.analogs[1]}));
    edit_prev = {};
    edit_b_prev = true;
    // AzaharFrame pads: Edit Controls starts on the Frame controllers.
    edit_device = Input::kFrameDeviceId;
    if (edit_pad_listening) {
        Input::Devices::CancelListen();
        edit_pad_listening = false;
    }
    UiSound::Prepare(); // AzaharFrame menuux: the click loaded before the first edit
    LOG_INFO(Frontend, "xr: in-game menu: Edit Controls");
}

// AzaharFrame pads: Edit Controls on a gamepad (its mapping from the registry, in the menu's
// terms) or the mouse (AzaharFrame padsui: its MouseInputs as they are, Touch Screen and Open
// Menu besides).
void MenuPanel::BeginDeviceEdit(const std::string& id, bool by_label) {
    if (edit_pad_listening) {
        Input::Devices::CancelListen();
        edit_pad_listening = false;
    }
    edit_device = id;
    edit_pad_by_label = by_label;
    if (id == Input::kMouseDeviceId) {
        const Input::GamepadMapping now = Input::Devices::GetMapping(id);
        const Input::GamepadMapping def = Input::Devices::GetDefaultMapping(id);
        remap.Begin(MouseTableOf(now), MouseTableOf(def), Menu::RemapSource::Mouse, {-1, -1},
                    {-1, -1}, {now.touch, now.open_menu}, {def.touch, def.open_menu});
        mouse_test_started = false;
    } else if (id == Input::kKeyboardDeviceId) {
        // AzaharFrame keys: its key codes as they are; Open Menu, the stick keys, the modifier.
        const Input::GamepadMapping now = Input::Devices::GetMapping(id);
        const Input::GamepadMapping def = Input::Devices::GetDefaultMapping(id);
        remap.Begin(MouseTableOf(now), MouseTableOf(def), Menu::RemapSource::Keyboard, {-1, -1},
                    {-1, -1}, {-1, now.open_menu}, {-1, def.open_menu}, KeysOf(now),
                    KeysOf(def));
    } else {
        const Input::GamepadMapping now = Input::Devices::GetMapping(id);
        const Input::GamepadMapping def = Input::Devices::GetDefaultMapping(id);
        remap.Begin(Input::ControlTableOf(now, by_label), Input::ControlTableOf(def, by_label),
                    Menu::RemapSource::Pad, {now.analogs[0], now.analogs[1]},
                    {def.analogs[0], def.analogs[1]});
    }
    edit_prev = {};
    LOG_INFO(Frontend, "xr: in-game menu: Edit Controls: another device");
}

// A change of the table is written to the controls profile at once (the Qt side keeps it in the
// profile and the config), and the diagram shows it in the next frame.
void MenuPanel::EditChanged(Menu::Remap::Event e, Host& host) {
    using Event = Menu::Remap::Event;
    if (e != Event::Moved && e != Event::Swapped && e != Event::Reset) {
        return;
    }
    UiSound::PlayClick(); // AzaharFrame menuux: Steam's soft click, off this thread
    // AzaharFrame pads: a gamepad's mapping goes to user/controller-mappings.txt.
    if (edit_device != Input::kFrameDeviceId) {
        if (edit_device == Input::kMouseDeviceId) {
            // AzaharFrame padsui: the mouse's buttons, Touch Screen and Open Menu; Reset to
            // Default leaves its Mouse Mode and Recentre at Edge as they are.
            const Input::GamepadMapping cur = Input::Devices::GetMapping(edit_device);
            Input::GamepadMapping m = cur;
            for (std::size_t i = 0; i < m.buttons.size() && i < remap.Table().size(); i++) {
                m.buttons[i] = remap.Table()[i];
            }
            m.touch = remap.Extras()[0];
            m.open_menu = remap.Extras()[1];
            const bool saved = Input::Devices::SetMapping(edit_device, m);
            LOG_INFO(Frontend, "xr: in-game menu: Edit Controls: mouse {}{}",
                     e == Event::Reset ? "Reset to Default" : e == Event::Swapped ? "swapped" : "moved",
                     saved ? "" : " (not saved)");
            return;
        }
        if (edit_device == Input::kKeyboardDeviceId) {
            // AzaharFrame keys: its keys, Open Menu, the stick keys and the modifier; Reset to
            // Default leaves Use This Device as it is.
            Input::GamepadMapping m = Input::Devices::GetMapping(edit_device);
            for (std::size_t i = 0; i < m.buttons.size() && i < remap.Table().size(); i++) {
                m.buttons[i] = remap.Table()[i];
            }
            m.open_menu = remap.Extras()[1];
            SetKeys(m, remap.Keys());
            const bool saved = Input::Devices::SetMapping(edit_device, m);
            LOG_INFO(Frontend, "xr: in-game menu: Edit Controls: keyboard {}{}",
                     e == Event::Reset ? "Reset to Default" : e == Event::Swapped ? "swapped" : "moved",
                     saved ? "" : " (not saved)");
            return;
        }
        Input::GamepadMapping m = Input::WithControlTable(Input::Devices::GetMapping(edit_device),
                                                          remap.Table(), edit_pad_by_label);
        m.analogs = {remap.Sticks()[0], remap.Sticks()[1]}; // AzaharFrame padsui: stick routes
        const bool saved = e == Event::Reset ? (Input::Devices::ResetMapping(edit_device), true)
                                             : Input::Devices::SetMapping(edit_device, m);
        LOG_INFO(Frontend, "xr: in-game menu: Edit Controls: gamepad {}{}",
                 e == Event::Reset ? "Reset to Default" : e == Event::Swapped ? "swapped" : "moved",
                 saved ? "" : " (not saved)");
        return;
    }
    ApplyTable(edit_buttons, remap.Table(), edit_table);
    edit_table = remap.Table();
    ApplySticks(edit_analogs, remap.Sticks(), edit_sticks); // AzaharFrame padsui
    edit_sticks = remap.Sticks();
    const Vis::HelpBindings given = Vis::CurrentHelpBindings();
    const Vis::HelpBindings b = given.valid ? given : Vis::DefaultHelpBindings();
    SetControlBindings(std::vector<std::string>(edit_buttons.begin(), edit_buttons.end()),
                       std::vector<std::string>(edit_analogs.begin(), edit_analogs.end()),
                       b.profile_name);
    MenuRequest r;
    r.command = MenuCommand::SetButtons;
    r.buttons.assign(edit_buttons.begin(), edit_buttons.end());
    r.analogs.assign(edit_analogs.begin(), edit_analogs.end());
    if (host.request) {
        host.request(r);
    }
    LOG_INFO(Frontend, "xr: in-game menu: Edit Controls: {}",
             e == Event::Reset ? "Reset to Default" : e == Event::Swapped ? "swapped" : "moved");
}

// A Frame button pressed while a chip listens (the XR thread's raw input): B cancels, the others
// move the chip's 3DS buttons to it. Presses are edges; the game is paused, so it sees none.
void MenuPanel::EditInput(const Input::InputFrame& in, bool have_input, float dt, Host& host) {
    using Input::Control;
    const bool pos = model.face_buttons == 0;
    const std::array<bool, 11> now = {
        have_input && in.a,
        have_input && in.x,
        have_input && in.y,
        have_input && in.menu,
        have_input && in.view,
        have_input && in.dpad_up,
        have_input && in.dpad_down,
        have_input && in.dpad_left,
        have_input && in.dpad_right,
        have_input && in.hand[0].bumper,
        have_input && in.hand[1].bumper,
    };
    // By position the Frame's A drives the pad's B (mapper.cpp), and so on.
    const std::array<Control, 11> as = {
        pos ? Control::B : Control::A, pos ? Control::Y : Control::X, pos ? Control::X : Control::Y,
        Control::Menu,     Control::View,   Control::DpadUp,  Control::DpadDown,
        Control::DpadLeft, Control::DpadRight, Control::BumperL, Control::BumperR};
    // AzaharFrame pads: a gamepad being edited: its own next press (devices.h); the Frame's
    // buttons are not its buttons (B still cancels, the beam still clicks).
    if (remap.Source() == Menu::RemapSource::Mouse) {
        // AzaharFrame padsui: the mouse being edited: its next click, button or wheel notch.
        if (remap.Listening() >= 0) {
            // AzaharFrame input2: in Pointer mode its touch input is the pointer's select (the
            // click that works this menu): never a binding, and never kept from the pointer, so
            // Cancel and the other chips still click. In Bottom Screen mode it may be bound, but
            // not in the first 300 ms (Remap::kSelectGuardSeconds).
            const Input::GamepadMapping mm = Input::Devices::GetMapping(edit_device);
            const bool pointer = mm.mode == static_cast<uint8_t>(Input::MouseMode::Pointer);
            remap.SetPointerSelect(mm.touch, pointer);
            if (!edit_pad_listening) {
                edit_pad_listening =
                    Input::Devices::BeginListen(edit_device, pointer ? mm.touch : -1);
            }
            if (edit_pad_listening) {
                using State = Input::GamepadHub::ListenResult::State;
                const Input::GamepadHub::ListenResult r = Input::Devices::PollListen();
                if (r.state == State::Pressed) {
                    edit_pad_listening = false;
                    EditChanged(remap.Press(r.input), host);
                } else if (r.state != State::Listening) {
                    edit_pad_listening = false;
                    EditChanged(remap.Cancel(), host); // the mouse went away
                }
            }
            EditChanged(remap.Tick(dt), host);
        } else if (edit_pad_listening) {
            Input::Devices::CancelListen();
            edit_pad_listening = false;
        }
        edit_prev = now;
        return;
    }
    // AzaharFrame input2: a gamepad never works the menu, and the Frame's triggers (the beams'
    // select) cannot be bound (Remap::Listenable): no pointer select to guard.
    remap.SetPointerSelect(-1, false);
    if (remap.Source() == Menu::RemapSource::Keyboard) {
        // AzaharFrame keys: the keyboard being edited: its next key (a stick: four in turn; the
        // hub's listening ends with each, so it starts again for the next).
        if (remap.Listening() >= 0) {
            if (!edit_pad_listening) {
                edit_pad_listening = Input::Devices::BeginListen(edit_device);
            }
            if (edit_pad_listening) {
                using State = Input::GamepadHub::ListenResult::State;
                const Input::GamepadHub::ListenResult r = Input::Devices::PollListen();
                if (r.state == State::Pressed) {
                    edit_pad_listening = false;
                    EditChanged(remap.Press(r.input), host);
                } else if (r.state != State::Listening) {
                    edit_pad_listening = false;
                    EditChanged(remap.Cancel(), host); // the keyboard went away
                }
            }
            EditChanged(remap.Tick(dt), host);
        } else if (edit_pad_listening) {
            Input::Devices::CancelListen();
            edit_pad_listening = false;
        }
        edit_prev = now;
        return;
    }
    if (remap.Source() == Menu::RemapSource::Pad) {
        if (remap.Listening() >= 0) {
            if (!edit_pad_listening) {
                edit_pad_listening = Input::Devices::BeginListen(edit_device);
            }
            if (edit_pad_listening) {
                using State = Input::GamepadHub::ListenResult::State;
                const Input::GamepadHub::ListenResult r = Input::Devices::PollListen();
                if (remap.ListeningStick()) {
                    // AzaharFrame padsui: a 3DS stick listens: the pad's sticks, two pushes. A
                    // button pressed meanwhile ends the hub's listening: listen again.
                    if (r.state == State::Listening) {
                        for (std::size_t i = 0; i < r.sticks.size(); i++) {
                            EditChanged(remap.Stick(static_cast<int>(i), r.sticks[i]), host);
                        }
                    } else if (r.state == State::Pressed) {
                        edit_pad_listening = false;
                    } else {
                        edit_pad_listening = false;
                        EditChanged(remap.Cancel(), host); // the pad went away
                    }
                    if (remap.Listening() < 0 && edit_pad_listening) {
                        Input::Devices::CancelListen();
                        edit_pad_listening = false;
                    }
                } else if (r.state == State::Pressed) {
                    edit_pad_listening = false;
                    const int control = Input::ControlOfPadInput(r.input, edit_pad_by_label);
                    if (control >= 0) {
                        EditChanged(remap.Press(control), host);
                    } // Guide, Misc1, the touchpad click: keep listening
                } else if (r.state != State::Listening) {
                    edit_pad_listening = false;
                    EditChanged(remap.Cancel(), host); // the pad went away
                }
            }
            EditChanged(remap.Tick(dt), host);
        } else if (edit_pad_listening) {
            Input::Devices::CancelListen();
            edit_pad_listening = false;
        }
        edit_prev = now;
        return;
    }
    if (remap.ListeningStick()) {
        // AzaharFrame padsui: a 3DS stick listens: each Frame stick, as the hand reads it. The
        // gestures stay on the physical sticks (mapper.cpp); only the 3DS stick's source moves.
        for (int h = 0; h < 2 && have_input; h++) {
            EditChanged(remap.Stick(h, in.hand[static_cast<std::size_t>(h)].stick), host);
        }
        EditChanged(remap.Tick(dt), host);
    } else if (remap.Listening() >= 0) {
        for (std::size_t i = 0; i < now.size(); i++) {
            if (now[i] && !edit_prev[i]) {
                EditChanged(remap.Press(static_cast<int>(as[i])), host);
                break;
            }
        }
        EditChanged(remap.Tick(dt), host);
    }
    edit_prev = now;
}

void MenuPanel::Close() {
    if (!open) {
        return;
    }
    remap.Done();
    if (edit_pad_listening) { // AzaharFrame pads
        Input::Devices::CancelListen();
        edit_pad_listening = false;
    }
    view.remap = remap.View();
    open = false;
    view.open_menu = view.dragging = Id::None;
    view.confirm = {}; // AzaharFrame compat: "This May Not Work" goes with the menu
    press_hand = -1;
    bar_press.End(); // AzaharFrame scroll
    list_drag.Cancel();
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
                m.slots.push_back({s.slot, s.time, s.other});
            }
        }
    }
    if (!per_game_ok) {
        scope = 0;
    }
    { // AzaharFrame cheatstab
        std::scoped_lock lock{g_mods_mutex};
        m.mods = g_mods;
        m.homebrew = g_homebrew; // AzaharFrame emerald
        m.notice = g_notice;     // AzaharFrame savestate
        m.autosaves = g_autosaves; // AzaharFrame autosave
        m.undo_load = g_undo_load;
    }
    if (view.tab == Menu::Tab::Homebrew && // AzaharFrame ui3: its own tab
        // AzaharFrame emerald: the progress ring turns
        std::any_of(m.homebrew.items.begin(), m.homebrew.items.end(),
                    [](const Menu::HomebrewItem& i) { return (i.busy && i.progress < 0) || i.checking; })) {
        m.homebrew.spin = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now().time_since_epoch())
                                               .count() /
                                           66);
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
    // AzaharFrame ctgp7: the CTGP-7 segment only while it shows (Mario Kart 7 open); its bar moves
    if (!m.mods.ctgp7.shown) {
        if (view.mods_segment == Menu::ModsSegment::Ctgp7) {
            view.mods_segment = Menu::ModsSegment::Cheats;
        }
        view.requires_ctgp7 = false;
    }
    if (m.mods.ctgp7.busy && m.mods.ctgp7.permille < 0 && view.tab == Menu::Tab::Mods &&
        view.mods_segment == Menu::ModsSegment::Ctgp7) {
        m.mods.ctgp7.spin = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 std::chrono::steady_clock::now().time_since_epoch())
                                                 .count() /
                                             66);
    }
    m.hide_bottom_screen = v.vr_hide_bottom_screen.GetValue();
    m.screen_shape = static_cast<int>(std::min<u32>(v.vr_top_aspect.GetValue(), 6)); // fixes7
    m.screen_curve = NearestStep(kCurveSteps, v.vr_screen_curve.GetValue());
    m.screen_layout = static_cast<int>(std::min<u32>(v.vr_screen_layout.GetValue(), 1));
    m.move_screens = static_cast<int>(std::min<u32>(v.vr_move_screens.GetValue(), 2)); // menu
    m.touchpad_touch = v.vr_touchpad_touch.GetValue() == 2 ? 1 : 0; // AzaharFrame menuux
    m.bottom_screen_reticle = // AzaharFrame mouse
        static_cast<int>(std::min<u32>(v.vr_bottom_screen_reticle.GetValue(), 3)); // input2
    m.reticle_at_edge = static_cast<int>(std::min<u32>(v.vr_reticle_at_edge.GetValue(), 1));
    m.reticle_speed = static_cast<int>(std::lround( // AzaharFrame reticle: tenths
        Input::ClampReticleSpeed(v.vr_reticle_speed.GetValue()) * 10.0f));
    m.reticle_center = v.vr_reticle_center_until_pressed.GetValue(); // AzaharFrame fixes7
    m.reticle_stick = v.vr_reticle_stick_moves.GetValue();
    m.reticle_edge_cstick = v.vr_reticle_edge_cstick.GetValue(); // AzaharFrame kiu
    if (remap.Editing()) {
        // AzaharFrame padsui + pads: Edit Controls' devices from the registry (Frame Controllers,
        // each gamepad, the mouse), each with its 3DS mapping in the menu's terms; the one being
        // edited shows the state machine's table.
        const Vis::HelpBindings given = Vis::CurrentHelpBindings();
        const Vis::HelpBindings hb = given.valid ? given : Vis::DefaultHelpBindings();
        std::array<std::string, Menu::kRemapButtons> profile_buttons;
        for (std::size_t i = 0; i < profile_buttons.size(); i++) {
            profile_buttons[i] = hb.buttons[i];
        }
        const Menu::RemapSticks profile_sticks = SticksOf({hb.analogs[0], hb.analogs[1]});
        m.devices.selected = -1;
        for (const Input::DeviceInfo& d : Input::Devices::List()) {
            Menu::EditDevice e;
            e.key = d.id;
            e.name = d.name;
            e.has_touchpad = d.has_touchpad;
            e.connected = d.connected; // AzaharFrame padsui: the row's status dot
            if (d.kind == Input::DeviceKind::Frame) {
                e.kind = Menu::DeviceKind::Frame;
                e.names = Menu::ButtonNames::Frame;
                e.mapping = TableOf(profile_buttons);
                e.sticks = profile_sticks;
            } else if (d.kind == Input::DeviceKind::Mouse) {
                // AzaharFrame padsui: the mouse's buttons, Touch Screen and Open Menu.
                e.kind = Menu::DeviceKind::Mouse;
                e.names = Menu::ButtonNames::Mouse;
                const Input::GamepadMapping gm = Input::Devices::GetMapping(d.id);
                e.mapping = MouseTableOf(gm);
                e.sticks = {-1, -1};
                e.extras = {gm.touch, gm.open_menu};
                e.mouse_mode = gm.mode;
                e.recentre = gm.recentre;
                e.speed = gm.speed;
                e.enabled = gm.enabled;
            } else if (d.kind == Input::DeviceKind::Keyboard) {
                // AzaharFrame keys: its keys, Open Menu, the stick keys and the modifier.
                e.kind = Menu::DeviceKind::Keyboard;
                e.names = Menu::ButtonNames::Keyboard;
                const Input::GamepadMapping gm = Input::Devices::GetMapping(d.id);
                e.mapping = MouseTableOf(gm);
                e.sticks = {-1, -1};
                e.extras = {-1, gm.open_menu};
                e.keys = KeysOf(gm);
                e.enabled = gm.enabled;
            } else {
                e.kind = Menu::DeviceKind::Gamepad;
                e.names = Menu::NamesFor(static_cast<int>(d.type), d.steam_input);
                const Input::GamepadMapping gm = Input::Devices::GetMapping(d.id);
                e.mapping = Input::ControlTableOf(gm, Input::ReportsByLabel(d.type, d.steam_input));
                e.sticks = {gm.analogs[0], gm.analogs[1]};
                e.recentre = gm.recentre; // AzaharFrame padsui
                e.enabled = gm.enabled;
            }
            if (d.id == edit_device) {
                m.devices.selected = static_cast<int>(m.devices.list.size());
                e.mapping = remap.Table();
                e.sticks = remap.Sticks();
                if (e.kind == Menu::DeviceKind::Mouse || e.kind == Menu::DeviceKind::Keyboard) {
                    e.extras = remap.Extras();
                    e.keys = remap.Keys(); // AzaharFrame keys (none for the mouse)
                }
            }
            m.devices.list.push_back(std::move(e));
        }
        if (m.devices.selected < 0) {
            // The device went from the list (it never does while the program runs): the Frame's.
            m.devices.selected = 0;
            edit_device = Input::kFrameDeviceId;
            BeginEdit();
            m.devices.list[0].mapping = remap.Table();
            m.devices.list[0].sticks = remap.Sticks();
        }
        // The Pointer Test: the chosen gamepad's touchpad, or the mouse over the window.
        const Menu::EditDevice& sel = m.devices.list[static_cast<std::size_t>(m.devices.selected)];
        if (sel.kind == Menu::DeviceKind::Mouse) {
            // AzaharFrame padsui: in Bottom Screen mode, a cursor of the menu's own, moved as the
            // game's would be (relative, clamped); its touch input pressed is a touch. In Pointer
            // mode the ray is the test.
            const Input::MouseFrame mf = Input::Devices::Hub().LastMouse();
            if (!mouse_test_started) {
                mouse_test = {0.5f, 0.5f};
                mouse_test_started = true;
            }
            const float gain = Input::MouseScreenGain(mf.speed);
            mouse_test.x = std::clamp(mouse_test.x + mf.motion.x * gain / 320.0f, 0.0f, 1.0f);
            mouse_test.y = std::clamp(mouse_test.y + mf.motion.y * gain / 240.0f, 0.0f, 1.0f);
            m.devices.pointer = {mf.present && sel.mouse_mode == 0, mf.Touch(), mouse_test.x,
                                 mouse_test.y};
        } else if (sel.kind == Menu::DeviceKind::Gamepad && sel.has_touchpad) {
            const Input::PadSample pad = Input::Devices::GetTouchpad(sel.key);
            const bool click_mode = m.touchpad_touch == 1; // AzaharFrame menuux
            m.devices.pointer = {pad.contact || pad.click, click_mode ? pad.click : pad.contact,
                                 std::clamp(pad.x, 0.0f, 1.0f), std::clamp(pad.y, 0.0f, 1.0f)};
        }
    }
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
    m.gyro_aim = static_cast<int>(std::min<u32>(v.vr_gyro_aim.GetValue(), 2)); // AzaharFrame kiugyro
    m.gyro_sensitivity = static_cast<int>(
        std::lround(Input::ClampGyroSensitivity(v.vr_gyro_sensitivity.GetValue()) * 10.0f));
    m.turn_zone = static_cast<int>(
        std::min<u32>(v.vr_light_gun_turn_zone.GetValue(), Input::kTurnZoneMaxPercent));
    m.turn_speed = static_cast<int>(
        std::lround(Input::ClampTurnSpeed(v.vr_light_gun_turn_speed.GetValue()) * 10.0f));
    m.recoil = v.vr_light_gun_recoil.GetValue();
    m.show_controllers = v.vr_show_controllers.GetValue();
    m.window_scale = NearestStep(kScaleSteps, v.vr_window_scale.GetValue());
    // AzaharFrame depth3d; natural: in the Full modes the Full modes' own 3D Depth.
    m.depth_3d = static_cast<int>(
        v.vr_immersive_mode.GetValue() >= 2
            ? Immersive::Layout::NaturalDepthPercent(
                  !v.vr_immersive_depth.UsingGlobal(), v.vr_immersive_depth.GetValue(),
                  !v.factor_3d.UsingGlobal(), v.factor_3d.GetValue())
            : std::min<u32>(v.factor_3d.GetValue(), 100));
    // AzaharFrame menuux: the renderer's settings.
    m.internal_resolution = static_cast<int>(std::min<u32>(v.resolution_factor.GetValue(), 18));
    m.dynamic_resolution = v.vr_dynamic_resolution.GetValue();
    m.texture_filter = 0;
    for (std::size_t i = 0; i < Menu::kTextureFilters.size(); i++) {
        if (static_cast<int>(v.texture_filter.GetValue()) == Menu::kTextureFilters[i]) {
            m.texture_filter = static_cast<int>(i);
        }
    }
    m.async_shaders = v.async_shader_compilation.GetValue();
    m.accurate_image_copies = // AzaharFrame accxfer, xfergpu: 0 Off, 1 Automatic, 2 On
        std::min(static_cast<int>(v.image_copy_accuracy.GetValue()), 2);
    m.fast_forward_audio = static_cast<int>(Settings::FastForwardAudioIndex(
        v.fast_forward_audio.GetValue())); // AzaharFrame ffaudio, ffspeed: the row's order
    m.fast_forward_speed = 0;
    {
        const double ff = v.turbo_limit.GetValue();
        double best = 1e9;
        for (std::size_t i = 0; i < Menu::kFastForwardSpeeds.size(); i++) {
            const double d = std::fabs(ff - Menu::kFastForwardSpeeds[i]);
            if (d < best) {
                best = d;
                m.fast_forward_speed = static_cast<int>(i);
            }
        }
    }
    // AzaharFrame compat: mods turned off after the game closed unexpectedly: the Mods tab's Mods
    // segment shows their snackbar, once per notice
    if (m.mods.mods.turned_off_serial != turned_off_seen) {
        turned_off_seen = m.mods.mods.turned_off_serial;
        if (!m.mods.mods.turned_off.empty()) {
            view.tab = Menu::Tab::Mods;
            view.mods_segment = Menu::ModsSegment::Mods;
            view.mods_page = Menu::ModsPage::Loaded;
            view.page = Menu::Page::Tabs;
        }
    }
    // AzaharFrame texui: the two switches straight from the settings (the rest is the Qt side's)
    m.mods.textures.custom_textures = v.custom_textures.GetValue();
    m.mods.textures.preload = v.preload_textures.GetValue();
    // AzaharFrame stall: also while Install Mods waits for its first answer (the catalog still
    // Idle): the ring turns from the start, and never stands still.
    {
        // AzaharFrame jsoncrash: the page asks again until something answers (see ShouldAskCatalog)
        const long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count();
        const bool shown = view.tab == Menu::Tab::Mods &&
                           view.mods_segment == Menu::ModsSegment::Mods &&
                           view.mods_page == Menu::ModsPage::Install;
        if (!shown) {
            catalog_ask_ms = -1;
        } else if (Menu::ShouldAskCatalog(m.mods.mods.catalog, shown, now_ms, catalog_ask_ms)) {
            catalog_ask_ms = now_ms;
            if (host.request) {
                MenuRequest r;
                r.command = MenuCommand::LoadModCatalog;
                host.request(r);
            }
        }
    }
    bool spinning = m.mods.mods.catalog == Menu::CatalogState::Loading ||
                    (m.mods.mods.catalog != Menu::CatalogState::Ready &&
                     m.mods.mods.catalog != Menu::CatalogState::Failed &&
                     view.mods_page == Menu::ModsPage::Install);
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
    if ((m.mods.zip.working || m.mods.zip.loading) && view.tab == Menu::Tab::Mods) {
        // AzaharFrame zipinstall: Install from Zip's ring
        m.mods.zip.spin = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
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
    if (m.mods.textures.parts.optimize.state == Menu::OptimizeState::Checking &&
        view.tab == Menu::Tab::Mods && view.textures_page == Menu::TexturesPage::Parts) {
        // AzaharFrame ui3: Optimize for Frame's ring while the estimate runs
        m.mods.textures.parts.optimize.spin = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count() /
            66);
    }
    m.immersive_mode = static_cast<int>(std::min<u32>(v.vr_immersive_mode.GetValue(), 3));
    m.wider_view = static_cast<int>(std::min<u32>(v.vr_immersive_wider_view_level.GetValue(), 4));
    m.game_fov = v.vr_immersive_game_fov.GetValue();
    // AzaharFrame depth, fovonce: 0 Off, 1 Set Once, 2 Always
    m.match_camera =
        static_cast<int>(std::min<u32>(v.vr_immersive_match_camera_mode.GetValue(), 2));
    // AzaharFrame kiu: signed percent
    m.scale_x = FrameXr::Immersive::Layout::ScalePercent(v.vr_immersive_scale_x.GetValue());
    m.scale_y = FrameXr::Immersive::Layout::ScalePercent(v.vr_immersive_scale_y.GetValue());
    m.scale_mode = v.vr_immersive_scale_mode.GetValue() == 0 ? 0 : 1; // AzaharFrame fovscale
    // AzaharFrame natural: World Scale's nearest stop, Follow Screen Size.
    m.world_scale_stop = Immersive::Layout::WorldScaleStop(v.vr_immersive_world_scale.GetValue());
    m.follow_screen = v.vr_immersive_follow_screen.GetValue();
    // AzaharFrame autosave: the running game's values (its own, else the global ones)
    m.autosave = v.autosave_enabled.GetValue();
    m.autosave_every = 0;
    for (std::size_t i = 0; i < Menu::kAutosaveEvery.size(); i++) {
        if (static_cast<u32>(Menu::kAutosaveEvery[i]) <= v.autosave_every.GetValue()) {
            m.autosave_every = static_cast<int>(i);
        }
    }
    m.autosave_keep = static_cast<int>(std::clamp<u32>(v.autosave_keep.GetValue(), 1, 10));

    PerGame::FillGameValues(m); // AzaharFrame useglobal; reticle: the scope control's count
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
        lock(Id::Scope, Lock::NoGame); // AzaharFrame reticle: the toggle shows Global alone
    }
    // AzaharFrame vis: no controller models in passthrough (vis/layer_plan.h); the switch keeps
    // its saved value, greyed.
    if (host.passthrough_ok && m.background == 1) {
        lock(Id::ShowControllers, Lock::InPassthrough);
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
        ShowToastText("Hide Bottom Screen: From the Next Start", ToastTime::Long);
        break;
    case Id::ScreenShape:
        Apply(v.vr_top_aspect, std::min<u32>(u, 6), per_game); // AzaharFrame fixes7: 6 Automatic
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
    case Id::TouchpadTouch: // AzaharFrame menu; menuux: 0 Tap (1), 1 Click (2); reticle: per game
        r.value = u >= 1 ? 2 : 1;
        Apply(v.vr_touchpad_touch, static_cast<u32>(r.value), per_game);
        r.setting = MenuSetting::TouchpadTouch;
        break;
    // AzaharFrame menuux: the renderer's settings. A new Internal Resolution or Texture Filter
    // takes effect at the next frame (the rasterizer cache starts again: a short hitch).
    case Id::InternalResolution:
        r.value = std::clamp(value + 1, 1, Menu::kMaxResolution);
        Apply(v.resolution_factor, static_cast<u32>(r.value), per_game);
        r.setting = MenuSetting::InternalResolution;
        DynamicResolution::Reset(); // from the new value, no step off
        break;
    case Id::DynamicResolution:
        Apply(v.vr_dynamic_resolution, value != 0, false);
        r.setting = MenuSetting::DynamicResolution;
        // AzaharFrame ui2: off is the full factor at once; on again carries on where it was.
        DynamicResolution::SetEnabled(value != 0);
        break;
    case Id::TextureFilter:
        r.value = Menu::kTextureFilters[std::min<std::size_t>(u, Menu::kTextureFilters.size() - 1)];
        Apply(v.texture_filter, static_cast<Settings::TextureFilter>(r.value), per_game);
        r.setting = MenuSetting::TextureFilter;
        break;
    case Id::AsyncShaders:
        Apply(v.async_shader_compilation, value != 0, per_game);
        r.setting = MenuSetting::AsyncShaders;
        ShowToastText("Async Shader Compilation: From the Next Start", ToastTime::Long);
        break;
    case Id::AccurateImageCopies: // AzaharFrame accxfer, xfergpu: from the next image copy
        r.value = static_cast<int>(std::min<u32>(u, 2));
        Apply(v.image_copy_accuracy, static_cast<Settings::ImageCopyAccuracy>(r.value), per_game);
        r.setting = MenuSetting::AccurateImageCopies;
        break;
    case Id::FastForwardAudio: // AzaharFrame ffaudio: from the next sound block
        r.value = static_cast<int>(std::min<u32>(u, 3)); // the row's index
        Apply(v.fast_forward_audio, Settings::FastForwardAudioFromIndex(u), per_game);
        r.setting = MenuSetting::FastForwardAudio;
        break;
    case Id::FastForwardSpeed:
        r.value =
            Menu::kFastForwardSpeeds[std::min<std::size_t>(u, Menu::kFastForwardSpeeds.size() - 1)];
        Apply(v.turbo_limit, static_cast<double>(r.value), false);
        r.setting = MenuSetting::FastForwardSpeed;
        break;
    // AzaharFrame autosave: per game in Per-Game (the game's file gets them), else global
    case Id::Autosave:
        Apply(v.autosave_enabled, value != 0, per_game);
        r.setting = MenuSetting::Autosave;
        break;
    case Id::AutosaveEvery:
        r.value = Menu::kAutosaveEvery[std::min<std::size_t>(u, Menu::kAutosaveEvery.size() - 1)];
        Apply(v.autosave_every, static_cast<u32>(r.value), per_game);
        r.setting = MenuSetting::AutosaveEvery;
        break;
    case Id::AutosaveKeep:
        r.value = std::clamp(value, 1, 10);
        Apply(v.autosave_keep, static_cast<u32>(r.value), per_game);
        r.setting = MenuSetting::AutosaveKeep;
        break;
    case Id::ReticleAtEdge: // AzaharFrame mouse; reticle: per game
        Apply(v.vr_reticle_at_edge, std::min<u32>(u, 1), per_game);
        r.setting = MenuSetting::ReticleAtEdge;
        LOG_INFO(Frontend, "xr: in-game menu: At Edge = {}",
                 Menu::OptionsFor(id)[std::min<u32>(u, 1)]);
        break;
    case Id::BottomScreenReticle: // AzaharFrame mouse; reticle: per game
        // AzaharFrame input2: 3 Headset
        Apply(v.vr_bottom_screen_reticle, std::min<u32>(u, 3), per_game);
        r.setting = MenuSetting::BottomScreenReticle;
        LOG_INFO(Frontend, "xr: in-game menu: Bottom Screen Reticle = {}",
                 Menu::OptionsFor(id)[std::min<u32>(u, 3)]);
        break;
    case Id::ReticleSpeed: { // AzaharFrame reticle: tenths of 1x from the slider
        r.value = std::clamp(value, 5, 40);
        Apply(v.vr_reticle_speed, static_cast<float>(r.value) / 10.0f, per_game);
        r.setting = MenuSetting::ReticleSpeed;
        LOG_INFO(Frontend, "xr: in-game menu: Reticle Speed = {:.1f}x",
                 static_cast<float>(r.value) / 10.0f);
        break;
    }
    case Id::ReticleCenter: // AzaharFrame fixes7: per game
        Apply(v.vr_reticle_center_until_pressed, value != 0, per_game);
        r.setting = MenuSetting::ReticleCenter;
        LOG_INFO(Frontend, "xr: in-game menu: Aim Only While Pressed = {}", value != 0);
        break;
    case Id::ReticleStick:
        Apply(v.vr_reticle_stick_moves, value != 0, per_game);
        r.setting = MenuSetting::ReticleStick;
        LOG_INFO(Frontend, "xr: in-game menu: Right Stick Moves Touch = {}", value != 0);
        break;
    case Id::ReticleEdgeCStick: // AzaharFrame kiu
        Apply(v.vr_reticle_edge_cstick, value != 0, per_game);
        r.setting = MenuSetting::ReticleEdgeCStick;
        LOG_INFO(Frontend, "xr: in-game menu: C-Stick at Edge = {}", value != 0);
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
    case Id::CursorSpeed: { // AzaharFrame padsui: the mouse's, kept with its mapping, live
        Input::GamepadMapping cm = Input::Devices::GetMapping(Input::kMouseDeviceId);
        cm.speed = static_cast<uint8_t>(std::clamp(value, 0, Input::kNumCursorSpeeds - 1));
        Input::Devices::SetMapping(Input::kMouseDeviceId, cm);
        LOG_INFO(Frontend, "xr: in-game menu: Cursor Speed = {}", cm.speed);
        return;
    }
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
    case Id::MotionControls: // AzaharFrame kiu: per game in Per-Game
        Apply(v.vr_motion_controls, std::min<u32>(u, 3), per_game);
        r.setting = MenuSetting::MotionControls;
        break;
    case Id::GyroAim: // AzaharFrame kiugyro: Aiming; per game in Per-Game
        // The shown index to the stored value (Direct Aim 2, Gyro Aim 1).
        r.value = Menu::AimingValueAt(static_cast<int>(std::min<u32>(u, 2)));
        Apply(v.vr_gyro_aim, static_cast<u32>(r.value), per_game);
        r.setting = MenuSetting::GyroAim;
        LOG_INFO(Frontend, "xr: in-game menu: Aiming = {}",
                 Menu::OptionsFor(id)[std::min<u32>(u, 2)]);
        break;
    case Id::GyroSensitivity: // tenths of 1x from the slider
        r.value = std::clamp(value, 2, 40);
        Apply(v.vr_gyro_sensitivity, static_cast<float>(r.value) / 10.0f, per_game);
        r.setting = MenuSetting::GyroSensitivity;
        break;
    case Id::TurnZone: // percent
        r.value = std::clamp(value, 0, static_cast<int>(Input::kTurnZoneMaxPercent));
        Apply(v.vr_light_gun_turn_zone, static_cast<u32>(r.value), per_game);
        r.setting = MenuSetting::TurnZone;
        break;
    case Id::TurnSpeed: // tenths of 1x
        r.value = std::clamp(value, 2, 40);
        Apply(v.vr_light_gun_turn_speed, static_cast<float>(r.value) / 10.0f, per_game);
        r.setting = MenuSetting::TurnSpeed;
        break;
    case Id::Recoil: // per game in Per-Game
        Apply(v.vr_light_gun_recoil, value != 0, per_game);
        r.setting = MenuSetting::Recoil;
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
    case Id::MatchCamera: // AzaharFrame depth, fovonce: live; Off, Set Once, Always
        r.value = static_cast<int>(std::min<u32>(u, 2));
        Apply(v.vr_immersive_match_camera_mode, static_cast<u32>(r.value), per_game);
        r.setting = MenuSetting::MatchCamera;
        // Set Once chosen, also again: the game on view measures its camera anew.
        if (r.value == static_cast<int>(Settings::MatchGameCamera::SetOnce)) {
            v.vr_immersive_fov_measured.SetGlobal(true);
            Immersive::MeasureCameraAgain();
        }
        break;
    case Id::ScaleX: // AzaharFrame kiu: signed percent from the slider, live
        r.value = FrameXr::Immersive::Layout::ScalePercent(value);
        Apply(v.vr_immersive_scale_x, static_cast<s32>(r.value), per_game);
        r.setting = MenuSetting::ScaleX;
        break;
    case Id::ScaleY:
        r.value = FrameXr::Immersive::Layout::ScalePercent(value);
        Apply(v.vr_immersive_scale_y, static_cast<s32>(r.value), per_game);
        r.setting = MenuSetting::ScaleY;
        break;
    case Id::ScaleMode: // AzaharFrame fovscale: live; Stretch, Field of View
        r.value = u == 0 ? 0 : 1;
        Apply(v.vr_immersive_scale_mode, static_cast<u32>(r.value), per_game);
        r.setting = MenuSetting::ScaleMode;
        break;
    case Id::WorldScale: { // AzaharFrame natural: `value` the slider's stop; eased in
        const auto& stops = Immersive::Layout::kWorldScaleStops;
        const float percent = stops[std::min<std::size_t>(u, stops.size() - 1)];
        Apply(v.vr_immersive_world_scale, percent, per_game);
        r.value = static_cast<int>(std::lround(percent * 100.0f));
        r.setting = MenuSetting::WorldScale;
        ShowToastText(Immersive::Layout::WorldScaleToast(percent));
        break;
    }
    case Id::FollowScreen: // AzaharFrame natural
        Apply(v.vr_immersive_follow_screen, value != 0, per_game);
        r.setting = MenuSetting::FollowScreen;
        break;
    case Id::Depth3d: // AzaharFrame depth3d: the HID's 3D slider reads it on every pad update
        // AzaharFrame natural: in the Full modes their own 3D Depth (percent of the natural 3D).
        if (v.vr_immersive_mode.GetValue() >= 2) {
            Apply(v.vr_immersive_depth, std::min<u32>(u, 100), per_game);
        } else {
            Apply(v.factor_3d, std::min<u32>(u, 100), per_game);
        }
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

// AzaharFrame useglobal: "Use Global Value": the game follows the global value again, at once;
// the Qt side takes the key out of the game's file.
void MenuPanel::UseGlobal(Id id, Host& host) {
    if (!PerGameSetting(id)) {
        return;
    }
    const auto& v = Settings::values;
    const bool hide = v.vr_hide_bottom_screen.GetValue();
    const bool async = v.async_shader_compilation.GetValue();
    if (!PerGame::UseGlobalValue(id)) {
        return;
    }
    if (id == Id::InternalResolution) {
        DynamicResolution::Reset(); // from the global value, no step off
    } else if (id == Id::HideBottomScreen && hide != v.vr_hide_bottom_screen.GetValue()) {
        ShowToastText("Hide Bottom Screen: From the Next Start", ToastTime::Long);
    } else if (id == Id::AsyncShaders && async != v.async_shader_compilation.GetValue()) {
        ShowToastText("Async Shader Compilation: From the Next Start", ToastTime::Long);
    }
    LOG_INFO(Frontend, "xr: in-game menu: {}: Use Global Value", Menu::LabelFor(id));
    MenuRequest r;
    r.command = MenuCommand::UseGlobalValue;
    r.setting = SettingOf(id);
    r.per_game = true;
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
            if (menu == Id::ScopeMore && t.index == 0) {
                // AzaharFrame useglobal; reticle: every row of this tab (the Settings tab: this
                // section) the game has its own value for
                for (const Id id : Menu::GameValueRows(model, view.tab, view.section)) {
                    UseGlobal(id, host);
                }
            } else if (menu == Id::AutosaveCard) { // AzaharFrame autosave: the card's list
                LOG_INFO(Frontend, "xr: in-game menu: Load Autosave {}", t.index + 1);
                // AzaharFrame advfix: the entry as drawn (its file and time), not its place
                const auto i = static_cast<std::size_t>(t.index);
                ask_text(MenuCommand::LoadAutosave,
                         i < model.autosaves.size() ? model.autosaves[i].id : std::string{},
                         t.index);
                view.page = Menu::Page::Tabs;
            } else {
                SetSetting(menu, t.index, host);
            }
        }
        return;
    }
    if (t.index == Menu::kLabelIndex || t.index == Menu::kMenuAreaIndex) {
        return;
    }
    static constexpr std::array<const char*, 7> kTabNames{"Main", "Position", "Stats", "Controls",
                                                          "Mods", "Homebrew", "Settings"}; // ui3
    static_assert(kTabNames.size() == static_cast<std::size_t>(Menu::Tab::Count));
    static constexpr std::array<const char*, 4> kSegmentNames{"Cheats", "Mods", "Textures",
                                                              "CTGP-7"}; // AzaharFrame ctgp7
    if (t.id == Id::TabButton) {
        LOG_INFO(Frontend, "xr: in-game menu: {} tab",
                 kTabNames[static_cast<std::size_t>(
                     std::clamp(t.index, 0, static_cast<int>(kTabNames.size()) - 1))]);
    } else if (t.id == Id::ModsSegment) { // AzaharFrame cheatstab
        LOG_INFO(Frontend, "xr: in-game menu: Mods tab: {}",
                 kSegmentNames[static_cast<std::size_t>(std::clamp(t.index, 0, 3))]);
    } else {
        LOG_INFO(Frontend, "xr: in-game menu: {}{}", Menu::LabelFor(t.id),
                 t.index >= 0 ? fmt::format(" ({})", t.index) : std::string{});
    }
    switch (t.id) {
    case Id::TabButton:
        remap.Done(); // AzaharFrame remap
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
    // AzaharFrame autosave: Load State's card (owner's Option B)
    case Id::AutosaveCard:
        if (model.autosaves.size() == 1) {
            // Keep 1 (or the only one so far): loads it (AzaharFrame advfix: as drawn)
            ask_text(MenuCommand::LoadAutosave, model.autosaves.front().id, 0);
            view.page = Menu::Page::Tabs;
        } else if (model.autosaves.size() > 1) {
            view.open_menu = Id::AutosaveCard;
        }
        break;
    case Id::UseGlobal: // AzaharFrame useglobal: one row (no chip draws it since round 24)
        if (t.index > 0 && t.index < Menu::kIdCount) {
            UseGlobal(static_cast<Id>(t.index), host);
        }
        break;
    case Id::AutosaveSettings: // "Autosave: Off": the Saves rows
        view.tab = Menu::Tab::Settings;
        view.page = Menu::Page::Tabs;
        view.section = Menu::SettingsSection::General;
        view.scroll[static_cast<std::size_t>(Menu::Tab::Settings)] = 7.0f * Menu::kRowH;
        break;
    case Id::UndoLoad:
        ask(MenuCommand::UndoLoad);
        view.page = Menu::Page::Tabs;
        break;
    case Id::Autosave:
        SetSetting(t.id, model.autosave ? 0 : 1, host);
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
    case Id::ReportProblem: // AzaharFrame crashreport
        ask(MenuCommand::ReportProblem);
        break;
    case Id::AboutApp:
        ask(MenuCommand::About);
        break;
    // AzaharFrame cheatstab: the Mods tab and its Cheats segment
    case Id::ModsSegment:
        if (t.index >= 0 && t.index <= static_cast<int>(Menu::ModsSegment::Ctgp7) &&
            (t.index != static_cast<int>(Menu::ModsSegment::Ctgp7) || model.mods.ctgp7.shown) &&
            static_cast<Menu::ModsSegment>(t.index) != view.mods_segment) {
            view.mods_segment = static_cast<Menu::ModsSegment>(t.index);
            view.scroll[static_cast<std::size_t>(Menu::Tab::Mods)] = 0.0f;
        }
        break;
    case Id::CheatsWindow:
        ask(MenuCommand::Cheats);
        break;
    case Id::HomebrewInstall: // AzaharFrame emerald: the Homebrew tab (citra_qt/frame_homebrew.cpp)
    case Id::HomebrewReinstall: // AzaharFrame ui3: Install again (Run without an update)
    case Id::HomebrewUpdate:
    case Id::HomebrewPlay:
    case Id::HomebrewCheck: // AzaharFrame ui3
        if (t.index >= 0 && t.index < static_cast<int>(model.homebrew.items.size())) {
            const MenuCommand c = t.id == Id::HomebrewInstall || t.id == Id::HomebrewReinstall
                                      ? MenuCommand::HomebrewInstall
                                  : t.id == Id::HomebrewUpdate ? MenuCommand::HomebrewUpdate
                                  : t.id == Id::HomebrewCheck  ? MenuCommand::HomebrewCheck
                                                               : MenuCommand::HomebrewPlay;
            ask_text(c, model.homebrew.items[static_cast<std::size_t>(t.index)].id);
        }
        break;
    case Id::HomebrewUninstall: // AzaharFrame emhb: its dialog's state is the Qt side's
        if (t.index >= 0 && t.index < static_cast<int>(model.homebrew.items.size())) {
            ask_text(MenuCommand::HomebrewUninstall,
                     model.homebrew.items[static_cast<std::size_t>(t.index)].id, 0);
        }
        break;
    case Id::HomebrewDialogCheck:
    case Id::HomebrewDialogCancel:
    case Id::HomebrewDialogConfirm:
        if (!model.homebrew.dialog.id.empty()) {
            ask_text(MenuCommand::HomebrewUninstall, model.homebrew.dialog.id,
                     t.id == Id::HomebrewDialogCheck    ? 1
                     : t.id == Id::HomebrewDialogCancel ? 2
                                                        : 3);
        }
        break;
    case Id::DownloadCheats:
        ask(MenuCommand::DownloadCheats);
        break;
    case Id::AddCheat: // AzaharFrame ui2: the keyboard shows over the menu (frame_cheats.cpp)
        ask(MenuCommand::AddCheat);
        break;
    case Id::InstallCheatFile: // AzaharFrame ui2: Install from Zip's browser, for a cheat file
        view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
        ask(MenuCommand::InstallFromZip, static_cast<int>(Menu::ZipTarget::Cheats));
        break;
    case Id::CheatsOtherVersion: // AzaharFrame scroll: opens or closes, kept per game this session
        view.cheats_other_open = !view.cheats_other_open;
        cheats_other_open[model.game_title] = view.cheats_other_open;
        break;
    case Id::Cheat:
        if (t.index >= 0) {
            // AzaharFrame compat: a cheat for another version asks before it turns on
            const auto& cheats = model.mods.cheats.cheats;
            if (t.index < static_cast<int>(cheats.size())) {
                const auto& cheat = cheats[static_cast<std::size_t>(t.index)];
                if (!cheat.enabled && !cheat.version_note.empty()) {
                    view.confirm = {Id::Cheat, t.index, cheat.name};
                    break;
                }
            }
            ask(MenuCommand::ToggleCheat, t.index);
        }
        break;
    // AzaharFrame texui: the Mods tab's Mods and Textures segments (citra_qt/frame_mods.cpp)
    case Id::CustomTextures:
        SetSetting(t.id, model.mods.textures.custom_textures ? 0 : 1, host);
        break;
    case Id::TexturePreload:
        if (model.mods.textures.preload) {
            SetSetting(t.id, 0, host);
        } else {
            // AzaharFrame memguard: "Preload Custom Textures" with the memory numbers first (the Qt
            // side fills the Textures dialog); Turn On sets it below
            ask(MenuCommand::AskPreload, 1);
        }
        break;
    case Id::TexturePack: // AzaharFrame texlayers: its checkbox, on or off
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.packs.size())) {
            const auto& p = model.mods.textures.packs[static_cast<std::size_t>(t.index)];
            ask_text(MenuCommand::SelectTexturePack, p.id, p.active ? 0 : 1);
        }
        break;
    case Id::TextureLayer:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.layers.size())) {
            ask(MenuCommand::TextureLayer, t.index);
        }
        break;
    case Id::LayerUp:
    case Id::LayerDown:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.layers.size())) {
            ask_text(MenuCommand::MoveTextureLayer, t.id == Id::LayerUp ? "up" : "down", t.index);
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
    // AzaharFrame texparts: a pack's Customize page
    case Id::PackCustomize:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.packs.size())) {
            view.textures_page = Menu::TexturesPage::Parts;
            view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
            ask_text(MenuCommand::OpenTextureParts,
                     model.mods.textures.packs[static_cast<std::size_t>(t.index)].id);
        }
        break;
    case Id::PartsBack:
        view.textures_page = Menu::TexturesPage::Packs;
        view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
        break;
    // AzaharFrame ui3: Optimize for Frame on the pack's page; the snackbar's Optimize opens it
    case Id::OptimizePack:
        ask_text(MenuCommand::OptimizePack, model.mods.textures.parts.pack_id);
        break;
    case Id::OptimizeStop:
        ask_text(MenuCommand::OptimizeStop, model.mods.textures.parts.pack_id);
        break;
    case Id::UseOriginal:
        ask_text(MenuCommand::UseOriginalPack, model.mods.textures.parts.pack_id);
        break;
    case Id::RemoveOriginal:
        ask_text(MenuCommand::RemoveOriginalPack, model.mods.textures.parts.pack_id);
        break;
    case Id::OptimizeNudge:
        if (!model.mods.textures.nudge_pack.empty()) {
            view.textures_page = Menu::TexturesPage::Parts;
            view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
            ask_text(MenuCommand::OpenTextureParts, model.mods.textures.nudge_pack);
        }
        break;
    case Id::PartsAllOn:
    case Id::PartsAllOff:
        ask(MenuCommand::TexturePartsAll, t.id == Id::PartsAllOn ? 1 : 0);
        break;
    case Id::TexturePart:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.parts.rows.size())) {
            ask(MenuCommand::TexturePart, t.index);
        }
        break;
    case Id::PartUp:
    case Id::PartDown:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.parts.rows.size())) {
            ask_text(MenuCommand::MoveTexturePart, t.id == Id::PartUp ? "up" : "down", t.index);
        }
        break;
    case Id::PackDownload:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.textures.downloads.size())) {
            const auto& o = model.mods.textures.downloads[static_cast<std::size_t>(t.index)];
            if (o.mismatch || o.unscanned) { // AzaharFrame compat: "This May Not Work" first
                view.confirm = {Id::PackDownload, t.index, o.id};
                break;
            }
            ask_text(MenuCommand::DownloadPack, o.id);
        }
        break;
    case Id::PackCancel:
        ask(MenuCommand::CancelPackDownload);
        break;
    case Id::ModCancel: // AzaharFrame integ3
        ask(MenuCommand::CancelModInstall);
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
    case Id::TurnBackOn: // AzaharFrame compat: the "Turned Off" snackbar's action
        ask(MenuCommand::TurnBackOn);
        break;
    // AzaharFrame zipinstall: Install from Zip (citra_qt/frame_mods.cpp)
    case Id::InstallFromZip:
        if (t.index == static_cast<int>(Menu::ZipTarget::Textures) ||
            t.index == static_cast<int>(Menu::ZipTarget::Mods) ||
            t.index == static_cast<int>(Menu::ZipTarget::Cheats)) { // AzaharFrame ui2
            view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
            ask(MenuCommand::InstallFromZip, t.index);
        }
        break;
    case Id::ZipBack:
        view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
        ask(MenuCommand::ZipClose);
        break;
    case Id::ZipChip:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.zip.chips.size())) {
            view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
            ask_text(MenuCommand::ZipBrowse,
                     model.mods.zip.chips[static_cast<std::size_t>(t.index)].path);
        }
        break;
    case Id::ZipUp:
        if (model.mods.zip.can_go_up) {
            view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
            const std::string& folder = model.mods.zip.folder;
            const std::size_t slash = folder.find_last_of('/');
            ask_text(MenuCommand::ZipBrowse,
                     slash == std::string::npos || slash == 0 ? std::string("/") : folder.substr(0, slash));
        }
        break;
    case Id::ZipEntry:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.zip.entries.size())) {
            const Menu::ZipEntry& e = model.mods.zip.entries[static_cast<std::size_t>(t.index)];
            const std::string& folder = model.mods.zip.folder;
            const std::string path = (folder == "/" ? folder : folder + "/") + e.name;
            if (e.folder) {
                view.scroll[static_cast<std::size_t>(view.tab)] = 0.0f;
                ask_text(MenuCommand::ZipBrowse, path);
            } else {
                ask_text(MenuCommand::ZipPick, path);
            }
        }
        break;
    case Id::ZipChoice:
        if (t.index >= 0 && t.index < static_cast<int>(model.mods.zip.choices.size())) {
            ask(MenuCommand::ZipChoose, t.index);
        }
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
            const auto& o = model.mods.mods.available[static_cast<std::size_t>(t.index)];
            // AzaharFrame ctgp7: a mod for CTGP-7 while CTGP-7 isn't installed: "Requires CTGP-7"
            if (o.requires_ctgp7 && !model.mods.ctgp7.installed && model.mods.ctgp7.shown) {
                view.requires_ctgp7 = true;
                break;
            }
            if (o.mismatch || o.unscanned) { // AzaharFrame compat: "This May Not Work" first
                view.confirm = {Id::ModInstall, t.index, o.id};
                break;
            }
            ask_text(MenuCommand::InstallMod, o.id);
        }
        break;
    case Id::ModLink: { // "Open GitHub?" first
        const auto& mm = model.mods.mods;
        const bool install = view.mods_page == Menu::ModsPage::Install;
        const std::size_t n = install ? mm.available.size() : mm.loaded.size();
        if (t.index >= 0 && static_cast<std::size_t>(t.index) < n) {
            const std::string& url = install ? mm.available[static_cast<std::size_t>(t.index)].url
                                             : mm.loaded[static_cast<std::size_t>(t.index)].url;
            if (!Menu::LinkText(url).empty()) { // AzaharFrame modcat: GameBanana pages too
                view.open_link = url;
            }
        }
        break;
    }
    case Id::DialogExtra: // AzaharFrame memwarn: only a notice has one
        if (model.notice.kind != Menu::ModsDialogKind::None) {
            ask(MenuCommand::NoticeAnswer, 2);
        } else if (view.mods_segment == Menu::ModsSegment::Ctgp7 && // AzaharFrame ctgp7
                   model.mods.ctgp7.dialog.kind != Menu::ModsDialogKind::None) {
            ask_text(MenuCommand::Ctgp7Dialog, view.dialog_check ? "checked" : "", 2);
            view.dialog_check = false;
        }
        break;
    case Id::DialogCheck: // AzaharFrame ctgp7: a dialog's checkbox
        view.dialog_check = !view.dialog_check;
        break;
    case Id::DialogConfirm:
    case Id::DialogCancel: {
        if (model.notice.kind != Menu::ModsDialogKind::None) { // AzaharFrame savestate: drawn on top
            ask(MenuCommand::NoticeAnswer, t.id == Id::DialogConfirm ? 1 : 0);
            break;
        }
        if (ZipShown(model, view) && model.mods.zip.dialog.kind != Menu::ModsDialogKind::None) {
            ask(MenuCommand::ZipDialog, t.id == Id::DialogConfirm ? 1 : 0); // AzaharFrame zipinstall
            break;
        }
        // AzaharFrame ctgp7: "Requires CTGP-7": Open CTGP-7 shows the segment
        if (view.requires_ctgp7 && view.mods_segment == Menu::ModsSegment::Mods) {
            view.requires_ctgp7 = false;
            if (t.id == Id::DialogConfirm && model.mods.ctgp7.shown) {
                LOG_INFO(Frontend, "xr: in-game menu: Requires CTGP-7: Open CTGP-7");
                view.mods_segment = Menu::ModsSegment::Ctgp7;
                view.mods_page = Menu::ModsPage::Loaded;
                view.scroll[static_cast<std::size_t>(Menu::Tab::Mods)] = 0.0f;
            }
            break;
        }
        // AzaharFrame ctgp7: the segment's question (the Qt side's), else Official Website's link
        if (view.mods_segment == Menu::ModsSegment::Ctgp7) {
            const bool yes = t.id == Id::DialogConfirm;
            if (model.mods.ctgp7.dialog.kind != Menu::ModsDialogKind::None) {
                ask_text(MenuCommand::Ctgp7Dialog, view.dialog_check ? "checked" : "", yes ? 1 : 0);
                view.dialog_check = false;
            } else if (!view.open_link.empty()) {
                const std::string url = std::exchange(view.open_link, {});
                if (yes) {
                    ask_text(MenuCommand::OpenUrl, url);
                }
            }
            break;
        }
        // AzaharFrame compat: "This May Not Work": Install Anyway, Download Anyway, Turn On Anyway
        // (the model's own question, drawn first, is answered first)
        if (view.confirm.id != Id::None) {
            const Menu::MenuView::Confirm c = std::exchange(view.confirm, {});
            const bool own = (view.mods_segment == Menu::ModsSegment::Textures
                                  ? model.mods.textures.dialog
                                  : model.mods.mods.dialog)
                                 .kind != Menu::ModsDialogKind::None;
            Menu::MenuView v = view;
            v.confirm = c;
            if (!own && Menu::CompatConfirmDialog(model, v).kind != Menu::ModsDialogKind::None) {
                if (t.id == Id::DialogConfirm) {
                    const int allowed = Menu::CompatConfirmFlags(model, v);
                    if (c.id == Id::ModInstall) {
                        ask_text(MenuCommand::InstallMod, c.key, allowed);
                    } else if (c.id == Id::PackDownload) {
                        ask_text(MenuCommand::DownloadPack, c.key, allowed);
                    } else if (c.id == Id::Cheat) {
                        ask(MenuCommand::ToggleCheat, c.index);
                    }
                    LOG_INFO(Frontend, "xr: in-game menu: {} anyway", c.key);
                }
                break;
            }
            if (!own) {
                break; // the row went: nothing to answer
            }
            view.confirm = c; // after the model's question
        }
        // The segment's own dialog (the Qt side's), else the link's (the view's).
        const bool textures = view.mods_segment == Menu::ModsSegment::Textures;
        const auto& dialog = textures ? model.mods.textures.dialog : model.mods.mods.dialog;
        const bool yes = t.id == Id::DialogConfirm;
        if (dialog.kind == Menu::ModsDialogKind::None && !view.open_link.empty()) {
            const std::string url = std::exchange(view.open_link, {});
            if (yes) {
                ask_text(MenuCommand::OpenUrl, url);
            }
        } else if (textures && dialog.kind == Menu::ModsDialogKind::Preload) {
            // AzaharFrame memguard: Turn On is the setting's own change (per game or global as
            // the switch is); the Qt side only closes its question
            if (yes) {
                LOG_INFO(Frontend, "xr: in-game menu: Preload Custom Textures: Turn On");
                SetSetting(Id::TexturePreload, 1, host);
            }
            ask(MenuCommand::TexturesDialog, 0);
        } else {
            ask(textures ? MenuCommand::TexturesDialog : MenuCommand::ModsDialog, yes ? 1 : 0);
        }
        break;
    }
    case Id::EditControls: // AzaharFrame remap
        BeginEdit();
        break;
    case Id::Ctgp7Install: // AzaharFrame ctgp7: the CTGP-7 segment (citra_qt/frame_ctgp7.cpp)
        ask(MenuCommand::Ctgp7Install);
        break;
    case Id::Ctgp7Launch:
        ask(MenuCommand::Ctgp7Launch);
        break;
    case Id::Ctgp7Cancel:
        ask(MenuCommand::Ctgp7Cancel);
        break;
    case Id::Ctgp7Uninstall:
        view.dialog_check = false;
        ask(MenuCommand::Ctgp7Uninstall);
        break;
    case Id::Ctgp7Website:
        view.open_link = Menu::kCtgp7Website; // "Open ctgp-7.github.io?" first
        break;
    case Id::RemapChip:
        remap.Choose(t.index);
        break;
    case Id::RemapCancel:
        remap.Cancel();
        break;
    case Id::RemapReset:
        EditChanged(remap.Reset(), host);
        break;
    case Id::RemapDone:
        remap.Done();
        break;
    case Id::MouseMode: { // AzaharFrame padsui: the mouse's, kept with its mapping
        Input::GamepadMapping mm = Input::Devices::GetMapping(Input::kMouseDeviceId);
        if (t.index >= 0 && t.index <= 1 && mm.mode != static_cast<uint8_t>(t.index)) {
            mm.mode = static_cast<uint8_t>(t.index);
            Input::Devices::SetMapping(Input::kMouseDeviceId, mm);
            ShowToastText(t.index == 1 ? "Mouse: Pointer" : "Mouse: Bottom Screen");
        }
        break;
    }
    case Id::UseDevice: { // AzaharFrame padsui: the device being edited; the Frame's is always on
        if (edit_device == Input::kFrameDeviceId) {
            break;
        }
        Input::GamepadMapping um = Input::Devices::GetMapping(edit_device);
        um.enabled = !um.enabled;
        Input::Devices::SetMapping(edit_device, um);
        LOG_INFO(Frontend, "xr: in-game menu: Use This Device {}", um.enabled ? "on" : "off");
        break;
    }
    case Id::RecentreAtEdge: { // AzaharFrame padsui: the device being edited
        if (edit_device == Input::kFrameDeviceId) {
            break;
        }
        Input::GamepadMapping rm = Input::Devices::GetMapping(edit_device);
        rm.recentre = !rm.recentre;
        Input::Devices::SetMapping(edit_device, rm);
        break;
    }
    case Id::EditDevice: { // AzaharFrame padsui + pads: another device's buttons
        if (t.index < 0 || t.index >= static_cast<int>(model.devices.list.size())) {
            break;
        }
        const Menu::EditDevice& d = model.devices.list[static_cast<std::size_t>(t.index)];
        if (d.key == edit_device) {
            break;
        }
        if (d.kind == Menu::DeviceKind::Frame) {
            BeginEdit();
        } else {
            BeginDeviceEdit(d.key, d.names == Menu::ButtonNames::Nintendo);
            if (d.kind == Menu::DeviceKind::Gamepad && d.has_touchpad) {
                Input::Devices::SetTouchpadDevice(d.key); // Touchpad Touch follows it
            }
        }
        break;
    }
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
    case Id::ReticleCenter: // AzaharFrame fixes7
        SetSetting(t.id, model.reticle_center ? 0 : 1, host);
        break;
    case Id::ReticleStick:
        SetSetting(t.id, model.reticle_stick ? 0 : 1, host);
        break;
    case Id::ReticleEdgeCStick: // AzaharFrame kiu
        SetSetting(t.id, model.reticle_edge_cstick ? 0 : 1, host);
        break;
    case Id::Recoil: // AzaharFrame kiugyro
        SetSetting(t.id, model.recoil ? 0 : 1, host);
        break;
    case Id::ShowControllers:
        SetSetting(t.id, model.show_controllers ? 0 : 1, host);
        break;
    case Id::DynamicResolution: // AzaharFrame menuux
        SetSetting(t.id, model.dynamic_resolution ? 0 : 1, host);
        break;
    case Id::FollowScreen: // AzaharFrame natural
        SetSetting(t.id, model.follow_screen ? 0 : 1, host);
        break;
    case Id::AsyncShaders:
        SetSetting(t.id, model.async_shaders ? 0 : 1, host);
        break;
    case Id::ImmersiveOffsetReset:
        SetSetting(t.id, 0, host);
        break;
    case Id::ScreenShape:
    case Id::ScreenCurve:
    case Id::ImmersiveMode:
    case Id::InternalResolution: // AzaharFrame menuux
    case Id::FastForwardAudio:   // AzaharFrame kiu2
        view.open_menu = t.id; // a menu button: its menu
        break;
    case Id::ControlsPage:
        controls_page = t.index;
        break;
    case Id::Scope: // AzaharFrame reticle: the scope toggle (Global, Per-Game), every tab's
        if (t.index >= 0) {
            scope = std::clamp(t.index, 0, 1);
            LOG_INFO(Frontend, "xr: in-game menu: scope: {}", scope ? "Per-Game" : "Global");
        }
        break;
    case Id::ScopeMore: // AzaharFrame reticle: ⋮: Use Global Values for All
        view.open_menu = Id::ScopeMore;
        break;
    case Id::SettingsSection: // AzaharFrame autosave: kept for the rest of the run
        if (t.index >= 0 && t.index < static_cast<int>(Menu::SettingsSection::Count) &&
            static_cast<Menu::SettingsSection>(t.index) != view.section) {
            view.section = static_cast<Menu::SettingsSection>(t.index);
            view.scroll[static_cast<std::size_t>(Menu::Tab::Settings)] = 0.0f;
        }
        break;
    case Id::ScreenLayout:
    case Id::MoveScreens:
    case Id::TouchpadTouch:
    case Id::BottomScreenReticle: // AzaharFrame mouse
    case Id::ReticleAtEdge:
    case Id::StatsPosition:
    case Id::Background:
    case Id::FaceButtons:
    case Id::MotionControls:
    case Id::GyroAim: // AzaharFrame kiugyro
    case Id::WindowScale:
    case Id::WiderView:
    case Id::MatchCamera: // AzaharFrame fovonce: Off | Set Once | Always
    case Id::ScaleMode:   // AzaharFrame fovscale: Stretch | Field of View
    case Id::TextureFilter:    // AzaharFrame menuux
    case Id::AccurateImageCopies: // AzaharFrame xfergpu: Off | Automatic | On
    case Id::FastForwardSpeed:
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
    if (model.game_title != cheats_other_game) {
        // AzaharFrame scroll: another game: its own For Another Version section (closed at first)
        cheats_other_game = model.game_title;
        const auto it = cheats_other_open.find(cheats_other_game);
        view.cheats_other_open = it != cheats_other_open.end() && it->second;
    }

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
    const bool listening = remap.Listening() >= 0; // AzaharFrame remap: A is then a button to map
    const bool a_edge = a && !a_down && !listening;
    a_down = a;
    const bool b = have_input && in.b;
    const bool b_edge = b && !b_down;
    b_down = b;

    if (remap.Editing()) {
        EditInput(in, have_input, dt, host); // AzaharFrame remap
    }

    // ---- Press, drag, release ----
    // AzaharFrame scroll: the list of the tab shown; another tab (or a click that moved it)
    // stops a fling and a stick's build-up.
    const auto list_scroll = [this]() -> float& {
        return view.scroll[static_cast<std::size_t>(view.tab)];
    };
    if (view.tab != scroll_tab || list_scroll() != scroll_seen) {
        scroll_tab = view.tab;
        list_drag.Stop();
        stick_scroll.Reset();
    }
    const bool can_scroll =
        layout.max_scroll > 0.0f && !layout.scroll_locked && view.open_menu == Id::None;
    const bool press_edge = press_hand < 0 && p >= 0 && (edge[p] || a_edge);
    if (press_edge) {
        list_drag.Stop(); // a press catches a coasting list
    }
    // AzaharFrame scroll: a press in the list's empty space can drag it too
    const bool on_list = press_edge && can_scroll && layout.bar.Valid() && hit[p].in_bounds &&
                         layout.bar.list.Contains(x, y);
    if (press_edge && hover.id == Id::Scrollbar && can_scroll && layout.bar.Valid()) {
        // AzaharFrame scroll: the thumb held (it follows the beam), or the track paged
        press_hand = p;
        view.pressed = hover;
        list_scroll() = bar_press.Begin(layout.bar, list_scroll(), y);
    } else if (press_edge && ((hover.id != Id::None && hover.id != Id::Scrollbar) || on_list)) {
        press_hand = p;
        view.pressed = hover;
        press_s = 0.0f;
        list_drag.Press(y, list_scroll(), press_s); // a press that moves scrolls the list
        if (Menu::SliderFor(hover.id).max > 1.0f && hover.index != Menu::kLabelIndex &&
            view.open_menu == Id::None) {
            view.dragging = hover.id;
        }
    }
    if (press_hand >= 0) {
        press_s += dt;
        if (bar_press.Active()) {
            if (p == press_hand && hit[p].on_plane && layout.bar.Valid()) {
                list_scroll() = bar_press.Update(layout.bar, list_scroll(), y, dt);
            }
        } else if (view.dragging != Id::None) {
            const float t = layout.SliderAt(view.dragging, x);
            if (t >= 0.0f) {
                view.drag_t = t;
            }
        } else if (can_scroll && p == press_hand && hit[p].on_plane) {
            // Dragged past the slop: the list follows the beam and nothing is clicked.
            float s = list_scroll();
            if (list_drag.Move(y, press_s, s, layout.max_scroll)) {
                list_scroll() = s;
                view.pressed = {};
            }
        }
        const bool let_go = !trigger_down[press_hand] && !a_down; // both let go
        if (let_go) {
            if (bar_press.Active()) {
                bar_press.End();
            } else if (list_drag.Dragging()) {
                // Let go while moving, it coasts on (no snapping to a row).
            } else if (view.dragging != Id::None) {
                const Menu::SliderRange r = Menu::SliderFor(view.dragging);
                const float value =
                    r.min + std::round(view.drag_t * (r.max - r.min) / r.step) * r.step;
                SetSetting(view.dragging, static_cast<int>(value), host);
                view.dragging = Id::None;
            } else if (view.pressed.id != Id::None && hover == view.pressed) {
                Click(hover, host);
            } else if (view.open_menu != Id::None && hover.id == Id::None) {
                view.open_menu = Id::None; // pressed off the menu
            }
            list_drag.Release(press_s);
            view.pressed = {};
            press_hand = -1;
        }
    } else if (p < 0 && (edge[0] || edge[1] || a_edge) && view.open_menu != Id::None) {
        view.open_menu = Id::None; // a click beside the panel closes a drop-down menu
    }
    if (bar_press.Active()) {
        hover = {Id::Scrollbar}; // AzaharFrame scroll: held, it stays wide wherever the beam is
    } else if (list_drag.Dragging()) {
        hover = {}; // no row lights up under a dragging beam
    }
    view.hover = hover;

    // ---- B: back ----
    if (b_edge && remap.Editing()) {
        // AzaharFrame remap: B stops listening, then leaves Edit Controls
        if (remap.Cancel() == Menu::Remap::Event::None) {
            remap.Done();
        }
    } else if (b_edge) {
        // AzaharFrame texui: the Mods segment's dialog (Cancel), then Install Mods (back).
        // The Textures segment's the same way: its dialog, then Download Packs.
        const bool mods_shown = view.tab == Menu::Tab::Mods &&
                                view.mods_segment == Menu::ModsSegment::Mods;
        const bool textures_shown = view.tab == Menu::Tab::Mods &&
                                    view.mods_segment == Menu::ModsSegment::Textures;
        const bool ctgp7_shown = view.tab == Menu::Tab::Mods && // AzaharFrame ctgp7
                                 view.mods_segment == Menu::ModsSegment::Ctgp7;
        // AzaharFrame zipinstall: Install from Zip's page over the segment it came from
        const auto& zip = model.mods.zip;
        const bool zip_shown = ZipShown(model, view);
        if (view.open_menu != Id::None) {
            view.open_menu = Id::None;
        } else if (view.tab == Menu::Tab::Mods && !zip_shown &&
                   Menu::CompatConfirmDialog(model, view).kind != Menu::ModsDialogKind::None) {
            Click({Id::DialogCancel}, host); // AzaharFrame compat: "This May Not Work"
        } else if (zip_shown && zip.dialog.kind != Menu::ModsDialogKind::None) {
            Click({Id::DialogCancel}, host);
        } else if (zip_shown) {
            if (!zip.working) {
                Click({Id::ZipBack}, host);
            }
        } else if (mods_shown && view.requires_ctgp7) { // AzaharFrame ctgp7
            Click({Id::DialogCancel}, host);
        } else if (ctgp7_shown && (model.mods.ctgp7.dialog.kind != Menu::ModsDialogKind::None ||
                                   !view.open_link.empty())) {
            Click({Id::DialogCancel}, host);
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
        } else if (textures_shown && view.textures_page == Menu::TexturesPage::Parts) {
            Click({Id::PartsBack}, host); // AzaharFrame texparts
        } else if (view.tab == Menu::Tab::Homebrew && !model.homebrew.dialog.id.empty()) {
            Click({Id::HomebrewDialogCancel}, host); // AzaharFrame emhb: Uninstall's dialog
        } else if (view.page != Menu::Page::Tabs) {
            view.page = Menu::Page::Tabs;
        } else {
            LOG_INFO(Frontend, "xr: in-game menu: B continues the game");
            Click({Id::Continue}, host);
        }
    }

    // ---- Sticks: scroll the list (AzaharFrame scroll: menu/list_scroll.h) ----
    float sy = 0.0f;
    if (have_input) {
        for (const auto& hand : in.hand) {
            if (std::fabs(hand.stick.y) > std::fabs(sy)) {
                sy = hand.stick.y;
            }
        }
    }
    {
        // Either stick; the further it is pushed the faster, and held over it speeds up. A
        // held thumb or a dragged list has the scroll to itself; let go, the stick stops it.
        float& s = list_scroll();
        const bool can = layout.max_scroll > 0.0f && !layout.scroll_locked &&
                         view.open_menu == Id::None && !bar_press.Active() &&
                         !list_drag.Dragging();
        if (can) {
            const float view_h = layout.bar.Valid() ? layout.bar.list.H() : Menu::kPanelH * 0.6f;
            s = stick_scroll.Update(sy, dt, s, layout.max_scroll, view_h, layout.scroll_step);
            if (stick_scroll.Active()) {
                list_drag.Stop();
            }
            s = list_drag.Coast(s, layout.max_scroll, dt);
        } else {
            stick_scroll.Reset();
            if (layout.scroll_locked || view.open_menu != Id::None) {
                list_drag.Stop();
            }
        }
        s = std::clamp(s, 0.0f, layout.max_scroll);
        scroll_seen = s;
    }

    // ---- The tooltip: a beam resting on a setting ----
    const Id rest = hover.id;
    const bool indexed = rest == Id::ModVersion || rest == Id::ModNote || rest == Id::PackNote ||
                         rest == Id::ModCompat || rest == Id::PackCompat || // AzaharFrame compat
                         rest == Id::PartUp || rest == Id::PartDown ||      // AzaharFrame texparts
                         rest == Id::LayerUp || rest == Id::LayerDown ||    // AzaharFrame texlayers
                         rest == Id::UseGlobal || // AzaharFrame useglobal: which row's chip
                         rest == Id::Scope ||     // AzaharFrame reticle: Per-Game's count
                         Menu::HasOptionTips(rest); // AzaharFrame xfergpu: a choice's own
    const int rest_idx = indexed ? hover.index : -1; // AzaharFrame modcat, modcat2
    if (rest != rest_id || rest_idx != rest_index) {
        rest_id = rest;
        rest_index = rest_idx;
        rest_s = 0.0f;
        view.tooltip = Id::None;
        view.tooltip_index = -1;
    } else if (rest != Id::None && view.pressed.id == Id::None) {
        rest_s += dt;
        const auto& offers = model.mods.mods.available;
        const auto& packs = model.mods.textures.downloads;
        const auto at_offer = [&](int i) -> const Menu::ModOffer* {
            return i >= 0 && i < static_cast<int>(offers.size()) ? &offers[static_cast<std::size_t>(i)] : nullptr;
        };
        bool row_text = false; // AzaharFrame modcat2: the chip's tooltip, a row's notes
        if (rest == Id::ModVersion) {
            row_text = at_offer(rest_idx) && !at_offer(rest_idx)->version_tip.empty();
        } else if (rest == Id::ModNote) {
            row_text = at_offer(rest_idx) && (!at_offer(rest_idx)->notes.empty() ||
                                              !at_offer(rest_idx)->mismatch_reason.empty());
        } else if (rest == Id::PackNote || rest == Id::PackCompat) {
            row_text = rest_idx >= 0 && rest_idx < static_cast<int>(packs.size()) &&
                       (rest == Id::PackNote ? !packs[static_cast<std::size_t>(rest_idx)].notes.empty() ||
                                                   !packs[static_cast<std::size_t>(rest_idx)].mismatch_reason.empty()
                                             : !packs[static_cast<std::size_t>(rest_idx)].mismatch_reason.empty());
        } else if (rest == Id::ModCompat) { // AzaharFrame compat
            row_text = at_offer(rest_idx) && !at_offer(rest_idx)->mismatch_reason.empty();
        } else if (rest == Id::PartUp || rest == Id::PartDown || // AzaharFrame texparts
                   rest == Id::LayerUp || rest == Id::LayerDown || // AzaharFrame texlayers
                   rest == Id::UseGlobal) { // AzaharFrame useglobal: "Use Global Value"
            // Move Up, Move Down
            row_text = true;
        } else if (rest == Id::Scope) { // AzaharFrame reticle: Per-Game, while picked
            row_text = rest_idx == 1 && model.scope == 1 &&
                       model.locked[static_cast<std::size_t>(Id::Scope)] == Lock::None;
        } else if (Menu::HasOptionTips(rest)) { // AzaharFrame xfergpu: a segment, or the label
            row_text = rest_idx >= 0 ? !Menu::ExplainOption(rest, rest_idx).empty()
                                     : !Menu::ExplainFor(rest).empty();
        }
        const bool has_text =
            row_text || (!indexed && !Menu::ExplainFor(rest).empty()) ||
            model.locked[static_cast<std::size_t>(rest)] != Lock::None;
        if (rest_s >= kTooltipRest && has_text) {
            view.tooltip = rest;
            view.tooltip_index = rest_idx;
        }
    }
    if (view.pressed.id != Id::None) {
        view.tooltip = Id::None;
        view.tooltip_index = -1;
    }

    view.remap = remap.View();

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
