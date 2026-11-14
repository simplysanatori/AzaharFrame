// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuflash: Muzzle Flash: a short flash at the weapon's muzzle on each of Kid Icarus:
// Uprising's own shots (the shots-fired record, frame_xr/kiu_weapon.h), with the weapon in hand.
// The weapon's own flash cards (core/frame_weapon_models.h FlashValue) are drawn added to what is
// behind (the glow pipeline), fading out over kRapidNs (rapid fire) or kChargedNs (a charged
// shot) as they grow a little. Pure (no Vulkan or OpenXR): unit tested.
//
//   Which flash: rapid fire's, or the charged shot's for a shot after Recoil's charge pause
//     (input/gyro_aim.h Recoil::kChargePauseNs, the same heuristic as Recoil's longer pulse; the
//     game's data marks no charged shot).
//   Where: from the muzzle, or where the weapon ends in front of it (FlashPush), a face card is
//     centred half its size ahead on the ray and turned to the eye
//     (its up the head's, turned by a spin of its own per shot); a fan starts at the muzzle and
//     lies along the ray, turned about it to face the eye as far as it can. So the flash points
//     the way the shot goes and is seen from the player's eye, and from the side too.
//   How big: the game's own sizes in weapon units, times the weapon's fit scale (metres per unit)
//     and kSizeScale (the game shows Pit far off; in the hand the flash is near the eye), from
//     kCardMinM to kFaceMaxM / kFanMaxM across. AZAHARFRAME_KIU_FLASH=size,ms (live) tunes the size factor
//     and the duration (ms, both kinds alike) for a headset check.
// Independent of Recoil (its own switch and timer) and of Laser Sight.

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace FrameXr::Vis {

/// What to draw this frame.
struct FlashFrame {
    bool shown = false;
    bool charged = false;
    float age = 0.0f;       ///< 0 at the shot, 1 at the end
    uint32_t shot = 0;      ///< which shot (the card's spin)
    float duration_s = 0.0f;
};

class MuzzleFlash {
public:
    static constexpr int64_t kRapidNs = 70'000'000;
    static constexpr int64_t kChargedNs = 100'000'000;

    /// Each frame: the shots total so far (Kiu::ShotTotal), whether it is being read, and
    /// `active`: Muzzle Flash on with the weapon in hand. Inactive or unread: nothing, and shots
    /// made meanwhile (a menu) flash nothing when it comes back.
    void Update(int64_t now_ns, uint64_t total, bool readable, bool active);
    FlashFrame Current(int64_t now_ns) const;
    /// AZAHARFRAME_KIU_FLASH's duration (ms, 0: the defaults).
    void SetDurationMs(int ms) {
        duration_override_ns = ms > 0 ? int64_t{ms} * 1'000'000 : 0;
    }
    void Reset() {
        has_ = false;
        start_ns_ = -1;
    }
    /// AzaharFrame kiuflash: the shots seen while active (the projectiles fire one each) and
    /// whether the last was charged.
    uint32_t Shots() const {
        return shot_;
    }
    bool LastCharged() const {
        return charged_;
    }

private:
    bool has_ = false;
    uint64_t last_ = 0;
    bool ever_ = false;
    int64_t last_shot_ns_ = 0;
    int64_t start_ns_ = -1; ///< the flash's start, or -1
    bool charged_ = false;
    uint32_t shot_ = 0;
    int64_t duration_override_ns = 0;
};

/// The flash's strength over its age: full for the first 15 %, then easing to 0 at the end.
float FlashIntensity(float age);
/// Its size against its start: from 1 to `grow` (eased), over its age.
float FlashGrowth(float age, float grow);

/// The size factor and the duration of AZAHARFRAME_KIU_FLASH=size,ms (either part may be left
/// out; out of range or unset: 1 and 0).
struct FlashTune {
    float size = 1.0f;
    int duration_ms = 0;
};
FlashTune ParseFlashTune(const std::optional<std::string>& value);

inline constexpr float kSizeScale = 0.3f;
inline constexpr float kFaceMaxM = 0.22f;
inline constexpr float kFanMaxM = 0.32f;
/// The smallest card (its larger side), so a flash reads at arm's length and beyond.
inline constexpr float kCardMinM = 0.05f;

/// Column major 4x4 from a card's plane (x across, y up or along, z toward the viewer; game
/// units) to the room: a face (`fan` false) or a fan. `origin` the muzzle, `forward` the ray's
/// direction (unit), `eye` the eye, `up` the head's up (unit); `metres` per game unit (the fit
/// scale times kSizeScale and the growth), `offset` ahead along the ray (game units), `spin` a
/// face's turn about its normal (radians).
std::array<float, 16> FlashCardTransform(bool fan, const std::array<float, 3>& origin,
                                         const std::array<float, 3>& forward,
                                         const std::array<float, 3>& eye,
                                         const std::array<float, 3>& up, float metres,
                                         float offset, float spin);

/// The metres per game unit for a card of this size (game units, its larger side): the fit scale
/// times kSizeScale and the tune, held between kCardMinM and the role's largest size (both
/// times the tune).
float FlashCardMetres(bool fan, float card_units, float fit_scale, float tune_size);

/// A card's extent in game units (MXFM vertices of 12 floats, the card's index range): half its
/// width, half its length, the middle of its length along y; `units` its larger side.
struct FlashCardExtent {
    float half_width = 0.0f, half_length = 0.0f, middle = 0.0f;
    float Units() const {
        return std::max(half_width, half_length) * 2.0f;
    }
};
FlashCardExtent FlashCardSize(const std::vector<float>& vertices,
                              const std::vector<uint32_t>& indices, uint32_t first,
                              uint32_t count);

/// How far ahead of the muzzle (metres, along the ray: aim space -z) the flash starts: past the
/// weapon's own vertices that lie on the ray in front of it (within kPushRadiusM of it: a
/// cannon's muzzle is deep in its barrel), at most kPushMaxM. `vertices` a part's (MXFM, 12
/// floats each, weapon space), `fit` its transform to aim space, `muzzle` in aim space. Worked
/// out once when the weapon loads, with the class's own fit.
inline constexpr float kPushMaxM = 0.10f;
inline constexpr float kPushRadiusM = 0.025f;
float FlashPush(const std::vector<float>& vertices, const std::array<float, 16>& fit,
                const std::array<float, 3>& muzzle);

/// A flash card's texture is at most kTextureMaxSide on a side: a texture pack's larger PNG is
/// halved (2x2 texel averages) as it loads until it fits. RGBA8, rows as given.
inline constexpr uint32_t kTextureMaxSide = 256;
void FlashTextureFit(std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height);

/// A shot's own spin (radians), spread evenly over the shots.
float FlashSpin(uint32_t shot);

/// The game's colour (0 to 1, not linear) to linear light.
float FlashLinear(float c);

} // namespace FrameXr::Vis
