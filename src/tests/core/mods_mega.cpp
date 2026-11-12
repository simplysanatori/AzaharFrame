// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame mega: link parsing, key and IV derivation, AES-128-CTR against vectors made with
// Python's cryptography package (including a start inside a block), the API replies, a folder
// listing, ranged zip reading, and a whole install through a fake MEGA.

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include "core/mods/library.h"
#include "core/mods/mega.h"

using namespace Mods;
namespace fs = std::filesystem;

namespace {

const char kLinkKey[] = "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA"; // bytes 1..32
const unsigned char kCt0[] = {0xf6,0xa9,0x9d,0x7e,0x91,0x9b,0x77,0x29,0x80,0xe1,0x75,0xba,0xed,0x47,0x29,0x26,0xe3,0x4b,0x39,0x5b,0x14,0x0f,0xde,0xb4,0x8b,0x18,0xe7,0x1d,0x98,0xdf,0xb1,0x48,0xb0,0x51,0x6d,0xab,0xd8,0xfb,0x90,0xa9,0x4e,0xb4,0xa4,0x14,0x09,0x63,0x53,0xcd,0x5f,0xa2,0xb6,0x58,0x9b,0x0a,0x56,0x2a,0x61,0x35,0xf7,0x3d,0xd8,0x95,0xb2,0x23,0xf3,0xfc,0x16,0x71,0x1e,0x2d,0x1d,0xd5,0x14,0xb0,0x7f,0xe2,0xfc,0xe2,0x50,0x19,0x73,0x67,0xa3,0x48,0xb5,0xd9,0x78,0x77,0x50,0x38,0xbf,0x5c,0xa7,0x71,0x29,0x3d,0x9a,0x31,0x88,0x7b};
const unsigned char kCt37[] = {0xfb,0x90,0xa9,0x4e,0xb4,0xa4,0x14,0x09,0x63,0x53,0xcd,0x5f,0xa2,0xb6,0x58,0x9b,0x0a,0x56,0x2a,0x61,0x35,0xf7,0x3d,0xd8,0x95,0xb2,0x23,0xf3,0xfc,0x16,0x71,0x1e,0x2d,0x1d,0xd5,0x14,0xb0,0x7f,0xe2,0xfc,0xe2,0x50,0x19,0x73,0x67,0xa3,0x48,0xb5,0xd9,0x78,0x77,0x50,0x38,0xbf,0x5c,0xa7,0x71,0x29,0x3d,0x9a,0x31,0x88,0x7b};
const unsigned char kZip[] = {0x50,0x4b,0x03,0x04,0x14,0x00,0x00,0x00,0x08,0x00,0x8e,0x08,0x6c,0x5d,0x8c,0xb2,0x9b,0x9a,0x0a,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x36,0x00,0x00,0x00,0x54,0x6f,0x70,0x2f,0x42,0x61,0x73,0x65,0x2f,0x55,0x53,0x41,0x2f,0x6c,0x6f,0x61,0x64,0x2f,0x6d,0x6f,0x64,0x73,0x2f,0x30,0x30,0x30,0x34,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x33,0x30,0x31,0x30,0x30,0x2f,0x65,0x78,0x65,0x66,0x73,0x2f,0x63,0x6f,0x64,0x65,0x2e,0x69,0x70,0x73,0x0b,0x70,0x0c,0x71,0xf6,0x70,0xf5,0x77,0x03,0x00,0x50,0x4b,0x03,0x04,0x14,0x00,0x00,0x00,0x08,0x00,0x8e,0x08,0x6c,0x5d,0x7d,0xdc,0x15,0x0a,0x12,0x00,0x00,0x00,0xb8,0x01,0x00,0x00,0x33,0x00,0x00,0x00,0x54,0x6f,0x70,0x2f,0x42,0x61,0x73,0x65,0x2f,0x55,0x53,0x41,0x2f,0x6c,0x6f,0x61,0x64,0x2f,0x6d,0x6f,0x64,0x73,0x2f,0x30,0x30,0x30,0x34,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x33,0x30,0x31,0x30,0x30,0x2f,0x72,0x6f,0x6d,0x66,0x73,0x2f,0x61,0x2e,0x62,0x69,0x6e,0xcb,0x48,0xcd,0xc9,0xc9,0x57,0xc8,0x4d,0x4d,0x4f,0x54,0xc8,0x18,0x65,0x0e,0x1d,0x26,0x00,0x50,0x4b,0x03,0x04,0x14,0x00,0x00,0x00,0x08,0x00,0x8e,0x08,0x6c,0x5d,0x8c,0xb2,0x9b,0x9a,0x0a,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x36,0x00,0x00,0x00,0x54,0x6f,0x70,0x2f,0x42,0x61,0x73,0x65,0x2f,0x45,0x55,0x52,0x2f,0x6c,0x6f,0x61,0x64,0x2f,0x6d,0x6f,0x64,0x73,0x2f,0x30,0x30,0x30,0x34,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x33,0x30,0x32,0x30,0x30,0x2f,0x65,0x78,0x65,0x66,0x73,0x2f,0x63,0x6f,0x64,0x65,0x2e,0x69,0x70,0x73,0x0b,0x70,0x0c,0x71,0xf6,0x70,0xf5,0x77,0x03,0x00,0x50,0x4b,0x03,0x04,0x14,0x00,0x00,0x00,0x08,0x00,0x8e,0x08,0x6c,0x5d,0x83,0x16,0xdc,0x8c,0x03,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0x54,0x6f,0x70,0x2f,0x72,0x65,0x61,0x64,0x6d,0x65,0x2e,0x74,0x78,0x74,0xab,0x00,0x00,0x50,0x4b,0x01,0x02,0x14,0x03,0x14,0x00,0x00,0x00,0x08,0x00,0x8e,0x08,0x6c,0x5d,0x8c,0xb2,0x9b,0x9a,0x0a,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x00,0x00,0x00,0x00,0x54,0x6f,0x70,0x2f,0x42,0x61,0x73,0x65,0x2f,0x55,0x53,0x41,0x2f,0x6c,0x6f,0x61,0x64,0x2f,0x6d,0x6f,0x64,0x73,0x2f,0x30,0x30,0x30,0x34,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x33,0x30,0x31,0x30,0x30,0x2f,0x65,0x78,0x65,0x66,0x73,0x2f,0x63,0x6f,0x64,0x65,0x2e,0x69,0x70,0x73,0x50,0x4b,0x01,0x02,0x14,0x03,0x14,0x00,0x00,0x00,0x08,0x00,0x8e,0x08,0x6c,0x5d,0x7d,0xdc,0x15,0x0a,0x12,0x00,0x00,0x00,0xb8,0x01,0x00,0x00,0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x5e,0x00,0x00,0x00,0x54,0x6f,0x70,0x2f,0x42,0x61,0x73,0x65,0x2f,0x55,0x53,0x41,0x2f,0x6c,0x6f,0x61,0x64,0x2f,0x6d,0x6f,0x64,0x73,0x2f,0x30,0x30,0x30,0x34,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x33,0x30,0x31,0x30,0x30,0x2f,0x72,0x6f,0x6d,0x66,0x73,0x2f,0x61,0x2e,0x62,0x69,0x6e,0x50,0x4b,0x01,0x02,0x14,0x03,0x14,0x00,0x00,0x00,0x08,0x00,0x8e,0x08,0x6c,0x5d,0x8c,0xb2,0x9b,0x9a,0x0a,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0xc1,0x00,0x00,0x00,0x54,0x6f,0x70,0x2f,0x42,0x61,0x73,0x65,0x2f,0x45,0x55,0x52,0x2f,0x6c,0x6f,0x61,0x64,0x2f,0x6d,0x6f,0x64,0x73,0x2f,0x30,0x30,0x30,0x34,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x33,0x30,0x32,0x30,0x30,0x2f,0x65,0x78,0x65,0x66,0x73,0x2f,0x63,0x6f,0x64,0x65,0x2e,0x69,0x70,0x73,0x50,0x4b,0x01,0x02,0x14,0x03,0x14,0x00,0x00,0x00,0x08,0x00,0x8e,0x08,0x6c,0x5d,0x83,0x16,0xdc,0x8c,0x03,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x1f,0x01,0x00,0x00,0x54,0x6f,0x70,0x2f,0x72,0x65,0x61,0x64,0x6d,0x65,0x2e,0x74,0x78,0x74,0x50,0x4b,0x05,0x06,0x00,0x00,0x00,0x00,0x04,0x00,0x04,0x00,0x65,0x01,0x00,0x00,0x4e,0x01,0x00,0x00,0x00,0x00};
const char kAttr[] = "BhJIL8SW1smv6YKR3FYSJP3T7eutwfAu8aJo-Sm8wew"; // {"n":"patch.zip"}, CBC, the file's AES key
const char kFolder[] = "{\"f\": [{\"h\": \"ROOT0001\", \"p\": \"\", \"u\": \"U\", \"t\": 2, \"a\": \"x\", \"k\": \"x:x\", \"s\": 0}, {\"h\": \"DIRAAAAA\", \"p\": \"ROOT0001\", \"t\": 1, \"a\": \"78Ge8HbhXhyLl_3TTHviD10l8KGIyY1SbJxQuHJETas\", \"k\": \"U:i50rX_KW2L0RFtHngxDNiA\"}, {\"h\": \"FILEBBBB\", \"p\": \"DIRAAAAA\", \"t\": 0, \"a\": \"bdPVjUBkvGVIyLeW5VO7C5MNw6kyansBpAJsXRQ6pN8\", \"k\": \"U:lytYE2dL4IYws9cZvqqdxILjMge8WAFqACSn-f1HpVg\", \"s\": 1234}]}";
const char kMaster[] = "ZGVmZ2hpamtsbW5vcHFycw";
const char kFolderFileKey[] = "MjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU5PUFE";

std::string Plain() {
    std::string s(100, '\0');
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i] = static_cast<char>((i * 7 + 3) & 255);
    }
    return s;
}
std::string Str(const unsigned char* p, std::size_t n) {
    return std::string(reinterpret_cast<const char*>(p), n);
}
std::string RawKey() {
    std::string k(32, '\0');
    for (int i = 0; i < 32; ++i) {
        k[i] = static_cast<char>(i + 1);
    }
    return k;
}
std::string TempDir(const std::string& name) {
    const fs::path p = fs::temp_directory_path() / ("azf-mega-" + name);
    std::error_code ec;
    fs::remove_all(p, ec);
    fs::create_directories(p, ec);
    return p.string() + "/";
}
std::string Read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

