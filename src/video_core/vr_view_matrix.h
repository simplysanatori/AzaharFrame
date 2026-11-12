// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: whether the four PICA uniform registers immersive mode 3 patches hold a view
// matrix in this draw. Header only, no dependencies (unit tested in
// src/tests/frame_xr/immersive_layout.cpp).
//
// Mode 3 patches one register (the preset's, the setting's or the heuristic's) in every draw.
// Only the draws of the game's 3D world keep their camera there; the others keep other data in
// it (Mario Kart 7's 2D and menu draws hold numbers like 21, 44, -25), or a 2D layout camera that
// looks straight down -Z (identity rotation). Patching those corrupted them (Mario Kart 7's race
// start was washed out white).

#pragma once

#include <cmath>

namespace VideoCore::Immersive {

/// `m` is the matrix's upper 3x3 (rows as stored in the registers). True for a rotation (rows of
/// unit length, at right angles) that is not the identity.
inline bool LooksLikeWorldView(const float m[3][3]) {
    constexpr float kUnit = 0.03f;  // |row| within 3 % of 1
    constexpr float kRight = 0.03f; // rows at right angles: |dot| under this
    constexpr float kIdentity = 1e-4f;
    bool identity = true;
    for (int i = 0; i < 3; i++) {
        float len2 = 0.0f;
        for (int j = 0; j < 3; j++) {
            if (!std::isfinite(m[i][j])) {
                return false;
            }
            len2 += m[i][j] * m[i][j];
            identity = identity && std::abs(m[i][j] - (i == j ? 1.0f : 0.0f)) < kIdentity;
        }
        if (std::abs(std::sqrt(len2) - 1.0f) > kUnit) {
            return false;
        }
    }
    for (int a = 0; a < 3; a++) {
        for (int b = a + 1; b < 3; b++) {
            const float dot = m[a][0] * m[b][0] + m[a][1] * m[b][1] + m[a][2] * m[b][2];
            if (std::abs(dot) > kRight) {
                return false;
            }
        }
    }
    return !identity;
}

/// AzaharFrame depth: what CitraVR's register search in mode 3 still has to find. A view matrix
/// register that is known (a preset such as Super Mario 3D Land's 90, or the setting) only ends
/// the search for the register: the eye indicator, which gives each eye its half of the eye
/// distance, is still searched for unless the setting names it. CitraVR searched for both or
/// neither, so a preset game got no eye indicator and both eyes the same picture (the game
/// itself is shown a 3D slider of 0.01 in mode 3, so its own stereo is gone too).
struct RegisterSearch {
    bool view_matrix = false;
    bool eye_indicator = false;
    bool Any() const {
        return view_matrix || eye_indicator;
    }
};

inline RegisterSearch RegisterSearchFor(int view_matrix_register, bool eye_indicator_set) {
    return {view_matrix_register == -1, !eye_indicator_set};
}

} // namespace VideoCore::Immersive
