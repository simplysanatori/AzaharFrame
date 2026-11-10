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
constexpr float kRowH = 72.0f;     ///< a list row
constexpr float kControlH = 48.0f; ///< segmented buttons, menu buttons, the rows' buttons
constexpr float kLabelGap = 24.0f; ///< between a row's label and its control
constexpr float kStatRowH = 44.0f;

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
    bool enabled = true;
    bool on = false;      ///< Switch
    int selected = 0;     ///< Segmented, MenuButton
    float t = 0.0f;       ///< Slider
    int stops = 0;        ///< Slider
    std::string value;    ///< Slider, Offset
    bool stub = false;    ///< lane/curve's settings, wired when it merges
};

std::string Format(const char* f, double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), f, v);
    return buf;
}

std::vector<Row> RowsFor(Tab tab, const MenuModel& m) {
    std::vector<Row> rows;
    if (tab == Tab::Position) {
        rows.push_back({Id::HideBottomScreen, RowKind::Switch, true, m.hide_bottom_screen});
        rows.push_back({Id::ScreenShape, RowKind::MenuButton, true, false, m.screen_shape});
        Row curve{Id::ScreenCurve, RowKind::Segmented, true, false, m.screen_curve};
        curve.stub = true;
        rows.push_back(curve);
        Row offset{Id::ImmersiveOffset, RowKind::Offset};
        offset.value = m.immersive_offset;
        offset.on = m.immersive_offset_set;
        rows.push_back(offset);
    } else if (tab == Tab::Settings) {
        const bool full = m.immersive_mode >= 2;
        rows.push_back({Id::Background, RowKind::Segmented, true, false, m.background});
        rows.push_back({Id::FaceButtons, RowKind::Segmented, true, false, m.face_buttons});
        rows.push_back({Id::MotionControls, RowKind::Segmented, true, false, m.motion_controls});
        rows.push_back({Id::ShowControllers, RowKind::Switch, true, m.show_controllers});
        rows.push_back({Id::WindowScale, RowKind::Segmented, true, false, m.window_scale});
        rows.push_back({Id::ImmersiveMode, RowKind::MenuButton, true, false, m.immersive_mode});
        Row wider{Id::WiderView, RowKind::Slider, full};
        wider.t = (m.wider_view - 1.0f) / 0.4f;
        wider.stops = 4;
        wider.value = Format("%.1fx", m.wider_view);
        wider.stub = true;
        rows.push_back(wider);
        Row fov{Id::GameFov, RowKind::Slider, full};
        fov.t = static_cast<float>(m.game_fov - 30) / 80.0f;
        fov.value = Format("%.0f°", m.game_fov);
        rows.push_back(fov);
        rows.push_back({Id::SqueezeWidth, RowKind::Segmented, full, false, m.squeeze_width});
        rows.push_back({Id::DepthFactor, RowKind::Segmented, full, false, m.depth_factor});
        rows.push_back({Id::MoreSettings, RowKind::Link});
    }
    return rows;
}

} // namespace

const std::vector<std::string>& OptionsFor(Id id) {
    static const std::vector<std::string> kNone;
    static const std::vector<std::string> kShape{"3DS (5:3)",       "Widescreen (16:9)",
                                                 "Ultrawide (21:9)", "Standard (4:3)",
                                                 "3:2",             "16:10"};
    static const std::vector<std::string> kCurve{"Off", "Gentle", "Medium", "Strong", "Max"};
    static const std::vector<std::string> kPage{"In Game", "Azahar's Window"};
    static const std::vector<std::string> kScope{"Global", "Per-Game"};
    static const std::vector<std::string> kBackground{"Black", "Passthrough"};
    static const std::vector<std::string> kFace{"By Position (Nintendo)", "By Label (Xbox)"};
    static const std::vector<std::string> kMotion{"Off", "Right Controller", "Left Controller"};
    static const std::vector<std::string> kScale{"100%", "150%", "200%"};
    static const std::vector<std::string> kMode{"Off", "Wide (3DoF)",
                                                "Full (6DoF): Ocarina of Time, Majora's Mask",
                                                "Full (6DoF): Other Games"};
    static const std::vector<std::string> kSqueeze{"Off", "10%", "20%"};
    static const std::vector<std::string> kDepth{"1x", "10x", "100x"};
    switch (id) {
    case Id::ScreenShape:
        return kShape;
    case Id::ScreenCurve:
        return kCurve;
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
    case Id::SqueezeWidth:
        return kSqueeze;
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
    case Id::ImmersiveOffset:
        return "Immersive Offset";
    case Id::ImmersiveOffsetReset:
        return "Reset";
    case Id::ShowStats:
        return "Show Stats";
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
    case Id::ImmersiveMode:
        return "Immersive Mode";
    case Id::WiderView:
        return "Wider View";
    case Id::GameFov:
        return "Immersive Game FOV";
    case Id::SqueezeWidth:
        return "Squeeze Width";
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
        return "Bends the screens so their sides come toward you.";
    case Id::ImmersiveOffset:
        return "Moves the immersive screen to fit your lenses, in every game.";
    case Id::ShowStats:
        return "Shows the stats under the top screen while you play.";
    case Id::Background:
        return "What shows behind the screens.";
    case Id::FaceButtons:
        return "How A, B, X and Y map to the 3DS.";
    case Id::MotionControls:
        return "The controller that acts as the 3DS gyroscope.";
    case Id::ShowControllers:
        return "Shows the controllers in VR.";
    case Id::WindowScale:
        return "Makes text and buttons in Azahar's window bigger.";
    case Id::ImmersiveMode:
        return "Draws a wider view of the game around you.";
    case Id::WiderView:
        return "Shows more of the game around the screen.";
    case Id::GameFov:
        return "The game camera's vertical field of view.";
    case Id::SqueezeWidth:
        return "Narrows the screen so the HUD at the sides moves into view.";
    case Id::DepthFactor:
        return "How far the game camera moves with your head.";
    default:
        return {};
    }
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
        hits.push_back({c, t});
    }
}