constexpr const char* kHost = "https://gfs302n132.userstorage.mega.co.nz";
constexpr u64 kTid = 0x0004000000030100ULL;

/// A fake MEGA: the API answers for one file, the storage host serves its encrypted bytes.
struct FakeMega {
    std::string zip; // plain
    std::string api_reply; // overrides the file reply when set
    int range_status = 206;
    int range_calls = 0;
    u64 range_bytes = 0;
    std::vector<std::string> posts;
    Library* library = nullptr; // Cancel() is called from the first range read
    Mega::Net Net() {
        const auto key = *Mega::DeriveFileKey(RawKey());
        Mega::Net n;
        n.post = [this](const std::string& host, const std::string& path, const std::string& body) {
            posts.push_back(host + path + " " + body);
            Mega::Reply r;
            r.status = 200;
            r.body = api_reply.empty()
                         ? fmt::format(R"([{{"s":{},"at":"{}","g":"{}/dl/abcdef"}}])", zip.size(), kAttr, kHost)
                         : api_reply;
            return r;
        };
        n.range = [this, key](const std::string& host, const std::string& path, u64 first, u64 last) {
            ++range_calls;
            Mega::Reply r;
            r.status = range_status;
            if (host != kHost || path != "/dl/abcdef") {
                r.status = 404;
                return r;
            }
            if (library && range_calls == 2) {
                library->Cancel(); // after the central directory, before the entries
            }
            Mega::CtrDecryptor enc(key, first);
            r.body = enc.Process(std::string_view(zip).substr(first, last - first + 1));
            range_bytes += r.body.size();
            return r;
        };
        return n;
    }
};

