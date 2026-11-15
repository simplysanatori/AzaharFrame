// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame useglobal: the in-game menu's per-game rows against Settings::values: which ones the
// running game has its own value for, and "Use Global Value" (the game follows the global value
// again). The game's file is the Qt side's (MenuCommand::UseGlobalValue, citra_qt.cpp): the key
// goes from it (QtConfig::WriteGlobalSetting), the global value is never written there.

#pragma once

#include <string>
#include <vector>
#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::PerGame {

/// Whether the row's setting follows the global value (true for a row that is not per game).
bool UsesGlobal(Menu::Id id);
/// The game's own value goes: the setting follows the global value. Returns whether it had one.
bool UseGlobalValue(Menu::Id id);
/// MenuModel::game_value for every per-game row.
void FillGameValues(Menu::MenuModel& model);
/// AzaharFrame fixa: the keys a per-game row writes to the game's file (empty for a row that is
/// not per game): both of 3D Depth's (the Full modes' and the 3DS one), Render Margin's axis key.
/// For the check that ForEachMenuKey (per_game_keys.h) keeps them all.
std::vector<std::string> KeysOf(Menu::Id id);

} // namespace FrameXr::PerGame
