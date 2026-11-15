// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Azahar for Frame's OpenXR frontend: one Vulkan device for the whole run of citra_qt --vr /
// --flat, an XrInstance and XrSession while in VR (both ended for flat, made again for VR:
// instance_policy.h), a frame
// loop thread at display rate that shows the game's three screens as quad layers, and the
// extension points the input, picker and immersive code build on.
//
// Threads: Start / Shutdown on the GUI thread. The renderer (emu thread) draws the
// screens into the swapchains through VideoCore::XrScreenSink. The XR thread waits,
// begins and ends frames and calls the frame callbacks; it never records or
// submits GPU work while a game runs.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "common/common_types.h"
#include "frame_xr/image_lag.h" // AzaharFrame imglag: ShownPicture
#include "frame_xr/input/input_frame.h"
#include "frame_xr/toast_fit.h" // AzaharFrame ui3: ToastTime
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
/// it, and FrontendHooks::vr_changed reports the result. False when XR is not started, and for
/// VR once the OpenXR runtime has gone away (AzaharFrame flat).
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
    /// AzaharFrame jump: xrLocateViews succeeded with both eyes' orientations valid this frame
    /// (else `eye` and `fov` are the defaults).
    bool views_valid = false;
    /// AzaharFrame ipd: both eye positions valid and tracked this frame (the IPD is measured
    /// from them).
    bool eye_positions_tracked = false;
};
ViewPose LatestViewPose();
/// AzaharFrame doubling: the head pose the game picture released last was drawn with (immersive
/// modes 2 and 3; invalid otherwise). Any thread.
VideoCore::XrContentPose ShownContentPose();
/// AzaharFrame heldquad: the last kShownPictures pictures released (ShownPicture, image_lag.h),
/// newest first. Any thread.
std::array<ShownPicture, kShownPictures> ShownPictures();
/// AzaharFrame imglag: the picture the compositor shows on this display refresh: with the image
/// lag of the runtime (ImageLagInEffect) the one the last xrEndFrame carried, found by its serial
/// (ImageLag::PictureOnView). XR thread.
ImageLag::OnView PictureOnView();
/// AzaharFrame imglag: the image lag in effect, 0 or 1 refresh (the setting vr_xr_image_lag, or
/// the live test switch AZAHARFRAME_XR_IMAGE_LAG). Any thread.
uint32_t ImageLagInEffect();
/// AzaharFrame imglag: the fallback and mispair counts since the runtime started. Any thread.
ImageLag::Counts ImageLagCounts();

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
    ReportProblem, ///< AzaharFrame crashreport: the same, with Report a Problem open
    AngelsAimReport, ///< AzaharFrame kiumenus: the same, with Angel's Aim's Send Report open
    About,         ///< AzaharFrame crashreport: the same, with About Azahar for Frame open
    Cheats,       ///< AzaharFrame menu: the same, with the game's Cheats tab open
                  ///< (the Cheats tab's Azahar's Window)
    EditControls, ///< AzaharFrame menu: Edit Controls started (nothing for the Qt side to do)
    SetButtons,   ///< AzaharFrame remap: `buttons` are the controls profile's 3DS buttons (18)
    SetSetting,   ///< `setting` to `value`, `per_game` for the running game only
    UseGlobalValue, ///< AzaharFrame useglobal: the running game's own `setting` goes (its file too)
    // AzaharFrame cheatstab: the Cheats tab (core/cheats/online.h does the work)
    ToggleCheat,    ///< `value`: the cheat's index (ListCheats order), on or off, at once, saved
    DownloadCheats, ///< the developer's and Sharkive's cheats for the game
    AddCheat,       ///< AzaharFrame ui2: a name and a code on the game's keyboard (Flat: dialogs)
    // AzaharFrame texui: the Mods tab's Mods and Textures segments (citra_qt/frame_mods.cpp;
    // core/mods/library.h and core/custom_tex/packs.h do the work). The snackbar's Restart is
    // Restart; Custom Textures and Preload are settings.
    SelectTexturePack, ///< `text`: the pack's id; `value` 1 on, 0 off (AzaharFrame texlayers)
    LoadModCatalog,    ///< Install Mods opened: the catalog (once a run), the title's mods
    InstallMod,        ///< `text`: the mod's id (a conflict asks first)
    SetModActive,      ///< `text`: a managed mod's id, `value` 1 into the mods folder, 0 out
    RemoveMod,         ///< `text`: the mod's id (your own files ask first)
    ModsDialog,        ///< the dialog's answer: `value` 1 Replace / Remove, 0 Cancel / OK
    OpenUrl,           ///< `text`: an author's page the Qt side listed, in Steam's browser
    LoadPackDownloads, ///< Download Packs opened: the packs for the title
    DownloadPack,      ///< `text`: the catalog pack's id (a large one asks first)
    CancelPackDownload,
    CancelModInstall, ///< AzaharFrame integ3: the catalog mod downloading now
                      ///< (Mods::Library::Cancel)
    DeletePack,       ///< `text`: the downloaded pack's id (CustomTex::Pack::id), asks first
    TexturesDialog,   ///< the Textures dialog's answer: `value` 1 Download / Delete, 0 Cancel
    AskPreload,       ///< AzaharFrame memguard: Preload switched on: ask first, with the numbers
    TurnBackOn,       ///< AzaharFrame compat: the "Turned Off" snackbar's action
    // AzaharFrame compat: InstallMod and DownloadPack with `value` 1: after "This May Not Work"
    // (Install Anyway, Download Anyway), for an entry made for another version
    // AzaharFrame zipinstall: Install from Zip (Menu::ZipModel, citra_qt/frame_mods.cpp)
    InstallFromZip, ///< `value`: Menu::ZipTarget; VR opens the browser, Flat Qt's file dialog
    ZipBrowse,      ///< `text`: the folder to list
    ZipPick,        ///< `text`: the archive to install
    ZipChoose,      ///< `value`: the index of the game in ZipModel::choices
    ZipDialog,      ///< the page's dialog: `value` 1 Replace, 0 Cancel / OK
    ZipClose,       ///< the page's back button
    // AzaharFrame emerald: the Mods tab's Homebrew segment (citra_qt/frame_homebrew.cpp), `text`:
    // the package's id
    HomebrewInstall, ///< Azahar's window, then the package's own steps (the ROM, a game folder)
    HomebrewUpdate,  ///< the same for an update
    HomebrewPlay,    ///< close the menu and start the installed package
    HomebrewCheck,   ///< AzaharFrame ui3: Check for Updates now (a toast says what it found)
    HomebrewUninstall, ///< AzaharFrame emhb: Uninstall's dialog: `value` 0 open, 1 the checkbox,
                       ///< 2 Cancel, 3 Uninstall
    // AzaharFrame texparts: a pack's Customize page (Menu::TexturePartsModel, frame_mods.cpp)
    OpenTextureParts, ///< `text`: the pack's id (CustomTex::Pack::id)
    TexturePart,      ///< `value`: the row clicked (a checkbox or a radio button)
    MoveTexturePart,  ///< `value`: the row; `text` "up" or "down"
    TexturePartsAll,  ///< `value` 1 All On, 0 All Off
    // AzaharFrame texlayers: Enabled Textures (Menu::TexturesModel::layers)
    TextureLayer,     ///< `value`: the row's checkbox
    MoveTextureLayer, ///< `value`: the row; `text` "up" or "down"
    // AzaharFrame ui3: Optimize for Frame on a pack's page (frame_mods.cpp), `text`: the page's
    // pack
    OptimizePack,       ///< the estimate, then "Optimize Texture Pack?" (TexturesDialog answers it)
    OptimizeStop,       ///< stops the conversion (carried on by the next Optimize)
    UseOriginalPack,    ///< the PNG pack again, from the next start
    RemoveOriginalPack, ///< "Remove the Original Pack?" (TexturesDialog answers it), then deletes
                        ///< it
    NoticeAnswer, ///< AzaharFrame savestate: SetInGameMenuNotice's dialog: `value` 1 its confirm, 0
                  ///< cancel, 2 extra (memwarn)
    // AzaharFrame autosave
    LoadAutosave, ///< `value`: the index in SetInGameMenuAutosaves' list (newest first)
    UndoLoad,     ///< back to the moment before the autosave was loaded
    // AzaharFrame ctgp7: the Mods tab's CTGP-7 segment (citra_qt/frame_ctgp7.cpp)
    Ctgp7Install, ///< install CTGP-7 for the open Mario Kart 7 (its questions in the segment)
    Ctgp7Launch,  ///< close the menu and start the CTGP-7 launcher
    Ctgp7Cancel,  ///< stop the install that runs
    Ctgp7Dialog,  ///< the segment's question: `value` 1 its confirm, 0 cancel, 2 its third
                  ///< button; `text` "checked" when its checkbox is ticked
    Ctgp7Uninstall, ///< asks "Uninstall CTGP-7?" (answered with Ctgp7Dialog)
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
    ControllersInFront, ///< AzaharFrame ctrlfront: `value` 0 Off, 1 Primary, 2 Secondary, 3 Both
    WindowScale, ///< `value`: percent
    ImmersiveMode,
    WiderView,
    /// AzaharFrame fovaxis: "Render Margin: Horizontal" and "Render Margin: Vertical", `value`
    /// the level 0 to 4 (vr_immersive_render_margin_x, _y; frame_xr/render_margin.h)
    MarginX,
    MarginY,
    GameFov, ///< `value`: degrees
    MatchCamera, ///< AzaharFrame depth, fovonce: `value` 0 Off, 1 Set Once, 2 Always
                 ///< (vr_immersive_match_camera_mode); Set Once clears vr_immersive_fov_measured
    ScaleX, ///< AzaharFrame kiu, fov100: `value` percent, -100 to 100 (vr_immersive_scale_x)
    ScaleY, ///< vr_immersive_scale_y
    ScaleMode, ///< AzaharFrame fovscale: `value` 0 Stretch, 1 Field of View (vr_immersive_scale_mode)
    WorldScale,   ///< AzaharFrame natural: `value` hundredths of a percent (vr_immersive_world_scale)
    FollowScreen, ///< AzaharFrame natural: `value` 0 or 1 (vr_immersive_follow_screen)
    MotionClarity, ///< AzaharFrame flash: `value` 0 or 1 (vr_motion_clarity)
    /// AzaharFrame natural: set while the game plays (Immersive::TakeWorldScaleSave), for its own
    /// file: `value` World Scale in hundredths of a percent (below 0: unchanged; `text` "migrated"
    /// sets vr_immersive_world_set too), `text` also "k=<eye per far>" for vr_immersive_eye_per_far.
    WorldScaleSaved,
    Depth3d, ///< AzaharFrame depth3d: `value` 0 to 100 (Settings::values.factor_3d)
    MoveScreens,   ///< AzaharFrame menu: `value` 0 Off, 1 Secondary, 2 Both (Detach Screens)
    TouchpadTouch, ///< AzaharFrame menu: `value` 1 Tap for Input, 2 Click for Input (0 Off)
    BottomScreenReticle, ///< AzaharFrame mouse: `value` 0 Off, 1 Left, 2 Right, 3 Headset
    ReticleAtEdge,       ///< AzaharFrame mouse: `value` 0 Follow, 1 Snap to Middle
    CustomTextures,  ///< AzaharFrame texui: `value` 0 or 1 (Settings::values.custom_textures)
    PreloadTextures, ///< AzaharFrame texui: `value` 0 or 1 (Settings::values.preload_textures)
    InternalResolution, ///< AzaharFrame menuux: `value` 1 to 8 (resolution_factor)
    DynamicResolution,  ///< AzaharFrame menuux: `value` 0 or 1 (vr_dynamic_resolution)
    TextureFilter,      ///< AzaharFrame menuux: `value` a Settings::TextureFilter
    AsyncShaders,       ///< AzaharFrame menuux: `value` 0 or 1 (async_shader_compilation)
    FastForwardSpeed,   ///< AzaharFrame menuux: `value` percent (turbo_limit)
    FastForwardAudio,   ///< AzaharFrame ffaudio: `value` a Settings::FastForwardAudio
    Autosave,      ///< AzaharFrame autosave: `value` 0 or 1 (autosave_enabled), may be per game
    AutosaveEvery, ///< `value` minutes (autosave_every)
    AutosaveKeep,  ///< `value` 1 to 10 (autosave_keep)
    ReticleSpeed,  ///< AzaharFrame reticle: `value` tenths of 1x, 5 to 40 (vr_reticle_speed)
    AccurateImageCopies, ///< AzaharFrame accxfer, xfergpu: `value` 0 Off, 1 Automatic, 2 On
                         ///< (image_copy_accuracy)
    /// AzaharFrame fovonce: Match Game Camera's Set Once measured the game on view: `value` its
    /// Immersive Game FOV (degrees), with vr_immersive_fov_measured, in the game's own file.
    MeasuredGameFov,
    ReticleCenter, ///< AzaharFrame fixes7: `value` 0 or 1 (vr_reticle_center_until_pressed)
    ReticleStick,  ///< AzaharFrame fixes7: `value` 0 or 1 (vr_reticle_stick_moves)
    ReticleEdgeCStick, ///< AzaharFrame kiu: `value` 0 or 1 (vr_reticle_edge_cstick)
    GyroAim,         ///< AzaharFrame kiugyro: `value` 0 Off, 1 Gyro, 2 Light Gun (vr_gyro_aim)
    GyroSensitivity, ///< `value` tenths of 1x, 2 to 40 (vr_gyro_sensitivity)
    TurnZone,        ///< `value` percent, 0 to 30 (vr_light_gun_turn_zone)
    TurnSpeed,       ///< `value` tenths of 1x, 2 to 40 (vr_light_gun_turn_speed)
    Recoil,          ///< `value` 0 or 1 (vr_light_gun_recoil)
    WeaponInHand,    ///< AzaharFrame kiuguns: `value` 0 or 1 (vr_light_gun_weapon)
    LaserSight,      ///< AzaharFrame kiuguns: `value` 0 or 1 (vr_light_gun_laser)
    MuzzleFlash,     ///< AzaharFrame kiuflash: `value` 0 or 1 (vr_light_gun_flash)
    RecoilCharged,   ///< AzaharFrame kiufix: `value` percent, 0 to 100 (vr_light_gun_recoil_charged)
    RecoilRapid,     ///< `value` percent, 0 to 100 (vr_light_gun_recoil_rapid)
    WeaponSize,      ///< `value` 0 Game Size, 1 Hand Size (vr_light_gun_weapon_size)
    WeaponScale,     ///< `value` percent, 50 to 150 (vr_light_gun_weapon_scale)
};
struct MenuRequest {
    MenuCommand command = MenuCommand::Continue;
    MenuSetting setting = MenuSetting::HideBottomScreen;
    int value = 0;
    bool per_game = false;
    std::string text; ///< AzaharFrame texui: a pack's or a mod's id
    std::vector<std::string> buttons; ///< AzaharFrame remap: SetButtons, Settings::InputProfile::buttons
    std::vector<std::string> analogs; ///< AzaharFrame padsui: SetButtons, InputProfile::analogs (or none)
};

