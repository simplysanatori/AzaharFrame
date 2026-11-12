// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emerald: the Pokémon Emerald 3DS package without a network or a ROM: the ROM check
// (dummy data only; no Pokémon ROM is used), the release's assets (fixtures cut from v0.5.0's
// replies), update planning, where the files go, and the state file.

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>
#include "common/logging/backend.h"
#include "common/request_path.h"
#include "core/custom_tex/download.h"
#include "core/homebrew/emerald3ds.h"
#include "core/homebrew/net.h"
#include "core/homebrew/packages.h"

using namespace Homebrew::Emerald3ds;

namespace {

const char* kRelease = R"({
  "tag_name": "v0.5.0", "name": "Beta 0.5.0", "draft": false, "prerelease": false,
  "assets": [
    {"name": "Emerald3DS-de.3dsx", "size": 6262592,
     "browser_download_url": "https://github.com/ZallaxDev/emerald-3ds/releases/download/v0.5.0/Emerald3DS-de.3dsx"},
    {"name": "Emerald3DS-v0.5.0-Windows.zip", "size": 51742129,
     "browser_download_url": "https://github.com/ZallaxDev/emerald-3ds/releases/download/v0.5.0/Emerald3DS-v0.5.0-Windows.zip"},
    {"name": "Emerald3DS-WebPayload.zip", "size": 29216490,
     "browser_download_url": "https://github.com/ZallaxDev/emerald-3ds/releases/download/v0.5.0/Emerald3DS-WebPayload.zip"},
    {"name": "Emerald3DS.3dsx", "size": 6266796,
     "browser_download_url": "https://github.com/ZallaxDev/emerald-3ds/releases/download/v0.5.0/Emerald3DS.3dsx"},
    {"name": "Emerald3DS.smdh", "size": 14016,
     "browser_download_url": "https://github.com/ZallaxDev/emerald-3ds/releases/download/v0.5.0/Emerald3DS.smdh"},
    {"name": "SHA256SUMS.txt", "size": 2936,
     "browser_download_url": "https://github.com/ZallaxDev/emerald-3ds/releases/download/v0.5.0/SHA256SUMS.txt"},
    {"name": "web-manifest.json", "size": 18142,
     "browser_download_url": "https://github.com/ZallaxDev/emerald-3ds/releases/download/v0.5.0/web-manifest.json"}
  ]})";

const char* kManifest = R"({
  "kind": "emerald3ds-web-payload", "schemaVersion": 1, "releaseTag": "v0.5.0",
  "dataAbi": "4b3cb7ab",
  "entrypoint": {"apiVersion": 2, "function": "run_web_build", "module": "emerald3ds_builder.web"},
  "assets": {"threeDsx": {"path": "payload/Emerald3DS.3dsx", "releaseAsset": "Emerald3DS.3dsx",
             "sha256": "2f71a35035ce7fe3bae7a54d0deb559e61eb7e407754776a8b62a6389c7a964f"}},
  "supportedRoms": [{"code": "BPEE", "language": "en",
                     "sha1": "f3ae088181bf583e55daf962a92bb46f4f1d07b7"}],
  "variants": [
    {"dataAbi": "734df4ff", "rom": {"code": "BPED", "language": "de",
                                    "sha1": "61c2eb2b380b1a75f0c94b767a2d4c26cd7ce4e3"},
     "assets": {"threeDsx": {"path": "payload/de/Emerald3DS.3dsx", "releaseAsset": "Emerald3DS-de.3dsx",
                "sha256": "955199d7eb31f3721414c70307e32da263f344ce0bad58e99fe9c20712bf861e"}}},
    {"dataAbi": "1df688e3", "rom": {"code": "BPEE", "language": "pt",
                                    "sha1": "f3ae088181bf583e55daf962a92bb46f4f1d07b7"},
     "assets": {"threeDsx": {"path": "payload/pt/Emerald3DS.3dsx", "releaseAsset": "Emerald3DS-pt.3dsx",
                "sha256": "00770f09786098b0ec2e09641304bd4554d52f0fe762364457387ebb2866a7a1"}}}
  ]})";

