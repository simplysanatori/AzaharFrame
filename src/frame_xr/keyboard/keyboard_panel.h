// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// Internal: the headset keyboard on the XR thread (keyboard.h has the outside's view of it).
// Shown only with Azahar's window (the home panel or the in-game menu), never in a game, where Y
// is the 3DS's Y.
//
//   Y                           show or hide it
//   Trigger on a key (or A)     press the key; hold Shift, Ctrl, Alt or Win half a second to lock
//   Grip or trigger on its bar  move it (it keeps its distance and turns to face you); the stick
//                               of that hand pushes it away and pulls it nearer meanwhile
//   Trigger on nothing          hide it
//
// It hangs under the window, as Moonlight XR's under its screen, and goes where the window goes:
// its place is kept relative to the window's bottom edge, in window widths, so it also sits right
// under the in-game menu's nearer, smaller window.

#pragma once

#include <optional>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/input_frame.h"
#include "frame_xr/keyboard/art.h"
#include "frame_xr/keyboard/layout.h"
#include "frame_xr/overlays.h"

namespace FrameXr {

class KeyboardPanel {
public:
    KeyboardPanel();
    ~KeyboardPanel();

    /// The window the keyboard belongs to, this frame.
    struct Window {
        XrPosef pose;
        float width_m = 1.6f;
        float height_m = 1.0f;
    };

    /// XR thread, each frame Azahar's window is shown, before the window's own input: Y, the keys,
    /// the bar. The hands it has (on the keyboard or its bar, or holding a press that began
    /// there) are taken out of `window_in` (not tracked, stick still), and A while it presses a
    /// key, so the window does not see them. Its quads go into `layers`, the dot's pose (a ray on
    /// it) into `cursor`.
    void Frame(XrSession session, VkFormat format, OverlayUploader& uploader,
               const FrameInfo& info, const Input::InputFrame& in, bool have_input,
               const Window& window, Input::InputFrame& window_in, std::vector<Quad>& layers,
               std::optional<XrPosef>& cursor);
    /// XR thread: the window is not shown. The keyboard goes away (and comes back closed).
    void Hide();
    /// XR thread, before the session ends.
    void Destroy(OverlayUploader& uploader);

private:
    void Open(bool by_text_field);
    void Close(const char* why);
    void Place(const Window& window);
    void Keep(const Window& window);
    void PressKey(int key, int source, int64_t now);

    // XR thread.
    Keyboard::Art art;
    OverlayChain key_chain, ring_chain, pill_chain;
    bool art_dirty = true;
    bool shown = false; // the window was shown last frame
    bool open = false;
    bool opened_by_text_field = false;

    int sheet = Keyboard::kSheetLower;
    int mods = 0;
    Keyboard::Sticky sticky;
    int sticky_source = -1; // 0, 1: a hand's trigger, 2: A
    int hover_key = -1;
    bool key_down = false;

    // Where it is: relative to the window's bottom edge (window widths), and turned against it.
    Input::Vec3f offset{0.0f, 0.0f, 0.0f};
    Input::Quatf turn{};
    Input::Posef pose{};
    float width_m = 0.0f, height_m = 0.0f;

    // Per hand.
    bool trigger_down[2]{};
    bool grip_down[2]{};
    bool trigger_ours[2]{}; // the trigger went down on the keyboard or its bar
    bool grip_ours[2]{};
    bool prev_a = true, prev_y = true;
    bool a_ours = false;
    int pointing = 1; // the hand whose ray marks keys
    bool bar_hot = false;

    // Moving it by its bar.
    int drag_hand = -1;
    bool drag_by_grip = false;
    float drag_t = 0.0f;
    Input::Vec3f drag_offset;
    XrTime last_time = 0;
};

} // namespace FrameXr
