// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fixd: the "Verified" chip after a verified entry's name in every catalog list (Install
// Mods, the loaded mods, Download Packs, the installed packs) and after the developer's cheats: on
// the verified rows only, right after the name, a long name cut before it, and clear of what
// follows (the version and compat chips, the buttons). "recommended" alone draws no chip.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/name_chip.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/ui/ui_canvas.h"

using namespace FrameXr::Menu;

namespace {

struct Drawn {
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
    /// The lines on view (not clipped away) whose text starts with `prefix`.
    std::vector<DrawnText> Starting(const std::string& prefix) const {
        std::vector<DrawnText> out;
        for (const auto& t : texts) {
            const float y0 = std::max(t.box.y0, t.clip.y0), y1 = std::min(t.box.y1, t.clip.y1);
            if (y1 > y0 && t.text.rfind(prefix, 0) == 0) {
                out.push_back(t);
            }
        }
        return out;
    }
    std::vector<DrawnText> Chips() const {
        std::vector<DrawnText> out;
        for (const auto& t : Starting(kVerifiedChip)) {
            if (t.text == kVerifiedChip) {
                out.push_back(t);
            }
        }
        return out;
    }
    /// Every chip keeps 8 dp from any other line beside it (tab_menu.cpp's audit, for the chips:
    /// a byline's parts are drawn end to end on purpose).
    void CheckNoOverlap(const char* where) const {
        std::vector<DrawnText> shown;
        for (const auto& t : texts) {
            const float y0 = std::max(t.box.y0, t.clip.y0), y1 = std::min(t.box.y1, t.clip.y1);
            if (y1 > y0) {
                shown.push_back(t);
            }
        }
        for (std::size_t i = 0; i < shown.size(); i++) {
            for (std::size_t j = i + 1; j < shown.size(); j++) {
                if (shown[i].text != kVerifiedChip && shown[j].text != kVerifiedChip) {
                    continue;
                }
                const Rect& a = shown[i].box;
                const Rect& b = shown[j].box;
                if (std::min(a.y1, b.y1) - std::max(a.y0, b.y0) <= 2.0f) {
                    continue; // different lines
                }
                const float gap = std::max(a.x0, b.x0) - std::min(a.x1, b.x1);
                INFO(where << ": \"" << shown[i].text << "\" and \"" << shown[j].text << "\" "
                           << gap << " dp apart");
                CHECK(gap >= 8.0f);
            }
        }
    }
};

/// The chip on the same line as `name` (its box overlapping the name's lines), nullptr none.
const DrawnText* ChipBeside(const std::vector<DrawnText>& chips, const DrawnText& name) {
    for (const auto& c : chips) {
        if (std::min(c.box.y1, name.box.y1) - std::max(c.box.y0, name.box.y0) > 2.0f &&
            c.box.x0 > name.box.x0) {
            return &c;
        }
    }
    return nullptr;
}

const std::string kLong =
    "Ocarina ReFramed (Single Screen) With the Restored Classic Item Menu and Every Extra";

} // namespace

TEST_CASE("fixd: the Verified chip's room", "[frame_xr][fixd]") {
    Kit kit{1.0f};
    CHECK(std::string{kVerifiedChip} == "Verified");
    const float room = NameChipRoom(kit);
    CHECK(room == kNameChipGap + kit.TextWidth(Type::Label, "Verified") + 2.0f * kNameChipPad);
    // the name gets the width less the chip; without the chip all of it
    CHECK(NameRoom(kit, 10.0f, 410.0f, true) == 400.0f - room);
    CHECK(NameRoom(kit, 10.0f, 410.0f, false) == 400.0f);
    // too narrow for both: the name keeps the room, no chip
    CHECK_FALSE(NameChipShown(kit, 0.0f, room + kNameChipMinName - 1.0f, true));
    CHECK(NameChipShown(kit, 0.0f, room + kNameChipMinName, true));
}

