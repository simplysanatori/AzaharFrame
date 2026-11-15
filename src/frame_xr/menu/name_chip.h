// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fixd: the "Verified" chip right after an entry's name: texture packs (Download Packs
// and the installed packs), mods and patches (Install Mods and the loaded list) from the catalogs'
// "verified", and the developer's own cheats. The style is the Settings rows' Experimental chip
// (tab_menu.cpp, RowChip): 32 dp high, 12 dp sides, 12 dp after the name, filled with the secondary
// container colour, Label type. The name is fitted to the width left after the chip, so a long
// name is cut short and the chip stays on view.

#pragma once

#include <algorithm>
#include <string>

#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/md3_fonts.h"

namespace FrameXr::Menu {

inline constexpr const char* kVerifiedChip = "Verified";
/// The catalog's "recommended" (the owner's picks, set only by the owner): instead of the Verified
/// chip, Material's filled verified badge (the check-mark seal) in the primary colour, as before
/// the Verified chip (the owner, 1.2.0 RC2: the badge is what Recommended should be).
inline constexpr const char* kRecommendedChip = "Recommended";
inline constexpr float kRecommendedBadge = 20.0f, kRecommendedBadgeGap = 6.0f;
inline constexpr float kNameChipH = 32.0f, kNameChipPad = 12.0f, kNameChipGap = 12.0f;
/// A name keeps at least this much room; under it the chip is left out.
inline constexpr float kNameChipMinName = 40.0f;

enum class NameChip { None, Verified, Recommended };

/// The chip an entry gets: Recommended over Verified.
constexpr NameChip ChipFor(bool verified, bool recommended = false) {
    return recommended ? NameChip::Recommended : verified ? NameChip::Verified : NameChip::None;
}

inline const char* ChipText(NameChip chip) {
    return chip == NameChip::Recommended ? kRecommendedChip : kVerifiedChip;
}

/// The room the chip takes after a name (its gap and itself).
inline float NameChipRoom(const Kit& kit, NameChip chip = NameChip::Verified) {
    if (chip == NameChip::Recommended) {
        return kRecommendedBadgeGap + kRecommendedBadge;
    }
    return kNameChipGap + kit.TextWidth(Type::Label, ChipText(chip)) + kNameChipPad * 2.0f;
}

/// Whether the chip goes after a name that must end by `right` (enough room for both).
inline bool NameChipShown(const Kit& kit, float x, float right, NameChip chip) {
    return chip != NameChip::None && right - x >= NameChipRoom(kit, chip) + kNameChipMinName;
}
inline bool NameChipShown(const Kit& kit, float x, float right, bool verified) {
    return NameChipShown(kit, x, right, ChipFor(verified));
}

/// The width a name may take from `x` to `right` with the chip after it (when it is shown).
inline float NameRoom(const Kit& kit, float x, float right, NameChip chip) {
    return std::max(0.0f, right - x - (NameChipShown(kit, x, right, chip) ? NameChipRoom(kit, chip) : 0.0f));
}
inline float NameRoom(const Kit& kit, float x, float right, bool verified) {
    return NameRoom(kit, x, right, ChipFor(verified));
}

/// Draws `name` (Body, centred on `cy`) from `x`, fitted to end by `right` with `chip` after it.
/// Returns the chip's rectangle (empty when there is none).
inline Rect NameWithChip(Kit& kit, float x, float right, float cy, const std::string& name,
                         NameChip chip, float op = 1.0f) {
    const bool shown = NameChipShown(kit, x, right, chip);
    const float drawn = kit.TextMid(x, cy, Type::Body, Tok::kOnSurface,
                                    kit.Fit(Type::Body, name, NameRoom(kit, x, right, chip)), 0, op);
    if (!shown) {
        return {};
    }
    if (chip == NameChip::Recommended) {
        const float x0 = x + drawn + kRecommendedBadgeGap;
        kit.Icon(x0 + kRecommendedBadge * 0.5f, cy, kRecommendedBadge, Icon::kVerified, true,
                 Tok::kPrimary, op);
        return {x0, cy - kRecommendedBadge * 0.5f, x0 + kRecommendedBadge,
                cy + kRecommendedBadge * 0.5f};
    }
    const char* text = ChipText(chip);
    const float x0 = x + drawn + kNameChipGap;
    const float w = kit.TextWidth(Type::Label, text) + kNameChipPad * 2.0f;
    const Rect r{x0, cy - kNameChipH * 0.5f, x0 + w, cy + kNameChipH * 0.5f};
    kit.Fill(r, 8.0f, Tok::kSecondaryContainer, op);
    kit.TextMid(r.Cx(), r.Cy(), Type::Label, Tok::kOnSecondaryContainer,
                text, 1, op);
    return r;
}
/// The Verified chip when `verified` (cheats, and the older callers).
inline Rect NameWithChip(Kit& kit, float x, float right, float cy, const std::string& name,
                         bool verified, float op = 1.0f) {
    return NameWithChip(kit, x, right, cy, name, ChipFor(verified), op);
}

} // namespace FrameXr::Menu
