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
//   - Render Margin (per game, 1.4x by default; AzaharFrame widerview, kiu): the game's periphery factor
//     across (CitraVR's 1.4 at most, `gl_Position.y /= factor`) is the level the player chose, 1.0x
//     to 1.4x. The default size grows with the factor, so a step keeps the picture's scale and gain
//     and only shows more or less around it (AzaharFrame immersive2). AzaharFrame fovaxis: a level
//     per axis (Options::wider_y up and down when it differs; frame_xr/render_margin.h).
//   - Horizontal Scale (per game, 0 % by default, live; AzaharFrame immersive2, kiu: signed,
//     positive is larger): the quad is that much wider or narrower than the game's picture
//     (-100 to +100 %, 5 % steps; AzaharFrame fov100, -30 to +30 % before; below -30 % the size
//     follows a curve that never reaches 0, ScaleFactor) at the same height, and the yaw gain is
//     the base gain / (1 + scale), so a narrower screen brings the HUD at the sides in while the
//     middle stays locked.
//     The world looks that much thinner. Vertical Scale does the same up and down (the pitch
//     gain / (1 + vertical scale)): the world looks that much flatter.
//     AzaharFrame fovscale: that is Stretch. Field of View (the default, per game) keeps the
//     same quad and has the game draw that much more (or less) on that axis instead, through the
//     periphery factors (ScaleSplit): the picture keeps one scale both ways, both gains stay the
//     base gain, a head roll is a plain rotation, and the HUD keeps its place. Only what the
//     factor cannot take (above kFovFactorMax, mode 2's menu rule) stays a stretch.
//     AzaharFrame fovaxis: below the game's own frame Field of View crops (kCropFactorMin), and
//     the caps against warping (SplitScales) keep Stretch's two axes within kMaxStretchRatio and
//     each scale's part of a gain within kScaleGainMin to kScaleGainMax; past them: May Warp.
//
// AzaharFrame immersive2: the default size is the world at its own scale (gain 1): the screen is
// as many degrees across as the game draws (its camera times the Render Margin factor), so nothing is
// magnified and a head turn moves the picture exactly with the head everywhere on it. The owner's
// headset test of the view-filling default (gain 0.4 to 0.6) found it far too close, and Mario
// Kart 7 stretched: a perspective picture magnified 2x is only right at its middle; 35 degrees to
// the side it is drawn 1.4 to 1.6 times taller than wide. Majora's Mask 3D with the owner's own
// settings (gain 1.03, Render Margin 1.4x, Horizontal Scale -10 %, 3 m) was "perfect", so that is the
// default. AzaharFrame oot: the screen takes the picture's own shape, and Render Margin pads it
// evenly both ways (`factor_y` = `factor`, Off: exactly the game's frame; it took the view's
// shape before, the game drawing more up and down than across), never stretched.
// It is never bigger than
// filling what either eye sees (kFillMarginDeg past it, DefaultCover): a very wide game camera is
// shrunk to that (gain above 1). A zoom of the game camera does not resize it: only the gain
// follows the live angle (CameraFilter).

#pragma once

#include <limits>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "frame_xr/immersive_names.h" // AzaharFrame natural
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
/// The rotation gain stays within these (past them the screen keeps its size and the world drifts
/// a little). At the screen's edge, angle e, a head turn moves the picture by
/// (1 + gain^2 tan^2 e) / (1 + tan^2 e) times the turn: 1.25 at gain 1.3 and e = 37 degrees.
inline constexpr float kMinGain = 0.4f;
/// 1.2.0 fix lane B (session 651, triage item 5): under this gain (either axis) a head turn
/// turns the game camera so little that the screen seems to follow the head (Super Mario 3D Land
/// at gain 0.65/0.73 after a resize, 0.41/0.52 in mode 2): the resize toast adds "May Feel
/// Locked" (MayFeelLocked). 0.6, not 0.7: the owner's own settled MK7 (0.65/0.62) and Majora's
/// Mask (0.72/0.64) setups are fine and must not warn; mode 2's 0.41/0.52 does.
inline constexpr float kLockedGain = 0.6f;
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

/// 1.2.0 fix lane B (session 651, triage item 6): both grips + D-pad steps the scales, latched.
/// Each raw grip is down past kGripPress and up only under kGripRelease (AzaharFrame kiu2's
/// hysteresis); once both are down while a grip holds the immersive screen, the chord holds
/// until BOTH are up, so a second grip that arrives late, or one that eases off, no longer drops
/// the D-pad to the single-grip chord (3D Depth or Render Margin, and the offset), which showed
/// "3D Depth: 100%" three times in the owner's session.
struct ScaleChord {
    bool down[2]{};
    bool latched = false;
    /// This frame's raw grips (left, right) and whether a grip holds the immersive screen: true
    /// while the D-pad steps the scales.
    bool Update(float grip_left, float grip_right, bool holding) {
        const float grips[2]{grip_left, grip_right};
        for (int h = 0; h < 2; ++h) {
            down[h] = down[h] ? grips[h] > kGripRelease : grips[h] > kGripPress;
        }
        if (holding && down[0] && down[1]) {
            latched = true;
        } else if (!down[0] && !down[1]) {
            latched = false;
        }
        return latched;
    }
};

