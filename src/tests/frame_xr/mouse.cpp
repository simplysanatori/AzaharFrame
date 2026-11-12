// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/gamepads.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/mouse.h"
#include "frame_xr/input/mouse_pointer.h"
#include "frame_xr/input/touchpad_touch.h"

// AzaharFrame padsui: the mouse in VR.
using Catch::Approx;
using namespace FrameXr::Input;

namespace {

constexpr int64_t kMs = 1'000'000;
constexpr std::size_t M(MouseInput i) {
    return static_cast<std::size_t>(i);
}

struct MouseRig {
    MouseLogic logic;
    MouseRaw raw;
    int64_t now = 1'000 * kMs;
    int menu = static_cast<int>(MouseInput::MiddleClick);
    MouseFrame f;
    MouseRig() {
        raw.present = true;
        Step(0);
    }
    MouseFrame& Step(int64_t ms) {
        now += ms * kMs;
        f = logic.Update(raw, menu, now);
        return f;
    }
    void Set(int button, bool down) {
        raw.buttons[static_cast<std::size_t>(button)] = down;
    }
};

struct MouseSource final : GamepadSource {
    MouseRaw raw;
    std::string devices;
    uint64_t Generation() override {
        return 1;
    }
    void Infos(std::vector<InputCommon::SDLPadInfo>& out) override {
        out.clear();
    }
    void States(std::vector<InputCommon::SDLPadState>& out) override {
        out.clear();
    }
    std::string InputDevices() override {
        return devices;
    }
    MouseRaw Mouse() override {
        return raw;
    }
};

} // namespace

TEST_CASE("Mouse: a click goes on a moment late, a chord reaches nothing", "[frame_xr]") {
    MouseRig r;
    r.Set(1, true); // Right Click
    CHECK_FALSE(r.Step(10).held[M(MouseInput::RightClick)]); // waiting for Left
    CHECK_FALSE(r.Step(50).held[M(MouseInput::RightClick)]);
    CHECK(r.Step(40).held[M(MouseInput::RightClick)]); // past the window: a click
    r.Set(1, false);
    CHECK_FALSE(r.Step(10).held[M(MouseInput::RightClick)]);

    // A quick tap, let go while it waited, still presses for a moment.
    r.Set(1, true);
    r.Step(10);
    r.Set(1, false);
    CHECK(r.Step(10).held[M(MouseInput::RightClick)]);
    CHECK_FALSE(r.Step(60).held[M(MouseInput::RightClick)]);

    // Left then Right within the window: the chord. Nothing reaches the game; the ring fills.
    r.Set(0, true);
    r.Step(10);
    r.Set(1, true);
    MouseFrame f = r.Step(30);
    CHECK_FALSE(f.held[M(MouseInput::LeftClick)]);
    CHECK_FALSE(f.held[M(MouseInput::RightClick)]);
    CHECK(f.chord == Approx(0.0f));
    f = r.Step(1500);
    CHECK(f.chord == Approx(0.5f));
    CHECK_FALSE(f.held[M(MouseInput::LeftClick)]);
    f = r.Step(1500);
    CHECK(f.switch_mode);
    f = r.Step(500);
    CHECK_FALSE(f.switch_mode); // once
    CHECK(f.chord == Approx(0.0f));
    r.Set(0, false);
    f = r.Step(10);
    CHECK_FALSE(f.held[M(MouseInput::RightClick)]); // stays away until let go
    r.Set(1, false);
    r.Step(10);

    // Let go early: no switch, and still nothing reached the game.
    r.Set(0, true);
    r.Set(1, true);
    r.Step(10);
    f = r.Step(2000);
    CHECK(f.chord > 0.6f);
    r.Set(1, false);
    f = r.Step(10);
    CHECK_FALSE(f.switch_mode);
    CHECK(f.chord == Approx(0.0f));
    CHECK_FALSE(f.held[M(MouseInput::LeftClick)]);
    r.Set(0, false);
    r.Step(10);

    // Right held as a press already: Left joining later is a plain press too.
    r.Set(1, true);
    r.Step(10);
    r.Step(100);
    r.Set(0, true);
    f = r.Step(10);
    CHECK(f.held[M(MouseInput::RightClick)]);
    CHECK(f.held[M(MouseInput::LeftClick)]);
    CHECK(f.chord == Approx(0.0f));
}

