// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texdl: texture pack catalog parsing, host rules, Google Drive warning pages (fixture
// HTML), where the textures are inside an archive, the free space check, and installing an archive
// into the pack library (a stored zip written here). No network.

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include "core/cheats/online.h"
#include "core/custom_tex/download.h"
#include "core/custom_tex/packs.h"
#include "core/mods/archive.h"
#include "core/mods/catalog.h"

namespace fs = std::filesystem;
using namespace CustomTex::Download;

namespace {

constexpr u64 kTitle = 0x0004000000125500ULL;
const std::string kTid = "0004000000125500";

struct Temp {
    fs::path root;
    Temp() {
        static int n = 0;
        root = fs::temp_directory_path() / ("azf_texdl_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
        fs::remove_all(root);
        fs::create_directories(root / "load");
    }
    ~Temp() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    std::string load() const { return (root / "load").string(); }
};

void Le16(std::string& s, unsigned v) {
    s += static_cast<char>(v & 255);
    s += static_cast<char>((v >> 8) & 255);
}
void Le32(std::string& s, unsigned v) {
    Le16(s, v & 0xffff);
    Le16(s, v >> 16);
}

// A zip of stored (uncompressed) members.
std::string StoredZip(const std::vector<std::pair<std::string, std::string>>& members) {
    std::string out, central;
    for (const auto& [name, data] : members) {
        const unsigned offset = static_cast<unsigned>(out.size());
        const unsigned crc = Mods::Crc32(data);
        out += "PK\x03\x04";
        Le16(out, 20), Le16(out, 0), Le16(out, 0), Le16(out, 0), Le16(out, 0x21);
        Le32(out, crc), Le32(out, static_cast<unsigned>(data.size())), Le32(out, static_cast<unsigned>(data.size()));
        Le16(out, static_cast<unsigned>(name.size())), Le16(out, 0);
        out += name + data;
        central += "PK\x01\x02";
        Le16(central, 20), Le16(central, 20), Le16(central, 0), Le16(central, 0), Le16(central, 0), Le16(central, 0x21);
        Le32(central, crc), Le32(central, static_cast<unsigned>(data.size())), Le32(central, static_cast<unsigned>(data.size()));
        Le16(central, static_cast<unsigned>(name.size())), Le16(central, 0), Le16(central, 0), Le16(central, 0);
        Le16(central, 0), Le32(central, 0), Le32(central, offset);
        central += name;
    }
    const unsigned cd_offset = static_cast<unsigned>(out.size());
    out += central;
    out += "PK\x05\x06";
    Le16(out, 0), Le16(out, 0), Le16(out, static_cast<unsigned>(members.size())),
        Le16(out, static_cast<unsigned>(members.size()));
    Le32(out, static_cast<unsigned>(central.size())), Le32(out, cd_offset), Le16(out, 0);
    return out;
}

void WriteFile(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << data;
}

Entry MakeEntry(Layout layout, const std::string& id = "test-pack") {
    Entry e;
    e.id = id;
    e.name = "Test Pack";
    e.author = "Someone";
    e.title_ids = {kTitle};
    e.folder = "Test HD";
    e.layout = layout;
    e.source.type = Source::Type::Https;
    e.source.url = "https://example.com/pack.zip";
    return e;
}

} // namespace

TEST_CASE("texdl: the compiled-in catalog parses cleanly", "[texdl]") {
    std::vector<std::string> warnings;
    const auto cat = ParseCatalog(DefaultCatalogJson(), nullptr, &warnings);
    REQUIRE(cat);
    CHECK(warnings.empty());
    CHECK(cat->packs.size() >= 3);
    const Entry* mm = cat->Find("mm3d-hd-deathwrench");
    REQUIRE(mm != nullptr);
    CHECK(mm->source.type == Source::Type::GithubRelease);
    CHECK(mm->source.repo == "DeathWrench/MM3DHD");
    CHECK(mm->layout == Layout::Variants);
    CHECK(mm->ServesTitle(0x0004000000125500ULL));
    CHECK_FALSE(mm->ServesTitle(0x0004000000033500ULL));
    CHECK(mm->sha256.size() == 64);
    const Entry* xy = cat->Find("pokemon-xy-hd");
    REQUIRE(xy != nullptr);
    CHECK(xy->format == "7z");
    CHECK(xy->size == 1834769856ULL);
}

TEST_CASE("texdl: catalog entries that break the rules are dropped", "[texdl]") {
    const char* json = R"({"schema":1,"packs":[
      {"id":"ok-drive","name":"A","title_ids":["0004000000033500"],"source":{"type":"gdrive","file_id":"1GxvnzsPQznZ_v_6s-YMgETyLe5rN2Fak"}},
      {"id":"ok-https","name":"B","title_ids":["0004000000033500"],"source":{"type":"https","url":"https://example.com/a.zip","hosts":["cdn.example.com"]}},
      {"id":"Bad Id","name":"C","title_ids":["0004000000033500"],"source":{"type":"gdrive","file_id":"1GxvnzsPQznZ_v_6s-YMgETyLe5rN2Fak"}},
      {"id":"short-drive","name":"D","title_ids":["0004000000033500"],"source":{"type":"gdrive","file_id":"abc"}},
      {"id":"drive-folder-url","name":"E","title_ids":["0004000000033500"],"source":{"type":"gdrive","file_id":"https://drive.google.com/drive/folders/1QQ"}},
      {"id":"plain-http","name":"F","title_ids":["0004000000033500"],"source":{"type":"https","url":"http://example.com/a.zip"}},
      {"id":"wild-host","name":"G","title_ids":["0004000000033500"],"source":{"type":"https","url":"https://example.com/a.zip","hosts":["*.example.com"]}},
      {"id":"bad-tid","name":"H","title_ids":["123"],"source":{"type":"gdrive","file_id":"1GxvnzsPQznZ_v_6s-YMgETyLe5rN2Fak"}},
      {"id":"bad-sha","name":"I","title_ids":["0004000000033500"],"sha256":"zz","source":{"type":"gdrive","file_id":"1GxvnzsPQznZ_v_6s-YMgETyLe5rN2Fak"}},
      {"id":"dotdot","name":"J","title_ids":["0004000000033500"],"source":{"type":"https","url":"https://example.com/../a.zip"}},
      {"id":"ok-drive","name":"dup","title_ids":["0004000000033500"],"source":{"type":"gdrive","file_id":"1GxvnzsPQznZ_v_6s-YMgETyLe5rN2Fak"}}
    ]})";
    std::vector<std::string> warnings;
    const auto cat = ParseCatalog(json, nullptr, &warnings);
    REQUIRE(cat);
    REQUIRE(cat->packs.size() == 2);
    CHECK(cat->packs[0].id == "ok-drive");
    CHECK(cat->packs[0].source.type == Source::Type::Gdrive);
    CHECK(cat->packs[1].source.hosts == std::vector<std::string>{"cdn.example.com"});
    CHECK(warnings.size() == 9);
    CHECK_FALSE(ParseCatalog("not json"));
    CHECK_FALSE(ParseCatalog(R"({"mods":[]})"));
}

