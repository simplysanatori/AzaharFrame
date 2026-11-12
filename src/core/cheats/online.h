// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cheats: downloading a game's cheats and fixes from two sources and merging them into the
// player's cheat file. No Qt here. Used by the Qt Cheats dialog and by the VR menu's Cheats tab.
//
//   Developer: github.com/simplysanatori/3ds-game-fixes (MIT), games/<slug>/cheats/<TITLEID>.txt and
//              games/<slug>/patch/<TITLEID>/code.ips. Found with ONE request to api.github.com (the
//              repository's file tree), then fetched from raw.githubusercontent.com.
//   Sharkive:  github.com/FlagBrew/Sharkive (GPL-3.0), raw.githubusercontent.com only.
//
// The only network traffic is what DownloadCheats, InstallFix and FixAvailable's refresh make, and
// those run only when the player asks (Download button, Install). All of it is blocking (up to about
// 30 s): call it off the GUI thread. The functions that take a CheatEngine are safe to call from any
// thread (the engine locks its own list); do not run two DownloadCheats for one title at once.
//
// Each downloaded cheat is saved with a first comment line "*source: Developer" or "*source: Sharkive";
// a cheat with no such line is the user's own (Source::User). Cheats arrive disabled.

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"
#include "core/cheats/sharkive.h"

namespace Cheats {
class CheatEngine;
}

