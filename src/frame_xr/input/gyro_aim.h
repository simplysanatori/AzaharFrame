// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiugyro: Gyro Aim, for Kid Icarus: Uprising with Darkshade's C-Stick & Gyro mod
// (v172; handoff/reports/37/kiugyro/design.md has the disassembly). Pure math, no OpenXR, Qt or
// emulator calls, so it is unit tested.
//
// What the mod does with the 3DS gyroscope (its "gyro" control scheme only): once per game frame
// it reads the latest HID gyroscope entry, takes z as the reticle's horizontal rate and x as its
// vertical rate (degrees per second), subtracts a 0.25 deg/s dead zone, clamps them to +-360 and
// feeds them into the same synthetic touch drag as its C-Stick. Its signs are a real 3DS's
// (Azahar's 3DS axes, motion.h): turning the console right reads negative z and moves the reticle
// right, tilting its top away (aiming up) reads negative x and moves it up. AzaharFrame kiufix:
// the first build sent positive z for a turn to the right (design.md read the drag's sign the
// other way), so Gyro Aim went left when the hand went right; the owner's headset check of
// 2026-11-14 found it, and the unit tests now hold Gyro Aim to Motion Controls' own axes. So its gyro is relative (a rate moves the reticle,
// like a mouse) and, on the ground, the game's reticle box turns the camera when the drag runs
// past it, exactly as with the C-Stick. A held touch stops both.
//
// Gyro Aim (Settings::values.vr_gyro_aim, per game):
//   Gyro: the controller's yaw and pitch rates go to the mod (z = yaw to the left, x = pitch
//     down, as Motion Controls would send them for a 3DS held like the controller), times Gyro
//     Sensitivity. Its grip held pauses them (a clutch: the hand can move back
//     without moving the reticle; nothing jumps when it is let go).
//   Light Gun: the reticle goes where the controller's ray hits the top screen. The app reads the
//     reticle's place from the game (kiu_probe.h) and feeds the mod's gyro the rate that brings it
//     there (LightGunLoop). Pointing into the Turn Zone at the screen's sides turns the camera.
//     Without a reading it falls back to Gyro.
// Yaw and pitch are of the ray in the room (about the vertical, and its elevation), so rolling the
// controller in the hand mixes nothing.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/input_frame.h"
#include "frame_xr/input/types.h"

