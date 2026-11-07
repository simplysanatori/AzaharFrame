// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/chord_latch.h"
#include "frame_xr/input/default_bindings.h"
#include "frame_xr/input/mapper.h"
#include "tests/frame_xr/input_test_util.h"

// AzaharFrame
using Catch::Approx;
using namespace FrameXr::Input;
using namespace FrameXr::Input::TestUtil;

namespace {

// Points in the standard layout.
constexpr Vec3f kOnTopCentre{0.0f, 0.0f, -2.0f};
constexpr Vec3f kOnBottomCentre{0.0f, -0.8f, -1.5f};

int CountTouch(const MapperOutput& out, TouchEventType type) {
    int count = 0;
    for (int i = 0; i < out.touch_event_count; ++i) {
        if (out.touch_events[static_cast<std::size_t>(i)].type == type) {
            ++count;
        }
    }
    return count;
}

/// Runs one frame and advances the clock.
struct Runner {
    Mapper mapper;
    PanelGeometry panels = StandardPanels();
    int64_t now = 0;

    explicit Runner(const MapperConfig& config = {}) : mapper(config) {}

    MapperOutput Step(InputFrame frame) {
        frame.time_ns = now;
        now += kFrameNs;
        return mapper.Update(frame, panels);
    }
};

} // namespace

TEST_CASE("FrameXr mapper: default bindings keep CitraVR's mapping", "[frame_xr]") {
    using Settings::NativeAnalog::CirclePad;
    using Settings::NativeAnalog::CStick;
    using namespace Settings::NativeButton;

    const auto pad_for = [](Control control) {
        ControlState state;
        state.Set(control, true);
        return ApplyDefaultBindings(state);
    };
    const auto only = [](const ThreeDsPadState& pad, Settings::NativeButton::Values button) {
        int count = 0;
        for (const bool pressed : pad.buttons) {
            count += pressed ? 1 : 0;
        }
        return count == 1 && pad.buttons[button];
    };

    REQUIRE(only(pad_for(Control::A), A));
    REQUIRE(only(pad_for(Control::B), B));
    REQUIRE(only(pad_for(Control::X), X));
    REQUIRE(only(pad_for(Control::Y), Y));
    REQUIRE(only(pad_for(Control::BumperL), L));
    REQUIRE(only(pad_for(Control::BumperR), R));
    REQUIRE(only(pad_for(Control::TriggerL), ZL));
    REQUIRE(only(pad_for(Control::TriggerR), ZR));
    REQUIRE(only(pad_for(Control::DpadUp), Up));
    REQUIRE(only(pad_for(Control::DpadDown), Down));
    REQUIRE(only(pad_for(Control::DpadLeft), Left));
    REQUIRE(only(pad_for(Control::DpadRight), Right));
    REQUIRE(only(pad_for(Control::Menu), Start));
    REQUIRE(only(pad_for(Control::View), Select));
    REQUIRE(only(pad_for(Control::StickClickL), Home));

    SECTION("grips and the reset click are unbound") {
        for (const Control control : {Control::GripL, Control::GripR, Control::StickClickR}) {
            const ThreeDsPadState pad = pad_for(control);
            for (const bool pressed : pad.buttons) {
                REQUIRE_FALSE(pressed);
            }
        }
    }
    SECTION("left stick is the circle pad, right stick the C-stick") {
        ControlState state;
        state.SetStick(AnalogControl::StickL, Vec2f{0.25f, -0.5f});
        state.SetStick(AnalogControl::StickR, Vec2f{-1.0f, 0.75f});
        const ThreeDsPadState pad = ApplyDefaultBindings(state);
        REQUIRE(pad.analogs[CirclePad].x == Approx(0.25f));
        REQUIRE(pad.analogs[CirclePad].y == Approx(-0.5f));
        REQUIRE(pad.analogs[CStick].x == Approx(-1.0f));
        REQUIRE(pad.analogs[CStick].y == Approx(0.75f));
    }
    SECTION("no 3DS button is driven by two controls") {
        std::array<int, NumButtons> uses{};
        for (const auto& binding : kDefaultButtonBindings) {
            ++uses[binding.button];
        }
        for (const int count : uses) {
            REQUIRE(count <= 1);
        }
    }
}

