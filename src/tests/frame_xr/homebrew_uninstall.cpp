// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emhb: the Homebrew tab's Uninstall, Repair and Uninstall's dialog (homebrew_tab.h).

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/ui/ui_canvas.h"

using namespace FrameXr::Menu;

namespace {

struct Drawn {
    Kit kit{1.0f};
    frame_xr::ui::Canvas canvas;
    TabMenu menu;
    void Draw(const MenuModel& m, const MenuView& v) {
        canvas.Init(static_cast<int>(kPanelW), static_cast<int>(kPanelH));
        kit.Begin(canvas);
        menu.Draw(kit, m, v);
    }
    const MenuLayout::Hit* Find(Id id, int index = -1) const {
        for (const auto& h : menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                return &h;
            }
        }
        return nullptr;
    }
};

HomebrewItem Item() {
    HomebrewItem it;
    it.id = "emerald3ds";
    it.title = "Pokémon Emerald 3DS";
    it.author = "ZallaxDev";
    return it;
}

} // namespace

TEST_CASE("Homebrew tab: Uninstall whenever anything is there, Repair when broken", "[frame_xr][emhb]") {
    HomebrewItem it = Item();
    CHECK(HomebrewButtons(it) == std::vector<Id>{Id::HomebrewInstall});
    CHECK(HomebrewButtonLabel(it, Id::HomebrewInstall) == "Install");

    it.installed = true;
    it.can_uninstall = true;
    CHECK(HomebrewButtons(it) == std::vector<Id>{Id::HomebrewPlay, Id::HomebrewCheck,
                                                 Id::HomebrewReinstall, Id::HomebrewUninstall});
    it.update = true;
    CHECK(HomebrewButtons(it) ==
          std::vector<Id>{Id::HomebrewPlay, Id::HomebrewUpdate, Id::HomebrewUninstall});
    it.busy = true;
    CHECK(HomebrewButtons(it).empty());

    HomebrewItem br = Item();
    br.repair = true;
    br.can_uninstall = true;
    CHECK(HomebrewButtons(br) == std::vector<Id>{Id::HomebrewInstall, Id::HomebrewUninstall});
    CHECK(HomebrewButtonLabel(br, Id::HomebrewInstall) == "Repair");
    CHECK(HomebrewButtonLabel(br, Id::HomebrewUninstall) == "Uninstall");
    CHECK(HomebrewButtonLabel(br, Id::HomebrewPlay) == "Play");

    // drawn: every button is a 48 dp target and the title keeps room
    Drawn d;
    MenuModel m;
    MenuView v;
    v.tab = Tab::Homebrew;
    HomebrewItem full = Item();
    full.installed = true;
    full.can_uninstall = true;
    full.author = "ZallaxDev · English";
    full.version = "v0.5.0";
    full.checked = "Checked Just Now";
    m.homebrew.items = {full};
    d.Draw(m, v);
    for (const Id id : HomebrewButtons(full)) {
        const auto* h = d.Find(id, 0);
        REQUIRE(h);
        CHECK(h->r.H() >= 47.9f);
    }
    // the title still fits: Reinstall and Uninstall go under Play and Check for Updates (the text
    // starts at the pane, after the 104 dp rail, plus its 24 dp padding, 24 dp from the buttons)
    const auto* check = d.Find(Id::HomebrewCheck, 0);
    const auto* play = d.Find(Id::HomebrewPlay, 0);
    const auto* reinstall = d.Find(Id::HomebrewReinstall, 0);
    const auto* uninstall = d.Find(Id::HomebrewUninstall, 0);
    REQUIRE(check);
    REQUIRE(play);
    REQUIRE(reinstall);
    REQUIRE(uninstall);
    CHECK(check->r.x0 - 24.0f - (104.0f + 24.0f) >=
          d.kit.TextWidth(Type::Body, "Pokémon Emerald 3DS"));
    CHECK(uninstall->r.y0 >= play->r.y1);      // a line of its own, no overlap
    CHECK(uninstall->r.x1 <= reinstall->r.x0); // right to left as on one line: Reinstall, Uninstall
    CHECK(uninstall->r.x1 <= play->r.x1 + 1.0f);
    // with Update there is room on one line
    m.homebrew.items[0].update = true;
    m.homebrew.items[0].latest = "v0.6.0";
    d.Draw(m, v);
    REQUIRE(d.Find(Id::HomebrewUninstall, 0));
    REQUIRE(d.Find(Id::HomebrewPlay, 0));
    CHECK(d.Find(Id::HomebrewUninstall, 0)->r.Cy() == d.Find(Id::HomebrewPlay, 0)->r.Cy());
    // Repair and Uninstall: one line
    HomebrewItem broken = Item();
    broken.repair = true;
    broken.can_uninstall = true;
    m.homebrew.items = {broken, full};
    d.Draw(m, v);
    REQUIRE(d.Find(Id::HomebrewInstall, 0));
    REQUIRE(d.Find(Id::HomebrewUninstall, 0));
    CHECK(d.Find(Id::HomebrewInstall, 0)->r.Cy() == d.Find(Id::HomebrewUninstall, 0)->r.Cy());
    // the second row starts below the first one's height, its Uninstall below its Play
    REQUIRE(d.Find(Id::HomebrewPlay, 1));
    REQUIRE(d.Find(Id::HomebrewUninstall, 1));
    CHECK(d.Find(Id::HomebrewPlay, 1)->r.y0 >= d.Find(Id::HomebrewUninstall, 0)->r.y1);
    CHECK(d.Find(Id::HomebrewUninstall, 1)->r.y0 >= d.Find(Id::HomebrewPlay, 1)->r.y1);
}

