// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: draws the game's keyboard (swkbd_model.h) in Material 3, with the in-game
// menu's kit (md3.h): a filled text field with the game's hint as its label, the count and any
// error as its supporting text, the keys, and the game's buttons (the confirm one filled).

#pragma once

#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/swkbd_model.h"

namespace FrameXr::Swkbd {

/// The whole panel, kPanelW x kPanelH dp, from the canvas's top left. `hover` and `pressed` are
/// key indices of the model's layout, or -1.
void Draw(Menu::Kit& kit, const Model& model, int hover, int pressed);

} // namespace FrameXr::Swkbd