TEST_CASE("FrameXr mapper: buttons, grips, sticks", "[frame_xr]") {
    Runner runner;

    SECTION("face buttons go by position (Nintendo) by default, the D-pad passes through") {
        // Frame A (bottom) = 3DS B, Frame B (right) = 3DS A, Frame X (left) = 3DS Y,
        // Frame Y (top) = 3DS X. The pad's A, B, X, Y are bound to the 3DS A, B, X, Y.
        const auto face = [&runner](bool a, bool b, bool x, bool y) {
            InputFrame frame = IdleFrame();
            frame.a = a;
            frame.b = b;
            frame.x = x;
            frame.y = y;
            return runner.Step(frame).controls;
        };
        ControlState pad = face(true, false, false, false);
        REQUIRE(pad.Get(Control::B));
        REQUIRE_FALSE(pad.Get(Control::A));
        pad = face(false, true, false, false);
        REQUIRE(pad.Get(Control::A));
        REQUIRE_FALSE(pad.Get(Control::B));
        pad = face(false, false, true, false);
        REQUIRE(pad.Get(Control::Y));
        REQUIRE_FALSE(pad.Get(Control::X));
        pad = face(false, false, false, true);
        REQUIRE(pad.Get(Control::X));
        REQUIRE_FALSE(pad.Get(Control::Y));

        InputFrame frame = IdleFrame();
        frame.dpad_left = true;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::DpadLeft));
        REQUIRE_FALSE(out.controls.Get(Control::DpadRight));
    }
    SECTION("face buttons go by label (Xbox) when asked") {
        MapperConfig config;
        config.face_buttons_by_position = false;
        Runner by_label{config};
        InputFrame frame = IdleFrame();
        frame.a = true;
        frame.y = true;
        const MapperOutput out = by_label.Step(frame);
        REQUIRE(out.controls.Get(Control::A));
        REQUIRE(out.controls.Get(Control::Y));
        REQUIRE_FALSE(out.controls.Get(Control::B));
        REQUIRE_FALSE(out.controls.Get(Control::X));
    }
    SECTION("the setting can change between frames") {
        InputFrame frame = IdleFrame();
        frame.a = true;
        REQUIRE(runner.Step(frame).controls.Get(Control::B));
        runner.mapper.SetFaceButtonsByPosition(false);
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::A));
        REQUIRE_FALSE(out.controls.Get(Control::B));
    }
    SECTION("View is Select once it has been held 150 ms (a bumper may still follow)") {
        InputFrame frame = IdleFrame();
        frame.view = true;
        MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        runner.now += 150'000'000;
        out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::View));
        REQUIRE_FALSE(out.commands.stop_game);
    }
    SECTION("a quick tap on View is Select for 100 ms, sent when it is let go") {
        InputFrame frame = IdleFrame();
        frame.view = true;
        runner.Step(frame);
        frame.view = false;
        MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::View));
        runner.now += 120'000'000;
        REQUIRE_FALSE(runner.Step(frame).controls.Get(Control::View));
    }
    SECTION("a short Menu press is Menu (Start) when let go, for 100 ms") {
        InputFrame frame = IdleFrame();
        frame.menu = true;
        MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.controls.Get(Control::Menu));
        frame.menu = false;
        out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::Menu));
        REQUIRE_FALSE(out.commands.menu_toggle);
        out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::Menu));
        runner.now += 120'000'000;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.controls.Get(Control::Menu));
    }
    SECTION("Menu held 1 s opens the in-game menu once and never reaches the game") {
        InputFrame frame = IdleFrame();
        frame.menu = true;
        bool toggled = false;
        for (int i = 0; i < 135; ++i) { // 1.5 s at 90 Hz (kFrameNs)
            const MapperOutput out = runner.Step(frame);
            REQUIRE_FALSE(out.controls.Get(Control::Menu));
            if (out.commands.menu_toggle) {
                REQUIRE_FALSE(toggled);
                toggled = true;
            }
        }
        REQUIRE(toggled);
        frame.menu = false;
        const MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.controls.Get(Control::Menu));
        REQUIRE_FALSE(out.commands.menu_toggle);
    }
    SECTION("grips have hysteresis") {
        InputFrame frame = IdleFrame();
        frame.hand[0].grip = 0.5f;
        REQUIRE_FALSE(runner.Step(frame).controls.Get(Control::GripL));
        frame.hand[0].grip = 0.7f;
        REQUIRE(runner.Step(frame).controls.Get(Control::GripL));
        frame.hand[0].grip = 0.5f;
        REQUIRE(runner.Step(frame).controls.Get(Control::GripL));
        frame.hand[0].grip = 0.3f;
        REQUIRE_FALSE(runner.Step(frame).controls.Get(Control::GripL));
    }
    SECTION("stick deadzone is radial and rescaled") {
        InputFrame frame = IdleFrame();
        frame.hand[0].stick = Vec2f{0.05f, 0.05f};
        frame.hand[1].stick = Vec2f{0.56f, 0.0f};
        MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.GetStick(AnalogControl::StickL).x == Approx(0.0f));
        REQUIRE(out.controls.GetStick(AnalogControl::StickR).x == Approx(0.5f));
        frame.hand[1].stick = Vec2f{1.0f, 0.0f};
        out = runner.Step(frame);
        REQUIRE(out.controls.GetStick(AnalogControl::StickR).x == Approx(1.0f));
    }
    SECTION("bumpers and stick clicks reach the pad") {
        InputFrame frame = IdleFrame();
        frame.hand[0].bumper = true;
        frame.hand[0].stick_click = true;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::BumperL));
        REQUIRE(out.controls.Get(Control::StickClickL));
        REQUIRE_FALSE(out.commands.reset_panels);
    }
}

TEST_CASE("FrameXr mapper: triggers away from the screens are ZL and ZR", "[frame_xr]") {
    Runner runner;
    InputFrame frame = IdleFrame();
    frame.hand[0].trigger = 1.0f;
    MapperOutput out = runner.Step(frame);
    REQUIRE(out.controls.Get(Control::TriggerL));
    REQUIRE_FALSE(out.controls.Get(Control::TriggerR));
    REQUIRE(out.touch_event_count == 0);
    REQUIRE_FALSE(out.cursor.visible);
    REQUIRE_FALSE(out.commands.top_drag_active);

    SECTION("an inactive controller's trigger is ignored") {
        Runner other;
        InputFrame inactive = IdleFrame();
        inactive.hand[1].active = false;
        inactive.hand[1].trigger = 1.0f;
        REQUIRE_FALSE(other.Step(inactive).controls.Get(Control::TriggerR));
    }
    SECTION("sweeping onto a screen with the trigger already held stays ZL") {
        Point(frame, Hand::Left, kOnBottomCentre);
        out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::TriggerL));
        REQUIRE(out.touch_event_count == 0);
        REQUIRE_FALSE(out.commands.top_drag_active);
    }
    SECTION("release lets go") {
        frame.hand[0].trigger = 0.0f;
        REQUIRE_FALSE(runner.Step(frame).controls.Get(Control::TriggerL));
    }
}

TEST_CASE("FrameXr mapper: a trigger over the top screen is ZL and ZR", "[frame_xr]") {
    Runner runner;
    InputFrame frame = IdleFrame();
    Point(frame, Hand::Right, Vec3f{0.5f, 0.2f, -2.0f});
    frame.hand[1].trigger = 1.0f;
    MapperOutput out = runner.Step(frame);
    REQUIRE(out.controls.Get(Control::TriggerR));
    REQUIRE_FALSE(out.commands.top_drag_active);
    REQUIRE(out.touch_event_count == 0);
    // No dot over the top screen: nothing to aim at there.
    REQUIRE_FALSE(out.cursor.visible);

    SECTION("moving the ray around keeps ZR until release") {
        Point(frame, Hand::Right, Vec3f{-0.5f, -0.1f, -2.0f});
        REQUIRE(runner.Step(frame).controls.Get(Control::TriggerR));
        frame.hand[1].trigger = 0.0f;
        REQUIRE_FALSE(runner.Step(frame).controls.Get(Control::TriggerR));
    }
    SECTION("the left trigger is ZL there too") {
        Point(frame, Hand::Left, kOnTopCentre);
        frame.hand[0].trigger = 1.0f;
        out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::TriggerL));
        REQUIRE_FALSE(out.commands.top_drag_active);
    }
}

