// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/logging/backend.h"
#include "common/param_package.h"
#include "core/frontend/input.h"
#include "frame_xr/input/default_bindings.h"
#include "input_common/frame_xr.h"

// AzaharFrame
using Catch::Approx;
namespace Fx = InputCommon::FrameXr;
using FrameXr::Input::AnalogControl;
using FrameXr::Input::Control;
using FrameXr::Input::ControlState;

namespace {

struct EngineScope {
    EngineScope() {
        Common::Log::DisableLoggingInTests();
        Fx::Register();
    }
    ~EngineScope() {
        Fx::Unregister();
    }
};

class RecordingSink final : public Fx::TouchSink {
public:
    void TouchPressed(float x, float y) override {
        log.push_back("press " + std::to_string(static_cast<int>(x)) + "," +
                      std::to_string(static_cast<int>(y)));
    }
    void TouchMoved(float x, float y) override {
        log.push_back("move " + std::to_string(static_cast<int>(x)) + "," +
                      std::to_string(static_cast<int>(y)));
    }
    void TouchReleased() override {
        log.push_back("release");
    }
    std::vector<std::string> log;
};

} // namespace

TEST_CASE("FrameXr engine: default profile devices follow the pad", "[frame_xr]") {
    EngineScope scope;
    const Fx::DefaultProfile profile = Fx::GetDefaultProfile();

    SECTION("every default-bound 3DS button has a frame_xr param") {
        using namespace Settings::NativeButton;
        for (const Values button : {A, B, X, Y, Up, Down, Left, Right, L, R, Start, Select, ZL,
                                    ZR, Home}) {
            const Common::ParamPackage param{profile.buttons[button]};
            REQUIRE(param.Get("engine", "") == "frame_xr");
            REQUIRE(param.Has("button"));
        }
        // Debug, Gpio14 and Power stay empty.
        REQUIRE(profile.buttons[Debug].empty());
        REQUIRE(profile.buttons[Gpio14].empty());
        REQUIRE(profile.buttons[Power].empty());
    }
    SECTION("a bound button follows its control") {
        const auto start = Input::CreateDevice<Input::ButtonDevice>(
            profile.buttons[Settings::NativeButton::Start]);
        REQUIRE_FALSE(start->GetStatus());
        ControlState state;
        state.Set(Control::Menu, true);
        Fx::GetPad().SetControls(state);
        REQUIRE(start->GetStatus());
        Fx::GetPad().ReleaseAll();
        REQUIRE_FALSE(start->GetStatus());
    }
    SECTION("the circle pad and C-stick follow the sticks") {
        const auto circle = Input::CreateDevice<Input::AnalogDevice>(
            profile.analogs[Settings::NativeAnalog::CirclePad]);
        const auto c_stick = Input::CreateDevice<Input::AnalogDevice>(
            profile.analogs[Settings::NativeAnalog::CStick]);
        ControlState state;
        state.SetStick(AnalogControl::StickL, {0.5f, -0.25f});
        state.SetStick(AnalogControl::StickR, {-1.0f, 1.0f});
        Fx::GetPad().SetControls(state);
        const auto [cx, cy] = circle->GetStatus();
        const auto [sx, sy] = c_stick->GetStatus();
        REQUIRE(cx == Approx(0.5f));
        REQUIRE(cy == Approx(-0.25f));
        REQUIRE(sx == Approx(-1.0f));
        REQUIRE(sy == Approx(1.0f));
        Fx::GetPad().ReleaseAll();
    }
    SECTION("a bad param reads as released") {
        const auto device = Input::CreateDevice<Input::ButtonDevice>("engine:frame_xr,button:99");
        REQUIRE_FALSE(device->GetStatus());
    }
    SECTION("ApplyDefaultProfile fills only buttons and analogs") {
        Settings::InputProfile input;
        input.name = "mine";
        input.motion_device = "motion";
        Fx::ApplyDefaultProfile(input);
        REQUIRE(input.name == "mine");
        REQUIRE(input.motion_device == "motion");
        REQUIRE(Fx::ProfileUsesFrameXr(input));
        REQUIRE_FALSE(Fx::ProfileUsesFrameXr(Settings::InputProfile{}));
    }
}

TEST_CASE("FrameXr engine: texts and pollers for the Controls dialog", "[frame_xr]") {
    EngineScope scope;
    const Common::ParamPackage menu{{"engine", "frame_xr"},
                                    {"button", std::to_string(static_cast<int>(Control::Menu))}};
    REQUIRE(Fx::ButtonToText(menu) == "Frame Menu");
    REQUIRE(InputCommon::ButtonToText(menu) == "Frame Menu");
    REQUIRE(Fx::ButtonToText(Common::ParamPackage{}) == "[not set]");

    std::vector<std::unique_ptr<InputCommon::Polling::DevicePoller>> pollers;
    Fx::AppendPollers(pollers, InputCommon::Polling::DeviceType::Button);
    REQUIRE(pollers.size() == 1);
    Fx::AppendPollers(pollers, InputCommon::Polling::DeviceType::Touchpad);
    REQUIRE(pollers.size() == 1);

    pollers[0]->Start();
    REQUIRE(pollers[0]->GetNextInput().Get("engine", "").empty()); // empty Serialize() is a placeholder
    ControlState state;
    state.Set(Control::B, true);
    Fx::GetPad().SetControls(state);
    const Common::ParamPackage got = pollers[0]->GetNextInput();
    REQUIRE(got.Get("engine", "") == "frame_xr");
    REQUIRE(got.Get("button", -1) == static_cast<int>(Control::B));
    // Held is not a new press.
    REQUIRE(pollers[0]->GetNextInput().Get("engine", "").empty()); // empty Serialize() is a placeholder
    pollers[0]->Stop();
    Fx::GetPad().ReleaseAll();
}

TEST_CASE("FrameXr engine: touch sink", "[frame_xr]") {
    RecordingSink sink;
    Fx::SendTouchPressed(1.0f, 2.0f); // no sink: ignored
    Fx::SetTouchSink(&sink);
    Fx::SendTouchPressed(10.0f, 20.0f);
    Fx::SendTouchMoved(30.0f, 40.0f);
    Fx::SendTouchReleased();
    Fx::SetTouchSink(nullptr);
    Fx::SendTouchPressed(5.0f, 5.0f);
    REQUIRE(sink.log == std::vector<std::string>{"press 10,20", "move 30,40", "release"});
}