/// AzaharFrame flat: FrontendHooks::flat_vr_prompt. OpenFromGame: the hold began in a game and
/// opened the Flat menu on its way (leaving Fullscreen), so No closes that menu again (the game
/// goes on, Fullscreen again if it was). Open: the game list, or a hold begun in the Flat menu.
enum class FlatVrPrompt { Open, OpenFromGame, Yes, No };

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
    /// AzaharFrame flat: Menu held 3 s in Flat: open the "Switch to VR?" question (Open), or A
    /// (Yes: switch) or B (No: close it) while it shows. The Qt side calls SetFlatVrPrompt(false)
    /// once it closes, however it closes. Queue to the GUI thread.
    std::function<void(FlatVrPrompt what)> flat_vr_prompt;
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
/// AzaharFrame flat: the "Switch to VR?" question (FrontendHooks::flat_vr_prompt) closed (false)
/// or opened (true). While it shows Flat's controller drives only it. Any thread.
void SetFlatVrPrompt(bool open);
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
    bool other = false; ///< AzaharFrame savestate: from another build: `time` is "From 1.1.1"
};
/// AzaharFrame savestate: a dialog over the in-game menu ("About Save States", "This Save State Is
/// From an Older Version"), answered with MenuCommand::NoticeAnswer; `title` "" takes it away;
/// `cancel` "" is a dialog of one button. AzaharFrame memwarn: `extra` a third button before
/// them (NoticeAnswer 2). Any thread.
void SetInGameMenuNotice(const std::string& title, const std::string& body,
                         const std::string& confirm, const std::string& cancel,
                         const std::string& extra = {});
