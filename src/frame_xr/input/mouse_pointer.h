// Adapted from Frametop's pointer helper (pointer/helper/ft-pointer.cpp, pointer/common/vec3.h,
// screens/posemath.h; https://github.com/Frametop/frametop) for Azahar for Frame: its 3D mouse's
// cursor model, driving our own ray instead of a SteamVR driver.
//
// MIT License
//
// Copyright (c) 2026 DeeJanuz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstdint>
#include "frame_xr/input/input_frame.h"
#include "frame_xr/input/mouse.h"
#include "frame_xr/input/types.h"

// AzaharFrame padsui
// Mouse Mode: Pointer. The mouse turns a cursor about you, fixed in the room: yaw and pitch from
// where the ray starts (the eye as the pointer wakes), so moving your head leaves it where it
// is. Its ray stands in for the right controller's (ApplyMousePointer), so the menu, the
// screens, the keyboard and the grab bars take it as they take a beam.
//   - Waking: a deliberate move (kWakeCounts within a second; a mouse resting on a desk
//     jitters less) or a click. A woken pointer starts where you look.
//   - Last used wins: the right controller moved on purpose (faster than 0.35 m/s or 2 rad/s
//     for 100 ms, half a second after the mouse was last used) has its ray back; so does every
//     controller after kIdleNs without the mouse.
//   - The dot: on what the ray hits, else on the plane of the panel it just left (within
//     kEdgeReach, for its edges and grab bars), else kFreeDistance out; while a click drags,
//     at the distance it had when the click went down.
//   - Head follow (off unless asked): past a leash the cursor comes along as the head turns.
namespace FrameXr::Input {

struct MousePointerSettings {
    float deg_per_count = 0.03f;
    int64_t idle_ns = 30'000'000'000;
    int64_t wake_counts = 40;
    float pickup_scale = 1.0f;
    float free_distance = 1.5f; ///< metres to the dot when the ray hits nothing
    float cursor_deg = 0.4f;    ///< the dot's size
    float edge_reach = 0.3f;    ///< metres off a panel's edge the dot stays on its plane
    bool follow = false;        ///< head follow
    float leash_deg = 10.0f;
    float leash_delay = 0.2f;  ///< seconds
    float leash_return = 0.2f; ///< seconds
    float follow_reach = 70.0f;
};

class MousePointer {
public:
    static constexpr float kPitchClampDeg = 85.0f;
    static constexpr int64_t kWakeWindowNs = 1'000'000'000;
    static constexpr int64_t kResumePauseNs = 1'500'000'000;
    static constexpr int64_t kPickupQuietNs = 500'000'000;
    static constexpr int64_t kPickupForNs = 100'000'000;

    struct Result {
        bool active = false;
        Posef aim;   ///< the ray: from the anchor, -Z along it
        Vec3f dir;   ///< the ray's direction
        bool took = false; ///< woke this frame
        bool gave_back = false; ///< a controller took the ray back this frame
    };

    /// One frame. `pointer`: Mouse Mode is Pointer and a mouse is there. `real`: the right
    /// controller as read (it takes the ray back). `head`: the head's pose.
    Result Update(const MouseFrame& mouse, bool pointer, const HandInput& real, const Posef& head,
                  bool head_valid, int64_t now_ns);

    struct Dot {
        bool shown = false;
        Vec3f position;
        float size_m = 0.0f;
        bool on_panel = false;
    };
    /// Where the dot goes for this frame's ray: `hit_t` the distance to the nearest thing it
    /// hits (< 0 none) and that thing's normal; `dragging` a click held.
    Dot PlaceDot(float hit_t, const Vec3f& hit_normal, bool dragging, const Vec3f& eye);

    void Reset();
    bool active() const {
        return active_;
    }
    float yaw_deg() const {
        return yaw_;
    }
    float pitch_deg() const {
        return pitch_;
    }
    void set_settings(const MousePointerSettings& s) {
        settings_ = s;
    }
    const MousePointerSettings& settings() const {
        return settings_;
    }

private:
    void Wake(int64_t now);
    void HeadFollow(const Posef& head, int64_t now);

    MousePointerSettings settings_;
    bool active_ = false;
    bool recenter_ = false;
    bool anchored_ = false;
    Vec3f anchor_;
    float yaw_ = 0.0f;   ///< degrees, 0 along -Z, left positive
    float pitch_ = 0.0f; ///< degrees, up positive
    int64_t last_used_ = 0;
    int64_t pending_ = 0;
    int64_t pending_since_ = 0;
    // The right controller, for last used wins.
    bool real_prev_valid_ = false;
    Posef real_prev_;
    int64_t real_prev_ns_ = 0;
    int64_t moving_since_ = 0;
    // Head follow.
    bool follow_reset_ = true;
    bool following_ = false;
    Vec3f follow_ref_;
    float follow_lag_ = 0.0f;
    int64_t leash_out_since_ = 0;
    int64_t follow_at_ = 0;
    // The dot.
    bool edge_known_ = false;
    Vec3f edge_point_;
    Vec3f edge_normal_;
    Vec3f edge_last_;
    float last_distance_ = 1.5f;
    float drag_distance_ = 1.5f;
    bool was_dragging_ = false;
};

/// A direction from yaw and pitch in degrees (-Z at 0, 0; left and up positive).
Vec3f DirectionOf(float yaw_deg, float pitch_deg);
/// An aim orientation looking along `dir` (-Z), its X level.
Quatf AimOrientation(const Vec3f& dir);

} // namespace FrameXr::Input
