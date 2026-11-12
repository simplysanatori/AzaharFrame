// Copyright 2017-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Common {
class ParamPackage;
}

namespace InputCommon {

/// Initializes and registers all built-in input device factories.
void Init();

/// Deregisters all built-in input device factories and shuts them down.
void Shutdown();

class Keyboard;

/// Gets the keyboard button device factory.
Keyboard* GetKeyboard();

class MotionEmu;

/// Gets the motion emulation factory.
MotionEmu* GetMotionEmu();

/// Generates a serialized param package for creating a keyboard button device
std::string GenerateKeyboardParam(int key_code);

/// Generates a serialized param package for creating an analog device taking input from keyboard
std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale);

std::string AnalogToText(const Common::ParamPackage& param, const std::string& dir);
std::string ButtonToText(const Common::ParamPackage& param);

Common::ParamPackage GetControllerButtonBinds(const Common::ParamPackage& params, int button);
Common::ParamPackage GetControllerAnalogBinds(const Common::ParamPackage& params, int analog);

/// Reloads the input devices
void ReloadInputDevices();

struct BatteryState {
    float percentage = 1.0f;
    bool charging = true;
};

BatteryState GetSystemBatteryState();

/// AzaharFrame touchpad: the touchpad of the first connected controller that has one (the right
/// pad when it has two, as the Steam Controller's). x and y are 0..1 from the top left.
struct TouchpadSample {
    bool present = false;
    bool contact = false;
    bool click = false;
    float x = 0.0f;
    float y = 0.0f;
};
/// Safe from any thread. False (and `sample` untouched) without SDL or a controller touchpad.
bool GetSDLTouchpad(TouchpadSample& sample);

/// AzaharFrame pads: every SDL game controller, for the VR gamepad merge (frame_xr/input/
/// gamepads.h). `id` is SDL's instance id (stable while connected). `vendor`, `product`, `name`
/// and `type` are the real controller's, also behind Steam Input (SDL reads Steam's
/// SteamVirtualGamepadInfo); `guid` is SDL's GUID string, which for a Steam Input pad is the
/// virtual pad's (the same for all of them). `type` is SDL_GameControllerType.
struct SDLPadInfo {
    int id = -1;
    std::string guid;
    std::string name;
    int type = 0;
    std::uint16_t vendor = 0;
    std::uint16_t product = 0;
    bool steam_virtual = false; ///< a Steam Input virtual pad (Valve 28de:11ff)
    bool has_touchpad = false;
};
/// One controller's state. `buttons` in SDL_GameControllerButton order (A, B, X, Y, Back,
/// Guide, Start, LeftStick, RightStick, LeftShoulder, RightShoulder, Up, Down, Left, Right, Misc1,
/// Paddle1..4, Touchpad); `axes` in SDL_GameControllerAxis order (LeftX, LeftY, RightX, RightY,
/// TriggerLeft, TriggerRight), -1..1 (triggers 0..1), Y down as SDL's.
struct SDLPadState {
    static constexpr int kButtons = 21;
    static constexpr int kAxes = 6;
    int id = -1;
    std::array<bool, kButtons> buttons{};
    std::array<float, kAxes> axes{};
    TouchpadSample touchpad; ///< present: the controller has a touchpad (the right one of two)
};
/// Counts controllers added and removed; the infos only change when it does. Any thread.
std::uint64_t GetSDLPadGeneration();
/// Fills `out` with the connected controllers (replacing its contents). Any thread.
void GetSDLPadInfos(std::vector<SDLPadInfo>& out);
/// Fills `out` with their states (replacing its contents; no allocation once it has grown).
void GetSDLPadStates(std::vector<SDLPadState>& out);

namespace Polling {

enum class DeviceType { Button, Analog, Touchpad };

/**
 * A class that can be used to get inputs from an input device like controllers without having to
 * poll the device's status yourself
 */
class DevicePoller {
public:
    virtual ~DevicePoller() = default;
    /// Setup and start polling for inputs, should be called before GetNextInput
    virtual void Start() = 0;
    /// Stop polling
    virtual void Stop() = 0;
    /**
     * Every call to this function returns the next input recorded since calling Start
     * @return A ParamPackage of the recorded input, which can be used to create an InputDevice.
     *         If there has been no input, the package is empty
     */
    virtual Common::ParamPackage GetNextInput() = 0;
};

// Get all DevicePoller from all backends for a specific device type
std::vector<std::unique_ptr<DevicePoller>> GetPollers(DeviceType type);
} // namespace Polling
} // namespace InputCommon
