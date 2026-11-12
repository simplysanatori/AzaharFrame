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
// request, run off the GUI thread. Only api.github.com, github.com, raw.githubusercontent.com,
// GitHub's own download hosts and (AzaharFrame modcat, for the catalog's GameBanana mods)
// gamebanana.com with its file hosts are contacted (redirects are followed to that list only, HTTPS only).
// Every function is safe to call from any thread; one Install runs at a time (a second returns
// Error "Another install is running"); GetState can be polled from the GUI thread.

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "core/mods/archive.h"
#include "core/mods/catalog.h"
#include "core/mods/mega.h" // AzaharFrame mega
#include "core/mods/multipart.h" // AzaharFrame catalog3
#include "core/mods/compat.h" // AzaharFrame compat

namespace Mods {

// ---------------------------------------------------------------------------------------------
// Layout (pure)

/// Where an archive file goes in <LoadDir>/mods/<TID>/ ("" = not part of the mod). See catalog.h
/// for the rule syntax; with no rules the layout is detected.
std::string MapPath(std::string_view archive_path, const std::vector<Rule>& rules, u64 title_id);

/// AzaharFrame modfix: a few words for the menu from an install error ("No Connection", "Timed Out",
/// "Rate Limited", "Not Found", "Bad Archive", "Disk Error"), so "Install Failed" says why. The
/// full error goes to the log.
std::string ShortFailure(std::string_view error);

/// True for a path Azahar's loader can use (see the top of this file).
bool IsModPath(std::string_view rel);

/// Two files with the same key cannot both apply: "code-patch" for the four code patch names,
/// "exefs:<name>" for exefs sections, "romfs:<path>" for romfs/<path> and romfs_ext/<path>.ips|.bps|
/// .stub, else the path itself.
std::string ConflictKey(std::string_view rel);

/// AzaharFrame catalog3: where an archive file goes in <LoadDir>/textures/<TID>/<mod id>/ ("" = not
/// a texture of the mod). `rules` are the entry's "textures" rules (no rules, no textures).
std::string MapTexturePath(std::string_view archive_path, const std::vector<Rule>& rules, u64 title_id);

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
    /// AzaharFrame catalog3: the textures part, relative to load/textures/<TID>/<id>/
    std::vector<std::string> texture_files;
    /// AzaharFrame catalog3: "activate" (downloaded, goes in when the running game stops), "deactivate"
    /// or "remove" (the same), "" none. `state` already shows the outcome.
    std::string pending;
    /// AzaharFrame catalog3: installed from a GameBanana file whose scan was "unknown" (the player allowed it)
    bool unscanned = false;
};

struct AvailableMod {
    std::string id;
    std::string name;
    std::string title;
    std::string author;
    std::string licence;
    std::string homepage;
    std::string notes;
    std::string author_url; // AzaharFrame modcat: what the author's name opens ("" none)
    std::string updated;    // AzaharFrame modcat: "YYYY-MM-DD", the source's own date, "" unknown;
                            // catalog3: the catalog's own value may be "YYYY-MM"
    u64 downloads = 0;      // AzaharFrame catalog3: lifetime downloads (CatalogEntry::downloads), 0 unknown
    u64 size = 0;           // AzaharFrame catalog3: bytes to download (CatalogEntry::size), 0 unknown
    /// AzaharFrame catalog3: a file of this mod (any part) has no GameBanana scan result yet. Known after
    /// an install was refused for it (Install without allow_unscanned) or went ahead with it, or from the
    /// installed copy. The menu asks before calling Install(..., allow_unscanned = true).
    bool unscanned = false;
    bool pending = false;   // AzaharFrame catalog3: downloaded for the running game; `active` is true, the files go in when it stops
    bool has_textures = false; // AzaharFrame catalog3: the mod also carries textures
    bool multipart = false;    // AzaharFrame catalog3: several downloads, joined
    std::string chip;       // AzaharFrame modcat: the version chip ("1.0"), "" none
    std::string chip_tip;   // its tooltip
    bool matches_game = false; // made for the game's own code
    bool verified = false;
    bool installable = true; // false: a 7z or rar download and no bsdtar
    bool active = false;     // installed and in the mods folder
    bool in_library = false; // downloaded, not active
    Compat::Verdict compat;  // AzaharFrame compat: for another region or version of the game
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
    /// AzaharFrame catalog3: the install put textures in load/textures/<TID>/<id>/ (the menu turns
    /// Custom Textures on for the game when it is off and says so)
    bool textures_installed = false;
    /// AzaharFrame catalog3: the target is the running game, so the mod is downloaded and waits in the
    /// library: "<Mod> Downloaded. Restart to Enable."
    bool staged = false;
    bool unscanned = false; // AzaharFrame catalog3: an unscanned GameBanana file was refused, or used (allowed)
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
    u64 done = 0;     // AzaharFrame mega: bytes fetched so far while Downloading (0 when unknown)
    u64 total = 0;    // and the bytes to fetch (0 when unknown)
};