TEST_CASE("fixd: Install Mods: the Verified chip after verified names only", "[frame_xr][fixd]") {
    Drawn d;
    MenuModel m;
    m.game_title = "The Legend of Zelda: Ocarina of Time 3D";
    auto& mm = m.mods.mods;
    mm.catalog = CatalogState::Ready;
    ModOffer a;
    a.id = "a";
    a.name = kLong;
    a.author = "OTPR26";
    a.verified = true;
    a.version = "1.0";
    a.version_match = true;
    ModOffer b;
    b.id = "b";
    b.name = "No Bloom";
    b.author = "simplysanatori";
    b.verified = true;
    b.mismatch = true;
    b.region_chip = "USA Only";
    ModOffer c;
    c.id = "c";
    c.name = "Classic Tunic Colours";
    c.author = "someone";
    c.recommended = true; // listed first, never a chip of its own
    mm.available = {a, b, c};
    MenuView v;
    v.tab = Tab::Mods;
    v.mods_segment = ModsSegment::Mods;
    v.mods_page = ModsPage::Install;
    d.Draw(m, v);
    const auto chips = d.Chips();
    CHECK(chips.size() == 2);
    const auto long_name = d.Starting("Ocarina ReFramed");
    REQUIRE(long_name.size() == 1);
    CHECK(long_name[0].text != kLong); // cut short...
    CHECK(long_name[0].text.find("…") != std::string::npos);
    const DrawnText* chip = ChipBeside(chips, long_name[0]);
    REQUIRE(chip != nullptr); // ...and the chip stays, after it
    CHECK(chip->box.x0 > long_name[0].box.x1 + kNameChipGap);
    const auto version = d.Starting("1.0");
    REQUIRE(version.size() == 1);
    CHECK(chip->box.x1 + kNameChipPad < version[0].box.x0); // clear of the version chip
    const auto bloom = d.Starting("No Bloom");
    REQUIRE(bloom.size() == 1);
    CHECK(bloom[0].text == "No Bloom"); // a short name whole
    CHECK(ChipBeside(chips, bloom[0]) != nullptr);
    const auto plain = d.Starting("Classic Tunic Colours");
    REQUIRE(plain.size() == 1);
    CHECK(ChipBeside(chips, plain[0]) == nullptr);
    d.CheckNoOverlap("Install Mods");
}

