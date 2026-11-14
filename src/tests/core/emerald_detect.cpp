// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emhb: Pokémon Emerald 3DS found from its files (an install this program made, one
// copied from elsewhere, an installed title alone, the build's data alone, broken ones) and what
// Uninstall removes, in throwaway folders. No ROM and no Emerald 3DS file: the game files are
// made-up ones with only the headers that are read.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <catch2/catch_test_macros.hpp>
#include "core/homebrew/emerald3ds.h"
#include "core/homebrew/emerald3ds_detect.h"

namespace E = Homebrew::Emerald3ds;
namespace fs = std::filesystem;

namespace {

// The German ROM (one game language, so a pack built from it names its language)
constexpr const char* kDeSha1 = "61c2eb2b380b1a75f0c94b767a2d4c26cd7ce4e3";
constexpr const char* kEnSha1 = "f3ae088181bf583e55daf962a92bb46f4f1d07b7";

struct Scratch {
    fs::path root;
    explicit Scratch(const char* name) {
        root = fs::temp_directory_path() / (std::string("emhb-test-") + name);
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root / "sdmc");
        fs::create_directories(root / "user");
        fs::create_directories(root / "games");
    }
    ~Scratch() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    E::Paths Paths() const {
        return E::PlanPaths((root / "sdmc").string(), (root / "user").string());
    }
    std::string Games() const {
        return (root / "games").string() + "/";
    }
    std::string TitleRoot() const {
        return (root / "sdmc" / "Nintendo 3DS" / "0" / "0" / "title").string() + "/";
    }
    E::DetectInput Input() const {
        E::DetectInput in;
        in.paths = Paths();
        in.game_dirs = {{Games(), false}};
        in.title_roots = {TitleRoot(), ""};
        return in;
    }
};

void Write(const std::string& path, const std::string& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path, std::ios::binary) << data;
}

bool Exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

void Put16(std::string& s, std::size_t at, unsigned v) {
    s[at] = static_cast<char>(v & 0xFF);
    s[at + 1] = static_cast<char>((v >> 8) & 0xFF);
}

void Put32(std::string& s, std::size_t at, unsigned v) {
    for (int i = 0; i < 4; i++) {
        s[at + i] = static_cast<char>((v >> (8 * i)) & 0xFF);
    }
}

/// An SMDH with these English titles (ASCII).
std::string Smdh(const std::string& short_title, const std::string& long_title,
                 const std::string& publisher) {
    std::string s(0x36C0, '\0');
    s.replace(0, 4, "SMDH");
    const auto put = [&s](std::size_t at, const std::string& text) {
        for (std::size_t i = 0; i < text.size(); i++) {
            Put16(s, at + i * 2, static_cast<unsigned char>(text[i]));
        }
    };
    const std::size_t english = 8 + 0x200;
    put(english, short_title);
    put(english + 0x80, long_title);
    put(english + 0x180, publisher);
    return s;
}

std::string EmeraldSmdh(const std::string& version = "v0.5.0") {
    return Smdh("Emerald 3DS", "Pokemon Emerald 3DS " + version, "ZallaxDev");
}

/// A .3dsx with its SMDH in the extended header.
std::string ThreeDsx(const std::string& smdh) {
    std::string s(0x40, '\0');
    s.replace(0, 4, "3DSX");
    Put16(s, 4, 0x2C);
    Put32(s, 0x20, 0x40);
    Put32(s, 0x24, static_cast<unsigned>(smdh.size()));
    return s + smdh;
}

/// An unencrypted NCCH (an installed title's .app) with the SMDH as its ExeFS icon.
std::string Ncch(const std::string& smdh) {
    std::string s(0x400, '\0');
    s.replace(0x100, 4, "NCCH");
    s[0x18F] = 0x4;         // NoCrypto
    Put32(s, 0x1A0, 1);     // ExeFS at 0x200
    Put32(s, 0x1A4, 0x20);  // its size in media units
    s.replace(0x200, 4, "icon");
    Put32(s, 0x208, 0);
    Put32(s, 0x20C, static_cast<unsigned>(smdh.size()));
    return s + smdh;
}

