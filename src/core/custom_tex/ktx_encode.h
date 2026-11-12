// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// AzaharFrame cppconv: one PNG of a texture pack -> one KTX 1 file in ASTC with its full mip
// chain, for Azahar for Frame (convert.h). This is af-ktxenc, the encoder that was a helper
// program, as a library: ARM's astcenc (Apache-2.0, externals/astcenc) compresses, and the
// stb_image copy astcenc ships (public domain / MIT) decodes the PNGs.
//
// What Azahar does with the same PNG, done once here instead of at every load:
//  - decoded to 8-bit RGBA as stored (no gamma or colour conversion; the image is uploaded as
//    R8G8B8A8_UNORM, so the ASTC is the linear LDR profile and the KTX format is
//    GL_COMPRESSED_RGBA_ASTC_*_KHR, which Azahar maps to VK_FORMAT_ASTC_*_UNORM_BLOCK)
//  - flip: turned upside down (the pack's flip_png_files), which Azahar does to PNGs only
//  - the mip chain: each level the 2x2 average of the one above, in the stored 8-bit values, as
//    the GPU's linear blit makes them for a pack with skip_mipmap

#pragma once

#include <optional>
#include <string>
#include <vector>
#include "common/common_types.h"

struct astcenc_context;

namespace CustomTex::Convert {

/// ASTC block sizes Azahar reads.
struct BlockSize {
    unsigned x = 4, y = 4;
};
/// "4x4", "6x6" or "8x6".
std::optional<BlockSize> ParseBlock(const std::string& name);
/// "fast", "medium", "thorough" or a number 0-100 (astcenc's quality).
std::optional<float> ParseQuality(const std::string& text);

struct Level {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> pixels; ///< RGBA8
};
/// One level down: every output texel is the average of the input texels it covers (2x2, or
/// 2x3/3x2/3x3 at the last row or column of an odd size), so no input texel is dropped.
Level Downsample(const Level& src);

/// One compression context (not thread safe, except Cancel); reused for every texture.
class AstcEncoder {
public:
    AstcEncoder(BlockSize block, float quality);
    ~AstcEncoder();
    AstcEncoder(const AstcEncoder&) = delete;
    AstcEncoder& operator=(const AstcEncoder&) = delete;
    /// "" when the encoder is ready, else why not.
    const std::string& Error() const {
        return error_;
    }

    /// Reads `in_path`, writes `out_path` (as out_path + ".part", renamed when complete). "" on
    /// success, else the reason; `cancelled` is set when Cancel() ended it (nothing is written).
    std::string EncodeFile(const std::string& in_path, const std::string& out_path, bool flip,
                           bool* cancelled);
    /// The KTX file of RGBA8 `pixels` (width * height * 4 bytes). Empty when cancelled or failed
    /// (`error` says which, "" for cancelled).
    std::vector<u8> EncodeRgba(u32 width, u32 height, std::vector<u8> pixels, bool flip,
                               std::string* error);
    /// From any thread: ends the texture in hand and every one after it. Final.
    void Cancel();

private:
    BlockSize block_;
    astcenc_context* context_ = nullptr;
    std::string error_;
    struct State;
    State* state_;
};

} // namespace CustomTex::Convert
