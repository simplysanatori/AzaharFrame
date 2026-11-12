// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame versions: which version a game is at. Two steps, kept apart:
//
//   1. The RAW version, a TMD-style u16 (6.6.4 bits), read from the first of these that exists:
//        installed update title's TMD, installed base title's TMD, the cartridge's NCSD card info
//        (0x310, the title version; 0x312 is its revision), or a CIA's own TMD. The cartridge or
//        CIA value counts too and the HIGHER of it and the installed one wins (an update installed
//        over a dump with an older one built in, or a dump with a newer one built in over an old
//        installed update). The ExHeader "remaster" field is the last resort, when none of those
//        exists. A .3dsx or an unknown file has no raw version. The system-update partitions of
//        a cartridge (6 and 7, the CVer firmware) are never read.
//   2. The DISPLAYED version, from a table of {title, raw version -> what the game shows, source}.
//      The table is data: a built-in JSON (game_version_default.cpp), extended or overridden by
//      the "versions" section of the online mods catalog. A raw version with no entry has no
//      displayed version: "Update Installed" or "Rev N", never a number decoded from the bits
//      (Mario Kart 7's "Ver. 1.2" is raw 3072 and Fire Emblem Echoes' "1.1" raw 1024).
//      The one exception is raw 0, the title as released: 1.0.
//
// No network and no Qt here, so it can be tested.

#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Cheats::Sharkive {

/// A game version as Sharkive names it ("v1.1").
struct Version {
    int major = 0;
    int minor = 0;
    bool operator==(const Version&) const = default;
};

/// A raw version split as its 6.6.4 bits: major = bits 10-15, minor = bits 4-9 (3072 gives 3.0).
/// For logs only: it is not the version the game shows.
Version FromTitleVersion(u16 raw);
/// "3.0.0": all three parts of the raw version, for logs.
std::string DecodedTriple(u16 raw);

// ---- Reading the raw version from files ----

/// The NCSD header's card info: the title version (0x310) and the card revision (0x312).
struct CardInfo {
    u16 title_version = 0;
    u16 revision = 0;
    bool operator==(const CardInfo&) const = default;
};
/// `header` is the file's first bytes (at least 0x314). nullopt without the "NCSD" magic at 0x100.
/// Reads nothing else: not the CVer title ID and version at 0x320/0x328, not the partitions.
std::optional<CardInfo> ParseNcsdCardInfo(std::span<const u8> header);

struct TmdInfo {
    u64 title_id = 0;
    u16 version = 0;
    bool operator==(const TmdInfo&) const = default;
};
/// A TMD (signature, header, ...): its title ID and title version. nullopt when too short or the
/// signature type is unknown.
std::optional<TmdInfo> ParseTmd(std::span<const u8> tmd);

/// Where the TMD is in a CIA, from the CIA's header (the first 0x20 bytes): {offset, size}. nullopt
/// when it is not a CIA header.
struct Range {
    u64 offset = 0;
    u64 size = 0;
};
std::optional<Range> CiaTmdRange(std::span<const u8> header);

/// Titles a game's version can come from: applications (00040000), demos (00040002), DLP children
/// (00040001) and updates (0004000E). System titles (firmware, CVer) are not.
bool IsGameTitleId(u64 title_id);

/// What the file itself says: card info for a cartridge dump (.3ds, .cci), the TMD's version for a
/// CIA. Both nullopt for anything else (.3dsx, .cxi, unreadable).
struct FileVersion {
    std::optional<u16> card_version;  // NCSD 0x310
    std::optional<u16> card_revision; // NCSD 0x312
    std::optional<u16> cia_tmd;       // a CIA's own TMD version (its title must be a game's)
};
/// Recognises the file by its contents, not its name.
FileVersion ReadFileVersion(const std::string& path);
/// The same from the file's first bytes (`prefix`, at least 0x400 bytes, more for a CIA's TMD when
/// `read_at` is given: it fills `size` bytes from `offset`, false when it cannot).
FileVersion ReadFileVersionFrom(std::span<const u8> prefix,
                                const std::function<bool(u64 offset, u8* out, u64 size)>& read_at);

