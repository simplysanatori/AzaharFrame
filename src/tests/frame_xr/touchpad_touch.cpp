// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/touchpad_touch.h"

// AzaharFrame touchpad
using Catch::Approx;
using namespace FrameXr::Input;

namespace {

constexpr int64_t kMs = 1'000'000;

struct Rig {
    TouchpadTouch t;
    TouchpadMode mode = TouchpadMode::Touch;
    PadSample pad{true, false, false, 0.5f, 0.5f};
    MouseSample mouse;
    bool visible = true;
    int64_t now = 1'000 * kMs;
    std::vector<TouchEvent> beam;
    std::vector<TouchEvent> events;
    TouchpadCursor cursor;

    // Runs one frame; the beam events are consumed.
    void Step() {
        TouchpadFrame f;
        f.mode = mode;
        f.pad = pad;
        f.mouse = mouse;
        f.bottom_visible = visible;
        f.time_ns = now;
        f.beam = beam.data();
        f.beam_count = static_cast<int>(beam.size());
        const TouchpadResult r = t.Update(f);
        events.assign(r.events.begin(), r.events.begin() + r.event_count);
        cursor = r.cursor;
        beam.clear();
        now += 8 * kMs;
    }
    void Settle() { // the first frame after a start: nothing down
        Step();
    }
};

} // namespace

TEST_CASE("FrameXr touchpad: mode values", "[frame_xr]") {
    REQUIRE(TouchpadModeFrom(0) == TouchpadMode::Off);
    REQUIRE(TouchpadModeFrom(1) == TouchpadMode::Touch);
    REQUIRE(TouchpadModeFrom(2) == TouchpadMode::Click);
    REQUIRE(TouchpadModeFrom(9) == TouchpadMode::Off);
}

TEST_CASE("FrameXr touchpad: the pad maps absolutely onto the bottom screen", "[frame_xr]") {
    Rig r;
    r.Settle();
    r.pad.contact = true;
    r.pad.x = 0.25f;
    r.pad.y = 0.75f;
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Pressed);
    REQUIRE(r.events[0].x == Approx(80.0f));
    REQUIRE(r.events[0].y == Approx(180.0f));
    // Corners stay inside the last pixel.
    r.pad.x = 1.0f;
    r.pad.y = 1.0f;
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Moved);
    REQUIRE(r.events[0].x < 320.0f);
    REQUIRE(r.events[0].x > 319.9f);
    REQUIRE(r.events[0].y < 240.0f);
    r.pad.x = -0.2f; // out of range is clamped
    r.pad.y = 0.0f;
    r.Step();
    REQUIRE(r.events[0].x == Approx(0.0f));
    r.pad.contact = false;
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Released);
}

TEST_CASE("FrameXr touchpad: a still finger sends no moves", "[frame_xr]") {
    Rig r;
    r.Settle();
    r.pad.contact = true;
    r.Step();
    r.Step();
    REQUIRE(r.events.empty());
    r.pad.x = 0.5f + 0.0005f; // 0.16 px: below the threshold
    r.Step();
    REQUIRE(r.events.empty());
}

TEST_CASE("FrameXr touchpad: Off changes nothing, the beam passes through", "[frame_xr]") {
    Rig r;
    r.mode = TouchpadMode::Off;
    r.Settle();
    r.pad.contact = true;
    r.pad.click = true;
    r.mouse = {true, true, 0.5f, 0.5f};
    r.Step();
    REQUIRE(r.events.empty());
    REQUIRE_FALSE(r.cursor.visible);
    r.beam = {{TouchEventType::Pressed, 10.0f, 20.0f}, {TouchEventType::Moved, 11.0f, 21.0f}};
    r.Step();
    REQUIRE(r.events.size() == 2);
    REQUIRE(r.events[0].type == TouchEventType::Pressed);
    REQUIRE(r.events[1].x == Approx(11.0f));
    r.beam = {{TouchEventType::Released, 0.0f, 0.0f}};
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Released);
}

