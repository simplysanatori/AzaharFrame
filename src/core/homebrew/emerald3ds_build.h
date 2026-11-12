// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cpphelpers: makes emerald3ds.pak from the player's own Pokémon Emerald ROM and the
// release's Emerald3DS-WebPayload.zip (github.com/ZallaxDev/emerald-3ds, CC BY-NC-SA 4.0, which is
// NOT part of this program and is downloaded when the player asks). This is the C++ port of what
// steam-frame/tools/emerald3ds-build.py ran (the release's Python builder: recipe.py, precompiled.py,
// pak.py, followers.py): the recipe's operations rebuild every game data file from the ROM, the
// release's precompiled programs write the 3D scenery from the ROM's own tiles, and the optional
// follower sprites are fetched (pinned by commit, size and SHA-256), converted and put in the pack.
// The ROM is only read. No Qt. Blocking: run off the GUI thread.
//
// What is not ported: the release's voxel generator scripts (35,000 lines of Python that a release
// runs only for files it did not precompile). v0.5.0 precompiles all of them; a release that does
// not is refused with a message saying so.

#pragma once

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Homebrew::Emerald3ds::Build {

// ---------------------------------------------------------------------------------------------
// Building blocks (public for the tests)

/// A .xz stream (what Python's lzma.compress writes), at most `max_size` bytes out.
std::optional<std::string> XzDecompress(std::string_view in, std::size_t max_size,
                                        std::string* error = nullptr);

/// GBA BIOS LZ77 (type 0x10) at `offset` of `src`. nullopt when there is no header or the data
/// ends early.
std::optional<std::string> Lz77Decompress(std::string_view src, std::size_t offset);

/// The pack: header, index sorted by id, payloads aligned to 32 bytes.
struct PakFile {
    std::string path;
    std::string data;
};
u64 PakPathId(std::string_view path);
u32 PakEngineAbi(const std::vector<std::pair<std::string, std::pair<u64, u32>>>& items);
std::string PakBytes(const std::vector<PakFile>& files, u32 abi, std::string_view rom_sha1_raw,
                     std::string* error);
/// Checks every payload's CRC-32 and the header and index checksums; the entry count, else 0 and
/// `error`.
std::size_t PakVerify(std::string_view pak, std::string* error);

// Followers (followers.py)
std::string ConvertNsbtx(std::string_view nsbtx, int flags, bool* ok);
std::vector<int> ParamFlags(std::string_view csv);
std::string EncodeMessage(std::string_view utf8_text, bool* ok);
std::string ConvertTexts(std::string_view corpus, bool* ok);

// ---------------------------------------------------------------------------------------------
// The whole build

struct Options {
    std::string payload_zip;
    std::string rom;
    std::string out_dir;
    std::string language;   ///< "" = the ROM's own (the first variant)
    std::string cache_dir;  ///< follower files by SHA-256; "" = out_dir/followers-cache
    bool no_network = false;
    /// 0..1000 and the step's text.
    std::function<void(int, const std::string&)> progress;
    const std::atomic<bool>* cancel = nullptr;
};

struct Result {
    bool ok = false;
    bool stopped = false;
    std::string error; ///< one line
    std::string abi;   ///< 8 hex digits
    std::string release;
    std::string language;
    bool variant_matches = true;
    u64 entries = 0;
    u64 bytes = 0;
    std::string ToJson() const; ///< one line
};

/// Writes OUT/emerald3ds.pak, OUT/Emerald3DS.3dsx and OUT/Emerald3DS.smdh (the ROM's language
/// variant).
Result BuildPack(const Options& options);

/// `--frame-emerald-build --payload-zip ZIP --rom ROM --out DIR [--language L] [--cache DIR]
/// [--no-network]`: prints "progress PERMILLE TEXT" lines, then "result {json}"; "error TEXT"
/// and exit 1 on failure (4 when stopped). Returns the exit code.
int RunCli(const std::vector<std::string>& args);

} // namespace Homebrew::Emerald3ds::Build
