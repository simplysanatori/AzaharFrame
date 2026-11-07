// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// The pointer beams and when the controllers count as idle. A beam is a quad of pre-drawn art
// (Moonlight XR's: a soft line, faded at both ends) laid along the ray and turned to the head;
// SteamVR has no colour scale for layers, so the art holds kBeamLevels copies at falling
// strength side by side and a fade picks one by its image rectangle. Nothing here is drawn per
// frame, so beams cost no GPU work in a game. No OpenXR or Vulkan here (unit tested).

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "frame_xr/input/geometry.h"
#include "frame_xr/input/input_frame.h"

namespace FrameXr::Vis {

constexpr int kBeamLevels = 8;
constexpr int kBeamTexWidth = 16; ///< one level
constexpr int kBeamTexHeight = 256;

/// The art: kBeamLevels columns of kBeamTexWidth x kBeamTexHeight, level 0 at full strength,
/// premultiplied RGBA8 (white), rows top down. The beam runs along the rows.
std::vector<uint8_t> BeamArt();

/// The art level for a strength 0..1 (no level for 0).
std::optional<int> BeamLevel(float strength);

/// A beam quad: centred between its ends, its +Y along the beam, facing the head.
struct BeamQuad {
    Input::Posef pose;
    float width = 0.0f;
    float length = 0.0f;
};

/// The quad for a beam from `start` to `end`, or none when too short or behind the head. The
/// part nearer than kBeamClipAhead in front of the head is cut (Moonlight XR's beam-streak fix:
/// a beam into or behind the eyes is drawn as a white streak across the view).
constexpr float kBeamClipAhead = 0.05f;
std::optional<BeamQuad> PlaceBeam(const Input::Vec3f& start, const Input::Vec3f& end,
                                  const Input::Posef& head, float width);

/// One controller's activity: when it last moved or was used, its fades, and the last pose it
/// was tracked at (kept a moment through a short loss of tracking).
class HandActivity {
public:
    /// Still this long: the beam fades. Still this long: the model goes too (put down).
    static constexpr int64_t kBeamIdleNs = 4'000'000'000;
    static constexpr int64_t kModelIdleNs = 30'000'000'000;
    /// A tracking loss shorter than this keeps the last pose.
    static constexpr int64_t kHoldNs = 250'000'000;

    /// Each frame. `used`: a button, trigger, grip or stick of this hand is in use.
    void Update(int64_t now_ns, const Input::HandInput& hand, bool used);

    /// Tracked now, or lost for less than kHoldNs.
    bool Tracked() const {
        return tracked;
    }
    const Input::Posef& Pose() const {
        return pose;
    }
    /// 0..1, eased in and out.
    float Beam() const {
        return beam;
    }
    float Model() const {
        return model;
    }

private:
    bool tracked = false;
    bool ever = false;
    int64_t last_seen = 0;
    int64_t last_active = 0;
    int64_t last_time = 0;
    Input::Posef pose{};
    Input::Posef anchor{};
    float beam = 0.0f;
    float model = 0.0f;
};

} // namespace FrameXr::Vis