TEST_CASE("FrameXr touchpad: Click mode, contact only aims", "[frame_xr]") {
    Rig r;
    r.mode = TouchpadMode::Click;
    r.Settle();
    r.pad.contact = true;
    r.pad.x = 0.5f;
    r.pad.y = 0.5f;
    r.Step();
    REQUIRE(r.events.empty());
    REQUIRE(r.cursor.visible); // the dot hovers
    REQUIRE(r.cursor.source == TouchSource::Pad);
    REQUIRE_FALSE(r.cursor.pressed);
    REQUIRE(r.cursor.px == Approx(160.0f));
    r.pad.click = true;
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Pressed);
    REQUIRE(r.cursor.pressed);
    r.pad.x = 0.75f; // drag while clicked
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Moved);
    r.pad.click = false; // let go of the click, finger stays: the touch ends, the dot stays
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Released);
    REQUIRE(r.cursor.visible);
    r.pad.contact = false;
    r.Step();
    REQUIRE_FALSE(r.cursor.visible);
}

TEST_CASE("FrameXr touchpad: Touch mode shows the dot while touching", "[frame_xr]") {
    Rig r;
    r.Settle();
    r.pad.contact = true;
    r.Step();
    REQUIRE(r.cursor.visible);
    REQUIRE(r.cursor.pressed);
    REQUIRE(r.cursor.u == Approx(0.5f));
    REQUIRE(r.cursor.v == Approx(0.5f));
    r.pad.contact = false;
    r.Step();
    REQUIRE_FALSE(r.cursor.visible);
}

TEST_CASE("FrameXr touchpad: no pad, no touch", "[frame_xr]") {
    Rig r;
    r.pad.present = false;
    r.Settle();
    r.pad.contact = true;
    r.Step();
    REQUIRE(r.events.empty());
    REQUIRE_FALSE(r.cursor.visible);
}

TEST_CASE("FrameXr touchpad: a hidden bottom screen has no touch", "[frame_xr]") {
    Rig r;
    r.Settle();
    r.pad.contact = true;
    r.Step();
    REQUIRE(r.t.touching());
    r.visible = false;
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Released);
    REQUIRE_FALSE(r.cursor.visible);
    r.Step();
    r.visible = true; // shown again with the finger still down: the touch begins again
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Pressed);
}

TEST_CASE("FrameXr touchpad: the last source wins, a release ends the touch", "[frame_xr]") {
    Rig r;
    r.Settle();
    r.pad.contact = true;
    r.pad.x = 0.0f;
    r.pad.y = 0.0f;
    r.Step();
    REQUIRE(r.t.owner() == TouchSource::Pad);
    // The beam presses: the touch moves to the beam's point, no second press.
    r.beam = {{TouchEventType::Pressed, 100.0f, 100.0f}};
    r.Step();
    REQUIRE(r.t.owner() == TouchSource::Beam);
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Moved);
    REQUIRE(r.events[0].x == Approx(100.0f));
    REQUIRE_FALSE(r.cursor.visible); // the beam has its own dot
    // The pad moving does not fight the beam.
    r.pad.x = 0.9f;
    r.Step();
    REQUIRE(r.events.empty());
    // The beam moves and releases: the touch ends, though the finger is still on the pad.
    r.beam = {{TouchEventType::Moved, 120.0f, 100.0f}};
    r.Step();
    REQUIRE(r.events.size() == 1);
    r.beam = {{TouchEventType::Released, 0.0f, 0.0f}};
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Released);
    REQUIRE_FALSE(r.t.touching());
    r.pad.x = 0.1f;
    r.Step();
    REQUIRE(r.events.empty()); // not back until a new contact
    r.pad.contact = false;
    r.Step();
    r.pad.contact = true;
    r.Step();
    REQUIRE(r.t.owner() == TouchSource::Pad);
}

TEST_CASE("FrameXr touchpad: the pad takes over from the beam on a new contact", "[frame_xr]") {
    Rig r;
    r.Settle();
    r.beam = {{TouchEventType::Pressed, 50.0f, 60.0f}};
    r.Step();
    REQUIRE(r.t.owner() == TouchSource::Beam);
    r.pad.contact = true;
    r.pad.x = 0.5f;
    r.pad.y = 0.5f;
    r.Step();
    REQUIRE(r.t.owner() == TouchSource::Pad);
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Moved);
    REQUIRE(r.events[0].x == Approx(160.0f));
    // The beam's late moves are not sent; its release is not the pad's.
    r.beam = {{TouchEventType::Moved, 55.0f, 60.0f}, {TouchEventType::Released, 0.0f, 0.0f}};
    r.Step();
    REQUIRE(r.events.empty());
    REQUIRE(r.t.owner() == TouchSource::Pad);
}

