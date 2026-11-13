// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: the maths of the immersive screen's placement (IMMERSIVE.md, "Placement"), the
// grip gesture that moves and resizes it, and the per-game file it is saved in. No OpenXR, no
// settings, no emulator: unit tested in src/tests/frame_xr/immersive_layout.cpp.
//
// Head-locked modes (2, 3). The game's camera turns with the head, and the quad shows the
// game's picture. The world stays put when a head turn moves the picture by the same angle, so:
//   - Distance: the quad's size scales with it, the angle does not change (no drift; only the
//     stereo depth of the flat picture in mode 2 changes, nothing at all in mode 3).
//   - Pitch: the quad turns down about the eye by `pitch`, and the same rotation is added to the
//     head rotation given to the game (camera = head x pitch), so the game looks down with it:
//     the world stays where it was and is seen through a lower window (no drift).
//   - Size: the quad is scaled against the game's view, and the head's yaw and pitch given to
//     the game are scaled by `gain` = game angle / screen angle (tangents, at the middle), so
//     the picture still moves with the head at the middle of the screen. The cost: a big head
//     turn turns the game camera `gain` times as far, and the edges of the planar picture move a
//     little against the head. The game's own view never widens with the size, only with the
//     periphery factors: past them a game's scenery can run out (black below the floor of a
//     Super Mario 3D Land level).
//   - Wider View (per game, 1.4x by default; AzaharFrame widerview): the game's periphery factor
//     across (CitraVR's 1.4 at most, `gl_Position.y /= factor`) is the level the player chose, 1.0x
//     to 1.4x. The default size grows with the factor, so a step keeps the picture's scale and gain
//     and only shows more or less around it (AzaharFrame immersive2).
//   - Narrow Screen (per game, 10 % by default, live; AzaharFrame immersive2): the quad is
//     narrower than the game's picture by 0 to 30 % (5 % steps) at the same height, and the yaw
//     gain is the base gain / (1 - narrow), so the HUD at the sides moves in while the middle
//     stays locked. The world looks that much thinner. Narrow Height does the same up and down
//     (the pitch gain / (1 - narrow height)): the world looks that much flatter.
//
// AzaharFrame immersive2: the default size is the world at its own scale (gain 1): the screen is
// as many degrees across as the game draws (its camera times the Wider View factor), so nothing is
// magnified and a head turn moves the picture exactly with the head everywhere on it. The owner's
// headset test of the view-filling default (gain 0.4 to 0.6) found it far too close, and Mario
// Kart 7 stretched: a perspective picture magnified 2x is only right at its middle; 35 degrees to
// the side it is drawn 1.4 to 1.6 times taller than wide. Majora's Mask 3D with the owner's own
// settings (gain 1.03, Wider View 1.4x, Narrow Screen 10 %, 3 m) was "perfect", so that is the
// default. The screen takes the view's shape, not the picture's: the Frame's view is taller than
// 5:3, so the game draws further up and down than across (`factor_y` over `factor`,
// `gl_Position.x` and `.y` divided separately), at the same scale both ways (not stretched).
// It is never bigger than
// filling what either eye sees (kFillMarginDeg past it, DefaultCover): a very wide game camera is
// shrunk to that (gain above 1). A zoom of the game camera does not resize it: only the gain
// follows the live angle (CameraFilter).

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "frame_xr/input/geometry.h"
#include "frame_xr/input/input_frame.h"