TEST_CASE("texdl: host rules", "[texdl]") {
    CHECK(HostAllowed("https://github.com"));
    CHECK(HostAllowed("https://release-assets.githubusercontent.com"));
    CHECK(HostAllowed("https://drive.usercontent.google.com"));
    CHECK(HostAllowed("https://drive.google.com"));
    CHECK_FALSE(HostAllowed("http://github.com"));
    CHECK_FALSE(HostAllowed("https://evil.example.com"));
    CHECK_FALSE(HostAllowed("https://github.com.evil.example.com"));
    CHECK_FALSE(HostAllowed("https://example.com"));
    CHECK(HostAllowed("https://example.com", {"example.com"}));
    CHECK_FALSE(HostAllowed("https://sub.example.com", {"example.com"}));
    std::string host, path;
    CHECK(SplitUrl("https://drive.google.com/uc?id=1", host, path));
    CHECK(host == "https://drive.google.com");
    CHECK(path == "/uc?id=1");
    CHECK_FALSE(SplitUrl("http://github.com/x", host, path));
    CHECK_FALSE(SplitUrl("https://user@github.com/x", host, path));
    CHECK_FALSE(SplitUrl("https://github.com:8443/x", host, path));
}

TEST_CASE("texdl: Google Drive ids and addresses", "[texdl]") {
    CHECK(DriveIdOk("1GxvnzsPQznZ_v_6s-YMgETyLe5rN2Fak"));
    CHECK_FALSE(DriveIdOk("short"));
    CHECK_FALSE(DriveIdOk("has/slash/in/the/id"));
    CHECK(DriveUrl("ABC1234567890") ==
          "https://drive.usercontent.google.com/download?id=ABC1234567890&export=download&confirm=t");
}

TEST_CASE("texdl: Drive confirm page gives the real address", "[texdl]") {
    const std::string html = R"(<!DOCTYPE html><html><head><title>Google Drive - Virus scan warning</title></head>
<body><div class="uc-main"><div id="uc-text"><p class="uc-warning-caption">Google Drive can't scan this file for viruses.</p>
<p class="uc-warning-subcaption"><span class="uc-name-size"><a href="/open?id=FILEID1234">Pack.zip</a> (1.3G)</span> is too large for Google to scan for viruses. Would you still like to download this file?</p>
<form id="download-form" action="https://drive.usercontent.google.com/download" method="get">
<input type="submit" id="uc-download-link" class="goog-inline-block jfk-button jfk-button-action" value="Download anyway"/>
<input type="hidden" name="id" value="FILEID1234">
<input type="hidden" name="export" value="download">
<input type="hidden" name="authuser" value="0">
<input type="hidden" name="confirm" value="t">
<input type="hidden" name="uuid" value="8c1f-aa&amp;bb">
<input type="hidden" name="at" value="AO=+x">
</form></div></div></body></html>)";
    const auto page = ParseDrivePage(html);
    REQUIRE(page.kind == DrivePage::Kind::Confirm);
    CHECK(page.url ==
          "https://drive.usercontent.google.com/download?id=FILEID1234&export=download&authuser=0&confirm=t&uuid=8c1f-aa%26bb&at=AO%3D%2Bx");
    std::string host, path;
    REQUIRE(SplitUrl(page.url, host, path));
    CHECK(HostAllowed(host));
}