inline constexpr float kDegToRad = 3.14159265f / 180.0f;
inline constexpr float kRadToDeg = 180.0f / 3.14159265f; ///< AzaharFrame stereo

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
    /// AzaharFrame kiu2: the factor the screen is sized with, when it differs from `wider` (0:
    /// `wider`). Mode 2's menu rule draws with factor 1 but keeps the padded screen's size, so the
    /// game's own frame fills what the padded picture filled (MenuExact); the gain stays as if
    /// sized with `wider`, so the head turns the game as before.
    float size_wider = 0.0f;
    /// The quad this much narrower than the picture (AzaharFrame kiu: -0.3 to 0.3, negative is
    /// wider: SqueezeForScale of Horizontal Scale; AzaharFrame fov100: -1 to 0.74).
    float squeeze = 0.0f;
    float squeeze_y = 0.0f;  ///< AzaharFrame immersive2: and this much lower (Vertical Scale)
    /// AzaharFrame fovaxis: Render Margin up and down when it differs from `wider` (0: `wider`;
    /// kFactorMin to kFactorMax), and the factor the screen's height is sized with (0: the one
    /// across, or `wider_y`). The screen is sized with each axis's own factor, so the picture
    /// keeps one scale both ways (the world its shape); mode 2's menu rule sizes both with the
    /// smaller one (MenuSizeFactors).
    float wider_y = 0.0f;
    float size_wider_y = 0.0f;
    /// AzaharFrame fovaxis: the caps against warping (0: none, every build before; 1: on; mode
    /// 2's menu rule eases them off with its factor): Stretch's axes within kMaxStretchRatio of
    /// each other and the gains the scales give within kScaleGainMin to kScaleGainMax
    /// (SplitScales).
    float caps = 0.0f;
    /// AzaharFrame immersive: the game camera's vertical angle right now (Match Game Camera: a
    /// zoom), degrees; 0 = the angle the screen is sized with. Only the gain follows it.
    float live_fov_y_deg = 0.0f;
    /// AzaharFrame fovscale: the share of the scales the game draws as field of view (ScaleSplit:
    /// 0 Stretch, 1 Field of View; mode 2's menu rule eases it to 0 with the factor).
    float scale_fov = 0.0f;
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
/// Horizontal Scale -10 %. The axis's gain is the base gain over the screen's size on that axis
/// (ScaleFactor): the middle stays locked at both signs (at -30 % a base gain of 1.4 reaches
/// kMaxGain; at +30 % it is / 1.3).
/// AzaharFrame fov100: -100 to +100 % (-30 to +30 % before). From -kLinearScalePercent up the
/// size is 1 + scale / 100, exactly as before (+100 % is twice the size), so a saved value keeps
/// its screen. Below -30 % a straight line would reach 0 at -100 %; there each step shrinks the
/// screen by the same share instead (0.7 x e^((scale + 30) / 70): the line's value and slope at
/// -30 %, no kink), down to kMinScaleFactor (0.26) at -100 %, so the HUD always fits.
inline constexpr int kScaleStepPercent = 5;
inline constexpr int kMaxScalePercent = 100;
/// The range of builds before fov100, where the size is the straight line 1 + scale / 100.
inline constexpr int kLinearScalePercent = 30;
/// ScaleFactor at -kMaxScalePercent (0.7 / e) and at +kMaxScalePercent.
inline constexpr float kMinScaleFactor = 0.25751561f;
inline constexpr float kMaxScaleFactor = 2.0f;
/// The setting clamped and on the 5 % grid (a hand-edited value too).
int ScalePercent(int setting);
/// AzaharFrame fov100: the screen's size on that axis over the picture's for the setting
/// (1 - SqueezeForScale): 1 + ScalePercent / 100 from -30 % up, the curve below it.
float ScaleFactor(int setting);
/// The setting as Options::squeeze (squeeze_y): -ScalePercent / 100 from -30 % up (bit for bit
/// as every earlier build), 1 - ScaleFactor below.
float SqueezeForScale(int setting);
/// The setting after `steps` presses (positive = larger), clamped.
int SteppedScale(int setting, int steps);
/// "Horizontal Scale: -10%", "Horizontal Scale: 0%", "Vertical Scale: +5%"; 1.2.0 fix lane B: at
/// an end "Vertical Scale: +100% (Max)", "Horizontal Scale: -100% (Min)".
std::string ScaleToast(int percent, bool vertical);
/// 1.2.0 fix lane B: both axes in one toast (a diagonal on the D-pad; one toast a frame, the
/// last wins): "Horizontal Scale: +45% \u00b7 Vertical Scale: +100% (Max)".
std::string ScaleToasts(int percent_x, int percent_y);
/// The old Narrow Screen or Narrow Height value as a scale: -NarrowPercent (the legacy 1 to 3 =
/// 10 % steps rule kept), so a game keeps exactly its screen.
int ScaleFromNarrow(uint32_t old_setting);

/// AzaharFrame fovscale: what the scales do (Settings::values.vr_immersive_scale_mode, per game).
/// The quad is the same in both: Stretch shows the same picture on it, stretched (the gain of
/// that axis follows); Field of View has the game draw that much more (or less) on that axis, so
/// the picture keeps one scale both ways and a head roll is a plain rotation (IMMERSIVE.md).
enum class ScaleMode : uint32_t { Stretch = 0, FieldOfView = 1 };
/// The highest factor Field of View draws on one axis, Render Margin included. AzaharFrame
/// fovaxis: 1.6 (the owner's cap after session 590: fov100's 2.8x spread the same pixels over a
/// 141-degree view for a 90-degree camera, edges stretched about 9 times). Past it the screen
/// still grows with the scale and the rest is a stretch (and the row says "May Warp"). The
/// shader has no limit of its own (it divides clip x and y by it: the tangents of the view
/// widen by it, so the view stays under 180 degrees at any factor).
inline constexpr float kFovFactorMax = 1.6f;
/// AzaharFrame fovaxis: below the game's own frame Field of View crops: it draws less than the
/// frame (a factor under 1, the world magnified to the smaller screen, its shape kept; the HUD
/// inside the frame goes past the edges with the world). This is the lowest it draws: under
/// the smallest scale at Render Margin Off (kMinScaleFactor), so the settings never reach it.
inline constexpr float kCropFactorMin = 0.25f;
/// AzaharFrame fovaxis: the caps against warping (session 590's findings, fix 7). Stretch keeps
/// the picture at most kMaxStretchRatio times as stretched on one axis as on the other (what is
/// past it is drawn as field of view instead, half on each axis, within kFactorMin and
/// kFovFactorMax, so the HUD stays in the picture), and the scales change an axis's gain (base
/// gain over the stretch) by at most kScaleGainMax and at least kScaleGainMin: the stretch kept
/// within 1/kScaleGainMax to 1/kScaleGainMin the same way, and past that the gain stops
/// following it (the world drifts a little on that axis). The size's own gain (Placement::size,
/// the game camera) keeps kMinGain and kMaxGain. Settings past what the caps can take show "May
/// Warp" on the scale's row (ScalesMayWarp).
inline constexpr float kMaxStretchRatio = 1.5f;
inline constexpr float kScaleGainMin = 0.6f;
inline constexpr float kScaleGainMax = 1.6f;
/// One axis of a scale split between what the game draws and a stretch of the picture.
struct AxisSplit {
    float drawn = kFactorMin; ///< the periphery factor the game draws with on this axis
    float stretch = 1.0f;     ///< the picture shown this much larger on this axis (1: true)
    /// AzaharFrame fovaxis: the screen this much of what the scale asks on this axis: 1, but
    /// under 1 where Stretch's Render Margin padding is trimmed at the draw's limit
    /// (SplitScales). drawn x stretch = factor x k x screen.
    float screen = 1.0f;
};
/// AzaharFrame fovscale: the scale `k` (1 + scale / 100; the quad's size on this axis over the
/// picture's) of an axis whose Render Margin factor is `factor`, taken as field of view by the
/// share `fov` (0 Stretch, 1 Field of View; mode 2's menu rule eases it to 0). Field of View
/// draws factor x k, within kCropFactorMin (AzaharFrame fovaxis: below the game's own frame it
/// crops; fovscale and fov100 stopped at kFactorMin and stretched the rest) and kFovFactorMax;
/// what it cannot take stays a stretch, so drawn x stretch = factor x k always, with no jump at
/// a limit. Stretch: {factor, k} exactly.
AxisSplit ScaleSplit(float factor, float k, float fov);
/// AzaharFrame fovaxis: in Stretch the caps act on the game's own frame (factor 1) and the
/// Render Margin padding is put around that, so a margin step never moves the frame; the
/// padding is trimmed where the draw would pass kFovFactorMax (AxisSplit::screen).
/// Both axes (ScaleSplit each), then the caps by the weight `caps`
/// (Options::caps) on what Stretch leaves (the share 1 - `fov`): the stretch on each axis within
/// 1/kScaleGainMax to 1/kScaleGainMin and the two within kMaxStretchRatio, by drawing more or
/// less on an axis (within kFactorMin and kFovFactorMax, or the factor already drawn), so
/// drawn x stretch stays factor x k. `warp_x`, `warp_y`: what is left past the caps, on that
/// axis (a stretch past them; in Field of View any stretch: the true shape lost past
/// kFovFactorMax).
struct ScalePicture {
    AxisSplit x;
    AxisSplit y;
    bool warp_x = false;
    bool warp_y = false;
};
ScalePicture SplitScales(float factor_x, float k_x, float factor_y, float k_y, float fov,
                         float caps);
