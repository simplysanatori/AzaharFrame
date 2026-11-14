// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiugyro: Gyro Aim, for Kid Icarus: Uprising with Darkshade's C-Stick & Gyro mod
// (v172; handoff/reports/37/kiugyro/design.md has the disassembly). Pure math, no OpenXR, Qt or
// emulator calls, so it is unit tested.
//
// What the mod does with the 3DS gyroscope (AzaharFrame kiufix2: in any of the game's control
// schemes; design.md fact 1 read it as the mod's gyro scheme only, but the owner's headset check
// of 2026-11-14 aimed with the game's controls on C-Stick): once per game frame
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
//   Light Gun (Direct Aim): the reticle goes where the controller's ray hits the top screen.
//     AzaharFrame kiuaim2: absolute. The app writes the pixel the ray hits into the game's reticle
//     (kiu_probe.h, DirectAim) between the game's frames; the mod's gyro gets zero, or the Turn
//     Zone's push. Before, it fed the mod a gyro rate towards the hit with a learnt gain (the
//     relative loop and Calibrate Aim): the gain was guessed and could drift. Pointing into the
//     Turn Zone at the screen's sides turns the camera. Without a reading it falls back to Gyro.
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

/// Turn Zone (Settings::values.vr_light_gun_turn_zone): the band, on each side, where Light Gun
/// turns the camera on the ground; percent of the top screen's width (sides) and of its height
/// (top and bottom). 0 turns it off. AzaharFrame kiuaim3: where it is measured from is Turn Zone
/// From's.
inline constexpr uint32_t kTurnZoneMaxPercent = 30;
inline constexpr uint32_t kTurnZoneDefaultPercent = 10;
/// AzaharFrame kiuaim3: Turn Zone From (Settings::values.vr_light_gun_turn_zone_from, per game):
///   Screen Edge: the outer Turn Zone % of the top screen (kiugyro to kiuaim2). Between the
///     game's aim box and that band the reticle waits at the box's edge and nothing turns.
///   Aim Box (the default): from the edge of the game's aim box outward (AimBoxTurnDepth): 0 at
///     the box's edge, full at the edge plus Turn Zone % of the screen's width (or height), so
///     there is no dead band between the box and the zone. The aim box is the one the C-Stick
///     turns at (GroundBox; MAPPING.md, Turn Zone).
enum class TurnZoneFrom : uint32_t { ScreenEdge = 0, AimBox = 1 };
constexpr TurnZoneFrom TurnZoneFromValue(uint32_t value) {
    return value == 0 ? TurnZoneFrom::ScreenEdge : TurnZoneFrom::AimBox;
}
inline constexpr uint32_t kTurnZoneFromDefault = 1;
/// AzaharFrame kiuaim3: Grip Frees Aim (Settings::values.vr_light_gun_grip, per game): Hold (the
/// default; the aim is free while the aiming controller's grip is held) or Toggle (a press frees
/// it, the next locks it again; AimGrip).
enum class AimGripMode : uint32_t { Hold = 0, Toggle = 1 };
constexpr AimGripMode AimGripModeFrom(uint32_t value) {
    return value == 1 ? AimGripMode::Toggle : AimGripMode::Hold;
}
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

/// AzaharFrame kiuaim2: the one ray Direct Aim aims and draws with. It starts at the weapon's
/// muzzle while the weapon is drawn in the hand (`muzzle`: its place in the aim pose's space, the
/// fit of vis/weapon_fit.h), else at the aim pose, and runs along the aim pose's -Z. The laser
/// sight is drawn along it and the reticle goes where it meets the top screen, so what is seen is
/// what is hit. Nothing in it depends on how the controller is rolled in the hand: the direction
/// is the pose's own; a muzzle beside the pose's axis moves the start (and so the hit) a little
/// as the hand rolls, and the reticle follows that hit, which is the beam's.
Ray AimRay(const Posef& aim, const std::optional<Vec3f>& muzzle);

