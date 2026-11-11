// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: what the in-game menu's Cheats tab shows (the values; TabMenu::DrawCheats
// in cheats_tab.cpp draws them). Three sections: the developer's own cheats and game fix
// (github.com/simplysanatori/3ds-game-fixes, made for Azahar for Frame, with a badge), Sharkive's
// and the player's own. A cheat's switch applies at once; the fix (code.ips) is copied to the
// game's mods folder and needs a restart of the game.
//
// The fields mirror core/cheats/online.h (Cheats::Online::CheatInfo, FixState, DownloadResult),
// which the host converts from; nothing here depends on core.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FrameXr::Menu {

/// As Cheats::Online::Source.
enum class CheatSource : uint8_t { User, Developer, Sharkive };

/// As Cheats::Online::CheatInfo; the index in CheatsModel::cheats is SetCheatEnabled's.
struct CheatItem {
    std::string name;
    CheatSource source = CheatSource::User;
    bool enabled = false;
    /// "" unless the cheat names another version or revision ("for v1.0, this game is v1.1");
    /// the chip shows its version ("v1.0", "Rev0").
    std::string version_note;
    bool operator==(const CheatItem&) const = default;
};

/// As Cheats::Online::FixState, with None when the title has no fix (FixAvailable false).
enum class FixState : uint8_t { None, NotInstalled, Installed, OtherPatch };

enum class DownloadState : uint8_t { Idle, Downloading, Done, Failed };

/// The developer's badge (owner to pick one; the other goes).
enum class CheatBadge : uint8_t {
    Star,     ///< A: a filled star before the name, "Made for Azahar for Frame"
    Verified, ///< B: Material's verified badge after the name, "Azahar for Frame"
};

struct CheatsModel {
    std::vector<CheatItem> cheats; ///< file order (ListCheats); drawn grouped by source
    FixState fix = FixState::None; ///< the developer's game fix, first in its section
    DownloadState download = DownloadState::Idle;
    int progress = 0; ///< 0 to 100 while Downloading
    /// After a download: StatusText ("12 cheats added, 3 for another version", or why it
    /// failed); shown in the error colour when `download` is Failed.
    std::string status;
    CheatBadge badge = CheatBadge::Star;
    bool operator==(const CheatsModel&) const = default;
};

/// The chip's text from a version note: "for v1.0, this game is v1.1" gives "v1.0".
std::string VersionChip(const std::string& version_note);

} // namespace FrameXr::Menu