const char* kSums =
    "b6f28ed15a327acafb73214543b69bfe7058990c7fbd5fb9796c59e3d9a7d590  "
    "Emerald3DS-v0.5.0-Windows.zip\n"
    "2f71a35035ce7fe3bae7a54d0deb559e61eb7e407754776a8b62a6389c7a964f  Emerald3DS.3dsx\r\n"
    "a4b1f9c6c4e4c5066441b6ec70136cf99b83f7107042d0216587af950fe26c10 *Emerald3DS.smdh\n"
    "not a line\n"
    "2f71a35035ce7fe3bae7a54d0deb559e61eb7e407754776a8b62a6389c7a964f  payload/Emerald3DS.3dsx\n";

} // namespace

TEST_CASE("Emerald 3DS: the registry", "[frame_xr][emerald]") {
    const auto* p = Homebrew::FindPackage("emerald3ds");
    REQUIRE(p != nullptr);
    CHECK(p->title == "Pokémon Emerald 3DS");
    CHECK(Homebrew::AddLabel(*p) == "Add Pokémon Emerald 3DS");
    CHECK(Homebrew::UpdateLabel(*p) == "Update Pokémon Emerald 3DS");
    CHECK(Homebrew::FindPackage("nothing") == nullptr);
}

TEST_CASE("Emerald 3DS: the ROM check", "[frame_xr][emerald]") {
    // SHA-1 itself (FIPS 180 "abc").
    const std::string abc = "abc";
    CHECK(Homebrew::Sha1Hex({reinterpret_cast<const u8*>(abc.data()), abc.size()}) ==
          "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(Homebrew::Sha256Hex("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // The six clean dumps, each a language; anything else refused.
    REQUIRE(SupportedRoms().size() == 6);
    std::set<std::string_view> languages;
    for (const RomProfile& r : SupportedRoms()) {
        CHECK(FindRom(r.sha1) == &r);
        languages.insert(r.language);
    }
    CHECK(languages == std::set<std::string_view>{"en", "es", "fr", "it", "de", "ja"});
    CHECK(FindRom("F3AE088181BF583E55DAF962A92BB46F4F1D07B7")->code == "BPEE");
    CHECK(FindRom("0000000000000000000000000000000000000000") == nullptr);

    // Dummy data, full size, trimmed and too large: refused with the short reason.
    std::vector<u8> dummy(kRomSize, 0x5A);
    RomCheck c = CheckRom(dummy);
    CHECK_FALSE(c.ok);
    CHECK(c.error == "Not a Clean Pokémon Emerald ROM");
    CHECK(c.sha1.size() == 40);
    // A trimmed dump is hashed padded with 0xFF, as the builder pads it.
    std::vector<u8> full(kRomSize, 0xFF);
    std::fill(full.begin(), full.begin() + 1024, 0x11);
    const std::vector<u8> trimmed(full.begin(), full.begin() + 1024);
    CHECK(CheckRom(trimmed).sha1 == Homebrew::Sha1Hex(full));
    CHECK_FALSE(CheckRom(std::vector<u8>(kRomSize + 1, 0)).ok);
    CHECK_FALSE(CheckRom({}).ok);

    // From a file.
    const auto dir = std::filesystem::temp_directory_path() / "azaharframe-emerald-test";
    std::filesystem::create_directories(dir);
    const auto file = dir / "dummy.gba";
    {
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(dummy.data()), 4096);
    }
    c = CheckRomFile(file.string());
    CHECK_FALSE(c.ok);
    CHECK(c.error == std::string(kRomRefused));
    CHECK(CheckRomFile((dir / "missing.gba").string()).error == "The file could not be read");
    CHECK(Homebrew::FileSha256((dir / "missing.gba").string()).empty());
    std::filesystem::remove_all(dir);
}