namespace FrameXr::Immersive::Layout {

// ---- Tuning (easy to change; see IMMERSIVE.md) ----
inline constexpr float kFactorMax = 1.4f; ///< CitraVR's periphery factor for modes 2 and 3
inline constexpr float kFactorMin = 1.0f; ///< the game's own frame, no periphery
inline constexpr float kWideFactor = 3.0f; ///< mode 1
inline constexpr float kDefaultDistance = 0.75f; ///< metres (CitraVR 0.5); mode 1
/// AzaharFrame immersive2: modes 2 and 3 (the owner's Majora's Mask 3D: 3 m; in mode 2 it is only
/// the flat picture's stereo depth, in mode 3 nothing visible changes).
inline constexpr float kDefaultHeadDistance = 3.0f;
inline constexpr float kMinDistance = 0.3f;
inline constexpr float kMaxDistance = 3.0f;
/// The screen's centre this far above the middle of the view (negative = down), degrees. 0: the
/// filling default is centred, so it reaches as far up as down.
inline constexpr float kDefaultPitchDeg = 0.0f;
inline constexpr float kMinPitchDeg = -40.0f;
inline constexpr float kMaxPitchDeg = 25.0f;
/// Size, times the default size (the world at its own scale, AzaharFrame immersive2).
inline constexpr float kMinSize = 0.6f;
inline constexpr float kMaxSize = 2.5f;
/// AzaharFrame immersive: the default screen's edges this far past the farthest either eye sees,
/// degrees (ViewFov::outer_*), and its half height at least this, degrees.
inline constexpr float kFillMarginDeg = 1.0f;
inline constexpr float kMinFillHalfHeightDeg = 35.0f;
/// AzaharFrame immersive: the game draws at most this much further up and down than its own
/// frame (`factor_y`); a taller view leaves the screen's top and bottom edges in it.
inline constexpr float kMaxFactorY = 2.0f;
/// The rotation gain stays within these (past them the screen keeps its size and the world drifts
/// a little). At the screen's edge, angle e, a head turn moves the picture by
/// (1 + gain^2 tan^2 e) / (1 + tan^2 e) times the turn: 1.25 at gain 1.3 and e = 37 degrees.
inline constexpr float kMinGain = 0.4f;
inline constexpr float kMaxGain = 2.0f;
/// Narrow Screen and Narrow Height (vr_immersive_squeeze, vr_immersive_squeeze_y): the screen
/// this many percent narrower (lower) than the picture, in kNarrowStepPercent steps up to
/// kMaxNarrowPercent. At 30 % the gain of that axis (gain / 0.7) reaches kMaxGain from a base gain
/// of 1.4 up; past that the world drifts a little with the head on that axis.
inline constexpr uint32_t kNarrowStepPercent = 5;
inline constexpr uint32_t kMaxNarrowPercent = 30;
/// Kept clear inside the view on every side when fitting, degrees.
inline constexpr float kFitMarginDeg = 2.5f;
/// The view assumed before the runtime reports one, half angles in degrees.
inline constexpr float kFallbackHalfFovDeg = 45.0f;
/// Grip gesture: the stick beyond this moves the screen (as in the normal placement).
inline constexpr float kStickThreshold = 0.5f;
/// Full stick: distance and size change by e^(rate x seconds).
inline constexpr float kDistanceRate = 1.0f;
inline constexpr float kSizeRate = 0.5f;
inline constexpr float kGripPress = 0.60f;
inline constexpr float kGripRelease = 0.40f;

inline constexpr float kDegToRad = 3.14159265f / 180.0f;

// ---- Immersive Offset (global: it is about the player's lenses, not the game) ----

/// Grip + D-pad: one press turns the immersive screen this far about the eye, degrees.
inline constexpr float kOffsetStepDeg = 0.5f;
/// The offset stays within this either way, on each axis, degrees.
inline constexpr float kMaxOffsetDeg = 10.0f;

/// The global "Immersive Offset" (Settings::values.vr_immersive_offset_x, _y), degrees:
/// positive = right, up. The immersive screen, and in modes 2 and 3 the game camera with it,
/// turn by it about the eye, after the per-game placement: it is an angle, so it is the same at
/// every distance and size, and the world stays where it is (only the window onto it moves).
struct LensOffset {
    float right_deg = 0.0f;
    float up_deg = 0.0f;
    bool operator==(const LensOffset&) const = default;
};
/// Within +-kMaxOffsetDeg, on the kOffsetStepDeg grid; not a number = 0.
LensOffset ClampedOffset(LensOffset o);
/// `o` moved by `right_steps` and `up_steps` presses of kOffsetStepDeg, clamped.
LensOffset NudgedOffset(LensOffset o, int right_steps, int up_steps);
/// The toast for the axis just nudged: "Offset Right 1.5°", "Offset Up 0.5°",
/// "Offset Centred" (UTF-8).
std::string OffsetToast(const LensOffset& o, bool horizontal);

/// The part of the view both eyes see: angles in radians from the head's forward axis (OpenXR
/// XrFovf signs: left and down are negative). `outer_left` and `outer_right` are how far either
/// eye sees to the side (the left eye farther left, the right eye farther right), `outer_up` and
/// `outer_down` how far either eye sees up and down, which the default screen covers.
struct ViewFov {
    float left = -kFallbackHalfFovDeg * kDegToRad;
    float right = kFallbackHalfFovDeg * kDegToRad;
    float up = kFallbackHalfFovDeg * kDegToRad;
    float down = -kFallbackHalfFovDeg * kDegToRad;
    float outer_left = -kFallbackHalfFovDeg * kDegToRad;
    float outer_right = kFallbackHalfFovDeg * kDegToRad;
    float outer_up = kFallbackHalfFovDeg * kDegToRad;
    float outer_down = -kFallbackHalfFovDeg * kDegToRad;
};

/// One eye's FOV and the eye's yaw and pitch relative to the head (canted displays), radians.
struct EyeFov {
    float left, right, up, down;
    float yaw = 0.0f;   ///< positive = turned left
    float pitch = 0.0f; ///< positive = turned up
};

/// What both eyes see: the intersection of the two eyes' FOVs, in head angles, and how far
/// either eye sees to each side (outer_*).
ViewFov BothEyes(const EyeFov& left, const EyeFov& right);

/// AzaharFrame jump: the view the head-locked screen is fitted to (its pitch, its height and the
/// game's up-and-down factor all follow it), held steady. SteamVR on the Frame can report the
/// eyes' views a little differently from one frame to the next (in the owner's Mario Kart 7
/// launches of 2026-11-12 the both-eyes view switched between two shapes 1.9 degrees apart, top
/// or bottom cut), and a screen refitted every frame jumped up and down with them while the head
/// was still. The lenses do not move, so the view is fitted once, from the mean of the first
/// kViewSettleFirstFrames frames, and again only when the mean of a whole block of
/// kViewSettleBlockFrames frames is more than kViewSettleChangeDeg (or kViewSettleIpdM) away: a
/// lasting change (IPD), never a frame-to-frame one. XR thread only.
inline constexpr int kViewSettleFirstFrames = 30;   ///< a quarter second at 120 Hz
inline constexpr int kViewSettleBlockFrames = 240;  ///< two seconds at 120 Hz
inline constexpr float kViewSettleChangeDeg = 0.5f;
inline constexpr float kViewSettleIpdM = 0.001f;

class ViewSettle {
public:
    /// One XR frame's view (BothEyes) and half the distance between the eyes, in metres. True
    /// when the view in use changed: the first frame, the settled first mean when it differs, or
    /// a lasting change.
    bool Add(const ViewFov& view, float half_ipd);
    const ViewFov& View() const {
        return view_;
    }
    float HalfIpd() const {
        return half_ipd_;
    }
    bool Has() const {
        return has_;
    }
    /// How far (radians, the largest of the eight angles) a view is from the one in use.
    float Distance(const ViewFov& view) const;

private:
    void Restart();
    ViewFov view_{};
    float half_ipd_ = 0.0f;
    bool has_ = false;
    bool settled_ = false; ///< the first mean is in use
    int count_ = 0;
    double sum_[9]{};
};

/// What the player chose (saved per game). Defaults: kDefault*.
struct Placement {
    float pitch_deg = kDefaultPitchDeg; ///< relative to the middle of the view
    float distance = kDefaultHeadDistance; ///< mode 1 uses kDefaultDistance (immersive.cpp)
    float size = 1.0f; ///< times the default size
    bool operator==(const Placement&) const = default;
};
Placement Clamped(Placement p);

/// The per-game options of the head-locked screen.
struct Options {
    float wider = kFactorMin; ///< Wider View: the periphery factor (kFactorMin to kFactorMax)
    /// The quad this much narrower than the picture (AzaharFrame kiu: -0.3 to 0.3, negative is
    /// wider: SqueezeForScale of Horizontal Scale).
    float squeeze = 0.0f;
    float squeeze_y = 0.0f;  ///< AzaharFrame immersive2: and this much lower (Vertical Scale)
    /// AzaharFrame immersive: the game camera's vertical angle right now (Match Game Camera: a
    /// zoom), degrees; 0 = the angle the screen is sized with. Only the gain follows it.
    float live_fov_y_deg = 0.0f;
};

/// AzaharFrame immersive2: the old Narrow Screen's or Narrow Height's setting in percent (0 to 30,
/// on the 5 % grid). 1, 2 and 3 are the steps of builds before 1.2.0 (10, 20, 30 %). AzaharFrame
/// kiu: read only to migrate them (ScaleFromNarrow).
uint32_t NarrowPercent(uint32_t setting);
/// The setting as a fraction (NarrowPercent / 100): Options::squeeze.
float SqueezeFor(uint32_t setting);

/// AzaharFrame kiu: Horizontal Scale and Vertical Scale (Settings::values.vr_immersive_scale_x,
/// _y), signed percent: the immersive screen that much larger (positive) or smaller (negative)
/// than the picture on that axis, -kMaxScalePercent to +kMaxScalePercent in kScaleStepPercent
/// steps, 0 by default. They replace Narrow Screen and Narrow Height: Narrow Screen 10 % is
/// Horizontal Scale -10 %. The axis's gain is the base gain over (1 + scale): the middle stays
/// locked at both signs (at -30 % a base gain of 1.4 reaches kMaxGain; at +30 % it is / 1.3).
inline constexpr int kScaleStepPercent = 5;
inline constexpr int kMaxScalePercent = 30;
/// The setting clamped and on the 5 % grid (a hand-edited value too).
int ScalePercent(int setting);
/// The setting as Options::squeeze (squeeze_y): -ScalePercent / 100.
float SqueezeForScale(int setting);
/// The setting after `steps` presses (positive = larger), clamped.
int SteppedScale(int setting, int steps);
/// "Horizontal Scale: -10%", "Horizontal Scale: 0%", "Vertical Scale: +5%".
std::string ScaleToast(int percent, bool vertical);
/// The old Narrow Screen or Narrow Height value as a scale: -NarrowPercent (the legacy 1 to 3 =
/// 10 % steps rule kept), so a game keeps exactly its screen.
int ScaleFromNarrow(uint32_t old_setting);

/// Modes 2 and 3, solved for one frame.
struct HeadLocked {
    float factor = kFactorMax; ///< the game draws this much wider (gl_Position.y /= factor)
    /// AzaharFrame immersive: and this much higher (gl_Position.x /= factor_y; the 3DS top screen
    /// is drawn turned, so x is up and down): at least `factor`, more when the screen is taller
    /// than the picture's shape.
    float factor_y = kFactorMax;
    float tan_half_y = 0.0f;   ///< the quad's half height over its distance
    float aspect = 5.0f / 3.0f; ///< the quad's width over its height (with both narrowings)
    float pitch_rad = 0.0f; ///< the quad and the game camera turn by this about the eye
    float distance = kDefaultDistance;
    /// The head's pitch x gain is given to the game. 1 when the quad has the game's angle:
    /// the base gain / (1 - squeeze_y).
    float gain = 1.0f;
    /// The head's yaw x gain_x is given to the game: the base gain / (1 - squeeze).
    float gain_x = 1.0f;
    /// The Immersive Offset's yaw (positive = left, about +Y). Its pitch is in pitch_rad; the
    /// size is fitted without either.
    float yaw_rad = 0.0f;
    float width() const {
        return 2.0f * distance * tan_half_y * aspect;
    }
    float height() const {
        return 2.0f * distance * tan_half_y;
    }
};

/// The middle of the view, as a pitch (radians).
float ViewMiddlePitch(const ViewFov& view);

/// The largest tan(half height) a quad of `aspect`, turned down by `pitch_rad` about the eye
/// (and seen from `eye_shift` times its distance to the side, mode 2's shared quad), can have
/// with all four corners inside `view` shrunk by `margin_rad` on every side.
float FitTanHalfY(const ViewFov& view, float aspect, float pitch_rad, float margin_rad,
                  float eye_shift = 0.0f);

/// AzaharFrame immersive: the screen that fills the view (the largest default): half its width
/// and height over its distance.
struct Cover {
    float tan_x = 1.0f;
    float tan_y = 1.0f;
};
/// The screen turned down by `pitch_rad` about the eye that covers what either eye sees, with
/// kFillMarginDeg to spare on every side and at least kMinFillHalfHeightDeg high (`eye_shift`:
/// mode 2's shared quad, seen from half the IPD to the side, needs that much more across).
Cover DefaultCover(const ViewFov& view, float pitch_rad, float eye_shift);

// ---- Wider View (AzaharFrame widerview) ----
/// Settings::values.vr_immersive_wider_view_level: 0 = Off (1.0x), 1 to 4 = 1.1x to 1.4x.
inline constexpr uint32_t kMaxWiderLevel = 4;
/// The periphery factor of a level (others are clamped).
float WiderFactor(uint32_t level);
/// "Render Margin: Off", "Render Margin: 1.2x" (AzaharFrame kiu: was Wider View; the name is
/// Names::kRenderMargin).
std::string WiderToast(uint32_t level);
/// The level for a game that had the old Wider View switch on, which let the factor follow the
/// size (about 0.78 x size on the Frame): the nearest step to that, 1.4x when the size is unknown.
uint32_t LevelForOldWiderView(std::optional<float> size);

/// AzaharFrame depth: the game camera's angle and shape the head-locked screen is sized with.
struct CameraShape {
    float fov_y_deg = 0.0f;
    float aspect = 0.0f;
};

/// AzaharFrame immersive: the game camera as measured (Match Game Camera), every quarter second,
/// split in two:
///   - the live angle, for the gain: a new angle is taken once two measurements in a row agree
///     on it (a single odd one is dropped), and nothing changes within 1.5 degrees (a wobbling
///     camera);
///   - the screen's camera, which sizes and shapes the screen. Its angle changes only once a new
///     one held for kSettleCount measurements (2 s: a new scene, a cutscene, a menu), so an aim
///     or a dash zooms the picture on a screen that keeps its size, as on the 3DS. Its shape
///     changes only with the same vertical angle (two measurements; a widescreen mod or the
///     game's own shape option widen the camera at the same height), never with a zoom, so the
///     screen's shape never changes while the game zooms.
/// Angles are clamped to 30 to 110 degrees (a 10 degree close-up would shrink the screen to a
/// stamp and push the gain past its limit).
/// AzaharFrame immersive: when the game camera's rest pose (where the head faced and was, which
/// the game sees as straight ahead) is taken. Nothing about the render size goes into it: an
/// Internal Resolution change, a Dynamic Resolution step or a game's own resolution option keeps
/// it. It is taken again when immersive mode resumes after a pause (the in-game menu, where those
/// changes are made, the bottom screen, the SteamVR dashboard): the old one belonged to where the
/// player faced and sat before, so after turning or leaning in the pause the character was off
/// to one side and, with the head's offset magnified by the game's world scale, the camera swung
/// round it. A recenter (SteamVR's or the right stick hold) takes it at the next head pose at or
/// after its time. XR thread only.
class RestPose {
public:
    /// One XR frame: immersive mode `engaged` now, the head pose `valid` and located at
    /// `pose_time`. True: take the rest pose from this frame's head pose.
    bool Update(bool engaged, bool valid, int64_t pose_time);
    /// A recenter: the rest pose again from the first head pose at or after `after_time`.
    void Recentre(int64_t after_time);
    /// No game: the next one takes a new rest pose.
    void Forget();
    bool Have() const {
        return have_;
    }

private:
    bool have_ = false;
    bool recentre_ = false;
    bool was_engaged_ = false;
    int64_t after_ = 0;
};

class CameraFilter {
public:
    static constexpr int kSettleCount = 8;
    static constexpr float kSameFovDeg = 1.5f;
    static constexpr float kSameAspect = 0.04f; ///< relative
    enum Change : unsigned { None = 0, Live = 1, Screen = 2 };

