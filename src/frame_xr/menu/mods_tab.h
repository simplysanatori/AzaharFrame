// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the in-game menu's Mods tab: its segments and their values together.
// The hooks for each segment (draw, model, Qt setter, ids, commands) are listed at the top of
// cheats_tab.h.

#pragma once

#include <cstdint>

#include "frame_xr/menu/cheats_tab.h"
#include "frame_xr/menu/ctgp7_tab.h" // AzaharFrame ctgp7
#include "frame_xr/menu/mods_segments.h"

namespace FrameXr::Menu {

// AzaharFrame emerald added Homebrew here; ui3 made it a tab of its own (Tab::Homebrew)
// AzaharFrame ctgp7: CTGP-7, shown only while Mario Kart 7 is open (ctgp7_tab.h)
enum class ModsSegment : uint8_t { Cheats, Mods, Textures, Ctgp7 };

struct ModsTabModel {
    CheatsModel cheats;
    ModsListModel mods;
    TexturesModel textures;
    ZipModel zip; ///< AzaharFrame zipinstall: Install from Zip
    Ctgp7Model ctgp7; ///< AzaharFrame ctgp7
    bool operator==(const ModsTabModel&) const = default;
};

} // namespace FrameXr::Menu
