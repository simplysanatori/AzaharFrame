// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ctgp7: the player's own Mario Kart 7 as a CIA, so it can be installed as a title (the
// CTGP-7 launcher only starts an installed Mario Kart 7). A decrypted cartridge image (.3ds, .cci,
// NCSD) gives its partition 0, the game's NCCH; a bare NCCH (.cxi) is used whole. The CIA has one
// content, unencrypted, with a ticket and a TMD made here (title ID, title version, the content's
// size and SHA-256); its signatures are left blank and it carries no certificate chain: Azahar's
// installer only checks signatures for a CIA a game installs through the AM service, never for one
// the frontend installs. The image is only read. A .cia is installed as it is (no conversion).
// The C++ form of handoff/reports/32/ctgp7/tools/ncsd2cia.py, without its template CIA.

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <span>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Ctgp7::Cia {

enum class Kind : u8 { Unknown, Ncsd, Ncch, Cia };

/// What the image holds.
struct Source {
    Kind kind = Kind::Unknown;
    u64 title_id = 0;
    u16 version = 0;     ///< the NCCH's version (the TMD's title version)
    u64 offset = 0;      ///< where the game's NCCH starts in the file
    u64 size = 0;        ///< and its length
    u32 save_size = 0;   ///< the exheader's save data size (the TMD's), 0 none
    bool encrypted = false;
    std::string error;   ///< "" when it can be converted (or, a CIA, installed)
};

/// Reads the image's headers (a .z3ds/.zcci is read through Azahar's decompressor).
Source Inspect(const std::string& path);
/// The same from the bytes at the start of the file (a CIA: its header; an NCSD: up to and
/// including partition 0's NCCH header and exheader, so at least partition 0's offset + 0x400).
Source InspectBytes(std::span<const u8> head, u64 file_size);

/// The ticket and TMD the CIA carries (exposed for the tests).
std::vector<u8> MakeTicket(u64 title_id, u16 version);
std::vector<u8> MakeTmd(u64 title_id, u16 version, u64 content_size,
                        const std::array<u8, 32>& content_sha256, u32 save_size);

/// Writes the CIA to `out` (through `out` + ".part", renamed when whole). `progress(done, total)`
/// in bytes of the game. "" on success, "Cancelled", or why.
std::string Build(const std::string& image, const std::string& out, const std::atomic_bool* cancel,
                  const std::function<void(u64 done, u64 total)>& progress);

} // namespace Ctgp7::Cia
