// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: CitraVR's immersive mode on the Steam Frame (see IMMERSIVE.md).
//
// Also "Hide Bottom Screen" (Settings::values.vr_hide_bottom_screen, per game): the bottom screen
// is not drawn and cannot be touched or grabbed, with or without immersive mode.
//
// "VR Screen Shape" (Settings::values.vr_top_aspect, per game) sets the top screen's width over
// its height: 5:3 (the 3DS), 16:9, 21:9, 4:3, 3:2 or 16:10, for games and mods that draw widescreen squeezed into
// the 400x240 top screen. "Immersive Game FOV" (vr_immersive_game_fov, per game) is the game
// camera's vertical angle that sizes the immersive screen. Both are read when a game starts.
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

/// The top screen's width over its height for the game on view (5:3, 16:9, 21:9, 4:3, 3:2 or 16:10; 5:3 when no
/// game has started). Latched when a game starts. Any thread.
float TopAspect();

/// Where the game screens go this frame. The bottom screen and, unless `top_space` is the VIEW
/// space, the top screen are in the LOCAL space.
struct ScreenLayers {
    XrSpace top_space = XR_NULL_HANDLE;
    XrPosef top_pose[2]{}; ///< left eye, right eye
    XrExtent2Df top_size{};
    bool show_top = true;
    /// The top screen is in LOCAL space and is not the immersive screen: it can be aimed at and
    /// grabbed.
    bool top_pointable = true;
    XrPosef bottom_pose{};
    XrExtent2Df bottom_size{};
    bool show_bottom = true;
    /// The bottom screen is farther away than the top one: submit it first. Quad layers are
    /// drawn in submission order, with no depth test, so the nearer one has to come last.
    bool bottom_first = false;
};

/// XR thread, while building the layers and the pointer targets. Not in immersive mode: the
/// normal placement in `local_space`.
ScreenLayers Screens(XrSpace local_space, XrSpace view_space, const ScreenPlacement& placement);

/// XR thread, once per game frame before the mapper runs, with the frame the mapper will get.
/// While the immersive screen is on view the grips move and resize it (immersive_layout.h);
/// the grips, and the stick of the hand that holds the screen, are then taken out of `in`.
void OnGameInput(Input::InputFrame& in);

/// Whether Left Bumper + Right Bumper + View may show or hide the bottom screen now: a game is
/// on view and either immersive mode or "Hide Bottom Screen" is on. The mapper then reports the
/// chord and keeps that View press from the game (PanelGeometry::bottom_toggle_enabled).
bool CanToggleBottom();

/// The in-game menu (Azahar's window over the paused game) is open: immersive modes 2 and 3
/// pause, as when the bottom screen is shown. XR thread.
void SetMenuOpen(bool open);

/// Left Bumper + Right Bumper + View: shows the bottom screen, or hides it again. In immersive
/// modes 2 and 3 showing it pauses immersive mode, as in CitraVR. XR thread.
void ToggleBottom();

/// The bottom screen is the screen in focus (it has the top screen's place; the screen layout
/// itself is not changed here). Immersive mode pauses while it is, as when the bottom screen
/// shows, and resumes after. Cleared when no game is on view. XR thread.
void SetBottomFocused(bool focused);

/// A grip holds the immersive screen now (OnGameInput). The stick click taps do not swap or
/// hide the screens then (a swap would cancel the move half way); a right stick click tap
/// calls ResetHeldScreen instead. XR thread.
bool HoldingScreen();

/// While a grip holds the immersive screen: it goes back to its default placement for this game
/// (the saved one is forgotten). No re-centre. Does nothing otherwise. XR thread.
void ResetHeldScreen();

/// Right Stick Click held for 1 second (reset_panels): re-centres the view on the head; while a grip holds the
/// immersive screen, the screen also goes back to its default placement for this game (the
/// saved one is forgotten). XR thread.
void OnResetPanels();

} // namespace FrameXr::Immersive