TEST_CASE("Emerald 3DS: game languages", "[frame_xr][emerald]") {
    // The English ROM builds English and the Brazilian Portuguese translation; the others one.
    const RomProfile* en = FindRom("f3ae088181bf583e55daf962a92bb46f4f1d07b7");
    REQUIRE(en != nullptr);
    CHECK(LanguagesFor(*en) == std::vector<std::string>{"en", "pt"});
    const RomProfile* de = FindRom("61c2eb2b380b1a75f0c94b767a2d4c26cd7ce4e3");
    REQUIRE(de != nullptr);
    CHECK(LanguagesFor(*de) == std::vector<std::string>{"de"});
    for (const RomProfile& r : SupportedRoms()) {
        CHECK(LanguagesFor(r).front() == r.language);
    }
    CHECK(LanguageName("en") == "English");
    CHECK(LanguageName("pt") == "Português (Brasil)");
    CHECK(LanguageName("ja") == "日本語");
    CHECK(LanguageName("xx") == "xx");
}

TEST_CASE("Emerald 3DS: versions", "[frame_xr][emerald]") {
    CHECK(ParseVersion("v0.5.0") == std::vector<int>{0, 5, 0});
    CHECK(ParseVersion("1.2") == std::vector<int>{1, 2});
    CHECK_FALSE(ParseVersion("v0.5.0-beta"));
    CHECK_FALSE(ParseVersion("v0..1"));
    CHECK_FALSE(ParseVersion(""));
    CHECK(IsNewer("v0.5.1", "v0.5.0"));
    CHECK(IsNewer("v0.10.0", "v0.9.9"));
    CHECK(IsNewer("v1.0", "v0.9.9"));
    CHECK_FALSE(IsNewer("v0.5.0", "v0.5.0"));
    CHECK_FALSE(IsNewer("v0.5", "v0.5.0"));
    CHECK_FALSE(IsNewer("v0.4.2", "v0.5.0"));
    CHECK_FALSE(IsNewer("", "v0.5.0"));
    CHECK(IsNewer("nightly-2", "nightly-1")); // not versions: when they differ
}

