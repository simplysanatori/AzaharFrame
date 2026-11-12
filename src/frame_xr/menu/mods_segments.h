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
    bool operator==(const ModOffer&) const = default;
};

enum class CatalogState : uint8_t { Idle, Loading, Ready };

/// The question over the panel, when there is one.
enum class ModsDialogKind : uint8_t {
    None,
    Replace, ///< "Install <mod>?", "Replaces <other>": Cancel, Replace
    Remove,  ///< your own files: "Remove <mod>?", "Its Files Are Deleted": Cancel, Remove
    Blocked, ///< a conflict with your own files: "Remove <other> First": OK
    OpenLink, ///< "Open GitHub?", "This opens <other> in your browser...": Cancel, Open
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
    int spin = 0;               ///< the progress ring's step (15 a second) while anything runs
    bool operator==(const ModsListModel&) const = default;
};

/// One installed pack for the running game (CustomTex::Pack).
struct TexturePackItem {
    std::string id;     ///< CustomTex::Pack::id (the command's `text`)
    std::string name;   ///< "4K", "1080p", "4K (PNG)" or the pack.json name
    std::string detail; ///< one line: "Henriko Magnifico · 6.0.0 · ASTC/KTX · 2.1 GB" ("" = none)
    bool active = false;
    bool operator==(const TexturePackItem&) const = default;
};

struct TexturesModel {
    std::vector<TexturePackItem> packs; ///< in the order shown; none active = Off
    bool custom_textures = false;       ///< Settings::values.custom_textures
    bool preload = false;               ///< Settings::values.preload_textures
    bool busy = false;                  ///< a switch of packs runs
    bool needs_restart = false;         ///< the snackbar
    bool operator==(const TexturesModel&) const = default;
};

/// "github.com/owner/repo" for a link the menu may open: https://github.com/<owner>/<repo> only
/// (letters, digits, '-', '_', '.'); "" for anything else.
std::string GithubLinkText(const std::string& url);

/// "a · b · c" of the parts that are not empty.
std::string JoinDetail(const std::vector<std::string>& parts);

/// "2.1 GB", "640 MB", "12 KB": a size the way Android's file manager shows it (1000s).
std::string SizeText(uint64_t bytes);

} // namespace FrameXr::Menu
