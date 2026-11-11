// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: the in-game menu's layout (frame_xr/menu/tab_menu.h): targets big enough
// for a beam, labels that fit, hit testing.

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/tab_menu.h"

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
    MenuView end;
    end.tab = Tab::Settings;
    end.scroll[static_cast<int>(Tab::Settings)] = 10000.0f;
    views.push_back(end);
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
            if ((v.tab == Tab::Position || v.tab == Tab::Settings) &&
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
    // The Main tab's tiles: three to a row in the pane.
    const float tile_w = (kPanelW - 104.0f - 12.0f - 48.0f - 24.0f) / 3.0f;
    for (Id id : {Id::QuickSave, Id::QuickLoad, Id::Screenshot, Id::SaveState, Id::LoadState,
                  Id::Restart, Id::AzaharWindow, Id::SwitchToFlat,
                  Id::CloseGame}) {
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

    // The Wider View row in an Off immersive mode: only its tooltip.
    v = {};
    v.tab = Tab::Settings;
    v.scroll[static_cast<int>(Tab::Settings)] = 10000.0f; // AzaharFrame depth3d: lower down now
    m.immersive_mode = 0;
    d.Draw(m, v);
    bool label = false;
    for (const auto& h : d.menu.Hits()) {
        CHECK(!(h.t.id == Id::WiderView && h.t.index >= 0));
        label |= h.t.id == Id::WiderView && h.t.index == kLabelIndex;
    }
    CHECK(label);

    // A locked Background: no segments.
    m = Sample();
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
    CHECK(layout.SliderAt(Id::WiderView, s.x0) < 0.0f);
}

// AzaharFrame depth3d
TEST_CASE("Tab menu: 3D Depth is a 0 to 100 % slider on the Settings tab", "[frame_xr][tabmenu]") {
    CHECK(LabelFor(Id::Depth3d) == "3D Depth");
    CHECK(ExplainFor(Id::Depth3d) == "How deep the 3D looks in this game.");
    const SliderRange r = SliderFor(Id::Depth3d);
    CHECK(r.min == 0.0f);
    CHECK(r.max == 100.0f);
    CHECK(r.step == 5.0f);

    Drawn d;
    MenuView v;
    v.tab = Tab::Settings;
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
TEST_CASE("Tab menu: Move Screens, Touchpad Touch, Edit Controls, Cheats", "[frame_xr][tabmenu]") {
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
    CHECK(OptionsFor(Id::MoveScreens) == std::vector<std::string>{"Off", "Other", "Both"});
    v.tab = Tab::Controls;
    d.Draw(m, v);
    CHECK(has(Id::TouchpadTouch, 0));
    CHECK(has(Id::TouchpadTouch, 2));
    CHECK(has(Id::EditControls, -1));
    CHECK(OptionsFor(Id::TouchpadTouch) == std::vector<std::string>{"Off", "Touch", "Click"});
}

// AzaharFrame cheatstab: the Cheats tab.
namespace {
CheatsModel SampleCheats() {
    CheatsModel c;
    c.fix = FixState::NotInstalled;
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

TEST_CASE("Tab menu: the Cheats tab", "[frame_xr][tabmenu]") {
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
    // The Main tab is 1.1.1's again: no Cheats tile; the rail has Cheats before Settings.
    d.Draw(m, v);
    CHECK(has(Id::TabButton, static_cast<int>(Tab::Cheats)));
    CHECK(static_cast<int>(Tab::Cheats) == static_cast<int>(Tab::Controls) + 1);
    CHECK(static_cast<int>(Tab::Settings) == static_cast<int>(Tab::Cheats) + 1);

    // Empty: Download in the middle, Azahar's Window at the top.
    v.tab = Tab::Cheats;
    d.Draw(m, v);
    CHECK(has(Id::DownloadCheats, -1));
    CHECK(has(Id::CheatsWindow, -1));
    CHECK(!has(Id::Cheat, 0));
    // Downloading with nothing yet: no Download.
    m.cheats.download = DownloadState::Downloading;
    d.Draw(m, v);
    CHECK(!has(Id::DownloadCheats, -1));

    // A list: every row and the fix, targets at least 48 dp, the list scrolls to its end.
    m.cheats = SampleCheats();
    d.Draw(m, v);
    CHECK(has(Id::DownloadCheats, -1));
    CHECK(has(Id::CheatFix, -1));
    CHECK(has(Id::Cheat, 0));
    REQUIRE(d.menu.MaxScroll() > 0.0f);
    v.scroll[static_cast<int>(Tab::Cheats)] = 10000.0f;
    d.Draw(m, v);
    CHECK(has(Id::Cheat, static_cast<int>(m.cheats.cheats.size()) - 1)); // the user's, last
    for (float scroll : {0.0f, 10000.0f}) {
        v.scroll[static_cast<int>(Tab::Cheats)] = scroll;
        d.Draw(m, v);
        for (const auto& h : d.menu.Hits()) {
            if (h.t.index == kLabelIndex || h.r.y1 >= kListBottom - 0.01f ||
                (h.t.id == Id::Cheat && h.r.H() < 56.0f)) {
                continue; // cut by the list's edges
            }
            INFO("id " << static_cast<int>(h.t.id) << " index " << h.t.index);
            CHECK(h.r.W() >= 48.0f);
            CHECK(h.r.H() >= 48.0f);
        }
    }
    // Another code.ips in the mods folder: the fix cannot be installed.
    m.cheats.fix = FixState::OtherPatch;
    v.scroll = {};
    d.Draw(m, v);
    CHECK(!has(Id::CheatFix, -1));
    CHECK(has(Id::CheatFix, kLabelIndex));

    CHECK(VersionChip("for v1.0, this game is v1.1") == "v1.0");
    CHECK(VersionChip("for Rev0, this cartridge is Rev1") == "Rev0");
    CHECK(VersionChip("") == "");
}