TEST_CASE("Mouse: wheel notches, Open Menu, a release", "[frame_xr]") {
    MouseRig r;
    r.raw.wheel_up += 2;
    CHECK(r.Step(10).held[M(MouseInput::WheelUp)]);
    CHECK_FALSE(r.Step(60).held[M(MouseInput::WheelUp)]); // lets go between notches
    CHECK(r.Step(50).held[M(MouseInput::WheelUp)]);       // the second notch
    CHECK_FALSE(r.Step(60).held[M(MouseInput::WheelUp)]);
    CHECK_FALSE(r.Step(60).held[M(MouseInput::WheelUp)]);

    r.Set(2, true); // Middle Click: Open Menu
    CHECK(r.Step(10).open_menu);
    CHECK_FALSE(r.Step(10).open_menu); // once per press
    r.Set(2, false);
    r.Step(10);

    r.Set(3, true); // Back held through a release
    CHECK(r.Step(10).held[M(MouseInput::Back)]);
    r.logic.Release();
    CHECK_FALSE(r.Step(10).held[M(MouseInput::Back)]);
    r.Set(3, false);
    r.Step(10);
    r.Set(3, true);
    CHECK(r.Step(10).held[M(MouseInput::Back)]);

    r.raw.x += 30;
    r.raw.y -= 4;
    const MouseFrame f = r.Step(10);
    CHECK(f.motion.x == Approx(30.0f));
    CHECK(f.motion.y == Approx(-4.0f));
    CHECK(f.used);
}

TEST_CASE("Mouse: its mapping, defaults and the file", "[frame_xr]") {
    const GamepadMapping d = DefaultMouseMapping();
    CHECK(d.buttons[0] == static_cast<int8_t>(MouseInput::RightClick)); // A
    CHECK(d.buttons[8] == static_cast<int8_t>(MouseInput::Back));       // L
    CHECK(d.buttons[9] == static_cast<int8_t>(MouseInput::Forward));    // R
    CHECK(d.buttons[1] == -1);
    CHECK(d.touch == static_cast<int8_t>(MouseInput::LeftClick));
    CHECK(d.open_menu == static_cast<int8_t>(MouseInput::MiddleClick));
    CHECK(d.mode == 0);
    CHECK(d.recentre);
    CHECK(std::string{MouseInputName(static_cast<int>(MouseInput::WheelDown))} == "Wheel Down");

    std::array<bool, kNumMouseInputs> held{};
    held[M(MouseInput::RightClick)] = true;
    held[M(MouseInput::LeftClick)] = true; // the touch: not a button
    const ThreeDsPadState s = ApplyMouseMapping(held, d);
    CHECK(s.buttons[0]);
    CHECK_FALSE(s.buttons[1]);

    // The mouse's line carries its extras; a gamepad's line without them reads as before.
    GamepadMapping m = d;
    m.mode = 1;
    m.speed = 2;
    m.recentre = false;
    m.touch = -1;
    MappingFile file;
    file.Set("mouse", {m, 0, 0, "Mouse"});
    const std::string text = file.Format();
    CHECK(text.find(" mode=1") != std::string::npos);
    CHECK(text.find(" recentre=0") != std::string::npos);
    CHECK(text.find(" touch=") == std::string::npos); // -1 is the plain value
    MappingFile back;
    back.Parse(text);
    REQUIRE(back.Find("mouse", 0, 0, ""));
    CHECK(back.Find("mouse", 0, 0, "")->mapping == m);

    MappingFile old;
    old.Parse("abc buttons=1,0,3,2,11,12,13,14,9,10,6,4,-1,-1,21,22,-1,-1 analogs=0,1 "
              "vendor=045e product=028e name=Old Pad\n");
    REQUIRE(old.Find("abc", 0, 0, ""));
    const GamepadMapping& o = old.Find("abc", 0, 0, "")->mapping;
    CHECK(o.touch == -1);
    CHECK(o.mode == 0);
    CHECK(o.speed == 1);
    CHECK(o.recentre);
}

