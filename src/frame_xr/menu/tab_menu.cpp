// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/menu/tab_menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "frame_xr/menu/md3_fonts.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;

namespace {

// Layout (dp). The rail on the left, the pane (the tab's content) to its right.
constexpr float kRailW = 104.0f;
constexpr float kRailItemH = 80.0f;
constexpr float kPaneMargin = 12.0f;
constexpr float kPaneRadius = 24.0f;
constexpr float kPad = 24.0f;      ///< inside the pane
constexpr float kControlH = 48.0f; ///< segmented buttons, menu buttons, the rows' buttons
constexpr float kLabelGap = 24.0f; ///< between a row's label and its control
constexpr float kStatRowH = 44.0f;
constexpr int kSlots = 10;

struct TabInfo {
    const char* label;
    char32_t icon;
};
constexpr std::array<TabInfo, static_cast<int>(Tab::Count)> kTabs{{
    {"Main", Sym::kHome},
    {"Position", Sym::kOpenWith},
    {"Stats", Sym::kMonitoring},
    {"Controls", Sym::kStadiaController},
    {"Settings", Sym::kSettings},
}};

enum class RowKind { Switch, Segmented, Slider, MenuButton, Offset, Link };

struct Row {
    Id id;
    RowKind kind;
    Lock lock = Lock::None;
    bool on = false;   ///< Switch
    int selected = 0;  ///< Segmented, MenuButton
    float t = 0.0f;    ///< Slider
    int stops = 0;     ///< Slider
    std::string value; ///< Slider, Offset
    bool Enabled() const {
        return lock == Lock::None;
    }
};

std::string Format(const char* f, double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), f, v);
    return buf;
}

std::string SliderText(Id id, float value) {
    if (id == Id::GameFov) {
        return Format("%.0f\u00b0", value);
    }
    if (id == Id::Depth3d) {
        return Format("%.0f%%", value);
    }
    return Format("%.0f", value);
}

std::vector<Row> RowsFor(Tab tab, const MenuModel& m) {
    const auto lock = [&m](Id id) { return m.locked[static_cast<std::size_t>(id)]; };
    const auto row = [&](Id id, RowKind kind, int selected = 0, bool on = false) {
        Row r{id, kind, lock(id)};
        r.selected = selected;
        r.on = on;
        return r;
    };
    std::vector<Row> rows;
    if (tab == Tab::Position) {
        rows.push_back(row(Id::HideBottomScreen, RowKind::Switch, 0, m.hide_bottom_screen));
        rows.push_back(row(Id::ScreenLayout, RowKind::Segmented, m.screen_layout));
        rows.push_back(row(Id::ScreenShape, RowKind::MenuButton, m.screen_shape));
        rows.push_back(row(Id::ScreenCurve, RowKind::MenuButton, m.screen_curve));
        Row offset = row(Id::ImmersiveOffset, RowKind::Offset);
        offset.value = m.immersive_offset;
        offset.on = m.immersive_offset_set;
        rows.push_back(offset);
    } else if (tab == Tab::Settings) {
        // Wider View and on: the Full (6DoF) modes only.
        const auto full = [&](Row r) {
            if (r.lock == Lock::None && m.immersive_mode < 2) {
                r.lock = Lock::FullOnly;
            }
            return r;
        };
        rows.push_back(row(Id::Background, RowKind::Segmented, m.background));
        rows.push_back(row(Id::FaceButtons, RowKind::Segmented, m.face_buttons));
        rows.push_back(row(Id::MotionControls, RowKind::Segmented, m.motion_controls));
        rows.push_back(row(Id::ShowControllers, RowKind::Switch, 0, m.show_controllers));
        rows.push_back(row(Id::WindowScale, RowKind::Segmented, m.window_scale));
        // AzaharFrame depth3d
        Row depth = row(Id::Depth3d, RowKind::Slider);
        const SliderRange dr = SliderFor(Id::Depth3d);
        depth.t = (static_cast<float>(m.depth_3d) - dr.min) / (dr.max - dr.min);
        depth.value = SliderText(Id::Depth3d, static_cast<float>(m.depth_3d));
        rows.push_back(depth);
        rows.push_back(row(Id::ImmersiveMode, RowKind::MenuButton, m.immersive_mode));
        rows.push_back(full(row(Id::WiderView, RowKind::Segmented, m.wider_view)));
        Row fov = full(row(Id::GameFov, RowKind::Slider));
        const SliderRange fr = SliderFor(Id::GameFov);
        fov.t = (static_cast<float>(m.game_fov) - fr.min) / (fr.max - fr.min);
        fov.value = SliderText(Id::GameFov, static_cast<float>(m.game_fov));
        rows.push_back(fov);
        rows.push_back(full(row(Id::NarrowScreen, RowKind::Segmented, m.narrow_screen)));
        rows.push_back(full(row(Id::DepthFactor, RowKind::Segmented, m.depth_factor)));
        rows.push_back(row(Id::MoreSettings, RowKind::Link));
    }
    return rows;
}

} // namespace