/// AzaharFrame fovaxis: the gain's divisor for an axis's stretch: the stretch, within
/// 1/kScaleGainMax to 1/kScaleGainMin by the weight `caps` (on a log scale).
float GainStretch(float stretch, float caps);
/// AzaharFrame fovaxis: the "May Warp" note on Horizontal and Vertical Scale's rows: the
/// settings (Render Margin's levels, the scales in percent, the mode) past what the caps can
/// take on that axis, at the default screen (SplitScales with the caps on).
struct WarpAxes {
    bool x = false;
    bool y = false;
    bool operator==(const WarpAxes&) const = default;
};
WarpAxes ScalesMayWarp(uint32_t level_x, uint32_t level_y, int scale_x, int scale_y,
                       ScaleMode mode);
/// AzaharFrame fovaxis: the periphery factor a Render Margin level gives in `mode`: Render
/// Margin applies with Stretch only (WiderFactor); in Field of View the scales alone decide what
/// is drawn, so the margin is 1.0x there (its setting kept for Stretch).
float MarginFactor(uint32_t level, ScaleMode mode);
/// The mode's name: "Stretch", "Field of View".
const char* ScaleModeName(ScaleMode mode);

/// AzaharFrame fovaxis: what grip + D-pad up or down does while one grip holds the immersive
/// screen in a Full mode: Render Margin's two axes with Stretch, 3D Depth with Field of View
/// (where Render Margin's rows are hidden; the owner's rule).
enum class GripUpDown { RenderMargin, Depth3d };
GripUpDown GripUpDownFor(ScaleMode mode);
/// The 3D Depth row's step (Menu::SliderFor(Id::Depth3d)): 5 % from 0 to 100 %.
inline constexpr int kDepthStepPercent = 5;
/// `percent` (0 to 100) moved `steps` steps (up +, down -): the first step goes to the next
/// multiple of kDepthStepPercent, as the 3D Factor shortcuts do; within 0 to 100.
int SteppedDepth(int percent, int steps);

/// Modes 2 and 3, solved for one frame.
struct HeadLocked {
    float factor = kFactorMax; ///< the game draws this much wider (gl_Position.y /= factor)
    /// AzaharFrame immersive: and this much higher (gl_Position.x /= factor_y; the 3DS top screen
    /// is drawn turned, so x is up and down): at least `factor`, more when the screen is taller
    /// than the picture's shape.
    float factor_y = kFactorMax;
    float tan_half_y = 0.0f;   ///< the quad's half height over its distance
    float aspect = 5.0f / 3.0f; ///< the quad's width over its height (with both scales)
    float pitch_rad = 0.0f; ///< the quad and the game camera turn by this about the eye
    float distance = kDefaultDistance;
    /// The head's pitch x gain is given to the game. 1 when the quad has the game's angle:
    /// the base gain / (1 - squeeze_y).
    float gain = 1.0f;
    /// The head's yaw x gain_x is given to the game: the base gain / (1 - squeeze).
    /// AzaharFrame fovscale: over the stretch left (`stretch_x`, `stretch_y`), which is the
    /// scale itself in Stretch and 1 in Field of View within its limits (ScaleSplit).
    float gain_x = 1.0f;
    float stretch_x = 1.0f; ///< AzaharFrame fovscale: the picture stretched this much across
    float stretch_y = 1.0f; ///< and this much up and down
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

/// 1.2.0 fix lane B: either gain under kLockedGain (the resize toast's "May Feel Locked").
inline bool MayFeelLocked(const HeadLocked& s) {
    return std::min(s.gain, s.gain_x) < kLockedGain;
}

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
/// AzaharFrame fovaxis: grip + D-pad up or down with Render Margin's axes at different levels:
/// "Render Margin: H 1.3x, V 1.1x" (the same level: WiderToast's "Render Margin: 1.2x").
std::string MarginToast(uint32_t level_x, uint32_t level_y);
/// AzaharFrame fovaxis: mode 2's menu rule (MenuExact) draws the game's own frame on both axes
/// at weight 1. With the axes' margins `factor_x`, `factor_y` unequal, a screen kept at its
/// padded size would stretch that frame by their ratio, so the screen is sized with the smaller
/// of the two on both axes at weight 1 (eased in between); equal margins keep the padded size
/// (as before).
struct SizeFactors {
    float x = kFactorMax;
    float y = kFactorMax;
};
SizeFactors MenuSizeFactors(float factor_x, float factor_y, float weight);

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

/// Always: moves a value toward its target at most kMaxSpeedDegS (12 degrees a second), in time:
/// each displayed frame moves it by its speed times that frame's real time (the display times'
/// difference), so it feels the same at 72, 90, 120 or 144 Hz and when frames drop. The speed
/// changes by at most kAccelDegS2 (full speed in kRampS), and it brakes so it arrives that slowly:
/// no kick when it starts or stops, the same both ways (a camera that comes back mid-way turns
/// smoothly). A frame's step is never more than kMaxStepDeg (a frame longer than kMaxFrameS, a
/// hitch, moves it as one of kMaxFrameS). A 10 degree cut takes about a second, 40 degrees about
/// 3.5 s. The first value is taken as it is; a frame with no time passed moves nothing.
class FovSlew {
public:
    static constexpr float kMaxSpeedDegS = 12.0f;
    static constexpr float kRampS = 0.2f;
    static constexpr float kAccelDegS2 = kMaxSpeedDegS / kRampS;
    /// The per-frame step cap: 12 degrees a second for a frame of 1/48 s.
    static constexpr float kMaxStepDeg = 0.25f;
    static constexpr float kMaxFrameS = kMaxStepDeg / kMaxSpeedDegS;

