// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame window: the 3DS bottom screen in its own desktop window during VR
// (Settings::values.vr_bottom_screen_window, config file only). Azahar's second render window
// (the one of the Separate Windows layout) is shown in VR with the bottom screen alone; its mouse
// input is Azahar's own mouse-to-touch path. In Game Mode gamescope turns it into a SteamVR
// overlay of its own: the window carries the STEAM_GAME property of an app id that is not the
// game's (AZAHARFRAME_BOTTOM_WINDOW_APPID, a number; "game" leaves the game's id, so it shares
// the game's overlay with the main window, which is see-through in VR), so the SteamVR dashboard
// gives it its own frame, which can be floated in the world. As the main window, it is never
// unmapped once shown (gamescope asserted when a focused window went away under it): out of VR
// it is only made see-through (gamescope skips such windows), and hidden when the program ends.

#pragma once

class GRenderWindow;
class QString;

namespace FrameBottomWindow {

/// Shows the window as the bottom screen (`in_vr`, a game running and the setting on) or puts it
/// back. Returns true while the window is the bottom screen; then the caller leaves it alone.
bool Update(GRenderWindow* window, bool in_vr, bool emulation_running);

/// The window was shown as the bottom screen in this run: do not hide it (see above).
bool KeepMapped();

/// The window's title while it is the bottom screen.
QString Title();

} // namespace FrameBottomWindow