const std::vector<std::string>& OptionsFor(Id id) {
    static const std::vector<std::string> kNone;
    static const std::vector<std::string> kShape{"3DS (5:3)",       "Widescreen (16:9)",
                                                 "Ultrawide (21:9)", "Standard (4:3)",
                                                 "3:2",             "16:10"};
    static const std::vector<std::string> kCurve{"Off", "15°", "30°", "45°",
                                                "60°", "75°", "90°"};
    static const std::vector<std::string> kLayout{"Stacked", "Side by Side"};
    static const std::vector<std::string> kStatsAt{"In Front", "On Wrist"}; // statspos
    static const std::vector<std::string> kPage{"In Game", "Azahar's Window"};
    static const std::vector<std::string> kScope{"Global", "Per-Game"};
    static const std::vector<std::string> kBackground{"Black", "Passthrough"};
    static const std::vector<std::string> kFace{"By Position (Nintendo)", "By Label (Xbox)"};
    static const std::vector<std::string> kMotion{"Off", "Right Controller", "Left Controller", "Headset"};
    static const std::vector<std::string> kScale{"100%", "150%", "200%"};
    static const std::vector<std::string> kMode{"Off", "Wide (3DoF)",
                                                "Full (6DoF): Ocarina of Time, Majora's Mask",
                                                "Full (6DoF): Other Games"};
    static const std::vector<std::string> kWider{"Off", "1.1x", "1.2x", "1.3x", "1.4x"};
    static const std::vector<std::string> kNarrow{"Off", "10%", "20%", "30%"};
    static const std::vector<std::string> kDepth{"1x", "10x", "100x"};
    switch (id) {
    case Id::ScreenShape:
        return kShape;
    case Id::ScreenCurve:
        return kCurve;
    case Id::ScreenLayout:
        return kLayout;
    case Id::StatsPosition:
        return kStatsAt;
    case Id::ControlsPage:
        return kPage;
    case Id::Scope:
        return kScope;
    case Id::Background:
        return kBackground;
    case Id::FaceButtons:
        return kFace;
    case Id::MotionControls:
        return kMotion;
    case Id::WindowScale:
        return kScale;
    case Id::ImmersiveMode:
        return kMode;
    case Id::WiderView:
        return kWider;
    case Id::NarrowScreen:
        return kNarrow;
    case Id::DepthFactor:
        return kDepth;
    default:
        return kNone;
    }
}

std::string LabelFor(Id id) {
    switch (id) {
    case Id::Continue:
        return "Continue";
    case Id::QuickSave:
        return "Quick Save";
    case Id::QuickLoad:
        return "Quick Load";
    case Id::SaveState:
        return "Save State";
    case Id::LoadState:
        return "Load State";
    case Id::Screenshot:
        return "Screenshot";
    case Id::Restart:
        return "Restart";
    case Id::AzaharWindow:
        return "Azahar's Window";
    case Id::SwitchToFlat:
        return "Switch to Flat";
    case Id::CloseGame:
        return "Close Game";
    case Id::ResetPosition:
        return "Reset Position";
    case Id::SwapScreens:
        return "Swap Screens";
    case Id::HideBottomScreen:
        return "Hide Bottom Screen";
    case Id::ScreenShape:
        return "Screen Shape";
    case Id::ScreenCurve:
        return "Screen Curve";
    case Id::ScreenLayout:
        return "Screen Layout";
    case Id::ImmersiveOffset:
        return "Immersive Offset";
    case Id::ImmersiveOffsetReset:
        return "Reset";
    case Id::ShowStats:
        return "Show Stats";
    case Id::StatsPosition:
        return "Stats Position";
    case Id::Background:
        return "Background";
    case Id::FaceButtons:
        return "Face Buttons";
    case Id::MotionControls:
        return "Motion Controls";
    case Id::ShowControllers:
        return "Show Controllers";
    case Id::WindowScale:
        return "Window Scale";
    case Id::Depth3d:
        return "3D Depth";
    case Id::ImmersiveMode:
        return "Immersive Mode";
    case Id::WiderView:
        return "Wider View";
    case Id::GameFov:
        return "Immersive Game FOV";
    case Id::NarrowScreen:
        return "Narrow Screen";
    case Id::DepthFactor:
        return "Depth Factor Multiplier";
    case Id::MoreSettings:
        return "More Settings";
    default:
        return {};
    }
}

