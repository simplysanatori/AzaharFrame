// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include "frame_xr/input/input_frame.h"
#include "frame_xr/input/types.h"

// AzaharFrame mouse
// Bottom Screen Reticle (Controls: Off, Left Controller, Right Controller): one Frame controller
// aims a point on the bottom screen as a stylus would, without pointing at the bottom screen;
// its trigger touches there (held, it drags). Made for the reticle of Kid Icarus: Uprising.
// Pure logic: the frame loop gives it the controller each frame and sends its point and touch
// through the mouse's cursor pipeline (TouchpadTouch, a source of its own).
//
// Absolute, with edge drift: the controller's yaw and pitch from a reference direction map
// straight onto the screen, the screen filling a cone (30 degrees wide at Cursor Speed Normal,
// 4:3 like the screen; Cursor Speed narrows or widens it). Pointing past an edge drags the
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
namespace FrameXr::Input {

/// Settings::values.vr_bottom_screen_reticle.
enum class ReticleHand : int { Off = 0, Left = 1, Right = 2 };

constexpr ReticleHand ReticleHandFrom(uint32_t value) {
    return value == 1 ? ReticleHand::Left : value == 2 ? ReticleHand::Right : ReticleHand::Off;
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

/// The hand index (HandIndex) of a choice, -1 for Off.
int ReticleHandIndex(ReticleHand hand);

/// The cone's half width in degrees for a Cursor Speed (15 at Normal, divided by the speed's
/// scale). The half height is three quarters of it.
float ReticleHalfWidthDeg(int speed);

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

/// SteamVR's recenter (any thread): the reticle's next frame re-zeroes. TakeReticleRezero
/// returns it once.
void RequestReticleRezero();
bool TakeReticleRezero();

class BottomScreenReticle {
public:
    static constexpr float kDeadzoneDeg = 0.08f;
    static constexpr float kMinCutoffHz = 1.5f;
    static constexpr float kBeta = 0.06f; ///< per degree a second
    static constexpr float kDCutoffHz = 1.0f;
    static constexpr float kTriggerPress = 0.55f; ///< as the mapper's
    static constexpr float kTriggerRelease = 0.40f;
    static constexpr float kGripPress = 0.60f;
    static constexpr float kGripRelease = 0.40f;
    /// Snap to Middle: a held touch lifts this long (and at least one frame).
    static constexpr int64_t kLiftNs = 50'000'000;
    /// Snap to Middle: no other snap for this long after the touch comes back (or the snap,
    /// with no touch).
    static constexpr int64_t kSnapCooldownNs = 200'000'000;

    /// One frame. `live`: on, a game running, the menu closed. `hand`: the chosen controller as
    /// read. A controller that is not tracked gives `on` false; when it comes back it starts at
    /// the middle.
    ReticleFrame Update(const HandInput& hand, bool live, int speed, int64_t now_ns,
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
