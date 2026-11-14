// Copyright 2015-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#include "common/lz11.h"

// AzaharFrame kiuguns (lz11.h): the APT service's DecompressLZ11, with bounds.
namespace Common {

namespace {
std::size_t Header(const u8* in, std::size_t in_size, std::size_t& size) {
    if (in == nullptr || in_size < 4 || in[0] != 0x11) {
        return 0;
    }
    size = static_cast<std::size_t>(in[1]) | static_cast<std::size_t>(in[2]) << 8 |
           static_cast<std::size_t>(in[3]) << 16;
    if (size != 0) {
        return 4;
    }
    if (in_size < 8) {
        return 0;
    }
    size = static_cast<std::size_t>(in[4]) | static_cast<std::size_t>(in[5]) << 8 |
           static_cast<std::size_t>(in[6]) << 16 | static_cast<std::size_t>(in[7]) << 24;
    return 8;
}
} // namespace

std::size_t LZ11DecompressedSize(const u8* in, std::size_t in_size) {
    std::size_t size = 0;
    return Header(in, in_size, size) ? size : 0;
}

std::size_t DecompressLZ11(const u8* in, std::size_t in_size, u8* out, std::size_t out_capacity) {
    std::size_t decompressed_size = 0;
    std::size_t pos = Header(in, in_size, decompressed_size);
    if (pos == 0 || decompressed_size > out_capacity || out == nullptr) {
        return 0;
    }
    const auto byte = [&](u8& b) {
        if (pos >= in_size) {
            return false;
        }
        b = in[pos++];
        return true;
    };
    std::size_t current_out_size = 0;
    u8 flags = 0, mask = 1;
    while (current_out_size < decompressed_size) {
        if (mask == 1) {
            if (!byte(flags)) {
                return 0;
            }
            mask = 0x80;
        } else {
            mask >>= 1;
        }

        if (flags & mask) {
            u8 byte1 = 0, byte2 = 0, byte3 = 0, byte4 = 0;
            if (!byte(byte1)) {
                return 0;
            }
            std::size_t length = byte1 >> 4;
            std::size_t offset;
            if (length == 0) {
                if (!byte(byte2) || !byte(byte3)) {
                    return 0;
                }
                length = (((byte1 & 0x0F) << 4) | (byte2 >> 4)) + 0x11;
                offset = (((byte2 & 0x0F) << 8) | byte3) + 0x1;
            } else if (length == 1) {
                if (!byte(byte2) || !byte(byte3) || !byte(byte4)) {
                    return 0;
                }
                length = (((byte1 & 0x0F) << 12) | (byte2 << 4) | (byte3 >> 4)) + 0x111;
                offset = (((byte3 & 0x0F) << 8) | byte4) + 0x1;
            } else {
                if (!byte(byte2)) {
                    return 0;
                }
                length = (byte1 >> 4) + 0x1;
                offset = (((byte1 & 0x0F) << 8) | byte2) + 0x1;
            }
            if (offset > current_out_size || length > decompressed_size - current_out_size) {
                return 0;
            }
            for (std::size_t i = 0; i < length; i++) {
                out[current_out_size] = out[current_out_size - offset];
                ++current_out_size;
            }
        } else {
            u8 b = 0;
            if (!byte(b)) {
                return 0;
            }
            out[current_out_size++] = b;
        }
    }
    return decompressed_size;
}

bool DecompressLZ11(const u8* in, std::size_t in_size, std::vector<u8>& out,
                    std::size_t max_size) {
    const std::size_t size = LZ11DecompressedSize(in, in_size);
    if (size == 0 || size > max_size) {
        return false;
    }
    out.resize(size);
    if (DecompressLZ11(in, in_size, out.data(), out.size()) != size) {
        out.clear();
        return false;
    }
    return true;
}

} // namespace Common