std::string ExplainFor(Id id) {
    switch (id) {
    case Id::HideBottomScreen:
        return "Shows only the top screen.";
    case Id::ScreenShape:
        return "The top screen's shape, for games or mods that change it.";
    case Id::ScreenCurve:
        return "Bends the main screen so its sides come toward you.";
    case Id::ScreenLayout:
        return "The smaller screen below the main one, or to its right.";
    case Id::ImmersiveOffset:
        return "Moves the immersive screen to fit your lenses, in every game.";
    case Id::ShowStats:
        return "Shows the stats while you play.";
    case Id::StatsPosition:
        return "Where the stats show: in front of you, or on your left wrist.";
    case Id::Background:
        return "What shows behind the screens.";
    case Id::FaceButtons:
        return "How A, B, X and Y map to the 3DS.";
    case Id::MotionControls:
        return "The controller or headset that acts as the 3DS gyroscope.";
    case Id::ShowControllers:
        return "Shows the controllers in VR.";
    case Id::WindowScale:
        return "Makes text and buttons in Azahar's window bigger.";
    case Id::Depth3d:
        return "How deep the 3D looks in this game.";
    case Id::ImmersiveMode:
        return "Draws a wider view of the game around you.";
    case Id::WiderView:
        return "Shows more of the game around the edges. Some games leave them empty.";
    case Id::GameFov:
        return "The game camera's vertical field of view.";
    case Id::NarrowScreen:
        return "Narrows the screen so the side HUD moves into view.";
    case Id::DepthFactor:
        return "How far the game camera moves with your head.";
    default:
        return {};
    }
}

std::string ExplainLock(Lock lock) {
    switch (lock) {
    case Lock::GameValue:
        return "This game has its own value: choose Per-Game to change it.";
    case Lock::FullOnly:
        return "Only in the Full immersive modes.";
    case Lock::Unavailable:
        return "Not available on this headset.";
    default:
        return {};
    }
}

SliderRange SliderFor(Id id) {
    if (id == Id::GameFov) {
        return {30.0f, 110.0f, 1.0f};
    }
    if (id == Id::Depth3d) {
        return {0.0f, 100.0f, 5.0f};
    }
    return {};
}

float MenuLayout::SliderAt(Id id, float x) const {
    for (const SliderTrack& s : sliders) {
        if (s.id == id) {
            // As Kit::Slider places the handle (4 dp wide, centred on the value).
            return std::clamp((x - s.x0 - 2.0f) / std::max(1.0f, s.x1 - s.x0 - 4.0f), 0.0f, 1.0f);
        }
    }
    return -1.0f;
}

State TabMenu::StateOf(const MenuView& view, Target t, bool enabled) const {
    if (!enabled) {
        return State::Disabled;
    }
    if (view.pressed == t) {
        return State::Pressed;
    }
    if (view.hover == t) {
        return State::Hover;
    }
    return State::Rest;
}

void TabMenu::AddHit(const Rect& r, Target t, const Rect& clip) {
    const Rect c{std::max(r.x0, clip.x0), std::max(r.y0, clip.y0), std::min(r.x1, clip.x1),
                 std::min(r.y1, clip.y1)};
    if (c.x1 > c.x0 && c.y1 > c.y0) {
        layout.hits.push_back({c, t});
    }
}

Target MenuLayout::HitTest(float x, float y) const {
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
        if (it->r.Contains(x, y)) {
            return it->t;
        }
    }
    return {};
}

void TabMenu::Draw(Kit& kit, const MenuModel& model, const MenuView& view) {
    layout = {};
    anchors.clear();
    const Rect panel{0, 0, kPanelW, kPanelH};
    kit.Fill(panel, kPanelRadius, Tok::kSurfaceContainer);
    DrawRail(kit, view);
    const Rect pane{kRailW, kPaneMargin, kPanelW - kPaneMargin, kPanelH - kPaneMargin};
    kit.Fill(pane, kPaneRadius, Tok::kSurface);
    switch (view.tab) {
    case Tab::Main:
        if (view.page == Page::Tabs) {
            DrawMain(kit, model, view, pane);
        } else {
            DrawSlots(kit, model, view, pane);
        }
        break;
    case Tab::Stats:
        DrawStats(kit, model, view, pane);
        break;
    case Tab::Controls:
        DrawControls(kit, model, view, pane);
        break;
    default:
        DrawRows(kit, model, view, pane);
        break;
    }
    DrawTooltip(kit, view, pane);
}

