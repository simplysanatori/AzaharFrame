// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: CitraVR's immersive mode on the Steam Frame (see IMMERSIVE.md).
//
// Also "Hide Bottom Screen" (Settings::values.vr_hide_bottom_screen, per game): the bottom screen
// is not drawn and cannot be touched or grabbed, with or without immersive mode.
//
// Settings::values.vr_immersive_mode (CitraVR's key, per game in the Qt window):
//   0  Off.
//   1  Wide: the game draws a 3x wider view (the shader divides gl_Position.xy by 3) on a big
//      top screen 0.5 m ahead, fixed in the room. No head tracking in the game.
//   2  Full, Ocarina of Time 3D / Majora's Mask 3D: the game's view matrix follows the head
//      (rotation and position) and the 1.4x wider view fills a head-locked top screen.
//   3  Full, other games: as 2, with the register found by CitraVR's heuristic (or set by
//      hand), and each eye's screen centred on that eye.
//
// Threads: Install / Uninstall on the GUI thread (frame_xr Start / Shutdown). Everything
// else runs on the XR thread: the frame callback publishes the head pose for the emulation
// thread (video_core/vr_immersive.h); the frame loop asks where the screens go; the
// controller code passes the input on.

#pragma once

#include "frame_xr/frame_xr.h"

namespace FrameXr::Input {
struct InputFrame;
}

namespace FrameXr::Immersive {

/// After the runtime exists (registers the frame callback, enables the resolution steps).
void Install();
/// Before the frame loop stops (publishes "off", removes the callback).
void Uninstall();

/// Where the game screens go this frame.
struct ScreenLayers {
    XrSpace top_space = XR_NULL_HANDLE;
    XrPosef top_pose[2]{}; ///< left eye, right eye
    XrExtent2Df top_size{};
    bool show_bottom = true; ///< at the normal bottom placement
};

/// XR thread, while building the layers. Not in immersive mode: the normal placement in
/// `local_space` (both eyes on one pose, bottom shown).
ScreenLayers Screens(XrSpace local_space, XrSpace view_space, const ScreenPlacement& placement);

/// XR thread, once per game frame before the mapper runs. Right stick click: re-centres the
/// immersive view on the head. Clears the pointer targets that are not on view (the top screen
/// is not grabbable in immersive mode; the hidden bottom screen cannot be touched or grabbed).
void OnGameInput(const Input::InputFrame& in, bool& top_pointable, bool& bottom_pointable);

/// Whether Left Bumper + Right Bumper + View may show or hide the bottom screen now: a game is
/// on view and either immersive mode or "Hide Bottom Screen" is on. The mapper then reports the
/// chord and keeps that View press from the game (PanelGeometry::bottom_toggle_enabled).
bool CanToggleBottom();

/// Shows the bottom screen, or hides it again. In immersive modes 2 and 3 showing it pauses
/// immersive mode, as in CitraVR. Starts hidden at every game start. XR thread.
void ToggleBottom();

} // namespace FrameXr::Immersive
