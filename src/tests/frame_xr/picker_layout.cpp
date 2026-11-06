// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>

#include "frame_xr/picker/library.h"
#include "frame_xr/picker/picker.h"
#include "frame_xr/ui/ui_canvas.h"

using namespace frame_xr::picker;
using namespace frame_xr::picker::layout;
namespace fs = std::filesystem;

namespace {

std::vector<GameEntry> MakeEntries(int n) {
    std::vector<GameEntry> v(n);
    for (int i = 0; i < n; i++) {
        char name[32];
        std::snprintf(name, sizeof(name), "Game %02d", i);
        v[i].title = name;
        v[i].path = std::string("/games/") + name + ".3ds";
        v[i].icon_w = v[i].icon_h = kIconSize;
        v[i].icon_rgba.assign(kIconSize * kIconSize * 4, static_cast<uint8_t>(40 + i * 10));
    }
    return v;
}

Vec2 UvOf(const Rect& r) {
    return {(r.x0 + r.x1) * 0.5f / kWidth, (r.y0 + r.y1) * 0.5f / kHeight};
}

PickerInput Point(const Rect& r, bool select = false) {
    PickerInput in;
    in.hover = UvOf(r);
    in.select = select;
    in.dt = 0.016f;
    return in;
}

uint32_t PixelAt(const Picker& p, int x, int y) {
    uint32_t v;
    std::memcpy(&v, p.Pixels() + (static_cast<size_t>(y) * p.Width() + x) * 4, 4);
    return v;
}

} // namespace

TEST_CASE("picker layout: tiles fit the texture without overlapping", "[frame_xr][picker]") {
    static_assert(kTileW == 342);
    for (int i = 0; i < kTilesPerPage; i++) {
        const Rect a = TileRect(i);
        CHECK(a.x0 >= kMargin);
        CHECK(a.x1 <= kWidth - kMargin);
        CHECK(a.y0 >= kGridTop);
        CHECK(a.y1 <= PreviousButton().y0 - 8); // clear of the footer
        for (int j = i + 1; j < kTilesPerPage; j++) {
            const Rect b = TileRect(j);
            const bool apart = a.x1 <= b.x0 || b.x1 <= a.x0 || a.y1 <= b.y0 || b.y1 <= a.y0;
            CHECK(apart);
        }
    }
    // Row-major: slot 4 starts the second row.
    CHECK(TileRect(4).x0 == TileRect(0).x0);
    CHECK(TileRect(4).y0 > TileRect(0).y1);
    CHECK(TileRect(3).x1 == kWidth - kMargin);
    // The buttons do not touch the tiles or each other.
    CHECK(RescanButton().y1 < kGridTop);
    CHECK(PreviousButton().x1 < NextButton().x0);
}

TEST_CASE("picker layout: PageCount", "[frame_xr][picker]") {
    CHECK(PageCount(0) == 1);
    CHECK(PageCount(1) == 1);
    CHECK(PageCount(8) == 1);
    CHECK(PageCount(9) == 2);
    CHECK(PageCount(16) == 2);
    CHECK(PageCount(17) == 3);
}

