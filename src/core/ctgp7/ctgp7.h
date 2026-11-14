// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ctgp7: the one-click CTGP-7 option of the in-game menu's Mods tab (its CTGP-7
// segment). CTGP-7 is a community mod pack for Mario Kart 7 (PabloMK7 and the CTGP-7 team): a
// plugin that the CTGP-7 launcher hands to Azahar's 3GX plugin loader, on top of the player's own
// Mario Kart 7. Nothing of it is bundled, re-hosted or patched (its licence asks for that): the
// official offline installer is downloaded from its GameBanana page when the player asks, and run
// in the emulator as it is.
//
// What it needs, found in a run on the Frame (handoff/reports/32/ctgp7/boot-test.md):
//   1. Mario Kart 7 installed as a title (the launcher refuses a cartridge or a ROM file): the
//      player's own .3ds/.cci is wrapped into a CIA (cia_build.h) and installed, or their .cia is.
//   2. The game's v1.2 update installed (the launcher checks it). Never fetched from Nintendo.
//   3. Save data: Mario Kart 7 run once.
//   4. The offline installer (a .3dsx) run in the emulator: about 5 minutes and one A press. It
//      writes sdmc/CTGP-7/ and installs the launcher title 0004000003070C00.
//   5. The plugin loader on for the launcher's run only (RunOverride below).
// CTGP-7's own updater keeps it up to date afterwards.
//
// No Qt here: the paths, the checks, the choice of the installer file, the download and the run's
// settings. The dialogs and the run of the installer are citra_qt/frame_ctgp7.cpp.

#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Mods::Transfer {
class Stopper;
}

