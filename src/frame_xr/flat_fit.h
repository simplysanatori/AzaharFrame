// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flatdialog: the size of a window Azahar opens in Flat (a dialog, a message box,
// Configure, a game's Properties). gamescope shows the newest window of the app as the picture,
// scaled to fit SteamVR's 16:9 theater screen; a dialog of another shape filled only part of it
// (a tall Configure stood narrow between black bars), and one taller than the screen lost its
// OK and Cancel buttons under the bottom edge. So in Flat every such window gets a 16:9 size
// inside the main window's: the theater shows it whole, the text no smaller than the main
// window's. Pure arithmetic, no Qt (unit-tested in src/tests/frame_xr/flat_fit.cpp).

#pragma once

#include <algorithm>
#include <cmath>

namespace FrameXr {

struct WindowSize {
    int width = 0;
    int height = 0;
};

/// The smallest part of the area a window takes, as a share of its width and height: a small
/// dialog is not stretched across the whole screen, nor shown huge.
constexpr double kFlatDialogMinShare = 0.5;

struct FlatDialogFit {
    WindowSize size;
    /// The window's minimum size does not fit the area (its content needs a scroll area).
    bool too_small = false;
};

/// The largest 16:9 size inside `window` (an even width, like the main window's in Flat).
inline WindowSize FlatArea(WindowSize window) {
    if (window.width <= 0 || window.height <= 0) {
        return {};
    }
    int width = window.width;
    int height = window.height;
    if (static_cast<long long>(width) * 9 > static_cast<long long>(height) * 16) {
        width = 2 * static_cast<int>(std::lround(height * 16.0 / 9.0 / 2.0));
    } else {
        height = static_cast<int>(std::lround(width * 9.0 / 16.0));
    }
    return {std::min(width, window.width), std::min(height, window.height)};
}

/// The size for a window of size `current` with the given minimum and maximum sizes, shown over
/// a main window of size `window`: 16:9, large enough for its current size, at least
/// kFlatDialogMinShare of the area and at most all of it. A maximum smaller than that (a message
/// box has a fixed size) is kept, and so is a minimum larger than the area (`too_small`).
inline FlatDialogFit FitFlatDialog(WindowSize window, WindowSize current, WindowSize minimum,
                                   WindowSize maximum) {
    FlatDialogFit fit;
    const WindowSize area = FlatArea(window);
    if (area.width <= 0 || area.height <= 0) {
        fit.size = current;
        return fit;
    }
    const int need_w = std::max(current.width, minimum.width);
    const int need_h = std::max(current.height, minimum.height);
    const double share =
        std::clamp(std::max(static_cast<double>(need_w) / area.width,
                            static_cast<double>(need_h) / area.height),
                   kFlatDialogMinShare, 1.0);
    WindowSize size{2 * static_cast<int>(std::lround(area.width * share / 2.0)),
                    static_cast<int>(std::lround(area.height * share))};
    size.width = std::min(size.width, area.width);
    size.height = std::min(size.height, area.height);
    if (maximum.width > 0) {
        size.width = std::min(size.width, maximum.width);
    }
    if (maximum.height > 0) {
        size.height = std::min(size.height, maximum.height);
    }
    fit.too_small = minimum.width > size.width || minimum.height > size.height;
    size.width = std::max(size.width, minimum.width);
    size.height = std::max(size.height, minimum.height);
    fit.size = size;
    return fit;
}

} // namespace FrameXr