void TabMenu::DrawRail(Kit& kit, const MenuView& view) {
    const float total = kRailItemH * static_cast<float>(kTabs.size());
    float y = (kPanelH - total) * 0.5f;
    const Rect all{0, 0, kPanelW, kPanelH};
    for (int i = 0; i < static_cast<int>(kTabs.size()); i++) {
        const Rect r{0.0f, y, kRailW, y + kRailItemH};
        const Target t{Id::TabButton, i};
        const bool active = static_cast<int>(view.tab) == i;
        kit.RailItem(r, kTabs[static_cast<std::size_t>(i)].icon, kTabs[static_cast<std::size_t>(i)].label,
                     active, StateOf(view, t));
        AddHit(r, t, all);
        y += kRailItemH;
    }
}

void TabMenu::DrawMain(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    float y = c.y0;
    if (!model.game_title.empty()) {
        kit.Text(c.x0, y + 28.0f, Type::Headline, Tok::kOnSurface,
                 kit.Fit(Type::Headline, model.game_title, c.W()));
        y += 52.0f;
    }
    const Rect cont{c.x0, y, c.x1, y + 64.0f};
    kit.Button(cont, ButtonStyle::Filled, Sym::kPlayArrow, LabelFor(Id::Continue),
               StateOf(view, {Id::Continue}), Type::Title);
    AddHit(cont, {Id::Continue}, pane);
    y = cont.y1 + 20.0f;

    struct TileDef {
        Id id;
        char32_t icon;
        ButtonStyle style;
    };
    const std::array<TileDef, 9> tiles{{
        {Id::QuickSave, Sym::kSave, ButtonStyle::Tonal},
        {Id::QuickLoad, Sym::kHistory, ButtonStyle::Tonal},
        {Id::Screenshot, Sym::kPhotoCamera, ButtonStyle::Tonal},
        {Id::SaveState, Sym::kSaveAs, ButtonStyle::Tonal},
        {Id::LoadState, Sym::kFolderOpen, ButtonStyle::Tonal},
        {Id::Restart, Sym::kRestartAlt, ButtonStyle::Tonal},
        {Id::AzaharWindow, Sym::kOpenInNew, ButtonStyle::Tonal},
        {Id::SwitchToFlat, Sym::kTv, ButtonStyle::Tonal},
        {Id::CloseGame, Sym::kClose, ButtonStyle::Danger},
    }};
    const float gap = 12.0f;
    const float tw = (c.W() - gap * 2.0f) / 3.0f;
    const float th = (c.y1 - y - gap * 2.0f) / 3.0f;
    for (std::size_t i = 0; i < tiles.size(); i++) {
        const float x = c.x0 + static_cast<float>(i % 3) * (tw + gap);
        const float ty = y + static_cast<float>(i / 3) * (th + gap);
        const Rect r{x, ty, x + tw, ty + th};
        const bool enabled = tiles[i].id != Id::QuickLoad || model.quick_load_available;
        kit.Tile(r, tiles[i].style, tiles[i].icon, LabelFor(tiles[i].id),
                 StateOf(view, {tiles[i].id}, enabled));
        if (enabled) {
            AddHit(r, {tiles[i].id}, pane);
        }
    }
}