/// A .cia with no certificates or content, only the meta section's icon.
std::string Cia(const std::string& smdh) {
    std::string s(0x2440, '\0');
    Put32(s, 0, 0x2020);
    Put32(s, 0x14, static_cast<unsigned>(0x400 + smdh.size()));
    return s + smdh;
}

std::string Pak(const char* rom_sha1, unsigned abi = 0x1234abcd) {
    std::string s(64, '\0');
    s.replace(0, 8, std::string("EM3DPAK\0", 8));
    Put32(s, 8, 1);
    Put32(s, 12, abi);
    for (int i = 0; i < 20; i++) {
        s[16 + i] = static_cast<char>(std::stoi(std::string(rom_sha1 + i * 2, 2), nullptr, 16));
    }
    Put32(s, 36, 3);
    return s + std::string(256, 'p');
}

std::string StateJson(const std::string& rom_dir_copy) {
    E::State st;
    st.tag = "v0.5.0";
    st.language = "pt";
    st.rom_sha1 = kEnSha1;
    st.data_abi = "1234abcd";
    st.rom_dir_copy = rom_dir_copy;
    return E::SerializeState(st);
}

/// The full layout an install of this program leaves (sdmc files, the copy, state, ROM, caches).
void AppMade(const Scratch& s) {
    const E::Paths p = s.Paths();
    Write(p.three_dsx, ThreeDsx(EmeraldSmdh()));
    Write(p.smdh, EmeraldSmdh());
    Write(p.pak, Pak(kEnSha1));
    Write(s.Games() + "Emerald3DS.3dsx", ThreeDsx(EmeraldSmdh()));
    Write(p.state_file, StateJson(s.Games() + "Emerald3DS.3dsx"));
    Write(p.rom_copy, std::string(1024, 'r'));
    Write(p.data_dir + "followers-cache/ab/cd.bin", "follower");
    Write(p.data_dir + "fallback-choices.json", "{}");
    Write(p.save, "save");
    Write(p.app_dir + "emerald3ds.clock", "clock");
    Write(p.backup_root + "20261111-221500/emerald3ds.sav", "old save");
    Write(p.dspfirm, E::DspPlaceholder());
}

std::set<std::string> Norm(const std::vector<std::string>& v) {
    std::set<std::string> out;
    for (std::string s : v) {
        while (s.size() > 1 && s.back() == '/') {
            s.pop_back();
        }
        out.insert(fs::path(s).lexically_normal().generic_string());
    }
    return out;
}

std::string N(std::string s) {
    while (s.size() > 1 && s.back() == '/') {
        s.pop_back();
    }
    return fs::path(s).lexically_normal().generic_string();
}

} // namespace

