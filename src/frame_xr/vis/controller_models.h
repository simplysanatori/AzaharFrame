// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// The Steam Frame controllers drawn in VR ("Show Controllers"): SteamVR's own models of them
// (model_file.h), lit as in Moonlight XR Native and with their buttons, sticks and triggers
// following the controls, in a projection layer of their own that goes over the screens and the
// panels (a controller held up in front of a screen is never lost behind it).
//
// This is the one part of frame_xr that renders on the XR thread while a game runs, so it keeps
// to the rules the screens keep: it never waits for the GPU on the XR thread, it holds the queue
// lock only around vkQueueSubmit and the swapchain calls, and an image is released only on a
// later frame once its fence has signalled, the way the renderer releases the screens. Until then
// the layer shows the image released before, with the eye poses it was drawn for, so SteamVR
// moves it with the head correctly; only the controllers can lag a frame behind when the GPU is
// busy. At most one image is in flight. The pass is small: two models at about half the eye
// resolution (never more than 1000 pixels high, so Valve's foveation layer leaves it alone),
// 4x MSAA where the GPU has it.
//
// The files are read, and the shaders compiled, on a thread of their own the first time models
// are wanted; the meshes and textures go to the GPU with a fence that is polled, too.

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <thread>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/types.h"
#include "frame_xr/vis/model_file.h"

namespace FrameXr {
class Runtime;
}

namespace FrameXr::Vis {

class ControllerModels {
public:
    ControllerModels();
    ~ControllerModels();
    ControllerModels(const ControllerModels&) = delete;
    ControllerModels& operator=(const ControllerModels&) = delete;

    struct Hand {
        bool shown = false;
        Input::Posef aim; ///< LOCAL space, the display time of the frame
        float fade = 1.0f;
        ModelInputs input{};
    };

    /// XR thread, each frame while models are wanted (anything shown or not): reads the files
    /// the first time, sets up the GPU side, releases a finished image and draws the next one.
    void Frame(Runtime& rt, const ViewPose& views, const std::array<Hand, 2>& hands);
    /// The layer for this frame's xrEndFrame, or null. Valid until the next Frame.
    const XrCompositionLayerBaseHeader* Layer() const;
    /// XR thread, before the session ends: everything on the GPU (the files stay read).
    void DestroyXr();

private:
    void Setup(Runtime& rt);
    void Record(const ViewPose& views, const std::array<Hand, 2>& hands);

    struct Files;
    struct Gpu;
    std::unique_ptr<Files> files;
    std::unique_ptr<Gpu> gpu;
    std::thread loader;
    std::atomic<int> load_state{0}; ///< 0 not started, 1 reading, 2 read, 3 failed
    bool layer_ready = false;
};

} // namespace FrameXr::Vis
