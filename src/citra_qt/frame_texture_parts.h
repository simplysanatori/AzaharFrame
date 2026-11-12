// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texparts: "Customize Texture Pack" in the Flat window: the parts of the game's pack
// (core/custom_tex/parts.h), turned on and off, a variant chosen, the order of the parts that
// conflict. It opens from the game list menu, and from Emulation while a game runs (for that
// game). The choices are saved on OK, and the loader reads them at the next start of the game, so
// with the game running the dialog says "Restart the Game to Apply". The in-game menu's Customize
// page (VR) does the same (citra_qt/frame_mods.cpp).

#pragma once

#include <string>
#include <QDialog>
#include "core/custom_tex/parts.h"

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class FrameTexturePartsDialog : public QDialog {
public:
    /// `pack`: the pack's folder (load/textures/<TID>, the symlink to it, works too).
    FrameTexturePartsDialog(QWidget* parent, const std::string& pack, bool running);

    /// The pack's parts could be read and there are two or more.
    bool HasParts() const;
    /// Saves the choices (OK).
    bool Save();

    /// Opens the dialog for the pack of `title_id` (load/textures/<TID>); a message instead when
    /// the game has no pack or the pack has one part. `running`: the game runs now.
    static void Open(QWidget* parent, u64 title_id, bool running);

private:
    enum Column { kPart, kTextures, kSize, kConflicts };
    void Rebuild();
    void ItemChanged(QTreeWidgetItem* item, int column);
    void Selected();
    void MoveSelected(int delta);
    /// The group of the selected row, -1 none.
    int SelectedGroup() const;

    std::string pack;
    CustomTex::Parts::Layout layout;
    CustomTex::Parts::State state;
    bool building = false;
    std::string selected_key; ///< the group selected, kept across a rebuild

    QObject* radio_groups = nullptr; ///< owns the variants' button groups of the list shown
    QLabel* header = nullptr;
    QLabel* note = nullptr;
    QTreeWidget* tree = nullptr;
    QPushButton* move_up = nullptr;
    QPushButton* move_down = nullptr;
    QPushButton* all_on = nullptr;
    QPushButton* all_off = nullptr;
};
