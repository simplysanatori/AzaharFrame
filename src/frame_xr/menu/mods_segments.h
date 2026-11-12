// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the Mods tab's Mods and Textures segments: their values (hooks:
// cheats_tab.h). Placeholders for the texui lane, which owns this file and mods_segments.cpp.

#pragma once

namespace FrameXr::Menu {

/// What the Mods segment shows (the mods loaded for the game; Install Mods).
struct ModsListModel {
    bool operator==(const ModsListModel&) const = default;
};

/// What the Textures segment shows (Off and the packs, Custom Textures, Preload).
struct TexturesModel {
    bool operator==(const TexturesModel&) const = default;
};

} // namespace FrameXr::Menu