TEST_CASE("Emerald detect: names, titles, versions and the pack's header", "[emhb]") {
    CHECK(E::IsEmeraldName("Emerald3DS.3dsx"));
    CHECK(E::IsEmeraldName("Emerald3DS-de.3dsx"));
    CHECK(E::IsEmeraldName("emerald 3ds (v0.5.0).cia"));
    CHECK(E::IsEmeraldName("Pokémon Emerald 3DS.3dsx"));
    CHECK_FALSE(E::IsEmeraldName("Pokemon Emerald.gba"));
    CHECK_FALSE(E::IsEmeraldName("Emerald.3dsx"));
    CHECK_FALSE(E::IsEmeraldName("ftpd.3dsx"));

    CHECK(E::IsEmeraldTitle({"Emerald 3DS", "", ""}));
    CHECK(E::IsEmeraldTitle({"Pokémon Emerald", "A 3D remake", "ZallaxDev"}));
    CHECK_FALSE(E::IsEmeraldTitle({"Pokémon Emerald", "", "Nintendo"}));
    CHECK_FALSE(E::IsEmeraldTitle({"Checkpoint", "", "FlagBrew"}));

    CHECK(E::VersionInText("Pokemon Emerald 3DS v0.5.0") == "v0.5.0");
    CHECK(E::VersionInText("Emerald 3DS V1.2 by ZallaxDev") == "v1.2");
    CHECK(E::VersionInText("dev build") == "");
    CHECK(E::VersionInText("Emerald 3DS v2") == "");    // not dotted
    CHECK(E::VersionInText("nav0.5.0") == "");          // inside a word
    CHECK(E::VersionInText("v1.2.3.4") == "");          // too many parts

    const std::string pak = Pak(kDeSha1, 0x00c0ffee);
    const auto h = E::ParsePakHeader(std::span(reinterpret_cast<const u8*>(pak.data()), pak.size()));
    REQUIRE(h.has_value());
    CHECK(h->abi == "00c0ffee");
    CHECK(h->rom_sha1 == kDeSha1);
    CHECK(h->entries == 3);
    std::string bad = pak;
    bad[3] = 'X';
    CHECK_FALSE(E::ParsePakHeader(std::span(reinterpret_cast<const u8*>(bad.data()), bad.size())));
}

TEST_CASE("Emerald detect: the SMDH of a .smdh, .3dsx, .cia and an installed title", "[emhb]") {
    Scratch s("smdh");
    const std::string dir = s.root.string() + "/";
    Write(dir + "a.smdh", EmeraldSmdh());
    Write(dir + "b.3dsx", ThreeDsx(EmeraldSmdh("v0.6.1")));
    Write(dir + "c.cia", Cia(Smdh("Emerald 3DS", "", "ZallaxDev")));
    Write(dir + "d.app", Ncch(Smdh("Emerald 3DS", "", "ZallaxDev")));
    std::string encrypted = Ncch(EmeraldSmdh());
    encrypted[0x18F] = 0;
    Write(dir + "e.app", encrypted);
    std::string old_3dsx = ThreeDsx(EmeraldSmdh());
    Put16(old_3dsx, 4, 0x20); // no extended header
    Write(dir + "f.3dsx", old_3dsx);

    const auto a = E::ReadGameSmdh(dir + "a.smdh");
    REQUIRE(a);
    CHECK(a->short_title == "Emerald 3DS");
    CHECK(a->publisher == "ZallaxDev");
    const auto b = E::ReadGameSmdh(dir + "b.3dsx");
    REQUIRE(b);
    CHECK(E::VersionInText(b->long_title) == "v0.6.1");
    REQUIRE(E::ReadGameSmdh(dir + "c.cia"));
    CHECK(E::IsEmeraldTitle(*E::ReadGameSmdh(dir + "c.cia")));
    REQUIRE(E::ReadGameSmdh(dir + "d.app"));
    CHECK(E::IsEmeraldTitle(*E::ReadGameSmdh(dir + "d.app")));
    CHECK_FALSE(E::ReadGameSmdh(dir + "e.app"));
    CHECK_FALSE(E::ReadGameSmdh(dir + "f.3dsx"));
    CHECK_FALSE(E::ReadGameSmdh(dir + "missing.3dsx"));
}

TEST_CASE("Emerald detect: nothing there, or only a save", "[emhb]") {
    Scratch s("none");
    E::Detection d = E::Detect(s.Input());
    CHECK_FALSE(d.Any());
    CHECK_FALSE(d.playable);
    CHECK(d.Problem().empty());

    const E::Paths p = s.Paths();
    Write(p.save, "save");
    Write(p.dspfirm, E::DspPlaceholder());
    Write(p.rom_copy, "rom"); // the ROM's copy is the player's, not an install
    Write(s.Games() + "Pokemon Emerald.gba", "rom");
    d = E::Detect(s.Input());
    CHECK_FALSE(d.Any());
    CHECK(d.saves.size() == 1);
}

