// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Internal: what the XR thread does with the controllers each frame. In a game:
// the Frame controllers drive the virtual pad and the bottom-screen touch
// (frame_xr/input), grab and push the top screen (the bottom one follows it), the
// right stick click resets both in front of the head, holding View stops the game.
// Without a game: Azahar's own Qt window mirrored on a panel (frame_xr/qt_mirror) or,
// when the Qt side set no home panel, the application picker panel (frame_xr/picker),
// pointed at with either controller. A pointer dot is drawn where the ray hits a panel.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/actions.h"
#include "frame_xr/input/input_bridge.h"
#include "frame_xr/keyboard/keyboard_panel.h" // AzaharFrame keyboard
#include "frame_xr/overlays.h"
#include "frame_xr/qt_mirror/mirror_panel.h"

namespace frame_xr::picker {
class Library;
class Picker;
} // namespace frame_xr::picker

namespace FrameXr {

class Runtime;

class VrUi {
public:
    VrUi();
    ~VrUi();

    /// Once, with the instance: the picker library (kept across sessions).
    void Init();
    /// After a session exists: actions (suggest + attach), uploads. Again for each session.
    bool Create(Runtime& runtime);
    /// Before the session is destroyed: what Create made.
    void DestroyXr();
    /// With the instance: everything.
    void Destroy();

    /// XR thread, about 120 times a second while flat (no session): the gamepad from
    /// SetFlatInput through the same mapper as in VR (no aim: no touch, no pointing).
    void FlatFrame(int64_t now_ns);
    /// XR thread: a session was made (true) or ended (false). Tells the Qt side.
    void NotifyVr(bool vr);

    /// XR thread, once per frame after the views were located. Applies drags to the
    /// screen placement and appends the picker / pointer quads to `layers`.
    void Frame(Runtime& runtime, const FrameInfo& info, std::vector<Quad>& layers);

    // GUI thread.
    void SetFrontendHooks(FrontendHooks hooks);
    void SetGameRunning(bool running);
    void SetLibraryDirs(const std::vector<LibraryDir>& dirs);
    // Any thread: the mirrored Qt window replaces the picker (frame_xr/qt_mirror).
    void SetHomePanel(std::function<void(const HomePanelEvent&)> on_event);
    void SetHomePanelImage(std::shared_ptr<const HomePanelImage> image);
    void SetInGameMenu(bool open);
    void SetFlatInput(FlatInputSource source);

private:
    void GameFrame(Runtime& runtime, const FrameInfo& info, const Input::InputFrame& in,
                   bool have_input, std::vector<Quad>& layers);
    void PickerFrame(Runtime& runtime, const FrameInfo& info, const Input::InputFrame& in,
                     bool have_input, std::vector<Quad>& layers);
    void MenuFrame(Runtime& runtime, const FrameInfo& info, const Input::InputFrame& in,
                   bool have_input, std::vector<Quad>& layers);
    void EnterMenu();
    void LeaveMenu(const Input::InputFrame& in);
    /// Buttons held as the in-game menu closed stay away from the game until let go.
    void FilterAfterMenu(Input::InputFrame& in);
    void ResetPlacement(Runtime& runtime);
    /// Both bumpers + View + Menu outside a game (the mapper finds it in a game): once per chord.
    bool ModeChord(const Input::InputFrame& in);
    /// Asks the Qt side to switch between VR and flat (FrontendHooks::toggle_vr).
    void RequestToggleVr(const char* where);
    /// Everything held stays away from the game until let go (a new mode, the menu closed).
    void SwallowHeld();
    void AddCursor(const XrPosef& pose, float size_m, std::vector<Quad>& layers);
    // AzaharFrame keyboard: Azahar's window and the headset keyboard under it (frame_xr/keyboard).
    void WindowFrame(Runtime& runtime, const FrameInfo& info, const Input::InputFrame& in,
                     bool have_input, std::vector<Quad>& layers, float dot_m);

    Input::Actions actions;
    Input::InputBridge bridge;
    bool actions_ok = false;
    bool bridge_live = false;

    OverlayUploader uploader;
    OverlayChain picker_chain;
    OverlayChain cursor_chain;
    std::shared_ptr<frame_xr::picker::Library> library;
    std::unique_ptr<frame_xr::picker::Picker> picker;
    bool picker_shown = false;
    MirrorPanel mirror; ///< the home panel when the Qt side set one, instead of the picker
    KeyboardPanel keyboard; ///< AzaharFrame keyboard: shown only with the home panel (Y)

    // The in-game menu: the home panel over the paused game (Menu held 1 s).
    std::atomic<bool> menu_open{false}; ///< set by the Qt side once the game paused
    bool menu_shown = false;
    MirrorPanel::Placement home_place{};
    OverlayChain dim_chain; ///< a dark see-through quad between the game screens and the panel
    /// A hold of one button (the mapper, which times Menu and View in a game, does not run
    /// while the menu shows). Arms only once the button was let go.
    struct Hold {
        bool armed = false;
        bool down = false;
        bool fired = false;
        XrTime since = 0;
        bool Update(bool pressed, XrTime now, XrTime need);
    };
    Hold menu_hold;
    Hold view_hold;
    struct Swallow {
        bool a, b, x, y, menu, view, trigger[2], grip[2], bumper[2];
    };
    Swallow swallow{};
    XrPosef picker_pose{{0, 0, 0, 1}, {0, 0, -2}};

    // Edges for the picker (the mapper keeps its own for the game).
    bool prev_select = false;
    bool prev_back = false;
    XrTime last_time = 0;
    uint64_t logged_generation = 0; // last library generation logged ("xr: picker: N games")
    bool stop_hold_logged = false;  // "xr: View hold started" logged for this press

    bool chord_down = false; ///< ModeChord's edge
    std::mutex flat_mutex;
    FlatInputSource flat_input;
    bool flat_had_input = false; ///< for the log line when the gamepad comes and goes

    std::mutex hooks_mutex;
    FrontendHooks hooks;
    std::atomic<bool> game_running{false};
    std::atomic<bool> launch_pending{false};
    XrTime launch_time = 0;
};

} // namespace FrameXr