// The Main tab's Save State and Load State: the slots of the game, two columns of five.
void TabMenu::DrawSlots(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    const bool saving = view.page == Page::SaveSlots;
    const Rect back{c.x0 - 4.0f, c.y0, c.x0 + 44.0f, c.y0 + 48.0f};
    kit.StateLayer(back, 24.0f, Tok::kOnSurfaceVariant, StateOf(view, {Id::SlotsBack}));
    kit.Icon(back.Cx(), back.Cy(), 24.0f, Sym::kArrowBack, false, Tok::kOnSurfaceVariant);
    AddHit(back, {Id::SlotsBack}, pane);
    kit.TextMid(back.x1 + 12.0f, back.Cy(), Type::Headline, Tok::kOnSurface,
                LabelFor(saving ? Id::SaveState : Id::LoadState));
    const float top = back.y1 + 16.0f;
    const float gap = 12.0f;
    const float cw = (c.W() - gap) * 0.5f;
    const float rh = (c.y1 - top - gap * 4.0f) / 5.0f;
    for (int i = 0; i < kSlots; i++) {
        const int slot = i + 1;
        std::string time;
        for (const SaveSlot& s : model.slots) {
            if (s.slot == slot) {
                time = s.time;
            }
        }
        const bool empty = time.empty();
        const bool enabled = saving || !empty;
        const float x = c.x0 + static_cast<float>(i / 5) * (cw + gap);
        const float y = top + static_cast<float>(i % 5) * (rh + gap);
        const Rect r{x, y, x + cw, y + rh};
        const Target t{Id::Slot, slot};
        const State st = StateOf(view, t, enabled);
        const float op = enabled ? 1.0f : kDisabledContent;
        kit.Fill(r, 16.0f, Tok::kSurfaceContainerHigh, enabled ? 1.0f : 0.5f);
        kit.StateLayer(r, 16.0f, Tok::kOnSurface, st);
        kit.TextMid(r.x0 + 20.0f, r.Cy(), Type::Label, Tok::kOnSurface,
                    "Slot " + std::to_string(slot), 0, op);
        kit.TextMid(r.x1 - 20.0f, r.Cy(), Type::Body, Tok::kOnSurfaceVariant,
                    empty ? std::string("Empty") : time, 2, op);
        if (enabled) {
            AddHit(r, t, pane);
        }
    }
}

void TabMenu::DrawStats(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    // Show Stats: a switch row (the whole row toggles it).
    const Lock lock = model.locked[static_cast<std::size_t>(Id::ShowStats)];
    const bool enabled = lock == Lock::None;
    const Rect row{c.x0 - 12.0f, c.y0 - 8.0f, c.x1 + 12.0f, c.y0 - 8.0f + kRowH - 8.0f};
    const State st = StateOf(view, {Id::ShowStats}, enabled);
    kit.StateLayer(row, 16.0f, Tok::kOnSurface, st);
    kit.TextMid(c.x0, row.Cy(), Type::Body, Tok::kOnSurface, LabelFor(Id::ShowStats), 0,
                enabled ? 1.0f : kDisabledContent);
    kit.Switch({c.x0, row.y0, c.x1, row.y1}, model.show_stats, st);
    AddHit(row, {Id::ShowStats, enabled ? -1 : kLabelIndex}, pane);
    anchors.push_back({Id::ShowStats, c.x0, row.y0 + 6.0f, lock});
    // AzaharFrame statspos: Stats Position under it, a label and its two choices.
    const Rect at_row{row.x0, row.y1, row.x1, row.y1 + kRowH - 8.0f};
    {
        const Lock at_lock = model.locked[static_cast<std::size_t>(Id::StatsPosition)];
        const bool at_enabled = at_lock == Lock::None;
        AddHit(at_row, {Id::StatsPosition, kLabelIndex}, pane);
        anchors.push_back({Id::StatsPosition, c.x0, at_row.y0 + 6.0f, at_lock});
        kit.TextMid(c.x0, at_row.Cy(), Type::Body, Tok::kOnSurface, LabelFor(Id::StatsPosition), 0,
                    at_enabled ? 1.0f : kDisabledContent);
        const auto& opts = OptionsFor(Id::StatsPosition);
        const float w = kit.SegmentedWidth(opts, c.W() * 0.6f);
        const Rect seg{c.x1 - w, at_row.Cy() - kControlH * 0.5f, c.x1,
                       at_row.Cy() + kControlH * 0.5f};
        const int hover = view.hover.id == Id::StatsPosition ? view.hover.index : -1;
        const int pressed = view.pressed.id == Id::StatsPosition ? view.pressed.index : -1;
        kit.Segmented(seg, opts, model.stats_position, hover, pressed, !at_enabled);
        if (at_enabled) {
            const auto e = kit.SegmentEdges(seg, opts);
            for (int i = 0; i < static_cast<int>(opts.size()); i++) {
                AddHit({e[static_cast<std::size_t>(i)], seg.y0, e[static_cast<std::size_t>(i + 1)],
                        seg.y1},
                       {Id::StatsPosition, i}, pane);
            }
        }
    }
    kit.Line(c.x0, at_row.y1 + 8.0f, c.x1, at_row.y1 + 8.0f, 1.0f, Tok::kOutlineVariant);

    const float col_gap = 40.0f;
    const float cw = (c.W() - col_gap) * 0.5f;
    const auto group = [&](float x, float& y, const char* title,
                           const std::vector<std::pair<std::string, std::string>>& rows) {
        if (rows.empty()) {
            return;
        }
        kit.TextMid(x, y + 16.0f, Type::Label, Tok::kPrimary, title);
        y += 36.0f;
        for (const auto& [label, value] : rows) {
            const float cy = y + kStatRowH * 0.5f;
            kit.TextMid(x, cy, Type::Body, Tok::kOnSurfaceVariant, label);
            kit.TextMid(x + cw, cy, Type::Value, Tok::kOnSurface, value, 2);
            y += kStatRowH;
        }
        y += 12.0f;
    };
    float yl = at_row.y1 + 24.0f, yr = yl;
    if (model.stats_game.empty() && model.stats_vr.empty()) {
        kit.TextMid(c.x0, yl + 16.0f, Type::Body, Tok::kOnSurfaceVariant, "No Stats Yet");
    }
    group(c.x0, yl, "Game", model.stats_game);
    group(c.x0 + cw + col_gap, yr, "VR App", model.stats_vr);
    group(c.x0 + cw + col_gap, yr, "System", model.stats_system);
    if (!model.version.empty()) {
        const float cy = c.y1 - kStatRowH * 0.5f;
        kit.TextMid(c.x0, cy, Type::Body, Tok::kOnSurfaceVariant, "Version");
        kit.TextMid(c.x0 + cw, cy, Type::Value, Tok::kOnSurface, model.version, 2);
    }
}

