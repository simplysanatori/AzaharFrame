// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame shaderbadge: where the "Preparing Shaders 3 / 9" badge goes and how it fades, with
// no OpenXR calls (unit tested in src/tests/frame_xr/shader_badge.cpp). The text and when it
// shows are ShaderNotice's (frame_xr/shader_notice.h); hud.cpp draws it.
//
// The badge is a small dark pill: a progress ring and the line. Its place:
//   - Normal view: just inside the main screen's top-right corner (the top screen, or the bottom
//     one while it is in focus), in the screen's plane (on its curve when the screen is curved),
//     so it moves with the screen. kScreenHeight of the screen's height, kScreenInset in from
//     both edges.
//   - Immersive view, or when the corner of the normal screen is not well in view when the badge
//     appears (PointInView, as the "Autosaved" label): head-locked kHeadDistance ahead, its
//     top-right corner at the top-right of the part of the screen that is in view (the screen's
//     edges, or the view's edges less kHeadMarginDeg when the screen reaches past them), so it
//     is never outside what both eyes see.
// The choice is made when it appears and kept until it has faded out.
// It fades in and out over kFadeNs, in kLevels pre-drawn opacities.
// AzaharFrame fixes7: never before the game screen is in front of the user (Gate): not while the
// view is still black at boot, only kGraceNs after the screen first shows pictures.

#pragma once

#include <cstdint>
#include <optional>

#include "frame_xr/input/types.h"

namespace FrameXr::Vis::Badge {

/// The badge's height over the normal screen's height, and its inset from the screen's top and
/// right edges (the same fraction).
constexpr float kScreenHeight = 0.08f;
constexpr float kScreenInset = 0.03f;
/// Head-locked: this far ahead (metres), this many degrees high (the normal one is 2.3 degrees on
/// the default 1 m high screen 2 m away), kept this many degrees inside the view's top and right
/// edges, kHeadInsetDeg inside the screen's.
constexpr float kHeadDistance = 1.0f;
constexpr float kHeadHeightDeg = 2.4f;
constexpr float kHeadMarginDeg = 12.0f;
constexpr float kHeadInsetDeg = 1.5f;
/// The fade in and the fade out.
constexpr int64_t kFadeNs = 150'000'000;
/// The opacities drawn into the atlas (the fade steps through them).
constexpr int kLevels = 4;

/// AzaharFrame fixes7: how long the game screen shows pictures before the badge may show.
constexpr int64_t kGraceNs = 3'000'000'000;

/// AzaharFrame fixes7: whether the badge may show. `live`: the game's screens are live (a game
/// runs); `shown`: the screen the badge goes on (the main one, or the immersive top screen) is
/// on view with a picture. The grace starts when that first happens in a game and is not
/// started again while the game runs; the badge shows only while the screen does.
class Gate {
public:
    bool Allow(bool live, bool shown, int64_t now_ns) {
        if (!live) {
            since_ = 0;
            return false;
        }
        if (shown && since_ == 0) {
            since_ = now_ns != 0 ? now_ns : 1;
        }
        return shown && since_ != 0 && now_ns - since_ >= kGraceNs;
    }

private:
    int64_t since_ = 0; ///< when the screen first showed pictures (0: not yet)
};

/// The badge's quad: its centre pose and size (metres).
struct Placed {
    Input::Posef pose;
    float width = 0.0f;
    float height = 0.0f;
};

/// Normal view: on the screen centred at `screen`, `width` x `height` metres, flat or (with
/// `curve_radius` above 0) a cylinder of that radius as ScreenRig::CylinderFor makes it. `aspect`:
/// the badge's width over its height. Shrunk to fit a very small screen.
Placed OnScreen(const Input::Posef& screen, float width, float height, float curve_radius,
                float aspect);

/// Angles from the head's forward axis, radians (OpenXR XrFovf signs: left and down negative).
struct Angles {
    float left = 0.0f, right = 0.0f, up = 0.0f, down = 0.0f;
};
/// The angles a flat screen centred at `screen` (head space: the head at the origin looking down
/// -Z), `width` x `height`, covers: its corners' extremes. Nothing when a corner is behind the
/// head.
std::optional<Angles> ScreenAngles(const Input::Posef& screen, float width, float height);
/// Head-locked, in head space: `view` is what both eyes see; `screen` the part the screen covers
/// (none: the view's corner). `aspect` as above.
Placed HeadLocked(const Angles& view, const std::optional<Angles>& screen, float aspect);

/// The fade: opacity 0 to 1, towards `shown`, by `dt_ns` over kFadeNs.
float Fade(float opacity, bool shown, int64_t dt_ns);
/// The pre-drawn level for an opacity (0 none, else 1 to kLevels).
int Level(float opacity);

} // namespace FrameXr::Vis::Badge
