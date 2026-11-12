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
#include "core/custom_tex/parts.h" // AzaharFrame texparts
#include "core/mods/compat.h" // AzaharFrame compat
#include "frame_xr/menu/mods_segments.h"

namespace Mods {
struct AvailableMod; // AzaharFrame stall
}
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
class QProcess;
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
    /// AzaharFrame catalog3: where short messages go (a toast in VR, the status bar in Flat).
    void SetNotifier(std::function<void(const std::string&)> notifier);
    /// AzaharFrame compat: the running game's region and version (GUI thread; the CRC is added
    /// from the source above), for the entries made for another version of it.
    void SetGameInfoSource(std::function<Mods::Compat::Game()> source);

    /// The in-game menu opened on this game (`run` counts the games started, so a restart of the
    /// same game clears the snackbar): read its mods and packs and publish them.
    void Open(u64 title_id, int run);
    /// One of the segments' commands (SelectTexturePack, LoadModCatalog, InstallMod,
    /// SetModActive, RemoveMod, ModsDialog, OpenUrl, LoadPackDownloads, DownloadPack,
    /// CancelPackDownload, DeletePack, TexturesDialog).
    void Command(const FrameXr::MenuRequest& request);
    /// Custom Textures or Preload changed (the Qt side saved it): the snackbar.
    void TextureSettingChanged();
    /// AzaharFrame ui3: whether a game runs now (GUI thread): a pack switch Optimize for Frame
    /// or Use Original asks for then waits for ApplyPendingPack.
    void SetGameRunningSource(std::function<bool()> source);
    /// AzaharFrame ui3: the switch that waited (the game stopped, or the next one boots, before
    /// its loader reads the folders): load/textures/<TID> points at the chosen pack of the pair,
    /// the Customize choices carried over. GUI thread; quick (a listing and a symlink).
    void ApplyPendingPack();