Catalog MegaCatalog() {
    const auto c = ParseCatalog(R"({"schema":1,"mods":[{
      "id":"kiu-cstick-gyro","name":"C-Stick & Gyro","title":"Kid Icarus: Uprising",
      "title_ids":["0004000000030100"],"author":"Darkshade",
      "author_url":"https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/",
      "source":{"type":"mega","link":"https://mega.nz/file/mS5QwCzb#AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA","file_pattern":"*.zip"},
      "install":{"rules":[{"from":"*/Base/*/load/mods/{tid}/","to":""}]}}]})");
    REQUIRE(c);
    return *c;
}

} // namespace

TEST_CASE("Mega: links", "[mega]") {
    const std::string key = kLinkKey;
    SECTION("file") {
        const auto l = Mega::ParseLink("https://mega.nz/file/mS5QwCzb#" + key);
        REQUIRE(l);
        REQUIRE_FALSE(l->folder);
        REQUIRE(l->handle == "mS5QwCzb");
        REQUIRE(l->key == RawKey());
    }
    SECTION("old style, spaces, no scheme") {
        const auto l = Mega::ParseLink(" mega.nz/#!mS5QwCzb!" + key + "\n");
        REQUIRE(l);
        REQUIRE(l->handle == "mS5QwCzb");
        REQUIRE(l->key == RawKey());
    }
    SECTION("folder, and a file of a folder") {
        const std::string k16 = kMaster;
        const auto f = Mega::ParseLink("https://mega.nz/folder/AbCdEfGh#" + k16);
        REQUIRE(f);
        REQUIRE(f->folder);
        REQUIRE(f->key.size() == 16);
        REQUIRE(f->node.empty());
        const auto n = Mega::ParseLink("https://mega.nz/folder/AbCdEfGh#" + k16 + "/file/FILEBBBB");
        REQUIRE(n);
        REQUIRE(n->node == "FILEBBBB");
        const auto old = Mega::ParseLink("https://mega.nz/#F!AbCdEfGh!" + k16);
        REQUIRE(old);
        REQUIRE(old->folder);
    }
    SECTION("refused") {
        REQUIRE_FALSE(Mega::ParseLink(""));
        REQUIRE_FALSE(Mega::ParseLink("https://example.com/file/mS5QwCzb#" + key));
        REQUIRE_FALSE(Mega::ParseLink("https://mega.nz/file/mS5QwCzb"));          // no key
        REQUIRE_FALSE(Mega::ParseLink("https://mega.nz/file/mS5QwCzb#AQIDBA"));   // short key
        REQUIRE_FALSE(Mega::ParseLink("https://mega.nz/file/m!S5Q#" + key));      // bad handle
        REQUIRE_FALSE(Mega::ParseLink("https://mega.nz/folder/AbCdEfGh#" + key)); // folder keys are 16 bytes
        REQUIRE_FALSE(Mega::ParseLink("https://mega.nz/file/mS5QwCzb#" + key + "*"));
    }
}

