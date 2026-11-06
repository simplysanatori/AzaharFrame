// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// Internal: the home panel, Azahar's own Qt window mirrored into VR. The Qt side
// (citra_qt/frame_mirror.cpp) captures the window into an RGBA image and turns the
// events this sends back into mouse and key input; this part (the XR thread, no Qt)
// shows the image on a quad, casts the controller rays at it and sends hover, clicks,
// wheel and Escape. Only while no game runs (no GPU work on the XR thread in a game).
//
//   trigger (or A)        left button       grip (or X)   right button
//   B                     Escape            stick Y       wheel
//   bumper on the panel   drag the panel    right stick click  panel back ahead

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/input_frame.h"
#include "frame_xr/overlays.h"

namespace FrameXr {

class MirrorPanel {
public:
    // Any thread.
    void SetHandler(std::function<void(const HomePanelEvent&)> handler);
    void SetImage(std::shared_ptr<const HomePanelImage> image);
    bool Enabled() const;

    /// XR thread, while no game runs: uploads a new image, handles the controllers,
    /// appends the panel quad; `cursor` gets the pointer dot's pose when the ray is on it.
    void Frame(XrSession session, VkFormat format, OverlayUploader& uploader,
               const FrameInfo& info, const Input::InputFrame& in, bool have_input,
               std::vector<Quad>& layers, std::optional<XrPosef>& cursor);
    /// XR thread: the panel is not shown (a game runs). Lets go of held buttons once.
    void Hide();
    /// XR thread, before the session ends.
    void Destroy(OverlayUploader& uploader);

private:
    void Send(const HomePanelEvent& e);
    void Pointer(const FrameInfo& info, const Input::InputFrame& in);

    mutable std::mutex mutex;
    std::function<void(const HomePanelEvent&)> handler;
    std::shared_ptr<const HomePanelImage> image;

    // XR thread.
    std::shared_ptr<const HomePanelImage> uploaded;
    OverlayChain chain;
    XrPosef pose{{0, 0, 0, 1}, {0, 0, -2}};
    float width_m = 1.6f;
    float height_m = 1.0f;
    bool shown = false;
    int active_hand = 1; // the hand that pulled its trigger last points
    bool trigger_down[2]{};
    bool grip_down[2]{};
    bool bumper_prev[2]{};
    bool left_down = false;  // sent Press(Left) without its Release
    bool right_down = false; // sent Press(Right) without its Release
    bool prev_a = false, prev_b = false, prev_x = false, prev_reset = true;
    bool pointer_in = false; // sent Move without a Leave
    float last_u = -1.0f, last_v = -1.0f;
    // Click stickiness: a press, and a second press soon after it, keep their point
    // while the ray shakes less than kSlop, so clicks do not turn into drags and
    // double clicks are recognised.
    bool sticky = false;
    float sticky_u = 0.0f, sticky_v = 0.0f;
    XrTime sticky_until = 0;
    float wheel_acc = 0.0f;
    XrTime last_time = 0;
    // Panel drag with a bumper.
    int drag_hand = -1;
    float drag_t = 0.0f;
    Input::Vec3f drag_offset; // panel centre minus the grabbed point
    std::optional<XrPosef> cursor_pose;
};

} // namespace FrameXr
