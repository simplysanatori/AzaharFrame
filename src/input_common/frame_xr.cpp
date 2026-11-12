// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <cstring>
#include <atomic>
#include <mutex>
#include <optional>
#include "frame_xr/input/default_bindings.h"
#include "frame_xr/input/stick_route.h"
#include "input_common/frame_xr.h"

// AzaharFrame
namespace InputCommon::FrameXr {

namespace {

using ::FrameXr::Input::AnalogControl;
using ::FrameXr::Input::Control;
using ::FrameXr::Input::ControlState;
using ::FrameXr::Input::kNumAnalogControls;
using ::FrameXr::Input::kNumControls;

std::uint64_t PackStick(float x, float y) {
    std::uint32_t xb = 0;
    std::uint32_t yb = 0;
    std::memcpy(&xb, &x, sizeof(xb));
    std::memcpy(&yb, &y, sizeof(yb));
    return (static_cast<std::uint64_t>(yb) << 32) | xb;
}

std::tuple<float, float> UnpackStick(std::uint64_t packed) {
    const std::uint32_t xb = static_cast<std::uint32_t>(packed & 0xFFFFFFFFu);
    const std::uint32_t yb = static_cast<std::uint32_t>(packed >> 32);
    float x = 0.0f;
    float y = 0.0f;
    std::memcpy(&x, &xb, sizeof(x));
    std::memcpy(&y, &yb, sizeof(y));
    return {x, y};
}

class FrameXrButton final : public Input::ButtonDevice {
public:
    explicit FrameXrButton(Control control_) : control(control_) {}

    bool GetStatus() const override {
        return GetPad().GetButton(control);
    }

private:
    Control control;
};

class FrameXrAnalog final : public Input::AnalogDevice {
public:
    // AzaharFrame padsui: `route` swaps or turns the axes round (Edit Controls' two pushes).
    FrameXrAnalog(AnalogControl control_, ::FrameXr::Input::StickRoute route_)
        : control(control_), route(route_) {}

    std::tuple<float, float> GetStatus() const override {
        const auto [x, y] = GetPad().GetStick(control);
        if (route.Plain()) {
            return {x, y};
        }
        const auto v = ::FrameXr::Input::ApplyStickRoute({x, y}, route);
        return {v.x, v.y};
    }

private:
    AnalogControl control;
    ::FrameXr::Input::StickRoute route;
};

class FrameXrMotion final : public Input::MotionDevice {
public:
    std::tuple<Common::Vec3<float>, Common::Vec3<float>> GetStatus() const override {
        return GetMotion().Get();
    }
};

class MotionFactory final : public Input::Factory<Input::MotionDevice> {
public:
    std::unique_ptr<Input::MotionDevice> Create(const Common::ParamPackage&) override {
        return std::make_unique<FrameXrMotion>();
    }
};

class ButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override {
        const int index = params.Get("button", -1);
        if (index < 0 || index >= kNumControls) {
            return std::make_unique<Input::ButtonDevice>(); // reads as released
        }
        return std::make_unique<FrameXrButton>(static_cast<Control>(index));
    }
};

class AnalogFactory final : public Input::Factory<Input::AnalogDevice> {
public:
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override {
        const int index = params.Get("analog", -1);
        if (index < 0 || index >= kNumAnalogControls) {
            return std::make_unique<Input::AnalogDevice>(); // reads as centred
        }
        ::FrameXr::Input::StickRoute route; // AzaharFrame padsui
        route.stick = index;
        route.swap_xy = params.Get("swap_xy", 0) != 0;
        route.invert_x = params.Get("invert_x", 0) != 0;
        route.invert_y = params.Get("invert_y", 0) != 0;
        return std::make_unique<FrameXrAnalog>(static_cast<AnalogControl>(index), route);
    }
};

/// Records the next VR button that goes down.
class ButtonPoller final : public Polling::DevicePoller {
public:
    void Start() override {
        previous = GetPad().Snapshot();
        active = true;
    }
    void Stop() override {
        active = false;
    }
    Common::ParamPackage GetNextInput() override {
        if (!active) {
            return {};
        }
        const ControlState now = GetPad().Snapshot();
        Common::ParamPackage result;
        for (int i = 0; i < kNumControls; ++i) {
            const auto idx = static_cast<std::size_t>(i);
            if (now.buttons[idx] && !previous.buttons[idx]) {
                result = Common::ParamPackage{{"engine", "frame_xr"},
                                              {"button", std::to_string(i)}};
                break;
            }
        }
        previous = now;
        return result;
    }

private:
    ControlState previous;
    bool active = false;
};

/// Records the next VR stick pushed past halfway.
class AnalogPoller final : public Polling::DevicePoller {
public:
    void Start() override {
        active = true;
    }
    void Stop() override {
        active = false;
    }
    Common::ParamPackage GetNextInput() override {
        if (!active) {
            return {};
        }
        for (int i = 0; i < kNumAnalogControls; ++i) {
            const auto [x, y] = GetPad().GetStick(static_cast<AnalogControl>(i));
            if (std::hypot(x, y) > 0.5f) {
                return Common::ParamPackage{{"engine", "frame_xr"},
                                            {"analog", std::to_string(i)}};
            }
        }
        return {};
    }

private:
    bool active = false;
};

std::mutex sink_mutex;
std::mutex mouse_mutex; // AzaharFrame touchpad
MouseState mouse_state;
TouchSink* touch_sink = nullptr;

} // namespace

