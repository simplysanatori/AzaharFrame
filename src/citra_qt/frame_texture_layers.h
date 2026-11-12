// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texlayers: the top of the Flat window's "Customize Texture Pack" dialog
// (frame_texture_parts.h): the game's packs, each with a checkbox (selecting one shows its parts
// below), and with two or more on, Enabled Textures: every entry of the packs on in one order
// (core/custom_tex/layers.h), Move Up and Move Down. The same as the in-game menu's Textures page.
// Saved with the dialog's OK.

#pragma once

#include <memory>
#include <string>
#include <QWidget>
#include "core/custom_tex/layers.h"

class FrameTexturePartsDialog;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class FrameTextureLayers : public QWidget {
public:
    FrameTextureLayers(FrameTexturePartsDialog* dialog,
                       std::shared_ptr<CustomTex::Layers::Game> game);

    /// The dialog's parts of `pack` changed: the list follows.
    void PackChanged(const std::string& pack, const CustomTex::Parts::State& state);

private:
    void Rebuild();
    void PackItemChanged(QListWidgetItem* item);
    void LayerItemChanged(QListWidgetItem* item);
    void MoveSelected(int delta);
    void Selected();
    /// The pack's choices as the list has them, to the dialog.
    void PushState(const std::string& pack);

    FrameTexturePartsDialog* dialog;
    std::shared_ptr<CustomTex::Layers::Game> game;
    bool building = false;
    QListWidget* packs = nullptr;
    QWidget* order_box = nullptr;
    QListWidget* order = nullptr;
    QPushButton* move_up = nullptr;
    QPushButton* move_down = nullptr;
};
