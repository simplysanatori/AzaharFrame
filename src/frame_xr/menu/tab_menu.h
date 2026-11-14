// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: the in-game menu as one panel in the structure of CitraVR's VR menu (a
// column of tabs on the left: Main, Position, Stats, Controls, Mods, Homebrew, Settings), drawn in Material 3
// (md3.h). This file is the menu's content, layout, drawing and hit testing; it knows nothing of
// OpenXR, Qt or the settings. The host fills a MenuModel with the current values, keeps a
// MenuView (the tab, what the beam points at, scrolling, an open menu), draws with Draw and maps
// the beam's point with HitTest; what a click does is the host's (frame_xr/menu_panel.cpp).
//
// The panel is kPanelW x kPanelH dp with kPanelRadius corners.

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "frame_xr/menu/edit_devices.h"
#include "frame_xr/menu/homebrew_tab.h" // AzaharFrame emerald
#include "frame_xr/menu/list_scroll.h"  // AzaharFrame scroll
#include "frame_xr/menu/mods_tab.h" // AzaharFrame cheatstab
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/remap.h"

namespace FrameXr::Menu {

constexpr float kPanelW = 840.0f;
constexpr float kPanelH = 560.0f;
constexpr float kPanelRadius = 32.0f;
constexpr float kRowH = 72.0f; ///< a settings row; the lists scroll in steps of one
constexpr float kAutosaveCardH = 72.0f;  ///< AzaharFrame autosave: Load State's card
constexpr float kAutosaveItemH = 56.0f;  ///< and a row of its list

// AzaharFrame cheatstab: Mods; ui3: Homebrew (was a segment of Mods)
enum class Tab : int { Main, Position, Stats, Controls, Mods, Homebrew, Settings, Count };

/// Everything on the menu a beam can point at.
enum class Id : int {
    None,
    TabButton, ///< index: the Tab
    // Main
    Continue,
    QuickSave,
    QuickLoad,
    SaveState,
    LoadState,
    Screenshot,
    Restart,
    AzaharWindow,
    SwitchToFlat,
    CloseGame,
    SlotsBack, ///< the slot list's back arrow
    Slot,      ///< index: the slot (1 to 10)
    // AzaharFrame autosave: Load State's Autosave card (owner's Option B)
    AutosaveCard,     ///< Keep 1: loads it; more: opens the list of them
    AutosaveEntry,    ///< index: MenuModel::autosaves (the card's list)
    AutosaveSettings, ///< "Autosave: Off"'s Settings: the Saves rows
    UndoLoad,         ///< the snackbar's Undo Load, after an autosave was loaded
    // Position
    ResetPosition,
    SwapScreens,
    HideBottomScreen,
    ScreenLayout, ///< AzaharFrame layout
    MoveScreens,  ///< AzaharFrame menu
    ScreenShape,
    ScreenCurve,
    ImmersiveOffset,
    ImmersiveOffsetReset,
    // Stats
    ShowStats,
    StatsPosition, ///< AzaharFrame statspos
    // Controls
    ControlsPage,
    TouchpadTouch,       ///< AzaharFrame menu: the Touchpad row (Tap for Input, Click for Input)
    EditControls,        ///< AzaharFrame menu
    RemapChip,           ///< AzaharFrame remap, index: the 3DS button (DsButton) of a chip
    RemapCancel,         ///< AzaharFrame remap: stop listening
    RemapReset,          ///< AzaharFrame remap: Reset to Default
    RemapDone,           ///< AzaharFrame remap: Done
    EditDevice,          ///< AzaharFrame padsui, index: the device in MenuModel::devices
    MouseMode,           ///< AzaharFrame padsui, index: 0 Bottom Screen, 1 Pointer
    RecentreAtEdge,      ///< AzaharFrame padsui: the selected device's Recentre at Edge
    UseDevice,           ///< AzaharFrame padsui: the selected device's Use This Device
    CursorSpeed,         ///< AzaharFrame padsui: the mouse's Cursor Speed (a slider, 0 to 4)
    BottomScreenReticle, ///< AzaharFrame mouse, index: 0 Off, 1 Left, 2 Right, 3 Headset
    ReticleAtEdge,       ///< AzaharFrame mouse, index: 0 Follow, 1 Snap to Middle
    ReticleSpeed,        ///< AzaharFrame reticle: a slider, 5 to 40 (tenths of 1x)
    ReticleCenter,       ///< AzaharFrame fixes7: Aim Only While Pressed (a switch)
    ReticleStick,        ///< AzaharFrame fixes7: Right Stick Moves Touch (a switch)
    ReticleEdgeCStick,   ///< AzaharFrame kiu: C-Stick at Edge (a switch)
    // Settings
    /// AzaharFrame reticle: the scope toggle on the Settings, Controls and Position tabs, index:
    /// 0 Global, 1 Per-Game (DrawScope); pointing at Per-Game while picked shows the count.
    Scope,
    /// AzaharFrame reticle: the scope's ⋮ menu button (no index); its menu's item 0 is Use
    /// Global Values for All.
    ScopeMore,
    SettingsSection, ///< AzaharFrame autosave: General, Graphics, Immersive (index)
    /// AzaharFrame useglobal: Use Global Value for one row (index: the row's Id). AzaharFrame
    /// reticle: no longer drawn (the chips went); Use Global Values for All uses it per row.
    UseGlobal,
    SavesHeader,   ///< AzaharFrame autosave: the "Saves" subheader (no action)
    Autosave,      ///< AzaharFrame autosave: autosave_enabled, per game
    AutosaveEvery, ///< a slider of kAutosaveEvery's stops (index 0 to 4), per game
    AutosaveKeep,  ///< a slider, 1 to 10, per game
    Background,
    FaceButtons,
    MotionControls,
    GyroAim,         ///< AzaharFrame kiugyro: Aiming, index: 0 Off, 1 Direct Aim, 2 Gyro Aim
    GyroSensitivity, ///< AzaharFrame kiugyro: a slider, 2 to 40 (tenths of 1x)
    TurnZone,        ///< AzaharFrame kiugyro: a slider, 0 to 30 (percent), Direct Aim only
    TurnSpeed,       ///< AzaharFrame kiugyro: a slider, 2 to 40 (tenths of 1x), Direct Aim only
    Recoil,          ///< AzaharFrame kiugyro: a switch, while Aiming is not Off
    WeaponInHand,    ///< AzaharFrame kiuguns: a switch, while Aiming is not Off
    LaserSight,      ///< AzaharFrame kiuguns: a switch, with Direct Aim
    ShowControllers,
    ControllersInFront, ///< AzaharFrame ctrlfront: Controllers in Front of Screen, index: 0 Off,
                        ///< 1 Primary, 2 Secondary, 3 Both (vr_controllers_in_front), global
    WindowScale,
    Depth3d,            ///< AzaharFrame depth3d: Settings::values.factor_3d, per game
    InternalResolution, ///< AzaharFrame menuux: resolution_factor, per game (a menu, 1x to 8x)
    DynamicResolution,  ///< AzaharFrame menuux: vr_dynamic_resolution, global
    TextureFilter,      ///< AzaharFrame menuux: texture_filter, per game
    AsyncShaders,       ///< AzaharFrame menuux: async_shader_compilation, per game
    AccurateImageCopies, ///< AzaharFrame accxfer, xfergpu: image_copy_accuracy, per game
    FastForwardSpeed,   ///< AzaharFrame menuux: turbo_limit, global
    FastForwardAudio,   ///< AzaharFrame ffaudio: fast_forward_audio, per game (Sped Up, Normal Pitch, Muted)
    ImmersiveMode,
    WiderView,
    GameFov,
    MatchCamera, ///< AzaharFrame depth: Detect Game FOV, per game
    ScaleX, ///< AzaharFrame kiu: Horizontal Scale, a slider of -30 to +30 % (5 % steps), per game
    ScaleY, ///< AzaharFrame kiu: Vertical Scale, as ScaleX
    /// AzaharFrame fovscale: how the scales act, Stretch | Field of View
    /// (vr_immersive_scale_mode), per game
    ScaleMode,
    WorldScale, ///< AzaharFrame natural: World Scale (vr_immersive_world_scale), per game
    FollowScreen, ///< AzaharFrame natural: Follow Screen Size (vr_immersive_follow_screen), per game
    MoreSettings,
    HelpHeader,    ///< AzaharFrame crashreport: General's "Help" subheader (no action)
    ReportProblem, ///< AzaharFrame crashreport: Report a Problem (a dialog in Azahar's window)
    AboutApp,      ///< AzaharFrame crashreport: About Azahar for Frame (the same)
    // Mods (AzaharFrame cheatstab): mods_tab.h
    ModsSegment,    ///< index: ModsSegment
    DownloadCheats, ///< the Cheats segment's
    CheatsWindow,   ///< Azahar's cheat editor
    Cheat,          ///< index: MenuModel::mods.cheats.cheats (Cheats::Online::SetCheatEnabled's)
    // The Mods tab's Mods and Textures segments (AzaharFrame texui): mods_segments.h
    InstallMods,   ///< opens Install Mods
    ModsBack,      ///< Install Mods' back button
    Mod,           ///< index: mods.mods.loaded (the row: a managed mod on or off)
    ModRemove,     ///< index: mods.mods.loaded
    ModInstall,    ///< index: mods.mods.available
    DialogConfirm, ///< the dialog's Replace / Remove
    DialogCancel,  ///< and its Cancel / OK
    CustomTextures,
    TexturePack, ///< index: mods.textures.packs (its checkbox; AzaharFrame texlayers)
    TexturePreload,
    RestartGame, ///< the snackbar's Restart
    ModNote,     ///< AzaharFrame modcat2: an Install Mods row's name (tooltip: the entry's notes);
                 ///< index: mods.mods.available
    PackNote,    ///< AzaharFrame modcat2: a Download Packs row's name (tooltip: the notes); index:
                 ///< mods.textures.downloads
    ModVersion,  ///< AzaharFrame modcat: a mod's version chip (the tooltip's anchor); index:
                 ///< mods.mods.available
    ModLink,     ///< "Created by <author>": index into the page's list (loaded or available)
    DownloadPacks, ///< the Textures segment: opens Download Packs
    TexturesBack,  ///< Download Packs' back button
    PackDownload,  ///< index: mods.textures.downloads (Download / Try Again)
    PackCancel,    ///< index: mods.textures.downloads
    PackLink,      ///< index: mods.textures.downloads ("Created by <author>")
    PackDelete,    ///< index: mods.textures.packs (a downloaded pack)
    // AzaharFrame zipinstall: Install from Zip (mods_segments.h ZipModel)
    InstallFromZip, ///< index: ZipTarget (on Download Packs and Install Mods)
    ZipBack,        ///< the page's back button
    ZipChip,        ///< index: mods.zip.chips
    ZipUp,          ///< the ".." row
    ZipEntry,       ///< index: mods.zip.entries
    ZipChoice,      ///< index: mods.zip.choices
    // The Homebrew tab (AzaharFrame emerald; ui3: its own tab): homebrew_tab.h, index:
    // homebrew.items
    HomebrewInstall,
    HomebrewUpdate,
    HomebrewPlay,
    // AzaharFrame ui2: the Cheats segment's own cheats (cheat_entry.h)
    AddCheat,         ///< a name and a code on the game's keyboard
    InstallCheatFile, ///< Install from Zip's browser, for a cheat file (.txt or a zip of one)
    ModCancel,        ///< AzaharFrame integ3: Cancel while a catalog mod downloads; index:
                      ///< mods.mods.available
    // AzaharFrame compat: entries for another version (mods_segments.h ModOffer::mismatch)
    ModCompat,  ///< a mod's "Not for Your Version" and region chips (the tooltip's anchor); index:
                ///< mods.mods.available
    PackCompat, ///< the same on Download Packs; index: mods.textures.downloads
    TurnBackOn, ///< the "Turned Off" snackbar's action
    // AzaharFrame texparts: a pack's Customize page (mods_segments.h TexturePartsModel)
    PackCustomize, ///< index: mods.textures.packs
    PartsBack,     ///< the page's back button
    PartsAllOn,
    PartsAllOff,
    TexturePart,       ///< index: mods.textures.parts.rows (a checkbox or a radio button)
    PartUp,            ///< index: mods.textures.parts.rows
    PartDown,          ///< index: mods.textures.parts.rows
    // AzaharFrame texlayers: Enabled Textures (mods_segments.h TexturesModel::layers)
    TextureLayer, ///< index: mods.textures.layers (its checkbox)
    LayerUp,      ///< index: mods.textures.layers
    LayerDown,    ///< index: mods.textures.layers
    HomebrewReinstall, ///< AzaharFrame ui3: installed and no newer release seen; index:
                       ///< homebrew.items
    HomebrewCheck,     ///< AzaharFrame ui3: Check for Updates; index: homebrew.items
    // AzaharFrame emhb: Uninstall and its dialog (homebrew_tab.h HomebrewDialog)
    HomebrewUninstall,     ///< index: homebrew.items
    HomebrewDialogCheck,   ///< Also Delete Save Data
    HomebrewDialogConfirm, ///< Uninstall (or OK)
    HomebrewDialogCancel,
    // AzaharFrame ui3: Optimize for Frame on a pack's page (mods_segments.h PackOptimize)
    OptimizePack,   ///< Optimize for Frame (the estimate, then the dialog)
    OptimizeStop,   ///< Stop while it converts
    UseOriginal,    ///< back to the PNG pack (from the next start)
    OptimizeNudge,  ///< the "Optimize <Pack> for Frame?" snackbar's Optimize: the pack's page
    RemoveOriginal, ///< AzaharFrame ui3: the optimized pack's original, deleted after a dialog
    DialogExtra,    ///< AzaharFrame memwarn: a notice's third button (ModsDialog::extra), first
    Scrollbar,      ///< AzaharFrame scroll: a long list's scrollbar (MenuLayout::bar)
    CheatsOtherVersion, ///< AzaharFrame scroll: the Cheats list's "For Another Version" section
    // AzaharFrame ctgp7: the Mods tab's CTGP-7 segment (ctgp7_tab.h)
    Ctgp7Install,
    Ctgp7Launch,
    Ctgp7Cancel,
    Ctgp7Website, ///< Official Website ("Open ctgp-7.github.io?" first)
    Ctgp7Uninstall, ///< "Uninstall CTGP-7?" (the Qt side asks)
    DialogCheck,    ///< a dialog's checkbox (ModsDialog::check; MenuView::dialog_check)
    Count
};
constexpr int kIdCount = static_cast<int>(Id::Count);

/// A row's label (no action; the tooltip's anchor).
constexpr int kLabelIndex = -3;
/// An open menu's own area (no action, but nothing under it is hit).
constexpr int kMenuAreaIndex = -2;

/// What the beam points at: an item and, for segmented buttons, menus and slots, the option.
struct Target {
    Id id = Id::None;
    int index = -1;
    bool operator==(const Target&) const = default;
};

/// Why an item cannot change now (its tooltip says so).
enum class Lock : uint8_t {
    None,
    GameValue,   ///< the game has its own value, while Global is chosen
    FullOnly,    ///< only in the Full immersive modes (TabMenu finds these itself)
    Unavailable, ///< the runtime does not offer it
    AtLowest,    ///< AzaharFrame menuux: Dynamic Resolution at 1x Internal Resolution
    InPassthrough, ///< AzaharFrame vis: Show Controllers while the Background is Passthrough
    AutosaveOff,   ///< AzaharFrame autosave: Every and Keep while Autosave is off
    NoGameValues,  ///< AzaharFrame reticle: the scope's ⋮ while nothing on the tab is per game
    NoGame,        ///< AzaharFrame reticle: the scope with no game that can have its own values
};

struct SaveSlot {
    int slot = 0;
    std::string time; ///< "" = empty
    bool other = false; ///< AzaharFrame savestate: from another build ("From 1.1.1"): dimmed
    bool operator==(const SaveSlot&) const = default;
};

/// AzaharFrame autosave: Every's stops, in minutes (core/autosave.h kEveryMinutes).
constexpr std::array<int, 5> kAutosaveEvery{1, 2, 5, 10, 15};

/// AzaharFrame autosave: an autosave's top screen (straight RGBA, rows top down).
struct AutosavePicture {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
};
/// AzaharFrame autosave: one autosave on Load State's card and its list, newest first.
struct AutosaveCard {
    std::string ago;   ///< "Just Now", "3 Minutes Ago", ... (core/autosave.h Ago)
    std::string time;  ///< "2026-11-12 10:14", or "From 1.1.1" when `other`
    std::string clock; ///< "10:14" (the list's right column), or "From 1.1.1"
    bool other = false; ///< made by another build: dimmed, still loads (after Load Anyway)
    std::shared_ptr<const AutosavePicture> picture; ///< null: an icon
    std::string id; ///< AzaharFrame advfix: Autosave::EntryId, sent back when it is tapped
    bool operator==(const AutosaveCard&) const = default;
};

/// The values the menu shows (the host's, read from the settings every frame).
/// AzaharFrame kiugyro: Aiming's choices are shown Off, Direct Aim, Gyro Aim, but stored as
/// vr_gyro_aim has always been (0 Off, 1 Gyro, 2 Light Gun), so older builds read the same value:
/// the two middle ones trade places between the stored value and the shown index.
constexpr int AimingIndexOf(int stored) {
    return stored == 1 ? 2 : (stored == 2 ? 1 : 0);
}
constexpr int AimingValueAt(int index) {
    return index == 1 ? 2 : (index == 2 ? 1 : 0);
}

struct MenuModel {
    std::string game_title;
    bool quick_load_available = true;
    std::vector<SaveSlot> slots; ///< slots 1 to 10
    ModsDialog notice; ///< AzaharFrame savestate: a dialog over the whole menu (ModsDialogKind::Notice)
    // Position
    bool hide_bottom_screen = false;
    int screen_shape = 0;  ///< Settings::values.vr_top_aspect
    int screen_layout = 0; ///< 0 Stacked, 1 Side by Side (AzaharFrame layout)
    int move_screens = 0;  ///< 0 Off, 1 Secondary, 2 Both (AzaharFrame menu)
    int screen_curve = 0;  ///< index of Off, 15, 30, 45, 60, 75, 90 degrees
    std::string immersive_offset = "None";
    bool immersive_offset_set = false;
    // Stats
    bool show_stats = false;
    int stats_position = 0; ///< 0 In Front, 1 On Wrist (AzaharFrame statspos)
    std::vector<std::pair<std::string, std::string>> stats_game, stats_vr, stats_system;
    std::string version;
    // Controls
    int controls_page = 0; ///< 0 In Game, 1 Azahar's Window
    int touchpad_touch = 0; ///< AzaharFrame menuux: 0 Tap for Input, 1 Click for Input
    int bottom_screen_reticle = 0; ///< AzaharFrame mouse: 0 Off, 1 Left, 2 Right, 3 Headset
    int reticle_at_edge = 0;       ///< AzaharFrame mouse: 0 Follow, 1 Snap to Middle
    int reticle_speed = 20;        ///< AzaharFrame reticle: tenths of 1x (5 to 40)
    bool reticle_center = false;   ///< AzaharFrame fixes7: Aim Only While Pressed
    bool reticle_stick = false;    ///< AzaharFrame fixes7: Right Stick Moves Touch
    bool reticle_edge_cstick = false; ///< AzaharFrame kiu: C-Stick at Edge
    EditDevices devices;    ///< AzaharFrame padsui: Edit Controls' devices
    // Settings
    int scope = 0; ///< 0 Global, 1 Per-Game (AzaharFrame autosave: the menu opens on Per-Game)
    int background = 0;
    int face_buttons = 0;
    int motion_controls = 1;
    int gyro_aim = 0;          ///< AzaharFrame kiugyro: the stored value: 0 Off, 1 Gyro, 2 Light Gun
    int gyro_sensitivity = 10; ///< tenths of 1x (2 to 40)
    int turn_zone = 10;        ///< percent (0 to 30)
    int turn_speed = 10;       ///< tenths of 1x (2 to 40)
    bool recoil = true;        ///< AzaharFrame kiugyro: vr_light_gun_recoil
    bool weapon_in_hand = true; ///< AzaharFrame kiuguns: vr_light_gun_weapon
    bool laser_sight = false;   ///< AzaharFrame kiuguns: vr_light_gun_laser
    bool show_controllers = true;
    int controllers_in_front = 3; ///< AzaharFrame ctrlfront: 0 Off, 1 Primary, 2 Secondary, 3 Both
    int window_scale = 1; ///< 100, 150, 200 %
    int depth_3d = 50;    ///< AzaharFrame depth3d: 0 to 100 %
    int internal_resolution = 1; ///< AzaharFrame menuux: resolution_factor (0 Auto, 1 to 18)
    bool dynamic_resolution = false;
    int texture_filter = 0;      ///< index into OptionsFor(Id::TextureFilter)
    bool async_shaders = false;
    int accurate_image_copies = 1; ///< AzaharFrame accxfer, xfergpu: 0 Off, 1 Automatic, 2 On
    int fast_forward_speed = 1;  ///< index into OptionsFor(Id::FastForwardSpeed)
    int fast_forward_audio = 1;  ///< Settings::FastForwardAudio, an index of OptionsFor(Id::FastForwardAudio)
    int immersive_mode = 0;
    int wider_view = 0;  ///< index of Off, 1.1x, 1.2x, 1.3x, 1.4x
    int game_fov = 60;   ///< 30 to 110 degrees
    int match_camera = 2; ///< AzaharFrame depth, fovonce: 0 Off, 1 Set Once, 2 Always
    int scale_x = 0; ///< AzaharFrame kiu: percent, -30 to 30
    int scale_y = 0;
    int scale_mode = 1; ///< AzaharFrame fovscale: 0 Stretch, 1 Field of View
    /// AzaharFrame natural: World Scale's slider stop (Layout::kWorldScaleStops; 20 = 100 %,
    /// Automatic) and Follow Screen Size.
    int world_scale_stop = 20;
    bool follow_screen = true;
    // AzaharFrame autosave: the running game's values (Per-Game, else Global)
    bool autosave = false;
    int autosave_every = 2; ///< index into kAutosaveEvery
    int autosave_keep = 3;
    std::vector<AutosaveCard> autosaves; ///< newest first
    std::string undo_load; ///< "Autosave Loaded: 10:12" until Undo Load or the game stops ("" none)
    ModsTabModel mods; ///< AzaharFrame cheatstab: the Mods tab's segments
    HomebrewTabModel homebrew; ///< AzaharFrame emerald
    /// Items that cannot change now, and why.
    std::array<Lock, kIdCount> locked{};
    /// AzaharFrame useglobal: per-game rows the running game has its own value for (the scope
    /// control's Per-Game count and Use Global Values for All; AzaharFrame reticle).
    std::array<bool, kIdCount> game_value{};
    bool operator==(const MenuModel&) const = default;
};

enum class Page { Tabs, SaveSlots, LoadSlots };

/// AzaharFrame autosave: the Settings tab's sections (owner decision, round 2).
enum class SettingsSection : int { General, Graphics, Immersive, Count };
/// The scope the menu opens on: Per-Game (1) whenever the game can have settings of its own.
constexpr int OpeningScope(bool per_game_ok) {
    return per_game_ok ? 1 : 0;
}
/// The section a Settings row is in (General for the rest).
SettingsSection SectionOf(Id id);
/// Per-game settings (Azahar's Properties have most of them too): a change in Per-Game is the
/// game's own value; in Global they are locked while the game has one. AzaharFrame reticle: the
/// Controls tab's rows too (ControlsScopeRows).
bool PerGameSetting(Id id);

// ---- AzaharFrame reticle: the scope control (owner's design, round 24) ----------------------
// The Settings, Controls and Position tabs show the same control at their top right: a
// Global | Per-Game toggle (MenuModel::scope, one for the whole menu), a ⋮ menu button whose
// one item is Use Global Values for All, and under the toggle, in Per-Game only, a quiet line
// counting the values this tab has for the game ("2 Per-Game Values"). With no game that can
// have settings of its own the toggle is Global alone. Drawn by TabMenu::DrawScope only.

/// Whether a tab shows the scope control.
constexpr bool TabHasScope(Tab tab) {
    return tab == Tab::Settings || tab == Tab::Controls || tab == Tab::Position;
}
/// The Controls tab's per-game rows (in the order they show; At Edge and Reticle Speed only show
/// while the reticle is on, but are counted and cleared all the same).
const std::vector<Id>& ControlsScopeRows();
/// The per-game rows of a tab (the Settings tab: of `section`) the running game has its own
/// value for, in order: what the Per-Game tooltip counts and Use Global Values for All clears.
std::vector<Id> GameValueRows(const MenuModel& model, Tab tab,
                              SettingsSection section = SettingsSection::General);
/// The Per-Game tooltip: "No Per-Game Values Set", "1 Per-Game Value Set", "3 Per-Game Values Set".
std::string GameValueCount(std::size_t n);

/// What the menu looks like now (the host's, changed by the beam and the sticks).
struct MenuView {
    Tab tab = Tab::Main;
    Page page = Page::Tabs; ///< the Main tab's slot lists
    SettingsSection section = SettingsSection::General; ///< AzaharFrame autosave
    Target hover;
    Target pressed;
    std::array<float, static_cast<int>(Tab::Count)> scroll{}; ///< dp scrolled down
    Id open_menu = Id::None;  ///< a menu button whose menu is open
    Id tooltip = Id::None;    ///< the one-line explanation of a row
    int tooltip_index = -1;   ///< AzaharFrame modcat: which row's (ModVersion, ModNote, PackNote, ModCompat, PackCompat)
    Id dragging = Id::None;   ///< a slider held: drawn at drag_t
    float drag_t = 0.0f;
    ModsSegment mods_segment = ModsSegment::Cheats; ///< AzaharFrame cheatstab
    ModsPage mods_page = ModsPage::Loaded;          ///< AzaharFrame texui: the Mods segment's page
    TexturesPage textures_page = TexturesPage::Packs; ///< AzaharFrame texui
    std::string open_link; ///< AzaharFrame texui: the link "Open GitHub?" asks about ("" = none)
    /// AzaharFrame scroll: the Cheats list's "For Another Version" section is open (per game,
    /// for the session: MenuPanel)
    bool cheats_other_open = false;
    RemapView remap; ///< AzaharFrame remap: Edit Controls
    /// AzaharFrame compat: "This May Not Work" asked before ModInstall, PackDownload or turning a
    /// Cheat on (`index` the row, `key` its id or name, checked again on the answer).
    struct Confirm {
        Id id = Id::None;
        int index = -1;
        std::string key;
        bool operator==(const Confirm&) const = default;
    } confirm;
    /// AzaharFrame ctgp7: "Requires CTGP-7" shows (an Install Mods row of a CTGP-7 mod was pressed)
    bool requires_ctgp7 = false;
    /// AzaharFrame ctgp7: the open dialog's checkbox (ModsDialog::check) is ticked; off for each new one
    bool dialog_check = false;
    bool operator==(const MenuView&) const = default;
};

/// AzaharFrame menuux: Internal Resolution's choices are 1x to kMaxResolution (the XR screens
/// stop at 8x: renderer_vulkan_xr.cpp); Texture Filter's are these Settings::TextureFilter values
/// (Anime4K is OpenGL only); Fast Forward Speed's these turbo_limit percentages.
constexpr int kMaxResolution = 8;
constexpr std::array<int, 5> kTextureFilters{0, 2, 3, 4, 5};
constexpr std::array<int, 4> kFastForwardSpeeds{150, 200, 300, 400};
/// AzaharFrame menuux: the line under the Touchpad row for its value (0 Tap, 1 Click).
std::string TouchpadSupporting(int value);

/// AzaharFrame autosave: the "Autosaved" label over the top screen's corner (hud.cpp draws it into
/// the toast atlas at its levels of opacity; the menu dump draws it too): a dark pill, the save
/// icon and the word, in `r` (5:1).
void DrawAutosavedLabel(Kit& kit, const Rect& r);

/// The option lists (Azahar for Frame's own strings).
const std::vector<std::string>& OptionsFor(Id id);
/// The label of an item, as on the menu.
std::string LabelFor(Id id);
/// One short sentence about a setting (the tooltip), or "".
std::string ExplainFor(Id id);
/// AzaharFrame xfergpu: a segmented row's tooltip for one of its choices (the beam resting on
/// that segment), or "" when the row has none; its label keeps ExplainFor's.
std::string ExplainOption(Id id, int index);
/// AzaharFrame xfergpu: whether a segmented row has tooltips for its choices (ExplainOption).
bool HasOptionTips(Id id);
/// The tooltip of a locked item.
std::string ExplainLock(Lock lock);
/// A slider's range and step: min, max, step.
struct SliderRange {
    float min = 0.0f, max = 1.0f, step = 1.0f;
};
SliderRange SliderFor(Id id);

/// Where things are on the menu as last drawn: what the host hit tests against (a copy of it can
/// go to another thread with the pixels).
struct MenuLayout {
    struct Hit {
        Rect r;
        Target t;
    };
    struct SliderTrack {
        Id id;
        float x0, x1;
    };
    std::vector<Hit> hits;
    std::vector<SliderTrack> sliders;
    float max_scroll = 0.0f; ///< how far the current tab can scroll (dp)
    /// AzaharFrame cheatstab: one row of the scrolling list (dp): what a stick's tap moves.
    float scroll_step = kRowH;
    /// AzaharFrame scroll: the list's scrollbar as drawn (Valid only while the list overflows),
    /// and whether a dialog over the list holds it still (its scroll is kept for after).
    Scrolling::Bar bar;
    bool scroll_locked = false;

