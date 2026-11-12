// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: the in-game menu as one panel in the structure of CitraVR's VR menu (a
// column of tabs on the left: Main, Position, Stats, Controls, Mods, Settings), drawn in Material 3
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
#include <string>
#include <utility>
#include <vector>

#include "frame_xr/menu/mods_tab.h" // AzaharFrame cheatstab
#include "frame_xr/menu/md3.h"

namespace FrameXr::Menu {

constexpr float kPanelW = 840.0f;
constexpr float kPanelH = 560.0f;
constexpr float kPanelRadius = 32.0f;
constexpr float kRowH = 72.0f; ///< a settings row; the lists scroll in steps of one

enum class Tab : int { Main, Position, Stats, Controls, Mods, Settings, Count }; // AzaharFrame cheatstab: Mods

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
    TouchpadTouch, ///< AzaharFrame menu
    EditControls,  ///< AzaharFrame menu
    // Settings
    Scope,
    Background,
    FaceButtons,
    MotionControls,
    ShowControllers,
    WindowScale,
    Depth3d, ///< AzaharFrame depth3d: Settings::values.factor_3d, per game
    ImmersiveMode,
    WiderView,
    GameFov,
    NarrowScreen,
    DepthFactor,
    MoreSettings,
    // Mods (AzaharFrame cheatstab): mods_tab.h
    ModsSegment,    ///< index: ModsSegment
    DownloadCheats, ///< the Cheats segment's
    CheatsWindow,   ///< Azahar's cheat editor
    Cheat,          ///< index: MenuModel::mods.cheats.cheats (Cheats::Online::SetCheatEnabled's)
    // The Mods tab's Mods and Textures segments (AzaharFrame texui): mods_segments.h
    InstallMods,    ///< opens Install Mods
    ModsBack,       ///< Install Mods' back button
    Mod,            ///< index: mods.mods.loaded (the row: a managed mod on or off)
    ModRemove,      ///< index: mods.mods.loaded
    ModInstall,     ///< index: mods.mods.available
    DialogConfirm,  ///< the dialog's Replace / Remove
    DialogCancel,   ///< and its Cancel / OK
    CustomTextures,
    TexturePack,    ///< index: 0 Off, i + 1 mods.textures.packs[i]
    TexturePreload,
    RestartGame,    ///< the snackbar's Restart
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
};

struct SaveSlot {
    int slot = 0;
    std::string time; ///< "" = empty
    bool operator==(const SaveSlot&) const = default;
};

/// The values the menu shows (the host's, read from the settings every frame).
struct MenuModel {
    std::string game_title;
    bool quick_load_available = true;
    std::vector<SaveSlot> slots; ///< slots 1 to 10
    // Position
    bool hide_bottom_screen = false;
    int screen_shape = 0;  ///< Settings::values.vr_top_aspect
    int screen_layout = 0; ///< 0 Stacked, 1 Side by Side (AzaharFrame layout)
    int move_screens = 0;  ///< 0 Together, 1 Separately (AzaharFrame menu)
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
    int touchpad_touch = 0; ///< 0 Off, 1 Touch, 2 Click (AzaharFrame menu)
    // Settings
    int scope = 0; ///< 0 Global, 1 Per-Game
    int background = 0;
    int face_buttons = 0;
    int motion_controls = 1;
    bool show_controllers = true;
    int window_scale = 1; ///< 100, 150, 200 %
    int depth_3d = 50;    ///< AzaharFrame depth3d: 0 to 100 %
    int immersive_mode = 0;
    int wider_view = 0;  ///< index of Off, 1.1x, 1.2x, 1.3x, 1.4x
    int game_fov = 60;   ///< 30 to 110 degrees
    int narrow_screen = 0;
    int depth_factor = 0;
    ModsTabModel mods; ///< AzaharFrame cheatstab: the Mods tab's segments
    /// Items that cannot change now, and why.
    std::array<Lock, kIdCount> locked{};
    bool operator==(const MenuModel&) const = default;
};

enum class Page { Tabs, SaveSlots, LoadSlots };

/// What the menu looks like now (the host's, changed by the beam and the sticks).
struct MenuView {
    Tab tab = Tab::Main;
    Page page = Page::Tabs; ///< the Main tab's slot lists
    Target hover;
    Target pressed;
    std::array<float, static_cast<int>(Tab::Count)> scroll{}; ///< dp scrolled down
    Id open_menu = Id::None;  ///< a menu button whose menu is open
    Id tooltip = Id::None;    ///< the one-line explanation of a row
    Id dragging = Id::None;   ///< a slider held: drawn at drag_t
    float drag_t = 0.0f;
    ModsSegment mods_segment = ModsSegment::Cheats; ///< AzaharFrame cheatstab
    ModsPage mods_page = ModsPage::Loaded;          ///< AzaharFrame texui: the Mods segment's page
    bool operator==(const MenuView&) const = default;
};

/// The option lists (Azahar for Frame's own strings).
const std::vector<std::string>& OptionsFor(Id id);
/// The label of an item, as on the menu.
std::string LabelFor(Id id);
/// One short sentence about a setting (the tooltip), or "".
std::string ExplainFor(Id id);
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
    /// AzaharFrame cheatstab: one row of the scrolling list (dp): a stick step, and where a beam
    /// drag settles. A multiple of it is max_scroll.
    float scroll_step = kRowH;

    /// What is at (x, y) dp.
    Target HitTest(float x, float y) const;
    /// A slider's value (0 to 1) at x dp (-1: no such slider).
    float SliderAt(Id id, float x) const;
};

class TabMenu {
public:
    /// Draws the controller diagram of `page` into `area` (dp) on the Controls tab.
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
    void DrawStats(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawControls(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    /// AzaharFrame cheatstab: the Mods tab (mods_tab.cpp) and its segments, each drawn in `area`
    /// (the pane's width, under the segmented button): cheats_tab.cpp, mods_segments.cpp.
    void DrawMods(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawCheats(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area);
    void DrawModsList(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area);
    void DrawTextures(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& area);
    /// The snackbar "Restart the Game to Apply" at the bottom of `area`; returns where a list
    /// over it must end.
    float DrawRestartSnackbar(Kit& kit, const MenuView& view, const Rect& area);
    void DrawModsDialog(Kit& kit, const ModsDialog& dialog, const MenuView& view);
    void DrawRows(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawTooltip(Kit& kit, const MenuView& view, const Rect& pane);
    State StateOf(const MenuView& view, Target t, bool enabled = true) const;
    void AddHit(const Rect& r, Target t, const Rect& clip);

    DiagramDraw diagram;
    MenuLayout layout;
    /// Where each row's label is (the tooltip goes over it).
    struct Anchor {
        Id id;
        float x, y;
        Lock lock;
    };
    std::vector<Anchor> anchors;
};

} // namespace FrameXr::Menu
