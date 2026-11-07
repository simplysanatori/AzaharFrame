// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: while Azahar for Frame runs flat (its window, no VR session; frame_xr), games see
// the 3D slider at 0 so they draw one eye. The 3D setting itself is left alone (never saved).
// Its own header so that setting it does not touch common/settings.h. The flat screen layout
// below is here too.

#pragma once

#include <atomic>

#include "common/settings.h"

namespace Settings {
inline std::atomic<bool> frame_flat_mono{false};

// AzaharFrame: the window's own screen layout in flat mode (citra_qt.cpp: the Frame's stick
// clicks and View > Screen Layout while flat), never saved. While `active`, the window is laid
// out with it in place of layout_option and swap_screen, which stay as set for VR (where the
// window's layout places the touch screen) and for the configuration.
struct FrameFlatLayout {
    std::atomic<bool> active{false};
    std::atomic<LayoutOption> layout{LayoutOption::LargeScreen};
    std::atomic<bool> swap{false};
};
inline FrameFlatLayout frame_flat_layout;

/// The layout the window uses: flat's own while it is active, else layout_option.
inline LayoutOption WindowLayoutOption() {
    return frame_flat_layout.active ? frame_flat_layout.layout.load()
                                    : values.layout_option.GetValue();
}
/// Swap Screens as the window uses it: flat's own while it is active, else swap_screen.
inline bool WindowSwapScreen() {
    return frame_flat_layout.active ? frame_flat_layout.swap.load() : values.swap_screen.GetValue();
}
} // namespace Settings