TEST_CASE("texdl: Drive confirm form pointing at another host is refused", "[texdl]") {
    const std::string html = R"(<form id="download-form" action="https://evil.example.com/download" method="get">
<input type="hidden" name="id" value="X"></form>)";
    CHECK(ParseDrivePage(html).kind != DrivePage::Kind::Confirm);
}

TEST_CASE("texdl: Drive quota, sign-in and missing pages give clear errors", "[texdl]") {
    const auto quota = ParseDrivePage(
        "<html><body><p>Sorry, you can't view or download this file at this time.</p><p>Too many users have viewed or "
        "downloaded this file recently. Please try accessing the file again later.</p></body></html>");
    CHECK(quota.kind == DrivePage::Kind::Quota);
    CHECK(quota.error == "Google Drive: too many downloads of this file today, try again later");
    const auto quota2 = ParseDrivePage("<p>Download quota exceeded for this file</p>");
    CHECK(quota2.kind == DrivePage::Kind::Quota);
    const auto access = ParseDrivePage("<html><h1>You need access</h1><a>Request access</a></html>");
    CHECK(access.kind == DrivePage::Kind::NoAccess);
    CHECK(access.error == "Google Drive: the file is not shared publicly");
    const auto missing = ParseDrivePage("<p>Sorry, the file you have requested does not exist.</p>");
    CHECK(missing.kind == DrivePage::Kind::NotFound);
    const auto other = ParseDrivePage("<html>hello</html>");
    CHECK(other.kind == DrivePage::Kind::Unknown);
    CHECK_FALSE(other.error.empty());
}

TEST_CASE("texdl: finding the textures inside an archive", "[texdl]") {
    SECTION("wrapper folders are dropped") {
        const auto r = FindPackFolders({"Pack/readme.txt", "Pack/Textures/tex1_8x8_AB_1.png", "Pack/Textures/tex1_8x8_AC_1.dds"},
                                       Layout::Single);
        REQUIRE(r.size() == 1);
        CHECK(r[0].dir == "Pack/Textures");
        CHECK(r[0].name.empty());
    }
    SECTION("textures at the top") {
        const auto r = FindPackFolders({"tex1_8x8_AB_1.png", "notes.txt"}, Layout::Single);
        REQUIRE(r.size() == 1);
        CHECK(r[0].dir == "");
    }
    SECTION("subfolders of one pack stay together") {
        const auto r = FindPackFolders({"USUM/tex1_8x8_AB_1.png", "USUM/ui/tex1_8x8_AC_1.png", "USUM/more/tex1_8x8_AD_1.ktx"},
                                       Layout::Single);
        REQUIRE(r.size() == 1);
        CHECK(r[0].dir == "USUM");
    }
    SECTION("pack.json decides the root") {
        const auto r = FindPackFolders({"Outer/Inner/pack.json", "Outer/Inner/a/tex1_8x8_AB_1.png", "Outer/Inner/b/tex1_8x8_AC_1.png",
                                        "Outer/other.txt"},
                                       Layout::Single);
        REQUIRE(r.size() == 1);
        CHECK(r[0].dir == "Outer/Inner");
    }
    SECTION("a pack.json that does not cover everything is not the root") {
        const auto r = FindPackFolders({"A/pack.json", "A/x/tex1_8x8_AB_1.png", "B/tex1_8x8_AC_1.png"}, Layout::Single);
        REQUIRE(r.size() == 1);
        CHECK(r[0].dir == "");
    }
    SECTION("variants") {
        const auto r = FindPackFolders({"[Fonts] N64/tex1_8x8_AB_1.png", "[Link] Nerrel/tex1_8x8_AC_1.png", "[Link] Nerrel/sub/tex1_8x8_AD_1.png",
                                        "readme.txt"},
                                       Layout::Variants);
        REQUIRE(r.size() == 2);
        CHECK(r[0].dir == "[Fonts] N64");
        CHECK(r[0].name == "[Fonts] N64");
        CHECK(r[1].dir == "[Link] Nerrel");
    }
    SECTION("variants inside a wrapper") {
        const auto r = FindPackFolders({"Wrap/4K/tex1_8x8_AB_1.png", "Wrap/1080p/tex1_8x8_AC_1.png"}, Layout::Variants);
        REQUIRE(r.size() == 2);
        CHECK(r[0].dir == "Wrap/1080p");
        CHECK(r[1].dir == "Wrap/4K");
    }
    SECTION("variants with textures at the common parent are one pack") {
        const auto r = FindPackFolders({"tex1_8x8_AB_1.png", "x/tex1_8x8_AC_1.png"}, Layout::Variants);
        REQUIRE(r.size() == 1);
        CHECK(r[0].dir == "");
    }
    SECTION("single folder under variants is one pack") {
        const auto r = FindPackFolders({"Only/tex1_8x8_AB_1.png"}, Layout::Variants);
        REQUIRE(r.size() == 1);
        CHECK(r[0].dir == "Only");
        CHECK(r[0].name.empty());
    }
    SECTION("no textures, mac junk and other names") {
        CHECK(FindPackFolders({"a.txt", "b/c.png", "tex2_8x8_AB_1.png"}, Layout::Single).empty());
        CHECK(FindPackFolders({"__MACOSX/Pack/tex1_8x8_AB_1.png", "Pack/._tex1_8x8_AB_1.png"}, Layout::Single).empty());
        CHECK(FindPackFolders({"P/TEX1_8x8_AB_1.PNG"}, Layout::Single).size() == 1);
    }
}

