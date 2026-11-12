// Copyright 2018-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "common/settings.h"
#include "common/threadsafe_queue.h"
#include "input_common/sdl/sdl_joystick.h"

namespace InputCommon::SDL {

class SDLButtonFactory;
class SDLAnalogFactory;
class SDLMotionFactory;
class SDLTouchFactory;

class SDLState : public State {
public:
    /// Initializes and registers SDL device factories
    SDLState();

    /// Unregisters SDL device factories and shut them down.
    ~SDLState() override;

    /// Handle SDL_Events for joysticks from SDL_PollEvent
    void HandleGameControllerEvent(const SDL_Event& event);

    // returns a pointer to a vector of joysticks that match the needs of this device.
    // will be a pointer because it will be updated when new joysticks are added
    std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> GetJoysticksByGUID(
        const std::string& guid);
    std::shared_ptr<SDLJoystick> GetSDLJoystickBySDLID(SDL_JoystickID sdl_id);

    Common::ParamPackage GetSDLControllerButtonBind(const Common::ParamPackage a_button_params,
                                                    Settings::NativeButton::Values button);
    Common::ParamPackage GetSDLControllerAnalogBindByGUID(const std::string& guid, int port,
                                                          Settings::NativeAnalog::Values analog);

    /// Get all DevicePoller that use the SDL backend for a specific device type
    Pollers GetPollers(Polling::DeviceType type) override;

    void GetSystemBatteryState(float& percentage, bool& charging) override;

    bool GetTouchpad(TouchpadSample& sample) override; // AzaharFrame touchpad
    std::uint64_t GetPadGeneration() override; // AzaharFrame pads
    std::uint64_t GetPumpCount() override;     // AzaharFrame flat
    void GetPadInfos(std::vector<SDLPadInfo>& out) override;
    void GetPadStates(std::vector<SDLPadState>& out) override;

    /// Used by the Pollers during config
    std::atomic<bool> polling = false;
    Common::SPSCQueue<SDL_Event> event_queue;
    static constexpr size_t MAX_EVENT_QUEUE_SIZE = 10000;

private:
    void InitJoystick(int joystick_index);
    void CloseJoystick(SDL_JoystickID instance_id);

    /// AzaharFrame flatfix: read the controllers through SDL (SDL's joystick lock, then the map's).
    void ReadPadInfos(std::vector<SDLPadInfo>& out);
    void ReadPadStates(std::vector<SDLPadState>& out);
    /// AzaharFrame flatfix: the poll thread (or the constructor, before it starts) copies them
    /// into the snapshot below after each SDL_PumpEvents.
    void PublishPads();

    /// Needs to be called before SDL_QuitSubSystem.
    void CloseJoysticks();

    /// Map of GUID of a list of corresponding virtual Joysticks
    std::unordered_map<std::string, std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>>>
        joystick_map;
    // This vector keeps a list of all joysticks, ignoring guid
    std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> joystick_vector =
        std::make_shared<std::vector<std::shared_ptr<SDLJoystick>>>();
    std::mutex joystick_map_mutex;
    std::atomic<std::uint64_t> pad_generation{1}; ///< AzaharFrame pads: bumped on add / remove
    std::atomic<int> pad_count{0};             ///< AzaharFrame flat: open controllers
    std::atomic<std::uint64_t> pump_count{0};  ///< AzaharFrame flat: SDL_PumpEvents calls

    // AzaharFrame flatfix: the controllers as the poll thread saw them after its last pump. Other
    // threads (the XR frame loop, in VR and in Flat) read this copy and never take SDL's joystick
    // lock: a frame loop that waited on SDL in Flat never got to switch back to VR.
    std::mutex snapshot_mutex;
    std::vector<SDLPadInfo> snapshot_infos;
    std::vector<SDLPadState> snapshot_states;
    std::atomic<std::uint64_t> snapshot_generation{0}; ///< pad_generation of snapshot_infos
    std::uint64_t published_generation = 0;              ///< poll thread only
    std::vector<SDLPadInfo> scratch_infos;               ///< poll thread only
    std::vector<SDLPadState> scratch_states;             ///< poll thread only

    std::shared_ptr<SDLTouchFactory> touch_factory;
    std::shared_ptr<SDLButtonFactory> button_factory;
    std::shared_ptr<SDLAnalogFactory> analog_factory;
    std::shared_ptr<SDLMotionFactory> motion_factory;

    bool start_thread = false;
    std::atomic<bool> initialized = false;

    std::thread poll_thread;
    // maximum events allowed in the queue - if more than this are there, something is wrong
};
} // namespace InputCommon::SDL