TEST_CASE("Emerald 3DS: the release and its assets", "[frame_xr][emerald]") {
    std::string error;
    const auto release = ParseRelease(kRelease, &error);
    REQUIRE(release);
    CHECK(release->tag == "v0.5.0");
    CHECK(release->assets.size() == 7);
    CHECK_FALSE(ParseRelease(R"({"message": "Not Found"})", &error));
    CHECK(error == "Not Found");
    CHECK_FALSE(ParseRelease("<html>", &error));

    const auto manifest = ParseManifest(kManifest, &error);
    REQUIRE(manifest);
    CHECK(manifest->api_version == 2);
    REQUIRE(manifest->variants.size() == 3);
    CHECK(manifest->variants[0].language == "en");
    CHECK(manifest->variants[0].three_dsx == "Emerald3DS.3dsx");
    CHECK(manifest->variants[0].data_abi == "4b3cb7ab");
    CHECK_FALSE(ParseManifest(R"({"kind": "other"})", &error));

    // The English ROM builds English by default, Portuguese when asked; the German ROM German.
    const std::string en = "f3ae088181bf583e55daf962a92bb46f4f1d07b7";
    const std::string de = "61c2eb2b380b1a75f0c94b767a2d4c26cd7ce4e3";
    CHECK(FindVariant(*manifest, en)->language == "en");
    CHECK(FindVariant(*manifest, en, "pt")->data_abi == "1df688e3");
    CHECK(FindVariant(*manifest, de)->three_dsx == "Emerald3DS-de.3dsx");
    CHECK(FindVariant(*manifest, de, "en") == nullptr);
    CHECK(FindVariant(*manifest, "d7cf8f156ba9c455d164e1ea780a6bf1945465c2") == nullptr);

    const auto sums = ParseSums(kSums);
    CHECK(sums.size() == 4);
    CHECK(SumFor(sums, "Emerald3DS.3dsx") ==
          "2f71a35035ce7fe3bae7a54d0deb559e61eb7e407754776a8b62a6389c7a964f");
    CHECK(SumFor(sums, "Emerald3DS.smdh") ==
          "a4b1f9c6c4e4c5066441b6ec70136cf99b83f7107042d0216587af950fe26c10");
    CHECK(SumFor(sums, "web-manifest.json").empty());

    // A full install: the builder's payload; a quick update: the language's .3dsx and the .smdh.
    auto plan = SelectAssets(*release, UpdateKind::Full, {}, &error);
    REQUIRE(plan);
    CHECK(plan->payload->name == "Emerald3DS-WebPayload.zip");
    CHECK(plan->manifest->name == "web-manifest.json");
    CHECK(plan->sums->name == "SHA256SUMS.txt");
    CHECK(plan->three_dsx == nullptr);
    plan = SelectAssets(*release, UpdateKind::Quick, "Emerald3DS-de.3dsx", &error);
    REQUIRE(plan);
    CHECK(plan->three_dsx->name == "Emerald3DS-de.3dsx");
    CHECK(plan->smdh->name == "Emerald3DS.smdh");
    CHECK(plan->payload == nullptr);
    CHECK_FALSE(SelectAssets(*release, UpdateKind::Quick, "Emerald3DS-ja.3dsx", &error));
    CHECK(error == "The release has no Emerald3DS-ja.3dsx");
    CHECK_FALSE(SelectAssets(*release, UpdateKind::Quick, {}, &error));

    // An asset that is not on the release's own download page is refused.
    Release moved = *release;
    for (Asset& a : moved.assets) {
        if (a.name == "Emerald3DS-WebPayload.zip") {
            a.url = "https://example.com/Emerald3DS-WebPayload.zip";
        }
    }
    CHECK_FALSE(SelectAssets(moved, UpdateKind::Full, {}, &error));
    CHECK_FALSE(Homebrew::HostAllowed("https://example.com"));
    CHECK(Homebrew::HostAllowed("https://release-assets.githubusercontent.com"));
}

TEST_CASE("Emerald 3DS: update planning", "[frame_xr][emerald]") {
    Variant v;
    v.language = "en";
    v.data_abi = "4b3cb7ab";
    v.three_dsx = "Emerald3DS.3dsx";
    CHECK(PlanUpdate("v0.5.0", "4b3cb7ab", "v0.5.0", &v) == UpdateKind::None);
    CHECK(PlanUpdate("v0.5.0", "4b3cb7ab", "v0.5.1", &v) == UpdateKind::Quick);
    CHECK(PlanUpdate("v0.5.0", "4B3CB7AB", "v0.5.1", &v) == UpdateKind::Quick);
    CHECK(PlanUpdate("v0.4.2", "0d1e2f3a", "v0.5.0", &v) == UpdateKind::Full);
    CHECK(PlanUpdate("v0.5.0", "", "v0.5.1", &v) == UpdateKind::Full);
    CHECK(PlanUpdate("v0.5.0", "4b3cb7ab", "v0.5.1", nullptr) == UpdateKind::Full);

    State s;
    s.tag = "v0.5.0";
    s.latest_tag = "v0.5.0";
    CHECK_FALSE(UpdateAvailable(s));
    s.latest_tag = "v0.6.0";
    CHECK(UpdateAvailable(s));

    CHECK(CheckDue(0, 1000));
    CHECK_FALSE(CheckDue(1000, 1000 + kCheckInterval - 1));
    CHECK(CheckDue(1000, 1000 + kCheckInterval));
    CHECK(CheckDue(5000, 1000)); // the clock went back
}

