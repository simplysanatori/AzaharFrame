// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/gamepads.h"

// AzaharFrame pads
using Catch::Approx;
using namespace FrameXr::Input;
using NB = Settings::NativeButton::Values;
using NA = Settings::NativeAnalog::Values;

namespace {

constexpr int64_t kMs = 1'000'000;
constexpr int P(PadInput i) {
    return static_cast<int>(i);
}
constexpr std::size_t C(Control c) {
    return static_cast<std::size_t>(c);
}

// SDL as the hub sees it.
struct FakeSource final : GamepadSource {
    uint64_t generation = 1;
    std::vector<InputCommon::SDLPadInfo> infos;
    std::vector<InputCommon::SDLPadState> states;
    std::string devices;
    int info_reads = 0;

    uint64_t Generation() override {
        return generation;
    }
    void Infos(std::vector<InputCommon::SDLPadInfo>& out) override {
        info_reads++;
        out = infos;
    }
    void States(std::vector<InputCommon::SDLPadState>& out) override {
        out = states;
    }
    std::string InputDevices() override {
        return devices;
    }
    MouseRaw mouse; // AzaharFrame padsui
    MouseRaw Mouse() override {
        return mouse;
    }

    void Add(int id, std::string guid, std::string name, PadType type, uint16_t vendor,
             uint16_t product, bool touchpad = false, bool steam = false) {
        InputCommon::SDLPadInfo info;
        info.id = id;
        info.guid = std::move(guid);
        info.name = std::move(name);
        info.type = static_cast<int>(type);
        info.vendor = vendor;
        info.product = product;
        info.has_touchpad = touchpad;
        info.steam_virtual = steam;
        infos.push_back(info);
        InputCommon::SDLPadState state;
        state.id = id;
        state.touchpad.present = touchpad;
        states.push_back(state);
        generation++;
    }
    void Remove(int id) {
        std::erase_if(infos, [id](const auto& i) { return i.id == id; });
        std::erase_if(states, [id](const auto& s) { return s.id == id; });
        generation++;
    }
    InputCommon::SDLPadState& State(int id) {
        for (auto& s : states) {
            if (s.id == id) {
                return s;
            }
        }
        FAIL("no pad " << id);
        return states.front();
    }
};

struct FakeStorage {
    std::string text;
    int saves = 0;
    GamepadStorage Get() {
        return {[this] { return text; },
                [this](const std::string& t) {
                    text = t;
                    saves++;
                    return true;
                }};
    }
};

ProfileRoutes DefaultRoutes() {
    ProfileRoutes r;
    r.buttons.fill(-1);
    r.analogs.fill(-1);
    for (const auto& b : kDefaultButtonBindings) {
        r.buttons[b.button] = static_cast<int8_t>(b.control);
    }
    for (const auto& a : kDefaultAnalogBindings) {
        r.analogs[a.analog] = static_cast<int8_t>(a.control);
    }
    return r;
}

const char* kDs4Guid = "050000004c050000cc09000000810000";
const char* k8BitDoGuid = "05000000c82d00000660000000010000";

struct Rig {
    FakeSource source;
    FakeStorage storage;
    GamepadHub hub{source, storage.Get()};
    ProfileRoutes routes = DefaultRoutes();
    int64_t now = 1'000 * kMs;
    InputFrame frame;

    ControlState Step(bool merge = true, int64_t dt = 11 * kMs) {
        now += dt;
        frame.time_ns = now;
        ControlState controls;
        hub.Merge(frame, routes, controls, merge);
        return controls;
    }
    // Presses until the echo guard took the pad for a real one (three presses of its own).
    void MakeReal(int id) {
        for (int i = 0; i < EchoGuard::kVerdictPresses; i++) {
            source.State(id).buttons[P(PadInput::Misc1)] = true;
            Step();
            Step(true, 100 * kMs);
            source.State(id).buttons[P(PadInput::Misc1)] = false;
            Step();
        }
    }
};

} // namespace

