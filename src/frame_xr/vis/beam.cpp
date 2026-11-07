// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "frame_xr/vis/beam.h"

namespace FrameXr::Vis {

namespace {

using Input::Vec3f;

// Moved this much from where it was last active: active again.
constexpr float kMoveMetres = 0.01f;
constexpr float kTurnCos = 0.99966f; // 1.5 degrees
// Fades, per second.
constexpr float kFadeIn = 1.0f / 0.12f;
constexpr float kFadeOut = 1.0f / 0.45f;
// Moonlight XR's beam (xr_assets.c uploadPointerArt): the across profile and the end fades.
constexpr float kAcross = 3.2f;
constexpr float kEnds = 0.12f;

Vec3f Forward(const Input::Posef& p) {
    return Input::Rotate(p.orientation, {0.0f, 0.0f, -1.0f});
}

float Step(float value, float target, float dt) {
    if (target > value) {
        return std::min(target, value + kFadeIn * dt);
    }
    return std::max(target, value - kFadeOut * dt);
}

} // namespace

std::vector<uint8_t> BeamArt() {
    const int w = kBeamLevels * kBeamTexWidth;
    std::vector<uint8_t> px(static_cast<std::size_t>(w) * kBeamTexHeight * 4, 0);
    const float mid = kBeamTexWidth * 0.5f;
    for (int level = 0; level < kBeamLevels; level++) {
        const float strength = static_cast<float>(kBeamLevels - level) / kBeamLevels;
        for (int y = 0; y < kBeamTexHeight; y++) {
            const float along = (y + 0.5f) / kBeamTexHeight;
            const float edge = std::min(along, 1.0f - along);
            const float ends = edge < kEnds ? edge / kEnds : 1.0f;
            for (int x = 0; x < kBeamTexWidth; x++) {
                const float t = std::fabs((x + 0.5f) - mid) / mid * kAcross;
                const float a = std::exp(-t * t) * ends * strength;
                const auto v = static_cast<uint8_t>(a * 255.0f + 0.5f);
                uint8_t* p = &px[(static_cast<std::size_t>(y) * w + level * kBeamTexWidth + x) * 4];
                p[0] = p[1] = p[2] = p[3] = v; // white, premultiplied
            }
        }
    }
    return px;
}

std::optional<int> BeamLevel(float strength) {
    if (strength <= 0.02f) {
        return std::nullopt;
    }
    const int level = static_cast<int>(std::lround((1.0f - std::min(strength, 1.0f)) * kBeamLevels));
    return std::clamp(level, 0, kBeamLevels - 1);
}

std::optional<BeamQuad> PlaceBeam(const Vec3f& start_in, const Vec3f& end_in,
                                  const Input::Posef& head, float width) {
    Vec3f start = start_in, end = end_in;
    const Vec3f fwd = Forward(head);
    const float s = Input::Dot(start - head.position, fwd) - kBeamClipAhead;
    const float e = Input::Dot(end - head.position, fwd) - kBeamClipAhead;
    if (s < 0.0f && e < 0.0f) {
        return std::nullopt;
    }
    if (s < 0.0f || e < 0.0f) {
        const float t = s / (s - e);
        const Vec3f cut = start + (end - start) * t;
        (s < 0.0f ? start : end) = cut;
    }
    const Vec3f along = end - start;
    const float length = Input::Length(along);
    if (length < 0.05f) {
        return std::nullopt;
    }
    const Vec3f y = along * (1.0f / length);
    const Vec3f mid = (start + end) * 0.5f;
    // Turned to the head: +Z as close to the head as the beam lets it be (Moonlight XR's
    // beamBasis, without its lean, which only matters for a beam pointing at the eyes, cut above).
    Vec3f to_head = head.position - mid;
    Vec3f z = to_head - y * Input::Dot(to_head, y);
    if (Input::Length(z) < 1e-4f) {
        const Vec3f right = Input::Rotate(head.orientation, {1.0f, 0.0f, 0.0f});
        z = Input::Cross(right, y);
        if (Input::Length(z) < 1e-4f) {
            return std::nullopt;
        }
    }
    z = Input::Normalized(z);
    const Vec3f x = Input::Normalized(Input::Cross(y, z));
    BeamQuad q;
    q.pose = {Input::FromBasis(x, y, z), mid};
    q.width = width;
    q.length = length;
    return q;
}

void HandActivity::Update(int64_t now, const Input::HandInput& hand, bool used) {
    const float dt =
        last_time ? std::clamp(static_cast<float>(now - last_time) * 1e-9f, 0.0f, 0.1f) : 0.0f;
    last_time = now;
    if (hand.active) {
        if (!ever || !tracked) {
            anchor = hand.aim;
            last_active = now; // picked up or woken: active
        }
        ever = true;
        tracked = true;
        last_seen = now;
        pose = hand.aim;
        const bool moved =
            Input::Length(hand.aim.position - anchor.position) > kMoveMetres ||
            Input::Dot(Forward(hand.aim), Forward(anchor)) < kTurnCos;
        if (moved || used) {
            anchor = hand.aim;
            last_active = now;
        }
    } else if (tracked && now - last_seen > kHoldNs) {
        tracked = false;
    }
    const int64_t still = now - last_active;
    beam = Step(beam, tracked && still < kBeamIdleNs ? 1.0f : 0.0f, dt);
    model = Step(model, tracked && still < kModelIdleNs ? 1.0f : 0.0f, dt);
    if (!tracked) {
        // Gone (asleep, out of view): no fade out of a pose that is no longer known.
        beam = 0.0f;
        model = 0.0f;
    }
}

} // namespace FrameXr::Vis
