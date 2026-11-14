// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: where Kid Icarus: Uprising's weapon goes in the hand. A weapon model is in
// its anchor bone's space (core/frame_weapon_models.h: Pit's axes, +y up, +z Pit's front, his
// right hand toward -x, the game's units); this gives the transform into the aiming
// controller's OpenXR aim space (-z along the ray, +y up, +x right, metres), from a small table
// of our own per class (not the game's: its bind pose is a T-pose), in the manner of Moonlight
// XR's light gun: the barrel along the ray and the grip in the palm, where a hand holds a pistol
// at the aim pose (kPalm: 5.25 cm under the ray, 4.3 cm behind its start).
//
//   Held (blades, staffs, bows, palms, clubs, cannons): the grip point at the palm, turned so the
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

struct FitWeapon {
    int cls = 0;
    std::array<float, 3> muzzle{};
    bool muzzle_valid = false;
};

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

/// The class's default scale (metres per game unit).
float FitClassScale(int cls);

/// A small motion of the parts that float (aim space, metres; added to the fit's place, never
/// turning anything, so the aim is not disturbed), from the time alone: the orbitars circle
/// 1.5 cm across the ray every 2.6 s and bob 1 cm every 2.1 s, the two halves opposite; a
/// detached effect part (`glow`) of any class bobs 0.8 cm every 2.4 s. Nothing else moves.
std::array<float, 3> FitMotion(int cls, FitSide side, bool glow, double time_s);

} // namespace FrameXr::Vis
