// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ctgp7: the CIA made from a cartridge image (a synthetic NCSD, read back with Azahar's
// own CIA reader), the segment's visibility rule, the checks before an install, the choice of the
// official installer file and its download (a fake transport), and the plugin loader's run-only
// setting (rollback safe: the config keeps the player's value).

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <catch2/catch_test_macros.hpp>
#include "common/file_util.h"
#include "common/settings.h"
#include "core/ctgp7/cia_build.h"
#include "core/ctgp7/ctgp7.h"
#include "core/file_sys/cia_container.h"
#include "core/hle/service/am/am.h" // AzaharFrame ctgp7b
#include "core/hle/service/fs/archive.h"
#include "core/homebrew/net.h"
#include "core/loader/loader.h"

namespace fs = std::filesystem;
using namespace Ctgp7;

namespace {

constexpr u64 kMk7Usa = 0x0004000000030800ULL;

std::string TempDir(const char* name) {
    const std::string dir = std::string("/tmp/azf_ctgp7_test_") + name + "/";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

std::string ReadAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void WriteAll(const std::string& path, const std::string& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path, std::ios::binary) << data;
}

void Le32(std::string& b, std::size_t at, u32 v) {
    for (int i = 0; i < 4; i++) {
        b[at + i] = static_cast<char>(v >> (8 * i));
    }
}

/// A game NCCH of `units` media units: header, exheader (save size), then pseudo-random bytes.
std::string MakeNcch(u64 title_id, u16 version, u32 units, bool decrypted, u32 save_size) {
    std::string n(static_cast<std::size_t>(units) * 0x200, '\0');
    std::mt19937 rng(1234);
    for (std::size_t i = 0x600; i < n.size(); i++) {
        n[i] = static_cast<char>(rng());
    }
    n.replace(0x100, 4, "NCCH");
    Le32(n, 0x104, units);
    n[0x112] = static_cast<char>(version & 0xFF);
    n[0x113] = static_cast<char>(version >> 8);
    Le32(n, 0x118, static_cast<u32>(title_id));
    Le32(n, 0x11C, static_cast<u32>(title_id >> 32));
    Le32(n, 0x180, 0x400); // exheader size
    n[0x188 + 7] = decrypted ? 0x04 : 0x00;
    Le32(n, 0x200 + 0x1C0, save_size);
    return n;
}

/// An NCSD (.3ds) with the NCCH as partition 0 at 0x4000 and a manual partition after it.
std::string MakeNcsd(const std::string& ncch) {
    std::string img(0x4000, '\0');
    img.replace(0x100, 4, "NCSD");
    const u32 p0_units = static_cast<u32>(ncch.size() / 0x200);
    Le32(img, 0x120, 0x4000 / 0x200);
    Le32(img, 0x124, p0_units);
    Le32(img, 0x128, 0x4000 / 0x200 + p0_units); // partition 1 (the manual): not taken
    Le32(img, 0x12C, 2);
    img += ncch;
    img += std::string(0x400, 'M');
    Le32(img, 0x104, static_cast<u32>(img.size() / 0x200));
    return img;
}

std::string Hex(const std::array<u8, 32>& a) {
    static const char* d = "0123456789abcdef";
    std::string out;
    for (const u8 b : a) {
        out += d[b >> 4];
        out += d[b & 15];
    }
    return out;
}

} // namespace