TEST_CASE("Emerald detect: an install this program made", "[emhb]") {
    Scratch s("appmade");
    AppMade(s);
    const E::Paths p = s.Paths();
    const E::Detection d = E::Detect(s.Input());
    CHECK(d.app_made);
    CHECK(d.playable);
    CHECK_FALSE(d.Broken());
    CHECK(d.version == "v0.5.0");
    CHECK(d.language == "pt"); // state.json's choice, not the pack's ROM
    CHECK(d.data_abi == "1234abcd");
    CHECK(N(d.game_path) == N(s.Games() + "Emerald3DS.3dsx"));
    CHECK(N(d.game_copy) == N(s.Games() + "Emerald3DS.3dsx"));
    CHECK(N(d.three_dsx) == N(p.three_dsx));
    CHECK(d.Has(E::Piece::SdmcGame));
    CHECK(d.Has(E::Piece::GameCopy));
    CHECK(d.Has(E::Piece::Smdh));
    CHECK(d.Has(E::Piece::Pak));
    CHECK(d.Has(E::Piece::State));
    CHECK(d.Has(E::Piece::FollowersCache));
    CHECK(d.Has(E::Piece::Choices));
    CHECK(Norm(d.saves) == Norm({p.save, p.app_dir + "emerald3ds.clock", p.backup_root}));
    for (const E::Found& f : d.found) {
        CHECK(N(f.path) != N(p.rom_copy));
        CHECK(N(f.path) != N(p.dspfirm));
    }
}

TEST_CASE("Emerald detect: a copy from another computer, with no state.json", "[emhb]") {
    Scratch s("imported");
    const E::Paths p = s.Paths();
    Write(p.three_dsx, ThreeDsx(EmeraldSmdh("v0.4.2")));
    Write(p.pak, Pak(kDeSha1, 0xdeadbeef));
    Write(p.save, "save");
    // the game list's copy under another name, known by its SMDH; other games left alone
    Write(s.Games() + "Pokemon E3D.3dsx", ThreeDsx(EmeraldSmdh("v0.4.2")));
    Write(s.Games() + "ftpd.3dsx", ThreeDsx(Smdh("ftpd", "FTP server", "mtheall")));
    Write(s.Games() + "Pokemon Emerald.gba", "rom");
    const E::Detection d = E::Detect(s.Input());
    CHECK_FALSE(d.app_made);
    CHECK(d.playable);
    CHECK(d.version == "v0.4.2"); // the SMDH's
    CHECK(d.language == "de");    // the pack's ROM
    CHECK(d.data_abi == "deadbeef");
    CHECK(d.rom_sha1 == kDeSha1);
    CHECK(N(d.game_path) == N(s.Games() + "Pokemon E3D.3dsx"));
    CHECK(N(d.game_copy) == N(s.Games() + "Pokemon E3D.3dsx"));
    CHECK(N(d.three_dsx) == N(p.three_dsx));
    std::size_t copies = 0;
    for (const E::Found& f : d.found) {
        copies += f.piece == E::Piece::GameCopy;
        CHECK(fs::path(f.path).filename() != "ftpd.3dsx");
        CHECK(fs::path(f.path).extension() != ".gba");
    }
    CHECK(copies == 1);

    SECTION("only on the SD card, in a folder of its own, with an English pack: no language") {
        Scratch t("imported-sd");
        const std::string dir = t.root.string() + "/sdmc/3ds/Emerald3DS/";
        Write(dir + "Emerald3DS.3dsx", ThreeDsx(Smdh("Emerald 3DS", "", "ZallaxDev")));
        Write(dir + "emerald3ds.pak", Pak(kEnSha1));
        const E::Detection e = E::Detect(t.Input());
        CHECK(e.playable);
        CHECK(e.version.empty());
        CHECK(e.language.empty()); // English or Português (Brasil)
        CHECK(N(e.game_path) == N(dir + "Emerald3DS.3dsx"));
        CHECK(N(e.three_dsx) == N(dir + "Emerald3DS.3dsx"));
        CHECK(e.game_copy.empty());
    }
    SECTION("a deep game folder") {
        Scratch t("imported-deep");
        const E::Paths q = t.Paths();
        Write(q.pak, Pak(kEnSha1));
        Write(t.Games() + "3DS/Homebrew/Emerald3DS.3dsx", ThreeDsx(EmeraldSmdh()));
        E::DetectInput in = t.Input();
        CHECK_FALSE(E::Detect(in).playable); // not scanned: game file missing
        in.game_dirs = {{t.Games(), true}};
        CHECK(E::Detect(in).playable);
    }
}

