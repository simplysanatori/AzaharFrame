// Copyright 2026 CitraFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cmath>
#include <cstdint>

// CitraFrame
// Plain math types for the controller input code. This header (and everything in
// frame_xr/input except actions.*) has no OpenXR or Qt dependency so it can be unit tested.
// Conventions follow OpenXR: right handed, +Y up, -Z forward, quaternions are x,y,z,w, and a quad
// layer faces +Z in its own space.

namespace FrameXr::Input {

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quatf {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Posef {
    Quatf orientation;
    Vec3f position;
};

enum class Hand : int { Left = 0, Right = 1 };
constexpr int kNumHands = 2;

constexpr int HandIndex(Hand hand) {
    return static_cast<int>(hand);
}

constexpr Hand OtherHand(Hand hand) {
    return hand == Hand::Left ? Hand::Right : Hand::Left;
}

/// Native resolution of the 3DS bottom (touch) screen. Touch events are in these pixels.
constexpr float kBottomScreenWidthPx = 320.0f;
constexpr float kBottomScreenHeightPx = 240.0f;

} // namespace FrameXr::Input