namespace Cheats::Online {

enum class Source { User, Developer, Sharkive };

std::string_view SourceName(Source source); // "Developer", "Sharkive", "Your Cheats"

/// The source named by a "source: X" line in a cheat's notes (User when there is none).
Source SourceOf(std::string_view comments);
/// `comments` with the "source: X\n" line put first (replacing an earlier one).
std::string WithSource(std::string_view comments, Source source);
/// The text after "Version note: " in a cheat's notes ("for Rev0, this cartridge is Rev1"), or "".
std::string VersionNote(std::string_view comments);

// ---------------------------------------------------------------------------------------------
// Pure parts (tested without a network)

struct TreeFiles {
    std::string cheats; // repo path of the title's cheat file, "" when there is none
    std::string patch;  // repo path of the title's code.ips, "" when there is none
};

/// Reads the GitHub "git/trees" reply (recursive) and finds games/<slug>/cheats/<TITLEID>.txt and
/// games/<slug>/patch/<TITLEID>/code.ips for the title. nullopt when the reply is not a tree
/// (`error` then says why).
std::optional<TreeFiles> ParseTree(std::string_view json, u64 title_id, std::string* error = nullptr);

struct Fetched {
    Source source;
    std::vector<Sharkive::Entry> entries;
};

struct MergeResult {
    std::vector<Sharkive::Entry> added; // tagged with their source, version notes added
    int developer = 0;
    int sharkive = 0;
    int mismatched = 0; // of `added`, for another version (or revision)
    bool by_revision = false;
};

/// Developer entries first, then Sharkive. Drops an entry whose name is in `existing_names` or was
/// already taken from an earlier source; keeps everything else of the user's.
MergeResult Merge(const std::vector<Fetched>& fetched, const std::vector<std::string>& existing_names,
                  const Sharkive::InstalledVersion& installed);

// ---------------------------------------------------------------------------------------------
// Network

struct HttpResult {
    int status = 0;    // 0: no reply
    std::string body;
    std::string error; // set when status is 0 or the reply was refused (rate limit, too large)
};
using HttpGet = std::function<HttpResult(const std::string& host, const std::string& path)>;

/// HTTPS GET with certificate checks against the system's store (static LibreSSL, httplib).
HttpResult HttpsGet(const std::string& host, const std::string& path);

struct RawReply {
    int status = 0;       // 0: no reply
    std::string body;     // capped at max_body
    std::string location; // Location header of a redirect
    std::string error;    // set when status is 0, the body was too large or the status is not 2xx/3xx/404
};

/// One HTTPS GET with no redirect followed (the caller checks where it leads). For the mod library.
RawReply HttpsGetRaw(const std::string& host, const std::string& path, std::size_t max_body);

struct DownloadResult {
    int developer_added = 0;
    int sharkive_added = 0;
    int mismatched = 0;        // of the added ones: for another version / revision
    bool by_revision = false;  // the ROM gave a cartridge revision, not a version
    bool found_any = false;    // at least one source had a file for the title
    bool fix_available = false;
    std::string update_version; // "v1.1" when an update title is installed
    std::string error;         // non-empty: nothing could be fetched (network, rate limit)
    std::string warning;       // one source failed, the other worked
    bool stale = false;        // AzaharFrame integ: another game runs now; nothing was added
};

/// One-line status: "12 cheats added, 3 for another version", "No cheats for this game",
/// "Download failed: ..." and so on.
std::string StatusText(const DownloadResult& result);

/// AzaharFrame integ: a download in two steps, so a download that ends after another game started
/// never lands in that game's list. FetchCheats (network, any thread) is tagged with the title it
/// was made for; ApplyFetch (the thread that starts games, i.e. the GUI thread) merges it only
/// while that title still runs.
struct Fetch {
    u64 title_id = 0;             // the title the download was made for
    std::vector<Fetched> fetched; // Developer first, then Sharkive
    Sharkive::InstalledVersion installed;
    DownloadResult result; // error, warning, found_any, fix_available so far
};

/// Fetches both sources for the title (Developer first). Touches no CheatEngine.
/// `game_path` is the game's file, for its cartridge revision when no TMD is installed.
Fetch FetchCheats(u64 title_id, const std::string& game_path);
/// The same with a fetch function of your own (tests).
Fetch FetchCheatsWith(const HttpGet& get, u64 title_id, const std::string& game_path);

/// Merges a fetch by name into the engine's list (user cheats and enabled states untouched, new
/// ones disabled) and saves the cheat file. Nothing is touched, and the result says `stale`, when
/// `running_title_id` (the game running now) or the title the engine holds is not the fetch's.
DownloadResult ApplyFetch(CheatEngine& engine, const Fetch& fetch, u64 running_title_id);

/// FetchCheats then ApplyFetch for the same title, on the calling thread.
DownloadResult DownloadCheats(CheatEngine& engine, u64 title_id, const std::string& game_path);
/// The same with a fetch function of your own (tests).
DownloadResult DownloadCheatsWith(const HttpGet& get, CheatEngine& engine, u64 title_id,
                                  const std::string& game_path);

/// The version or revision the game runs at (update TMD, base TMD, cartridge NCCH, unknown).
Sharkive::InstalledVersion QueryInstalledVersion(u64 title_id, const std::string& game_path);

// ---------------------------------------------------------------------------------------------
// The list for a UI (VR tab)

struct CheatInfo {
    std::string name;
    Source source = Source::User;
    bool enabled = false;
    std::string version_note; // "" unless the cheat names another version or revision
};

/// The cheats of the title in file order (loads the file when the engine has not). The index is the
/// one SetCheatEnabled takes.
std::vector<CheatInfo> ListCheats(CheatEngine& engine, u64 title_id);

/// Turns cheat `index` on or off, live (the engine runs it from its next tick), and saves the
/// file. False when the index is out of range.
bool SetCheatEnabled(CheatEngine& engine, u64 title_id, std::size_t index, bool enabled);

// ---------------------------------------------------------------------------------------------
// Fixes (the Developer source's code.ips)

enum class FixState {
    NotInstalled,
    Installed,  // our code.ips is in the title's mods folder
    OtherPatch, // the player's own code.ips is there (or exefs/code.ips): left alone
};

enum class FixResult { Ok, AlreadyInstalled, Conflict, NotAvailable, Error };

struct FixOutcome {
    FixResult result = FixResult::Ok;
    std::string error;
};

/// "<load dir>/mods/<TITLEID>/", the folder Azahar's "Open Mods Location" shows.
std::string ModsDir(u64 title_id);

FixState GetFixState(u64 title_id);
/// True when the last DownloadCheats or InstallFix saw a code.ips for the title (this run), or the mod
/// catalog has the maintainer's fix for it.
bool FixAvailable(u64 title_id);
/// Installs the maintainer's fix for the title through the mod library (core/mods/library.h): the
/// catalog entry with verified=true, downloaded, kept in <LoadDir>/mod-library and copied to
/// <mods>/code.ips. Another code patch there gives Conflict and nothing is written. Takes effect on
/// the next game start. (InstallFixWith / InstallFixTo below are the older direct path, for tests.)
FixOutcome InstallFix(u64 title_id);
/// Removes the fix InstallFix (or an earlier version's InstallFix) wrote. A code.ips that is not ours
/// is never touched (Conflict).
FixOutcome RemoveFix(u64 title_id);

/// The file side of the above, on a folder of your choosing (tests).
FixOutcome InstallFixTo(const std::string& dir, std::string_view ips);
FixOutcome RemoveFixFrom(const std::string& dir);
FixState GetFixStateIn(const std::string& dir);
FixOutcome InstallFixWith(const HttpGet& get, u64 title_id, const std::string& dir);

} // namespace Cheats::Online