TEST_CASE("CTGP-7: a cartridge image becomes a CIA Azahar's installer reads", "[ctgp7]") {
    const std::string dir = TempDir("cia");
    const std::string ncch = MakeNcch(kMk7Usa, 3, 0x900, true, 0x80000);
    const std::string rom = dir + "mk7.3ds";
    WriteAll(rom, MakeNcsd(ncch));

    const Cia::Source s = Cia::Inspect(rom);
    REQUIRE(s.error.empty());
    CHECK(s.kind == Cia::Kind::Ncsd);
    CHECK(s.title_id == kMk7Usa);
    CHECK(s.version == 3);
    CHECK(s.offset == 0x4000);
    CHECK(s.size == ncch.size());
    CHECK(s.save_size == 0x80000);

    const std::string out = dir + "mk7.cia";
    u64 last = 0;
    REQUIRE(Cia::Build(rom, out, nullptr, [&](u64 d, u64 t) {
                CHECK(t == ncch.size());
                last = d;
            }).empty());
    CHECK(last == ncch.size());
    CHECK_FALSE(FileUtil::Exists(out + ".part"));

    // Azahar's own reader: the header, the ticket, the TMD, the content
    FileUtil::IOFile f(out, "rb");
    FileSys::CIAContainer c;
    REQUIRE(c.Load(&f) == Loader::ResultStatus::Success);
    CHECK(c.GetCertificateSize() == 0);
    CHECK(c.GetTicket().GetTitleID() == kMk7Usa);
    const auto& tmd = c.GetTitleMetadata();
    CHECK(tmd.GetTitleID() == kMk7Usa);
    CHECK(tmd.GetTitleVersion() == 3);
    CHECK(tmd.GetContentCount() == 1);
    CHECK(tmd.GetContentSizeByIndex(0) == ncch.size());
    CHECK(Hex(tmd.GetContentHashByIndex(0)) == Homebrew::Sha256Hex(ncch));
    CHECK_FALSE(tmd.HasEncryptedContent());
    CHECK(tmd.VerifyContentInfoHashes());
    // the content is partition 0, byte for byte, and nothing of the manual partition
    const std::string cia = ReadAll(out);
    REQUIRE(cia.size() == c.GetContentOffset(0) + ncch.size());
    CHECK(cia.compare(c.GetContentOffset(0), ncch.size(), ncch) == 0);
    // the image was only read
    CHECK(ReadAll(rom) == MakeNcsd(ncch));
}

TEST_CASE("CTGP-7: images that can't be converted say why", "[ctgp7]") {
    const std::string dir = TempDir("cia_bad");
    SECTION("an encrypted cartridge") {
        WriteAll(dir + "enc.3ds", MakeNcsd(MakeNcch(kMk7Usa, 0, 0x10, false, 0)));
        CHECK(Cia::Inspect(dir + "enc.3ds").error == "This Copy Is Encrypted");
        CHECK(Cia::Build(dir + "enc.3ds", dir + "enc.cia", nullptr, {}) == "This Copy Is Encrypted");
        CHECK_FALSE(FileUtil::Exists(dir + "enc.cia"));
    }
    SECTION("not a 3DS image") {
        WriteAll(dir + "x.3ds", std::string(0x1000, 'x'));
        CHECK(Cia::Inspect(dir + "x.3ds").error == "Not a 3DS Game Image");
    }
    SECTION("a cut-off cartridge") {
        std::string img = MakeNcsd(MakeNcch(kMk7Usa, 0, 0x40, true, 0));
        img.resize(0x5000);
        WriteAll(dir + "cut.3ds", img);
        CHECK(Cia::Inspect(dir + "cut.3ds").error == "The Cartridge Image Is Incomplete");
    }
    SECTION("a bare NCCH (.cxi) is used whole") {
        const std::string ncch = MakeNcch(kMk7Usa, 1, 0x20, true, 0);
        WriteAll(dir + "g.cxi", ncch);
        const auto s = Cia::Inspect(dir + "g.cxi");
        CHECK(s.error.empty());
        CHECK(s.kind == Cia::Kind::Ncch);
        CHECK(s.size == ncch.size());
        REQUIRE(Cia::Build(dir + "g.cxi", dir + "g.cia", nullptr, {}).empty());
        // and a CIA is recognised as one (installed as it is)
        const auto c = Cia::Inspect(dir + "g.cia");
        CHECK(c.error.empty());
        CHECK(c.kind == Cia::Kind::Cia);
        CHECK(c.title_id == kMk7Usa);
        CHECK(c.version == 1);
    }
    SECTION("Cancel leaves nothing") {
        WriteAll(dir + "c.3ds", MakeNcsd(MakeNcch(kMk7Usa, 0, 0x20, true, 0)));
        std::atomic_bool cancel{true};
        CHECK(Cia::Build(dir + "c.3ds", dir + "c.cia", &cancel, {}) == "Cancelled");
        CHECK_FALSE(FileUtil::Exists(dir + "c.cia"));
        CHECK_FALSE(FileUtil::Exists(dir + "c.cia.part"));
    }
}

