// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// AzaharFrame cppconv: see ktx_encode.h. The encoder is af-ktxenc.cpp's, made a library.

#include "core/custom_tex/ktx_encode.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "astcenc.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_FAILURE_USERMSG
#define STBI_MAX_DIMENSIONS 16384 // a downloaded pack's PNG is not trusted with more
#include "stb_image.h"

namespace CustomTex::Convert {

namespace {

constexpr u32 GL_RGBA = 0x1908;

void Put32(std::vector<u8>& out, u32 value) {
    for (int i = 0; i < 4; i++) {
        out.push_back(static_cast<u8>(value >> (8 * i)));
    }
}

void FlipRows(std::vector<u8>& pixels, u32 width, u32 height) {
    const size_t row = static_cast<size_t>(width) * 4;
    std::vector<u8> tmp(row);
    for (u32 y = 0; y < height / 2; y++) {
        u8* a = &pixels[y * row];
        u8* b = &pixels[(height - 1 - y) * row];
        std::memcpy(tmp.data(), a, row);
        std::memcpy(a, b, row);
        std::memcpy(b, tmp.data(), row);
    }
}

} // namespace

struct AstcEncoder::State {
    std::atomic<bool> cancelled{false};
};

std::optional<BlockSize> ParseBlock(const std::string& name) {
    if (name == "4x4") {
        return BlockSize{4, 4};
    }
    if (name == "6x6") {
        return BlockSize{6, 6};
    }
    if (name == "8x6") {
        return BlockSize{8, 6};
    }
    return std::nullopt;
}

std::optional<float> ParseQuality(const std::string& text) {
    if (text == "fast") {
        return ASTCENC_PRE_FAST;
    }
    if (text == "medium") {
        return ASTCENC_PRE_MEDIUM;
    }
    if (text == "thorough") {
        return ASTCENC_PRE_THOROUGH;
    }
    char* end = nullptr;
    const float quality = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || !(quality >= 0.0f && quality <= 100.0f)) {
        return std::nullopt;
    }
    return quality;
}

Level Downsample(const Level& src) {
    Level dst;
    dst.width = src.width > 1 ? src.width / 2 : 1;
    dst.height = src.height > 1 ? src.height / 2 : 1;
    dst.pixels.resize(static_cast<size_t>(dst.width) * dst.height * 4);
    for (u32 y = 0; y < dst.height; y++) {
        const u32 y0 = static_cast<u32>(u64{y} * src.height / dst.height);
        const u32 y1 = static_cast<u32>(u64{y + 1} * src.height / dst.height);
        for (u32 x = 0; x < dst.width; x++) {
            const u32 x0 = static_cast<u32>(u64{x} * src.width / dst.width);
            const u32 x1 = static_cast<u32>(u64{x + 1} * src.width / dst.width);
            u32 sum[4] = {};
            for (u32 sy = y0; sy < y1; sy++) {
                const u8* row = &src.pixels[(static_cast<size_t>(sy) * src.width + x0) * 4];
                for (u32 sx = x0; sx < x1; sx++, row += 4) {
                    sum[0] += row[0];
                    sum[1] += row[1];
                    sum[2] += row[2];
                    sum[3] += row[3];
                }
            }
            const u32 count = (y1 - y0) * (x1 - x0);
            u8* out = &dst.pixels[(static_cast<size_t>(y) * dst.width + x) * 4];
            for (int c = 0; c < 4; c++) {
                out[c] = static_cast<u8>((sum[c] + count / 2) / count);
            }
        }
    }
    return dst;
}

AstcEncoder::AstcEncoder(BlockSize block, float quality) : block_(block), state_(new State) {
    astcenc_config config{};
    astcenc_error err =
        astcenc_config_init(ASTCENC_PRF_LDR, block.x, block.y, 1, quality, 0, &config);
    if (err != ASTCENC_SUCCESS) {
        error_ = std::string("astcenc_config_init: ") + astcenc_get_error_string(err);
        return;
    }
    err = astcenc_context_alloc(&config, 1, &context_, nullptr);
    if (err != ASTCENC_SUCCESS) {
        context_ = nullptr;
        error_ = std::string("astcenc_context_alloc: ") + astcenc_get_error_string(err);
    }
}

