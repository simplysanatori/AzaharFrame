// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "core/frontend/input.h"
#include "input_common/main.h"

union SDL_Event;

namespace Common {
class ParamPackage;
} // namespace Common

namespace InputCommon::Polling {
class DevicePoller;
enum class DeviceType;
} // namespace InputCommon::Polling

namespace InputCommon::SDL {

class State {
public:
    using Pollers = std::vector<std::unique_ptr<Polling::DevicePoller>>;

    /// Unregisters SDL device factories and shut them down.
    virtual ~State() = default;

    virtual Pollers GetPollers(Polling::DeviceType type) = 0;

    virtual void GetSystemBatteryState(float& percentage, bool& charging) = 0;

    /// AzaharFrame touchpad: see InputCommon::GetSDLTouchpad.
    virtual bool GetTouchpad(TouchpadSample& sample) {
        return false;
    }
    /// AzaharFrame pads: see InputCommon::GetSDLPadInfos and friends.
    virtual std::uint64_t GetPadGeneration() {
        return 0;
    }
    virtual void GetPadInfos(std::vector<SDLPadInfo>& out) {}
    virtual void GetPadStates(std::vector<SDLPadState>& out) {}
};

class NullState : public State {
public:
    Pollers GetPollers(Polling::DeviceType type) override {
        return {};
    }

    void GetSystemBatteryState(float& percentage, bool& charging) override {
        percentage = 1.0f;
        charging = true;
    }
};

std::unique_ptr<State> Init();

} // namespace InputCommon::SDL
