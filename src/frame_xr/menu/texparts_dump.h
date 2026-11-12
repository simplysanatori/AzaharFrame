// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texparts: a design mockup of "Customize" for texture packs (turn parts of a pack on
// and off, pick one of a pack's variants), drawn with the menu's own Kit over the real Mods tab.
// Dump-only (frame_xr_menu_dump --texparts THUMBS_DIR), fake data, no feature code: nothing here
// is linked into the emulator.

#pragma once

#include <functional>
#include <string>

#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu::TexPartsMock {

/// Draws the menu with `model` and `view`, calls `overlay` on top, saves `<name>.png`.
using Shot = std::function<bool(const std::string& name, const MenuModel& model,
                                const MenuView& view, const std::function<void(Kit&)>& overlay)>;

/// Renders every mock screen. `thumbs_dir` holds <key>.png part previews (missing: a placeholder).
bool Run(const std::string& thumbs_dir, const MenuModel& base, const Shot& shot);

} // namespace FrameXr::Menu::TexPartsMock
