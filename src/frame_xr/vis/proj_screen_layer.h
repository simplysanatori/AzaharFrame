// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame projscreen: what app.cpp hands the controller models' pass for the top screen
// (proj_screen.h says why), and what it gets back.

#pragma once

#include <array>
#include <cstdint>

#include "frame_xr/xr_includes.h"

namespace FrameXr::Vis {

/// One eye's top screen for this refresh, as its quad would have been sent.
struct ProjScreenEye {
    bool shown = false;
    VkImage image = VK_NULL_HANDLE; ///< the swapchain image released last (ScreenChain::shown_index)
    uint32_t image_width = 0;       ///< the swapchain's size
    uint32_t image_height = 0;
    XrRect2Di rect{};               ///< the eye's part of it (packed: its half)
    XrPosef pose{{0, 0, 0, 1}, {0, 0, 0}}; ///< the quad's middle, LOCAL space
    XrExtent2Df size{0.0f, 0.0f};
    float curve = 0.0f;    ///< a cylinder's radius (0: flat), as MakeCylinder
    uint64_t serial = 0;   ///< the picture (ScreenChain::frame_serial)
};

struct ProjScreens {
    std::array<ProjScreenEye, 2> eye{};
    bool Any() const {
        return eye[0].shown || eye[1].shown;
    }
};

/// What FrameScreens gives app.cpp: the projection layer (or null), and whether the image it shows
/// has the top screen in it (else the top screen goes as quads this refresh).
struct ProjLayer {
    const XrCompositionLayerBaseHeader* layer = nullptr;
    bool has_screen = false;
};

} // namespace FrameXr::Vis
