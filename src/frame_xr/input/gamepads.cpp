// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "frame_xr/input/gamepads.h"

// AzaharFrame pads
namespace FrameXr::Input {

namespace {

using NB = Settings::NativeButton::Values;
using NA = Settings::NativeAnalog::Values;

constexpr int I(PadInput input) {
    return static_cast<int>(input);
}

enum class Family { Xbox, PlayStation, Nintendo, Steam };

Family FamilyOf(PadType type) {
    switch (type) {
    case PadType::PS3:
    case PadType::PS4:
    case PadType::PS5:
        return Family::PlayStation;
    case PadType::SwitchPro:
    case PadType::JoyConLeft:
    case PadType::JoyConRight:
    case PadType::JoyConPair:
        return Family::Nintendo;
    case PadType::Steam:
        return Family::Steam;
    default:
        return Family::Xbox;
    }
}

float Magnitude(Vec2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

// Radial dead zone, rescaled so the edge of the zone is 0 and full tilt stays 1.
Vec2f DeadZone(Vec2f v, float zone) {
    const float m = Magnitude(v);
    if (m <= zone || zone >= 1.0f) {
        return {};
    }
    const float scaled = std::min(1.0f, (m - zone) / (1.0f - zone));
    return {v.x / m * scaled, v.y / m * scaled};
}

constexpr float kTriggerPress = 0.55f;
constexpr float kTriggerRelease = 0.40f;

bool TriggerHeld(float value, bool was) {
    return was ? value > kTriggerRelease : value > kTriggerPress;
}

std::string Hex4(uint16_t v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04x", v);
    return buf;
}

std::string JoinValues(const int8_t* values, std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count; i++) {
        if (i) {
            out += ',';
        }
        out += std::to_string(values[i]);
    }
    return out;
}

// Reads comma-separated numbers into `out`; false (and `out` untouched) unless all fit.
template <std::size_t N>
bool ParseValues(const std::string& text, std::array<int8_t, N>& out, int max) {
    std::array<int8_t, N> values;
    values.fill(-1);
    std::size_t i = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find(',', start), text.size());
        const std::string item = text.substr(start, end - start);
        if (i >= N || item.empty()) {
            return false;
        }
        char* tail = nullptr;
        const long v = std::strtol(item.c_str(), &tail, 10);
        if (tail == item.c_str() || *tail != '\0' || v < -1 || v >= max) {
            return false;
        }
        values[i++] = static_cast<int8_t>(v);
        start = end + 1;
    }
    if (i != N) {
        return false;
    }
    out = values;
    return true;
}

} // namespace

const char* PadTypeName(PadType type) {
    switch (type) {
    case PadType::Xbox360:
        return "Xbox 360";
    case PadType::XboxOne:
        return "Xbox One";
    case PadType::PS3:
        return "DualShock 3";
    case PadType::PS4:
        return "DualShock 4";
    case PadType::PS5:
        return "DualSense";
    case PadType::SwitchPro:
        return "Switch Pro";
    case PadType::JoyConLeft:
        return "Joy-Con (L)";
    case PadType::JoyConRight:
        return "Joy-Con (R)";
    case PadType::JoyConPair:
        return "Joy-Con";
    case PadType::AmazonLuna:
        return "Luna";
    case PadType::GoogleStadia:
        return "Stadia";
    case PadType::NvidiaShield:
        return "Shield";
    case PadType::Steam:
        return "Steam Controller";
    default:
        return "Gamepad";
    }
}

bool NintendoLabels(PadType type) {
    return FamilyOf(type) == Family::Nintendo;
}

const char* PadInputName(PadType type, int input) {
    if (input < 0 || input >= kNumPadInputs) {
        return "";
    }
    const Family f = FamilyOf(type);
    const bool ps = f == Family::PlayStation;
    const bool nin = f == Family::Nintendo;
    switch (static_cast<PadInput>(input)) {
    case PadInput::South:
        return ps ? "Cross" : nin ? "B" : "A";
    case PadInput::East:
        return ps ? "Circle" : nin ? "A" : "B";
    case PadInput::West:
        return ps ? "Square" : nin ? "Y" : "X";
    case PadInput::North:
        return ps ? "Triangle" : nin ? "X" : "Y";
    case PadInput::Back:
        return type == PadType::PS4 ? "Share"
               : type == PadType::PS5 ? "Create"
               : nin                  ? "-"
               : type == PadType::XboxOne || f == Family::Steam ? "View"
                                                                : "Back";
    case PadInput::Guide:
        return ps ? "PS" : nin ? "Home" : f == Family::Steam ? "Steam" : "Guide";
    case PadInput::Start:
        return type == PadType::PS4 || type == PadType::PS5 ? "Options"
               : nin                                         ? "+"
               : type == PadType::XboxOne || f == Family::Steam ? "Menu"
                                                                : "Start";
    case PadInput::LeftStick:
        return ps ? "L3" : "Left Stick Click";
    case PadInput::RightStick:
        return ps ? "R3" : "Right Stick Click";
    case PadInput::LeftShoulder:
        return ps ? "L1" : nin ? "L" : "Left Bumper";
    case PadInput::RightShoulder:
        return ps ? "R1" : nin ? "R" : "Right Bumper";
    case PadInput::DpadUp:
        return "D-Pad Up";
    case PadInput::DpadDown:
        return "D-Pad Down";
    case PadInput::DpadLeft:
        return "D-Pad Left";
    case PadInput::DpadRight:
        return "D-Pad Right";
    case PadInput::Misc1:
        return type == PadType::PS5 ? "Mute" : nin ? "Capture" : "Share";
    case PadInput::Paddle1:
        return f == Family::Steam ? "R4" : "Paddle 1";
    case PadInput::Paddle2:
        return f == Family::Steam ? "L4" : "Paddle 2";
    case PadInput::Paddle3:
        return f == Family::Steam ? "R5" : "Paddle 3";
    case PadInput::Paddle4:
        return f == Family::Steam ? "L5" : "Paddle 4";
    case PadInput::Touchpad:
        return "Touchpad Click";
    case PadInput::LeftTrigger:
        return ps ? "L2" : nin ? "ZL" : "Left Trigger";
    case PadInput::RightTrigger:
        return ps ? "R2" : nin ? "ZR" : "Right Trigger";
    default:
        return "";
    }
}