TEST_CASE("FrameXr mapper: touching the bottom screen", "[frame_xr]") {
    Runner runner;
    InputFrame frame = IdleFrame();
    Point(frame, Hand::Right, kOnBottomCentre);
    runner.Step(frame); // hovering

    SECTION("hovering shows the cursor with the 3DS pixel under it") {
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.cursor.visible);
        REQUIRE(out.cursor.panel == Panel::Bottom);
        REQUIRE(out.cursor.hand == Hand::Right);
        REQUIRE(out.cursor.px == Approx(160.0f));
        REQUIRE(out.cursor.py == Approx(120.0f));
        REQUIRE_FALSE(out.cursor.pressed);
        REQUIRE(out.cursor.scale_m == Approx(0.01f + 0.003f * 1.3f).epsilon(0.05));
    }
    SECTION("pull, drag, release") {
        frame.hand[1].trigger = 1.0f;
        MapperOutput out = runner.Step(frame);
        REQUIRE(CountTouch(out, TouchEventType::Pressed) == 1);
        REQUIRE(out.touch_events[0].x == Approx(160.0f).margin(0.5));
        REQUIRE(out.touch_events[0].y == Approx(120.0f).margin(0.5));
        REQUIRE_FALSE(out.controls.Get(Control::TriggerR));
        REQUIRE(out.cursor.pressed);

        // Move the pointer 0.15 m right: a quarter of the 0.6 m screen, 80 pixels.
        Point(frame, Hand::Right, Vec3f{0.15f, -0.8f, -1.5f});
        out = runner.Step(frame);
        REQUIRE(CountTouch(out, TouchEventType::Moved) == 1);
        REQUIRE(out.touch_events[0].x == Approx(240.0f).margin(0.5));
        REQUIRE(out.touch_events[0].y == Approx(120.0f).margin(0.5));

        // Staying still sends nothing.
        out = runner.Step(frame);
        REQUIRE(out.touch_event_count == 0);

        frame.hand[1].trigger = 0.0f;
        out = runner.Step(frame);
        REQUIRE(CountTouch(out, TouchEventType::Released) == 1);
        REQUIRE_FALSE(out.controls.Get(Control::TriggerR));
    }
    SECTION("sliding off the edge clamps and keeps the touch down") {
        frame.hand[1].trigger = 1.0f;
        runner.Step(frame);
        Point(frame, Hand::Right, Vec3f{0.9f, -0.8f, -1.5f});
        MapperOutput out = runner.Step(frame);
        REQUIRE(CountTouch(out, TouchEventType::Moved) == 1);
        REQUIRE(out.touch_events[0].x > 319.0f);
        REQUIRE(out.touch_events[0].x < 320.0f);
        REQUIRE(CountTouch(out, TouchEventType::Released) == 0);
        REQUIRE(out.cursor.visible);
        REQUIRE(out.cursor.pressed);
    }
    SECTION("the other hand cannot start a second touch, and is not ZL either") {
        frame.hand[1].trigger = 1.0f;
        runner.Step(frame);
        Point(frame, Hand::Left, kOnBottomCentre);
        frame.hand[0].trigger = 1.0f;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(CountTouch(out, TouchEventType::Pressed) == 0);
        REQUIRE_FALSE(out.controls.Get(Control::TriggerL));
    }
    SECTION("Reset reports a touch in progress so the caller can lift it") {
        frame.hand[1].trigger = 1.0f;
        runner.Step(frame);
        REQUIRE(runner.mapper.Reset());
        REQUIRE_FALSE(runner.mapper.Reset());
    }
}

