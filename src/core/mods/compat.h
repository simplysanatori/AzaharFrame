// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame compat: catalog entries (mods, texture packs) made for the running game but another
// region, release or version. Install Mods and Download Packs list them after the ones for this
// game, marked "Not for Your Version", and installing one asks first ("This May Not Work"). Pure:
// no Qt, no core state, so the tests reach all of it.
//
// The same game: two title IDs belong to one game when a catalog entry lists both, or when two
// entries have the same title once a trailing "(USA)" style note is dropped. The catalog lists
// every region a mod serves, so its own lists join a game's regions (Ocarina of Time 3D's USA,
// Europe, Japan, Korea and Taiwan IDs), and nothing ever joins two different games.
//
// Why it may not fit, first that applies:
//   Build    the entry lists code CRC32s (Project Restoration), the running game's code matches
//            another entry of the game, and not this one ("Matches Your Game" on that one); when
//            no entry matches the game's code, the version below decides
//   Version  the entry's version chip is a plain version ("1.1") and the game's differs; the
//            game's version is the one it shows (Cheats::Sharkive: its raw version, from an
//            installed update, the cartridge or CIA itself, looked up in a table of what each
//            raw version shows; 1.0 for a game as released). A raw version the table does not
//            list ("Update Installed", "Rev 1") confirms no version: no entry's chip fits it
//   Region   the entry does not list the game's title ID and serves other regions only
//   Release  the entry does not list the game's title ID and the regions do not tell why (another
//            edition, such as Fire Emblem Fates' Special Edition and Birthright)
// Nothing is ever said for certain: every text says "may".

#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Mods::Compat {

enum class Region : u8 { Unknown, Usa, Europe, Japan, Korea, Taiwan, China };

/// The region of a title ID this program knows (the catalogs' games), else Unknown.
Region RegionOfTitle(u64 title_id);
/// "USA", "Europe", "Japan", "Korea", "Taiwan", "China" ("" for Unknown): for the chips.
std::string RegionName(Region region);
/// "USA", "European", "Japanese", "Korean", "Taiwanese", "Chinese": "the European version".
std::string RegionAdjective(Region region);

struct Version {
    int major = 0;
    int minor = 0;
    bool operator==(const Version&) const = default;
};
/// "1.1" or "v1.1" (a whole label); nullopt for "Card 1.1", "Fallback", "1", "".
std::optional<Version> ParseVersion(std::string_view label);
/// "1.1".
std::string VersionText(const Version& v);
/// <0, 0, >0 as a is before, equal to or after b.
int CompareVersions(const Version& a, const Version& b);

/// What is known about the running game.
struct Game {
    u64 title_id = 0;
    Region region = Region::Unknown;   ///< RegionOfTitle, else from the game's own region lock
    std::optional<Version> version;    ///< see the top of this file
    std::string version_source;        ///< "update, TMD v3072", "installed title, TMD v0", "assumed": for the log
    bool unmapped = false;             ///< AzaharFrame versions: the game has a raw version above its
                                       ///< release and the table does not know what it shows: no
                                       ///< entry's version chip fits
    std::string version_state;         ///< for a sentence: "has an update installed", "is Rev 1"
    std::optional<u16> raw_version;    ///< the raw version (TMD style), for UpdateVersion and the log
    std::optional<u32> code_crc;       ///< the CRC32 of the game's .code
};

/// The game's version for callers that have none from Cheats::Online::QueryInstalledVersion: an
/// installed update title's (or the installed title's own) version; with neither, 1.0 unless the
/// cartridge names a later revision (then unknown). `source` gets "update", "installed title",
/// "assumed" or "".
std::optional<Version> GameVersion(std::optional<Version> update_tmd, std::optional<Version> base_tmd,
                                   std::optional<int> cartridge_revision, std::string* source);

/// Looks the game's raw version up again in the table (the online "versions" section may have
/// arrived since the game's info was made): an entry gives the version it shows.
void UpdateVersion(Game& game);

/// "the legend of zelda: ocarina of time 3d" from "The Legend of Zelda: Ocarina of Time 3D (USA)".
std::string NormalizeTitle(std::string_view title);

/// Which title IDs are one game (see the top of this file).
class Families {
public:
    void Add(std::string_view title, const std::vector<u64>& title_ids);
    bool SameGame(u64 a, u64 b) const;

private:
    u64 Root(u64 id) const;
    void Join(u64 a, u64 b);
    std::map<u64, u64> parent_;
    std::map<std::string, u64> by_title_;
};

enum class Mismatch : u8 { None, Build, Version, Region, Release };

/// The parts of a catalog entry the verdict needs.
struct EntryFacts {
    std::vector<u64> title_ids;
    std::string chip;            ///< the version chip's label ("1.1", "Card 1.1"), "" none
    std::vector<u32> code_crc32; ///< the game builds it was made for
};

struct Verdict {
    bool shown = false; ///< for this game (its title ID, or another release of the same game)
    Mismatch kind = Mismatch::None;
    std::vector<Region> regions; ///< Region: the ones it was made for (known ones, in order)
    Region yours = Region::Unknown;
    std::optional<Version> made_for, your_version; ///< Version (your_version unset: a raw version
                                                   ///< the table does not know, see your_state)
    std::string your_state; ///< Version without your_version: "has an update installed", "is Rev 1"
    std::string your_build;                        ///< Build: the label that matches the game

    /// "has an update installed" when your_state is empty.
    std::string YourState() const;
    bool Fits() const {
        return kind == Mismatch::None;
    }
    /// The region chip: "USA Only", "USA and Japan", "USA, Europe and Japan" (Region only, "" else).
    std::string RegionChip() const;
    /// The tooltip: "Made for the USA version. Yours is the European version. It may not work."
    std::string Reason() const;
    /// The dialog's body: "This mod was made for the USA version of the game, and yours is the
    /// European version. It may not work, or the game may not start." `what`: "mod", "texture pack".
    std::string Body(std::string_view what) const;
};

/// The verdict on one entry. `matched_build`: the version chip of the game's entry whose
/// code_crc32 holds the game's CRC ("" when none does).
Verdict Judge(const EntryFacts& entry, const Game& game, const Families& families,
              const std::string& matched_build = {});

} // namespace Mods::Compat
