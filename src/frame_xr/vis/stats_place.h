// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame statspos
// Where the stats panel goes ("Stats Position", Settings::values.vr_stats_position), with no
// OpenXR calls (unit tested in src/tests/frame_xr/vis.cpp):
//
//   - In Front: placed once, ahead of where you look and a little below, then fixed in the room
//     (never head-locked). It is never put behind a screen or panel: its distance is at most the
//     nearest one under it less kStatsScreenGap (StatsClearDistance). A grip on it moves it on
//     the sphere round your head (StatsGrab), and that hand's stick sets its distance and size.
//   - On Wrist: on the left controller like a watch face (WristStatsPose), shown only while it
//     faces your eyes (WristFacing / WristShown).

#pragma once

#include <vector>

#include "frame_xr/input/geometry.h"

namespace FrameXr::Vis {

// ---- In Front --------------------------------------------------------------------------------

/// Where it is placed: this far from the eyes, this far below the direction you look in.
constexpr float kStatsFrontDistance = 1.0f;
constexpr float kStatsFrontBelowDeg = 12.0f;
/// The look's pitch counts only up to this far up or down (lying back, looking at the floor).
constexpr float kStatsFrontMaxPitchDeg = 25.0f;
/// Its width at size 1 (the height follows the art, 512 x 320).
constexpr float kStatsFrontWidth = 0.30f;
constexpr float kStatsMinScale = 0.5f, kStatsMaxScale = 2.0f;
/// Stick up / down moves it this fast; sideways scales it by e^(rate x s), as the screens.
constexpr float kStatsDepthSpeed = 1.0f;
constexpr float kStatsScaleRate = 0.5f;
constexpr float kStatsStickThreshold = 0.5f;
/// Its distance from the eyes, and how far in front of a screen or panel it stays.
constexpr float kStatsMinDistance = 0.4f, kStatsMaxDistance = 3.0f;
constexpr float kStatsScreenGap = 0.15f;

/// The distance for a panel w x h facing `eye` along `dir` (unit): `wanted`, but no farther than
/// the nearest obstacle seen through the panel (rays from the eye through a 3 x 3 grid of its
/// points) less kStatsScreenGap, and kept in [kStatsMinDistance, kStatsMaxDistance].
float StatsClearDistance(const Input::Vec3f& eye, const Input::Vec3f& dir, float wanted, float w,
                         float h, const std::vector<Input::PanelQuad>& obstacles);

/// The direction it is placed in for this head pose: the look's heading, its pitch (within
/// kStatsFrontMaxPitchDeg) less kStatsFrontBelowDeg.
Input::Vec3f StatsFrontDirection(const Input::Posef& head);

/// Turned about the vertical only, so its front (+Z) faces `eye`.
Input::Quatf StatsFacing(const Input::Vec3f& centre, const Input::Vec3f& eye);

/// The pose at `distance` along `dir` from `eye`, facing it.
Input::Posef StatsPoseAt(const Input::Vec3f& eye, const Input::Vec3f& dir, float distance);

/// A grip holding the panel: it keeps its distance from the eyes and the angle between the
/// beam's point and its middle as seen from them.
class StatsGrab {
public:
    /// The grip closed with the beam on the panel at `hit` (world).
    void Begin(const Input::Vec3f& eye, const Input::Posef& panel, const Input::Vec3f& hit);
    /// Each frame while held: the direction its middle goes in for this beam.
    Input::Vec3f Direction(const Input::Vec3f& eye, const Input::Ray& ray, float distance) const;

private:
    float yaw_offset = 0.0f, pitch_offset = 0.0f;
};

/// Heading (about +Y, 0 = -Z, positive to the left) and pitch (up positive) of a direction.
float YawOf(const Input::Vec3f& dir);
float PitchOf(const Input::Vec3f& dir);
Input::Vec3f DirectionFrom(float yaw, float pitch);

// ---- On Wrist --------------------------------------------------------------------------------

/// Its size on the left controller.
constexpr float kWristWidth = 0.10f;
/// It shows while its front is within kWristShowDeg of the eyes, and goes past kWristHideDeg.
constexpr float kWristShowDeg = 35.0f, kWristHideDeg = 40.0f;
/// Seconds of the fade in or out.
constexpr float kWristFadeSeconds = 0.15f;

/// The panel's pose in the left controller's aim space (the comment over it in stats_place.cpp
/// says how it was found from the model).
Input::Posef WristLocalPose();
/// In LOCAL space for the left controller's aim pose.
Input::Posef WristStatsPose(const Input::Posef& left_aim);
/// The angle (degrees) between the panel's front and the direction to `eye`.
float WristFacingDeg(const Input::Posef& panel, const Input::Vec3f& eye);
/// Whether it should show, with the hysteresis (`shown`: it shows now).
bool WristShown(float facing_deg, bool shown);

} // namespace FrameXr::Vis