/// Light Gun's target: where the ray hits the top screen, as a share of its width (u, from the
/// left) and height (v, from the top); outside the screen the values go below 0 or above 1 and
/// are clamped to [-0.5, 1.5]. Not valid when the ray does not reach the screen's plane from the
/// front, or the screen is not shown. AzaharFrame kiuaim2: nor while the ray is on `other` (the
/// bottom screen, in front of the top screen's plane or not): a touch goes there, the reticle
/// stays. `point` is the hit in the room (where the laser ends).
struct LightGunTarget {
    bool valid = false;
    float u = 0.5f;
    float v = 0.5f;
    Vec3f point{};
    /// AzaharFrame kiuaim3: Keep Reticle on Screen (LockedTargetFor): the ray misses the top
    /// screen and (u, v) is the nearest point of its edge (`point` there too). `past_u`, `past_v`:
    /// the sides it is past (-1 left or top, +1 right or bottom, 0 not past); `past_deg`: the
    /// angle between the ray and that edge point; `resting`: past kRestingPastDeg (no turn).
    bool edge = false;
    int past_u = 0;
    int past_v = 0;
    float past_deg = 0.0f;
    bool resting = false;
};
LightGunTarget LightGunTargetFor(const Ray& ray, const PanelQuad& top,
                                 const PanelQuad* other = nullptr);
/// The aim pose's own ray (no weapon); nothing for a hand that is not tracked.
LightGunTarget LightGunTargetFor(const HandInput& hand, const PanelQuad& top);

/// AzaharFrame kiuaim3: Keep Reticle on Screen (Settings::values.vr_light_gun_lock, per game, on
/// by default): while the aiming controller's aim is locked (not freed by its grip), a ray that
/// misses the top screen keeps the reticle on it: the nearest point of the screen's edge to where
/// the ray meets the screen's plane (the bottom screen too: it is not touched then); a ray that
/// does not reach the plane from the front (pointing away, or along it) gets the edge point in
/// its direction from the screen's centre. On the ground that point is past the aim box, so the
/// Turn Zone turns at full depth (DirectAim), as the C-Stick held all the way does. Resting
/// guard: a ray more than kRestingPastDeg past the edge (the arm lowered to rest, say) keeps the
/// reticle at the edge and turns nothing. Not valid only when the screen is not shown.
inline constexpr float kRestingPastDeg = 25.0f;
LightGunTarget LockedTargetFor(const Ray& ray, const PanelQuad& top);

/// AzaharFrame kiufix: the world point at (u, v) of a screen as drawn (flat or curved, as
/// IntersectQuad reads them back). AzaharFrame kiuaim2: where Check Aim's targets are drawn.
Vec3f PointOnQuad(const PanelQuad& quad, float u, float v);

/// AzaharFrame kiuaim2: Check Aim (per game, optional). Direct Aim puts the game's reticle at the
/// pixel of the top screen the ray hits (ReticlePixel). Check Aim measures what is left: where
/// the game shows its reticle for the pixel written (the sprite's own offset, a mod build that
/// draws the picture another way), from the player putting the reticle on targets drawn over the
/// top screen. The map is per axis, offset and scale (u' = u0 + u1 u, v' = v0 + v1 v), with a
/// small skew term (u2 v, v2 u) only when the points need it; identity when the points show
/// nothing to correct. Applied in reverse: the pixel written so the reticle shows on the ray's
/// hit. Kept in config/custom/<title id>.aim as a "v2" line (AimMapText), after the line an
/// older build wrote there ("v1", kept as it was: an older build reads only its first line and
/// ignores a "v2" one), and checked when read: a map out of the limits is not used (logged).
/// A "v1" line alone (the old Calibrate Aim, made for the gyro loop) is not used either.
struct AimMap {
    bool set = false;
    float u[3]{0.0f, 1.0f, 0.0f}; ///< shown u = u[0] + u[1] written u + u[2] written v
    float v[3]{0.0f, 1.0f, 0.0f}; ///< shown v = v[0] + v[1] written v + v[2] written u
    float error_px = 0.0f;        ///< the largest distance left at the points (game pixels)
    int points = 0;
};
/// The limits a map is kept within (the rest is a mis-click or another game: not used).
inline constexpr float kAimMapMaxOffset = 0.1f; ///< at the centre, of the screen
inline constexpr float kAimMapMinScale = 0.85f;
inline constexpr float kAimMapMaxScale = 1.15f;
inline constexpr float kAimMapMaxSkew = 0.05f;
bool AimMapValid(const AimMap& m);
/// The point to write (u, v of the top screen) so that the game shows its reticle at `wanted`.
LightGunTarget ApplyAimMap(const AimMap& m, const LightGunTarget& wanted);
/// From the points written and where the reticle showed (u, v), the screen's pixel size: offset
/// and scale per axis, the skew when it brings the largest distance left down by a third and
/// past 1.5 px; identity (set, nothing to correct) when that is within 1 px of the fit's.
/// Nothing when it is out of the limits or there are fewer than 3 points.
std::optional<AimMap> FitAimMap(const std::vector<Vec2f>& written, const std::vector<Vec2f>& shown,
                                float w_px, float h_px);