AstcEncoder::~AstcEncoder() {
    if (context_) {
        astcenc_context_free(context_);
    }
    delete state_;
}

void AstcEncoder::Cancel() {
    state_->cancelled = true;
    if (context_) {
        astcenc_compress_cancel(context_);
    }
}

std::vector<u8> AstcEncoder::EncodeRgba(u32 width, u32 height, std::vector<u8> pixels, bool flip,
                                        std::string* error) {
    error->clear();
    if (!context_) {
        *error = error_;
        return {};
    }
    if (state_->cancelled) {
        return {};
    }
    std::vector<Level> levels(1);
    levels[0].width = width;
    levels[0].height = height;
    levels[0].pixels = std::move(pixels);
    if (flip) {
        FlipRows(levels[0].pixels, width, height);
    }
    while (levels.back().width > 1 || levels.back().height > 1) {
        levels.push_back(Downsample(levels.back()));
    }

    const astcenc_swizzle swizzle{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
    // KTX 1: identifier, then the header in little-endian 32-bit words
    std::vector<u8> out = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    const u32 internal_format = block_.x == 4 ? 0x93B0 : (block_.x == 6 ? 0x93B4 : 0x93B6);
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
    Put32(out, static_cast<u32>(levels.size()));
    Put32(out, 0); // bytesOfKeyValueData

    size_t total = out.size();
    for (const Level& level : levels) {
        total += 4 + ((level.width + block_.x - 1) / block_.x) *
                         ((level.height + block_.y - 1) / block_.y) * 16;
    }
    out.reserve(total);

    // A cancelled texture left the context mid-image: reset before the next, like any image
    astcenc_compress_reset(context_);
    for (Level& level : levels) {
        const size_t blocks_x = (level.width + block_.x - 1) / block_.x;
        const size_t blocks_y = (level.height + block_.y - 1) / block_.y;
        const size_t size = blocks_x * blocks_y * 16;
        void* slices[1] = {level.pixels.data()};
        astcenc_image image{level.width, level.height, 1, ASTCENC_TYPE_U8, slices};
        Put32(out, static_cast<u32>(size));
        const size_t at = out.size();
        out.resize(at + size); // a multiple of 16, so no level padding is needed
        const astcenc_error err =
            astcenc_compress_image(context_, &image, &swizzle, out.data() + at, size, 0);
        if (state_->cancelled) {
            return {};
        }
        if (err != ASTCENC_SUCCESS) {
            *error = std::string("astcenc_compress_image: ") + astcenc_get_error_string(err);
            return {};
        }
        astcenc_compress_reset(context_);
        level.pixels = {};
    }
    return out;
}

std::string AstcEncoder::EncodeFile(const std::string& in_path, const std::string& out_path,
                                    bool flip, bool* cancelled) {
    *cancelled = false;
    if (state_->cancelled) {
        *cancelled = true;
        return {};
    }
    int w = 0, h = 0, channels = 0;
    stbi_uc* decoded = stbi_load(in_path.c_str(), &w, &h, &channels, 4);
    if (!decoded) {
        return in_path + ": " + stbi_failure_reason();
    }
    std::vector<u8> pixels(decoded, decoded + static_cast<size_t>(w) * h * 4);
    stbi_image_free(decoded);

    std::string error;
    const std::vector<u8> ktx =
        EncodeRgba(static_cast<u32>(w), static_cast<u32>(h), std::move(pixels), flip, &error);
    if (ktx.empty()) {
        if (error.empty()) {
            *cancelled = true;
            return {};
        }
        return in_path + ": " + error;
    }

    const std::string part_path = out_path + ".part";
    FILE* file = std::fopen(part_path.c_str(), "wb");
    if (!file) {
        return part_path + ": " + std::strerror(errno);
    }
    const bool written = std::fwrite(ktx.data(), 1, ktx.size(), file) == ktx.size();
    if (std::fclose(file) != 0 || !written) {
        std::remove(part_path.c_str());
        return part_path + ": write failed";
    }
    if (std::rename(part_path.c_str(), out_path.c_str()) != 0) {
        const std::string why = std::strerror(errno);
        std::remove(part_path.c_str());
        return out_path + ": " + why;
    }
    return {};
}

} // namespace CustomTex::Convert