void PadState::SetControls(const ControlState& controls) {
    for (int i = 0; i < kNumControls; ++i) {
        buttons_[static_cast<std::size_t>(i)].store(controls.buttons[static_cast<std::size_t>(i)],
                                                    std::memory_order_relaxed);
    }
    for (int i = 0; i < kNumAnalogControls; ++i) {
        const auto& stick = controls.sticks[static_cast<std::size_t>(i)];
        sticks_[static_cast<std::size_t>(i)].store(PackStick(stick.x, stick.y),
                                                   std::memory_order_relaxed);
    }
}

void PadState::ReleaseAll() {
    SetControls(ControlState{});
}

bool PadState::GetButton(Control control) const {
    return buttons_[static_cast<std::size_t>(control)].load(std::memory_order_relaxed);
}

std::tuple<float, float> PadState::GetStick(AnalogControl control) const {
    return UnpackStick(sticks_[static_cast<std::size_t>(control)].load(std::memory_order_relaxed));
}

ControlState PadState::Snapshot() const {
    ControlState state;
    for (int i = 0; i < kNumControls; ++i) {
        state.buttons[static_cast<std::size_t>(i)] = GetButton(static_cast<Control>(i));
    }
    for (int i = 0; i < kNumAnalogControls; ++i) {
        const auto [x, y] = GetStick(static_cast<AnalogControl>(i));
        state.sticks[static_cast<std::size_t>(i)] = {x, y};
    }
    return state;
}

PadState& GetPad() {
    static PadState pad;
    return pad;
}

void MotionState::Set(const Common::Vec3f& accel, const Common::Vec3f& gyro) {
    std::lock_guard lock{mutex_};
    accel_ = accel;
    gyro_ = gyro;
}

void MotionState::Reset() {
    Set(Common::Vec3f{0.0f, -1.0f, 0.0f}, Common::Vec3f{0.0f, 0.0f, 0.0f});
}

std::tuple<Common::Vec3f, Common::Vec3f> MotionState::Get() const {
    std::lock_guard lock{mutex_};
    return {accel_, gyro_};
}

MotionState& GetMotion() {
    static MotionState motion;
    return motion;
}

void Register() {
    Input::RegisterFactory<Input::ButtonDevice>("frame_xr", std::make_shared<ButtonFactory>());
    Input::RegisterFactory<Input::AnalogDevice>("frame_xr", std::make_shared<AnalogFactory>());
    Input::RegisterFactory<Input::MotionDevice>("frame_xr", std::make_shared<MotionFactory>());
}