TEST_CASE("CTGP-7: the segment shows only for Mario Kart 7 CTGP-7 supports", "[ctgp7]") {
    for (const u64 tid : {0x0004000000030800ULL, 0x0004000000030700ULL, 0x0004000000030600ULL,
                          0x0004000000030A00ULL}) {
        CHECK(SegmentShown(tid, false));
        CHECK(IsSupportedMarioKart7(tid));
    }
    CHECK(RegionOf(0x0004000000030800ULL) == Region::Usa);
    CHECK(RegionOf(0x0004000000030A00ULL) == Region::Kor);
    // the Chinese and Taiwanese releases, another game, no game: hidden
    for (const u64 tid : {0x000400000008B400ULL, 0x000400000008B500ULL, 0x0004000000033500ULL, 0ULL,
                          0x0004000E00030800ULL /* the update title */}) {
        CHECK_FALSE(SegmentShown(tid, false));
    }
    // CTGP-7's launcher (and Mario Kart 7 after it), and the installer's run
    CHECK(SegmentShown(kLauncherTitleId, false));
    CHECK(SegmentShown(0, true));
    CHECK(SegmentShown(0x0004000000033500ULL, true));
    CHECK(UpdateTitleOf(0x0004000000030800ULL) == 0x0004000E00030800ULL);
}

TEST_CASE("CTGP-7: what stops an install before it starts", "[ctgp7]") {
    GameCheck g;
    g.title_id = kMk7Usa;
    CHECK(BlockerOf(g) == Blocker::Update); // no update
    g.update_version = 1040;                // 1.1
    CHECK(BlockerOf(g) == Blocker::Update);
    g.update_version = kUpdateV12;
    CHECK(BlockerOf(g) == Blocker::SaveData);
    g.save_data = true;
    CHECK(BlockerOf(g) == Blocker::None);
    g.update_version = 4096; // anything later
    CHECK(BlockerOf(g) == Blocker::None);
    g.title_id = 0x000400000008B400ULL;
    CHECK(BlockerOf(g) == Blocker::NotMarioKart7);
    CHECK_FALSE(UpdateReady(std::nullopt));
    CHECK(UpdateReady(3072));
}

namespace {
std::string GbFile(int id, const std::string& name, const std::string& av, bool archived,
                   long long added, const std::string& md5 = "ABCDEF", u64 size = 3,
                   const std::string& url = "") {
    return "{\"_idRow\":" + std::to_string(id) + ",\"_sFile\":\"" + name + "\",\"_nFilesize\":" +
           std::to_string(size) + ",\"_tsDateAdded\":" + std::to_string(added) +
           ",\"_sDownloadUrl\":\"" +
           (url.empty() ? "https://gamebanana.com/dl/" + std::to_string(id) : url) +
           "\",\"_sMd5Checksum\":\"" + md5 + "\",\"_sAvResult\":\"" + av +
           "\",\"_bIsArchived\":" + (archived ? "true" : "false") + "}";
}
std::string GbPage(const std::string& files) {
    return "{\"_sName\":\"CTGP-7\",\"_aFiles\":[" + files + "]}";
}
} // namespace