TEST_CASE("texdl: free space check", "[texdl]") {
    // 1 GB archive: the rest of the download, the unpacked copy and 256 MB
    constexpr u64 gb = 1024ull * 1024 * 1024;
    CHECK(RequiredSpace(gb) == 2 * gb + 256 * 1024 * 1024);
    CHECK(RequiredSpace(gb, gb / 2) == gb / 2 + gb + 256 * 1024 * 1024);
    CHECK(RequiredSpace(gb, 2 * gb) == gb + 256 * 1024 * 1024);
    CHECK(FreeSpaceError(5 * gb, 6 * gb).empty());
    CHECK(FreeSpaceError(5 * gb, 5 * gb).empty());
    CHECK(FreeSpaceError(5 * gb, 1 * gb) == "Not enough free space: 5.0 GB needed, 1.0 GB free");
    CHECK(FreeSpaceError(300 * 1024 * 1024, 0) == "Not enough free space: 300 MB needed, 0 MB free");
    Temp t;
    const auto free = FreeSpace((fs::path(t.load()) / "not" / "made" / "yet").string());
    REQUIRE(free);
    CHECK(*free > 0);
    CHECK(SizeText(0).empty());
    CHECK(SizeText(78109346) == "74 MB");
    CHECK(SizeText(1834769856ull) == "1.7 GB");
}

TEST_CASE("texdl: folder names", "[texdl]") {
    CHECK(SanitizeFolderName("MM3D HD") == "MM3D HD");
    CHECK(SanitizeFolderName("a/b\\c:d") == "a_b_c_d");
    CHECK(SanitizeFolderName("..") == "Pack");
    CHECK(SanitizeFolderName("  x. ") == "x");
}

TEST_CASE("texdl: installing a zip into the pack library", "[texdl]") {
    Temp t;
    const auto zip = StoredZip({{"Wrapper/readme.txt", "hi"},
                                {"Wrapper/Textures/tex1_8x8_ABCDEF0123456789_1.png", "png"},
                                {"Wrapper/Textures/tex1_16x16_ABCDEF0123456780_1.png", "png2"}});
    const std::string archive = (fs::path(t.load()) / "x.zip").string();
    WriteFile(archive, zip);
    std::vector<std::string> installed;
    const auto error = InstallArchive(archive, MakeEntry(Layout::Single), kTitle, t.load(), nullptr, &installed);
    REQUIRE(error.empty());
    REQUIRE(installed == std::vector<std::string>{"Test HD"});
    const fs::path pack = fs::path(t.load()) / "texture-packs" / kTid / "Test HD";
    CHECK(fs::exists(pack / "tex1_8x8_ABCDEF0123456789_1.png"));
    CHECK(fs::exists(pack / "tex1_16x16_ABCDEF0123456780_1.png"));
    CHECK_FALSE(fs::exists(pack / "readme.txt"));
    CHECK(fs::exists(pack / ".azaharframe-download.json"));
    CHECK(fs::exists(pack / "pack.json"));
    CHECK_FALSE(fs::exists(fs::path(t.load()) / "texture-downloads" / "test-pack.x"));

    // the texdata pack list shows it
    CustomTex::Paths paths;
    paths.load_dir = t.load();
    const auto packs = CustomTex::ListPacks(paths, kTitle);
    REQUIRE(packs.size() == 1);
    CHECK(packs[0].name == "Test Pack");
    CHECK(packs[0].author == "Someone");
    CHECK(packs[0].managed);

    // the downloader lists it as installed, and deletes it
    Catalog cat;
    cat.packs.push_back(MakeEntry(Layout::Single));
    Downloader dl(t.load(), cat);
    auto list = dl.ListForTitle(kTitle);
    REQUIRE(list.size() == 1);
    CHECK(list[0].installed);
    CHECK(list[0].folders == std::vector<std::string>{"Test HD"});
    CHECK(dl.ListForTitle(0x0004000000033500ULL).empty());
    CHECK_FALSE(dl.Delete(kTitle, "../x").ok);
    CHECK_FALSE(dl.Delete(kTitle, "Nope").ok);
    CHECK(dl.Delete(kTitle, "Test HD").ok);
    CHECK_FALSE(fs::exists(pack));
    CHECK_FALSE(dl.ListForTitle(kTitle)[0].installed);

    // installing again replaces our own copy, never another folder
    REQUIRE(InstallArchive(archive, MakeEntry(Layout::Single), kTitle, t.load(), nullptr, &installed).empty());
    REQUIRE(InstallArchive(archive, MakeEntry(Layout::Single), kTitle, t.load(), nullptr, &installed).empty());
    CHECK(installed == std::vector<std::string>{"Test HD"});
}

