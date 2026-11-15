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
//     "verified": true,                        // honoured ONLY for source types owner_repo_path and
//                                              // gamebanana; AzaharFrame fixd: the menu's Verified
//                                              // chip, listed first
//     "recommended": true,                     // AzaharFrame fixes7: listed first (after verified),
//                                              // any source; default false. We never set it.
//     "aspect": "16:9",                        // AzaharFrame fixes7: the picture shape it makes
//                                              // ("16:9", "21:9", "16:10", "4:3", "5:3"), optional
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
//   {"type": "gamebanana", "mod": 695893, "file_pattern": "*.zip"}            AzaharFrame modcat: a
//   GameBanana mod; its newest clean file matching the pattern is found through GameBanana's public
//   API at install time (nothing re-hosted, nothing pinned to one file id).
//
//   {"type": "mega", "link": "https://mega.nz/file/<handle>#<key>", "file_pattern": "*.zip"}
//   AzaharFrame mega: a public MEGA link (a folder link picks the file matching file_pattern). The
//   link's key is part of the link, which the author published. A zip is read in place, only the
//   entries the install rules keep are downloaded.
//
// AzaharFrame catalog3, optional entry fields:
//   "textures": {"rules": [{"from": "...", "to": "..."}]}   the mod also carries textures. Same rule
//     syntax; "to" is relative to the game's texture folder, load/textures/<TID>/, and the part installs
//     under the entry's own subfolder: load/textures/<TID>/<entry id>/<to...> (so it can be removed on
//     its own; removing the mod removes both parts). Custom Textures is turned on
//     for the game by the install.
//   "parts": [source, source, ...]   in place of "source" (when both are there, parts wins): several downloads,
//     in order, joined and unpacked as one archive (multi-volume RAR, split 7z, split zip). Each part
//     is any source type, usually gamebanana with a file_pattern.
//   "size": 1234567890     bytes to download (all parts); a mod over 1 GB asks before it downloads
//   "downloads": 12345      the source's lifetime download count (catalog-refresh.py keeps it)
//   "updated": "2026-11"    "YYYY-MM" is accepted as well as "YYYY-MM-DD"
//
// Optional entry fields (AzaharFrame modcat):
//   "author_url": "https://gamebanana.com/members/1544353"   what the author's name opens
//   "updated": "2026-11-12" or "2026-11"                     shown until the source's own date is known
//   "version": "v172"                                         AzaharFrame ui3: the mod's own version,
//     shown after the author until the source's own is known (a GitHub release's tag, GameBanana's
//     version); at most 24 characters, else dropped. Older builds ignore it (unknown keys are).
//   "downloads": 12345                                        AzaharFrame compat: lifetime downloads
//     (the order of Install Mods, and "10K+ Downloads" on the row)
//   "version_chip": {"label": "1.0", "tip": "For the original 1.0 game.", "code_crc32": ["c75021d9"]}
//     or {"0004000000033500": ["9d9683e5"]} for CRC32s read from one title ID's build only: the
//     other title IDs of the entry have none, and an unknown CRC is never a reason for anything.
//     A mod made for one build of a game, shown as a chip on its row ("label", with "tip" as its
//     tooltip). "code_crc32": the CRC32 of the game's .code this mod's code patch was made for (a
//     BPS patch's own source checksum); the row whose list holds the running game's CRC32 is
//     marked "Matches Your Game" and listed first. Nothing is hidden: the others stay.
//
// AzaharFrame ctgp7, optional entry fields (builds before 1.2.0 ignore both, and the entry keeps
// "install": {"rules": []}, so an older build installs nothing of it into the mods folder):
//   "requires": "ctgp7"     the mod only works inside CTGP-7 (the menu offers to install CTGP-7 first;
//     any other value: a newer build's pack, never installed here)
//   "ctgp7_install": {"rules": [{"from": "*.chpack", "to": "MyStuff/Characters/"},
//                               {"from": "Menu3D.szs", "to": "gamefs/"}]}
//     the second install target: CTGP-7's folder on the emulated SD card (sdmc/CTGP-7/). "from" is a
//     '*' glob (GlobMatch) of a file's name, or of its whole path when it has a '/'; "to" is relative
//     to sdmc/CTGP-7/ and starts with MyStuff/ (character packs: MyStuff/Characters/, where CTGP-7's
//     installer puts its own .chpack files) or gamefs/ (CTGP-7's game files, e.g. a Menu3D.szs, as
//     its author says); "to" ending in '/' keeps the file's name. The first rule that matches wins.
//     Removing or turning the mod off takes out only the files it put there.
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
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Mods {

struct Source {
    enum class Type { OwnerRepoPath, GithubRelease, GithubRepoPath, Gamebanana, Mega, Https, Gdrive, GdriveFolder };
    Type type = Type::OwnerRepoPath;
    std::string repo;          // "owner/name" (OwnerRepoPath: always the maintainer's repository)
    std::string tag = "latest"; // GithubRelease
    std::string asset_pattern; // GithubRelease, Gamebanana, Mega (file name pattern)
    std::string mega_link;     // Mega: the public file or folder link, key included (AzaharFrame mega)
    u64 gb_mod = 0;            // Gamebanana: the mod's id (gamebanana.com/mods/<id>)
    std::string ref = "main";  // repo paths
    std::string path;          // repo paths
    // AzaharFrame catalog3 round 2: sources shared with the texture list
    std::string url;                // Https: the direct link
    std::string file_id;            // Gdrive: a public file's id
    std::string folder_id;          // GdriveFolder: a public folder (file_pattern picks the file)
    std::vector<std::string> hosts; // Https: extra exact host names a redirect may go to
    std::string name;               // optional file name for a part that is a loose file (not an archive)
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
    std::string author_url; // AzaharFrame modcat
    std::string updated;    // AzaharFrame modcat: "YYYY-MM-DD" or ""; catalog3: also "YYYY-MM"
    std::string version;    // AzaharFrame ui3: the mod's own version ("v172"), "" none
    u64 size = 0;           // AzaharFrame catalog3: bytes to download (all parts), 0 unknown; over 1 GB the menu asks first
    u64 downloads = 0;      // AzaharFrame catalog3: the source's lifetime download count, 0 unknown
    bool verified = false;     // the maintainer's fix (the legacy fix); AzaharFrame fixd: the Verified chip, listed first
    bool recommended = false;  // AzaharFrame fixes7: listed first (after verified); no badge
    std::string aspect;        // AzaharFrame fixes7: "16:9" (NormalizeAspect), "" not given
    Source source;
    std::vector<Rule> rules;
    std::vector<Rule> texture_rules; // AzaharFrame catalog3: "textures": {"rules": [...]}, empty = none
    std::vector<Source> parts;       // AzaharFrame catalog3: "parts": downloaded in order, joined, unpacked as one
    std::string chip;     // AzaharFrame modcat: "1.0", "Card 1.1", "Fallback" ("" none)
    std::string chip_tip; // its tooltip
    std::vector<u32> code_crc32; // the game code CRC32s it was made for (any title ID of the entry)
    std::map<u64, std::vector<u32>> code_crc32_by_title; // or per title ID ({"<tid>": [...]})
    std::string notes;
    /// AzaharFrame ctgp7: "requires" ("ctgp7"; "" none) and the CTGP-7 folder's rules
    std::string requires_pack;
    std::vector<Rule> ctgp7_rules;

    bool ServesTitle(u64 title_id) const;
    /// The code CRC32s for this title ID: its own list in the per-title form (none for a title ID
    /// the form does not name), else the entry's list.
    std::vector<u32> CodeCrcsFor(u64 title_id) const;
    /// False for a .7z or .rar asset when the system has no bsdtar.
    bool Installable() const;
};

struct Catalog {
    int schema = 1;
    std::vector<CatalogEntry> mods;

    const CatalogEntry* Find(std::string_view id) const;
};

/// AzaharFrame ctgp7: the pack a "requires" names that this build knows.
constexpr const char* kRequiresCtgp7 = "ctgp7";

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

/// AzaharFrame modcat: CRC32 (the one BPS patches use) of a buffer.
u32 Crc32(const u8* data, std::size_t size);

/// "0004000000173700" for the title ID.
std::string TitleIdText(u64 title_id);

} // namespace Mods
