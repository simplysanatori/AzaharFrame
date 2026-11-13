// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>

#include "common/logging/log.h"
#include "frame_xr/auto_shape.h"
#include "video_core/vr_auto_shape.h"

namespace FrameXr::AutoShape {

namespace {
constexpr std::array<Shape, 4> kShapes{{
    {0, kAspect3ds},
    {5, 16.0f / 10.0f},
    {1, 16.0f / 9.0f},
    {2, 21.0f / 9.0f},
}};

const char* NameOf(std::uint32_t value) {
    switch (value) {
    case 1:
        return "16:9";
    case 2:
        return "21:9";
    case 5:
        return "16:10";
    default:
        return "5:3";
    }
}
} // namespace

std::optional<Shape> NearestShape(float aspect) {
    if (!(aspect > 0.0f)) {
        return std::nullopt;
    }
    const Shape* best = nullptr;
    float best_err = 0.0f;
    for (const Shape& s : kShapes) {
        const float err = std::abs(aspect - s.aspect) / s.aspect;
        if (!best || err < best_err) {
            best = &s;
            best_err = err;
        }
    }
    if (best_err > Decider::kTolerance) {
        return std::nullopt;
    }
    return *best;
}

void Decider::Reset(float start_aspect) {
    *this = Decider{};
    if (const auto s = NearestShape(start_aspect)) {
        current_ = *s;
    }
    from_aspect_ = current_.aspect;
}

bool Decider::Add(float aspect, std::int64_t now_ms) {
    measured_ = aspect;
    if (last_ms_ != 0 && now_ms - last_ms_ > kGapMs) {
        pending_.reset(); // not seen for a while: start counting again
    }
    last_ms_ = now_ms;
    const std::optional<Shape> near = NearestShape(aspect);
    // Hysteresis: within the tolerance of the shape in use, it stays.
    if (!near || near->value == current_.value ||
        std::abs(aspect - current_.aspect) <= kTolerance * current_.aspect) {
        pending_.reset();
        return false;
    }
    if (!pending_ || pending_->value != near->value) {
        pending_ = near;
        pending_since_ = now_ms;
        return false;
    }
    if (now_ms - pending_since_ < kStableMs) {
        return false;
    }
    from_aspect_ = AspectAt(now_ms);
    current_ = *pending_;
    changed_ms_ = now_ms;
    pending_.reset();
    return true;
}

float Decider::AspectAt(std::int64_t now_ms) const {
    if (changed_ms_ == 0 || now_ms - changed_ms_ >= kEaseMs) {
        return current_.aspect;
    }
    float t = static_cast<float>(std::max<std::int64_t>(now_ms - changed_ms_, 0)) /
              static_cast<float>(kEaseMs);
    t = t * t * (3.0f - 2.0f * t); // smoothstep
    return from_aspect_ + (current_.aspect - from_aspect_) * t;
}

namespace {
std::atomic<float> g_aspect{kAspect3ds};
std::atomic<float> g_hint{0.0f};
struct State {
    Decider decider;
    bool live = false;
    bool measuring = false;
    bool logged_first = false;
    std::int64_t start_ms = 0;
    std::int64_t stamp = 0;
};
State& S() {
    static State s;
    return s;
}
} // namespace

void SetHint(float aspect) {
    g_hint.store(aspect > 0.0f ? aspect : 0.0f, std::memory_order_relaxed);
}

void Frame(bool screens_live, bool automatic) {
    namespace VI = VideoCore::Immersive;
    State& s = S();
    const bool on = screens_live && automatic;
    const float hint_aspect = g_hint.load(std::memory_order_relaxed);
    VI::g_measure_shape.store(on, std::memory_order_relaxed);
    const std::int64_t now = VI::SteadyMs();
    if (!screens_live) {
        s.live = false;
        s.measuring = false;
        return;
    }
    if (!s.live) {
        s.live = true;
        s.start_ms = now;
        s.stamp = 0;
        s.logged_first = false;
        s.decider.Reset(hint_aspect > 0.0f ? hint_aspect : kAspect3ds);
        g_aspect = s.decider.Current().aspect;
    }
    if (on != s.measuring) {
        s.measuring = on;
        if (on) {
            LOG_INFO(Frontend, "xr: Screen Shape Automatic: {} to start{}",
                     NameOf(s.decider.Current().value),
                     hint_aspect > 0.0f ? " (a mod's aspect)" : "");
        }
    }
    if (on) {
        float tan_half_y = 0.0f, aspect = 0.0f;
        std::int64_t stamp = 0;
        if (VI::g_measured_shape.Read(std::max(now - 1000, s.start_ms), tan_half_y, aspect,
                                      stamp) &&
            stamp != s.stamp) {
            s.stamp = stamp;
            if (!s.logged_first) {
                s.logged_first = true;
                LOG_INFO(Frontend,
                         "xr: Screen Shape Automatic: game camera measured {:.3f} wide, {:.1f} "
                         "degrees high",
                         aspect, 2.0f * std::atan(tan_half_y) * 57.29578f);
            }
            const std::uint32_t before = s.decider.Current().value;
            if (s.decider.Add(aspect, stamp)) {
                LOG_INFO(Frontend, "xr: Screen Shape Automatic: {} to {} (measured {:.3f} for {} s)",
                         NameOf(before), NameOf(s.decider.Current().value), aspect,
                         Decider::kStableMs / 1000);
            }
        }
    }
    g_aspect = s.decider.AspectAt(now);
}

float Aspect() {
    return g_aspect.load(std::memory_order_relaxed);
}

} // namespace FrameXr::AutoShape
