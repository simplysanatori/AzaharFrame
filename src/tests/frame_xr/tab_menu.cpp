// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: the in-game menu's layout (frame_xr/menu/tab_menu.h): targets big enough
// for a beam, labels that fit, hit testing.

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/ui/ui_ttf.h"
#include "frame_xr/input/mouse.h"   // AzaharFrame mouse
#include "frame_xr/input/reticle.h" // AzaharFrame mouse
#include "frame_xr/toast_fit.h"     // AzaharFrame menuux
#include "frame_xr/ui/ui_canvas.h"

using namespace FrameXr::Menu;

namespace {

constexpr float kListBottom = kPanelH - 12.0f - 8.0f; // where a scrolled list is cut

MenuModel Sample() {
    MenuModel m;
    m.game_title = "The Legend of Zelda: Ocarina of Time 3D";
    m.slots = {{1, "2026-11-08 21:14"}, {3, "2026-11-09 19:02"}};
    m.immersive_mode = 2;
    m.stats_game = {{"App FPS", "30"}};
    return m;
}

struct Drawn {
    Kit kit{1.0f};
    frame_xr::ui::Canvas canvas;
    TabMenu menu;
    void Draw(const MenuModel& m, const MenuView& v) {
        canvas.Init(static_cast<int>(kPanelW), static_cast<int>(kPanelH));
        kit.Begin(canvas);
        menu.Draw(kit, m, v);
    }
};

std::vector<MenuView> AllViews() {
    std::vector<MenuView> views;
    for (int t = 0; t < static_cast<int>(Tab::Count); t++) {
        MenuView v;
        v.tab = static_cast<Tab>(t);
        views.push_back(v);
    }
    MenuView slots;
    slots.page = Page::SaveSlots;
    views.push_back(slots);
    slots.page = Page::LoadSlots;
    views.push_back(slots);
    // AzaharFrame autosave: each Settings section, at its top and its end.
    for (int sec = 0; sec < static_cast<int>(SettingsSection::Count); sec++) {
        MenuView top;
        top.tab = Tab::Settings;
        top.section = static_cast<SettingsSection>(sec);
        views.push_back(top);
        MenuView end = top;
        end.scroll[static_cast<int>(Tab::Settings)] = 10000.0f;
        views.push_back(end);
    }
    return views;
}

} // namespace

TEST_CASE("Tab menu: every target is at least 48 dp", "[frame_xr][tabmenu]") {
    Drawn d;
    for (const MenuView& v : AllViews()) {
        d.Draw(Sample(), v);
        REQUIRE(!d.menu.Hits().empty());
        for (const auto& h : d.menu.Hits()) {
            if (h.t.index == kLabelIndex || h.t.index == kMenuAreaIndex) {
                continue;
            }
            if (h.t.id == Id::Scrollbar) { // AzaharFrame kiumenus: every list's bar, one standard
                INFO("tab " << static_cast<int>(v.tab) << " scrollbar " << h.r.W() << " x " << h.r.H());
                CHECK(h.r.W() >= Scrolling::kHitLeft + Scrolling::kHitRight); // the beam's strip
                CHECK(h.r.H() >= 48.0f);
                continue;
            }
            if ((v.tab == Tab::Position || v.tab == Tab::Settings || v.tab == Tab::Controls) &&
                h.r.y1 >= kListBottom - 0.01f) {
                continue; // cut by the scrolled list's edge: only the part on view is a target
            }
            INFO("tab " << static_cast<int>(v.tab) << " id " << static_cast<int>(h.t.id)
                        << " index " << h.t.index << " " << h.r.W() << " x " << h.r.H());
            CHECK(h.r.W() >= 48.0f);
            CHECK(h.r.H() >= 48.0f);
        }
    }
}

TEST_CASE("Tab menu: labels and options are not cut", "[frame_xr][tabmenu]") {
    Kit kit(1.0f);
    // The Main tab's tiles: four to a row in the pane (AzaharFrame menuux).
    const float tile_w = (kPanelW - 104.0f - 12.0f - 48.0f - 36.0f) / 4.0f;
    for (Id id : {Id::QuickSave, Id::QuickLoad, Id::Screenshot, Id::SaveState, Id::LoadState,
                  Id::Restart, Id::AzaharWindow, Id::CloseGame}) {
        const std::string label = LabelFor(id);
        REQUIRE(!label.empty());
        CHECK(kit.Fit(Type::Label, label, tile_w - 16.0f) == label);
    }
    // Each segment is as wide as its label and padding.
    Drawn d;
    MenuModel m = Sample();
    for (const MenuView& v : AllViews()) {
        d.Draw(m, v);
        for (const auto& h : d.menu.Hits()) {
            const auto& options = OptionsFor(h.t.id);
            if (h.t.index < 0 || options.empty() || h.t.id == Id::ScreenShape ||
                h.t.id == Id::ImmersiveMode || h.r.y1 >= kListBottom - 0.01f) {
                continue;
            }
            const std::string& o = options[static_cast<std::size_t>(h.t.index)];
            INFO(LabelFor(h.t.id) << ": " << o);
            CHECK(h.r.W() + 0.5f >= kit.TextWidth(Type::Label, o) + 24.0f);
        }
    }
}

TEST_CASE("Tab menu: hit testing", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    d.Draw(m, v);
    const auto centre = [&](Id id, int index = -1) -> Target {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                return d.menu.HitTest(h.r.Cx(), h.r.Cy());
            }
        }
        return {};
    };
    CHECK(centre(Id::Continue) == Target{Id::Continue});
    CHECK(centre(Id::SwitchToFlat) == Target{Id::SwitchToFlat});
    CHECK(centre(Id::TabButton, 4) == Target{Id::TabButton, 4});
    CHECK(d.menu.HitTest(-5.0f, -5.0f) == Target{});

    // Load State: the empty slots are no targets.
    v.page = Page::LoadSlots;
    d.Draw(m, v);
    CHECK(centre(Id::Slot, 1) == Target{Id::Slot, 1});
    CHECK(centre(Id::Slot, 2) == Target{});
    v.page = Page::SaveSlots;
    d.Draw(m, v);
    CHECK(centre(Id::Slot, 2) == Target{Id::Slot, 2});

    // An open menu is hit before the rows under it.
    v = {};
    v.tab = Tab::Position;
    v.open_menu = Id::ScreenShape;
    d.Draw(m, v);
    CHECK(centre(Id::ScreenShape, 3) == Target{Id::ScreenShape, 3});

    // The Render Margin row in an Off immersive mode: only its tooltip (AzaharFrame fovaxis:
    // Render Margin: Horizontal, one of its two rows).
    v = {};
    v.tab = Tab::Settings;
    v.section = SettingsSection::Immersive; // AzaharFrame autosave
    m.immersive_mode = 0;
    // AzaharFrame depth3d: lower down now; menuux: not at the end any more, so every scroll.
    bool label = false;
    for (float scroll = 0.0f;; scroll += 32.0f) {
        v.scroll[static_cast<int>(Tab::Settings)] = scroll;
        d.Draw(m, v);
        for (const auto& h : d.menu.Hits()) {
            CHECK(!(h.t.id == Id::MarginX && h.t.index >= 0));
            CHECK(!(h.t.id == Id::MarginY && h.t.index >= 0));
            label |= h.t.id == Id::MarginX && h.t.index == kLabelIndex;
        }
        if (scroll >= d.menu.MaxScroll()) {
            break;
        }
    }
    CHECK(label);

    // A locked Background: no segments.
    m = Sample();
    v.section = SettingsSection::General;
    v.scroll[static_cast<int>(Tab::Settings)] = 0.0f;
    m.locked[static_cast<std::size_t>(Id::Background)] = Lock::Unavailable;
    d.Draw(m, v);
    for (const auto& h : d.menu.Hits()) {
        CHECK(!(h.t.id == Id::Background && h.t.index >= 0));
    }
    CHECK(!ExplainLock(Lock::GameValue).empty());
    CHECK(!ExplainLock(Lock::Unavailable).empty());
}

TEST_CASE("Tab menu: sliders map the beam to their range", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuView v;
    v.tab = Tab::Settings;
    v.section = SettingsSection::Immersive; // AzaharFrame autosave
    v.scroll[static_cast<int>(Tab::Settings)] = 10000.0f;
    d.Draw(Sample(), v);
    const MenuLayout& layout = d.menu.Layout();
    const MenuLayout::SliderTrack* fov = nullptr;
    for (const auto& track : layout.sliders) {
        if (track.id == Id::GameFov) {
            fov = &track;
        }
    }
    REQUIRE(fov != nullptr);
    const auto& s = *fov;
    CHECK(layout.SliderAt(Id::GameFov, s.x0 - 50.0f) == 0.0f);
    CHECK(layout.SliderAt(Id::GameFov, s.x1 + 50.0f) == 1.0f);
    const float mid = layout.SliderAt(Id::GameFov, (s.x0 + s.x1) * 0.5f);
    CHECK(mid > 0.45f);
    CHECK(mid < 0.55f);
    CHECK(layout.SliderAt(Id::MarginX, s.x0) < 0.0f);
}

// AzaharFrame depth3d
TEST_CASE("Tab menu: 3D Depth is a 0 to 100 % slider on the Settings tab", "[frame_xr][tabmenu]") {
    CHECK(LabelFor(Id::Depth3d) == "3D Depth");
    CHECK(ExplainFor(Id::Depth3d) == // AzaharFrame natural: both meanings
          "How deep the 3D looks. In the Full modes, 100% puts the farthest scenery at infinity.");
    const SliderRange r = SliderFor(Id::Depth3d);
    CHECK(r.min == 0.0f);
    CHECK(r.max == 100.0f);
    CHECK(r.step == 5.0f);

    Drawn d;
    MenuView v;
    v.tab = Tab::Settings;
    v.section = SettingsSection::Graphics; // AzaharFrame autosave
    MenuModel m = Sample();
    m.depth_3d = 50;
    bool found = false;
    for (float scroll = 0.0f; scroll <= 2000.0f && !found; scroll += kRowH) {
        v.scroll[static_cast<int>(Tab::Settings)] = scroll;
        d.Draw(m, v);
        for (const auto& track : d.menu.Layout().sliders) {
            if (track.id == Id::Depth3d) {
                found = true;
                const float mid = d.menu.Layout().SliderAt(Id::Depth3d, (track.x0 + track.x1) * 0.5f);
                CHECK(mid > 0.45f);
                CHECK(mid < 0.55f);
            }
        }
    }
    CHECK(found);
}

// AzaharFrame menu: Move Screens, Touchpad Touch, Edit Controls and Cheats are on the menu.
TEST_CASE("Tab menu: Move Screens, Touchpad, Edit Controls, Cheats", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    const auto has = [&d](Id id, int index) {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                return true;
            }
        }
        return false;
    };
    MenuView v;
    v.tab = Tab::Position;
    d.Draw(m, v);
    CHECK(has(Id::MoveScreens, 0));
    CHECK(has(Id::MoveScreens, 2));
    CHECK(OptionsFor(Id::MoveScreens) == std::vector<std::string>{"Off", "Secondary", "Both"});
    v.tab = Tab::Controls;
    d.Draw(m, v);
    CHECK(has(Id::TouchpadTouch, 0));
    CHECK(has(Id::TouchpadTouch, 1));
    CHECK_FALSE(has(Id::TouchpadTouch, 2));
    CHECK(has(Id::EditControls, -1));
    // AzaharFrame menuux: the owner's names.
    CHECK(LabelFor(Id::TouchpadTouch) == "Touchpad");
    CHECK(OptionsFor(Id::TouchpadTouch) ==
          std::vector<std::string>{"Tap for Input", "Click for Input"});
    // AzaharFrame mouse: Bottom Screen Reticle under Touchpad.
    CHECK(has(Id::BottomScreenReticle, 0));
    CHECK(has(Id::BottomScreenReticle, 2));
    CHECK(has(Id::BottomScreenReticle, 3)); // AzaharFrame input2
    CHECK(OptionsFor(Id::BottomScreenReticle) ==
          std::vector<std::string>{"Off", "Left Controller", "Right Controller", "Headset"});
    CHECK(LabelFor(Id::BottomScreenReticle) == "Bottom Screen Reticle");
    // At Edge: only while the reticle is on.
    CHECK_FALSE(has(Id::ReticleAtEdge, 0));
    m.bottom_screen_reticle = 2;
    // AzaharFrame kiumenus: the Controls page scrolls; At Edge is under the picture and the rows
    // above it (the picture keeps its height), so one step down brings it into view
    v.scroll[static_cast<int>(Tab::Controls)] = 1.0e4f;
    d.Draw(m, v);
    CHECK(has(Id::ReticleAtEdge, 0));
    CHECK(has(Id::ReticleAtEdge, 1));
    v.scroll[static_cast<int>(Tab::Controls)] = 0.0f;
    d.Draw(m, v);
    CHECK(has(Id::TouchpadTouch, 1));
    CHECK(has(Id::EditControls, -1));
    CHECK(OptionsFor(Id::ReticleAtEdge) == std::vector<std::string>{"Follow", "Snap to Middle"});
    CHECK(LabelFor(Id::ReticleAtEdge) == "At Edge");
}

// AzaharFrame ui2: the controller picture at the top, the Touchpad, Bottom Screen Reticle and
// At Edge rows under it at the bottom of the pane (owner's report 14).
TEST_CASE("Tab menu: Controls has the picture on top, the rows at the bottom", "[frame_xr][tabmenu]") {
    for (const int reticle : {0, 2}) {
        Drawn d;
        Rect picture{};
        d.menu.SetDiagram([&picture](Kit&, const Rect& area, int) { picture = area; });
        MenuModel m = Sample();
        m.bottom_screen_reticle = reticle;
        MenuView v;
        v.tab = Tab::Controls;
        d.Draw(m, v);
        // most of the pane; AzaharFrame reticle: two rows fewer of it while the reticle is on;
        // AzaharFrame fixes7: three (the switches' row); kiugyro: C-Stick at Edge has no row
        REQUIRE(picture.H() > (reticle != 0 ? 90.0f : 200.0f));
        Rect edit{}, pages{};
        float rows_y0 = 1e9f, rows_y1 = 0.0f;
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == Id::EditControls) {
                edit = h.r;
            }
            if (h.t.id == Id::ControlsPage) {
                pages.y1 = std::max(pages.y1, h.r.y1);
            }
            if (h.t.id == Id::TouchpadTouch || h.t.id == Id::BottomScreenReticle ||
                h.t.id == Id::ReticleSpeed || h.t.id == Id::ReticleAtEdge ||
                h.t.id == Id::ReticleCenter || h.t.id == Id::ReticleStick) { // fixes7
                rows_y0 = std::min(rows_y0, h.r.y0);
                rows_y1 = std::max(rows_y1, h.r.y1);
            }
        }
        // AzaharFrame reticle: under the pages; Edit Controls in a column at its right
        CHECK(picture.y0 >= pages.y1);
        CHECK(picture.x1 <= edit.x0);
        CHECK(edit.y1 <= rows_y0);
        CHECK(rows_y0 >= picture.y1);   // the rows under the picture
        CHECK(rows_y1 <= kPanelH);      // and inside the panel
        CHECK(rows_y1 > kPanelH - 80.0f); // at its bottom
    }
}

// AzaharFrame cheatstab: the Mods tab and its Cheats segment.
namespace {
CheatsModel SampleCheats() {
    CheatsModel c;
    c.cheats = {{"No Bloom (USA)", CheatSource::Developer, true, ""},
                {"Infinite HP", CheatSource::Sharkive, false, "for v1.0, this game is v1.1"},
                {"Max Money", CheatSource::User, false, ""}};
    for (int i = 0; i < 12; i++) {
        c.cheats.push_back({"Sharkive " + std::to_string(i), CheatSource::Sharkive, false, ""});
    }
    c.download = DownloadState::Done;
    c.status = "15 cheats added, 1 for another version";
    return c;
}
} // namespace

TEST_CASE("Tab menu: the Mods tab", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    const auto has = [&d](Id id, int index) {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                return true;
            }
        }
        return false;
    };
    MenuView v;
    // The rail: Main, Position, Stats, Controls, Mods, Homebrew (AzaharFrame ui3), Settings.
    d.Draw(m, v);
    CHECK(has(Id::TabButton, static_cast<int>(Tab::Mods)));
    CHECK(has(Id::TabButton, static_cast<int>(Tab::Homebrew)));
    CHECK(static_cast<int>(Tab::Mods) == static_cast<int>(Tab::Controls) + 1);
    CHECK(static_cast<int>(Tab::Homebrew) == static_cast<int>(Tab::Mods) + 1);
    CHECK(static_cast<int>(Tab::Settings) == static_cast<int>(Tab::Homebrew) + 1);
    // Seven rail items, each a 48 dp target at least, inside the panel.
    int rail = 0;
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id == Id::TabButton) {
            rail++;
            CHECK(h.r.H() >= 48.0f);
            CHECK(h.r.y0 >= 0.0f);
            CHECK(h.r.y1 <= kPanelH);
        }
    }
    CHECK(rail == static_cast<int>(Tab::Count));

    // The segmented button on every segment; Cheats first. Homebrew is a tab of its own (ui3).
    v.tab = Tab::Mods;
    CHECK(OptionsFor(Id::ModsSegment) == std::vector<std::string>{"Cheats", "Mods", "Textures"});
    for (const ModsSegment s : {ModsSegment::Cheats, ModsSegment::Mods, ModsSegment::Textures}) {
        v.mods_segment = s;
        d.Draw(m, v);
        CHECK(has(Id::ModsSegment, 0));
        CHECK(has(Id::ModsSegment, 2));
        CHECK(has(Id::CheatsWindow, -1) == (s == ModsSegment::Cheats));
    }
    v.mods_segment = ModsSegment::Cheats;

    // Empty: Download in the middle, Azahar's Window at the top, nothing scrolls.
    d.Draw(m, v);
    CHECK(has(Id::DownloadCheats, -1));
    CHECK(has(Id::CheatsWindow, -1));
    CHECK(!has(Id::Cheat, 0));
    CHECK(d.menu.MaxScroll() == 0.0f);
    // Downloading with nothing yet: no Download. Failed: Try Again (the same target).
    m.mods.cheats.download = DownloadState::Downloading;
    d.Draw(m, v);
    CHECK(!has(Id::DownloadCheats, -1));
    m.mods.cheats.download = DownloadState::Failed;
    m.mods.cheats.status = "No reply";
    d.Draw(m, v);
    CHECK(has(Id::DownloadCheats, -1));

    // A list: one row per cheat in file order (no patch row), targets at least 48 dp, the list
    // scrolls to its end. AzaharFrame scroll: the one for another version (1) is in the closed
    // section at the end, shown when it opens.
    m.mods.cheats = SampleCheats();
    d.Draw(m, v);
    CHECK(has(Id::DownloadCheats, -1));
    CHECK(has(Id::Cheat, 0));
    CHECK(!has(Id::Cheat, 1));
    CHECK(has(Id::Cheat, 2));
    REQUIRE(d.menu.MaxScroll() > 0.0f);
    {
        // Rows follow each other with nothing between them.
        float y0 = -1.0f, y1 = -1.0f;
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == Id::Cheat && h.t.index == 0) {
                y0 = h.r.y0;
            } else if (h.t.id == Id::Cheat && h.t.index == 2) {
                y1 = h.r.y0;
            }
        }
        CHECK(y1 - y0 == 48.0f);
    }
    v.cheats_other_open = true;
    v.scroll[static_cast<int>(Tab::Mods)] = 10000.0f;
    d.Draw(m, v);
    CHECK(has(Id::Cheat, 1));
    CHECK(has(Id::CheatsOtherVersion, -1));
    v.cheats_other_open = false;
    v.scroll[static_cast<int>(Tab::Mods)] = 10000.0f;
    d.Draw(m, v);
    CHECK(has(Id::Cheat, static_cast<int>(m.mods.cheats.cheats.size()) - 1));
    CHECK(!has(Id::Cheat, 0));
    for (float scroll : {0.0f, 10000.0f}) {
        v.scroll[static_cast<int>(Tab::Mods)] = scroll;
        d.Draw(m, v);
        for (const auto& h : d.menu.Hits()) {
            if (h.t.index == kLabelIndex || h.r.y1 >= kListBottom - 0.01f ||
                (h.t.id == Id::Cheat && h.r.H() < 48.0f)) {
                continue; // cut by the list's edges
            }
            INFO("id " << static_cast<int>(h.t.id) << " index " << h.t.index);
            CHECK(h.r.W() >= 48.0f);
            CHECK(h.r.H() >= 48.0f);
        }
    }
    // While a download runs over a list, Download waits.
    v.scroll = {};
    m.mods.cheats.download = DownloadState::Downloading;
    d.Draw(m, v);
    CHECK(!has(Id::DownloadCheats, -1));
    CHECK(has(Id::Cheat, 0));

    CHECK(VersionChip("for v1.0, this game is v1.1") == "v1.0");
    CHECK(VersionChip("for Rev0, this cartridge is Rev1") == "Rev0");
    CHECK(VersionChip("") == "");
    CHECK(FailureReason("Download failed: No reply") == "No reply");
    CHECK(FailureReason("Download failed: server returned 500") == "Server returned 500");
    CHECK(FailureReason("No Internet Connection") == "No Internet Connection");
}

// AzaharFrame texui: the Mods tab's Mods and Textures segments (mods_segments.h).
namespace {

constexpr Tab kModsTab = Tab::Mods;

bool HasHit(const Drawn& d, Id id, int index) {
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id == id && h.t.index == index) {
            return true;
        }
    }
    return false;
}

const MenuLayout::Hit* FindHit(const Drawn& d, Id id, int index) {
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id == id && h.t.index == index) {
            return &h;
        }
    }
    return nullptr;
}

// AzaharFrame kiumenus: the Controls page scrolls; draws it with the reticle's rows (`rows_up` dp
// above Aiming) or with Aiming itself at the top of the list.
void DrawControlsAt(Drawn& d, MenuModel m, MenuView& v, float rows_up) {
    if (rows_up > 0.0f) {
        m.gyro_aim = 2; // Aiming's rows below, so the page scrolls as far as the reticle's rows
    }
    v.scroll[static_cast<int>(Tab::Controls)] = 0.0f;
    d.Draw(m, v);
    v.scroll[static_cast<int>(Tab::Controls)] = std::max(0.0f, d.menu.Layout().aiming_scroll - rows_up);
    d.Draw(m, v);
}
constexpr float kReticleRowsUp = 330.0f; ///< Touchpad's row down to At Edge's pair, above Aiming

/// Every target of the segment is at least 48 dp and inside the panel.
// AzaharFrame useglobal; reticle: the views with per-game rows, and a model where the game has
// its own value for every one.
namespace {
std::vector<MenuView> PerGameViews() {
    std::vector<MenuView> views;
    for (int sec = 0; sec < static_cast<int>(SettingsSection::Count); sec++) {
        for (float scroll : {0.0f, 10000.0f}) {
            MenuView v;
            v.tab = Tab::Settings;
            v.section = static_cast<SettingsSection>(sec);
            v.scroll[static_cast<int>(Tab::Settings)] = scroll;
            views.push_back(v);
        }
    }
    MenuView position;
    position.tab = Tab::Position;
    views.push_back(position);
    MenuView controls; // AzaharFrame reticle
    controls.tab = Tab::Controls;
    views.push_back(controls);
    return views;
}

MenuModel AllOwn(MenuModel m) {
    for (int i = 0; i < kIdCount; i++) {
        m.game_value[static_cast<std::size_t>(i)] = PerGameSetting(static_cast<Id>(i));
    }
    return m;
}
} // namespace