/// One line of text and back ("v2 u0 u1 u2 v0 v1 v2 error points"); bad or out of limits: none.
std::string AimMapText(const AimMap& m);
AimMap AimMapFrom(const std::string& line);
/// The old Calibrate Aim's line ("v1 ..."), kept for older builds, recognised (not used).
bool IsOldAimCalibration(const std::string& line);
/// The .aim file's text: `old_text` (the file as it was) keeps its "v1" line, the map follows (or
/// none: Reset). Empty: no file.
std::string AimFileText(const std::string& old_text, const AimMap& m);
/// The map in a file's text (its "v2" line), or none.
AimMap AimMapFromFile(const std::string& text, bool* had_old = nullptr, bool* had_bad = nullptr);

/// Where Check Aim's targets go, as a share of the top screen: the corners of `region` (the area
/// the reticle can reach: ReachableBox, as u, v), its centre, and the middles of its left and
/// right sides (the top screen is wide: 5:3, and the mod's 16:9 and 21:9 builds wider still), each
/// kAimTargetInset of the region in from its edges so the reticle can sit on it.
inline constexpr int kAimTargets = 7;
inline constexpr float kAimTargetInset = 0.08f;
struct AimRegion {
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
};
std::array<Vec2f, kAimTargets> AimTargets(const AimRegion& region);

/// The steps of Check Aim: one target at a time (all shown, the current one marked), each a pull
/// of the aiming controller's trigger with the reticle on it. A pull too far from the target
/// (kAimCheckMaxPx), or, from the third point, too far from the line the others give
/// (kAimCheckOutlierPx), is a slip: that target is asked again; three slips at one target start
/// again from the first (an earlier point may be the bad one). After the last, the map.
inline constexpr float kAimCheckMaxPx = 24.0f;
inline constexpr float kAimCheckOutlierPx = 6.0f;
class AimCheckFlow {
public:
    struct Out {
        bool changed = false; ///< on to the next target (a prompt to show)
        bool retry = false;   ///< that pull did not fit: the same target again
        bool done = false;    ///< all taken: `result`, or nothing when it did not fit
        bool restart = false; ///< three slips at one target: from the first target again
        std::optional<AimMap> result;
    };
    void Start(const AimRegion& region, float w_px, float h_px);
    void Stop() {
        step_ = -1;
    }
    bool active() const {
        return step_ >= 0;
    }
    int step() const {
        return step_;
    }
    const std::array<Vec2f, kAimTargets>& targets() const {
        return targets_;
    }
    /// Each frame while active: `pulled` (the trigger pressed this frame), `written` (where
    /// Direct Aim wrote the reticle, u and v, without a map).
    Out Update(bool pulled, const LightGunTarget& written);

private:
    int step_ = -1;
    std::array<Vec2f, kAimTargets> targets_{};
    float w_ = 400.0f, h_ = 240.0f;
    int retries_ = 0;
    std::vector<Vec2f> written_, shown_;
};
/// The prompts (toasts).
inline constexpr const char* kAimCheckStartToast =
    "Check Aim: Put the Reticle on the Target, Pull the Trigger";
inline constexpr const char* kAimCheckRetryToast = "Check Aim: Try That Target Again";
inline constexpr const char* kAimCheckFailedToast = "Check Aim: Try Again";
inline constexpr const char* kAimCheckResetToast = "Aim Check Reset";
std::string AimCheckStepToast(int step);   ///< "Check Aim: Target 2 of 7"
std::string AimCheckedToast(float error_px); ///< "Aim Checked · Within 1 px"