TEST_CASE("FrameXr mapper: a grip drags the screen under the ray", "[frame_xr]") {
    MapperConfig config;
    config.horizontal_lock = false;
    Runner runner(config);
    InputFrame frame = IdleFrame();
    Point(frame, Hand::Right, Vec3f{0.5f, 0.2f, -2.0f});
    frame.hand[1].grip = 1.0f;

    SECTION("grabbing off-centre does not make the screen jump") {
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.commands.top_drag_active);
        REQUIRE(out.commands.top_drag_target.x == Approx(0.0f).margin(1e-3));
        REQUIRE(out.commands.top_drag_target.y == Approx(0.0f).margin(1e-3));
        REQUIRE(out.commands.top_drag_target.z == Approx(-2.0f).margin(1e-3));
        REQUIRE_FALSE(out.controls.Get(Control::TriggerR));
        REQUIRE(out.touch_event_count == 0);
        REQUIRE(out.cursor.panel == Panel::Top);
        REQUIRE(out.cursor.pressed);
    }
    SECTION("moving the pointer moves the target by the same amount") {
        runner.Step(frame);
        Point(frame, Hand::Right, Vec3f{0.9f, 0.6f, -2.0f});
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.commands.top_drag_target.x == Approx(0.4f).margin(1e-3));
        REQUIRE(out.commands.top_drag_target.y == Approx(0.4f).margin(1e-3));
    }
    SECTION("the horizontal lock zeroes x") {
        MapperConfig locked;
        locked.horizontal_lock = true;
        Runner locked_runner(locked);
        locked_runner.Step(frame);
        Point(frame, Hand::Right, Vec3f{0.9f, 0.6f, -2.0f});
        const MapperOutput out = locked_runner.Step(frame);
        REQUIRE(out.commands.top_drag_target.x == Approx(0.0f));
        REQUIRE(out.commands.top_drag_target.y == Approx(0.4f).margin(1e-3));
    }
    SECTION("stick Y beyond the threshold moves the screen in depth; the stick stays off the game") {
        runner.Step(frame);
        frame.hand[1].stick = Vec2f{0.0f, 1.0f};
        MapperOutput out = runner.Step(frame);
        // Full deflection up = away (negative z) at 3 m/s for one 90 Hz frame.
        REQUIRE(out.commands.top_depth_delta_z == Approx(-3.0f * 0.0111111f).epsilon(0.01));
        REQUIRE(out.controls.GetStick(AnalogControl::StickR).y == Approx(0.0f));

        frame.hand[1].stick = Vec2f{0.0f, -1.0f};
        out = runner.Step(frame);
        REQUIRE(out.commands.top_depth_delta_z > 0.0f);

        frame.hand[1].stick = Vec2f{0.0f, 0.3f};
        out = runner.Step(frame);
        REQUIRE(out.commands.top_depth_delta_z == Approx(0.0f));
    }
    SECTION("the other stick keeps working during a drag") {
        runner.Step(frame);
        frame.hand[0].stick = Vec2f{0.0f, 1.0f};
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.GetStick(AnalogControl::StickL).y == Approx(1.0f));
        REQUIRE(out.commands.top_depth_delta_z == Approx(0.0f));
    }
    SECTION("the stick reaches the game again once the grip lets go") {
        runner.Step(frame);
        frame.hand[1].grip = 0.0f;
        frame.hand[1].stick = Vec2f{0.0f, 1.0f};
        const MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.top_drag_active);
        REQUIRE(out.controls.GetStick(AnalogControl::StickR).y == Approx(1.0f));
    }
    SECTION("the other hand's trigger is still ZL during the drag") {
        runner.Step(frame);
        Point(frame, Hand::Left, kOnTopCentre);
        frame.hand[0].trigger = 1.0f;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.commands.top_drag_active);
        REQUIRE(out.controls.Get(Control::TriggerL));
    }
    SECTION("a second grip does not start a second drag") {
        runner.Step(frame);
        Point(frame, Hand::Left, Vec3f{-0.5f, 0.0f, -2.0f});
        frame.hand[0].grip = 1.0f;
        runner.Step(frame);
        // Letting go of the first hand ends it; the second hand's grip was never a drag.
        frame.hand[1].grip = 0.0f;
        const MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.top_drag_active);
    }
    SECTION("a grip already held when the ray sweeps onto the screen grabs nothing") {
        Runner other(config);
        InputFrame sweep = IdleFrame();
        sweep.hand[1].grip = 1.0f;
        REQUIRE_FALSE(other.Step(sweep).commands.top_drag_active);
        Point(sweep, Hand::Right, kOnTopCentre);
        REQUIRE_FALSE(other.Step(sweep).commands.top_drag_active);
    }
    SECTION("a grip away from both screens grabs nothing") {
        Runner other(config);
        InputFrame idle = IdleFrame();
        idle.hand[1].grip = 1.0f;
        const MapperOutput out = other.Step(idle);
        REQUIRE_FALSE(out.commands.top_drag_active);
        REQUIRE_FALSE(out.controls.Get(Control::TriggerR));
    }
}

TEST_CASE("FrameXr mapper: a grip on the bottom screen moves both screens", "[frame_xr]") {
    MapperConfig config;
    config.horizontal_lock = false;
    Runner runner(config);
    InputFrame frame = IdleFrame();
    Point(frame, Hand::Left, Vec3f{0.1f, -0.7f, -1.5f}); // 0.1 m right of the bottom centre
    frame.hand[0].grip = 1.0f;

    MapperOutput out = runner.Step(frame);
    REQUIRE(out.commands.top_drag_active);
    // No jump: the top screen stays where it is.
    REQUIRE(out.commands.top_drag_target.x == Approx(0.0f).margin(1e-3));
    REQUIRE(out.commands.top_drag_target.y == Approx(0.0f).margin(1e-3));
    REQUIRE(out.commands.top_drag_target.z == Approx(-2.0f).margin(1e-3));
    REQUIRE(out.cursor.panel == Panel::Bottom);
    REQUIRE(out.touch_event_count == 0);

    // The grabbed point goes 0.2 m right and 0.1 m up: the top screen goes the same way.
    Point(frame, Hand::Left, Vec3f{0.3f, -0.6f, -1.5f});
    out = runner.Step(frame);
    REQUIRE(out.commands.top_drag_target.x == Approx(0.2f).margin(1e-3));
    REQUIRE(out.commands.top_drag_target.y == Approx(0.1f).margin(1e-3));
    REQUIRE(out.commands.top_drag_target.z == Approx(-2.0f).margin(1e-3));
}

TEST_CASE("FrameXr mapper: the nearer screen wins", "[frame_xr]") {
    // Put the bottom screen in front of the top one along the same line of sight.
    Runner runner;
    runner.panels.bottom.pose.position = Vec3f{0.1f, -0.2f, -1.0f};
    InputFrame frame = IdleFrame();
    Point(frame, Hand::Right, Vec3f{0.0f, 0.0f, -2.0f});
    frame.hand[1].trigger = 1.0f;
    const MapperOutput out = runner.Step(frame);
    REQUIRE(CountTouch(out, TouchEventType::Pressed) == 1);
    REQUIRE_FALSE(out.commands.top_drag_active);
    REQUIRE_FALSE(out.controls.Get(Control::TriggerR));
}

TEST_CASE("FrameXr mapper: the top screen in front of the bottom one is ZR, not touch",
          "[frame_xr]") {
    Runner runner;
    runner.panels.bottom.pose.position = Vec3f{0.0f, 0.0f, -3.0f};
    runner.panels.bottom.width_m = 4.0f;
    runner.panels.bottom.height_m = 3.0f;
    InputFrame frame = IdleFrame();
    Point(frame, Hand::Right, Vec3f{0.0f, 0.0f, -2.0f});
    frame.hand[1].trigger = 1.0f;
    const MapperOutput out = runner.Step(frame);
    REQUIRE(out.touch_event_count == 0);
    REQUIRE(out.controls.Get(Control::TriggerR));
}

TEST_CASE("FrameXr mapper: a hidden bottom screen is neither touched nor grabbed", "[frame_xr]") {
    Runner runner;
    runner.panels.bottom.visible = false;
    InputFrame frame = IdleFrame();
    Point(frame, Hand::Right, kOnBottomCentre);

    SECTION("the trigger is ZR") {
        frame.hand[1].trigger = 1.0f;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.touch_event_count == 0);
        REQUIRE(out.controls.Get(Control::TriggerR));
        REQUIRE_FALSE(out.cursor.visible);
    }
    SECTION("the grip grabs nothing") {
        runner.Step(frame);
        frame.hand[1].grip = 1.0f;
        REQUIRE_FALSE(runner.Step(frame).commands.top_drag_active);
    }
    SECTION("the top screen can still be grabbed") {
        Point(frame, Hand::Right, kOnTopCentre);
        frame.hand[1].grip = 1.0f;
        REQUIRE(runner.Step(frame).commands.top_drag_active);
    }
}