void SetInGameMenuInfo(const std::string& title, const std::vector<MenuSaveSlot>& slots,
                       bool per_game);
/// AzaharFrame cheatstab: the Mods tab's segments' values (frame_xr/menu/cheats_tab.h,
/// mods_segments.h), from the Qt side whenever they change. Any thread.
namespace Menu {
struct CheatsModel;
struct ModsListModel;
struct TexturesModel;
struct ZipModel;
struct HomebrewTabModel; // AzaharFrame emerald
struct AutosaveCard;     // AzaharFrame autosave
struct Ctgp7Model;       // AzaharFrame ctgp7
struct AngelsAimModel;   // AzaharFrame kiufix
} // namespace Menu
void SetInGameMenuCheats(const Menu::CheatsModel& cheats); ///< citra_qt/frame_cheats.cpp
void SetInGameMenuModsList(const Menu::ModsListModel& mods); ///< citra_qt/frame_mods.cpp
void SetInGameMenuTextures(const Menu::TexturesModel& textures);
void SetInGameMenuZip(const Menu::ZipModel& zip); ///< AzaharFrame zipinstall
/// AzaharFrame ctgp7: the Mods tab's CTGP-7 segment (frame_xr/menu/ctgp7_tab.h). Any thread.
void SetInGameMenuCtgp7(const Menu::Ctgp7Model& ctgp7); ///< citra_qt/frame_ctgp7.cpp
/// AzaharFrame kiufix: the Mods tab's Angel's Aim segment (frame_xr/menu/angels_aim_tab.h). Any
/// thread.
void SetInGameMenuAngelsAim(const Menu::AngelsAimModel& angels_aim); ///< citra_qt/frame_mods.cpp
/// AzaharFrame emerald: the Mods tab's Homebrew segment's rows (frame_xr/menu/homebrew_tab.h). Any thread.
void SetInGameMenuHomebrew(const Menu::HomebrewTabModel& homebrew); ///< citra_qt/frame_homebrew.cpp
/// AzaharFrame autosave: the running game's autosaves for Load State's card (newest first), and
/// the Undo Load snackbar's text ("" none). Any thread.
void SetInGameMenuAutosaves(const std::vector<Menu::AutosaveCard>& autosaves,
                            const std::string& undo_load);

