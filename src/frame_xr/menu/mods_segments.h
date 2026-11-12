// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texui: what the Mods tab's Mods and Textures segments show (mods_tab.h); the values
// only. TabMenu::DrawModsList and TabMenu::DrawTextures (mods_segments.cpp) draw them.
//
// Mods: one flat list of what is loaded for the game (core/mods/library.h ListLoaded): the
// developer's own mods carry the verified badge after the name; a managed mod has a switch (on:
// in the mods folder, off: kept in the library) and every row a Remove. Install Mods opens the
// list of mods for this title (ListAvailable). A conflict asks before anything is replaced.
//
// Textures: Custom Textures, the game's packs (core/custom_tex/packs.h) as a single-select list
// with Off first, Preload.
//
// Both: a change applies from the next start, so a snackbar says "Restart the Game to Apply"
// with a Restart action. The Qt side (citra_qt/frame_mods.cpp) fills these; nothing here depends
// on core.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FrameXr::Menu {

/// The Mods segment's two pages.
enum class ModsPage : uint8_t { Loaded, Install };
/// The Textures segment's two pages.
enum class TexturesPage : uint8_t { Packs, Download };

/// One row of the loaded list (Mods::ModInfo).
struct ModRow {
    std::string id;     ///< the library's id (the commands' `text`)
    std::string name;
    std::string author; ///< "Created by <author>" ("" for your own files)
    std::string url;    ///< the author's repository, https://github.com/<owner>/<repo> ("" = no link)
    std::string detail; ///< after the author: "v1.5.9"; your own files: "214 Files"
    bool verified = false; ///< the developer's: the verified badge after the name
    bool managed = false;  ///< installed by the menu: has a switch (unmanaged: Remove only)
    bool active = true;    ///< in the mods folder (managed and off: in the library only)
    bool operator==(const ModRow&) const = default;
};

/// What an available mod's row ends with.
enum class OfferState : uint8_t {
    Install,     ///< the Install button
    Installing,  ///< a progress ring and the phase ("Downloading")
    Installed,   ///< in the mods folder
    Unavailable, ///< Install disabled, `note` says why ("Unsupported Archive")
};

/// One row of Install Mods (Mods::AvailableMod).
struct ModOffer {
    std::string id;
    std::string name;
    std::string author;
    std::string url; ///< as ModRow::url
    bool verified = false;
    OfferState state = OfferState::Install;
    std::string note;   ///< after the author: why it cannot install, the phase, or what failed
    bool error = false; ///< `note` is a failure (the error colour)
    std::string updated; ///< AzaharFrame modcat: "Updated Sep 2026" ("" unknown), as PackOffer
    std::string version;     ///< AzaharFrame modcat: the version chip ("1.0", "Card 1.1"), "" none
    std::string version_tip; ///< its tooltip
    bool version_match = false; ///< made for the game's own build: a filled chip, "Matches Your Game"
    std::string notes;       ///< AzaharFrame modcat2: the entry's short description (the row's tooltip)
    // AzaharFrame compat: made for another region, release or version of the game (core/mods/compat.h):
    // the "Not for Your Version" chip, a region chip, the reason (both chips' and the row's tooltip)
    // and Install asks "This May Not Work" with `mismatch_body` first.
    bool mismatch = false;
    std::string region_chip;     ///< "USA Only", "USA and Japan", "" none
    std::string mismatch_reason; ///< "Made for the USA version. Yours is the European version. ..."
    std::string mismatch_body;   ///< "This mod was made for the USA version of the game, ..."
    std::string downloads; ///< AzaharFrame compat: "10K+ Downloads" (DownloadsText), "" unknown
    /// AzaharFrame compat: GameBanana has not scanned its file yet (AvailableMod::unscanned, known
    /// after an install was refused for it): Install asks first
    bool unscanned = false;
    bool operator==(const ModOffer&) const = default;
};

enum class CatalogState : uint8_t { Idle, Loading, Ready };