TEST_CASE("fixd: the loaded mods, Download Packs, the packs and the cheats", "[frame_xr][fixd]") {
    Drawn d;
    MenuModel m;
    MenuView v;
    v.tab = Tab::Mods;

    // the loaded list
    v.mods_segment = ModsSegment::Mods;
    m.mods.mods.loaded = {
        {"a", kLong, "OTPR26", "https://github.com/OTPR26/OOT3DHud", "v1.5.9", false, true, true, true},
        {"b", "No Bloom", "simplysanatori", "", "", false, true, true, true},
        {"c", "Classic Tunic Colours", "someone", "", "v2", true, true, false, false},
        {"unmanaged:romfs", "romfs", "", "", "214 Files", false, false, true}};
    d.Draw(m, v);
    CHECK(d.Chips().size() == 2);
    {
        const auto n = d.Starting("Ocarina ReFramed");
        REQUIRE(n.size() == 1);
        CHECK(n[0].text != kLong);
        CHECK(ChipBeside(d.Chips(), n[0]) != nullptr);
        const auto plain = d.Starting("Classic Tunic Colours");
        REQUIRE(plain.size() == 1);
        CHECK(ChipBeside(d.Chips(), plain[0]) == nullptr);
    }
    d.CheckNoOverlap("loaded mods");

    // Download Packs
    v.mods_segment = ModsSegment::Textures;
    v.textures_page = TexturesPage::Download;
    PackOffer h;
    h.id = "mm3d-4k-henriko";
    h.name = "Majora's Mask 3D 4K Remastered Textures With Every Optional Extra Included Too";
    h.author = "Henriko Magnifico";
    h.size_text = "3.4 GB";
    h.verified = true;
    PackOffer o;
    o.id = "mm3d-hd";
    o.name = "Majora's Mask 3D HD";
    o.author = "DeathWrench";
    o.downloads = "10K+ Downloads";
    PackOffer x = h;
    x.id = "other";
    x.name = "Majora's Mask 3D 1080p";
    x.mismatch = true;
    x.region_chip = "USA Only";
    m.mods.textures.custom_textures = true;
    m.mods.textures.downloads = {h, x, o};
    d.Draw(m, v);
    CHECK(d.Chips().size() == 2);
    {
        const auto n = d.Starting("Majora's Mask 3D 4K");
        REQUIRE(n.size() == 1);
        CHECK(n[0].text.find("…") != std::string::npos);
        CHECK(ChipBeside(d.Chips(), n[0]) != nullptr);
        const auto hd = d.Starting("Majora's Mask 3D HD");
        REQUIRE(hd.size() == 1);
        CHECK(ChipBeside(d.Chips(), hd[0]) == nullptr);
        // the row with the compat chips: its name (cut short beside them) and the chip after it
        const auto usa = d.Starting("USA Only");
        REQUIRE(usa.size() == 1);
        std::vector<DrawnText> other; // the name nearest the chips (one row: within 40 dp)
        for (const auto& t : d.Starting("Majora")) {
            if (std::abs((t.box.y0 + t.box.y1) - (usa[0].box.y0 + usa[0].box.y1)) * 0.5f < 40.0f) {
                other.push_back(t);
            }
        }
        REQUIRE(other.size() == 1);
        INFO(other[0].text);
        const DrawnText* chip = ChipBeside(d.Chips(), other[0]);
        REQUIRE(chip != nullptr);
        CHECK(chip->box.x1 + kNameChipPad < usa[0].box.x0); // clear of the compat chips
    }
    d.CheckNoOverlap("Download Packs");

    // the installed packs
    v.textures_page = TexturesPage::Packs;
    m.mods.textures.packs = {{"oot3d-1080p", "1080p", "Henriko Magnifico · 3.2 · 640 MB", false},
                             {"oot3d-4k-png", "4K (PNG)", "PNG · 5.8 GB", true}};
    m.mods.textures.packs[0].verified = true;
    d.Draw(m, v);
    CHECK(d.Chips().size() == 1);
    {
        const auto n = d.Starting("1080p");
        REQUIRE(n.size() == 1);
        CHECK(ChipBeside(d.Chips(), n[0]) != nullptr);
    }
    d.CheckNoOverlap("packs");

    // the cheats: the developer's
    v.mods_segment = ModsSegment::Cheats;
    m.mods.cheats.cheats = {
        {"No Bloom (USA)", CheatSource::Developer, true, ""},
        {"Faster Walking Speed in Every Town and Dungeon (Hold R to Run Faster Still, Twice)",
         CheatSource::Developer, false, ""},
        {"Infinite HP in Battle", CheatSource::Sharkive, true, ""},
        {"My Speed Hack", CheatSource::User, false, ""}};
    d.Draw(m, v);
    CHECK(d.Chips().size() == 2);
    {
        const auto n = d.Starting("Faster Walking");
        REQUIRE(n.size() == 1);
        CHECK(n[0].text.find("…") != std::string::npos);
        CHECK(ChipBeside(d.Chips(), n[0]) != nullptr);
        const auto hp = d.Starting("Infinite HP");
        REQUIRE(hp.size() == 1);
        CHECK(ChipBeside(d.Chips(), hp[0]) == nullptr);
    }
    d.CheckNoOverlap("cheats");
}

// The catalog's "recommended" shows the Recommended chip instead of Verified; Angel's Aim's
// Suggested row is one pack, found by its catalogue id.
TEST_CASE("Recommended chip over Verified", "[frame_xr][verified_chip]") {
    using namespace FrameXr::Menu;
    CHECK(ChipFor(false) == NameChip::None);
    CHECK(ChipFor(true) == NameChip::Verified);
    CHECK(ChipFor(true, true) == NameChip::Recommended);
    CHECK(ChipFor(false, true) == NameChip::Recommended);
    CHECK(std::string(ChipText(NameChip::Recommended)) == "Recommended");
    CHECK(std::string(ChipText(NameChip::Verified)) == "Verified");
}
