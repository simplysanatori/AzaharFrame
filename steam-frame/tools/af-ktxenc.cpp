// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// af-ktxenc: one PNG of a texture pack -> one KTX 1 file in ASTC with its full mip chain,
// for Azahar for Frame. build-astcenc.sh builds it against ARM's astcenc library (Apache-2.0)
// and loads PNGs with the stb_image copy astcenc ships (public domain / MIT).
//
//   af-ktxenc [--flip] [--block 4x4|6x6|8x6] [--quality fast|medium|thorough|0-100] IN.png OUT.ktx
//
// What Azahar does with the same PNG, done once here instead of at every load:
//  - decoded to 8-bit RGBA as stored (no gamma or colour conversion; the image is uploaded
//    as R8G8B8A8_UNORM, so the ASTC is the linear LDR profile and the KTX format is
//    GL_COMPRESSED_RGBA_ASTC_*_KHR, which Azahar maps to VK_FORMAT_ASTC_*_UNORM_BLOCK)
//  - --flip: turned upside down (the pack's flip_png_files), which Azahar does to PNGs only
//  - the mip chain: each level the 2x2 average of the one above, in the stored 8-bit values,
//    as the GPU's linear blit makes them for a pack with skip_mipmap
// The file is written as OUT.ktx.part and renamed when complete.

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "astcenc.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_FAILURE_USERMSG
#include "ThirdParty/stb_image.h"

namespace {

constexpr uint32_t GL_RGBA = 0x1908;

struct Level {
    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> pixels; // RGBA8
};

[[noreturn]] void Fail(const std::string& message) {
    std::fprintf(stderr, "af-ktxenc: %s\n", message.c_str());
    std::exit(1);
}

void Usage() {
    std::fprintf(stderr, "usage: af-ktxenc [--flip] [--block 4x4|6x6|8x6] "
                         "[--quality fast|medium|thorough|0-100] IN.png OUT.ktx\n");
    std::exit(2);
}

// One level down: every output texel is the average of the input texels it covers (2x2, or
// 2x3/3x2/3x3 at the last row or column of an odd size), so no input texel is dropped.
Level Downsample(const Level& src) {
    Level dst;
    dst.width = src.width > 1 ? src.width / 2 : 1;
    dst.height = src.height > 1 ? src.height / 2 : 1;
    dst.pixels.resize(static_cast<size_t>(dst.width) * dst.height * 4);
    for (uint32_t y = 0; y < dst.height; y++) {
        const uint32_t y0 = static_cast<uint32_t>(uint64_t{y} * src.height / dst.height);
        const uint32_t y1 = static_cast<uint32_t>(uint64_t{y + 1} * src.height / dst.height);
        for (uint32_t x = 0; x < dst.width; x++) {
            const uint32_t x0 = static_cast<uint32_t>(uint64_t{x} * src.width / dst.width);
            const uint32_t x1 = static_cast<uint32_t>(uint64_t{x + 1} * src.width / dst.width);
            uint32_t sum[4] = {};
            for (uint32_t sy = y0; sy < y1; sy++) {
                const uint8_t* row = &src.pixels[(static_cast<size_t>(sy) * src.width + x0) * 4];
                for (uint32_t sx = x0; sx < x1; sx++, row += 4) {
                    sum[0] += row[0];
                    sum[1] += row[1];
                    sum[2] += row[2];
                    sum[3] += row[3];
                }
            }
            const uint32_t count = (y1 - y0) * (x1 - x0);
            uint8_t* out = &dst.pixels[(static_cast<size_t>(y) * dst.width + x) * 4];
            for (int c = 0; c < 4; c++) {
                out[c] = static_cast<uint8_t>((sum[c] + count / 2) / count);
            }
        }
    }
    return dst;
}

void Put32(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        out.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
}

} // namespace