const char* PadStickName(int stick) {
    return stick == 0 ? "Left Stick" : stick == 1 ? "Right Stick" : "";
}

bool ReportsByLabel(PadType type, bool steam_virtual) {
    return NintendoLabels(type) && !steam_virtual;
}

PadInputs PadInputsFromRaw(const InputCommon::SDLPadState& raw, const PadInputs& previous,
                           bool by_label, float stick_deadzone) {
    PadInputs out;
    for (int b = 0; b < InputCommon::SDLPadState::kButtons; b++) {
        out.pressed[static_cast<std::size_t>(b)] = raw.buttons[static_cast<std::size_t>(b)];
    }
    if (by_label) {
        // SDL's A is the button marked A, on the right; its X the top one.
        out.pressed[I(PadInput::South)] = raw.buttons[1];
        out.pressed[I(PadInput::East)] = raw.buttons[0];
        out.pressed[I(PadInput::West)] = raw.buttons[3];
        out.pressed[I(PadInput::North)] = raw.buttons[2];
    }
    out.pressed[I(PadInput::LeftTrigger)] =
        TriggerHeld(raw.axes[4], previous.Get(PadInput::LeftTrigger));
    out.pressed[I(PadInput::RightTrigger)] =
        TriggerHeld(raw.axes[5], previous.Get(PadInput::RightTrigger));
    out.sticks[0] = DeadZone({raw.axes[0], -raw.axes[1]}, stick_deadzone);
    out.sticks[1] = DeadZone({raw.axes[2], -raw.axes[3]}, stick_deadzone);
    return out;
}

GamepadMapping EmptyMapping() {
    GamepadMapping m;
    m.buttons.fill(-1);
    m.analogs.fill(-1);
    return m;
}

GamepadMapping DefaultGamepadMapping(PadType type, bool by_position) {
    GamepadMapping m = EmptyMapping();
    const auto set = [&m](NB button, PadInput input) {
        m.buttons[button] = static_cast<int8_t>(input);
    };
    if (by_position || NintendoLabels(type)) {
        set(NB::A, PadInput::East);
        set(NB::B, PadInput::South);
        set(NB::X, PadInput::North);
        set(NB::Y, PadInput::West);
    } else {
        set(NB::A, PadInput::South);
        set(NB::B, PadInput::East);
        set(NB::X, PadInput::West);
        set(NB::Y, PadInput::North);
    }
    set(NB::Up, PadInput::DpadUp);
    set(NB::Down, PadInput::DpadDown);
    set(NB::Left, PadInput::DpadLeft);
    set(NB::Right, PadInput::DpadRight);
    set(NB::L, PadInput::LeftShoulder);
    set(NB::R, PadInput::RightShoulder);
    set(NB::ZL, PadInput::LeftTrigger);
    set(NB::ZR, PadInput::RightTrigger);
    set(NB::Start, PadInput::Start);
    set(NB::Select, PadInput::Back);
    m.analogs[NA::CirclePad] = static_cast<int8_t>(PadStick::Left);
    m.analogs[NA::CStick] = static_cast<int8_t>(PadStick::Right);
    return m;
}

GamepadMapping DefaultMouseMapping() {
    GamepadMapping m = EmptyMapping();
    m.buttons[NB::A] = static_cast<int8_t>(MouseInput::RightClick);
    m.buttons[NB::L] = static_cast<int8_t>(MouseInput::Back);
    m.buttons[NB::R] = static_cast<int8_t>(MouseInput::Forward);
    m.touch = static_cast<int8_t>(MouseInput::LeftClick);
    m.open_menu = static_cast<int8_t>(MouseInput::MiddleClick);
    return m;
}

ThreeDsPadState ApplyMouseMapping(const std::array<bool, kNumMouseInputs>& held,
                                  const GamepadMapping& mapping) {
    ThreeDsPadState out;
    for (std::size_t b = 0; b < mapping.buttons.size(); b++) {
        const int input = mapping.buttons[b];
        if (input >= 0 && input < kNumMouseInputs && held[static_cast<std::size_t>(input)]) {
            out.buttons[b] = true;
        }
    }
    return out;
}

ThreeDsPadState ApplyGamepadMapping(const PadInputs& pad, const GamepadMapping& mapping) {
    ThreeDsPadState out;
    for (std::size_t b = 0; b < mapping.buttons.size(); b++) {
        const int input = mapping.buttons[b];
        if (input >= 0 && input < kNumPadInputs && pad.pressed[static_cast<std::size_t>(input)]) {
            out.buttons[b] = true;
        }
    }
    for (std::size_t a = 0; a < mapping.analogs.size(); a++) {
        // AzaharFrame padsui: a stick route (stick_route.h): the stick, its axes as pushed.
        const StickRoute route = DecodeStickRoute(mapping.analogs[a]);
        if (route.stick >= 0 && route.stick < kNumPadSticks) {
            out.analogs[a] = ApplyStickRoute(pad.sticks[static_cast<std::size_t>(route.stick)], route);
        }
    }
    return out;
}

void MergeIntoFrame(const ThreeDsPadState& pad, const ProfileRoutes& routes,
                    ControlState& controls) {
    for (std::size_t b = 0; b < pad.buttons.size(); b++) {
        const int c = routes.buttons[b];
        if (pad.buttons[b] && c >= 0 && c < kNumControls) {
            controls.buttons[static_cast<std::size_t>(c)] = true;
        }
    }
    for (std::size_t a = 0; a < pad.analogs.size(); a++) {
        // AzaharFrame padsui: the profile's route may turn the Frame stick's axes; the pad's
        // 3DS value goes in so that the route gives it back.
        const StickRoute route = DecodeStickRoute(routes.analogs[a]);
        const int c = route.stick;
        if (c < 0 || c >= kNumAnalogControls) {
            continue;
        }
        Vec2f& frame = controls.sticks[static_cast<std::size_t>(c)];
        if (Magnitude(pad.analogs[a]) > Magnitude(frame)) {
            frame = UnapplyStickRoute(pad.analogs[a], route);
        }
    }
}