    /// One displayed frame at display time `now_ns` (nanoseconds, any monotonic clock): the
    /// value now.
    float Update(float target, int64_t now_ns);
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
    /// The speed now (signed, degrees a second).
    float Speed() const {
        return speed_;
    }

private:
    bool have_ = false;
    bool moving_ = false;
    float value_ = 0.0f;
    float step_ = 0.0f;
    float speed_ = 0.0f;
    int64_t last_ns_ = 0;
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
    /// `mode`: 0 Off, 1 Set Once, 2 Always (Settings::MatchGameCamera). `now_ms` times Set
    /// Once's ease, `display_ns` (the frame's display time) Always's slew.
    Out Update(unsigned mode, float screen_deg, float live_deg, int64_t now_ms,
               int64_t display_ns);
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

/// AzaharFrame fovaxis: Fill View, set once (no mode). 1.2.0: across only, at kFillAcrossShare
/// of the view. The placement size and Horizontal Scale that make the screen's width exactly
/// kFillAcrossShare of the half width (tangent) that would cover what either eye sees
/// (DefaultCover at the screen's own pitch, with kFillMarginDeg to spare, and mode 2's
/// `eye_shift`), from the screen as SolveHeadLocked gives it with `options` (its scales replaced;
/// each step solved, the mode and the caps included). Vertical Scale stays the player's
/// `scale_y` (on the grid); nothing searches the height. Horizontal Scale is kFillScalePercent,
/// or the largest 5 % step under it whose width is at or under what the share needs, of the
/// steps that do not warp across at that Vertical Scale (any step when every one would); the
/// size then makes the width exact (so at least 1: the size, not the scale, makes up the rest).
/// `covers` is false where the size's range (kMinSize to kMaxSize) cannot make it exact: then
/// the size at its end.
/// Session 651: covering the whole view (width and height, the height by lowering Horizontal
/// Scale and raising the size) was far too big; the owner's settled screens were 0.58 to 0.93
/// as wide as that (median about 0.74), the height 0.54 to 0.82 (triage item 2). The owner's
/// rule (2026-11-15): a moderate Horizontal Scale, about 25 % more on each side, and the size
/// raised until the width fits.
inline constexpr float kFillAcrossShare = 0.75f;
inline constexpr int kFillScalePercent = 50;
struct Filled {
    float size = 1.0f;
    int scale_x = 0;
    int scale_y = 0; ///< the player's Vertical Scale, unchanged
    bool covers = true; ///< the width exactly kFillAcrossShare of the view
    /// The screen's height over the height that would cover the view (0.8: 80 % of it); logged.
    float share_y = 0.0f;
};
Filled FillView(const Placement& placement, float game_fov_y_deg, float aspect,
                const ViewFov& view, float eye_shift, int scale_y, const Options& options,
                const LensOffset& offset = {});
/// "Fill View: Horizontal +35%, Vertical +60%" (Vertical: the player's, unchanged), with
/// " (Partial)" when the width is not exact and " (May Warp)" when `warp`.
std::string FillToast(const Filled& filled, bool warp);

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

/// AzaharFrame stereo: the game's far scenery on the Full modes' screen. The game's own 3D puts
/// each eye's far scenery `far_width` of its picture's width apart (the right eye's to the
/// right: uncrossed); the picture is 1/factor of the quad's width, so on the quad they are
/// `separation_m` apart, plus the eye quads' own distance apart in mode 3. The eyes, half the
/// IPD each side of the middle, look at them `vergence_deg` inwards (0: parallel, as at
/// infinity); below 0 they would have to diverge, which they cannot (the far scenery doubles).
struct StereoFar {
    float separation_m = 0.0f; ///< the two eyes' far points apart on the quad (with the quads')
    float far_deg = 0.0f;      ///< that, as seen from between the eyes
    float allow_deg = 0.0f;    ///< the most it may be: the eyes parallel (IPD apart)
    float vergence_deg = 0.0f; ///< the eyes' angle inwards at the far scenery
    bool diverges = false;     ///< vergence below -kDivergeToleranceDeg
    /// The game's 3D times this would put the far scenery at infinity (eyes parallel); 0 when
    /// the game shows no far disparity of its own.
    float natural_scale = 0.0f;
};
/// Divergence this small is not counted (nor warned about): the eyes fuse it with ease, and mode
/// 3's slider of 0.01 leaves Mario Kart 7 and Kid Icarus 0.05 to 0.08 degrees past parallel.
inline constexpr float kDivergeToleranceDeg = 0.1f;
/// AzaharFrame natural: `quad_half`, mode 3's eye quads from the middle (NaN: on the eyes).
StereoFar StereoFarOnQuad(const HeadLocked& solved, uint32_t mode, float half_ipd,
                          float far_width,
                          float quad_half = std::numeric_limits<float>::quiet_NaN());

/// AzaharFrame stereo: the cap keeps the eyes at least this far from parallel at the far scenery.
inline constexpr float kStereoMarginDeg = 0.05f;

/// AzaharFrame stereo: the largest slider (0 to 1) at which mode 2's screen shows the game's far
/// scenery with the eyes `margin_deg` inwards of parallel, for the game's far width per slider
/// unit `far_per_slider` (video_core/vr_view_matrix.h FarPerSlider). Below 0 when that is not
/// known; 0 when even a flat picture is too near parallel.
float StereoSliderLimit(const HeadLocked& solved, float half_ipd, float far_per_slider,
                        float margin_deg = kStereoMarginDeg);

/// AzaharFrame stereo: the slider mode 2 sends the game: the user's (3D Depth / 100) when the
/// limit allows it (exactly that), else the limit. Without jitter: the limit counts lower at
/// once, higher only once it is kStep above the one in use; the slider sent follows a change of
/// the limit at most kDownPerS (down) or kUpPerS (up) a second. A change of the user's own slider
/// goes through at once (up to the limit). The first known limit is taken at once. XR thread;
/// pure (unit tested).
class StereoCap {
public:
    static constexpr float kStep = 0.005f;
    static constexpr float kDownPerS = 4.0f;
    static constexpr float kUpPerS = 0.25f;
    static constexpr float kMinSlider = 0.001f;

