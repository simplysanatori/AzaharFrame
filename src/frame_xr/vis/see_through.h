// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuopacity
// Weapon Opacity and Projectile Opacity (vr_light_gun_weapon_opacity, _projectile_opacity,
// 20 to 100 %): the weapon in hand and the projectiles drawn see-through, so the screens and the
// room show behind them. The pass that draws them is controller_models.cpp; this is the plan it
// follows, kept free of Vulkan so it is unit tested.
//
// The controllers' image is premultiplied: the layer has XR_COMPOSITION_LAYER_BLEND_TEXTURE_
// SOURCE_ALPHA_BIT and not the UNPREMULTIPLIED bit (Controllers in Front of Screen's eye quads
// too), so a pixel holds colour x alpha and alpha, and the compositor puts it over what is behind
// as colour + behind x (1 - alpha).
//
//   - Opacity 1: one stage, Whole: the draws as they always were (the lit parts in the opaque
//     pipeline, the added ones after them), no extra pass, nothing blended.
//   - Under 1, the lit (opaque) parts take two stages, so only the surface nearest the eye shows
//     (no inner or back face seen through the front, a clean uniform fade):
//       Depth:  every lit part into the depth buffer only (no colour written; the shader
//               discards as usual, then returns at flag +32 before any lighting);
//       Colour: the same draws again, depth test LEQUAL against that depth and no depth
//               written (gl_Position is invariant, so both stages give the same depth), the
//               shader's fade (params.z) at fade x opacity, so it writes premultiplied
//               (colour x a, a), blended over what is there (ONE, ONE_MINUS_SRC_ALPHA).
//     Then Added: the glows, a palm's flames, the muzzle flash and added bullets, their light
//     and coverage alpha both times the opacity (the coverage, clamp(max(light) x 0.6, 0, 0.6),
//     is taken from the light after it is scaled; never under 4/255 where there is light),
//     on top of the faded surface. Their light still adds, but their coverage is held at the
//     most of what is there and their own (blend op MAX), not summed: overlapping glow cards
//     summed it to 1 and hid the screen behind a glowing weapon at any opacity. So nothing of a
//     see-through weapon covers more than its opacity.
//   - A faded weapon is drawn after the other hand's controller, so the controller shows
//     through it too.
// Laser Sight is drawn elsewhere and is unchanged.

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace FrameXr::Vis {

inline constexpr int kOpacityMinPercent = 20;
inline constexpr int kOpacityMaxPercent = 100;
/// The shader's flag (params.w) for the Depth stage: discard as usual, then nothing else.
inline constexpr float kDepthOnlyFlag = 32.0f;

/// A setting's percent as a factor (20 % to 100 %).
constexpr float OpacityFactor(int percent) {
    return static_cast<float>(std::clamp(percent, kOpacityMinPercent, kOpacityMaxPercent)) /
           100.0f;
}

/// Under 1 (by more than rounding): drawn see-through.
constexpr bool SeeThrough(float opacity) {
    return opacity < 0.999f;
}

enum class DrawStage : uint8_t {
    Whole,  ///< opacity 1: every part, as before
    Depth,  ///< see-through: the lit parts' depth only
    Colour, ///< see-through: the lit parts, nearest surface, blended at the opacity
    Added,  ///< see-through: the added parts, light and coverage times the opacity
};

/// The stages in order: Whole, or Depth, Colour, Added.
struct DrawStages {
    std::array<DrawStage, 3> list{};
    int count = 0;
    const DrawStage* begin() const {
        return list.data();
    }
    const DrawStage* end() const {
        return list.data() + count;
    }
};
constexpr DrawStages StagesFor(float opacity) {
    if (!SeeThrough(opacity)) {
        return {{DrawStage::Whole}, 1};
    }
    return {{DrawStage::Depth, DrawStage::Colour, DrawStage::Added}, 3};
}

/// Whether a part is drawn in a stage (`added`: a glow, a flash card, an added bullet).
constexpr bool DrawnIn(DrawStage stage, bool added) {
    switch (stage) {
    case DrawStage::Whole:
        return true;
    case DrawStage::Depth:
    case DrawStage::Colour:
        return !added;
    case DrawStage::Added:
        return added;
    }
    return false;
}

/// The pipelines of the controllers' pass.
enum class Pipe : uint8_t {
    Opaque,    ///< lit, depth written, nothing blended (as always)
    Added,     ///< added light, no depth written (as always)
    DepthOnly, ///< kiuopacity: no colour, depth written
    Blended,   ///< kiuopacity: premultiplied over, depth LEQUAL, no depth written
    AddedHeld, ///< kiuopacity: added light, its coverage held (MAX, not summed), no depth written
};
constexpr Pipe PipeFor(DrawStage stage, bool added) {
    switch (stage) {
    case DrawStage::Whole:
        return added ? Pipe::Added : Pipe::Opaque;
    case DrawStage::Depth:
        return Pipe::DepthOnly;
    case DrawStage::Colour:
        return Pipe::Blended;
    case DrawStage::Added:
        return Pipe::AddedHeld;
    }
    return Pipe::Opaque;
}

/// The muzzle flash's pipeline (drawn on its own, after the weapon): as the glows'.
constexpr Pipe FlashPipe(float opacity) {
    return SeeThrough(opacity) ? Pipe::AddedHeld : Pipe::Added;
}

/// The shader's fade (params.z) for a stage: Whole keeps `fade` as it was.
constexpr float StageFade(DrawStage stage, float fade, float opacity) {
    return stage == DrawStage::Whole ? fade : fade * std::clamp(opacity, 0.0f, 1.0f);
}

/// The shader's flags (params.w) for a stage.
constexpr float StageFlags(DrawStage stage, float flags) {
    return stage == DrawStage::Depth ? flags + kDepthOnlyFlag : flags;
}

/// Hands in draw order: a faded weapon's hand after the other one.
constexpr std::array<int, 2> HandOrder(bool faded0, bool faded1) {
    return faded0 && !faded1 ? std::array<int, 2>{1, 0} : std::array<int, 2>{0, 1};
}

/// The coverage alpha the shaders give added light (the glows', the flash's), as
/// model_shaders.h coverOf: clamp(max(light) x 0.6, 0, 0.6), and at least kCoverFloor where
/// there is any light (a pixel with light at alpha 0 can be dropped as clear).
inline constexpr float kCoverFloor = 4.0f / 255.0f;
constexpr float CoverAlpha(float r, float g, float b) {
    const float m = std::max(r, std::max(g, b));
    return m > 0.0f ? std::clamp(std::max(m * 0.6f, kCoverFloor), 0.0f, 0.6f) : 0.0f;
}

/// A glow's light as the shader makes it (for the tests): its texture doubled and held at 1, a
/// moving glow's flicker `gain` (0 to 2, held at 1 again; below 0: not moving), then the fade.
struct GlowOut {
    std::array<float, 3> light{};
    float cover = 0.0f;
};
GlowOut GlowLight(const std::array<float, 3>& texture, float gain, float fade);

/// AZAHARFRAME_KIU_OPACITY=weapon,projectile (percent, live, for mocks): either part may be left
/// out ("60", ",30"); -1 where none (the setting holds).
struct OpacityTune {
    int weapon = -1;
    int projectile = -1;
};
OpacityTune ParseOpacityTune(const std::optional<std::string>& value);

/// The opacity to draw with: the tune's percent if it has one, else the setting's.
constexpr float OpacityOf(int setting_percent, int tune_percent) {
    return OpacityFactor(tune_percent >= 0 ? tune_percent : setting_percent);
}

} // namespace FrameXr::Vis
