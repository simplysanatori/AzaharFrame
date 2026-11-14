// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiufix: the menu's part of Angel's Aim's fixes: Aiming's rows on the Controls tab
// (moved from Settings, General) and the Mods tab's Angel's Aim segment.

#include <algorithm>
#include <iterator>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/angels_aim_tab.h"
#include "frame_xr/menu/mods_tab.h"
#include "frame_xr/menu/tab_menu.h"

using namespace FrameXr::Menu;

TEST_CASE("Controls tab: Aiming's rows are per game there, and gone from General",
          "[kiufix]") {
    using namespace FrameXr::Menu;
    const auto& rows = ControlsScopeRows();
    for (const Id id : {Id::GyroAim, Id::GyroSensitivity, Id::Recoil, Id::RecoilCharged,
                        Id::RecoilRapid, Id::WeaponInHand, Id::WeaponSize, Id::WeaponScale,
                        Id::LaserSight, Id::TurnZone, Id::TurnSpeed}) {
        INFO(LabelFor(id));
        CHECK(std::find(rows.begin(), rows.end(), id) != rows.end());
    }
    for (const Id id : {Id::RecoilCharged, Id::RecoilRapid, Id::WeaponSize, Id::WeaponScale}) {
        CHECK(PerGameSetting(id));
    }
    MenuModel m;
    m.gyro_aim = 2;
    m.game_value[static_cast<std::size_t>(Id::GyroAim)] = true;
    m.game_value[static_cast<std::size_t>(Id::RecoilCharged)] = true;
    const auto settings = GameValueRows(m, Tab::Settings, SettingsSection::General);
    CHECK(std::find(settings.begin(), settings.end(), Id::GyroAim) == settings.end());
    const auto controls = GameValueRows(m, Tab::Controls, SettingsSection::General);
    CHECK(std::find(controls.begin(), controls.end(), Id::GyroAim) != controls.end());
    CHECK(std::find(controls.begin(), controls.end(), Id::RecoilCharged) != controls.end());
    CHECK(SliderFor(Id::RecoilCharged).max == 100.0f);
    CHECK(SliderFor(Id::WeaponScale).min == 50.0f);
    CHECK(OptionsFor(Id::WeaponSize).size() == 2);
}

TEST_CASE("Angel's Aim segment: shown with Kid Icarus only, after CTGP-7's place", "[kiufix]") {
    using namespace FrameXr::Menu;
    ModsTabModel m;
    CHECK(ModsSegmentsShown(m).size() == 3);
    m.angels_aim.shown = true;
    const auto shown = ModsSegmentsShown(m);
    REQUIRE(shown.size() == 4);
    CHECK(shown[3] == ModsSegment::AngelsAim); // the fourth button, though CTGP-7 is hidden
    CHECK(ShownSegment(m, ModsSegment::Ctgp7) == ModsSegment::Cheats);
    CHECK(ShownSegment(m, ModsSegment::AngelsAim) == ModsSegment::AngelsAim);
    m.angels_aim.shown = false;
    CHECK(ShownSegment(m, ModsSegment::AngelsAim) == ModsSegment::Cheats);
}

TEST_CASE("Angel's Aim: Report an Issue's page", "[kiufix]") {
    using namespace FrameXr::Menu;
    const std::string url =
        AngelsAimReportUrl("1.1.1-524 kiufix", "Europe 1.0", true);
    CHECK(IsAngelsAimReportUrl(url));
    CHECK(url.find("labels=Angel%27s%20Aim") != std::string::npos);
    CHECK(url.rfind(kAngelsAimIssuesUrl, 0) == 0);
    CHECK(url.find("&body=Azahar%20for%20Frame%201.1.1-524%20kiufix%0AGame%3A%20Europe%201.0%0A"
                   "C-Stick%20%26%20Gyro%20mod%3A%20installed") != std::string::npos);
    CHECK(url.size() <= 300);      // the menu's links are at most 300 characters
    CHECK_FALSE(LinkText(url).empty()); // and pass its checks
    CHECK(LinkHost(url) == "github.com");
    // A long version or game name is cut, never pushed past the limit.
    const std::string long_url = AngelsAimReportUrl(std::string(200, 'x'), std::string(200, 'y'), false);
    CHECK(long_url.size() <= 300);
    CHECK_FALSE(IsAngelsAimReportUrl("https://github.com/simplysanatori/AzaharFrame"));
}

TEST_CASE("Angel's Aim: the Required section's mods are the C-Stick mods", "[kiufix]") {
    using namespace FrameXr::Menu;
    ModsListModel m;
    for (const char* id : kAngelsAimModIds) {
        ModOffer o;
        o.id = id;
        m.available.push_back(o);
    }
    ModOffer other;
    other.id = "kiu-something-else";
    m.available.push_back(other);
    CHECK(AngelsAimModRows(m).size() == std::size(kAngelsAimModIds));
}
