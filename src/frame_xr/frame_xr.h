// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Azahar for Frame's OpenXR frontend: one XrInstance and Vulkan device for the whole run of
// citra_qt --vr / --flat, an XrSession while in VR (ended for flat, made again for VR), a frame
// loop thread at display rate that shows the game's three screens as quad layers, and the
// extension points the input, picker and immersive code build on.
//
// Threads: Start / Shutdown on the GUI thread. The renderer (emu thread) draws the
// screens into the swapchains through VideoCore::XrScreenSink. The XR thread waits,
// begins and ends frames and calls the frame callbacks; it never records or
// submits GPU work while a game runs.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "common/common_types.h"
#include "frame_xr/input/input_frame.h"
#include "frame_xr/xr_includes.h"
#include "video_core/vulkan_hooks.h"

namespace Frontend {
enum class WindowSystemType : u8;
}

namespace FrameXr {

struct StartOptions {
    /// The window system of the Qt window (picks the Vulkan surface extension).
    Frontend::WindowSystemType window_type{};
    /// Instance extensions enabled when the runtime offers them (other parts add theirs).
    std::vector<std::string> optional_extensions{"XR_FB_display_refresh_rate",
                                                 "XR_VALVE_frame_controller_interaction",
                                                 "XR_KHR_composition_layer_cylinder"};
    /// True: VR at once (the session too). False: flat first (no session until SetVr(true)).
    bool vr = true;
};

/// Makes the XR instance, the Vulkan device (through Azahar's own create infos), the
/// session (StartOptions::vr) and the frame loop thread. Call before any game boots. False:
/// no XR (the emulator runs flat for good; nothing is installed). True with vr set but no
/// session made: flat for now (VrShowing() false), SetVr(true) tries again.
bool Start(const StartOptions& options);

/// Ends the session and destroys everything Start made. Call once no renderer
/// exists any more (after the Qt event loop returned).
void Shutdown();

/// True between a successful Start and Shutdown (the session may be idle).
bool IsStarted();

/// True if an instance extension was enabled.
bool HasExtension(const char* name);

// ---- VR and flat in one run --------------------------------------------------------
// VR: a session runs, the headset shows the screens, the renderer draws only into the XR
// swapchains. Flat: no session (the instance and the Vulkan device stay), the renderer draws
// Azahar's window only, games see the 3D slider at 0, and the controllers come from
// SetFlatInput (Steam Input's gamepad) through the same mapping as in VR.

/// Asks for VR (true: make a session) or flat (false: end it). Any thread; the XR thread does
/// it, and FrontendHooks::vr_changed reports the result. False when XR is not started.
bool SetVr(bool vr);
/// The mode asked for last.
bool WantsVr();
/// A session exists (VR). False in flat, before Start and after the instance was lost.
bool VrShowing();

/// Flat: fills one frame of controller input (buttons, sticks, triggers; no aim) and returns
/// true, or false when no gamepad is there. Called on the XR thread about 120 times a second.
using FlatInputSource = std::function<bool(Input::InputFrame& frame)>;
void SetFlatInput(FlatInputSource source);

// ---- Extension points ------------------------------------------------------

struct FrameInfo {
    XrInstance instance;
    XrSession session;
    XrSpace local_space;   ///< LOCAL reference space (all quads are placed in it)
    XrSpace view_space;    ///< VIEW reference space
    XrTime display_time;   ///< predicted display time of the frame
    XrDuration display_period;
    XrSessionState state;
    bool should_render;
    bool screens_live;     ///< a game is producing images
};

/// Called on the XR thread once per display frame, after xrBeginFrame and the view
/// locate, before the layers are built (input sync, picker, placement updates).
/// Must not block for long and must not submit GPU work while a game runs.
using FrameCallback = std::function<void(const FrameInfo&)>;
int AddFrameCallback(FrameCallback callback);
void RemoveFrameCallback(int id);

/// Called on the XR thread for every session state change.
using StateCallback = std::function<void(XrSessionState state)>;
int AddStateCallback(StateCallback callback);
void RemoveStateCallback(int id);

/// A quad layer in LOCAL space, submitted every frame while visible.
struct Quad {
    XrSwapchain swapchain = XR_NULL_HANDLE;
    XrRect2Di image_rect{};
    XrPosef pose{{0, 0, 0, 1}, {0, 0, 0}};
    XrExtent2Df size{1.0f, 1.0f};
    XrEyeVisibility eye = XR_EYE_VISIBILITY_BOTH;
    XrCompositionLayerFlags flags = 0;
    int order = 0; ///< larger is drawn later (on top); the game screens are order 0
    bool visible = true;
    bool with_game = true; ///< false: hidden while a game's screens are shown
};
int AddQuad(const Quad& quad);
bool SetQuad(int id, const Quad& quad);
void RemoveQuad(int id);

/// The last located head and eye poses (LOCAL space).
struct ViewPose {
    bool valid = false;
    XrTime time = 0;
    XrPosef head{{0, 0, 0, 1}, {0, 0, 0}};
    XrPosef eye[2]{{{0, 0, 0, 1}, {0, 0, 0}}, {{0, 0, 0, 1}, {0, 0, 0}}};
    XrFovf fov[2]{};
};
ViewPose LatestViewPose();

/// Where the game screens are (LOCAL space; sizes in metres).
struct ScreenPlacement {
    XrPosef top_pose;
    XrExtent2Df top_size;
    XrPosef bottom_pose;
    XrExtent2Df bottom_size;
    /// AzaharFrame vrsize: the top screen's size, times NormalTopSize (grip + stick sideways;
    /// input/screen_rig.h). The bottom screen keeps its size and moves down with the top edge.
    float top_scale = 1.0f;
    /// AzaharFrame move: Detach Screens (input/screen_rig.h Slots). `top_pose` and `top_scale`
    /// are the main slot's; `bottom_pose` and `bottom_size` the second slot's as drawn. While
    /// `second_free` (and the setting is not Off) the second slot is at `second_pose`, NormalBottomSize
    /// times `second_scale`; else it follows the main slot by the Screen Layout.
    bool second_free = false;
    XrPosef second_pose{{0, 0, 0, 1}, {0, 0, 0}};
    float second_scale = 1.0f;
};
/// The normal top screen's size in metres: 1 m high times `scale`, as wide as "VR Screen
/// Shape" says.
XrExtent2Df NormalTopSize(float scale = 1.0f);
/// AzaharFrame move: the second slot's size at `scale` (1: the bottom screen as CitraVR).
XrExtent2Df NormalBottomSize(float scale = 1.0f);
ScreenPlacement DefaultScreenPlacement();
ScreenPlacement GetScreenPlacement();
void SetScreenPlacement(const ScreenPlacement& placement);

// ---- Qt side -------------------------------------------------------------------

/// AzaharFrame flat3: what a stick click asks of the window's screen layout in flat, in a game.
enum class FlatScreens {
    Swap,        ///< right stick click, tap: Swap Screens
    ToggleOther, ///< left stick click, tap: Single Screen, or back to the layout before it
    Reset,       ///< right stick click, held 1 s: flat's default layout (Large Screen, not swapped)
    CycleLayout,    ///< AzaharFrame layout: Large Screen, Side by Side, Default, and round again
    PreviousLayout, ///< AzaharFrame layout: the same cycle backwards (grip + D-pad left)
};

/// AzaharFrame tabmenu: what the in-game menu asks of the Qt side (FrontendHooks::menu_request).
enum class MenuCommand {
    Continue,     ///< close the menu, the game continues
    QuickSave,    ///< close the menu, then Quick Save
    QuickLoad,    ///< close the menu, then Quick Load
    SaveSlot,     ///< `value`: slot 1 to 10
    LoadSlot,     ///< `value`: slot 1 to 10
    Screenshot,   ///< close the menu, then Tools > Capture Screenshot
    Restart,      ///< Emulation > Restart
    CloseGame,    ///< stop the game (as View held)
    SwitchToFlat, ///< as the four-button chord
    ShowWindow,   ///< Azahar's window over the paused game (the mirrored window)
    MoreSettings, ///< the same, with Emulation > Configure... open
    Cheats,       ///< AzaharFrame menu: the same, with the game's Cheats tab open
                  ///< (the Cheats tab's Azahar's Window)
    EditControls, ///< AzaharFrame menu: the controls remap (the remap lane's)
    SetSetting,   ///< `setting` to `value`, `per_game` for the running game only
    // AzaharFrame cheatstab: the Cheats tab (core/cheats/online.h does the work)
    ToggleCheat,    ///< `value`: the cheat's index (ListCheats order), on or off, at once, saved
    DownloadCheats, ///< the developer's and Sharkive's cheats for the game
    // AzaharFrame texui: the Mods tab's Mods and Textures segments (citra_qt/frame_mods.cpp;
    // core/mods/library.h and core/custom_tex/packs.h do the work). The snackbar's Restart is
    // Restart; Custom Textures and Preload are settings.
    SelectTexturePack, ///< `text`: the pack's id, "" for Off
    LoadModCatalog,    ///< Install Mods opened: the catalog (once a run), the title's mods
    InstallMod,        ///< `text`: the mod's id (a conflict asks first)
    SetModActive,      ///< `text`: a managed mod's id, `value` 1 into the mods folder, 0 out
    RemoveMod,         ///< `text`: the mod's id (your own files ask first)
    ModsDialog,        ///< the dialog's answer: `value` 1 Replace / Remove, 0 Cancel / OK
    OpenUrl,           ///< `text`: an author's page the Qt side listed, in Steam's browser
    LoadPackDownloads, ///< Download Packs opened: the packs for the title
    DownloadPack,      ///< `text`: the catalog pack's id (a large one asks first)
    CancelPackDownload,
    DeletePack,        ///< `text`: the downloaded pack's id (CustomTex::Pack::id), asks first
    TexturesDialog,    ///< the Textures dialog's answer: `value` 1 Download / Delete, 0 Cancel
};
enum class MenuSetting {
    HideBottomScreen,
    ScreenShape,
    ScreenCurve, ///< `value`: degrees (0, 15, 30, 45, 60, 75, 90)
    ScreenLayout, ///< AzaharFrame layout: `value` 0 Stacked, 1 Side by Side
    ImmersiveOffsetReset,
    ShowStats,
    StatsPosition, ///< AzaharFrame statspos: `value` 0 In Front, 1 On Wrist
    Background,
    FaceButtons,
    MotionControls,
    ShowControllers,
    WindowScale, ///< `value`: percent
    ImmersiveMode,
    WiderView,
    GameFov, ///< `value`: degrees
    NarrowScreen,
    DepthFactor,
    Depth3d, ///< AzaharFrame depth3d: `value` 0 to 100 (Settings::values.factor_3d)
    MoveScreens,   ///< AzaharFrame menu: `value` 0 Off, 1 Other, 2 Both (Detach Screens)
    TouchpadTouch, ///< AzaharFrame menu: `value` 0 Off, 1 Touch, 2 Click
    CustomTextures,  ///< AzaharFrame texui: `value` 0 or 1 (Settings::values.custom_textures)
    PreloadTextures, ///< AzaharFrame texui: `value` 0 or 1 (Settings::values.preload_textures)
};
struct MenuRequest {
    MenuCommand command = MenuCommand::Continue;
    MenuSetting setting = MenuSetting::HideBottomScreen;
    int value = 0;
    bool per_game = false;
    std::string text; ///< AzaharFrame texui: a pack's or a mod's id
};

/// What the XR thread asks the Qt side to do (both must queue to the GUI thread).
struct FrontendHooks {
    std::function<void(const std::string& path)> launch_game; ///< picker selection
    std::function<void()> stop_game;                          ///< View held 1.5 s
    /// Menu held 1 s in a game: open or close the in-game menu (VR) or the Flat menu (flat,
    /// Azahar's window over the paused game).
    std::function<void()> toggle_menu;
    /// AzaharFrame flat4: B in the Flat menu: close the menu or dialog that is open, else the
    /// Flat menu. Queue to the GUI thread.
    std::function<void()> menu_back;
    /// Left Bumper + Right Bumper + View + Menu (VR or flat): switch modes (the Qt side calls
    /// SetVr, writes the mode file, fades the window).
    std::function<void()> toggle_vr;
    /// View, then the Right Bumper (VR or flat, in a game): Quick Save. Queue to the GUI thread.
    std::function<void()> quick_save;
    /// View, then the Left Bumper: Quick Load.
    std::function<void()> quick_load;
    /// View, then the Right Trigger (VR or flat, in a game): fast forward on or off (Azahar's
    /// turbo speed). Queue to the GUI thread.
    std::function<void()> toggle_fast_forward;
    /// The XR thread made a session (true) or ended it (false), asked for or not (the runtime
    /// may end it, or a new one may fail): the mode now in effect.
    std::function<void(bool vr)> vr_changed;
    /// AzaharFrame vrvis: the Stats button next to Azahar's window switched "Show Stats" (the Qt
    /// side sets Settings::values.vr_show_stats and saves the config).
    std::function<void(bool on)> show_stats_changed;
    /// AzaharFrame vrsize: grip + D-pad changed a global VR setting (the XR thread has set
    /// Settings::values.vr_immersive_offset_x and _y, or vr_screen_curve): save the config.
    /// Any thread.
    std::function<void()> vr_setting_changed;
    /// AzaharFrame widerview: grip + D-pad changed a per-game setting of the game on view (the XR
    /// thread has set Settings::values.vr_immersive_wider_view_level for it): save that game's
    /// own config. Any thread.
    std::function<void()> game_setting_changed;
    /// AzaharFrame flat3: a stick click in flat, in a game (the window's screen layout; the game
    /// sees nothing of it). Queue to the GUI thread.
    std::function<void(FlatScreens what)> flat_screens;
    /// AzaharFrame tabmenu: a button or setting of the in-game menu (menu_panel.h). Queue to the
    /// GUI thread.
    std::function<void(const MenuRequest& request)> menu_request;
};
void SetFrontendHooks(FrontendHooks hooks);

/// A game started (BootGame done) or stopped (ShutdownGame done). GUI thread.
void SetGameRunning(bool running);

/// The title id of the game being started (0 for none or homebrew without one), before it
/// runs. The immersive screen's placement is saved per title id. Any thread.
void SetGameTitleId(uint64_t title_id);
/// AzaharFrame move: the title id SetGameTitleId gave last (0 for none). Any thread.
uint64_t GameTitleId();

/// The folders the picker scans (UISettings::values.game_dirs). GUI thread; rescans.
struct LibraryDir {
    std::string path;
    bool deep_scan = false;
    int max_depth = -1; ///< >= 0 overrides the depth deep_scan implies
};
void SetLibraryDirs(const std::vector<LibraryDir>& dirs);

// ---- The home panel: Azahar's Qt window mirrored into VR (citra_qt/frame_mirror.cpp) ----

/// An image of the Qt window: RGBA8, sRGB values, opaque, rows top down.
struct HomePanelImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba;
    /// How much wider the panel is than the width it was placed with (1 = as placed): a window
    /// shown larger, or a dialog that does not fit it, gets a larger panel (>= 1).
    float panel_scale = 1.0f;
};

