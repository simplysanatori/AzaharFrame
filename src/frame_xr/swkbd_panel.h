// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd
// Internal: the game's keyboard (swkbd.h) on a quad in front of the paused game, on the XR
// thread. It draws on the in-game menu's swapchain (SteamVR allows 16 swapchains a session and the
// port is near that; the menu and the keyboard never show together), on a thread of its own as
// the menu does, and is uploaded without waiting (vis/async_overlay.h).
//
//   Trigger on a key (or A)     type it (Backspace repeats while held)
//   Trigger on a game's button  on release: the game's answer (the confirm button checks the
//                               text first and says why it was refused under the field)
//   B                           Backspace
// A physical keyboard types into the field too (swkbd.h; Enter is the confirm button).

#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/input_frame.h"
#include "frame_xr/menu/swkbd_model.h"
#include "frame_xr/swkbd.h"

namespace FrameXr {

namespace Vis {
class AsyncOverlay;
}

class SwkbdPanel {
public:
    SwkbdPanel();
    ~SwkbdPanel();

    /// After a session exists: the drawing thread.
    void Create();
    /// Before the session ends: an open keyboard goes back to Azahar's dialog.
    void DestroyXr();

    /// XR thread, each frame: takes a new request and the physical keys. True while the game
    /// waits for the keyboard.
    bool Poll();
    /// XR thread: the keyboard shows here from now on, this wide (metres).
    void Place(const XrPosef& pose, float width_m);
    /// XR thread, each frame the keyboard shows: input, a redraw when anything changed, the quad
    /// (on `chain`, the menu's). `cursor` gets the pointer dot while a beam is on the panel.
    void Frame(const FrameInfo& info, const Input::InputFrame& in, bool have_input,
               Vis::AsyncOverlay& chain, std::vector<Quad>& layers, std::optional<XrPosef>& cursor);

    XrPosef Pose() const {
        return pose;
    }
    float Width() const {
        return width_m;
    }
    float Height() const {
        return width_m * Swkbd::kPanelH / Swkbd::kPanelW;
    }

private:
    struct Shared;
    void Finish(const Swkbd::Result& result);
    void TestRequest();
    void Worker();

    // XR thread.
    std::optional<Swkbd::Model> model;
    uint64_t request = 0; ///< the request on view
    XrPosef pose{{0, 0, 0, 1}, {0, 0, -1}};
    float width_m = 1.0f;
    int hover = -1;
    int pressed = -1;
    int press_hand = -1;
    std::array<bool, 2> trigger_down{};
    bool a_down = false, b_down = false;
    int active_hand = 1;
    float repeat_s = 0.0f; ///< Backspace held
    XrTime last_time = 0;
    bool posted_any = false;
    uint64_t posted_rev = 0;
    int posted_hover = -1, posted_pressed = -1;
    uint64_t posted = 0, uploaded = 0;
    bool our_upload = false; ///< our pixels were uploaded since Place
    bool ready = false;      ///< our pixels are in the chain: the quad may show

    std::chrono::steady_clock::time_point created{}; ///< the test driver's clock
    bool test_fired = false;

    std::unique_ptr<Shared> shared;
    std::thread worker;
};

} // namespace FrameXr