TEST_CASE("Mega: key, IV and CTR against known vectors", "[mega]") {
    REQUIRE(Mega::Base64UrlDecode("AQID") == std::string("\x01\x02\x03", 3));
    REQUIRE(Mega::Base64UrlDecode("-_8") == std::string("\xfb\xff", 2));
    REQUIRE_FALSE(Mega::Base64UrlDecode("a*b"));
    const auto key = Mega::DeriveFileKey(RawKey());
    REQUIRE(key);
    for (int i = 0; i < 16; ++i) { // aes = k[0..16) ^ k[16..32)
        REQUIRE(key->aes[i] == static_cast<u8>((i + 1) ^ (i + 17)));
    }
    for (int i = 0; i < 8; ++i) { // the nonce is k[16..24)
        REQUIRE(key->nonce[i] == static_cast<u8>(17 + i));
    }
    REQUIRE_FALSE(Mega::DeriveFileKey("short"));

    const std::string plain = Plain();
    SECTION("from the start") {
        Mega::CtrDecryptor c(*key, 0);
        REQUIRE(c.Process(plain) == Str(kCt0, sizeof(kCt0)));
        Mega::CtrDecryptor d(*key, 0); // and back
        REQUIRE(d.Process(Str(kCt0, sizeof(kCt0))) == plain);
    }
    SECTION("in pieces gives the same bytes") {
        Mega::CtrDecryptor c(*key, 0);
        const std::string out =
            c.Process(plain.substr(0, 13)) + c.Process(plain.substr(13, 40)) + c.Process(plain.substr(53));
        REQUIRE(out == Str(kCt0, sizeof(kCt0)));
    }
    SECTION("starting inside a block (byte 37)") {
        Mega::CtrDecryptor c(*key, 37);
        REQUIRE(c.Process(plain.substr(37)) == Str(kCt37, sizeof(kCt37)));
    }
}

