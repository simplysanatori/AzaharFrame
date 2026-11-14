// Copyright 2020-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/unordered_map.hpp>
#include "common/archives.h"
#include "common/common_types.h"

namespace Common::Compression {

/**
 * Compresses a source memory region with Zstandard and returns the compressed data in a vector.
 *
 * @param source the uncompressed source memory region.
 * @param compression_level the used compression level. Should be between 1 and 22.
 *
 * @return the compressed data.
 */
[[nodiscard]] std::vector<u8> CompressDataZSTD(std::span<const u8> source, s32 compression_level);

/**
 * Compresses a source memory region with Zstandard with the default compression level and returns
 * the compressed data in a vector.
 *
 * @param source the uncompressed source memory region.
 *
 * @return the compressed data.
 */
[[nodiscard]] std::vector<u8> CompressDataZSTDDefault(std::span<const u8> source);

/**
 * AzaharFrame savemem: compresses `source` into one Zstandard frame as CompressDataZSTD does (the
 * same level, the content size in the frame header, which DecompressDataZSTD needs), but hands
 * the compressed bytes to `sink` piece by piece (ZSTD_CStreamOutSize(), about 128 KiB) as they
 * come. No buffer of ZSTD_compressBound(source) (about the source's size, zero-filled, so all of
 * it in memory) is held, nor a copy of the result: a save state's 300 MB system needs a few MB
 * here instead of 300 MB more.
 *
 * @param sink receives each piece in order; returning false stops the compression.
 *
 * @return the compressed size, or nullopt on a failure (or when `sink` stopped it).
 */
[[nodiscard]] std::optional<std::size_t> CompressDataZSTDTo(
    std::span<const u8> source, s32 compression_level,
    const std::function<bool(std::span<const u8>)>& sink);

/// AzaharFrame savemem: CompressDataZSTDTo at the default level (CompressDataZSTDDefault's).
[[nodiscard]] std::optional<std::size_t> CompressDataZSTDDefaultTo(
    std::span<const u8> source, const std::function<bool(std::span<const u8>)>& sink);

/**
 * Gets the decompressed size of the specified Zstandard compressed memory region.
 *
 * @param compressed the compressed source memory region.
 *
 * @return the size of the decompressed data.
 */
[[nodiscard]] std::size_t GetDecompressedSize(std::span<const u8> compressed);

/**
 * Decompresses a source memory region with Zstandard and returns the uncompressed data in a vector.
 *
 * @param compressed the compressed source memory region.
 *
 * @return the decompressed data.
 */
[[nodiscard]] std::vector<u8> DecompressDataZSTD(std::span<const u8> compressed);

} // namespace Common::Compression

namespace FileUtil {

using ProgressCallback = void(std::size_t, std::size_t);

bool CompressZ3DSFile(const std::string& src_file, const std::string& dst_file,
                      const std::array<u8, 4>& underlying_magic, size_t frame_size,
                      std::function<ProgressCallback>&& update_callback = nullptr,
                      std::unordered_map<std::string, std::vector<u8>> metadata = {});

bool DeCompressZ3DSFile(const std::string& src_file, const std::string& dst_file,
                        std::function<ProgressCallback>&& update_callback = nullptr);

} // namespace FileUtil