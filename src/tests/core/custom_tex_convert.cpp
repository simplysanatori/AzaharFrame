// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version

// AzaharFrame cppconv: the texture pack converter (core/custom_tex/convert.h): the file walk, the
// names, the sizes, pack.json, the marker, stop, and the switch to the copy and back.

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include "core/custom_tex/convert.h"
#include "core/custom_tex/ktx_encode.h"

namespace fs = std::filesystem;
namespace C = CustomTex::Convert;

namespace {

fs::path TempDir(const std::string& name) {
    const fs::path p =
        fs::temp_directory_path() / ("azf-convert-" + name + "-" + std::to_string(::getpid()));
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}

void Write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::string Read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::uint32_t Crc(const std::string& data) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (unsigned char b : data) {
        c ^= b;
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

void Be32(std::string& s, std::uint32_t v) {
    for (int i = 3; i >= 0; i--) {
        s.push_back(static_cast<char>(v >> (8 * i)));
    }
}

void Chunk(std::string& png, const char* type, const std::string& body) {
    Be32(png, static_cast<std::uint32_t>(body.size()));
    std::string tb = std::string(type, 4) + body;
    png += tb;
    Be32(png, Crc(tb));
}

/// An RGBA PNG (zlib stored blocks) with a gradient.
std::string MakePng(std::uint32_t w, std::uint32_t h, int seed) {
    std::string raw;
    for (std::uint32_t y = 0; y < h; y++) {
        raw.push_back(0);
        for (std::uint32_t x = 0; x < w; x++) {
            raw.push_back(static_cast<char>(x * 255 / std::max(w, 1u) + seed));
            raw.push_back(static_cast<char>(y * 255 / std::max(h, 1u)));
            raw.push_back(static_cast<char>(seed * 37));
            raw.push_back(static_cast<char>(255 - x));
        }
    }
    std::string z = "\x78\x01";
    std::uint32_t a = 1, b = 0;
    for (unsigned char c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t at = 0; at < raw.size(); at += 65535) {
        const size_t n = std::min<size_t>(65535, raw.size() - at);
        z.push_back(at + n >= raw.size() ? 1 : 0);
        z.push_back(static_cast<char>(n & 0xFF));
        z.push_back(static_cast<char>(n >> 8));
        z.push_back(static_cast<char>(~n & 0xFF));
        z.push_back(static_cast<char>((~n >> 8) & 0xFF));
        z.append(raw, at, n);
    }
    Be32(z, (b << 16) | a);
    std::string png = "\x89PNG\r\n\x1a\n";
    std::string ihdr;
    Be32(ihdr, w);
    Be32(ihdr, h);
    ihdr += std::string("\x08\x06\x00\x00\x00", 5);
    Chunk(png, "IHDR", ihdr);
    Chunk(png, "IDAT", z);
    Chunk(png, "IEND", "");
    return png;
}

std::uint32_t Le32(const std::string& s, size_t at) {
    return static_cast<unsigned char>(s[at]) | (static_cast<unsigned char>(s[at + 1]) << 8) |
           (static_cast<unsigned char>(s[at + 2]) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(s[at + 3])) << 24);
}

C::Request Req(const fs::path& pack) {
    C::Request r;
    r.pack = pack.string();
    r.quality = "fast";
    r.jobs = 2;
    r.low_priority = false;
    return r;
}

} // namespace

