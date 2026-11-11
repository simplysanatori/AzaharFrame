// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame moddata: zip extraction with no library (stored and deflate entries, CRC checked).
// 7z, rar and any other format libarchive reads go through the system's bsdtar (a pacman
// dependency, so on every SteamOS), run with a fixed argv, no shell, a time and size cap.
// No Qt. Not supported, and reported as such: zip64, encryption, other zip methods.

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

/// Path of the system's bsdtar ("" when there is none). Looked up in the usual system folders and
/// PATH; the result is cached.
std::string FindBsdtar();

/// For a non-zip archive: lists it (`bsdtar -tf`, `-tvf`), refuses unsafe paths, links, special files
/// and a total over `max_total`, extracts to a private temporary folder with the system's library
/// path (never the app bundle's), reads the files back and removes the folder. Error text names the
/// missing tool ("7z needs bsdtar, which wasn't found").
ExtractResult ExtractWithBsdtar(std::string_view bytes, ArchiveKind kind,
                                std::size_t max_total = 256u * 1024 * 1024, int timeout_seconds = 120,
                                const std::string& tool_override = {});

/// Zip by the built-in reader, everything else by bsdtar. `kind` from DetectArchive.
ExtractResult ExtractArchive(std::string_view bytes, ArchiveKind kind);

/// `path` cleaned for use as a relative path ('\\' to '/', "./" dropped); nullopt when it is absolute
/// or has a ".." part or a drive colon.
std::optional<std::string> SafeRelativePath(std::string_view path);

} // namespace Mods
