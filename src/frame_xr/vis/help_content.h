// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// What the controls help panel says: one row per Frame control, worked out from the controls
// profile in use (the 3DS buttons bound to frame_xr controls), the Face Buttons setting and the
// fixed rules of the mapper, the home panel and the in-game menu (holds, chords, grips). No
// OpenXR, Vulkan or Qt here, so it is unit tested (src/tests/frame_xr/vis_help.cpp).

#pragma once

#include <array>
#include <string>
#include <vector>

#include "common/settings.h"

namespace FrameXr::Vis {

/// The 3DS side of the controls profile: Settings::InputProfile::buttons and ::analogs, as the
/// Qt side copied them (param strings such as "engine:frame_xr,button:0").
struct HelpBindings {
    std::array<std::string, Settings::NativeButton::NumButtons> buttons;
    std::array<std::string, Settings::NativeAnalog::NumAnalogs> analogs;
    std::string profile_name;
    bool valid = false; ///< false: the default profile is shown
};

/// Where the player is, for the rows and the "Now" mark.
struct HelpContext {
    bool face_by_position = true; ///< Face Buttons: By Position (Nintendo)
    bool in_menu = false;         ///< the in-game menu is open
    bool game_running = false;
};

struct HelpRow {
    enum class Kind { Row, Heading };
    Kind kind = Kind::Row;
    std::string control; ///< Heading: the heading's text
    std::string action;
    bool now = false; ///< Heading: what the controls do right now
};

struct HelpPage {
    std::vector<HelpRow> game;   ///< "In Game"
    std::vector<HelpRow> window; ///< "Azahar's Window", the in-game menu, everywhere
    std::string footer;          ///< the Face Buttons setting and the profile
};

/// The bindings of the default VR profile (frame_xr/input/default_bindings.h).
HelpBindings DefaultHelpBindings();

HelpPage BuildHelp(const HelpBindings& bindings, const HelpContext& context);

} // namespace FrameXr::Vis