/// What the controllers did on the home panel. u, v: 0..1 across the image (v down).
struct HomePanelEvent {
    /// AzaharFrame gamelist: ListStep moves the game list's highlight `step` games (-1 up, +1
    /// down), ListStart starts the highlighted game (input/list_nav.h).
    enum class Type : u8 { Move, Leave, Press, Release, Wheel, Escape, ListStep, ListStart };
    enum class Button : u8 { Left, Right };
    Type type = Type::Move;
    Button button = Button::Left;
    float u = 0.0f;
    float v = 0.0f;
    float wheel = 0.0f; ///< Wheel: notches, positive scrolls up
    int step = 0;       ///< ListStep
};

/// Non-empty: the home panel replaces the picker while no game runs, and `on_event`
/// gets its input (called on the XR thread: queue to the GUI thread). Empty: the
/// picker again. Call after Start.
void SetHomePanel(std::function<void(const HomePanelEvent&)> on_event);
/// The home panel's newest image (any thread). Shown from the next frame on.
void SetHomePanelImage(std::shared_ptr<const HomePanelImage> image);
/// AzaharFrame gamelist: whether the controllers drive the game list now (input/list_nav.h,
/// ListNavAllowed), in VR and in Flat: the left stick moves the highlight instead of scrolling,
/// and the D-pad and A's double tap work. Any thread; GUI thread when the window changes.
void SetHomePanelListNav(bool enabled);
/// The in-game menu: the home panel in front of the (paused) game's screens, which are dimmed;
/// the controllers drive the panel and not the game. GUI thread, after the game paused.
void SetInGameMenu(bool open);
/// AzaharFrame tabmenu: while the in-game menu is open, show Azahar's window (true; the Qt side
/// captures it as the home panel, for "Azahar's Window", "More Settings" and any dialog) or the
/// menu (false). GUI thread.
void SetInGameMenuWindow(bool window);
/// AzaharFrame tabmenu: the in-game menu's game title, the game's save state slots (0 is the
/// quick save, 1 to 10 the others; slots not listed are empty) and whether it can have settings of
/// its own (it has a title id). GUI thread, as the menu opens.
struct MenuSaveSlot {
    int slot = 0;
    std::string time;
};
void SetInGameMenuInfo(const std::string& title, const std::vector<MenuSaveSlot>& slots,
                       bool per_game);
