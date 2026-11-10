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

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/actions.h"
#include "frame_xr/input/chord_latch.h"
#include "frame_xr/input/input_bridge.h"
#include "frame_xr/input/screen_rig.h"  // AzaharFrame vrsize
#include "frame_xr/input/screen_view.h" // AzaharFrame screenswap
#include "frame_xr/keyboard/keyboard_panel.h" // AzaharFrame keyboard
#include "frame_xr/menu_panel.h"            // AzaharFrame tabmenu
#include "frame_xr/overlays.h"
#include "frame_xr/qt_mirror/mirror_panel.h"
#include "frame_xr/vis/visuals.h" // AzaharFrame vrvis

namespace frame_xr::picker {
class Library;
class Picker;
} // namespace frame_xr::picker

namespace FrameXr {

class Runtime;
namespace Immersive {
struct ScreenLayers;
}

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
    void FlatMenuFrame(const Input::InputFrame& in, int64_t now_ns); // AzaharFrame flat4
    /// XR thread: a session was made (true) or ended (false). Tells the Qt side.
    void NotifyVr(bool vr);

    /// XR thread, once per frame after the views were located. Applies drags to the
    /// screen placement and appends the picker / pointer quads to `layers`.
    void Frame(Runtime& runtime, const FrameInfo& info, std::vector<Quad>& layers);
    /// AzaharFrame vrvis, app.cpp after Frame: the controller models' layer (or null) and the
    /// beams and pointer dots that go over it.
    const XrCompositionLayerBaseHeader* TopLayers(std::vector<Quad>& top, std::size_t used) const {
        return visuals.TopLayers(top, used);
    }
    std::size_t MaxLayers() const {
        return visuals.MaxLayers();
    }

    /// AzaharFrame screenswap, XR thread (app.cpp, while building the layers): where each 3DS
    /// screen goes for the screens immersive.cpp gives (Immersive::Screens), with the focus
    /// (right stick click) and hiding (left stick click) applied. When `swapped` is false the
    /// screens go where `shown` says, and only `bottom_visible` is new (the bottom screen hidden
    /// by the left stick click).
    struct ScreenDraw {
        bool swapped = false; ///< the bottom screen is in focus: use the poses and sizes below
        bool top_visible = true;
        bool bottom_visible = true;
        XrPosef top_pose{{0, 0, 0, 1}, {0, 0, 0}};
        XrExtent2Df top_size{};
        XrPosef bottom_pose{{0, 0, 0, 1}, {0, 0, 0}};
        XrExtent2Df bottom_size{};
        /// AzaharFrame curve: above 0, the screen is a cylinder layer of this radius (screen_rig.h).
        float top_curve = 0.0f;
        float bottom_curve = 0.0f;
    };
    ScreenDraw ScreenDrawFor(const Immersive::ScreenLayers& shown) const;
    /// AzaharFrame curve: the Screen Curve (degrees) for the screens as `shown`: 0 unless they
    /// are the normal screens (not the immersive ones) and the runtime takes cylinder layers.
    uint32_t CurveFor(const Immersive::ScreenLayers& shown) const;