namespace FrameXr::Input {

enum class GyroAimMode : uint32_t { Off = 0, Gyro = 1, LightGun = 2 };

constexpr GyroAimMode GyroAimModeFrom(uint32_t value) {
    return value == 1 ? GyroAimMode::Gyro
                      : (value == 2 ? GyroAimMode::LightGun : GyroAimMode::Off);
}

/// Gyro Sensitivity (Settings::values.vr_gyro_sensitivity): the gyroscope sent for Motion
/// Controls times this, in every game; 1.0x is the controller's own rate.
inline constexpr float kGyroSensitivityMin = 0.2f;
inline constexpr float kGyroSensitivityMax = 4.0f;
inline constexpr float kGyroSensitivityDefault = 1.0f;
float ClampGyroSensitivity(float value);

/// Turn Zone (Settings::values.vr_light_gun_turn_zone): the outer share of the top screen, on
/// each side, where Light Gun turns the camera; percent of the width (sides) and of the height
/// (top and bottom). 0 turns it off.
inline constexpr uint32_t kTurnZoneMaxPercent = 30;
inline constexpr uint32_t kTurnZoneDefaultPercent = 10;
/// Turn Speed (Settings::values.vr_light_gun_turn_speed): at 1.0x, pointing at the screen's very
/// edge pushes the reticle past the game's aim box by one screen width a second.
inline constexpr float kTurnSpeedMin = 0.2f;
inline constexpr float kTurnSpeedMax = 4.0f;
inline constexpr float kTurnSpeedDefault = 1.0f;
float ClampTurnSpeed(float value);

/// The motion source Gyro Aim uses (Settings::values.vr_motion_controls' numbering: 1 right,
/// 2 left, 3 headset): Motion Controls' own, or the right controller when that is Off.
constexpr uint32_t GyroAimSource(uint32_t motion_controls) {
    return motion_controls >= 1 && motion_controls <= 3 ? motion_controls : 1u;
}

/// The direction of a ray as yaw (radians, to the right positive, about the room's vertical)
/// and pitch (radians, down positive). The room is OpenXR's LOCAL space: +Y up, -Z ahead.
struct AimAngles {
    float yaw = 0.0f;
    float pitch = 0.0f;
};
AimAngles AnglesOf(const Vec3f& direction);
/// The aim pose's pointing direction (-Z of the pose).
Vec3f AimDirection(const Posef& aim);

/// The mod's gyroscope input, in the 3DS axes of the motion device (deg/s), as a 3DS turned that
/// way reads it (motion.h: z toward the 3DS's top, so a turn to the right is negative z; x toward
/// its left, so aiming down is positive x): positive x moves the reticle down, negative z moves
/// it right, y is not read. AzaharFrame kiufix: z was +right before (left and right swapped).
constexpr Vec3f ModGyro(float right_dps, float down_dps) {
    return {down_dps, 0.0f, -right_dps};
}

/// The mod's own numbers (design.md, fact 3).
inline constexpr float kModDeadZoneDps = 0.25f;
inline constexpr float kModMaxDps = 360.0f;
/// Azahar's gyroscope coefficient (raw = deg/s x this, truncated to s16): the smallest step.
inline constexpr float kHidGyroStepDps = 1.0f / 14.375f;

/// A rate the mod should apply as it is: the dead zone added back (half a raw step more, against
/// the truncation), clamped to what it accepts; tiny rates are sent as zero.
float ModRateFor(float dps);

/// A grip used as a clutch: pressed above 0.60, let go below 0.40 (the mapper's hysteresis).
class GripClutch {
public:
    bool Update(float grip);
    bool held() const {
        return held_;
    }
    void Reset() {
        held_ = false;
    }

private:
    bool held_ = false;
};

/// Gyro: the ray's yaw and pitch rates (deg/s, right and down positive) times the sensitivity.
/// Zero while `clutch`; the pose seen while it is held is not used, so the frame after the clutch
/// is let go starts from the hand's new place with a zero rate (no jump). A gap of more than
/// 100 ms, or the hand not tracked, starts again the same way.
class GyroAimTracker {
public:
    struct Out {
        float right_dps = 0.0f;
        float down_dps = 0.0f;
    };
    Out Update(const HandInput& hand, int64_t time_ns, float sensitivity, bool clutch);
    void Reset() {
        have_last_ = false;
        last_out_ = {};
    }

private:
    bool have_last_ = false;
    AimAngles last_;
    int64_t last_time_ns_ = 0;
    Out last_out_;
};

/// Light Gun's target: where the ray hits the top screen, as a share of its width (u, from the
/// left) and height (v, from the top); outside the screen the values go below 0 or above 1 and
/// are clamped to [-0.5, 1.5]. Not valid when the ray does not reach the screen's plane from the
/// front, or the screen is not shown.
struct LightGunTarget {
    bool valid = false;
    float u = 0.5f;
    float v = 0.5f;
};
LightGunTarget LightGunTargetFor(const HandInput& hand, const PanelQuad& top);

/// AzaharFrame kiufix: Calibrate Aim (per game, optional). Light Gun's target is where the ray
/// hits the top screen; this maps that point to where the player means it, per axis, as
/// u' = a + b u (and v the same), from the player's own pointing: the top screen's centre, then
/// (optionally) its top-left and bottom-right corners, each confirmed with the aiming
/// controller's grip. The centre alone corrects an offset (b = 1); with the corners, a line is
/// fitted through the three (least squares), which corrects the scale too. Computed live from
/// what the player points at, never fitted to presets. Kept in a file of its own beside the
/// game's settings file (config/custom/<title id>.aim, one line: AimCalibrationText), which
/// older builds never read or write, so going back to one and forward again keeps it. Without
/// one, the identity.
struct AimCalibration {
    bool set = false;
    float au = 0.0f, bu = 1.0f; ///< u' = au + bu u
    float av = 0.0f, bv = 1.0f;
    bool scaled = false; ///< fitted with the corners (else an offset only)
};
/// The limits a calibration is kept within: an offset of at most half the screen, a scale of
/// 0.5 to 2 (a point beyond them is a slip of the hand: that step is asked again).
inline constexpr float kCalibrationMaxOffset = 0.5f;
inline constexpr float kCalibrationMinScale = 0.5f;
inline constexpr float kCalibrationMaxScale = 2.0f;
LightGunTarget ApplyCalibration(const AimCalibration& c, const LightGunTarget& t);
/// From the points pointed at (raw u, v) for the wanted ones (0.5, 0.5), (0, 0), (1, 1): the
/// centre alone, or all three. Returns nothing when the result is out of the limits.
std::optional<AimCalibration> FitCalibration(const std::vector<Vec2f>& raw,
                                             const std::vector<Vec2f>& wanted);
/// The calibration as one line of text and back ("v1 au bu av bv scaled"); bad text: none.
std::string AimCalibrationText(const AimCalibration& c);
AimCalibration AimCalibrationFrom(const std::string& text);

/// The steps of Calibrate Aim, each a grip press of the aiming controller with its ray on the
/// top screen: the centre, then the top-left and bottom-right corners. After the centre an
/// offset-only calibration is ready (kept even if the player stops there).
class AimCalibrationFlow {
public:
    enum class Step : uint8_t { Off, Centre, TopLeft, BottomRight };
    struct Out {
        bool changed = false;             ///< the step changed (a prompt to show)
        std::optional<AimCalibration> result; ///< a calibration to keep (after each step)
        bool retry = false;               ///< a press that did not fit: the step is asked again
    };
    void Start();
    void Stop() {
        step_ = Step::Off;
    }
    Step step() const {
        return step_;
    }
    bool active() const {
        return step_ != Step::Off;
    }
    /// Each frame while active: the grip (0 to 1) and the raw target (no calibration).
    Out Update(float grip, const LightGunTarget& raw);

private:
    Step step_ = Step::Off;
    GripClutch grip_;
    bool was_held_ = true; ///< a grip held at the start is no press
    std::vector<Vec2f> raw_;
};
/// AzaharFrame kiufix: the world point at (u, v) of a screen as drawn (flat or curved, as
/// IntersectQuad reads them back): where Light Gun's calibrated target is, for the laser's dot.
Vec3f PointOnQuad(const PanelQuad& quad, float u, float v);
/// The prompt for a step (a toast), and the one when it is done.
const char* AimCalibrationPrompt(AimCalibrationFlow::Step step);
inline constexpr const char* kAimCalibratedToast = "Aim Calibrated";
inline constexpr const char* kAimCalibrationRetryToast = "Calibrate Aim: Try That Point Again";

/// Light Gun's laser sight (the red beam and dot, vis/beam.h): Direct Aim (Light Gun) on, a game
/// on view with the menu closed, and the ray on the top screen. AzaharFrame kiufix: no longer
/// only while the reticle is read from the game (locked): Direct Aim that had fallen back to Gyro
/// (the game's controls not on gyro, say) showed no beam at all, so Laser Sight looked broken.
constexpr bool LaserSight(GyroAimMode mode, bool in_game, const LightGunTarget& target) {
    return mode == GyroAimMode::LightGun && in_game && target.valid && target.u >= 0.0f &&
           target.u <= 1.0f && target.v >= 0.0f && target.v <= 1.0f;
}

/// AzaharFrame kiuguns: the aiming hand's beam in a game. Laser Sight (vr_light_gun_laser, per
/// game, off by default): on, Direct Aim's red laser while LaserSight holds; off, no beam or dot
/// where the ray meets the top screen (the game's reticle shows the aim), with Direct Aim and
/// with the weapon in hand in any aiming. The bottom screen, menus and the room keep their beam.
/// AzaharFrame kiufix: the laser no longer waits for the reticle's reading (LaserSight).
struct AimBeam {
    bool laser = false;    ///< the red beam and dot (from the weapon's muzzle when it is shown)
    bool hide_top = false; ///< no beam or dot over the top screen
};
constexpr AimBeam AimBeamFor(GyroAimMode mode, bool laser_setting, bool weapon_shown,
                             const LightGunTarget& target) {
    AimBeam b;
    b.laser = laser_setting && LaserSight(mode, true, target);
    b.hide_top = !laser_setting && (mode == GyroAimMode::LightGun || weapon_shown);
    return b;
}

/// AzaharFrame kiuguns: Recoil's switch gates every pulse, the timed ones and those on the
/// game's own shots: a controller aims (Aiming not Off) and Recoil is on.
constexpr bool RecoilActive(int aim_hand, bool recoil_setting) {
    return aim_hand >= 0 && recoil_setting;
}

/// AzaharFrame kiufix: Direct Aim and Gyro Aim steer the reticle through the mod's gyro, which
/// the game reads only in the mod's gyro control scheme (kiu_scan.h kSchemeGyro). In any other
/// the controller does nothing to the reticle, silently (the owner's session of 2026-11-14 was in
/// the C-Stick scheme throughout). The toast says so: when Aiming is on and the game's scheme is
/// read and is not the gyro one, again only after it was the gyro one or Aiming was off, and at
/// most once every kRepeatNs.
inline constexpr const char* kAimNeedsGyroToast = "Aiming: Set the Game's Controls to Gyro";
class AimSchemeHint {
public:
    static constexpr int64_t kRepeatNs = 120'000'000'000;
    /// Each frame: Aiming on (a controller aims) in the game, the scheme read (-1 not known).
    /// Returns true when the toast is to show now.
    bool Update(int64_t now_ns, bool aiming, int scheme);

private:
    bool armed_ = true;
    bool shown_once_ = false;
    int64_t shown_ns_ = 0;
};

/// How deep (0 to 1) the target is in the Turn Zone on each side: x negative left, positive
/// right; y negative top, positive bottom. 1 at the screen's edge and beyond; 0 inside.
Vec2f TurnZoneDepth(const LightGunTarget& target, float zone_fraction);

/// Recoil (Settings::values.vr_light_gun_recoil, per game too, on by default): a short strong
/// buzz in the aiming controller on each shot while Gyro Aim steers the reticle in a game
/// (Moonlight XR's light gun recoil: full amplitude, 40 ms, the runtime's frequency).
/// The game's own shots are not read (no guarded shot signal found yet; design.md), so a shot is
/// the fire press (either trigger as ZL/ZR, which also presses L while Gyro Aim is on), then one
/// every kRecoilPaceNs while it is held: Kid Icarus: Uprising's held fire is continuous. A
/// HEURISTIC for the charged shot: a press after at least kChargePauseNs without firing (the
/// weapon charges while it does not fire) gets the longer kChargedPulseNs. A weapon's real fire
/// rate can replace the pace later (one constant).
struct RecoilPulse {
    bool fire = false;
    int64_t duration_ns = 0;
    bool charged = false; ///< AzaharFrame kiufix: the charged-shot heuristic's (a long pulse)
};
/// AzaharFrame kiufix: Recoil's strength for a pulse (0 to 1): Charged Shot's percent for a
/// charged one, Rapid Fire's for the rest (vr_light_gun_recoil_charged / _rapid, per game; 100 %
/// and 50 % by default). 0: no pulse.
float RecoilAmplitude(const RecoilPulse& pulse, int charged_percent, int rapid_percent);
class Recoil {
public:
    static constexpr float kAmplitude = 1.0f;
    static constexpr int64_t kPulseNs = 40'000'000;
    static constexpr int64_t kChargedPulseNs = 90'000'000;
    /// The pace of pulses while fire is held (8 a second).
    static constexpr int64_t kRecoilPaceNs = 125'000'000;
    /// Heuristic: a pause this long before a press reads as a charged shot.
    static constexpr int64_t kChargePauseNs = 1'500'000'000;

