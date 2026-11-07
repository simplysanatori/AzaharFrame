// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include "video_core/custom_textures/custom_format.h"

namespace VideoCore {

std::string_view CustomPixelFormatAsString(CustomPixelFormat format) {
    switch (format) {
    case CustomPixelFormat::RGBA8:
        return "RGBA8";
    case CustomPixelFormat::BC1:
        return "BC1";
    case CustomPixelFormat::BC3:
        return "BC3";
    case CustomPixelFormat::BC5:
        return "BC5";
    case CustomPixelFormat::BC7:
        return "BC7";
    case CustomPixelFormat::ASTC4:
        return "ASTC4";
    case CustomPixelFormat::ASTC6:
        return "ASTC6";
    case CustomPixelFormat::ASTC8:
        return "ASTC8";
    default:
        return "NotReal";
    }
}

bool IsCustomFormatCompressed(CustomPixelFormat format) {
    return format != CustomPixelFormat::RGBA8 && format != CustomPixelFormat::Invalid;
}

std::size_t CustomFormatLevelSize(CustomPixelFormat format, u32 width, u32 height) {
    // Block width, block height and bytes per block
    u32 block_w = 4;
    u32 block_h = 4;
    u32 block_bytes = 16;
    switch (format) {
    case CustomPixelFormat::RGBA8:
        return static_cast<std::size_t>(width) * height * 4;
    case CustomPixelFormat::BC1:
        block_bytes = 8;
        break;
    case CustomPixelFormat::BC3:
    case CustomPixelFormat::BC5:
    case CustomPixelFormat::BC7:
    case CustomPixelFormat::ASTC4:
        break;
    case CustomPixelFormat::ASTC6:
        block_w = 6;
        block_h = 6;
        break;
    case CustomPixelFormat::ASTC8:
        // ASTC8 is the 8x6 block size (see ToCustomPixelFormat and MakeCustomFormat)
        block_w = 8;
        block_h = 6;
        break;
    default:
        return 0;
    }
    const std::size_t blocks_x = (width + block_w - 1) / block_w;
    const std::size_t blocks_y = (height + block_h - 1) / block_h;
    return blocks_x * blocks_y * block_bytes;
}

} // namespace VideoCore
