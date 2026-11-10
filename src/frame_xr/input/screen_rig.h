// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include "frame_xr/input/geometry.h"
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

// ---- AzaharFrame layout: Screen Layout --------------------------------------------------------
//
// Where the second slot goes for the main one (Settings::values.vr_screen_layout, global):
// Stacked is BottomFromTop; Side by Side is the main slot turned about the viewer (the LOCAL
// origin's vertical axis) far enough that the second screen sits kSideGap to the right of it,
// at the same distance and height (vertically centred on the main screen), facing you as the
// main one does, not tilted back. The widths are the unswapped ones (the top screen in the main
// slot); swapped, the bottom screen in the main slot is narrower, so the gap only grows.

inline constexpr uint32_t kLayoutStacked = 0;
inline constexpr uint32_t kLayoutSideBySide = 1;
/// The gap between the main screen's right edge and the second screen's left edge, in metres.
inline constexpr float kSideGap = 0.1f;

/// "Stacked", "Side by Side".
const char* LayoutName(uint32_t layout);
/// "Screen Layout: Side by Side".
std::string LayoutToast(uint32_t layout);
/// The other layout (two of them: the cycle is a switch).
uint32_t NextLayout(uint32_t layout);
/// The second slot's pose for the main slot's pose, scale and width and the second screen's
/// width.
Posef SecondFromMain(const Posef& main, float scale, float main_width, float second_width,
                     uint32_t layout);

/// The top screen turned about the viewer (the LOCAL origin) by `right_steps` and `up_steps`
/// presses: its distance does not change and it faces the viewer. Nothing (no move) when that
/// would take it past kMaxNudgeAngleDeg or nearer than kNearestTopZ.
std::optional<Posef> NudgeTop(const Posef& top, int right_steps, int up_steps);

/// "Moved Right", "Moved Left", "Moved Up", "Moved Down".
const char* MovedToast(int right_steps, int up_steps);

// ---- AzaharFrame curve: Screen Curve ------------------------------------------------------
//
// The normal screens can be cylinder layers (XR_KHR_composition_layer_cylinder) instead of
// quads, bent about a vertical axis (their own +Y) so the sides come towards you. The setting
// (Settings::values.vr_screen_curve, global) is the angle the main screen's width spans: its
// radius is width / angle, so the arc is as long as the flat screen was wide and the picture
// keeps its shape and size. Only the screen in the main slot bends (the top screen, or the bottom
// one when swapped); the smaller one in the second slot always stays flat. At the step where
// the radius equals the distance, the centre of curvature is you. A cylinder layer's middle is at its pose's -Z, `radius` away: the pose
// is the screen's own, moved `radius` towards the viewer (CylinderFor), so the screen's place,
// the grip move, the size and the nudge all stay as they are for the flat screen.

/// The steps of grip + D-pad up and down and of the settings, in degrees (0 = flat): Off,
/// Subtle, Gentle, Medium, Strong, Heavy, Max (CurveName).
inline constexpr std::array<uint32_t, 7> kCurveStepsDeg{0, 15, 30, 45, 60, 75, 90};
/// Never bent further than this (a value typed into the config).
inline constexpr uint32_t kMaxCurveDeg = 180;

/// The next step up (dir > 0) or down (dir < 0) from `deg`; a value between two steps goes to
/// the next one that way. At the ends it stays.
uint32_t StepCurve(uint32_t deg, int dir);
/// "Off", "Gentle", "Medium", "Strong", "Max" for the steps; a value between steps (typed into
/// the config) has the name of the nearest one.
const char* CurveName(uint32_t deg);
/// "Screen Curve: Medium".
std::string CurveToast(uint32_t deg);
/// The radius that bends a screen `width` wide by `deg` degrees (arc length = width); 0 = flat.
float CurveRadius(float width, uint32_t deg);
/// Sets curve_radius for `deg`: the screen in the main slot bends (0 leaves it flat), the one in
/// the second slot is always flat.
void ApplyCurve(PanelQuad& main, PanelQuad& secondary, uint32_t deg);

/// What a cylinder layer needs to show a screen of this size bent with this radius.
struct Cylinder {
    Posef pose;
    float radius = 0.0f;
    float central_angle = 0.0f; ///< radians: width / radius
    float aspect_ratio = 1.0f;  ///< width / height of the arc, as the flat screen
};
Cylinder CylinderFor(const Posef& screen, float width, float height, float radius);

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
        /// AzaharFrame curve: the new presses of up (+1) and down (-1) only, without repeats.
        int up_presses = 0;
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
