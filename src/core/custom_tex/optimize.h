// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ui3: Optimize for Frame on the VR menu's pack page. The work is done by the texture
// pack converter (steam-frame/tools/convert-texture-pack.sh, the Flat game list's "Optimize Texture
// Pack for Frame"): a PNG pack is copied as ASTC 4x4 KTX files into a sibling folder
// "<pack>-astc4x4" with the marker kMarker (written last), the original untouched. This module is
// the part with no Qt: which folders make a pair, what the converter prints, the parts choices
// carried across, and the packs already asked about. citra_qt/frame_mods.cpp runs the converter.
//
// A pair: the PNG pack and its optimized copy. The copy's marker names its source (relative). Both
// are packs of the game (ListPacks), so the player switches between them like any two packs; the
// pack page shows the pair's state, and the Customize choices (Parts::kStateFile) go with the
// switch both ways, since both folders hold the same part folders.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include "common/common_types.h"

namespace CustomTex::Optimize {

inline constexpr const char* kMarker = ".azaharframe-ktx.json";
/// The converter's exit codes (convert_texture_pack.py).
inline constexpr int kExitNoEncoder = 3;
inline constexpr int kExitStopped = 4;
inline constexpr int kExitNoSpace = 5;
/// A PNG pack this big becomes active or finishes downloading: "Optimize <Pack> for Frame?" once.
inline constexpr u64 kNudgeBytes = 1'000'000'000;

/// The pack's optimized copy as the converter names it: "<pack>-astc4x4" ("-original" dropped).
std::string OptimizedPath(const std::string& pack);
/// The folder holds the marker: it is an optimized copy (complete; the marker is written last).
bool IsOptimized(const std::string& pack);
/// The optimized copy's source pack (from the marker, made absolute and normal), nullopt when
/// `optimized` is not one or names none.
std::optional<std::string> SourceOf(const std::string& optimized);

struct Pair {
    std::string png;       ///< the original ("" none)
    std::string optimized; ///< its complete copy ("" none yet)
};
/// The pair `pack` belongs to: an optimized copy and its source, or a PNG pack (`is_png`) and its
/// copy when one is complete. Neither: both empty.
Pair PairOf(const std::string& pack, bool is_png);

/// "estimate FILES TOTAL_BYTES BYTES_STILL_TO_WRITE".
struct Estimate {
    u64 textures = 0, total_bytes = 0, need_bytes = 0;
};
std::optional<Estimate> ParseEstimate(std::string_view line);
/// "progress DONE TOTAL PERMILLE" (--machine).
struct Progress {
    int done = 0, total = 0, permille = 0;
};
std::optional<Progress> ParseProgress(std::string_view line);

/// A failed run in a few words, from its exit code: "Not Enough Space", "Encoder Not Built",
/// "Pack Not Readable" (2: usage, the folder), else "Conversion Failed".
std::string FailureText(int exit_code);

/// Copies the Customize choices (Parts::kStateFile) from one pack of the pair to the other, or
/// removes the target's when the source has none. False on a write error.
bool CarryParts(const std::string& from, const std::string& to);

/// The packs already asked about (by id), in <load>/texture-packs/optimize-asked.txt.
bool AskedBefore(const std::string& load_dir, const std::string& pack_id);
void MarkAsked(const std::string& load_dir, const std::string& pack_id);

} // namespace CustomTex::Optimize
