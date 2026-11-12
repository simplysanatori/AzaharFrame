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

#include <fmt/format.h>
#include "common/param_package.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/input/default_bindings.h"
#include "frame_xr/input/devices.h" // AzaharFrame pads
#include "frame_xr/input/geometry.h"
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu_panel.h"
#include "frame_xr/session.h"
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

/// Per-game settings (Azahar's Properties have them too): locked in Global while the game has
/// its own value.
bool PerGameSetting(Id id) {
    switch (id) {
    case Id::HideBottomScreen:
    case Id::ScreenShape:
    case Id::ImmersiveMode:
    case Id::WiderView:
    case Id::GameFov:
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

} // namespace

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
    m.hide_bottom_screen = v.vr_hide_bottom_screen.GetValue();
    m.screen_shape = static_cast<int>(std::min<u32>(v.vr_top_aspect.GetValue(), 5));
    m.screen_curve = NearestStep(kCurveSteps, v.vr_screen_curve.GetValue());
    m.screen_layout = static_cast<int>(std::min<u32>(v.vr_screen_layout.GetValue(), 1));
    m.move_screens = static_cast<int>(std::min<u32>(v.vr_move_screens.GetValue(), 2)); // menu
    m.touchpad_touch = static_cast<int>(std::min<u32>(v.vr_touchpad_touch.GetValue(), 2));
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
                if (e.kind == Menu::DeviceKind::Mouse) {
                    e.extras = remap.Extras();
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
            const bool click_mode = m.touchpad_touch == 2;
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
    m.show_controllers = v.vr_show_controllers.GetValue();
    m.window_scale = NearestStep(kScaleSteps, v.vr_window_scale.GetValue());
    m.depth_3d = static_cast<int>(std::min<u32>(v.factor_3d.GetValue(), 100)); // depth3d
    m.immersive_mode = static_cast<int>(std::min<u32>(v.vr_immersive_mode.GetValue(), 3));
    m.wider_view = static_cast<int>(std::min<u32>(v.vr_immersive_wider_view_level.GetValue(), 4));
    m.game_fov = v.vr_immersive_game_fov.GetValue();
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
    static constexpr std::array<const char*, 5> kTabNames{"Main", "Position", "Stats",
                                                          "Controls", "Settings"};
    if (t.id == Id::TabButton) {
        LOG_INFO(Frontend, "xr: in-game menu: {} tab",
                 kTabNames[static_cast<std::size_t>(std::clamp(t.index, 0, 4))]);
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
    case Id::Cheats: // AzaharFrame menu
        ask(MenuCommand::Cheats);
        break;
    case Id::EditControls: // AzaharFrame remap
        BeginEdit();
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
    case Id::ShowControllers:
        SetSetting(t.id, model.show_controllers ? 0 : 1, host);
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
    if (press_hand < 0 && p >= 0 && (edge[p] || a_edge) && hover.id != Id::None) {
        press_hand = p;
        view.pressed = hover;
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
        }
        const bool let_go = !trigger_down[press_hand] && !a_down; // both let go
        if (let_go) {
            if (view.dragging != Id::None) {
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
    if (b_edge && remap.Editing()) {
        // AzaharFrame remap: B stops listening, then leaves Edit Controls
        if (remap.Cancel() == Menu::Remap::Event::None) {
            remap.Done();
        }
    } else if (b_edge) {
        if (view.open_menu != Id::None) {
            view.open_menu = Id::None;
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
            s = std::clamp(s + Menu::kRowH * static_cast<float>(dir), 0.0f, layout.max_scroll);
        }
    } else if (dir != 0) {
        stick_hold += dt;
        if (stick_hold >= stick_repeat) {
            stick_repeat += kStickRepeat;
            float& s = view.scroll[static_cast<std::size_t>(view.tab)];
            s = std::clamp(s + Menu::kRowH * static_cast<float>(dir), 0.0f, layout.max_scroll);
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
        }
    }
    if (chain.HasImage() && info.should_render) {
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
