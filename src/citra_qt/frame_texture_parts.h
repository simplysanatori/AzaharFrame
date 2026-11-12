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

#include <functional>
#include <map>
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

    /// Opens the dialog for the packs of `title_id`: AzaharFrame texlayers: its packs with a
    /// checkbox each and, with two or more on, their order (frame_texture_layers.h) over the parts
    /// of the pack selected; a message instead when the game has no pack. `running`: the game
    /// runs now.
    static void Open(QWidget* parent, u64 title_id, bool running);

    // AzaharFrame texlayers: several packs in one dialog
    /// Shows another pack's parts; the choices made in the one shown are kept for Save. `state`:
    /// its choices as they are now (else read from the pack).
    void ShowPack(const std::string& pack, const CustomTex::Parts::State* state = nullptr);
    const std::string& ShownPack() const {
        return pack;
    }
    /// A pack's choices changed outside (the order of the packs): shown again when it is shown.
    void SetPackState(const std::string& pack, const CustomTex::Parts::State& state);
    /// A widget over the parts list.
    void AddTop(QWidget* widget);
    /// After each change of the shown pack's parts.
    std::function<void(const std::string& pack, const CustomTex::Parts::State& state)> on_change;
    /// OK, after the parts are saved: false when it could not save.
    std::function<bool()> on_save;

private:
    enum Column { kPart, kTextures, kSize, kConflicts };
    void Rebuild();
    void ItemChanged(QTreeWidgetItem* item, int column);
    void Selected();
    void MoveSelected(int delta);
    /// The group of the selected row, -1 none.
    int SelectedGroup() const;

    void Changed(); ///< on_change, then the list again
    std::string pack;
    CustomTex::Parts::Layout layout;
    CustomTex::Parts::State state;
    std::map<std::string, CustomTex::Parts::State> other_states; ///< changed packs not shown
    QWidget* top = nullptr;
    QLabel* title = nullptr;
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