TEST_CASE("texdl: variants become separate packs, a folder of the player's is never touched", "[texdl]") {
    Temp t;
    WriteFile(fs::path(t.load()) / "texture-packs" / kTid / "Test HD - Alpha" / "mine.txt", "mine");
    const auto zip = StoredZip({{"Alpha/tex1_8x8_ABCDEF0123456789_1.png", "a"},
                                {"Beta/tex1_8x8_ABCDEF0123456780_1.png", "b"},
                                {"Beta/deep/tex1_8x8_ABCDEF0123456781_1.png", "c"}});
    const std::string archive = (fs::path(t.load()) / "v.zip").string();
    WriteFile(archive, zip);
    std::vector<std::string> installed;
    REQUIRE(InstallArchive(archive, MakeEntry(Layout::Variants), kTitle, t.load(), nullptr, &installed).empty());
    REQUIRE(installed.size() == 2);
    CHECK(installed[0] == "Test HD - Alpha (2)");
    CHECK(installed[1] == "Test HD - Beta");
    CHECK(fs::exists(fs::path(t.load()) / "texture-packs" / kTid / "Test HD - Alpha" / "mine.txt"));
    CHECK(fs::exists(fs::path(t.load()) / "texture-packs" / kTid / "Test HD - Beta" / "deep" / "tex1_8x8_ABCDEF0123456781_1.png"));
    Catalog cat;
    cat.packs.push_back(MakeEntry(Layout::Variants));
    Downloader dl(t.load(), cat);
    CHECK(dl.ListForTitle(kTitle)[0].folders.size() == 2);
    // a folder with no marker is refused
    CHECK_FALSE(dl.Delete(kTitle, "Test HD - Alpha").ok);
    CHECK(fs::exists(fs::path(t.load()) / "texture-packs" / kTid / "Test HD - Alpha" / "mine.txt"));
}

TEST_CASE("texdl: bad archives are refused", "[texdl]") {
    Temp t;
    const std::string junk = (fs::path(t.load()) / "j.bin").string();
    WriteFile(junk, "this is not an archive at all");
    CHECK(InstallArchive(junk, MakeEntry(Layout::Single), kTitle, t.load(), nullptr, nullptr) ==
          "The download is not a zip, 7z or rar archive");
    const std::string empty = (fs::path(t.load()) / "e.zip").string();
    WriteFile(empty, StoredZip({{"a/readme.txt", "x"}}));
    CHECK(InstallArchive(empty, MakeEntry(Layout::Single), kTitle, t.load(), nullptr, nullptr) ==
          "No textures found in the download");
    CHECK_FALSE(fs::exists(fs::path(t.load()) / "texture-packs" / kTid));
    CHECK(InstallArchive((fs::path(t.load()) / "missing.zip").string(), MakeEntry(Layout::Single), kTitle, t.load(),
                         nullptr, nullptr) == "Cannot read the downloaded file");
}

TEST_CASE("texdl: Start refuses what it should", "[texdl]") {
    Temp t;
    Catalog cat;
    cat.packs.push_back(MakeEntry(Layout::Single));
    Downloader dl(t.load(), cat);
    CHECK_FALSE(dl.Start("unknown", kTitle));
    CHECK_FALSE(dl.Start("test-pack", 0x0004000000033500ULL)); // not for this title
    CHECK(dl.GetState().phase == State::Phase::Idle);
}