/// A short message on a small panel in front of you (any thread; VR only): Quick Saved, Quick
/// Loaded and No Quick Save for five seconds, Fast Forward and Normal Speed for two (ToastTimeOf).
enum class Toast {
    QuickSaved = 0,
    QuickLoaded = 1,
    NoQuickSave = 2,
    FastForward = 3,
    NormalSpeed = 4,
    Autosaved = 5, ///< AzaharFrame autosave: where ShowAutosaved's label would not be seen
};
void ShowToast(Toast kind);
/// AzaharFrame autosave: "Autosaved" for 1.5 s, world-locked 3 cm over the top screen's top-right
/// corner; the Autosaved toast when that corner is not well in view (or in the immersive modes).
/// Any thread.
void ShowAutosaved();
/// AzaharFrame ui3: how long each of those stays.
constexpr ToastTime ToastTimeOf(Toast kind) {
    return kind == Toast::FastForward || kind == Toast::NormalSpeed ||
                   kind == Toast::Autosaved // AzaharFrame autosave
               ? ToastTime::Short
                                                                    : ToastTime::Long;
}
/// AzaharFrame vrsize: a short message of any text (UTF-8, one line; a degree sign is drawn
/// too) in the same place as a toast. Any thread. AzaharFrame ui3: `time` (toast_fit.h): Short
/// (2 s) for the answer to a press, Long (5 s) for a one-shot message.
void ShowToastText(const std::string& text, ToastTime time = ToastTime::Short);
/// AzaharFrame controls: takes a text toast back before its time if it is `text` (any thread).
void HideToastText(const std::string& text);
/// AzaharFrame controls: the grip hint chip (two short lines, '\n'; "↕" and "↔" are drawn as
/// arrows) at `pose` in LOCAL space, `width_m` wide, for three seconds (any thread; VR only).
void ShowHint(const std::string& text, const XrPosef& pose, float width_m);
/// AzaharFrame shaderbadge: the "Preparing Shaders 3 / 9" badge (frame_xr/shader_notice.h says
/// when): a small pill with a progress ring (`progress` 0 to 1) just inside the main screen's
/// top-right corner, or head-locked at the top right of what is in view in the immersive modes
/// (vis/shader_badge.h). It fades in and out; "" fades it out. Any thread; VR only.
void SetShaderBadge(const std::string& text, float progress);
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
