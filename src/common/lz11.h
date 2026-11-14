// Copyright 2015-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

// AzaharFrame kiuguns: the LZ11 decompressor that was a static function of the APT service
// (the shared font), moved here with bounds on both buffers so it can also read files from a
// game's RomFS (frame_xr's weapon models). The algorithm is unchanged.

#pragma once

#include <cstddef>
#include <vector>
#include "common/common_types.h"

namespace Common {

/// The decompressed size an LZ11 stream says it has (0 when `in` is not LZ11: type byte 0x11,
/// then a 24-bit size, or 0 and a 32-bit size).
std::size_t LZ11DecompressedSize(const u8* in, std::size_t in_size);

/// Decompresses `in` into `out` (at most `out_capacity` bytes). Returns the decompressed size, or
/// 0 when the stream is not LZ11, runs past either buffer or refers before the start.
std::size_t DecompressLZ11(const u8* in, std::size_t in_size, u8* out, std::size_t out_capacity);

/// The same into a vector sized to the stream's own size (capped at `max_size`). False on error.
bool DecompressLZ11(const u8* in, std::size_t in_size, std::vector<u8>& out,
                    std::size_t max_size = std::size_t{64} << 20);

} // namespace Common