/// The question over the panel, when there is one.
enum class ModsDialogKind : uint8_t {
    None,
    Replace, ///< "Install <mod>?", "Replaces <other>": Cancel, Replace
    Remove,  ///< your own files: "Remove <mod>?", "Its Files Are Deleted": Cancel, Remove
    Blocked, ///< a conflict with your own files: "Remove <other> First": OK
    OpenLink, ///< "Open GitHub?" (or "Open <host>?", `mod` = the host), "This opens <other> in
              ///< Steam's browser.": Cancel, Open
    BigDownload, ///< "Download <other>?" (the size), `mod` = the pack: Cancel, Download
    DeletePack,  ///< "Delete <mod>?", "You can download it again.": Cancel, Delete
    Mismatch,    ///< AzaharFrame compat: "This May Not Work", `mod` = the body, `other` = the
                 ///< confirming button ("Install Anyway", "Download Anyway", "Turn On Anyway"): Cancel
    Unscanned,   ///< AzaharFrame compat: "GameBanana Hasn't Scanned This File Yet", kUnscannedBody:
                 ///< Cancel, Download Anyway
};

struct ModsDialog {
    ModsDialogKind kind = ModsDialogKind::None;
    std::string mod;   ///< the mod being installed or removed
    std::string other; ///< the mod it replaces
    bool operator==(const ModsDialog&) const = default;
};

struct ModsListModel {
    std::vector<ModRow> loaded;      ///< ListLoaded's order: active, inactive, your own
    std::vector<ModOffer> available; ///< for the title, in the catalog's order
    CatalogState catalog = CatalogState::Idle;
    bool busy = false; ///< an install or remove runs: the other actions wait
    ModsDialog dialog;
    bool needs_restart = false; ///< the snackbar
    std::string notice; ///< AzaharFrame catalog3: the snackbar's text instead of "Restart the Game to Apply"
    int spin = 0;               ///< the progress ring's step (15 a second) while anything runs
    /// AzaharFrame compat: mods and packs turned off before this start because the game closed
    /// unexpectedly after they were installed (core/mods/crash_guard.h): the snackbar
    /// "Turned Off <names>: ..." with Turn Back On, on the Mods and Textures segments.
    std::vector<std::string> turned_off;
    /// Raised when `turned_off` is new: the menu shows the Mods tab's Mods segment once.
    int turned_off_serial = 0;
    bool operator==(const ModsListModel&) const = default;
};

/// One installed pack for the running game (CustomTex::Pack).
struct TexturePackItem {
    std::string id;     ///< CustomTex::Pack::id (the command's `text`)
    std::string name;   ///< "4K", "1080p", "4K (PNG)" or the pack.json name
    std::string detail; ///< one line: "Henriko Magnifico · 6.0.0 · ASTC/KTX · 2.1 GB" ("" = none)
    bool active = false;
    bool downloaded = false; ///< from Download Packs: it has Delete
    bool operator==(const TexturePackItem&) const = default;
};

/// What a downloadable pack's row ends with.
enum class PackOfferState : uint8_t {
    Download,    ///< the Download button
    Downloading, ///< a progress ring, the percent and Cancel
    Installed,   ///< in the pack list
    Failed,      ///< `note` is the error, the button is Try Again
};

/// One pack of Download Packs (CustomTex::Download::PackInfo).
struct PackOffer {
    std::string id;
    std::string name;
    std::string author;
    std::string url;       ///< the author's page (https; ModRow::url)
    std::string size_text; ///< "2.1 GB", "" unknown
    PackOfferState state = PackOfferState::Download;
    int percent = -1;      ///< Downloading: 0 to 100, -1 unknown
    std::string note;      ///< Downloading: the phase ("Unpacking"); Failed: why
    std::string updated;   ///< AzaharFrame zipinstall: "Updated Sep 2026" ("" unknown)
    bool verified = false; ///< AzaharFrame zipinstall: known to work: the verified badge
    std::string notes;     ///< AzaharFrame modcat2: the entry's short description (the row's tooltip)
    bool mismatch = false;       ///< AzaharFrame compat: as ModOffer (Download asks first)
    std::string region_chip;
    std::string mismatch_reason;
    std::string mismatch_body;
    std::string downloads; ///< AzaharFrame compat: "10K+ Downloads" (DownloadsText), "" unknown
    bool unscanned = false; ///< AzaharFrame compat: as ModOffer (PackInfo::unscanned)
    bool operator==(const PackOffer&) const = default;
};

struct TexturesModel {
    std::vector<TexturePackItem> packs; ///< in the order shown; none active = Off
    bool custom_textures = false;       ///< Settings::values.custom_textures
    bool preload = false;               ///< Settings::values.preload_textures
    bool busy = false;                  ///< a switch of packs runs
    bool needs_restart = false;         ///< the snackbar
    std::vector<PackOffer> downloads;   ///< Download Packs, for the title
    bool downloading = false;           ///< a download runs: the other Download buttons wait
    ModsDialog dialog;                  ///< BigDownload, DeletePack
    int spin = 0;                       ///< the progress ring's step while a download runs
    bool operator==(const TexturesModel&) const = default;
};