TEST_CASE("FrameXr mapper: both bumpers and View toggle the bottom screen", "[frame_xr]") {
    Runner runner;
    runner.panels.bottom_toggle_enabled = true;
    InputFrame frame = IdleFrame();
    frame.hand[0].bumper = true;
    frame.hand[1].bumper = true;
    MapperOutput out = runner.Step(frame);
    REQUIRE_FALSE(out.commands.toggle_bottom);
    REQUIRE(out.controls.Get(Control::BumperL));
    REQUIRE(out.controls.Get(Control::BumperR));

    SECTION("View pressed with both bumpers held toggles once and never reaches the game") {
        frame.view = true;
        out = runner.Step(frame);
        // It waits for Menu (the VR / flat chord) first.
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        runner.now += 150'000'000;
        out = runner.Step(frame);
        REQUIRE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        // The bumpers let go first: the same press still stays away from the game.
        frame.hand[0].bumper = false;
        frame.hand[1].bumper = false;
        REQUIRE_FALSE(runner.Step(frame).controls.Get(Control::View));
        frame.view = false;
        runner.Step(frame);
        frame.view = true;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.controls.Get(Control::View)); // held back while a bumper may follow
        runner.now += 150'000'000;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE(out.controls.Get(Control::View));
    }
    SECTION("the chord does not start the stop hold") {
        frame.view = true;
        runner.Step(frame);
        runner.now += 2'000'000'000;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.stop_game);
        REQUIRE(out.commands.stop_hold_progress == Approx(0.0f));
    }
    SECTION("one bumper is not the chord") {
        frame.hand[1].bumper = false;
        frame.view = true;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE(out.controls.Get(Control::View));
    }
    SECTION("View held first, bumpers pressed after, is Select and no toggle") {
        Runner other;
        other.panels.bottom_toggle_enabled = true;
        InputFrame view_first = IdleFrame();
        view_first.view = true;
        other.Step(view_first);
        view_first.hand[0].bumper = true;
        view_first.hand[1].bumper = true;
        out = other.Step(view_first);
        // Both at once is no quick action: View goes on as Select, the bumpers as L and R.
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.commands.quick_save);
        REQUIRE_FALSE(out.commands.quick_load);
        REQUIRE(out.controls.Get(Control::View));
        REQUIRE(out.controls.Get(Control::BumperL));
    }
    SECTION("a tap on View toggles once, when let go") {
        frame.view = true;
        runner.Step(frame);
        frame.view = false;
        out = runner.Step(frame);
        REQUIRE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.controls.Get(Control::View));
    }
    SECTION("without the toggle enabled the game sees L, R and Select, Select a little later") {
        runner.panels.bottom_toggle_enabled = false;
        frame.view = true;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        REQUIRE(out.controls.Get(Control::BumperL));
        runner.now += 150'000'000;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE(out.controls.Get(Control::View));
        REQUIRE(out.controls.Get(Control::BumperL));
    }
    SECTION("without the toggle enabled a quick tap still reaches the game as Select") {
        runner.panels.bottom_toggle_enabled = false;
        frame.view = true;
        runner.Step(frame);
        frame.view = false;
        out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::View));
        runner.now += 120'000'000;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.controls.Get(Control::View));
    }
}

TEST_CASE("FrameXr mapper: both bumpers, View and Menu switch between VR and flat", "[frame_xr]") {
    Runner runner;
    runner.panels.bottom_toggle_enabled = true;
    InputFrame frame = IdleFrame();
    frame.hand[0].bumper = true;
    frame.hand[1].bumper = true;
    runner.Step(frame);

    const auto release_all = [&] {
        InputFrame idle = IdleFrame();
        MapperOutput last;
        for (int i = 0; i < 12; ++i) { // over 100 ms: no Start or Select pulse may follow
            last = runner.Step(idle);
            REQUIRE_FALSE(last.controls.Get(Control::Menu));
            REQUIRE_FALSE(last.controls.Get(Control::View));
            REQUIRE_FALSE(last.commands.toggle_bottom);
            REQUIRE_FALSE(last.commands.menu_toggle);
        }
    };

    SECTION("Menu first, then View: switches once; the game sees neither, nor L and R after") {
        frame.menu = true;
        MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_vr);
        frame.view = true;
        out = runner.Step(frame);
        REQUIRE(out.commands.toggle_vr);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        REQUIRE_FALSE(out.controls.Get(Control::Menu));
        REQUIRE_FALSE(out.controls.Get(Control::BumperL));
        REQUIRE_FALSE(out.controls.Get(Control::BumperR));
        for (int i = 0; i < 180; ++i) { // held on for 2 s: no menu, no stop, no second switch
            out = runner.Step(frame);
            REQUIRE_FALSE(out.commands.toggle_vr);
            REQUIRE_FALSE(out.commands.menu_toggle);
            REQUIRE_FALSE(out.commands.stop_game);
            REQUIRE_FALSE(out.controls.Get(Control::View));
            REQUIRE_FALSE(out.controls.Get(Control::Menu));
        }
        release_all();
    }
    SECTION("View first, Menu within the wait: switches, no bottom toggle, no Select") {
        frame.view = true;
        MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_vr);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        frame.menu = true;
        out = runner.Step(frame);
        REQUIRE(out.commands.toggle_vr);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        release_all();
    }
    SECTION("Menu after the wait is not the switch") {
        frame.view = true;
        runner.Step(frame);
        runner.now += 150'000'000;
        MapperOutput out = runner.Step(frame);
        REQUIRE(out.commands.toggle_bottom);
        frame.menu = true;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_vr);
    }
    SECTION("the bumpers reach the game again once let go and pressed again") {
        frame.menu = true;
        frame.view = true;
        REQUIRE(runner.Step(frame).commands.toggle_vr);
        release_all();
        InputFrame again = IdleFrame();
        again.hand[0].bumper = true;
        const MapperOutput out = runner.Step(again);
        REQUIRE(out.controls.Get(Control::BumperL));
    }
}