TEST_CASE("convert: sizes, names, comments", "[texparts][convert]") {
    const fs::path dir = TempDir("sizes");
    Write(dir / "a.png", MakePng(5, 3, 1));
    Write(dir / "junk.png", "not a png at all, but over 24 bytes long......");
    const auto size = C::PngSize((dir / "a.png").string());
    REQUIRE(size);
    CHECK(size->first == 5);
    CHECK(size->second == 3);
    CHECK_FALSE(C::PngSize((dir / "junk.png").string()));
    CHECK_FALSE(C::PngSize((dir / "missing.png").string()));

    // 4x4 ASTC: 64 header bytes, then 4 + 16 per block for 4x4, 2x2, 1x1
    CHECK(C::KtxSize(4, 4, {4, 4}) == 64 + 3 * 20);
    CHECK(C::KtxSize(1, 1, {4, 4}) == 64 + 20);
    CHECK(C::KtxSize(16, 8, {8, 6}) == 64 + (4 + 2 * 2 * 16) + (4 + 1 * 1 * 16) * 4);
    CHECK(C::KtxSize(5, 3, {6, 6}) == 64 + 3 * 20); // 5x3, 2x1, 1x1: one block each

    CHECK(C::DefaultOutput("/l/textures/0004000000033500", "4x4") == "/l/textures/0004000000033500-astc4x4");
    CHECK(C::DefaultOutput("/l/textures/0004000000033500-original", "6x6") ==
          "/l/textures/0004000000033500-astc6x6");

    CHECK(C::StripJsonComments("{\"a\": 1, // x\n \"b\": \"//not\" /* y */, \"c\": \"\\\" /*\"}") ==
          "{\"a\": 1, \n \"b\": \"//not\" , \"c\": \"\\\" /*\"}");
    CHECK(C::StripJsonComments("1 /* open") == "1 ");

    CHECK(C::ParseBlock("6x6"));
    CHECK_FALSE(C::ParseBlock("5x5"));
    CHECK(C::ParseQuality("fast"));
    CHECK(C::ParseQuality("42.5").value() == 42.5f);
    CHECK_FALSE(C::ParseQuality("101"));
    CHECK_FALSE(C::ParseQuality("slow"));
    CHECK(C::Human(512'000'000) == "512 MB");
    CHECK(C::Human(6'030'000'000) == "6.0 GB");
}

TEST_CASE("convert: the walk", "[texparts][convert]") {
    const fs::path dir = TempDir("walk");
    Write(dir / "pack.json", "{}");
    Write(dir / "b.png", "x");
    Write(dir / "a.txt", "x");
    Write(dir / "z" / "1.png", "x");
    Write(dir / "m" / "2.png", "x");
    Write(dir / "m" / "tmp.png.part", "x");
    Write(dir / C::kMarker, "{}");
    Write(dir / C::kPartsState, "{}");
    fs::create_directory_symlink(dir / "m", dir / "link");   // followed once, as a folder
    fs::create_directory_symlink(dir, dir / "m" / "up");     // a loop back: not walked again
    const auto files = C::Scan(dir.string());
    // files of a folder in name order, then its folders in name order ("link" is "m" again and is
    // walked first; "m" is then a folder already seen, and "up" a loop back)
    const std::vector<std::string> expect = {"a.txt", "b.png", "pack.json", "link/2.png", "z/1.png"};
    CHECK(files == expect);
}

TEST_CASE("convert: downsample averages every texel", "[texparts][convert]") {
    C::Level src;
    src.width = 3;
    src.height = 1;
    src.pixels = {0, 0, 0, 255, 100, 100, 100, 255, 200, 200, 200, 255};
    const C::Level dst = C::Downsample(src);
    CHECK(dst.width == 1);
    CHECK(dst.height == 1);
    CHECK(dst.pixels[0] == 100); // (0 + 100 + 200) / 3: all three texels count
    CHECK(dst.pixels[3] == 255);
}

TEST_CASE("convert: a pack, its copy, a second run, a bad PNG", "[texparts][convert]") {
    const fs::path root = TempDir("run");
    const fs::path pack = root / "0004000000033500";
    Write(pack / "pack.json",
          "{\n // comment\n \"options\": {\"flip_png_files\": false},\n \"textures\": {\"a\": \"a.png\", "
          "\"b\": [\"sub/b.png\", \"bad.png\"], \"c\": 3}\n}");
    Write(pack / "a.png", MakePng(16, 8, 1));
    Write(pack / "sub" / "b.png", MakePng(9, 7, 2));
    Write(pack / "bad.png", "not a png at all, but over 24 bytes long......");
    Write(pack / "notes.txt", "hello");
    Write(pack / C::kPartsState, "{\"x\":1}");

    int last_done = -1, last_total = -1, last_permille = -1, calls = 0;
    C::Converter converter(Req(pack), [&](const C::Progress& p) {
        last_done = p.done;
        last_total = p.total;
        last_permille = p.permille;
        calls++;
    });
    const C::Result r = converter.Run();
    REQUIRE(r.code == C::kOk);
    const fs::path out = root / "0004000000033500-astc4x4";
    CHECK(r.output == out.string());
    CHECK(last_done == 3);
    CHECK(last_total == 3);
    CHECK(last_permille == 1000);
    CHECK(calls == 4); // the start, then one a texture

    // KTX files with the full chain, the PNG kept where it could not be encoded
    CHECK_FALSE(fs::exists(out / "a.png"));
    const std::string ktx = Read(out / "a.ktx");
    REQUIRE(ktx.size() > 64);
    CHECK(ktx.size() == C::KtxSize(16, 8, {4, 4}));
    CHECK(ktx.substr(1, 6) == "KTX 11");
    CHECK(Le32(ktx, 36) == 16);
    CHECK(Le32(ktx, 40) == 8);
    CHECK(Le32(ktx, 56) == 5); // 16x8, 8x4, 4x2, 2x1, 1x1
    CHECK(fs::file_size(out / "sub" / "b.ktx") == C::KtxSize(9, 7, {4, 4}));
    CHECK(fs::exists(out / "bad.png"));
    CHECK_FALSE(fs::exists(out / "bad.ktx"));
    CHECK(Read(out / "notes.txt") == "hello");
    CHECK(Read(out / C::kPartsState) == "{\"x\":1}");
    REQUIRE(r.kept_as_png.size() == 1);
    CHECK(r.kept_as_png[0].first == "bad.png");

    // pack.json: comments gone, indent 4, order kept, names renamed except the one kept as PNG
    CHECK(Read(out / "pack.json") ==
          "{\n    \"options\": {\n        \"flip_png_files\": false\n    },\n    \"textures\": {\n"
          "        \"a\": \"a.ktx\",\n        \"b\": [\n            \"sub/b.ktx\",\n            \"bad.png\"\n"
          "        ],\n        \"c\": 3\n    }\n}\n");
    CHECK(Read(out / C::kMarker) ==
          "{\n    \"format\": \"ASTC 4x4\",\n    \"quality\": \"fast\",\n    \"source\": "
          "\"../0004000000033500\",\n    \"textures\": 3,\n    \"kept_as_png\": 1\n}\n");
    // the flip option: false here, so the first texel row is the PNG's first (not its last)
    CHECK(Read(pack / "a.png").size() > 0);

    // A second run redoes only the PNG that failed
    int second_start_done = -1;
    C::Converter again(Req(pack), [&](const C::Progress& p) {
        if (second_start_done < 0) {
            second_start_done = p.done;
        }
    });
    const C::Result r2 = again.Run();
    CHECK(r2.code == C::kOk);
    CHECK(second_start_done == 2);

    // The estimate counts what is left to write
    std::string error;
    const auto est = C::EstimatePack(Req(pack), &error);
    REQUIRE(est);
    CHECK(est->textures == 3);
    CHECK(est->need_bytes == fs::file_size(pack / "bad.png"));
    C::Request on_copy = Req(out);
    CHECK_FALSE(C::EstimatePack(on_copy, &error));
    CHECK(C::Converter(on_copy).Run().already_optimized);
}

TEST_CASE("convert: flip matches the PNG's orientation", "[texparts][convert]") {
    C::AstcEncoder encoder({4, 4}, 0.0f);
    REQUIRE(encoder.Error().empty());
    // 4x8 pixels: the top half red, the bottom half blue
    std::vector<std::uint8_t> px;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 4; x++) {
            px.insert(px.end(), y < 4 ? std::initializer_list<std::uint8_t>{255, 0, 0, 255}
                                      : std::initializer_list<std::uint8_t>{0, 0, 255, 255});
        }
    }
    std::string error;
    const auto plain = encoder.EncodeRgba(4, 8, px, false, &error);
    const auto flipped = encoder.EncodeRgba(4, 8, px, true, &error);
    REQUIRE(plain.size() == flipped.size());
    CHECK(plain != flipped);
    // level 0 is two blocks of 16 bytes after the 64 byte header and its 4 byte size; swapped by the flip
    CHECK(std::memcmp(plain.data() + 68, flipped.data() + 68 + 16, 16) == 0);
    CHECK(std::memcmp(plain.data() + 68 + 16, flipped.data() + 68, 16) == 0);
}

