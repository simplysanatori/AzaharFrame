// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame moddata: the mod library behind the VR menu's Mods tab. No Qt.
//
// What Azahar loads for a game lives in <LoadDir>/mods/<TITLE ID>/ (Azahar's own loader reads, from
// there: exheader.bin; code.bin, code.ips or code.bps, in the folder or in exefs/; icon.bin,
// banner.bnr and logo.bcma.lz in the folder or in exefs/; romfs/ files that replace the game's;
// romfs_ext/ with <file>.ips|.bps patches and <file>.stub deletions). Only ONE code patch applies:
// exefs/code.ips, exefs/code.bps, code.ips, code.bps are tried in that order and the first found wins,
// so any two of them conflict.
//
// This module lists what is there, installs mods from the catalog (catalog.h) and removes them:
//   * Managed mods: downloaded (from the author's GitHub, nothing is re-hosted), unpacked, their files
//     mapped to the mods folder layout and kept in <LoadDir>/mod-library/<TITLE ID>/<mod id>/ (files/
//     and manifest.json), then copied into mods/<TITLE ID>/. Remove deletes only that mod's files.
//     A mod can sit in the library without being active (it was replaced, or deactivated).
//   * Unmanaged files (anything in mods/<TITLE ID>/ no manifest owns: a hand-made code.ips, a folder
//     of romfs files) are listed as mods named from the file or folder. They are never touched
//     except by an explicit Remove of that item.
//   * Conflicts: two mods writing the same target (same file, or two code patches, or romfs/X and
//     romfs_ext/X.ips) are never merged silently. Install returns Conflict and names the other mod;
//     Install with replace=true deactivates the other managed mod (it stays in the library). A
//     conflict with an unmanaged file is reported the same way but replace does not delete it: the
//     player removes that item first.
//
// Network (Install for a mod not yet in the library, RefreshCatalog): blocking, up to a minute per
// request, run off the GUI thread. Only api.github.com, github.com, raw.githubusercontent.com and
// GitHub's own download hosts are contacted (redirects are followed to that list only, HTTPS only).
// Every function is safe to call from any thread; one Install runs at a time (a second returns
// Error "Another install is running"); GetState can be polled from the GUI thread.

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "core/mods/archive.h"
#include "core/mods/catalog.h"

namespace Mods {

// ---------------------------------------------------------------------------------------------
// Layout (pure)

/// Where an archive file goes in <LoadDir>/mods/<TID>/ ("" = not part of the mod). See catalog.h
/// for the rule syntax; with no rules the layout is detected.
std::string MapPath(std::string_view archive_path, const std::vector<Rule>& rules, u64 title_id);

/// True for a path Azahar's loader can use (see the top of this file).
bool IsModPath(std::string_view rel);

/// Two files with the same key cannot both apply: "code-patch" for the four code patch names,
/// "exefs:<name>" for exefs sections, "romfs:<path>" for romfs/<path> and romfs_ext/<path>.ips|.bps|
/// .stub, else the path itself.
std::string ConflictKey(std::string_view rel);

struct MappedFile {
    std::string path; // relative to the mods folder
    std::string data;
};
struct MapResult {
    std::vector<MappedFile> files; // sorted by path; the later of two files with one path wins
    int skipped = 0;               // archive files no rule matched
};
MapResult MapArchive(const std::vector<ArchiveFile>& archive, const std::vector<Rule>& rules,
                     u64 title_id);

// ---------------------------------------------------------------------------------------------
// The API

enum class ModState { Active, Inactive }; // Inactive: in the library only

struct ModInfo {
    std::string id;       // catalog id, "unmanaged:<item>" or kLegacyFixId
    std::string name;
    std::string version;  // release tag, "" for a repo file
    std::string author;
    std::string licence;
    std::string source;   // "github.com/owner/repo", "" when unmanaged
    bool verified = false; // the maintainer's own mod
    bool managed = false;
    ModState state = ModState::Active;
    std::vector<std::string> files; // paths relative to the mods folder
};

struct AvailableMod {
    std::string id;
    std::string name;
    std::string title;
    std::string author;
    std::string licence;
    std::string homepage;
    std::string notes;
    bool verified = false;
    bool installable = true; // false: a 7z or rar download and no bsdtar
    bool active = false;     // installed and in the mods folder
    bool in_library = false; // downloaded, not active
};

enum class InstallResult { Ok, AlreadyInstalled, Conflict, NotAvailable, Error };

struct InstallOutcome {
    InstallResult result = InstallResult::Ok;
    std::string error;
    // Conflict: the other mod (its id works with Install(replace) / Remove) and the file
    std::string conflict_id;
    std::string conflict_name;
    std::string conflict_path;
    bool conflict_unmanaged = false; // replace cannot help: Remove it first
};

struct RemoveOutcome {
    bool ok = true;
    std::string error;
};

struct State {
    enum class Phase { Idle, Resolving, Downloading, Extracting, Installing, Done, Failed };
    Phase phase = Phase::Idle;
    std::string mod_id;
    std::string text; // "Downloading Ocarina ReFramed", "Install failed: ...", "" when idle
};

/// The id of the mods an earlier version of the Cheats tab installed (code.ips + code.ips.developer).
constexpr const char* kLegacyFixId = "legacy-fix";
constexpr const char* kUnmanagedPrefix = "unmanaged:";

struct HttpReply {
    int status = 0; // 0: no reply
    std::string body;
    std::string location; // for a redirect
    std::string error;
};
/// One GET, no redirects. `host` is "https://host". The reply body is capped at `max_bytes`.
using Transport = std::function<HttpReply(const std::string& host, const std::string& path,
                                          std::size_t max_bytes)>;

class Library {
public:
    /// `load_dir` ends with '/'. The default transport is HTTPS with the system's certificates.
    explicit Library(std::string load_dir, Catalog catalog = DefaultCatalog(),
                     Transport transport = {});

