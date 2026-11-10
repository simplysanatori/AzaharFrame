// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu
// Internal: the in-game menu in VR (menu/tab_menu.h) on a quad in front of the paused game, on
// the XR thread. Each frame it reads the settings into the menu's model, casts the controller
// rays at the quad, and turns trigger (or A) clicks, B, the sticks and a beam's rest into what
// the menu shows and does. The pixels are drawn on a thread of its own (a redraw takes a few
// milliseconds, too long for a frame) and uploaded without waiting (vis/async_overlay.h).
//
// What the menu does is mostly the Qt side's (FrontendHooks::menu_request): pausing, save
// states, the settings files. A setting is also set here at once, so the menu shows it in the
// next frame; the Qt side sets it again and saves it (per game when Per-Game is chosen).
//
// Controls: Trigger or A clicks (on release, where it was pressed); a held slider follows the
// beam. B closes an open drop-down menu, then a slot list, then the menu (Continue). Either
// stick up or down scrolls a list by a row. A beam that rests 0.6 s on a setting shows a
// one-line tooltip.

#pragma once

#include <array>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/input_frame.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/vis/async_overlay.h"

namespace FrameXr {

class Runtime;

class MenuPanel {
public:
    MenuPanel();
    ~MenuPanel();

    /// What the menu needs from VrUi (XR thread).
    struct Host {
        std::function<void(const MenuRequest&)> request; ///< to the Qt side
        std::function<void()> reset_position;
        std::function<void()> swap_screens;
        bool curve_ok = true;       ///< the runtime takes cylinder layers (Screen Curve)
        bool passthrough_ok = true; ///< the runtime offers passthrough (Background)
    };

    /// After a session exists: the swapchain, the drawing thread.
    void Create(Runtime& rt);
    /// Before the session is destroyed.
    void DestroyXr();

    /// Any thread (the Qt side, as the menu opens): the game's title, its save state slots
    /// (slot 0 is the quick save) and whether it can have settings of its own (a title id).
    void SetInfo(const std::string& title, const std::vector<MenuSaveSlot>& slots, bool per_game);

    /// XR thread: the menu opens here, this wide (metres); `stats` are the Stats tab's rows.
    void Open(const XrPosef& pose, float width_m,
              std::vector<std::pair<std::string, std::string>> stats);
    /// XR thread: the menu closed (or shows Azahar's window instead).
    void Close();
    /// XR thread: back from Azahar's window, on the tab it was on.
    void Resume();
    bool IsOpen() const {
        return open;
    }

    /// XR thread, each frame the menu shows: input, a redraw when anything changed, the quad.
    /// `cursor` gets the pointer dot's pose while a beam is on the menu.
    void Frame(const FrameInfo& info, const Input::InputFrame& in, bool have_input, Host& host,
               std::vector<Quad>& layers, std::optional<XrPosef>& cursor);

    XrPosef Pose() const {
        return pose;
    }
    float Width() const {
        return width_m;
    }
    float Height() const {
        return width_m * Menu::kPanelH / Menu::kPanelW;
    }

private:
    struct Shared;
    void BuildModel(const Host& host);
    void Click(Menu::Target t, Host& host);
    void SetSetting(Menu::Id id, int value, Host& host);
    void Worker();

    // XR thread.
    Vis::AsyncOverlay chain;
    XrPosef pose{{0, 0, 0, 1}, {0, 0, -1}};
    float width_m = 1.0f;
    bool open = false;
    Menu::MenuModel model;
    Menu::MenuView view;
    Menu::MenuLayout layout; ///< as last drawn
    std::vector<std::pair<std::string, std::string>> stats;
    int scope = 0;
    int controls_page = 0;
    std::array<bool, 2> trigger_down{};
    bool a_down = false, b_down = false;
    int active_hand = 1;
    int press_hand = -1; ///< the hand that holds a press (-1: none)
    float stick_hold = 0.0f;  ///< seconds a stick has been held up or down
    float stick_repeat = 0.0f;
    int stick_dir = 0;
    Menu::Id rest_id = Menu::Id::None; ///< what the beam rests on, for the tooltip
    float rest_s = 0.0f;
    XrTime last_time = 0;
    uint64_t posted = 0;   ///< redraws asked for
    uint64_t uploaded = 0; ///< the newest drawn pixels uploaded
    bool posted_any = false;
    Menu::MenuModel posted_model;
    Menu::MenuView posted_view;

    // The drawing thread and what it shares with the XR thread.
    std::unique_ptr<Shared> shared;
    std::thread worker;
};

} // namespace FrameXr
