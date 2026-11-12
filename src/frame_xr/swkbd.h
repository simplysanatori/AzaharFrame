// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: the game's keyboard in VR, as the Qt side sees it. When a game opens the 3DS
// software keyboard in VR, citra_qt/applets/swkbd.cpp asks for it here instead of opening its
// dialog (not on view in the headset), and the emulation thread waits for the answer as it waits
// for the dialog. The XR side (swkbd_panel.h) shows it in front of the player, over the paused
// game, the way the in-game menu shows, and takes the screens out of immersive mode meanwhile.
// No Qt and no OpenXR in this header.

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "frame_xr/menu/swkbd_model.h"

namespace FrameXr {

struct SwkbdOutcome {
    /// False: VR ended before an answer (no session any more): use Azahar's dialog instead.
    bool shown = false;
    std::string text;
    uint8_t button = 0;
};

/// Any thread: shows the keyboard in the headset; `done` is called once, on any thread, with the
/// answer. False (and `done` never called): no VR session.
bool OpenGameKeyboard(Swkbd::Spec spec, std::function<void(const SwkbdOutcome&)> done);
/// The keyboard is open (waiting for an answer). Any thread.
bool GameKeyboardOpen();
/// AzaharFrame ui2: the open keyboard was asked for by the in-game menu (Spec::over_menu). Any
/// thread.
bool GameKeyboardOverMenu();
/// A physical keyboard: text typed into the field (UTF-8), and the editing keys. Any thread.
void GameKeyboardText(const std::string& utf8);
/// AzaharFrame input2: Enter is the confirm button, NewLine (Shift+Enter) a new line in multi-line
/// text, Escape the game's cancel button.
enum class GameKeyboardKey { Backspace, Delete, Left, Right, Home, End, Enter, NewLine, Escape };
void GameKeyboardPress(GameKeyboardKey key);
/// AzaharFrame input2: the XR side reads the physical keyboards itself (their event nodes, while
/// the keyboard is open): the Qt side then passes no keys on, or each would type twice. Any thread.
bool GameKeyboardReadsKeyboards();
/// Closes the keyboard from outside: with the game's cancel button and no text (the game is
/// stopping), or, with `fallback`, as not shown (VR ended: Azahar's dialog takes over). Any thread.
void CancelGameKeyboard(bool fallback = false);
/// AzaharFrame ui2: the menu closed: a keyboard it asked for (Spec::over_menu) closes too, as with
/// its Cancel button. False: none was open. Any thread.
bool CancelMenuKeyboard();

} // namespace FrameXr