    /// Everything in mods/<TID>/: managed (active), the library's inactive ones, then unmanaged.
    std::vector<ModInfo> ListLoaded(u64 title_id) const;
    /// Catalog mods for the title, with their state.
    std::vector<AvailableMod> ListAvailable(u64 title_id) const;

    InstallOutcome Install(u64 title_id, const std::string& mod_id, bool replace = false);
    /// Takes a managed mod out of mods/<TID>/ but keeps it in the library.
    RemoveOutcome Deactivate(u64 title_id, const std::string& mod_id);
    /// Managed: its files from the mods folder and the library. Unmanaged / legacy: that item's files.
    RemoveOutcome Remove(u64 title_id, const std::string& mod_id);

    State GetState() const;

    Catalog GetCatalog() const;
    void SetCatalog(Catalog catalog);
    /// Fetches catalog/mods.json from the maintainer's repository. A catalog that does not parse
    /// leaves the current one. One request.
    bool RefreshCatalog(std::string* error = nullptr);
    /// RefreshCatalog once per run (later calls do nothing).
    void RefreshCatalogOnce();

    std::string ModsDir(u64 title_id) const;
    std::string LibraryDir(u64 title_id) const;

private:
    struct Manifest;
    bool ReadManifest(const std::string& dir, Manifest& out) const;
    bool WriteManifest(const std::string& dir, const Manifest& m) const;
    bool Fetch(const std::string& url, std::size_t max_bytes, HttpReply& out, std::string& error);
    bool DownloadSource(const CatalogEntry& entry, u64 title_id, std::vector<ArchiveFile>& files,
                        std::string& version, std::string& error);
    void SetState(State::Phase phase, const std::string& mod_id, const std::string& text);
    std::vector<ModInfo> ListLoadedLocked(u64 title_id) const;
    RemoveOutcome RemoveFilesOf(u64 title_id, const std::vector<std::string>& files);
    RemoveOutcome DeactivateLocked(u64 title_id, const std::string& mod_id);

    std::string load_dir_;
    Transport transport_;
    mutable std::mutex mutex_; // disk and catalog
    Catalog catalog_;
    bool catalog_refreshed_ = false;
    mutable std::mutex state_mutex_;
    State state_;
    bool busy_ = false;
};

/// The program's library (LoadDir of the user directory, the compiled-in catalog until refreshed).
Library& Default();

} // namespace Mods
