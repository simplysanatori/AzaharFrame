// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the Qt side of the in-game menu's Cheats segment (Mods tab). It reads the
// running game's cheats (core/cheats/online.h) into the segment's model
// (frame_xr/menu/cheats_tab.h), publishes it to the menu (FrameXr::SetInGameMenuCheats), and does
// what the segment asks: a cheat on or off (live, saved) and Download (both sources, on a worker
// thread). GUI thread only.

#pragma once

#include <functional>
#include <string>

#include <QObject>

#include "common/common_types.h"
#include "frame_xr/menu/cheats_tab.h"

namespace Core {
class System;
}
namespace FrameXr {
struct MenuRequest;
}

class FrameCheats : public QObject {
public:
    /// `menu_open`: whether the in-game menu is open (the list is read again every second
    /// then, for changes made in Azahar's window).
    FrameCheats(Core::System& system, std::function<bool()> menu_open, QObject* parent);
    ~FrameCheats() override;

    /// The in-game menu opened on this game: read its cheats and publish them.
    void Open(u64 title_id, const std::string& game_path);
    /// One of the segment's commands (ToggleCheat, DownloadCheats).
    void Command(const FrameXr::MenuRequest& request);

private:
    void Publish();
    void Download();
    void SetCheat(int index, bool on);

    Core::System& system;
    u64 title_id = 0;
    std::string game_path;
    FrameXr::Menu::DownloadState download = FrameXr::Menu::DownloadState::Idle;
    std::string status;
    FrameXr::Menu::CheatsModel model; ///< as last published
    bool published = false;
    std::function<bool()> menu_open;
};