void CheckTargets(const Drawn& d) {
    // AzaharFrame scroll: a list scrolls smoothly, so a row can show cut at its top or bottom
    const MenuLayout& layout = d.menu.Layout();
    const auto cut = [&layout](const Rect& r) {
        return layout.bar.Valid() && (r.y0 <= layout.bar.list.y0 + 0.01f ||
                                      r.y1 >= layout.bar.list.y1 - 0.01f);
    };
    for (const auto& h : d.menu.Hits()) {
        if (h.t.index == kLabelIndex || h.t.index == kMenuAreaIndex) {
            continue;
        }
        if (h.t.id == Id::Scrollbar) {
            continue; // a strip down the list's edge (list_scroll.cpp checks it)
        }
        if (h.r.H() < 48.0f && cut(h.r)) {
            continue;
        }
        INFO("id " << static_cast<int>(h.t.id) << " index " << h.t.index << " " << h.r.W()
                   << " x " << h.r.H());
        CHECK(h.r.W() >= 48.0f);
        CHECK(h.r.H() >= 48.0f);
        CHECK(h.r.x1 <= kPanelW + 0.01f);
        CHECK(h.r.y1 <= kPanelH + 0.01f);
    }
}

TexturesModel Packs(int n) {
    TexturesModel t;
    t.custom_textures = true;
    for (int i = 0; i < n; i++) {
        t.packs.push_back({"p" + std::to_string(i), "Pack " + std::to_string(i),
                           i % 2 ? "" : JoinDetail({"Henriko Magnifico", "3.2", "PNG", "5.8 GB"}),
                           i == 1});
    }
    return t;
}

} // namespace

TEST_CASE("Tab menu: the Textures segment", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Textures;

    // No packs: only the empty state, nothing to switch.
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::CustomTextures, -1));
    CHECK(!HasHit(d, Id::TexturePack, 0));
    CHECK(d.menu.MaxScroll() == 0.0f);

    // Custom Textures, three packs (a checkbox each; AzaharFrame texlayers), Preload: all on view
    // without scrolling.
    m.mods.textures = Packs(3);
    d.Draw(m, v);
    CHECK(HasHit(d, Id::CustomTextures, -1));
    CHECK(HasHit(d, Id::TexturePreload, -1));
    for (int i = 0; i < 3; i++) {
        CHECK(HasHit(d, Id::TexturePack, i));
    }
    CHECK(!HasHit(d, Id::TexturePack, 3)); // no Off row
    CHECK(d.menu.MaxScroll() == 0.0f);
    CheckTargets(d);
    // In that order.
    REQUIRE(FindHit(d, Id::TexturePack, 0));
    CHECK(FindHit(d, Id::CustomTextures, -1)->r.y1 <= FindHit(d, Id::TexturePack, 0)->r.y0);
    CHECK(FindHit(d, Id::TexturePack, 0)->r.y1 <= FindHit(d, Id::TexturePack, 1)->r.y0 + 0.01f);
    CHECK(FindHit(d, Id::TexturePack, 2)->r.y1 <= FindHit(d, Id::TexturePreload, -1)->r.y0);
    // No Enabled Textures with one pack on
    CHECK(!HasHit(d, Id::TextureLayer, 0));
    CHECK(!HasHit(d, Id::LayerUp, 1));
    // Download Packs above them.
    REQUIRE(FindHit(d, Id::DownloadPacks, -1));
    CHECK(FindHit(d, Id::DownloadPacks, -1)->r.y1 <= FindHit(d, Id::CustomTextures, -1)->r.y0);
    // With the snackbar too: Preload a short scroll away.
    m.mods.textures.needs_restart = true;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::RestartGame, -1));
    CHECK(d.menu.MaxScroll() <= 48.0f);
    v.scroll[static_cast<int>(kModsTab)] = 10000.0f;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::TexturePreload, -1));
    v.scroll[static_cast<int>(kModsTab)] = 0.0f;

    // Custom Textures off: the packs and Preload are labels only; busy: everything waits.
    m.mods.textures.custom_textures = false;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::CustomTextures, -1));
    CHECK(!HasHit(d, Id::TexturePack, 1));
    CHECK(HasHit(d, Id::TexturePack, kLabelIndex));
    CHECK(!HasHit(d, Id::TexturePreload, -1));
    m.mods.textures.custom_textures = true;
    m.mods.textures.busy = true;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::CustomTextures, -1));
    CHECK(!HasHit(d, Id::TexturePack, 0));

    // Many packs: the list scrolls above the snackbar, the last row shows whole at the end.
    m.mods.textures = Packs(10);
    m.mods.textures.needs_restart = true;
    d.Draw(m, v);
    REQUIRE(d.menu.MaxScroll() > 0.0f);
    const float snack_y0 = FindHit(d, Id::RestartGame, -1)->r.y0;
    for (const float scroll : {0.0f, 130.0f, 10000.0f}) {
        v.scroll[static_cast<int>(kModsTab)] = scroll;
        d.Draw(m, v);
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id != Id::RestartGame && h.t.id != Id::TabButton) {
                CHECK(h.r.y1 <= snack_y0 + 0.01f); // nothing under the snackbar
            }
        }
        CheckTargets(d);
    }
    CHECK(HasHit(d, Id::TexturePreload, -1)); // at the end
    CHECK(LabelFor(Id::CustomTextures) == "Custom Textures");
    CHECK(LabelFor(Id::TexturePreload) == "Preload");
    CHECK(LabelFor(Id::RestartGame) == "Restart");
}

TEST_CASE("Tab menu: Download Packs", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Textures;

    // No packs: the empty state's button opens Download Packs.
    d.Draw(m, v);
    CHECK(HasHit(d, Id::DownloadPacks, -1));

    // A downloaded pack has Delete; one added by hand has not.
    m.mods.textures = Packs(2);
    m.mods.textures.packs[1].downloaded = true;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::DownloadPacks, -1));
    CHECK(!HasHit(d, Id::PackDelete, 0));
    REQUIRE(FindHit(d, Id::PackDelete, 1));
    CHECK(FindHit(d, Id::PackDelete, 1)->r.H() >= 48.0f);
    m.mods.textures.downloading = true; // not while a download runs
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::PackDelete, 1));
    m.mods.textures.downloading = false;
    d.Draw(m, v);
    REQUIRE(FindHit(d, Id::PackDelete, 1));
    const auto del = FindHit(d, Id::PackDelete, 1)->r;
    m.mods.textures.dialog = {ModsDialogKind::DeletePack, "4K", ""};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::DialogConfirm, -1));
    CHECK(d.menu.HitTest(del.Cx(), del.Cy()).id != Id::PackDelete); // the dialog is modal
    m.mods.textures.dialog = {};

    // The page: back, then one row per pack with its action.
    v.textures_page = TexturesPage::Download;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::TexturesBack, -1));
    CHECK(!HasHit(d, Id::CustomTextures, -1));
    m.mods.textures.downloads = {
        {"a", "Pack A", "Author", "https://github.com/owner/a", "78 MB", PackOfferState::Download, -1, ""},
        {"b", "Pack B", "Someone", "https://example.org/packs/b", "1.8 GB", PackOfferState::Installed, -1, ""},
        {"c", "Pack C", "", "", "", PackOfferState::Failed, -1, "Download Failed"}};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::PackDownload, 0));
    // AzaharFrame modcat2: a row's name is the anchor of its notes tooltip
    CHECK(HasHit(d, Id::PackNote, 0));
    CHECK(HasHit(d, Id::PackNote, 2));
    {
        const auto* name = FindHit(d, Id::PackNote, 0);
        REQUIRE(name);
        // on the name's line: the author link under it starts at the row's middle line
        CHECK(d.menu.HitTest(name->r.x0 + 40.0f, name->r.y0 + 16.0f) == Target{Id::PackNote, 0});
        m.mods.textures.downloads[0].notes =
            "A 4K pack for the whole game. It is large, so it asks first. Restart the game after it installs.";
        v.tooltip = Id::PackNote;
        v.tooltip_index = 0;
        d.Draw(m, v); // the wrapped tooltip draws without disturbing the hits
        CHECK(HasHit(d, Id::PackNote, 0));
        CHECK(HasHit(d, Id::PackDownload, 0));
        v.tooltip = Id::None;
        v.tooltip_index = -1;
        m.mods.textures.downloads[0].notes.clear();
        d.Draw(m, v);
    }
    CHECK(!HasHit(d, Id::PackDownload, 1)); // Installed: no action
    CHECK(HasHit(d, Id::PackDownload, 2));  // Try Again
    CHECK(HasHit(d, Id::PackLink, 0));
    CHECK(HasHit(d, Id::PackLink, 1));
    CHECK(!HasHit(d, Id::PackLink, 2));
    CheckTargets(d);
    // The link's target stays off the name line.
    REQUIRE(FindHit(d, Id::PackLink, 0));
    REQUIRE(FindHit(d, Id::PackDownload, kLabelIndex));
    CHECK(d.menu.HitTest(FindHit(d, Id::PackLink, 0)->r.x0 + 30.0f,
                         FindHit(d, Id::PackLink, 0)->r.y0 - 20.0f)
              .id != Id::PackLink);

    // Downloading: Cancel; the other Download buttons wait.
    m.mods.textures.downloading = true;
    m.mods.textures.downloads[0].state = PackOfferState::Downloading;
    m.mods.textures.downloads[0].percent = 42;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::PackCancel, 0));
    CHECK(!HasHit(d, Id::PackDownload, 0));
    CHECK(!HasHit(d, Id::PackDownload, 2));
    CheckTargets(d);

    // Many: the list scrolls, whole rows only.
    m.mods.textures.downloading = false;
    m.mods.textures.downloads.resize(12, m.mods.textures.downloads[2]);
    d.Draw(m, v);
    CHECK(d.menu.MaxScroll() > 0.0f);
    v.scroll[static_cast<int>(kModsTab)] = 10000.0f;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::PackDownload, 11));
    CheckTargets(d);

    // Nothing for the title.
    m.mods.textures.downloads.clear();
    d.Draw(m, v);
    CHECK(HasHit(d, Id::TexturesBack, -1));
    CHECK(!HasHit(d, Id::PackDownload, 0));
    CHECK(LabelFor(Id::DownloadPacks) == "Download Packs");
    CHECK(LabelFor(Id::PackCancel) == "Cancel");
    CHECK(LabelFor(Id::PackDelete) == "Delete");
}

TEST_CASE("Tab menu: the Mods segment", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Mods;

    // Nothing loaded: Install Mods only.
    d.Draw(m, v);
    CHECK(HasHit(d, Id::InstallMods, -1));
    CHECK(!HasHit(d, Id::Mod, 0));
    CheckTargets(d);

    // A managed mod (its row switches it), one off in the library, your own files (Remove only).
    auto& mm = m.mods.mods;
    mm.loaded = {{"a", "No Bloom", "simplysanatori", "https://github.com/simplysanatori/3ds-game-fixes", "", true, true, true},
                 {"b", "Ocarina ReFramed (Dual Screen)", "OTPR26", "https://github.com/OTPR26/OOT3DHud", "v1.5.9", false, true, false},
                 {"unmanaged:code.ips", "code.ips", "", "", "", false, false, true}};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::InstallMods, -1));
    CHECK(HasHit(d, Id::Mod, 0));
    CHECK(HasHit(d, Id::Mod, 1));
    CHECK(!HasHit(d, Id::Mod, 2));
    // "Created by <author>": a link for GitHub repositories only, over its row.
    CHECK(HasHit(d, Id::ModLink, 0));
    CHECK(HasHit(d, Id::ModLink, 1));
    CHECK(!HasHit(d, Id::ModLink, 2)); // your own files: no author
    {
        const auto* link = FindHit(d, Id::ModLink, 0);
        REQUIRE(link);
        CHECK(d.menu.HitTest(link->r.Cx(), link->r.y1 - 4.0f) == Target{Id::ModLink, 0});
        // Not over the name line: the row's top 32 dp toggle the row.
        const auto* row = FindHit(d, Id::Mod, 0);
        REQUIRE(row);
        CHECK(link->r.y0 >= row->r.y0 + 32.0f - 0.01f);
        CHECK(link->r.H() >= 48.0f);
        CHECK(d.menu.HitTest(link->r.Cx(), row->r.y0 + 24.0f) == Target{Id::Mod, 0});
    }
    for (int i = 0; i < 3; i++) {
        CHECK(HasHit(d, Id::ModRemove, i));
    }
    CheckTargets(d);
    // Remove sits on its row and wins over it.
    const auto* remove = FindHit(d, Id::ModRemove, 0);
    REQUIRE(remove);
    CHECK(d.menu.HitTest(remove->r.Cx(), remove->r.Cy()) == Target{Id::ModRemove, 0});
    // Busy: nothing to press but Install Mods.
    mm.busy = true;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::Mod, 0));
    CHECK(!HasHit(d, Id::ModRemove, 0));
    CHECK(HasHit(d, Id::InstallMods, -1));
    mm.busy = false;

    // Install Mods: loading, then the title's mods.
    v.mods_page = ModsPage::Install;
    mm.catalog = CatalogState::Loading;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModsBack, -1));
    CHECK(!HasHit(d, Id::InstallMods, -1));
    CHECK(!HasHit(d, Id::ModInstall, 0));
    // AzaharFrame jsoncrash: the page asks again while the catalog is Idle (its request was dropped)
    CHECK(ShouldAskCatalog(CatalogState::Idle, true, 100, -1));
    CHECK_FALSE(ShouldAskCatalog(CatalogState::Idle, true, 1000, 100));
    CHECK(ShouldAskCatalog(CatalogState::Idle, true, 1700, 100));
    CHECK_FALSE(ShouldAskCatalog(CatalogState::Idle, false, 9999, -1));
    CHECK_FALSE(ShouldAskCatalog(CatalogState::Loading, true, 9999, 0));
    CHECK_FALSE(ShouldAskCatalog(CatalogState::Ready, true, 9999, 0));
    CHECK_FALSE(ShouldAskCatalog(CatalogState::Failed, true, 9999, 0));
    // AzaharFrame jsoncrash: a list that could not be made says so, with Retry (a hit that asks for
    // the catalog again), and no ring
    mm.catalog = CatalogState::Failed;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModsBack, -1));
    CHECK(HasHit(d, Id::InstallMods, -1));
    mm.catalog = CatalogState::Ready;
    mm.available = {{"s", "Ocarina ReFramed (Single Screen)", "OTPR26", "https://github.com/OTPR26/OOT3DHud", false, OfferState::Install, "", false},
                    {"d", "Ocarina ReFramed (Dual Screen)", "OTPR26", "https://github.com/OTPR26/OOT3DHud", false, OfferState::Installed, "", false},
                    {"r", "Project Restoration", "leoetlino", "https://github.com/leoetlino/project-restoration", false, OfferState::Unavailable,
                     "Unsupported Archive", false},
                    {"n", "No Bloom", "simplysanatori", "http://example.com/x", true, OfferState::Installing, "Downloading", false}};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModInstall, 0));
    CHECK(!HasHit(d, Id::ModInstall, 1)); // installed
    CHECK(!HasHit(d, Id::ModInstall, 2)); // cannot install yet
    CHECK(!HasHit(d, Id::ModInstall, 3)); // installing
    // AzaharFrame integ3: while it downloads, Cancel in the Install button's place (only there)
    // (AzaharFrame compat: the taller catalog rows put the fourth one under the fold: scrolled)
    v.scroll[static_cast<int>(kModsTab)] = 10000.0f;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModCancel, 3));
    CHECK(!HasHit(d, Id::ModCancel, 0));
    CHECK(LabelFor(Id::ModCancel) == "Cancel");
    {
        const auto* cancel = FindHit(d, Id::ModCancel, 3);
        REQUIRE(cancel);
        CHECK(d.menu.HitTest(cancel->r.Cx(), cancel->r.Cy()) == Target{Id::ModCancel, 3});
    }
    v.scroll[static_cast<int>(kModsTab)] = 0.0f;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModLink, 0));
    CHECK(!HasHit(d, Id::ModLink, 3)); // not an https address: no link
    CheckTargets(d);
    CHECK(!HasHit(d, Id::ModVersion, 2)); // no version chip
    // AzaharFrame modcat2: every row's name anchors its notes tooltip
    CHECK(HasHit(d, Id::ModNote, 0));
    CHECK(HasHit(d, Id::ModNote, 2));
    mm.available[0].notes = "Covers all regions. The folder for the game's own title ID is used. Replaces any other code patch.";
    v.tooltip = Id::ModNote;
    v.tooltip_index = 0;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModNote, 0));
    CHECK(HasHit(d, Id::ModInstall, 0));
    v.tooltip = Id::None;
    v.tooltip_index = -1;
    mm.available[0].notes.clear();
    {
        // three sentences wrap to the width and the box fits them
        const std::string three = "A single screen layout with no touch screen needed. It covers the North America, Europe and Japan releases. It replaces any other code patch for the game.";
        const auto lines = d.kit.TooltipLines(three, 300.0f);
        CHECK(lines.size() >= 3);
        for (const auto& l : lines) {
            CHECK(d.kit.TextWidth(Type::Tooltip, l) <= 300.0f);
        }
        CHECK(d.kit.TooltipLines(three, 1000000.0f).size() == 1);
        CHECK(d.kit.TooltipLines("", 300.0f).empty());
    }
    // AzaharFrame modcat: version chips (a hit each), the matching one filled, a tooltip on hover
    mm.available[2].version = "1.0";
    mm.available[2].version_tip = "For the original 1.0 game, or 1.0 with the 1.1 update.";
    mm.available[0].version = "Card 1.1";
    mm.available[0].version_tip = "For game cards with 1.1 built in.";
    mm.available[0].version_match = true;
    mm.available[0].updated = "Updated Mar 2022";
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModVersion, 0));
    CHECK(HasHit(d, Id::ModVersion, 2));
    CHECK(HasHit(d, Id::ModInstall, 0)); // the chip does not take the button
    CHECK(HasHit(d, Id::ModLink, 0));
    const auto* chip = FindHit(d, Id::ModVersion, 0);
    REQUIRE(chip);
    CHECK(d.menu.HitTest(chip->r.Cx(), chip->r.Cy()) == Target{Id::ModVersion, 0});
    v.tooltip = Id::ModVersion;
    v.tooltip_index = 0;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModVersion, 0));
    v.tooltip = Id::None;
    v.tooltip_index = -1;
    CheckTargets(d);
    mm.available[0].version.clear();
    mm.available[0].version_tip.clear();
    mm.available[0].version_match = false;
    mm.available[2].version.clear();
    mm.busy = true; // another install runs: Install waits
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::ModInstall, 0));
    mm.busy = false;

    // A conflict: the dialog's two answers, nothing under it.
    mm.dialog = {ModsDialogKind::Replace, "Ocarina ReFramed (Single Screen)",
                 "Ocarina ReFramed (Dual Screen)"};
    d.Draw(m, v);
    const auto* confirm = FindHit(d, Id::DialogConfirm, -1);
    const auto* cancel = FindHit(d, Id::DialogCancel, -1);
    REQUIRE(confirm);
    REQUIRE(cancel);
    CHECK(cancel->r.x1 <= confirm->r.x0); // Replace last
    CheckTargets(d);
    CHECK(d.menu.HitTest(confirm->r.Cx(), confirm->r.Cy()) == Target{Id::DialogConfirm});
    CHECK(d.menu.HitTest(10.0f, 10.0f).index == kMenuAreaIndex); // the rail too
    CHECK(d.menu.HitTest(kPanelW - 40.0f, kPanelH - 40.0f).index == kMenuAreaIndex);
    // A model dialog wins over a link's question; with none, "Open GitHub?" with Cancel, Open.
    mm.dialog = {};
    v.open_link = "https://github.com/OTPR26/OOT3DHud";
    d.Draw(m, v);
    CHECK(HasHit(d, Id::DialogConfirm, -1));
    CHECK(d.menu.HitTest(10.0f, 10.0f).index == kMenuAreaIndex);
    v.open_link.clear();
    mm.dialog.kind = ModsDialogKind::Blocked;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::DialogCancel, -1));
    CHECK(!HasHit(d, Id::DialogConfirm, -1));
    mm.dialog = {};

    // The snackbar after a change, on both pages.
    mm.needs_restart = true;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::RestartGame, -1));
    v.mods_page = ModsPage::Loaded;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::RestartGame, -1));
    CheckTargets(d);
    CHECK(LabelFor(Id::InstallMods) == "Install Mods");
    CHECK(LabelFor(Id::ModRemove) == "Remove");
    CHECK(LabelFor(Id::ModInstall) == "Install");
}

TEST_CASE("Mods segments: links", "[frame_xr][tabmenu]") {
    CHECK(LinkText("https://github.com/Gray-Rice/PokeTex-3DS") == "github.com/Gray-Rice/PokeTex-3DS");
    CHECK(LinkHost("https://github.com/Gray-Rice/PokeTex-3DS") == "github.com");
    CHECK(LinkText("https://gamebanana.com/mods/12345/") == "gamebanana.com/mods/12345");
    CHECK(LinkHost("https://gamebanana.com/mods/12345") == "gamebanana.com");
    for (const char* bad : {"http://github.com/a/b", "https://user@host.com/x", "https://host.com:8080/x",
                            "https://localhost/x", "https://Host.com/x", "javascript:alert(1)",
                            "https://host.com/a b", "https://.host.com/x", ""}) {
        CHECK(LinkText(bad).empty());
        CHECK(LinkHost(bad).empty());
    }
}

