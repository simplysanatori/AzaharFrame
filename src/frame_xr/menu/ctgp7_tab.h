// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctgp7: the Mods tab's CTGP-7 segment (Cheats | Mods | Textures | CTGP-7), shown only
// while Mario Kart 7 (a region CTGP-7 supports) is open, CTGP-7 runs, or its install runs
// (core/ctgp7/ctgp7.h SegmentShown). Material 3, labels only beyond the About section the owner
// asked for:
//
//   About CTGP-7                                                     (title)
//   CTGP-7 is a community mod pack for Mario Kart 7, ...             (two short paragraphs)
//   [↗ Official Website]
//   ─────────────────────────────────────────────────────────────
//   CTGP-7                                               [⬇ Install]   not installed
//   Not Installed  /  Needs Mario Kart 7 Version 1.2 (error colour)
//   CTGP-7                                               [▶ Launch]    installed
//   Version 1.6.30
//   Downloading CTGP-7                                    [Cancel]     installing
//   ███████████░░░░░░░░░░░░░░                                          (linear progress)
//   120 of 805 MB
//
// The values come from the Qt side (citra_qt/frame_ctgp7.cpp, FrameXr::SetInGameMenuCtgp7); the
// buttons send MenuCommand::Ctgp7Install / Ctgp7Launch / Ctgp7Cancel, Official Website asks
// "Open ctgp-7.github.io?" first (the link dialog) and sends OpenUrl. A question from the Qt side
// (Update Needed, ...) is `dialog` (a Notice), answered with Ctgp7Dialog.
//
// "Requires CTGP-7" (an Install Mods row of a "requires": "ctgp7" mod while CTGP-7 is not
// installed) is the menu's own dialog: Open CTGP-7 shows this segment.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "frame_xr/menu/mods_segments.h"

namespace FrameXr::Menu {

struct Ctgp7Model {
    bool shown = false;     ///< the segment shows
    bool installed = false; ///< Launch and Uninstall instead of Install
    /// CTGP-7's SD folder is there without the rest (copied from another computer or a 3DS):
    /// Finish Setup instead of Install, "Setup Not Finished"
    bool setup_unfinished = false;
    std::string version;    ///< "1.6.30" ("" unknown)
    bool busy = false;      ///< an install runs: its phase, the bar and Cancel
    std::string phase;      ///< "Downloading CTGP-7"
    std::string detail;     ///< "120 of 805 MB", "This Takes About 5 Minutes"
    int permille = -1;      ///< the bar: 0 to 1000, -1 none
    bool can_cancel = true; ///< Cancel shows while busy
    std::string error;      ///< why the last install stopped ("" none)
    std::string blocker;    ///< why Install can't go ahead yet ("Needs Mario Kart 7 Version 1.2")
    ModsDialog dialog;      ///< a Notice from the Qt side (Update Needed, ...)
    int spin = 0;
    bool operator==(const Ctgp7Model&) const = default;
};

/// The segment's strings (the owner approves them: handoff/reports/34/ctgp7/REPORT.md).
inline constexpr const char* kCtgp7Segment = "CTGP-7";
inline constexpr const char* kCtgp7AboutTitle = "About CTGP-7";
inline constexpr const char* kCtgp7About1 =
    "CTGP-7 is a community mod pack for Mario Kart 7, made by PabloMK7 and the CTGP-7 team. It "
    "runs as a plugin on top of your own Mario Kart 7.";
inline constexpr const char* kCtgp7About2 =
    "It adds 90+ custom tracks, 50+ custom characters, extra game modes, custom online servers and "
    "configurable gameplay changes, and keeps itself up to date.";
inline constexpr const char* kCtgp7Website = "https://ctgp-7.github.io/";
inline constexpr const char* kCtgp7NotInstalled = "Not Installed";
inline constexpr const char* kCtgp7SetupUnfinished = "Setup Not Finished";
inline constexpr const char* kCtgp7FinishSetup = "Finish Setup";
/// "Requires CTGP-7" (owner's wording, ui-direction.md)
inline constexpr const char* kRequiresCtgp7Title = "Requires CTGP-7";
inline constexpr const char* kRequiresCtgp7Body =
    "This mod works only inside CTGP-7. Azahar for Frame can install CTGP-7 for you in one click.";
inline constexpr const char* kRequiresCtgp7Open = "Open CTGP-7";

/// The status row's second line: the phase's detail while busy, else the error, the blocker, the
/// version ("Version 1.6.30"), or Not Installed. `error_colour` when it is a failure or a blocker.
std::string Ctgp7Line(const Ctgp7Model& m, bool* error_colour = nullptr);
/// The status row's buttons, right to left: Cancel while busy (when it can), else Launch and
/// Uninstall, or Install (Finish Setup).
std::vector<int> Ctgp7Buttons(const Ctgp7Model& m); ///< Menu::Id values

/// The segmented button's labels: Cheats, Mods, Textures, and CTGP-7 when `ctgp7_shown`.
std::vector<std::string> ModsSegmentLabels(bool ctgp7_shown);

} // namespace FrameXr::Menu