    /// `limit` below 0: not known yet (the user's slider goes through, so it can be measured;
    /// once known, the last limit holds until it is measured again).
    float Update(float user, float limit, int64_t now_ms);
    /// The limit in use is below the user's slider.
    bool Limited() const {
        return limited_;
    }
    /// The limit in use (with the step above), below 0 before one was known.
    float Target() const {
        return limit_;
    }
    void Reset() {
        *this = StereoCap{};
    }

private:
    float out_ = -1.0f, limit_ = -1.0f, last_user_ = -1.0f;
    int64_t last_ms_ = 0;
    bool limited_ = false;
};

/// AzaharFrame oot: mode 2's menu rule. While the game shows a screen of its own in front of its
/// world (video_core/vr_view_matrix.h WorldOnceRun: its world drawn for one eye only, or not at
/// all, after it drew two), the game draws no Render Margin (factor 1): the screen is the
/// game's own picture at its own size, as it laid it out. Eased in and out over kEaseMs
/// (smoothstep), so the screen never jumps. XR thread; pure (unit tested).
class MenuExact {
public:
    static constexpr float kEaseMs = 250.0f;

    /// `on`: the rule holds now. The weight: 0 the Render Margin factor, 1 factor 1.
    float Update(bool on, int64_t now_ms);
    float Weight() const;
    /// The factor at weight `weight` between the Render Margin factor `wider` and 1.
    static float Factor(float wider, float weight) {
        return wider + (1.0f - wider) * weight;
    }
    void Reset() {
        *this = MenuExact{};
    }

private:
    float progress_ = 0.0f;
    int64_t last_ms_ = 0;
};

/// AzaharFrame stereo: CitraVR's head-motion scale in the Full modes, 10^multiplier x (3D Depth
/// / 10, an integer), from the user's 3D Depth: the stereo cap never changes it.
float GamePositionScale(int multiplier, uint32_t depth_percent);

// ---- AzaharFrame natural: 3D Depth and World Scale ----
//
// At the natural slider (StereoSliderLimit with no margin) the game's far scenery is exactly the
// IPD apart on the screen, so the eyes look at it parallel, as at infinity, and every point of
// the game appears as a true-scale copy of its world at W / K metres per game unit (W the
// picture's width on the screen, K the game's eye separation per picture width of far scenery,
// video_core/vr_view_matrix.h EyePerFar; the same at any slider). Leaning moves the game camera
// K / W game units per metre: one game eye separation per IPD of head movement, whatever the 3D
// Depth or the IPD. Mode 3 builds the same with its own eyes (Mode3Natural). So the world's size
// is the picture's width in metres: World Scale sets the Full modes' screen distance at the
// angular size as set (FullDistance), and the leaning follows it. IMMERSIVE.md "Natural 3D".

/// No margin: exactly parallel (the leaning scale's definition).
inline constexpr float kNaturalMarginDeg = 0.0f;

/// 3D Depth in the Full modes, percent of the natural 3D (Settings::values.vr_immersive_depth).
/// A game with no value of its own whose own 3DS 3D Depth (factor_3d) is set to 0 keeps 0 (it
/// was meant flat); anything else is the setting (default 100). At most 100.
uint32_t NaturalDepthPercent(bool depth_own, uint32_t depth, bool factor_3d_own,
                             uint32_t factor_3d);

/// The leaning scale, game units per metre: `eye_per_far` (K) over the picture's width on the
/// screen (the quad's width / factor). Below 0 when not known.
float NaturalLeaning(const HeadLocked& solved, float eye_per_far);

/// The K that leans by `leaning` on `solved` (the fallback: 1.2.0's GamePositionScale at the
/// screen as saved). Below 0 when `leaning` is not above 0.
float EyePerFarFor(const HeadLocked& solved, float leaning);

/// World Scale (Settings::values.vr_immersive_world_scale), percent: 100 is Automatic, the
/// screen at kWorldRefDistance (the default distance) with the angular size as set.
/// (Its range, stops and names: frame_xr/immersive_names.h, header only for the menu and Qt.)
using Names::kWorldScaleAuto;
using Names::kWorldScaleMin;
using Names::kWorldScaleMax;
using Names::kWorldScaleStops;
using Names::ClampWorldScale;
using Names::WorldScaleStop;
using Names::WorldScaleAfterGrip;
using Names::WorldScaleWhole;
using Names::WorldScaleToastGate;
using Names::kWorldToastIntervalNs;
using Names::WorldScaleText;
using Names::WorldScaleToast;
inline constexpr float kWorldRefDistance = 3.0f;
/// The distances World Scale may put the screen at (Follow Screen Size off and an extreme size
/// can ask for more; the leaning is made from the distance used, so it stays matched).
inline constexpr float kFullMinDistance = 0.1f;
inline constexpr float kFullMaxDistance = 20.0f;
/// The Full modes' screen distance (m) for World Scale `percent`: percent / 100 x
/// kWorldRefDistance, / `size` (the grip's screen size, 1 the default) when Follow Screen Size
/// is off, so the world keeps its size when the screen is resized. Clamped to kFullMinDistance
/// to kFullMaxDistance.
float FullDistance(float percent, float size, bool follow);
/// The World Scale that puts the screen at `distance` (FullDistance's inverse, unclamped).
float WorldScaleForDistance(float distance, float size, bool follow);
/// Migration, mode 3: the World Scale at which the head moves `leaning` (1.2.0's
/// GamePositionScale) on this screen (`solved`, its angular size; its distance does not matter),
/// with the game's K: its eye offset and leaning stay exactly as they were. Automatic when
/// `leaning` is not above 0 (1.2.0 did not move the camera) or K is not known. Clamped.
/// `exact` (when given): true when the value keeps that leaning exactly (neither clamped nor
/// Automatic for want of it), so the switch can be made at once; else it is eased.
float MigrateWorldScaleMode3(const HeadLocked& solved, float eye_per_far, float leaning,
                             float size, bool follow, bool* exact = nullptr);
/// Migration, mode 2: the World Scale that keeps the screen at its saved `distance`. Clamped.
float MigrateWorldScaleMode2(float distance, float size, bool follow);
/// Whether a game's World Scale is still to be made from 1.2.0's settings: it has no World Scale
/// of its own (`world_scale_own`) and no vr_immersive_world_set of its own that is on
/// (`world_set_own_on`: migrated once, then perhaps put back on the global value by Use Global
/// Values for All, which keeps it). Rollback: an older version drops both keys together when it
/// saves the game's file (they are new), so the next run makes World Scale again from the old
/// keys as they are then; the old keys are only read, never written with another meaning.
bool WorldScaleToMigrate(bool world_scale_own, bool world_set_own_on);

/// A value moving towards its target at most `rate` in ratio a second (ln 2: a factor 2 a
/// second), eased by kTauS: never a jump. The first value is taken at once; Set takes one at
/// once (a change nobody can see). AzaharFrame overshoot: `ceiling`, it goes no higher than that
/// (or where it is, when that is already higher): it waits there, and goes on eased.
class RatioRamp {
public:
    static constexpr float kMaxRatePerS = 0.6931472f; ///< ln 2
    static constexpr float kTauS = 0.25f;
    explicit RatioRamp(float rate = kMaxRatePerS) : rate_(rate) {}
    float Update(float target, int64_t now_ms,
                 float ceiling = std::numeric_limits<float>::infinity());
    void Set(float value) {
        value_ = eased_ = value;
    }
    float Value() const {
        return value_;
    }
    float Rate() const {
        return rate_;
    }
    void Reset() {
        *this = RatioRamp{rate_};
    }

private:
    float rate_;
    float value_ = -1.0f, eased_ = -1.0f;
    int64_t last_ms_ = 0;
};

/// The game's K in use (game units per picture width of far scenery), never jumping (RatioRamp,
/// a factor 2 a second): measured live; through frames without a measure (a menu) the last one;
/// before the first, the one saved from an earlier run of this game; else the fallback (1.2.0's
/// scale at the screen as saved, EyePerFarFor). XR thread; pure (unit tested).
class EyePerFarInUse {
public:
    static constexpr int kSaveAfter = 9; ///< the game's frames read before K counts as measured
    enum Source { kMeasured = 0, kSaved = 1, kFallback = 2, kNone = 3 };
    /// Below 0: not known. `samples`: the game's frames read so far (EyePerFar).
    float Update(float measured, int samples, float saved, float fallback, int64_t now_ms);
    /// Takes `k` at once (migration: the screen moves where nobody sees it, the leaning stays).
    void Set(float k) {
        ramp_.Set(k);
        target_ = k;
    }
    float Value() const {
        return ramp_.Value();
    }
    float Target() const {
        return target_;
    }
    Source From() const {
        return source_;
    }
    void Reset() {
        *this = EyePerFarInUse{};
    }

private:
    RatioRamp ramp_;
    float target_ = -1.0f;
    Source source_ = kNone;
};

/// Mode 2's slider: 3D Depth `f` (0 to 1) x the natural limit, without jitter: the limit counts
/// lower at once and higher only once it is kRise above the one in use; the slider follows
/// f x limit down at once (never past parallel) and eases up to it (kUpTauS); a change of `f`
/// and the first known limit go through at once. Before the limit is known, f x kProbe (small,
/// so it does not double on a big screen, and above 0 so the game draws two eyes to be
/// measured). At most 1 (the 3DS's slider): Clamped() says so. XR thread; pure (unit tested).
class NaturalSlider {
public:
    static constexpr float kRise = 0.01f; ///< relative
    /// AzaharFrame overshoot: 0.1 s (was 0.25), and eased up to within kSnap of the target, then
    /// the rest at once (too small to see): the game's measure waits while the slider moves
    /// (FarPerSlider), so the easing ends within half a second rather than over two.
    static constexpr float kUpTauS = 0.1f;
    static constexpr float kProbe = 0.03f;
    static constexpr float kSnap = 0.005f;