    /// One measurement: tan of half the vertical angle, and the shape. Returns what changed.
    unsigned Add(float tan_half_y, float aspect);
    void Reset() {
        *this = CameraFilter{};
    }
    /// 0 until the first measurement was taken.
    float ScreenFovDeg() const {
        return screen_fov_;
    }
    float ScreenAspect() const {
        return screen_aspect_;
    }
    float LiveFovDeg() const {
        return live_fov_;
    }

private:
    float screen_fov_ = 0.0f;
    float screen_aspect_ = 0.0f;
    float live_fov_ = 0.0f;
    float live_pending_ = 0.0f;
    int live_count_ = 0;
    float settle_fov_ = 0.0f;
    int settle_count_ = 0;
    float shape_pending_ = 0.0f;
    int shape_count_ = 0;
};
// ---- AzaharFrame fovonce: Match Game Camera's Set Once and Always ----
// Times, not frame counts or angles fitted to a game: the measurements come about four times a
// second (ProjectionTracker), the frames at whatever rate the headset runs.

/// Set Once: the game camera's usual vertical angle, from its measurements over a while of play.
/// A value counts only once it held for kHoldMs (each measurement within kStepDeg of the one
/// before, none missing for kGapMs): cuts, Z-targeting, menus and flashes that last less are left
/// out. Of what counts, the 3-degree band that holds the most time is the game's angle, taken
/// once kEnoughMs counted with it holding kShareEarly of that time, or kPlentyMs with kShareLate
/// (a game with long cutscenes at other angles). The value is the band's time-weighted mean,
/// rounded to a degree, within the setting's 30 to 110. Measuring for kGiveUpMs with no value:
/// Failed (no camera found, or none steady).
class FovEstimate {
public:
    static constexpr int64_t kHoldMs = 2000;
    static constexpr int64_t kGapMs = 1000;
    static constexpr float kStepDeg = 1.5f;
    static constexpr int64_t kSampleMs = 250; ///< the time a measurement stands for at most
    static constexpr int64_t kEnoughMs = 12000;
    static constexpr int64_t kPlentyMs = 20000;
    static constexpr float kShareEarly = 0.6f;
    static constexpr float kShareLate = 0.4f;
    static constexpr int64_t kGiveUpMs = 60000;
    static constexpr int kMinDeg = 30;
    static constexpr int kMaxDeg = 110;
    enum class State { Measuring, Done, Failed };