TEST_CASE("texdl: installing a 7z through bsdtar", "[texdl]") {
    const std::string tool = Mods::FindBsdtar();
    if (tool.empty()) {
        SKIP("no bsdtar");
    }
    Temp t;
    const fs::path src = fs::path(t.root) / "src";
    WriteFile(src / "USUM" / "tex1_8x8_ABCDEF0123456789_1.png", "png");
    WriteFile(src / "USUM" / "other.png", "o");
    const std::string archive = (fs::path(t.load()) / "x.7z").string();
    const std::string cmd = tool + " --format 7zip -cf '" + archive + "' -C '" + src.string() + "' USUM";
    if (std::system(cmd.c_str()) != 0) {
        SKIP("bsdtar cannot write 7z here");
    }
    std::vector<std::string> installed;
    REQUIRE(InstallArchive(archive, MakeEntry(Layout::Single), kTitle, t.load(), nullptr, &installed).empty());
    CHECK(fs::exists(fs::path(t.load()) / "texture-packs" / kTid / "Test HD" / "tex1_8x8_ABCDEF0123456789_1.png"));
    // a cancelled unpack reports it and leaves no pack
    std::atomic_bool cancel{true};
    fs::remove_all(fs::path(t.load()) / "texture-packs");
    CHECK(InstallArchive(archive, MakeEntry(Layout::Single), kTitle, t.load(), &cancel, nullptr) == "Cancelled");
    CHECK_FALSE(fs::exists(fs::path(t.load()) / "texture-packs" / kTid));
}