    /// What is at (x, y) dp.
    Target HitTest(float x, float y) const;
    /// A slider's value (0 to 1) at x dp (-1: no such slider).
    float SliderAt(Id id, float x) const;
};

class TabMenu {
public:
    /// Draws the controller diagram of `page` into `area` (dp) on the Controls tab (Edit
    /// Controls draws the 3DS itself, ds_drawing.h).
    using DiagramDraw = std::function<void(Kit& kit, const Rect& area, int page)>;

    void SetDiagram(DiagramDraw draw) {
        diagram = std::move(draw);
    }

    /// The whole panel, kPanelW x kPanelH dp, from the canvas's top left.
    void Draw(Kit& kit, const MenuModel& model, const MenuView& view);
    const MenuLayout& Layout() const {
        return layout;
    }
    Target HitTest(float x, float y) const {
        return layout.HitTest(x, y);
    }
    float MaxScroll() const {
        return layout.max_scroll;
    }
    float SliderAt(Id id, float x) const {
        return layout.SliderAt(id, x);
    }
    const std::vector<MenuLayout::Hit>& Hits() const {
        return layout.hits;
    }

private:
    void DrawRail(Kit& kit, const MenuView& view);
    void DrawMain(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawSlots(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    /// AzaharFrame autosave: Load State's Autosave card, and its list when open.
    void DrawAutosaveCard(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& r,
                          const Rect& pane);
    void DrawAutosaveList(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& card);
    void DrawStats(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawEditControls(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawControls(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    /// AzaharFrame cheatstab: the Mods tab (mods_tab.cpp) and its segments, each drawn in `area`
    /// (the pane's width, under the segmented button): cheats_tab.cpp, mods_segments.cpp.
    void DrawMods(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawCheats(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area);
    void DrawModsList(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area);
    void DrawTextures(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area);
    /// The snackbar "Restart the Game to Apply" at the bottom of `area`; returns where a list
    /// over it must end.
    float DrawRestartSnackbar(Kit& kit, const MenuView& view, const Rect& area,
                              const std::string& text = {}); // AzaharFrame catalog3: "" = Restart the Game to Apply
    /// AzaharFrame compat: "Turned Off <names>: ..." with Turn Back On, ending at `bottom`;
    /// returns where a list over it must end.
    float DrawTurnedOffSnackbar(Kit& kit, const MenuView& view, const Rect& area,
                                const std::vector<std::string>& names, float bottom);
    void DrawModsDialog(Kit& kit, const ModsDialog& dialog, const MenuView& view);
    /// AzaharFrame ui3: a snackbar with one action ending at `bottom` (one or two lines); returns
    /// where a list over it must end.
    float DrawActionSnackbar(Kit& kit, const MenuView& view, const Rect& area,
                             const std::string& text, Id action, float bottom);
    /// AzaharFrame ui3: Optimize for Frame's row on a pack's page, from `y`; returns its bottom.
    float DrawOptimizeRow(Kit& kit, const PackOptimize& o, const MenuView& view, const Rect& area,
                          float x0, float x1, float y);
    /// Download Packs, the Textures segment's second page; the list ends at `bottom`.
    void DrawPackDownloads(Kit& kit, const TexturesModel& m, const MenuView& view,
                           const Rect& area, float bottom);
    /// AzaharFrame texparts: a pack's Customize page (mods_segments.cpp); the list ends at `bottom`.
    void DrawTextureParts(Kit& kit, const TexturePartsModel& m, const MenuView& view,
                          const Rect& area, float bottom);
    /// AzaharFrame zipinstall: Install from Zip's page (mods_segments.cpp); the list ends at `bottom`.
    void DrawZip(Kit& kit, const ZipModel& m, const MenuView& view, const Rect& area, float bottom);
    /// AzaharFrame ctgp7: the CTGP-7 segment (ctgp7_tab.cpp)
    void DrawCtgp7(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area);
    /// AzaharFrame ctgp7b: rows of catalog mods (`rows`: indices of model.mods.mods.available) in
    /// `list`, `c` the content's edges (mods_segments.cpp): Install Mods' list and the CTGP-7
    /// segment's Mods for CTGP-7. Sets the list's scroll.
    void DrawOfferList(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area,
                       const Rect& c, const Rect& list, const std::vector<int>& rows,
                       const OfferListOptions& opt);
    /// AzaharFrame ctgp7b: how tall DrawOfferList's rows are, all of them
    float OfferListHeight(const Kit& kit, const ModsListModel& m, const Rect& c,
                          const std::vector<int>& rows, const OfferListOptions& opt) const;
    void DrawHomebrew(Kit& kit, const MenuModel& model, const MenuView& view,
                      const Rect& pane); ///< AzaharFrame emerald: homebrew_tab.cpp; ui3: the Homebrew tab
    void DrawHomebrewDialog(Kit& kit, const HomebrewDialog& dialog,
                            const MenuView& view); ///< AzaharFrame emhb: homebrew_tab.cpp
    /// AzaharFrame ui2: Install from Zip at a download page's bar end.
    void DrawZipButton(Kit& kit, const MenuView& view, const Rect& area, ZipTarget target,
                       bool enabled);
    void DrawRows(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    /// AzaharFrame reticle: the scope control at the top right of `c` (the pane's content);
    /// returns its left edge. DrawScopeMenu draws its ⋮ menu when open (last, over the rest).
    float DrawScope(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& c,
                    const Rect& pane);
    void DrawScopeMenu(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawTooltip(Kit& kit, const MenuView& view, const Rect& pane);
    /// AzaharFrame scroll: the scrollbar of the list in `list` (hanging off `right`), only when
    /// it overflows: slim at rest, wider with a beam on it, the primary colour while held. Its
    /// hit strip goes in as Id::Scrollbar and the bar in layout.bar.
    void DrawScrollbar(Kit& kit, const MenuView& view, const Rect& list, float right,
                       float content_h, float max_scroll, float scroll);
    State StateOf(const MenuView& view, Target t, bool enabled = true) const;
    void AddHit(const Rect& r, Target t, const Rect& clip);

    DiagramDraw diagram;
    MenuLayout layout;
    Rect scope_more_; ///< AzaharFrame reticle: the ⋮ button as last drawn (its menu's anchor)
    /// Where each row's label is (the tooltip goes over it).
    struct Anchor {
        Id id;
        float x, y;
        Lock lock;
        int index = -1;   ///< AzaharFrame useglobal: UseGlobal's row
        float cx = -1.0f; ///< the tooltip's centre when not over the label
        std::string text; ///< AzaharFrame reticle: the tooltip's own text (else the row's)
        bool below = false; ///< AzaharFrame reticle: under `y` (the scope toggle's bottom)
    };
    std::vector<Anchor> anchors;
};

/// AzaharFrame compat: "This May Not Work" for the row `view.confirm` names (kind None when the
/// row is gone, changed, or fits after all). The body comes from the row (ModOffer, PackOffer) or
/// the cheat's version note (CheatMismatchBody).
ModsDialog CompatConfirmDialog(const MenuModel& model, const MenuView& view);
/// AzaharFrame compat: what answering it allows (kAnywayOtherVersion | kAnywayUnscanned), 0 none.
int CompatConfirmFlags(const MenuModel& model, const MenuView& view);
/// AzaharFrame ctgp7b: the Mods list's question as drawn: the Qt side's, "This May Not Work", or
/// "Open <host>?" for a link (kind None: none). The CTGP-7 segment shows it under its own.
ModsDialog ModsDialogFor(const MenuModel& model, const MenuView& view);

} // namespace FrameXr::Menu
