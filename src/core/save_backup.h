// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame savestate: Load Anyway's safety net. Before a state from another build is loaded,
// the game's own save data folder is copied to <user>/save-backups/<TITLE ID>/<stamp>/ (the
// newest kKeep kept), and a marker names that copy until the session ends cleanly. A marker
// still there at the next start of the game (the session crashed, or the load failed) offers
// Restore Save Data once. No Qt.
//
// AzaharFrame advfix:
// - A backup holds the game's parts, each in a folder of its own: its save data ("save") and,
//   when the game has any, its extra data ("extdata", the SD card's extdata folder for the
//   game's extdata ID). <stamp>/save/..., <stamp>/extdata/...
// - Restore never loses newer progress: the parts it replaces are copied to
//   root/<stamp>-before-restore/ first (in the same rotation), and the question names the
//   backup's time. A part the backup does not hold (the game had none then) is left as it is.
// - The offer ends after kOfferPlayMinutes of play with no error since the Load Anyway.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Core::SaveBackup {

inline constexpr std::size_t kKeep = 3;
/// AzaharFrame advfix: the parts' folder names in a backup.
inline constexpr const char* kSavePart = "save";
inline constexpr const char* kExtdataPart = "extdata";
/// AzaharFrame advfix: the copy Restore makes of the save data it replaces: "<stamp>" + this.
inline constexpr std::string_view kBeforeRestoreSuffix = "-before-restore";
/// AzaharFrame advfix: a Load Anyway's session that played this long with no error disarms the
/// offer (a later crash is unlikely to be the state's doing). Minutes of play.
inline constexpr int kOfferPlayMinutes = 10;
/// AzaharFrame advfix: the question's buttons and body (the title is RestoreQuestion's).
inline constexpr const char* kRestoreButton = "Restore";
inline constexpr const char* kKeepCurrentButton = "Keep Current";
inline constexpr const char* kRestoreBody =
    "A save state from another version was loaded last time, and the game didn't close normally. "
    "Restore puts back this game's save data from before that load. Your current save data is "
    "backed up first.";

/// AzaharFrame advfix: one folder of the game's data: its name in a backup and where it lives.
struct Part {
    std::string name, dir;
    bool operator==(const Part&) const = default;
};

/// <user_dir>/save-backups/<TITLE ID>/ (ends with '/').
std::string Root(const std::string& user_dir, u64 title_id);
/// "20261112-101500" (local time) for `unix_seconds`.
std::string Stamp(s64 unix_seconds);

/// Copies each part's folder that exists (recursively; links copied as links, never followed) to
/// root/<stamp>/<name>/, then keeps only the newest kKeep backups. The backup's path, or "" when
/// no part's folder exists or a copy failed (nothing half-copied is left).
std::string Backup(const std::vector<Part>& parts, const std::string& root,
                   const std::string& stamp);
/// The backups, newest first.
std::vector<std::string> List(const std::string& root);
/// Removes all but the newest `keep`.
void Rotate(const std::string& root, std::size_t keep);
/// Puts the backup's parts in place of the game's. AzaharFrame advfix: the game's parts are first
/// copied to root/<stamp>-before-restore/ (`kept` gets its path; "" when the game had none), and
/// nothing changes when that copy fails. Then each part the backup holds is copied beside its
/// folder and swapped in, all or none, so a failure leaves the game's folders as they were; on
/// success the newest kKeep backups are kept. False on failure.
bool Restore(const std::string& backup, const std::vector<Part>& parts, const std::string& root,
             const std::string& stamp, std::string* kept = nullptr);
/// AzaharFrame advfix: when a backup was made, from its name: "10:26" on `now`'s local day, else
/// "2026-11-11 10:26"; "" when the name has no stamp.
std::string TimeText(const std::string& backup, s64 now_unix);
/// "Restore Save Data From 10:26?" ("Restore Save Data?" when `time` is "").
std::string RestoreQuestion(const std::string& time);

/// The marker: Load Anyway ran with `backup` of `parts`; Disarm on a clean end.
void Arm(const std::string& root, const std::string& backup, const std::vector<Part>& parts);
void Disarm(const std::string& root);
struct Pending {
    std::string backup;
    std::vector<Part> parts;
};
std::optional<Pending> Armed(const std::string& root);

} // namespace Core::SaveBackup