TEST_CASE("FrameXr mapper: View held stops the game", "[frame_xr]") {
    Runner runner;
    InputFrame frame = IdleFrame();
    frame.view = true;

    MapperOutput out = runner.Step(frame);
    REQUIRE_FALSE(out.controls.Get(Control::View)); // held back while a bumper may follow
    runner.now += 150'000'000;
    out = runner.Step(frame);
    REQUIRE(out.controls.Get(Control::View));
    REQUIRE(out.commands.stop_hold_progress == Approx(0.0f).margin(0.01));

    runner.now += 750'000'000;
    out = runner.Step(frame);
    REQUIRE(out.commands.stop_hold_progress == Approx(0.5f).margin(0.02));
    REQUIRE_FALSE(out.commands.stop_game);

    runner.now += 800'000'000;
    out = runner.Step(frame);
    REQUIRE(out.commands.stop_game);
    REQUIRE_FALSE(out.controls.Get(Control::View));

    // Fires once per hold.
    out = runner.Step(frame);
    REQUIRE_FALSE(out.commands.stop_game);
    REQUIRE_FALSE(out.controls.Get(Control::View));

    // A new hold works again.
    frame.view = false;
    runner.Step(frame);
    frame.view = true;
    out = runner.Step(frame);
    REQUIRE_FALSE(out.controls.Get(Control::View));
    runner.now += 150'000'000;
    out = runner.Step(frame);
    REQUIRE(out.controls.Get(Control::View));
    runner.now += 1'600'000'000;
    REQUIRE(runner.Step(frame).commands.stop_game);
}

TEST_CASE("FrameXr mapper: View, then a bumper, is Quick Save or Quick Load", "[frame_xr]") {
    Runner runner;
    runner.panels.bottom_toggle_enabled = true;
    InputFrame frame = IdleFrame();
    const auto idle_steps = [&](int n) {
        InputFrame idle = IdleFrame();
        for (int i = 0; i < n; ++i) {
            const MapperOutput out = runner.Step(idle);
            REQUIRE_FALSE(out.controls.Get(Control::View));
            REQUIRE_FALSE(out.controls.Get(Control::BumperL));
            REQUIRE_FALSE(out.controls.Get(Control::BumperR));
        }
    };

    SECTION("the Right Bumper after View is Quick Save, once, and the game sees nothing") {
        frame.view = true;
        MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        frame.hand[1].bumper = true;
        out = runner.Step(frame);
        REQUIRE(out.commands.quick_save);
        REQUIRE_FALSE(out.commands.quick_load);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        REQUIRE_FALSE(out.controls.Get(Control::BumperR));
        for (int i = 0; i < 40; ++i) { // held on: no second action, no Select, no bumper
            out = runner.Step(frame);
            REQUIRE_FALSE(out.commands.quick_save);
            REQUIRE_FALSE(out.controls.Get(Control::View));
            REQUIRE_FALSE(out.controls.Get(Control::BumperR));
        }
        frame.hand[1].bumper = false;
        frame.view = false;
        runner.Step(frame);
        idle_steps(12); // no Select pulse follows
    }
    SECTION("the Left Bumper after View is Quick Load") {
        frame.view = true;
        runner.Step(frame);
        frame.hand[0].bumper = true;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.commands.quick_load);
        REQUIRE_FALSE(out.commands.quick_save);
        REQUIRE_FALSE(out.controls.Get(Control::BumperL));
        REQUIRE_FALSE(out.controls.Get(Control::View));
    }
    SECTION("it works after the wait too, and cancels the stop hold") {
        frame.view = true;
        runner.Step(frame);
        runner.now += 1'000'000'000;
        MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::View)); // Select was already on
        runner.now += 600'000'000;
        out = runner.Step(frame);
        REQUIRE(out.commands.stop_hold_progress > 0.0f);
        frame.hand[0].bumper = true;
        out = runner.Step(frame);
        REQUIRE(out.commands.quick_load);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        REQUIRE(out.commands.stop_hold_progress == Approx(0.0f));
        runner.now += 2'000'000'000;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.stop_game);
    }
    SECTION("the other bumper while the first is held is not the bottom screen chord") {
        frame.view = true;
        runner.Step(frame);
        frame.hand[0].bumper = true;
        REQUIRE(runner.Step(frame).commands.quick_load);
        runner.now += 1'200'000'000; // past the debounce
        frame.hand[1].bumper = true;
        MapperOutput out = runner.Step(frame);
        REQUIRE(out.commands.quick_save);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.commands.toggle_vr);
        REQUIRE_FALSE(out.controls.Get(Control::BumperL));
        REQUIRE_FALSE(out.controls.Get(Control::BumperR));
        REQUIRE_FALSE(out.controls.Get(Control::View));
        runner.now += 200'000'000;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_bottom);
        REQUIRE_FALSE(out.controls.Get(Control::View));
    }
    SECTION("Quick Load is debounced") {
        frame.view = true;
        runner.Step(frame);
        frame.hand[0].bumper = true;
        REQUIRE(runner.Step(frame).commands.quick_load);
        frame.hand[0].bumper = false; // a bounce
        runner.Step(frame);
        frame.hand[0].bumper = true;
        MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.quick_load);
        REQUIRE_FALSE(out.controls.Get(Control::BumperL)); // still swallowed
        frame.hand[0].bumper = false;
        runner.Step(frame);
        runner.now += 1'000'000'000;
        frame.hand[0].bumper = true;
        REQUIRE(runner.Step(frame).commands.quick_load);
    }
    SECTION("a bumper held first is not the combo") {
        frame.hand[0].bumper = true;
        runner.Step(frame);
        frame.hand[1].bumper = true;
        runner.Step(frame);
        frame.view = true;
        runner.Step(frame);
        runner.now += 150'000'000;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.commands.toggle_bottom); // the bottom screen chord, as before
        REQUIRE_FALSE(out.commands.quick_save);
        REQUIRE_FALSE(out.commands.quick_load);
    }
    SECTION("one bumper held, then View, then the other bumper is not the combo") {
        frame.hand[0].bumper = true;
        runner.Step(frame);
        frame.view = true;
        runner.Step(frame);
        frame.hand[1].bumper = true;
        const MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.quick_save);
        REQUIRE_FALSE(out.commands.quick_load);
    }
    SECTION("a bumper without View is just L or R") {
        frame.hand[1].bumper = true;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::BumperR));
        REQUIRE_FALSE(out.commands.quick_save);
    }
    SECTION("View then both bumpers at once is neither") {
        frame.view = true;
        runner.Step(frame);
        frame.hand[0].bumper = true;
        frame.hand[1].bumper = true;
        const MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.quick_save);
        REQUIRE_FALSE(out.commands.quick_load);
        REQUIRE_FALSE(out.commands.toggle_bottom);
    }
    SECTION("a View hold alone still stops the game") {
        frame.view = true;
        runner.Step(frame);
        runner.now += 200'000'000;
        runner.Step(frame);
        runner.now += 1'600'000'000;
        REQUIRE(runner.Step(frame).commands.stop_game);
    }
}

