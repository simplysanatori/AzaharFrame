// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include "frame_xr/input/input_frame.h"
#include "frame_xr/input/types.h"

// AzaharFrame mouse
// Bottom Screen Reticle (Controls: Off, Left Controller, Right Controller, Headset): one Frame
// controller (or the head) aims a point on the bottom screen as a stylus would, without pointing
// at the bottom screen; its trigger touches there (held, it drags). Made for the reticle of Kid
// Icarus: Uprising. AzaharFrame input2: with Headset the head aims and the trigger of either
// controller touches (either one held is a touch: the cleanest rule, nothing to choose); the
// grips keep their jobs and only Recenter re-zeroes.
// Pure logic: the frame loop gives it the controller each frame and sends its point and touch
// through the mouse's cursor pipeline (TouchpadTouch, a source of its own).
//
// Absolute, with edge drift: the controller's yaw and pitch from a reference direction map
// straight onto the screen, the screen filling a cone (AzaharFrame input2: 40 degrees wide for a
// controller, 30 for the head, 4:3 like the screen; no longer the mouse's Cursor Speed, which
// made it 21 degrees at Fast). AzaharFrame reticle: divided by Reticle Speed (2x by default:
// 20 and 15 degrees). The yaw is measured across the cone at the reference's pitch
// (a turn times its cosine), so a controller held pointing down is as fast sideways as up and
// down. Pointing past an edge drags the
// reference along, so the point stays on the edge and comes back in the moment you turn back:
// there is nothing to clutch. A grip press of that controller puts the reference where it points
// (the point to the middle); so does Recenter.
//
// Why absolute and not a gyro mouse (motion turned into relative deltas): a stylus is absolute,
// and so is the reticle that follows it; with absolute aim the same direction is the same point
// every time, small corrections never drift away, and a slow, fine move is not lost in a dead
// zone (a relative mouse needs one against tremor, and it eats exactly those moves). Tremor is
// taken out by a One Euro filter (smooth at rest, little lag when moving fast) and a dead zone
// that drags the point (it follows once the aim leaves 0.08 degrees, without steps).
// AzaharFrame input2: the filter follows a move sooner (beta 0.25 per degree a second, the speed
// estimate at 2 Hz): at 20 degrees a second the point trails by about 0.3 degrees with the dead
// zone, not 0.6, and a flick settles in about 17 ms, not 58 (simulated with tremor; tested).
// Edge drift only moves the reference while the aim is past an edge: tremor at an edge moves it
// at most by the tremor's size, never step by step (tested).
namespace FrameXr::Input {

/// Settings::values.vr_bottom_screen_reticle (AzaharFrame input2: 3 Headset).
enum class ReticleHand : int { Off = 0, Left = 1, Right = 2, Headset = 3 };

constexpr ReticleHand ReticleHandFrom(uint32_t value) {
    return value == 1   ? ReticleHand::Left
           : value == 2 ? ReticleHand::Right
           : value == 3 ? ReticleHand::Headset
                        : ReticleHand::Off;
}
/// Settings::values.vr_reticle_at_edge (At Edge): Follow is the edge drift; Snap to Middle
/// re-zeroes when the aim passes an edge, and a touch held then lifts at the edge (a release the
/// game sees for kLiftNs, at least one frame) and touches again at the middle: a stylus's
/// swipe, lift, swipe (in Kid Icarus: Uprising on foot, that turns the camera). After a snap
/// another waits kSnapCooldownNs past the touch again (meanwhile the edges drift).
enum class ReticleEdge : int { Follow = 0, SnapToMiddle = 1 };

constexpr ReticleEdge ReticleEdgeFrom(uint32_t value) {
    return value == 1 ? ReticleEdge::SnapToMiddle : ReticleEdge::Follow;
}

/// The hand index (HandIndex) of a choice, -1 for Off and Headset.
int ReticleHandIndex(ReticleHand hand);

/// AzaharFrame input2: the cone's half width in degrees at Reticle Speed 1x: 20 for a
/// controller, 15 for the head (0 for Off). The half height is three quarters of it.
inline constexpr float kReticleControllerHalfWidthDeg = 20.0f;
inline constexpr float kReticleHeadHalfWidthDeg = 15.0f;

/// AzaharFrame reticle: Reticle Speed (Settings::values.vr_reticle_speed), one value for the
/// head and the controllers: the point moves `speed` times as far for the same turn from where
/// aiming started or was re-centred, so the cone is the 1x cone divided by it (the screen's
/// edges clamp the point and drift as before; Recenter and a grip still re-centre).
/// Why 2x by default: the normal bottom screen (0.75 x 0.56 m, 1.5 m ahead and 0.75 m below the
/// eyes, tilted back) looks about 25 x 18 degrees. At 1x the head had to sweep 30 x 22.5 degrees
/// to cross it, more than the screen itself looks, its bottom edge 38 degrees down: the owner
/// found that too much. At 2x it is 15 x 11 degrees (7.5 either side), well inside the turns
/// people make without effort, and a controller's 20 x 15 is about what pointing straight at the
/// screen takes.
inline constexpr float kReticleSpeedMin = 0.5f;
inline constexpr float kReticleSpeedMax = 4.0f;
inline constexpr float kReticleSpeedDefault = 2.0f;
float ClampReticleSpeed(float speed);
/// The cone's half width for a source at a Reticle Speed (1x: the values above).
float ReticleHalfWidthDeg(ReticleHand source, float speed = 1.0f);

/// AzaharFrame input2: what aims and touches this frame: a controller (its aim pose, trigger and
/// grip) or the head (its orientation, either trigger, no grip).
struct ReticleAim {
    bool active = false; ///< tracked this frame
    Quatf orientation;   ///< points along its -Z
    float trigger = 0.0f;
    float grip = 0.0f; ///< a press re-zeroes (0 for the head)
};
ReticleAim ReticleAimOf(const HandInput& hand);
/// The head aims; the touch is the larger of the two triggers (either one held touches).
ReticleAim ReticleHeadAim(bool head_active, const Quatf& head, const HandInput& left,
                          const HandInput& right);

/// AzaharFrame polish: the controller (HandIndex) that drives the reticle this frame, or -1.
/// Only while it aims the bottom screen in a game: `game_frame` is a game's frame (a game
/// running with its screens live, no in-game menu, no keyboard, the controllers read) and the
/// chosen controller is tracked. Then it draws no beam and points at nothing. Anywhere else (the
/// in-game menu, Azahar's window with or without a game, the keyboard) it is a normal pointer
/// with its beam and its clicks, and Off and Headset take no controller.
int ReticleDrivingHand(ReticleHand source, bool game_frame, const InputFrame& in);

/// The point (0..1 from the top left) for an aim `yaw_deg` and `pitch_deg` from the reference
/// (left and up positive), clamped to the screen.
Vec2f ReticlePoint(float yaw_deg, float pitch_deg, float half_width_deg);

/// One Euro filter (Casiez et al. 2012) for one value.
class OneEuro {
public:
    OneEuro(float min_cutoff_hz, float beta, float d_cutoff_hz)
        : min_cutoff_(min_cutoff_hz), beta_(beta), d_cutoff_(d_cutoff_hz) {}
    /// `dt` seconds since the last value; <= 0 or over 0.1 s starts again at `x`.
    float Filter(float x, float dt);
    void Reset() {
        started_ = false;
    }

private:
    float min_cutoff_;
    float beta_;
    float d_cutoff_;
    bool started_ = false;
    float x_ = 0.0f;
    float dx_ = 0.0f;
};

struct ReticleFrame {
    bool on = false;   ///< the controller is tracked and the reticle is live
    Vec2f point;       ///< 0..1 from the top left
    bool down = false; ///< the trigger held: touching
    bool rezeroed = false; ///< the reference moved to where it points this frame
    bool snapped = false;  ///< At Edge: Snap to Middle went off this frame
};

/// The one-time toast when the reticle is first set to a controller.
inline constexpr const char* kReticleHint = "Trigger Touches \xC2\xB7 Grip Re-centres";
/// AzaharFrame input2: the same when it is first set to Headset.
inline constexpr const char* kReticleHeadHint = "Head Aims \xC2\xB7 Trigger Touches";

/// SteamVR's recenter (any thread): the reticle's next frame re-zeroes. TakeReticleRezero
/// returns it once.
void RequestReticleRezero();
bool TakeReticleRezero();

class BottomScreenReticle {
public:
    static constexpr float kDeadzoneDeg = 0.08f;
    static constexpr float kMinCutoffHz = 1.5f;
    static constexpr float kBeta = 0.25f; ///< per degree a second (AzaharFrame input2: was 0.06)
    static constexpr float kDCutoffHz = 2.0f; ///< AzaharFrame input2: was 1
    /// The yaw's scale at the reference's pitch never goes under this (pointing near straight
    /// down or up).
    static constexpr float kMinYawScale = 0.3f;
    static constexpr float kTriggerPress = 0.55f; ///< as the mapper's
    static constexpr float kTriggerRelease = 0.40f;
    static constexpr float kGripPress = 0.60f;
    static constexpr float kGripRelease = 0.40f;
    /// Snap to Middle: a held touch lifts this long (and at least one frame).
    static constexpr int64_t kLiftNs = 50'000'000;
    /// Snap to Middle: no other snap for this long after the touch comes back (or the snap,
    /// with no touch).
    static constexpr int64_t kSnapCooldownNs = 200'000'000;

