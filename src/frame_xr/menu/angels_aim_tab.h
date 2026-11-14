// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiufix: the Mods tab's Angel's Aim segment (Cheats | Mods | Textures | Angel's
// Aim), shown only while Kid Icarus: Uprising (any region) is open; built like the CTGP-7 segment
// (ctgp7_tab.h). Material 3, one page that scrolls as a whole:
//
//   About Angel's Aim  [Alpha]                                       (title, an assist chip)
//   Alpha Pre-Release · Very Experimental · Have Fun!                (the owner's words)
//   Angel's Aim brings the game into VR: ...                         (short paragraphs)
//   [↗ Report an Issue]
//   Bug reports and feature ideas welcome.
//   ─────────────────────────────────────────────────────────────
//   Required                                                         (title)
//   Darkshade's C-Stick & Gyro Mod                     Installed / Not Installed
//   Then set the game's own Controls to gyro.
//   (the catalog's C-Stick & Gyro rows, as Install Mods draws them: Install, its versions)
//   ─────────────────────────────────────────────────────────────
//   Suggested                                                        (title)
//   Weapon Textures                                    Installed / Not Installed
//   Effect Textures                                    Installed / Not Installed
//   ─────────────────────────────────────────────────────────────
//   Settings                                                         (title)
//   [Aiming Settings]   (the Controls tab, scrolled to Aiming: no second copy of the rows)
//
// The values come from the Qt side (citra_qt/frame_mods.cpp, FrameXr::SetInGameMenuAngelsAim).
// Report an Issue asks "Open GitHub?" first, then sends OpenUrl with `report_url`: the app's
// GitHub issues page for a new issue, prefilled for Angel's Aim with the app's version, the
// game's region and version, and whether the mod is installed (nothing personal).

#pragma once

#include <string>
#include <vector>

#include "frame_xr/menu/mods_segments.h"

namespace FrameXr::Menu {

struct AngelsAimModel {
    bool shown = false;         ///< Kid Icarus: Uprising is open
    bool mod_installed = false; ///< a C-Stick & Gyro mod (FrameXr::CStickModInstalled)
    bool weapons_pack = false;  ///< the weapon texture pack's folder is there
    bool effects_pack = false;  ///< the effects texture pack's folder is there
    std::string report_url;     ///< Report an Issue's page ("" none)
    bool operator==(const AngelsAimModel&) const = default;
};

/// The segment's strings (the owner approves them: handoff/reports/52/kiufix).
inline constexpr const char* kAngelsAimSegment = "Angel's Aim";
inline constexpr const char* kAngelsAimTitle = "About Angel's Aim";
inline constexpr const char* kAngelsAimAlpha = "Alpha";
inline constexpr const char* kAngelsAimStatus = "Alpha Pre-Release \xC2\xB7 Very Experimental \xC2\xB7 Have Fun!";
inline constexpr const char* kAngelsAimAbout1 =
    "Angel's Aim brings Kid Icarus: Uprising into VR. Aim with your controller: Direct Aim puts "
    "the reticle where you point, Gyro Aim moves it as you turn your hand. Your equipped weapon "
    "is in your hand, and each real shot buzzes.";
inline constexpr const char* kAngelsAimAbout2 =
    "It's the first step of a bigger project: full 6DOF aiming.";
inline constexpr const char* kAngelsAimReportLine = "Bug reports and feature ideas welcome.";
inline constexpr const char* kAngelsAimRequired = "Required";
inline constexpr const char* kAngelsAimModName = "Darkshade's C-Stick & Gyro Mod";
inline constexpr const char* kAngelsAimGyroReminder = "Then Set the Game's Own Controls to Gyro";
inline constexpr const char* kAngelsAimSuggested = "Suggested";
inline constexpr const char* kAngelsAimWeaponsPack = "Weapon Textures";
inline constexpr const char* kAngelsAimEffectsPack = "Effect Textures";
inline constexpr const char* kAngelsAimSettings = "Settings";
inline constexpr const char* kAngelsAimInstalled = "Installed";
inline constexpr const char* kAngelsAimNotInstalled = "Not Installed";
/// Send Report's dialog (citra_qt/frame_crash_dialog.cpp, in Azahar's window with the VR keyboard).
inline constexpr const char* kAngelsAimSendTitle = "Send Report";
inline constexpr const char* kAngelsAimBug = "Bug";
inline constexpr const char* kAngelsAimIdea = "Idea";
inline constexpr const char* kAngelsAimBugNote = "What Happened? (Optional)";
inline constexpr const char* kAngelsAimIdeaNote = "What's Your Idea? (Optional)";
inline constexpr const char* kAngelsAimConsent =
    "Sends this report and your Angel's Aim settings to the developer.";
inline constexpr const char* kAngelsAimSending = "Sending...";
inline constexpr const char* kAngelsAimSent = "Report Sent";
inline constexpr const char* kAngelsAimNotSent = "Couldn't Send Report";
inline constexpr const char* kAngelsAimNotSentHint = "Try Again, or Use Report an Issue.";
/// Report an Issue's page: a new issue, titled and labelled for Angel's Aim (GitHub ignores a
/// label the repository does not have). The body is added by the Qt side (AngelsAimReportUrl).
inline constexpr const char* kAngelsAimIssuesUrl =
    "https://github.com/simplysanatori/AzaharFrame/issues/new?title=%5BAngel%27s%20Aim%5D%20&labels=Angel%27s%20Aim";

/// The page with a prefilled body (percent-encoded; kept short, as the menu's links are at most
/// 300 characters): the app's version, the game's region and version, the mod's state.
std::string AngelsAimReportUrl(const std::string& app_version, const std::string& game,
                               bool mod_installed);
/// A URL is Report an Issue's page (its prefix): the "Open GitHub?" dialog names github.com.
bool IsAngelsAimReportUrl(const std::string& url);

/// The C-Stick mods' catalogue ids (FrameXr::kCStickModIds, controls_presets.h; the menu library
/// does not link the emulator's: a unit test keeps the two the same).
inline constexpr const char* kAngelsAimModIds[] = {"kiu-cstick-gyro",
                                                   "kiu-cstick-gyro-widescreen-16-9",
                                                   "kiu-cstick-gyro-ultrawide-21-9",
                                                   "kiu-cstick-aim"};
/// The catalog rows (indices of `available`) the Required section lists: the C-Stick mods.
std::vector<int> AngelsAimModRows(const ModsListModel& m);

} // namespace FrameXr::Menu
