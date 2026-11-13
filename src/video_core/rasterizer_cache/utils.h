// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <span>
#include "common/math_util.h"
#include "common/settings.h" // AzaharFrame xfergpu: ImageCopyAccuracy
#include "common/vector_math.h"

namespace VideoCore {

struct Offset {
    u32 x = 0;
    u32 y = 0;
};

struct Extent {
    u32 width = 1;
    u32 height = 1;
};

union ClearValue {
    Common::Vec4f color;
    struct {
        float depth;
        u8 stencil;
    };
};

struct TextureClear {
    u32 texture_level;
    u32 texture_layer;
    Common::Rectangle<u32> texture_rect;
    ClearValue value;
};

struct TextureCopy {
    u32 src_level;
    u32 dst_level;
    u32 src_layer;
    u32 dst_layer;
    Offset src_offset;
    Offset dst_offset;
    Extent extent;
};

struct TextureBlit {
    u32 src_level;
    u32 dst_level;
    u32 src_layer;
    u32 dst_layer;
    Common::Rectangle<u32> src_rect;
    Common::Rectangle<u32> dst_rect;
};

struct BufferTextureCopy {
    u32 buffer_offset;
    u32 buffer_size;
    Common::Rectangle<u32> texture_rect;
    u32 texture_level;
};

struct StagingData {
    u32 size;
    u32 offset;
    std::span<u8> mapped;
};

class SurfaceParams;
struct FramebufferParams;

u32 MipLevels(u32 width, u32 height, u32 max_level);

/**
 * Encodes a linear texture to the expected linear or tiled format.
 *
 * @param surface_info Structure used to query the surface information.
 * @param start_addr The start address of the dest data. Used if tiled.
 * @param end_addr The end address of the dest data. Used if tiled.
 * @param source_tiled The source linear texture data.
 * @param dest_linear The output buffer where the encoded linear or tiled data will be written to.
 * @param convert Whether the pixel format needs to be converted.
 */
void EncodeTexture(const SurfaceParams& surface_info, PAddr start_addr, PAddr end_addr,
                   std::span<u8> source, std::span<u8> dest, bool convert = false);

/**
 * Decodes a linear or tiled texture to the expected linear format.
 *
 * @param surface_info Structure used to query the surface information.
 * @param start_addr The start address of the source data. Used if tiled.
 * @param end_addr The end address of the source data. Used if tiled.
 * @param source_tiled The source linear or tiled texture data.
 * @param dest_linear The output buffer where the decoded linear data will be written to.
 * @param convert Whether the pixel format needs to be converted.
 */
void DecodeTexture(const SurfaceParams& surface_info, PAddr start_addr, PAddr end_addr,
                   std::span<u8> source, std::span<u8> dest, bool convert = false);

/// AzaharFrame feshimmer: whether a transfer's output [out, out + out_size) overlaps its
/// input [in, in + in_size) in guest memory, as an in-place downscale does (see
/// AccelerateDisplayTransfer).
[[nodiscard]] constexpr bool TransferOverlaps(u32 in, u32 in_size, u32 out, u32 out_size) {
    return in_size != 0 && out_size != 0 && out < in + in_size && in < out + out_size;
}

/// AzaharFrame accxfer, xfergpu: whether a display transfer takes the software path, by
/// Accurate Image Copies (Settings::values.image_copy_accuracy). On: always. Off: never, as
/// upstream. Automatic: when its output overlaps its input and the GPU path can't do it: the
/// GPU path keeps the input's pixel format and tiling (the cache then moves the data between
/// the strides with ValidateByRestride); AccelerateDisplayTransfer also sends an output that
/// lands in the input's own surface here.
[[nodiscard]] constexpr bool DisplayTransferInSoftware(Settings::ImageCopyAccuracy accuracy, u32 in,
                                                       u32 in_size, u32 out, u32 out_size,
                                                       bool gpu_can_overlap) {
    switch (accuracy) {
    case Settings::ImageCopyAccuracy::Off:
        return false;
    case Settings::ImageCopyAccuracy::On:
        return true;
    case Settings::ImageCopyAccuracy::Automatic:
        break;
    }
    return TransferOverlaps(in, in_size, out, out_size) && !gpu_can_overlap;
}

/// AzaharFrame accxfer, xfergpu: texture copies take the software path with Accurate Image
/// Copies On only.
[[nodiscard]] constexpr bool TextureCopyInSoftware(Settings::ImageCopyAccuracy accuracy) {
    return accuracy == Settings::ImageCopyAccuracy::On;
}

/// AzaharFrame xfergpu: whether the cache copies memory between strides on the GPU
/// (ValidateByRestride): not with Accurate Image Copies Off, which keeps upstream's skip.
[[nodiscard]] constexpr bool RestrideOnGpu(Settings::ImageCopyAccuracy accuracy) {
    return accuracy != Settings::ImageCopyAccuracy::Off;
}

/// AzaharFrame xfergpu: splits [start, end) into pieces that each lie within one row of tiles
/// (8 pixel rows; one pixel row for a linear surface) both of a surface at dst_addr with rows of
/// dst_row_bytes and of one at src_addr with rows of src_row_bytes. Each piece is then a
/// rectangle one row of tiles high in both surfaces, whatever their strides, so the data moves
/// from one to the other with an image copy at the scaled resolution, as the guest sees the
/// same memory with another stride. start must not lie before either address.
template <typename Func>
constexpr void ForEachRestrideChunk(u32 start, u32 end, u32 dst_addr, u32 dst_row_bytes,
                                    u32 src_addr, u32 src_row_bytes, Func&& func) {
    for (u32 pos = start; pos < end;) {
        const u32 dst_row_end = dst_addr + ((pos - dst_addr) / dst_row_bytes + 1) * dst_row_bytes;
        const u32 src_row_end = src_addr + ((pos - src_addr) / src_row_bytes + 1) * src_row_bytes;
        u32 chunk_end = end < dst_row_end ? end : dst_row_end;
        chunk_end = chunk_end < src_row_end ? chunk_end : src_row_end;
        func(pos, chunk_end);
        pos = chunk_end;
    }
}

} // namespace VideoCore
