// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame tabmenu: the in-game menu's layout (frame_xr/menu/tab_menu.h): targets big enough
// for a beam, labels that fit, hit testing.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/tab_menu.h"
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

    // The Wider View row in an Off immersive mode: only its tooltip.
    v = {};
    v.tab = Tab::Settings;
    m.immersive_mode = 0;
    // AzaharFrame depth3d: lower down now; menuux: not at the end any more, so every scroll.
    bool label = false;
    for (float scroll = 0.0f;; scroll += 32.0f) {
        v.scroll[static_cast<int>(Tab::Settings)] = scroll;
        d.Draw(m, v);
        for (const auto& h : d.menu.Hits()) {
            CHECK(!(h.t.id == Id::WiderView && h.t.index >= 0));
            label |= h.t.id == Id::WiderView && h.t.index == kLabelIndex;
        }
        if (scroll >= d.menu.MaxScroll()) {
            break;
        }
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
    d.Draw(m, v);
    CHECK(has(Id::ReticleAtEdge, 0));
    CHECK(has(Id::ReticleAtEdge, 1));
    CHECK(has(Id::TouchpadTouch, 1));
    CHECK(has(Id::EditControls, -1));
    CHECK(OptionsFor(Id::ReticleAtEdge) == std::vector<std::string>{"Follow", "Snap to Middle"});
    CHECK(LabelFor(Id::ReticleAtEdge) == "At Edge");
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
    CHECK(OptionsFor(Id::ModsSegment) ==
          std::vector<std::string>{"Cheats", "Mods", "Textures", "Homebrew"}); // emerald
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
    CHECK(HasHit(d, Id::ModLink, 0));
    CHECK(!HasHit(d, Id::ModLink, 3)); // not an https address: no link
    CheckTargets(d);
    CHECK(!HasHit(d, Id::ModVersion, 2)); // no version chip
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
         seen_ff = false;
    for (float scroll = 0.0f; scroll <= 3000.0f; scroll += kRowH) {
        v.scroll[static_cast<int>(Tab::Settings)] = scroll;
        d.Draw(m, v);
        for (const auto& h : d.menu.Hits()) {
            seen_res |= h.t.id == Id::InternalResolution && h.t.index == -1;
            seen_dyn |= h.t.id == Id::DynamicResolution && h.t.index == -1;
            seen_filter |= h.t.id == Id::TextureFilter && h.t.index == 4;
            seen_async |= h.t.id == Id::AsyncShaders && h.t.index == -1;
            seen_ff |= h.t.id == Id::FastForwardSpeed && h.t.index == 3;
        }
    }
    CHECK(seen_res);
    CHECK(seen_dyn);
    CHECK(seen_filter);
    CHECK(seen_async);
    CHECK(seen_ff);
    CHECK(OptionsFor(Id::InternalResolution).size() == static_cast<std::size_t>(kMaxResolution));
    CHECK(OptionsFor(Id::InternalResolution).front() == "1x (400x240)");
    CHECK(OptionsFor(Id::InternalResolution).back() == "8x (3200x1920)");
    CHECK(OptionsFor(Id::TextureFilter) ==
          std::vector<std::string>{"None", "Bicubic", "ScaleForce", "xBRZ", "MMPX"});
    CHECK(OptionsFor(Id::FastForwardSpeed) ==
          std::vector<std::string>{"150%", "200%", "300%", "400%"});
    for (Id id : {Id::InternalResolution, Id::DynamicResolution, Id::TextureFilter,
                  Id::AsyncShaders, Id::FastForwardSpeed}) {
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

TEST_CASE("Tab menu: Dynamic Resolution has nothing to do at 1x", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Settings;
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
        for (float scroll = 0.0f; scroll <= 3000.0f; scroll += kRowH) {
            v.scroll[static_cast<std::size_t>(t)] = scroll;
            a.Draw(m, v);
            a.Check(("tab " + std::to_string(t)).c_str());
            if (scroll >= a.menu.MaxScroll()) {
                break;
            }
        }
    }
    for (Page page : {Page::SaveSlots, Page::LoadSlots}) {
        MenuView v;
        v.page = page;
        a.Draw(m, v);
        a.Check("slots");
    }
    // Both scopes of the Settings tab (Per-Game shows the title beside them).
    MenuModel per_game = m;
    per_game.scope = 1;
    MenuView v;
    v.tab = Tab::Settings;
    a.Draw(per_game, v);
    a.Check("Settings, Per-Game");
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

// AzaharFrame zipinstall: Install from Zip on the Textures page and Install Mods, and its page.
TEST_CASE("Tab menu: Install from Zip", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = kModsTab;
    v.mods_segment = ModsSegment::Textures;
    CHECK(LabelFor(Id::InstallFromZip) == "Install from Zip");

    // the Textures page, with and without packs
    d.Draw(m, v);
    CHECK(HasHit(d, Id::InstallFromZip, static_cast<int>(ZipTarget::Textures)));
    CHECK(HasHit(d, Id::DownloadPacks, -1));
    m.mods.textures = Packs(2);
    d.Draw(m, v);
    REQUIRE(FindHit(d, Id::InstallFromZip, static_cast<int>(ZipTarget::Textures)));
    CHECK(FindHit(d, Id::InstallFromZip, 0)->r.x0 > FindHit(d, Id::DownloadPacks, -1)->r.x1);
    CheckTargets(d);

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

// AzaharFrame emerald: the Mods tab's Homebrew segment.
TEST_CASE("Tab menu: the Homebrew segment", "[frame_xr][tabmenu]") {
    Drawn d;
    MenuModel m = Sample();
    MenuView v;
    v.tab = Tab::Mods;
    v.mods_segment = ModsSegment::Homebrew;
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

    // Not installed: Install only.
    m.homebrew.items = {item};
    d.Draw(m, v);
    CHECK(has(Id::ModsSegment, 3));
    CHECK(has(Id::HomebrewInstall, 0));
    CHECK(!has(Id::HomebrewPlay, 0));
    CHECK(!has(Id::HomebrewUpdate, 0));

    // Installed and current: Play; with a newer release: Play and Update.
    m.homebrew.items[0].installed = true;
    d.Draw(m, v);
    CHECK(has(Id::HomebrewPlay, 0));
    CHECK(!has(Id::HomebrewInstall, 0));
    CHECK(!has(Id::HomebrewUpdate, 0));
    m.homebrew.items[0].update = true;
    d.Draw(m, v);
    CHECK(has(Id::HomebrewPlay, 0));
    CHECK(has(Id::HomebrewUpdate, 0));

    // Busy: no buttons until it ends.
    m.homebrew.items[0].busy = true;
    m.homebrew.items[0].progress = 420;
    m.homebrew.items[0].status = "Downloading";
    d.Draw(m, v);
    CHECK(!has(Id::HomebrewPlay, 0));
    CHECK(!has(Id::HomebrewUpdate, 0));
    CHECK(d.menu.MaxScroll() == 0.0f);

    CHECK(LabelFor(Id::HomebrewInstall) == "Install");
    CHECK(LabelFor(Id::HomebrewUpdate) == "Update");
    CHECK(LabelFor(Id::HomebrewPlay) == "Play");
}