TEST_CASE("CTGP-7: the official offline installer is the file picked", "[ctgp7]") {
    const std::string page = GbPage(
        GbFile(1865990, "ctgp-7_cia_online_installer_d768d.zip", "clean", false, 1793121569) + "," +
        GbFile(1866025, "ctgp-7_cia_offline_installer.zip", "clean", false, 1793122926) + "," +
        GbFile(1591547, "ctgp-7_exe_online_installer.zip", "clean", false, 1767106447) + "," +
        GbFile(2, "ctgp-7_cia_offline_installer_new.zip", "infected", false, 1800000000) + "," +
        GbFile(3, "ctgp-7_cia_offline_installer_old.zip", "clean", true, 1800000001) + "," +
        GbFile(4, "ctgp-7_cia_offline_installer_x.zip", "clean", false, 1800000002, "", 3,
               "https://example.com/x.zip"));
    std::string error;
    const auto f = PickInstaller(page, &error);
    REQUIRE(f.has_value());
    CHECK(f->name == "ctgp-7_cia_offline_installer.zip");
    CHECK(f->url == "https://gamebanana.com/dl/1866025");
    CHECK(f->md5 == "abcdef");
    CHECK_FALSE(PickInstaller(GbPage(GbFile(1, "ctgp-7_cia_online_installer.zip", "clean", false, 1)),
                              &error));
    CHECK(error == "The CTGP-7 installer isn't on its GameBanana page");
    CHECK_FALSE(PickInstaller("{\"_bIsTrashed\":true,\"_aFiles\":[]}", &error));
    CHECK_FALSE(PickInstaller("not json", &error));

    CHECK(DownloadHostAllowed("https://gamebanana.com"));
    CHECK(DownloadHostAllowed("https://files.gamebanana.com"));
    CHECK(DownloadHostAllowed("https://filecache40.gamebanana.com"));
    CHECK_FALSE(DownloadHostAllowed("https://filecachex.gamebanana.com"));
    CHECK_FALSE(DownloadHostAllowed("https://evil.com"));
    CHECK_FALSE(DownloadHostAllowed("https://gamebanana.com.evil.com"));
    CHECK_FALSE(DownloadHostAllowed("http://gamebanana.com"));
}

TEST_CASE("CTGP-7: the download follows GameBanana's redirects and checks the file", "[ctgp7]") {
    const std::string dir = TempDir("fetch");
    const Paths p = PlanPaths(dir + "sdmc/", dir + "user/");
    const std::string zip = "PK-fake-installer";
    const std::string md5 = "4197fb3281c20dc42d99df5882639f70"; // md5("PK-fake-installer")
    struct Hop {
        int status;
        std::string location, body;
    };
    std::map<std::string, Hop> net;
    std::vector<std::string> log;
    const auto get = [&](const std::string& host, const std::string& path, const std::string& dest,
                         u64 resume, const std::function<void(u64, u64)>& progress) {
        log.push_back(host + path);
        GetReply r;
        const auto it = net.find(host + path);
        if (it == net.end()) {
            r.status = 404;
            return r;
        }
        r.status = it->second.status;
        r.location = it->second.location;
        if (r.status == 200) {
            WriteAll(dest, it->second.body);
            r.total = it->second.body.size();
            if (progress) {
                progress(r.total, r.total);
            }
        }
        (void)resume;
        return r;
    };
    const std::string page_key = "https://gamebanana.com/apiv11/Mod/50221/ProfilePage";
    REQUIRE(FileMd5("/dev/null") == "d41d8cd98f00b204e9800998ecf8427e");
    SECTION("a good download") {
        WriteAll(dir + "probe", zip);
        REQUIRE(FileMd5(dir + "probe") == md5);
        net[page_key] = {200, "", GbPage(GbFile(1866025, "ctgp-7_cia_offline_installer.zip", "clean",
                                                false, 5, md5, zip.size()))};
        net["https://gamebanana.com/dl/1866025"] = {302, "https://files.gamebanana.com/mods/c.zip", ""};
        net["https://files.gamebanana.com/mods/c.zip"] = {302, "https://filecache7.gamebanana.com/mods/c.zip", ""};
        net["https://filecache7.gamebanana.com/mods/c.zip"] = {200, "", zip};
        const auto r = FetchInstaller(p, get, nullptr, {});
        INFO(r.error);
        REQUIRE(r.error.empty());
        CHECK(ReadAll(r.zip) == zip);
        CHECK(log.size() == 4);
        CHECK_FALSE(fs::exists(p.work + "profile.json"));
    }
    SECTION("a redirect away from GameBanana is refused") {
        net[page_key] = {200, "", GbPage(GbFile(1866025, "ctgp-7_cia_offline_installer.zip", "clean",
                                                false, 5, md5, zip.size()))};
        net["https://gamebanana.com/dl/1866025"] = {302, "https://mirror.example.com/c.zip", ""};
        const auto r = FetchInstaller(p, get, nullptr, {});
        CHECK(r.error == "The Download Left GameBanana");
        CHECK(log.size() == 2);
    }
    SECTION("a damaged download is deleted") {
        net[page_key] = {200, "", GbPage(GbFile(1866025, "ctgp-7_cia_offline_installer.zip", "clean",
                                                false, 5, "00000000000000000000000000000000",
                                                zip.size()))};
        net["https://gamebanana.com/dl/1866025"] = {200, "", zip};
        const auto r = FetchInstaller(p, get, nullptr, {});
        CHECK(r.error == "The Download Is Damaged");
        CHECK_FALSE(fs::exists(p.work + "ctgp-7_cia_offline_installer.zip.part"));
        CHECK_FALSE(fs::exists(p.work + "ctgp-7_cia_offline_installer.zip"));
    }
    SECTION("offline") {
        const auto r = FetchInstaller(p, get, nullptr, {});
        CHECK_FALSE(r.error.empty());
    }
}

