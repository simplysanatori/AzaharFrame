// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cpphelpers: the C++ ports of the Python helpers (Emerald 3DS pack builder pieces, the
// Steam CEF client's pure parts, the dock icon files, the controller model image scaling). No network,
// no Steam, no ROM.

#include <cstring>
#include <filesystem>
#include <fstream>
#include <catch2/catch_test_macros.hpp>
#include "core/frame_controller_models.h"
#include "core/frame_dock_icon.h"
#include "core/homebrew/emerald3ds_build.h"
#include "core/steam_cef.h"

namespace B = Homebrew::Emerald3ds::Build;

TEST_CASE("Emerald build: xz stream decodes, a bad one does not", "[cpphelpers]") {
    // lzma.compress(b'hello hello hello', check=CHECK_CRC64)
    static const unsigned char xz[] = {
        0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6, 0xb4, 0x46, 0x02, 0x00, 0x21,
        0x01, 0x16, 0x00, 0x00, 0x00, 0x74, 0x2f, 0xe5, 0xa3, 0xe0, 0x00, 0x10, 0x00, 0x0b, 0x5d,
        0x00, 0x34, 0x19, 0x49, 0xee, 0x8d, 0xe9, 0x4f, 0x7e, 0x15, 0xe0, 0x00, 0x00, 0x00, 0x4b,
        0x88, 0xe6, 0xda, 0x44, 0x80, 0x07, 0x76, 0x00, 0x01, 0x27, 0x11, 0xbc, 0x27, 0xf3, 0x90,
        0x1f, 0xb6, 0xf3, 0x7d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a};
    std::string in(reinterpret_cast<const char*>(xz), sizeof xz);
    std::string err;
    const auto out = B::XzDecompress(in, 1000, &err);
    REQUIRE(out.has_value());
    CHECK(*out == "hello hello hello");
    CHECK_FALSE(B::XzDecompress(in, 5, &err).has_value()); // over the size limit
    CHECK_FALSE(B::XzDecompress(in.substr(0, 40), 1000, &err).has_value()); // cut short
    CHECK_FALSE(B::XzDecompress("not xz", 1000, &err).has_value());
}

TEST_CASE("Emerald build: GBA LZ77", "[cpphelpers]") {
    // header 0x10, size 8; flags 0x00 = eight literals
    std::string src = std::string("\x10\x08\x00\x00", 4) + std::string("\x00", 1) + "ABCDEFGH";
    const auto out = B::Lz77Decompress(src, 0);
    REQUIRE(out.has_value());
    CHECK(*out == "ABCDEFGH");
    // flags 0x40: literal 'A', then a copy of 3 bytes from distance 1 -> "AAAA"
    src = std::string("\x10\x04\x00\x00", 4) + std::string("\x40", 1) + "A" + std::string("\x00\x00", 2);
    const auto rep = B::Lz77Decompress(src, 0);
    REQUIRE(rep.has_value());
    CHECK(*rep == "AAAA");
    CHECK_FALSE(B::Lz77Decompress(src.substr(0, 6), 0).has_value()); // ends early
    CHECK_FALSE(B::Lz77Decompress("\x11\x04\x00\x00", 0).has_value()); // not type 0x10
}

