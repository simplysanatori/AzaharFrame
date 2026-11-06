// Copyright 2026 CitraFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Internal: what the XR thread does with the controllers each frame. In a game:
// the Frame controllers drive the virtual pad and the bottom-screen touch
// (frame_xr/input), grab and push the top screen (the bottom one follows it), the
// right stick click resets both in front of the head, holding View stops the game.
// Without a game: the application picker panel (frame_xr/picker), pointed at with
// either controller. A pointer dot is drawn where the ray hits a panel.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/actions.h"
#include "frame_xr/input/input_bridge.h"
#include "frame_xr/overlays.h"

namespace frame_xr::picker {
class Library;
class Picker;
} // namespace frame_xr::picker

namespace FrameXr {

class Runtime;

class VrUi {
public:
    VrUi();
    ~VrUi();

    /// After the session exists: actions (suggest + attach), uploads, picker library.
    bool Create(Runtime& runtime);
    /// Before the session is destroyed.
    void Destroy();

    /// XR thread, once per frame after the views were located. Applies drags to the
    /// screen placement and appends the picker / pointer quads to `layers`.
    void Frame(Runtime& runtime, const FrameInfo& info, std::vector<Quad>& layers);

    // GUI thread.
    void SetFrontendHooks(FrontendHooks hooks);
    void SetGameRunning(bool running);
    void SetLibraryDirs(const std::vector<LibraryDir>& dirs);

private:
    void GameFrame(Runtime& runtime, const FrameInfo& info, const Input::InputFrame& in,
                   bool have_input, std::vector<Quad>& layers);
    void PickerFrame(Runtime& runtime, const FrameInfo& info, const Input::InputFrame& in,
                     bool have_input, std::vector<Quad>& layers);
    void ResetPlacement(Runtime& runtime);
    void AddCursor(const XrPosef& pose, float size_m, std::vector<Quad>& layers);

    Input::Actions actions;
    Input::InputBridge bridge;
    bool actions_ok = false;
    bool bridge_live = false;

    OverlayUploader uploader;
    OverlayChain picker_chain;
    OverlayChain cursor_chain;
    std::shared_ptr<frame_xr::picker::Library> library;
    std::unique_ptr<frame_xr::picker::Picker> picker;
    bool picker_shown = false;
    XrPosef picker_pose{{0, 0, 0, 1}, {0, 0, -2}};

    // Edges for the picker (the mapper keeps its own for the game).
    bool prev_select = false;
    bool prev_back = false;
    XrTime last_time = 0;

    std::mutex hooks_mutex;
    FrontendHooks hooks;
    std::atomic<bool> game_running{false};
    std::atomic<bool> launch_pending{false};
    XrTime launch_time = 0;
};

} // namespace FrameXr
