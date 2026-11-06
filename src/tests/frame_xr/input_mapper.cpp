// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
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
    REQUIRE(only(pad_for(Control::GripL), L));
    REQUIRE(only(pad_for(Control::GripR), R));
    REQUIRE(only(pad_for(Control::TriggerL), ZL));
    REQUIRE(only(pad_for(Control::TriggerR), ZR));
    REQUIRE(only(pad_for(Control::DpadUp), Up));
    REQUIRE(only(pad_for(Control::DpadDown), Down));
    REQUIRE(only(pad_for(Control::DpadLeft), Left));
    REQUIRE(only(pad_for(Control::DpadRight), Right));
    REQUIRE(only(pad_for(Control::Menu), Start));
    REQUIRE(only(pad_for(Control::View), Select));
    REQUIRE(only(pad_for(Control::StickClickL), Home));

    SECTION("bumpers and the reset click are unbound") {
        for (const Control control : {Control::BumperL, Control::BumperR, Control::StickClickR}) {
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

    SECTION("face buttons and the D-pad pass straight through") {
        InputFrame frame = IdleFrame();
        frame.a = true;
        frame.y = true;
        frame.dpad_left = true;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::A));
        REQUIRE(out.controls.Get(Control::Y));
        REQUIRE_FALSE(out.controls.Get(Control::B));
        REQUIRE(out.controls.Get(Control::DpadLeft));
        REQUIRE_FALSE(out.controls.Get(Control::DpadRight));
    }
    SECTION("Menu and View are Menu and View while View is tapped") {
        InputFrame frame = IdleFrame();
        frame.menu = true;
        frame.view = true;
        const MapperOutput out = runner.Step(frame);
        REQUIRE(out.controls.Get(Control::Menu));
        REQUIRE(out.controls.Get(Control::View));
        REQUIRE_FALSE(out.commands.stop_game);
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

TEST_CASE("FrameXr mapper: dragging the top screen", "[frame_xr]") {
    MapperConfig config;
    config.horizontal_lock = false;
    Runner runner(config);
    InputFrame frame = IdleFrame();
    Point(frame, Hand::Right, Vec3f{0.5f, 0.2f, -2.0f});
    frame.hand[1].trigger = 1.0f;

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
    SECTION("letting go ends the drag") {
        runner.Step(frame);
        frame.hand[1].trigger = 0.0f;
        const MapperOutput out = runner.Step(frame);
        REQUIRE_FALSE(out.commands.top_drag_active);
    }
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
}

TEST_CASE("FrameXr mapper: View held stops the game", "[frame_xr]") {
    Runner runner;
    InputFrame frame = IdleFrame();
    frame.view = true;

    MapperOutput out = runner.Step(frame);
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
    REQUIRE(out.controls.Get(Control::View));
    runner.now += 1'600'000'000;
    REQUIRE(runner.Step(frame).commands.stop_game);
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