TEST_CASE("picker layout: HitTest", "[frame_xr][picker]") {
    const Rect c0 = TileRect(0), c7 = TileRect(7);
    const auto centre = [](const Rect& r) { return std::pair{(r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2}; };

    SECTION("tiles map to entry indices on each page") {
        auto [x0, y0] = centre(c0);
        CHECK(HitTest(x0, y0, 0, 10) == Hit{HitKind::Tile, 0});
        auto [x7, y7] = centre(c7);
        CHECK(HitTest(x7, y7, 0, 10) == Hit{HitKind::Tile, 7});
        CHECK(HitTest(x0, y0, 1, 10) == Hit{HitKind::Tile, 8});
        auto [x1, y1] = centre(TileRect(1));
        CHECK(HitTest(x1, y1, 1, 10) == Hit{HitKind::Tile, 9});
        // Slot 2 of page 1 has no entry.
        auto [x2, y2] = centre(TileRect(2));
        CHECK(HitTest(x2, y2, 1, 10).kind == HitKind::None);
    }
    SECTION("tile edges are half open and gaps hit nothing") {
        const Rect r = TileRect(0);
        CHECK(HitTest(r.x0, r.y0, 0, 10).kind == HitKind::Tile);
        CHECK(HitTest(r.x0 - 1, r.y0, 0, 10).kind == HitKind::None);
        CHECK(HitTest(r.x1, r.y0 + 10, 0, 10).kind == HitKind::None); // gap
        CHECK(HitTest(r.x0 + 10, r.y1 + 1, 0, 10).kind == HitKind::None);
        CHECK(HitTest(-5, -5, 0, 10).kind == HitKind::None);
        CHECK(HitTest(kWidth + 5, kHeight + 5, 0, 10).kind == HitKind::None);
    }
    SECTION("rescan is always there") {
        auto [x, y] = centre(RescanButton());
        CHECK(HitTest(x, y, 0, 0).kind == HitKind::Rescan);
        CHECK(HitTest(x, y, 0, 100).kind == HitKind::Rescan);
    }
    SECTION("paging buttons exist only where there is somewhere to go") {
        auto [px, py] = centre(PreviousButton());
        auto [nx, ny] = centre(NextButton());
        CHECK(HitTest(px, py, 0, 10).kind == HitKind::None);
        CHECK(HitTest(nx, ny, 0, 10).kind == HitKind::Next);
        CHECK(HitTest(px, py, 1, 10).kind == HitKind::Previous);
        CHECK(HitTest(nx, ny, 1, 10).kind == HitKind::None);
        CHECK(HitTest(nx, ny, 0, 8).kind == HitKind::None); // one page only
    }
}

TEST_CASE("picker ui: WrapText", "[frame_xr][picker]") {
    using namespace frame_xr::ui;
    InitFonts();
    CHECK(WrapText(kFontSmall, "", 300, 3).empty());
    CHECK(WrapText(kFontSmall, "Short", 300, 3) == std::vector<std::string>{"Short"});

    const auto lines = WrapText(kFontSmall, "Pokemon Ultra Sun and Moon Special Edition", 310, 3);
    CHECK(lines.size() >= 2);
    CHECK(lines.size() <= 3);
    for (const auto& l : lines) {
        CHECK(TextWidth(kFontSmall, l.c_str()) <= 310.0f);
    }

    // Too long for the lines available: the last one ends in "...".
    const auto cut = WrapText(kFontSmall,
                              "A Very Long Title That Will Never Fit In Two Lines Of This Width At All",
                              250, 2);
    REQUIRE(cut.size() == 2);
    CHECK(cut[1].size() > 3);
    CHECK(cut[1].substr(cut[1].size() - 3) == "...");
    for (const auto& l : cut) {
        CHECK(TextWidth(kFontSmall, l.c_str()) <= 250.0f);
    }

    // One word wider than the line is split by character.
    const auto word = WrapText(kFontSmall, "Supercalifragilisticexpialidocious", 150, 4);
    CHECK(word.size() >= 2);
    for (const auto& l : word) {
        CHECK(TextWidth(kFontSmall, l.c_str()) <= 150.0f);
    }
}

TEST_CASE("picker ui: canvas drawing", "[frame_xr][picker]") {
    using namespace frame_xr::ui;
    Canvas cv;
    cv.Init(8, 8);
    cv.Clear(rgb(10, 20, 30));
    CHECK(cv.px[0] == (10u | 20u << 8 | 30u << 16 | 255u << 24));

    // Nearest blit of 2x1 RGBA onto 4x2: left half red, right half blue.
    const uint8_t src[8] = {255, 0, 0, 255, 0, 0, 255, 255};
    cv.BlitRGBA(src, 2, 1, 0, 0, 4, 2, false);
    CHECK((cv.px[0] & 0xFF) == 255);
    CHECK(((cv.px[1 * 8 + 1] >> 16) & 0xFF) == 0);
    CHECK(((cv.px[0 * 8 + 3] >> 16) & 0xFF) == 255);
    // Outside the destination rectangle nothing changed.
    CHECK(cv.px[5 * 8 + 5] == (10u | 20u << 8 | 30u << 16 | 255u << 24));

    // A filled rounded rectangle covers its middle and leaves the outside.
    cv.Clear(rgb(0, 0, 0));
    cv.RoundRect(1, 1, 7, 7, 2, rgb(200, 100, 50));
    CHECK((cv.px[4 * 8 + 4] & 0xFF) == 200);
    CHECK(cv.px[0] == (0u | 255u << 24));
}

TEST_CASE("picker: draws only when something changed", "[frame_xr][picker]") {
    Picker p;
    p.SetLibraryState(MakeEntries(10), true, false);

    PickerInput idle;
    idle.dt = 0.016f;
    CHECK(p.Update(idle).dirty);        // first frame
    CHECK_FALSE(p.Update(idle).dirty);  // nothing moved
    CHECK_FALSE(p.Update(idle).dirty);

    // Pointing at a tile redraws once, staying on it does not.
    const auto on_tile = Point(TileRect(1));
    CHECK(p.Update(on_tile).dirty);
    CHECK(p.hovered() == Hit{HitKind::Tile, 1});
    CHECK_FALSE(p.Update(on_tile).dirty);
    // Moving inside the same tile does not redraw either.
    auto nudge = on_tile;
    nudge.hover->u += 0.01f;
    CHECK_FALSE(p.Update(nudge).dirty);
    // Pointing away does.
    CHECK(p.Update(idle).dirty);
    CHECK(p.hovered().kind == HitKind::None);

    p.Invalidate();
    CHECK(p.Update(idle).dirty);
}

TEST_CASE("picker: hover highlight changes the tile's pixels", "[frame_xr][picker]") {
    Picker p;
    p.SetLibraryState(MakeEntries(3), true, false);
    PickerInput idle;
    p.Update(idle);
    // A tile corner (outside the icon) in the idle and hovered states.
    const Rect r = TileRect(0);
    const int x = static_cast<int>(r.x0) + 40, y = static_cast<int>(r.y0) + 10;
    const uint32_t before = PixelAt(p, x, y);
    p.Update(Point(r));
    CHECK(PixelAt(p, x, y) != before);
    // The background is opaque.
    CHECK((PixelAt(p, 0, 0) >> 24) == 255);
    // The icon area differs from the tile around it.
    CHECK(PixelAt(p, static_cast<int>((r.x0 + r.x1) / 2), static_cast<int>(r.y0) + 100) !=
          PixelAt(p, x, y));
}

TEST_CASE("picker: selecting a tile launches its path", "[frame_xr][picker]") {
    Picker p;
    const auto entries = MakeEntries(10);
    p.SetLibraryState(entries, true, false);

    CHECK_FALSE(p.Update(Point(TileRect(3))).launch_path.has_value()); // hover alone does not
    const auto result = p.Update(Point(TileRect(3), true));
    REQUIRE(result.launch_path.has_value());
    CHECK(*result.launch_path == entries[3].path);

    // Selecting empty space, a slot without an entry or with no ray does nothing.
    PickerInput none;
    none.select = true;
    CHECK_FALSE(p.Update(none).launch_path.has_value());
    p.Update(Point(NextButton(), true)); // to page 1 (entries 8, 9)
    REQUIRE(p.page() == 1);
    CHECK_FALSE(p.Update(Point(TileRect(5), true)).launch_path.has_value());
    const auto second = p.Update(Point(TileRect(1), true));
    REQUIRE(second.launch_path.has_value());
    CHECK(*second.launch_path == entries[9].path);
}

TEST_CASE("picker: paging with buttons, back and the stick", "[frame_xr][picker]") {
    Picker p;
    p.SetLibraryState(MakeEntries(20), true, false); // 3 pages
    REQUIRE(p.page_count() == 3);

    // Buttons.
    p.Update(Point(NextButton(), true));
    CHECK(p.page() == 1);
    p.Update(Point(NextButton(), true));
    CHECK(p.page() == 2);
    p.Update(Point(NextButton(), true)); // disabled on the last page
    CHECK(p.page() == 2);
    p.Update(Point(PreviousButton(), true));
    CHECK(p.page() == 1);

    // Back goes up a page, then reports the root.
    PickerInput back;
    back.back = true;
    CHECK_FALSE(p.Update(back).back_at_root);
    CHECK(p.page() == 0);
    CHECK(p.Update(back).back_at_root);
    CHECK(p.page() == 0);

    // The stick: one page per push, no repeat before the delay, re-arms on release.
    PickerInput stick;
    stick.dt = 0.016f;
    stick.scroll = 1.0f;
    p.Update(stick);
    CHECK(p.page() == 1);
    for (int i = 0; i < 10; i++) { // 160 ms held
        p.Update(stick);
    }
    CHECK(p.page() == 1);
    stick.dt = 0.4f; // 0.5 s in total: repeats
    p.Update(stick);
    CHECK(p.page() == 2);
    stick.dt = 0.016f;
    p.Update(stick);
    CHECK(p.page() == 2); // clamped at the last page and not repeating yet

    stick.scroll = 0.0f;
    p.Update(stick);
    stick.scroll = -1.0f; // up
    p.Update(stick);
    CHECK(p.page() == 1);
    // A small deflection does nothing, even repeatedly.
    stick.scroll = 0.0f;
    p.Update(stick);
    stick.scroll = -0.4f;
    for (int i = 0; i < 100; i++) {
        p.Update(stick);
    }
    CHECK(p.page() == 1);
}

TEST_CASE("picker: page is clamped when the list shrinks", "[frame_xr][picker]") {
    Picker p;
    p.SetLibraryState(MakeEntries(20), true, false);
    PickerInput stick;
    stick.scroll = 1.0f;
    p.Update(stick);
    stick.scroll = 0;
    p.Update(stick);
    stick.scroll = 1.0f;
    p.Update(stick);
    REQUIRE(p.page() == 2);
    p.SetLibraryState(MakeEntries(5), true, false);
    CHECK(p.page() == 0);
    p.SetLibraryState({}, true, false);
    CHECK(p.page() == 0);
    CHECK(p.page_count() == 1);
}

TEST_CASE("picker: empty states draw something different", "[frame_xr][picker]") {
    Picker p;
    PickerInput idle;

    const auto hash = [&] {
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < p.PixelBytes(); i++) {
            h = (h ^ p.Pixels()[i]) * 1099511628211ull;
        }
        return h;
    };

    p.SetLibraryState({}, false, false); // no folders set
    CHECK(p.Update(idle).dirty);
    const uint64_t no_dirs = hash();
    p.SetLibraryState({}, true, true); // scanning
    p.Update(idle);
    const uint64_t scanning = hash();
    p.SetLibraryState({}, true, false); // folders set, nothing found
    p.Update(idle);
    const uint64_t none_found = hash();
    p.SetLibraryState(MakeEntries(2), true, false);
    p.Update(idle);
    const uint64_t list = hash();

    CHECK(no_dirs != scanning);
    CHECK(scanning != none_found);
    CHECK(none_found != no_dirs);
    CHECK(list != none_found);

    // Nothing to launch from an empty list.
    p.SetLibraryState({}, true, false);
    CHECK_FALSE(p.Update(Point(TileRect(0), true)).launch_path.has_value());
}

TEST_CASE("picker: follows its Library and Rescan starts a scan", "[frame_xr][picker]") {
    const fs::path dir = fs::temp_directory_path() / ("cf_picker_lib_" + std::to_string(::getpid()));
    fs::create_directories(dir);
    for (const char* name : {"b.3ds", "a.cci", "c.cxi"}) {
        std::ofstream(dir / name) << "x";
    }

    auto lib = std::make_shared<Library>(
        [](const std::string& path) -> std::optional<GameMeta> {
            GameMeta m;
            m.title = FileStem(path);
            return m;
        },
        "");
    Picker p(lib);
    PickerInput idle;
    p.Update(idle);
    CHECK(p.Entries().empty());

    lib->SetDirs({{dir.string(), false}});
    // Rescan tile (the header button) kicks the worker off.
    p.Update(Point(RescanButton(), true));
    lib->WaitIdle();
    p.Update(idle);
    REQUIRE(p.Entries().size() == 3);
    CHECK(p.Entries()[0].title == "a");
    CHECK(p.Entries()[2].title == "c");
    CHECK_FALSE(lib->IsScanning());

    std::error_code ec;
    fs::remove_all(dir, ec);
}