    /// `limit` below 0: not known (the last known one holds once there was one).
    /// AzaharFrame overshoot: `no_higher`, the far scenery on view is at parallel already (the
    /// game ahead of its measure, which waits while the slider moves): it goes no higher now.
    float Update(float f, float limit, int64_t now_ms, bool no_higher = false);
    float Limit() const {
        return limit_;
    }
    bool Clamped() const {
        return clamped_;
    }
    void Reset() {
        *this = NaturalSlider{};
    }

private:
    float out_ = -1.0f, limit_ = -1.0f, last_f_ = -1.0f;
    int64_t last_ms_ = 0;
    bool clamped_ = false;
};

/// AzaharFrame overshoot: the game's far width per slider unit (FarPerSlider) as mode 2's natural
/// limit is made from it. The game's far scenery moves with its scene and camera (Ocarina of
/// Time 3D: about 2 % either way within seconds), so a limit made from each measure went past
/// parallel whenever the measure rose after the slider was set (the game draws a few frames
/// behind), and hunted with it several times a second. Here:
/// - the measure counts at the highest it was over the last kHoldBuckets (a higher one at once:
///   the limit lower at once);
/// - times 1 + a margin from the measure's own noise: how far it went above that highest as it
///   was kRiseBuckets before (about the time a change of the game's far scenery takes to be
///   measured and a lower slider drawn: what the game can be ahead of the measure in use), the
///   most seen, fading by kNoiseTauS; at least kMarginFloor, at most kMarginMax. A steady game
///   gets only the floor, a noisy one only what its noise goes past the highest of before;
/// - and that is held until it goes lower by more than half the measure's spread over the hold
///   window (its noise; at least NaturalSlider::kRise): a lower measure waits until it is
///   clearly lower, then the slider eases up (NaturalSlider). A higher one counts at once.
/// Its clock runs only while the game is measured (its windows are of the game's measures).
/// Nothing is fitted to a game or a screen: the margin is the measure's own, in proportion.
/// XR thread; pure (unit tested).
class FarMeasureHold {
public:
    static constexpr int64_t kBucketMs = 100;
    static constexpr int kHoldBuckets = 20; ///< 2 s
    static constexpr int kRiseBuckets = 4;  ///< 0.4 s
    static constexpr float kNoiseTauS = 20.0f;
    static constexpr float kMarginFloor = 0.0025f;
    static constexpr float kMarginMax = 0.05f;

