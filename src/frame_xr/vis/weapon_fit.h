// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: where Kid Icarus: Uprising's weapon goes in the hand. AzaharFrame
// kiuweap2: blades, staffs, clubs, bows and cannons now take the game's own hold (FitHold,
// HoldMap below); the table described here is their fallback (a weapon made without it). A weapon model is in
// its anchor bone's space (core/frame_weapon_models.h: Pit's axes, +y up, +z Pit's front, his
// right hand toward -x, the game's units); this gives the transform into the aiming
// controller's OpenXR aim space (-z along the ray, +y up, +x right, metres), from a small table
// of our own per class (not the game's: its bind pose is a T-pose), in the manner of Moonlight
// XR's light gun: the barrel along the ray and the grip in the palm, where a hand holds a pistol
// at the aim pose (kPalm: 5.25 cm under the ray, 4.3 cm behind its start).
//
//   Held (blades, staffs, bows, palms, clubs, cannons): the grip point at the palm (a palm's own:
//     Pit's palm on the controller, his forearm back along the player's, cut off at
//     kPalmForearmCm, its flames kept and still), turned so the
//     weapon's muzzle (its json's muzzle node and offset) lies on the ray, its up axis kept up.
//     Bows have no useful muzzle (it is the hand): the arrow's axis (+x, away from Pit) along
//     the ray, the limbs upright. Cannons replace the hand: the grip is where Pit's hand would
//     be, 2.17 units out along the forearm from the anchor.
//   Worn (claws, arms): the hand's own frame on the player's hand: Pit's fingers (-x right, +x
//     left) along the ray, the back of the hand outward, the thumb up; the wrist bone just
//     behind the controller's grip. Each claw stays on its own hand's side (the aiming hand
//     shows its claw).
//   Pair (orbitars): both, one either side of the aiming hand, a little above and ahead of it,
//     facing along the ray.
//
// A class's scale (metres per game unit) makes each a hand size; clubs, 2.5 m long at Pit's
// scale, are made about 1 m. The left hand gets the right hand's fit mirrored (worn and paired
// halves have their own). Each class can be tuned live with a test switch
// (test-switches.txt): AZAHARFRAME_KIU_FIT_<CLASS>=dx,dy,dz,yaw,pitch,roll,scale: an offset in
// cm (aim space), turns in degrees about the grip (yaw about +y, pitch about +x, roll about -z,
// in that order) and a factor on the class's scale; fewer values leave the rest.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace FrameXr::Vis {

/// Column major 4x4.
using FitMat = std::array<float, 16>;

enum class FitSide : uint8_t { Centre, Right, Left };