TEST_CASE("Mouse: the hub keeps its mapping, listens, switches the mode", "[frame_xr]") {
    MouseSource src;
    src.raw.present = true;
    src.devices = "I: Bus=0003 Vendor=046d Product=c077 Version=0111\n"
                  "N: Name=\"Logitech USB Optical Mouse\"\n"
                  "H: Handlers=mouse0 event7\n"
                  "B: REL=903\n\n"
                  "I: Bus=0006 Vendor=4d44 Product=0001 Version=0001\n"
                  "N: Name=\"frametop virtual mouse\"\n"
                  "H: Handlers=mouse1 event9\n"
                  "B: REL=1943\n\n";
    std::string saved;
    GamepadHub hub(src, {[&] { return saved; }, [&](const std::string& t) {
                             saved = t;
                             return true;
                         }});
    const auto mice = ParseInputDevices(src.devices);
    REQUIRE(mice.size() == 1);
    CHECK(mice[0].event == "event7");
    const auto relay = ParseRelayMice(src.devices);
    REQUIRE(relay.size() == 1);
    CHECK(relay[0].event == "event9");

    const std::vector<DeviceInfo> list = hub.Devices(0);
    REQUIRE(list.size() == 2);
    CHECK(list[1].kind == DeviceKind::Mouse);
    CHECK(hub.Mapping(kMouseDeviceId, {}) == DefaultMouseMapping());

    GamepadMapping m = DefaultMouseMapping();
    m.buttons[1] = static_cast<int8_t>(MouseInput::WheelDown); // B
    REQUIRE(hub.SetMapping(kMouseDeviceId, m));
    CHECK(saved.find("mouse buttons=") != std::string::npos);
    CHECK(hub.Mapping(kMouseDeviceId, {}) == m);
    REQUIRE(hub.ResetMapping(kMouseDeviceId));
    CHECK(hub.Mapping(kMouseDeviceId, {}) == DefaultMouseMapping());

    // Listening: a button held as it starts waits for its release; the next press answers.
    src.raw.buttons[3] = true; // Back
    REQUIRE(hub.BeginListen(kMouseDeviceId));
    CHECK(hub.PollListen().state == GamepadHub::ListenResult::State::Listening);
    src.raw.buttons[3] = false;
    hub.PollListen();
    src.raw.wheel_down += 1;
    const auto heard = hub.PollListen();
    CHECK(heard.state == GamepadHub::ListenResult::State::Pressed);
    CHECK(heard.input == static_cast<int>(MouseInput::WheelDown));

    // The chord switches the mode and saves it.
    int64_t now = 10'000 * kMs;
    hub.PollMouse(now);
    src.raw.buttons[0] = src.raw.buttons[1] = true;
    MouseFrame f = hub.PollMouse(now += 10 * kMs);
    CHECK(f.mode == MouseMode::BottomScreen);
    for (int i = 0; i < 40 && !f.switch_mode; i++) {
        f = hub.PollMouse(now += 100 * kMs);
    }
    CHECK(f.switch_mode);
    CHECK(f.mode == MouseMode::Pointer);
    CHECK(hub.Mapping(kMouseDeviceId, {}).mode == 1);
    CHECK(saved.find(" mode=1") != std::string::npos);

    // In a game, its buttons press the Frame controls the profile gives the 3DS buttons.
    src.raw.buttons = {};
    hub.PollMouse(now += 10 * kMs);
    src.raw.buttons[1] = true; // Right Click: A
    hub.PollMouse(now += 10 * kMs);
    hub.PollMouse(now += 100 * kMs);
    ProfileRoutes routes;
    routes.buttons.fill(-1);
    routes.buttons[0] = static_cast<int8_t>(Control::B); // the 3DS's A on the Frame's B control
    ControlState controls{};
    InputFrame frame;
    hub.Merge(frame, routes, controls, true);
    CHECK(controls.Get(Control::B));
    ControlState none{};
    hub.Merge(frame, routes, none, false); // the menu open: nothing
    CHECK_FALSE(none.Get(Control::B));
}

