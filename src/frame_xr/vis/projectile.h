// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuflash: Projectiles: each of Kid Icarus: Uprising's own shots (the shots record,
// frame_xr/kiu_weapon.h) flies as the weapon's own bullet (core/frame_weapon_models.h: its
// effect file's bullet model, or its bullet sprites) from the weapon's muzzle to where the aim
// ray meets the top screen (Calibrate Aim's point when the game has one), then goes into the
// screen as the game's own shot appears there. Pure (no Vulkan or OpenXR): unit tested.
//
//   Timing: the distance over a speed of our own (kRapidSpeed, kChargedSpeed metres a second,
//     times Projectile Speed), held between kRapidMinNs..kRapidMaxNs (charged: kChargedMinNs..
//     kChargedMaxNs) so a near or far screen still reads; then kEnterNs going into the screen
//     (shrinking to nothing at the point it hit).
//   Several at once: each shot its own, at most kMaxShots (the oldest goes first).
//   Rapid or charged: as Muzzle Flash (Recoil's charge pause).
//   Size: kRapidSizeM / kChargedSizeM across (the bullet's largest side), times Projectile Size.
// Recoil, Muzzle Flash and Laser Sight are untouched by it.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace FrameXr::Vis {

inline constexpr int kMaxShots = 8;

/// What the controls hand the drawing each frame (controls.cpp, Visuals::SetShots).
struct ShotSignal {
    bool on = false;      ///< Projectiles on, the weapon in hand, the shots read
    uint32_t shots = 0;   ///< the shots seen (MuzzleFlash::Shots): a new one fires
    bool charged = false; ///< the last one's kind
    bool target_valid = false;
    std::array<float, 3> target{}; ///< the room: where the aim ray meets the top screen
    float speed = 1.0f, size = 1.0f; ///< Projectile Speed and Size, factors (the tune on top)
};
inline constexpr float kRapidSpeed = 14.0f;   ///< metres a second
inline constexpr float kChargedSpeed = 9.0f;
inline constexpr int64_t kRapidMinNs = 110'000'000;
inline constexpr int64_t kRapidMaxNs = 220'000'000;
inline constexpr int64_t kChargedMinNs = 180'000'000;
inline constexpr int64_t kChargedMaxNs = 380'000'000;
inline constexpr int64_t kEnterNs = 60'000'000;
inline constexpr float kRapidSizeM = 0.07f;
inline constexpr float kChargedSizeM = 0.12f;

/// How long a shot flies (ns): `metres` over the kind's speed times `speed` (Projectile Speed, a
/// factor), within the kind's limits (shorter limits too when faster).
int64_t ShotFlightNs(float metres, bool charged, float speed);

/// One shot in flight, or going into the screen.
struct ShotDraw {
    std::array<float, 3> at{};      ///< where it is (the room)
    std::array<float, 3> forward{}; ///< unit, from the muzzle to the screen
    float size = 0.0f;              ///< metres across (its largest side)
    float strength = 0.0f;          ///< 0 to 1: fading in, then out as it goes in
    float spin = 0.0f;              ///< radians about `forward`
    bool charged = false;
    uint32_t serial = 0;
};

class Projectiles {
public:
    /// A shot: from the muzzle to the screen's point (the room), at `now_ns`. `speed` and `size`
    /// are Projectile Speed and Projectile Size as factors (1 = 100 %).
    void Fire(const std::array<float, 3>& from, const std::array<float, 3>& to, bool charged,
              int64_t now_ns, float speed, float size);
    /// What to draw now (shots done are let go).
    std::vector<ShotDraw> Current(int64_t now_ns);
    std::size_t InFlight() const {
        return shots_.size();
    }
    void Clear() {
        shots_.clear();
    }

private:
    struct Shot {
        std::array<float, 3> from{}, to{};
        int64_t start_ns = 0, flight_ns = 0;
        bool charged = false;
        float size = 1.0f;
        uint32_t serial = 0;
    };
    std::vector<Shot> shots_;
    uint32_t serial_ = 0;
};

/// AZAHARFRAME_KIU_SHOT=speed,size (factors on top of the settings; unset: 1, 1).
struct ShotTune {
    float speed = 1.0f;
    float size = 1.0f;
};
ShotTune ParseShotTune(const std::optional<std::string>& value);

/// Column major 4x4 from a bullet's own space (normalised by the converter: centred, its largest
/// side 1, its front along +z) to the room: at `d.at`, +z along `d.forward`, `d.size` metres,
/// turned `d.spin` about the way it flies, its up as near the room's as it can be.
std::array<float, 16> ShotTransform(const ShotDraw& d);

} // namespace FrameXr::Vis