TEST_CASE("Mega: API replies", "[mega]") {
    const auto key = *Mega::DeriveFileKey(RawKey());
    std::string error;
    SECTION("a file") {
        const auto r = Mega::ParseFileReply(
            fmt::format(R"([{{"s":900,"at":"{}","g":"{}/dl/xyz"}}])", kAttr, kHost), key, &error);
        INFO(error);
        REQUIRE(r);
        REQUIRE(r->size == 900);
        REQUIRE(r->name == "patch.zip");
        REQUIRE(r->url == std::string(kHost) + "/dl/xyz");
    }
    SECTION("errors, in the player's words") {
        REQUIRE_FALSE(Mega::ParseFileReply("[-9]", key, &error));
        REQUIRE(error == "MEGA Link Not Found");
        REQUIRE_FALSE(Mega::ParseFileReply("-16", key, &error));
        REQUIRE(error == "MEGA Link Not Found");
        REQUIRE_FALSE(Mega::ParseFileReply("[-17]", key, &error));
        REQUIRE(error == "MEGA Download Quota Reached, Try Again Later");
        REQUIRE_FALSE(Mega::ParseFileReply("[-5]", key, &error));
        REQUIRE(error == "MEGA Error -5");
        REQUIRE_FALSE(Mega::ParseFileReply("<html>", key, &error));
        REQUIRE(error == "MEGA Link Not Found");
    }
    SECTION("an address outside MEGA's storage is refused") {
        for (const char* url : {"http://gfs1.userstorage.mega.co.nz/dl/a", "https://evil.example/dl/a",
                                "https://gfs1.userstorage.mega.co.nz.evil.example/dl/a",
                                "https://g.api.mega.co.nz.evil.example/dl/a"}) {
            REQUIRE_FALSE(Mega::ParseFileReply(fmt::format(R"([{{"s":1,"g":"{}"}}])", url), key, &error));
        }
    }
    SECTION("hosts") {
        REQUIRE(Mega::HostAllowed("https://g.api.mega.co.nz"));
        REQUIRE(Mega::HostAllowed("https://gfs302n132.userstorage.mega.co.nz"));
        REQUIRE_FALSE(Mega::HostAllowed("https://mega.nz"));
        REQUIRE_FALSE(Mega::HostAllowed("https://x.y.userstorage.mega.co.nz"));
    }
}

