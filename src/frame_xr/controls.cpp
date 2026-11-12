// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

#include "common/file_util.h" // AzaharFrame move
#include "common/logging/log.h"
#include "common/settings.h" // AzaharFrame curve
#include "frame_xr/controls.h"
#include "frame_xr/immersive.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/picker/library.h"
#include "frame_xr/picker/library_loader.h"
#include "frame_xr/picker/logo_loader.h"
#include "frame_xr/picker/picker.h"
#include "frame_xr/session.h"

namespace FrameXr {

namespace {

namespace picker = frame_xr::picker;

// The picker panel: 1.6 m wide (1536 x 1024 texels), 2 m ahead at eye height.
constexpr float kPickerWidth = 1.6f;
constexpr float kPickerHeight = kPickerWidth * 1024.0f / 1536.0f;
// The bottom screen follows the top one (input/screen_rig.h: at size 1, 0.75 m lower, 0.5 m
// nearer, tilted 45 degrees back, as CitraVR). The top screen stays at least 1 m ahead.
namespace Rig = Input::ScreenRig;
constexpr float kNearestTopZ = Rig::kNearestTopZ;
// The pointer dot texture.
constexpr uint32_t kDotSize = 64;
// The in-game menu: the panel this far ahead (nearer than the screens), as wide as the home
// panel looks (1.6 m at 2 m), and a dark quad (alpha kDimAlpha) behind it over the game.
constexpr float kMenuDistance = 1.2f;
constexpr float kMenuNearest = 0.6f;
constexpr float kMenuWidthPerMetre = 0.8f;
constexpr uint32_t kDimSize = 8;
// AzaharFrame controls: the View hold's label, and the grip hint chip (shown once per grab, not
// again within kHintQuietNs; "↕" and "↔" are drawn as arrows by the HUD).
constexpr const char* kStoppingText = "Stopping...";
constexpr const char* kHintNormal = "D-Pad \xE2\x86\x95 Curve   \xE2\x86\x94 Nudge\nStick: Size, Distance";
constexpr const char* kHintSecond = "D-Pad \xE2\x86\x94 Nudge\nStick: Size, Distance"; // AzaharFrame move
constexpr const char* kHintImmersive =
    "D-Pad \xE2\x86\x95 Wider View   \xE2\x86\x94 Offset\nStick: Size, Distance";
constexpr int64_t kHintQuietNs = 10'000'000'000;
constexpr uint8_t kDimAlpha = 150;
constexpr XrTime kMenuHoldNs = 1'000'000'000;
constexpr XrTime kStopHoldNs = 1'500'000'000;
constexpr float kStickPage = 0.6f;

Input::Posef ToInput(const XrPosef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef ToXr(const Input::Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef BottomFromTop(const XrPosef& top, float scale) {
    return ToXr(Rig::BottomFromTop(ToInput(top), scale));
}

// AzaharFrame move: Detach Screens. The placement's slots (input/screen_rig.h Slots), and the
// per-game file in the user directory.
constexpr const char* kScreenPlacementFile = "screen-placement.txt";

Rig::Slots SlotsOf(const ScreenPlacement& p) {
    Rig::Slots s;
    s.main = ToInput(p.top_pose);
    s.main_scale = p.top_scale;
    s.second_free = p.second_free;
    s.second = ToInput(p.second_pose);
    s.second_scale = p.second_scale;
    return s;
}

void PutSlots(ScreenPlacement& p, const Rig::Slots& s) {
    p.top_pose = ToXr(s.main);
    p.top_scale = s.main_scale;
    p.top_size = NormalTopSize(s.main_scale);
    p.second_free = s.second_free;
    p.second_pose = ToXr(s.second);
    p.second_scale = s.second_scale;
}

uint32_t DetachMode() {
    return Rig::ClampDetach(Settings::values.vr_move_screens.GetValue());
}

void LogSlots(const char* what, const Rig::Slots& s) {
    LOG_INFO(Frontend, "xr: screens {}: main at {:.2f}, {:.2f}, {:.2f} size {:.2f}; second {}", what,
             s.main.position.x, s.main.position.y, s.main.position.z, s.main_scale,
             s.second_free ? fmt::format("at {:.2f}, {:.2f}, {:.2f} size {:.2f}",
                                         s.second.position.x, s.second.position.y,
                                         s.second.position.z, s.second_scale)
                           : std::string("linked"));
}

// A white dot with a dark rim, premultiplied alpha, sRGB values.
std::vector<uint8_t> MakeDot() {
    std::vector<uint8_t> px(kDotSize * kDotSize * 4);
    const float c = (kDotSize - 1) / 2.0f;
    const float r = kDotSize / 2.0f - 1.0f;
    for (uint32_t y = 0; y < kDotSize; y++) {
        for (uint32_t x = 0; x < kDotSize; x++) {
            const float d = std::hypot(x - c, y - c);
            const float alpha = std::clamp(r - d, 0.0f, 1.0f);
            const float white = std::clamp(r * 0.72f - d, 0.0f, 1.0f);
            const float v = white * 255.0f + (1.0f - white) * 40.0f;
            uint8_t* p = &px[(y * kDotSize + x) * 4];
            p[0] = p[1] = p[2] = static_cast<uint8_t>(v * alpha);
            p[3] = static_cast<uint8_t>(alpha * 255.0f);
        }
    }
    return px;
}

} // namespace

VrUi::VrUi() = default;
VrUi::~VrUi() = default;

void VrUi::Init() {
    library = std::make_shared<picker::Library>(picker::ReadMetadataWithLoader,
                                                picker::DefaultIconCacheDir());
    picker = std::make_unique<picker::Picker>(library);
    { // AzaharFrame: the "Azahar for Frame" lockup for the picker's header (text when missing).
        std::vector<uint8_t> logo;
        int logo_w = 0, logo_h = 0;
        const std::string logo_path = picker::FindArtFile("logo.png");
        if (!logo_path.empty() && picker::LoadPngFile(logo_path, logo, logo_w, logo_h)) {
            picker->SetLogo(logo, logo_w, logo_h);
        } else {
            LOG_WARNING(Frontend, "xr: art/logo.png not found, the picker shows the name as text");
        }
    }
}

bool VrUi::Create(Runtime& rt) {
    menu_shown = false;
    flat_menu_shown = false; // AzaharFrame flat4: a Flat menu open before VR closes with it
    picker_shown = false;
    bridge_live = false;
    chord_down = false;
    SwallowHeld(); // the buttons of the chord that made this session are still held
    mode_latch.Arm();
    actions_ok = actions.Create(rt.instance, rt.HasExtension(Input::Actions::kFrameControllerExtension));
    cylinder_ok = rt.HasExtension(XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME); // AzaharFrame curve
    if (actions_ok) {
        actions_ok = actions.Attach(rt.session);
    }
    if (!actions_ok) {
        LOG_ERROR(Frontend, "xr: controller actions unavailable; no VR input");
    }

    uploader.Init(rt.vulkan->Device(), rt.vulkan->PhysicalDevice(), rt.vulkan->QueueFamily(),
                  rt.queue_lock);
    if (uploader.CreateChain(rt.session, rt.swapchain_format, kDotSize, kDotSize, cursor_chain)) {
        const auto dot = MakeDot();
        uploader.Upload(cursor_chain, dot.data());
    }

    if (uploader.CreateChain(rt.session, rt.swapchain_format, kDimSize, kDimSize, dim_chain)) {
        std::vector<uint8_t> dim(kDimSize * kDimSize * 4, 0); // premultiplied black
        for (std::size_t i = 3; i < dim.size(); i += 4) {
            dim[i] = kDimAlpha;
        }
        uploader.Upload(dim_chain, dim.data());
    }
    visuals.Create(rt); // AzaharFrame vrvis
    tab_menu.Create(rt); // AzaharFrame tabmenu
    return true;
}

void VrUi::DestroyXr() {
    visuals.DestroyXr(); // AzaharFrame vrvis
    tab_menu.DestroyXr(); // AzaharFrame tabmenu
    if (bridge_live) {
        bridge.Release();
        bridge_live = false;
    }
    if (menu_shown) {
        mirror.SetPlacement(home_place);
        Immersive::SetMenuOpen(false);
        menu_shown = false;
    }
    mirror.Destroy(uploader);
    keyboard.Destroy(uploader); // AzaharFrame keyboard
    uploader.DestroyChain(dim_chain);
    uploader.DestroyChain(picker_chain);
    uploader.DestroyChain(cursor_chain);
    uploader.Shutdown();
    actions.Destroy();
    actions_ok = false;
    picker_shown = false;
    chord_down = false;
    SwallowHeld(); // flat starts with the chord still held
    mode_latch.Arm(); // and its gamepad may first report the chord up, then held again
}

void VrUi::Destroy() {
    DestroyXr();
    picker.reset();
    library.reset();
}

void VrUi::SetFlatInput(FlatInputSource source) {
    std::scoped_lock lock{flat_mutex};
    flat_input = std::move(source);
}

void VrUi::NotifyVr(bool vr) {
    std::scoped_lock lock{hooks_mutex};
    if (hooks.vr_changed) {
        hooks.vr_changed(vr);
    }
}

void VrUi::SwallowHeld() {
    swallow = {true, true, true, true, true, true, {true, true}, {true, true}, {true, true}};
}

bool VrUi::ModeChord(const Input::InputFrame& in) {
    const bool all = in.hand[0].bumper && in.hand[1].bumper && in.view && in.menu;
    const bool edge = all && !chord_down;
    chord_down = all;
    return edge;
}

void VrUi::RequestToggleVr(const char* where) {
    mode_latch.Arm(); // once per chord, whatever the input says until it is let go
    std::scoped_lock lock{hooks_mutex};
    if (hooks.toggle_vr) {
        LOG_INFO(Frontend, "xr: both bumpers + View + Menu ({}): switching VR / flat", where);
        hooks.toggle_vr();
    }
}

void VrUi::RequestFastForward(const Input::PanelCommands& cmd, const char* where) {
    if (!cmd.toggle_fast_forward) {
        return;
    }
    std::scoped_lock lock{hooks_mutex};
    if (hooks.toggle_fast_forward) {
        LOG_INFO(Frontend, "xr: {}: View, then Right Trigger: fast forward on / off", where);
        hooks.toggle_fast_forward();
    }
}

void VrUi::RequestQuick(const Input::PanelCommands& cmd, const char* where) {
    if (!cmd.quick_save && !cmd.quick_load) {
        return;
    }
    std::scoped_lock lock{hooks_mutex};
    auto& hook = cmd.quick_save ? hooks.quick_save : hooks.quick_load;
    if (hook) {
        LOG_INFO(Frontend, "xr: {}: View, then {} Bumper: Quick {}", where,
                 cmd.quick_save ? "Right" : "Left", cmd.quick_save ? "Save" : "Load");
        hook();
    }
}

void VrUi::FlatFrame(int64_t now_ns) {
    Input::InputFrame in;
    bool have = false;
    {
        std::scoped_lock lock{flat_mutex};
        have = flat_input && flat_input(in);
    }
    in.time_ns = now_ns;
    if (have) {
        mode_latch.Filter(in);
    }
    if (have != flat_had_input) {
        flat_had_input = have;
        LOG_INFO(Frontend, "xr: flat: {}", have ? "gamepad input" : "no gamepad");
    }
    if (!have || !game_running) {
        if (bridge_live) {
            bridge.Release();
            bridge_live = false;
        }
        if (have && ModeChord(in)) {
            RequestToggleVr("flat, no game");
        }
        if (!have) {
            chord_down = false;
        }
        // AzaharFrame gamelist: the game list with the D-pad, the left stick and A's double tap,
        // as on VR's window (there is no beam: one A does nothing here).
        if (have && !game_running) {
            const Input::ListNav::Result list =
                flat_list_nav.Update(mirror.Enabled() && mirror.ListNavEnabled(), in.dpad_up,
                                     in.dpad_down, in.hand[0].stick.y, in.a, now_ns);
            if (list.step != 0) {
                HomePanelEvent e{HomePanelEvent::Type::ListStep};
                e.step = list.step;
                mirror.Send(e);
            }
            if (list.activate) {
                mirror.Send({HomePanelEvent::Type::ListStart});
            }
        } else {
            flat_list_nav.Reset();
        }
        return;
    }
    flat_list_nav.Reset(); // AzaharFrame gamelist: a game runs; buttons held now are no press
    // AzaharFrame flat4: the Flat menu, Azahar's window over the paused game (Menu held 1 s).
    if (menu_open) {
        FlatMenuFrame(in, now_ns);
        return;
    }
    if (flat_menu_shown) {
        // Closed: what is still held stays away from the game until let go, as after VR's menu.
        flat_menu_shown = false;
        swallow = {in.a,
                   in.b,
                   in.x,
                   in.y,
                   in.menu,
                   in.view,
                   {in.hand[0].trigger > 0.40f, in.hand[1].trigger > 0.40f},
                   {in.hand[0].grip > 0.40f, in.hand[1].grip > 0.40f},
                   {in.hand[0].bumper, in.hand[1].bumper}};
        LOG_INFO(Frontend, "xr: flat: Flat menu closed, the game gets the buttons again");
    }
    for (auto& hand : in.hand) {
        // Flat has no aim: a default ray and no screens to hit (below), so the triggers are
        // ZL and ZR and the grips grab nothing. Active, or the mapper ignores the triggers.
        hand.active = true;
        hand.aim = {};
    }
    FilterAfterMenu(in);
    Input::PanelGeometry panels;
    panels.top.visible = false;
    panels.bottom.visible = false;
    panels.bottom_toggle_enabled = false; // the stick clicks lay out the window instead
    panels.grip_dpad_cycles_layout = true; // AzaharFrame layout: grip + D-pad left or right
    {
        const ViewPose view = LatestViewPose(); // the head drives Motion Controls: Headset
        in.head_active = view.valid;
        in.head = ToInput(view.head);
    }
    const Input::MapperOutput& out = bridge.Process(in, panels);
    bridge_live = true;
    LogFlatGameButtons(out.controls);
    const auto& cmd = out.commands;
    if (cmd.toggle_vr) {
        RequestToggleVr("flat");
    }
    RequestQuick(cmd, "flat"); // AzaharFrame vrqol
    RequestFastForward(cmd, "flat");
    // AzaharFrame flat3: the stick clicks change the window's screen layout, as they change the
    // screens in VR (swap, hide the other one, reset).
    const auto flat_screens = [this](bool fired, FlatScreens what, const char* name) {
        if (!fired) {
            return;
        }
        std::scoped_lock lock{hooks_mutex};
        if (hooks.flat_screens) {
            LOG_INFO(Frontend, "xr: flat: {}", name);
            hooks.flat_screens(what);
        }
    };
    flat_screens(cmd.swap_screens, FlatScreens::Swap, "right stick click: Swap Screens");
    flat_screens(cmd.reset_panels, FlatScreens::Reset, "right stick click held: flat layout");
    flat_screens(cmd.toggle_other_screen, FlatScreens::ToggleOther,
                 "left stick click: Single Screen on / off");
    // AzaharFrame layout: left stick click held, or a grip + D-pad right (next) or left.
    flat_screens(cmd.cycle_layout > 0, FlatScreens::CycleLayout, "next layout");
    flat_screens(cmd.cycle_layout < 0, FlatScreens::PreviousLayout, "previous layout");
    if (cmd.menu_toggle) {
        std::scoped_lock lock{hooks_mutex};
        if (hooks.toggle_menu) {
            LOG_INFO(Frontend, "xr: flat: Menu held, opening the Flat menu");
            hooks.toggle_menu();
        }
    }
    if (cmd.stop_game) {
        std::scoped_lock lock{hooks_mutex};
        if (hooks.stop_game) {
            LOG_INFO(Frontend, "xr: flat: View held, stopping the game");
            hooks.stop_game();
        }
    }
}

// AzaharFrame flat4: the Flat menu. The game sees no button; Menu held 1 s again closes the menu
// (and the game continues), B goes back (closes a menu or dialog that is open, else the Flat
// menu), View held stops the game, the four-button chord switches to VR.
void VrUi::FlatMenuFrame(const Input::InputFrame& in, int64_t now_ns) {
    if (!flat_menu_shown) {
        flat_menu_shown = true;
        if (bridge_live) {
            bridge.Release(); // nothing stays held for the game while the menu shows
            bridge_live = false;
        }
        menu_hold = {}; // Menu is still held from opening: a new hold closes the menu
        view_hold = {};
        flat_back_down = in.b; // a B held from before is no press
        LOG_INFO(Frontend, "xr: flat: Flat menu shown, the game gets no buttons");
    }
    if (ModeChord(in)) {
        RequestToggleVr("Flat menu");
    }
    const bool bumper = in.hand[0].bumper || in.hand[1].bumper;
    if (in.menu && (bumper || in.view)) {
        // A chord being formed (VR / flat): no close; a new Menu press starts a new hold.
        menu_hold.armed = false;
        menu_hold.down = false;
    }
    if (in.view && (bumper || in.menu)) {
        view_hold.armed = false;
        view_hold.down = false;
    }
    if (menu_hold.Update(in.menu, now_ns, kMenuHoldNs)) {
        std::scoped_lock lock{hooks_mutex};
        if (hooks.toggle_menu) {
            LOG_INFO(Frontend, "xr: flat: Menu held, closing the Flat menu");
            hooks.toggle_menu();
        }
    }
    if (view_hold.Update(in.view, now_ns, kStopHoldNs)) {
        std::scoped_lock lock{hooks_mutex};
        if (hooks.stop_game) {
            LOG_INFO(Frontend, "xr: flat: View held in the Flat menu, stopping the game");
            hooks.stop_game();
        }
    }
    const bool back = in.b && !flat_back_down;
    flat_back_down = in.b;
    if (back) {
        std::scoped_lock lock{hooks_mutex};
        if (hooks.menu_back) {
            LOG_INFO(Frontend, "xr: flat: B in the Flat menu");
            hooks.menu_back();
        }
    }
}

// Test driver (with AZAHARFRAME_TEST_FLAT_PAD, citra_qt/frame_mode.cpp): what the game is sent
// in flat, one line per change, so a mock run can check what reaches it.
void VrUi::LogFlatGameButtons(const Input::ControlState& controls) {
    static const bool enabled = std::getenv("AZAHARFRAME_TEST_FLAT_PAD") != nullptr;
    if (!enabled) {
        return;
    }
    static constexpr std::array<const char*, Input::kNumControls> kNames{
        "A",  "B",  "X",    "Y",      "Up",  "Down",   "Left",        "Right",       "GripL",
        "GripR", "ZL", "ZR", "L", "R", "Start", "Select", "StickClickL", "StickClickR"};
    for (int i = 0; i < Input::kNumControls; i++) {
        const bool now = controls.buttons[static_cast<std::size_t>(i)];
        if (now != flat_sent[static_cast<std::size_t>(i)]) {
            flat_sent[static_cast<std::size_t>(i)] = now;
            LOG_INFO(Frontend, "xr test: flat: the game sees {} {}", kNames[i], now ? "down" : "up");
        }
    }
}

void VrUi::SetFrontendHooks(FrontendHooks new_hooks) {
    visuals.SetStatsChanged(new_hooks.show_stats_changed); // AzaharFrame vrvis
    std::scoped_lock lock{hooks_mutex};
    hooks = std::move(new_hooks);
}

void VrUi::SetGameRunning(bool running) {
    game_running = running;
    launch_pending = false;
}

void VrUi::SetHomePanel(std::function<void(const HomePanelEvent&)> on_event) {
    mirror.SetHandler(std::move(on_event));
}

void VrUi::SetHomePanelImage(std::shared_ptr<const HomePanelImage> image) {
    mirror.SetImage(std::move(image));
}

void VrUi::SetInGameMenu(bool open) {
    if (!open) {
        menu_window = false; // the next one opens on the menu
    }
    menu_open = open;
}

void VrUi::SetInGameMenuWindow(bool window) {
    menu_window = window;
}

void VrUi::SetInGameMenuInfo(const std::string& title, const std::vector<MenuSaveSlot>& slots,
                             bool per_game) {
    tab_menu.SetInfo(title, slots, per_game);
}

bool VrUi::Hold::Update(bool pressed, XrTime now, XrTime need) {
    if (!pressed) {
        armed = true;
        down = false;
        return false;
    }
    if (!armed) {
        return false;
    }
    if (!down) {
        down = true;
        since = now;
        fired = false;
    }
    if (!fired && now - since >= need) {
        fired = true;
        return true;
    }
    return false;
}

void VrUi::EnterMenu() {
    if (bridge_live) {
        bridge.Release(); // the game sees nothing held while the menu shows
        bridge_live = false;
    }
    home_place = mirror.GetPlacement();
    mirror.Hide();
    keyboard.Hide(); // AzaharFrame keyboard
    // In front of the head (level), nearer than the game screens.
    const ViewPose view = LatestViewPose();
    const Input::Vec3f head = view.valid ? ToInput(view.head).position : Input::Vec3f{0, 0, 0};
    Input::Vec3f forward = view.valid ? Input::Rotate(ToInput(view.head).orientation, {0, 0, -1})
                                      : Input::Vec3f{0, 0, -1};
    forward.y = 0.0f;
    forward = Input::Length(forward) > 1e-3f ? Input::Normalized(forward) : Input::Vec3f{0, 0, -1};
    const ScreenPlacement place = GetScreenPlacement();
    const float top = Input::Length(ToInput(place.top_pose).position - head);
    const float bottom = Input::Length(ToInput(place.bottom_pose).position - head);
    const float d =
        std::clamp(std::min({kMenuDistance, top - 0.2f, bottom - 0.2f}), kMenuNearest, kMenuDistance);
    const Input::Vec3f at = head + forward * d;
    const Input::Posef pose{Input::FaceViewer(at, head, {0, 1, 0}), at};
    mirror.Place(ToXr(pose), kMenuWidthPerMetre * d);
    tab_menu.Open(ToXr(pose), kMenuWidthPerMetre * d, visuals.MenuStats()); // AzaharFrame tabmenu
    menu_hold = {}; // Menu is still held from opening: a new hold closes the menu
    view_hold = {};
    Immersive::SetMenuOpen(true);
    menu_shown = true;
    LOG_INFO(Frontend, "xr: in-game menu shown {:.2f} m ahead", d);
}

void VrUi::LeaveMenu(const Input::InputFrame& in) {
    tab_menu.Close(); // AzaharFrame tabmenu
    mirror.Hide();
    keyboard.Hide(); // AzaharFrame keyboard
    mirror.SetPlacement(home_place);
    Immersive::SetMenuOpen(false);
    menu_shown = false;
    swallow = {in.a,
               in.b,
               in.x,
               in.y,
               in.menu,
               in.view,
               {in.hand[0].trigger > 0.40f, in.hand[1].trigger > 0.40f},
               {in.hand[0].grip > 0.40f, in.hand[1].grip > 0.40f},
               {in.hand[0].bumper, in.hand[1].bumper}};
    LOG_INFO(Frontend, "xr: in-game menu hidden");
}

void VrUi::FilterAfterMenu(Input::InputFrame& in) {
    const auto button = [](bool& swallowed, bool& pressed) {
        if (swallowed && !pressed) {
            swallowed = false;
        }
        if (swallowed) {
            pressed = false;
        }
    };
    button(swallow.a, in.a);
    button(swallow.b, in.b);
    button(swallow.x, in.x);
    button(swallow.y, in.y);
    button(swallow.menu, in.menu);
    button(swallow.view, in.view);
    for (int h = 0; h < Input::kNumHands; h++) {
        const auto axis = [](bool& swallowed, float& value) {
            if (swallowed && value <= 0.40f) {
                swallowed = false;
            }
            if (swallowed) {
                value = 0.0f;
            }
        };
        axis(swallow.trigger[h], in.hand[h].trigger);
        axis(swallow.grip[h], in.hand[h].grip);
        button(swallow.bumper[h], in.hand[h].bumper);
    }
}

void VrUi::MenuFrame(Runtime& rt, const FrameInfo& info, const Input::InputFrame& in,
                     bool have_input, std::vector<Quad>& layers) {
    if (have_input && ModeChord(in)) {
        RequestToggleVr("in-game menu");
    }
    if (have_input) {
        if (in.menu && (in.hand[0].bumper || in.hand[1].bumper || in.view)) {
            // A chord being formed (VR / flat): no close; a new Menu press starts a new hold.
            menu_hold.armed = false;
            menu_hold.down = false;
        }
        if (menu_hold.Update(in.menu, info.display_time, kMenuHoldNs)) {
            std::scoped_lock lock{hooks_mutex};
            if (hooks.toggle_menu) {
                LOG_INFO(Frontend, "xr: Menu held, closing the in-game menu");
                hooks.toggle_menu();
            }
        }
        if (view_hold.Update(in.view, info.display_time, kStopHoldNs)) {
            std::scoped_lock lock{hooks_mutex};
            if (hooks.stop_game) {
                LOG_INFO(Frontend, "xr: View held, stopping the game");
                hooks.stop_game();
            }
        }
    }
    const bool window = menu_window; // AzaharFrame tabmenu: Azahar's window, else the menu
    if (info.should_render && dim_chain.has_image) {
        Quad dim;
        dim.swapchain = dim_chain.handle;
        dim.image_rect = {{0, 0}, {static_cast<int32_t>(kDimSize), static_cast<int32_t>(kDimSize)}};
        dim.pose = window ? mirror.Pose() : tab_menu.Pose();
        // AzaharFrame vrqol: 2 cm behind the window. SteamVR depth-sorts quad layers (Frame
        // port notes), and a dim quad in the window's own plane would fight it.
        const Input::Vec3f behind = Input::Rotate(ToInput(dim.pose).orientation, {0, 0, -0.02f});
        dim.pose.position.x += behind.x;
        dim.pose.position.y += behind.y;
        dim.pose.position.z += behind.z;
        const float w = window ? mirror.Width() : tab_menu.Width();
        dim.size = {w * 6.0f, w * 5.0f};
        dim.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        layers.push_back(dim);
    }
    if (window) {
        tab_menu.Close();
        WindowFrame(rt, info, in, have_input, layers, 0.02f * mirror.Width() / 1.6f); // AzaharFrame keyboard
    } else {
        mirror.Hide();
        keyboard.Hide();
        TabMenuFrame(rt, info, in, have_input, layers);
    }
}

// AzaharFrame tabmenu: the menu panel; the screens' part of its buttons is done here.
void VrUi::TabMenuFrame(Runtime& rt, const FrameInfo& info, const Input::InputFrame& in,
                        bool have_input, std::vector<Quad>& layers) {
    MenuPanel::Host host;
    host.curve_ok = cylinder_ok;
    host.passthrough_ok = PassthroughSupported();
    host.request = [this](const MenuRequest& r) {
        std::scoped_lock lock{hooks_mutex};
        if (hooks.menu_request) {
            hooks.menu_request(r);
        }
    };
    host.reset_position = [this, &rt] {
        ResetPlacement(rt);
        Immersive::OnResetPanels();
        Vis::RecentreStats(); // AzaharFrame statspos
        ShowToastText("Position Reset");
    };
    host.swap_screens = [this] {
        const bool managed = Immersive::CanToggleBottom();
        const Input::ScreenView::Effect effect = screen_view.Swap(managed);
        if (effect.toggle_immersive_bottom) {
            Immersive::ToggleBottom();
        }
        Immersive::SetBottomFocused(screen_view.bottom_focus());
        LOG_INFO(Frontend, "xr: in-game menu: {} screen in focus",
                 screen_view.bottom_focus() ? "bottom" : "top");
    };
    if (!tab_menu.IsOpen()) {
        tab_menu.Resume(); // back from Azahar's window
    }
    std::optional<XrPosef> cursor;
    tab_menu.Frame(info, in, have_input, host, layers, cursor);
    if (cursor) {
        AddCursor(*cursor, 0.02f * tab_menu.Width() / 1.6f, layers);
    }
}

// AzaharFrame keyboard: the window and the headset keyboard under it. The keyboard takes its
// input first and hides the hands it has from the window; its quads go over the window's, and
// only once the window shows.
void VrUi::WindowFrame(Runtime& rt, const FrameInfo& info, const Input::InputFrame& in,
                       bool have_input, std::vector<Quad>& layers, float dot_m) {
    Input::InputFrame window_in = in;
    std::vector<Quad> keyboard_layers;
    std::optional<XrPosef> keyboard_cursor;
    const float width = mirror.Width();
    keyboard.Frame(rt.session, rt.swapchain_format, uploader, info, in, have_input,
                   {mirror.Pose(), width, mirror.HeightFor(width)}, window_in, keyboard_layers,
                   keyboard_cursor);
    const std::size_t before = layers.size();
    std::optional<XrPosef> cursor;
    mirror.Frame(rt.session, rt.swapchain_format, uploader, info, window_in, have_input, layers,
                 cursor);
    if (layers.size() == before) {
        return; // no window yet (no image), nor a keyboard
    }
    layers.insert(layers.end(), keyboard_layers.begin(), keyboard_layers.end());
    if (cursor) {
        AddCursor(*cursor, dot_m, layers);
    }
    if (keyboard_cursor) {
        AddCursor(*keyboard_cursor, dot_m, layers);
    }
}

void VrUi::SetLibraryDirs(const std::vector<LibraryDir>& dirs) {
    if (!library || mirror.Enabled()) {
        return; // the mirrored Qt window has its own game list
    }
    std::vector<picker::GameDir> out;
    for (const auto& d : dirs) {
        out.push_back({d.path, d.deep_scan, d.max_depth});
    }
    library->SetDirs(std::move(out));
    library->Rescan();
}

void VrUi::ResetPlacement(Runtime& rt) {
    // AzaharFrame move: both slots linked at 1.0x, Detach Screens as it is; forgotten for this game.
    SetScreenPlacement(DefaultScreenPlacement());
    free_rebase = true;
    ErasePlacement();
    static_cast<void>(rt);
}

void VrUi::WatchPlacement(bool screens_live) {
    if (!screens_live) {
        placement_live = false; // the next game loads its own
        return;
    }
    const uint32_t detach = DetachMode();
    const uint32_t layout = Settings::values.vr_screen_layout.GetValue();
    ScreenPlacement place = GetScreenPlacement();
    if (!placement_live) {
        placement_live = true;
        placement_title = GameTitleId();
        seen_detach = detach;
        seen_layout = layout;
        free_held = false;
        if (placement_title == 0) {
            PutSlots(place, Rig::ForMode(SlotsOf(place), detach)); // no title id: nothing saved
            SetScreenPlacement(place);
            return;
        }
        if (!placement_file_read) {
            placement_file_read = true;
            std::string text;
            const std::string path =
                FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + kScreenPlacementFile;
            if (FileUtil::ReadFileToString(true, path, text) > 0) {
                placement_file.Parse(text);
            }
        }
        const auto saved = placement_file.Get(placement_title);
        const Rig::Slots slots = Rig::ForMode(saved.value_or(Rig::Slots{}), detach);
        PutSlots(place, slots);
        SetScreenPlacement(place);
        LogSlots(saved ? "saved for this game" : "at the defaults (none saved for this game)",
                 slots);
        return;
    }
    if (detach == seen_detach && layout == seen_layout) {
        return;
    }
    Rig::Slots slots = SlotsOf(place);
    if (detach != seen_detach) {
        LOG_INFO(Frontend, "xr: {}", Rig::DetachToast(detach));
        slots = Rig::ForMode(slots, detach); // Off: the second slot back to its linked place
        free_held = false;
    }
    if (layout != seen_layout) {
        Rig::Relink(slots); // a new Screen Layout re-places the second slot
    }
    seen_detach = detach;
    seen_layout = layout;
    PutSlots(place, slots);
    SetScreenPlacement(place);
    bridge.mapper().ReanchorDrag();
    SavePlacement("Detach Screens or Screen Layout changed");
}

void VrUi::SavePlacement(const char* why) {
    if (placement_title == 0 || !placement_live) {
        return;
    }
    const Rig::Slots slots = SlotsOf(GetScreenPlacement());
    placement_file.Set(placement_title, slots);
    WritePlacementFile();
    LogSlots(why, slots);
}

void VrUi::ErasePlacement() {
    if (placement_title == 0 || !placement_live || !placement_file.Get(placement_title)) {
        return;
    }
    placement_file.Erase(placement_title);
    WritePlacementFile();
}

void VrUi::WritePlacementFile() {
    const std::string path =
        FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + kScreenPlacementFile;
    const std::string tmp = path + ".new";
    const std::string text = placement_file.Format();
    if (FileUtil::WriteStringToFile(true, tmp, text) != text.size() ||
        !FileUtil::Rename(tmp, path)) {
        LOG_WARNING(Frontend, "xr: could not save the screen placement");
    }
}

void VrUi::AddCursor(const XrPosef& pose, float size_m, std::vector<Quad>& layers) {
    if (!cursor_chain.has_image) {
        return;
    }
    Quad q;
    q.swapchain = cursor_chain.handle;
    q.image_rect = {{0, 0}, {static_cast<int32_t>(kDotSize), static_cast<int32_t>(kDotSize)}};
    q.pose = pose;
    // A hair in front of the panel so it is not hidden by it.
    const Input::Vec3f forward = Input::Rotate(ToInput(pose).orientation, {0, 0, 0.005f});
    q.pose.position.x += forward.x;
    q.pose.position.y += forward.y;
    q.pose.position.z += forward.z;
    q.size = {size_m, size_m};
    q.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layers.push_back(q);
}

uint32_t VrUi::CurveFor(const Immersive::ScreenLayers& shown) const {
    if (!cylinder_ok || !shown.top_pointable) {
        return 0; // the runtime has no cylinder, or the immersive screen is on view
    }
    return std::min(Settings::values.vr_screen_curve.GetValue(), Rig::kMaxCurveDeg);
}

void VrUi::ShowGripHint(bool immersive, const Input::ScreenQuads& quads, bool swapped,
                        const Input::PanelQuad* held, bool curve) {
    const ViewPose view = LatestViewPose();
    if (!view.valid) {
        return;
    }
    const Input::Vec3f head = ToInput(view.head).position;
    Input::Vec3f at;
    if (immersive) {
        // Head-locked or wide, the immersive screen fills the middle of the view: the chip goes
        // low in front of you, under where the toasts show.
        Input::Vec3f forward = Input::Rotate(ToInput(view.head).orientation, {0, 0, -1});
        forward.y = 0.0f;
        forward =
            Input::Length(forward) > 1e-3f ? Input::Normalized(forward) : Input::Vec3f{0, 0, -1};
        at = head + forward * 0.9f + Input::Vec3f{0.0f, -0.45f, 0.0f};
    } else {
        // Under the screen whose bottom edge is lower (Stacked: the one in the second slot; Side
        // by Side: the main one), a little below that edge. A hidden one does not count.
        const auto bottom_edge = [](const Input::PanelQuad& q) {
            return q.pose.position.y -
                   std::abs(Input::Rotate(q.pose.orientation, {0.0f, q.height_m * 0.5f, 0.0f}).y);
        };
        const Input::PanelQuad& in_main = swapped ? quads.bottom : quads.top;
        const Input::PanelQuad& second = swapped ? quads.top : quads.bottom;
        const bool use_second =
            second.visible && (!in_main.visible || bottom_edge(second) < bottom_edge(in_main));
        // AzaharFrame move: Detach Screens: under the screen that is held.
        const Input::PanelQuad& under = held ? *held : use_second ? second : in_main;
        at = under.pose.position +
             Input::Rotate(under.pose.orientation, {0.0f, -under.height_m * 0.5f - 0.05f, 0.0f});
    }
    const float distance = std::max(Input::Length(at - head), 0.3f);
    // Below the screen and a little nearer, so it never hides inside a curved one.
    at = at + Input::Normalized(head - at) * std::min(0.1f, distance * 0.1f);
    const Input::Posef pose{Input::FaceViewer(at, head, {0, 1, 0}), at};
    ShowHint(immersive ? kHintImmersive : curve ? kHintNormal : kHintSecond, ToXr(pose),
             0.32f * distance);
    LOG_INFO(Frontend, "xr: grip hint shown ({})", immersive ? "immersive" : "screens");
}

void VrUi::CycleScreenLayout() {
    const uint32_t after = Rig::NextLayout(Settings::values.vr_screen_layout.GetValue());
    Settings::values.vr_screen_layout.SetValue(after);
    bridge.mapper().ReanchorDrag(); // the second screen moved under a hand that holds it
    ShowToastText(Rig::LayoutToast(after));
    LOG_INFO(Frontend, "xr: screen layout {}", Rig::LayoutName(after));
    std::scoped_lock lock{hooks_mutex};
    if (hooks.vr_setting_changed) {
        hooks.vr_setting_changed();
    }
}

void VrUi::StepScreenCurve(int dir) {
    if (!cylinder_ok) {
        ShowToastText("Screen Curve: Not Available");
        LOG_INFO(Frontend, "xr: screen curve: the runtime has no cylinder layers");
        return;
    }
    const uint32_t before = Settings::values.vr_screen_curve.GetValue();
    const uint32_t after = Rig::StepCurve(before, dir);
    ShowToastText(Rig::CurveToast(after)); // also at the ends, so the press shows
    if (after == before) {
        return;
    }
    Settings::values.vr_screen_curve.SetValue(after);
    bridge.mapper().ReanchorDrag(); // the surface under the hand moved
    // Only the screen in the main slot bends; the second one stays flat.
    const ScreenPlacement now = GetScreenPlacement();
    LOG_INFO(Frontend, "xr: screen curve {} degrees (main slot radius {:.2f} m at the top size)",
             after, Rig::CurveRadius(now.top_size.width, after));
    std::scoped_lock lock{hooks_mutex};
    if (hooks.vr_setting_changed) {
        hooks.vr_setting_changed();
    }
}

VrUi::ScreenDraw VrUi::ScreenDrawFor(const Immersive::ScreenLayers& shown) const {
    // The slots as immersive.cpp draws them (only their visibility matters when not swapped;
    // swapped, immersive is paused and both are in the LOCAL space).
    const Input::PanelQuad main_slot{ToInput(shown.top_pose[0]), shown.top_size.width,
                                     shown.top_size.height, shown.show_top};
    const Input::PanelQuad secondary_slot{ToInput(shown.bottom_pose), shown.bottom_size.width,
                                          shown.bottom_size.height, shown.show_bottom};
    Input::ScreenQuads quads =
        screen_view.Layout(main_slot, secondary_slot, Immersive::CanToggleBottom());
    if (screen_view.bottom_focus()) { // AzaharFrame curve
        Rig::ApplyCurve(quads.bottom, quads.top, CurveFor(shown));
    } else {
        Rig::ApplyCurve(quads.top, quads.bottom, CurveFor(shown));
    }
    ScreenDraw draw;
    draw.swapped = screen_view.bottom_focus();
    draw.top_visible = quads.top.visible;
    draw.bottom_visible = quads.bottom.visible;
    draw.top_pose = ToXr(quads.top.pose);
    draw.top_size = {quads.top.width_m, quads.top.height_m};
    draw.bottom_pose = ToXr(quads.bottom.pose);
    draw.bottom_size = {quads.bottom.width_m, quads.bottom.height_m};
    draw.top_curve = quads.top.curve_radius;
    draw.bottom_curve = quads.bottom.curve_radius;
    return draw;
}

void VrUi::Frame(Runtime& rt, const FrameInfo& info, std::vector<Quad>& layers) {
    if (!info.screens_live) {
        // No game: the next one starts with the top screen in focus and the bottom screen as its
        // settings say (immersive.cpp forgets its half of the state at the same moment).
        screen_view.Reset();
        Immersive::SetBottomFocused(false);
    }
    WatchPlacement(info.screens_live); // AzaharFrame move
    const bool focused = info.state == XR_SESSION_STATE_FOCUSED;
    Input::InputFrame in;
    in.time_ns = info.display_time;
    const bool have_input = focused && actions_ok &&
                            actions.Sync(info.session, info.local_space, info.display_time, in);
    if (have_input) {
        mode_latch.Filter(in); // the chord that made this session stays away until let go
    }

    if (launch_pending && info.display_time - launch_time > 10'000'000'000LL) {
        launch_pending = false; // the boot failed or was refused; allow another pick
    }

    const bool want_menu = game_running && menu_open && mirror.Enabled();
    if (want_menu && !menu_shown) {
        EnterMenu();
    } else if (!want_menu && menu_shown) {
        LeaveMenu(in);
    }
    // AzaharFrame vrvis: a hand on the help panel or the buttons is taken from the window.
    const Vis::Where where =
        menu_shown ? Vis::Where::Menu : game_running ? Vis::Where::Game : Vis::Where::Home;
    Input::InputFrame window_in = in;
    visuals.TakeInput(info, where, window_in, have_input);
    const std::size_t layers_before = layers.size();
    stats_screens.clear(); // AzaharFrame statspos: GameFrame fills it
    if (menu_shown) {
        MenuFrame(rt, info, window_in, have_input, layers);
    } else if (game_running) {
        if (picker_shown) {
            picker_shown = false;
        }
        mirror.Hide();
        keyboard.Hide(); // AzaharFrame keyboard
        Input::InputFrame game_in = window_in; // AzaharFrame statspos: without a stats grab
        FilterAfterMenu(game_in);
        GameFrame(rt, info, game_in, have_input, layers);
    } else {
        if (bridge_live) {
            bridge.Release();
            bridge_live = false;
        }
        if (have_input && ModeChord(in)) {
            RequestToggleVr("home");
        }
        if (mirror.Enabled()) {
            WindowFrame(rt, info, window_in, have_input, layers, 0.02f); // AzaharFrame keyboard
        } else {
            PickerFrame(rt, info, window_in, have_input, layers);
        }
    }
    // AzaharFrame vrvis: the help, the buttons and the stats; the beams, dots and models.
    Vis::Window window;
    if (where != Vis::Where::Game) {
        if (where == Vis::Where::Menu && !menu_window) {
            window.shown = false; // AzaharFrame tabmenu: the menu, no help or buttons beside it
        } else if (mirror.Enabled()) {
            const std::size_t dim = where == Vis::Where::Menu && dim_chain.has_image &&
                                            info.should_render
                                        ? 1
                                        : 0;
            window.shown = layers.size() > layers_before + dim;
            window.pose = mirror.Pose();
            window.width = mirror.Width();
            window.height = mirror.HeightFor(window.width);
        } else {
            window.shown = picker_shown && picker_chain.has_image;
            window.pose = picker_pose;
            window.width = kPickerWidth;
            window.height = kPickerHeight;
        }
    }
    visuals.Frame(rt, info, where, window, stats_screens, in, have_input, layers,
                  cursor_chain.handle, dim_chain.handle);
    last_time = info.display_time;
}

void VrUi::GameFrame(Runtime& rt, const FrameInfo& info, const Input::InputFrame& in,
                     bool have_input, std::vector<Quad>& layers) {
    if (!have_input || !info.screens_live) {
        if (bridge_live) {
            bridge.Release();
            bridge_live = false;
        }
        nudger.Reset(); // AzaharFrame vrsize
        was_holding = false;
        return;
    }
    ScreenPlacement place = GetScreenPlacement();
    // The screens where they are drawn this frame (immersive.cpp) are the two slots (main,
    // secondary; AzaharFrame screenswap): the focus says which screen is in which, and the
    // pointer and the touch follow them. A hidden screen or the immersive screen is not a target.
    const bool managed = Immersive::CanToggleBottom();
    const Immersive::ScreenLayers shown =
        Immersive::Screens(info.local_space, info.view_space, place);
    const Input::PanelQuad main_slot{ToInput(shown.top_pose[0]), shown.top_size.width,
                                     shown.top_size.height, shown.show_top && shown.top_pointable};
    const Input::PanelQuad secondary_slot{ToInput(shown.bottom_pose), shown.bottom_size.width,
                                          shown.bottom_size.height, shown.show_bottom};
    Input::ScreenQuads quads = screen_view.Layout(main_slot, secondary_slot, managed);
    // AzaharFrame curve: the pointer, the touch and the drag hit the screens as they are drawn.
    if (screen_view.bottom_focus()) {
        Rig::ApplyCurve(quads.bottom, quads.top, CurveFor(shown));
    } else {
        Rig::ApplyCurve(quads.top, quads.bottom, CurveFor(shown));
    }
    Input::PanelGeometry panels;
    panels.top = quads.top;
    panels.bottom = quads.bottom;
    panels.bottom_toggle_enabled = managed;
    // AzaharFrame statspos: the beam stops at the stats in front of the screens; the stats are
    // never placed behind the screens as drawn, the immersive one too.
    panels.occluder = visuals.StatsOccluder();
    stats_screens = {quads.top, quads.bottom};
    if (shown.show_top && !shown.top_pointable) {
        Input::Posef top = ToInput(shown.top_pose[0]);
        if (shown.top_space == info.view_space) {
            const Input::Posef head = ToInput(LatestViewPose().head);
            top = {Input::Normalized(Input::Multiply(head.orientation, top.orientation)),
                   Input::Transform(head, top.position)};
        }
        stats_screens.push_back({top, shown.top_size.width, shown.top_size.height, true});
    }
    // While the immersive screen is on view the grips move it, not the mapper. The bottom
    // screen in focus pauses immersive mode (SetBottomFocused below), so the grips move the
    // normal screens then.
    Input::InputFrame game_in = in;
    Immersive::OnGameInput(game_in); // immersive.cpp
    // AzaharFrame vrsize: while a grip holds a screen the D-pad nudges it (the immersive screen:
    // the global Immersive Offset; the normal pair: a step about the head) and never reaches
    // the game. The normal pair's drag is the mapper's, as of the last frame.
    const bool holding_immersive = Immersive::HoldingScreen();
    const bool holding_normal = !holding_immersive && bridge.mapper().DragActive();
    const Rig::DpadNudger::Steps nudge =
        nudger.Filter(game_in, holding_immersive || holding_normal);
    // AzaharFrame move: Detach Screens: a grip moves the slot under it (main or second).
    const uint32_t detach = DetachMode();
    // AzaharFrame controls: a grab starts: the hint chip, unless one showed in the last 10 s.
    if ((holding_immersive || holding_normal) && !was_holding &&
        (last_hint_ns == 0 || in.time_ns - last_hint_ns >= kHintQuietNs)) {
        last_hint_ns = in.time_ns;
        const Input::Panel held = bridge.mapper().DragPanel();
        if (holding_normal && detach != Rig::kDetachOff && held != Input::Panel::None) {
            const bool main = screen_view.InMainSlot(held == Input::Panel::Top);
            ShowGripHint(false, quads, screen_view.bottom_focus(),
                         held == Input::Panel::Top ? &quads.top : &quads.bottom, main);
        } else {
            ShowGripHint(holding_immersive, quads, screen_view.bottom_focus());
        }
    }
    was_holding = holding_immersive || holding_normal;
    {
        const ViewPose view = LatestViewPose(); // the head drives Motion Controls: Headset
        game_in.head_active = view.valid;
        game_in.head = ToInput(view.head);
    }
    // AzaharFrame pads: the other gamepads play too (the raw `in` tells Steam's echo of the Frame).
    const Input::MapperOutput& out = bridge.Process(game_in, panels, &in);
    bridge_live = true;

    const auto& cmd = out.commands;
    {
        Input::ScreenView::Effect effect;
        if (cmd.toggle_bottom) {
            effect = screen_view.ToggleBottom(managed); // Left Bumper + Right Bumper + View
        } else if (Immersive::HoldingScreen()) {
            // A grip holds the immersive screen: no swap or hide meanwhile (a swap would cancel
            // the move). The reset is the 1 s hold, as everywhere (AzaharFrame controls).
        } else if (cmd.swap_screens) {
            effect = screen_view.Swap(managed); // right stick click, tap
            LOG_INFO(Frontend, "xr: {} screen in focus",
                     screen_view.bottom_focus() ? "bottom" : "top");
        } else if (cmd.toggle_other_screen) {
            effect = screen_view.ToggleOther(managed); // left stick click, tap
            LOG_INFO(Frontend, "xr: {} screen {}", screen_view.bottom_focus() ? "top" : "bottom",
                     (screen_view.bottom_focus() ? !screen_view.top_hidden()
                                                 : screen_view.BottomShown(managed))
                         ? "shown"
                         : "hidden");
        }
        if (effect.toggle_immersive_bottom) {
            Immersive::ToggleBottom();
        }
        Immersive::SetBottomFocused(screen_view.bottom_focus());
    }
    if (cmd.cycle_layout) {
        CycleScreenLayout(); // AzaharFrame layout: left stick click held 1 s
    }
    // AzaharFrame move: the slot the grip holds this frame, and whether it moves freely.
    const bool drag_main = cmd.drag_panel != Input::Panel::None &&
                           screen_view.InMainSlot(cmd.drag_panel == Input::Panel::Top);
    const bool drag_free = cmd.top_drag_active && Rig::SlotFree(detach, drag_main);
    if (!cmd.top_drag_active) {
        free_held = false;
    }
    if (cmd.top_drag_active && detach != Rig::kDetachOff && !place.second_free) {
        // Detached: from the first grab the second slot stays where it is drawn now and no
        // longer follows the main one.
        Rig::Slots slots = SlotsOf(place);
        if (Rig::Detach(slots, detach, ToInput(place.bottom_pose))) {
            PutSlots(place, slots);
            SetScreenPlacement(place);
            place = GetScreenPlacement();
        }
    }
    const ViewPose head_view = LatestViewPose();
    const Input::Vec3f eye = head_view.valid ? ToInput(head_view.head).position : Input::Vec3f{};
    if (cmd.reset_panels) {
        ResetPlacement(rt);
        // AzaharFrame controls: one rule, the 1 s hold resets the screens and re-centres, in every
        // mode (the immersive screen too, held or not); a tap never does.
        Immersive::OnResetPanels();
        Vis::RecentreStats(); // AzaharFrame statspos: the stats in front of you again
    } else if (drag_free && cmd.drag_hand >= 0) {
        // AzaharFrame move: a free slot, as the stats panel In Front: round you with the beam,
        // facing you; the stick sets its distance and size.
        const Input::Ray ray =
            Input::RayFromPose(game_in.hand[static_cast<std::size_t>(cmd.drag_hand)].aim);
        XrPosef& pose_xr = drag_main ? place.top_pose : place.second_pose;
        float& scale = drag_main ? place.top_scale : place.second_scale;
        Input::Posef pose = ToInput(pose_xr);
        if (!free_held || free_held_main != drag_main || free_rebase) {
            free_grab.Begin(eye, pose, ray);
            free_distance = Rig::FreeDistance(Input::Length(pose.position - eye), 0.0f);
            free_held = true;
            free_held_main = drag_main;
            free_rebase = false;
        }
        free_distance = Rig::FreeDistance(free_distance, cmd.top_depth_delta_z);
        pose = Rig::FreePose(eye, free_grab.Direction(eye, ray, free_distance), free_distance);
        pose_xr = ToXr(pose);
        if (cmd.top_scale_stick != 0.0f) {
            scale = Rig::ScaleWithStick(scale, cmd.top_scale_stick, cmd.top_scale_dt, 0.0f);
        }
        place.top_size = NormalTopSize(place.top_scale);
        SetScreenPlacement(place);
    } else if (cmd.top_drag_active || cmd.top_depth_delta_z != 0.0f ||
               cmd.top_scale_stick != 0.0f) {
        Input::Posef top = ToInput(place.top_pose);
        if (cmd.top_drag_active) {
            Input::DragLimits limits;
            limits.max_z = kNearestTopZ;
            // The target is for the mapper's top screen as drawn (panels.top), which sits in
            // the secondary slot while the bottom one is in focus. Move the stored top placement
            // (the main slot) by the same amount.
            const Input::Vec3f main_target =
                screen_view.MainDragTarget(cmd.top_drag_target, quads.top, main_slot);
            const Input::Vec3f target = top.position + (main_target - main_slot.pose.position);
            // The bottom screen follows the top one: no distance check between them.
            if (auto moved = Input::SolveSphereDrag(top, target, {0, 0, 0}, nullptr, limits)) {
                top = *moved;
            }
        }
        if (cmd.top_depth_delta_z != 0.0f) {
            top.position.z = Input::ApplyDepth(top.position.z, cmd.top_depth_delta_z, kNearestTopZ);
        }
        // AzaharFrame vrsize: the stick sideways makes the pair bigger or smaller.
        if (cmd.top_scale_stick != 0.0f) {
            place.top_scale =
                Rig::ScaleWithStick(place.top_scale, cmd.top_scale_stick, cmd.top_scale_dt, 0.0f);
            place.top_size = NormalTopSize(place.top_scale);
        }
        place.top_pose = ToXr(top);
        place.bottom_pose = BottomFromTop(place.top_pose, place.top_scale);
        SetScreenPlacement(place);
    }
    if (nudge.Any()) {
        if (holding_immersive) {
            // Left and right: the lens offset; up and down: Wider View (AzaharFrame widerview;
            // the vertical offset stays in the settings). Both show their own toast.
            if (nudge.right != 0 && Immersive::NudgeOffset(nudge.right, 0)) {
                std::scoped_lock lock{hooks_mutex};
                if (hooks.vr_setting_changed) {
                    hooks.vr_setting_changed();
                }
            }
            if (nudge.up_presses != 0 &&
                Immersive::StepWiderView(nudge.up_presses) == Immersive::WiderStep::Changed) {
                std::scoped_lock lock{hooks_mutex};
                if (hooks.game_setting_changed) {
                    hooks.game_setting_changed();
                }
            }
        } else if (holding_normal && bridge.mapper().DragActive() && drag_free) {
            // AzaharFrame move: a free slot turns about you; up and down are the Screen Curve
            // only while the main slot is held.
            if (nudge.right != 0) {
                ScreenPlacement now = GetScreenPlacement();
                XrPosef& pose_xr = drag_main ? now.top_pose : now.second_pose;
                pose_xr = ToXr(Rig::NudgeFree(ToInput(pose_xr), eye, nudge.right));
                SetScreenPlacement(now);
                free_rebase = true; // the hand carries on from the new place
                bridge.mapper().ReanchorDrag();
                ShowToastText(Rig::MovedToast(nudge.right, 0));
            }
            if (nudge.up_presses != 0 && drag_main) {
                StepScreenCurve(nudge.up_presses > 0 ? 1 : -1);
            }
        } else if (holding_normal && bridge.mapper().DragActive()) {
            // Left and right turn the pair about you; up and down step the Screen Curve
            // (AzaharFrame curve; the hand moves the pair up and down).
            // AzaharFrame move: detached (Secondary), only the main slot moves this way.
            ScreenPlacement now = GetScreenPlacement();
            const auto moved = nudge.right != 0
                                   ? Rig::NudgeTop(ToInput(now.top_pose), nudge.right, 0)
                                   : std::nullopt;
            if (moved) {
                now.top_pose = ToXr(*moved);
                now.bottom_pose = BottomFromTop(now.top_pose, now.top_scale);
                SetScreenPlacement(now);
                bridge.mapper().ReanchorDrag(); // the hand carries on from the new place
                ShowToastText(Rig::MovedToast(nudge.right, 0));
                LOG_INFO(Frontend, "xr: screens nudged {}: top at {:.2f}, {:.2f}, {:.2f}",
                         nudge.right, moved->position.x, moved->position.y, moved->position.z);
            }
            if (nudge.up_presses != 0 && (detach == Rig::kDetachOff || drag_main)) {
                StepScreenCurve(nudge.up_presses > 0 ? 1 : -1);
            }
        }
    }
    // AzaharFrame vrsize: where a move of the normal screens ended.
    if (cmd.top_drag_active != normal_dragging) {
        normal_dragging = cmd.top_drag_active;
        if (!normal_dragging) {
            const ScreenPlacement now = GetScreenPlacement();
            LOG_INFO(Frontend,
                     "xr: screens moved: top at {:.2f}, {:.2f}, {:.2f}, size {:.2f} ({:.2f} x {:.2f} m, "
                     "3D depth x{:.2f})",
                     now.top_pose.position.x, now.top_pose.position.y, now.top_pose.position.z,
                     now.top_scale, now.top_size.width, now.top_size.height,
                     Rig::DepthScale(now.top_scale));
            SavePlacement("moved"); // AzaharFrame move: per game
        }
    }
    // From 20 % on, so a short View press (Select) logs nothing.
    if (cmd.stop_hold_progress > 0.2f && !stop_hold_logged) {
        stop_hold_logged = true;
        LOG_INFO(Frontend, "xr: View hold started (the game stops when it completes)");
    } else if (cmd.stop_hold_progress <= 0.0f && stop_hold_logged && !cmd.stop_game) {
        stop_hold_logged = false;
        LOG_INFO(Frontend, "xr: View hold let go before the stop");
    }
    // AzaharFrame controls: "Stopping..." from half the hold (0.75 s of 1.5 s), taken back if
    // View is let go first.
    if (cmd.stop_hold_progress >= 0.5f && !stopping_shown) {
        stopping_shown = true;
        ShowToastText(kStoppingText);
    } else if (cmd.stop_hold_progress <= 0.0f && stopping_shown && !cmd.stop_game) {
        stopping_shown = false;
        HideToastText(kStoppingText);
    }
    if (cmd.toggle_vr) {
        RequestToggleVr("game");
    }
    RequestQuick(cmd, "game"); // AzaharFrame vrqol
    RequestFastForward(cmd, "game");
    if (cmd.menu_toggle) {
        std::scoped_lock lock{hooks_mutex};
        if (hooks.toggle_menu) {
            LOG_INFO(Frontend, "xr: Menu held, opening the in-game menu");
            hooks.toggle_menu();
        }
    }
    if (cmd.stop_game) {
        stop_hold_logged = false;
        stopping_shown = false;
        std::scoped_lock lock{hooks_mutex};
        if (hooks.stop_game) {
            LOG_INFO(Frontend, "xr: View held, stopping the game");
            hooks.stop_game();
        }
    }
    // AzaharFrame touchpad: the dot of the touchpad or the mouse on the bottom screen.
    if (const Input::TouchpadCursor& pad_dot = bridge.touchpad_cursor(); pad_dot.visible) {
        const Input::DotPose dot = Input::DotOnQuad(panels.bottom, pad_dot.u, pad_dot.v, eye);
        const XrPosef pose{{dot.orientation.x, dot.orientation.y, dot.orientation.z,
                            dot.orientation.w},
                           {dot.position.x, dot.position.y, dot.position.z}};
        AddCursor(pose, dot.size_m, layers);
    }
    if (out.cursor.visible) {
        const XrPosef pose{{out.cursor.orientation.x, out.cursor.orientation.y,
                            out.cursor.orientation.z, out.cursor.orientation.w},
                           {out.cursor.position.x, out.cursor.position.y, out.cursor.position.z}};
        AddCursor(pose, out.cursor.scale_m, layers);
    }
}

void VrUi::PickerFrame(Runtime& rt, const FrameInfo& info, const Input::InputFrame& in,
                       bool have_input, std::vector<Quad>& layers) {
    if (!picker || !info.should_render) {
        return;
    }
    if (!picker_shown) {
        picker_shown = true;
        picker->Invalidate();
        prev_select = prev_back = true; // no select from a button already held
    }

    picker::PickerInput pin;
    pin.dt = last_time ? static_cast<float>(info.display_time - last_time) * 1e-9f : 0.0f;
    std::optional<XrPosef> cursor;
    if (have_input) {
        const Input::PanelQuad quad{ToInput(picker_pose), kPickerWidth, kPickerHeight, true};
        // The right hand first, then the left.
        for (int h : {1, 0}) {
            const Input::HandInput& hand = in.hand[h];
            if (!hand.active) {
                continue;
            }
            const Input::QuadHit hit = Input::IntersectQuad(Input::RayFromPose(hand.aim), quad);
            if (hit.in_bounds) {
                pin.hover = picker::Vec2{hit.u, hit.v};
                cursor = XrPosef{picker_pose.orientation,
                                 {hit.point_world.x, hit.point_world.y, hit.point_world.z}};
                break;
            }
        }
        const bool select = in.a || in.hand[0].trigger > 0.55f || in.hand[1].trigger > 0.55f;
        const bool back = in.b;
        pin.select = select && !prev_select;
        pin.back = back && !prev_back;
        prev_select = select;
        prev_back = back;
        const Input::Vec2f stick = std::abs(in.hand[1].stick.x) + std::abs(in.hand[1].stick.y) >
                                           std::abs(in.hand[0].stick.x) + std::abs(in.hand[0].stick.y)
                                       ? in.hand[1].stick
                                       : in.hand[0].stick;
        const float s = std::abs(stick.x) > std::abs(stick.y) ? stick.x : -stick.y;
        pin.scroll = std::abs(s) > kStickPage ? s : 0.0f;
    }

    // One log line per finished scan.
    if (library && !library->IsScanning() && library->Generation() != logged_generation) {
        logged_generation = library->Generation();
        const auto snapshot = library->Snapshot();
        LOG_INFO(Frontend, "xr: picker: {} games", snapshot ? snapshot->size() : 0);
    }

    const picker::PickerResult r = picker->Update(pin);
    if (!picker_chain.handle) {
        uploader.CreateChain(rt.session, rt.swapchain_format, picker->Width(), picker->Height(),
                             picker_chain);
        picker->Invalidate();
    }
    if (r.dirty || !picker_chain.has_image) {
        uploader.Upload(picker_chain, picker->Pixels());
    }
    if (r.launch_path && !launch_pending) {
        std::scoped_lock lock{hooks_mutex};
        if (hooks.launch_game) {
            LOG_INFO(Frontend, "xr: picker launches {}", *r.launch_path);
            launch_pending = true;
            launch_time = info.display_time;
            hooks.launch_game(*r.launch_path);
        }
    }

    if (picker_chain.has_image) {
        Quad q;
        q.swapchain = picker_chain.handle;
        q.image_rect = {{0, 0},
                        {static_cast<int32_t>(picker_chain.width),
                         static_cast<int32_t>(picker_chain.height)}};
        q.pose = picker_pose;
        q.size = {kPickerWidth, kPickerHeight};
        layers.push_back(q);
    }
    if (cursor) {
        AddCursor(*cursor, 0.02f, layers);
    }
}

} // namespace FrameXr