void TabMenu::DrawControls(Kit& kit, const MenuModel& model, const MenuView& view,
                           const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    const auto& pages = OptionsFor(Id::ControlsPage);
    const float w = kit.SegmentedWidth(pages);
    const Rect seg{c.Cx() - w * 0.5f, c.y0, c.Cx() + w * 0.5f, c.y0 + kControlH};
    const int hover = view.hover.id == Id::ControlsPage ? view.hover.index : -1;
    const int pressed = view.pressed.id == Id::ControlsPage ? view.pressed.index : -1;
    kit.Segmented(seg, pages, model.controls_page, hover, pressed);
    const auto pe = kit.SegmentEdges(seg, pages);
    for (int i = 0; i < static_cast<int>(pages.size()); i++) {
        AddHit({pe[static_cast<std::size_t>(i)], seg.y0, pe[static_cast<std::size_t>(i + 1)], seg.y1},
               {Id::ControlsPage, i}, pane);
    }
    const Rect area{pane.x0 + 8.0f, seg.y1 + 8.0f, pane.x1 - 8.0f, pane.y1 - 8.0f};
    if (diagram) {
        diagram(kit, area, model.controls_page);
    }
}

void TabMenu::DrawRows(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    float top = c.y0;
    if (view.tab == Tab::Position) {
        // The two actions of CitraVR's Position tab (Reset) and of the right stick (Swap).
        const float bw = (c.W() - 12.0f) * 0.5f;
        const Rect a{c.x0, top, c.x0 + bw, top + 56.0f};
        const Rect b{c.x1 - bw, top, c.x1, top + 56.0f};
        kit.Button(a, ButtonStyle::Tonal, Sym::kRecenter, LabelFor(Id::ResetPosition),
                   StateOf(view, {Id::ResetPosition}));
        kit.Button(b, ButtonStyle::Tonal, Sym::kSwapVert, LabelFor(Id::SwapScreens),
                   StateOf(view, {Id::SwapScreens}));
        AddHit(a, {Id::ResetPosition}, pane);
        AddHit(b, {Id::SwapScreens}, pane);
        top = a.y1 + 12.0f;
    } else if (view.tab == Tab::Settings) {
        // Owner decision: CitraVR's Global / Per-game choice at the top of its settings.
        const auto& scopes = OptionsFor(Id::Scope);
        const float w = kit.SegmentedWidth(scopes);
        const Rect seg{c.x0, top, c.x0 + w, top + kControlH};
        const int hover = view.hover.id == Id::Scope ? view.hover.index : -1;
        const int pressed = view.pressed.id == Id::Scope ? view.pressed.index : -1;
        const bool enabled = model.locked[static_cast<std::size_t>(Id::Scope)] == Lock::None;
        kit.Segmented(seg, scopes, model.scope, hover, pressed, !enabled);
        if (enabled) {
            const auto se = kit.SegmentEdges(seg, scopes);
            for (int i = 0; i < static_cast<int>(scopes.size()); i++) {
                AddHit({se[static_cast<std::size_t>(i)], seg.y0,
                        se[static_cast<std::size_t>(i + 1)], seg.y1},
                       {Id::Scope, i}, pane);
            }
        }
        if (!model.game_title.empty() && model.scope == 1) {
            kit.TextMid(seg.x1 + 16.0f, seg.Cy(), Type::Body, Tok::kOnSurfaceVariant,
                        kit.Fit(Type::Body, model.game_title, c.x1 - seg.x1 - 16.0f));
        }
        top = seg.y1 + 12.0f;
    }

    const std::vector<Row> rows = RowsFor(view.tab, model);
    // The labels' column: as wide as the widest label.
    float label_w = 0.0f;
    for (const Row& r : rows) {
        label_w = std::max(label_w, kit.TextWidth(Type::Body, LabelFor(r.id)));
    }
    const float control_x0 = c.x0 + label_w + kLabelGap;
    const Rect list{pane.x0, top, pane.x1, pane.y1 - 8.0f};
    const float content_h = kRowH * static_cast<float>(rows.size());
    // Whole rows: the list scrolls in row steps, so no row shows cut at the top.
    const float max_scroll =
        std::max(0.0f, std::ceil((content_h - (list.y1 - list.y0)) / kRowH) * kRowH);
    layout.max_scroll = max_scroll;
    const float scroll = std::clamp(view.scroll[static_cast<int>(view.tab)], 0.0f, max_scroll);
    kit.Clip(list);
    Rect open_anchor{};
    int open_selected = -1;
    float y = top - scroll;
    for (const Row& r : rows) {
        const Rect row{c.x0 - 12.0f, y + 4.0f, c.x1 + 12.0f, y + kRowH - 4.0f};
        const float cy = row.Cy();
        const Target whole{r.id};
        const bool enabled = r.Enabled();
        const bool row_hit = r.kind == RowKind::Switch || r.kind == RowKind::Link;
        if (row.y1 > list.y0 && row.y0 < list.y1) {
            anchors.push_back({r.id, c.x0, std::max(row.y0, list.y0) + 6.0f, r.lock});
        }
        if (row_hit && enabled) {
            kit.StateLayer(row, 16.0f, Tok::kOnSurface, StateOf(view, whole));
            AddHit(row, whole, list);
        } else {
            // The row itself (its label, and a disabled control): only the tooltip.
            AddHit(row, {r.id, kLabelIndex}, list);
        }
        kit.TextMid(c.x0, cy, Type::Body, Tok::kOnSurface, LabelFor(r.id), 0,
                    enabled ? 1.0f : kDisabledContent);
        const Rect ctl{control_x0, cy - kControlH * 0.5f, c.x1, cy + kControlH * 0.5f};
        switch (r.kind) {
        case RowKind::Switch:
            kit.Switch(ctl, r.on, StateOf(view, whole, enabled));
            break;
        case RowKind::Segmented: {
            const auto& opts = OptionsFor(r.id);
            const float w = kit.SegmentedWidth(opts, ctl.W());
            const Rect seg{ctl.x1 - w, ctl.y0, ctl.x1, ctl.y1};
            const int hover = view.hover.id == r.id ? view.hover.index : -1;
            const int pressed = view.pressed.id == r.id ? view.pressed.index : -1;
            kit.Segmented(seg, opts, r.selected, hover, pressed, !enabled);
            if (enabled) {
                const auto e = kit.SegmentEdges(seg, opts);
                for (int i = 0; i < static_cast<int>(opts.size()); i++) {
                    AddHit({e[static_cast<std::size_t>(i)], seg.y0, e[static_cast<std::size_t>(i + 1)],
                            seg.y1},
                           {r.id, i}, list);
                }
            }
            break;
        }
        case RowKind::Slider: {
            const Rect sl{std::max(ctl.x0, ctl.x1 - 420.0f), row.y0, ctl.x1, row.y1};
            float t = r.t;
            std::string value = r.value;
            if (view.dragging == r.id) {
                // Held: where the beam is, at the slider's steps.
                const SliderRange range = SliderFor(r.id);
                const float v = range.min + std::round(view.drag_t * (range.max - range.min) /
                                                       range.step) * range.step;
                t = (v - range.min) / (range.max - range.min);
                value = SliderText(r.id, v);
            }
            const Target held{r.id};
            kit.Slider(sl, t, value,
                       view.dragging == r.id ? State::Pressed : StateOf(view, held, enabled),
                       r.stops);
            layout.sliders.push_back({r.id, sl.x0, sl.x1 - 80.0f});
            if (enabled) {
                AddHit(sl, whole, list);
            }
            break;
        }
        case RowKind::MenuButton: {
            const auto& opts = OptionsFor(r.id);
            const Rect mb{std::max(ctl.x0, ctl.x1 - 420.0f), ctl.y0, ctl.x1, ctl.y1};
            const std::size_t sel =
                std::min(static_cast<std::size_t>(std::max(r.selected, 0)), opts.size() - 1);
            kit.MenuButton(mb, opts[sel], view.open_menu == r.id, StateOf(view, whole, enabled));
            if (enabled) {
                AddHit(mb, whole, list);
            }
            if (view.open_menu == r.id) {
                open_anchor = mb;
                open_selected = r.selected;
            }
            break;
        }
        case RowKind::Offset: {
            const Target reset{Id::ImmersiveOffsetReset};
            const Rect rb{ctl.x1 - 96.0f, ctl.y0, ctl.x1, ctl.y1};
            const bool can = enabled && r.on;
            kit.Button(rb, ButtonStyle::Text, 0, LabelFor(Id::ImmersiveOffsetReset),
                       StateOf(view, reset, can));
            if (can) {
                AddHit(rb, reset, list);
            }
            kit.TextMid(rb.x0 - 12.0f, cy, Type::Value, Tok::kOnSurfaceVariant, r.value, 2);
            break;
        }
        case RowKind::Link:
            kit.Icon(c.x1 - 12.0f, cy, 24.0f, Sym::kOpenInNew, false, Tok::kOnSurfaceVariant);
            break;
        }
        y += kRowH;
    }
    kit.ClearClip();
    if (max_scroll > 0.0f) {
        // A thin scroll indicator at the pane's right edge.
        const float track_h = list.y1 - list.y0 - 16.0f;
        const float thumb_h = std::max(40.0f, track_h * (list.y1 - list.y0) / content_h);
        const float ty = list.y0 + 8.0f + (track_h - thumb_h) * (scroll / max_scroll);
        kit.Fill({pane.x1 - 10.0f, ty, pane.x1 - 6.0f, ty + thumb_h}, 2.0f, Tok::kOnSurfaceVariant,
                 0.38f);
    }
    // An open menu goes over everything, its items before the rest in hit testing.
    if (view.open_menu != Id::None && open_anchor.W() > 0.0f) {
        const auto& opts = OptionsFor(view.open_menu);
        const float item = Kit::MenuItemHeight();
        const float h = item * static_cast<float>(opts.size()) + 16.0f;
        float y0 = open_anchor.y1 + 4.0f;
        if (y0 + h > pane.y1) {
            y0 = std::max(pane.y0, open_anchor.y0 - 4.0f - h);
        }
        const int hover = view.hover.id == view.open_menu ? view.hover.index : -1;
        const Rect box = kit.Menu({open_anchor.x0, y0, open_anchor.x1, y0 + h}, opts, open_selected,
                                  hover);
        const Rect everything{0, 0, kPanelW, kPanelH};
        AddHit(box, {Id::None, kMenuAreaIndex}, everything); // the menu's own area eats clicks
        for (int i = 0; i < static_cast<int>(opts.size()); i++) {
            AddHit({box.x0, box.y0 + 8.0f + item * static_cast<float>(i), box.x1,
                    box.y0 + 8.0f + item * static_cast<float>(i + 1)},
                   {view.open_menu, i}, everything);
        }
    }
}

// The tooltip (owner option A: after the beam rests on a row) over the row's label.
void TabMenu::DrawTooltip(Kit& kit, const MenuView& view, const Rect& pane) {
    if (view.tooltip == Id::None || view.open_menu != Id::None) {
        return;
    }
    for (const Anchor& a : anchors) {
        if (a.id != view.tooltip) {
            continue;
        }
        const std::string text = a.lock != Lock::None ? ExplainLock(a.lock) : ExplainFor(a.id);
        if (text.empty()) {
            return;
        }
        const float half = kit.TextWidth(Type::Tooltip, text) * 0.5f + 10.0f;
        const float lx = a.x + kit.TextWidth(Type::Body, LabelFor(a.id)) * 0.5f;
        const float cx = std::clamp(lx, pane.x0 + half + 4.0f, pane.x1 - half - 4.0f);
        // Above the row; under it when the row is the first one shown.
        const bool above = a.y - 40.0f > pane.y0;
        kit.Tooltip(cx, above ? a.y : a.y + kRowH - 16.0f, text, above);
        return;
    }
}

} // namespace FrameXr::Menu
