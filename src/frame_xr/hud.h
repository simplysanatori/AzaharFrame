// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Internal: the small things the frame loop adds on top of the screens.
//  - Passthrough: the "Background" setting (Settings::values.vr_background). The runtime shows
//    the room only when the frame is ended with XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND and holds
//    a projection layer; this is a 16x16 fully transparent one, drawn behind every quad.
//    AzaharFrame vis: only on frames without the controller models; their layer is transparent
//    round them and is then the one projection layer (vis/layer_plan.h: SteamVR shakes a second).
//    Both live in one swapchain (SteamVR allows 16 per session).
//  - A toast: a short message ("Quick Saved") on a small quad in front of you for two or five
//    seconds (AzaharFrame ui3: ToastTime, toast_fit.h), or until a newer toast replaces it.
//    The messages are drawn once into one swapchain when the session is made, so showing
//    one needs no GPU work (the XR thread does none while a game runs).
//    "Fast Forward" and "Normal Speed" are toasts too (fast forward turned on or off).
//    AzaharFrame vrsize: a toast of any text ("Offset Right 1.5°", ShowToastText) is drawn into
//    one of two spare rows of the atlas when asked for, and the swapchain copy never waits for
//    the GPU (vis/async_overlay.h); it shows once that copy is done, until then the toast before.
//  - AzaharFrame shaderbadge: the "Preparing Shaders 3 / 9" badge in the main screen's top-right
//    corner, or head-locked at the top right of what is in view (vis/shader_badge.h). Drawn into
//    the atlas at its opacities when its text changes (at most every kBadgeRedrawNs), with its own
//    copy of the menu's font (the menu's glyph caches belong to the menu's thread).

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/geometry.h" // AzaharFrame kiu2: the toast in front of the screens
#include "frame_xr/toast_depth.h" // AzaharFrame integ9: the toast's depth, eased
#include "frame_xr/toast_fit.h" // AzaharFrame menuux
#include "frame_xr/ui/ui_canvas.h"
#include "frame_xr/ui/ui_ttf.h" // AzaharFrame shaderbadge
#include "frame_xr/vis/async_overlay.h"
#include "frame_xr/vis/shader_badge.h" // AzaharFrame fixes7: Vis::Badge::Gate

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

    /// AzaharFrame flash: Motion Clarity's black backing (motion_clarity.h): the swapchain and
    /// rect of `quad` (an opaque black swapchain of its own, never shared, so the overlay SteamVR
    /// makes of the backing's layer never changes its texture); pose, size and eye are the
    /// caller's (the screen's). False until its image is released.
    bool BackingImage(Quad& quad) const;
    /// integ13: a stand-in for the toast, hint chip, Autosaved label and shader badge (they share
    /// one swapchain) before any of them shows, for their reserved layer slots (layer_slots.h
    /// Reserve): that swapchain, a small rect of it, 1 m square at the origin, to be collapsed by
    /// the caller. False until its image is released (a layer may not name it before then).
    bool HudPlaceholder(Quad& quad) const;

    /// Any thread.
    void ShowToast(Toast kind);
    /// Any thread. One line of UTF-8; "°" is drawn as a small ring (the font has none).
    void ShowToastText(const std::string& text, ToastTime time = ToastTime::Short);
    /// Any thread. AzaharFrame controls: takes a text toast back before its time, if `text` is
    /// the one showing or waiting ("Stopping..." when the View hold is let go).
    void HideToastText(const std::string& text);
    /// Any thread. AzaharFrame controls: the grip hint chip, up to two lines ('\n'; "↕" and "↔"
    /// are drawn as arrows), at `pose` (LOCAL space), `width_m` wide, for three seconds.
    void ShowHint(const std::string& text, const XrPosef& pose, float width_m);
    /// XR thread, each frame: the toast quad while one shows. `time` is the display time.
    /// AzaharFrame kiu2: `screens` (LOCAL space, as drawn this frame): the toast comes nearer
    /// than every one it is over (toast_depth.h).
    bool ToastQuad(const ViewPose& head, XrTime time, Quad& quad,
                   const std::vector<Input::PanelQuad>* screens = nullptr);
    /// XR thread, each frame after ToastQuad: the hint chip while it shows.
    bool HintQuad(XrTime time, Quad& quad, const ViewPose& head = {},
                  const std::vector<Input::PanelQuad>* screens = nullptr);
    /// AzaharFrame autosave: any thread. "Autosaved" over the top screen's corner (vis/
    /// autosave_label.h), else the Autosaved toast.
    void ShowAutosaved();
    /// XR thread, each frame: the label while it shows. `top_ok`: the top screen is the normal
    /// one, at `top_pose` (LOCAL space), `top_size` (not immersive, not swapped, not hidden).
    bool LabelQuad(const ViewPose& head, bool top_ok, const XrPosef& top_pose, XrExtent2Df top_size,
                   XrTime time, Quad& quad);

    /// AzaharFrame shaderbadge: any thread. The badge's line and its ring's progress (0 to 1);
    /// "" fades it out.
    void SetShaderBadge(const std::string& text, float progress);
    /// Where the badge may go this frame (app.cpp).
    struct BadgeAnchor {
        /// The normal main screen, in LOCAL space (`curve` above 0: a cylinder of that radius).
        bool screen = false;
        XrPosef screen_pose{{0, 0, 0, 1}, {0, 0, 0}};
        XrExtent2Df screen_size{0.0f, 0.0f};
        float curve = 0.0f;
        /// Else the immersive top screen, one pose per eye, in VIEW space or (false) LOCAL space.
        bool immersive = false;
        bool immersive_in_view = false;
        XrPosef immersive_pose[2]{{{0, 0, 0, 1}, {0, 0, 0}}, {{0, 0, 0, 1}, {0, 0, 0}}};
        XrExtent2Df immersive_size{0.0f, 0.0f};
        /// AzaharFrame fixes7: a game's screens are live, and the screen above is on view with a
        /// picture (Vis::Badge::Gate).
        bool live = false;
        bool pictures = false;
    };
    /// XR thread, each frame: the badge while it shows. `in_view_space`: the quad's pose is in
    /// VIEW space (head-locked), else LOCAL.
    bool BadgeQuad(const ViewPose& head, const BadgeAnchor& anchor, XrTime time, Quad& quad,
                   bool& in_view_space);

