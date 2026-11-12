// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the in-game menu's Mods tab: its segments and their values together.
// The hooks for each segment (draw, model, Qt setter, ids, commands) are listed at the top of
// cheats_tab.h.

#pragma once

#include <cstdint>

#include "frame_xr/menu/cheats_tab.h"
#include "frame_xr/menu/mods_segments.h"

namespace FrameXr::Menu {

enum class ModsSegment : uint8_t { Cheats, Mods, Textures };

struct ModsTabModel {
    CheatsModel cheats;
    ModsListModel mods;
    TexturesModel textures;
    bool operator==(const ModsTabModel&) const = default;
};

} // namespace FrameXr::Menu