TEST_CASE("Mega: folder listing", "[mega]") {
    std::string error;
    const auto master = *Mega::Base64UrlDecode(kMaster);
    const auto files = Mega::ParseFolderReply(kFolder, master, &error);
    INFO(error);
    REQUIRE(files);
    REQUIRE(files->size() == 1);
    REQUIRE((*files)[0].path == "Emulator/mod.zip");
    REQUIRE((*files)[0].handle == "FILEBBBB");
    REQUIRE((*files)[0].size == 1234);
    REQUIRE((*files)[0].key == *Mega::Base64UrlDecode(kFolderFileKey));
    REQUIRE_FALSE(Mega::ParseFolderReply("[-9]", master, &error));
    REQUIRE(error == "MEGA Link Not Found");
}

TEST_CASE("Mega: a zip read in place", "[mega]") {
    FakeMega mega;
    mega.zip = Str(kZip, sizeof(kZip));
    const Mega::Net net = mega.Net();
    const auto link = *Mega::ParseLink(std::string("https://mega.nz/file/mS5QwCzb#") + kLinkKey);
    std::string error;
    const auto resolved = Mega::Resolve(net, link, "*.zip", &error);
    INFO(error);
    REQUIRE(resolved);
    Mega::RemoteFile file(net, *resolved);
    std::vector<Mega::ZipEntry> entries;
    bool not_zip = true;
    REQUIRE(Mega::ListZip(file, entries, not_zip, error));
    REQUIRE_FALSE(not_zip);
    REQUIRE(entries.size() == 4);
    std::string data;
    for (const auto& e : entries) {
        if (e.name.find("a.bin") != std::string::npos) {
            REQUIRE(Mega::ReadZipEntry(file, e, data, error));
            REQUIRE(data.size() == 440);
            REQUIRE(data.compare(0, 11, "hello mega ") == 0);
            Mega::ZipEntry bad = e; // a wrong CRC is caught
            bad.crc ^= 1;
            REQUIRE_FALSE(Mega::ReadZipEntry(file, bad, data, error));
        }
    }
    SECTION("not a zip") {
        FakeMega other;
        other.zip = Plain();
        const Mega::Net n2 = other.Net();
        const auto r2 = Mega::Resolve(n2, link, "*", &error);
        REQUIRE(r2);
        Mega::RemoteFile f2(n2, *r2);
        REQUIRE_FALSE(Mega::ListZip(f2, entries, not_zip, error));
        REQUIRE(not_zip);
    }
}

TEST_CASE("Mega: a catalog entry", "[mega]") {
    const Catalog c = MegaCatalog();
    REQUIRE(c.mods.size() == 1);
    REQUIRE(c.mods[0].source.type == Source::Type::Mega);
    REQUIRE(c.mods[0].author_url == "https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/");
    std::vector<std::string> warnings;
    const auto bad = ParseCatalog(R"({"schema":1,"mods":[{"id":"x","name":"X","title_ids":["0004000000030100"],
      "source":{"type":"mega","link":"https://example.com/file/a#b"}}]})",
                                  nullptr, &warnings);
    REQUIRE(bad);
    REQUIRE(bad->mods.empty());
    REQUIRE(warnings.size() == 1);
}