// Real downloads (hidden: run with "[.network]"). The GitHub one goes through the API, a redirect to
// GitHub's asset host, streaming, the checksum and the 7z unpack; the Drive one only fetches a small
// range of a public file to check the address and the answer.
TEST_CASE("texdl: real download of a small GitHub pack", "[.network]") {
    Temp t;
    const auto cat = DefaultCatalog();
    const Entry* usum = cat.Find("pokemon-usum-hd");
    REQUIRE(usum != nullptr);
    Catalog one;
    one.packs.push_back(*usum);
    Downloader dl(t.load(), one);
    REQUIRE(dl.Start("pokemon-usum-hd", 0x00040000001B5000ULL));
    for (int i = 0; i < 1200 && dl.GetState().Busy(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    const auto st = dl.GetState();
    INFO(st.error);
    REQUIRE(st.phase == State::Phase::Done);
    CHECK(st.installed == std::vector<std::string>{"USUM HD"});
    CHECK(fs::exists(fs::path(t.load()) / "texture-packs" / "00040000001B5000" / "USUM HD"));
    CHECK_FALSE(fs::exists(fs::path(t.load()) / "texture-downloads" / "pokemon-usum-hd.part"));
}

namespace {

// Condensed from the page https://drive.google.com/embeddedfolderview?id=<folder> (file entries as
// Drive writes them; the subfolder entry follows the same shape with a /drive/folders/ link).
const char* kFolderPage = R"(<html><head><title>OoT 3D 4K 5.0 Mirror 1</title></head><body><div class="flip-entries">
<div class="flip-entry" id="entry-1w1Ez_V2ky56Kc-Gz18cH3VnpraxiOD0Q" tabindex="0" role="link"><div class="flip-entry-info"><a href="https://drive.google.com/file/d/1w1Ez_V2ky56Kc-Gz18cH3VnpraxiOD0Q/view?usp=drive_web" target="_blank"><div class="flip-entry-visual"><div class="flip-entry-visual-card"><div class="flip-entry-icon"><img src="https://drive-thirdparty.googleusercontent.com/128/type/application/x-zip-compressed" alt="Compressed Archive"/></div></div></div><div class="flip-entry-list-icon"><img src="x" alt=""/></div><div class="flip-entry-title">OOT3D 4K 5.0a (1080p).zip</div></a></div><div class="flip-entry-last-modified"><div>Sep 4</div></div></div>
<div class="flip-entry" id="entry-1wwEdr86qgwV4Erp2W_phcDu73QlBWgzo" tabindex="0" role="link"><div class="flip-entry-info"><a href="https://drive.google.com/file/d/1wwEdr86qgwV4Erp2W_phcDu73QlBWgzo/view?usp=drive_web" target="_blank"><div class="flip-entry-visual"></div><div class="flip-entry-title">OOT3D 4K 5.0a (4K).zip</div></a></div><div class="flip-entry-last-modified"><div>Sep 4</div></div></div>
<div class="flip-entry" id="entry-1SubFolderIdSubFolderId0001" tabindex="0" role="link"><div class="flip-entry-info"><a href="https://drive.google.com/drive/folders/1SubFolderIdSubFolderId0001" target="_blank"><div class="flip-entry-title">Old &amp; Extra</div></a></div></div>
</div></body></html>)";

std::vector<DriveItem> Items(std::initializer_list<DriveItem> l) {
    return std::vector<DriveItem>(l);
}

} // namespace

TEST_CASE("texdl: Drive folder listing page", "[texdl]") {
    const auto items = ParseDriveFolder(kFolderPage);
    REQUIRE(items.size() == 3);
    CHECK(items[0].id == "1w1Ez_V2ky56Kc-Gz18cH3VnpraxiOD0Q");
    CHECK(items[0].name == "OOT3D 4K 5.0a (1080p).zip");
    CHECK_FALSE(items[0].is_folder);
    CHECK(items[1].name == "OOT3D 4K 5.0a (4K).zip");
    CHECK(items[2].is_folder);
    CHECK(items[2].name == "Old & Extra");
    CHECK(ParseDriveFolder("<html>nothing</html>").empty());
    CHECK(ParseDriveFolder("").empty());
}

TEST_CASE("texdl: picking a file from a Drive folder", "[texdl]") {
    const auto items = ParseDriveFolder(kFolderPage);
    int reads = 0;
    const DriveLister lister = [&](const std::string& id) -> std::optional<std::vector<DriveItem>> {
        ++reads;
        if (id == "ROOT123456") {
            return items;
        }
        if (id == "1SubFolderIdSubFolderId0001") {
            return Items({{"1DeepFileIdDeepFileId000001", "Extra pack (4K).zip", false},
                          {"1DeeperFolderIdDeeperFolder01", "deeper", true}});
        }
        if (id == "1DeeperFolderIdDeeperFolder01") {
            return Items({{"1VeryDeepFileIdVeryDeep00001", "Hidden (4K).zip", false},
                          {"1Deeper2FolderIdDeeper2Folder", "deeper2", true}});
        }
        if (id == "1Deeper2FolderIdDeeper2Folder") {
            return Items({{"1TooDeepFileIdTooDeepFile001", "Too Deep (4K).zip", false},
                          {"1Deeper3FolderIdDeeper3Folder", "deeper3", true}});
        }
        return std::nullopt;
    };
    std::string error;
    auto pick = PickDriveFile(lister, "ROOT123456", "OOT3D 4K * (4K).zip", &error);
    REQUIRE(pick);
    CHECK(pick->id == "1wwEdr86qgwV4Erp2W_phcDu73QlBWgzo");
    pick = PickDriveFile(lister, "ROOT123456", "OOT3D 4K * (1080p).zip", &error);
    REQUIRE(pick);
    CHECK(pick->id == "1w1Ez_V2ky56Kc-Gz18cH3VnpraxiOD0Q");
    // exact names work too, and a folder is never picked
    CHECK(PickDriveFile(lister, "ROOT123456", "OOT3D 4K 5.0a (4K).zip", &error));
    CHECK_FALSE(PickDriveFile(lister, "ROOT123456", "Old & Extra", &error));
    // subfolders are searched, to a depth cap
    pick = PickDriveFile(lister, "ROOT123456", "Extra pack*", &error);
    REQUIRE(pick);
    CHECK(pick->id == "1DeepFileIdDeepFileId000001");
    CHECK(PickDriveFile(lister, "ROOT123456", "Hidden*", &error));
    CHECK_FALSE(PickDriveFile(lister, "ROOT123456", "Too Deep*", &error, 2));
    // the cap on folder reads
    reads = 0;
    CHECK_FALSE(PickDriveFile(lister, "ROOT123456", "Nothing*", &error, 10, 2));
    CHECK(reads == 2);
    CHECK(error == "Google Drive: no file named \"Nothing*\" in the folder");
    // an unreadable folder
    CHECK_FALSE(PickDriveFile(lister, "OTHER", "x*", &error));
    CHECK(error == "Google Drive: the folder could not be read (is it shared publicly?)");
}

TEST_CASE("texdl: catalog sources for Drive folders and alternates", "[texdl]") {
    const char* json = R"({"schema":1,"packs":[
      {"id":"folder-ok","name":"A","title_ids":["0004000000033500"],
       "source":{"type":"gdrive_folder","folder_id":"1QQ-4b8SvR2d4VKKQNS-I1WOIB5EaTuzt","file_pattern":"OOT3D 4K * (4K).zip"},
       "alternates":[{"type":"gdrive_folder","folder_id":"1F2lJ_TJyZovp5TtKmqrl24fy97IdEh85","file_pattern":"OOT3D 4K * (4K).zip"}]},
      {"id":"folder-no-pattern","name":"B","title_ids":["0004000000033500"],"source":{"type":"gdrive_folder","folder_id":"1QQ-4b8SvR2d4VKKQNS-I1WOIB5EaTuzt"}},
      {"id":"folder-bad-id","name":"C","title_ids":["0004000000033500"],"source":{"type":"gdrive_folder","folder_id":"nope","file_pattern":"*.zip"}},
      {"id":"bad-alt","name":"D","title_ids":["0004000000033500"],"source":{"type":"gdrive","file_id":"1GxvnzsPQznZ_v_6s-YMgETyLe5rN2Fak"},
       "alternates":[{"type":"ftp","url":"ftp://x"}]},
      {"id":"alt-wild","name":"E","title_ids":["0004000000033500"],"source":{"type":"gdrive","file_id":"1GxvnzsPQznZ_v_6s-YMgETyLe5rN2Fak"},
       "alternates":[{"type":"https","url":"https://example.com/a.zip","hosts":["*.example.com"]}]}
    ]})";
    std::vector<std::string> warnings;
    const auto cat = ParseCatalog(json, nullptr, &warnings);
    REQUIRE(cat);
    REQUIRE(cat->packs.size() == 1);
    CHECK(warnings.size() == 4);
    const Entry& e = cat->packs[0];
    CHECK(e.source.type == Source::Type::GdriveFolder);
    CHECK(e.source.file_pattern == "OOT3D 4K * (4K).zip");
    REQUIRE(e.alternates.size() == 1);
    CHECK(e.alternates[0].folder_id == "1F2lJ_TJyZovp5TtKmqrl24fy97IdEh85");
}