// AzaharFrame ui3: an Install Mods row's version after the author, and its date, whenever known.
TEST_CASE("Mods segments: a catalog row's version and date", "[frame_xr][tabmenu]") {
    CHECK(ModVersionText("") == "");
    CHECK(ModVersionText("172") == "v172");
    CHECK(ModVersionText("1.0.0") == "v1.0.0");
    CHECK(ModVersionText("v1.5.9") == "v1.5.9");
    CHECK(ModVersionText("Beta 2") == "Beta 2");

    // Kid Icarus: Uprising's C-Stick & Gyro patch (MEGA, no date or version from the source): the
    // catalog's "version" and "updated".
    ModOffer kiu;
    kiu.id = "kiu-cstick-gyro";
    kiu.name = "C-Stick & Gyro";
    kiu.author = "Darkshade";
    kiu.mod_version = "v172";
    kiu.updated = "Updated Oct 2026";
    const auto all = [](const std::vector<std::string>& parts) {
        return BylineText("Darkshade", parts, parts.size());
    };
    CHECK(all(OfferParts(kiu)) == "Created by Darkshade · v172 · Updated Oct 2026");
    // With the rest known: the version, then the downloads, the date last.
    kiu.downloads = "1K+ Downloads";
    kiu.version_match = true;
    CHECK(all(OfferParts(kiu)) ==
          "Created by Darkshade · v172 · Matches Your Game · 1K+ Downloads · Updated Oct 2026");
    // Installing or failed: the version and the note.
    kiu.note = "Downloading 42%";
    CHECK(all(OfferParts(kiu)) == "Created by Darkshade · v172 · Downloading 42%");
    CHECK(OfferParts(kiu)[1] == "Downloading 42%"); // the part drawn in red when it failed
    kiu.note.clear();
    // Nothing known: the author alone.
    ModOffer bare;
    bare.author = "Someone";
    CHECK(BylineText("Someone", OfferParts(bare), 4) == "Created by Someone");
    // A pack: the downloads and the date; the phase while it downloads.
    PackOffer pack;
    pack.downloads = "10K+ Downloads";
    pack.updated = "Updated Mar 2026";
    CHECK(BylineText("Henriko Magnifico", PackOfferParts(pack), 2) ==
          "Created by Henriko Magnifico · 10K+ Downloads · Updated Mar 2026");
    pack.state = PackOfferState::Downloading;
    pack.note = "Unpacking";
    CHECK(PackOfferParts(pack) == std::vector<std::string>{"Unpacking"});

    // The row draws in the Install Mods list with its link and button, the version beside the
    // author (the hit target stays the author's name alone).
    Drawn d;
    MenuModel m = Sample();
    m.mods.mods.catalog = CatalogState::Ready;
    kiu.note.clear();
    kiu.url = "https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/";
    m.mods.mods.available = {kiu};
    MenuView v;
    v.tab = Tab::Mods;
    v.mods_segment = ModsSegment::Mods;
    v.mods_page = ModsPage::Install;
    d.Draw(m, v);
    bool install = false, link = false;
    for (const auto& h : d.menu.Hits()) {
        install |= h.t.id == Id::ModInstall && h.t.index == 0;
        if (h.t.id == Id::ModLink && h.t.index == 0) {
            link = true;
            CHECK(h.r.W() < 200.0f);
        }
    }
    CHECK(install);
    CHECK(link);
}

TEST_CASE("Mods segments: supporting lines", "[frame_xr][tabmenu]") {
    CHECK(GithubLinkText("https://github.com/OTPR26/OOT3DHud") == "github.com/OTPR26/OOT3DHud");
    CHECK(GithubLinkText("https://github.com/simplysanatori/3ds-game-fixes/") ==
          "github.com/simplysanatori/3ds-game-fixes");
    CHECK(GithubLinkText("http://github.com/a/b").empty());
    CHECK(GithubLinkText("https://github.com.evil.example/a/b").empty());
    CHECK(GithubLinkText("https://github.com/a").empty());
    CHECK(GithubLinkText("https://github.com/a/b/releases").empty());
    CHECK(GithubLinkText("https://github.com/a/b?x=1").empty());
    CHECK(GithubLinkText("https://github.com/../b").empty());
    CHECK(JoinDetail({"OTPR26", "", "v1.5.9"}) == "OTPR26 · v1.5.9");
    CHECK(JoinDetail({"", ""}) == "");
    CHECK(SizeText(512) == "512 B");
    // AzaharFrame texrow: 1024s, as the download sizes and the free space lines
    CHECK(SizeText(640ull << 20) == "640 MB");
    CHECK(SizeText(2'254'857'830) == "2.1 GB");
    CHECK(SizeText(12'345) == "12 KB");
    CHECK(SizeText(435'611'336) == "415 MB"); // an optimized 5X Redrawn UI: 415 MiB
    CHECK(SizeText(61'865'984) == "59 MB");   // its PNG original: 59 MiB
    CHECK(SizeText(1000ull << 20) == "1000 MB");
    CHECK(SizeText(1ull << 30) == "1.0 GB");
}

TEST_CASE("texrow: Delete's text for a pack and an Optimize for Frame pair", "[texrow]") {
    CHECK(DeletePackBody(false, false) == "You can download it again.");
    CHECK(DeletePackBody(false, true) == "You can install it again from its zip.");
    CHECK(DeletePackBody(true, false) ==
          "Deletes the optimized pack and the original PNG pack. You can download it again.");
    CHECK(DeletePackBody(true, true) ==
          "Deletes the optimized pack and the original PNG pack. You can install it again from its zip.");
}

// AzaharFrame menuux: Switch to Flat, the Settings tab's renderer rows, the Touchpad row's line,
// and no text running into other text or out of its place with the longest strings.
namespace {

MenuModel Longest() {
    MenuModel m = Sample();
    m.game_title = "The Legend of Zelda: Majora's Mask 3D (Europe) (En,Fr,De,Es,It)";
    m.immersive_offset = "Right 10.0°, Down 10.0°";
    m.immersive_offset_set = true;
    for (int i = 1; i <= 10; i++) {
        m.slots.push_back({i, "2026-12-31 23:59"});
    }
    m.stats_game = {{"App FPS", "60"}, {"Emulation Speed", "100%"}, {"Frame Time", "16.7 ms"},
                    {"Internal Resolution", "8x"}, {"Shaders Compiled", "1234"}};
    m.stats_vr = {{"Display FPS", "120"}, {"Late Frames", "30"}};
    m.stats_system = {{"Battery", "100%"}, {"Temperature", "45°C"}};
    m.version = "1.2.0";
    m.immersive_mode = 2; // the longest option
    m.screen_shape = 1;
    m.motion_controls = 1;
    m.internal_resolution = 8;
    m.texture_filter = 2;
    m.fast_forward_speed = 3;
    m.bottom_screen_reticle = 3; // AzaharFrame mouse: the Reticle and At Edge rows too (Headset)
    return m;
}

struct Audit {
    Kit kit{1.0f};
    frame_xr::ui::Canvas canvas;
    TabMenu menu;
    std::vector<DrawnText> texts;
    void Draw(const MenuModel& m, const MenuView& v) {
        canvas.Init(static_cast<int>(kPanelW), static_cast<int>(kPanelH));
        kit.Begin(canvas);
        texts.clear();
        kit.RecordText(&texts);
        menu.Draw(kit, m, v);
        kit.RecordText(nullptr);
    }
    bool Drew(const std::string& s) const {
        for (const auto& t : texts) {
            if (t.text == s) {
                return true;
            }
        }
        return false;
    }
    // Every line on view stays inside the panel and its clip, and keeps 8 dp from any other
    // line beside it.
    void Check(const char* where) const {
        constexpr float kGap = 8.0f;
        std::vector<Rect> shown;
        std::vector<std::string> what;
        for (const auto& t : texts) {
            const float y0 = std::max(t.box.y0, t.clip.y0), y1 = std::min(t.box.y1, t.clip.y1);
            if (y1 <= y0) {
                continue; // scrolled out of view
            }
            INFO(where << ": \"" << t.text << "\" " << t.box.x0 << ".." << t.box.x1);
            CHECK(t.box.x0 >= -0.5f);
            CHECK(t.box.x1 <= kPanelW + 0.5f);
            CHECK(t.box.x0 >= t.clip.x0 - 0.5f);
            CHECK(t.box.x1 <= t.clip.x1 + 0.5f);
            shown.push_back({t.box.x0, y0, t.box.x1, y1});
            what.push_back(t.text);
        }
        for (std::size_t i = 0; i < shown.size(); i++) {
            for (std::size_t j = i + 1; j < shown.size(); j++) {
                const Rect& a = shown[i];
                const Rect& b = shown[j];
                const float v = std::min(a.y1, b.y1) - std::max(a.y0, b.y0);
                if (v <= 2.0f) {
                    continue; // different lines
                }
                const float gap = std::max(a.x0, b.x0) - std::min(a.x1, b.x1);
                INFO(where << ": \"" << what[i] << "\" and \"" << what[j] << "\" " << gap
                           << " dp apart");
                CHECK(gap >= kGap);
            }
        }
    }
};

} // namespace

TEST_CASE("Tab menu: Switch to Flat on the Main and Settings tabs", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    d.Draw(m, v);
    const auto find = [&d](Id id) -> const MenuLayout::Hit* {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id && h.t.index == -1) {
                return &h;
            }
        }
        return nullptr;
    };
    const auto* main_flat = find(Id::SwitchToFlat);
    const auto* cont = find(Id::Continue);
    REQUIRE(main_flat != nullptr);
    REQUIRE(cont != nullptr);
    CHECK(main_flat->r.W() == cont->r.W()); // as wide as Continue
    CHECK(main_flat->r.y0 > cont->r.y1);
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id != Id::SwitchToFlat && h.t.id != Id::TabButton) {
            CHECK(h.r.y1 <= main_flat->r.y0); // at the bottom
        }
    }
    CHECK(LabelFor(Id::SwitchToFlat) == "Switch to Flat");
    const float main_w = main_flat->r.W(); // the hits are drawn again below
    // Settings: the last row, as wide as the pane's content, both scopes.
    for (int scope = 0; scope < 2; scope++) {
        m.scope = scope;
        v = {};
        v.tab = Tab::Settings;
        v.scroll[static_cast<int>(Tab::Settings)] = 10000.0f;
        d.Draw(m, v);
        const auto* flat = find(Id::SwitchToFlat);
        REQUIRE(flat != nullptr);
        CHECK(flat->r.W() == main_w);
        const auto* more = find(Id::MoreSettings);
        REQUIRE(more != nullptr);
        CHECK(flat->r.y0 >= more->r.y1);
        CHECK(d.menu.HitTest(flat->r.Cx(), flat->r.Cy()) == Target{Id::SwitchToFlat});
    }
}

TEST_CASE("Tab menu: Internal Resolution, Dynamic Resolution, Texture Filter and the picks",
          "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    m.internal_resolution = 3;
    MenuView v;
    v.tab = Tab::Settings;
    bool seen_res = false, seen_dyn = false, seen_filter = false, seen_async = false,
         seen_ff = false, seen_copies = false;
    for (int sec = 0; sec < static_cast<int>(SettingsSection::Count); sec++) // AzaharFrame autosave
    for (float scroll = 0.0f; scroll <= 3000.0f; scroll += kRowH) {
        v.section = static_cast<SettingsSection>(sec);
        v.scroll[static_cast<int>(Tab::Settings)] = scroll;
        d.Draw(m, v);
        for (const auto& h : d.menu.Hits()) {
            seen_res |= h.t.id == Id::InternalResolution && h.t.index == -1;
            seen_dyn |= h.t.id == Id::DynamicResolution && h.t.index == -1;
            seen_filter |= h.t.id == Id::TextureFilter && h.t.index == 4;
            seen_async |= h.t.id == Id::AsyncShaders && h.t.index == -1;
            seen_copies |= h.t.id == Id::AccurateImageCopies && h.t.index == 2; // xfergpu
            seen_ff |= h.t.id == Id::FastForwardSpeed && h.t.index == 3;
        }
    }
    CHECK(seen_res);
    CHECK(seen_dyn);
    CHECK(seen_filter);
    CHECK(seen_async);
    CHECK(seen_copies);
    CHECK(SectionOf(Id::AccurateImageCopies) == SettingsSection::Graphics);
    CHECK(LabelFor(Id::AccurateImageCopies) == "Accurate Image Copies");
    CHECK(ExplainFor(Id::AccurateImageCopies) ==
          "Fixes flickering, shimmering or glow glitches in some games.");
    CHECK(OptionsFor(Id::AccurateImageCopies) ==
          std::vector<std::string>{"Off", "Automatic", "On"}); // AzaharFrame xfergpu
    CHECK(seen_ff);
    CHECK(OptionsFor(Id::InternalResolution).size() == static_cast<std::size_t>(kMaxResolution));
    CHECK(OptionsFor(Id::InternalResolution).front() == "1x (400x240)");
    CHECK(OptionsFor(Id::InternalResolution).back() == "8x (3200x1920)");
    CHECK(OptionsFor(Id::TextureFilter) ==
          std::vector<std::string>{"None", "Bicubic", "ScaleForce", "xBRZ", "MMPX"});
    CHECK(OptionsFor(Id::FastForwardSpeed) ==
          std::vector<std::string>{"150%", "200%", "300%", "400%"});
    for (Id id : {Id::InternalResolution, Id::DynamicResolution, Id::TextureFilter,
                  Id::AsyncShaders, Id::AccurateImageCopies, Id::FastForwardSpeed}) {
        CHECK(!LabelFor(id).empty());
        CHECK(!ExplainFor(id).empty());
    }
    // No volume, frame rate or refresh rate row (owner decision).
    for (int i = 0; i < kIdCount; i++) {
        const std::string label = LabelFor(static_cast<Id>(i));
        CHECK(label.find("Volume") == std::string::npos);
        CHECK(label.find("Frame Rate") == std::string::npos);
        CHECK(label.find("Refresh") == std::string::npos);
    }
}

// AzaharFrame fovonce: Detect Game FOV (Match Game Camera) is Off | Once | Always on the Immersive section, per
// game, each choice with its own tooltip; its label keeps the row's.
TEST_CASE("Tab menu: Detect Game FOV is Off, Once or Always", "[frame_xr][tabmenu]") {
    CHECK(LabelFor(Id::MatchCamera) == "Detect Game FOV");
    CHECK(SectionOf(Id::MatchCamera) == SettingsSection::Immersive);
    CHECK(PerGameSetting(Id::MatchCamera));
    CHECK(OptionsFor(Id::MatchCamera) == std::vector<std::string>{"Off", "Once", "Always"});
    CHECK(ExplainFor(Id::MatchCamera) ==
          "Reads the game's field of view so the world lines up with your head.");
    CHECK(HasOptionTips(Id::MatchCamera));
    CHECK(ExplainOption(Id::MatchCamera, 0) == "Uses Immersive Game FOV as set.");
    CHECK(ExplainOption(Id::MatchCamera, 1) ==
          "Measures it once while you play and saves it. Choose again to remeasure.");
    CHECK(ExplainOption(Id::MatchCamera, 2) == "Keeps measuring and eases into changes.");
    CHECK(ExplainOption(Id::MatchCamera, 3).empty());
    CHECK(ExplainOption(Id::MatchCamera, kLabelIndex).empty());
    CHECK(MenuModel{}.match_camera == 2); // Always, the earlier "on"

    // Every tooltip fits the Settings pane on one line (DrawTooltip centres it within the pane;
    // older builds aborted on one wider than the pane).
    {
        Kit kit{1.0f};
        for (const std::string& text :
             {ExplainFor(Id::MatchCamera), ExplainOption(Id::MatchCamera, 0),
              ExplainOption(Id::MatchCamera, 1), ExplainOption(Id::MatchCamera, 2)}) {
            INFO(text);
            CHECK(kit.TextWidth(Type::Tooltip, text) + 28.0f <= kPanelW * 0.7f);
        }
    }

    // Each choice is a segment of the row, whichever is picked (Once can be picked again).
    for (int pick = 0; pick < 3; pick++) {
        Drawn d;
        MenuModel m = Sample();
        m.match_camera = pick;
        MenuView v;
        v.tab = Tab::Settings;
        v.section = SettingsSection::Immersive;
        bool seen[3] = {};
        for (float scroll = 0.0f; scroll <= 3000.0f; scroll += kRowH) {
            v.scroll[static_cast<int>(Tab::Settings)] = scroll;
            d.Draw(m, v);
            for (int i = 0; i < 3; i++) {
                seen[i] |= FindHit(d, Id::MatchCamera, i) != nullptr;
            }
        }
        INFO("picked " << pick);
        CHECK(seen[0]);
        CHECK(seen[1]);
        CHECK(seen[2]);
    }
}

// AzaharFrame xfergpu: Accurate Image Copies is Off | Automatic | On, each choice with its own
// tooltip over its segment (the owner's words); its label keeps the row's.
TEST_CASE("Tab menu: a segmented row's choices have their own tooltips", "[frame_xr][tabmenu]") {
    CHECK(HasOptionTips(Id::AccurateImageCopies));
    CHECK(!HasOptionTips(Id::TextureFilter));
    CHECK(ExplainOption(Id::TextureFilter, 0).empty());
    CHECK(ExplainOption(Id::AccurateImageCopies, 0) ==
          "No effect on performance, but graphical glitches may occur.");
    CHECK(ExplainOption(Id::AccurateImageCopies, 1) ==
          "Fixes glitches only where a game needs it. Little to no effect on performance.");
    CHECK(ExplainOption(Id::AccurateImageCopies, 2) ==
          "Makes every image copy exact. Substantially lowers performance.");
    CHECK(ExplainOption(Id::AccurateImageCopies, 3).empty());
    CHECK(ExplainOption(Id::AccurateImageCopies, kLabelIndex).empty());

    Drawn d;
    MenuModel m = Sample();
    m.accurate_image_copies = 1;
    MenuView v;
    v.tab = Tab::Settings;
    v.section = SettingsSection::Graphics;
    const MenuLayout::Hit* seg[3] = {};
    for (float scroll = 0.0f; scroll <= 3000.0f && !seg[0]; scroll += kRowH) {
        v.scroll[static_cast<int>(Tab::Settings)] = scroll;
        d.Draw(m, v);
        seg[0] = FindHit(d, Id::AccurateImageCopies, 0);
    }
    REQUIRE(seg[0]);
    const std::vector<MenuLayout::Hit> hits = d.menu.Hits(); // the layout without a tooltip
    const auto hit = [&](int index) -> const MenuLayout::Hit* {
        for (const auto& h : hits) {
            if (h.t.id == Id::AccurateImageCopies && h.t.index == index) {
                return &h;
            }
        }
        return nullptr;
    };
    REQUIRE(hit(1));
    REQUIRE(hit(2));
    REQUIRE(hit(kLabelIndex));
    // The horizontal extent of what the tooltip adds, with the beam resting on `index`.
    const auto tooltip = [&](int index) {
        MenuView tv = v;
        tv.hover = {Id::AccurateImageCopies, index};
        d.Draw(m, tv);
        const std::vector<uint32_t> plain = d.canvas.px;
        tv.tooltip = Id::AccurateImageCopies;
        tv.tooltip_index = index;
        d.Draw(m, tv);
        int x0 = d.canvas.w, x1 = -1;
        for (int y = 0; y < d.canvas.h; y++) {
            for (int x = 0; x < d.canvas.w; x++) {
                const std::size_t i = static_cast<std::size_t>(y) * d.canvas.w + x;
                if (d.canvas.px[i] != plain[i]) {
                    x0 = std::min(x0, x);
                    x1 = std::max(x1, x);
                }
            }
        }
        return std::pair{x0, x1};
    };
    std::vector<std::pair<int, int>> drawn;
    for (int i = 0; i < 3; i++) {
        const auto [x0, x1] = tooltip(i);
        INFO("segment " << i);
        REQUIRE(x1 >= x0); // a tooltip drew
        // centred over its own segment (kept inside the pane)
        CHECK(static_cast<float>(x0) <= hit(i)->r.Cx());
        CHECK(static_cast<float>(x1) >= hit(i)->r.Cx());
        drawn.push_back({x0, x1});
    }
    CHECK(drawn[0] != drawn[1]); // each choice's own text
    CHECK(drawn[1] != drawn[2]);
    // the label: the row's own tooltip
    const auto [lx0, lx1] = tooltip(kLabelIndex);
    CHECK(lx1 >= lx0);
}

TEST_CASE("Tab menu: Dynamic Resolution has nothing to do at 1x", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Settings;
    v.section = SettingsSection::Graphics; // AzaharFrame autosave
    // Whether its switch can be pressed (-1), or only its tooltip shows (kLabelIndex).
    const auto hit = [&](int factor) {
        m.internal_resolution = factor;
        for (float scroll = 0.0f; scroll <= 3000.0f; scroll += kRowH) {
            v.scroll[static_cast<int>(Tab::Settings)] = scroll;
            d.Draw(m, v);
            for (const auto& h : d.menu.Hits()) {
                if (h.t.id == Id::DynamicResolution) {
                    return h.t.index;
                }
            }
        }
        return -100;
    };
    CHECK(hit(1) == kLabelIndex);
    CHECK(!ExplainLock(Lock::AtLowest).empty());
    CHECK(!ExplainLock(Lock::InPassthrough).empty()); // AzaharFrame vis
    CHECK(hit(2) == -1);
    CHECK(hit(0) == -1); // Auto: whatever factor it works out
}

TEST_CASE("Tab menu: an Internal Resolution the menu does not offer still shows",
          "[frame_xr][tabmenu]") {
    Audit a;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Settings;
    v.section = SettingsSection::Graphics; // AzaharFrame autosave
    for (const auto& [factor, text] : std::vector<std::pair<int, std::string>>{
             {0, "Auto"}, {12, "12x"}, {4, "4x (1600x960)"}}) {
        m.internal_resolution = factor;
        bool drawn = false;
        for (float scroll = 0.0f; scroll <= 3000.0f && !drawn; scroll += kRowH) {
            v.scroll[static_cast<int>(Tab::Settings)] = scroll;
            a.Draw(m, v);
            drawn = a.Drew(text);
        }
        INFO(factor);
        CHECK(drawn);
    }
}

TEST_CASE("Tab menu: the Touchpad row says what its value does", "[frame_xr][tabmenu]") {
    Audit a;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Controls;
    for (int value = 0; value < 2; value++) {
        m.touchpad_touch = value;
        a.Draw(m, v);
        CHECK(a.Drew(TouchpadSupporting(value)));
        CHECK_FALSE(a.Drew(TouchpadSupporting(1 - value)));
        a.Check("Controls");
    }
    CHECK(TouchpadSupporting(0) != TouchpadSupporting(1));
}

TEST_CASE("Tab menu: no text overflows, with the longest strings", "[frame_xr][tabmenu]") {
    Audit a;
    const MenuModel m = Longest();
    for (int t = 0; t < static_cast<int>(Tab::Count); t++) {
        MenuView v;
        v.tab = static_cast<Tab>(t);
        if (v.tab == Tab::Mods) {
            continue; // its segments have their own tests
        }
        // AzaharFrame autosave: every Settings section
        const int sections = v.tab == Tab::Settings ? static_cast<int>(SettingsSection::Count) : 1;
        for (int sec = 0; sec < sections; sec++) {
            v.section = static_cast<SettingsSection>(sec);
            for (float scroll = 0.0f; scroll <= 3000.0f; scroll += kRowH) {
                v.scroll[static_cast<std::size_t>(t)] = scroll;
                a.Draw(m, v);
                a.Check(("tab " + std::to_string(t)).c_str());
                if (scroll >= a.menu.MaxScroll()) {
                    break;
                }
            }
        }
    }
    for (Page page : {Page::SaveSlots, Page::LoadSlots}) {
        MenuView v;
        v.page = page;
        a.Draw(m, v);
        a.Check("slots");
    }
    // Both scopes of the Settings tab, each section (AzaharFrame autosave: the scope's menu
    // button at the section row's end).
    MenuModel per_game = m;
    per_game.scope = 1;
    MenuView v;
    v.tab = Tab::Settings;
    for (int sec = 0; sec < static_cast<int>(SettingsSection::Count); sec++) {
        v.section = static_cast<SettingsSection>(sec);
        a.Draw(per_game, v);
        a.Check("Settings, Per-Game");
        v.open_menu = Id::ScopeMore; // AzaharFrame reticle: the ⋮ menu
        a.Draw(AllOwn(per_game), v);
        a.Check("Settings, the scope's menu");
        v.open_menu = Id::None;
    }
    // AzaharFrame reticle: Per-Game's count tooltip on every tab with the scope control
    for (const Tab t : {Tab::Settings, Tab::Controls, Tab::Position}) {
        MenuView sv;
        sv.tab = t;
        sv.hover = {Id::Scope, 1};
        sv.tooltip = Id::Scope;
        sv.tooltip_index = 1;
        MenuModel own = AllOwn(per_game);
        own.scope = 1;
        own.bottom_screen_reticle = 3;
        a.Draw(own, sv);
        a.Check("the scope control's Per-Game tooltip");
    }
    // The Touchpad row's longer line.
    per_game.touchpad_touch = 1;
    v.tab = Tab::Controls;
    a.Draw(per_game, v);
    a.Check("Controls, Click for Input");
    // Edit Controls on a pad with a touchpad: the Touchpad choices beside Reset to Default and
    // Done, whole.
    MenuModel edit = Longest();
    EditDevice frame;
    frame.name = "Frame Controllers";
    EditDevice pad;
    pad.name = "DualShock 4";
    pad.kind = DeviceKind::Gamepad;
    pad.names = ButtonNames::DualShock;
    pad.has_touchpad = true;
    edit.devices.list = {frame, pad};
    edit.devices.selected = 1;
    v = {};
    v.tab = Tab::Controls;
    v.remap.editing = true;
    for (int value = 0; value < 2; value++) {
        edit.touchpad_touch = value;
        a.Draw(edit, v);
        CHECK(a.Drew("Tap for Input"));
        CHECK(a.Drew("Click for Input"));
        a.Check("Edit Controls");
    }
}

