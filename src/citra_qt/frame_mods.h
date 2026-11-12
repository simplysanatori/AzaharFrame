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
//
// Install from Zip (core/mods/zip_install.h; AzaharFrame zipinstall): in VR the menu's own file
// browser (the system's file chooser portal opens its window on Steam's display, outside the game's
// VR layers, so it cannot be seen or used in the headset), in Flat Qt's file dialog (a window of the
// app, which gamescope shows in the theater). The archive is unpacked and installed off the GUI
// thread; the page shows the progress, asks before anything is replaced, and a toast says when it is
// done.

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <QObject>

#include "common/common_types.h"
#include "core/custom_tex/packs.h"
#include "frame_xr/menu/mods_segments.h"

namespace Mods::Zip {
struct Job;
}
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

    /// AzaharFrame modcat: how to read the CRC32 of the running game's .code (GUI thread), for mods
    /// made for one build of a game (Project Restoration); nullopt when it cannot be read.
    void SetCodeCrcSource(std::function<std::optional<u32>()> source);
    /// AzaharFrame catalog3: turns Custom Textures on for the running game (GUI thread); true when it
    /// was off and now is on. Called after a mod with a textures part installs.
    void SetTexturesEnabler(std::function<bool()> enabler);

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
    std::function<std::optional<u32>()> code_crc_source; // AzaharFrame modcat
    std::function<bool()> textures_enabler;              // AzaharFrame catalog3
    std::map<std::string, u64> mod_sizes; ///< AzaharFrame catalog3: bytes a mod still has to download (0 unknown or here already)
    std::optional<u32> game_crc;
    bool game_crc_read = false;
    struct Pending {
        enum class Kind { None, Install, Remove, Download, Delete, BigInstall } kind = Kind::None;
        std::string id;
    };

    void RefreshLoaded();
    void RefreshAvailable();
    void RefreshPacks();
    void ComputeSizes();
    void Install(const std::string& id, bool replace, bool confirmed = false);
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
    // AzaharFrame zipinstall: Install from Zip
    void ZipStart(int target);
    void ZipBrowse(const std::string& folder);
    void ZipPick(const std::string& path);
    void ZipInstall(std::shared_ptr<Mods::Zip::Job> job, u64 tid, bool replace);
    void ZipAnswer(bool yes);
    void ZipDone(bool texture, u64 tid);
    void ZipFail(const std::string& error);
    void ZipCheats(const std::string& path); ///< AzaharFrame ui2: a cheat file, or a zip of one
    void ZipCloseJob();
    void PublishZip();

    u64 title_id = 0;
    int run = -1;
    FrameXr::Menu::ModsListModel mods;
    FrameXr::Menu::TexturesModel textures;
    std::vector<CustomTex::Pack> packs; ///< as listed, for Activate
    std::string installing;             ///< the mod an install runs for
    std::string failed;                 ///< the mod whose last install failed
    std::string failed_reason;          ///< AzaharFrame modfix: why, in a few words (Mods::ShortFailure)
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
    // AzaharFrame zipinstall
    enum class ZipQuestion { None, ReplaceSame, ReplaceOther, Blocked };
    FrameXr::Menu::ZipModel zip;
    std::shared_ptr<Mods::Zip::Job> zip_job; ///< unpacked, waiting for a choice or an answer
    std::vector<u64> zip_choices;            ///< the games ZipModel::choices names
    u64 zip_title = 0;                       ///< the title the install is for
    ZipQuestion zip_question = ZipQuestion::None;
    std::string zip_mod_id;                  ///< ReplaceOther, Blocked: the mod in the library
    std::string zip_folder;                  ///< the folder browsed last (this session)
    std::set<std::string> zip_packs;         ///< pack folders Install from Zip made (for Delete)
};