TEST_CASE("Mega: install", "[mega]") {
    const std::string dir = TempDir("install");
    const std::string mods = dir + "mods/0004000000030100/";
    FakeMega mega;
    mega.zip = Str(kZip, sizeof(kZip));
    Library library(dir, MegaCatalog(), [](const std::string&, const std::string&, std::size_t) {
        HttpReply r;
        r.status = 404; // nothing but MEGA is contacted
        return r;
    });
    library.SetMegaNet(mega.Net());

    SECTION("only the region's files, from the right folder") {
        const auto out = library.Install(kTid, "kiu-cstick-gyro");
        INFO(out.error);
        REQUIRE(out.result == InstallResult::Ok);
        REQUIRE(Read(mods + "exefs/code.ips") == "PATCHEOF");
        REQUIRE(Read(mods + "romfs/a.bin").compare(0, 11, "hello mega ") == 0);
        REQUIRE_FALSE(fs::exists(mods + "readme.txt"));
        REQUIRE_FALSE(fs::exists(dir + "mods/0004000000030200"));
        const auto loaded = library.ListLoaded(kTid);
        REQUIRE(loaded.size() == 1);
        REQUIRE(loaded[0].source == "mega.nz");
        REQUIRE(loaded[0].author == "Darkshade");
        REQUIRE(library.Remove(kTid, "kiu-cstick-gyro").ok);
    }
    SECTION("a deleted link") {
        mega.api_reply = "[-9]";
        const auto out = library.Install(kTid, "kiu-cstick-gyro");
        REQUIRE(out.result == InstallResult::Error);
        REQUIRE(out.error == "MEGA Link Not Found");
        REQUIRE(library.GetState().text == "Install failed: MEGA Link Not Found");
        REQUIRE_FALSE(fs::exists(mods + "exefs"));
    }
    SECTION("the bandwidth quota") {
        mega.range_status = 509;
        const auto out = library.Install(kTid, "kiu-cstick-gyro");
        REQUIRE(out.result == InstallResult::Error);
        REQUIRE(out.error == "MEGA Download Quota Reached, Try Again Later");
        REQUIRE_FALSE(fs::exists(mods + "exefs"));
    }
    SECTION("the API's quota answer") {
        mega.api_reply = "[-17]";
        REQUIRE(library.Install(kTid, "kiu-cstick-gyro").error == "MEGA Download Quota Reached, Try Again Later");
    }
    SECTION("Cancel stops it and installs nothing") {
        mega.library = &library;
        const auto out = library.Install(kTid, "kiu-cstick-gyro");
        REQUIRE(out.result == InstallResult::Error);
        REQUIRE(out.error == "Cancelled");
        REQUIRE_FALSE(fs::exists(mods + "exefs"));
        mega.library = nullptr;
        REQUIRE(library.Install(kTid, "kiu-cstick-gyro").result == InstallResult::Ok); // and a retry works
    }
    SECTION("a title the entry does not serve") {
        REQUIRE(library.Install(0x0004000000030200ULL, "kiu-cstick-gyro").result == InstallResult::NotAvailable);
    }
}

// Hidden: the real MEGA. AZF_CATALOG_FILE names a catalog/mods.json; every MEGA entry for every region it
// lists is installed into a temporary load folder, the files are listed, and the entry is removed again.
TEST_CASE("Mega: real downloads of the catalog's MEGA entries", "[.network][.mega]") {
    const char* file = std::getenv("AZF_CATALOG_FILE");
    REQUIRE(file != nullptr);
    const auto catalog = ParseCatalog(Read(file));
    REQUIRE(catalog);
    const std::string dir = TempDir("real");
    Library library(dir, *catalog);
    for (const auto& e : catalog->mods) {
        if (e.source.type != Source::Type::Mega) {
            continue;
        }
        for (const u64 tid : e.title_ids) {
            const auto out = library.Install(tid, e.id);
            INFO(e.id << " " << TitleIdText(tid) << ": " << out.error);
            REQUIRE(out.result == InstallResult::Ok);
            std::string listing;
            for (const auto& f : fs::recursive_directory_iterator(dir + "mods/" + TitleIdText(tid))) {
                if (f.is_regular_file()) {
                    listing += fmt::format("\n  {} ({})", fs::relative(f.path(), dir + "mods/" + TitleIdText(tid)).string(),
                                           f.file_size());
                }
            }
            WARN(e.id << " " << TitleIdText(tid) << ":" << listing);
            REQUIRE(library.Remove(tid, e.id).ok);
        }
    }
}