TEST_CASE("Gamepads: raw SDL state as inputs by position", "[frame_xr]") {
    InputCommon::SDLPadState raw;
    raw.buttons[0] = true; // SDL's A
    raw.buttons[2] = true; // SDL's X
    raw.axes = {0.5f, -1.0f, 0.1f, 0.0f, 0.6f, 0.3f};
    PadInputs xbox = PadInputsFromRaw(raw, {}, false);
    REQUIRE(xbox.Get(PadInput::South));
    REQUIRE(xbox.Get(PadInput::West));
    REQUIRE_FALSE(xbox.Get(PadInput::East));
    // A Nintendo pad reports by label: its A is the right button, its X the top one.
    PadInputs nin = PadInputsFromRaw(raw, {}, true);
    REQUIRE(nin.Get(PadInput::East));
    REQUIRE(nin.Get(PadInput::North));
    REQUIRE_FALSE(nin.Get(PadInput::South));
    // Triggers: pressed past 0.55, held down to 0.40.
    REQUIRE(xbox.Get(PadInput::LeftTrigger));
    REQUIRE_FALSE(xbox.Get(PadInput::RightTrigger));
    raw.axes[4] = 0.45f;
    REQUIRE(PadInputsFromRaw(raw, xbox, false).Get(PadInput::LeftTrigger));
    REQUIRE_FALSE(PadInputsFromRaw(raw, {}, false).Get(PadInput::LeftTrigger));
    // Sticks: up positive, a dead zone that keeps full tilt.
    REQUIRE(xbox.sticks[0].y == Approx(0.894f).margin(0.01f));
    REQUIRE(xbox.sticks[0].x > 0.4f);
    REQUIRE(xbox.sticks[1].x == 0.0f);
    REQUIRE(ReportsByLabel(PadType::SwitchPro, false));
    REQUIRE_FALSE(ReportsByLabel(PadType::SwitchPro, true));
    REQUIRE_FALSE(ReportsByLabel(PadType::PS4, false));
}

TEST_CASE("Gamepads: default mapping", "[frame_xr]") {
    const GamepadMapping pos = DefaultGamepadMapping(PadType::XboxOne, true);
    REQUIRE(pos.buttons[NB::A] == P(PadInput::East));
    REQUIRE(pos.buttons[NB::B] == P(PadInput::South));
    REQUIRE(pos.buttons[NB::X] == P(PadInput::North));
    REQUIRE(pos.buttons[NB::Y] == P(PadInput::West));
    REQUIRE(pos.buttons[NB::L] == P(PadInput::LeftShoulder));
    REQUIRE(pos.buttons[NB::ZR] == P(PadInput::RightTrigger));
    REQUIRE(pos.buttons[NB::Start] == P(PadInput::Start));
    REQUIRE(pos.buttons[NB::Select] == P(PadInput::Back));
    REQUIRE(pos.buttons[NB::Up] == P(PadInput::DpadUp));
    REQUIRE(pos.buttons[NB::Home] == -1);
    REQUIRE(pos.analogs[NA::CirclePad] == 0);
    REQUIRE(pos.analogs[NA::CStick] == 1);
    const GamepadMapping label = DefaultGamepadMapping(PadType::XboxOne, false);
    REQUIRE(label.buttons[NB::A] == P(PadInput::South));
    REQUIRE(label.buttons[NB::X] == P(PadInput::West));
    // A Nintendo pad's A is on the right either way.
    REQUIRE(DefaultGamepadMapping(PadType::SwitchPro, false) == pos);
    REQUIRE(std::string{PadInputName(PadType::PS4, P(PadInput::South))} == "Cross");
    REQUIRE(std::string{PadInputName(PadType::SwitchPro, P(PadInput::South))} == "B");
    REQUIRE(std::string{PadInputName(PadType::PS5, P(PadInput::LeftTrigger))} == "L2");
    REQUIRE(std::string{PadInputName(PadType::Steam, P(PadInput::Paddle2))} == "L4");
    REQUIRE(std::string{PadInputName(PadType::XboxOne, 99)}.empty());
}

TEST_CASE("Gamepads: mapping and merge into the Frame's pad", "[frame_xr]") {
    const ProfileRoutes routes = DefaultRoutes();
    PadInputs pad;
    pad.pressed[P(PadInput::East)] = true;
    pad.pressed[P(PadInput::LeftTrigger)] = true;
    pad.sticks[0] = {0.2f, 0.0f};
    pad.sticks[1] = {0.0f, 0.9f};
    const ThreeDsPadState three =
        ApplyGamepadMapping(pad, DefaultGamepadMapping(PadType::XboxOne, true));
    REQUIRE(three.buttons[NB::A]);
    REQUIRE(three.buttons[NB::ZL]);
    REQUIRE_FALSE(three.buttons[NB::B]);

    ControlState controls;
    controls.Set(Control::B, true);                          // the Frame presses B
    controls.SetStick(AnalogControl::StickL, {0.0f, 0.5f}); // and pushes its left stick further
    MergeIntoFrame(three, routes, controls);
    REQUIRE(controls.Get(Control::A));
    REQUIRE(controls.Get(Control::B)); // OR
    REQUIRE(controls.Get(Control::TriggerL));
    REQUIRE(controls.GetStick(AnalogControl::StickL).y == Approx(0.5f)); // the Frame's wins
    REQUIRE(controls.GetStick(AnalogControl::StickR).y == Approx(0.9f)); // the pad's wins

    // A 3DS button the profile does not route to a Frame control is not reachable.
    ProfileRoutes no_zl = routes;
    no_zl.buttons[NB::ZL] = -1;
    ControlState c2;
    MergeIntoFrame(three, no_zl, c2);
    REQUIRE_FALSE(c2.Get(Control::TriggerL));
    // A remapped profile: the 3DS's A on the Frame's X; the pad's A follows.
    ProfileRoutes moved = routes;
    moved.buttons[NB::A] = static_cast<int8_t>(Control::X);
    ControlState c3;
    MergeIntoFrame(three, moved, c3);
    REQUIRE(c3.Get(Control::X));
    REQUIRE_FALSE(c3.Get(Control::A));
    REQUIRE(MappingOfRoutes(routes).buttons[NB::Start] == static_cast<int>(Control::Menu));
}