/// Light Gun's laser sight (the red beam and dot, vis/beam.h): Direct Aim (Light Gun) on, a game
/// on view with the menu closed, and the ray on the top screen. AzaharFrame kiufix: no longer
/// only while the reticle is read from the game (locked): Direct Aim that had fallen back to Gyro
/// (no reticle read, say) showed no beam at all, so Laser Sight looked broken.
/// AzaharFrame kiuaim3: not for Keep Reticle on Screen's edge point (the ray is off the screen).
constexpr bool LaserSight(GyroAimMode mode, bool in_game, const LightGunTarget& target) {
    return mode == GyroAimMode::LightGun && in_game && target.valid && !target.edge &&
           target.u >= 0.0f &&
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

/// How deep (0 to 1) the target is in the Turn Zone on each side: x negative left, positive
/// right; y negative top, positive bottom. 1 at the screen's edge and beyond; 0 inside.
/// (Turn Zone From: Screen Edge.)
Vec2f TurnZoneDepth(const LightGunTarget& target, float zone_fraction);

/// AzaharFrame kiuaim3: a grip that frees Direct Aim (Grip Frees Aim). Hold: free while it is held
/// (GripClutch's hysteresis). Toggle: each press frees or locks, on its release, unless the other
/// controller's grip was down at some point of it (both grips keep their own meaning: the
/// immersive screen's shape); `suspend` (Check Aim running) locks and ignores presses. Changing
/// the mode locks.
class AimGrip {
public:
    struct Out {
        bool free = false;
        bool toggled = false; ///< Toggle: the state changed this frame (a toast)
    };
    Out Update(float grip, bool other_grip, AimGripMode mode, bool suspend);
    void Reset() {
        *this = {};
    }

private:
    GripClutch clutch_;
    AimGripMode mode_ = AimGripMode::Hold;
    bool toggled_free_ = false;
    bool was_held_ = false;
    bool joined_ = false;
};
inline constexpr const char* kAimUnlockedToast = "Aim Unlocked";
inline constexpr const char* kAimLockedToast = "Aim Locked";

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
    /// AzaharFrame kiuaim2: the box's top and bottom, as shares of the centre's y below it
    /// (+0xF4, +0xF0: the game writes them each frame, 0x6D163C: -0.6, and 0.25 to 0.8 with
    /// its state); the defaults when they read out of range.
    float box_top = kBoxTopDefault;
    float box_bottom = kBoxBottomDefault;
    /// AzaharFrame kiuaim2: the game places the reticle itself (+0xD5 != 0: it sets it from the
    /// scene each frame, 0x6D16A4): Direct Aim leaves it alone.
    bool game_places = false;

    static constexpr float kBoxTopDefault = -0.6f;
    static constexpr float kBoxBottomDefault = 0.25f;
};

/// The ground reticle's box, from the game's own function (0x6D163C in EUR v172): x from 0.7 to
/// 1.3 of the centre's x, y from (1 + box_top) to (1 + box_bottom) of the centre's y.
struct ReticleBox {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};
ReticleBox GroundBox(const ReticleReading& r);
/// AzaharFrame kiuaim2: where Direct Aim may put the reticle: the ground box on the ground, the
/// whole screen in the air, kReachInsetPx inside it. The game clamps the reticle to the box each
/// frame and turns the camera by what it clamps off; a reticle written just inside never makes a
/// turn of its own (the Turn Zone makes them, DirectAim).
inline constexpr float kReachInsetPx = 0.5f;
ReticleBox ReachableBox(const ReticleReading& r);
/// AzaharFrame kiuaim2: the game's reticle pixel for a point of the top screen as shown (u from
/// the left, v from the top): the reticle's coordinates are the top screen's own pixels
/// (+0xE0 x +0xE8, 400 x 240; the mod's 16:9 and 21:9 builds draw a wider view into the same
/// pixels, stretched to the screen's shape with the picture), so x = u w, y = v h. And back.
Vec2f ReticlePixel(const ReticleReading& r, float u, float v);
Vec2f ReticleUv(const ReticleReading& r, float x, float y);
AimRegion ReachableRegion(const ReticleReading& r);
/// AzaharFrame kiuaim3: Turn Zone From: Aim Box: how deep (0 to 1) the pixel `px` is in the Turn
/// Zone past the ground box (GroundBox, not inset): 0 at the box's edge and inside, 1 at the edge
/// plus `zone_fraction` of the screen's width (x) or height (y) and beyond; x negative left, y
/// negative up.
Vec2f AimBoxTurnDepth(const ReticleReading& r, Vec2f px, float zone_fraction);
/// The Turn Zone's depth for a target on the ground, from either basis; past the screen's edge
/// (Keep Reticle on Screen) full on the sides it is past, nothing while resting or in the air.
Vec2f TurnDepthFor(const ReticleReading& r, const LightGunTarget& target, float zone_fraction,
                   TurnZoneFrom from);

/// AzaharFrame kiufix2: the C-Stick wins over Direct Aim. While the right stick (the C-Stick,
/// AnalogControl::StickR, after the mapper's dead zone and the other gamepads) is pushed, Direct
/// Aim pauses as for its grip: the stick moves the reticle and, past the ground box, turns the
/// camera, the mod's own way. When it is let go Direct Aim eases back in (DirectAim::kEaseInNs).
/// Gyro Aim adds the hand's turn to the stick and needs no pause.
inline constexpr float kCStickTakesAimMin = 0.01f; ///< past the mapper's dead zone (rescaled)
constexpr bool CStickTakesAim(Vec2f c_stick) {
    return c_stick.x * c_stick.x + c_stick.y * c_stick.y > kCStickTakesAimMin * kCStickTakesAimMin;
}

