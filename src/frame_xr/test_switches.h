// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame integ9: the test switches while running (common/frame_test_switches.h reads the
// file again once a second). Which ones apply at once, the log line and the toast for a change,
// and the values of the switches made for the headset A/B. Pure (unit-tested).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/frame_test_switches.h"

namespace FrameXr::TestSwitches {

enum class Apply : uint8_t { Live, Restart };

/// The switches applied as they change; every other one is read only at the start (it sizes or
/// makes something once: the session, the screen's place, a thread, a trace) and needs a restart.
inline Apply ApplyFor(std::string_view name) {
    constexpr std::string_view kLive[] = {
        "AZAHARFRAME_IMMERSIVE_ANCHOR",         "AZAHARFRAME_IMMERSIVE_ANCHOR_MAX_MS",
        "AZAHARFRAME_IMMERSIVE_ANCHOR_EYE",     "AZAHARFRAME_IMMERSIVE_ANCHOR_DEPTH",
        "AZAHARFRAME_IMMERSIVE_FOV_TRACE",      "AZAHARFRAME_IMMERSIVE_MENU_EXACT",
        "AZAHARFRAME_IMMERSIVE_LOCAL",          "AZAHARFRAME_IMMERSIVE_WORLD_TAG",
        "AZAHARFRAME_IMMERSIVE_TAG_OFFSET",     "AZAHARFRAME_IMMERSIVE_POSITION_SCALE",
        "AZAHARFRAME_XR_SINGLE_FLASH",          "AZAHARFRAME_XR_REFRESH_RATE",
        "AZAHARFRAME_IMMERSIVE_HOLD",           "AZAHARFRAME_XR_IMAGE_LAG",
    };
    return std::find(std::begin(kLive), std::end(kLive), name) != std::end(kLive) ? Apply::Live
                                                                                  : Apply::Restart;
}

/// The log line for a change (after "xr: "), e.g. "test switch NAME=value (live)".
inline std::string LogLine(const Common::FrameTestSwitches::Change& c) {
    if (c.env_wins) {
        return "test switch " + c.name + " changed in the file, ignored: set in the environment";
    }
    const std::string what = c.removed ? c.name + " removed (its default)" : c.name + "=" + c.value;
    return "test switch " + what +
           (ApplyFor(c.name) == Apply::Live ? " (live)" : " (restart needed)");
}

/// One toast for the changes of one read ("" when none shows): "Test Switch: NAME=value", two
/// joined with " · " (a toast has two lines at most), more than two counted.
inline std::string ToastText(const std::vector<Common::FrameTestSwitches::Change>& changes) {
    std::vector<std::string> parts;
    for (const auto& c : changes) {
        if (c.env_wins) {
            continue;
        }
        std::string p = c.removed ? c.name + " Cleared" : c.name + "=" + c.value;
        if (ApplyFor(c.name) == Apply::Restart) {
            p += " (Restart to Apply)";
        }
        parts.push_back(std::move(p));
    }
    if (parts.empty()) {
        return {};
    }
    if (parts.size() > 2) {
        return "Test Switches: " + std::to_string(parts.size()) + " Changed";
    }
    std::string out = parts.size() == 1 ? "Test Switch: " : "Test Switches: ";
    for (std::size_t i = 0; i < parts.size(); i++) {
        out += (i ? " · " : "") + parts[i];
    }
    return out;
}

/// AZAHARFRAME_IMMERSIVE_TAG_OFFSET: -1, 0 or +1 (anything else, or unset: 0).
inline int TagOffsetFrom(const std::optional<std::string>& v) {
    if (!v) {
        return 0;
    }
    if (*v == "-1") {
        return -1;
    }
    if (*v == "+1" || *v == "1") {
        return 1;
    }
    return 0;
}

/// AZAHARFRAME_IMMERSIVE_POSITION_SCALE: the head position's share in the game camera, 0 to 10
/// (unset or not a number: 1, as without it).
constexpr float kPositionScaleMax = 10.0f;
inline float PositionScaleFrom(const std::optional<std::string>& v) {
    if (!v || v->empty()) {
        return 1.0f;
    }
    char* end = nullptr;
    const float f = std::strtof(v->c_str(), &end);
    if (end == v->c_str() || !std::isfinite(f)) {
        return 1.0f;
    }
    return std::clamp(f, 0.0f, kPositionScaleMax);
}

/// AzaharFrame heldquad: AZAHARFRAME_IMMERSIVE_HOLD: how a picture shown on a second display
/// refresh is held: "warp" (the default: as the game would draw it for the head now, through the
/// screen's gains, Layout::HeldQuad) or "rigid" (fixed in the room where it was drawn, as up to
/// 1.2). Anything else, or unset: warp.
inline bool HoldWarpFrom(const std::optional<std::string>& v) {
    return !(v && *v == "rigid");
}

// AzaharFrame imglag: AZAHARFRAME_XR_IMAGE_LAG=0|1 (live) replaces the A/B switch
// AZAHARFRAME_IMMERSIVE_ANCHOR_LAG; its value is read by ImageLag::OverrideFrom (image_lag.h).

/// A switch that is on with "1" (AZAHARFRAME_XR_SINGLE_FLASH).
inline bool OnFrom(const std::optional<std::string>& v) {
    return v && !v->empty() && (*v)[0] == '1';
}

/// AZAHARFRAME_XR_SINGLE_FLASH (lane dbl): each top screen picture on one display refresh only.
/// Hide: this refresh shows the same picture again, so its layer is left out. Follows the
/// pictures while off too, so turning it on live never hides a new one.
class SingleFlash {
public:
    bool Hide(bool on, bool has_image, uint64_t serial) {
        if (!has_image) {
            return false;
        }
        const bool again = serial == flashed_;
        flashed_ = serial;
        return on && again;
    }

private:
    uint64_t flashed_ = 0;
};

} // namespace FrameXr::TestSwitches