TEST_CASE("convert: Stop ends the run and carries on later", "[texparts][convert]") {
    const fs::path root = TempDir("stop");
    const fs::path pack = root / "pack";
    for (int i = 0; i < 4; i++) {
        Write(pack / ("t" + std::to_string(i) + ".png"), MakePng(32, 32, i));
    }
    C::Converter converter(Req(pack));
    converter.Stop();
    const C::Result r = converter.Run();
    CHECK(r.code == C::kStopped);
    CHECK_FALSE(fs::exists(root / "pack-astc4x4" / C::kMarker)); // not complete: no marker
    // Run again: it finishes
    const C::Result r2 = C::Converter(Req(pack)).Run();
    CHECK(r2.code == C::kOk);
    CHECK(fs::exists(root / "pack-astc4x4" / C::kMarker));

    // The output must be outside the pack, and the block must be one Azahar reads
    C::Request inside = Req(pack);
    inside.output = (pack / "copy").string();
    CHECK(C::Converter(inside).Run().code == C::kUsage);
    C::Request odd = Req(pack);
    odd.block = "5x5";
    CHECK(C::Converter(odd).Run().code == C::kUsage);
    CHECK(C::Converter(Req(root / "nothing")).Run().code == C::kUsage);
}

TEST_CASE("convert: Activate and Restore", "[texparts][convert]") {
    const fs::path root = TempDir("switch");
    const fs::path link = root / "textures" / "0004000000033500";
    Write(link / "a.png", MakePng(8, 8, 1));
    Write(link / C::kPartsState, "{\"parts\":1}");
    const C::Result r = C::Converter(Req(link)).Run();
    REQUIRE(r.code == C::kOk);
    std::string error;
    REQUIRE(C::Activate(link.string(), r.output, &error));
    CHECK(fs::is_symlink(link));
    CHECK(fs::exists(root / "textures" / "0004000000033500-original" / "a.png"));
    CHECK(fs::exists(link / "a.ktx"));
    CHECK(Read(link / C::kMarker).find("\"source\": \"../0004000000033500-original\"") != std::string::npos);
    CHECK(Read(link / C::kPartsState) == "{\"parts\":1}");

    // The marker is there: a run on the folder says so
    CHECK(C::Converter(Req(link)).Run().already_optimized);

    std::string message;
    REQUIRE(C::Restore(link.string(), &message, &error));
    CHECK_FALSE(fs::is_symlink(link));
    CHECK(fs::exists(link / "a.png"));
    CHECK(fs::exists(root / "textures" / "0004000000033500-astc4x4" / "a.ktx"));
    // Nothing to restore now
    REQUIRE(C::Restore(link.string(), &message, &error));
    CHECK(message.find("nothing to do") != std::string::npos);
}

