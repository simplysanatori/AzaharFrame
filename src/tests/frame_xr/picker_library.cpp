// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "frame_xr/picker/library.h"

using namespace frame_xr::picker;
namespace fs = std::filesystem;

namespace {

// A scratch folder removed on destruction.
struct TempDir {
    fs::path path;
    TempDir() {
        static std::atomic<int> counter{0};
        path = fs::temp_directory_path() /
               ("cf_picker_test_" + std::to_string(::getpid()) + "_" + std::to_string(counter++));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    std::string Touch(const std::string& rel, const std::string& content = "x") const {
        const fs::path p = path / rel;
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << content;
        return p.string();
    }
    std::string Str() const {
        return path.string();
    }
};

GameMeta MakeMeta(const std::string& title, uint8_t shade = 7) {
    GameMeta m;
    m.title = title;
    m.icon_w = kIconSize;
    m.icon_h = kIconSize;
    m.icon_rgba.assign(kIconSize * kIconSize * 4, shade);
    return m;
}

} // namespace

TEST_CASE("picker: extension filter matches Azahar's game list", "[frame_xr][picker]") {
    for (const char* ext : {"3ds", "cci", "cxi", "3dsx", "app", "elf", "axf", "z3dsx", "zcci",
                            "zcxi", "bcci"}) {
        INFO(ext);
        CHECK(HasSupportedExtension(std::string("/games/Some Game.") + ext));
    }
    CHECK(HasSupportedExtension("/games/Loud.CCI"));
    CHECK(HasSupportedExtension("/games/Mixed.3dSx"));

    // Installable CIAs are not in the game list; neither are other files.
    CHECK_FALSE(HasSupportedExtension("/games/Installer.cia"));
    CHECK_FALSE(HasSupportedExtension("/games/readme.txt"));
    CHECK_FALSE(HasSupportedExtension("/games/save.sav"));
    CHECK_FALSE(HasSupportedExtension("/games/noextension"));
    CHECK_FALSE(HasSupportedExtension("/games.3ds/noextension"));
    CHECK_FALSE(HasSupportedExtension("/games/archive.3ds.zip"));
    CHECK_FALSE(HasSupportedExtension(""));
}

TEST_CASE("picker: FileStem", "[frame_xr][picker]") {
    CHECK(FileStem("/a/b/Mario Kart 7.cci") == "Mario Kart 7");
    CHECK(FileStem("Game.3ds") == "Game");
    CHECK(FileStem("/a/b/noext") == "noext");
    CHECK(FileStem("/a.b/noext") == "noext");
}

TEST_CASE("picker: Rgb565ToRgba", "[frame_xr][picker]") {
    const auto rgba = Rgb565ToRgba({0xF800, 0x07E0, 0x001F, 0x0000, 0xFFFF});
    REQUIRE(rgba.size() == 20);
    CHECK(std::vector<uint8_t>(rgba.begin(), rgba.begin() + 4) ==
          std::vector<uint8_t>{255, 0, 0, 255});
    CHECK(std::vector<uint8_t>(rgba.begin() + 4, rgba.begin() + 8) ==
          std::vector<uint8_t>{0, 255, 0, 255});
    CHECK(std::vector<uint8_t>(rgba.begin() + 8, rgba.begin() + 12) ==
          std::vector<uint8_t>{0, 0, 255, 255});
    CHECK(std::vector<uint8_t>(rgba.begin() + 12, rgba.begin() + 16) ==
          std::vector<uint8_t>{0, 0, 0, 255});
    CHECK(std::vector<uint8_t>(rgba.begin() + 16, rgba.begin() + 20) ==
          std::vector<uint8_t>{255, 255, 255, 255});
}

TEST_CASE("picker: FindGameFiles walks folders", "[frame_xr][picker]") {
    TempDir t;
    const std::string zelda = t.Touch("a/Zelda.3ds");
    t.Touch("a/notes.txt");
    t.Touch("a/Installer.cia");
    const std::string mario = t.Touch("a/sub/Mario.cci");
    const std::string kart = t.Touch("a/sub/deep/Kart.cxi");
    const std::string pilot = t.Touch("b/Pilot.3dsx");

    SECTION("not recursive") {
        const auto files = FindGameFiles({{t.Str() + "/a", false}});
        REQUIRE(files.size() == 1);
        CHECK(files[0] == zelda);
    }
    SECTION("recursive") {
        auto files = FindGameFiles({{t.Str() + "/a", true}});
        std::sort(files.begin(), files.end());
        std::vector<std::string> want{kart, mario, zelda};
        std::sort(want.begin(), want.end());
        CHECK(files == want);
    }
    SECTION("depth override") {
        const auto files = FindGameFiles({{t.Str() + "/a", false, 1}});
        CHECK(files.size() == 2); // Zelda and Mario, not Kart
    }
    SECTION("overlapping and repeated folders list a file once") {
        const auto files = FindGameFiles({{t.Str() + "/a", true},
                                          {t.Str() + "/a/sub", true},
                                          {t.Str() + "/a", false},
                                          {t.Str() + "/b", false}});
        CHECK(files.size() == 4);
        CHECK(std::count(files.begin(), files.end(), mario) == 1);
        CHECK(std::count(files.begin(), files.end(), pilot) == 1);
    }
    SECTION("missing and empty folders are skipped") {
        const auto files = FindGameFiles({{t.Str() + "/nope", true}, {"", true}});
        CHECK(files.empty());
    }
    SECTION("cancel stops the walk") {
        const auto files = FindGameFiles({{t.Str() + "/a", true}}, [] { return true; });
        CHECK(files.empty());
    }
}

TEST_CASE("picker: SortEntries is case-insensitive then by path", "[frame_xr][picker]") {
    std::vector<GameEntry> e(4);
    e[0].title = "zelda";
    e[0].path = "/z";
    e[1].title = "Animal Crossing";
    e[1].path = "/a";
    e[2].title = "mario";
    e[2].path = "/m2";
    e[3].title = "Mario";
    e[3].path = "/m1";
    SortEntries(e);
    CHECK(e[0].title == "Animal Crossing");
    CHECK(e[1].path == "/m1"); // "Mario" == "mario": path decides
    CHECK(e[2].path == "/m2");
    CHECK(e[3].title == "zelda");
}

TEST_CASE("picker: icon cache round trip", "[frame_xr][picker]") {
    TempDir t;
    IconCache cache(t.Str() + "/cache");

    CHECK_FALSE(cache.Load("/g/a.3ds", 100, 5).has_value());
    cache.Store("/g/a.3ds", 100, 5, MakeMeta("Game A", 9));

    const auto hit = cache.Load("/g/a.3ds", 100, 5);
    REQUIRE(hit.has_value());
    CHECK(hit->title == "Game A");
    CHECK(hit->icon_w == kIconSize);
    CHECK(hit->icon_rgba == MakeMeta("", 9).icon_rgba);

    // A changed file (time or size) or another path misses.
    CHECK_FALSE(cache.Load("/g/a.3ds", 101, 5).has_value());
    CHECK_FALSE(cache.Load("/g/a.3ds", 100, 6).has_value());
    CHECK_FALSE(cache.Load("/g/b.3ds", 100, 5).has_value());

    // An entry without an icon keeps its title.
    GameMeta no_icon;
    no_icon.title = "No Icon";
    cache.Store("/g/n.3ds", 1, 1, no_icon);
    const auto n = cache.Load("/g/n.3ds", 1, 1);
    REQUIRE(n.has_value());
    CHECK(n->title == "No Icon");
    CHECK(n->icon_rgba.empty());

    // A damaged file is ignored, not trusted.
    const fs::path file = fs::path(t.Str()) / "cache" / IconCache::KeyName("/g/a.3ds", 100, 5);
    REQUIRE(fs::exists(file));
    fs::resize_file(file, fs::file_size(file) - 10);
    CHECK_FALSE(cache.Load("/g/a.3ds", 100, 5).has_value());

    // A disabled cache does nothing.
    IconCache off("");
    off.Store("/g/a.3ds", 1, 1, MakeMeta("x"));
    CHECK_FALSE(off.Load("/g/a.3ds", 1, 1).has_value());
}

TEST_CASE("picker: Library scans, filters and sorts on a worker thread", "[frame_xr][picker]") {
    TempDir t;
    t.Touch("games/zelda.3ds");
    t.Touch("games/Mario.cci");
    t.Touch("games/broken.cxi");
    t.Touch("games/notitle.3dsx");
    t.Touch("games/readme.txt");

    std::atomic<int> reads{0};
    Library lib(
        [&](const std::string& path) -> std::optional<GameMeta> {
            reads++;
            if (path.find("broken") != std::string::npos) {
                return std::nullopt; // not a bootable application
            }
            if (path.find("zelda") != std::string::npos) {
                return MakeMeta("The Legend of Zelda");
            }
            if (path.find("Mario") != std::string::npos) {
                return MakeMeta("mario kart");
            }
            return GameMeta{}; // no title, no icon
        },
        t.Str() + "/cache");

    CHECK_FALSE(lib.HasDirs());
    lib.SetDirs({{t.Str() + "/games", false}});
    CHECK(lib.HasDirs());
    CHECK(lib.Snapshot()->empty());

    const uint64_t before = lib.Generation();
    lib.Rescan();
    lib.WaitIdle();
    CHECK_FALSE(lib.IsScanning());
    CHECK(lib.Generation() > before);

    const auto list = lib.Snapshot();
    REQUIRE(list->size() == 3);
    CHECK((*list)[0].title == "mario kart");
    CHECK((*list)[1].title == "notitle"); // file name stands in
    CHECK((*list)[2].title == "The Legend of Zelda");
    CHECK((*list)[0].icon_w == kIconSize);
    CHECK((*list)[1].icon_rgba.empty());
    CHECK(reads == 4); // readme.txt is never opened

    SECTION("a second scan reads from the icon cache") {
        reads = 0;
        lib.Rescan();
        lib.WaitIdle();
        // Only the file that was rejected is read again; accepted ones are cached.
        CHECK(reads == 1);
        CHECK(lib.Snapshot()->size() == 3);
    }
    SECTION("a changed file is read again") {
        const std::string zelda = t.Str() + "/games/zelda.3ds";
        std::ofstream(zelda, std::ios::binary | std::ios::app) << "more bytes";
        reads = 0;
        lib.Rescan();
        lib.WaitIdle();
        CHECK(reads == 2); // zelda (new size) and the rejected file
    }
    SECTION("new files appear after a rescan") {
        t.Touch("games/Added.3ds");
        lib.Rescan();
        lib.WaitIdle();
        CHECK(lib.Snapshot()->size() == 4);
    }
    SECTION("removing the folders empties the list") {
        lib.SetDirs({});
        CHECK_FALSE(lib.HasDirs());
        lib.Rescan();
        lib.WaitIdle();
        CHECK(lib.Snapshot()->empty());
    }
}

TEST_CASE("picker: Library survives rapid rescans and destruction mid-scan", "[frame_xr][picker]") {
    TempDir t;
    for (int i = 0; i < 40; i++) {
        t.Touch("g/Game" + std::to_string(i) + ".3ds");
    }
    {
        Library lib([](const std::string& p) { return std::optional<GameMeta>(MakeMeta(p)); }, "");
        lib.SetDirs({{t.Str() + "/g", false}});
        for (int i = 0; i < 20; i++) {
            lib.Rescan();
        }
        lib.WaitIdle();
        CHECK_FALSE(lib.IsScanning());
        CHECK(lib.Snapshot()->size() == 40);
        lib.Rescan();
    } // destructor joins the worker while it may still be scanning
}