GamepadMapping MappingOfRoutes(const ProfileRoutes& routes) {
    GamepadMapping m;
    m.buttons = routes.buttons;
    m.analogs = routes.analogs;
    return m;
}

int ControlOfPadInput(int input, bool by_label) {
    switch (static_cast<PadInput>(input)) {
    case PadInput::South:
        return static_cast<int>(by_label ? Control::B : Control::A);
    case PadInput::East:
        return static_cast<int>(by_label ? Control::A : Control::B);
    case PadInput::West:
        return static_cast<int>(by_label ? Control::Y : Control::X);
    case PadInput::North:
        return static_cast<int>(by_label ? Control::X : Control::Y);
    case PadInput::Back:
        return static_cast<int>(Control::View);
    case PadInput::Start:
        return static_cast<int>(Control::Menu);
    case PadInput::LeftStick:
        return static_cast<int>(Control::StickClickL);
    case PadInput::RightStick:
        return static_cast<int>(Control::StickClickR);
    case PadInput::LeftShoulder:
        return static_cast<int>(Control::BumperL);
    case PadInput::RightShoulder:
        return static_cast<int>(Control::BumperR);
    case PadInput::DpadUp:
        return static_cast<int>(Control::DpadUp);
    case PadInput::DpadDown:
        return static_cast<int>(Control::DpadDown);
    case PadInput::DpadLeft:
        return static_cast<int>(Control::DpadLeft);
    case PadInput::DpadRight:
        return static_cast<int>(Control::DpadRight);
    case PadInput::Paddle1:
    case PadInput::Paddle3:
        return static_cast<int>(Control::GripR);
    case PadInput::Paddle2:
    case PadInput::Paddle4:
        return static_cast<int>(Control::GripL);
    case PadInput::LeftTrigger:
        return static_cast<int>(Control::TriggerL);
    case PadInput::RightTrigger:
        return static_cast<int>(Control::TriggerR);
    default:
        return -1;
    }
}

int PadInputOfControl(int control, bool by_label) {
    if (control < 0 || control >= kNumControls) {
        return -1;
    }
    for (const PadInput input : {PadInput::South, PadInput::East, PadInput::West,
                                 PadInput::North, PadInput::Back, PadInput::Start,
                                 PadInput::LeftStick, PadInput::RightStick,
                                 PadInput::LeftShoulder, PadInput::RightShoulder,
                                 PadInput::DpadUp, PadInput::DpadDown, PadInput::DpadLeft,
                                 PadInput::DpadRight, PadInput::Paddle1, PadInput::Paddle2,
                                 PadInput::LeftTrigger, PadInput::RightTrigger}) {
        if (ControlOfPadInput(I(input), by_label) == control) {
            return I(input);
        }
    }
    return -1;
}

ButtonTable ControlTableOf(const GamepadMapping& mapping, bool by_label) {
    ButtonTable t;
    for (std::size_t b = 0; b < t.size(); b++) {
        t[b] = static_cast<int8_t>(ControlOfPadInput(mapping.buttons[b], by_label));
    }
    return t;
}

GamepadMapping WithControlTable(GamepadMapping mapping, const ButtonTable& table, bool by_label) {
    const ButtonTable before = ControlTableOf(mapping, by_label);
    for (std::size_t b = 0; b < table.size(); b++) {
        if (table[b] != before[b]) {
            mapping.buttons[b] = static_cast<int8_t>(PadInputOfControl(table[b], by_label));
        }
    }
    return mapping;
}

bool IsValveGamepad(uint16_t vendor, uint16_t product) {
    if (vendor != kValveVendor) {
        return false;
    }
    switch (product) {
    case 0x1101: // Steam Controller (legacy)
    case 0x1102: // Steam Controller, wired
    case 0x1105: // Steam Controller, Bluetooth
    case 0x1106:
    case 0x1142: // Steam Controller dongle
    case 0x1201: // Steam Controller V2
    case 0x1202:
    case 0x1205: // Steam Deck
    case 0x1302: // Steam Controller (2026)
    case 0x1303: // its Bluetooth
    case 0x1304: // its dongles
    case 0x1305:
        return true;
    default:
        return false;
    }
}

bool IsFrameOwnPad(const InputCommon::SDLPadInfo& info) {
    // 28de:11ff is Steam's virtual pad with no word of what is behind it: unknown (EchoGuard).
    return info.vendor == kValveVendor && info.product != 0x11ff &&
           !IsValveGamepad(info.vendor, info.product);
}

PadType PadTypeOf(const InputCommon::SDLPadInfo& info) {
    if (IsValveGamepad(info.vendor, info.product)) {
        return PadType::Steam;
    }
    if (info.type >= 0 && info.type <= static_cast<int>(PadType::JoyConPair)) {
        return static_cast<PadType>(info.type);
    }
    return PadType::Unknown;
}

std::string GamepadKey(const InputCommon::SDLPadInfo& info) {
    if (info.steam_virtual) {
        return "steam:" + Hex4(info.vendor) + ":" + Hex4(info.product);
    }
    if (!info.guid.empty()) {
        return info.guid;
    }
    return "pad:" + Hex4(info.vendor) + ":" + Hex4(info.product);
}

// ---- Pointer devices --------------------------------------------------------------------

