// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cheats: reading and merging a Sharkive cheat file. No network and no Qt here, so it can
// be tested. Sharkive (FlagBrew/Sharkive, GPL-3.0) keeps one file per title in Azahar's own gateway
// format.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Cheats::Sharkive {

struct Entry {
    std::string name;
    std::string comments; // lines without the leading '*', each ending in '\n'
    std::string code;     // "XXXXXXXX YYYYYYYY" lines joined by '\n', no trailing newline
};

/// A game version as Sharkive names it ("v1.1") and as the 3DS stores it (TMD version, 6.6.4 bits).
struct Version {
    int major = 0;
    int minor = 0;
    bool operator==(const Version&) const = default;
};

/// A TMD title version (u16) split as its 6.6.4 bits: major = bits 10-15, minor = bits 4-9 (3072
/// gives 3.0). For logs only: it is not the version the game shows (AzaharFrame echoes: Mario Kart
/// 7's 1.2 update is TMD 3072, Fire Emblem Echoes' 1.1 update TMD 1024). See GameVersionOf.
Version FromTitleVersion(u16 title_version);

/// AzaharFrame echoes: the version the game itself shows ("Ver. 1.2") for TMD title version `tmd`
/// of its installed update (or of its own installed title). The one function that names a game's
/// version: the mods' and cheats' version chips, Not for Your Version and their matching all come
/// from it. A TMD below 1024 (bits 10-15 zero) is the game as released, 1.0. A later TMD counts
/// update builds, not the version the game shows (Mario Kart 7: TMD 3072 is "Ver. 1.2"), so only
/// the releases listed in sharkive.cpp have one; any other gives nullopt (unknown).
std::optional<Version> GameVersionOf(u64 title_id, u16 tmd);

enum class VersionSource { Unknown, UpdateTmd, BaseTmd, Ncch };

struct InstalledVersion {
    std::optional<Version> version;  // from a TMD: GameVersionOf, nullopt when not known
    std::optional<int> revision;     // from the ROM: the cartridge revision (Rev0, Rev1)
    VersionSource source = VersionSource::Unknown;
    std::optional<u16> tmd;          // AzaharFrame echoes: the raw TMD title version, for logs
    int updates = 0;                 // AzaharFrame echoes: TMD bits 10-15; above 0, an update's code
};

/// Which version the game runs at, first that exists: the installed update title's TMD, the
/// installed base title's TMD (both TMD-style u16 versions, giving `version`), the running ROM's
/// ExHeader remaster version (giving `revision`), else unknown.
/// A cartridge dump has no TMD, and its remaster field is a revision counter, not a version:
/// Mario Kart 7 Rev 1 reads 0x0002, so the revision is remaster / 2. Zero is ambiguous (Rev 0
/// carts read 0x0000, and so does Super Mario 3D Land Rev 1), so 0 is unknown, and so is an odd
/// value. Only a non-zero even value gives a revision.
InstalledVersion SelectInstalledVersion(u64 title_id, std::optional<u16> update_tmd,
                                        std::optional<u16> base_tmd, std::optional<u16> ncch_remaster);

/// The cartridge revision a cheat names ("Rev1", "rev 1", "Rev02"), nullopt without one.
std::optional<int> TagRevision(std::string_view name, std::string_view comments = {});

/// Appends "Version note: for Rev0, this cartridge is Rev1" to the notes of each entry tagged with
/// another revision. Entries with no Rev tag are left alone. Returns how many differ.
int MarkOtherRevisions(std::vector<Entry>& entries, int installed_revision);

/// The game version a cheat names in its name or notes: "v1.1", "V 1.1", "Game v1.1", "Rev0 v1.1";
/// a bare "1.1" counts only when the text also says "only". "v1" (the cheat's own version), "Rev0"
/// (the cartridge revision) and numbers such as 9.999 are not versions. No tag gives nullopt.
std::optional<Version> TagVersion(std::string_view name, std::string_view comments = {});

/// A cheat with no tag matches every version; with a tag it matches an unknown installed version
/// too (nothing to compare against).
bool VersionMatches(const std::optional<Version>& tag, const std::optional<Version>& installed);
/// AzaharFrame echoes: the same against an installed title. When its version is not known but its
/// TMD counts updates, a tag of 1.0 (the game as released) does not match; any other tag does
/// (nothing to compare against).
bool VersionMatches(const std::optional<Version>& tag, const InstalledVersion& installed);

/// Appends "Version note: for v1.1, installed v1.0" to the notes of each entry whose tag differs from
/// `installed`. Returns how many entries differ.
int MarkOtherVersions(std::vector<Entry>& entries, const std::optional<Version>& installed);
/// AzaharFrame echoes: the same against an installed title (VersionMatches above); an update of
/// unknown version gives "Version note: for v1.0, this game has an update".
int MarkOtherVersions(std::vector<Entry>& entries, const InstalledVersion& installed);

std::string ToString(const Version& v);

/// Path of the title's file on raw.githubusercontent.com ("/FlagBrew/Sharkive/master/3ds/<ID>.txt").
std::string UrlPath(u64 title_id);

/// Reads a Sharkive file. Drops the "---" line, "*citra_enabled" (downloaded cheats arrive disabled),
/// blocks with no code, blocks with a line that is not "8 hex digits, space, 8 hex digits", and later
/// blocks whose name was already seen.
std::vector<Entry> Parse(std::string_view text);

/// The entries of `downloaded` whose name is not in `existing_names` (names compared after trimming).
std::vector<Entry> NewEntries(const std::vector<Entry>& downloaded,
                              const std::vector<std::string>& existing_names);

} // namespace Cheats::Sharkive