void Unregister() {
    Input::UnregisterFactory<Input::ButtonDevice>("frame_xr");
    Input::UnregisterFactory<Input::AnalogDevice>("frame_xr");
    Input::UnregisterFactory<Input::MotionDevice>("frame_xr");
    GetPad().ReleaseAll();
    GetMotion().Reset();
}

void SetTouchSink(TouchSink* sink) {
    std::lock_guard lock{sink_mutex};
    touch_sink = sink;
}

void SendTouchPressed(float x, float y) {
    std::lock_guard lock{sink_mutex};
    if (touch_sink) {
        touch_sink->TouchPressed(x, y);
    }
}

void SendTouchMoved(float x, float y) {
    std::lock_guard lock{sink_mutex};
    if (touch_sink) {
        touch_sink->TouchMoved(x, y);
    }
}

void SendTouchReleased() {
    std::lock_guard lock{sink_mutex};
    if (touch_sink) {
        touch_sink->TouchReleased();
    }
}

void SetMouseState(const MouseState& state) {
    std::lock_guard lock{mouse_mutex};
    mouse_state = state;
}

void ClearMouseState() {
    std::lock_guard lock{mouse_mutex};
    mouse_state = {};
}

MouseState GetMouseState() {
    std::lock_guard lock{mouse_mutex};
    return mouse_state;
}

namespace {
std::atomic<bool> vr_mouse_owned{false}; // AzaharFrame padsui
}

void SetVrMouseOwned(bool owned) {
    vr_mouse_owned.store(owned, std::memory_order_relaxed);
}

bool VrMouseOwned() {
    return vr_mouse_owned.load(std::memory_order_relaxed);
}

DefaultProfile GetDefaultProfile() {
    DefaultProfile profile;
    profile.name = "Azahar for Frame VR";
    for (const auto& binding : ::FrameXr::Input::kDefaultButtonBindings) {
        profile.buttons[binding.button] =
            Common::ParamPackage{{"engine", "frame_xr"},
                                 {"button", std::to_string(static_cast<int>(binding.control))}}
                .Serialize();
    }
    for (const auto& binding : ::FrameXr::Input::kDefaultAnalogBindings) {
        profile.analogs[binding.analog] =
            Common::ParamPackage{{"engine", "frame_xr"},
                                 {"analog", std::to_string(static_cast<int>(binding.control))}}
                .Serialize();
    }
    return profile;
}

void ApplyDefaultProfile(Settings::InputProfile& profile) {
    const DefaultProfile defaults = GetDefaultProfile();
    for (std::size_t i = 0; i < profile.buttons.size(); ++i) {
        profile.buttons[i] = defaults.buttons[i];
    }
    for (std::size_t i = 0; i < profile.analogs.size(); ++i) {
        profile.analogs[i] = defaults.analogs[i];
    }
}

bool MigrateGripBindings(Settings::InputProfile& profile) {
    using ::FrameXr::Input::Control;
    bool changed = false;
    const auto move = [&](Settings::NativeButton::Values button, Control from, Control to) {
        std::string& text = profile.buttons[button];
        const Common::ParamPackage param{text};
        if (param.Get("engine", "") == "frame_xr" &&
            param.Get("button", -1) == static_cast<int>(from)) {
            text = Common::ParamPackage{{"engine", "frame_xr"},
                                        {"button", std::to_string(static_cast<int>(to))}}
                       .Serialize();
            changed = true;
        }
    };
    move(Settings::NativeButton::L, Control::GripL, Control::BumperL);
    move(Settings::NativeButton::R, Control::GripR, Control::BumperR);
    return changed;
}

bool UseFrameMotionDevice(Settings::InputProfile& profile) {
    const Common::ParamPackage param{profile.motion_device};
    const std::string engine = param.Get("engine", "motion_emu");
    if (engine != "motion_emu") {
        return false; // the user picked something else (or it is already ours)
    }
    Common::ParamPackage frame_param;
    frame_param.Set("engine", "frame_xr");
    profile.motion_device = frame_param.Serialize();
    return true;
}

