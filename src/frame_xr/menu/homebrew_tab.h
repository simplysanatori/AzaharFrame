// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: the in-game menu's Homebrew tab (a segment of the Mods tab until ui3). One
// row per homebrew package (core/homebrew/packages.h): its title and author, and Install, or Play
// with Update (a newer release was seen) or Reinstall (AzaharFrame ui3: none was). The values come
// from the Qt side (citra_qt/frame_homebrew.cpp, FrameXr::SetInGameMenuHomebrew); a button sends
// MenuCommand::HomebrewInstall (Install, Reinstall) / HomebrewUpdate / HomebrewPlay with the
// package's id, and Install and Update continue in Azahar's window (the ROM and folder dialogs).
// AzaharFrame emhb: an install found from its files whoever made it; Repair when some of it is
// missing, and Uninstall (MenuCommand::HomebrewUninstall) with its dialog over the menu, whose
// state is the Qt side's (HomebrewTabModel::dialog).
//
// The hooks: Tab::Homebrew and Id::Homebrew* in tab_menu.h, TabMenu::DrawHomebrew
// (homebrew_tab.cpp), the model's copy and the clicks in menu_panel.cpp, the commands in
// citra_qt.cpp.

#pragma once

#include <string>
#include <vector>

namespace FrameXr::Menu {

enum class Id : int;

struct HomebrewItem {
    std::string id;     ///< the package's id ("emerald3ds")
    std::string title;  ///< "Pokémon Emerald 3DS"
    std::string author; ///< "ZallaxDev"
    bool installed = false;
    bool update = false; ///< a newer release was seen
    bool busy = false;   ///< installing or updating
    int progress = -1;   ///< 0 to 1000 while busy, -1 when unknown
    std::string status;  ///< while busy ("Downloading"), or the last error
    // AzaharFrame ui3: Check for Updates
    bool checking = false; ///< a check asks GitHub: the button's ring
    std::string version;   ///< the installed release ("v0.5.0"), "" unknown
    std::string latest;    ///< with `update`: the newer release ("v0.6.0")
    std::string checked;   ///< "Checked Just Now" (Homebrew::CheckedText), "" never
    // AzaharFrame emhb
    bool repair = false;        ///< some of it is there but it cannot be played: Repair (Install)
    bool can_uninstall = false; ///< anything of it is there: Uninstall
    bool operator==(const HomebrewItem&) const = default;
};

/// AzaharFrame emhb: Uninstall's dialog (Material 3 basic dialog over the whole menu): a headline,
/// the supporting text, an optional checkbox, then text buttons, the confirming one last. Its
/// buttons send MenuCommand::HomebrewUninstall with `id`: Id::HomebrewDialogCheck 1, Cancel 2,
/// the confirming one 3.
struct HomebrewDialog {
    std::string id; ///< the package's id ("" no dialog)
    std::string title;
    std::string body;
    std::string check; ///< the checkbox's label ("" none): "Also Delete Save Data"
    bool checked = false;
    std::string confirm; ///< "Uninstall"
    std::string cancel;  ///< "Cancel" ("" none)
    bool operator==(const HomebrewDialog&) const = default;
};

struct HomebrewTabModel {
    std::vector<HomebrewItem> items;
    int spin = 0; ///< the progress ring's step (menu_panel.cpp)
    HomebrewDialog dialog; ///< AzaharFrame emhb
    bool operator==(const HomebrewTabModel&) const = default;
};

/// AzaharFrame ui3: a row's buttons, right to left (none while busy): Install; or Play, then
/// Update when a newer release was seen, else Check for Updates and the quieter Reinstall (a text
/// button: a repair, not the main path). AzaharFrame emhb: then Uninstall (a text button) whenever
/// anything is there; a broken install has Repair (Id::HomebrewInstall) and Uninstall.
std::vector<Id> HomebrewButtons(const HomebrewItem& item);
/// AzaharFrame ui3: the row's second line: the author, the installed release and, after a check,
/// the newer release or when it was checked ("ZallaxDev · English · v0.5.0 · Checked Just Now",
/// "ZallaxDev · English · v0.5.0 · v0.6.0 Available"); the phase with its percentage while busy
/// ("Downloading 42%"), or the last error.
std::string HomebrewLine(const HomebrewItem& item);
/// AzaharFrame ui3: the Check for Updates button's label: "Checking" while it asks.
std::string HomebrewCheckLabel(const HomebrewItem& item);
/// AzaharFrame emhb: a row button's label: "Repair" for Install on a broken install, "Uninstall",
/// else LabelFor(id).
std::string HomebrewButtonLabel(const HomebrewItem& item, Id id);

} // namespace FrameXr::Menu
