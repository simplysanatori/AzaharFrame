// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emerald: the homebrew packager registry. A package is a homebrew game that Azahar for
// Frame can set up from its author's own release (downloaded when the player asks, never bundled),
// offered in the Homebrew menu (before Help) and the Homebrew segment of the in-game menu's Mods
// tab; once installed, its game is a file in a game folder like any other. Each one has its logic
// next to this file (emerald3ds.h) and its dialogs in citra_qt (frame_emerald.cpp, registered in
// frame_homebrew.cpp). No Qt here.

#pragma once

#include <span>
#include <string>
#include <string_view>

namespace Homebrew {

struct PackageInfo {
    std::string_view id;     ///< "emerald3ds": what the menus and the game list pass around
    std::string_view title;  ///< "Pokémon Emerald 3DS"
    std::string_view author; ///< "ZallaxDev"
    std::string_view site;   ///< "emerald-3ds.com"
};

/// Every package, in the order the menus list them.
std::span<const PackageInfo> Packages();
const PackageInfo* FindPackage(std::string_view id);

/// "Add Pokémon Emerald 3DS", "Update Pokémon Emerald 3DS"
std::string AddLabel(const PackageInfo& package);
std::string UpdateLabel(const PackageInfo& package);

} // namespace Homebrew
