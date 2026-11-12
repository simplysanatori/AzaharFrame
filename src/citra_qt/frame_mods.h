// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texui: the Qt side of the in-game menu's Mods and Textures segments. It reads the
// running game's mods (core/mods/library.h) and texture packs (core/custom_tex/packs.h) into the
// segments' models (frame_xr/menu/mods_segments.h), publishes them to the menu
// (FrameXr::SetInGameMenuModsList, SetInGameMenuTextures), and does what the segments ask. Disk and
// network work runs on threads of its own; this object lives on the GUI thread.
//
// Nothing is replaced without a question: an install that conflicts with a mod asks "Replaces
// <name>" (Replace installs with replace = true, which keeps the other mod in the library); one
// that conflicts with your own files says to remove them first. Removing your own files asks too.
// Every change applies from the next start of the game (the snackbar's Restart).
//
// Download Packs (core/custom_tex/download.h): one Downloader for the session; a download over
// 1 GB asks "Download 2.1 GB?" first, and Delete (downloaded packs only) asks too. Finished packs
// show up in the pack list.

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <QObject>

#include "common/common_types.h"
#include "core/custom_tex/packs.h"
#include "frame_xr/menu/mods_segments.h"

namespace CustomTex::Download {
class Downloader;
struct PackInfo;
} // namespace CustomTex::Download
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
    /// SetModActive, RemoveMod, ModsDialog, OpenUrl, LoadPackDownloads, DownloadPack,
    /// CancelPackDownload, DeletePack, TexturesDialog).
    void Command(const FrameXr::MenuRequest& request);
    /// Custom Textures or Preload changed (the Qt side saved it): the snackbar.
    void TextureSettingChanged();

private:
    struct Pending {
        enum class Kind { None, Install, Remove, Download, Delete } kind = Kind::None;
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
    void LoadPackDownloads();
    void ListPackDownloads();
    void StartDownload(const std::string& id);
    void DeletePack(const std::string& folder);
    void PollDownload();
    void SetOffers(const std::vector<CustomTex::Download::PackInfo>& list);
    FrameXr::Menu::PackOffer* PackOffer(const std::string& id);
    FrameXr::Menu::ModOffer* Offer(const std::string& id);

    u64 title_id = 0;
    int run = -1;
    FrameXr::Menu::ModsListModel mods;
    FrameXr::Menu::TexturesModel textures;
    std::vector<CustomTex::Pack> packs; ///< as listed, for Activate
    std::string installing;             ///< the mod an install runs for
    std::string failed;                 ///< the mod whose last install failed
    Pending pending;                    ///< what the dialog's Replace / Remove does
    Pending pack_pending;               ///< what the Textures dialog's Download / Delete does
    std::shared_ptr<CustomTex::Download::Downloader> downloader; ///< one for the session
    bool catalog_fetched = false;       ///< the texture catalog's one request a run
    std::set<std::string> downloaded;   ///< folder names Download Packs installed (for Delete)
    std::map<std::string, u64> offer_sizes; ///< the offers' sizes in bytes (0 unknown)
    std::string pack_failed, pack_error; ///< the pack whose last download failed, and why
    u64 download_title = 0;             ///< the title the running download is for
    QTimer* download_poll = nullptr;
    bool sizing = false;
    QTimer* progress = nullptr;
};