// AzaharFrame mouse: the one-time toasts fit one line of a toast in the large font, by lane
// menuux's toast_fit.h (rows 1024 px wide, 40 px of padding each side; the box sizes to its
// text), and the font has the middle dot.
TEST_CASE("Toasts: the mouse and the reticle's one-time toasts fit", "[frame_xr][tabmenu]") {
    namespace ui = frame_xr::ui;
    ui::InitFonts(); // alone, no other test has loaded them
    const float dot = ui::TextWidth(ui::kFontLarge, "\xC2\xB7");
    CHECK(dot != ui::TextWidth(ui::kFontLarge, "?"));
    constexpr float kOneLine =
        static_cast<float>(FrameXr::ToastFit::kAtlasW) - 2.0f * FrameXr::ToastFit::kPadPx;
    CHECK(ui::TextWidth(ui::kFontLarge, FrameXr::Input::kReticleHint) <= kOneLine);
    CHECK(ui::TextWidth(ui::kFontLarge, FrameXr::Input::kReticleHeadHint) <= kOneLine); // input2
    CHECK(ui::TextWidth(ui::kFontLarge, FrameXr::Input::kMouseHint) <= kOneLine);
}

// AzaharFrame ui2: Add Cheat and Install from File in the Cheats segment, with a list and without,
// and Install from Zip's browser over the segment for a cheat file.
TEST_CASE("Tab menu: Cheats add and install from a file", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Cheats;
    CHECK(LabelFor(Id::AddCheat) == "Add Cheat");
    CHECK(LabelFor(Id::InstallCheatFile) == "Install from File");
    d.Draw(m, v); // no cheats
    CHECK(HasHit(d, Id::AddCheat, -1));
    CHECK(HasHit(d, Id::DownloadCheats, -1));
    CHECK(HasHit(d, Id::InstallCheatFile, -1));
    CHECK(HasHit(d, Id::CheatsWindow, -1));
    CheckTargets(d);
    m.mods.cheats.cheats = {{"Infinite Hearts", CheatSource::User, true, ""}};
    d.Draw(m, v);
    REQUIRE(FindHit(d, Id::AddCheat, -1));
    REQUIRE(FindHit(d, Id::InstallCheatFile, -1));
    CHECK(FindHit(d, Id::AddCheat, -1)->r.x0 > FindHit(d, Id::DownloadCheats, -1)->r.x1);
    // At the bar's end, Azahar's Window before it.
    CHECK(FindHit(d, Id::InstallCheatFile, -1)->r.x0 > FindHit(d, Id::CheatsWindow, -1)->r.x1);
    CHECK(FindHit(d, Id::CheatsWindow, -1)->r.x0 > FindHit(d, Id::AddCheat, -1)->r.x1);
    CheckTargets(d);
    // The browser over the segment, not over the others.
    m.mods.zip.open = true;
    m.mods.zip.target = ZipTarget::Cheats;
    m.mods.zip.browse = true;
    m.mods.zip.entries = {{"codes.txt", false, "1 KB"}};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ZipBack, -1));
    CHECK(HasHit(d, Id::ZipEntry, 0));
    CHECK(!HasHit(d, Id::Cheat, 0));
    v.mods_segment = ModsSegment::Mods;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::ZipBack, -1));
}

// AzaharFrame zipinstall: Install from Zip on Download Packs and Install Mods, and its page.
TEST_CASE("Tab menu: Install from Zip", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Textures;
    CHECK(LabelFor(Id::InstallFromZip) == "Install from Zip");

    // AzaharFrame ui2: not on the Textures page itself, with or without packs: Download Packs
    // leads to the page that has it, as Install Mods does.
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::InstallFromZip, static_cast<int>(ZipTarget::Textures)));
    CHECK(HasHit(d, Id::DownloadPacks, -1));
    m.mods.textures = Packs(2);
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::InstallFromZip, static_cast<int>(ZipTarget::Textures)));
    CHECK(HasHit(d, Id::DownloadPacks, -1));
    // Download Packs: at the bar's end, empty list or not, where Install Mods has it.
    v.textures_page = TexturesPage::Download;
    d.Draw(m, v);
    const auto* tz = FindHit(d, Id::InstallFromZip, static_cast<int>(ZipTarget::Textures));
    REQUIRE(tz);
    const MenuLayout::Hit tex_zip = *tz;
    CHECK(HasHit(d, Id::TexturesBack, -1));
    CheckTargets(d);
    {
        MenuModel mi = m;
        MenuView vi = v;
        vi.mods_segment = ModsSegment::Mods;
        vi.mods_page = ModsPage::Install;
        mi.mods.mods.catalog = CatalogState::Ready;
        d.Draw(mi, vi);
        const auto* mz = FindHit(d, Id::InstallFromZip, static_cast<int>(ZipTarget::Mods));
        REQUIRE(mz);
        CHECK(mz->r.x0 == tex_zip.r.x0);
        CHECK(mz->r.x1 == tex_zip.r.x1);
        CHECK(mz->r.y0 == tex_zip.r.y0);
        CHECK(mz->r.y1 == tex_zip.r.y1);
    }

    // the browser over the Textures page
    ZipModel& z = m.mods.zip;
    z.open = true;
    z.target = ZipTarget::Textures;
    z.browse = true;
    z.folder = "/home/u/Downloads";
    z.can_go_up = true;
    z.chips = {{"Downloads", "/home/u/Downloads"}, {"Documents", "/home/u/Documents"}, {"Home", "/home/u"}};
    for (int i = 0; i < 20; i++) {
        z.entries.push_back({"Pack " + std::to_string(i) + ".zip", false, "12 MB"});
    }
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::TexturePack, 0));
    CHECK(HasHit(d, Id::ZipBack, -1));
    CHECK(HasHit(d, Id::ZipChip, 0));
    CHECK(HasHit(d, Id::ZipChip, 2));
    CHECK(HasHit(d, Id::ZipUp, -1));
    CHECK(HasHit(d, Id::ZipEntry, 0));
    CHECK(!HasHit(d, Id::ZipEntry, 19)); // scrolled away
    CHECK(d.menu.MaxScroll() > 0.0f);
    CheckTargets(d);
    v.scroll[static_cast<int>(kModsTab)] = 10000.0f;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ZipEntry, 19));
    v.scroll[static_cast<int>(kModsTab)] = 0.0f;

    // only on its own segment
    v.mods_segment = ModsSegment::Mods;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::ZipBack, -1));
    v.mods_segment = ModsSegment::Textures;

    // working: no way back, no list
    z.working = true;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::ZipBack, -1));
    CHECK(!HasHit(d, Id::ZipEntry, 0));
    z.working = false;

    // several games: choose one
    z.choices = {"0004000000033500", "0004000000033600"};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ZipChoice, 0));
    CHECK(HasHit(d, Id::ZipChoice, 1));
    CHECK(!HasHit(d, Id::ZipEntry, 0));
    z.choices.clear();

    // a question is modal
    z.dialog = {ModsDialogKind::Replace, "Pack 1", "Pack 1"};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::DialogConfirm, -1));
    CHECK(HasHit(d, Id::DialogCancel, -1));
    CHECK(d.menu.HitTest(kPanelW * 0.5f, 140.0f).id != Id::ZipEntry);
    z = {};

    // Install Mods' bar
    v.mods_segment = ModsSegment::Mods;
    v.mods_page = ModsPage::Install;
    m.mods.mods.catalog = CatalogState::Ready;
    d.Draw(m, v);
    REQUIRE(FindHit(d, Id::InstallFromZip, static_cast<int>(ZipTarget::Mods)));
    CHECK(FindHit(d, Id::InstallFromZip, 1)->r.H() >= 48.0f);
    m.mods.mods.busy = true; // not while an install runs
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::InstallFromZip, 1));
}

// AzaharFrame zipinstall: Download Packs rows: the verified badge and "<author> · Updated <Mon YYYY>",
// which fits beside the row's button for the catalog's longest author.
TEST_CASE("Tab menu: Download Packs dates fit", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Textures;
    v.textures_page = TexturesPage::Download;
    const std::string author = "Henriko Magnifico"; // the longest in catalog/textures.json
    m.mods.textures.downloads = {
        {"a", "Ocarina of Time 3D 4K", author, "", "", PackOfferState::Download, -1, "", "Updated Sep 2026", true},
        {"b", "Ocarina of Time 3D 1080p", author, "", "", PackOfferState::Downloading, 42, "Downloading",
         "Updated Sep 2026", true}};
    m.mods.textures.downloading = true;
    d.Draw(m, v);
    const auto* row = FindHit(d, Id::PackDownload, kLabelIndex);
    REQUIRE(row);
    const float x0 = row->r.x0 + 12.0f;
    constexpr float kNameGap = 24.0f;
    const auto line = [&](const std::string& rest) {
        return d.kit.TextWidthSized(14.0f, false, author + " · " + rest);
    };
    // Download: the button (disabled while another downloads, so measured by its label)
    const float button = d.kit.TextWidth(Type::Label, "Download") + 48.0f;
    CHECK(line("Updated Sep 2026") <= (row->r.x1 - 12.0f - button - kNameGap) - x0);
    // Downloading: Cancel, the percent and the ring
    const auto* cancel = FindHit(d, Id::PackCancel, 1);
    REQUIRE(cancel);
    const float right = cancel->r.x0 - 4.0f - d.kit.TextWidth(Type::Label, "100%") - 12.0f - 24.0f - kNameGap;
    CHECK(line("Downloading") <= right - x0);
    CHECK(line("Updated Sep 2026") <= right - x0); // even the longer text would fit
}

// AzaharFrame emerald: the Homebrew tab (a segment of the Mods tab until ui3).
TEST_CASE("Tab menu: the Homebrew tab", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Homebrew;
    const auto has = [&d](Id id, int index) {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                CHECK(h.r.H() >= 47.9f);
                return true;
            }
        }
        return false;
    };
    HomebrewItem item;
    item.id = "emerald3ds";
    item.title = "Pokémon Emerald 3DS";
    item.author = "ZallaxDev";

    // No items: the empty state, no segmented button (its own tab now).
    d.Draw(m, v);
    CHECK(!has(Id::ModsSegment, 0));
    CHECK(!has(Id::HomebrewInstall, 0));

    // Not installed: Install only.
    m.homebrew.items = {item};
    d.Draw(m, v);
    CHECK(!has(Id::ModsSegment, 0));
    CHECK(has(Id::HomebrewInstall, 0));
    CHECK(!has(Id::HomebrewPlay, 0));
    CHECK(!has(Id::HomebrewUpdate, 0));
    CHECK(!has(Id::HomebrewReinstall, 0));
    CHECK(HomebrewButtons(m.homebrew.items[0]) == std::vector<Id>{Id::HomebrewInstall});
    CHECK(HomebrewLine(m.homebrew.items[0]) == "ZallaxDev");
    // The row starts where the other tabs' content does (the pane's padding).
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id == Id::HomebrewInstall) {
            CHECK(h.r.y0 >= 12.0f + 24.0f);
        }
    }

    // Installed and current (AzaharFrame ui3): Play, Check for Updates and the quieter Reinstall;
    // with a newer release: Play and Update.
    m.homebrew.items[0].installed = true;
    m.homebrew.items[0].author = "ZallaxDev \u00b7 English";
    m.homebrew.items[0].version = "v0.5.0";
    d.Draw(m, v);
    CHECK(has(Id::HomebrewPlay, 0));
    CHECK(has(Id::HomebrewCheck, 0));
    CHECK(has(Id::HomebrewReinstall, 0));
    CHECK(!has(Id::HomebrewInstall, 0));
    CHECK(!has(Id::HomebrewUpdate, 0));
    CHECK(HomebrewButtons(m.homebrew.items[0]) ==
          std::vector<Id>{Id::HomebrewPlay, Id::HomebrewCheck, Id::HomebrewReinstall});
    CHECK(HomebrewLine(m.homebrew.items[0]) == "ZallaxDev \u00b7 English \u00b7 v0.5.0");
    CHECK(HomebrewCheckLabel(m.homebrew.items[0]) == "Check for Updates");
    // Checking: "Checking" with its ring, not pressable; Reinstall stays.
    m.homebrew.items[0].checking = true;
    d.Draw(m, v);
    CHECK(!has(Id::HomebrewCheck, 0));
    CHECK(has(Id::HomebrewReinstall, 0));
    CHECK(HomebrewCheckLabel(m.homebrew.items[0]) == "Checking");
    // No newer release: when it was checked.
    m.homebrew.items[0].checking = false;
    m.homebrew.items[0].checked = "Checked Just Now";
    CHECK(HomebrewLine(m.homebrew.items[0]) ==
          "ZallaxDev \u00b7 English \u00b7 v0.5.0 \u00b7 Checked Just Now");
    // A newer release: Update, and the new version on the row.
    m.homebrew.items[0].update = true;
    m.homebrew.items[0].latest = "v0.6.0";
    d.Draw(m, v);
    CHECK(has(Id::HomebrewPlay, 0));
    CHECK(has(Id::HomebrewUpdate, 0));
    CHECK(!has(Id::HomebrewCheck, 0));
    CHECK(!has(Id::HomebrewReinstall, 0));
    CHECK(HomebrewButtons(m.homebrew.items[0]) ==
          std::vector<Id>{Id::HomebrewPlay, Id::HomebrewUpdate});
    CHECK(HomebrewLine(m.homebrew.items[0]) ==
          "ZallaxDev \u00b7 English \u00b7 v0.5.0 \u00b7 v0.6.0 Available");

    // Busy: no buttons until it ends.
    m.homebrew.items[0].busy = true;
    m.homebrew.items[0].progress = 420;
    m.homebrew.items[0].status = "Downloading";
    d.Draw(m, v);
    CHECK(!has(Id::HomebrewPlay, 0));
    CHECK(!has(Id::HomebrewUpdate, 0));
    CHECK(HomebrewButtons(m.homebrew.items[0]).empty());
    CHECK(HomebrewLine(m.homebrew.items[0]) == "Downloading 42%");
    CHECK(d.menu.MaxScroll() == 0.0f);
    // A failure: the line is the error once it is no longer busy.
    m.homebrew.items[0].busy = false;
    m.homebrew.items[0].status = "Download Failed";
    CHECK(HomebrewLine(m.homebrew.items[0]) == "Download Failed");

    CHECK(LabelFor(Id::HomebrewInstall) == "Install");
    CHECK(LabelFor(Id::HomebrewUpdate) == "Update");
    CHECK(LabelFor(Id::HomebrewReinstall) == "Reinstall");
    CHECK(LabelFor(Id::HomebrewCheck) == "Check for Updates");
    CHECK(LabelFor(Id::HomebrewPlay) == "Play");
}

// AzaharFrame emerald: the menu's text fonts (menu/md3_fonts.h) cover the Latin-1 Supplement
// ("Pokémon") and the bullet and curly quotes; anything else draws the missing-glyph box.
TEST_CASE("Tab menu: the fonts have Latin-1 letters", "[frame_xr][tabmenu]") {
    for (const char* data : {FrameXr::Menu::Fonts::kSans400, FrameXr::Menu::Fonts::kSans500}) {
        frame_xr::ui::TtfFont font;
        REQUIRE(font.LoadBase64(data));
        for (char32_t cp = 0x20; cp < 0x7F; cp++) {
            INFO(static_cast<uint32_t>(cp));
            CHECK(font.HasGlyph(cp));
        }
        for (char32_t cp = 0xA0; cp <= 0xFF; cp++) {
            INFO(static_cast<uint32_t>(cp));
            CHECK(font.HasGlyph(cp));
        }
        for (const char32_t cp : {U'•', U'‘', U'’', U'“', U'”', U'–', U'—', U'…'}) {
            INFO(static_cast<uint32_t>(cp));
            CHECK(font.HasGlyph(cp));
        }
        CHECK_FALSE(font.HasGlyph(U'\u4E00')); // CJK stays out
    }
}

// AzaharFrame compat: entries for another version, "This May Not Work", the cheat body, the
// download buckets and the Turned Off text.
namespace {

ModOffer OtherVersionMod(std::string id, std::string name) {
    ModOffer o;
    o.id = std::move(id);
    o.name = std::move(name);
    o.author = "Author";
    o.url = "https://github.com/owner/repo";
    o.mismatch = true;
    o.region_chip = "USA Only";
    o.mismatch_reason = "Made for the USA version. Yours is the European version. It may not work.";
    o.mismatch_body = "This mod was made for the USA version of the game, and yours is the European "
                      "version. It may not work, or the game may not start.";
    return o;
}

} // namespace

TEST_CASE("Compat: download buckets and the bullet line", "[frame_xr][tabmenu][compat]") {
    CHECK(DownloadsText(0) == "Under 100 Downloads");
    CHECK(DownloadsText(99) == "Under 100 Downloads");
    CHECK(DownloadsText(100) == "100+ Downloads");
    CHECK(DownloadsText(999) == "100+ Downloads");
    CHECK(DownloadsText(1000) == "1K+ Downloads");
    CHECK(DownloadsText(4999) == "1K+ Downloads");
    CHECK(DownloadsText(5000) == "5K+ Downloads");
    CHECK(DownloadsText(9999) == "5K+ Downloads");
    CHECK(DownloadsText(10000) == "10K+ Downloads");
    CHECK(DownloadsText(49999) == "10K+ Downloads");
    CHECK(DownloadsText(50000) == "50K+ Downloads");
    CHECK(DownloadsText(5000000) == "50K+ Downloads");
}

// AzaharFrame ui3: a byline on one line, going on to a second only when it runs out of width, and
// then at a " · " (never inside a part), the author kept on the first.
TEST_CASE("Mods segments: where a byline wraps", "[frame_xr][tabmenu]") {
    // "Created by Darkshade" 150 wide, " · " 12, the parts 40, 90, 120.
    const std::vector<float> parts{40.0f, 90.0f, 120.0f};
    const float all = 150.0f + 12.0f * 3 + 40.0f + 90.0f + 120.0f; // 436
    CHECK(BylineSplit(150.0f, parts, 12.0f, 1000.0f) == 3); // short: one line
    CHECK(BylineSplit(150.0f, parts, 12.0f, all) == 3);     // exactly fits
    CHECK(BylineSplit(150.0f, parts, 12.0f, all - 1.0f) == 2); // the last part goes down whole
    CHECK(BylineSplit(150.0f, parts, 12.0f, 310.0f) == 2); // 150 + 12 + 40 + 12 + 90 = 304
    CHECK(BylineSplit(150.0f, parts, 12.0f, 250.0f) == 1);
    CHECK(BylineSplit(150.0f, parts, 12.0f, 160.0f) == 0); // the author alone, the rest below
    CHECK(BylineSplit(150.0f, {}, 12.0f, 100.0f) == 0);
    // No author: the first part stays on the first line even when cut.
    CHECK(BylineSplit(0.0f, parts, 12.0f, 30.0f) == 1);
    CHECK(BylineSplit(0.0f, parts, 12.0f, 1000.0f) == 3);
    // The text it makes: the break at a separator, the author on the first line.
    const std::vector<std::string> p{"v172", "10K+ Downloads", "Updated Oct 2026"};
    CHECK(BylineText("Darkshade", p, 3) ==
          "Created by Darkshade · v172 · 10K+ Downloads · Updated Oct 2026");
    CHECK(BylineText("Darkshade", p, 2) ==
          "Created by Darkshade · v172 · 10K+ Downloads\nUpdated Oct 2026");
    CHECK(BylineText("Darkshade", p, 0) ==
          "Created by Darkshade\nv172 · 10K+ Downloads · Updated Oct 2026");
    CHECK(BylineText("Darkshade", {"", "v172", ""}, 1) == "Created by Darkshade · v172");

    // Drawn: a short row is the one-line height, a long one (a long author and all four parts in
    // a narrow column) wraps and is taller; the link stays the author's name on the first line.
    Drawn d;
    MenuModel m = Sample();
    m.mods.mods.catalog = CatalogState::Ready;
    ModOffer s1;
    s1.id = "short";
    s1.name = "Short";
    s1.author = "Darkshade";
    s1.url = "https://gamebanana.com/members/1";
    s1.mod_version = "v172";
    ModOffer s2 = s1;
    s2.id = "long";
    s2.name = "Long";
    s2.author = "The Very Long Author Name of a GameBanana Member";
    s2.version_match = true;
    s2.downloads = "10K+ Downloads";
    s2.updated = "Updated Oct 2026";
    s2.version = "v1.0 USA";
    s2.mismatch = true;
    s2.region_chip = "EUR";
    m.mods.mods.available = {s1, s2};
    MenuView v;
    v.tab = Tab::Mods;
    v.mods_segment = ModsSegment::Mods;
    v.mods_page = ModsPage::Install;
    d.Draw(m, v);
    Rect r0{}, r1{}, l1{};
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id == Id::ModNote && h.t.index == 0) {
            r0 = h.r;
        }
        if (h.t.id == Id::ModNote && h.t.index == 1) {
            r1 = h.r;
        }
        if (h.t.id == Id::ModLink && h.t.index == 1) {
            l1 = h.r;
        }
    }
    CHECK(std::abs(r0.H() - 80.0f) < 0.01f);
    CHECK(std::abs(r1.H() - 96.0f) < 0.01f);
    REQUIRE(l1.W() > 0.0f);
    CHECK(std::abs(l1.Cy() - (r1.y0 + 50.0f)) < 0.01f); // the link on the first byline line
}

TEST_CASE("Compat: the cheat's This May Not Work body", "[frame_xr][tabmenu][compat]") {
    CHECK(CheatMismatchBody("for v1.1, installed v1.0") ==
          "This cheat was made for version 1.1, and your game is version 1.0. It may not work.");
    CHECK(CheatMismatchBody("for v1.0, this game is v1.1") ==
          "This cheat was made for version 1.0, and your game is version 1.1. It may not work.");
    CHECK(CheatMismatchBody("for Rev0, this cartridge is Rev1") ==
          "This cheat was made for revision 0, and your game is revision 1. It may not work.");
    CHECK(CheatMismatchBody("another version") ==
          "This cheat was made for another version of the game. It may not work.");
    CHECK(CheatMismatchBody("").empty());
}