bool ProfileUsesFrameXr(const Settings::InputProfile& profile) {
    for (const std::string& button : profile.buttons) {
        if (Common::ParamPackage{button}.Get("engine", "") == "frame_xr") {
            return true;
        }
    }
    return false;
}

namespace {
std::mutex routes_mutex; // AzaharFrame pads
std::optional<ProfileRoutes> routes_set;
} // namespace

ProfileRoutes RoutesOf(const std::array<std::string, Settings::NativeButton::NumButtons>& buttons,
                       const std::array<std::string, Settings::NativeAnalog::NumAnalogs>& analogs) {
    ProfileRoutes routes;
    for (std::size_t i = 0; i < buttons.size(); i++) {
        const Common::ParamPackage param{buttons[i]};
        const int c = param.Get("engine", "") == "frame_xr" ? param.Get("button", -1) : -1;
        routes.buttons[i] = static_cast<std::int8_t>(c >= 0 && c < kNumControls ? c : -1);
    }
    for (std::size_t i = 0; i < analogs.size(); i++) {
        const Common::ParamPackage param{analogs[i]};
        const int a = param.Get("engine", "") == "frame_xr" ? param.Get("analog", -1) : -1;
        // AzaharFrame padsui: with the route's axes (stick_route.h).
        ::FrameXr::Input::StickRoute route;
        route.stick = a >= 0 && a < kNumAnalogControls ? a : -1;
        route.swap_xy = param.Get("swap_xy", 0) != 0;
        route.invert_x = param.Get("invert_x", 0) != 0;
        route.invert_y = param.Get("invert_y", 0) != 0;
        routes.analogs[i] = ::FrameXr::Input::EncodeStickRoute(route);
    }
    return routes;
}

void SetProfileRoutes(const ProfileRoutes& routes) {
    std::lock_guard lock{routes_mutex};
    routes_set = routes;
}

ProfileRoutes GetProfileRoutes() {
    {
        std::lock_guard lock{routes_mutex};
        if (routes_set) {
            return *routes_set;
        }
    }
    static const ProfileRoutes defaults = [] {
        const DefaultProfile profile = GetDefaultProfile();
        return RoutesOf(profile.buttons, profile.analogs);
    }();
    return defaults;
}

std::string ButtonToText(const Common::ParamPackage& param) {
    const int index = param.Get("button", -1);
    if (index < 0 || index >= kNumControls) {
        return "[not set]";
    }
    return std::string("Frame ") + ::FrameXr::Input::ControlName(static_cast<Control>(index));
}

std::string AnalogToText(const Common::ParamPackage& param, const std::string& dir) {
    const int index = param.Get("analog", -1);
    if (index < 0 || index >= kNumAnalogControls) {
        return "[not set]";
    }
    if (dir == "modifier") {
        return "[unused]";
    }
    const char* name = ::FrameXr::Input::AnalogControlName(static_cast<AnalogControl>(index));
    if (dir == "left") {
        return std::string("Frame ") + name + " Left";
    }
    if (dir == "right") {
        return std::string("Frame ") + name + " Right";
    }
    if (dir == "up") {
        return std::string("Frame ") + name + " Up";
    }
    if (dir == "down") {
        return std::string("Frame ") + name + " Down";
    }
    return {};
}

void AppendPollers(std::vector<std::unique_ptr<Polling::DevicePoller>>& pollers,
                   Polling::DeviceType type) {
    switch (type) {
    case Polling::DeviceType::Button:
        pollers.push_back(std::make_unique<ButtonPoller>());
        break;
    case Polling::DeviceType::Analog:
        pollers.push_back(std::make_unique<AnalogPoller>());
        break;
    default:
        break;
    }
}

} // namespace InputCommon::FrameXr
