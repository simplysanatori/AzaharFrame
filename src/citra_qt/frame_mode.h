// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: the Qt side of switching between VR and flat in one run (frame_xr.h, SetVr).
//
// - The gamepad for flat: Steam Input shows the Frame's controllers as a game controller; SDL
//   reads it (input_common's SDL thread pumps the events) and FrameXr maps it as in VR.
// - The mode file: the launcher (steam-frame/azaharframe.sh) starts in the mode used last,
//   the word "vr" or "flat" in $AZAHARFRAME_HOME/mode. Written on every switch, only when the
//   launcher started this run (AZAHARFRAME_MODE is set).
// - The window in Game Mode (gamescope): never unmapped or hidden (gamescope asserted in
//   wlr_seat_keyboard_notify_enter when a window went away under it in Moonlight XR); in VR
//   it is only made fully see-through (_NET_WM_WINDOW_OPACITY 0 on xcb, which gamescope
//   skips), in flat opaque again. SteamVR closes the game's flat screen when a session starts
//   and does not open it again when the session ends, so after VR Steam is asked to show it
//   (steam-cef.py next to the program, ShowDashboard of valve.steam.desktopgame.<SteamAppId>)
//   at 0, 1000 and 2500 ms and once the window was exposed; the window is shown and raised
//   again at 1 s and 3 s if it is not exposed. Every state change of the window is logged.

#pragma once

#include "frame_xr/input/input_frame.h"

class QWidget;

namespace FrameMode {

/// Steam Input's gamepad as one frame of Frame controller input (no aim). False: no gamepad.
/// Any thread.
bool ReadGamepad(FrameXr::Input::InputFrame& frame);

/// "vr" or "flat" into the mode file, when the launcher started this run.
void WriteModeFile(bool vr);

/// True under gamescope (Game Mode); AZAHARFRAME_FADE_WINDOW=0 / 1 overrides.
bool FadesWindow();

/// VR: the window see-through (stays mapped); flat: opaque. Game Mode only (FadesWindow).
void FadeWindow(QWidget* window, bool out);

/// Flat after VR: asks Steam for the flat screen and watches the window come back (logged).
void ReturnToFlat(QWidget* window);

} // namespace FrameMode