int main(int argc, char** argv) {
    bool flip = false;
    unsigned block_x = 4;
    unsigned block_y = 4;
    float quality = ASTCENC_PRE_MEDIUM;
    std::vector<std::string> files;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--flip") {
            flip = true;
        } else if (arg == "--block" && i + 1 < argc) {
            const std::string block = argv[++i];
            if (block == "4x4") {
                block_x = block_y = 4;
            } else if (block == "6x6") {
                block_x = block_y = 6;
            } else if (block == "8x6") {
                block_x = 8;
                block_y = 6;
            } else {
                Fail("Azahar reads ASTC 4x4, 6x6 and 8x6 only, not " + block);
            }
        } else if (arg == "--quality" && i + 1 < argc) {
            const std::string q = argv[++i];
            if (q == "fast") {
                quality = ASTCENC_PRE_FAST;
            } else if (q == "medium") {
                quality = ASTCENC_PRE_MEDIUM;
            } else if (q == "thorough") {
                quality = ASTCENC_PRE_THOROUGH;
            } else {
                char* end = nullptr;
                quality = std::strtof(q.c_str(), &end);
                if (end == q.c_str() || *end != '\0' || quality < 0.0f || quality > 100.0f) {
                    Fail("bad --quality " + q);
                }
            }
        } else if (!arg.empty() && arg[0] == '-') {
            Usage();
        } else {
            files.push_back(arg);
        }
    }
    if (files.size() != 2) {
        Usage();
    }
    const std::string& in_path = files[0];
    const std::string& out_path = files[1];

    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load(in_path.c_str(), &w, &h, &channels, 4);
    if (!decoded) {
        Fail(in_path + ": " + stbi_failure_reason());
    }
    std::vector<Level> levels(1);
    levels[0].width = static_cast<uint32_t>(w);
    levels[0].height = static_cast<uint32_t>(h);
    levels[0].pixels.assign(decoded, decoded + static_cast<size_t>(w) * h * 4);
    stbi_image_free(decoded);

    if (flip) {
        const size_t row = static_cast<size_t>(w) * 4;
        std::vector<uint8_t> tmp(row);
        for (int y = 0; y < h / 2; y++) {
            uint8_t* a = &levels[0].pixels[y * row];
            uint8_t* b = &levels[0].pixels[(h - 1 - y) * row];
            std::memcpy(tmp.data(), a, row);
            std::memcpy(a, b, row);
            std::memcpy(b, tmp.data(), row);
        }
    }
    while (levels.back().width > 1 || levels.back().height > 1) {
        levels.push_back(Downsample(levels.back()));
    }

    astcenc_config config{};
    astcenc_error err =
        astcenc_config_init(ASTCENC_PRF_LDR, block_x, block_y, 1, quality, 0, &config);
    if (err != ASTCENC_SUCCESS) {
        Fail(std::string("astcenc_config_init: ") + astcenc_get_error_string(err));
    }
    astcenc_context* context = nullptr;
    err = astcenc_context_alloc(&config, 1, &context, nullptr);
    if (err != ASTCENC_SUCCESS) {
        Fail(std::string("astcenc_context_alloc: ") + astcenc_get_error_string(err));
    }
    const astcenc_swizzle swizzle{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};

    // KTX 1: identifier, then the header in little-endian 32-bit words
    std::vector<uint8_t> out = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31,
                                0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    const uint32_t internal_format = block_x == 4 ? 0x93B0 : (block_x == 6 ? 0x93B4 : 0x93B6);
    Put32(out, 0x04030201); // endianness
    Put32(out, 0);          // glType (compressed)
    Put32(out, 1);          // glTypeSize
    Put32(out, 0);          // glFormat (compressed)
    Put32(out, internal_format);
    Put32(out, GL_RGBA);
    Put32(out, levels[0].width);
    Put32(out, levels[0].height);
    Put32(out, 0); // pixelDepth
    Put32(out, 0); // numberOfArrayElements
    Put32(out, 1); // numberOfFaces
    Put32(out, static_cast<uint32_t>(levels.size()));
    Put32(out, 0); // bytesOfKeyValueData

    size_t total = out.size();
    for (const Level& level : levels) {
        total += 4 + ((level.width + block_x - 1) / block_x) *
                         ((level.height + block_y - 1) / block_y) * 16;
    }
    out.reserve(total);

    for (Level& level : levels) {
        const size_t blocks_x = (level.width + block_x - 1) / block_x;
        const size_t blocks_y = (level.height + block_y - 1) / block_y;
        const size_t size = blocks_x * blocks_y * 16;
        void* slices[1] = {level.pixels.data()};
        astcenc_image image{level.width, level.height, 1, ASTCENC_TYPE_U8, slices};
        Put32(out, static_cast<uint32_t>(size));
        const size_t at = out.size();
        out.resize(at + size); // a multiple of 16, so no level padding is needed
        err = astcenc_compress_image(context, &image, &swizzle, out.data() + at, size, 0);
        if (err != ASTCENC_SUCCESS) {
            Fail(in_path + ": astcenc_compress_image: " + astcenc_get_error_string(err));
        }
        astcenc_compress_reset(context);
        level.pixels = {};
    }
    astcenc_context_free(context);

    const std::string part_path = out_path + ".part";
    FILE* file = std::fopen(part_path.c_str(), "wb");
    if (!file) {
        Fail(part_path + ": " + std::strerror(errno));
    }
    const bool written = std::fwrite(out.data(), 1, out.size(), file) == out.size();
    if (std::fclose(file) != 0 || !written) {
        std::remove(part_path.c_str());
        Fail(part_path + ": write failed");
    }
    if (std::rename(part_path.c_str(), out_path.c_str()) != 0) {
        std::remove(part_path.c_str());
        Fail(out_path + ": " + std::strerror(errno));
    }
    return 0;
}
