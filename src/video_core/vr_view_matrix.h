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
#include <cstdint>

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

/// AzaharFrame depth: the game camera's angle and shape, read from its projection matrix (the
/// four registers just before the view matrix: 86 to 89 before Super Mario 3D Land's 90, 0 to 3
/// before Ocarina of Time 3D's 4). The 3DS draws its top screen turned by 90 degrees, so either
/// of the first two rows can hold the horizontal scale; the other holds the vertical one. The
/// stereo shear in the third column (the game's own 3D) does not matter.
struct Projection {
    float tan_half_y = 0.0f; ///< tan of half the vertical angle
    float aspect = 0.0f;     ///< tan of half the horizontal angle over tan_half_y
};

/// `r` is the four registers, rows as stored. Nothing when they hold no perspective matrix, or
/// one a top screen camera would not have: 10 to 150 degrees high, 1.2 to 3 times as wide (4:3
/// to wider than 21:9; other matrices in the same registers, a shadow's or a reflection's, are
/// often narrow).
inline bool ProjectionFromRows(const float r[4][4], Projection& out) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!std::isfinite(r[i][j])) {
                return false;
            }
        }
    }
    // The last row of a perspective matrix: (0, 0, +-1, 0).
    if (std::abs(r[3][0]) > 1e-4f || std::abs(r[3][1]) > 1e-4f ||
        std::abs(std::abs(r[3][2]) - 1.0f) > 1e-3f || std::abs(r[3][3]) > 1e-3f) {
        return false;
    }
    const bool first_is_x = std::abs(r[0][0]) > std::abs(r[0][1]);
    const float* hr = first_is_x ? r[0] : r[1];
    const float* vr = first_is_x ? r[1] : r[0];
    const float sx = std::abs(hr[0]);
    const float sy = std::abs(vr[1]);
    // Each row scales one axis only (2 % of cross term at most).
    if (sx < 1e-6f || sy < 1e-6f || std::abs(hr[1]) > 0.02f * sx || std::abs(vr[0]) > 0.02f * sy) {
        return false;
    }
    const float tan_half_y = 1.0f / sy;
    const float aspect = sy / sx;
    constexpr float kMinTan = 0.0875f; // 10 degrees high
    constexpr float kMaxTan = 3.732f;  // 150 degrees high
    if (tan_half_y < kMinTan || tan_half_y > kMaxTan || aspect < 1.2f || aspect > 3.0f) {
        return false;
    }
    out = {tan_half_y, aspect};
    return true;
}

/// AzaharFrame depth: the projection most world draws used in each window of time (a quarter
/// of a second), so a draw with another camera (a menu's, a model on the HUD) does not flicker
/// the screen's size. Emulation thread only.
class ProjectionTracker {
public:
    static constexpr int64_t kWindowMs = 250;

    /// One world draw's projection at `now_ms`. True when a window ended: `winner` is then the
    /// projection most draws in it used.
    bool Add(const Projection& p, int64_t now_ms, Projection& winner) {
        if (count_ == 0) {
            start_ms_ = now_ms;
        }
        Count(p);
        if (now_ms - start_ms_ < kWindowMs) {
            return false;
        }
        int best = 0;
        for (int i = 1; i < candidates_; i++) {
            if (counts_[i] > counts_[best]) {
                best = i;
            }
        }
        winner = seen_[best];
        candidates_ = 0;
        count_ = 0;
        return true;
    }

private:
    static constexpr int kCandidates = 4;
    void Count(const Projection& p) {
        count_++;
        for (int i = 0; i < candidates_; i++) {
            // Wide enough that a camera that wobbles (Jabu-Jabu's Belly in Ocarina of Time 3D
            // changes its projection every frame) stays one candidate.
            if (std::abs(seen_[i].tan_half_y - p.tan_half_y) < 0.02f * p.tan_half_y &&
                std::abs(seen_[i].aspect - p.aspect) < 0.05f * p.aspect) {
                counts_[i]++;
                return;
            }
        }
        if (candidates_ < kCandidates) {
            seen_[candidates_] = p;
            counts_[candidates_] = 1;
            candidates_++;
        }
    }
    Projection seen_[kCandidates]{};
    int counts_[kCandidates]{};
    int candidates_ = 0;
    int count_ = 0;
    int64_t start_ms_ = 0;
};

} // namespace VideoCore::Immersive
