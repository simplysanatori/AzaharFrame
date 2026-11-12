// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "frame_xr/input/devices.h"
#include "input_common/frame_xr.h"
#include "input_common/main.h"

// AzaharFrame pads
namespace FrameXr::Input::Devices {

namespace {

constexpr const char* kMappingFile = "controller-mappings.txt";

std::string MappingPath() {
    return FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + kMappingFile;
}

/// input_common's SDL, and the kernel's list of input devices.
class SystemSource final : public GamepadSource {
public:
    uint64_t Generation() override {
        return InputCommon::GetSDLPadGeneration();
    }
    void Infos(std::vector<InputCommon::SDLPadInfo>& out) override {
        InputCommon::GetSDLPadInfos(out);
        // Once per change: what came, for the owner's logs (no serials, names and ids only).
        for (const auto& info : out) {
            LOG_INFO(Frontend,
                     "xr: gamepad {:04x}:{:04x} \"{}\" type {} ({}){}{}{}", info.vendor,
                     info.product, info.name, info.type, PadTypeName(PadTypeOf(info)),
                     info.steam_virtual ? ", Steam Input" : "",
                     info.has_touchpad ? ", touchpad" : "",
                     IsFrameOwnPad(info) ? ", the Frame's own: not merged" : "");
        }
        if (out.empty()) {
            LOG_INFO(Frontend, "xr: no gamepads");
        }
    }
    void States(std::vector<InputCommon::SDLPadState>& out) override {
        InputCommon::GetSDLPadStates(out);
    }
    std::string InputDevices() override {
        std::ifstream in("/proc/bus/input/devices");
        if (!in) {
            return {};
        }
        std::ostringstream text;
        text << in.rdbuf();
        return text.str();
    }
};

GamepadStorage SystemStorage() {
    GamepadStorage s;
    s.load = [] {
        std::string text;
        FileUtil::ReadFileToString(true, MappingPath(), text);
        return text;
    };
    s.save = [](const std::string& text) {
        const std::string path = MappingPath();
        const std::string tmp = path + ".new";
        if (FileUtil::WriteStringToFile(true, tmp, text) != text.size() ||
            !FileUtil::Rename(tmp, path)) {
            LOG_ERROR(Frontend, "xr: could not save {}", kMappingFile);
            return false;
        }
        LOG_INFO(Frontend, "xr: saved {}", kMappingFile);
        return true;
    };
    return s;
}

int64_t NowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

GamepadHub& Hub() {
    static SystemSource source;
    static GamepadHub hub(source, SystemStorage());
    static const bool configured = [] {
        const char* env = std::getenv("AZAHARFRAME_PADS");
        if (env && std::string{env} == "0") {
            hub.SetMergeEnabled(false);
            LOG_INFO(Frontend, "xr: gamepads in VR: off (AZAHARFRAME_PADS=0)");
        }
        return true;
    }();
    static_cast<void>(configured);
    return hub;
}

std::vector<DeviceInfo> List() {
    return Hub().Devices(NowNs());
}

uint32_t Revision() {
    return Hub().Revision();
}

GamepadMapping GetMapping(const std::string& id) {
    return Hub().Mapping(id, InputCommon::FrameXr::GetProfileRoutes());
}

GamepadMapping GetDefaultMapping(const std::string& id) {
    return Hub().DefaultMapping(id);
}

bool SetMapping(const std::string& id, const GamepadMapping& mapping) {
    return Hub().SetMapping(id, mapping);
}

bool SetButton(const std::string& id, int button, int input) {
    if (button < 0 || button >= Settings::NativeButton::NumButtons || input < -1 ||
        input >= kNumPadInputs || id == kFrameDeviceId || id == kMouseDeviceId) {
        return false;
    }
    GamepadMapping m = GetMapping(id);
    m.buttons[static_cast<std::size_t>(button)] = static_cast<int8_t>(input);
    return SetMapping(id, m);
}

bool ResetMapping(const std::string& id) {
    return Hub().ResetMapping(id);
}

bool BeginListen(const std::string& id) {
    return Hub().BeginListen(id);
}

GamepadHub::ListenResult PollListen() {
    return Hub().PollListen();
}

void CancelListen() {
    Hub().CancelListen();
}

void SetTouchpadDevice(const std::string& id) {
    Hub().SetTouchpadDevice(id);
}

std::string TouchpadDevice() {
    return Hub().TouchpadDevice();
}

PadSample GetTouchpad(const std::string& id) {
    return Hub().Touchpad(id);
}

} // namespace FrameXr::Input::Devices