/// AzaharFrame cheatstab: the Mods tab's segments' values (frame_xr/menu/cheats_tab.h,
/// mods_segments.h), from the Qt side whenever they change. Any thread.
namespace Menu {
struct CheatsModel;
struct ModsListModel;
struct TexturesModel;
} // namespace Menu
void SetInGameMenuCheats(const Menu::CheatsModel& cheats); ///< citra_qt/frame_cheats.cpp
void SetInGameMenuModsList(const Menu::ModsListModel& mods); ///< citra_qt/frame_mods.cpp
void SetInGameMenuTextures(const Menu::TexturesModel& textures);

/// A short message on a small panel in front of you for two seconds (any thread; VR only).
enum class Toast {
    QuickSaved = 0,
    QuickLoaded = 1,
    NoQuickSave = 2,
    FastForward = 3,
    NormalSpeed = 4,
};
void ShowToast(Toast kind);
/// AzaharFrame vrsize: a short message of any text (UTF-8, one line; a degree sign is drawn
/// too) in the same place and for as long as a toast. Any thread.
void ShowToastText(const std::string& text);
/// AzaharFrame controls: takes a text toast back before its time if it is `text` (any thread).
void HideToastText(const std::string& text);
/// AzaharFrame controls: the grip hint chip (two short lines, '\n'; "↕" and "↔" are drawn as
/// arrows) at `pose` in LOCAL space, `width_m` wide, for three seconds (any thread; VR only).
void ShowHint(const std::string& text, const XrPosef& pose, float width_m);
/// Fast forward turned on or off: the "Fast Forward" or "Normal Speed" toast (VR only). Any thread.
void SetFastForwardShown(bool on);