TEST_CASE("CTGP-7: the plugin loader is on for the launcher's run only", "[ctgp7]") {
    auto& loader = Settings::values.plugin_loader_enabled;
    auto& allow = Settings::values.allow_plugin_loader;
    loader.SetValue(false);
    allow.SetValue(false);
    CHECK(RunOverride::LoaderToSave(false) == false);
    RunOverride::Begin();
    CHECK(RunOverride::Active());
    CHECK(loader.GetValue());
    CHECK(allow.GetValue());
    // a config save in the run writes the player's own values (an older build reading the same
    // config.ini sees them)
    CHECK(RunOverride::LoaderToSave(loader.GetValue()) == false);
    CHECK(RunOverride::AllowToSave(allow.GetValue()) == false);
    RunOverride::Begin(); // again: nothing
    // the launcher's own plg:ldr call writes the setting too
    loader.SetValue(true);
    RunOverride::End();
    CHECK_FALSE(RunOverride::Active());
    CHECK(loader.GetValue() == false);
    CHECK(allow.GetValue() == false);
    RunOverride::End(); // again: nothing
    CHECK(loader.GetValue() == false);
    // the player's own "on" stays on
    loader.SetValue(true);
    allow.SetValue(true);
    RunOverride::Begin();
    RunOverride::End();
    CHECK(loader.GetValue());
    CHECK(allow.GetValue());
    CHECK(RunOverride::LoaderToSave(true) == true);
    loader.SetValue(false);
}

TEST_CASE("CTGP-7: paths and space", "[ctgp7]") {
    const Paths p = PlanPaths("/u/sdmc", "/u");
    CHECK(p.dir == "/u/sdmc/CTGP-7/");
    CHECK(p.version_file == "/u/sdmc/CTGP-7/config/version.bin");
    CHECK(p.plugin == "/u/sdmc/CTGP-7/resources/CTGP-7.3gx");
    CHECK(p.work == "/u/ctgp7/");
    // the zip and the installer at once, the installer and its files at once; the game twice
    CHECK(RequiredBytes(800, 0) >= 1800);
    CHECK(RequiredBytes(800, 1000) > RequiredBytes(800, 0));
    const std::string dir = TempDir("version");
    const Paths q = PlanPaths(dir + "sdmc/", dir);
    WriteAll(q.version_file, "1.6.30\n");
    CHECK(InstalledVersion(q) == "1.6.30");
    CHECK(FolderBytes(q.dir) == 7);
}