private:
    /// The toasts and the passthrough layer's transparent square.
    Vis::AsyncOverlay toast_chain;
    Vis::AsyncOverlay backing_chain; ///< AzaharFrame flash: kBackingSize square, opaque black
    frame_xr::ui::Canvas atlas; ///< its pixels (the text rows are redrawn)
    std::vector<ToastFit::Box> row_boxes; ///< AzaharFrame menuux: each row's box (toast_fit.h)
    bool logged_unsupported = false;

    std::atomic<int> toast_kind{-1}; ///< a request not yet taken by the XR thread
    int shown_kind = -1; ///< the atlas row on view, -1 none
    XrTime shown_since = 0;
    XrTime shown_ns = ToastFit::kShortNs; ///< AzaharFrame ui3: how long the one on view stays
    XrPosef shown_pose{{0, 0, 0, 1}, {0, 0, 0}};
    bool toast_nearer_logged = false; ///< AzaharFrame kiu2
    ToastDepth::Ease toast_ease; ///< AzaharFrame integ9: its depth, eased (toast_depth.h)
    ToastDepth::Ease hint_ease;  ///< AzaharFrame integ9: the hint chip's

    std::mutex text_mutex;
    std::string text_request; ///< ShowToastText not yet taken (text_requested)
    ToastTime text_request_time = ToastTime::Short; ///< AzaharFrame ui3
    bool text_requested = false;
    int text_row_waiting = -1; ///< a text row uploaded, shown once the copy is done
    std::string waiting_text;  ///< its text
    ToastTime waiting_time = ToastTime::Short; ///< AzaharFrame ui3: and how long it stays
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
    // AzaharFrame autosave: the label
    std::atomic<bool> label_requested{false};
    bool label_shown = false;
    XrTime label_since = 0;
    // AzaharFrame shaderbadge: requested under text_mutex, then on the XR thread.
    std::string badge_request;
    float badge_request_progress = 0.0f;
    bool badge_requested = false;
    frame_xr::ui::TtfFont badge_font;
    std::string badge_want; ///< the line wanted ("" hidden)
    float badge_want_progress = 0.0f;
    std::string badge_drawn; ///< the line in the atlas (on view, or being copied)
    float badge_drawn_progress = -1.0f;
    XrTime badge_drawn_at = 0;
    bool badge_waiting = false; ///< uploaded, on view once the copy is done
    int badge_waiting_px = 0;
    bool badge_ready = false; ///< the badge's pixels are on view
    int badge_box_px = 0;     ///< their width
    float badge_opacity = 0.0f;
    XrTime badge_time = 0;   ///< the last frame's time (the fade's step)
    bool badge_head = false; ///< this showing is head-locked (chosen when it appears)
    Vis::Badge::Gate badge_gate; ///< AzaharFrame fixes7: not before the game screen is on view
};

} // namespace FrameXr