/// True when the runtime can show the room behind the screens (Settings "Background:
/// Passthrough" then works): it offers XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND. False before Start.
bool PassthroughSupported();

// ---- AzaharFrame vrvis: controls help, stats, controller models (frame_xr/vis) ----------------

/// The emulator's numbers for the stats panel, as Azahar's status bar shows them.
struct EmuStats {
    double game_fps = 0.0;        ///< "App: N FPS"
    double emulation_speed = 0.0; ///< 1.0 = 100 %
    double frame_ms = 0.0;        ///< "Frame: N ms"
    double frame_peak_ms = 0.0;
    uint32_t slow_frames = 0;
};
/// GUI thread, each time the status bar updates (about once a second in a game).
void SetEmuStats(const EmuStats& stats);
/// The controls profile in use (Settings::InputProfile buttons and analogs), for the controls
/// help panel. GUI thread: when VR starts, after Configure, when a game boots.
void SetControlBindings(const std::vector<std::string>& buttons,
                        const std::vector<std::string>& analogs, const std::string& profile);
/// Shows, hides or toggles the controls help next to Azahar's window. Any thread.
void ShowControlsHelp(bool show);
void ToggleControlsHelp();

/// Handles for the other parts (XR_NULL_HANDLE before Start / before the first game).
XrInstance Instance();
XrSystemId System();
XrSession Session();
XrSwapchain ScreenSwapchain(VideoCore::XrScreen screen);

} // namespace FrameXr