    /// One frame. `live`: on, a game running, the menu closed. `aim`: the chosen controller or
    /// the head as read; `half_width_deg` its cone (ReticleHalfWidthDeg). Not tracked gives `on`
    /// false; when it comes back it starts at the middle.
    ReticleFrame Update(const ReticleAim& aim, bool live, float half_width_deg, int64_t now_ns,
                        ReticleEdge edge = ReticleEdge::Follow);
    /// The next frame puts the reference where the controller points (Recenter).
    void Rezero() {
        rezero_ = true;
    }
    /// Off: everything forgotten; the next live frame starts at the middle.
    void Reset();

    float ref_yaw_deg() const {
        return ref_yaw_;
    }
    float ref_pitch_deg() const {
        return ref_pitch_;
    }

private:
    bool started_ = false;
    bool rezero_ = false;
    float raw_yaw_ = 0.0f; ///< unwrapped
    float ref_yaw_ = 0.0f;
    float ref_pitch_ = 0.0f;
    Vec2f out_; ///< yaw, pitch from the reference after the dead zone
    OneEuro yaw_filter_{kMinCutoffHz, kBeta, kDCutoffHz};
    OneEuro pitch_filter_{kMinCutoffHz, kBeta, kDCutoffHz};
    int64_t last_ns_ = 0;
    bool trigger_ = false;
    // Snap to Middle.
    bool lifting_ = false;
    int64_t lift_until_ = 0;
    int64_t snap_ready_ = 0;
    bool grip_ = false;
};

} // namespace FrameXr::Input