TEST_CASE("Homebrew tab: Uninstall's dialog", "[frame_xr][emhb]") {
    Drawn d;
    MenuModel m;
    MenuView v;
    v.tab = Tab::Homebrew;
    HomebrewItem it = Item();
    it.installed = true;
    it.can_uninstall = true;
    m.homebrew.items = {it};
    HomebrewDialog& dl = m.homebrew.dialog;
    dl.id = "emerald3ds";
    dl.title = "Uninstall Pokémon Emerald 3DS?";
    dl.body = "Removes the game and its build files. Your ROM and save data are kept.";
    dl.check = "Also Delete Save Data";
    dl.confirm = "Uninstall";
    dl.cancel = "Cancel";
    d.Draw(m, v);
    const auto* check = d.Find(Id::HomebrewDialogCheck);
    const auto* confirm = d.Find(Id::HomebrewDialogConfirm);
    const auto* cancel = d.Find(Id::HomebrewDialogCancel);
    REQUIRE(check);
    REQUIRE(confirm);
    REQUIRE(cancel);
    CHECK(check->r.H() >= 47.9f);
    CHECK(confirm->r.H() >= 47.9f);
    CHECK(cancel->r.x1 <= confirm->r.x0); // Cancel, then Uninstall last
    CHECK(check->r.y1 <= confirm->r.y0);
    // modal: the row's buttons under it are not hit
    const auto* row = d.Find(Id::HomebrewUninstall, 0);
    REQUIRE(row);
    CHECK(d.menu.Layout().HitTest(row->r.Cx(), row->r.Cy()).id == Id::None);
    CHECK(d.menu.Layout().HitTest(confirm->r.Cx(), confirm->r.Cy()).id == Id::HomebrewDialogConfirm);

    // no save: no checkbox; one button (OK) for the running game
    dl.check.clear();
    d.Draw(m, v);
    CHECK_FALSE(d.Find(Id::HomebrewDialogCheck));
    dl.cancel.clear();
    dl.confirm = "OK";
    d.Draw(m, v);
    CHECK(d.Find(Id::HomebrewDialogConfirm));
    CHECK_FALSE(d.Find(Id::HomebrewDialogCancel));
    // none: nothing over the tab
    dl = {};
    d.Draw(m, v);
    CHECK_FALSE(d.Find(Id::HomebrewDialogConfirm));
    CHECK(d.Find(Id::HomebrewUninstall, 0));
}
