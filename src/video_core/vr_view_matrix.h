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

#include <algorithm>
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

/// AzaharFrame immersive: whether a draw in mode 2 (Ocarina of Time 3D, Majora's Mask 3D) is
/// drawn through a perspective camera, the only kind that gets the head pose. `r` is the four
/// registers before the view matrix (0 to 3 there, where the game keeps its projection). CitraVR
/// patched every draw with the bool uniform set and skipped only the HUD (by one value of its
/// orthographic projection), so other screen-space passes got the head pose too: a full-screen
/// pass that draws an earlier picture again was turned and moved with the head and fed into
/// itself frame after frame, and what it no longer covered kept old pixels. A perspective
/// projection's last row is (0, 0, +-1, 0); an orthographic one's is (0, 0, 0, 1).
inline bool HasPerspectiveProjection(const float r[4][4]) {
    for (int j = 0; j < 4; j++) {
        if (!std::isfinite(r[3][j])) {
            return false;
        }
    }
    return std::abs(r[3][0]) <= 1e-4f && std::abs(r[3][1]) <= 1e-4f &&
           std::abs(std::abs(r[3][2]) - 1.0f) <= 1e-3f && std::abs(r[3][3]) <= 1e-3f;
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

/// AzaharFrame stereo: where a top screen camera's projection (`r`, as ProjectionFromRows) puts
/// a point straight ahead at infinity, across the picture: -1 to 1 over its width, 0 without a
/// stereo shear. The game's own 3D gives each eye's projection an opposite shear (the eye's
/// shift itself moves nothing at infinity), so the two eyes' values are the far scenery's
/// disparity. False when `r` is not a top screen camera.
inline bool FarAcrossFromRows(const float r[4][4], float& ndc) {
    Projection p;
    if (!ProjectionFromRows(r, p)) {
        return false;
    }
    const bool first_is_x = std::abs(r[0][0]) > std::abs(r[0][1]);
    const float* hr = first_is_x ? r[0] : r[1];
    ndc = hr[2] / r[3][2];
    return std::isfinite(ndc) && std::abs(ndc) < 1.0f;
}

/// AzaharFrame stereo: the far points (FarAcrossFromRows) of one top screen frame's world draws,
/// grouped: the game's eyes are the groups (one per eye with its 3D on, one when it draws its
/// world once). Emulation thread; pure, unit tested in src/tests/frame_xr/stereo.cpp.
class StereoFrame {
public:
    static constexpr int kGroups = 4;
    /// Two far points in one group: this close (NDC) or this share of the larger one.
    static constexpr float kTolerance = 2e-4f;
    static constexpr float kShare = 0.01f;
    /// A group counts as an eye with at least this share of the largest group's draws.
    static constexpr float kEyeShare = 0.2f;

    struct Result {
        uint32_t draws = 0;    ///< world draws with a top screen camera
        int eyes = 0;          ///< 0 (none), 1 (drawn once), 2 (two eyes)
        float far_ndc = 0.0f;  ///< the two eyes' far points apart (NDC, 0 to 2); 0 below 2 eyes
    };

    void Draw(float ndc) noexcept {
        ++draws_;
        for (int i = 0; i < groups_; ++i) {
            const float tol = std::max(kTolerance, kShare * std::abs(group_[i].ndc));
            if (std::abs(group_[i].ndc - ndc) <= tol) {
                ++group_[i].draws;
                return;
            }
        }
        if (groups_ < kGroups) {
            group_[groups_++] = {ndc, 1};
        }
    }

    /// The frame ended: what it held, and a fresh start.
    Result End() noexcept {
        Result r;
        r.draws = draws_;
        int a = -1, b = -1; // the two largest groups
        for (int i = 0; i < groups_; ++i) {
            if (a < 0 || group_[i].draws > group_[a].draws) {
                b = a;
                a = i;
            } else if (b < 0 || group_[i].draws > group_[b].draws) {
                b = i;
            }
        }
        if (a >= 0) {
            r.eyes = 1;
            if (b >= 0 && static_cast<float>(group_[b].draws) >=
                              kEyeShare * static_cast<float>(group_[a].draws)) {
                r.eyes = 2;
                r.far_ndc = std::abs(group_[a].ndc - group_[b].ndc);
            }
        }
        *this = StereoFrame{};
        return r;
    }

private:
    struct Group {
        float ndc = 0.0f;
        uint32_t draws = 0;
    };
    Group group_[kGroups]{};
    int groups_ = 0;
    uint32_t draws_ = 0;
};

/// AzaharFrame stereo: the game's far scenery per unit of the 3D slider it is shown (its far
/// width, StereoFrame's far_ndc / 2, over the slider), from frames with both eyes drawn while the
/// slider had held for kStableSwaps swaps (the game reads it once a frame, so a frame just after
/// a change may still have the old one); the median of the last kSamples. The game's own 3D is
/// linear in the slider (Ocarina of Time 3D: 0.117 % of the width per percent, at 10, 30 and 50).
class FarPerSlider {
public:
    static constexpr int kStableSwaps = 4;
    static constexpr int kSamples = 9;
    static constexpr float kMinSlider = 0.001f;

    void Swap(int eyes, float far_width, float slider) noexcept {
        if (slider != last_slider_) {
            last_slider_ = slider;
            stable_ = 0;
            return;
        }
        if (stable_ < kStableSwaps) {
            ++stable_;
        }
        if (stable_ < kStableSwaps || eyes < 2 || slider < kMinSlider || !(far_width > 0.0f)) {
            return;
        }
        samples_[next_] = far_width / slider;
        next_ = (next_ + 1) % kSamples;
        count_ = count_ < kSamples ? count_ + 1 : kSamples;
    }
    /// Below 0 until a frame was measured.
    float Value() const noexcept {
        if (count_ == 0) {
            return -1.0f;
        }
        float v[kSamples];
        for (int i = 0; i < count_; ++i) {
            v[i] = samples_[i];
        }
        std::nth_element(v, v + count_ / 2, v + count_);
        return v[count_ / 2];
    }
    void Reset() noexcept {
        *this = FarPerSlider{};
    }

private:
    float samples_[kSamples]{};
    int count_ = 0, next_ = 0, stable_ = 0;
    float last_slider_ = -1.0f;
};

/// AzaharFrame oot: a screen of the game's own in front of its world (Ocarina of Time 3D's item
/// screen): with the game's 3D on (slider above 0) its world is drawn for one eye only, or not at
/// all, for kSwaps top screen swaps in a row, after the game drew both eyes earlier (so a game or
/// a screen that never draws two eyes is never taken for one). The world's draws are still there
/// (the paused world behind it), so "no world draw" alone would miss it; a game that draws its
/// world on every other swap only (Super Mario 3D Land's title) never has kSwaps in a row.
/// Gameplay draws both eyes in every frame, standing still too (measured: Ocarina of Time 3D,
/// Majora's Mask 3D, Mario Kart 7, Kid Icarus: Uprising). A heuristic: the Full mode 2 uses it to
/// show such a screen at the game's own size (frame_xr/immersive.cpp, IMMERSIVE.md).
class WorldOnceRun {
public:
    static constexpr int kSwaps = 4;

    /// One top screen swap: its eyes (StereoFrame) and the slider the game was shown. True while
    /// the run holds.
    bool Swap(int eyes, float slider) noexcept {
        if (!(slider > 0.0f)) {
            run_ = 0;
            return on_ = false;
        }
        if (eyes >= 2) {
            seen_two_ = true;
            run_ = 0;
            return on_ = false;
        }
        if (run_ < kSwaps) {
            ++run_;
        }
        return on_ = seen_two_ && run_ >= kSwaps;
    }
    bool On() const noexcept {
        return on_;
    }
    void Reset() noexcept {
        *this = WorldOnceRun{};
    }

private:
    int run_ = 0;
    bool seen_two_ = false;
    bool on_ = false;
};

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
