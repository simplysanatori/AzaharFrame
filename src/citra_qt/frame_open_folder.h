// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame qtfiles: Azahar's "open a folder" actions (the game list's Open menu, File > Open
// Azahar Folder, the log folder, Configure's NAND and SDMC buttons, a RomFS dump) hand the folder
// to the desktop's file manager, which never shows in VR or in Game Mode. There they open Qt's own
// file dialog instead (the one Add New Game Directory uses), non-modal so it is drawn on the VR
// panel like the other windows and B closes it: a detail list of the folder's files, with Rename,
// Delete and New Folder on its right-click menu. Desktop Mode keeps the file manager.
// AZAHARFRAME_FILE_BROWSER=app or =system picks one.

#pragma once

#include <QString>

class QWidget;

namespace FrameOpenFolder {

/// True in Game Mode (gamescope) or while VR shows, unless AZAHARFRAME_FILE_BROWSER says.
bool UseDialog();

/// Opens the folder `path` in Qt's file dialog (UseDialog) or the system's file manager.
/// `label` names it in the title ("Mods" -> "Mods Folder").
void Open(QWidget* parent, const QString& path, const QString& label);

} // namespace FrameOpenFolder
