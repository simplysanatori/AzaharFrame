// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame catalog3: multipart downloads. A catalog entry may list "parts" (several sources, in
// order) in place of one file: the parts are downloaded to disk, joined into one archive and
// extracted as one. Joined by byte concatenation: multi-volume RAR (.part1.rar ... or .rar, .r00 ...;
// libarchive reads the concatenated volumes as one archive) and split 7z (.7z.001 ...). A split zip
// (.z01 ... .zip) is concatenated and its central directory is rewritten to the joined offsets.
// No Qt, no network in the pure parts.

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Mods::Multipart {

/// "4.6 GB", "300 MB".
std::string SizeText(u64 bytes);

/// Bytes to have free before starting: (parts + extracted) x 1.1. `extracted` is the unpacked size
/// (an estimate: the parts' own total when nothing better is known).
u64 RequiredBytes(u64 parts_bytes, u64 extracted_bytes);

/// "" when it fits in `free_bytes`, else "Not Enough Space: Needs 4.6 GB Free".
std::string SpaceError(u64 parts_bytes, u64 extracted_bytes, u64 free_bytes);

/// Free bytes of the file system holding `dir` (the nearest existing folder up the path).
bool FreeBytes(const std::string& dir, u64* out);

/// What the file names of an archive's parts say about it: "rar", "7z", "zip" or "" (not a split
/// archive, or a name this module does not know).
std::string SplitKindOfName(const std::string& name);

/// Concatenates `parts` (in order) into `out_path`. For a split zip (the last part ends with an end
/// of central directory record that names another disk) the central directory is rewritten so the
/// joined file is one ordinary zip. "" on success, else the error.
std::string JoinParts(const std::vector<std::string>& parts, const std::string& out_path,
                      const std::atomic_bool* cancel = nullptr,
                      const std::function<void(u64 done, u64 total)>& progress = {});

/// The joined-offsets rewrite of a split zip, alone (the file is the parts concatenated;
/// `part_sizes` the sizes in order). Nothing is changed for a single-disk zip. "" on success.
std::string FixSplitZip(const std::string& joined_path, const std::vector<u64>& part_sizes);

// ---------------------------------------------------------------------------------------------
// Download to a file

struct FileReply {
    int status = 0; // 0: no reply
    std::string location;
    std::string error;
    u64 total = 0; // Content-Length, 0 unknown
};
using Progress = std::function<void(u64 done, u64 total)>;
/// One GET of `https://host` + `path` into the file `dest` (no redirects followed: a 3xx comes back
/// with `location` and nothing written). Calls `progress` as bytes arrive.
using FileTransport = std::function<FileReply(const std::string& host, const std::string& path,
                                              const std::string& dest, const std::atomic_bool* cancel,
                                              const Progress& progress)>;
/// The real one (HTTPS, system certificates).
FileReply StreamGet(const std::string& host, const std::string& path, const std::string& dest,
                    const std::atomic_bool* cancel, const Progress& progress);

} // namespace Mods::Multipart
