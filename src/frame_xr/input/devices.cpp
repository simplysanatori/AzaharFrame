// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#ifdef __linux__
#include <cerrno>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
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

int64_t NowNs();

std::string ReadInputDevices() {
    std::ifstream in("/proc/bus/input/devices");
    if (!in) {
        return {};
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

/// AzaharFrame padsui: the mice, read from their event nodes (not the window's pointer: a
/// relative mouse needs the counts, and works with no window under it). Every real mouse adds
/// in; so does Frametop's relay mouse, but only while no real mouse reports (Frametop holds the
/// real mice with EVIOCGRAB and, paused for a VR game, feeds them into its virtual mouse; when
/// it does not hold them, the real ones report and its mouse would count twice). Nothing is
/// grabbed: the desktop keeps the mouse.
class MouseReader {
public:
    ~MouseReader() {
        for (Node& n : nodes_) {
            Close(n);
        }
    }

    MouseRaw Read() {
        const int64_t now = NowNs();
        if (!scanned_ || now - scanned_ns_ >= 2'000'000'000 || rescan_) {
            Scan(now);
        }
        raw_.present = present_;
#ifdef __linux__
        bool real_seen = false;
        int64_t relay_dx = 0, relay_dy = 0, relay_up = 0, relay_down = 0;
        for (Node& n : nodes_) {
            if (n.fd < 0) {
                continue;
            }
            input_event ev[64];
            while (true) {
                const ssize_t got = ::read(n.fd, ev, sizeof(ev));
                if (got <= 0) {
                    if (got < 0 && errno != EAGAIN && errno != EINTR) {
                        Close(n); // unplugged
                        rescan_ = true;
                    }
                    break;
                }
                const std::size_t count = static_cast<std::size_t>(got) / sizeof(input_event);
                for (std::size_t i = 0; i < count; i++) {
                    const input_event& e = ev[i];
                    if (e.type == EV_SYN && e.code == SYN_DROPPED) {
                        RefreshButtons(n);
                        continue;
                    }
                    if (!n.relay && (e.type == EV_REL || e.type == EV_KEY)) {
                        real_seen = true;
                    }
                    if (e.type == EV_REL) {
                        int64_t& dx = n.relay ? relay_dx : raw_.x;
                        int64_t& dy = n.relay ? relay_dy : raw_.y;
                        int64_t& up = n.relay ? relay_up : raw_.wheel_up;
                        int64_t& down = n.relay ? relay_down : raw_.wheel_down;
                        if (e.code == REL_X) {
                            dx += e.value;
                        } else if (e.code == REL_Y) {
                            dy += e.value;
                        } else if (e.code == REL_WHEEL && e.value != 0) {
                            if (e.value > 0) {
                                up += e.value;
                            } else {
                                down -= e.value;
                            }
                        }
                    } else if (e.type == EV_KEY && e.code >= BTN_LEFT && e.code <= BTN_EXTRA) {
                        n.buttons[static_cast<std::size_t>(e.code - BTN_LEFT)] = e.value != 0;
                    }
                }
            }
        }
        if (real_seen) {
            real_until_ = now + 500'000'000;
        }
        const bool use_relay = now >= real_until_;
        if (use_relay) {
            raw_.x += relay_dx;
            raw_.y += relay_dy;
            raw_.wheel_up += relay_up;
            raw_.wheel_down += relay_down;
        }
        raw_.buttons = {};
        for (const Node& n : nodes_) {
            if (n.fd < 0 || (n.relay && !use_relay)) {
                continue;
            }
            for (std::size_t b = 0; b < raw_.buttons.size(); b++) {
                raw_.buttons[b] = raw_.buttons[b] || n.buttons[b];
            }
        }
#endif
        return raw_;
    }

private:
    struct Node {
        std::string event;
        bool relay = false;
        int fd = -1;
        std::array<bool, kNumMouseButtons> buttons{};
    };

    static void Close(Node& n) {
#ifdef __linux__
        if (n.fd >= 0) {
            ::close(n.fd);
        }
#endif
        n.fd = -1;
        n.buttons = {};
    }

    static void RefreshButtons(Node& n) {
#ifdef __linux__
        unsigned long keys[(KEY_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
        if (::ioctl(n.fd, EVIOCGKEY(sizeof(keys)), keys) >= 0) {
            for (int b = 0; b < kNumMouseButtons; b++) {
                const int code = BTN_LEFT + b;
                const unsigned long word = keys[code / (8 * sizeof(unsigned long))];
                n.buttons[static_cast<std::size_t>(b)] =
                    (word >> (code % (8 * sizeof(unsigned long)))) & 1UL;
            }
        }
#endif
    }

    void Scan(int64_t now) {
        scanned_ = true;
        scanned_ns_ = now;
        rescan_ = false;
        const std::string text = ReadInputDevices();
        const std::vector<MouseInfo> real = ParseInputDevices(text);
        const std::vector<MouseInfo> relay = ParseRelayMice(text);
        present_ = !real.empty();
        std::vector<Node> want;
        for (const MouseInfo& m : real) {
            if (!m.event.empty()) {
                want.push_back({m.event, false});
            }
        }
        for (const MouseInfo& m : relay) {
            if (!m.event.empty()) {
                want.push_back({m.event, true});
            }
        }
        std::vector<Node> kept;
        for (Node& n : nodes_) {
            const bool still = std::any_of(want.begin(), want.end(), [&n](const Node& w) {
                return w.event == n.event && w.relay == n.relay;
            });
            if (still && n.fd >= 0) {
                kept.push_back(n);
            } else {
                Close(n);
            }
        }
        for (Node& w : want) {
            const bool have = std::any_of(kept.begin(), kept.end(),
                                          [&w](const Node& k) { return k.event == w.event; });
            if (have) {
                continue;
            }
#ifdef __linux__
            const std::string path = "/dev/input/" + w.event;
            w.fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (w.fd < 0) {
                LOG_WARNING(Frontend, "xr: mouse: cannot read {} ({})", path, std::strerror(errno));
                continue;
            }
            RefreshButtons(w);
            LOG_INFO(Frontend, "xr: mouse: reading {}{}", path, w.relay ? " (Frametop's relay)" : "");
#endif
            kept.push_back(w);
        }
        nodes_ = std::move(kept);
    }

    std::vector<Node> nodes_;
    bool scanned_ = false;
    bool rescan_ = false;
    int64_t scanned_ns_ = 0;
    bool present_ = false;
    int64_t real_until_ = 0;
    MouseRaw raw_;
};

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
        return ReadInputDevices();
    }
    MouseRaw Mouse() override {
        return mouse_.Read();
    }

private:
    MouseReader mouse_;
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
    const int inputs = id == kMouseDeviceId ? kNumMouseInputs : kNumPadInputs;
    if (button < 0 || button >= Settings::NativeButton::NumButtons || input < -1 ||
        input >= inputs || id == kFrameDeviceId) {
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