TEST_CASE("FrameXr mapper: right stick click resets the screens once", "[frame_xr]") {
    Runner runner;
    InputFrame frame = IdleFrame();
    frame.hand[1].stick_click = true;
    REQUIRE(runner.Step(frame).commands.reset_panels);
    REQUIRE_FALSE(runner.Step(frame).commands.reset_panels);
    frame.hand[1].stick_click = false;
    runner.Step(frame);
    frame.hand[1].stick_click = true;
    REQUIRE(runner.Step(frame).commands.reset_panels);
}

TEST_CASE("FrameXr mapper: a right stick click with the stick pushed over is ignored",
          "[frame_xr]") {
    Runner runner;
    InputFrame frame = IdleFrame();
    // Free camera: the stick is pushed over while it clicks. No reset, even after it recentres.
    frame.hand[1].stick = {0.0f, 0.8f};
    frame.hand[1].stick_click = true;
    REQUIRE_FALSE(runner.Step(frame).commands.reset_panels);
    frame.hand[1].stick = {0.0f, 0.0f};
    REQUIRE_FALSE(runner.Step(frame).commands.reset_panels);
    frame.hand[1].stick_click = false;
    runner.Step(frame);
    // A diagonal push also counts, by its length.
    frame.hand[1].stick = {0.5f, 0.5f};
    frame.hand[1].stick_click = true;
    REQUIRE_FALSE(runner.Step(frame).commands.reset_panels);
    frame.hand[1].stick_click = false;
    runner.Step(frame);
    // Nearly centred still resets.
    frame.hand[1].stick = {0.2f, 0.1f};
    frame.hand[1].stick_click = true;
    REQUIRE(runner.Step(frame).commands.reset_panels);
}

TEST_CASE("FrameXr mapper: the hand that pulled last points", "[frame_xr]") {
    Runner runner;
    REQUIRE(runner.mapper.preferred_hand() == Hand::Right);
    InputFrame frame = IdleFrame();
    frame.hand[0].trigger = 1.0f;
    runner.Step(frame);
    REQUIRE(runner.mapper.preferred_hand() == Hand::Left);

    SECTION("with both hands on the bottom screen the cursor follows the pointing hand") {
        frame.hand[0].trigger = 0.0f;
        runner.Step(frame);
        Point(frame, Hand::Left, Vec3f{-0.1f, -0.8f, -1.5f});
        Point(frame, Hand::Right, Vec3f{0.1f, -0.8f, -1.5f});
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.cursor.visible);
        REQUIRE(out.cursor.hand == Hand::Left);
    }
}

TEST_CASE("FrameXr mapper: View, then the Right Trigger, toggles fast forward", "[frame_xr]") {
    Runner runner;
    InputFrame frame = IdleFrame();
    const auto pull = [&](float value) { frame.hand[1].trigger = value; };

    SECTION("on, once per pull, and the game sees neither Select nor ZR") {
        frame.view = true;
        MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.controls.Get(Control::View));
        pull(1.0f);
        out = runner.Step(frame);
        REQUIRE(out.commands.toggle_fast_forward);
        REQUIRE_FALSE(out.controls.Get(Control::TriggerR));
        REQUIRE_FALSE(out.controls.Get(Control::View));
        for (int i = 0; i < 200; ++i) { // held on for 2 s: nothing more, and no stop
            out = runner.Step(frame);
            REQUIRE_FALSE(out.commands.toggle_fast_forward);
            REQUIRE_FALSE(out.commands.stop_game);
            REQUIRE(out.commands.stop_hold_progress == Approx(0.0f));
            REQUIRE_FALSE(out.controls.Get(Control::TriggerR));
            REQUIRE_FALSE(out.controls.Get(Control::View));
        }
        pull(0.0f);
        out = runner.Step(frame);
        REQUIRE_FALSE(out.controls.Get(Control::View)); // View still held: still swallowed
        frame.view = false;
        runner.Step(frame);
        for (int i = 0; i < 12; ++i) { // no late Select pulse
            out = runner.Step(frame);
            REQUIRE_FALSE(out.controls.Get(Control::View));
        }
    }
    SECTION("the same combo again is the second toggle (off)") {
        frame.view = true;
        runner.Step(frame);
        pull(1.0f);
        REQUIRE(runner.Step(frame).commands.toggle_fast_forward);
        pull(0.0f);
        frame.view = false;
        runner.Step(frame);
        runner.now += 1'000'000'000;
        frame.view = true;
        runner.Step(frame);
        pull(1.0f);
        REQUIRE(runner.Step(frame).commands.toggle_fast_forward);
    }
    SECTION("a bounce within 1 s does not toggle back") {
        frame.view = true;
        runner.Step(frame);
        pull(1.0f);
        REQUIRE(runner.Step(frame).commands.toggle_fast_forward);
        pull(0.0f);
        runner.Step(frame);
        pull(1.0f);
        MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_fast_forward);
        REQUIRE_FALSE(out.controls.Get(Control::TriggerR)); // still swallowed
        pull(0.0f);
        runner.Step(frame);
        runner.now += 1'000'000'000;
        pull(1.0f);
        REQUIRE(runner.Step(frame).commands.toggle_fast_forward); // View still held: allowed
    }
    SECTION("the Left Trigger with View is plain ZL") {
        frame.view = true;
        runner.Step(frame);
        frame.hand[0].trigger = 1.0f;
        const MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_fast_forward);
        REQUIRE(out.controls.Get(Control::TriggerL));
    }
    SECTION("the trigger first, then View, is ZR and not the combo") {
        pull(1.0f);
        MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::TriggerR));
        frame.view = true;
        out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_fast_forward);
        REQUIRE(out.controls.Get(Control::TriggerR));
    }
    SECTION("a bumper held first is not the combo") {
        frame.hand[0].bumper = true;
        runner.Step(frame);
        frame.view = true;
        runner.Step(frame);
        pull(1.0f);
        const MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_fast_forward);
        REQUIRE(out.controls.Get(Control::TriggerR));
    }
    SECTION("a trigger pulled on the bottom screen without View is still a touch") {
        Point(frame, Hand::Right, kOnBottomCentre);
        pull(1.0f);
        const MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.toggle_fast_forward);
        REQUIRE(CountTouch(out, TouchEventType::Pressed) == 1);
    }
    SECTION("Quick Save still works after fast forward, with View held") {
        frame.view = true;
        runner.Step(frame);
        pull(1.0f);
        REQUIRE(runner.Step(frame).commands.toggle_fast_forward);
        pull(0.0f);
        runner.Step(frame);
        frame.hand[1].bumper = true;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.commands.quick_save);
        REQUIRE_FALSE(out.commands.toggle_fast_forward);
    }
}

