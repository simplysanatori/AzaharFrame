// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fixes7: Screen Shape: Automatic (Settings::values.vr_top_aspect kAutomatic). The
// top screen takes the shape the game camera draws with: a widescreen mod or a game's own aspect
// option draws anamorphic into the 400x240 top screen (16:9 about 1.78, 21:9 about 2.33), the
// game itself at 5:3. The emulation thread measures the projection (video_core/vr_auto_shape.h);
// Decider picks the shape from it:
//   - the nearest of 5:3, 16:10, 16:9 and 21:9 within kTolerance (anything else, a 4:3 close-up
//     or a stretched effect, is not a shape and is ignored);
//   - only after the same shape was measured for kStableMs without a gap over kGapMs, so a short
//     cutscene or a menu's camera does not switch it;
//   - with hysteresis: while the measurement is within kTolerance of the shape in use, it stays;
//   - nothing measured (a 2D game, a menu, a game whose projection cannot be read): the shape in
//     use stays;
//   - the change is eased over kEaseMs (no jump, no flicker).
// A new game starts at the hint (a catalog mod's "aspect"), else 5:3. Nothing is written to the
// settings: the measured shape lives here only.

#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

namespace FrameXr::AutoShape {

/// Settings::values.vr_top_aspect's Automatic.
inline constexpr std::uint32_t kAutomatic = 6;
inline constexpr float kAspect3ds = 400.0f / 240.0f;

/// The Screen Shape value (vr_top_aspect) and its aspect: 5:3 (0), 16:10 (5), 16:9 (1), 21:9 (2).
struct Shape {
    std::uint32_t value = 0;
    float aspect = kAspect3ds;
};
/// The candidate nearest `aspect` within kTolerance (relative), or none.
std::optional<Shape> NearestShape(float aspect);

class Decider {
public:
    static constexpr float kTolerance = 0.035f;
    static constexpr std::int64_t kStableMs = 2000;
    static constexpr std::int64_t kGapMs = 1000;
    static constexpr std::int64_t kEaseMs = 400;

    /// A new game: the shape `start` (the hint's, or 5:3), at once.
    void Reset(float start_aspect = kAspect3ds);
    /// One measurement (a quarter second's winner) at `now_ms`. True when the shape changed.
    bool Add(float aspect, std::int64_t now_ms);
    /// The aspect to draw with at `now_ms` (eased towards Current()).
    float AspectAt(std::int64_t now_ms) const;
    const Shape& Current() const {
        return current_;
    }
    /// The aspect last measured (0: none since Reset).
    float Measured() const {
        return measured_;
    }

private:
    Shape current_{};
    float from_aspect_ = kAspect3ds;
    std::int64_t changed_ms_ = 0; ///< when the ease started (0: no ease)
    std::optional<Shape> pending_;
    std::int64_t pending_since_ = 0;
    std::int64_t last_ms_ = 0;
    float measured_ = 0.0f;
};

/// GUI thread, when a game boots: the aspect its catalog mods give (0: none), the shape a new
/// game starts at with Automatic until it is measured otherwise.
void SetHint(float aspect);
/// XR thread, each frame: measures while a game runs with Automatic (`screens_live`, the
/// setting), resets for a new game. Logs each change.
void Frame(bool screens_live, bool automatic);
/// The aspect Automatic draws with now (5:3 until a game measured otherwise). Any thread.
float Aspect();
/// The shape Automatic decided for the game running now (XR thread: the log and tests).
Shape CurrentShape();

} // namespace FrameXr::AutoShape