/// The id of the mods an earlier version of the Cheats tab installed (code.ips + code.ips.developer).
constexpr const char* kLegacyFixId = "legacy-fix";
constexpr const char* kUnmanagedPrefix = "unmanaged:";
/// AzaharFrame zipinstall: mods from Install from Zip have ids "zip-<name>" and this source.
constexpr const char* kZipPrefix = "zip-";
constexpr const char* kZipSource = "zip";

/// AzaharFrame zipinstall: one file of a mod from a zip: where it is on disk, where it goes in the
/// mods folder (MapPath's answer).
struct LocalFile {
    std::string source;
    std::string target;
};

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
    /// Catalog mods for the title, with their state. AzaharFrame modcat: `game_code_crc` is the
    /// CRC32 of the game's .code as Azahar reads it (Crc32 of the loader's ReadCode); the mods
    /// made for it are marked and come first.
    /// AzaharFrame compat: with `game`, the mods made for another region, release or version of the
    /// same game (compat.h) are listed too, after the others, each with its verdict; game's
    /// code_crc is used and `game_code_crc` ignored.
    std::vector<AvailableMod> ListAvailable(u64 title_id,
                                            std::optional<u32> game_code_crc = std::nullopt,
                                            const Compat::Game* game = nullptr) const;

    /// AzaharFrame catalog3: a GameBanana file with scan result "unknown" is used only with
    /// `allow_unscanned` (a clean file that matches is always preferred); without it the install fails
    /// with "GameBanana Hasn't Scanned This File Yet" and `outcome.unscanned` is true. Every other
    /// non-clean result is refused either way.
    /// AzaharFrame compat: `other_version` installs a mod made for another region or release of
    /// the game (Install Anyway) into this title's folder: rules with {tid} are read for this
    /// title, and when nothing in the download fits it, for the mod's own title IDs in turn. A mod
    /// already in this title's library installs with or without it.
    InstallOutcome Install(u64 title_id, const std::string& mod_id, bool replace = false,
                           bool allow_unscanned = false, bool other_version = false);
    /// AzaharFrame zipinstall: a mod from a local archive, kept in the library like a downloaded one
    /// (source kZipSource, no catalog entry), then put in the mods folder as Install does. `id` starts
    /// with kZipPrefix. A mod with that id already in the library is a Conflict naming itself
    /// (conflict_id == id) unless `replace_same`; a clash with another mod is the usual Conflict, and
    /// Install(id, true) afterwards replaces it from the library copy.
    InstallOutcome InstallLocal(u64 title_id, const std::string& id, const std::string& name,
                                const std::vector<LocalFile>& files, bool replace_same = false);
    /// Takes a managed mod out of mods/<TID>/ but keeps it in the library.
    RemoveOutcome Deactivate(u64 title_id, const std::string& mod_id);
    /// Managed: its files from the mods folder and the library. Unmanaged / legacy: that item's files.
    RemoveOutcome Remove(u64 title_id, const std::string& mod_id);

    State GetState() const;
    /// AzaharFrame catalog3: the title that is running (0 none). Its mods/<TID>/ and load/textures/<TID>/
    /// are never touched: installs there are staged in the library, removals wait. Set 0 when it stops.
    void SetRunningTitle(u64 title_id);
    u64 RunningTitle() const { return running_title_.load(); }
    /// Applies what waited (staged installs, deactivations, removals) for every title but the running
    /// one. Call when a game has stopped and before the next one loads. Returns how many were applied.
    int ApplyPending();
    /// AzaharFrame mega: stops the running download soon (the install fails with "Cancelled"; nothing
    /// is installed). Does nothing when idle.
    void Cancel() { cancel_ = true; }
    /// AzaharFrame catalog3: where multipart downloads are written (tests replace it).
    void SetFileTransport(Multipart::FileTransport transport) { file_transport_ = std::move(transport); }
    /// AzaharFrame catalog3: free bytes of the file system holding a folder (tests replace it).
    using FreeSpaceFn = std::function<bool(const std::string& dir, u64* out)>;
    void SetFreeSpace(FreeSpaceFn fn) { free_space_ = std::move(fn); }
    /// AzaharFrame mega: the network MEGA downloads go through (tests replace it).
    void SetMegaNet(Mega::Net net) { mega_net_ = std::move(net); }

    Catalog GetCatalog() const;
    void SetCatalog(Catalog catalog);
    /// Fetches catalog/mods.json from the maintainer's repository. A catalog that does not parse
    /// leaves the current one. One request.
    bool RefreshCatalog(std::string* error = nullptr);
    /// RefreshCatalog once per run (later calls do nothing).
    void RefreshCatalogOnce();
    /// AzaharFrame modcat: looks up the source's own date (a release's published_at, a repository
    /// file's last commit, a GameBanana mod's update) for the catalog mods of the title that have
    /// none yet this run, so ListAvailable can say "updated". Blocking, a request per source;
    /// failures are ignored (the catalog's "updated" stays).
    void RefreshDates(u64 title_id);

    std::string ModsDir(u64 title_id) const;
    std::string LibraryDir(u64 title_id) const;