    /// One measurement, made at `t_ms` (steady clock, rising): the vertical angle in degrees.
    void Add(int64_t t_ms, float fov_deg);
    /// Time spent measuring (a game in a Full mode on view, the menu closed), with or without
    /// measurements. Gives up after kGiveUpMs.
    void Tick(int64_t dt_ms);
    void Reset() {
        *this = FovEstimate{};
    }
    State GetState() const {
        return state_;
    }
    /// Done: the angle, whole degrees.
    int Value() const {
        return value_;
    }
    /// The time that counted, and the band's share of it (the log line).
    int64_t CountedMs() const {
        return counted_ms_;
    }
    float Share() const {
        return share_;
    }
    int64_t MeasuringMs() const {
        return measuring_ms_;
    }

private:
    void Count(float fov_deg, int64_t ms);
    void Evaluate();

    State state_ = State::Measuring;
    int value_ = 0;
    float share_ = 0.0f;
    int64_t measuring_ms_ = 0;
    int64_t counted_ms_ = 0;
    std::array<int64_t, kMaxDeg - kMinDeg + 1> bin_ms_{}; ///< per whole degree
    std::array<double, kMaxDeg - kMinDeg + 1> bin_sum_{}; ///< degrees x ms, for the mean
    // The run being followed: its start, its last measurement, and while it is shorter than
    // kHoldMs its measurements (they count once it held).
    bool in_run_ = false;
    bool run_counts_ = false;
    int64_t run_start_ms_ = 0;
    int64_t last_ms_ = 0;
    float last_deg_ = 0.0f;
    std::vector<std::pair<float, int64_t>> pending_;
};

/// Always: the angle to follow, from the measurements: the median of the last kCount (an even
/// count, at the start, takes the middle one nearer the angle followed so far). A new angle wins
/// with the third measurement in a row (about half a second), however the measurements are
/// spaced (a game that skips some), so a flash or a one-off reading changes nothing; a gradual
/// change (Mario Kart 7 widening with speed) is followed about half a second late. FovSlew then
/// moves to it gently, so a longer hold is not needed: a cut that does win is slewed, never
/// stepped.
class FovFollow {
public:
    static constexpr std::size_t kCount = 5;

