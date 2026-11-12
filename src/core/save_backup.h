// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame savestate: Load Anyway's safety net. Before a state from another build is loaded,
// the game's own save data folder is copied to <user>/save-backups/<TITLE ID>/<stamp>/ (the
// newest kKeep kept), and a marker names that copy until the session ends cleanly. A marker
// still there at the next start of the game (the session crashed, or the load failed) offers
// Restore Save Data once. No Qt.

#pragma once

#include <optional>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Core::SaveBackup {

inline constexpr std::size_t kKeep = 3;

/// <user_dir>/save-backups/<TITLE ID>/ (ends with '/').
std::string Root(const std::string& user_dir, u64 title_id);
/// "20261112-101500" (local time) for `unix_seconds`.
std::string Stamp(s64 unix_seconds);

/// Copies `save_dir` (recursively; links copied as links, never followed) to root/<stamp>/, then
/// keeps only the newest kKeep backups. The backup's path, or "" when `save_dir` is not a folder
/// or the copy failed (nothing half-copied is left).
std::string Backup(const std::string& save_dir, const std::string& root, const std::string& stamp);
/// The backups, newest first.
std::vector<std::string> List(const std::string& root);
/// Removes all but the newest `keep`.
void Rotate(const std::string& root, std::size_t keep);
/// Puts the backup's files in place of `save_dir`'s: copied beside it first, then swapped, so a
/// failure leaves `save_dir` as it was. False on failure.
bool Restore(const std::string& backup, const std::string& save_dir);

/// The marker: Load Anyway ran with `backup` for `save_dir`; Disarm on a clean end.
void Arm(const std::string& root, const std::string& backup, const std::string& save_dir);
void Disarm(const std::string& root);
struct Pending {
    std::string backup, save_dir;
};
std::optional<Pending> Armed(const std::string& root);

} // namespace Core::SaveBackup