TEST_CASE("Gamepads: keys and the Frame's own pad", "[frame_xr]") {
    InputCommon::SDLPadInfo info;
    info.guid = kDs4Guid;
    info.vendor = 0x054c;
    info.product = 0x09cc;
    REQUIRE(GamepadKey(info) == kDs4Guid);
    info.steam_virtual = true;
    REQUIRE(GamepadKey(info) == "steam:054c:09cc");
    REQUIRE_FALSE(IsFrameOwnPad(info));
    info.vendor = kValveVendor;
    info.product = 0x1205; // Steam Deck
    REQUIRE_FALSE(IsFrameOwnPad(info));
    REQUIRE(PadTypeOf(info) == PadType::Steam);
    info.product = 0x1302; // Steam Controller (2026)
    REQUIRE_FALSE(IsFrameOwnPad(info));
    info.product = 0x11ff; // Steam's virtual pad, nothing said: unknown, the echo guard decides
    REQUIRE_FALSE(IsFrameOwnPad(info));
    info.product = 0x2300; // any other Valve device: the Frame's controllers
    REQUIRE(IsFrameOwnPad(info));
}

TEST_CASE("Gamepads: mice in /proc/bus/input/devices", "[frame_xr]") {
    const std::string text = R"(I: Bus=0005 Vendor=046d Product=b034 Version=0001
N: Name="MX Master 3S"
H: Handlers=sysrq kbd event5 mouse0
B: EV=17
B: REL=1943

I: Bus=0003 Vendor=28de Product=1205 Version=0111
N: Name="Valve Software Steam Deck Controller"
H: Handlers=event6 mouse1
B: REL=103

I: Bus=0018 Vendor=0000 Product=0000 Version=0000
N: Name="Touchpad"
H: Handlers=event7 mouse2
B: ABS=260800000000003

I: Bus=0003 Vendor=045e Product=0b13 Version=0509
N: Name="Microsoft Xbox Controller"
H: Handlers=event8 js0
B: ABS=3003f
)";
    const std::vector<MouseInfo> mice = ParseInputDevices(text);
    REQUIRE(mice.size() == 1);
    REQUIRE(mice[0].name == "MX Master 3S");
    REQUIRE(mice[0].bus == 5);
    REQUIRE(mice[0].vendor == 0x046d);
    REQUIRE(ParseInputDevices("").empty());
}

TEST_CASE("Gamepads: mapping file", "[frame_xr]") {
    MappingFile file;
    GamepadMapping m = DefaultGamepadMapping(PadType::PS4, true);
    m.buttons[NB::A] = P(PadInput::South);
    file.Set(kDs4Guid, {m, 0x054c, 0x09cc, "Wireless Controller"});
    file.Set("steam:2dc8:6006", {EmptyMapping(), 0x2dc8, 0x6006, "8BitDo Pro 2 = name"});
    const std::string text = file.Format();
    MappingFile back;
    back.Parse(text + "garbage line\n" + "x buttons=1,2 analogs=0,1 name=short\n");
    REQUIRE(back.size() == 2);
    const MappingEntry* e = back.Find(kDs4Guid, 0, 0, "");
    REQUIRE(e);
    REQUIRE(e->mapping == m);
    REQUIRE(e->name == "Wireless Controller");
    // The same model through Steam Input (another key) finds it by vendor, product and name.
    REQUIRE(back.Find("steam:054c:09cc", 0x054c, 0x09cc, "Wireless Controller") == e);
    REQUIRE_FALSE(back.Find("steam:054c:09cc", 0x054c, 0x09cc, "Other"));
    const MappingEntry* named = back.Find("steam:2dc8:6006", 0, 0, "");
    REQUIRE(named);
    REQUIRE(named->name == "8BitDo Pro 2 = name");
    REQUIRE(back.Remove("steam:054c:09cc", 0x054c, 0x09cc, "Wireless Controller"));
    REQUIRE(back.size() == 1);
    // Out-of-range values make a line invalid.
    MappingFile bad;
    bad.Parse("k buttons=99,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 analogs=0,1 "
              "vendor=0 product=0 name=x\n");
    REQUIRE(bad.size() == 0);
}

