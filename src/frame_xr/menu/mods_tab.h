// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the in-game menu's Mods tab: its segments and their values together.
// The hooks for each segment (draw, model, Qt setter, ids, commands) are listed at the top of
// cheats_tab.h.

#pragma once

#include <cstdint>

#include "frame_xr/menu/cheats_tab.h"
#include "frame_xr/menu/angels_aim_tab.h" // AzaharFrame kiufix
#include "frame_xr/menu/ctgp7_tab.h"      // AzaharFrame ctgp7
#include "frame_xr/menu/mods_segments.h"

namespace FrameXr::Menu {

// AzaharFrame emerald added Homebrew here; ui3 made it a tab of its own (Tab::Homebrew)
// AzaharFrame ctgp7: CTGP-7, shown only while Mario Kart 7 is open (ctgp7_tab.h)
// AzaharFrame kiufix: Angel's Aim, shown only while Kid Icarus: Uprising is open (angels_aim_tab.h)
enum class ModsSegment : uint8_t { Cheats, Mods, Textures, Ctgp7, AngelsAim };

struct ModsTabModel {
    CheatsModel cheats;
    ModsListModel mods;
    TexturesModel textures;
    ZipModel zip; ///< AzaharFrame zipinstall: Install from Zip
    Ctgp7Model ctgp7; ///< AzaharFrame ctgp7
    AngelsAimModel angels_aim; ///< AzaharFrame kiufix
    bool operator==(const ModsTabModel&) const = default;
};

/// AzaharFrame kiufix: the segments shown, in order: Cheats, Mods, Textures, then CTGP-7 and
/// Angel's Aim while their games are open (the segmented button's index is a place in this).
std::vector<ModsSegment> ModsSegmentsShown(const ModsTabModel& m);
/// The segment shown for the view's choice: itself while shown, else Cheats.
ModsSegment ShownSegment(const ModsTabModel& m, ModsSegment chosen);

} // namespace FrameXr::Menu