    // GUI thread.
    void SetFrontendHooks(FrontendHooks hooks);
    void SetGameRunning(bool running);
    void SetLibraryDirs(const std::vector<LibraryDir>& dirs);
    // Any thread: the mirrored Qt window replaces the picker (frame_xr/qt_mirror).
    void SetHomePanel(std::function<void(const HomePanelEvent&)> on_event);
    void SetHomePanelImage(std::shared_ptr<const HomePanelImage> image);
    void SetInGameMenu(bool open);
    // AzaharFrame tabmenu: any thread.
    void SetInGameMenuWindow(bool window);
    void SetInGameMenuInfo(const std::string& title, const std::vector<MenuSaveSlot>& slots,
                           bool per_game);
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
    /// AzaharFrame curve: grip + D-pad up (+1) or down (-1) on the normal screens.
    void StepScreenCurve(int dir);
    /// AzaharFrame controls: the hint chip under the held screens (or low in front of you for
    /// the immersive screen).
    void ShowGripHint(bool immersive, const Input::ScreenQuads& quads, bool swapped);
    /// Both bumpers + View + Menu outside a game (the mapper finds it in a game): once per chord.
    bool ModeChord(const Input::InputFrame& in);
    /// Asks the Qt side to switch between VR and flat (FrontendHooks::toggle_vr).
    void RequestToggleVr(const char* where);
    /// View, then a bumper (the mapper's quick_save / quick_load): asks the Qt side.
    void RequestQuick(const Input::PanelCommands& cmd, const char* where);
    /// View, then the Right Trigger (the mapper's toggle_fast_forward): asks the Qt side.
    void RequestFastForward(const Input::PanelCommands& cmd, const char* where);
    /// Everything held stays away from the game until let go (a new mode, the menu closed).
    void SwallowHeld();
    void AddCursor(const XrPosef& pose, float size_m, std::vector<Quad>& layers);
    // AzaharFrame keyboard: Azahar's window and the headset keyboard under it (frame_xr/keyboard).
    void WindowFrame(Runtime& runtime, const FrameInfo& info, const Input::InputFrame& in,
                     bool have_input, std::vector<Quad>& layers, float dot_m);

    Input::Actions actions;
    Input::InputBridge bridge;
    Input::ScreenView screen_view; ///< AzaharFrame screenswap: focus and hiding, XR thread only
    /// AzaharFrame vrsize: grip + D-pad turns the held screen a step (XR thread only).
    Input::ScreenRig::DpadNudger nudger;
    /// AzaharFrame curve: the runtime takes cylinder layers (XR_KHR_composition_layer_cylinder).
    bool cylinder_ok = false;
    bool normal_dragging = false; ///< a grip held the normal screens last frame (for the log)
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
    Vis::Visuals visuals;   ///< AzaharFrame vrvis: help, stats, beams, controller models

    // The in-game menu: the home panel over the paused game (Menu held 1 s).
    std::atomic<bool> menu_open{false}; ///< set by the Qt side once the game paused
    /// AzaharFrame tabmenu: the menu (false) or Azahar's window (true) shows; the Qt side's.
    std::atomic<bool> menu_window{false};
    MenuPanel tab_menu;
    /// The tab menu's work: VrUi's part of it (the screens) and the Qt side's (MenuFrame).
    void TabMenuFrame(Runtime& runtime, const FrameInfo& info, const Input::InputFrame& in,
                      bool have_input, std::vector<Quad>& layers);
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
    bool stopping_shown = false;    ///< AzaharFrame controls: "Stopping..." shown for this hold
    bool was_holding = false;       ///< a grip held a screen last game frame (the hint chip)
    int64_t last_hint_ns = 0;       ///< when the hint chip last showed

    bool chord_down = false; ///< ModeChord's edge
    /// After a VR / flat switch the chord's buttons stay away from everything until all four
    /// were let go, at least 1 s into the new mode (see ChordLatch).
    Input::ChordLatch mode_latch;
    std::mutex flat_mutex;
    FlatInputSource flat_input;
    bool flat_had_input = false; ///< for the log line when the gamepad comes and goes
    bool flat_menu_shown = false; ///< AzaharFrame flat4: FlatFrame is in the Flat menu
    bool flat_back_down = false;  ///< B's edge in the Flat menu
    /// Test driver: what the game was sent in flat (LogFlatGameButtons).
    void LogFlatGameButtons(const Input::ControlState& controls);
    std::array<bool, Input::kNumControls> flat_sent{};

    std::mutex hooks_mutex;
    FrontendHooks hooks;
    std::atomic<bool> game_running{false};
    std::atomic<bool> launch_pending{false};
    XrTime launch_time = 0;
};

} // namespace FrameXr
