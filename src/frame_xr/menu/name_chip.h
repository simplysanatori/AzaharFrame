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

namespace FrameXr::Menu {

inline constexpr const char* kVerifiedChip = "Verified";
inline constexpr float kNameChipH = 32.0f, kNameChipPad = 12.0f, kNameChipGap = 12.0f;
/// A name keeps at least this much room; under it the chip is left out.
inline constexpr float kNameChipMinName = 40.0f;

/// The room the chip takes after a name (its gap and itself).
inline float NameChipRoom(const Kit& kit) {
    return kNameChipGap + kit.TextWidth(Type::Label, kVerifiedChip) + kNameChipPad * 2.0f;
}

/// Whether the chip goes after a name that must end by `right` (enough room for both).
inline bool NameChipShown(const Kit& kit, float x, float right, bool verified) {
    return verified && right - x >= NameChipRoom(kit) + kNameChipMinName;
}

/// The width a name may take from `x` to `right` with the chip after it (when it is shown).
inline float NameRoom(const Kit& kit, float x, float right, bool verified) {
    return std::max(0.0f, right - x - (NameChipShown(kit, x, right, verified) ? NameChipRoom(kit) : 0.0f));
}

/// Draws `name` (Body, centred on `cy`) from `x`, fitted to end by `right` with the chip after it
/// when `verified`. Returns the chip's rectangle (empty when there is none).
inline Rect NameWithChip(Kit& kit, float x, float right, float cy, const std::string& name,
                         bool verified, float op = 1.0f) {
    const bool chip = NameChipShown(kit, x, right, verified);
    const float drawn = kit.TextMid(x, cy, Type::Body, Tok::kOnSurface,
                                    kit.Fit(Type::Body, name, NameRoom(kit, x, right, verified)), 0, op);
    if (!chip) {
        return {};
    }
    const float x0 = x + drawn + kNameChipGap;
    const float w = kit.TextWidth(Type::Label, kVerifiedChip) + kNameChipPad * 2.0f;
    const Rect r{x0, cy - kNameChipH * 0.5f, x0 + w, cy + kNameChipH * 0.5f};
    kit.Fill(r, 8.0f, Tok::kSecondaryContainer, op);
    kit.TextMid(r.Cx(), r.Cy(), Type::Label, Tok::kOnSecondaryContainer, kVerifiedChip, 1, op);
    return r;
}

} // namespace FrameXr::Menu