TEST_CASE("Compat: This May Not Work for a mod, a pack and a cheat", "[frame_xr][tabmenu][compat]") {
    MenuModel m = Sample();
    m.mods.mods.available = {ModOffer{"fit", "Fits", "A"}, OtherVersionMod("usa", "USA Mod")};
    PackOffer p;
    p.id = "pack";
    p.name = "HD Pack";
    p.mismatch = true;
    p.mismatch_body = "This texture pack was made for the Japanese version of the game. It may not "
                      "work, or the game may not start.";
    m.mods.textures.downloads = {PackOffer{"ok", "Fine"}, p};
    m.mods.cheats.cheats = {CheatItem{"Plain"},
                            CheatItem{"Moon Jump", CheatSource::Sharkive, false, "for v1.1, installed v1.0"}};

    MenuView v;
    CHECK(CompatConfirmDialog(m, v).kind == ModsDialogKind::None);

    v.confirm = {Id::ModInstall, 1, "usa"};
    ModsDialog d = CompatConfirmDialog(m, v);
    CHECK(d.kind == ModsDialogKind::Mismatch);
    CHECK(d.mod == m.mods.mods.available[1].mismatch_body);
    CHECK(d.other == "Install Anyway");
    v.confirm = {Id::ModInstall, 0, "fit"}; // fits: nothing to ask
    CHECK(CompatConfirmDialog(m, v).kind == ModsDialogKind::None);
    v.confirm = {Id::ModInstall, 0, "usa"}; // the list moved: stale, nothing
    CHECK(CompatConfirmDialog(m, v).kind == ModsDialogKind::None);
    v.confirm = {Id::ModInstall, 7, "usa"};
    CHECK(CompatConfirmDialog(m, v).kind == ModsDialogKind::None);

    v.confirm = {Id::PackDownload, 1, "pack"};
    d = CompatConfirmDialog(m, v);
    CHECK(d.kind == ModsDialogKind::Mismatch);
    CHECK(d.mod == p.mismatch_body);
    CHECK(d.other == "Download Anyway");

    v.confirm = {Id::Cheat, 1, "Moon Jump"};
    d = CompatConfirmDialog(m, v);
    CHECK(d.kind == ModsDialogKind::Mismatch);
    CHECK(d.mod == "This cheat was made for version 1.1, and your game is version 1.0. It may not work.");
    CHECK(d.other == "Turn On Anyway");
    v.confirm = {Id::Cheat, 0, "Plain"}; // no version note: turns on without asking
    CHECK(CompatConfirmDialog(m, v).kind == ModsDialogKind::None);
    CHECK(std::string(kMayNotWorkTitle) == "This May Not Work");
    CHECK(std::string(kNotForYourVersion) == "Not for Your Version");
}

TEST_CASE("Compat: the chips, their tooltips and the dialog in the lists", "[frame_xr][tabmenu][compat]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Mods;
    v.mods_page = ModsPage::Install;
    m.mods.mods.catalog = CatalogState::Ready;
    ModOffer fit{"fit", "Fits", "A", "https://github.com/owner/fit"};
    fit.downloads = "10K+ Downloads";
    fit.updated = "Updated Mar 2026";
    m.mods.mods.available = {fit, OtherVersionMod("usa", "USA Mod")};
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::ModCompat, 0));
    REQUIRE(HasHit(d, Id::ModCompat, 1));
    CHECK(HasHit(d, Id::ModInstall, 1)); // Install still there: it asks first
    CheckTargets(d);

    // The chip's tooltip draws, the hits stay.
    v.tooltip = Id::ModCompat;
    v.tooltip_index = 1;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModCompat, 1));
    CHECK(HasHit(d, Id::ModInstall, 1));
    v.tooltip = Id::None;
    v.tooltip_index = -1;

    // The dialog is modal and has Install Anyway and Cancel.
    const auto install = FindHit(d, Id::ModInstall, 1)->r;
    v.confirm = {Id::ModInstall, 1, "usa"};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::DialogConfirm, -1));
    CHECK(HasHit(d, Id::DialogCancel, -1));
    CHECK(d.menu.HitTest(install.Cx(), install.Cy()).id != Id::ModInstall);
    CheckTargets(d);
    v.confirm = {};

    // Download Packs: the same chip.
    v.mods_segment = ModsSegment::Textures;
    v.textures_page = TexturesPage::Download;
    PackOffer p{"p", "HD Pack", "B", "https://github.com/owner/p", "1 GB"};
    p.mismatch = true;
    p.region_chip = "Japan Only";
    p.mismatch_reason = "Made for the Japanese version. It may not work.";
    p.mismatch_body = "This texture pack was made for the Japanese version of the game. It may not "
                      "work, or the game may not start.";
    m.mods.textures.downloads = {PackOffer{"ok", "Fine", "C", "https://github.com/owner/ok", "2 GB"}, p};
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::PackCompat, 0));
    CHECK(HasHit(d, Id::PackCompat, 1));
    CHECK(HasHit(d, Id::PackDownload, 1));
    CheckTargets(d);
    v.confirm = {Id::PackDownload, 1, "p"};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::DialogConfirm, -1));
    CHECK(LabelFor(Id::TurnBackOn) == "Turn Back On");
}

TEST_CASE("Compat: the Turned Off snackbar", "[frame_xr][tabmenu][compat]") {
    CHECK(TurnedOffText({"No Bloom"}) ==
          "Turned Off No Bloom: The game closed unexpectedly after it was installed.");
    CHECK(TurnedOffText({"No Bloom", "HD Textures"}) ==
          "Turned Off No Bloom and HD Textures: The game closed unexpectedly after they were installed.");
    CHECK(TurnedOffText({"A", "B", "C"}) ==
          "Turned Off A, B and C: The game closed unexpectedly after they were installed.");

    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Mods;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::TurnBackOn, -1));
    m.mods.mods.turned_off = {"Ocarina ReFramed (Single Screen)", "HD Textures"};
    m.mods.mods.turned_off_serial = 1;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::TurnBackOn, -1));
    CheckTargets(d);
    v.mods_segment = ModsSegment::Textures; // shown on the Textures segment too
    d.Draw(m, v);
    CHECK(HasHit(d, Id::TurnBackOn, -1));
}

TEST_CASE("Compat: GameBanana hasn't scanned the file yet", "[frame_xr][tabmenu][compat]") {
    MenuModel m = Sample();
    ModOffer plain{"gb", "GameBanana Mod", "A"};
    plain.unscanned = true;
    ModOffer both = OtherVersionMod("usa", "USA Mod");
    both.unscanned = true;
    m.mods.mods.available = {plain, both, OtherVersionMod("only", "Only Version")};
    PackOffer pack{"p", "Pack", "B"};
    pack.unscanned = true;
    m.mods.textures.downloads = {pack};

    MenuView v;
    v.confirm = {Id::ModInstall, 0, "gb"};
    ModsDialog d = CompatConfirmDialog(m, v);
    CHECK(d.kind == ModsDialogKind::Unscanned);
    CHECK(d.mod == "GameBanana hasn't finished checking this file for viruses. Download it anyway?");
    CHECK(d.other == "Download Anyway");
    CHECK(CompatConfirmFlags(m, v) == kAnywayUnscanned);

    // both: one dialog, the version reason first
    v.confirm = {Id::ModInstall, 1, "usa"};
    d = CompatConfirmDialog(m, v);
    CHECK(d.kind == ModsDialogKind::Mismatch);
    CHECK(d.mod == both.mismatch_body + " GameBanana also hasn't finished checking this file for viruses.");
    CHECK(d.other == "Install Anyway");
    CHECK(CompatConfirmFlags(m, v) == (kAnywayOtherVersion | kAnywayUnscanned));

    v.confirm = {Id::ModInstall, 2, "only"};
    CHECK(CompatConfirmFlags(m, v) == kAnywayOtherVersion);
    v.confirm = {Id::ModInstall, 2, "stale"};
    CHECK(CompatConfirmFlags(m, v) == 0);

    v.confirm = {Id::PackDownload, 0, "p"};
    CHECK(CompatConfirmDialog(m, v).kind == ModsDialogKind::Unscanned);
    CHECK(CompatConfirmFlags(m, v) == kAnywayUnscanned);

    // drawn: the title, Download Anyway and Cancel; modal
    Drawn dr;
    MenuView dv;
    dv.tab = kModsTab;
    dv.mods_segment = ModsSegment::Mods;
    dv.mods_page = ModsPage::Install;
    m.mods.mods.catalog = CatalogState::Ready;
    m.mods.mods.dialog = {ModsDialogKind::Unscanned, "GameBanana Mod", "Download Anyway"}; // after a refusal
    dr.Draw(m, dv);
    CHECK(HasHit(dr, Id::DialogConfirm, -1));
    CHECK(HasHit(dr, Id::DialogCancel, -1));
    CheckTargets(dr);
    m.mods.mods.dialog = {};
    dr.Draw(m, dv);
    REQUIRE(FindHit(dr, Id::ModInstall, 0));
    const auto install = FindHit(dr, Id::ModInstall, 0)->r;
    m.mods.mods.dialog = {ModsDialogKind::Unscanned, "GameBanana Mod", "Download Anyway"};
    dr.Draw(m, dv);
    CHECK(dr.menu.HitTest(install.Cx(), install.Cy()).id != Id::ModInstall); // modal
    CHECK(std::string(kUnscannedTitle) == "GameBanana Hasn't Scanned This File Yet");
}

// AzaharFrame texlayers: Enabled Textures, with two packs on: the entries under the packs, a
// checkbox for a part, none for a whole pack, Move Up and Move Down; Preload after them.
TEST_CASE("Tab menu: enabled textures", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Textures;
    m.mods.textures = Packs(2);
    m.mods.textures.packs[0].active = true;
    m.mods.textures.layers = {
        {"Gen 1", "Modern Pokemon Sprites · Replaces 120 Textures From 5X Redrawn UI HD", 2, false, true},
        {"5X Redrawn UI HD", "2,014 Textures", -1, true, true},
        {"Gen 2", "Modern Pokemon Sprites", 0, true, false}};
    d.Draw(m, v);
    CheckTargets(d);
    REQUIRE(HasHit(d, Id::TextureLayer, 0));
    CHECK(!HasHit(d, Id::TextureLayer, 1)); // a whole pack: no checkbox
    CHECK(!HasHit(d, Id::LayerUp, 0));      // the top cannot move up
    REQUIRE(HasHit(d, Id::LayerDown, 0));
    REQUIRE(HasHit(d, Id::LayerUp, 1));
    REQUIRE(HasHit(d, Id::LayerDown, 1));
    CHECK(!HasHit(d, Id::LayerDown, 2));
    CHECK(LabelFor(Id::LayerUp) == "Move Up");
    CHECK(LabelFor(Id::LayerDown) == "Move Down");
    // under the packs, over Preload
    CHECK(FindHit(d, Id::TexturePack, 1)->r.y1 <= FindHit(d, Id::TextureLayer, 0)->r.y0);
    CHECK(FindHit(d, Id::LayerDown, 0)->r.x1 <= FindHit(d, Id::TextureLayer, 0)->r.x1 + 0.01f);
    v.scroll[static_cast<int>(kModsTab)] = 10000.0f;
    d.Draw(m, v);
    REQUIRE(HasHit(d, Id::TexturePreload, -1));
    CHECK(FindHit(d, Id::TextureLayer, 2)->r.y1 <= FindHit(d, Id::TexturePreload, -1)->r.y0);
    // Custom Textures off: no changes
    m.mods.textures.custom_textures = false;
    v.scroll[static_cast<int>(kModsTab)] = 0.0f;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::TextureLayer, 0));
    CHECK(!HasHit(d, Id::LayerDown, 0));
}

// AzaharFrame texrow: the owner's page (two packs on, the snackbar, seven entries): the entry cut
// by the list's bottom shows its top part, so Enabled Textures never looks empty.
TEST_CASE("Tab menu: enabled textures under the fold", "[frame_xr][tabmenu][texrow]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Textures;
    m.mods.textures = Packs(2);
    m.mods.textures.packs[0].active = true;
    m.mods.textures.packs[1].active = true;
    m.mods.textures.needs_restart = true;
    for (int g = 1; g <= 6; g++) {
        m.mods.textures.layers.push_back({"Gen " + std::to_string(g), "Modern Pokemon Sprites", 2,
                                          g > 1, true});
    }
    m.mods.textures.layers.push_back({"5X Redrawn UI HD", "2,012 Textures", -1, true, false});
    d.Draw(m, v);
    CheckTargets(d);
    Audit a;
    a.Draw(m, v);
    a.Check("enabled textures under the fold");
    // The entry under the last whole one is drawn, cut by the list's bottom; it takes clicks once
    // scrolled into view
    int whole = -1;
    for (int i = 0; i < 7; i++) {
        if (HasHit(d, Id::TextureLayer, i)) {
            whole = i;
        }
    }
    REQUIRE(whole < 5);
    CHECK(a.Drew("Gen " + std::to_string(whole + 2)));
    CHECK(!HasHit(d, Id::TextureLayer, whole + 1));
    CHECK_FALSE(a.Drew("Gen " + std::to_string(whole + 3)));
    v.scroll[static_cast<int>(kModsTab)] = 10000.0f;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::TextureLayer, 5));
}

// AzaharFrame texparts: Customize on a pack row, and the pack's Customize page.
TEST_CASE("Tab menu: texture pack parts", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Textures;
    m.mods.textures = Packs(3);
    m.mods.textures.packs[1].customize = true;
    m.mods.textures.packs[2].customize = true;
    m.mods.textures.packs[2].downloaded = true;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::PackCustomize, 0));
    REQUIRE(HasHit(d, Id::PackCustomize, 1));
    REQUIRE(HasHit(d, Id::PackCustomize, 2));
    // beside Delete, not over it, and inside the pack's row
    CHECK(FindHit(d, Id::PackCustomize, 2)->r.x1 <= FindHit(d, Id::PackDelete, 2)->r.x0 + 0.01f);
    CHECK(FindHit(d, Id::PackCustomize, 1)->r.y0 >= FindHit(d, Id::TexturePack, 1)->r.y0 - 0.01f);
    CheckTargets(d);
    CHECK(LabelFor(Id::PackCustomize) == "Customize");

    // The page: a checkbox part with order buttons, a folder and its part, a choice.
    v.textures_page = TexturesPage::Parts;
    TexturePartsModel& p = m.mods.textures.parts;
    p.pack_id = "p1";
    p.title = "4K";
    p.author = "Henriko Magnifico";
    p.rest = "6.0.0 · 2,034 Textures On · 5.0 GB";
    p.all_buttons = true;
    p.note = true;
    p.rows.resize(6);
    p.rows[0] = {PartRowKind::Check, "Effects", "52 Textures · 73 MB · Conflicts with UI"};
    p.rows[0].arrows = true;
    p.rows[0].down = true;
    p.rows[1] = {PartRowKind::Folder, "Characters", "291 Textures · 263 MB"};
    p.rows[2] = {PartRowKind::Check, "Sprites", "9 Textures · 1.8 MB"};
    p.rows[2].child = true;
    p.rows[3] = {PartRowKind::ChoiceTitle, "Fonts"};
    p.rows[4] = {PartRowKind::ChoiceOff, "Off"};
    p.rows[5] = {PartRowKind::Choice, "Original N64", "1 Texture · 178 KB"};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::PartsBack, -1));
    CHECK(HasHit(d, Id::PartsAllOn, -1));
    CHECK(HasHit(d, Id::PartsAllOff, -1));
    for (const int i : {0, 1, 2, 4}) {
        CHECK(HasHit(d, Id::TexturePart, i));
    }
    {
        // under the list's end: cut by it (AzaharFrame scroll: a part row shows)
        const MenuLayout::Hit* h = FindHit(d, Id::TexturePart, 5);
        CHECK((h == nullptr || h->r.y1 >= d.menu.Layout().bar.list.y1 - 0.01f));
    }
    v.scroll[static_cast<int>(kModsTab)] = 10000.0f;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::TexturePart, 5));
    v.scroll[static_cast<int>(kModsTab)] = 0.0f;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::TexturePart, 3)); // a choice's title is not a button
    CHECK(!HasHit(d, Id::PartUp, 0));      // the top row cannot move up
    REQUIRE(HasHit(d, Id::PartDown, 0));
    CHECK(!HasHit(d, Id::PartUp, 1)); // no conflict, no order buttons
    CHECK(FindHit(d, Id::PartDown, 0)->r.x1 <= FindHit(d, Id::TexturePart, 0)->r.x1 + 0.01f);
    CHECK(FindHit(d, Id::TexturePart, 0)->r.y1 <= FindHit(d, Id::TexturePart, 1)->r.y0 + 0.01f);
    CHECK(!HasHit(d, Id::TexturePack, 0)); // the pack list is not under it
    CheckTargets(d);
    // A choice title with order buttons is a full row.
    p.rows[3].arrows = true;
    p.rows[3].up = true;
    d.Draw(m, v);
    REQUIRE(HasHit(d, Id::PartUp, 3));
    CheckTargets(d);
    // Loading: no rows, no All On / All Off.
    p.loading = true;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::TexturePart, 0));
    CHECK(!HasHit(d, Id::PartsAllOn, -1));
    CHECK(HasHit(d, Id::PartsBack, -1));
    // Many rows: the list scrolls, whole rows only.
    p.loading = false;
    for (int i = 0; i < 20; i++) {
        p.rows.push_back({PartRowKind::Check, "Part " + std::to_string(i), "1 Texture · 1 KB"});
    }
    d.Draw(m, v);
    CHECK(d.menu.MaxScroll() > 0.0f);
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id == Id::TexturePart) {
            CHECK(h.r.y1 <= kListBottom + 0.01f);
        }
    }
}

// AzaharFrame ui3: Optimize for Frame, the pack-level row on a pack's page, its dialog and the
// snackbar that points to it. No colons anywhere (label and value rows instead).
TEST_CASE("Tab menu: Optimize for Frame", "[frame_xr][tabmenu]") {
    // The row's text, state by state.
    PackOptimize o;
    CHECK(OptimizeText(o).title.empty());
    CHECK(OptimizeText(o).button == 0);
    o.state = OptimizeState::Png;
    o.size_text = "4.7 GB";
    OptimizeRowText t = OptimizeText(o);
    CHECK(t.title == "PNG · 4.7 GB");
    CHECK(t.line == kOptimizeLine);
    CHECK(t.button == 1);
    o.state = OptimizeState::Checking;
    t = OptimizeText(o);
    CHECK(t.ring);
    CHECK(t.button == 0);
    o.state = OptimizeState::Converting;
    o.phase = "Preparing the Encoder";
    t = OptimizeText(o);
    CHECK(t.line == "Preparing the Encoder");
    CHECK(t.bar);
    CHECK(t.button == 2);
    o.done = 1234;
    o.total = 2308;
    o.permille = 534;
    t = OptimizeText(o);
    CHECK(t.line == "1,234 of 2,308 Textures · 53%");
    o.state = OptimizeState::Stopped;
    t = OptimizeText(o);
    CHECK(t.line == "Stopped. Optimize Again to Carry On.");
    CHECK(t.button == 1);
    CHECK_FALSE(t.bar);
    o.state = OptimizeState::Failed;
    o.error = "Not Enough Space";
    t = OptimizeText(o);
    CHECK(t.line == "Not Optimized · Not Enough Space");
    CHECK(t.line_error);
    o.state = OptimizeState::Optimized;
    o.size_text = "2.1 GB";
    t = OptimizeText(o);
    CHECK(t.title == "Optimized for Frame · ASTC · 2.1 GB");
    CHECK(t.line == "Original PNG Pack");
    CHECK(t.button == 3);
    CHECK_FALSE(t.remove); // the game still reads the original
    // AzaharFrame ui3: Remove Original next to Use Original, the original's size under the title.
    o.original_size = "4.7 GB";
    o.can_remove = true;
    t = OptimizeText(o);
    CHECK(t.line == "Original PNG Pack · 4.7 GB");
    CHECK(t.remove);
    CHECK(t.button == 3);
    // Removed: no Use Original, no Remove Original.
    o.original_removed = true;
    t = OptimizeText(o);
    CHECK(t.title == "Optimized for Frame · ASTC · 2.1 GB · Original Removed");
    CHECK(t.button == 0);
    CHECK_FALSE(t.remove);
    CHECK(t.line.empty());
    o.original_removed = false;
    CHECK(RemoveOriginalBody("4.7 GB", true) ==
          "Frees 4.7 GB. The optimized pack keeps working. To go back to the original, you'd need "
          "to download it again.");
    CHECK(RemoveOriginalBody("4.7 GB", false) ==
          "Frees 4.7 GB. The optimized pack keeps working. To go back to the original, you'd need "
          "to add it again.");
    CHECK(RemoveOriginalBody("", true).rfind("The optimized pack", 0) == 0);
    CHECK(std::string(kRemoveOriginalTitle) == "Remove the Original Pack?");
    CHECK(LabelFor(Id::RemoveOriginal) == "Remove Original");
    CHECK(CountWithCommas(0) == "0");
    CHECK(CountWithCommas(999) == "999");
    CHECK(CountWithCommas(2308) == "2,308");
    CHECK(CountWithCommas(1234567) == "1,234,567");
    CHECK(OptimizeNudgeText("4K (PNG)") == "Optimize 4K (PNG) for Frame?");
    CHECK(LabelFor(Id::OptimizePack) == "Optimize for Frame");
    CHECK(LabelFor(Id::OptimizeStop) == "Stop");
    CHECK(LabelFor(Id::UseOriginal) == "Use Original");
    CHECK(LabelFor(Id::OptimizeNudge) == "Optimize");
    for (const std::string& s : {std::string(kOptimizeLine), std::string(kOptimizeBody),
                                std::string(kOptimizeStopped), LabelFor(Id::OptimizePack)}) {
        CHECK(s.find(':') == std::string::npos);
    }

    // On the page: under the pack's line, the button; the parts list starts below the row.
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Textures;
    v.textures_page = TexturesPage::Parts;
    m.mods.textures = Packs(2);
    TexturePartsModel& p = m.mods.textures.parts;
    p.pack_id = "p1";
    p.title = "4K (PNG)";
    p.author = "Henriko Magnifico";
    p.rest = "3.2 · 2,308 Textures On · 4.7 GB";
    p.rows = {{PartRowKind::Check, "Effects", "52 Textures · 73 MB"},
              {PartRowKind::Check, "UI", "40 Textures · 20 MB"}};
    p.optimize.state = OptimizeState::Png;
    p.optimize.size_text = "4.7 GB";
    d.Draw(m, v);
    REQUIRE(HasHit(d, Id::OptimizePack, -1));
    REQUIRE(HasHit(d, Id::TexturePart, 0));
    CHECK(FindHit(d, Id::TexturePart, 0)->r.y0 > FindHit(d, Id::OptimizePack, -1)->r.y1);
    CheckTargets(d);
    p.optimize.state = OptimizeState::Converting;
    p.optimize.total = 2308;
    p.optimize.done = 10;
    p.optimize.permille = 4;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::OptimizeStop, -1));
    CHECK(!HasHit(d, Id::OptimizePack, -1));
    p.optimize.state = OptimizeState::Optimized;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::UseOriginal, -1));
    p.optimize.state = OptimizeState::None; // a KTX pack of its own: no row
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::OptimizePack, -1));
    CHECK(!HasHit(d, Id::UseOriginal, -1));

    // The dialog: Optimize and Cancel over the page.
    p.optimize.state = OptimizeState::Png;
    m.mods.textures.dialog = {ModsDialogKind::Optimize, "2,308", "About 4.7 GB"};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::DialogConfirm, -1));
    CHECK(HasHit(d, Id::DialogCancel, -1));
    { // under the scrim: the beam on the button hits the dialog's area
        const MenuLayout::Hit* b = FindHit(d, Id::OptimizePack, -1);
        REQUIRE(b != nullptr);
        CHECK(d.menu.HitTest(b->r.Cx(), b->r.Cy()).id != Id::OptimizePack);
    }
    m.mods.textures.dialog = {};

    // The snackbar on the pack list, with the restart one under it.
    v.textures_page = TexturesPage::Packs;
    m.mods.textures.nudge_pack = "p1";
    m.mods.textures.nudge_name = "4K (PNG)";
    m.mods.textures.needs_restart = true;
    d.Draw(m, v);
    REQUIRE(HasHit(d, Id::OptimizeNudge, -1));
    REQUIRE(HasHit(d, Id::RestartGame, -1));
    CHECK(FindHit(d, Id::OptimizeNudge, -1)->r.y1 <= FindHit(d, Id::RestartGame, -1)->r.y0);
    CheckTargets(d);
    v.textures_page = TexturesPage::Parts; // not over the pack's own page
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::OptimizeNudge, -1));
}

