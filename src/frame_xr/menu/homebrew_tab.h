// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: the Homebrew segment of the in-game menu's Mods tab. One row per homebrew
// package (core/homebrew/packages.h): its title and author, and Install, or Play and Update. The
// values come from the Qt side (citra_qt/frame_homebrew.cpp, FrameXr::SetInGameMenuHomebrew); a
// button sends MenuCommand::HomebrewInstall / HomebrewUpdate / HomebrewPlay with the package's id,
// and Install and Update continue in Azahar's window (the ROM and folder dialogs).
//
// The hooks: ModsSegment::Homebrew (mods_tab.h), Id::Homebrew* in tab_menu.h, TabMenu::DrawHomebrew
// (homebrew_tab.cpp), the model's copy and the clicks in menu_panel.cpp, the commands in
// citra_qt.cpp.

#pragma once

#include <string>
#include <vector>

namespace FrameXr::Menu {

struct HomebrewItem {
    std::string id;     ///< the package's id ("emerald3ds")
    std::string title;  ///< "Pokémon Emerald 3DS"
    std::string author; ///< "ZallaxDev"
    bool installed = false;
    bool update = false; ///< a newer release was seen
    bool busy = false;   ///< installing or updating
    int progress = -1;   ///< 0 to 1000 while busy, -1 when unknown
    std::string status;  ///< while busy ("Downloading"), or the last error
    bool operator==(const HomebrewItem&) const = default;
};

struct HomebrewTabModel {
    std::vector<HomebrewItem> items;
    int spin = 0; ///< the progress ring's step (menu_panel.cpp)
    bool operator==(const HomebrewTabModel&) const = default;
};

} // namespace FrameXr::Menu