/// AzaharFrame zipinstall: Install from Zip, on the Textures page and on Install Mods. The page shows
/// over the segment it was opened from (`target`): in VR a file browser (quick chips for Downloads,
/// Documents and Home, the folder's subfolders and .zip / .7z files), in Flat only the progress (Qt's
/// file dialog picks the file); then the progress, a choice of game when the archive has several, and
/// the questions. The Qt side (citra_qt/frame_mods.cpp) fills it.
/// AzaharFrame ui2: Cheats: a cheat file (.txt, or a zip holding one), from the Cheats segment.
enum class ZipTarget : uint8_t { Textures, Mods, Cheats };

struct ZipEntry {
    std::string name;
    bool folder = false;
    std::string detail; ///< an archive's size ("1.2 GB")
    bool operator==(const ZipEntry&) const = default;
};

struct ZipChip {
    std::string label; ///< "Downloads", "Documents", "Home"
    std::string path;
    bool operator==(const ZipChip&) const = default;
};

struct ZipModel {
    bool open = false; ///< the page shows over its segment
    ZipTarget target = ZipTarget::Textures;
    bool browse = false;          ///< VR: the file browser
    std::string folder;           ///< the folder listed (absolute)
    std::string folder_text;      ///< as shown: "~/Downloads/Packs"
    bool can_go_up = false;       ///< the ".." row
    std::vector<ZipChip> chips;   ///< the quick folders that exist
    std::vector<ZipEntry> entries; ///< folders first, then archives, by name
    bool loading = false;         ///< the folder is being read
    bool working = false;         ///< unpacking or installing `file`
    std::string file;             ///< the archive's file name
    std::string phase;            ///< "Unpacking", "Installing"
    std::string error;            ///< why the last one was not installed ("Not a Texture Pack or Mod")
    std::vector<std::string> choices; ///< several games in the archive: their title IDs
    ModsDialog dialog;            ///< Replace / Blocked, over the page
    int spin = 0;                 ///< the progress ring's step
    bool operator==(const ZipModel&) const = default;
};

/// "github.com/owner/repo" for a link the menu may open: https://github.com/<owner>/<repo> only
/// (letters, digits, '-', '_', '.'); "" for anything else.
std::string GithubLinkText(const std::string& url);

/// "host/path" for a link the menu may open: an https address with a plain host and path; "" for
/// anything else. The Qt side opens only links it put in the model itself.
std::string LinkText(const std::string& url);
/// The host of such a link ("github.com"), "" when LinkText refuses it.
std::string LinkHost(const std::string& url);

/// AzaharFrame compat: the menu's own words for an entry made for another version.
constexpr const char* kNotForYourVersion = "Not for Your Version";
constexpr const char* kMayNotWorkTitle = "This May Not Work";
/// AzaharFrame compat: a GameBanana file whose virus scan has not finished.
constexpr const char* kUnscannedTitle = "GameBanana Hasn't Scanned This File Yet";
constexpr const char* kUnscannedBody =
    "GameBanana hasn't finished checking this file for viruses. Download it anyway?";
/// After a version reason, in one dialog with it.
constexpr const char* kUnscannedAlso = "GameBanana also hasn't finished checking this file for viruses.";
/// What InstallMod / DownloadPack's value says the player allowed (bits).
constexpr int kAnywayOtherVersion = 1;
constexpr int kAnywayUnscanned = 2;
/// "Turned Off No Bloom: The game closed unexpectedly after it was installed." (as
/// Mods::CrashGuard::TurnedOffText, which the Flat message box uses).
std::string TurnedOffText(const std::vector<std::string>& names);

/// AzaharFrame compat: lifetime downloads, roughly: "Under 100 Downloads", "100+ Downloads",
/// "1K+ Downloads", "5K+ Downloads", "10K+ Downloads", "50K+ Downloads".
std::string DownloadsText(uint64_t count);
/// "• a • b" of the parts that are not empty ("" when none is).
std::string BulletLine(const std::vector<std::string>& parts);

/// "a · b · c" of the parts that are not empty.
std::string JoinDetail(const std::vector<std::string>& parts);

/// "2.1 GB", "640 MB", "12 KB": a size the way Android's file manager shows it (1000s).
std::string SizeText(uint64_t bytes);

} // namespace FrameXr::Menu