/// AzaharFrame kiuweap2: how Pit holds the weapon in the game (core/frame_weapon_models.h
/// HoldInfo, one of its poses): the weapon space in his holding hand's (row major 3x4), the
/// fist's centre in that hand's space (game units), and whether that hand is his left (a bow).
struct FitHold {
    std::array<float, 12> to_hand{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    std::array<float, 3> fist{};
    bool left = false;
};

struct FitWeapon {
    int cls = 0;
    std::array<float, 3> muzzle{};
    bool muzzle_valid = false;
    std::optional<FitHold> hold; ///< AzaharFrame kiuweap2: none: the class's own fit (as before)
};

/// AzaharFrame kiuweap2: the game's hold puts Pit's hand on the player's. Two ways, by class:
///   Grip (blades, staffs, clubs: a hilt or a shaft in the fist): the fist's axis (Pit's hand
///     +z, the thumb's side) along the controller's handle (kHandleAxis), the back of the hand
///     (+y) outward, the fist's centre on the handle (kHandleCentre). The controller's handle
///     sits where Pit's hand holds the hilt, the hilt through the fist.
///   Point (bows, cannons): the fingers' line (-x) along the ray, the thumb (+z) up, the back
///     of the hand outward, the fist's centre at the palm point (kPalm): the bow upright (as the
///     owner had it), the cannon's forearm along the ray, its housing on the hand's back
///     (outward: the right for the right hand, as in the game).
/// Claws, palms, arms and orbitars keep their own fits (the game's hold of them is the hand
/// itself, which those fits already are; orbitars float).
enum class HoldMap : uint8_t { None, Grip, Point };
HoldMap HoldMapFor(int cls);
/// The Frame controller's handle in aim space (its model's body, the centres of its cross
/// sections: frame_controller_right.model; the left one is its mirror): its axis (up through
/// the fist, 42 degrees forward of straight up, 4 to the left) and its centre.
inline constexpr std::array<float, 3> kHandleAxis{-0.0550f, 0.7387f, -0.6718f};
inline constexpr std::array<float, 3> kHandleCentre{0.013f, -0.0444f, 0.0871f};

/// AzaharFrame kiuweap2: which of the game's poses gives the hold (live test switch
/// AZAHARFRAME_KIU_HOLD=idle|shot): his idle (the owner's choice: how Pit holds it at rest) for
/// every class but staffs, whose idle grip points the staff back along the forearm (in the game
/// his turned wrist points it ahead): theirs is his aiming grip, the staff ahead along the ray.
enum class HoldPick : uint8_t { Idle, Shot };
inline constexpr const char* kHoldSwitch = "AZAHARFRAME_KIU_HOLD";
HoldPick HoldPickFor(int cls, const std::optional<std::string>& value);

struct FitOverride {
    std::array<float, 3> offset_cm{};
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    float scale = 1.0f;
    bool set = false;
};
/// Parses "dx,dy,dz,yaw,pitch,roll,scale" (any leading part of it). Unset or empty: none.
FitOverride ParseFitOverride(const std::optional<std::string>& value);
/// "AZAHARFRAME_KIU_FIT_BLADE", ... for a class (0 to 8).
std::string FitSwitchName(int cls);

struct FitPart {
    FitSide side = FitSide::Centre; ///< which component of the model
    FitMat transform{};             ///< weapon space -> aim space
    bool mirrored = false;          ///< the transform turns the triangles over (det < 0)
};

/// The parts to draw for the aiming hand (`left_hand`), given the sides the model has.
std::vector<FitPart> FitWeaponToHand(const FitWeapon& w, bool left_hand,
                                     const std::vector<FitSide>& sides,
                                     const FitOverride& tune = {});

/// Where the weapon's shots come out, in aim space: its muzzle (the json's node and offset) as
/// fitted; a pair's two muzzles' middle; without a muzzle, the grip. Laser Sight's beam starts
/// there. Every class puts its muzzle on the aim ray (held: the fit; worn: the hand frame is
/// raised onto it; a pair: the middle of the two), so the shot's line runs along the ray.
std::optional<std::array<float, 3>> FitMuzzle(const FitWeapon& w, bool left_hand,
                                              const std::vector<FitSide>& sides,
                                              const FitOverride& tune = {});

/// AzaharFrame kiuguns: a palm is a whole forearm and hand. Two ways to wear it, a live test
/// switch (AZAHARFRAME_KIU_PALM_MODE=wrist|elbow, wrist by default):
///   Wrist: the hand on the controller and a short stub of the forearm (kPalmStubCm).
///   Elbow: the hand on the controller, the forearm (kPalmForearmCm) turned about the wrist
///     toward where the player's elbow is estimated to be (EstimateElbow), smoothed.
/// AZAHARFRAME_KIU_PALM_FOREARM_CM sets how much forearm is drawn in either (1 to 200 cm).
enum class PalmMode : uint8_t { Wrist, Elbow };
inline constexpr const char* kPalmModeSwitch = "AZAHARFRAME_KIU_PALM_MODE";
inline constexpr const char* kPalmForearmSwitch = "AZAHARFRAME_KIU_PALM_FOREARM_CM";
inline constexpr float kPalmStubCm = 6.0f;
inline constexpr float kPalmForearmCm = 22.0f;
PalmMode PalmModeFrom(const std::optional<std::string>& value);
/// Where the weapon is cut off along its anchor's +x (Pit's forearm, toward the shoulder), in
/// game units (weapon space): palms only (`forearm_cm`: the switch's value); 0 for none.
float FitClipX(int cls, const FitOverride& tune, const std::optional<std::string>& forearm_cm,
               PalmMode mode = PalmMode::Wrist);

/// Elbow mode's estimate (one space for all, metres, +y up): the shoulder 0.17 m under the head
/// and 0.16 m to the aiming hand's side in the head's yaw frame, then a two-bone arm (upper arm
/// 0.30 m, forearm 0.27 m) from it to the wrist, the elbow bent down and a little outward.
std::array<float, 3> EstimateElbow(const std::array<float, 3>& head,
                                   const std::array<float, 3>& head_forward, bool left_hand,
                                   const std::array<float, 3>& wrist);
/// A unit direction eased toward `target` (time constant 0.08 s), so the forearm does not jitter.
std::array<float, 3> SmoothDirection(const std::array<float, 3>& previous,
                                     const std::array<float, 3>& target, float dt_s);
/// The rotation about `pivot` that turns direction `from` onto `to` (the shortest), column major.
FitMat RotationBetween(const std::array<float, 3>& pivot, const std::array<float, 3>& from,
                       const std::array<float, 3>& to);

/// The class's default scale (metres per game unit).
float FitClassScale(int cls);

/// AzaharFrame kiufix: Weapon Size (vr_light_gun_weapon_size, per game): Game Size, the class's
/// fit as the game has it next to Pit's hand (as before); Hand Size, a weapon longer than its
/// class's hand-held length (kHandSizeCapM: 35 to 45 cm) brought down to it, smaller ones left
/// as they are. Weapon Scale (vr_light_gun_weapon_scale, 50 to 150 %) on top. The factor goes on
/// the fit's scale (FitOverride::scale), which scales about the grip: the grip stays in the palm
/// and the muzzle on the ray (FitWeaponToHand puts it there at any scale).
enum class WeaponSize : uint8_t { Game = 0, Hand = 1 };
float HandSizeCapM(int cls);
/// The factor on the fit's scale. `bounds_*`: the model's (weapon space); `fit_scale`: the class's
/// scale times any live tune (metres per unit).
float WeaponSizeFactor(int cls, const std::array<float, 3>& bounds_min,
                       const std::array<float, 3>& bounds_max, float fit_scale, WeaponSize size,
                       int scale_percent);

/// A small motion of the parts that float (aim space, metres; added to the fit's place, never
/// turning anything, so the aim is not disturbed), from the time alone: the orbitars circle
/// 1.5 cm across the ray every 2.6 s and bob 1 cm every 2.1 s, the two halves opposite; a
/// detached effect part (`glow`) of any class bobs 0.8 cm every 2.4 s. Nothing else moves.
std::array<float, 3> FitMotion(int cls, FitSide side, bool glow, double time_s);

/// AzaharFrame kiumodels: a glow card's look at a time (seconds; the same time gives the same
/// look, every value bounded), for its glow pass:
/// - its texture slides by (du, dv) over each loop of `frames` game frames (the converter's
///   UvMotion: the game's own animation, or a flow along the card), `u`, `v` added to its UVs
///   (|u| <= |du|, |v| <= |dv|);
/// - it flickers: its light times `gain`, within 1 +- kGlowFlicker (palms' flames) or half that
///   (the others' glows), from three slow waves (about 1.7, 3.1 and 4.3 per second: a flame's
///   breathing, never a strobe);
/// - palms' flames sway ("lick"): each vertex moves by `sway` times its distance from the anchor
///   bone (the hand: the flames' roots stay, their tips move most) along a wave of `wave`
///   radians per weapon unit, its two phases `phase_a`, `phase_b` in [0, 2 pi).
struct GlowMotion {
    float u = 0.0f, v = 0.0f;
    float gain = 1.0f;
    float sway = 0.0f;
    float wave = 0.0f;
    float phase_a = 0.0f, phase_b = 0.0f;
};
inline constexpr float kGlowFlicker = 0.16f;
inline constexpr float kPalmSway = 0.05f;
/// `source`: 0 none, 1 the game's, 2 a flow (FrameWeapons::UvSource); `game_fps`: the frames'
/// rate (FrameWeapons::kGameFps).
GlowMotion AnimateGlow(int cls, int source, float frames, float du, float dv, double time_s,
                       float game_fps = 30.0f);

} // namespace FrameXr::Vis