TEST_CASE("Mouse: Bottom Screen cursor: clamped, a click touches, held it drags", "[frame_xr]") {
    TouchpadTouch t;
    int64_t now = 1'000 * kMs;
    TouchpadFrame f;
    f.bottom_visible = true;
    f.cursor.on = true;
    f.cursor.gain = 1.0f;
    const auto step = [&](float dx, float dy, bool down) {
        now += 10 * kMs;
        f.time_ns = now;
        f.cursor.motion = {dx, dy};
        f.cursor.down = down;
        return t.Update(f);
    };
    TouchpadResult r = step(0, 0, false);
    CHECK(r.event_count == 0);
    r = step(-1000, -1000, false); // far past the top left: clamped
    CHECK(r.cursor.visible);
    CHECK(r.cursor.source == TouchSource::Cursor);
    CHECK(r.cursor.px == Approx(0.0f));
    CHECK(r.cursor.py == Approx(0.0f));
    r = step(100, 50, false);
    CHECK(r.cursor.px == Approx(100.0f));
    CHECK(r.cursor.py == Approx(50.0f));
    r = step(0, 0, true);
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Pressed);
    CHECK(r.events[0].x == Approx(100.0f));
    r = step(10, 5, true);
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Moved);
    CHECK(r.events[0].x == Approx(110.0f));
    CHECK(r.events[0].y == Approx(55.0f));
    r = step(0, 0, false);
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Released);
    // Off: no cursor.
    f.cursor.on = false;
    r = step(10, 10, true);
    CHECK_FALSE(r.cursor.visible);
    CHECK(r.event_count == 0);
}

TEST_CASE("Mouse: Recentre at Edge lifts, waits, touches again at the middle", "[frame_xr]") {
    TouchpadTouch t;
    int64_t now = 1'000 * kMs;
    TouchpadFrame f;
    f.bottom_visible = true;
    f.cursor = {true, {}, 1.0f, false, true};
    const auto step = [&](float dx, bool down, int64_t ms = 10) {
        now += ms * kMs;
        f.time_ns = now;
        f.cursor.motion = {dx, 0.0f};
        f.cursor.down = down;
        return t.Update(f);
    };
    step(0, false);
    TouchpadResult r = step(0, true); // touches at the middle
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].x == Approx(160.0f));
    r = step(150, true); // to the right edge (319.99)
    CHECK(r.events[0].type == TouchEventType::Moved);
    r = step(20, true); // pushed past it: lift
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Released);
    r = step(0, true, 20); // still lifted: the game sees it
    CHECK(r.event_count == 0);
    CHECK(r.cursor.visible);
    r = step(0, true, 40); // touches again at the middle
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Pressed);
    CHECK(r.events[0].x == Approx(160.0f));
    CHECK(r.events[0].y == Approx(120.0f));
    r = step(5, true);
    CHECK(r.events[0].x == Approx(165.0f));

    // Off: the drag stays at the edge.
    f.cursor.recentre = false;
    step(0, false);
    step(0, true);
    step(500, true);
    r = step(50, true);
    CHECK(r.event_count == 0); // clamped: no move, no lift
}

TEST_CASE("Touchpad: Recentre at Edge carries the drag on from the middle", "[frame_xr]") {
    TouchpadTouch t;
    int64_t now = 1'000 * kMs;
    TouchpadFrame f;
    f.mode = TouchpadMode::Touch;
    f.bottom_visible = true;
    f.pad_recentre = true;
    const auto step = [&](float x, bool contact, int64_t ms = 10) {
        now += ms * kMs;
        f.time_ns = now;
        f.pad = {true, contact, false, x, 0.5f};
        return t.Update(f);
    };
    step(0.5f, false); // a finger already down at the start waits for a lift
    TouchpadResult r = step(0.5f, true);
    REQUIRE(r.event_count == 1);
    r = step(0.9f, true);
    r = step(0.99f, true); // the edge: lift
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Released);
    r = step(0.99f, true, 60); // touches again at the middle
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].type == TouchEventType::Pressed);
    CHECK(r.events[0].x == Approx(160.0f).margin(0.5f));
    r = step(0.89f, true); // the finger back a tenth: the touch a tenth left of the middle
    REQUIRE(r.event_count == 1);
    CHECK(r.events[0].x == Approx(128.0f).margin(0.5f));
    r = step(0.5f, false); // a new contact is where the finger is again
    r = step(0.25f, true);
    CHECK(r.events[0].x == Approx(80.0f).margin(0.5f));
}

