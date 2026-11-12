// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texparts: the texture pack Customize screens for the menu dump
// (frame_xr_menu_dump --texparts THUMBS_DIR): the real Textures segment and Customize page
// (mods_segments.cpp) with sample packs, values taken from real packs. Dump only.

#pragma once

#include <functional>
#include <string>

#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu::TexPartsDump {

/// Draws the menu with `model` and `view` and saves `<name>.png`.
using Shot = std::function<bool(const std::string& name, const MenuModel& model, MenuView view)>;

/// Renders the screens. `thumbs_dir` holds <key>.png part previews (missing: the placeholder).
bool Run(const std::string& thumbs_dir, const MenuModel& base, const Shot& shot);

} // namespace FrameXr::Menu::TexPartsDump