// AzaharFrame savestate: a state from another build is dimmed with "From 1.1.1" in place of its
// date, still tappable; the About and mismatch notices lie over everything, and only their
// buttons are hit.
TEST_CASE("Tab menu: save states from another build, and the notice", "[frame_xr][tabmenu]") {
    Audit a;
    MenuModel m = Sample();
    m.slots = {{1, "From 1.1.1", true}, {3, "2026-11-09 19:02"}};
    MenuView v;
    v.page = Page::LoadSlots;
    a.Draw(m, v);
    a.Check("load slots, one from 1.1.1");
    CHECK(a.Drew("From 1.1.1"));
    CHECK(a.Drew("2026-11-09 19:02"));
    const auto centre = [&](Id id, int index = -1) -> Target {
        for (const auto& h : a.menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                return a.menu.HitTest(h.r.Cx(), h.r.Cy());
            }
        }
        return {};
    };
    CHECK(centre(Id::Slot, 1) == Target{Id::Slot, 1});

    // Saved over: a plain slot again (the list comes from the files each time)
    m.slots = {{1, "2026-11-12 10:15"}, {3, "2026-11-09 19:02"}};
    a.Draw(m, v);
    CHECK(!a.Drew("From 1.1.1"));
    CHECK(a.Drew("2026-11-12 10:15"));

    m.notice.kind = ModsDialogKind::Notice;
    m.notice.mod = "This Save State Is From an Older Version";
    m.notice.other = "It was made with Azahar for Frame 1.1.1. It may not load, and it could "
                     "corrupt this game's progress. Save states are NOT a replacement for saving "
                     "in the game.";
    m.notice.confirm = "Load Anyway";
    m.notice.cancel = "Cancel";
    // (the rows under the scrim are not checked against the dialog's lines)
    for (const Page page : {Page::LoadSlots, Page::Tabs}) {
        v.page = page;
        a.Draw(m, v);
        std::string all; // the headline wraps
        for (const auto& t : a.texts) {
            all += t.text + " ";
        }
        CHECK(all.find("This Save State Is From an Older Version") != std::string::npos);
        CHECK(all.find("Save states are NOT a replacement for saving in the game.") !=
              std::string::npos);
        CHECK(a.Drew("Load Anyway"));
        CHECK(a.Drew("Cancel"));
        CHECK(centre(Id::DialogConfirm) == Target{Id::DialogConfirm});
        CHECK(centre(Id::DialogCancel) == Target{Id::DialogCancel});
        CHECK(centre(Id::Slot, 3) != Target{Id::Slot, 3});
        CHECK(centre(Id::Continue) != Target{Id::Continue});
    }

    // About Save States: one button
    m.notice.mod = "About Save States";
    m.notice.other = "Save states are NOT a replacement for saving in the game.";
    m.notice.confirm = "Got It";
    m.notice.cancel.clear();
    a.Draw(m, v);
    CHECK(a.Drew("Got It"));
    CHECK(centre(Id::DialogConfirm) == Target{Id::DialogConfirm});
    CHECK(centre(Id::DialogCancel) == Target{});
    // AzaharFrame advfix: Couldn't Save State, its reason on a line of its own
    m.notice.mod = "Couldn't Save State";
    m.notice.other = "Not Enough Space\nThe game is still running. Try another slot, or free up "
                     "space on the Frame.";
    m.notice.confirm = "OK";
    a.Draw(m, v);
    a.Check("Couldn't Save State");
    CHECK(a.Drew("Couldn't Save State"));
    CHECK(a.Drew("Not Enough Space"));
    CHECK(!a.Drew("Not Enough Space The game is still running."));
    CHECK(centre(Id::DialogConfirm) == Target{Id::DialogConfirm});
}

// AzaharFrame autosave: Settings in three sections, each with the scope's menu button; Per-Game
// as the menu opens while a game runs.
TEST_CASE("Tab menu: Settings sections and the scope's menu button", "[frame_xr][tabmenu]") {
    CHECK(OpeningScope(true) == 1);
    CHECK(OpeningScope(false) == 0);
    CHECK(OptionsFor(Id::SettingsSection) ==
          std::vector<std::string>{"General", "Graphics", "Immersive"});
    Drawn d;
    MenuModel m = Sample();
    m.scope = 1;
    for (int sec = 0; sec < static_cast<int>(SettingsSection::Count); sec++) {
        MenuView v;
        v.tab = Tab::Settings;
        v.section = static_cast<SettingsSection>(sec);
        INFO("section " << sec);
        for (float scroll = 0.0f;; scroll += kRowH) {
            v.scroll[static_cast<int>(Tab::Settings)] = scroll;
            d.Draw(m, v);
            CHECK(HasHit(d, Id::Scope, 0)); // General too: its Saves rows are per game
            CHECK(HasHit(d, Id::Scope, 1));
            CHECK(HasHit(d, Id::SettingsSection, (sec + 1) % 3));
            for (const auto& h : d.menu.Hits()) {
                const Id id = h.t.id;
                if (id == Id::Scope || id == Id::ScopeMore || id == Id::SettingsSection ||
                    id == Id::TabButton || id == Id::None ||
                    id == Id::Scrollbar) { // AzaharFrame kiu: Immersive scrolls now
                    continue;
                }
                INFO("id " << static_cast<int>(id));
                CHECK(static_cast<int>(SectionOf(id)) == sec); // only its own rows
            }
            if (scroll >= d.menu.MaxScroll()) {
                break;
            }
        }
        // AzaharFrame reticle: the toggle's two halves, Global and Per-Game
        v.scroll[static_cast<int>(Tab::Settings)] = 0.0f;
        d.Draw(m, v);
        const MenuLayout::Hit* global = FindHit(d, Id::Scope, 0);
        REQUIRE(global != nullptr);
        CHECK(d.menu.HitTest(global->r.Cx(), global->r.Cy()) == Target{Id::Scope, 0});
        CheckTargets(d);
    }
    // In Global a game's own value is locked on General's Saves rows as anywhere else; the other
    // General rows (global only) stay usable in either scope.
    m.scope = 0;
    m.locked[static_cast<std::size_t>(Id::Autosave)] = Lock::GameValue;
    MenuView v;
    v.tab = Tab::Settings;
    // AzaharFrame integ3: scrolled down to the Saves rows (the Help group follows them now)
    for (float scroll = 0.0f; scroll < 3000.0f; scroll += kRowH) {
        v.scroll[static_cast<int>(Tab::Settings)] = scroll;
        d.Draw(m, v);
        if (HasHit(d, Id::Autosave, kLabelIndex)) {
            break;
        }
    }
    CHECK(HasHit(d, Id::Autosave, kLabelIndex));
    CHECK(!HasHit(d, Id::Autosave, -1));
    v.scroll[static_cast<int>(Tab::Settings)] = 0.0f;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::Background, 1));
}

TEST_CASE("Tab menu: the Saves group at General's end", "[frame_xr][tabmenu]") {
    CHECK(LabelFor(Id::SavesHeader) == "Saves");
    CHECK(LabelFor(Id::Autosave) == "Autosave");
    CHECK(LabelFor(Id::AutosaveEvery) == "Every");
    CHECK(LabelFor(Id::AutosaveKeep) == "Keep");
    for (Id id : {Id::Autosave, Id::AutosaveEvery, Id::AutosaveKeep}) {
        CHECK(!ExplainFor(id).empty());
        CHECK(SectionOf(id) == SettingsSection::General);
    }
    CHECK(SliderFor(Id::AutosaveEvery).max == 4.0f);
    CHECK(SliderFor(Id::AutosaveKeep).min == 1.0f);
    CHECK(SliderFor(Id::AutosaveKeep).max == 10.0f);
    CHECK(!ExplainLock(Lock::AutosaveOff).empty());

    Audit a;
    MenuModel m = Sample();
    m.scope = 1;
    m.autosave = true;
    m.autosave_every = 2;
    m.autosave_keep = 3;
    MenuView v;
    v.tab = Tab::Settings;
    // AzaharFrame integ3: scrolled until the whole Saves group shows (the Help group follows it)
    const auto saves_shown = [&a] {
        bool sw = false, keep = false;
        for (const auto& h : a.menu.Hits()) {
            sw |= h.t.id == Id::Autosave && h.t.index == -1;
            keep |= h.t.id == Id::AutosaveKeep && h.t.index == -1;
        }
        return sw && keep;
    };
    for (float scroll = 0.0f; scroll < 3000.0f; scroll += kRowH) {
        v.scroll[static_cast<int>(Tab::Settings)] = scroll;
        a.Draw(m, v);
        if (saves_shown()) {
            break;
        }
    }
    a.Check("Saves");
    CHECK(a.Drew("Saves"));
    CHECK(a.Drew("5 Min"));
    for (const char* stop : {"1", "2", "5", "10", "15"}) {
        CHECK(a.Drew(stop)); // the stops' minutes under Every's track
    }
    bool every = false, keep = false, sw = false;
    for (const auto& h : a.menu.Hits()) {
        every |= h.t.id == Id::AutosaveEvery && h.t.index == -1;
        keep |= h.t.id == Id::AutosaveKeep && h.t.index == -1;
        sw |= h.t.id == Id::Autosave && h.t.index == -1;
        CHECK(h.t.id != Id::SavesHeader); // the subheader is no target
    }
    CHECK(every);
    CHECK(keep);
    CHECK(sw);
    // The beam on Keep's track maps to 1 to 10
    for (const auto& track : a.menu.Layout().sliders) {
        if (track.id == Id::AutosaveKeep) {
            CHECK(a.menu.Layout().SliderAt(Id::AutosaveKeep, track.x0 - 10.0f) == 0.0f);
            CHECK(a.menu.Layout().SliderAt(Id::AutosaveKeep, track.x1 + 10.0f) == 1.0f);
        }
    }
    // Off: Every and Keep greyed (only their tooltip)
    m.autosave = false;
    a.Draw(m, v);
    for (const auto& h : a.menu.Hits()) {
        CHECK(!((h.t.id == Id::AutosaveEvery || h.t.id == Id::AutosaveKeep) && h.t.index != kLabelIndex));
    }
    // Every held: the stop's minutes as the value
    m.autosave = true;
    v.dragging = Id::AutosaveEvery;
    v.drag_t = 0.75f;
    a.Draw(m, v);
    CHECK(a.Drew("10 Min"));
    v.drag_t = 1.0f;
    a.Draw(m, v);
    CHECK(a.Drew("15 Min"));
}

TEST_CASE("Tab menu: Load State's Autosave card", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.page = Page::LoadSlots;
    // Off: "Autosave: Off" and Settings, which goes to the Saves rows
    d.Draw(m, v);
    CHECK(HasHit(d, Id::AutosaveSettings, -1));
    CHECK(!HasHit(d, Id::AutosaveCard, -1));
    CheckTargets(d);
    // On, none yet: nothing to tap
    m.autosave = true;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::AutosaveCard, -1));
    CHECK(!HasHit(d, Id::AutosaveSettings, -1));
    // Three: the card, then its list over the slots
    for (int i = 0; i < 3; i++) {
        m.autosaves.push_back({i == 0 ? "Just Now" : std::to_string(i * 5) + " Minutes Ago",
                               "2026-11-12 10:2" + std::to_string(6 - i), "10:2" + std::to_string(6 - i)});
    }
    d.Draw(m, v);
    REQUIRE(HasHit(d, Id::AutosaveCard, -1));
    CHECK(HasHit(d, Id::Slot, 1)); // the slots stay
    CheckTargets(d);
    v.open_menu = Id::AutosaveCard;
    d.Draw(m, v);
    for (int i = 0; i < 3; i++) {
        const MenuLayout::Hit* e = FindHit(d, Id::AutosaveEntry, i);
        REQUIRE(e != nullptr);
        CHECK(d.menu.HitTest(e->r.Cx(), e->r.Cy()) == Target{Id::AutosaveEntry, i});
    }
    CheckTargets(d);
    // Ten (Keep 10): two columns, every row a target of 48 dp or more
    m.autosaves.clear();
    for (int i = 0; i < 10; i++) {
        m.autosaves.push_back({std::to_string(i * 5 + 1) + " Minutes Ago", "2026-11-12 09:59", "09:59"});
    }
    d.Draw(m, v);
    for (int i = 0; i < 10; i++) {
        const MenuLayout::Hit* e = FindHit(d, Id::AutosaveEntry, i);
        REQUIRE(e != nullptr);
        CHECK(e->r.H() >= 48.0f);
        CHECK(e->r.y1 <= kPanelH);
    }
    // AzaharFrame advfix: Keep 10 and the menu-open autosave, eleven: still in the panel
    m.autosaves.push_back({"Just Now", "2026-11-12 10:30", "10:30"});
    d.Draw(m, v);
    for (int i = 0; i < 11; i++) {
        const MenuLayout::Hit* e = FindHit(d, Id::AutosaveEntry, i);
        REQUIRE(e != nullptr);
        CHECK(e->r.H() >= 48.0f);
        CHECK(e->r.y1 <= kPanelH);
        CHECK(d.menu.HitTest(e->r.Cx(), e->r.Cy()) == Target{Id::AutosaveEntry, i});
    }
    CheckTargets(d);
    // After a load: Undo Load under the slots, the slots still 48 dp
    v.open_menu = Id::None;
    m.undo_load = "Autosave Loaded: 10:22";
    m.slots.push_back({5, "2026-11-10 08:47"}); // the bottom row
    d.Draw(m, v);
    REQUIRE(HasHit(d, Id::UndoLoad, -1));
    CHECK(FindHit(d, Id::UndoLoad, -1)->r.y0 >= FindHit(d, Id::Slot, 5)->r.y1);
    CHECK(FindHit(d, Id::Slot, 5)->r.H() >= 48.0f);
    CheckTargets(d);
    // No text runs into another, with the longest strings
    Audit a;
    MenuModel lm = Longest();
    lm.autosave = true;
    lm.autosaves = {{"59 Minutes Ago", "2026-12-31 23:59", "23:59"},
                    {"Yesterday", "From Another Version", "From Another Version", true}};
    lm.undo_load = "Autosave Loaded: 23:59";
    a.Draw(lm, v);
    a.Check("Load State, the card");
    // AzaharFrame advfix: Undo Load after a slot's load too, the longest text
    lm.undo_load = "Quick Save Loaded: 23:59";
    a.Draw(lm, v);
    a.Check("Load State, Undo Load after Quick Load");
    CHECK(a.Drew("Quick Save Loaded: 23:59"));
    lm.undo_load = "Slot 10 Loaded: 23:59";
    a.Draw(lm, v);
    a.Check("Load State, Undo Load after a slot");
    CHECK(a.Drew("Autosave · 59 Minutes Ago"));
    lm.autosave = false;
    a.Draw(lm, v);
    a.Check("Load State, Autosave: Off");
    CHECK(a.Drew("Autosave: Off"));
}

// AzaharFrame reticle: the scope control replaced the per-row "This Game" chips (owner's design,
// round 24): Global | Per-Game at the top right of Settings, Controls and Position, ⋮ with Use
// Global Values for All, and Per-Game's count in a tooltip.
TEST_CASE("Tab menu: the scope control is the same on Settings, Controls and Position",
          "[frame_xr][tabmenu][scope]") {
    CHECK(TabHasScope(Tab::Settings));
    CHECK(TabHasScope(Tab::Controls));
    CHECK(TabHasScope(Tab::Position));
    CHECK(!TabHasScope(Tab::Main));
    CHECK(!TabHasScope(Tab::Stats));
    CHECK(!TabHasScope(Tab::Mods));
    CHECK(!TabHasScope(Tab::Homebrew));
    CHECK(OptionsFor(Id::Scope) == std::vector<std::string>{"Global", "Per-Game"});
    CHECK(LabelFor(Id::ScopeMore) == "Use Global Values for All");
    Drawn d;
    MenuModel m = Sample();
    m.scope = 1;
    Rect first_global{}, first_more{};
    bool first = true;
    for (const Tab t : {Tab::Settings, Tab::Controls, Tab::Position}) {
        MenuView v;
        v.tab = t;
        d.Draw(m, v);
        INFO("tab " << static_cast<int>(t));
        const MenuLayout::Hit* global = FindHit(d, Id::Scope, 0);
        const MenuLayout::Hit* per_game = FindHit(d, Id::Scope, 1);
        REQUIRE(global != nullptr);
        REQUIRE(per_game != nullptr);
        // ⋮ is there but greyed while nothing on the tab is per game
        const MenuLayout::Hit* more = FindHit(d, Id::ScopeMore, kLabelIndex);
        REQUIRE(more != nullptr);
        CHECK(!HasHit(d, Id::ScopeMore, -1));
        if (first) {
            first_global = global->r;
            first_more = more->r;
            first = false;
        } else {
            // the same place and size on every tab
            CHECK(std::fabs(global->r.x0 - first_global.x0) < 0.01f);
            CHECK(std::fabs(global->r.y0 - first_global.y0) < 0.01f);
            CHECK(std::fabs(more->r.x1 - first_more.x1) < 0.01f);
        }
        CHECK(global->r.x1 <= per_game->r.x0 + 0.01f);
        CHECK(per_game->r.x1 <= more->r.x0 + 0.01f);
        CHECK(d.menu.HitTest(per_game->r.Cx(), per_game->r.Cy()) == Target{Id::Scope, 1});
        if (t == Tab::Controls) {
            // Edit Controls under Per-Game's count tooltip (34 dp under the toggle), not under it
            for (const int reticle : {0, 3}) {
                MenuModel rm = m;
                rm.bottom_screen_reticle = reticle;
                Drawn dc;
                dc.Draw(rm, v);
                const MenuLayout::Hit* edit = FindHit(dc, Id::EditControls, -1);
                const MenuLayout::Hit* tp = FindHit(dc, Id::TouchpadTouch, kLabelIndex);
                REQUIRE(edit != nullptr);
                REQUIRE(tp != nullptr);
                CHECK(edit->r.y0 >= per_game->r.y1 + 34.0f + 4.0f);
                CHECK(edit->r.y1 + 8.0f <= tp->r.y0); // above the rows
            }
        }
        CheckTargets(d);
        // no tab's own control under it
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == Id::Scope || h.t.id == Id::ScopeMore || h.t.id == Id::None ||
                h.t.index == kLabelIndex || h.t.id == Id::TabButton) {
                continue;
            }
            const bool apart = h.r.x1 <= global->r.x0 || h.r.y0 >= global->r.y1 ||
                               h.r.y1 <= global->r.y0;
            INFO("id " << static_cast<int>(h.t.id));
            CHECK(apart);
        }
    }
    // No game that can have its own values: Global alone, no Per-Game, ⋮ greyed
    MenuModel none = Sample();
    none.scope = 0;
    none.locked[static_cast<std::size_t>(Id::Scope)] = Lock::NoGame;
    for (const Tab t : {Tab::Settings, Tab::Controls, Tab::Position}) {
        MenuView v;
        v.tab = t;
        d.Draw(AllOwn(none), v);
        CHECK(!HasHit(d, Id::Scope, 0));
        CHECK(!HasHit(d, Id::Scope, 1));
        CHECK(HasHit(d, Id::Scope, kLabelIndex));
        CHECK(!HasHit(d, Id::ScopeMore, -1));
        Audit a;
        a.Draw(AllOwn(none), v);
        CHECK(a.Drew("Global"));
        CHECK(!a.Drew("Per-Game"));
        CHECK(!a.Drew("Per-Game Values"));
    }
    CHECK(ExplainLock(Lock::NoGame) == "Per-Game values need a running game.");
    CHECK(ExplainLock(Lock::NoGameValues) == "Nothing on this tab is set for this game.");
}

TEST_CASE("Tab menu: no per-row chips any more", "[frame_xr][tabmenu][scope]") {
    Drawn d;
    MenuModel m = AllOwn(Sample());
    m.autosave = true;
    m.bottom_screen_reticle = 3;
    for (const int scope : {0, 1}) {
        m.scope = scope;
        for (const MenuView& v : PerGameViews()) {
            d.Draw(m, v);
            CHECK(!HasHit(d, Id::UseGlobal, -1));
            for (const auto& h : d.menu.Hits()) {
                CHECK(h.t.id != Id::UseGlobal);
            }
        }
    }
}

