// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cstring>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/texture.h"
#include "core/frontend/image_interface.h"
#include "video_core/custom_textures/material.h"

namespace VideoCore {

namespace {

CustomPixelFormat ToCustomPixelFormat(ddsktx_format format) {
    switch (format) {
    case DDSKTX_FORMAT_RGBA8:
        return CustomPixelFormat::RGBA8;
    case DDSKTX_FORMAT_BC1:
        return CustomPixelFormat::BC1;
    case DDSKTX_FORMAT_BC3:
        return CustomPixelFormat::BC3;
    case DDSKTX_FORMAT_BC5:
        return CustomPixelFormat::BC5;
    case DDSKTX_FORMAT_BC7:
        return CustomPixelFormat::BC7;
    case DDSKTX_FORMAT_ASTC4x4:
        return CustomPixelFormat::ASTC4;
    case DDSKTX_FORMAT_ASTC6x6:
        return CustomPixelFormat::ASTC6;
    case DDSKTX_FORMAT_ASTC8x6:
        return CustomPixelFormat::ASTC8;
    default:
        LOG_ERROR(Common, "Unknown dds/ktx pixel format {}", format);
        return CustomPixelFormat::Invalid;
    }
}

std::string_view MapTypeName(MapType type) {
    switch (type) {
    case MapType::Color:
        return "Color";
    case MapType::Normal:
        return "Normal";
    default:
        return "Invalid";
    }
}

} // Anonymous namespace

CustomTexture::CustomTexture(Frontend::ImageInterface& image_interface_)
    : image_interface{image_interface_} {}

CustomTexture::~CustomTexture() = default;

void CustomTexture::LoadFromDisk(bool flip_png) {
    std::scoped_lock lock{decode_mutex};
    if (IsLoaded()) {
        return;
    }

    FileUtil::IOFile file{path, "rb"};
    std::vector<u8> input(file.GetSize());
    if (input.empty() || file.ReadBytes(input.data(), input.size()) != input.size()) {
        LOG_CRITICAL(Render, "Failed to open custom texture: {}", path);
        return;
    }
    switch (file_format) {
    case CustomFileFormat::PNG:
        LoadPNG(input, flip_png);
        break;
    case CustomFileFormat::DDS:
    case CustomFileFormat::KTX:
        LoadDDS(std::move(input));
        break;
    default:
        LOG_ERROR(Render, "Unknown file format {}", file_format);
    }
}

bool CustomTexture::Release() {
    // A file mapped to more than one hash belongs to more than one material; it stays
    if ((file_format != CustomFileFormat::DDS && file_format != CustomFileFormat::KTX) ||
        hashes.size() > 1) {
        return false;
    }
    std::scoped_lock lock{decode_mutex};
    data = {};
    levels.clear();
    return true;
}

void CustomTexture::LoadPNG(std::span<const u8> input, bool flip_png) {
    if (!image_interface.DecodePNG(data, width, height, input)) {
        LOG_ERROR(Render, "Failed to decode png: {}", path);
        data.clear();
        return;
    }
    if (flip_png) {
        Common::FlipRGBA8Texture(data, width, height);
    }
    format = CustomPixelFormat::RGBA8;
    levels.assign(1, CustomLevel{
                         .offset = 0,
                         .size = data.size(),
                         .width = width,
                         .height = height,
                     });
}

// AzaharFrame: keeps every mip level of the file, not just the first. The levels are found
// here rather than with ddsktx_get_sub, which sizes every compressed format in 4x4 blocks
// (wrong for ASTC 6x6 and 8x6). The file stays in memory as read; levels point into it.
void CustomTexture::LoadDDS(std::vector<u8>&& input) {
    ddsktx_texture_info info{};
    ddsktx_error error{};
    if (!ddsktx_parse(&info, input.data(), static_cast<int>(input.size()), &error)) {
        LOG_ERROR(Render, "Failed to parse {}: {}", path, error.msg);
        return;
    }
    if ((info.flags & (DDSKTX_TEXTURE_FLAG_CUBEMAP | DDSKTX_TEXTURE_FLAG_VOLUME)) ||
        info.num_layers > 1 || info.depth > 1) {
        LOG_ERROR(Render, "{} is not a plain 2D texture, skipping", path);
        return;
    }
    const CustomPixelFormat parsed_format = ToCustomPixelFormat(info.format);
    if (parsed_format == CustomPixelFormat::Invalid || info.width <= 0 || info.height <= 0) {
        return;
    }

    // KTX 1 puts a 32-bit image size before each level and pads each level to 4 bytes.
    // DDS stores the levels of the first (only) image back to back.
    const bool is_ktx = (info.flags & DDSKTX_TEXTURE_FLAG_KTX) != 0;
    std::vector<CustomLevel> parsed_levels;
    std::size_t offset = static_cast<std::size_t>(info.data_offset);
    u32 level_width = static_cast<u32>(info.width);
    u32 level_height = static_cast<u32>(info.height);
    for (int level = 0; level < info.num_mips; level++) {
        const std::size_t size = CustomFormatLevelSize(parsed_format, level_width, level_height);
        if (is_ktx) {
            u32 image_size{};
            if (offset + sizeof(image_size) > input.size()) {
                break;
            }
            std::memcpy(&image_size, input.data() + offset, sizeof(image_size));
            offset += sizeof(image_size);
            if (image_size != size) {
                LOG_ERROR(Render, "{} level {} holds {} bytes, expected {}", path, level,
                          image_size, size);
                break;
            }
        }
        if (size == 0 || offset + size > input.size()) {
            LOG_ERROR(Render, "{} ends inside level {}", path, level);
            break;
        }
        parsed_levels.push_back(CustomLevel{
            .offset = offset,
            .size = size,
            .width = level_width,
            .height = level_height,
        });
        offset += size;
        if (is_ktx) {
            offset = (offset + 3) & ~std::size_t{3};
        }
        level_width = std::max(level_width >> 1, 1U);
        level_height = std::max(level_height >> 1, 1U);
    }
    if (parsed_levels.empty()) {
        return;
    }

    width = static_cast<u32>(info.width);
    height = static_cast<u32>(info.height);
    format = parsed_format;
    levels = std::move(parsed_levels);
    data = std::move(input);
}

void Material::LoadFromDisk(bool flip_png) noexcept {
    if (IsDecoded()) {
        return;
    }
    size = 0;
    for (CustomTexture* const texture : textures) {
        if (!texture) {
            continue;
        }
        texture->LoadFromDisk(flip_png);
        size += texture->data.size();
        LOG_DEBUG(Render, "Loading {} map {}", MapTypeName(texture->type), texture->path);
    }
    if (!textures[0]) {
        LOG_ERROR(Render, "Unable to create material without color texture!");
        state = DecodeState::Failed;
        return;
    }
    // AzaharFrame: a file that could not be read or decoded fails the material, rather than
    // leaving it decoded with no pixels and an unset size
    for (const CustomTexture* texture : textures) {
        if (texture && !texture->IsLoaded()) {
            LOG_ERROR(Render, "{} map {} could not be loaded", MapTypeName(texture->type),
                      texture->path);
            state = DecodeState::Failed;
            return;
        }
    }
    width = textures[0]->width;
    height = textures[0]->height;
    format = textures[0]->format;
    levels = textures[0]->NumLevels();
    for (const CustomTexture* texture : textures) {
        if (!texture) {
            continue;
        }
        if (texture->width != width || texture->height != height) {
            LOG_ERROR(Render,
                      "{} map {} of material with hash {:#016X} has dimentions {}x{} "
                      "which do not match the color texture dimentions {}x{}",
                      MapTypeName(texture->type), texture->path, hash, texture->width,
                      texture->height, width, height);
            state = DecodeState::Failed;
            return;
        }
        if (texture->format != format) {
            LOG_ERROR(
                Render, "{} map {} is stored with {} format which does not match color format {}",
                MapTypeName(texture->type), texture->path,
                CustomPixelFormatAsString(texture->format), CustomPixelFormatAsString(format));
            state = DecodeState::Failed;
            return;
        }
        levels = std::min(levels, texture->NumLevels());
    }
    state = DecodeState::Decoded;
}

u32 Material::ImageLevels(u32 surface_levels) const noexcept {
    if (!HasStoredMips()) {
        // A PNG: the rest of the chain is generated, or comes from one file per level
        return surface_levels;
    }
    if (levels == 1 && per_level_files) {
        // A single-level DDS/KTX in a pack that has a file for each level
        return surface_levels;
    }
    return std::clamp(levels, 1U, std::max(surface_levels, 1U));
}

bool Material::IsResident() const noexcept {
    for (const CustomTexture* texture : textures) {
        if (texture && !texture->IsLoaded()) {
            return false;
        }
    }
    return true;
}

void Material::Release() noexcept {
    if (!IsDecoded()) {
        return;
    }
    bool released = false;
    for (CustomTexture* const texture : textures) {
        if (texture) {
            released |= texture->Release();
        }
    }
    if (released) {
        size = 0;
        state = DecodeState::None;
    }
}

void Material::AddMapTexture(CustomTexture* texture) noexcept {
    const std::size_t index = static_cast<std::size_t>(texture->type);
    if (textures[index]) {
        LOG_ERROR(Render, "Textures {} and {} are assigned to the same material, ignoring!",
                  textures[index]->path, texture->path);
        return;
    }
    textures[index] = texture;
}

} // namespace VideoCore