private:
    std::function<std::optional<u32>()> code_crc_source; // AzaharFrame modcat
    std::function<bool()> textures_enabler;              // AzaharFrame catalog3
    std::function<void(const std::string&)> notifier;    // AzaharFrame catalog3
    std::map<std::string, u64> mod_sizes; ///< AzaharFrame catalog3: bytes a mod still has to download (0 unknown or here already)
    std::optional<u32> game_crc;
    bool game_crc_read = false;
    std::function<Mods::Compat::Game()> game_info_source; // AzaharFrame compat
    std::optional<Mods::Compat::Game> game_info;           // read once a run
    const Mods::Compat::Game& GameInfo();
    std::set<std::string> anyway; ///< AzaharFrame compat: mods and packs confirmed "Anyway"
    /// AzaharFrame compat: mods and packs whose unscanned GameBanana file the player allowed
    std::set<std::string> unscanned_ok;
    struct Pending {
        enum class Kind { None, Install, Remove, Download, Delete, BigInstall,
                          Unscanned /* AzaharFrame compat */,
                          Optimize /* AzaharFrame ui3: the PNG pack's path */,
                          RemoveOriginal /* AzaharFrame ui3: the optimized copy's path */ } kind = Kind::None;
        std::string id;
    };

    void AllowAnyway(const std::string& id, int value); // AzaharFrame compat
    void RefreshLoaded();
    void RefreshAvailable();
    /// AzaharFrame stall: Install Mods' list from the catalog in hand (no request); with
    /// `network`, the catalog's latest copy and the sources' dates first.
    void ListAvailable(bool network);
    void SetAvailable(const std::vector<Mods::AvailableMod>& list);
    void RefreshPacks();
    void ComputeSizes();
    void Install(const std::string& id, bool replace, bool confirmed = false);
    void Deactivate(const std::string& id);
    void Remove(const std::string& id);
    void SelectPack(const std::string& id);
    void PollProgress();
    void PublishMods();
    void PublishTextures();
    // AzaharFrame texparts: a pack's Customize page (core/custom_tex/parts.h)
    void OpenParts(const std::string& pack_id);
    void PartClicked(int row);
    void MovePart(int row, int delta);
    void AllParts(bool on);
    void PartsChanged(); ///< saves the state, the page and the pack row again
    void BuildPartRows();
    struct PartAction {
        enum class Kind { None, Part, Folder, Choose } kind = Kind::None;
        std::size_t group = 0; ///< index into parts_layout.groups
        std::string key;       ///< Part: the part; Choose: the part ("" Off)
    };
    struct PackParts {
        std::size_t count = 0; ///< Layout::PartCount
        std::string on_text;   ///< "6 of 7 Parts"
        bool optimizable = false; ///< AzaharFrame ui3: PNG, or an optimized copy: it has a page
    };
    std::map<std::string, PackParts> pack_parts; ///< by pack id, from RefreshPacks
    std::string parts_path;                      ///< the pack the page is for
    CustomTex::Parts::Layout parts_layout;
    CustomTex::Parts::State parts_state;
    std::vector<PartAction> part_actions; ///< one for each row of textures.parts
    /// The page's previews, by part key.
    std::map<std::string, std::shared_ptr<const std::vector<uint8_t>>> part_previews;
    int parts_serial = 0; ///< the page opened last (a scan of an older one is dropped)
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
    void ZipDone(bool texture, u64 tid, const std::string& mod_id = {});
    void TurnBackOn(); ///< AzaharFrame compat: the "Turned Off" snackbar's action
    void ZipFail(const std::string& error);
    void ZipCheats(const std::string& path); ///< AzaharFrame ui2: a cheat file, or a zip of one
    void ZipCloseJob();
    void PublishZip();

    // AzaharFrame ui3: Optimize for Frame (frame_mods_optimize.cpp; core/custom_tex/optimize.h)
    void OptimizeAsk(const std::string& pack_id);   ///< the estimate, then the dialog
    void OptimizeStart(const std::string& png);     ///< the dialog's Optimize
    void OptimizeConvert(const std::string& png);   ///< after the encoder is there
    void OptimizeStop();
    void OptimizeDone(int code, bool stopped);
    void UseOriginal(const std::string& pack_id);
    /// AzaharFrame ui3: Remove Original: "Remove the Original Pack?" for the page's pair, then (the
    /// dialog's Remove) the guarded delete off the GUI thread (CustomTex::Optimize::RemoveOriginal).
    void RemoveOriginalAsk(const std::string& pack_id);
    void RemoveOriginalDo(const std::string& optimized);
    /// The folder the game reads its pack from (load/textures/<TID>).
    std::string ActiveLink() const;
    /// A pack's size as the page shows it ("4.7 GB"), "" unknown.
    std::string PackSizeText(const std::string& path) const;
    /// Makes `path` (a pack of the game `tid`) the one used, now or (a game running) when it stops;
    /// `carry_from`: the pair's other pack, whose Customize choices go with it.
    void ChoosePack(u64 tid, const std::string& path, const std::string& carry_from);
    void UpdateOptimizeRow(); ///< the page's row from the pair, the job and the pending switch
    void CheckNudge();        ///< "Optimize <Pack> for Frame?" for a candidate, once a pack
    const CustomTex::Pack* PackByPath(const std::string& path) const;
    std::function<bool()> game_running_source;
    QProcess* optimize_process = nullptr;
    std::string optimize_png;   ///< the pack converting now (canonical path)
    u64 optimize_title = 0;
    FrameXr::Menu::PackOptimize optimize_progress; ///< its Converting state
    bool optimize_stopping = false;
    std::string optimize_output; ///< its last lines, for the log
    std::string optimize_checking; ///< the pack whose estimate runs
    /// Stopped or Failed, by the PNG pack's canonical path (until it runs again or ends well)
    std::map<std::string, FrameXr::Menu::PackOptimize> optimize_states;
    struct PendingSwitch {
        u64 title = 0;
        std::string path;       ///< the pack to use (canonical)
        std::string carry_from; ///< its pair's other pack
    };
    std::optional<PendingSwitch> pending_switch;
    /// Packs that became active (id) or were downloaded ("folder:" + folder name): the nudge's
    /// candidates, checked once their size is known.
    std::set<std::string> nudge_candidates;

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
    u64 mods_network_title = ~0ULL;     ///< AzaharFrame stall: the title the mod list's requests ran for
    bool mods_network_running = false;  ///< AzaharFrame stall: they run now
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
