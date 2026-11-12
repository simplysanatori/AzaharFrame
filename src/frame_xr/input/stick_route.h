// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cmath>
#include <cstdint>
#include "frame_xr/input/types.h"

// AzaharFrame padsui
// Which stick of a device drives a 3DS stick (Circle Pad, C-Stick), and how: its axes may be
// swapped and either may be turned round, as Edit Controls learns them from two pushes ("Push the
// Stick Up", then "Right"). Kept in one int8_t (the RemapTable style, -1 for none): bits 0-1 the
// Input::AnalogControl, bit 2 axes swapped, bit 3 X turned round, bit 4 Y turned round. A plain
// route is just the AnalogControl's number, so older tables read the same.
//
// In the controls profile (engine frame_xr) the route is the analog's parameters:
// "analog:<n>" plus "swap_xy:1", "invert_x:1", "invert_y:1" when set.

namespace FrameXr::Input {

struct StickRoute {
    int stick = -1; ///< Input::AnalogControl, or -1: none
    bool swap_xy = false;
    bool invert_x = false;
    bool invert_y = false;
    bool Plain() const {
        return !swap_xy && !invert_x && !invert_y;
    }
    bool operator==(const StickRoute&) const = default;
};

constexpr int8_t EncodeStickRoute(const StickRoute& r) {
    if (r.stick < 0 || r.stick > 3) {
        return -1;
    }
    return static_cast<int8_t>(r.stick | (r.swap_xy ? 4 : 0) | (r.invert_x ? 8 : 0) |
                               (r.invert_y ? 16 : 0));
}

constexpr StickRoute DecodeStickRoute(int8_t v) {
    if (v < 0) {
        return {};
    }
    return {v & 3, (v & 4) != 0, (v & 8) != 0, (v & 16) != 0};
}

/// The 3DS stick's value from the device stick's (up and right positive).
inline Vec2f ApplyStickRoute(Vec2f raw, const StickRoute& r) {
    Vec2f out{r.swap_xy ? raw.y : raw.x, r.swap_xy ? raw.x : raw.y};
    if (r.invert_x) {
        out.x = -out.x;
    }
    if (r.invert_y) {
        out.y = -out.y;
    }
    return out;
}

/// One push of a stick: which stick, which of its axes (0 X, 1 Y) and which way (+1, -1).
struct StickPush {
    int stick = -1;
    int axis = 0;
    int sign = 1;
    bool operator==(const StickPush&) const = default;
};

/// A stick pushed clearly along one axis (past `on`, the other axis under half of it), or none.
inline StickPush DetectStickPush(int stick, Vec2f v, float on = 0.7f) {
    const float ax = std::fabs(v.x), ay = std::fabs(v.y);
    if (ax >= on && ay < ax * 0.5f) {
        return {stick, 0, v.x > 0.0f ? 1 : -1};
    }
    if (ay >= on && ax < ay * 0.5f) {
        return {stick, 1, v.y > 0.0f ? 1 : -1};
    }
    return {};
}

/// The route from the push that means up and the one that means right: the same stick, two
/// different axes, else none.
constexpr StickRoute RouteFromPushes(const StickPush& up, const StickPush& right) {
    if (up.stick < 0 || up.stick != right.stick || up.axis == right.axis) {
        return {};
    }
    // The 3DS's X comes from the axis pushed for right, its Y from the one pushed for up.
    StickRoute r;
    r.stick = up.stick;
    r.swap_xy = right.axis == 1;
    r.invert_x = right.sign < 0;
    r.invert_y = up.sign < 0;
    return r;
}

} // namespace FrameXr::Input