    void Add(int64_t t_ms, float fov_deg);
    void Reset() {
        *this = FovFollow{};
    }
    /// 0 before the first measurement.
    float Target() const {
        return target_;
    }

private:
    std::vector<std::pair<int64_t, float>> window_;
    float target_ = 0.0f;
};

/// Eases a value toward its target, so a new camera angle never changes the screen or the gain in
/// one step: critically damped (no overshoot), about 95 % of the way in kEaseMs, and the speed
/// carries on smoothly when the target moves again before it arrived (a gradual change, Mario
/// Kart 7's speed). The first value is taken as it is.
class FovEase {
public:
    static constexpr int64_t kEaseMs = 500;

    float Update(float target, int64_t now_ms);
    void Reset() {
        *this = FovEase{};
    }
    /// Still on its way to the target.
    bool Moving() const {
        return moving_;
    }
    float Value() const {
        return value_;
    }

private:
    bool have_ = false;
    bool moving_ = false;
    float value_ = 0.0f;
    float speed_ = 0.0f; ///< per second
    int64_t last_ms_ = 0;
};

/// Always: moves a value toward its target by at most kMaxStepDeg each displayed frame, the step
/// itself changing by at most kStepChangeDeg a frame (kRampFrames to full speed), and braking so it
/// arrives with a step that small: no kick when it starts or stops, the same both ways (a camera
/// that comes back mid-way turns smoothly). 0.1 degrees a frame is 12 degrees a second at 120 Hz:
/// a 10 degree cut takes about 0.8 s, 40 degrees about 3.5 s. The first value is taken as it is.
class FovSlew {
public:
    static constexpr float kMaxStepDeg = 0.1f;
    static constexpr int kRampFrames = 24;
    static constexpr float kStepChangeDeg = kMaxStepDeg / kRampFrames;

