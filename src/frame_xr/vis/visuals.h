// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// What the player sees of the controllers and the emulator besides the screens and Azahar's
// window, on the XR thread (VrUi in controls.cpp calls it; app.cpp submits its layers):
//
//   - the controls help panel, to the left of Azahar's window (home panel and in-game menu),
//     labelled from the controls profile in use (help_content.h). Shown the first time VR starts
//     (remembered by the file frame-vr-controls-shown in the user folder), and by the Controls
//     button or ShowControlsHelp / ToggleControlsHelp;
//   - the Controls and Stats buttons above the window's top left corner;
//   - the stats panel ("Show Stats"), while a game runs: in front of you and fixed in the room,
//     moved with a grip, or on the left wrist ("Stats Position", stats_place.h);
//   - a beam from each controller to what it points at, fading when the controller is idle, and
//     a short faint one into nothing next to Azahar's window; the pointer dots go with them;
//   - the controllers themselves ("Show Controllers", controller_models.h).
//
// Swapchains: the atlas (help, buttons, beam), the stats (both positions) and the models: three.

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/input_frame.h"
#include "frame_xr/vis/beam.h"

namespace FrameXr {
class Runtime;
}

namespace FrameXr::Vis {

/// app.cpp, once a second while in VR: the frame loop's counters (LogSummary).
struct XrSecond {
    double seconds = 0.0;
    uint32_t display_frames = 0;
    uint32_t late = 0;          ///< top screen images released after their frame
    double wait_max_ms = 0.0;   ///< the longest xrWaitFrame period
};
void NoteXrSecond(const XrSecond& second);
/// AzaharFrame statspos: a re-centre (SteamVR's, or the 1 s right stick click hold): the stats,
/// In Front, are placed in front of you again. Any thread.
void RecentreStats();

enum class Where { Home, Menu, Game };

/// AzaharFrame tabmenu: the controls profile in use (SetControlBindings), for the in-game menu's
/// Controls tab. Any thread.
struct HelpBindings;
HelpBindings CurrentHelpBindings();

/// Azahar's window (or the picker) as shown this frame.
struct Window {
    bool shown = false;
    XrPosef pose{{0, 0, 0, 1}, {0, 0, -2}};
    float width = 1.6f;
    float height = 1.0f;
};

class Visuals {
public:
    Visuals();
    ~Visuals();

    /// After a session exists (each session).
    void Create(Runtime& rt);
    /// Before the session is destroyed.
    void DestroyXr();
    /// GUI thread (FrontendHooks::show_stats_changed).
    void SetStatsChanged(std::function<void(bool)> callback);

    /// Home and in-game menu, before Azahar's window and the keyboard see the controllers: a hand
    /// on the help panel or the buttons is taken out of `in` and works them.
    void TakeInput(const FrameInfo& info, Where where, Input::InputFrame& in, bool have_input);
    /// After everything else of the frame: the panels into `layers`; the pointer dots (quads of
    /// `dot`) out of it, to go over the models with the beams (TopLayers). `dim` is the in-game
    /// menu's dark quad, which no beam stops at. `screens`: the game's screens as drawn (LOCAL),
    /// which the stats are never placed behind, as the panels in `layers`.
    void Frame(Runtime& rt, const FrameInfo& info, Where where, const Window& window,
               const std::vector<Input::PanelQuad>& screens, const Input::InputFrame& in,
               bool have_input, std::vector<Quad>& layers, XrSwapchain dot, XrSwapchain dim);
    /// AzaharFrame statspos: the stats panel In Front as shown last frame (moved by a grip this
    /// frame), for the mapper: a beam on it hits no screen behind it. Not visible when it does
    /// not show.
    Input::PanelQuad StatsOccluder() const;
    /// app.cpp, after the other layers (`used` of them): the models' projection layer (or null)
    /// and the quads that go over it, as many as the runtime's layer limit leaves room for: the
    /// pointer dots first, then the models, then the beams.
    const XrCompositionLayerBaseHeader* TopLayers(std::vector<Quad>& top, std::size_t used) const;
    /// The runtime's limit of layers a frame (XrSystemGraphicsProperties::maxLayerCount).
    std::size_t MaxLayers() const;
    /// AzaharFrame tabmenu: the stats as the stats panel would show them now (the in-game menu's
    /// Stats tab, as it opens: the last second the game ran). XR thread.
    std::vector<std::pair<std::string, std::string>> MenuStats();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace FrameXr::Vis
