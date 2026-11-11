// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>
#include "common/param_package.h"
#include "common/settings.h"
#include "common/vector_math.h"
#include "core/frontend/input.h"
#include "frame_xr/input/controls.h"
#include "input_common/main.h"

// AzaharFrame
// The "frame_xr" input engine: the Steam Frame controllers as a virtual gamepad.
//
// The XR thread (frame_xr/input/input_bridge) writes the pad state once per display frame with
// SetControls(); the emulation thread reads it through the usual Azahar button and analog
// devices. Parameters of a device:
//     engine:frame_xr,button:N     N = int(FrameXr::Input::Control)
//     engine:frame_xr,analog:N     N = int(FrameXr::Input::AnalogControl)
//     engine:frame_xr              (motion device) a Frame controller as the 3DS's motion sensors
// An unbound control, or a pad nobody feeds, reads as released / centred; a motion device
// nobody feeds reads as a 3DS lying flat.

namespace InputCommon::FrameXr {

/// Where touches on the bottom screen go (the XR core wires it to GRenderWindow::TouchPressed /
/// TouchMoved / TouchReleased). Coordinates are the 3DS bottom screen's native pixels, 0..320 x
/// 0..240, origin top left; scale them with the framebuffer layout's bottom_screen rectangle.
/// Called on the XR thread.
class TouchSink {
public:
    virtual ~TouchSink() = default;
    virtual void TouchPressed(float x, float y) = 0;
    virtual void TouchMoved(float x, float y) = 0;
    virtual void TouchReleased() = 0;
};

/// The virtual pad shared between the XR thread (writer) and the emulation thread (readers).
/// Every value is atomic; a stick's x and y are stored together so they are read consistently.
class PadState {
public:
    void SetControls(const ::FrameXr::Input::ControlState& controls);
    void ReleaseAll();
    bool GetButton(::FrameXr::Input::Control control) const;
    std::tuple<float, float> GetStick(::FrameXr::Input::AnalogControl control) const;
    ::FrameXr::Input::ControlState Snapshot() const;

private:
    std::array<std::atomic<bool>, ::FrameXr::Input::kNumControls> buttons_{};
    std::array<std::atomic<std::uint64_t>, ::FrameXr::Input::kNumAnalogControls> sticks_{};
};

/// The motion sensors shared between the XR thread (writer) and the emulation thread (the HID
/// service reads them through the "frame_xr" motion device). Axes and units are the ones of
/// Azahar's other motion devices: accelerometer = gravity direction in g (a 3DS lying flat
/// reads (0, -1, 0)), gyroscope in degrees per second; see frame_xr/input/motion.h.
class MotionState {
public:
    void Set(const Common::Vec3f& accel, const Common::Vec3f& gyro);
    /// Back to a 3DS lying flat and still.
    void Reset();
    std::tuple<Common::Vec3f, Common::Vec3f> Get() const;

private:
    mutable std::mutex mutex_;
    Common::Vec3f accel_{0.0f, -1.0f, 0.0f};
    Common::Vec3f gyro_{0.0f, 0.0f, 0.0f};
};

/// The process wide pad. Valid whether or not the engine is registered.
PadState& GetPad();
/// The process wide motion sensors. Valid whether or not the engine is registered.
MotionState& GetMotion();

/// Registers the "frame_xr" button and analog factories (called from InputCommon::Init).
void Register();
/// Unregisters them (called from InputCommon::Shutdown).
void Unregister();

/// Sets or clears the touch sink. Safe against a call in flight: when it returns, the previous
/// sink is no longer being called.
void SetTouchSink(TouchSink* sink);
void SendTouchPressed(float x, float y);
void SendTouchMoved(float x, float y);
void SendTouchReleased();

/// AzaharFrame touchpad: the real mouse over the render window (invisible in VR), written by the
/// Qt thread, read by the XR thread. x and y are 0..1 over the window. Never fed by the mirror
/// panel's synthesised mouse (the menu), only by the mouse events the window itself gets.
struct MouseState {
    bool inside = false;
    bool down = false;
    float x = 0.0f;
    float y = 0.0f;
};
void SetMouseState(const MouseState& state);
void ClearMouseState();
MouseState GetMouseState();

/// The default VR profile: CitraVR's mapping with the Frame's D-pad (see MAPPING.md). The XR core
/// applies it when VR starts so the Qt Controls dialog edits a profile that already works.
struct DefaultProfile {
    std::string name;
    std::array<std::string, Settings::NativeButton::NumButtons> buttons;
    std::array<std::string, Settings::NativeAnalog::NumAnalogs> analogs;
};
DefaultProfile GetDefaultProfile();

/// Copies the default profile's buttons and analogs into an input profile (name, motion and touch
/// devices and everything else are left alone). Afterwards call
/// Core::System::ReloadInputDevices() so the HID service picks it up.
void ApplyDefaultProfile(Settings::InputProfile& profile);

/// Moves L and R from the grips (the first default, `button:8`/`button:9`) to the bumpers
/// (`button:12`/`button:13`) in a profile saved by an older build. Other bindings stay. Returns
/// true when something changed.
bool MigrateGripBindings(Settings::InputProfile& profile);

/// Points the profile's motion device at the Frame controller ("engine:frame_xr") unless the user
/// chose another device in Configure > Controls: only the default (Mouse, "motion_emu") or an
/// empty setting is replaced. Returns true when something changed.
bool UseFrameMotionDevice(Settings::InputProfile& profile);

/// True when the profile already binds frame_xr to at least one button (a user profile saved
/// earlier), so VR start-up can leave their bindings alone.
bool ProfileUsesFrameXr(const Settings::InputProfile& profile);

/// Display text for the Controls dialog (used by InputCommon::ButtonToText / AnalogToText).
std::string ButtonToText(const Common::ParamPackage& param);
std::string AnalogToText(const Common::ParamPackage& param, const std::string& dir);

/// Adds the frame_xr poller for the given type to the list (so a VR button can be recorded in the
/// Controls dialog). Touchpad has none.
void AppendPollers(std::vector<std::unique_ptr<Polling::DevicePoller>>& pollers,
                   Polling::DeviceType type);

} // namespace InputCommon::FrameXr
