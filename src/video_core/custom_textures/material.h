// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <span>
#include <string>
#include <vector>
#include "video_core/custom_textures/custom_format.h"

namespace Frontend {
class ImageInterface;
}

namespace VideoCore {

enum class MapType : u32 {
    Color = 0,
    Normal = 1,
    MapCount = 2,
};
constexpr std::size_t MAX_MAPS = static_cast<std::size_t>(MapType::MapCount);

enum class DecodeState : u32 {
    None = 0,
    Pending = 1,
    Decoded = 2,
    Failed = 3,
};

/// AzaharFrame: one stored mip level of a custom texture, as a range of CustomTexture::data
struct CustomLevel {
    std::size_t offset;
    std::size_t size;
    u32 width;
    u32 height;
};

class CustomTexture {
public:
    explicit CustomTexture(Frontend::ImageInterface& image_interface);
    ~CustomTexture();

    void LoadFromDisk(bool flip_png);

    /// AzaharFrame: drops the pixel data of a DDS or KTX file once the GPU has it. Reading it
    /// again is a plain file read, unlike a PNG, which is decoded. Returns true if it was dropped.
    bool Release();

    [[nodiscard]] bool IsParsed() const noexcept {
        return file_format != CustomFileFormat::None && !hashes.empty();
    }

    [[nodiscard]] bool IsLoaded() const noexcept {
        return !data.empty();
    }

    /// AzaharFrame: the number of mip levels stored in the file (1 for a PNG)
    [[nodiscard]] u32 NumLevels() const noexcept {
        return static_cast<u32>(levels.size());
    }

    /// AzaharFrame: the pixel data of one stored mip level
    [[nodiscard]] std::span<const u8> LevelData(u32 level) const noexcept {
        const CustomLevel& info = levels[level];
        return std::span<const u8>{data}.subspan(info.offset, info.size);
    }

private:
    void LoadPNG(std::span<const u8> input, bool flip_png);

    void LoadDDS(std::vector<u8>&& input);

public:
    Frontend::ImageInterface& image_interface;
    std::string path;
    u32 width{};
    u32 height{};
    std::vector<u64> hashes;
    std::mutex decode_mutex;
    CustomPixelFormat format{CustomPixelFormat::Invalid};
    CustomFileFormat file_format{CustomFileFormat::None};
    std::vector<u8> data;
    std::vector<CustomLevel> levels; ///< AzaharFrame: mip levels in data, level 0 first
    MapType type;
};

struct Material {
    u32 width{};
    u32 height{};
    u64 size{};
    u64 hash;
    CustomPixelFormat format{CustomPixelFormat::Invalid};
    u32 levels{1}; ///< AzaharFrame: mip levels every map of this material has stored
    bool per_level_files{}; ///< AzaharFrame: the pack replaces each mip level with its own file
    std::array<CustomTexture*, MAX_MAPS> textures;
    std::atomic<DecodeState> state{};

    void LoadFromDisk(bool flip_png) noexcept;

    void AddMapTexture(CustomTexture* texture) noexcept;

    /// AzaharFrame: true when the maps bring their own mip chain (DDS/KTX with mips, or any
    /// compressed format, which the GPU cannot blit into), so mipmaps are never generated
    [[nodiscard]] bool HasStoredMips() const noexcept {
        return levels > 1 || IsCustomFormatCompressed(format);
    }

    /// AzaharFrame: the mip levels to give the image of a surface with `surface_levels`.
    /// A stored chain shorter than the surface clamps the image, so no level is left unwritten
    [[nodiscard]] u32 ImageLevels(u32 surface_levels) const noexcept;

    /// AzaharFrame: true when every map has its pixel data in memory
    [[nodiscard]] bool IsResident() const noexcept;

    /// AzaharFrame: drops the pixel data of maps that are cheap to read again. The material
    /// goes back to unloaded and is read again when a surface needs it. Render thread only
    void Release() noexcept;

    [[nodiscard]] CustomTexture* Map(MapType type) const noexcept {
        return textures.at(static_cast<std::size_t>(type));
    }

    [[nodiscard]] bool IsPending() const noexcept {
        return state == DecodeState::Pending;
    }

    [[nodiscard]] bool IsFailed() const noexcept {
        return state == DecodeState::Failed;
    }

    [[nodiscard]] bool IsDecoded() const noexcept {
        return state == DecodeState::Decoded;
    }

    [[nodiscard]] bool IsUnloaded() const noexcept {
        return state == DecodeState::None;
    }
};

} // namespace VideoCore