namespace Ctgp7 {

/// The CTGP-7 launcher, the title the installer installs (and Launch boots).
constexpr u64 kLauncherTitleId = 0x0004000003070C00ULL;
/// The official download page: gamebanana.com/mods/50221, uploaded by PabloMK7 (ctgp-7.github.io
/// links to it).
constexpr u64 kGameBananaMod = 50221;
/// The offline installer's file on that page ("ctgp-7_cia_offline_installer.zip").
constexpr std::string_view kInstallerPattern = "ctgp-7_cia_offline_installer*.zip";
/// The official website (the segment's Official Website button).
constexpr std::string_view kWebsite = "https://ctgp-7.github.io/";
/// The update's title version that is v1.2 (Mario Kart 7's December 2022 update, raw 3072; 1.1 is
/// 1040). Any later version passes too.
constexpr u16 kUpdateV12 = 3072;

enum class Region : u8 { None, Usa, Eur, Jpn, Kor };

/// The region of a Mario Kart 7 that CTGP-7 supports (USA, Europe, Japan, Korea), else None
/// (another title, or the Chinese and Taiwanese releases, which CTGP-7 does not support).
Region RegionOf(u64 title_id);
inline bool IsSupportedMarioKart7(u64 title_id) {
    return RegionOf(title_id) != Region::None;
}
/// "USA", "Europe", "Japan", "Korea" ("" None).
std::string_view RegionName(Region region);
/// The update title of a game (0004000E instead of 00040000).
constexpr u64 UpdateTitleOf(u64 title_id) {
    return (title_id & 0xFFFFFFFFULL) | 0x0004000E00000000ULL;
}

/// The segment's rule: it shows while Mario Kart 7 (a region CTGP-7 supports) is open, while the
/// CTGP-7 launcher runs (and Mario Kart 7 with CTGP-7 after it, the same title), and while an
/// install of CTGP-7 runs the installer (`installing`).
bool SegmentShown(u64 running_title_id, bool installing);

/// Whether the installed update's version is v1.2 or later (nullopt: no update installed).
bool UpdateReady(std::optional<u16> update_version);

/// What stops the install before it starts, in the order the player meets them.
enum class Blocker : u8 {
    None,
    NotMarioKart7, ///< the game open is not a Mario Kart 7 CTGP-7 supports
    Update,        ///< the v1.2 update is missing
    SaveData,      ///< Mario Kart 7 has never been run (no save data)
};
struct GameCheck {
    u64 title_id = 0;
    std::optional<u16> update_version; ///< the installed update's title version
    bool save_data = false;            ///< its save data exists
    bool installed = false;            ///< the game is an installed title
};
Blocker BlockerOf(const GameCheck& check);
/// Reads the emulated SD card (the user folder's sdmc) for the game.
GameCheck CheckGame(u64 title_id);

// ---- Where things are ----

struct Paths {
    std::string sdmc;         ///< the emulated SD card, ends with '/'
    std::string dir;          ///< sdmc/CTGP-7/
    std::string version_file; ///< sdmc/CTGP-7/config/version.bin (written last by the installer)
    std::string plugin;       ///< sdmc/CTGP-7/resources/CTGP-7.3gx
    std::string work;         ///< user/ctgp7/ (downloads, the converted game; emptied after)
    std::string zip;          ///< work/<the installer's zip>
    std::string unpacked;     ///< work/unpacked/
    std::string game_cia;     ///< work/mario-kart-7.cia (the converted game, deleted once installed)
};
/// `sdmc_dir` and `user_dir` end with '/'.
Paths PlanPaths(const std::string& sdmc_dir, const std::string& user_dir);
/// The paths of this user folder.
Paths DefaultPaths();

/// What is on the emulated SD card and in the titles, whoever put it there: this app's install,
/// one copied from another computer, or a real 3DS's SD card. Installed means the launcher title
/// and resources/CTGP-7.3gx are both there.
enum class Found : u8 {
    None,         ///< no plugin (nothing, or only bits: a bare launcher title, an empty folder)
    Installed,    ///< the launcher and the plugin, with the emulator's files (config/citra.flag)
    FinishSetup,  ///< the SD folder without the launcher title, and its own copy of the launcher
                  ///< (cia/CTGP-7.cia, the installer's): installing that CIA finishes it
    NeedsInstaller, ///< the SD folder, but no launcher to install from it, or the console's files
                    ///< only (no config/citra.flag: a real 3DS's card): the official installer
                    ///< runs over it, the folder's other files (MyStuff, progress) stay
};
struct Layout {
    bool plugin = false;          ///< resources/CTGP-7.3gx
    bool citra_flag = false;      ///< config/citra.flag (the installer's emulator variant)
    bool launcher_cia = false;    ///< cia/CTGP-7.cia
    bool launcher_title = false;  ///< 0004000003070C00 installed
};
Layout ReadLayout(const Paths& paths, bool launcher_title_installed);
Found Detect(const Layout& layout);
/// The disk's answer (ReadLayout with the launcher title looked up).
Found DetectInstall(const Paths& paths);
/// The launcher's own copy the installer keeps on the SD card.
std::string LauncherCia(const Paths& paths);

/// CTGP-7 is installed (DetectInstall: Installed).
bool ModInstalled(const Paths& paths);
/// "1.6.30" from version.bin ("" none).
std::string InstalledVersion(const Paths& paths);
/// The launcher's content to boot ("" when it is not installed).
std::string LauncherPath();
/// Whether a title is installed on the emulated SD card.
bool TitleInstalled(u64 title_id);
/// Bytes in sdmc/CTGP-7/.
u64 FolderBytes(const std::string& dir);
/// Bytes of the files in `dir` written at or after `since` (the installer's progress: it also
/// rewrites files of a copy already there, so the folder's size alone does not show it).
u64 BytesWrittenSince(const std::string& dir, std::filesystem::file_time_type since);
/// Whether `path` was written at or after `since`.
bool WrittenSince(const std::string& path, std::filesystem::file_time_type since);

// ---- Uninstall ----

/// What Uninstall removes and keeps, every path absolute. CTGP-7's own progress (sdmc/CTGP-7/savefs,
/// savefsCitra, and its backups sdmc/CTGP-7savebak*) stays unless `delete_progress`; Mario Kart
/// 7's save data (its title's data/ folder) and the player's game file are never in it; the Mario
/// Kart 7 title's content goes only with `remove_game` (its data/ folder stays even then).
struct UninstallPlan {
    std::vector<std::string> remove; ///< files and folders (folders end with '/')
    std::vector<std::string> keep;   ///< what stays inside a removed folder (CTGP-7's progress)
};
/// `title_root`: the SD card's title folder (".../Nintendo 3DS/<id0>/<id1>/title/"), `tickets`:
/// the ticket folder (nand .../dbs/ticket.db/), `game`: the Mario Kart 7 the app installed (0 none).
UninstallPlan PlanUninstall(const Paths& paths, const std::string& title_root,
                            const std::string& tickets, bool delete_progress, u64 game);
/// Carries it out: only paths under the CTGP-7 folders, the two titles' folders, the tickets of
/// those titles and the app's work folder are ever touched; anything else in the plan is skipped
/// (and logged). The kept progress folders are left in place. "" on success, else what failed.
std::string RunUninstall(const UninstallPlan& plan, const Paths& paths, const std::string& title_root,
                         const std::string& tickets, u64 game);
/// The SD card's title folder and the ticket folder of this user folder.
std::string TitleRoot();
std::string TicketFolder();
/// The Mario Kart 7 the app installed as a title, and the file it came from ({0, ""} none).
struct InstalledGame {
    u64 title_id = 0;
    std::string source;
};
InstalledGame ReadInstalledGame(const Paths& paths);
void WriteInstalledGame(const Paths& paths, const InstalledGame& game);

// ---- The installer file ----

struct InstallerFile {
    std::string name;
    std::string url; ///< https://gamebanana.com/dl/<id>
    std::string md5; ///< GameBanana's checksum of the file (lower case hex), "" none
    u64 size = 0;
};
/// From GameBanana's ProfilePage of mod 50221: the newest file matching kInstallerPattern that
/// GameBanana's scan found clean and that is not archived. nullopt (with `error`) when there is
/// none, or the mod is gone.
std::optional<InstallerFile> PickInstaller(std::string_view profile_json, std::string* error = nullptr);
/// A host the download may go to: gamebanana.com, files.gamebanana.com and filecacheN.gamebanana.com.
bool DownloadHostAllowed(std::string_view host_with_scheme);

/// One GET into a file (Mods::Multipart::StreamGet's shape), replaced by tests.
struct GetReply {
    int status = 0;
    std::string location;
    std::string error;
    u64 total = 0;
    u64 start = 0; ///< where the bytes written began (a 206 resumes)
};
using GetToFile = std::function<GetReply(const std::string& host, const std::string& path,
                                         const std::string& dest, u64 resume_from,
                                         const std::function<void(u64 done, u64 total)>& progress)>;
/// The real one: HTTPS with the system's certificates; `stopper` ends it at once on Cancel.
GetToFile RealGet(const std::atomic_bool* cancel, Mods::Transfer::Stopper* stopper);

struct FetchResult {
    std::string error; ///< "" ok, "Cancelled", or why
    std::string zip;   ///< the downloaded file
};
/// Asks GameBanana for the file, downloads it to `paths.zip`'s folder (a kept partial download is
/// resumed), follows redirects only among DownloadHostAllowed's hosts and checks its MD5.
FetchResult FetchInstaller(const Paths& paths, const GetToFile& get, const std::atomic_bool* cancel,
                           const std::function<void(u64 done, u64 total)>& progress);
/// Unpacks the zip into `paths.unpacked` and returns the installer (.3dsx) in it ("" with
/// `error`). The zip is deleted afterwards.
std::string UnpackInstaller(const Paths& paths, const std::string& zip, const std::atomic_bool* cancel,
                            std::string* error);
/// Lower-case MD5 of a file ("" when it cannot be read).
std::string FileMd5(const std::string& path);

/// Free space the whole install needs: the zip, the installer unpacked, what it writes to the SD
/// card (about the same again) and, when the game is converted, the CIA and its installed copy.
u64 RequiredBytes(u64 zip_bytes, u64 game_bytes);
/// The installer's sizes as GameBanana gives them, for before the file is known.
constexpr u64 kInstallerZipBytes = 805ULL * 1024 * 1024;

// ---- The launcher's run ----

/// The plugin loader on for one run of the CTGP-7 launcher (and Mario Kart 7 after it), the player's
/// own setting left as it is. Settings::values.plugin_loader_enabled is a global setting with no
/// per-game value, and the launcher's plg:ldr call also writes it; so the player's value is kept
/// here, every config save writes it (ValueToSave) and End puts it back. Nothing is written to the
/// config for this, so an older version reading the same config.ini sees the player's own value.
class RunOverride {
public:
    /// Turns the loader (and "allow applications to change it") on, keeping the player's values.
    /// Again while active: nothing.
    static void Begin();
    /// The player's values back. Nothing when not active.
    static void End();
    static bool Active();
    /// What a config save writes for plugin_loader_enabled / allow_plugin_loader: the player's own
    /// value while active, else `current`.
    static bool LoaderToSave(bool current);
    static bool AllowToSave(bool current);
};

} // namespace Ctgp7