TEST_CASE("Emerald build: the pack is written, sorted and verifies", "[cpphelpers]") {
    std::vector<B::PakFile> files{{"data/b.bin", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
                                  {"data/a.bin", "aa"},
                                  {"c", std::string(100, '\x7f')}};
    std::string err;
    const std::string rom_sha1(20, 'x');
    const std::string pak = B::PakBytes(files, 0x12345678, rom_sha1, &err);
    REQUIRE(!pak.empty());
    CHECK(pak.compare(0, 7, "EM3DPAK") == 0);
    CHECK(B::PakVerify(pak, &err) == 3);
    // identical input, identical bytes
    CHECK(B::PakBytes(files, 0x12345678, rom_sha1, &err) == pak);
    // one flipped payload bit is found
    std::string bad = pak;
    bad[bad.find('\x7f')] ^= 1;
    CHECK(B::PakVerify(bad, &err) == 0);
    CHECK_FALSE(err.empty());
    // ids: FNV-1a 64 of the path
    CHECK(B::PakPathId("") == 0xcbf29ce484222325ULL);
    CHECK(B::PakPathId("a") == 0xaf63dc4c8601ec8cULL);
}

TEST_CASE("Emerald build: follower parameters and messages", "[cpphelpers]") {
    // a header line, then columns 3 to 5 (true/false) are the three flag bits
    CHECK(B::ParamFlags("id,name,a,b,c\n0,x,true,false,true\r\n1,y,false,true,false\n") ==
          std::vector<int>{5, 2});
    CHECK(B::ParamFlags("").empty());
    bool ok = false;
    const std::string enc = B::EncodeMessage("A", &ok);
    CHECK(ok);
    CHECK(!enc.empty());
    CHECK(enc.back() == '\xff'); // a Gen 3 string ends with 0xFF
}

TEST_CASE("Steam CEF: JavaScript strings and app ids", "[cpphelpers]") {
    CHECK(SteamCef::JsString("a'b\"c\\d") == R"("a'b\"c\\d")");
    CHECK(SteamCef::JsString("caf\xc3\xa9") == "\"caf\\u00e9\"");
    CHECK(SteamCef::JsString("x\ny") == "\"x\\ny\"");
    CHECK(SteamCef::ValidAppId("2147483648"));
    CHECK(SteamCef::ValidAppId("7"));
    CHECK_FALSE(SteamCef::ValidAppId(""));
    CHECK_FALSE(SteamCef::ValidAppId("12345678901"));
    CHECK_FALSE(SteamCef::ValidAppId("1; alert(1)"));
    CHECK_FALSE(SteamCef::ValidAppId("-5"));
}

TEST_CASE("Steam CEF: shortcut calls carry quoted text, never raw", "[cpphelpers]") {
    const std::string add = SteamCef::AddShortcutJs("Azahar'); evil('", "/a b/exe", "/a b");
    CHECK(add.find("AddShortcut(\"Azahar'); evil('\", \"/a b/exe\", \"/a b\", '')") != std::string::npos);
    const std::string up = SteamCef::UpdateShortcutJs("123", "/x/azaharframe.sh", "/x");
    CHECK(up.find("SetShortcutExe(123, \"\\\"/x/azaharframe.sh\\\"\")") != std::string::npos);
    CHECK(SteamCef::RemoveShortcutJs("5") == "(() => { SteamClient.Apps.RemoveShortcut(5); return 1; })()");
    CHECK(SteamCef::FindShortcutJs("Azahar for Frame").find("display_name === \"Azahar for Frame\"") !=
          std::string::npos);
}

TEST_CASE("Steam CEF: a client websocket frame is masked and sized", "[cpphelpers]") {
    const unsigned char mask[4] = {1, 2, 3, 4};
    const std::string f = SteamCef::WsTextFrame("hi", mask);
    REQUIRE(f.size() == 2 + 4 + 2);
    CHECK(static_cast<unsigned char>(f[0]) == 0x81);
    CHECK(static_cast<unsigned char>(f[1]) == (0x80 | 2));
    CHECK(static_cast<unsigned char>(f[6]) == ('h' ^ 1));
    CHECK(static_cast<unsigned char>(f[7]) == ('i' ^ 2));
    const std::string big = SteamCef::WsTextFrame(std::string(300, 'x'), mask);
    CHECK(static_cast<unsigned char>(big[1]) == (0x80 | 126));
    CHECK(big.size() == 4 + 4 + 300);
}

TEST_CASE("Steam CEF: art plan lists calls for the files that exist", "[cpphelpers]") {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "cpphelpers-art-test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream(dir / "portrait.png", std::ios::binary) << "PNGDATA";
    std::vector<std::string> notes;
    const auto calls = SteamCef::PlanArt("2147483648", dir.string(), &notes);
    CHECK(!calls.empty());
    bool portrait = false;
    for (const auto& c : calls) {
        portrait |= c.dry_line.find("SetCustomArtworkForApp(2147483648") != std::string::npos;
    }
    CHECK(portrait);
    CHECK(!notes.empty()); // hero.png and the rest are missing
    fs::remove_all(dir);
}

TEST_CASE("Steam CEF: vrcmd overlay lines", "[cpphelpers]") {
    const auto s = SteamCef::ParseOverlayLine("'k' x size=64x64 flags=0x8000 visible VROverlayType");
    CHECK(s.visible);
    CHECK(s.picture);
    CHECK(s.in_dashboard);
    CHECK_FALSE(SteamCef::FindOverlayLine("nothing here", "zzz").has_value());
}

TEST_CASE("Dock icon: appconfig manifest_paths", "[cpphelpers]") {
    bool changed = false;
    std::string out = FrameDockIcon::EditAppConfig(R"({"z":1,"manifest_paths":["/a"],"a":2})", "/b", "", &changed);
    CHECK(changed);
    CHECK(out.find("\"/b\"") != std::string::npos);
    CHECK(out.find("\"z\"") < out.find("\"manifest_paths\"")); // other keys keep their order
    CHECK(out.find("\"manifest_paths\"") < out.find("\"a\": 2"));
    // adding what is there is no change
    FrameDockIcon::EditAppConfig(out, "/b", "", &changed);
    CHECK_FALSE(changed);
    // dropping
    out = FrameDockIcon::EditAppConfig(out, "", "/a", &changed);
    CHECK(changed);
    CHECK(out.find("\"/a\"") == std::string::npos);
    // garbage becomes a fresh object
    out = FrameDockIcon::EditAppConfig("not json", "/b", "", &changed);
    CHECK(changed);
    CHECK(out.find("/b") != std::string::npos);
}

TEST_CASE("Dock icon: desktop file and manifest", "[cpphelpers]") {
    const std::string desktop = FrameDockIcon::DesktopText(2147483648ULL);
    CHECK(desktop.find("[Desktop Entry]") == 0);
    CHECK(desktop.find("Azahar for Frame") != std::string::npos);
    const std::string m = FrameDockIcon::ManifestText("/opt/x/app", 2147483648ULL, "/nonexistent");
    CHECK(m.find("\"app_key\"") != std::string::npos);
    CHECK(m.find("/opt/x/app") != std::string::npos);
}

TEST_CASE("Controller models: area-average scaling", "[cpphelpers]") {
    FrameControllerModels::Image src;
    src.width = 2;
    src.height = 2;
    src.rgb = {0, 0, 0, 100, 100, 100, 200, 200, 200, 100, 100, 100};
    const auto one = FrameControllerModels::Scale(src, 1, 1);
    REQUIRE(one.rgb.size() == 3);
    CHECK(one.rgb[0] == 100);
    const auto same = FrameControllerModels::Scale(src, 2, 2);
    CHECK(same.rgb == src.rgb);
    std::string err;
    FrameControllerModels::Model m;
    CHECK_FALSE(FrameControllerModels::Convert("left", "/nonexistent", 256, nullptr, &m, &err));
    CHECK_FALSE(err.empty());
}