TEST_CASE("texdl: the Henriko Magnifico entries", "[texdl]") {
    const auto cat = DefaultCatalog();
    // the file names Drive listed on 2026-11-11
    const std::vector<std::pair<std::string, std::string>> listed = {
        {"oot3d-4k-henriko", "OOT3D 4K 5.0a (4K).zip"},      {"oot3d-1080p-henriko", "OOT3D 4K 5.0a (1080p).zip"},
        {"mm3d-4k-henriko", "MM 3D 4K 3.0b-1 (4K).zip"},     {"mm3d-1080p-henriko", "MM 3D 4K 3.0b-1 (1080p).zip"},
        {"sm3dl-4k-henriko", "SM3DL 4K 1.2.0b (4K).zip"},    {"sm3dl-1080p-henriko", "SM3DL 4K 1.2.0b (1080p).zip"},
        {"albw-4k-henriko", "ALBW 4K 2.0b (4K).zip"},        {"albw-1080p-henriko", "ALBW 4K 2.0b (1080p).zip"}};
    const std::vector<std::string> all_names = {"OOT3D 4K 5.0a (4K).zip",      "OOT3D 4K 5.0a (1080p).zip",
                                                "MM 3D 4K 3.0b-1 (4K).zip",    "MM 3D 4K 3.0b-1 (1080p).zip",
                                                "SM3DL 4K 1.2.0b (4K).zip",    "SM3DL 4K 1.2.0b (1080p).zip",
                                                "ALBW 4K 2.0b (4K).zip",       "ALBW 4K 2.0b (1080p).zip"};
    for (const auto& [id, name] : listed) {
        const Entry* e = cat.Find(id);
        REQUIRE(e != nullptr);
        CHECK(e->source.type == Source::Type::GdriveFolder);
        CHECK(e->author == "Henriko Magnifico");
        CHECK(e->author_url.rfind("https://www.henrikomagnifico.com/", 0) == 0);
        int matches = 0;
        for (const auto& n : all_names) {
            matches += Mods::GlobMatch(e->source.file_pattern, n) ? 1 : 0;
        }
        CHECK(matches == 1); // the pattern picks its own variant and no other
        CHECK(Mods::GlobMatch(e->source.file_pattern, name));
    }
    CHECK(cat.Find("mm3d-4k-henriko")->ServesTitle(0x0004000000125600ULL));
    CHECK(cat.Find("sm3dl-1080p-henriko")->ServesTitle(0x0004000000054000ULL));
    CHECK(cat.Find("albw-4k-henriko")->ServesTitle(0x00040000000EC300ULL));
    CHECK(cat.Find("oot3d-4k-henriko")->alternates.size() == 1);
}

// Real Drive folder listing and download of the smallest Henriko pack (107 MB), then every Henriko
// folder listed only (hidden: "[.network]").
TEST_CASE("texdl: real Drive folder download (SM3DL 1080p)", "[.network]") {
    Temp t;
    const auto cat = DefaultCatalog();
    Catalog one;
    one.packs.push_back(*cat.Find("sm3dl-1080p-henriko"));
    Downloader dl(t.load(), one);
    REQUIRE(dl.Start("sm3dl-1080p-henriko", 0x0004000000054000ULL));
    for (int i = 0; i < 2400 && dl.GetState().Busy(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    const auto st = dl.GetState();
    INFO(st.error);
    REQUIRE(st.phase == State::Phase::Done);
    REQUIRE(st.installed.size() == 1);
    const fs::path pack = fs::path(t.load()) / "texture-packs" / "0004000000054000" / st.installed[0];
    CHECK(fs::exists(pack / "pack.json"));
    std::size_t textures = 0;
    for (const auto& f : fs::recursive_directory_iterator(pack)) {
        textures += f.path().filename().string().rfind("tex1_", 0) == 0 ? 1 : 0;
    }
    CHECK(textures > 50);
}

TEST_CASE("texdl: every Henriko pack is found in its live Drive folder", "[.network]") {
    const auto cat = DefaultCatalog();
    for (const auto& e : cat.packs) {
        if (e.source.type != Source::Type::GdriveFolder) {
            continue;
        }
        const auto lister = [](const std::string& id) -> std::optional<std::vector<DriveItem>> {
            const auto r = Cheats::Online::HttpsGetRaw("https://drive.google.com", "/embeddedfolderview?id=" + id, 2 << 20);
            if (r.status != 200) {
                return std::nullopt;
            }
            return ParseDriveFolder(r.body);
        };
        std::string error;
        const auto item = PickDriveFile(lister, e.source.folder_id, e.source.file_pattern, &error);
        INFO(e.id << ": " << error);
        CHECK(item);
    }
}