    /// Each frame: `fire` held this frame, `active` Gyro Aim steering in a game with Recoil on.
    /// Inactive: nothing, and a press only counts once active again.
    RecoilPulse Update(int64_t now_ns, bool fire, bool active);
    void Reset() {
        held_ = false;
    }

private:
    bool held_ = false;
    bool ever_released_ = false;
    int64_t last_pulse_ns_ = 0;
    int64_t released_ns_ = 0;
};

/// AzaharFrame kiuguns: Recoil from the game's own shots (kiu_weapon.h: the save's shots-fired
/// record, read about once a game frame): a pulse for each read that saw new shots (one pulse
/// for several, a little longer), never for a press that fired nothing. Charged shots are not
/// marked in the game's data that was found, so the heuristic stays: a shot after
/// Recoil::kChargePauseNs without one gets the longer pulse. While the record cannot be read,
/// Recoil's timed pulses are used instead (the caller's choice: `readable`).
class ShotRecoil {
public:
    static constexpr int64_t kBurstPulseNs = 60'000'000;

    /// Each frame: the shots total so far, whether it is being read, `active` as for Recoil.
    RecoilPulse Update(int64_t now_ns, uint64_t total, bool readable, bool active);
    void Reset() {
        has_ = false;
    }

private:
    bool has_ = false;
    uint64_t last_ = 0;
    int64_t last_shot_ns_ = 0;
    bool ever_ = false;
};

/// The reticle as read from the game (kiu_probe.h), in the game's screen pixels.
struct ReticleReading {
    bool valid = false;
    float x = 0.0f; ///< the reticle (+0xA4)
    float y = 0.0f; ///< (+0xA8)
    float w = 0.0f; ///< the screen's size (+0xE0, +0xE8)
    float h = 0.0f;
    float cx = 0.0f; ///< its centre (+0xE4, +0xEC)
    float cy = 0.0f;
    /// The ground aim box applies (+0x81 == 1); in the air the reticle roams the whole screen.
    bool ground = false;
};

/// The ground reticle's box, from the game's own function (0x6D163C in EUR v172): x from 0.7 to
/// 1.3 of the centre's x, y from 0.4 to 1.25 of the centre's y (its lower edge varies 1.25 to 1.8
/// with the game's state, so the smallest is used: never a push into it by mistake).
struct ReticleBox {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};
ReticleBox GroundBox(const ReticleReading& r);

/// Light Gun's control loop (emulator thread, at each gyroscope poll): from the reticle's
/// reading and the target, the gyro rate for the mod.
///
/// The game moves the reticle once per game frame by the rate it read, a frame or two after we
/// send it. A Smith predictor takes that into account: the reticle's place to come is the reading
/// plus what was sent over the last `latency` (kLatencyFrames game frames); the rate closes the
/// gap to the target in kTauFrames game frames. The game frame's length is measured from the
/// readings (they change once per game frame), so 30 and 60 fps both converge without overshoot.
/// The gain (pixels per degree, each axis, sign included) is measured from the reticle's moves
/// against the rates sent; it starts at kInitialGainPxPerDeg and takes in the game's own
/// sensitivity and invert options and Azahar's slow-down stretch.
/// On the ground the target is kept in the box; the Turn Zone adds a push past it (not counted in
/// the prediction, as the box stops it), which the mod turns into a turn.
class LightGunLoop {
public:
    static constexpr float kLatencyFrames = 1.0f;
    static constexpr float kTauFrames = 2.0f;
    static constexpr float kInitialGainPxPerDeg = 12.0f;
    static constexpr float kMinGain = 0.25f;
    static constexpr float kMaxGain = 200.0f;
    /// Errors smaller than this (pixels) send nothing: no dither around the target.
    static constexpr float kHoldPx = 0.5f;