TEST_CASE("CTGP-7: copies found on the SD card, whoever put them there", "[ctgp7]") {
    // app-made or copied whole from another computer: the launcher title and the emulator's files
    Layout l;
    l.plugin = l.citra_flag = l.launcher_cia = l.launcher_title = true;
    CHECK(Detect(l) == Found::Installed);
    l.launcher_cia = false; // the installer's CIA copy deleted by hand: still installed
    CHECK(Detect(l) == Found::Installed);
    // only the SD folder copied, the launcher's CIA in it: Finish Setup installs that CIA
    l = {};
    l.plugin = l.citra_flag = l.launcher_cia = true;
    CHECK(Detect(l) == Found::FinishSetup);
    // the SD folder without it: the official installer runs over the folder
    l.launcher_cia = false;
    CHECK(Detect(l) == Found::NeedsInstaller);
    // a real 3DS's card (the console's files, no config/citra.flag): the installer, launcher or not
    l = {};
    l.plugin = l.launcher_cia = l.launcher_title = true;
    CHECK(Detect(l) == Found::NeedsInstaller);
    // partial or broken: no plugin (a launcher alone, an empty or half-copied folder)
    l = {};
    l.launcher_title = true;
    CHECK(Detect(l) == Found::None);
    l.citra_flag = l.launcher_cia = true;
    CHECK(Detect(l) == Found::None);
    CHECK(Detect(Layout{}) == Found::None);

    // and from real folders
    const std::string dir = TempDir("detect");
    const Paths p = PlanPaths(dir + "sdmc/", dir + "user/");
    CHECK(ReadLayout(p, false).plugin == false);
    WriteAll(p.plugin, "3GX");
    WriteAll(p.dir + "config/citra.flag", "");
    WriteAll(LauncherCia(p), "CIA");
    WriteAll(p.dir + "resources/empty.3gx", ""); // an empty plugin does not count
    const Layout r = ReadLayout(p, false);
    CHECK(r.plugin);
    CHECK(r.citra_flag);
    CHECK(r.launcher_cia);
    CHECK(Detect(r) == Found::FinishSetup);
    CHECK(Detect(ReadLayout(p, true)) == Found::Installed);
    WriteAll(p.plugin, "");
    CHECK(Detect(ReadLayout(p, true)) == Found::None);
}