TEST_CASE("FrameXr chord latch: the VR / flat chord fires once per press", "[frame_xr]") {
    constexpr int64_t kFlatFrameNs = 8'000'000; // the flat loop's period
    const auto chord = [](InputFrame& frame, bool down) {
        frame.hand[0].bumper = down;
        frame.hand[1].bumper = down;
        frame.view = down;
        frame.menu = down;
    };

    SECTION("Filter does nothing until armed") {
        ChordLatch latch;
        InputFrame frame = IdleFrame();
        chord(frame, true);
        REQUIRE_FALSE(latch.Filter(frame));
        REQUIRE(frame.view);
        REQUIRE(frame.menu);
        REQUIRE(frame.hand[0].bumper);
    }
    SECTION("held through the switch: masked until let go, after the cooldown") {
        ChordLatch latch;
        latch.Arm();
        InputFrame frame = IdleFrame(5'000'000'000); // the new mode's clock
        chord(frame, true);
        frame.a = true;
        REQUIRE(latch.Filter(frame));
        REQUIRE_FALSE(frame.view);
        REQUIRE_FALSE(frame.menu);
        REQUIRE_FALSE(frame.hand[0].bumper);
        REQUIRE_FALSE(frame.hand[1].bumper);
        REQUIRE(frame.a); // only the chord's buttons
        frame = IdleFrame(5'000'000'000 + 3'000'000'000LL);
        chord(frame, true); // still held 3 s later
        REQUIRE(latch.Filter(frame));
        frame = IdleFrame(5'000'000'000 + 3'010'000'000LL);
        REQUIRE_FALSE(latch.Filter(frame)); // all up, past the cooldown
        REQUIRE_FALSE(latch.latched());
        chord(frame, true);
        REQUIRE_FALSE(latch.Filter(frame)); // a new press goes through
        REQUIRE(frame.view);
    }
    SECTION("a gamepad that first reads all up does not clear it") {
        ChordLatch latch;
        latch.Arm();
        int64_t t = 0;
        InputFrame frame = IdleFrame(t); // SDL's first read of a newly opened pad: zeros
        REQUIRE(latch.Filter(frame));
        for (int i = 0; i < 40; ++i) { // then the held chord, 0.32 s
            t += kFlatFrameNs;
            frame = IdleFrame(t);
            chord(frame, true);
            REQUIRE(latch.Filter(frame));
            REQUIRE_FALSE(frame.view);
        }
    }
    SECTION("the cooldown starts at the first frame after Arm, in that frame's clock") {
        ChordLatch latch(1'000'000'000);
        latch.Arm();
        InputFrame frame = IdleFrame(900'000'000'000LL); // a huge OpenXR time is irrelevant
        REQUIRE(latch.Filter(frame));
        frame = IdleFrame(900'000'000'000LL + 999'000'000);
        REQUIRE(latch.Filter(frame));
        frame = IdleFrame(900'000'000'000LL + 1'000'000'000);
        REQUIRE_FALSE(latch.Filter(frame));
    }
    SECTION("with the mapper (flat after VR): no second switch while held") {
        // The log that showed it: VR -> flat, then flat -> VR 0.29 s later, the chord held.
        Mapper mapper;
        PanelGeometry panels;
        ChordLatch latch;
        int64_t t = 0;
        int toggles = 0;
        const auto step = [&](InputFrame frame) {
            frame.time_ns = t;
            t += kFlatFrameNs;
            latch.Filter(frame);
            const MapperOutput out = mapper.Update(frame, panels);
            if (out.commands.toggle_vr) {
                ++toggles;
                latch.Arm(); // as VrUi::RequestToggleVr does
            }
            return out;
        };
        latch.Arm(); // DestroyXr: flat begins
        mapper.Reset(); // a fresh bridge in flat: edges start over
        InputFrame frame = IdleFrame();
        step(frame); // the first read: all up
        chord(frame, true);
        for (int i = 0; i < 100; ++i) { // held 0.8 s
            const MapperOutput out = step(frame);
            REQUIRE_FALSE(out.controls.Get(Control::BumperL));
            REQUIRE_FALSE(out.controls.Get(Control::View));
            REQUIRE_FALSE(out.controls.Get(Control::Menu));
        }
        REQUIRE(toggles == 0);
        chord(frame, false);
        for (int i = 0; i < 50; ++i) { // let go; the cooldown ends meanwhile
            step(frame);
        }
        REQUIRE_FALSE(latch.latched());
        // A real new press: both bumpers, Menu, then View (the chord's order).
        frame.hand[0].bumper = frame.hand[1].bumper = true;
        frame.menu = true;
        step(frame);
        frame.view = true;
        step(frame);
        REQUIRE(toggles == 1);
        REQUIRE(latch.latched());
    }
}
