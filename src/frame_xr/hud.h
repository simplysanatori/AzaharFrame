// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Internal: the small things the frame loop adds on top of the screens.
//  - Passthrough: the "Background" setting (Settings::values.vr_background). The runtime shows
//    the room only when the frame is ended with XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND and holds
//    a projection layer; this is a 16x16 fully transparent one, drawn behind every quad.
//    Both live in one swapchain (SteamVR allows 16 per session).
//  - A toast: a short message ("Quick Saved") on a small quad in front of you for two seconds.
//    The messages are drawn once into one swapchain when the session is made, so showing
//    one needs no GPU work (the XR thread does none while a game runs).
//    "Fast Forward" and "Normal Speed" are toasts too (fast forward turned on or off).
//    AzaharFrame vrsize: a toast of any text ("Offset Right 1.5°", ShowToastText) is drawn into
//    one of two spare rows of the atlas when asked for, and the swapchain copy never waits for
//    the GPU (vis/async_overlay.h); it shows once that copy is done, until then the toast before.

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/ui/ui_canvas.h"
#include "frame_xr/vis/async_overlay.h"

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
    /// Any thread. One line of UTF-8; "°" is drawn as a small ring (the font has none).
    void ShowToastText(const std::string& text);
    /// Any thread. AzaharFrame controls: takes a text toast back before its time, if `text` is
    /// the one showing or waiting ("Stopping..." when the View hold is let go).
    void HideToastText(const std::string& text);
    /// Any thread. AzaharFrame controls: the grip hint chip, up to two lines ('\n'; "↕" and "↔"
    /// are drawn as arrows), at `pose` (LOCAL space), `width_m` wide, for three seconds.
    void ShowHint(const std::string& text, const XrPosef& pose, float width_m);
    /// XR thread, each frame: the toast quad while one shows. `time` is the display time.
    bool ToastQuad(const ViewPose& head, XrTime time, Quad& quad);
    /// XR thread, each frame after ToastQuad: the hint chip while it shows.
    bool HintQuad(XrTime time, Quad& quad);

private:
    /// The toasts and the passthrough layer's transparent square.
    Vis::AsyncOverlay toast_chain;
    frame_xr::ui::Canvas atlas; ///< its pixels (the text rows are redrawn)
    bool logged_unsupported = false;

    std::atomic<int> toast_kind{-1}; ///< a request not yet taken by the XR thread
    int shown_kind = -1; ///< the atlas row on view, -1 none
    XrTime shown_since = 0;
    XrPosef shown_pose{{0, 0, 0, 1}, {0, 0, 0}};

    std::mutex text_mutex;
    std::string text_request; ///< ShowToastText not yet taken (text_requested)
    bool text_requested = false;
    int text_row_waiting = -1; ///< a text row uploaded, shown once the copy is done
    std::string waiting_text;  ///< its text
    std::string shown_text;    ///< the text row's text on view
    std::string hide_request;  ///< HideToastText not yet taken
    bool hide_requested = false;
    // The hint chip (ShowHint): requested under text_mutex, then on the XR thread.
    std::string hint_request;
    XrPosef hint_request_pose{{0, 0, 0, 1}, {0, 0, 0}};
    float hint_request_width = 0.4f;
    bool hint_requested = false;
    bool hint_waiting = false; ///< uploaded, shown once the copy is done
    bool hint_shown = false;
    XrTime hint_since = 0;
    XrPosef hint_pose{{0, 0, 0, 1}, {0, 0, 0}};
    float hint_width = 0.4f;
};

} // namespace FrameXr