    /// `measure` below 0: not known now (the last one in use holds); `taken`, the samples it was
    /// made of so far (FarPerSlider::Taken): the same for longer than a bucket, it is not being
    /// measured now (the slider moving, no far scenery) and the one in use holds, nothing ageing.
    /// Returns the measure in use (held, with the margin), below 0 before one was known.
    float Update(float measure, uint32_t taken, int64_t now_ms);
    float Value() const {
        return held_;
    }
    /// The margin in use (relative, 0.01 = 1 %).
    float Margin() const {
        return margin_;
    }
    void Reset() {
        *this = FarMeasureHold{};
    }

private:
    struct Bucket {
        int64_t index = -1;
        float lo = 0.0f, hi = 0.0f;
    };
    /// A bucket's hold window as it was kRiseBuckets before, and the bucket.
    static constexpr int kRing = kHoldBuckets + kRiseBuckets + 1;
    std::array<Bucket, kRing> buckets_{};
    float held_ = -1.0f, margin_ = 0.0f;
    int64_t noise_at_ = -1; ///< the bucket being filled when noise_ was last brought up to date
    float noise_ = 0.0f;    ///< the most a bucket went beyond the hold, fading
    int64_t clock_ms_ = 0;  ///< runs only while measured
    int64_t last_ms_ = 0, fresh_ms_ = 0;
    uint32_t taken_ = 0;
};

/// AzaharFrame overshoot: the farthest the Full modes' screen may be (m) for the game's far
/// scenery `far_width` of its picture's width apart (as drawn now) to stay within parallel at the
/// screen's angular size and factor: past it, the eyes would have to diverge. Infinity when the
/// game shows no far disparity.
float ParallelDistance(const HeadLocked& solved, float half_ipd, float far_width);

/// Mode 3's own stereo: each eye's camera `f x leaning x IPD / 2` to its side (game units,
/// rasterizer GameEyeOffset's scaler f x leaning) and the eye quads the IPD apart, less the
/// game's own far scenery apart on the screen (`game_far_m`, at the 0.01 slider mode 3 sends):
/// the far scenery exactly parallel at any 3D Depth (the owner's decision: 3D Depth scales the
/// relief, infinity stays at infinity). At f = 1 it shows every point where mode 2 shows it.
struct Mode3Stereo {
    float eye_scaler = 0.0f; ///< game units per metre of IPD (GameEyeOffset's scaler)
    float quad_half = 0.0f;  ///< each eye quad's distance from the middle (m)
};
Mode3Stereo Mode3Natural(float f, float leaning, float half_ipd, float game_far_m);

/// The game's own far scenery apart on the quad (m), for its far width share `far_width`.
float GameFarOnQuad(const HeadLocked& solved, float far_width);

/// AzaharFrame turns2: the Full modes' quad eye `eye` sees, in the head's (VIEW) space: in mode 3
/// centred on that eye, in mode 2 one quad for both (centred between them).
/// AzaharFrame natural: in mode 3 `quad_half` from the middle (Mode3Natural; NaN: on the eyes).
inline constexpr float kQuadsOnEyes = std::numeric_limits<float>::quiet_NaN();
Input::Posef FullQuadInView(const HeadLocked& solved, uint32_t mode, int eye, float half_ipd,
                            float quad_half = kQuadsOnEyes);

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
/// AzaharFrame dbl: the same with the eye's move since the picture followed by `follow` (1: as
/// `follow_eye` true, what the picture shows behaves as far away; 0: as false, as at the quad's
/// distance; 1 - distance / z: as `z` metres away, below 0 for nearer than the quad). A picture
/// shown on two display refreshes is right, while the head moves, only for scenery at that
/// depth; the rest steps by its parallax when the next picture comes (a double image).
Input::Posef AnchoredQuadPose(const Input::Posef& picture, const Input::Posef& now,
                              const Input::Posef& in_view, float eye_x, float follow);
/// AzaharFrame dbl: `follow` for scenery that moves `parallax` radians a metre of the eye's move
/// (the game's camera moves `scaler` units a metre, the scenery `depth` units ahead, the screen
/// shows `1 / gain` of its angles): 1 - parallax x distance, kept from -20 to 1; 1 when not known.
float AnchorFollow(float distance, float scaler, float depth_units, float gain);

/// AzaharFrame turns2: the Full modes' anchored quad for eye `eye` (Immersive::Screens):
/// AnchoredQuadPose of FullQuadInView about EyeX.
Input::Posef AnchoredFullQuad(const HeadLocked& solved, uint32_t mode, int eye, float half_ipd,
                              const Input::Posef& picture, const Input::Posef& now,
                              bool follow_eye, float quad_half = kQuadsOnEyes);

/// AzaharFrame heldquad: the head-locked quad `in_view` (head frame), held on a display refresh
/// after its picture's, as the game would draw that picture now. The picture is magnified: the
/// game camera turns `gain` times the head, and the screen shows its angles 1 / gain times as
/// large, so a picture held rigid in the room (AnchoredQuadPose) is right only at its middle. Off
/// the middle a fresh picture moves its scenery with the head by (1 - gain^2) sin^2(e) of the
/// turn (e: the angle off the middle), and the rigid one, shown again, catches up in one jump at
/// the next picture: what the eye follows is seen twice.
///
/// For a point p of the quad, relative to the point the picture's rays come from, in the quad's
/// frame, the game ray is M^-1 p, M = diag(1 / gain_x, 1 / gain_y, 1). With the game camera
/// turned by R since the picture (conj(q_cam_now) x q_cam_then, both GameCameraRotation, so R is
/// in the quad's frame), the same scenery belongs along A p, A = M R M^-1. Seen from the eye at
/// `eye_x` that is a linear map of the picture (A, plus the rays' point's offset from the eye
/// carried along: exact for a planar picture), so the held quad is a parallelogram. A pure yaw or
/// a pure pitch of the game camera (in the quad's frame) makes it a rectangle. Its roll does not
/// with unequal gains: the picture's axes are magnified differently, so a roll is shown sheared,
/// which no rectangle can; a yaw of a pitched camera has some roll in it. The rectangle nearest
/// the parallelogram is used, the skew split evenly between its edges.
/// The result is scaled about the eye back to the quad's distance from it, so nothing changes as
/// seen from the eye and the layer order holds.
///   - `distance`: the quad's centre this far along its axis (HeadLocked::distance).
///   - `eye_share`: where the picture's rays come from, this share of the way from the quad's
///     axis to the eye. Mode 3 centres each eye's quad on its eye (any value: the eye). Mode 2's
///     one picture per eye on the shared quad comes from the game's own two cameras: 0 when they
///     are one (3D Depth 0), 1 when its far scenery is IPD apart on the quad (parallel, 3D
///     Depth 100 % of the natural 3D) (EyeShareOnQuad).
/// Gains 1 and the rays from the eye: A = R, exactly the pose of AnchoredQuadPose (follow 1) for
/// the same turn.
struct HeldQuadPose {
    Input::Posef pose;        ///< in the head's frame now (as `in_view`)
    float width_scale = 1.0f; ///< the quad's width times this
    float height_scale = 1.0f;
};
HeldQuadPose HeldQuad(const Input::Posef& in_view, float eye_x, float distance, float eye_share,
                      const Input::Quatf& q_cam_then, const Input::Quatf& q_cam_now, float gain_x,
                      float gain_y);

/// AzaharFrame heldquad: HeldQuad's `eye_share` for mode 2 from the game's far scenery
/// (StereoFarOnQuad's separation on the quad over the IPD): 0 with no far width known (below 0)
/// or none (one camera), 1 at parallel; kept from 0 to 2. Mode 3: 1, the rays from the eye.
/// AzaharFrame natural: also with mode 3's eye quads moved in by half the game's own far scenery
/// (Mode3Natural): that far scenery puts straight ahead exactly in front of the eye. Exactly 1,
/// not worked out from the far width again: the quads' place came from an earlier read of it.
float EyeShareOnQuad(const HeadLocked& solved, uint32_t mode, float half_ipd, float far_width);

/// AzaharFrame heldquad: one display frame's head pose, as the game camera was made from it
/// (Immersive OnFrame, with VideoCore::Immersive::VRData): its time (XrContentPose::time, the
/// key the picture drawn with it carries), the head in LOCAL space, the game camera's rotation
/// (GameCameraRotation) and the solved screen it used (the gains, the quad's rotation). A picture
/// shown later is held with these, so a camera angle that slewed (Detect Game FOV), a zoom or a
/// size change since it was drawn is never taken for a head turn.
struct HeldPose {
    int64_t time = 0;
    Input::Posef head;
    Input::Quatf q_cam;
    HeadLocked solved;
};

/// AzaharFrame heldquad: the last kSize HeldPoses (120 Hz times the anchor's 250 ms, doubled).
/// Cleared when the game camera's rest pose changes (a recentre, a pause) or the mode does, as
/// the old cameras no longer compare with the new ones. XR thread; pure.
class PoseRing {
public:
    static constexpr std::size_t kSize = 64;

