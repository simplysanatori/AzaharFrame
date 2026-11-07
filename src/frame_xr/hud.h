// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Internal: the small things the frame loop adds on top of the screens.
//  - Passthrough: the "Background" setting (Settings::values.vr_background). The runtime shows
//    the room only when the frame is ended with XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND and holds
//    a projection layer; this is a 16x16 fully transparent one, drawn behind every quad.
//    Both live in one swapchain (SteamVR allows 16 per session).
//  - A toast: a short message ("Quick Saved") on a small quad in front of you for two seconds.
//    The three messages are drawn once into one swapchain when the session is made, so showing
//    one needs no GPU work (the XR thread does none while a game runs).

#pragma once

#include <atomic>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/overlays.h"

namespace FrameXr {

class Runtime;

class Hud {
public:
    /// XR thread, once the session exists (after the swapchain format is known). Makes the
    /// uploader and the two swapchains; either may fail on its own (the feature is then off).
    void Create(Runtime& rt);
    /// XR thread, before the session is destroyed.
    void Destroy();

    /// "Background: Passthrough" is set, the runtime offers alpha blending and the layer exists.
    /// Logs the first time it is asked for and cannot be shown.
    bool PassthroughActive(const Runtime& rt);
    /// Fills a transparent projection layer for this frame's eyes (`views` has two entries).
    /// False when the eyes are not known yet.
    bool PassthroughLayer(const ViewPose& pose, XrSpace space, XrCompositionLayerProjection& layer,
                          XrCompositionLayerProjectionView (&views)[2]) const;

    /// Any thread.
    void ShowToast(Toast kind);
    /// XR thread, each frame: the toast quad while one shows. `time` is the display time.
    bool ToastQuad(const ViewPose& head, XrTime time, Quad& quad);

private:
    OverlayUploader uploader;
    OverlayChain toast_chain; ///< the toasts and the passthrough layer's transparent square
    bool logged_unsupported = false;

    std::atomic<int> toast_kind{-1}; ///< a request not yet taken by the XR thread
    int shown_kind = -1;
    XrTime shown_since = 0;
    XrPosef shown_pose{{0, 0, 0, 1}, {0, 0, 0}};
};

} // namespace FrameXr
