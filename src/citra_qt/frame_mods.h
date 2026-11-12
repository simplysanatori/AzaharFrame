// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texui: the Qt side of the in-game menu's Mods and Textures segments. It reads the
// running game's mods (core/mods/library.h) and texture packs (core/custom_tex/packs.h) into the
// segments' models (frame_xr/menu/mods_segments.h), publishes them to the menu
// (FrameXr::SetInGameMenuMods, SetInGameMenuTextures), and does what the segments ask. Disk and
// network work runs on threads of its own; this object lives on the GUI thread.
//
// Nothing is replaced without a question: an install that conflicts with a mod asks "Replaces
// <name>" (Replace installs with replace = true, which keeps the other mod in the library); one
// that conflicts with your own files says to remove them first. Removing your own files asks too.
// Every change applies from the next start of the game (the snackbar's Restart).

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QObject>

#include "common/common_types.h"
#include "core/custom_tex/packs.h"
#include "frame_xr/menu/mods_segments.h"

namespace FrameXr {
struct MenuRequest;
}
class QTimer;

class FrameMods : public QObject {
public:
    FrameMods(QObject* parent);
    ~FrameMods() override;

    /// The in-game menu opened on this game (`run` counts the games started, so a restart of the
    /// same game clears the snackbar): read its mods and packs and publish them.
    void Open(u64 title_id, int run);
    /// One of the segments' commands (SelectTexturePack, LoadModCatalog, InstallMod,
    /// SetModActive, RemoveMod, ModsDialog).
    void Command(const FrameXr::MenuRequest& request);
    /// Custom Textures or Preload changed (the Qt side saved it): the snackbar.
    void TextureSettingChanged();

private:
    struct Pending {
        enum class Kind { None, Install, Remove } kind = Kind::None;
        std::string id;
    };

    void RefreshLoaded();
    void RefreshAvailable();
    void RefreshPacks();
    void ComputeSizes();
    void Install(const std::string& id, bool replace);
    void Deactivate(const std::string& id);
    void Remove(const std::string& id);
    void SelectPack(const std::string& id);
    void PollProgress();
    void PublishMods();
    void PublishTextures();
    FrameXr::Menu::ModOffer* Offer(const std::string& id);

    u64 title_id = 0;
    int run = -1;
    FrameXr::Menu::ModsListModel mods;
    FrameXr::Menu::TexturesModel textures;
    std::vector<CustomTex::Pack> packs; ///< as listed, for Activate
    std::string installing;             ///< the mod an install runs for
    std::string failed;                 ///< the mod whose last install failed
    Pending pending;                    ///< what the dialog's Replace / Remove does
    bool sizing = false;
    QTimer* progress = nullptr;
};
