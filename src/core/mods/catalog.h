// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame moddata: the mod catalog. A JSON file in the maintainer's repository
// (simplysanatori/3ds-game-fixes, catalog/mods.json) lists the mods the Mods tab can install, with a
// copy compiled into the program for when the repository cannot be read. Nothing is re-hosted:
// every mod is downloaded from its author's own GitHub repository (or the maintainer's).
//
// {
//   "schema": 1,
//   "mods": [{
//     "id": "stella-glow-usa-no-bloom",       // [a-z0-9._-], unique, the library folder name
//     "name": "No Bloom",                      // what the list shows (the game README's H1 after the colon)
//     "title": "Stella Glow (USA)",           // the game's name, for notes
//     "title_ids": ["0004000000173700"],      // 16 hex digits each: every region the mod serves
//     "author": "simplysanatori",
//     "licence": "MIT",
//     "homepage": "https://github.com/...",
//     "verified": true,                        // honoured ONLY for source type owner_repo_path
//     "source": { ... },                       // see below
//     "install": { "rules": [ {"from": "...", "to": "..."} ] },
//     "notes": "free text: region, version, caveats"
//   }]
// }
//
// source (one of):
//   {"type": "owner_repo_path", "path": "games/x/patch/<TID>/code.ips"}       the maintainer's repo, main
//   {"type": "github_release", "repo": "owner/name", "tag": "latest" | "v1.5.9",
//    "asset_pattern": "Name-*-Mods.zip"}                                       a release asset ('*' = any text)
//   {"type": "github_repo_path", "repo": "owner/name", "ref": "main", "path": "dir/" or "file"}
//   path ending in '/' = every file under that folder (names relative to it); else one file.
//
// install.rules map an archive's (or folder's) file paths to the game's mods folder
// (<LoadDir>/mods/<TID>/). The first rule whose "from" matches wins; files no rule matches are
// skipped (READMEs, licences, other regions). "from" is a path pattern: '/'-separated parts, a part
// is literal, "*" (any one part) or "{tid}" (the 16-digit title ID being installed, any case). A
// "from" ending in '/' matches a folder: the rest of the path is appended to "to" ("" = the mods
// folder itself, e.g. "exefs/"). A "from" without the trailing '/' names one file and "to" is its
// new path. Without any rules the layout is detected: a folder .../mods/<TID>/ or
// .../luma/titles/<TID>/ (any region folder around it), else the first part that is exefs, romfs,
// romfs_ext, exheader.bin, code.bin|ips|bps. Only these end up in the mods folder: exefs/, romfs/,
// romfs_ext/, exheader.bin, code.bin|ips|bps, icon.bin, banner.bnr, logo.bcma.lz.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Mods {

struct Source {
    enum class Type { OwnerRepoPath, GithubRelease, GithubRepoPath };
    Type type = Type::OwnerRepoPath;
    std::string repo;          // "owner/name" (OwnerRepoPath: always the maintainer's repository)
    std::string tag = "latest"; // GithubRelease
    std::string asset_pattern; // GithubRelease
    std::string ref = "main";  // repo paths
    std::string path;          // repo paths
};

struct Rule {
    std::string from;
    std::string to;
};

struct CatalogEntry {
    std::string id;
    std::string name;
    std::string title;
    std::vector<u64> title_ids;
    std::string author;
    std::string licence;
    std::string homepage;
    bool verified = false;
    Source source;
    std::vector<Rule> rules;
    std::string notes;

    bool ServesTitle(u64 title_id) const;
    /// False for a source this program cannot unpack yet (a .7z or .rar asset).
    bool Installable() const;
};

struct Catalog {
    int schema = 1;
    std::vector<CatalogEntry> mods;

    const CatalogEntry* Find(std::string_view id) const;
};

constexpr const char* kOwnerRepo = "simplysanatori/3ds-game-fixes";
constexpr const char* kCatalogPath = "catalog/mods.json";

/// Parses the JSON; entries that break the rules above (bad id, title ID, repo, path with "..") are
/// dropped with a note in `warnings`. nullopt when the text is not a catalog at all.
std::optional<Catalog> ParseCatalog(std::string_view json, std::string* error = nullptr,
                                    std::vector<std::string>* warnings = nullptr);

/// The copy compiled into the program.
const char* DefaultCatalogJson();
Catalog DefaultCatalog();

/// "*" glob (any run of characters) match of a whole string, case-sensitive.
bool GlobMatch(std::string_view pattern, std::string_view text);

/// "0004000000173700" for the title ID.
std::string TitleIdText(u64 title_id);

} // namespace Mods