namespace {

template <typename Keep>
std::vector<MouseInfo> ParseMice(const std::string& text, Keep keep) {
    std::vector<MouseInfo> out;
    std::istringstream in(text);
    std::string line;
    MouseInfo cur;
    bool mouse_handler = false;
    bool rel_xy = false;
    bool any = false;
    const auto finish = [&] {
        if (any && mouse_handler && rel_xy && keep(cur)) {
            out.push_back(cur);
        }
        cur = {};
        mouse_handler = rel_xy = any = false;
    };
    while (std::getline(in, line)) {
        if (line.empty()) {
            finish();
            continue;
        }
        any = true;
        if (line.rfind("I:", 0) == 0) {
            unsigned bus = 0, vendor = 0, product = 0;
            const auto field = [&line](const char* key, unsigned& v) {
                const std::size_t p = line.find(key);
                if (p != std::string::npos) {
                    v = static_cast<unsigned>(std::strtoul(line.c_str() + p +
                                                           std::char_traits<char>::length(key),
                                                           nullptr, 16));
                }
            };
            field("Bus=", bus);
            field("Vendor=", vendor);
            field("Product=", product);
            cur.bus = static_cast<uint16_t>(bus);
            cur.vendor = static_cast<uint16_t>(vendor);
            cur.product = static_cast<uint16_t>(product);
        } else if (line.rfind("N: Name=", 0) == 0) {
            std::string name = line.substr(8);
            if (name.size() >= 2 && name.front() == '"' && name.back() == '"') {
                name = name.substr(1, name.size() - 2);
            }
            cur.name = name;
        } else if (line.rfind("H: Handlers=", 0) == 0) {
            std::istringstream words(line.substr(12));
            std::string w;
            while (words >> w) {
                if (w.rfind("mouse", 0) == 0) {
                    mouse_handler = true;
                } else if (w.rfind("event", 0) == 0) {
                    cur.event = w;
                }
            }
        } else if (line.rfind("B: REL=", 0) == 0) {
            // REL_X is bit 0, REL_Y bit 1 of the last (lowest) word.
            const std::string bits = line.substr(7);
            const std::size_t sp = bits.find_last_of(' ');
            const std::string low = sp == std::string::npos ? bits : bits.substr(sp + 1);
            const unsigned long v = std::strtoul(low.c_str(), nullptr, 16);
            rel_xy = (v & 0x3) == 0x3;
        }
    }
    finish();
    return out;
}

} // namespace

std::vector<MouseInfo> ParseInputDevices(const std::string& text) {
    return ParseMice(text, [](const MouseInfo& m) {
        return (m.bus == 0x03 || m.bus == 0x05) && m.vendor != kValveVendor;
    });
}

std::vector<MouseInfo> ParseRelayMice(const std::string& text) {
    return ParseMice(text, [](const MouseInfo& m) {
        return m.bus == 0x06 && m.name.rfind("frametop virtual mouse", 0) == 0;
    });
}

// ---- EchoGuard ----------------------------------------------------------------------------

std::array<bool, kNumPadInputs> FrameAsPad(const InputFrame& frame) {
    std::array<bool, kNumPadInputs> p{};
    p[I(PadInput::South)] = frame.a;
    p[I(PadInput::East)] = frame.b;
    p[I(PadInput::West)] = frame.x;
    p[I(PadInput::North)] = frame.y;
    p[I(PadInput::Start)] = frame.menu;
    p[I(PadInput::Back)] = frame.view;
    p[I(PadInput::DpadUp)] = frame.dpad_up;
    p[I(PadInput::DpadDown)] = frame.dpad_down;
    p[I(PadInput::DpadLeft)] = frame.dpad_left;
    p[I(PadInput::DpadRight)] = frame.dpad_right;
    p[I(PadInput::LeftShoulder)] = frame.hand[0].bumper;
    p[I(PadInput::RightShoulder)] = frame.hand[1].bumper;
    p[I(PadInput::LeftStick)] = frame.hand[0].stick_click;
    p[I(PadInput::RightStick)] = frame.hand[1].stick_click;
    p[I(PadInput::LeftTrigger)] = frame.hand[0].trigger > kTriggerRelease;
    p[I(PadInput::RightTrigger)] = frame.hand[1].trigger > kTriggerRelease;
    return p;
}

std::array<bool, kNumPadInputs> EchoGuard::Filter(const std::array<bool, kNumPadInputs>& frame,
                                                  const std::array<bool, kNumPadInputs>& pad,
                                                  int64_t now_ns) {
    std::array<bool, kNumPadInputs> out{};
    const auto stamp = [now_ns] { return now_ns != 0 ? now_ns : 1; };
    for (int i = 0; i < kNumPadInputs; i++) {
        const std::size_t k = static_cast<std::size_t>(i);
        if (frame[k] && !frame_prev_[k]) {
            frame_down_ns_[k] = stamp();
        }
        const bool pad_edge = pad[k] && !pad_prev_[k];
        if (!pad[k]) {
            pending_ns_[k] = 0;
            passing_[k] = false;
        }
        if (verdict_ == Verdict::Echo) {
            continue;
        }
        if (verdict_ == Verdict::Real) {
            out[k] = pad[k];
            continue;
        }
        if (pad_edge) {
            // The Frame pressed it a moment ago: the same press.
            if (frame_down_ns_[k] != 0 && now_ns - frame_down_ns_[k] <= kWindowNs) {
                coincident_++;
                pending_ns_[k] = 0;
            } else {
                pending_ns_[k] = stamp();
            }
        }
        if (pending_ns_[k] != 0) {
            if (frame[k] && frame_down_ns_[k] >= pending_ns_[k]) {
                // The Frame followed within the window: the same press after all.
                coincident_++;
                pending_ns_[k] = 0;
            } else if (now_ns - pending_ns_[k] > kWindowNs) {
                independent_++;
                pending_ns_[k] = 0;
                passing_[k] = true;
            }
        }
        out[k] = passing_[k] && pad[k];
    }
    pad_prev_ = pad;
    frame_prev_ = frame;
    if (verdict_ == Verdict::Unknown) {
        if (independent_ >= kVerdictPresses) {
            verdict_ = Verdict::Real;
            out = pad;
        } else if (coincident_ >= kVerdictPresses && independent_ == 0) {
            verdict_ = Verdict::Echo;
            out = {};
        }
    }
    return out;
}

// ---- MappingFile --------------------------------------------------------------------------