TEST_CASE("FrameXr touchpad: the mouse touches with the left button", "[frame_xr]") {
    Rig r;
    r.Settle();
    r.mouse = {true, false, 0.5f, 0.25f};
    r.Step();
    REQUIRE(r.events.empty()); // hover only
    REQUIRE(r.cursor.visible == false); // first sighting is not a move
    r.mouse.x = 0.6f;
    r.Step();
    REQUIRE(r.cursor.visible);
    REQUIRE(r.cursor.source == TouchSource::Mouse);
    REQUIRE(r.cursor.py == Approx(60.0f));
    r.mouse.down = true;
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Pressed);
    REQUIRE(r.events[0].x == Approx(192.0f));
    r.mouse.inside = false; // the pointer left the window: the touch ends
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Released);
}

TEST_CASE("FrameXr touchpad: the mouse dot fades after the pointer rests", "[frame_xr]") {
    Rig r;
    r.Settle();
    r.mouse = {true, false, 0.5f, 0.5f};
    r.Step();
    r.mouse.x = 0.55f;
    r.Step();
    REQUIRE(r.cursor.visible);
    r.now += TouchpadTouch::kMouseIdleNs + kMs;
    r.Step();
    REQUIRE_FALSE(r.cursor.visible);
    r.mouse.y = 0.6f;
    r.Step();
    REQUIRE(r.cursor.visible);
}

TEST_CASE("FrameXr touchpad: a button already down at the start waits for a release",
          "[frame_xr]") {
    Rig r;
    r.mouse = {true, true, 0.5f, 0.5f};
    r.Step();
    REQUIRE(r.events.empty());
    r.mouse.down = false;
    r.Step();
    r.mouse.down = true;
    r.Step();
    REQUIRE(r.events.size() == 1);
    // Reset (the menu opened): forgets, reports a touch in progress once.
    REQUIRE(r.t.Reset());
    REQUIRE_FALSE(r.t.Reset());
    r.Step();
    REQUIRE(r.events.empty());
}

TEST_CASE("FrameXr touchpad: the pad and the mouse arbitrate, the later wins", "[frame_xr]") {
    Rig r;
    r.Settle();
    r.pad.contact = true;
    r.Step();
    REQUIRE(r.t.owner() == TouchSource::Pad);
    r.mouse = {true, false, 0.1f, 0.1f};
    r.Step();
    r.mouse.down = true;
    r.Step();
    REQUIRE(r.t.owner() == TouchSource::Mouse);
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Moved);
    r.mouse.down = false;
    r.Step();
    REQUIRE(r.events.size() == 1);
    REQUIRE(r.events[0].type == TouchEventType::Released);
}

TEST_CASE("FrameXr touchpad: the dot sits on the screen", "[frame_xr]") {
    PanelQuad flat{Posef{Quatf{}, Vec3f{0.0f, 1.0f, -2.0f}}, 0.8f, 0.6f, true};
    const Vec3f eye{0.0f, 1.0f, 0.0f};
    DotPose d = DotOnQuad(flat, 0.5f, 0.5f, eye);
    REQUIRE(d.position.x == Approx(0.0f));
    REQUIRE(d.position.y == Approx(1.0f));
    REQUIRE(d.position.z == Approx(-2.0f));
    REQUIRE(d.size_m == Approx(0.01f + 0.003f * 2.0f));
    d = DotOnQuad(flat, 0.0f, 0.0f, eye); // top left
    REQUIRE(d.position.x == Approx(-0.4f));
    REQUIRE(d.position.y == Approx(1.3f));
    // Bent: the edge comes towards the viewer, as the beam's hit does.
    PanelQuad bent = flat;
    bent.curve_radius = 1.0f;
    d = DotOnQuad(bent, 1.0f, 0.5f, eye);
    REQUIRE(d.position.z > -2.0f);
    Ray ray{{0, 1.0f, 0}, Normalized(d.position - Vec3f{0, 1.0f, 0})};
    const QuadHit hit = IntersectQuad(ray, bent);
    REQUIRE(hit.in_bounds);
    REQUIRE(hit.u == Approx(1.0f).margin(0.01f));
    REQUIRE(hit.v == Approx(0.5f).margin(0.01f));
}