TEST_CASE("Mouse: Pointer: the ray from where you look, turned by the counts", "[frame_xr]") {
    MousePointer p;
    MouseFrame m;
    m.present = true;
    m.speed = 1; // 0.03 degrees a count
    HandInput real; // no controller
    Posef head;     // at the origin, looking down -Z
    head.position = {0.0f, 1.6f, 0.0f};
    int64_t now = 1'000 * kMs;
    const auto step = [&](float dx, float dy, int64_t ms = 10) {
        now += ms * kMs;
        m.motion = {dx, dy};
        m.used = dx != 0.0f || dy != 0.0f;
        return p.Update(m, true, real, head, true, now);
    };
    // A little jitter does not wake it; a deliberate move does.
    CHECK_FALSE(step(5, 0).active);
    CHECK_FALSE(step(5, 0).active);
    MousePointer::Result r = step(40, 0);
    REQUIRE(r.active);
    CHECK(r.took);
    CHECK(r.aim.position.y == Approx(1.6f));
    // Woken where you look, then turned by this move's 40 counts (1.2 degrees right).
    CHECK(p.yaw_deg() == Approx(-1.2f).margin(0.01f));
    r = step(-1000, -1000); // 30 degrees left, 30 up
    CHECK(p.yaw_deg() == Approx(28.8f).margin(0.01f));
    CHECK(p.pitch_deg() == Approx(30.0f).margin(0.01f));
    const Vec3f d = Rotate(r.aim.orientation, {0.0f, 0.0f, -1.0f});
    CHECK(d.x == Approx(-std::sin(28.8f * 3.14159265f / 180.0f) * std::cos(30.0f * 3.14159265f / 180.0f)).margin(1e-3f));
    CHECK(d.y == Approx(0.5f).margin(1e-3f));
    CHECK(Dot(d, r.dir) == Approx(1.0f).margin(1e-4f));
    r = step(0, -100000); // pitch clamped
    CHECK(p.pitch_deg() == Approx(MousePointer::kPitchClampDeg));

    // The dot: on a hit, then on that plane just off it, else out in the room.
    MousePointer::Dot dot = p.PlaceDot(2.0f, {0.0f, 0.0f, 1.0f}, false, head.position);
    CHECK(dot.on_panel);
    dot = p.PlaceDot(-1.0f, {}, false, head.position);
    CHECK(dot.shown);

    // The right controller picked up: it has its ray back.
    real.active = true;
    real.aim.position = {0.2f, 1.0f, -0.3f};
    step(0, 0, 600);
    REQUIRE(p.active());
    for (int i = 0; i < 20 && p.active(); i++) {
        real.aim.position.x += 0.01f; // 1 m/s
        step(0, 0, 10);
    }
    CHECK_FALSE(p.active());
    // Mouse Mode back to Bottom Screen: no ray.
    m.motion = {200, 0};
    CHECK_FALSE(p.Update(m, false, real, head, true, now += 10 * kMs).active);
}

TEST_CASE("Mouse: the ray stands in for the right controller", "[frame_xr]") {
    InputFrame in;
    in.hand[1].active = true;
    in.hand[1].stick = {0.2f, 0.3f};
    in.hand[1].trigger = 0.0f;
    Posef aim;
    aim.position = {1, 2, 3};
    ApplyMousePointer(in, 1, aim, true, 0.0f);
    CHECK(in.hand[1].mouse);
    CHECK(in.hand[1].trigger == Approx(1.0f));
    CHECK(in.hand[1].aim.position.z == Approx(3.0f));
    CHECK(in.hand[1].stick.x == Approx(0.2f)); // the controller's own stick stays
    ApplyMousePointer(in, 1, aim, false, -1.0f);
    CHECK(in.hand[1].stick.y == Approx(-1.0f)); // the wheel, in the menu
    CHECK_FALSE(in.hand[0].mouse);
}