void MappingFile::Parse(const std::string& text) {
    entries_.clear();
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::size_t sp = line.find(' ');
        if (sp == std::string::npos || sp == 0) {
            continue;
        }
        const std::string key = line.substr(0, sp);
        std::string rest = line.substr(sp + 1);
        MappingEntry e;
        e.mapping = EmptyMapping();
        bool have_buttons = false;
        bool have_analogs = false;
        // name= runs to the end of the line.
        const std::size_t name_at = rest.find("name=");
        if (name_at != std::string::npos) {
            e.name = rest.substr(name_at + 5);
            rest = rest.substr(0, name_at);
        }
        std::istringstream words(rest);
        std::string w;
        while (words >> w) {
            const std::size_t eq = w.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            const std::string k = w.substr(0, eq);
            const std::string v = w.substr(eq + 1);
            if (k == "buttons") {
                have_buttons = ParseValues(v, e.mapping.buttons, kNumPadInputs);
            } else if (k == "analogs") {
                // AzaharFrame padsui: stick routes (0 and 1 as before, flags above them).
                have_analogs = ParseValues(v, e.mapping.analogs, 32);
                for (int8_t& a : e.mapping.analogs) {
                    if (DecodeStickRoute(a).stick >= kNumPadSticks) {
                        a = -1;
                    }
                }
            } else if (k == "touch" || k == "menu") {
                // AzaharFrame padsui: the mouse's touch and Open Menu inputs.
                const long n = std::strtol(v.c_str(), nullptr, 10);
                const int8_t value = n >= -1 && n < kNumMouseInputs ? static_cast<int8_t>(n) : -1;
                (k == "touch" ? e.mapping.touch : e.mapping.open_menu) = value;
            } else if (k == "mode") {
                e.mapping.mode = v == "1" ? 1 : 0;
            } else if (k == "speed") {
                const long n = std::strtol(v.c_str(), nullptr, 10);
                e.mapping.speed = static_cast<uint8_t>(std::clamp<long>(n, 0, kNumCursorSpeeds - 1));
            } else if (k == "recentre") {
                e.mapping.recentre = v != "0";
            } else if (k == "use") {
                e.mapping.enabled = v != "0";
            } else if (k == "vendor") {
                e.vendor = static_cast<uint16_t>(std::strtoul(v.c_str(), nullptr, 16));
            } else if (k == "product") {
                e.product = static_cast<uint16_t>(std::strtoul(v.c_str(), nullptr, 16));
            }
        }
        if (have_buttons && have_analogs) {
            entries_[key] = std::move(e);
        }
    }
}

std::string MappingFile::Format() const {
    std::string out = "# Azahar for Frame: 3DS mappings of other controllers (Edit Controls).\n"
                      "# <key> buttons=<3DS A,B,X,Y,Up,Down,Left,Right,L,R,Start,Select,Debug,"
                      "Gpio14,ZL,ZR,Home,Power> analogs=<Circle Pad,C-Stick> vendor= product= "
                      "name=\n";
    for (const auto& [key, e] : entries_) {
        out += key;
        out += " buttons=" + JoinValues(e.mapping.buttons.data(), e.mapping.buttons.size());
        out += " analogs=" + JoinValues(e.mapping.analogs.data(), e.mapping.analogs.size());
        // AzaharFrame padsui: what is not the plain value (MappingFile in gamepads.h).
        const GamepadMapping plain;
        if (e.mapping.touch != plain.touch) {
            out += " touch=" + std::to_string(e.mapping.touch);
        }
        if (e.mapping.open_menu != plain.open_menu) {
            out += " menu=" + std::to_string(e.mapping.open_menu);
        }
        if (e.mapping.mode != plain.mode) {
            out += " mode=" + std::to_string(e.mapping.mode);
        }
        if (e.mapping.speed != plain.speed) {
            out += " speed=" + std::to_string(e.mapping.speed);
        }
        if (e.mapping.recentre != plain.recentre) {
            out += " recentre=0";
        }
        if (e.mapping.enabled != plain.enabled) {
            out += " use=0";
        }
        out += " vendor=" + Hex4(e.vendor) + " product=" + Hex4(e.product);
        out += " name=" + e.name + "\n";
    }
    return out;
}

const MappingEntry* MappingFile::Find(const std::string& key, uint16_t vendor, uint16_t product,
                                      const std::string& name) const {
    if (const auto it = entries_.find(key); it != entries_.end()) {
        return &it->second;
    }
    if (vendor == 0 && product == 0) {
        return nullptr;
    }
    for (const auto& [k, e] : entries_) {
        if (e.vendor == vendor && e.product == product && e.name == name) {
            return &e;
        }
    }
    return nullptr;
}

void MappingFile::Set(const std::string& key, const MappingEntry& entry) {
    entries_[key] = entry;
}

bool MappingFile::Remove(const std::string& key, uint16_t vendor, uint16_t product,
                         const std::string& name) {
    bool removed = entries_.erase(key) > 0;
    if (vendor != 0 || product != 0) {
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second.vendor == vendor && it->second.product == product &&
                it->second.name == name) {
                it = entries_.erase(it);
                removed = true;
            } else {
                ++it;
            }
        }
    }
    return removed;
}

// ---- GamepadHub ---------------------------------------------------------------------------

GamepadHub::GamepadHub(GamepadSource& source, GamepadStorage storage)
    : source_(source), storage_(std::move(storage)) {}

void GamepadHub::LoadLocked() {
    if (loaded_) {
        return;
    }
    loaded_ = true;
    if (storage_.load) {
        file_.Parse(storage_.load());
    }
}

bool GamepadHub::SaveLocked() {
    return storage_.save && storage_.save(file_.Format());
}