TEST_CASE("Tab menu: the Per-Game count and Use Global Values for All, per tab",
          "[frame_xr][tabmenu][scope]") {
    CHECK(GameValueCount(0) == "No Per-Game Values Set");
    CHECK(GameValueCount(1) == "1 Per-Game Value Set");
    CHECK(GameValueCount(2) == "2 Per-Game Values Set");
    // Global-only rows are never per game
    // (AzaharFrame kiu: Motion Controls is per game now.)
    for (Id id : {Id::Background, Id::WindowScale, Id::FaceButtons, Id::ShowControllers,
                  Id::FastForwardSpeed, Id::DynamicResolution, Id::ScreenLayout, Id::MoveScreens,
                  Id::ScreenCurve, Id::CursorSpeed}) {
        CHECK(!PerGameSetting(id));
    }
    for (Id id : {Id::Depth3d, Id::InternalResolution, Id::TextureFilter, Id::AsyncShaders,
                  Id::AccurateImageCopies, Id::ImmersiveMode, Id::MarginX, Id::MarginY, Id::GameFov,
                  Id::MatchCamera, Id::ScaleX, Id::ScaleY, Id::WorldScale, Id::FollowScreen,
                  Id::MotionClarity, // AzaharFrame flash
                  Id::Autosave,
                  Id::AutosaveEvery, Id::AutosaveKeep, Id::HideBottomScreen, Id::ScreenShape,
                  Id::TouchpadTouch, Id::BottomScreenReticle, Id::ReticleSpeed,
                  Id::ReticleAtEdge, Id::MotionControls, Id::ReticleEdgeCStick}) {
        CHECK(PerGameSetting(id));
    }
    // AzaharFrame kiugyro: C-Stick at Edge shelved: not a row.
    // AzaharFrame kiufix: and Aiming's rows (moved here from Settings, General); kiuflash's
    // Muzzle Flash with them (integ13).
    CHECK(ControlsScopeRows() ==
          std::vector<Id>{Id::TouchpadTouch, Id::BottomScreenReticle, Id::ReticleSpeed,
                          Id::ReticleAtEdge, Id::ReticleCenter, Id::ReticleStick, Id::GyroAim,
                          Id::GyroSensitivity, Id::Recoil, Id::RecoilCharged, Id::RecoilRapid,
                          Id::WeaponInHand, Id::WeaponSize, Id::WeaponScale, Id::MuzzleFlash,
                          Id::LaserSight, Id::TurnZone, Id::TurnSpeed});
    MenuModel m = Sample();
    m.scope = 1;
    for (const Tab t : {Tab::Settings, Tab::Controls, Tab::Position}) {
        CHECK(GameValueRows(m, t).empty());
    }
    m.game_value[static_cast<std::size_t>(Id::TextureFilter)] = true;
    m.game_value[static_cast<std::size_t>(Id::Depth3d)] = true;
    m.game_value[static_cast<std::size_t>(Id::GameFov)] = true;
    m.game_value[static_cast<std::size_t>(Id::AutosaveKeep)] = true;
    m.game_value[static_cast<std::size_t>(Id::ScreenShape)] = true;
    m.game_value[static_cast<std::size_t>(Id::ReticleAtEdge)] = true; // shown or not
    m.game_value[static_cast<std::size_t>(Id::BottomScreenReticle)] = true;
    m.game_value[static_cast<std::size_t>(Id::ScreenLayout)] = true; // global only: never
    // The Settings tab: its section's
    CHECK(GameValueRows(m, Tab::Settings, SettingsSection::Graphics) ==
          std::vector<Id>{Id::Depth3d, Id::TextureFilter});
    CHECK(GameValueRows(m, Tab::Settings, SettingsSection::Immersive) ==
          std::vector<Id>{Id::GameFov});
    CHECK(GameValueRows(m, Tab::Settings, SettingsSection::General) ==
          std::vector<Id>{Id::AutosaveKeep});
    CHECK(GameValueRows(m, Tab::Position) == std::vector<Id>{Id::ScreenShape});
    CHECK(GameValueRows(m, Tab::Controls) ==
          std::vector<Id>{Id::BottomScreenReticle, Id::ReticleAtEdge});
    CHECK(GameValueRows(m, Tab::Main).empty());

    Drawn d;
    struct Case {
        Tab tab;
        SettingsSection section;
        const char* count;
    };
    for (const Case& c : {Case{Tab::Settings, SettingsSection::Graphics, "2 Per-Game Values Set"},
                          Case{Tab::Settings, SettingsSection::General, "1 Per-Game Value Set"},
                          Case{Tab::Position, SettingsSection::General, "1 Per-Game Value Set"},
                          Case{Tab::Controls, SettingsSection::General, "2 Per-Game Values Set"}}) {
        INFO("tab " << static_cast<int>(c.tab) << " " << c.count);
        MenuView v;
        v.tab = c.tab;
        v.section = c.section;
        // Per-Game: no line of its own; ⋮ is the one way to the menu
        m.scope = 1;
        Audit a;
        a.Draw(m, v);
        CHECK(!a.Drew(c.count));
        d.Draw(m, v);
        int more_hits = 0;
        for (const auto& h : d.menu.Hits()) {
            more_hits += h.t == Target{Id::ScopeMore} ? 1 : 0;
        }
        CHECK(more_hits == 1); // ⋮ only
        // Pointing at Per-Game while it is picked: the count, under the toggle
        const MenuLayout::Hit* pg = FindHit(d, Id::Scope, 1);
        REQUIRE(pg != nullptr);
        MenuView tv = v;
        tv.hover = {Id::Scope, 1};
        tv.tooltip = Id::Scope;
        tv.tooltip_index = 1;
        Audit t;
        t.Draw(m, tv);
        CHECK(t.Drew(c.count));
        // Global: none on Per-Game, ⋮ still clears (the locked rows' way back)
        m.scope = 0;
        Audit g;
        g.Draw(m, tv);
        CHECK(!g.Drew(c.count));
        d.Draw(m, v);
        CHECK(HasHit(d, Id::ScopeMore, -1));
        // The ⋮ menu: one item over the rest, and no tooltip under it
        v.open_menu = Id::ScopeMore;
        m.scope = 1;
        d.Draw(m, v);
        const MenuLayout::Hit* all = FindHit(d, Id::ScopeMore, 0);
        REQUIRE(all != nullptr);
        CHECK(d.menu.HitTest(all->r.Cx(), all->r.Cy()) == Target{Id::ScopeMore, 0});
        Audit om;
        om.Draw(m, v);
        CHECK(om.Drew("Use Global Values for All"));
        m.scope = 1;
    }
    // Nothing set for this game: the tooltip says so, ⋮ greyed
    MenuModel none = Sample();
    none.scope = 1;
    for (const Tab t : {Tab::Settings, Tab::Controls, Tab::Position}) {
        MenuView v;
        v.tab = t;
        v.hover = {Id::Scope, 1};
        v.tooltip = Id::Scope;
        v.tooltip_index = 1;
        Audit a;
        a.Draw(none, v);
        CHECK(a.Drew("No Per-Game Values Set"));
        d.Draw(none, v);
        CHECK(!HasHit(d, Id::ScopeMore, -1));
        // pointing at Global shows nothing
        v.hover = {Id::Scope, 0};
        v.tooltip_index = 0;
        Audit g;
        g.Draw(none, v);
        CHECK(!g.Drew("No Per-Game Values Set"));
    }
}

// AzaharFrame reticle: Reticle Speed, a slider under Bottom Screen Reticle while it is on, its
// value as "2.0x"; greyed in Global while the game has its own value.
TEST_CASE("Tab menu: Reticle Speed under Bottom Screen Reticle", "[frame_xr][tabmenu][reticle]") {
    CHECK(LabelFor(Id::ReticleSpeed) == "Reticle Speed");
    CHECK(!ExplainFor(Id::ReticleSpeed).empty());
    const SliderRange r = SliderFor(Id::ReticleSpeed);
    CHECK(r.min == 5.0f);
    CHECK(r.max == 40.0f);
    CHECK(r.step == 1.0f);
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Controls;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::ReticleSpeed, -1)); // the reticle is off
    for (const int source : {1, 2, 3}) {
        m.bottom_screen_reticle = source;
        m.reticle_speed = 20;
        DrawControlsAt(d, m, v, kReticleRowsUp);
        const MenuLayout::Hit* speed = FindHit(d, Id::ReticleSpeed, -1);
        REQUIRE(speed != nullptr);
        const MenuLayout::Hit* reticle = FindHit(d, Id::BottomScreenReticle, 0);
        const MenuLayout::Hit* edge = FindHit(d, Id::ReticleAtEdge, 0);
        REQUIRE(reticle != nullptr);
        REQUIRE(edge != nullptr);
        CHECK(speed->r.y0 >= reticle->r.y1 - 0.01f); // under Bottom Screen Reticle
        CHECK(edge->r.y0 >= speed->r.y1 - 0.01f);    // and over At Edge
        // The track: 2.0x at (20 - 5) / 35 of the way
        float x0 = 0.0f, x1 = 0.0f;
        for (const auto& t : d.menu.Layout().sliders) {
            if (t.id == Id::ReticleSpeed) {
                x0 = t.x0;
                x1 = t.x1;
            }
        }
        REQUIRE(x1 > x0);
        const float t = d.menu.SliderAt(Id::ReticleSpeed, x1);
        CHECK(std::fabs(t - 1.0f) < 0.02f);
        Audit a;
        a.Draw(m, v);
        a.Check("Controls, Reticle Speed");
        CHECK(a.Drew("2.0x"));
        CHECK(a.Drew("Reticle Speed"));
    }
    m.reticle_speed = 5;
    Audit lo;
    lo.Draw(m, v);
    CHECK(lo.Drew("0.5x"));
    m.reticle_speed = 40;
    Audit hi;
    hi.Draw(m, v);
    CHECK(hi.Drew("4.0x"));
    // Global with the game's own value: greyed, the tooltip says why
    m.scope = 0;
    m.locked[static_cast<std::size_t>(Id::ReticleSpeed)] = Lock::GameValue;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::ReticleSpeed, -1));
    CHECK(HasHit(d, Id::ReticleSpeed, kLabelIndex));
}

// AzaharFrame fixes7: Aim Only While Pressed and Right Stick Moves Touch, switch rows under At
// Edge while the reticle is on, per game; greyed in Global while the game has its own value.
// AzaharFrame kiugyro: C-Stick at Edge (kiu) is shelved: no row, no label, no tooltip, even with
// its value on; At Edge has its row to itself again.
// AzaharFrame kiugyro: Aiming shows Off, Direct Aim, Gyro Aim; vr_gyro_aim keeps its stored values
// (0 Off, 1 Gyro, 2 Light Gun) for older builds, so the middle two trade places.
TEST_CASE("Tab menu: Aiming's names and order", "[frame_xr][tabmenu][kiugyro]") {
    CHECK(LabelFor(Id::GyroAim) == "Aiming");
    CHECK(OptionsFor(Id::GyroAim) == std::vector<std::string>{"Off", "Direct Aim", "Gyro Aim"});
    CHECK(ExplainFor(Id::GyroAim) ==
          "Made for Kid Icarus: Uprising with Darkshade's C-Stick & Gyro mod. Direct Aim puts the "
          "reticle where you point. Hold the grip to pause.");
    CHECK(AimingIndexOf(0) == 0);
    CHECK(AimingIndexOf(2) == 1); // Direct Aim is stored Light Gun
    CHECK(AimingIndexOf(1) == 2); // Gyro Aim is stored Gyro
    for (int i = 0; i < 3; i++) {
        CHECK(AimingIndexOf(AimingValueAt(i)) == i);
    }
    CHECK(AimingValueAt(1) == 2);
    CHECK(AimingValueAt(2) == 1);
    // Turn Zone and Turn Speed with Direct Aim only; Recoil whenever Aiming is not Off.
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Controls; // AzaharFrame kiumenus: Aiming is on the Controls page now
    const auto shown = [&d](Id id) {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id) {
                return true;
            }
        }
        return false;
    };
    for (const int stored : {0, 1, 2}) {
        INFO("stored " << stored);
        m.gyro_aim = stored;
        DrawControlsAt(d, m, v, 0.0f); // Aiming at the top of the list
        CHECK(shown(Id::GyroAim));
        CHECK(shown(Id::Recoil) == (stored != 0));
        // Gyro Sensitivity is right under Aiming
        const MenuLayout::Hit* aim = FindHit(d, Id::GyroAim, 0);
        const MenuLayout::Hit* sens = FindHit(d, Id::GyroSensitivity, -1);
        REQUIRE(aim != nullptr);
        REQUIRE(sens != nullptr);
        CHECK(sens->r.y0 >= aim->r.y1 - 0.01f);
        CHECK(sens->r.y0 - aim->r.y1 < 24.0f);
    }
}

// AzaharFrame kiuflash: Muzzle Flash, a game's switch under Weapon in Hand, shown only with it on.
// integ13: on the Controls tab with Aiming's other rows (lane/kiufix moved them there).
TEST_CASE("Tab menu: Muzzle Flash under Weapon in Hand", "[frame_xr][tabmenu][kiuflash]") {
    CHECK(LabelFor(Id::MuzzleFlash) == "Muzzle Flash");
    CHECK(ExplainFor(Id::MuzzleFlash) == "A flash at your weapon's tip on each shot.");
    CHECK(PerGameSetting(Id::MuzzleFlash));
    CHECK(SectionOf(Id::MuzzleFlash) == SectionOf(Id::WeaponInHand));
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Controls;
    for (const int aiming : {0, 1, 2}) {
        for (const bool weapon : {false, true}) {
            INFO("aiming " << aiming << ", weapon " << weapon);
            m.gyro_aim = aiming;
            m.weapon_in_hand = weapon;
            m.muzzle_flash = true;
            // Scrolled down until the row is on view (Aiming's rows are under the picture).
            bool row = false;
            for (float scroll = 0.0f; scroll <= 3000.0f && !row; scroll += kRowH) {
                v.scroll[static_cast<int>(Tab::Controls)] = scroll;
                d.Draw(m, v);
                row = HasHit(d, Id::MuzzleFlash, -1) && HasHit(d, Id::WeaponInHand, -1);
                if (scroll >= d.menu.MaxScroll()) {
                    break;
                }
            }
            CHECK(row == (aiming != 0 && weapon));
            if (row) {
                const MenuLayout::Hit* in_hand = FindHit(d, Id::WeaponInHand, -1);
                const MenuLayout::Hit* flash = FindHit(d, Id::MuzzleFlash, -1);
                REQUIRE(in_hand != nullptr);
                REQUIRE(flash != nullptr);
                CHECK(flash->r.y0 >= in_hand->r.y1 - 0.01f); // under it
                Audit a;
                a.Draw(m, v);
                a.Check("Controls, Muzzle Flash");
                CHECK(a.Drew("Muzzle Flash"));
            }
        }
    }
    m.scope = 1;
    m.game_value = {};
    m.game_value[static_cast<std::size_t>(Id::MuzzleFlash)] = true;
    CHECK(GameValueRows(m, Tab::Controls, SectionOf(Id::MuzzleFlash)) ==
          std::vector<Id>{Id::MuzzleFlash});
}

TEST_CASE("Tab menu: the reticle's switches", "[frame_xr][tabmenu][reticle]") {
    CHECK(LabelFor(Id::ReticleCenter) == "Aim Only While Pressed");
    CHECK(LabelFor(Id::ReticleStick) == "Right Stick Moves Touch");
    CHECK(LabelFor(Id::ReticleEdgeCStick) != "C-Stick at Edge");
    CHECK(!ExplainFor(Id::ReticleCenter).empty());
    CHECK(!ExplainFor(Id::ReticleStick).empty());
    CHECK(ExplainFor(Id::ReticleEdgeCStick).empty());
    CHECK(PerGameSetting(Id::ReticleCenter));
    CHECK(PerGameSetting(Id::ReticleStick));
    CHECK(PerGameSetting(Id::MotionControls)); // AzaharFrame kiu: per game too
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Controls;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::ReticleCenter, -1)); // the reticle is off
    CHECK(!HasHit(d, Id::ReticleStick, -1));
    for (const int source : {1, 2, 3}) {
        m.bottom_screen_reticle = source;
        m.reticle_center = source == 2;
        m.reticle_stick = source == 3;
        m.reticle_edge_cstick = true; // stored on (the earlier preset's): still no row
        DrawControlsAt(d, m, v, kReticleRowsUp);
        const MenuLayout::Hit* edge = FindHit(d, Id::ReticleAtEdge, 0);
        const MenuLayout::Hit* center = FindHit(d, Id::ReticleCenter, -1);
        const MenuLayout::Hit* stick = FindHit(d, Id::ReticleStick, -1);
        REQUIRE(edge != nullptr);
        REQUIRE(center != nullptr);
        REQUIRE(stick != nullptr);
        CHECK(!HasHit(d, Id::ReticleEdgeCStick, -1));
        CHECK(!HasHit(d, Id::ReticleEdgeCStick, kLabelIndex));
        // At Edge, then the pair, side by side
        CHECK(center->r.y0 >= edge->r.y1 - 0.01f);
        CHECK(stick->r.y0 == center->r.y0);
        CHECK(stick->r.x0 >= center->r.x1 - 0.01f);
        Audit a;
        a.Draw(m, v);
        a.Check("Controls, reticle switches");
        CHECK(a.Drew("Aim Only While Pressed"));
        CHECK(a.Drew("Right Stick Moves Touch"));
        CHECK(!a.Drew("C-Stick at Edge"));
    }
    m.scope = 0;
    m.locked[static_cast<std::size_t>(Id::ReticleStick)] = Lock::GameValue;
    d.Draw(m, v);
    CHECK(!HasHit(d, Id::ReticleStick, -1));
    CHECK(HasHit(d, Id::ReticleStick, kLabelIndex));
    CHECK(HasHit(d, Id::ReticleCenter, -1));
}

// AzaharFrame ffaudio
TEST_CASE("Tab menu: the Fast Forward Audio row", "[frame_xr][tabmenu][ffaudio]") {
    CHECK(LabelFor(Id::FastForwardAudio) == "Fast Forward Audio");
    CHECK(OptionsFor(Id::FastForwardAudio) ==
          std::vector<std::string>{"Sped Up", "Normal Pitch", "Normal Speed (Experimental)",
                                   "Muted"});
    CHECK(ExplainFor(Id::FastForwardAudio) ==
          "How the sound plays while fast forwarding. Normal Pitch keeps the pitch. Normal Speed "
          "(Experimental) plays at real speed.");
    CHECK(PerGameSetting(Id::FastForwardAudio));
    CHECK(SectionOf(Id::FastForwardAudio) == SettingsSection::General);
    MenuModel m = Sample();
    m.scope = 1;
    m.game_value[static_cast<std::size_t>(Id::FastForwardAudio)] = true;
    CHECK(GameValueRows(m, Tab::Settings, SettingsSection::General) ==
          std::vector<Id>{Id::FastForwardAudio});
    CHECK(GameValueRows(m, Tab::Settings, SettingsSection::Graphics).empty());
    // AzaharFrame kiu2: a menu button, whose value shows whole (420 wide less its padding and
    // arrow), and whose menu's items set the choice.
    Drawn d;
    MenuView v;
    v.tab = Tab::Settings;
    v.section = SettingsSection::General;
    v.scroll[static_cast<int>(Tab::Settings)] = 300.0f;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::FastForwardAudio, -1));
    CHECK(!HasHit(d, Id::FastForwardAudio, 2));
    v.open_menu = Id::FastForwardAudio;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::FastForwardAudio, 2));
    Kit kit(1.0f);
    for (const std::string& o : OptionsFor(Id::FastForwardAudio)) {
        INFO(o);
        CHECK(kit.Fit(Type::Label, o, 420.0f - 64.0f) == o);
    }
}

// AzaharFrame fixes7: Screen Shape: Automatic, the last choice (vr_top_aspect 6).
TEST_CASE("Tab menu: Screen Shape has Automatic", "[frame_xr][tabmenu][autoshape]") {
    const auto& opts = OptionsFor(Id::ScreenShape);
    REQUIRE(opts.size() == 7);
    CHECK(opts[0] == "3DS (5:3)");
    CHECK(opts[6] == "Automatic");
    CHECK(ExplainFor(Id::ScreenShape) ==
          "The top screen's shape, for games or mods that change it. Automatic follows the "
          "game's camera.");
}

// AzaharFrame kiu: a selected segment shows its check, the widest last one too, wherever the
// row sits (the equal segments' last width came out a hair short: Detect Game FOV's Always).
TEST_CASE("Tab menu: every selected segment keeps its check", "[frame_xr][tabmenu][kiu]") {
    Kit kit{1.0f};
    frame_xr::ui::Canvas canvas;
    canvas.Init(static_cast<int>(kPanelW), static_cast<int>(kPanelH));
    kit.Begin(canvas);
    const std::vector<std::vector<std::string>> rows{
        OptionsFor(Id::MatchCamera), {"Off", "Once", "Always"}, {"Off", "10%", "20%", "30%"},
        {"1x", "10x", "100x"}, OptionsFor(Id::MarginX)};
    for (const auto& opts : rows) {
        const float w = kit.SegmentedWidth(opts);
        for (float x1 = 300.0f; x1 < kPanelW; x1 += 1.37f) {
            const Rect r{x1 - w, 100.0f, x1, 140.0f};
            const auto e = kit.SegmentEdges(r, opts);
            for (int sel = 0; sel < static_cast<int>(opts.size()); sel++) {
                std::vector<DrawnText> texts;
                kit.RecordText(&texts);
                kit.Segmented(r, opts, sel, -1, -1);
                kit.RecordText(nullptr);
                const std::string& want = opts[static_cast<std::size_t>(sel)];
                const DrawnText* label = nullptr;
                for (const auto& t : texts) {
                    if (t.text == want) {
                        label = &t;
                    }
                }
                INFO(want << " at x1 " << x1);
                REQUIRE(label != nullptr); // not cut short either
                // With the check the label sits right of the segment's middle (the check before it).
                const float mid = (e[static_cast<std::size_t>(sel)] +
                                   e[static_cast<std::size_t>(sel + 1)]) * 0.5f;
                CHECK((label->box.x0 + label->box.x1) * 0.5f > mid + 6.0f);
            }
        }
    }
}