TEST_CASE("Emerald detect: an installed title alone, and a .cia", "[emhb]") {
    Scratch s("title");
    const std::string title = s.TitleRoot() + "00040000/0e3d5000/";
    Write(title + "content/00000000.tmd", "tmd");
    Write(title + "content/00000000.app", Ncch(Smdh("Emerald 3DS", "", "ZallaxDev")));
    Write(title + "data/00000001.sav", "title save");
    // another installed title, left alone
    Write(s.TitleRoot() + "00040000/00030700/content/00000000.app", Ncch(Smdh("Mario Kart 7", "", "Nintendo")));
    E::Detection d = E::Detect(s.Input());
    CHECK(d.launchable);
    CHECK_FALSE(d.has_pak);
    CHECK(d.Broken()); // the game reads its pack from the SD card
    CHECK(d.Problem() == "Game Data Missing");
    CHECK(N(d.game_path) == N(title + "content/00000000.app"));
    REQUIRE(d.found.size() == 1);
    CHECK(d.found[0].piece == E::Piece::Title);
    CHECK(Norm(d.saves) == Norm({title + "data/"}));

    Write(s.Paths().pak, Pak(kEnSha1));
    Write(s.Games() + "Emerald 3DS.cia", Cia(EmeraldSmdh()));
    d = E::Detect(s.Input());
    CHECK(d.playable);
    CHECK(N(d.game_path) == N(s.Games() + "Emerald 3DS.cia")); // the game list's file first
}

TEST_CASE("Emerald detect: the build's data alone", "[emhb]") {
    Scratch s("data");
    const E::Paths p = s.Paths();
    Write(p.work_dir + "out/emerald3ds.pak", "half");
    Write(p.data_dir + "followers-cache/x.bin", "f");
    Write(p.rom_copy, "rom");
    const E::Detection d = E::Detect(s.Input());
    CHECK(d.Any());
    CHECK(d.Broken());
    CHECK_FALSE(d.launchable);
    CHECK(d.Problem() == "Game File Missing");
    CHECK(d.game_path.empty());
    CHECK(d.Has(E::Piece::Work));
    CHECK(d.Has(E::Piece::FollowersCache));
}

TEST_CASE("Emerald detect: broken installs", "[emhb]") {
    SECTION("a damaged pack") {
        Scratch s("broken-pak");
        const E::Paths p = s.Paths();
        Write(p.three_dsx, ThreeDsx(EmeraldSmdh()));
        Write(p.pak, "not a pack");
        const E::Detection d = E::Detect(s.Input());
        CHECK(d.Broken());
        CHECK(d.Problem() == "Game Data Missing");
        CHECK(d.Has(E::Piece::Pak)); // still removed by Uninstall
    }
    SECTION("state.json, the game gone") {
        Scratch s("broken-state");
        const E::Paths p = s.Paths();
        Write(p.state_file, StateJson(s.Games() + "Emerald3DS.3dsx"));
        Write(p.pak, Pak(kEnSha1));
        Write(p.three_dsx + ".new", "half");
        const E::Detection d = E::Detect(s.Input());
        CHECK(d.app_made);
        CHECK(d.Broken());
        CHECK(d.Problem() == "Game File Missing");
        CHECK(d.version == "v0.5.0");
        CHECK(d.Has(E::Piece::Temp));
    }
    SECTION("a state.json that cannot be read") {
        Scratch s("broken-json");
        const E::Paths p = s.Paths();
        Write(p.state_file, "{");
        Write(p.three_dsx, ThreeDsx(EmeraldSmdh()));
        Write(p.pak, Pak(kEnSha1));
        const E::Detection d = E::Detect(s.Input());
        CHECK_FALSE(d.app_made); // treated as a copy: still installed
        CHECK(d.playable);
        CHECK(d.version == "v0.5.0");
    }
}