void GamepadHub::RefreshLocked() {
    const uint64_t generation = source_.Generation();
    if (generation_seen_ && generation == generation_) {
        return;
    }
    generation_seen_ = true;
    generation_ = generation;
    source_.Infos(infos_);
    for (Pad& pad : pads_) {
        pad.connected = false;
    }
    for (const auto& info : infos_) {
        auto it = std::find_if(pads_.begin(), pads_.end(), [&info](const Pad& p) {
            return p.info.id == info.id && p.info.guid == info.guid;
        });
        const std::string key = GamepadKey(info);
        if (it == pads_.end()) {
            // A pad that comes back takes its old entry (and place in the list).
            it = std::find_if(pads_.begin(), pads_.end(), [&key](const Pad& p) {
                return !p.connected && p.key == key;
            });
        }
        if (it == pads_.end()) {
            Pad pad;
            pad.key = key;
            pad.order = next_order_++;
            pads_.push_back(std::move(pad));
            it = pads_.end() - 1;
        }
        Pad& pad = *it;
        const bool was_connected = pad.info.id == info.id && pad.connected;
        pad.info = info;
        pad.key = key;
        pad.type = PadTypeOf(info);
        pad.frame_own = IsFrameOwnPad(info);
        pad.by_label = ReportsByLabel(pad.type, info.steam_virtual);
        pad.connected = true;
        if (!was_connected) {
            pad.inputs = {};
            pad.raw = {};
            pad.swallowed.fill(true); // what is held as it comes is no press
        }
    }
    // Unique ids: a second pad of one model is "<key>#2".
    for (Pad& pad : pads_) {
        int same = 0;
        for (const Pad& other : pads_) {
            if (&other == &pad) {
                break;
            }
            if (other.key == pad.key) {
                same++;
            }
        }
        pad.id = same == 0 ? pad.key : pad.key + "#" + std::to_string(same + 1);
    }
    revision_++;
}

void GamepadHub::ReadStatesLocked() {
    source_.States(states_);
    for (const auto& state : states_) {
        auto it = std::find_if(pads_.begin(), pads_.end(), [&state](const Pad& p) {
            return p.connected && p.info.id == state.id;
        });
        if (it == pads_.end()) {
            continue;
        }
        Pad& pad = *it;
        pad.inputs = PadInputsFromRaw(state, pad.inputs, pad.by_label);
        for (int b = 0; b < InputCommon::SDLPadState::kButtons; b++) {
            pad.raw[static_cast<std::size_t>(b)] = state.buttons[static_cast<std::size_t>(b)];
        }
        pad.raw[I(PadInput::LeftTrigger)] = pad.inputs.Get(PadInput::LeftTrigger);
        pad.raw[I(PadInput::RightTrigger)] = pad.inputs.Get(PadInput::RightTrigger);
        pad.touch = state.touchpad;
    }
}

GamepadHub::Pad* GamepadHub::FindLocked(const std::string& id) {
    for (Pad& pad : pads_) {
        if (pad.id == id && !pad.frame_own) {
            return &pad;
        }
    }
    return nullptr;
}

bool GamepadHub::ListedLocked(const Pad& pad) const {
    return !pad.frame_own && pad.echo.verdict() != EchoGuard::Verdict::Echo;
}

GamepadMapping GamepadHub::DefaultLocked(const Pad& pad) const {
    return DefaultGamepadMapping(pad.type, by_position_);
}

GamepadMapping GamepadHub::MappingLocked(const Pad& pad) {
    LoadLocked();
    if (const MappingEntry* e =
            file_.Find(pad.key, pad.info.vendor, pad.info.product, pad.info.name)) {
        return e->mapping;
    }
    return DefaultLocked(pad);
}

std::vector<DeviceInfo> GamepadHub::Devices(int64_t now_ns) {
    std::scoped_lock lock{mutex_};
    LoadLocked();
    RefreshLocked();
    std::vector<DeviceInfo> out;
    {
        DeviceInfo frame;
        frame.kind = DeviceKind::Frame;
        frame.id = kFrameDeviceId;
        frame.name = "Frame Controllers";
        out.push_back(frame);
    }
    std::vector<const Pad*> listed;
    for (const Pad& pad : pads_) {
        if (ListedLocked(pad)) {
            listed.push_back(&pad);
        }
    }
    std::stable_sort(listed.begin(), listed.end(), [](const Pad* a, const Pad* b) {
        if (a->connected != b->connected) {
            return a->connected;
        }
        return a->order < b->order;
    });
    for (const Pad* pad : listed) {
        DeviceInfo d;
        d.kind = DeviceKind::Gamepad;
        d.id = pad->id;
        d.name = pad->info.name.empty() ? PadTypeName(pad->type) : pad->info.name;
        d.type = pad->type;
        d.vendor = pad->info.vendor;
        d.product = pad->info.product;
        d.has_touchpad = pad->info.has_touchpad;
        d.connected = pad->connected;
        d.steam_input = pad->info.steam_virtual;
        d.custom =
            file_.Find(pad->key, pad->info.vendor, pad->info.product, pad->info.name) != nullptr;
        out.push_back(std::move(d));
    }
    if (!mice_read_ || now_ns - mice_read_ns_ >= 2'000'000'000 || now_ns < mice_read_ns_) {
        std::vector<MouseInfo> mice = ParseInputDevices(source_.InputDevices());
        const bool changed =
            mice.size() != mice_.size() ||
            !std::equal(mice.begin(), mice.end(), mice_.begin(), [](const auto& a, const auto& b) {
                return a.name == b.name && a.vendor == b.vendor && a.product == b.product;
            });
        mice_ = std::move(mice);
        mice_read_ = true;
        mice_read_ns_ = now_ns;
        if (changed) {
            revision_++;
        }
    }
    if (!mice_.empty()) {
        DeviceInfo mouse;
        mouse.kind = DeviceKind::Mouse;
        mouse.id = kMouseDeviceId;
        mouse.name = mice_.size() == 1 && !mice_[0].name.empty() ? mice_[0].name : "Mouse";
        mouse.vendor = mice_[0].vendor;
        mouse.product = mice_[0].product;
        out.push_back(std::move(mouse));
    }
    return out;
}

uint32_t GamepadHub::Revision() {
    std::scoped_lock lock{mutex_};
    RefreshLocked();
    return revision_;
}

GamepadMapping GamepadHub::MouseMappingLocked() {
    LoadLocked();
    if (const MappingEntry* e = file_.Find(kMouseDeviceId, 0, 0, "")) {
        return e->mapping;
    }
    return DefaultMouseMapping();
}

bool GamepadHub::SetMouseMappingLocked(const GamepadMapping& mapping) {
    LoadLocked();
    file_.Remove(kMouseDeviceId, 0, 0, "");
    if (!(mapping == DefaultMouseMapping())) {
        file_.Set(kMouseDeviceId, {mapping, 0, 0, "Mouse"});
    }
    revision_++;
    return SaveLocked();
}