// ---- The table ----

/// What a game shows for one raw version of one title.
struct TableEntry {
    u32 title_low = 0; // the low half of the title ID (a game's and its update's are the same)
    u16 raw = 0;
    Version shown;
    std::string source; // where the entry comes from: patch notes, a title screen, ...
};

class VersionTable {
public:
    /// {"versions": [{"title_ids": ["00030800", ...], "raw": 3072, "shown": "1.2", "source": "..."}]}
    /// (a catalog file with other keys is fine). Title IDs are 8 or 16 hex digits. Bad entries are
    /// dropped (with a note in `warnings`). nullopt when the text is not JSON or has no "versions".
    static std::optional<VersionTable> Parse(std::string_view json, std::string* error = nullptr,
                                             std::vector<std::string>* warnings = nullptr);

    const TableEntry* Find(u64 title_id, u16 raw) const;
    /// Entries of `other` replace entries with the same title and raw version; the rest are added.
    void Merge(const VersionTable& other);
    const std::vector<TableEntry>& Entries() const {
        return entries_;
    }

private:
    std::vector<TableEntry> entries_;
};

/// The table compiled in.
const VersionTable& BuiltInVersionTable();
/// The "versions" section of the online catalog (replaces the one before it); the built-in table
/// stays under it. Returns false, changing nothing, when `catalog_json` has no readable section.
bool SetOnlineVersions(std::string_view catalog_json);
/// Drops the online section (tests).
void ClearOnlineVersions();
/// The entry for this title and raw version: the online table first, then the built-in one.
std::optional<TableEntry> LookupVersion(u64 title_id, u16 raw);

/// The version the game shows for a raw version: the table, or 1.0 for raw 0 (the game as
/// released). nullopt: not known. Never decoded from the bits.
std::optional<Version> GameVersionOf(u64 title_id, u16 raw);

// ---- Putting it together ----

enum class VersionSource {
    Unknown,
    UpdateTmd, // an installed update title
    BaseTmd,   // an installed title
    CardInfo,  // the cartridge dump's NCSD card info
    CiaTmd,    // the CIA's own TMD
    Ncch,      // last resort: the ExHeader remaster version (gives a revision only)
};

struct InstalledVersion {
    std::optional<Version> version; // what the game shows (the table), nullopt when not known
    std::optional<int> revision;    // the cartridge revision (NCSD 0x312; else remaster / 2)
    VersionSource source = VersionSource::Unknown; // where `raw` is from
    std::optional<u16> raw;         // the raw version (TMD style), nullopt when none was found
    std::string table_source;       // the table entry's source note ("as released" for raw 0)
    /// The raw version is known, the table has no entry for it, and it is not a plain 0 (raw 0 with
    /// a cartridge revision above 0 counts as not plain): the game has an
    /// update (or a later revision) built in or installed, and what it shows is not known.
    bool unmapped = false;

    /// What to show: "1.2" (the table), else "Rev 1" (a cartridge) or "Update Installed" (a
    /// TMD above the release), else "" when nothing is known (or nothing but raw 0).
    std::string Display() const;
    /// The same as a state for a sentence: "has an update installed", "is Rev 1", "" none.
    std::string State() const;
    /// For the log: "raw 3072 (3.0.0) from the update TMD, shows 1.2 (source), rev 1".
    std::string LogText() const;
};

/// Which version the game is at, first that exists (see the top of this file). `file` is what the
/// game's file says (ReadFileVersion; {} for none), `remaster` its ExHeader remaster version.
InstalledVersion SelectInstalledVersion(u64 title_id, std::optional<u16> update_tmd,
                                        std::optional<u16> base_tmd, const FileVersion& file,
                                        std::optional<u16> ncch_remaster);

} // namespace Cheats::Sharkive
