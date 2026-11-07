// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "frame_xr/input/input_frame.h"
#include "frame_xr/input/types.h"

// AzaharFrame vrsize: the normal (not immersive) screens' size and the grip + D-pad nudge, with
// no OpenXR calls. Unit tested in src/tests/frame_xr/screen_rig.cpp.
//
// The normal screens are a pair: the top screen (1 m high at size 1, 2 m ahead to start) and
// the bottom one, which follows it. A grip on either moves both (mapper.h); while it holds them
// the stick of that hand moves them nearer or farther (up, down) and makes them bigger or
// smaller (right, left), and the D-pad turns them a small step about the head. The size is a
// scale, not a nearer screen: the top screen goes no nearer than 1 m, the bottom one is 0.5 m
// nearer than that, and a scale keeps both well away from the eyes.
// A bigger screen also gets less 3D depth (DepthScale).
namespace FrameXr::Input::ScreenRig {

/// The top screen's scale, times its normal size (1 m high).
inline constexpr float kMinScale = 0.5f;
/// 3 times: at the start distance (2 m) a 5:3 screen is then 2 x atan(1.25) = 103 degrees wide,
/// more than the Frame's whole view; at 1 m it is 136 degrees.
inline constexpr float kMaxScale = 3.0f;
/// Full stick sideways: the scale changes by e^(rate x seconds) (as the immersive screen's size).
inline constexpr float kScaleRate = 0.5f;
/// The top screen stays at least this far ahead (z), as when it is dragged.
inline constexpr float kNearestTopZ = -1.0f;
/// The bottom screen at scale 1 (CitraVR GameSurfaceLayer.cpp:125-139): 0.75 m below the top
/// screen's centre, 0.5 m nearer, tilted back 45 degrees.
inline constexpr float kBottomBelow = 0.75f;
inline constexpr float kBottomNearer = 0.5f;
inline constexpr float kBottomPitch = -0.785398163f;
/// The top screen's half height at scale 1.
inline constexpr float kTopHalfHeight = 0.5f;
/// One D-pad press turns the pair this far about the head (degrees).
inline constexpr float kNudgeStepDeg = 0.5f;
/// A nudge never takes the top screen further than this from straight ahead, either axis.
inline constexpr float kMaxNudgeAngleDeg = 60.0f;

float ClampScale(float scale);
/// The scale after `dt` seconds of the stick at `stick_x` (-1..1; within `threshold` nothing).
float ScaleWithStick(float scale, float stick_x, float dt, float threshold);

/// The share of the 3D depth setting the game gets at this scale: 1 up to the normal size, then
/// 1 / scale, so the two eyes' pictures of a distant object stay as far apart in metres as at the
/// normal size (common/frame_vr_depth.h).
float DepthScale(float scale);

/// Where the bottom screen goes for the top screen's pose and scale, in the top screen's frame:
/// its top edge keeps its place just under the top screen's bottom edge, so a bigger top screen
/// never covers it, and it is 0.5 m nearer. Its own size does not change (it stays a touch
/// screen in reach of the beam).
Vec3f BottomOffset(float scale);
Posef BottomFromTop(const Posef& top, float scale);

/// The top screen turned about the viewer (the LOCAL origin) by `right_steps` and `up_steps`
/// presses: its distance does not change and it faces the viewer. Nothing (no move) when that
/// would take it past kMaxNudgeAngleDeg or nearer than kNearestTopZ.
std::optional<Posef> NudgeTop(const Posef& top, int right_steps, int up_steps);

/// "Moved Right", "Moved Left", "Moved Up", "Moved Down".
const char* MovedToast(int right_steps, int up_steps);

/// The D-pad while a grip holds a screen (the normal pair or the immersive screen). While
/// `holding`, the D-pad never reaches the game; a direction pressed then stays away from it until
/// it is let go, even after the grip. A press is one step, and held it repeats.
class DpadNudger {
public:
    static constexpr int64_t kRepeatDelayNs = 400'000'000;
    static constexpr int64_t kRepeatNs = 100'000'000;

    struct Steps {
        int right = 0;
        int up = 0;
        bool Any() const {
            return right != 0 || up != 0;
        }
    };

    /// One frame (in.time_ns): clears the D-pad in `in` as above; returns this frame's steps
    /// (none unless `holding`).
    Steps Filter(InputFrame& in, bool holding);
    void Reset();

private:
    struct Dir {
        bool down = false;
        bool swallowed = false;
        int64_t next_ns = 0;
    };
    std::array<Dir, 4> dirs_{}; ///< right, left, up, down
};

} // namespace FrameXr::Input::ScreenRig
