// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ui3: Optimize for Frame on the VR menu's pack page. The work is done by the texture
// pack converter (convert.h, in the app; the Flat game list's "Optimize Texture
// Pack for Frame" runs it too): a PNG pack is copied as ASTC 4x4 KTX files into a sibling folder
// "<pack>-astc4x4" with the marker kMarker (written last), the original untouched. This module is
// the part with no Qt: which folders make a pair, the parts choices
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
#include <vector>
#include "common/common_types.h"

namespace CustomTex::Optimize {

inline constexpr const char* kMarker = ".azaharframe-ktx.json";
/// The converter's exit codes (convert.h's).
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

/// A failed run in a few words, from its exit code: "Not Enough Space", /// "Pack Not Readable" (2: usage, the folder), else "Conversion Failed".
std::string FailureText(int exit_code);

/// Copies the Customize choices (Parts::kStateFile) from one pack of the pair to the other, or
/// removes the target's when the source has none. False on a write error.
bool CarryParts(const std::string& from, const std::string& to);

/// AzaharFrame ui3: Remove Original. The one folder it may delete: the source the optimized
/// copy's marker names, resolved from the copy's real place, when it
/// - is there, a folder and not a link itself;
/// - is (after every link on the way is resolved) inside one of `roots` (the managed texture
///   folders: <load>/textures and <load>/texture-packs) and not a root itself;
/// - is not the copy, inside it or round it, nor an optimized copy itself;
/// - is not the pack `active` points at (load/textures/<TID>: what a running game reads).
/// `original` is that folder (canonical), or `error` why not.
struct RemoveCheck {
    std::string original;
    std::string error; ///< "Original Not Found", "Not the Original", "Original Is in Use", ...
    bool Ok() const {
        return error.empty();
    }
};
RemoveCheck CheckRemoveOriginal(const std::string& optimized, const std::vector<std::string>& roots,
                                const std::string& active);
/// Checks again and deletes it (no trash, as Delete does a downloaded pack). False with `error`.
bool RemoveOriginal(const std::string& optimized, const std::vector<std::string>& roots,
                    const std::string& active, std::string* error);
/// The pack came from the catalog (Download Packs' marker, not an Install from Zip one): Remove
/// Original's dialog says "download it again", else "add it again".
bool FromCatalog(const std::string& pack);
/// The managed texture folders of a load folder: <load>/textures, <load>/texture-packs.
std::vector<std::string> ManagedRoots(const std::string& load_dir);

/// The packs already asked about (by id), in <load>/texture-packs/optimize-asked.txt.
bool AskedBefore(const std::string& load_dir, const std::string& pack_id);
void MarkAsked(const std::string& load_dir, const std::string& pack_id);

} // namespace CustomTex::Optimize