    struct Out {
        float right_dps = 0.0f; ///< for the mod (before ModRateFor)
        float down_dps = 0.0f;
        bool turning = false;
    };

    Out Update(int64_t time_ns, const ReticleReading& reading, const LightGunTarget& target,
               float zone_fraction, float turn_speed, bool clutch);
    void Reset();

    float gain_x() const {
        return gain_[0];
    }
    float gain_y() const {
        return gain_[1];
    }
    float frame_s() const {
        return frame_s_;
    }

private:
    struct Sent {
        int64_t time_ns = 0;
        float deg[2]{}; ///< degrees sent since the previous entry (rate x dt), each axis
    };
    static constexpr std::size_t kHistory = 512; ///< ~5 s of polls

    float SentSince(int axis, int64_t from_ns) const;
    void Learn(int64_t time_ns, const ReticleReading& r, bool pushing);

    std::array<Sent, kHistory> sent_{};
    std::size_t sent_head_ = 0;
    std::size_t sent_count_ = 0;
    bool have_last_ = false;
    int64_t last_time_ns_ = 0;
    float last_rate_[2]{}; ///< the loop's part of the last rate (deg/s), held until now
    // Game frame length, from the readings' changes.
    float last_x_ = 0.0f;
    float last_y_ = 0.0f;
    float count_time_s_ = 0.0f;
    int count_changes_ = 0;
    float frame_s_ = 1.0f / 60.0f;
    // Gain learning: each axis' travel from rest to rest against the degrees sent.
    float gain_[2]{kInitialGainPxPerDeg, kInitialGainPxPerDeg};
    struct Window {
        int64_t start_ns = 0;
        float pos0 = 0.0f;
        bool clean = false;
    };
    Window window_[2]{};
    int64_t mark_ns_[2]{}; ///< the run-away check's start, each axis
    float mark_pos_[2]{};
    int runaway_votes_[2]{};
    int sign_votes_[2]{};
    float last_target_[2]{}; ///< the target at the last poll (pixels)
    // Overshoot guard: the seen error changing sign again and again.
    int error_sign_[2]{};
    int flips_[2]{};
    int64_t flips_since_ns_[2]{};
};

} // namespace FrameXr::Input