    /// Every frame, before Push: the mode and the game camera's rest pose in use. A change of
    /// either clears the ring.
    void Keep(uint32_t mode, const Input::Posef& rest);
    void Push(const HeldPose& p);
    /// The pose of `time`, newest first; nullptr when it is not (or no longer) in the ring.
    const HeldPose* Find(int64_t time) const;
    /// The pose nearest to `time` (either side), nullptr when empty.
    const HeldPose* Nearest(int64_t time) const;
    void Clear() {
        count_ = 0;
        next_ = 0;
    }
    std::size_t Count() const {
        return count_;
    }

private:
    std::array<HeldPose, kSize> poses_{};
    std::size_t next_ = 0;
    std::size_t count_ = 0;
    bool kept_ = false;
    uint32_t mode_ = 0;
    Input::Posef rest_{};
};

/// AzaharFrame heldquad: the Full modes' anchored quad for eye `eye` (Immersive::Screens), in
/// LOCAL space, with its size. `held` (the shown picture's camera from the PoseRing) given:
/// HeldQuad about the eye (rays from `eye_share`, EyeShareOnQuad), with the game camera now made
/// from `now` (relative to `rest`, the game camera's rest pose) through the picture's own solved
/// screen; nullptr: rigid (AnchoredQuadPose). Either way the eye's move since the picture is
/// followed by `follow`, as in AnchoredQuadPose, and the quad's shape is `solved`'s (the live
/// placement). AzaharFrame natural: `quad_half`, mode 3's eye quads from the middle
/// (FullQuadInView); the eye stays at EyeX.
struct HeldScreen {
    Input::Posef pose;
    float width = 0.0f;
    float height = 0.0f;
};
HeldScreen HeldFullQuad(const HeadLocked& solved, uint32_t mode, int eye, float half_ipd,
                        const Input::Posef& picture, const Input::Posef& now, float follow,
                        const HeldPose* held, const Input::Quatf& rest, float eye_share = 1.0f,
                        float quad_half = kQuadsOnEyes);

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
    /// AzaharFrame natural: with `world_ratio` (the Full modes) the stick forward/back
    /// multiplies it (World Scale) instead of the placement's distance.
    Result Update(const Input::InputFrame& in, const Input::Posef& head, float dt,
                  bool enabled, Placement& placement, float* world_ratio = nullptr);
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
