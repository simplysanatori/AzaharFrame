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
#include "core/cheats/game_version.h" // AzaharFrame versions: Version, InstalledVersion, detection

namespace Cheats::Sharkive {

struct Entry {
    std::string name;
    std::string comments; // lines without the leading '*', each ending in '\n'
    std::string code;     // "XXXXXXXX YYYYYYYY" lines joined by '\n', no trailing newline
};

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
/// AzaharFrame versions: the same against an installed title. When its raw version is known but
/// the table has no entry for it (`unmapped`), no tag matches: only the table says which version
/// a raw one is. With no raw version at all, every tag matches (nothing to compare against).
bool VersionMatches(const std::optional<Version>& tag, const InstalledVersion& installed);

/// Appends "Version note: for v1.1, installed v1.0" to the notes of each entry whose tag differs from
/// `installed`. Returns how many entries differ.
int MarkOtherVersions(std::vector<Entry>& entries, const std::optional<Version>& installed);
/// AzaharFrame versions: the same against an installed title (VersionMatches above); a version
/// the table does not know gives "Version note: for v1.0, this game has an update installed" (or
/// "is Rev 1").
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
