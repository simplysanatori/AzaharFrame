// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the Mods tab's Mods and Textures segments (hooks: cheats_tab.h).
// Placeholders for the texui lane, which owns this file: a centred label in the segment's area.

#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/menu/mods_tab.h"
#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;

void TabMenu::DrawModsList(Kit& kit, const MenuModel& /*model*/, const MenuView& /*view*/,
                           const Rect& area) {
    kit.Icon(area.Cx(), area.Cy() - 24.0f, 48.0f, Sym::kExtension, false, Tok::kOnSurfaceVariant);
    kit.TextMid(area.Cx(), area.Cy() + 28.0f, Type::Title, Tok::kOnSurface, "Mods", 1);
}

void TabMenu::DrawTextures(Kit& kit, const MenuModel& /*model*/, const MenuView& /*view*/,
                           const Rect& area) {
    kit.TextMid(area.Cx(), area.Cy(), Type::Title, Tok::kOnSurface, "Textures", 1);
}

} // namespace FrameXr::Menu
