// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame moddata: zip extraction with no library (stored and deflate entries, CRC checked).
// No Qt. Not supported, and reported as such: zip64, encryption, other methods, 7z, rar.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Mods {

struct ArchiveFile {
    std::string path; // '/' separated, relative, never ".." or absolute
    std::string data;
};

struct ExtractResult {
    std::string error; // non-empty: failed, `files` is empty
    std::vector<ArchiveFile> files;
};

enum class ArchiveKind { Zip, SevenZip, Rar, Unknown };

u32 Crc32(std::string_view data);
ArchiveKind DetectArchive(std::string_view bytes);

/// Raw deflate (RFC 1951). nullopt on bad data or when the output would pass `max_size`.
std::optional<std::string> Inflate(std::string_view in, std::size_t max_size);

/// Extracts every file (directories are skipped). `max_total` caps the sum of the file sizes.
ExtractResult ExtractZip(std::string_view zip, std::size_t max_total = 256u * 1024 * 1024);

/// `path` cleaned for use as a relative path ('\\' to '/', "./" dropped); nullopt when it is absolute
/// or has a ".." part or a drive colon.
std::optional<std::string> SafeRelativePath(std::string_view path);

} // namespace Mods
