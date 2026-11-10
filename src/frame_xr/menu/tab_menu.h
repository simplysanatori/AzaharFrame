// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: the in-game menu as one panel in the structure of CitraVR's VR menu (a
// column of tabs on the left: Main, Position, Stats, Controls, Settings), drawn in Material 3
// (md3.h). This file is the menu's content, layout, drawing and hit testing; it knows nothing of
// OpenXR or Qt. The host fills a MenuModel with the current values, keeps a MenuView (the tab,
// what the beam points at, scrolling, an open menu), draws with Draw and maps the beam's point
// with HitTest; what a click does is the host's (an Id and an option index).
//
// The panel is kPanelW x kPanelH dp with kPanelRadius corners.

#pragma once

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "frame_xr/menu/md3.h"

namespace FrameXr::Menu {

constexpr float kPanelW = 840.0f;
constexpr float kPanelH = 560.0f;
constexpr float kPanelRadius = 32.0f;

enum class Tab : int { Main, Position, Stats, Controls, Settings, Count };

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
    CloseGame,
    // Position
    ResetPosition,
    SwapScreens,
    HideBottomScreen,
    ScreenShape,
    ScreenCurve,
    ImmersiveOffset,
    ImmersiveOffsetReset,
    // Stats
    ShowStats,
    // Controls
    ControlsPage,
    // Settings
    Scope,
    Background,
    FaceButtons,
    MotionControls,
    ShowControllers,
    WindowScale,
    ImmersiveMode,
    WiderView,
    GameFov,
    SqueezeWidth,
    DepthFactor,
    MoreSettings,
    Count
};

/// What the beam points at: an item and, for segmented buttons and menus, the option.
struct Target {
    Id id = Id::None;
    int index = -1;
    bool operator==(const Target&) const = default;
};

/// The values the menu shows (the host's, read when the menu opens and after each change).
struct MenuModel {
    std::string game_title;
    bool quick_load_available = true;
    // Position
    bool hide_bottom_screen = false;
    int screen_shape = 0;  ///< Settings::values.vr_top_aspect
    int screen_curve = 0;  ///< lane/curve: index of Off, Gentle, Medium, Strong, Max
    std::string immersive_offset = "None";
    bool immersive_offset_set = false;
    // Stats
    bool show_stats = false;
    std::vector<std::pair<std::string, std::string>> stats_game, stats_vr, stats_system;
    std::string version;
    // Controls
    int controls_page = 0; ///< 0 In Game, 1 Azahar's Window
    // Settings
    int scope = 0; ///< 0 Global, 1 Per-Game (owner decision: may go)
    int background = 0;
    bool passthrough_supported = true;
    int face_buttons = 0;
    int motion_controls = 1;
    bool show_controllers = true;
    int window_scale = 1; ///< 100, 150, 200 %
    int immersive_mode = 0;
    float wider_view = 1.0f; ///< lane/curve: 1.0 to 1.4
    int game_fov = 60;       ///< 30 to 110 degrees
    int squeeze_width = 0;
    int depth_factor = 0;
};

/// What the menu looks like now (the host's, changed by the beam and the sticks).
struct MenuView {
    Tab tab = Tab::Main;
    Target hover;
    Target pressed;
    std::array<float, static_cast<int>(Tab::Count)> scroll{}; ///< dp scrolled down
    Id open_menu = Id::None;  ///< a menu button whose menu is open
    Id tooltip = Id::None;    ///< owner option: the one-line explanation of a row
};

/// The option lists (Azahar for Frame's own strings).
const std::vector<std::string>& OptionsFor(Id id);
/// The row label of an item, as on the menu.
std::string LabelFor(Id id);
/// Owner option: one short sentence about a setting (tooltip), or "".
std::string ExplainFor(Id id);

class TabMenu {
public:
    /// Draws the controller diagram of `page` into `area` (dp) on the Controls tab.
    using DiagramDraw = std::function<void(Kit& kit, const Rect& area, int page)>;

    void SetDiagram(DiagramDraw draw) {
        diagram = std::move(draw);
    }

    /// The whole panel, kPanelW x kPanelH dp, from the canvas's top left.
    void Draw(Kit& kit, const MenuModel& model, const MenuView& view);
    /// What is at (x, y) dp, as last drawn.
    Target HitTest(float x, float y) const;
    /// How far the current tab can scroll (dp), as last drawn.
    float MaxScroll() const {
        return max_scroll;
    }

private:
    struct Hit {
        Rect r;
        Target t;
    };
    void DrawRail(Kit& kit, const MenuView& view);
    void DrawMain(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawStats(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawControls(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    void DrawRows(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane);
    State StateOf(const MenuView& view, Target t, bool enabled = true) const;
    void AddHit(const Rect& r, Target t, const Rect& clip);

    DiagramDraw diagram;
    std::vector<Hit> hits;
    float max_scroll = 0.0f;
};

} // namespace FrameXr::Menu