TEST_CASE("Emerald 3DS: where the files go", "[frame_xr][emerald]") {
    const Paths p = PlanPaths("/u/sdmc/", "/u");
    CHECK(p.app_dir == "/u/sdmc/3ds/emerald3ds/");
    CHECK(p.three_dsx == "/u/sdmc/3ds/emerald3ds/Emerald3DS.3dsx");
    CHECK(p.smdh == "/u/sdmc/3ds/emerald3ds/Emerald3DS.smdh");
    CHECK(p.pak == "/u/sdmc/3ds/emerald3ds/emerald3ds.pak");
    CHECK(p.save == "/u/sdmc/3ds/emerald3ds/emerald3ds.sav");
    CHECK(p.dspfirm == "/u/sdmc/3ds/dspfirm.cdc");
    CHECK(p.backup_root == "/u/sdmc/3ds/emerald3ds/backup/");
    CHECK(p.state_file == "/u/emerald3ds/state.json");
    CHECK(p.rom_copy == "/u/emerald3ds/rom.gba");
    CHECK(p.work_dir == "/u/emerald3ds/work/");
    CHECK(PlanPaths("/u/sdmc", "/u/").app_dir == p.app_dir);

    CHECK(RomDirCopy("/u/player/Documents/ROMs/3DS") ==
          "/u/player/Documents/ROMs/3DS/Emerald3DS.3dsx");
    CHECK(DefaultRomDir("/u/player") == "/u/player/Documents/ROMs/3DS");
    CHECK(ChooseRomDir(0) == RomDirChoice::Prompt);
    CHECK(ChooseRomDir(1) == RomDirChoice::UseOnly);
    CHECK(ChooseRomDir(3) == RomDirChoice::Ask);

    CHECK(IsEmeraldPath("/u/player/Documents/ROMs/3DS/Emerald3DS.3dsx"));
    CHECK(IsEmeraldPath("/u/sdmc/3ds/emerald3ds/emerald3ds.3dsx"));
    CHECK_FALSE(IsEmeraldPath("/roms/Emerald3DS.3dsx.bak"));
    CHECK_FALSE(IsEmeraldPath("/roms/Pokemon Emerald.gba"));

    // The save first, never a file name the game does not use.
    const auto saves = SaveFiles();
    CHECK(saves.front() == "emerald3ds.sav");
    CHECK(saves.size() == 7);

    // Backups: the time stamp, then -2, -3 when taken (or left half-made).
    std::set<std::string> taken;
    const auto exists = [&taken](const std::string& d) { return taken.count(d) != 0; };
    CHECK(BackupDir(p.backup_root, "20261111-221500", exists) ==
          "/u/sdmc/3ds/emerald3ds/backup/20261111-221500/");
    taken.insert("/u/sdmc/3ds/emerald3ds/backup/20261111-221500/");
    taken.insert("/u/sdmc/3ds/emerald3ds/backup/20261111-221500-2.partial");
    CHECK(BackupDir(p.backup_root, "20261111-221500", exists) ==
          "/u/sdmc/3ds/emerald3ds/backup/20261111-221500-3/");

    // The DSP placeholder holds no firmware and is not empty.
    const std::string dsp = DspPlaceholder();
    CHECK(dsp.size() == 0x400);
    CHECK(dsp.rfind("Azahar for Frame: placeholder", 0) == 0);
}

TEST_CASE("Emerald 3DS: the state file", "[frame_xr][emerald]") {
    State s;
    s.tag = "v0.5.0";
    s.language = "pt";
    s.rom_sha1 = "f3ae088181bf583e55daf962a92bb46f4f1d07b7";
    s.data_abi = "1df688e3";
    s.rom_dir_copy = "/u/player/Documents/ROMs/3DS/Emerald3DS.3dsx";
    s.last_check = 1794000000;
    s.latest_tag = "v0.5.1";
    const auto back = ParseState(SerializeState(s));
    REQUIRE(back);
    CHECK(back->tag == s.tag);
    CHECK(back->language == s.language);
    CHECK(back->rom_sha1 == s.rom_sha1);
    CHECK(back->data_abi == s.data_abi);
    CHECK(back->rom_dir_copy == s.rom_dir_copy);
    CHECK(back->last_check == s.last_check);
    CHECK(back->latest_tag == s.latest_tag);
    CHECK_FALSE(ParseState("{}"));
    CHECK_FALSE(ParseState("not json"));
}