TEST_CASE("Emerald detect: the variant a .3dsx comes from", "[emhb]") {
    E::Manifest m;
    E::Variant en, de;
    en.language = "en";
    en.three_dsx_sha256 = std::string(64, 'a');
    de.language = "de";
    de.three_dsx_sha256 = std::string(64, 'b');
    m.variants = {en, de};
    REQUIRE(E::FindVariantByThreeDsx(m, std::string(64, 'B')));
    CHECK(E::FindVariantByThreeDsx(m, std::string(64, 'B'))->language == "de");
    CHECK_FALSE(E::FindVariantByThreeDsx(m, std::string(64, 'c')));
    CHECK_FALSE(E::FindVariantByThreeDsx(m, ""));
}

TEST_CASE("Emerald uninstall: only the install's files go; the ROM and the save stay", "[emhb]") {
    Scratch s("uninstall");
    AppMade(s);
    const E::Paths p = s.Paths();
    // not Emerald 3DS's, or the player's: never removed
    Write(s.Games() + "Pokemon Emerald.gba", "rom");
    Write(s.Games() + "ftpd.3dsx", ThreeDsx(Smdh("ftpd", "", "mtheall")));
    Write(p.app_dir + "settings.ini", "the game's own");
    Write(s.root.string() + "/sdmc/3ds/ftpd/ftpd.3dsx", "other");

    const E::Detection d = E::Detect(s.Input());
    const E::UninstallPlan plan = E::PlanUninstall(d, p, false);
    const std::set<std::string> want_files = Norm(
        {p.three_dsx, p.smdh, p.pak, s.Games() + "Emerald3DS.3dsx", p.state_file,
         p.data_dir + "fallback-choices.json"});
    CHECK(Norm(plan.files) == want_files);
    CHECK(Norm(plan.dirs) == Norm({p.data_dir + "followers-cache/"}));
    CHECK(Norm(plan.kept) ==
          Norm({p.save, p.app_dir + "emerald3ds.clock", p.backup_root, p.rom_copy}));

    const E::UninstallResult r = E::RunUninstall(plan);
    CHECK(r.Ok());
    for (const std::string& f : want_files) {
        CHECK_FALSE(Exists(f));
    }
    CHECK_FALSE(Exists(p.data_dir + "followers-cache"));
    // kept
    CHECK(Exists(p.rom_copy));
    CHECK(Exists(s.Games() + "Pokemon Emerald.gba"));
    CHECK(Exists(p.save));
    CHECK(Exists(p.app_dir + "emerald3ds.clock"));
    CHECK(Exists(p.backup_root + "20261111-221500/emerald3ds.sav"));
    CHECK(Exists(p.dspfirm));
    CHECK(Exists(p.app_dir + "settings.ini"));
    CHECK(Exists(s.Games() + "ftpd.3dsx"));
    CHECK(Exists(s.root.string() + "/sdmc/3ds/ftpd/ftpd.3dsx"));
    // and nothing of the install is left to find
    const E::Detection after = E::Detect(s.Input());
    CHECK_FALSE(after.Any());
    CHECK(after.saves.size() == 3);
}