TEST_CASE("convert: Stop from another thread, then carry on", "[texparts][convert]") {
    const fs::path root = TempDir("stopmid");
    const fs::path pack = root / "pack";
    for (int i = 0; i < 12; i++) {
        Write(pack / ("t" + std::to_string(i) + ".png"), MakePng(256, 256, i));
    }
    C::Request request = Req(pack);
    request.quality = "thorough";
    C::Converter converter(request);
    std::thread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        converter.Stop();
    });
    const C::Result r = converter.Run();
    stopper.join();
    // A fast machine may finish first; either way nothing half-written is left behind
    CHECK((r.code == C::kStopped || r.code == C::kOk));
    for (const auto& e : fs::recursive_directory_iterator(root / "pack-astc4x4")) {
        CHECK(e.path().extension() != ".part");
    }
    if (r.code == C::kStopped) {
        CHECK_FALSE(fs::exists(root / "pack-astc4x4" / C::kMarker));
    }
    request.quality = "fast";
    CHECK(C::Converter(request).Run().code == C::kOk);
    for (int i = 0; i < 12; i++) {
        CHECK(fs::file_size(root / "pack-astc4x4" / ("t" + std::to_string(i) + ".ktx")) ==
              C::KtxSize(256, 256, {4, 4}));
    }
}