namespace {
std::vector<std::string> TreeOf(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file()) {
            out.push_back(it->path().lexically_relative(dir).generic_string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}
} // namespace

TEST_CASE("CTGP-7: Uninstall removes only CTGP-7's files; saves and the game stay", "[ctgp7]") {
    const std::string dir = TempDir("uninstall");
    const std::string sdmc = dir + "sdmc/";
    const std::string titles = sdmc + "Nintendo 3DS/0/0/title/";
    const std::string tickets = dir + "nand/dbs/ticket.db/";
    const Paths p = PlanPaths(sdmc, dir + "user/");
    const auto layout = [&] {
        WriteAll(p.plugin, "3GX");
        WriteAll(p.version_file, "1.6.30");
        WriteAll(p.dir + "config/citra.flag", "");
        WriteAll(p.dir + "gamefs/Course/x.szs", "track");
        WriteAll(p.dir + "MyStuff/Characters/Midna.chpack", "mod");
        WriteAll(p.dir + "savefs/game.sav", "CTGP-7 progress");
        WriteAll(sdmc + "CTGP-7savebak/game.sav", "CTGP-7 progress backup");
        WriteAll(sdmc + "3ds/CTGP-7/CTGP-7.3dsx", "launcher");
        WriteAll(sdmc + "3ds/other/other.3dsx", "another homebrew");
        WriteAll(titles + "00040000/03070c00/content/00000000.app", "launcher");
        WriteAll(titles + "00040000/03070c00/content/00000000.tmd", "tmd");
        WriteAll(titles + "00040000/00030800/content/00000000.app", "MK7");
        WriteAll(titles + "00040000/00030800/data/00000001/save.bin", "MK7 SAVE");
        WriteAll(titles + "0004000e/00030800/content/00000000.tmd", "MK7 update");
        WriteAll(tickets + "0004000003070C00.0000000000000000.tik", "t");
        WriteAll(tickets + "0004000000030800.0000000000000000.tik", "t");
        WriteAll(tickets + "0004000000033500.0000000000000000.tik", "another game's");
        WriteAll(p.work + "installed-game.txt", "0004000000030800\n" + dir + "roms/mk7.3ds\n");
        WriteAll(dir + "roms/mk7.3ds", "THE PLAYER'S ROM");
    };
    layout();
    REQUIRE(ReadInstalledGame(p).title_id == kMk7Usa);
    REQUIRE(ReadInstalledGame(p).source == dir + "roms/mk7.3ds");

    SECTION("Uninstall: CTGP-7's progress and Mario Kart 7 stay") {
        const auto plan = PlanUninstall(p, titles, tickets, false, 0);
        REQUIRE(RunUninstall(plan, p, titles, tickets, 0).empty());
        CHECK(TreeOf(p.dir) == std::vector<std::string>{"savefs/game.sav"});
        CHECK(fs::exists(sdmc + "CTGP-7savebak/game.sav"));
        CHECK_FALSE(fs::exists(sdmc + "3ds/CTGP-7"));
        CHECK(fs::exists(sdmc + "3ds/other/other.3dsx"));
        CHECK_FALSE(fs::exists(titles + "00040000/03070c00"));
        CHECK(fs::exists(titles + "00040000/00030800/content/00000000.app"));
        CHECK(ReadAll(titles + "00040000/00030800/data/00000001/save.bin") == "MK7 SAVE");
        CHECK(fs::exists(titles + "0004000e/00030800/content/00000000.tmd"));
        CHECK(TreeOf(tickets) == std::vector<std::string>{"0004000000030800.0000000000000000.tik",
                                                          "0004000000033500.0000000000000000.tik"});
        CHECK_FALSE(fs::exists(p.work));
        CHECK(ReadAll(dir + "roms/mk7.3ds") == "THE PLAYER'S ROM");
        CHECK(DetectInstall(p) == Found::None);
    }
    SECTION("Uninstall Both, Also Delete CTGP-7's Progress: still never the save or the ROM") {
        const auto plan = PlanUninstall(p, titles, tickets, true, kMk7Usa);
        REQUIRE(RunUninstall(plan, p, titles, tickets, kMk7Usa).empty());
        CHECK_FALSE(fs::exists(p.dir));
        CHECK_FALSE(fs::exists(sdmc + "CTGP-7savebak"));
        CHECK_FALSE(fs::exists(titles + "00040000/00030800/content"));
        CHECK(ReadAll(titles + "00040000/00030800/data/00000001/save.bin") == "MK7 SAVE");
        CHECK(fs::exists(titles + "0004000e/00030800/content/00000000.tmd")); // the update stays
        CHECK(TreeOf(tickets) == std::vector<std::string>{"0004000000033500.0000000000000000.tik"});
        CHECK(ReadAll(dir + "roms/mk7.3ds") == "THE PLAYER'S ROM");
    }
    SECTION("a plan with anything else in it removes only CTGP-7's") {
        UninstallPlan plan = PlanUninstall(p, titles, tickets, false, 0);
        plan.remove.push_back(titles + "00040000/00030800/");
        plan.remove.push_back(titles + "00040000/00030800/data/");
        plan.remove.push_back(dir + "roms/mk7.3ds");
        plan.remove.push_back(sdmc + "3ds/");
        plan.remove.push_back(tickets + "0004000000033500.0000000000000000.tik");
        RunUninstall(plan, p, titles, tickets, 0);
        CHECK(ReadAll(titles + "00040000/00030800/data/00000001/save.bin") == "MK7 SAVE");
        CHECK(fs::exists(titles + "00040000/00030800/content/00000000.app"));
        CHECK(ReadAll(dir + "roms/mk7.3ds") == "THE PLAYER'S ROM");
        CHECK(fs::exists(sdmc + "3ds/other/other.3dsx"));
        CHECK(fs::exists(tickets + "0004000000033500.0000000000000000.tik"));
    }
    SECTION("an imported copy (no app notes) uninstalls the same way") {
        fs::remove_all(p.work);
        CHECK(ReadInstalledGame(p).title_id == 0);
        const auto plan = PlanUninstall(p, titles, tickets, false, ReadInstalledGame(p).title_id);
        REQUIRE(RunUninstall(plan, p, titles, tickets, 0).empty());
        CHECK(TreeOf(p.dir) == std::vector<std::string>{"savefs/game.sav"});
        CHECK(fs::exists(titles + "00040000/00030800/content/00000000.app"));
    }
}

// AzaharFrame ctgp7b: Install, then the launcher's first run, then Launch or the game list without a
// restart. The launcher installs its own title again on that run (the owner's log: AM's
// BeginImportProgram at 583 s): 00000000.tmd with 404c70be.app became 00000001.tmd with
// 60986578.app, and the game list's row, still holding 404c70be.app, started nothing until Azahar
// was started again.
TEST_CASE("CTGP-7: Launch and the game list follow the launcher installing itself again",
          "[ctgp7]") {
    const std::string dir = TempDir("listed");
    const std::string old_sdmc = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
    fs::create_directories(dir + "sdmc/");
    FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir, dir + "sdmc/");
    using Service::FS::MediaType;
    const std::string content = Service::AM::GetTitlePath(MediaType::SDMC, kLauncherTitleId) + "content/";
    REQUIRE(content.rfind(dir + "sdmc/", 0) == 0);
    const auto put_tmd = [&](const std::string& name, u32 content_id) {
        std::vector<u8> tmd = Cia::MakeTmd(kLauncherTitleId, 0, 16, {}, 0);
        const std::size_t chunk = tmd.size() - 0x30; // the one content chunk: its ID first, big endian
        for (int i = 0; i < 4; i++) {
            tmd[chunk + i] = static_cast<u8>(content_id >> (24 - 8 * i));
        }
        WriteAll(content + name, std::string(tmd.begin(), tmd.end()));
    };
    // what the installer left (the list scanned right after it)
    put_tmd("00000000.tmd", 0x404c70be);
    WriteAll(content + "404c70be.app", "launcher, as the installer put it");
    const std::string listed = Service::AM::GetTitleContentPath(MediaType::SDMC, kLauncherTitleId);
    CHECK(listed == content + "404c70be.app");
    CHECK(LauncherPath() == listed);
    CHECK(Service::AM::ListedContentPath(listed, MediaType::SDMC, kLauncherTitleId) == listed);

    // the launcher's first run installs it again
    put_tmd("00000001.tmd", 0x60986578);
    WriteAll(content + "60986578.app", "launcher, as it installed itself");
    fs::remove(content + "00000000.tmd");
    fs::remove(content + "404c70be.app");
    // Launch reads the title as it is
    CHECK(LauncherPath() == content + "60986578.app");
    // the list's row still holds 404c70be.app: it starts the title's current content
    CHECK(Service::AM::ListedContentPath(listed, MediaType::SDMC, kLauncherTitleId) ==
          content + "60986578.app");
    // only an installed title is looked up: a ROM file that is gone stays gone, and so does an
    // installed title that is gone
    CHECK(Service::AM::ListedContentPath(listed, MediaType::GameCard, kLauncherTitleId).empty());
    CHECK(Service::AM::ListedContentPath(listed, MediaType::SDMC, 0).empty());
    CHECK(Service::AM::ListedContentPath(dir + "roms/gone.3ds", MediaType::GameCard, 0).empty());
    fs::remove_all(content);
    CHECK(Service::AM::ListedContentPath(listed, MediaType::SDMC, kLauncherTitleId).empty());
    CHECK(LauncherPath().empty());
    FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir, old_sdmc);
}