GamepadMapping GamepadHub::Mapping(const std::string& id, const ProfileRoutes& routes) {
    if (id == kFrameDeviceId) {
        return MappingOfRoutes(routes);
    }
    std::scoped_lock lock{mutex_};
    if (id == kMouseDeviceId) {
        return MouseMappingLocked();
    }
    RefreshLocked();
    const Pad* pad = FindLocked(id);
    return pad ? MappingLocked(*pad) : EmptyMapping();
}

GamepadMapping GamepadHub::DefaultMapping(const std::string& id) {
    if (id == kMouseDeviceId) {
        return DefaultMouseMapping();
    }
    std::scoped_lock lock{mutex_};
    RefreshLocked();
    const Pad* pad = FindLocked(id);
    return pad ? DefaultLocked(*pad) : EmptyMapping();
}

bool GamepadHub::SetMapping(const std::string& id, const GamepadMapping& mapping) {
    std::scoped_lock lock{mutex_};
    if (id == kMouseDeviceId) {
        return SetMouseMappingLocked(mapping);
    }
    LoadLocked();
    RefreshLocked();
    const Pad* pad = FindLocked(id);
    if (!pad) {
        return false;
    }
    if (mapping == DefaultLocked(*pad)) {
        file_.Remove(pad->key, pad->info.vendor, pad->info.product, pad->info.name);
    } else {
        // One entry per model: the key now, and no stale entry under another bus.
        file_.Remove(pad->key, pad->info.vendor, pad->info.product, pad->info.name);
        file_.Set(pad->key, {mapping, pad->info.vendor, pad->info.product, pad->info.name});
    }
    revision_++;
    return SaveLocked();
}

bool GamepadHub::ResetMapping(const std::string& id) {
    std::scoped_lock lock{mutex_};
    LoadLocked();
    if (id == kMouseDeviceId) {
        if (!file_.Remove(kMouseDeviceId, 0, 0, "")) {
            return false;
        }
        revision_++;
        SaveLocked();
        return true;
    }
    RefreshLocked();
    const Pad* pad = FindLocked(id);
    if (!pad || !file_.Remove(pad->key, pad->info.vendor, pad->info.product, pad->info.name)) {
        return false;
    }
    revision_++;
    SaveLocked();
    return true;
}

bool GamepadHub::BeginListen(const std::string& id) {
    std::scoped_lock lock{mutex_};
    if (id == kMouseDeviceId) {
        // AzaharFrame padsui: the mouse's next click, button or wheel notch.
        listen_mouse_ = source_.Mouse();
        if (!listen_mouse_.present) {
            return false;
        }
        listen_id_ = id;
        listen_is_mouse_ = true;
        listening_ = true;
        listen_result_ = {ListenResult::State::Listening, -1};
        return true;
    }
    listen_is_mouse_ = false;
    RefreshLocked();
    Pad* pad = FindLocked(id);
    if (!pad || !pad->connected) {
        return false;
    }
    ReadStatesLocked();
    listen_id_ = id;
    listening_ = true;
    listen_held_ = pad->inputs.pressed;
    listen_result_ = {ListenResult::State::Listening, -1};
    return true;
}

void GamepadHub::CancelListen() {
    std::scoped_lock lock{mutex_};
    listening_ = false;
    listen_id_.clear();
    listen_result_ = {};
}

GamepadHub::ListenResult GamepadHub::PollListen() {
    std::scoped_lock lock{mutex_};
    if (!listening_) {
        const ListenResult r = listen_result_;
        listen_result_ = {};
        return r;
    }
    if (listen_is_mouse_) {
        const MouseRaw now = source_.Mouse();
        if (!now.present) {
            listening_ = false;
            listen_result_ = {};
            return {ListenResult::State::Gone, -1};
        }
        int input = -1;
        for (int b = 0; b < kNumMouseButtons && input < 0; b++) {
            const std::size_t k = static_cast<std::size_t>(b);
            if (!now.buttons[k]) {
                listen_mouse_.buttons[k] = false;
            } else if (!listen_mouse_.buttons[k]) {
                input = b;
            }
        }
        if (input < 0 && now.wheel_up > listen_mouse_.wheel_up) {
            input = static_cast<int>(MouseInput::WheelUp);
        } else if (input < 0 && now.wheel_down > listen_mouse_.wheel_down) {
            input = static_cast<int>(MouseInput::WheelDown);
        }
        listen_mouse_.wheel_up = now.wheel_up;
        listen_mouse_.wheel_down = now.wheel_down;
        if (input >= 0) {
            listening_ = false;
            listen_result_ = {};
            mouse_logic_.Swallow(input); // held until let go: never the game's
            return {ListenResult::State::Pressed, input};
        }
        return {ListenResult::State::Listening, -1};
    }
    RefreshLocked();
    Pad* pad = FindLocked(listen_id_);
    if (!pad || !pad->connected) {
        listening_ = false;
        listen_result_ = {};
        return {ListenResult::State::Gone, -1};
    }
    ReadStatesLocked();
    for (int i = 0; i < kNumPadInputs; i++) {
        const std::size_t k = static_cast<std::size_t>(i);
        const bool now = pad->inputs.pressed[k];
        if (!now) {
            listen_held_[k] = false;
        } else if (!listen_held_[k]) {
            listening_ = false;
            listen_result_ = {};
            // Held until let go: the press never reaches the game when the menu closes.
            pad->swallowed[k] = true;
            return {ListenResult::State::Pressed, i};
        }
    }
    ListenResult r{ListenResult::State::Listening, -1};
    r.sticks = pad->inputs.sticks; // AzaharFrame padsui
    return r;
}

void GamepadHub::SetTouchpadDevice(const std::string& id) {
    std::scoped_lock lock{mutex_};
    touchpad_id_ = id;
}

std::string GamepadHub::TouchpadDevice() {
    std::scoped_lock lock{mutex_};
    return touchpad_id_;
}

PadSample GamepadHub::Touchpad(const std::string& id) {
    std::scoped_lock lock{mutex_};
    RefreshLocked();
    ReadStatesLocked();
    const Pad* pad = FindLocked(id);
    if (!pad || !pad->connected || !pad->touch.present) {
        return {};
    }
    return {true, pad->touch.contact, pad->touch.click, pad->touch.x, pad->touch.y};
}

