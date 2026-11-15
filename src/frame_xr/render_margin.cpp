// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fovaxis: see render_margin.h.

#include <algorithm>

#include "common/settings.h"
#include "frame_xr/render_margin.h"

namespace FrameXr::Immersive::Margin {

namespace {
constexpr uint32_t kMaxLevel = 4; // Layout::kMaxWiderLevel

uint32_t Clamp(int64_t level) {
    return static_cast<uint32_t>(std::clamp<int64_t>(level, 0, kMaxLevel));
}

auto& OldKey() {
    return Settings::values.vr_immersive_wider_view_level;
}
auto& AxisKey(Axis axis) {
    return axis == Axis::Horizontal ? Settings::values.vr_immersive_render_margin_x
                                    : Settings::values.vr_immersive_render_margin_y;
}
Axis Other(Axis axis) {
    return axis == Axis::Horizontal ? Axis::Vertical : Axis::Horizontal;
}
} // namespace

uint32_t Resolve(const Keys& k) {
    if (k.axis_game_set && k.axis_game >= 0) {
        return Clamp(k.axis_game);
    }
    if (k.old_game_set) {
        return Clamp(k.old_game);
    }
    return ResolveGlobal(k);
}

uint32_t ResolveGlobal(const Keys& k) {
    return k.axis_global >= 0 ? Clamp(k.axis_global) : Clamp(k.old_global);
}

bool GameDecides(const Keys& k) {
    return (k.axis_game_set && k.axis_game >= 0) || k.old_game_set;
}

Keys Read(Axis axis) {
    const auto& old = OldKey();
    const auto& own = AxisKey(axis);
    Keys k;
    k.old_global = old.GetValue(true);
    k.old_game_set = !old.UsingGlobal();
    k.old_game = old.GetValue();
    k.axis_global = own.GetValue(true);
    k.axis_game_set = !own.UsingGlobal();
    k.axis_game = own.GetValue();
    return k;
}

uint32_t Level(Axis axis) {
    return Resolve(Read(axis));
}

uint32_t GlobalLevel(Axis axis) {
    return ResolveGlobal(Read(axis));
}

bool GameHasOwn(Axis axis) {
    return GameDecides(Read(axis));
}

void Set(Axis axis, uint32_t level, bool per_game) {
    auto& own = AxisKey(axis);
    const int32_t value = static_cast<int32_t>(Clamp(level));
    if (per_game) {
        own.SetGlobal(false);
        own.SetValue(value);
        return;
    }
    if (GameHasOwn(axis)) {
        return; // the game's own value decides it; the menu locks the global row then
    }
    own.SetValue(value); // on the global value: the global one
}

bool UseGlobal(Axis axis) {
    auto& own = AxisKey(axis);
    auto& old = OldKey();
    const bool had = GameHasOwn(axis);
    own.SetGlobal(true);
    if (!old.UsingGlobal()) {
        // The game's own Render Margin decided this axis: it goes, and the other axis keeps
        // the level it showed (its own value) if it followed it.
        const Axis other = Other(axis);
        auto& other_key = AxisKey(other);
        if (other_key.UsingGlobal() || other_key.GetValue() < 0) {
            other_key.SetGlobal(false);
            other_key.SetValue(static_cast<int32_t>(Clamp(old.GetValue())));
        }
        old.SetGlobal(true);
    }
    return had;
}

Stepped StepBoth(int dir) {
    Stepped s;
    s.x_before = Level(Axis::Horizontal);
    s.y_before = Level(Axis::Vertical);
    const auto step = [dir](uint32_t level) {
        return Clamp(static_cast<int64_t>(level) + (dir > 0 ? 1 : -1));
    };
    s.x = step(s.x_before);
    s.y = step(s.y_before);
    s.changed = s.x != s.x_before || s.y != s.y_before;
    if (dir == 0 || !s.changed) {
        s.x = s.x_before;
        s.y = s.y_before;
        s.changed = false;
        return s;
    }
    const auto follows = [](Axis axis) {
        const Keys k = Read(axis);
        return !(k.axis_game_set && k.axis_game >= 0) && (k.old_game_set || k.axis_global < 0);
    };
    auto& old = OldKey();
    if (follows(Axis::Horizontal) && follows(Axis::Vertical)) {
        // Both follow Render Margin (and so are the same level): step that key, per game, as
        // every build before.
        old.SetGlobal(false);
        old.SetValue(s.x);
        return s;
    }
    Set(Axis::Horizontal, s.x, true);
    Set(Axis::Vertical, s.y, true);
    return s;
}

} // namespace FrameXr::Immersive::Margin