    /// One displayed frame: the value now.
    float Update(float target);
    void Reset() {
        *this = FovSlew{};
    }
    bool Moving() const {
        return moving_;
    }
    float Value() const {
        return value_;
    }
    /// This frame's step (signed, degrees).
    float Step() const {
        return step_;
    }

private:
    bool have_ = false;
    bool moving_ = false;
    float value_ = 0.0f;
    float step_ = 0.0f;
};

/// The angles Match Game Camera hands the solver each frame, from the ones the choice gives
/// (`screen_deg` sizes the screen, `live_deg` is the gain's): Off as they are; Set Once eased
/// (FovEase, only the value it writes ever changes them); Always slewed (FovSlew).
class CameraAngles {
public:
    struct Out {
        float screen_deg;
        float live_deg;
    };
    /// `mode`: 0 Off, 1 Set Once, 2 Always (Settings::MatchGameCamera).
    Out Update(unsigned mode, float screen_deg, float live_deg, int64_t now_ms);
    void Reset() {
        *this = CameraAngles{};
    }

private:
    FovEase ease_screen_, ease_live_;
    FovSlew slew_screen_, slew_live_;
};

/// Match Game Camera on and a measurement made (`measured_fov_deg` > 0): the measured angle and
/// shape. Otherwise the settings' (Immersive Game FOV, Screen Shape), exactly as given.
CameraShape CameraInUse(bool match_camera, float measured_fov_deg, float measured_aspect,
                        float setting_fov_deg, float setting_aspect);

/// `aspect` is the game picture's width over its height (the Screen Shape), `game_fov_y_deg`
/// the camera angle the screen is sized with (options.live_fov_y_deg the one right now). At size
/// 1 the screen is as wide as the game draws (gain 1), in the view's shape, never bigger than
/// filling the view.
/// `offset` turns the solved screen (yaw_rad, and pitch_rad on top of the placement's pitch);
/// the size is fitted for the placement alone, so nudging never resizes the screen.
HeadLocked SolveHeadLocked(const Placement& placement, float game_fov_y_deg, float aspect,
                           const ViewFov& view, float eye_shift = 0.0f,
                           const Options& options = {}, const LensOffset& offset = {});

/// The head-locked quad's rotation about the eye: the offset's yaw, then the pitch.
Input::Quatf QuadRotation(const HeadLocked& solved);

/// A rotation about +X by `pitch` (positive = up).
Input::Quatf PitchRotation(float pitch_rad);

/// `q` with its angle times `gain` (the same axis). gain 1 returns q.
Input::Quatf ScaleRotation(const Input::Quatf& q, float gain);

/// `q` as yaw (about +Y), then pitch (about +X), then roll (about -Z), with the yaw times
/// `yaw_gain` and the pitch times `pitch_gain`; the roll is kept (a picture turned in its plane
/// keeps its angle at any scale). Both gains 1 return q.
Input::Quatf ScaleYawPitch(const Input::Quatf& q, float yaw_gain, float pitch_gain);

/// The rotation given to the game: the head (relative to the rest pose) with the gains, then the
/// screen's rotation (QuadRotation), so the game camera looks where the screen is.
Input::Quatf GameCameraRotation(const Input::Quatf& head_rel, const HeadLocked& solved);

/// The head-locked quad's pose in the head's (VIEW) space for an eye `eye_x` to the side.
Input::Posef HeadLockedQuadPose(const HeadLocked& solved, float eye_x);

/// AzaharFrame turns2: where eye `eye` (0 left, 1 right) is in the head's frame, across.
float EyeX(int eye, float half_ipd);

/// AzaharFrame turns2: mode 2's shared quad is seen from half the IPD to the side, that over its
/// distance (SolveHeadLocked's `eye_shift`); 0 in mode 3, whose quads are each on their eye.
float SharedQuadEyeShift(uint32_t mode, const Placement& placement, float half_ipd);

/// AzaharFrame turns2: the Full modes' quad eye `eye` sees, in the head's (VIEW) space: in mode 3
/// centred on that eye, in mode 2 one quad for both (centred between them).
Input::Posef FullQuadInView(const HeadLocked& solved, uint32_t mode, int eye, float half_ipd);

/// AzaharFrame turns: the head-locked quad (`in_view`, in the head's frame) placed for the
/// picture drawn with the head at `picture`, while the head is at `now` (LOCAL space), for the
/// eye at `eye_x` across in the head's frame (in mode 3 the quad is centred on it, in mode 2 it
/// is not).
///   - `follow_eye` false: where it was in the picture's head frame (Compose(picture, in_view)).
///     The turn is undone, but a real head turns about the neck, so the eyes move sideways with
///     it, and the quad (at its distance) moves against the scenery it shows as if the scenery
///     were that near: about r / distance of the turn (r, the eyes ahead of the neck), 23 % at
///     Mario Kart 7's 0.37 m, until the next picture puts it back: a double image on fast turns.
///   - `follow_eye` true: the picture's orientation, about the eye where it is now: the eye sees
///     the quad exactly as it saw it when the picture was drawn, in the same directions, from
///     wherever it has moved, as a world far away is (what a rotational reprojection does). The
///     game camera follows the head's position on the next picture, as before. With the head at
///     `picture` it is the placement above, whatever the quad and the eye.
/// AzaharFrame turns2: about each eye in mode 2 too (it was about the middle of the head, so each
/// eye was off by half the IPD / distance of the turn: 1 % at 3 m, 11 % at 0.3 m).
Input::Posef AnchoredQuadPose(const Input::Posef& picture, const Input::Posef& now,
                              const Input::Posef& in_view, float eye_x, bool follow_eye);

/// AzaharFrame turns2: the Full modes' anchored quad for eye `eye` (Immersive::Screens):
/// AnchoredQuadPose of FullQuadInView about EyeX.
Input::Posef AnchoredFullQuad(const HeadLocked& solved, uint32_t mode, int eye, float half_ipd,
                              const Input::Posef& picture, const Input::Posef& now,
                              bool follow_eye);

/// AzaharFrame turns2: how far far scenery at the middle of `placed` (a quad in LOCAL space),
/// seen from `eye_now` (LOCAL), is from where that eye saw it when the picture was drawn (the
/// quad `in_view` in the head frame `picture`, seen from the eye at `eye_x` in it): radians
/// across and up, in the picture quad's frame. 0 and 0 when the screen keeps its scenery still.
Input::Vec2f FarSceneryError(const Input::Posef& picture, const Input::Posef& in_view, float eye_x,
                             const Input::Posef& placed, const Input::Vec3f& eye_now);

/// Mode 1: the wide screen around the rest pose (room-fixed), at `pitch` (the player's pitch
/// plus the middle of the view) and `distance`, turned by `yaw` (positive = left; the
/// Immersive Offset).
Input::Posef WideQuadPose(const Input::Posef& rest, float pitch_rad, float distance,
                          float yaw_rad = 0.0f);

/// Mode 1 with the bottom screen shown: below the wide screen's centre, nearer than it (so it is
/// drawn over it, never through it), with the normal bottom screen's angular size.
struct BottomQuad {
    Input::Posef pose;
    float width = 0.0f;
    float height = 0.0f;
};
BottomQuad WideBottomQuad(const Input::Posef& rest, float wide_pitch_rad, float wide_distance,
                          float yaw_rad = 0.0f);

// ---- Which screens show ----

struct ScreenState {
    uint32_t mode = 0;          ///< vr_immersive_mode
    bool live = false;          ///< a game shows its screens and the session is visible
    bool hide_bottom = false;   ///< "Hide Bottom Screen" (per game)
    bool bottom_toggled = false; ///< the bumper chord flipped the bottom screen
    bool bottom_focused = false; ///< the bottom screen is the screen in focus
    bool menu_open = false;     ///< the in-game menu shows
    bool view_valid = false;    ///< the head pose is known
};

struct ScreenDecision {
    bool immersive = false;   ///< immersive mode is on this frame (not paused)
    bool show_bottom = false; ///< the bottom screen is drawn (and can be touched)
};

/// The bottom screen starts hidden with "Hide Bottom Screen" or any immersive mode; the bumper
/// chord flips it; it always shows while in focus. Immersive mode pauses (modes 2 and 3) while
/// the bottom screen shows or the in-game menu is open, and (all modes) while the bottom screen
/// is in focus. A hidden bottom screen is never drawn.
ScreenDecision Decide(const ScreenState& s);

// ---- The grip gesture ----

/// One grip at a time. A grip squeezed (not one already down) starts it; that hand's aim moves
/// the screen up and down (the point under the ray stays under it, on the sphere of the screen's
/// distance around `head`); that hand's stick Y moves it farther (up) or nearer, X makes it
/// bigger (right) or smaller. While it runs the hand's grip and stick do not reach the game.
class Adjuster {
public:
    struct Result {
        bool active = false;  ///< a hand holds the screen this frame
        int hand = -1;        ///< which (0 left, 1 right) while active
        bool changed = false; ///< the placement changed this frame
        bool released = false; ///< the grip was let go this frame (save now)
    };