private:
    struct Manifest;
    InstallOutcome InstallImpl(u64 title_id, const std::string& mod_id, bool replace,
                               bool other_version); // AzaharFrame compat: other_version
    // AzaharFrame catalog3: unscanned GameBanana files (install thread; the set under mutex_)
    bool allow_unscanned_ = false;
    bool hit_unscanned_ = false; // this install met or used an unscanned file
    std::set<std::string> unscanned_ids_;
    bool ReadManifest(const std::string& dir, Manifest& out) const;
    bool WriteManifest(const std::string& dir, const Manifest& m) const;
    bool Fetch(const std::string& url, std::size_t max_bytes, HttpReply& out, std::string& error);
    bool DownloadMega(const CatalogEntry& entry, u64 title_id, std::vector<ArchiveFile>& files,
                      std::string& error); // AzaharFrame mega
    /// AzaharFrame catalog3: a multipart entry: the parts to disk, joined, unpacked; `root` is the
    /// folder holding the files. The work folder is the caller's to remove (WorkDir).
    bool DownloadParts(const CatalogEntry& entry, u64 title_id, std::string& root, std::string& version,
                       std::string& error);
    bool FetchToFile(const std::string& url, const std::string& dest, const Multipart::Progress& progress,
                     std::string& error, const std::vector<std::string>& extra_hosts = {});
    bool DownloadBig(const CatalogEntry& entry, u64 title_id, std::string& version, std::string& error);
    std::string WorkDir(const std::string& mod_id) const;
    bool DownloadSource(const CatalogEntry& entry, u64 title_id, std::vector<ArchiveFile>& files,
                        std::string& version, std::string& error);
    std::string LookupDate(const CatalogEntry& entry); // AzaharFrame modcat
    void SetProgress(u64 done, u64 total); // AzaharFrame mega
    void SetState(State::Phase phase, const std::string& mod_id, const std::string& text);
    std::vector<ModInfo> ListLoadedLocked(u64 title_id) const;
    std::string TexturesDir(u64 title_id, const std::string& mod_id) const; // AzaharFrame catalog3
    RemoveOutcome RemoveTextureFiles(u64 title_id, const std::string& mod_id,
                                     const std::vector<std::string>& files);
    RemoveOutcome RemoveFilesOf(u64 title_id, const std::vector<std::string>& files);
    RemoveOutcome DeactivateLocked(u64 title_id, const std::string& mod_id);
    /// The conflict check and the copy into the mods folder (mutex_ held). AzaharFrame zipinstall
    InstallOutcome ActivateLocked(u64 title_id, const std::string& mod_id, Manifest& manifest,
                                  bool verified, bool replace);
    /// Install for a mod only in the library (from a zip). AzaharFrame zipinstall
    InstallOutcome InstallFromLibrary(u64 title_id, const std::string& mod_id, bool replace);

    std::string load_dir_;
    Transport transport_;
    mutable std::mutex mutex_; // disk and catalog
    Catalog catalog_;
    bool catalog_refreshed_ = false;
    std::map<std::string, std::string> dates_; // catalog id -> "YYYY-MM-DD" (AzaharFrame modcat)
    mutable std::mutex state_mutex_;
    State state_;
    bool busy_ = false;
    std::atomic_bool cancel_{false}; // AzaharFrame mega
    Mega::Net mega_net_ = Mega::DefaultNet();
    Multipart::FileTransport file_transport_ = Multipart::StreamGet; // AzaharFrame catalog3
    FreeSpaceFn free_space_ = Multipart::FreeBytes;
    std::atomic<u64> running_title_{0}; // AzaharFrame catalog3
    std::string big_root_; // AzaharFrame catalog3: where DownloadBig unpacked (install thread only)
};

/// The program's library (LoadDir of the user directory, the compiled-in catalog until refreshed).
Library& Default();

} // namespace Mods