Target TabMenu::HitTest(float x, float y) const {
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
        if (it->r.Contains(x, y)) {
            return it->t;
        }
    }
    return {};
}

void TabMenu::Draw(Kit& kit, const MenuModel& model, const MenuView& view) {
    hits.clear();
    max_scroll = 0.0f;
    const Rect panel{0, 0, kPanelW, kPanelH};
    kit.Fill(panel, kPanelRadius, Tok::kSurfaceContainer);
    DrawRail(kit, view);
    const Rect pane{kRailW, kPaneMargin, kPanelW - kPaneMargin, kPanelH - kPaneMargin};
    kit.Fill(pane, kPaneRadius, Tok::kSurface);
    switch (view.tab) {
    case Tab::Main:
        DrawMain(kit, model, view, pane);
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
    const std::array<TileDef, 8> tiles{{
        {Id::QuickSave, Sym::kSave, ButtonStyle::Tonal},
        {Id::QuickLoad, Sym::kHistory, ButtonStyle::Tonal},
        {Id::SaveState, Sym::kSaveAs, ButtonStyle::Tonal},
        {Id::LoadState, Sym::kFolderOpen, ButtonStyle::Tonal},
        {Id::Screenshot, Sym::kPhotoCamera, ButtonStyle::Tonal},
        {Id::Restart, Sym::kRestartAlt, ButtonStyle::Tonal},
        {Id::AzaharWindow, Sym::kOpenInNew, ButtonStyle::Tonal},
        {Id::CloseGame, Sym::kClose, ButtonStyle::Danger},
    }};
    const float gap = 12.0f;
    const float tw = (c.W() - gap * 3.0f) / 4.0f;
    const float th = 132.0f;
    for (std::size_t i = 0; i < tiles.size(); i++) {
        const float x = c.x0 + static_cast<float>(i % 4) * (tw + gap);
        const float ty = y + static_cast<float>(i / 4) * (th + gap);
        const Rect r{x, ty, x + tw, ty + th};
        const bool enabled = tiles[i].id != Id::QuickLoad || model.quick_load_available;
        kit.Tile(r, tiles[i].style, tiles[i].icon, LabelFor(tiles[i].id),
                 StateOf(view, {tiles[i].id}, enabled));
        if (enabled) {
            AddHit(r, {tiles[i].id}, pane);
        }
    }
}

void TabMenu::DrawStats(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    // Show Stats: a switch row (the whole row toggles it).
    const Rect row{c.x0 - 12.0f, c.y0 - 8.0f, c.x1 + 12.0f, c.y0 - 8.0f + kRowH - 8.0f};
    const State st = StateOf(view, {Id::ShowStats});
    kit.StateLayer(row, 16.0f, Tok::kOnSurface, st);
    kit.TextMid(c.x0, row.Cy(), Type::Body, Tok::kOnSurface, LabelFor(Id::ShowStats));
    kit.Switch({c.x0, row.y0, c.x1, row.y1}, model.show_stats, st);
    AddHit(row, {Id::ShowStats}, pane);
    kit.Line(c.x0, row.y1 + 8.0f, c.x1, row.y1 + 8.0f, 1.0f, Tok::kOutlineVariant);

    const float col_gap = 40.0f;
    const float cw = (c.W() - col_gap) * 0.5f;
    const auto group = [&](float x, float& y, const char* title,
                           const std::vector<std::pair<std::string, std::string>>& rows) {
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
    float yl = row.y1 + 24.0f, yr = yl;
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
        kit.Segmented(seg, scopes, model.scope, hover, -1);
        const auto se = kit.SegmentEdges(seg, scopes);
        for (int i = 0; i < static_cast<int>(scopes.size()); i++) {
            AddHit({se[static_cast<std::size_t>(i)], seg.y0, se[static_cast<std::size_t>(i + 1)],
                    seg.y1},
                   {Id::Scope, i}, pane);
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
    max_scroll = std::max(0.0f, std::ceil((content_h - (list.y1 - list.y0)) / kRowH) * kRowH);
    const float scroll = std::clamp(view.scroll[static_cast<int>(view.tab)], 0.0f, max_scroll);
    kit.Clip(list);
    Rect open_anchor{};
    int open_selected = -1;
    float y = top - scroll;
    for (const Row& r : rows) {
        const Rect row{c.x0 - 12.0f, y + 4.0f, c.x1 + 12.0f, y + kRowH - 4.0f};
        const float cy = row.Cy();
        const Target whole{r.id};
        const bool row_hit = r.kind == RowKind::Switch || r.kind == RowKind::Link;
        if (row_hit) {
            kit.StateLayer(row, 16.0f, Tok::kOnSurface, StateOf(view, whole, r.enabled));
            if (r.enabled) {
                AddHit(row, whole, list);
            }
        }
        kit.TextMid(c.x0, cy, Type::Body, Tok::kOnSurface, LabelFor(r.id), 0,
                    r.enabled ? 1.0f : kDisabledContent);
        const Rect ctl{control_x0, cy - kControlH * 0.5f, c.x1, cy + kControlH * 0.5f};
        switch (r.kind) {
        case RowKind::Switch:
            kit.Switch(ctl, r.on, StateOf(view, whole, r.enabled));
            break;
        case RowKind::Segmented: {
            const auto& opts = OptionsFor(r.id);
            const float w = kit.SegmentedWidth(opts, ctl.W());
            const Rect seg{ctl.x1 - w, ctl.y0, ctl.x1, ctl.y1};
            const int hover = view.hover.id == r.id ? view.hover.index : -1;
            const int pressed = view.pressed.id == r.id ? view.pressed.index : -1;
            kit.Segmented(seg, opts, r.selected, hover, pressed, !r.enabled);
            if (r.enabled) {
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
            kit.Slider(sl, r.t, r.value, StateOf(view, whole, r.enabled), r.stops);
            if (r.enabled) {
                AddHit(sl, whole, list);
            }
            break;
        }
        case RowKind::MenuButton: {
            const auto& opts = OptionsFor(r.id);
            const Rect mb{std::max(ctl.x0, ctl.x1 - 420.0f), ctl.y0, ctl.x1, ctl.y1};
            kit.MenuButton(mb, opts[static_cast<std::size_t>(r.selected)], view.open_menu == r.id,
                           StateOf(view, whole, r.enabled));
            if (r.enabled) {
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
            kit.Button(rb, ButtonStyle::Text, 0, LabelFor(Id::ImmersiveOffsetReset),
                       StateOf(view, reset, r.on));
            if (r.on) {
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
    // The tooltip (owner option) over the row it explains.
    if (view.tooltip != Id::None) {
        float ty = top - scroll;
        for (const Row& r : rows) {
            if (r.id == view.tooltip) {
                const std::string text = ExplainFor(r.id);
                if (!text.empty()) {
                    const float lx = c.x0 + kit.TextWidth(Type::Body, LabelFor(r.id)) * 0.5f;
                    kit.Tooltip(std::max(lx, c.x0 + kit.TextWidth(Type::Tooltip, text) * 0.5f + 10.0f),
                                ty + 10.0f, text, true);
                }
            }
            ty += kRowH;
        }
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
        AddHit(box, {Id::None, -2}, everything); // the menu's own area eats clicks
        for (int i = 0; i < static_cast<int>(opts.size()); i++) {
            AddHit({box.x0, box.y0 + 8.0f + item * static_cast<float>(i), box.x1,
                    box.y0 + 8.0f + item * static_cast<float>(i + 1)},
                   {view.open_menu, i}, everything);
        }
    }
}

} // namespace FrameXr::Menu