TEST_CASE("Emerald uninstall: Also Delete Save Data, only when ticked", "[emhb]") {
    Scratch s("uninstall-save");
    AppMade(s);
    const E::Paths p = s.Paths();
    Write(s.Games() + "Pokemon Emerald.gba", "rom");
    const E::Detection d = E::Detect(s.Input());
    const E::UninstallPlan plan = E::PlanUninstall(d, p, true);
    CHECK(Norm(plan.kept) == Norm({p.rom_copy})); // the ROM's copy, even now
    const E::UninstallResult r = E::RunUninstall(plan);
    CHECK(r.Ok());
    CHECK_FALSE(Exists(p.save));
    CHECK_FALSE(Exists(p.app_dir + "emerald3ds.clock"));
    CHECK_FALSE(Exists(p.backup_root));
    CHECK_FALSE(Exists(p.app_dir)); // empty now: gone
    CHECK(Exists(p.rom_copy));
    CHECK(Exists(p.data_dir)); // it holds the ROM's copy
    CHECK(Exists(s.Games() + "Pokemon Emerald.gba"));
    CHECK(Exists(p.dspfirm));
}

TEST_CASE("Emerald uninstall: an imported copy and an installed title", "[emhb]") {
    Scratch s("uninstall-imported");
    const E::Paths p = s.Paths();
    const std::string dir = s.root.string() + "/sdmc/3ds/Emerald3DS/";
    Write(dir + "Emerald3DS.3dsx", ThreeDsx(EmeraldSmdh()));
    Write(p.pak, Pak(kEnSha1));
    Write(p.save, "save");
    const std::string title = s.TitleRoot() + "00040000/0e3d5000/";
    Write(title + "content/00000000.app", Ncch(EmeraldSmdh()));
    Write(title + "data/00000001.sav", "title save");
    Write(s.Games() + "Emerald3DS.cia", Cia(EmeraldSmdh()));

    E::Detection d = E::Detect(s.Input());
    REQUIRE(d.playable);
    E::UninstallResult r = E::RunUninstall(E::PlanUninstall(d, p, false));
    CHECK(r.Ok());
    CHECK_FALSE(Exists(dir)); // the folder it came in, empty
    CHECK_FALSE(Exists(p.pak));
    CHECK_FALSE(Exists(s.Games() + "Emerald3DS.cia"));
    CHECK_FALSE(Exists(title + "content"));
    CHECK(Exists(title + "data/00000001.sav")); // the title's save
    CHECK(Exists(p.save));
    CHECK_FALSE(E::Detect(s.Input()).Any());

    // ticked: the title's save goes with it
    Write(title + "content/00000000.app", Ncch(EmeraldSmdh()));
    d = E::Detect(s.Input());
    r = E::RunUninstall(E::PlanUninstall(d, p, true));
    CHECK(r.Ok());
    CHECK_FALSE(Exists(title));
    CHECK_FALSE(Exists(p.save));
}

TEST_CASE("Emerald uninstall: never a ROM, whatever the list says", "[emhb]") {
    Scratch s("uninstall-guard");
    const E::Paths p = s.Paths();
    Write(p.rom_copy, "rom");
    Write(s.Games() + "emerald3ds.gba", "rom");
    Write(p.dspfirm, "dsp");
    E::Detection d;
    d.found = {{E::Piece::GameCopy, s.Games() + "emerald3ds.gba"},
               {E::Piece::Temp, p.rom_copy},
               {E::Piece::Work, p.data_dir},
               {E::Piece::Temp, p.dspfirm}};
    const E::UninstallPlan plan = E::PlanUninstall(d, p, true);
    CHECK(plan.files.empty());
    CHECK(plan.dirs.empty());
    E::RunUninstall(plan);
    CHECK(Exists(p.rom_copy));
    CHECK(Exists(s.Games() + "emerald3ds.gba"));
    CHECK(Exists(p.dspfirm));
}

TEST_CASE("Emerald uninstall: the dialog's text", "[emhb]") {
    CHECK(E::UninstallBody(true, false) ==
          "Removes the game and its build files. Your ROM and save data are kept.");
    CHECK(E::UninstallBody(true, true) ==
          "Removes the game, its build files and your save data. Your ROM is kept.");
    CHECK(E::UninstallBody(false, false) == "Removes the game and its build files. Your ROM is kept.");
}