namespace {
void StartConsoleLog() {
    static bool started = false;
    if (!started) {
        Common::Log::Initialize();
        Common::Log::SetColorConsoleBackendEnabled(true);
        Common::Log::Start();
        started = true;
    }
}
} // namespace

// AzaharFrame emerald: the real downloads, by hand only (hidden tag; needs the network and
// spends a GitHub API call): ./tests "[.network]"
TEST_CASE("Emerald 3DS: the release's small files download", "[.network]") {
    StartConsoleLog();
    const auto dir = std::filesystem::temp_directory_path() / "azaharframe-emerald-net";
    std::filesystem::create_directories(dir);
    const Homebrew::Reply latest = Homebrew::GetText(std::string(kLatestUrl), 1 << 20);
    REQUIRE(latest.error.empty());
    const auto release = ParseRelease(latest.body);
    REQUIRE(release);
    for (const std::string name : {"SHA256SUMS.txt", "web-manifest.json", "Emerald3DS.smdh"}) {
        const std::string url = std::string(kAssetPrefix) + release->tag + "/" + name;
        const std::string error =
            Homebrew::Download(url, (dir / name).string(), [](u64, u64) { return true; });
        INFO(name);
        CHECK(error.empty());
        CHECK(std::filesystem::file_size(dir / name) > 0);
    }
    std::filesystem::remove_all(dir);
}

// AzaharFrame emerald: request lines keep a signed URL's query byte for byte.
TEST_CASE("Emerald 3DS: request paths go out as written", "[frame_xr][emerald]") {
    // GitHub's release-assets redirect (an Azure SAS URL): '+' is a space in "rscd", and the
    // signature covers it, so it must not become %2B (that was a 403 AuthenticationFailed).
    const std::string signed_path =
        "/github-production-release-asset/1/abc?sp=r&se=2026-11-12T06%3A41%3A19Z"
        "&rscd=attachment%3B+filename%3DEmerald3DS.smdh&rsct=application%2Foctet-stream"
        "&sig=AbC%2Bd%2Fe%3D&jwt=a.b.c&x=1,2;3'4";
    CHECK(Common::RequestPath(signed_path) == signed_path);
    CHECK(Common::RequestPath("/a b/\xC3\xA9\t") == "/a%20b/%C3%A9%09");
    CHECK(Common::RequestPath("") == "");
}


// AzaharFrame emerald: a large GitHub release pack through the same redirect to
// release-assets.githubusercontent.com, by hand only: ./tests "[.network]" (mods_library.cpp and
// custom_tex_download.cpp have the mod and small-pack ones).
TEST_CASE("Download Packs: Majora's Mask 3D HD downloads", "[.network]") {
    StartConsoleLog();
    const auto dir = std::filesystem::temp_directory_path() / "azaharframe-packs-net";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        CustomTex::Download::Downloader packs(dir.string() + "/");
        REQUIRE(packs.Start("mm3d-hd-deathwrench", 0x0004000000125500));
        CustomTex::Download::State st;
        for (int i = 0; i < 600; i++) { // 5 minutes at most
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            st = packs.GetState();
            if (!st.Busy()) {
                break;
            }
        }
        INFO(st.error);
        CHECK(st.phase == CustomTex::Download::State::Phase::Done);
        CHECK(!st.installed.empty()); // its folders (78 MB zip, sha256 checked)
    }
    std::filesystem::remove_all(dir);
}
