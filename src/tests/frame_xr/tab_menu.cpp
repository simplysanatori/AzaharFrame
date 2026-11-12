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
    // The rail: Main, Position, Stats, Controls, Mods, Settings.
    d.Draw(m, v);
    CHECK(has(Id::TabButton, static_cast<int>(Tab::Mods)));
    CHECK(static_cast<int>(Tab::Mods) == static_cast<int>(Tab::Controls) + 1);
    CHECK(static_cast<int>(Tab::Settings) == static_cast<int>(Tab::Mods) + 1);

    // The segmented button on every segment; Cheats first.
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

    // A list: one row per cheat in file order (no subheaders, no patch row), targets at least
    // 48 dp, the list scrolls to its end.
    m.mods.cheats = SampleCheats();
    d.Draw(m, v);
    CHECK(has(Id::DownloadCheats, -1));
    CHECK(has(Id::Cheat, 0));
    CHECK(has(Id::Cheat, 1));
    CHECK(has(Id::Cheat, 2));
    REQUIRE(d.menu.MaxScroll() > 0.0f);
    {
        // Rows follow each other with nothing between them.
        float y0 = -1.0f, y1 = -1.0f;
        for (const auto& h : d.menu.Hits()) {
            if (h.t.id == Id::Cheat && h.t.index == 0) {
                y0 = h.r.y0;
            } else if (h.t.id == Id::Cheat && h.t.index == 1) {
                y1 = h.r.y0;
            }
        }
        CHECK(y1 - y0 == 48.0f);
    }
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

/// Every target of the segment is at least 48 dp and inside the panel.
void CheckTargets(const Drawn& d) {
    for (const auto& h : d.menu.Hits()) {
        if (h.t.index == kLabelIndex || h.t.index == kMenuAreaIndex) {
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

    // Custom Textures, Off and three packs, Preload: all on view without scrolling.
    m.mods.textures = Packs(3);
    d.Draw(m, v);
    CHECK(HasHit(d, Id::CustomTextures, -1));
    CHECK(HasHit(d, Id::TexturePreload, -1));
    for (int i = 0; i <= 3; i++) {
        CHECK(HasHit(d, Id::TexturePack, i));
    }
    CHECK(d.menu.MaxScroll() == 0.0f);
    CheckTargets(d);
    // In that order, Off first.
    REQUIRE(FindHit(d, Id::TexturePack, 0));
    CHECK(FindHit(d, Id::CustomTextures, -1)->r.y1 <= FindHit(d, Id::TexturePack, 0)->r.y0);
    CHECK(FindHit(d, Id::TexturePack, 0)->r.y1 <= FindHit(d, Id::TexturePack, 1)->r.y0 + 0.01f);
    CHECK(FindHit(d, Id::TexturePack, 3)->r.y1 <= FindHit(d, Id::TexturePreload, -1)->r.y0);
    // With the snackbar too.
    m.mods.textures.needs_restart = true;
    d.Draw(m, v);
    CHECK(HasHit(d, Id::RestartGame, -1));
    CHECK(HasHit(d, Id::TexturePreload, -1));
    CHECK(d.menu.MaxScroll() == 0.0f);

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
    mm.catalog = CatalogState::Ready;
    mm.available = {{"s", "Ocarina ReFramed (Single Screen)", "OTPR26", "https://github.com/OTPR26/OOT3DHud", false, OfferState::Install, "", false},
                    {"d", "Ocarina ReFramed (Dual Screen)", "OTPR26", "https://github.com/OTPR26/OOT3DHud", false, OfferState::Installed, "", false},
                    {"r", "Project Restoration", "leoetlino", "https://github.com/leoetlino/project-restoration", false, OfferState::Unavailable,
                     "Unsupported Archive", false},
                    {"n", "No Bloom", "simplysanatori", "https://example.com/x", true, OfferState::Installing, "Downloading", false}};
    d.Draw(m, v);
    CHECK(HasHit(d, Id::ModInstall, 0));
    CHECK(!HasHit(d, Id::ModInstall, 1)); // installed
    CHECK(!HasHit(d, Id::ModInstall, 2)); // cannot install yet
    CHECK(!HasHit(d, Id::ModInstall, 3)); // installing
    CHECK(HasHit(d, Id::ModLink, 0));
    CHECK(!HasHit(d, Id::ModLink, 3)); // not a GitHub address: no link
    CheckTargets(d);
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
    CHECK(SizeText(640'000'000) == "640 MB");
    CHECK(SizeText(2'100'000'000) == "2.1 GB");
    CHECK(SizeText(12'345) == "12 KB");
}
