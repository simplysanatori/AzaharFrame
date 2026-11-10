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
//     little against the head. The game's own view never widens with the size: past its edges
//     its culling and scenery run out (black boxes in Super Mario 3D Land).
//   - Wider View (per game, off by default): as in earlier builds, the game's periphery factor
//     (CitraVR's 1.4, `gl_Position.xy /= factor`) then goes between 1.0 and 1.4 with the size,
//     so the quad and the game's view grow together (gain 1 in that range).
//   - Squeeze Width (per game, off by default): the quad is narrower than the game's picture
//     by 10, 20 or 30 %, at the same height, and the yaw gain is the pitch gain / (1 - squeeze), so
//     the HUD at the sides moves in while the middle stays locked. The world looks that much
//     thinner.
//
// The default size fits the quad's width (at its middle row) inside +-kComfortHalfWidthDeg (the
// HUD at the sides
// stays out of the blurry lens edge) and the whole quad inside the part of the view both eyes see
// (from the runtime's per-eye FOV, minus a margin), centred a few degrees below the middle of
// that view, never larger than the game's own angle (gain 1). When the game's frame is wider than
// that, the default is already scaled and the gain is above 1.

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "frame_xr/input/geometry.h"
#include "frame_xr/input/input_frame.h"

namespace FrameXr::Immersive::Layout {

// ---- Tuning (easy to change; see IMMERSIVE.md) ----
inline constexpr float kFactorMax = 1.4f; ///< CitraVR's periphery factor for modes 2 and 3
inline constexpr float kFactorMin = 1.0f; ///< the game's own frame, no periphery
inline constexpr float kWideFactor = 3.0f; ///< mode 1
inline constexpr float kDefaultDistance = 0.75f; ///< metres (CitraVR 0.5)
inline constexpr float kMinDistance = 0.3f;
inline constexpr float kMaxDistance = 3.0f;
/// The screen's centre this far below the middle of the view (negative = down), degrees.
inline constexpr float kDefaultPitchDeg = -5.0f;
inline constexpr float kMinPitchDeg = -40.0f;
inline constexpr float kMaxPitchDeg = 25.0f;
/// Size, times the default (fitted) size.
inline constexpr float kMinSize = 0.6f;
inline constexpr float kMaxSize = 2.5f;
/// The default screen is no wider than this either side of its middle, degrees (the HUD at the
/// sides stays in the sharp part of the lenses). The grips make it bigger.
inline constexpr float kComfortHalfWidthDeg = 37.0f;
/// The rotation gain stays within these (past them the screen keeps its size and the world drifts
/// a little). At the screen's edge, angle e, a head turn moves the picture by
/// (1 + gain^2 tan^2 e) / (1 + tan^2 e) times the turn: 1.25 at gain 1.3 and e = 37 degrees.
inline constexpr float kMinGain = 0.4f;
inline constexpr float kMaxGain = 2.0f;
/// Squeeze Width (vr_immersive_squeeze 0 to 3): the screen this much narrower than the picture.
/// At 30 % the yaw gain (gain / 0.7) reaches kMaxGain from a pitch gain of 1.4 up; past that the
/// world drifts a little sideways with the head.
inline constexpr float kSqueezeSteps[] = {0.0f, 0.10f, 0.20f, 0.30f};
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

/// The part of the view the screen must fit in: angles in radians from the head's forward axis
/// (OpenXR XrFovf signs: left and down are negative).
struct ViewFov {
    float left = -kFallbackHalfFovDeg * kDegToRad;
    float right = kFallbackHalfFovDeg * kDegToRad;
    float up = kFallbackHalfFovDeg * kDegToRad;
    float down = -kFallbackHalfFovDeg * kDegToRad;
};

/// One eye's FOV and the eye's yaw and pitch relative to the head (canted displays), radians.
struct EyeFov {
    float left, right, up, down;
    float yaw = 0.0f;   ///< positive = turned left
    float pitch = 0.0f; ///< positive = turned up
};

/// What both eyes see: the intersection of the two eyes' FOVs, in head angles.
ViewFov BothEyes(const EyeFov& left, const EyeFov& right);

/// What the player chose (saved per game). Defaults: kDefault*.
struct Placement {
    float pitch_deg = kDefaultPitchDeg; ///< relative to the middle of the view
    float distance = kDefaultDistance;
    float size = 1.0f; ///< times the default size
    bool operator==(const Placement&) const = default;
};
Placement Clamped(Placement p);

/// The per-game options of the head-locked screen.
struct Options {
    bool wider_view = false; ///< the periphery factor grows with the size (1.0 to 1.4)
    float squeeze = 0.0f;    ///< the quad this much narrower than the picture (0 to 0.3)
};

/// Squeeze Width's setting (0, 1, 2) as a fraction; other values are 0.
float SqueezeFor(uint32_t setting);

/// Modes 2 and 3, solved for one frame.
struct HeadLocked {
    float factor = kFactorMax; ///< the game draws this much wider (gl_Position.xy /= factor)
    float tan_half_y = 0.0f;   ///< the quad's half height over its distance
    float aspect = 5.0f / 3.0f; ///< the quad's width over its height (the picture's x (1 - squeeze))
    float pitch_rad = 0.0f; ///< the quad and the game camera turn by this about the eye
    float distance = kDefaultDistance;
    /// The head's pitch x gain is given to the game. 1 when the quad has the game's angle.
    float gain = 1.0f;
    /// The head's yaw x gain_x is given to the game: gain / (1 - squeeze).
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

/// The default screen's tan(half height) (size 1): the quad's width (at its middle row) inside
/// +-kComfortHalfWidthDeg, the quad inside `view` minus kFitMarginDeg, no larger than the game's
/// own angle (times 1.4 with Wider View). `aspect` is the picture's (unsqueezed).
float DefaultTanHalfY(float game_fov_y_deg, float aspect, float pitch_rad, const ViewFov& view,
                      float eye_shift, bool wider_view);

/// `aspect` is the game picture's width over its height (the Screen Shape).
/// `offset` turns the solved screen (yaw_rad, and pitch_rad on top of the placement's pitch);
/// the size is fitted for the placement alone, so nudging never resizes the screen.
HeadLocked SolveHeadLocked(const Placement& placement, float game_fov_y_deg, float aspect,
                           const ViewFov& view, float eye_shift = 0.0f,
                           const Options& options = {}, const LensOffset& offset = {});

/// The head-locked quad's rotation about the eye: the offset's yaw, then the pitch.
Input::Quatf QuadRotation(const HeadLocked& solved);

/// A size saved by builds before the comfortable default (the "head" lines of the file) was
/// relative to the old default, which filled the whole view both eyes see (at most 1.4 times
/// the game's angle). The size that keeps that screen's angle now.
float SizeFromOldDefault(float old_size, const Placement& placement, float game_fov_y_deg,
                         float aspect, const ViewFov& view, float eye_shift, bool wider_view);

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
/// comment. Kinds: "view" (modes 2 and 3), "wide" (mode 1), and "head" (modes 2 and 3, saved by
/// earlier builds: its size is relative to their default, see SizeFromOldDefault; kept as it is
/// until converted). Unknown or broken lines are kept out (and dropped when saving).
class Store {
public:
    void Parse(const std::string& text);
    std::string Format() const;
    std::optional<Placement> Get(uint64_t title_id, bool wide) const;
    /// The head-locked placement of this game is an old "head" line (its size needs converting).
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