TEST_CASE("Gamepads: the device registry and hot-plug", "[frame_xr]") {
    Rig rig;
    std::vector<DeviceInfo> list = rig.hub.Devices(rig.now);
    REQUIRE(list.size() == 1);
    REQUIRE(list[0].kind == DeviceKind::Frame);
    REQUIRE(list[0].id == kFrameDeviceId);
    REQUIRE(list[0].name == "Frame Controllers");

    const uint32_t rev0 = rig.hub.Revision();
    rig.source.Add(1, kDs4Guid, "Wireless Controller", PadType::PS4, 0x054c, 0x09cc, true);
    rig.source.Add(2, k8BitDoGuid, "8BitDo Pro 2", PadType::SwitchPro, 0x2dc8, 0x6006);
    rig.source.Add(3, "03000000de2800000023000000000000", "Frame", PadType::Unknown, kValveVendor,
                   0x2300);
    REQUIRE(rig.hub.Revision() != rev0);
    list = rig.hub.Devices(rig.now);
    REQUIRE(list.size() == 3); // the Frame's own pad is not listed
    REQUIRE(list[1].name == "Wireless Controller");
    REQUIRE(list[1].type == PadType::PS4);
    REQUIRE(list[1].has_touchpad);
    REQUIRE(list[1].connected);
    REQUIRE(list[2].id == k8BitDoGuid);
    REQUIRE(list[2].type == PadType::SwitchPro);

    // No change: SDL's infos are not read again.
    const int reads = rig.source.info_reads;
    rig.hub.Devices(rig.now);
    rig.Step();
    REQUIRE(rig.source.info_reads == reads);

    // Unplugged: listed after the connected ones, not connected; back: its old entry.
    rig.source.Remove(1);
    list = rig.hub.Devices(rig.now);
    REQUIRE(list.size() == 3);
    REQUIRE(list[1].id == k8BitDoGuid);
    REQUIRE(list[2].id == kDs4Guid);
    REQUIRE_FALSE(list[2].connected);
    rig.source.Add(7, kDs4Guid, "Wireless Controller", PadType::PS4, 0x054c, 0x09cc, true);
    list = rig.hub.Devices(rig.now);
    REQUIRE(list.size() == 3);
    REQUIRE(list[1].id == kDs4Guid);
    REQUIRE(list[1].connected);

    // A second pad of the same model gets an id of its own.
    rig.source.Add(8, kDs4Guid, "Wireless Controller", PadType::PS4, 0x054c, 0x09cc, true);
    list = rig.hub.Devices(rig.now);
    REQUIRE(list.size() == 4);
    REQUIRE(list[3].id == std::string{kDs4Guid} + "#2");

    // A mouse shows (read again at most every 2 s).
    rig.source.devices = "I: Bus=0005 Vendor=046d Product=b034\nN: Name=\"MX Anywhere\"\n"
                         "H: Handlers=event5 mouse0\nB: REL=143\n";
    REQUIRE(rig.hub.Devices(rig.now).size() == 4);
    list = rig.hub.Devices(rig.now + 2'000 * kMs);
    REQUIRE(list.size() == 5);
    REQUIRE(list[4].kind == DeviceKind::Mouse);
    REQUIRE(list[4].id == kMouseDeviceId);
    REQUIRE(list[4].name == "MX Anywhere");
}

TEST_CASE("Gamepads: a pad plays alongside the Frame", "[frame_xr]") {
    Rig rig;
    rig.source.Add(1, k8BitDoGuid, "8BitDo Pro 2", PadType::XboxOne, 0x2dc8, 0x6006);
    rig.Step();
    auto& state = rig.source.State(1);

    // An unknown pad's press reaches the game once the Frame did not press with it (80 ms).
    state.buttons[P(PadInput::East)] = true;
    REQUIRE_FALSE(rig.Step().Get(Control::A));
    REQUIRE(rig.Step(true, 100 * kMs).Get(Control::A));
    state.buttons[P(PadInput::East)] = false;
    rig.Step();
    rig.MakeReal(1);

    // Real: at once, ORed with the Frame, sticks and triggers too.
    state.buttons[P(PadInput::South)] = true;
    state.axes[5] = 1.0f;
    state.axes[0] = -1.0f;
    ControlState c = rig.Step();
    REQUIRE(c.Get(Control::B));
    REQUIRE(c.Get(Control::TriggerR));
    REQUIRE(c.GetStick(AnalogControl::StickL).x == Approx(-1.0f));
    // Not in a game / the menu is open: nothing.
    ControlState off = rig.Step(false);
    REQUIRE_FALSE(off.Get(Control::B));
    REQUIRE(off.GetStick(AnalogControl::StickL).x == 0.0f);
    // The menu closed with B held: B stays away until let go.
    rig.hub.Release();
    REQUIRE_FALSE(rig.Step().Get(Control::B));
    REQUIRE(rig.Step().Get(Control::TriggerR) == false);
    state.buttons[P(PadInput::South)] = false;
    rig.Step();
    state.buttons[P(PadInput::South)] = true;
    REQUIRE(rig.Step().Get(Control::B));

    // Merge off (AZAHARFRAME_PADS=0).
    rig.hub.SetMergeEnabled(false);
    REQUIRE_FALSE(rig.Step().Get(Control::B));
}

TEST_CASE("Gamepads: a pad held as it connects waits for a release", "[frame_xr]") {
    Rig rig;
    rig.source.Add(1, k8BitDoGuid, "8BitDo Pro 2", PadType::XboxOne, 0x2dc8, 0x6006);
    rig.source.State(1).buttons[P(PadInput::Start)] = true;
    rig.Step();
    REQUIRE_FALSE(rig.Step(true, 200 * kMs).Get(Control::Menu));
    rig.source.State(1).buttons[P(PadInput::Start)] = false;
    rig.Step();
    rig.source.State(1).buttons[P(PadInput::Start)] = true;
    rig.Step();
    REQUIRE(rig.Step(true, 100 * kMs).Get(Control::Menu));
}

TEST_CASE("Gamepads: Steam's echo of the Frame never reaches the game", "[frame_xr]") {
    Rig rig;
    rig.source.Add(1, "03000000de280000ff11000001000000", "Steam Virtual Gamepad",
                   PadType::XboxOne, kValveVendor, 0x11ff, false, true);
    rig.Step();
    REQUIRE(rig.hub.Devices(rig.now).size() == 2);
    auto& state = rig.source.State(1);
    for (int i = 0; i < EchoGuard::kVerdictPresses; i++) {
        // The Frame's B is the trigger aimed at a screen: the mapper keeps it from the game;
        // the echo must not bring it back.
        rig.frame.b = true;
        state.buttons[P(PadInput::East)] = true;
        REQUIRE_FALSE(rig.Step().Get(Control::A));
        REQUIRE_FALSE(rig.Step(true, 100 * kMs).Get(Control::A));
        rig.frame.b = false;
        state.buttons[P(PadInput::East)] = false;
        rig.Step();
    }
    // The pad's press a frame before the Frame's is the same press too.
    state.buttons[P(PadInput::South)] = true;
    REQUIRE_FALSE(rig.Step().Get(Control::B));
    REQUIRE(rig.hub.Devices(rig.now).size() == 1); // an echo is not listed
    state.buttons[P(PadInput::South)] = false;
    rig.Step();
}

TEST_CASE("Gamepads: per-device mappings are saved", "[frame_xr]") {
    FakeStorage storage;
    {
        FakeSource source;
        GamepadHub hub(source, storage.Get());
        source.Add(1, kDs4Guid, "Wireless Controller", PadType::PS4, 0x054c, 0x09cc, true);
        REQUIRE_FALSE(hub.SetMapping(kFrameDeviceId, EmptyMapping()));
        REQUIRE_FALSE(hub.SetMapping("nope", EmptyMapping()));
        GamepadMapping m = hub.Mapping(kDs4Guid, DefaultRoutes());
        REQUIRE(m == DefaultGamepadMapping(PadType::PS4, true));
        m.buttons[NB::A] = P(PadInput::South);
        REQUIRE(hub.SetMapping(kDs4Guid, m));
        REQUIRE(storage.saves == 1);
        REQUIRE(hub.Devices(0)[1].custom);
        // The Frame's mapping is the profile's routes.
        REQUIRE(hub.Mapping(kFrameDeviceId, DefaultRoutes()).buttons[NB::A] ==
                static_cast<int>(Control::A));
    }
    {
        // A new run, the same pad through Steam Input: the saved mapping.
        FakeSource source;
        GamepadHub hub(source, storage.Get());
        source.Add(4, "03000000de280000ff11000001000000", "Wireless Controller", PadType::PS4,
                   0x054c, 0x09cc, true, true);
        const std::string id = "steam:054c:09cc";
        REQUIRE(hub.Devices(0)[1].id == id);
        REQUIRE(hub.Mapping(id, DefaultRoutes()).buttons[NB::A] == P(PadInput::South));
        REQUIRE(hub.ResetMapping(id));
        REQUIRE_FALSE(hub.ResetMapping(id));
        REQUIRE(hub.Mapping(id, DefaultRoutes()) == DefaultGamepadMapping(PadType::PS4, true));
        REQUIRE(storage.text.find("054c") == std::string::npos);
        // A mapping equal to the default saves nothing of its own.
        REQUIRE(hub.SetMapping(id, DefaultGamepadMapping(PadType::PS4, true)));
        REQUIRE_FALSE(hub.Devices(0)[1].custom);
        // Face Buttons by label changes the default.
        hub.SetFaceButtonsByPosition(false);
        REQUIRE(hub.DefaultMapping(id).buttons[NB::A] == P(PadInput::South));
    }
}

TEST_CASE("Gamepads: listening for the next input", "[frame_xr]") {
    Rig rig;
    rig.source.Add(1, k8BitDoGuid, "8BitDo Pro 2", PadType::SwitchPro, 0x2dc8, 0x6006);
    auto& state = rig.source.State(1);
    REQUIRE_FALSE(rig.hub.BeginListen(kFrameDeviceId));
    REQUIRE(rig.hub.PollListen().state == GamepadHub::ListenResult::State::Idle);
    state.buttons[P(PadInput::Start)] = true; // held as listening starts
    REQUIRE(rig.hub.BeginListen(k8BitDoGuid));
    REQUIRE(rig.hub.PollListen().state == GamepadHub::ListenResult::State::Listening);
    state.axes[4] = 0.3f;
    REQUIRE(rig.hub.PollListen().state == GamepadHub::ListenResult::State::Listening);
    // SDL's A on a Nintendo pad is the right button: East.
    state.buttons[0] = true;
    const auto got = rig.hub.PollListen();
    REQUIRE(got.state == GamepadHub::ListenResult::State::Pressed);
    REQUIRE(got.input == P(PadInput::East));
    REQUIRE(rig.hub.PollListen().state == GamepadHub::ListenResult::State::Idle);
    // The trigger counts as an input; Start, held from before, after a release.
    REQUIRE(rig.hub.BeginListen(k8BitDoGuid));
    state.buttons[P(PadInput::Start)] = false;
    REQUIRE(rig.hub.PollListen().state == GamepadHub::ListenResult::State::Listening);
    state.axes[4] = 0.9f;
    REQUIRE(rig.hub.PollListen().input == P(PadInput::LeftTrigger));
    // Gone.
    REQUIRE(rig.hub.BeginListen(k8BitDoGuid));
    rig.source.Remove(1);
    REQUIRE(rig.hub.PollListen().state == GamepadHub::ListenResult::State::Gone);
    REQUIRE_FALSE(rig.hub.BeginListen(k8BitDoGuid));
}

TEST_CASE("Gamepads: the touchpad for Touchpad Touch", "[frame_xr]") {
    Rig rig;
    ControlState c;
    REQUIRE_FALSE(rig.hub.Merge(rig.frame, rig.routes, c, false).touchpad.pad.present);
    rig.source.Add(1, kDs4Guid, "Wireless Controller", PadType::PS4, 0x054c, 0x09cc, true);
    rig.source.Add(2, "0300000028de00000211000000000000", "Steam Controller", PadType::Unknown,
                   kValveVendor, 0x1102, true);
    TouchpadPick pick = rig.hub.Merge(rig.frame, rig.routes, c, false).touchpad;
    REQUIRE(pick.pad.present);
    REQUIRE(pick.id == kDs4Guid); // the first with a touchpad
    // The one touched takes over, and stays after the finger left.
    rig.source.State(2).touchpad.contact = true;
    rig.source.State(2).touchpad.x = 0.25f;
    pick = rig.hub.Merge(rig.frame, rig.routes, c, false).touchpad;
    REQUIRE(pick.id == "0300000028de00000211000000000000");
    REQUIRE(pick.pad.contact);
    REQUIRE(pick.pad.x == Approx(0.25f));
    rig.source.State(2).touchpad.contact = false;
    REQUIRE(rig.hub.Merge(rig.frame, rig.routes, c, false).touchpad.id ==
            "0300000028de00000211000000000000");
    // The menu's choice wins.
    rig.hub.SetTouchpadDevice(kDs4Guid);
    REQUIRE(rig.hub.TouchpadDevice() == kDs4Guid);
    rig.source.State(2).touchpad.contact = true;
    REQUIRE(rig.hub.Merge(rig.frame, rig.routes, c, false).touchpad.id == kDs4Guid);
    REQUIRE(rig.hub.Touchpad("0300000028de00000211000000000000").contact);
    REQUIRE_FALSE(rig.hub.Touchpad("nope").present);
}

TEST_CASE("Gamepads: the menu's terms", "[frame_xr]") {
    // Xbox-style: SDL's A (the bottom button) is Control::A.
    REQUIRE(ControlOfPadInput(P(PadInput::South), false) == static_cast<int>(Control::A));
    REQUIRE(ControlOfPadInput(P(PadInput::Start), false) == static_cast<int>(Control::Menu));
    REQUIRE(ControlOfPadInput(P(PadInput::Paddle4), false) == static_cast<int>(Control::GripL));
    REQUIRE(ControlOfPadInput(P(PadInput::Guide), false) == -1);
    // A Switch pad reported by label: SDL's A is the right button.
    REQUIRE(ControlOfPadInput(P(PadInput::East), true) == static_cast<int>(Control::A));
    for (int c = 0; c < kNumControls; c++) {
        for (const bool label : {false, true}) {
            REQUIRE(ControlOfPadInput(PadInputOfControl(c, label), label) == c);
        }
    }
    // The positional default in the menu's terms: the 3DS's A on SDL's B (Xbox) or A (Switch).
    const GamepadMapping def = DefaultGamepadMapping(PadType::XboxOne, true);
    REQUIRE(ControlTableOf(def, false)[NB::A] == static_cast<int>(Control::B));
    REQUIRE(ControlTableOf(DefaultGamepadMapping(PadType::SwitchPro, true), true)[NB::A] ==
            static_cast<int>(Control::A));
    // A change from the menu keeps the inputs it did not touch, Home on Guide included.
    GamepadMapping m = def;
    m.buttons[NB::Home] = P(PadInput::Guide);
    ButtonTable t = ControlTableOf(m, false);
    REQUIRE(t[NB::Home] == -1);
    t[NB::A] = static_cast<int8_t>(Control::A);
    const GamepadMapping after = WithControlTable(m, t, false);
    REQUIRE(after.buttons[NB::A] == P(PadInput::South));
    REQUIRE(after.buttons[NB::Home] == P(PadInput::Guide));
    REQUIRE(after.buttons[NB::B] == P(PadInput::South)); // the menu swaps; here only A changed
}

// AzaharFrame padsui: a 3DS stick on a gamepad stick with its axes as Edit Controls learnt them.
TEST_CASE("Gamepads: stick routes", "[frame_xr]") {
    PadInputs pad;
    pad.sticks[0] = {0.0f, 0.8f};
    pad.sticks[1] = {0.6f, 0.0f};
    GamepadMapping m = DefaultGamepadMapping(PadType::XboxOne, true);
    // The Circle Pad on the right stick with its X turned round, the C-Stick on the left stick
    // turned a quarter.
    m.analogs[NA::CirclePad] = EncodeStickRoute({1, false, true, false});
    m.analogs[NA::CStick] = EncodeStickRoute({0, true, false, false});
    const ThreeDsPadState three = ApplyGamepadMapping(pad, m);
    REQUIRE(three.analogs[NA::CirclePad].x == Approx(-0.6f));
    REQUIRE(three.analogs[NA::CStick].x == Approx(0.8f));
    REQUIRE(three.analogs[NA::CStick].y == Approx(0.0f));

    // The profile's own route on the Frame's stick: the pad's value goes in so that the route
    // gives it back.
    ProfileRoutes routes;
    routes.buttons.fill(-1);
    routes.analogs = {EncodeStickRoute({0, false, false, true}), -1};
    ControlState controls;
    MergeIntoFrame(three, routes, controls);
    const Vec2f raw = controls.GetStick(AnalogControl::StickL);
    const Vec2f seen = ApplyStickRoute(raw, DecodeStickRoute(routes.analogs[0]));
    REQUIRE(seen.x == Approx(three.analogs[NA::CirclePad].x));
    REQUIRE(seen.y == Approx(three.analogs[NA::CirclePad].y));

    // The file keeps them; older files' 0 and 1 read as before; a route on a third stick is none.
    MappingFile file;
    file.Set("k", {m, 1, 2, "Pad"});
    MappingFile back;
    back.Parse(file.Format());
    REQUIRE(back.Find("k", 0, 0, "")->mapping.analogs == m.analogs);
    MappingFile old;
    old.Parse("o buttons=-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 analogs=1,0 "
              "vendor=0 product=0 name=x\n");
    REQUIRE(old.Find("o", 0, 0, "")->mapping.analogs == AnalogTable{1, 0});
    MappingFile third;
    third.Parse("t buttons=-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 analogs=2,0 "
                "vendor=0 product=0 name=x\n");
    REQUIRE(third.Find("t", 0, 0, "")->mapping.analogs == AnalogTable{-1, 0});
}

TEST_CASE("Gamepads: listening shows the pad's sticks", "[frame_xr]") {
    Rig rig;
    rig.source.Add(1, k8BitDoGuid, "8BitDo Pro 2", PadType::SwitchPro, 0x2dc8, 0x6006);
    auto& state = rig.source.State(1);
    REQUIRE(rig.hub.BeginListen(k8BitDoGuid));
    state.axes[0] = 0.9f; // the left stick pushed sideways
    const auto r = rig.hub.PollListen();
    REQUIRE(r.state == GamepadHub::ListenResult::State::Listening);
    REQUIRE(std::fabs(r.sticks[0].x) > 0.5f);
    REQUIRE(r.sticks[1].x == Approx(0.0f));
}

// AzaharFrame padsui: every device feeds the game at once; Edit Controls' device row only picks
// which mapping is edited.
TEST_CASE("Devices: the Frame, the mouse and a gamepad press together", "[frame_xr]") {
    Rig rig;
    rig.source.Add(1, kDs4Guid, "Wireless Controller", PadType::PS4, 0x054c, 0x09cc, true);
    rig.MakeReal(1);
    rig.source.mouse.present = true;
    rig.source.devices = "I: Bus=0003 Vendor=046d Product=c077 Version=0111\n"
                         "N: Name=\"USB Optical Mouse\"\nH: Handlers=mouse0 event7\nB: REL=903\n\n";
    // The owner's profile: the 3DS's A on the left controller's D-pad Left.
    rig.routes.buttons[NB::A] = static_cast<int8_t>(Control::DpadLeft);
    const auto step = [&](bool frame_dpad_left) {
        rig.now += 11 * kMs;
        rig.frame.time_ns = rig.now;
        rig.hub.PollMouse(rig.now);
        ControlState controls; // the Frame controllers' own, as the mapper gave them
        controls.buttons[C(Control::DpadLeft)] = frame_dpad_left;
        rig.hub.Merge(rig.frame, rig.routes, controls, true);
        return controls;
    };
    step(false);
    const GamepadMapping pad = rig.hub.Mapping(kDs4Guid, rig.routes);
    const int pad_b = pad.buttons[NB::B];
    REQUIRE(pad_b >= 0);
    const auto b_control = static_cast<std::size_t>(rig.routes.buttons[NB::B]);
    const auto l_control = static_cast<std::size_t>(rig.routes.buttons[NB::L]);

    // One frame: the Frame's D-pad Left (3DS A), the mouse's Right Click (3DS A too) and Back
    // (L), the gamepad's 3DS B and L.
    rig.source.mouse.buttons[1] = true; // Right Click
    rig.source.mouse.buttons[3] = true; // Back
    rig.source.State(1).buttons[pad_b] = true;
    rig.source.State(1).buttons[pad.buttons[NB::L]] = true;
    ControlState c = step(true);
    CHECK(c.buttons[C(Control::DpadLeft)]);
    CHECK(c.buttons[b_control]);
    CHECK(c.buttons[l_control]);
    // The Frame lets go: the mouse still holds the 3DS's A.
    c = step(false);
    CHECK(c.buttons[C(Control::DpadLeft)]);
    // The mouse lets go too, the gamepad still holds B and L.
    rig.source.mouse.buttons = {};
    c = step(false);
    CHECK_FALSE(c.buttons[C(Control::DpadLeft)]);
    CHECK(c.buttons[b_control]);
    CHECK(c.buttons[l_control]);

    // Use This Device off: that device reaches nothing, the others still do.
    GamepadMapping off = pad;
    off.enabled = false;
    REQUIRE(rig.hub.SetMapping(kDs4Guid, off));
    rig.source.mouse.buttons[1] = true;
    c = step(false);
    CHECK_FALSE(c.buttons[b_control]);
    CHECK(c.buttons[C(Control::DpadLeft)]); // the mouse's A
    CHECK(rig.storage.text.find(" use=0") != std::string::npos);
    GamepadMapping mouse_off = rig.hub.Mapping(kMouseDeviceId, rig.routes);
    mouse_off.enabled = false;
    REQUIRE(rig.hub.SetMapping(kMouseDeviceId, mouse_off));
    c = step(true);
    CHECK(c.buttons[C(Control::DpadLeft)]); // the Frame's own
    c = step(false);
    CHECK_FALSE(c.buttons[C(Control::DpadLeft)]);
    CHECK(rig.hub.LastMouse().disabled);
    CHECK_FALSE(rig.hub.LastMouse().present);
}
