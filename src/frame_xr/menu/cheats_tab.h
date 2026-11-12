// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the in-game menu's Mods tab (rail: Main, Position, Stats, Controls, Mods,
// Settings). A Material 3 segmented button at the top (Cheats | Mods | Textures), the chosen
// segment under it. The tab shell is mods_tab.cpp; this file is the Cheats segment's values.
//
// Hooks for the Mods and Textures segments (one per segment, the same shape as the Cheats one):
//
//   draw      TabMenu::DrawCheats / DrawModsList / DrawTextures (Kit&, const MenuModel&,
//             const MenuView&, const Rect& area): `area` is the pane's width under the segmented
//             button (pad it yourself, 24 dp as the other tabs). Set layout.max_scroll when the
//             segment scrolls and read the scroll from view.scroll[Tab::Mods] (shared by the
//             three, set to 0 on a segment change; the beam drags it and the stick steps it).
//             Cheats: cheats_tab.cpp. Mods, Textures: mods_segments.cpp (placeholders).
//   model     MenuModel::mods (ModsTabModel, mods_tab.h): .cheats (CheatsModel, below),
//             .mods (ModsListModel), .textures (TexturesModel) (mods_segments.h).
//   from Qt   FrameXr::SetInGameMenuCheats / SetInGameMenuModsList / SetInGameMenuTextures
//             (frame_xr.h; any thread). The menu copies the last values into its model each frame.
//   ids       Menu::Id after Id::Cheat (tab_menu.h), clicked in MenuPanel::Click (menu_panel.cpp).
//   commands  FrameXr::MenuCommand (frame_xr.h), handled in GMainWindow::FrameMenuRequest
//             (citra_qt.cpp). Cheats: ToggleCheat (`value` = index), DownloadCheats, Cheats
//             (Azahar's window), handled by FrameCheats (citra_qt/frame_cheats.cpp).
//
// The Cheats segment: one flat list of the game's cheats in file order, no subheaders. The
// developer's own (github.com/simplysanatori/3ds-game-fixes) have Material's verified badge after
// the name; Sharkive's and the player's have no marker. A cheat made for another version has an
// outlined chip in the error colour ("v1.0"). Each row's switch applies at once. Patches are not
// here (the Mods segment).
//
// The fields mirror core/cheats/online.h (CheatInfo, DownloadResult), which the Qt side converts
// from; nothing here depends on core.

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

enum class DownloadState : uint8_t { Idle, Downloading, Done, Failed };

struct CheatsModel {
    std::vector<CheatItem> cheats; ///< file order (ListCheats), drawn in that order
    DownloadState download = DownloadState::Idle;
    int spin = 0; ///< the progress indicators' animation step while Downloading (15 a second)
    /// After a download: StatusText ("12 cheats added, 3 for another version"). When `download`
    /// is Failed, only why ("No Internet Connection"): shown under "Download Failed".
    std::string status;
    bool operator==(const CheatsModel&) const = default;
};

/// The chip's text from a version note: "for v1.0, this game is v1.1" gives "v1.0".
std::string VersionChip(const std::string& version_note);

/// The reason alone from a failed download's text: "Download failed: No Internet Connection"
/// gives "No Internet Connection" (the title already says the download failed).
std::string FailureReason(const std::string& error);

} // namespace FrameXr::Menu
