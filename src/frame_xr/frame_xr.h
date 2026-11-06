// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Azahar for Frame's OpenXR frontend: one XrInstance, Vulkan device and XrSession for
// the whole run of citra_qt --vr, a frame loop thread at display rate that shows
// the game's three screens as quad layers, and the extension points the input,
// picker and immersive code build on.
//
// Threads: Start / Shutdown on the GUI thread. The renderer (emu thread) draws the
// screens into the swapchains through VideoCore::XrScreenSink. The XR thread waits,
// begins and ends frames and calls the frame callbacks; it never records or
// submits GPU work while a game runs.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "common/common_types.h"
#include "frame_xr/xr_includes.h"
#include "video_core/vulkan_hooks.h"

namespace Frontend {
enum class WindowSystemType : u8;
}

namespace FrameXr {

struct StartOptions {
    /// The window system of the Qt window (picks the Vulkan surface extension).
    Frontend::WindowSystemType window_type{};
    /// Instance extensions enabled when the runtime offers them (other parts add theirs).
    std::vector<std::string> optional_extensions{"XR_FB_display_refresh_rate",
                                                 "XR_VALVE_frame_controller_interaction"};
};

/// Makes the XR instance, the Vulkan device (through Azahar's own create infos),
/// the session and the frame loop thread. Call before any game boots. False: no
/// XR (the emulator runs flat; nothing is installed).
bool Start(const StartOptions& options);

/// Ends the session and destroys everything Start made. Call once no renderer
/// exists any more (after the Qt event loop returned).
void Shutdown();

/// True between a successful Start and Shutdown (the session may be idle).
bool IsStarted();

/// True if an instance extension was enabled.
bool HasExtension(const char* name);

// ---- Extension points ------------------------------------------------------

struct FrameInfo {
    XrInstance instance;
    XrSession session;
    XrSpace local_space;   ///< LOCAL reference space (all quads are placed in it)
    XrSpace view_space;    ///< VIEW reference space
    XrTime display_time;   ///< predicted display time of the frame
    XrDuration display_period;
    XrSessionState state;
    bool should_render;
    bool screens_live;     ///< a game is producing images
};

/// Called on the XR thread once per display frame, after xrBeginFrame and the view
/// locate, before the layers are built (input sync, picker, placement updates).
/// Must not block for long and must not submit GPU work while a game runs.
using FrameCallback = std::function<void(const FrameInfo&)>;
int AddFrameCallback(FrameCallback callback);
void RemoveFrameCallback(int id);

/// Called on the XR thread for every session state change.
using StateCallback = std::function<void(XrSessionState state)>;
int AddStateCallback(StateCallback callback);
void RemoveStateCallback(int id);

/// A quad layer in LOCAL space, submitted every frame while visible.
struct Quad {
    XrSwapchain swapchain = XR_NULL_HANDLE;
    XrRect2Di image_rect{};
    XrPosef pose{{0, 0, 0, 1}, {0, 0, 0}};
    XrExtent2Df size{1.0f, 1.0f};
    XrEyeVisibility eye = XR_EYE_VISIBILITY_BOTH;
    XrCompositionLayerFlags flags = 0;
    int order = 0; ///< larger is drawn later (on top); the game screens are order 0
    bool visible = true;
    bool with_game = true; ///< false: hidden while a game's screens are shown
};
int AddQuad(const Quad& quad);
bool SetQuad(int id, const Quad& quad);
void RemoveQuad(int id);

/// The last located head and eye poses (LOCAL space).
struct ViewPose {
    bool valid = false;
    XrTime time = 0;
    XrPosef head{{0, 0, 0, 1}, {0, 0, 0}};
    XrPosef eye[2]{{{0, 0, 0, 1}, {0, 0, 0}}, {{0, 0, 0, 1}, {0, 0, 0}}};
    XrFovf fov[2]{};
};
ViewPose LatestViewPose();

/// Where the game screens are (LOCAL space; sizes in metres).
struct ScreenPlacement {
    XrPosef top_pose;
    XrExtent2Df top_size;
    XrPosef bottom_pose;
    XrExtent2Df bottom_size;
};
ScreenPlacement DefaultScreenPlacement();
ScreenPlacement GetScreenPlacement();
void SetScreenPlacement(const ScreenPlacement& placement);

// ---- Qt side -------------------------------------------------------------------

/// What the XR thread asks the Qt side to do (both must queue to the GUI thread).
struct FrontendHooks {
    std::function<void(const std::string& path)> launch_game; ///< picker selection
    std::function<void()> stop_game;                          ///< View held 1.5 s
};
void SetFrontendHooks(FrontendHooks hooks);

/// A game started (BootGame done) or stopped (ShutdownGame done). GUI thread.
void SetGameRunning(bool running);

/// The folders the picker scans (UISettings::values.game_dirs). GUI thread; rescans.
struct LibraryDir {
    std::string path;
    bool deep_scan = false;
    int max_depth = -1; ///< >= 0 overrides the depth deep_scan implies
};
void SetLibraryDirs(const std::vector<LibraryDir>& dirs);

/// Handles for the other parts (XR_NULL_HANDLE before Start / before the first game).
XrInstance Instance();
XrSystemId System();
XrSession Session();
XrSwapchain ScreenSwapchain(VideoCore::XrScreen screen);

} // namespace FrameXr