    /// `head`: the frame the screen is placed in (the head for modes 2 and 3, the rest pose for
    /// mode 1), in the aim poses' space. `enabled` false cancels a gesture in progress.
    Result Update(const Input::InputFrame& in, const Input::Posef& head, float dt,
                  bool enabled, Placement& placement);
    void Cancel();
    bool Active() const {
        return hand_ >= 0;
    }

private:
    int hand_ = -1;
    float start_elevation_ = 0.0f;
    float start_pitch_deg_ = 0.0f;
    float start_radius_ = kDefaultDistance;
    bool grip_down_[2]{};
};

/// The elevation (radians) of the point where `aim` (in `head`'s space) hits the sphere of
/// `radius` around `head`, or of the ray's direction when it does not.
float AimElevation(const Input::Posef& head, const Input::Posef& aim, float radius);

// ---- The per-game file ----

/// Lines "<title id, 16 hex digits> <kind> <pitch degrees> <distance m> <size>"; '#' starts a
/// comment. Kinds: "world" (modes 2 and 3; AzaharFrame immersive2: sizes against the true-scale
/// default), "wide" (mode 1), and "screen", "view" and "head" (modes 2 and 3, saved by earlier
/// builds: their sizes are relative to other defaults (the view-filling one of the 1.2.0 test
/// builds, smaller ones before), so OldSize() and the game's default size is used; the pitch and
/// distance are kept). Unknown or broken lines are kept out (and dropped when saving).
class Store {
public:
    void Parse(const std::string& text);
    std::string Format() const;
    std::optional<Placement> Get(uint64_t title_id, bool wide) const;
    /// The head-locked placement of this game is from an earlier build ("view" or "head"): its
    /// size was relative to another default.
    bool OldSize(uint64_t title_id) const;
    void Set(uint64_t title_id, bool wide, const Placement& placement);
    void Erase(uint64_t title_id, bool wide);
    std::size_t Size() const {
        return entries_.size();
    }

private:
    struct Entry {
        Placement placement;
        bool old_size = false;
    };
    std::map<std::string, Entry> entries_;
};

} // namespace FrameXr::Immersive::Layout