void GamepadHub::SetFaceButtonsByPosition(bool by_position) {
    std::scoped_lock lock{mutex_};
    if (by_position_ != by_position) {
        by_position_ = by_position;
        revision_++;
    }
}

void GamepadHub::SetMergeEnabled(bool enabled) {
    std::scoped_lock lock{mutex_};
    merge_enabled_ = enabled;
}

TouchpadPick GamepadHub::PickTouchpadLocked() {
    const auto sample = [this](const Pad& p) -> TouchpadPick {
        return {{true, p.touch.contact, p.touch.click, p.touch.x, p.touch.y}, p.id,
                MappingLocked(p).recentre};
    };
    const auto usable = [this](const Pad& p) {
        return p.connected && !p.frame_own && p.touch.present && MappingLocked(p).enabled;
    };
    // The menu's choice, while it is connected.
    if (!touchpad_id_.empty()) {
        for (const Pad& p : pads_) {
            if (p.id == touchpad_id_ && usable(p)) {
                return sample(p);
            }
        }
    }
    // A pad that is touched now takes over.
    for (const Pad& p : pads_) {
        if (usable(p) && (p.touch.contact || p.touch.click)) {
            touchpad_last_id_ = p.id;
            return sample(p);
        }
    }
    // Else the last one touched, else the first with a touchpad.
    for (const Pad& p : pads_) {
        if (usable(p) && p.id == touchpad_last_id_) {
            return sample(p);
        }
    }
    for (const Pad& p : pads_) {
        if (usable(p)) {
            return sample(p);
        }
    }
    return {};
}

GamepadHub::MergeResult GamepadHub::Merge(const InputFrame& frame, const ProfileRoutes& routes,
                                          ControlState& controls, bool merge_buttons) {
    std::scoped_lock lock{mutex_};
    LoadLocked();
    RefreshLocked();
    MergeResult result;
    const bool swallow_now = release_pending_;
    release_pending_ = false;
    // AzaharFrame padsui: the mouse's 3DS buttons (the frame PollMouse read).
    if (swallow_now) {
        mouse_logic_.Release();
    } else if (merge_buttons && merge_enabled_ && mouse_frame_.present &&
               MouseMappingLocked().enabled) {
        MergeIntoFrame(ApplyMouseMapping(mouse_frame_.held, MouseMappingLocked()), routes, controls);
    }
    if (pads_.empty()) {
        return result;
    }
    ReadStatesLocked();
    result.touchpad = PickTouchpadLocked();
    const std::array<bool, kNumPadInputs> frame_pad = FrameAsPad(frame);
    // The Frame's sticks by label, for an unknown pad's sticks.
    const std::array<bool, kNumPadSticks> frame_stick_moved = {
        Magnitude(frame.hand[0].stick) > 0.2f, Magnitude(frame.hand[1].stick) > 0.2f};
    for (Pad& pad : pads_) {
        if (!pad.connected || pad.frame_own) {
            continue;
        }
        if (swallow_now) {
            pad.swallowed = pad.inputs.pressed;
        }
        const std::array<bool, kNumPadInputs> allowed_raw =
            pad.echo.Filter(frame_pad, pad.raw, frame.time_ns);
        const EchoGuard::Verdict verdict = pad.echo.verdict();
        if (verdict != pad.logged) {
            pad.logged = verdict;
            revision_++; // an echo leaves the list
        }
        // Held inputs stay away until let go.
        PadInputs inputs = pad.inputs;
        for (std::size_t k = 0; k < inputs.pressed.size(); k++) {
            if (!inputs.pressed[k]) {
                pad.swallowed[k] = false;
            }
        }
        if (!merge_buttons || !merge_enabled_ || verdict == EchoGuard::Verdict::Echo ||
            !MappingLocked(pad).enabled) { // AzaharFrame padsui: Use This Device off
            continue;
        }
        // The echo filter works on SDL's numbering; turn it into positions as the inputs are.
        std::array<bool, kNumPadInputs> allowed = allowed_raw;
        if (pad.by_label) {
            allowed[I(PadInput::South)] = allowed_raw[1];
            allowed[I(PadInput::East)] = allowed_raw[0];
            allowed[I(PadInput::West)] = allowed_raw[3];
            allowed[I(PadInput::North)] = allowed_raw[2];
        }
        for (std::size_t k = 0; k < inputs.pressed.size(); k++) {
            inputs.pressed[k] = inputs.pressed[k] && allowed[k] && !pad.swallowed[k];
        }
        if (verdict != EchoGuard::Verdict::Real) {
            for (std::size_t s = 0; s < inputs.sticks.size(); s++) {
                if (frame_stick_moved[s]) {
                    inputs.sticks[s] = {};
                }
            }
        }
        const ThreeDsPadState three = ApplyGamepadMapping(inputs, MappingLocked(pad));
        MergeIntoFrame(three, routes, controls);
        result.merged++;
    }
    return result;
}

void GamepadHub::Release() {
    std::scoped_lock lock{mutex_};
    release_pending_ = true;
    mouse_logic_.Release();
}

MouseFrame GamepadHub::PollMouse(int64_t now_ns) {
    std::scoped_lock lock{mutex_};
    GamepadMapping m = MouseMappingLocked();
    MouseFrame f = mouse_logic_.Update(source_.Mouse(), m.open_menu, now_ns);
    if (!m.enabled) {
        // AzaharFrame padsui: Use This Device off: no motion, no inputs, no ray.
        const bool present = f.present;
        f = MouseFrame{};
        f.disabled = present;
    }
    if (f.switch_mode) {
        m.mode = m.mode == 0 ? 1 : 0;
        SetMouseMappingLocked(m);
    }
    f.mode = m.mode == 1 ? MouseMode::Pointer : MouseMode::BottomScreen;
    f.speed = m.speed;
    f.recentre = m.recentre;
    f.touch_input = m.touch;
    mouse_frame_ = f;
    return f;
}

MouseFrame GamepadHub::LastMouse() {
    std::scoped_lock lock{mutex_};
    return mouse_frame_;
}

} // namespace FrameXr::Input