/// AzaharFrame kiuaim2: Direct Aim (emulation thread, at each HID pad update, about 234 times a
/// second of the game's time, and at each VBlank): the pixel to write into the game's reticle
/// (+0xA4, +0xA8), so the reticle is where the ray hits the top screen, and the Turn Zone's push.
///
/// Absolute: the game moves its reticle by adding to it (the drag of a touch, the C-Stick or the
/// mod's gyro, then the clamp to the box: 0x6D2B5C, 0x6D2C08), so a pixel written between two of
/// its frames is where its next frame starts; nothing sent to the mod (its gyro gets zero), there
/// is nothing to learn and nothing to lose. On the ground the pixel is kept in the box
/// (ReachableBox). Pointing into the Turn Zone (the outer Turn Zone % of the top screen, on the
/// ground) writes the box's edge and sends the mod a gyro rate outward, depth x Turn Speed x
/// kTurnPushDps: the mod drags the reticle past the box and the game turns the camera by the
/// overshoot, as with the C-Stick; the turn follows the depth, in the mod's own units (no gain).
/// Paused (`pause`: its grip, the C-Stick pushed, CStickTakesAim) or while the game places the
/// reticle itself, it writes nothing and sends nothing: the reticle stays (or the stick moves it).
/// After a pause it eases back in: from where the reticle is to the ray's point, a smoothstep
/// over kEaseInNs. A ray off the top screen's surroundings (no target: the bottom screen, or past
/// half a screen out) writes nothing either. AzaharFrame kiuaim3: with Keep Reticle on Screen
/// there is always a target (LockedTargetFor): the edge point, the Turn Zone at full depth past
/// it, and no turn while resting. The Turn Zone is measured from Turn Zone From's basis.
class DirectAim {
public:
    static constexpr int64_t kEaseInNs = 300'000'000;
    /// At the screen's very edge at Turn Speed 1.0x: the mod's gyro at this many degrees a
    /// second outward (one screen width a second at its old starting gain of 12 px a degree, the
    /// Turn Zone's default feel).
    static constexpr float kTurnPushDps = 400.0f / 12.0f;

    struct Out {
        bool write = false; ///< write (x, y) into the reticle
        float x = 0.0f;
        float y = 0.0f;
        float right_dps = 0.0f; ///< the Turn Zone's push, for the mod (before ModRateFor)
        float down_dps = 0.0f;
        bool turning = false;
        bool easing = false;
        bool resting = false; ///< AzaharFrame kiuaim3: at the edge, the ray resting (no turn)
    };
    /// AzaharFrame kiuaim3: the Turn Zone's settings.
    struct Turn {
        float zone_fraction = 0.0f;
        TurnZoneFrom from = TurnZoneFrom::AimBox;
        float speed = 1.0f;
    };
    Out Update(int64_t time_ns, const ReticleReading& reading, const LightGunTarget& target,
               const Turn& turn, bool pause);
    /// Turn Zone From: Screen Edge (kiuaim2's).
    Out Update(int64_t time_ns, const ReticleReading& reading, const LightGunTarget& target,
               float zone_fraction, float turn_speed, bool pause) {
        return Update(time_ns, reading, target,
                      Turn{zone_fraction, TurnZoneFrom::ScreenEdge, turn_speed}, pause);
    }
    void Reset() {
        *this = {};
    }

private:
    bool paused_ = false;
    bool easing_ = false;
    int64_t resume_ns_ = 0;
    float from_x_ = 0.0f, from_y_ = 0.0f;
};

/// AzaharFrame kiuaim2: Direct Aim's target between the XR thread and the emulation thread. The
/// XR thread pushes one sample a display frame (120 a second on the Frame), its pose predicted by
/// the runtime to that frame's display time; the emulation thread asks at each write (about 300
/// a second). Between two samples the latest is carried forward along its motion (from the one
/// before, when they are at most kMaxGapNs apart) for at most kMaxLeadNs, so a write between XR
/// frames keeps the runtime's look-ahead instead of falling up to a frame behind.
class AimSampler {
public:
    static constexpr int64_t kMaxLeadNs = 12'000'000;
    static constexpr int64_t kMaxGapNs = 40'000'000;
    void Push(int64_t host_ns, const LightGunTarget& target);
    LightGunTarget At(int64_t host_ns) const;
    void Reset() {
        *this = {};
    }

private:
    LightGunTarget last_, before_;
    int64_t last_ns_ = 0, before_ns_ = 0;
    bool have_last_ = false, have_before_ = false;
};
} // namespace FrameXr::Input