// AzaharFrame ctgp7: the CTGP-7 segment: a fourth segment only while Mario Kart 7 is open, its
// buttons for each state, and "Requires CTGP-7" over Install Mods.
TEST_CASE("Tab menu: the CTGP-7 segment", "[frame_xr][tab_menu][ctgp7]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Mods;
    const auto has = [&d](Id id, int index) {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                return true;
            }
        }
        return false;
    };
    // what the beam gets at the middle of an item (a dialog's scrim covers what is under it)
    const auto reachable = [&d](Id id, int index) {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                const Target t = d.menu.HitTest(h.r.Cx(), h.r.Cy());
                return t.id == id && t.index == index;
            }
        }
        return false;
    };
    // Another game: Cheats | Mods | Textures, as before
    CHECK(ModsSegmentLabels(false) == std::vector<std::string>{"Cheats", "Mods", "Textures"});
    CHECK(ModsSegmentLabels(true) ==
          std::vector<std::string>{"Cheats", "Mods", "Textures", "CTGP-7"});
    d.Draw(m, v);
    CHECK(has(Id::ModsSegment, 2));
    CHECK_FALSE(has(Id::ModsSegment, 3));
    // a view left on CTGP-7 for a game without it draws Cheats
    v.mods_segment = ModsSegment::Ctgp7;
    d.Draw(m, v);
    CHECK(has(Id::CheatsWindow, -1));
    CHECK_FALSE(has(Id::Ctgp7Install, -1));

    // Mario Kart 7: the fourth segment, to the right of Textures, inside the pane
    m.mods.ctgp7.shown = true;
    d.Draw(m, v);
    CHECK(has(Id::ModsSegment, 3));
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id == Id::ModsSegment) {
            CHECK(h.r.x1 <= kPanelW);
            CHECK(h.r.H() >= 48.0f);
        }
    }
    CHECK(has(Id::Ctgp7Install, -1));
    CHECK(has(Id::Ctgp7Website, -1));
    CHECK_FALSE(has(Id::Ctgp7Launch, -1));
    CHECK(Ctgp7Line(m.mods.ctgp7) == "Not Installed");
    bool error = false;
    m.mods.ctgp7.blocker = "Needs Mario Kart 7 Version 1.2";
    CHECK(Ctgp7Line(m.mods.ctgp7, &error) == "Needs Mario Kart 7 Version 1.2");
    CHECK(error);
    // busy: Cancel only, the phase's detail
    m.mods.ctgp7.busy = true;
    m.mods.ctgp7.phase = "Downloading CTGP-7";
    m.mods.ctgp7.detail = "120 MB of 767 MB";
    m.mods.ctgp7.permille = 150;
    d.Draw(m, v);
    CHECK(has(Id::Ctgp7Cancel, -1));
    CHECK_FALSE(has(Id::Ctgp7Install, -1));
    CHECK(Ctgp7Line(m.mods.ctgp7, &error) == "120 MB of 767 MB");
    CHECK_FALSE(error);
    m.mods.ctgp7.can_cancel = false;
    CHECK(Ctgp7Buttons(m.mods.ctgp7).empty());
    // installed: Launch and the version
    m.mods.ctgp7 = {};
    m.mods.ctgp7.shown = true;
    m.mods.ctgp7.installed = true;
    m.mods.ctgp7.version = "1.6.30";
    d.Draw(m, v);
    CHECK(has(Id::Ctgp7Launch, -1));
    CHECK_FALSE(has(Id::Ctgp7Install, -1));
    CHECK(Ctgp7Line(m.mods.ctgp7) == "Version 1.6.30");
    CHECK(has(Id::Ctgp7Uninstall, -1));
    CHECK(LabelFor(Id::Ctgp7Uninstall) == "Uninstall");
    CHECK(LabelFor(Id::Ctgp7Website) == "Official Website");
    // "Uninstall CTGP-7?" with its checkbox and a third button
    m.mods.ctgp7.dialog.kind = ModsDialogKind::Notice;
    m.mods.ctgp7.dialog.mod = "Uninstall CTGP-7?";
    m.mods.ctgp7.dialog.other = "This removes CTGP-7.";
    m.mods.ctgp7.dialog.confirm = "Uninstall";
    m.mods.ctgp7.dialog.cancel = "Cancel";
    m.mods.ctgp7.dialog.extra = "Uninstall Both";
    m.mods.ctgp7.dialog.check = "Also Delete CTGP-7's Progress";
    d.Draw(m, v);
    CHECK(reachable(Id::DialogCheck, -1));
    CHECK(reachable(Id::DialogExtra, -1));
    CHECK(reachable(Id::DialogConfirm, -1));
    CHECK_FALSE(reachable(Id::Ctgp7Uninstall, -1));
    m.mods.ctgp7.dialog = {};
    // a copy found on the SD card without the rest: Finish Setup
    m.mods.ctgp7.installed = false;
    m.mods.ctgp7.setup_unfinished = true;
    CHECK(Ctgp7Line(m.mods.ctgp7) == "Setup Not Finished");
    CHECK(Ctgp7Buttons(m.mods.ctgp7) == std::vector<int>{static_cast<int>(Id::Ctgp7Install)});
    m.mods.ctgp7.setup_unfinished = false;
    m.mods.ctgp7.installed = true;
    // the website asks first (the link dialog): only its buttons are hit
    v.open_link = kCtgp7Website;
    d.Draw(m, v);
    CHECK(reachable(Id::DialogConfirm, -1));
    CHECK_FALSE(reachable(Id::Ctgp7Launch, -1));
    CHECK(LinkText(kCtgp7Website) == "ctgp-7.github.io");

    // "Requires CTGP-7" over Install Mods. AzaharFrame ctgp7b: while the segment shows, a mod for
    // CTGP-7 is listed there only; Install Mods lists it while it doesn't
    v = {};
    v.tab = Tab::Mods;
    v.mods_segment = ModsSegment::Mods;
    v.mods_page = ModsPage::Install;
    m.mods.ctgp7.installed = false;
    ModOffer o;
    o.id = "mk7-midna";
    o.name = "Midna";
    o.author = "TheHumanBanana";
    o.requires_ctgp7 = true;
    m.mods.mods.available = {o};
    m.mods.mods.catalog = CatalogState::Ready;
    d.Draw(m, v);
    CHECK_FALSE(has(Id::ModInstall, 0));
    m.mods.ctgp7.shown = false;
    d.Draw(m, v);
    CHECK(has(Id::ModInstall, 0));
    m.mods.ctgp7.shown = true;
    v.requires_ctgp7 = true;
    d.Draw(m, v);
    CHECK(reachable(Id::DialogConfirm, -1));
    CHECK(reachable(Id::DialogCancel, -1));
    CHECK_FALSE(reachable(Id::ModInstall, 0));
}

// AzaharFrame fovscale: Scale Mode is Stretch | Field of View, per game, on the Immersive
// section right under Horizontal and Vertical Scale, Field of View by default; each choice with
// its own tooltip, and every tooltip on one line of the pane.
TEST_CASE("Tab menu: Scale Mode is Stretch or Field of View", "[frame_xr][tabmenu][fovscale]") {
    CHECK(LabelFor(Id::ScaleMode) == "Scale Mode");
    CHECK(SectionOf(Id::ScaleMode) == SettingsSection::Immersive);
    CHECK(PerGameSetting(Id::ScaleMode));
    CHECK(OptionsFor(Id::ScaleMode) == std::vector<std::string>{"Stretch", "Field of View"});
    CHECK(HasOptionTips(Id::ScaleMode));
    CHECK(!ExplainFor(Id::ScaleMode).empty());
    CHECK(!ExplainOption(Id::ScaleMode, 0).empty());
    CHECK(!ExplainOption(Id::ScaleMode, 1).empty());
    CHECK(ExplainOption(Id::ScaleMode, 2).empty());
    CHECK(ExplainOption(Id::ScaleMode, kLabelIndex).empty());
    CHECK(MenuModel{}.scale_mode == 1);
    {
        Kit kit{1.0f};
        for (const std::string& text :
             {ExplainFor(Id::ScaleMode), ExplainOption(Id::ScaleMode, 0),
              ExplainOption(Id::ScaleMode, 1), ExplainFor(Id::ScaleX)}) {
            INFO(text);
            CHECK(kit.TextWidth(Type::Tooltip, text) + 28.0f <= kPanelW * 0.7f);
        }
    }
    for (int pick = 0; pick < 2; pick++) {
        Drawn d;
        MenuModel m = Sample();
        m.scale_mode = pick;
        MenuView v;
        v.tab = Tab::Settings;
        v.section = SettingsSection::Immersive;
        bool seen[2] = {};
        for (float scroll = 0.0f; scroll <= 3000.0f; scroll += kRowH) {
            v.scroll[static_cast<int>(Tab::Settings)] = scroll;
            d.Draw(m, v);
            for (int i = 0; i < 2; i++) {
                seen[i] |= FindHit(d, Id::ScaleMode, i) != nullptr;
            }
        }
        INFO("picked " << pick);
        CHECK(seen[0]);
        CHECK(seen[1]);
    }
}

// AzaharFrame fov100: Horizontal and Vertical Scale are sliders from -100 % to +100 % in 5 %
// steps (-30 to +30 % before), with no tick marks (40 stops); the ends read "-100%" and "+100%"
// and fit their row.
TEST_CASE("Tab menu: the scales from -100 % to +100 %", "[frame_xr][tabmenu][fov100]") {
    for (Id id : {Id::ScaleX, Id::ScaleY}) {
        const SliderRange r = SliderFor(id);
        CHECK(r.min == -100.0f);
        CHECK(r.max == 100.0f);
        CHECK(r.step == 5.0f);
    }
    const auto text = [](int p) { return (p > 0 ? "+" : "") + std::to_string(p) + "%"; };
    for (const auto& [x, y] :
         {std::pair{-100, 100}, std::pair{100, -100}, std::pair{-100, -100}, std::pair{100, 100}}) {
        Audit a;
        MenuModel m = Sample();
        m.scale_x = x;
        m.scale_y = y;
        MenuView v;
        v.tab = Tab::Settings;
        v.section = SettingsSection::Immersive;
        bool shown = false;
        for (float scroll = 0.0f; scroll <= 3000.0f && !shown; scroll += kRowH) {
            v.scroll[static_cast<int>(Tab::Settings)] = scroll;
            a.Draw(m, v);
            bool sx = false, sy = false;
            for (const auto& h : a.menu.Hits()) {
                sx |= h.t.id == Id::ScaleX;
                sy |= h.t.id == Id::ScaleY;
            }
            shown = sx && sy;
        }
        INFO("scales " << x << ", " << y);
        REQUIRE(shown);
        CHECK(a.Drew(text(x))); // "-100%", "+100%"
        CHECK(a.Drew(text(y)));
        a.Check("the scales at their ends");
    }
}

// AzaharFrame ctgp7b: the catalog's mods for CTGP-7 ("requires": "ctgp7", the eight of the
// catalog's ctgp7 branch) are listed in the CTGP-7 segment under its status row, not in the Mods
// segment (Install Mods or the loaded list); the others (Sonic works with or without CTGP-7) stay
// in Mods. Install waits for CTGP-7; an installed one keeps its switch and Remove.
TEST_CASE("Tab menu: mods for CTGP-7 are listed in its segment", "[frame_xr][tab_menu][ctgp7]") {
    Audit d;
    const auto has = [&d](Id id, int index) {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                return true;
            }
        }
        return false;
    };
    const auto reachable = [&d](Id id, int index) {
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                const Target t = d.menu.HitTest(h.r.Cx(), h.r.Cy());
                return t.id == id && t.index == index;
            }
        }
        return false;
    };
    // the segment scrolls as a whole: reachable at some scroll of it
    const auto somewhere = [&](const MenuModel& mm, MenuView vv, Id id, int index) {
        d.Draw(mm, vv);
        const float end = d.menu.MaxScroll();
        for (float sc = 0.0f; sc <= end + 24.0f; sc += 24.0f) {
            vv.scroll[static_cast<int>(Tab::Mods)] = sc;
            d.Draw(mm, vv);
            if (reachable(id, index)) {
                return true;
            }
        }
        return false;
    };
    MenuModel m = Sample();
    m.game_title = "Mario Kart 7";
    m.mods.ctgp7.shown = true;
    const auto offer = [](const char* id, const char* name, const char* author, bool ctgp7) {
        ModOffer o;
        o.id = id;
        o.name = name;
        o.author = author;
        o.url = "https://gamebanana.com/members/1";
        o.notes = ctgp7 ? "Requires CTGP-7." : "";
        o.requires_ctgp7 = ctgp7;
        return o;
    };
    auto& mm = m.mods.mods;
    mm.available = {offer("mk7-sonic", "Sonic", "Blurro", false),
                    offer("mk7-hatsune-miku", "Hatsune Miku", "Sedral", true),
                    offer("mk7-sm64-mario", "SM64 Mario", "NintendoGameTuber", false),
                    offer("mk7-pikmin-red", "Pikmin Red", "Yasioen", true),
                    offer("mk7-pikmin-blue", "Pikmin Blue", "Yasioen", true),
                    offer("mk7-pikmin-yellow", "Pikmin Yellow", "Yasioen", true),
                    offer("mk7-pikmin-purple", "Pikmin Purple", "Yasioen", true),
                    offer("mk7-pikmin-white", "Pikmin White", "Yasioen", true),
                    offer("mk7-pikmin-mushroom", "Pikmin Mushroom (Puffmin)", "Yasioen", true),
                    offer("mk7-midna", "Midna", "TheHumanBanana", true)};
    mm.catalog = CatalogState::Ready;
    const std::vector<int> ctgp7_rows{1, 3, 4, 5, 6, 7, 8, 9};

    // which list has which row
    CHECK(InstallModsRows(mm, true) == std::vector<int>{0, 2});
    CHECK(Ctgp7ModRows(mm, true) == ctgp7_rows);
    CHECK(InstallModsRows(mm, false).size() == mm.available.size()); // no segment: Mods as before
    CHECK(Ctgp7ModRows(mm, false).empty());

    // Install Mods: Sonic and SM64 Mario only
    MenuView v;
    v.tab = Tab::Mods;
    v.mods_segment = ModsSegment::Mods;
    v.mods_page = ModsPage::Install;
    d.Draw(m, v);
    CHECK(has(Id::ModInstall, 0));
    CHECK(has(Id::ModInstall, 2));
    for (const int r : ctgp7_rows) {
        CHECK_FALSE(has(Id::ModInstall, r));
    }
    CHECK(d.Drew("Sonic"));
    CHECK_FALSE(d.Drew("Hatsune Miku"));
    // only mods for CTGP-7: "No Mods for This Game"
    {
        MenuModel only = m;
        only.mods.mods.available.erase(only.mods.mods.available.begin() + 2);
        only.mods.mods.available.erase(only.mods.mods.available.begin());
        d.Draw(only, v);
        CHECK(d.Drew("No Mods for This Game"));
    }

    // the CTGP-7 segment, CTGP-7 not installed: all eight, Install not pressable, the line
    v = {};
    v.tab = Tab::Mods;
    v.mods_segment = ModsSegment::Ctgp7;
    d.Draw(m, v);
    CHECK(d.Drew(kCtgp7ModsTitle));
    CHECK(d.Drew(kCtgp7ModsFirst));
    CHECK_FALSE(d.Drew("Sonic"));
    {
        bool miku = false;
        MenuView vv = v;
        for (float sc = 0.0f; sc <= d.menu.MaxScroll() + 24.0f; sc += 24.0f) {
            vv.scroll[static_cast<int>(Tab::Mods)] = sc;
            d.Draw(m, vv);
            miku |= d.Drew("Hatsune Miku");
        }
        CHECK(miku);
        d.Draw(m, v);
    }
    for (const int r : ctgp7_rows) {
        CHECK_FALSE(somewhere(m, v, Id::ModInstall, r));
    }
    d.Draw(m, v);
    CHECK(reachable(Id::Ctgp7Install, -1)); // the status row as before, at the top
    CHECK(d.menu.MaxScroll() > 0.0f); // eight rows under the About section: the page scrolls
    // scrolled to its end: the last one shows whole, inside the pane
    v.scroll[static_cast<int>(Tab::Mods)] = 10000.0f;
    d.Draw(m, v);
    CHECK(d.Drew("Midna"));
    for (const auto& h : d.menu.Hits()) {
        CHECK(h.r.y1 <= kPanelH);
    }
    v.scroll[static_cast<int>(Tab::Mods)] = 0.0f;

    // installed: Install, and an installed one's switch and Remove (the loaded list's index)
    m.mods.ctgp7.installed = true;
    m.mods.ctgp7.version = "1.6.30";
    ModRow sonic, miku;
    sonic.id = "mk7-sonic";
    sonic.name = "Sonic";
    sonic.managed = true;
    miku.id = "mk7-hatsune-miku";
    miku.name = "Hatsune Miku";
    miku.author = "Sedral";
    miku.managed = true;
    mm.loaded = {sonic, miku};
    mm.available[1].state = OfferState::Installed;
    d.Draw(m, v);
    CHECK_FALSE(d.Drew(kCtgp7ModsFirst));
    CHECK(has(Id::Ctgp7Launch, -1));
    CHECK(somewhere(m, v, Id::ModInstall, 3));
    CHECK(somewhere(m, v, Id::ModInstall, 9));
    CHECK_FALSE(somewhere(m, v, Id::ModInstall, 1));
    CHECK(somewhere(m, v, Id::Mod, 1));       // Hatsune Miku's switch
    CHECK(somewhere(m, v, Id::ModRemove, 1)); // and Remove
    CHECK_FALSE(somewhere(m, v, Id::Mod, 0)); // Sonic is the Mods segment's
    // an install that runs: Cancel and the ring
    mm.available[3].state = OfferState::Installing;
    mm.available[3].note = "Downloading 42%";
    mm.busy = true;
    CHECK(somewhere(m, v, Id::ModCancel, 3));
    CHECK_FALSE(somewhere(m, v, Id::ModInstall, 4)); // the others wait
    CHECK_FALSE(somewhere(m, v, Id::ModRemove, 1));
    mm.busy = false;
    mm.available[3].state = OfferState::Install;
    mm.available[3].note.clear();
    // CTGP-7 being uninstalled: its mods wait
    m.mods.ctgp7.busy = true;
    CHECK_FALSE(somewhere(m, v, Id::ModInstall, 3));
    m.mods.ctgp7.busy = false;

    // the Mods segment's loaded list: Sonic, not Hatsune Miku
    CHECK(LoadedRows(mm, true) == std::vector<int>{0});
    CHECK(LoadedRows(mm, false) == std::vector<int>{0, 1});
    v.mods_segment = ModsSegment::Mods;
    d.Draw(m, v);
    CHECK(has(Id::Mod, 0));
    CHECK_FALSE(has(Id::Mod, 1));
    CHECK_FALSE(has(Id::ModRemove, 1));
    // only Hatsune Miku installed: "No Mods" there, Install Mods to the catalog
    {
        MenuModel one = m;
        one.mods.mods.loaded = {miku};
        d.Draw(one, v);
        CHECK(d.Drew("No Mods"));
    }
    // one the catalog no longer lists stays in the Mods segment (nothing goes unlisted)
    {
        MenuModel gone = m;
        gone.mods.mods.available.erase(gone.mods.mods.available.begin() + 1);
        CHECK(LoadedRows(gone.mods.mods, true) == std::vector<int>{0, 1});
    }

    // a link and a question from a CTGP-7 row show over the segment and take the beam
    v.mods_segment = ModsSegment::Ctgp7;
    v.open_link = "https://gamebanana.com/members/1";
    d.Draw(m, v);
    CHECK(reachable(Id::DialogConfirm, -1));
    CHECK_FALSE(somewhere(m, v, Id::ModInstall, 3)); // the scrim takes the beam
    v.open_link.clear();
    mm.available[3].mismatch = true;
    mm.available[3].mismatch_body = "This mod was made for the USA version of the game.";
    v.confirm = {Id::ModInstall, 3, "mk7-pikmin-red"};
    d.Draw(m, v);
    CHECK(reachable(Id::DialogConfirm, -1));
    CHECK_FALSE(somewhere(m, v, Id::ModInstall, 4));
}

// AzaharFrame ctrlfront: Controllers in Front of Screen, a global segmented row worded as
// Detach Screens (Off, Secondary, Both) with Primary added, directly under Show Controllers and
// only while it is on; each choice with its own tooltip.
TEST_CASE("Tab menu: Controllers in Front of Screen", "[frame_xr][tabmenu]") {
    CHECK(!PerGameSetting(Id::ControllersInFront));
    CHECK(SectionOf(Id::ControllersInFront) == SettingsSection::General);
    CHECK(LabelFor(Id::ControllersInFront) == "Controllers in Front of Screen");
    CHECK(OptionsFor(Id::ControllersInFront) ==
          std::vector<std::string>{"Off", "Primary", "Secondary", "Both"});
    // The same words, in the same order, as Detach Screens.
    CHECK(OptionsFor(Id::MoveScreens) == std::vector<std::string>{"Off", "Secondary", "Both"});
    CHECK(MenuModel{}.controllers_in_front == 3); // Both, the default
    CHECK(HasOptionTips(Id::ControllersInFront));
    {
        Kit kit{1.0f};
        for (const std::string& text :
             {ExplainFor(Id::ControllersInFront), ExplainOption(Id::ControllersInFront, 0),
              ExplainOption(Id::ControllersInFront, 1), ExplainOption(Id::ControllersInFront, 2),
              ExplainOption(Id::ControllersInFront, 3)}) {
            INFO(text);
            CHECK(!text.empty());
            CHECK(kit.TextWidth(Type::Tooltip, text) + 28.0f <= kPanelW * 0.7f);
        }
    }
    CHECK(ExplainOption(Id::ControllersInFront, 4).empty());
    for (const bool shown : {true, false}) {
        for (int pick = 0; pick < 4; pick++) {
            Drawn d;
            MenuModel m = Sample();
            m.show_controllers = shown;
            m.controllers_in_front = pick;
            MenuView v;
            v.tab = Tab::Settings;
            v.section = SettingsSection::General;
            bool seen[4] = {};
            bool under_show = false, fits = false;
            for (float scroll = 0.0f; scroll <= 3000.0f; scroll += kRowH) {
                v.scroll[static_cast<int>(Tab::Settings)] = scroll;
                d.Draw(m, v);
                for (int i = 0; i < 4; i++) {
                    seen[i] |= FindHit(d, Id::ControllersInFront, i) != nullptr;
                }
                const auto* sc = FindHit(d, Id::ShowControllers, -1);
                const auto* first = FindHit(d, Id::ControllersInFront, 0);
                const auto* label = FindHit(d, Id::ControllersInFront, kLabelIndex);
                if (sc && first) {
                    // the next row down: one row height below Show Controllers
                    const float dy =
                        0.5f * ((first->r.y0 + first->r.y1) - (sc->r.y0 + sc->r.y1));
                    under_show |= std::abs(dy - kRowH) < 1.0f;
                }
                if (first && label) {
                    // The four choices beside the label, none of them cut short (the row's hit
                    // is the label's, 12 dp outside the content each side; 24 dp label gap and
                    // segment padding, as tab_menu.cpp and md3.cpp).
                    Kit kit{1.0f};
                    const auto& opts = OptionsFor(Id::ControllersInFront);
                    const float label_end =
                        label->r.x0 + 12.0f +
                        kit.TextWidth(Type::Body, LabelFor(Id::ControllersInFront)) + 24.0f;
                    const auto e =
                        kit.SegmentEdges({first->r.x0, 0.0f, label->r.x1 - 12.0f, 48.0f}, opts);
                    bool whole = first->r.x0 >= label_end;
                    for (std::size_t k = 0; k < opts.size(); k++) {
                        whole &= kit.Fit(Type::Label, opts[k], e[k + 1] - e[k] - 24.0f + 0.5f) ==
                                 opts[k];
                    }
                    fits |= whole;
                }
            }
            INFO("Show Controllers " << shown << ", picked " << pick);
            for (int i = 0; i < 4; i++) {
                CHECK(seen[i] == shown);
            }
            if (shown) {
                CHECK(under_show);
                CHECK(fits);
            }
        }
    }
}

// AzaharFrame fovaxis: Render Margin applies with Stretch only: in Field of View its two rows
// stay editable (their segments hit) with the note; Fill View is a button under Scale Mode in a
// Full mode.
TEST_CASE("Tab menu: Render Margin's note and Fill View", "[frame_xr][tabmenu][fovaxis]") {
    CHECK(LabelFor(Id::FillView) == "Fill View");
    CHECK(SectionOf(Id::FillView) == SettingsSection::Immersive);
    CHECK(!PerGameSetting(Id::FillView));
    CHECK(!ExplainFor(Id::FillView).empty());
    Drawn d;
    MenuModel m = Sample();
    m.immersive_mode = 2;
    m.scale_mode = 1; // Field of View
    MenuView v;
    v.tab = Tab::Settings;
    v.section = SettingsSection::Immersive;
    bool margin_segment = false, fill = false;
    for (float scroll = 0.0f;; scroll += 32.0f) {
        v.scroll[static_cast<int>(Tab::Settings)] = scroll;
        d.Draw(m, v);
        for (const auto& h : d.menu.Hits()) {
            margin_segment |= (h.t.id == Id::MarginX || h.t.id == Id::MarginY) && h.t.index >= 0;
            fill |= h.t.id == Id::FillView;
        }
        if (scroll >= d.menu.MaxScroll()) {
            break;
        }
    }
    CHECK(margin_segment); // never locked in Field of View
    CHECK(fill);
}
