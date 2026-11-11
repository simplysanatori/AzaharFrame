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

/// "v1.1" from a TMD version (u16): major = bits 10-15, minor = bits 4-9.
Version FromTitleVersion(u16 title_version);

enum class VersionSource { Unknown, UpdateTmd, BaseTmd, Ncch };

struct InstalledVersion {
    std::optional<Version> version;
    VersionSource source = VersionSource::Unknown;
};

/// Which version the game runs at, first that exists: the installed update title's TMD, the
/// installed base title's TMD, the running ROM's own NCCH (ExHeader remaster version), else unknown.
/// All three are TMD-style u16 versions. A remaster version below 0x0400 is a plain cartridge
/// revision number (Rev0, Rev1), not a version 1.x or later, so it is not used.
InstalledVersion SelectInstalledVersion(std::optional<u16> update_tmd, std::optional<u16> base_tmd,
                                        std::optional<u16> ncch_remaster);

/// The game version a cheat names in its name or notes: "v1.1", "V 1.1", "Game v1.1", "Rev0 v1.1";
/// a bare "1.1" counts only when the text also says "only". "v1" (the cheat's own version), "Rev0"
/// (the cartridge revision) and numbers such as 9.999 are not versions. No tag gives nullopt.
std::optional<Version> TagVersion(std::string_view name, std::string_view comments = {});

/// A cheat with no tag matches every version; with a tag it matches an unknown installed version
/// too (nothing to compare against).
bool VersionMatches(const std::optional<Version>& tag, const std::optional<Version>& installed);

/// Appends "Sharkive: for v1.1, installed v1.0" to the notes of each entry whose tag differs from
/// `installed`. Returns how many entries differ.
int MarkOtherVersions(std::vector<Entry>& entries, const std::optional<Version>& installed);

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
