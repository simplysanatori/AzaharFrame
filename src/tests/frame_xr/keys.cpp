// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/gamepads.h"
#include "frame_xr/input/key_device.h"
#include "frame_xr/input/key_names.h"

// AzaharFrame keys: a physical keyboard as a game control device in VR.
using Catch::Approx;
using namespace FrameXr::Input;
namespace NB = Settings::NativeButton;

namespace {

constexpr std::size_t K(int key) {
    return static_cast<std::size_t>(key);
}

// /proc/bus/input/devices as a Frame shows it with a Logitech MX Master 3 and a Keychron Q1 Pro
// paired (addresses made up), and the Frame's own Valve keyboard.
const char* const kDevices =
    "I: Bus=0005 Vendor=046d Product=b023 Version=0015\n"
    "N: Name=\"MX Master 3 Keyboard\"\n"
    "P: Phys=00:11:22:33:44:55\n"
    "U: Uniq=66:77:88:99:aa:bb\n"
    "H: Handlers=kbd sysrq kbd event4\n"
    "B: PROP=0\n"
    "B: EV=100013\n"
    "B: KEY=1000000000007 ff800000000007ff febeffdfffefffff fffffffffffffffe\n"
    "B: MSC=10\n"
    "\n"
    "I: Bus=0005 Vendor=046d Product=b023 Version=0015\n"
    "N: Name=\"MX Master 3 Mouse\"\n"
    "P: Phys=00:11:22:33:44:55\n"
    "U: Uniq=66:77:88:99:aa:bb\n"
    "H: Handlers=event5\n"
    "B: PROP=0\n"
    "B: EV=17\n"
    "B: KEY=ffff0000 0 0 0 0\n"
    "B: REL=1943\n"
    "B: MSC=10\n"
    "\n"
    "I: Bus=0005 Vendor=3434 Product=0610 Version=0132\n"
    "N: Name=\"Keychron Q1 Pro Keyboard\"\n"
    "P: Phys=00:11:22:33:44:55\n"
    "U: Uniq=cc:dd:ee:ff:00:11\n"
    "H: Handlers=sysrq kbd leds event7\n"
    "B: PROP=0\n"
    "B: EV=12001f\n"
    "B: KEY=3f00733fff 0 0 483ffff17aff32d bfd4444600000000 1 130ff38b17d007 ffff7bfad9415fff "
    "ffbeffdfffefffff fffffffffffffffe\n"
    "B: REL=1040\n"
    "B: ABS=100000000\n"
    "B: MSC=10\n"
    "B: LED=1f\n"
    "\n"
    "I: Bus=0005 Vendor=3434 Product=0610 Version=0132\n"
    "N: Name=\"Keychron Q1 Pro Mouse\"\n"
    "P: Phys=00:11:22:33:44:55\n"
    "U: Uniq=cc:dd:ee:ff:00:11\n"
    "H: Handlers=event8\n"
    "B: PROP=0\n"
    "B: EV=17\n"
    "B: KEY=1f0000 0 0 0 0\n"
    "B: REL=1943\n"
    "B: MSC=10\n"
    "\n"
    "I: Bus=0003 Vendor=28de Product=1205 Version=0111\n"
    "N: Name=\"Valve Software Steam Controller\"\n"
    "H: Handlers=sysrq kbd leds event9\n"
    "B: EV=12001f\n"
    "B: KEY=ffffffffffffffff ffffffffffffffff fffffffffffffffe\n"
    "B: LED=1f\n"
    "\n";

struct KeySource final : GamepadSource {
    KeyboardRaw raw;
    std::string devices;
    uint64_t Generation() override {
        return 1;
    }
    void Infos(std::vector<InputCommon::SDLPadInfo>& out) override {
        out.clear();
    }
    void States(std::vector<InputCommon::SDLPadState>& out) override {
        out.clear();
    }
    std::string InputDevices() override {
        return devices;
    }
    KeyboardRaw Keyboard() override {
        return raw;
    }
    void Press(int key) {
        raw.down[K(key)] = true;
        raw.presses[K(key)]++;
    }
    void Let(int key) {
        raw.down[K(key)] = false;
    }
};

struct KeyRig {
    KeySource src;
    std::string saved;
    GamepadHub hub{src, {[this] { return saved; }, [this](const std::string& t) {
                             saved = t;
                             return true;
                         }}};
    ProfileRoutes routes;
    InputFrame frame;
    int64_t now = 1'000'000'000;
    KeyRig() {
        src.devices = kDevices;
        src.raw.present = true;
        routes.buttons.fill(-1);
        routes.analogs.fill(-1);
        routes.buttons[NB::A] = static_cast<int8_t>(Control::B);
        routes.buttons[NB::Up] = static_cast<int8_t>(Control::DpadUp);
        routes.analogs[0] = static_cast<int8_t>(AnalogControl::StickL);
        routes.analogs[1] = static_cast<int8_t>(AnalogControl::StickR);
    }
    /// One display frame in a game: the keyboard's frame, then the merge.
    ControlState Game(bool typing = false, bool merge = true) {
        now += 11'000'000;
        hub.PollKeyboard(now, typing);
        ControlState c{};
        hub.Merge(frame, routes, c, merge);
        return c;
    }
};

} // namespace

TEST_CASE("Keys: names by key code, US key caps", "[frame_xr]") {
    CHECK(std::string(KeyName(Key::A)) == "A");
    CHECK(std::string(KeyName(Key::Esc)) == "Esc");
    CHECK(std::string(KeyName(Key::Num1)) == "1");
    CHECK(std::string(KeyName(Key::Up)) == "Up Arrow");
    CHECK(std::string(KeyName(Key::LeftShift)) == "Left Shift");
    CHECK(std::string(KeyName(72)) == "Keypad 8");
    CHECK(std::string(KeyName(127)) == "Menu Key");
    CHECK(std::string(KeyName(0)).empty());
    CHECK(std::string(KeyName(115)).empty()); // Volume Up: the system's
    CHECK(std::string(KeyName(-1)).empty());
    CHECK(std::string(KeyName(kNumKeys)).empty());
    for (int k = 0; k < kNumKeys; k++) {
        REQUIRE(KeyName(k) != nullptr);
    }
    CHECK_FALSE(BindableKey(116)); // Power
    CHECK(BindableKey(Key::Space));
}

TEST_CASE("Keys: a stick's chip names its four keys", "[frame_xr]") {
    CHECK(KeyStickName({Key::Up, Key::Down, Key::Left, Key::Right}) == "Arrow Keys");
    CHECK(KeyStickName({Key::I, Key::K, Key::J, Key::L}) == "I J K L");
    CHECK(KeyStickName({Key::W, Key::S, Key::A, Key::D}) == "W A S D");
    CHECK(KeyStickName({Key::W, -1, Key::A, Key::D}) == "W A \xE2\x80\x93 D");
    CHECK(KeyStickName({72, 80, 75, 77}) == "Keypad 8, Keypad 4, Keypad 2, Keypad 6");
    CHECK(KeyStickName({-1, -1, -1, -1}).empty());
}

TEST_CASE("Keys: the default is Azahar's desktop keyboard", "[frame_xr]") {
    const GamepadMapping m = DefaultKeyboardMapping();
    const std::vector<std::pair<int, int>> want{
        {NB::A, Key::A},       {NB::B, Key::S},     {NB::X, Key::Z},      {NB::Y, Key::X},
        {NB::Up, Key::T},      {NB::Down, Key::G},  {NB::Left, Key::F},   {NB::Right, Key::H},
        {NB::L, Key::Q},       {NB::R, Key::W},     {NB::Start, Key::M},  {NB::Select, Key::N},
        {NB::ZL, Key::Num1},   {NB::ZR, Key::Num2}, {NB::Home, Key::B},   {NB::Debug, -1},
        {NB::Gpio14, -1},      {NB::Power, -1}};
    for (const auto& [button, key] : want) {
        CHECK(m.buttons[static_cast<std::size_t>(button)] == key);
    }
    CHECK(m.StickKeys(0) == std::array<int8_t, 4>{Key::Up, Key::Down, Key::Left, Key::Right});
    CHECK(m.StickKeys(1) == std::array<int8_t, 4>{Key::I, Key::K, Key::J, Key::L});
    CHECK(m.modifier == Key::D);
    CHECK(m.open_menu == Key::Esc);
    CHECK(m.enabled);
    // No key does two things.
    std::vector<int> used;
    for (const int8_t k : m.buttons) {
        if (k > 0) {
            used.push_back(k);
        }
    }
    for (const int8_t k : m.stick_keys) {
        used.push_back(k);
    }
    used.push_back(m.modifier);
    used.push_back(m.open_menu);
    std::sort(used.begin(), used.end());
    CHECK(std::adjacent_find(used.begin(), used.end()) == used.end());
}

TEST_CASE("Keys: a stick from four keys, as Azahar's analog from buttons", "[frame_xr]") {
    std::array<bool, kNumKeys> held{};
    const std::array<int8_t, 4> keys{Key::W, Key::S, Key::A, Key::D};
    CHECK(KeyStickValue(held, keys, false).x == 0.0f);
    held[K(Key::W)] = true;
    Vec2f v = KeyStickValue(held, keys, false);
    CHECK(v.x == 0.0f);
    CHECK(v.y == 1.0f);
    held[K(Key::D)] = true;
    v = KeyStickValue(held, keys, false);
    CHECK(v.x == Approx(0.7071f));
    CHECK(v.y == Approx(0.7071f));
    v = KeyStickValue(held, keys, true);
    CHECK(v.x == Approx(0.35355f));
    CHECK(v.y == Approx(0.35355f));
    held[K(Key::S)] = true; // up and down: no tilt up or down
    v = KeyStickValue(held, keys, false);
    CHECK(v.x == 1.0f);
    CHECK(v.y == 0.0f);
}

TEST_CASE("Keys: held keys, quick taps, a release, Open Menu", "[frame_xr]") {
    KeyboardLogic logic;
    KeyboardRaw raw;
    raw.present = true;
    raw.down[K(Key::A)] = true; // held as it is first read: no press
    CHECK_FALSE(logic.Update(raw, Key::Esc).Held(Key::A));
    raw.down[K(Key::A)] = false;
    CHECK_FALSE(logic.Update(raw, Key::Esc).Held(Key::A));
    raw.down[K(Key::A)] = true;
    raw.presses[K(Key::A)]++;
    KeyboardFrame f = logic.Update(raw, Key::Esc);
    CHECK(f.Held(Key::A));
    CHECK(f.used);
    CHECK(logic.Update(raw, Key::Esc).Held(Key::A));

    // A tap shorter than a frame still counts, for one frame.
    raw.presses[K(Key::S)]++;
    CHECK(logic.Update(raw, Key::Esc).Held(Key::S));
    CHECK_FALSE(logic.Update(raw, Key::Esc).Held(Key::S));

    // Released (the menu opened): A stays away until let go.
    logic.Release();
    CHECK_FALSE(logic.Update(raw, Key::Esc).Held(Key::A));
    raw.down[K(Key::A)] = false;
    logic.Update(raw, Key::Esc);
    raw.down[K(Key::A)] = true;
    raw.presses[K(Key::A)]++;
    CHECK(logic.Update(raw, Key::Esc).Held(Key::A));

    // Open Menu goes down once.
    raw.down[K(Key::Esc)] = true;
    raw.presses[K(Key::Esc)]++;
    CHECK(logic.Update(raw, Key::Esc).open_menu);
    CHECK_FALSE(logic.Update(raw, Key::Esc).open_menu);
    // A swallowed key (just learnt in Edit Controls) is no press of Open Menu.
    raw.down[K(Key::Esc)] = false;
    logic.Update(raw, Key::Esc);
    raw.down[K(Key::Esc)] = true;
    raw.presses[K(Key::Esc)]++;
    logic.Swallow(Key::Esc);
    CHECK_FALSE(logic.Update(raw, Key::Esc).open_menu);
}

TEST_CASE("Keys: the 3DS buttons and sticks of a mapping", "[frame_xr]") {
    const GamepadMapping m = DefaultKeyboardMapping();
    std::array<bool, kNumKeys> held{};
    held[K(Key::A)] = true;
    held[K(Key::Up)] = true;
    held[K(Key::L)] = true; // the C-Stick's right
    ThreeDsPadState s = ApplyKeyboardMapping(held, m);
    CHECK(s.buttons[NB::A]);
    CHECK_FALSE(s.buttons[NB::B]);
    CHECK(s.analogs[0].y == 1.0f);
    CHECK(s.analogs[1].x == 1.0f);
    held[K(Key::D)] = true; // the modifier tilts both halfway
    s = ApplyKeyboardMapping(held, m);
    CHECK(s.analogs[0].y == Approx(0.5f));
    CHECK(s.analogs[1].x == Approx(0.5f));
}

TEST_CASE("Keys: the mapping file's keyboard line holds only what differs", "[frame_xr]") {
    MappingFile file;
    GamepadMapping m = DefaultKeyboardMapping();
    m.buttons[NB::A] = Key::Space;
    m.buttons[NB::ZL] = -1;
    m.stick_keys[0] = Key::W;
    m.stick_keys[1] = Key::S;
    m.stick_keys[2] = Key::A;
    m.stick_keys[3] = Key::D;
    m.open_menu = Key::Num2;
    m.enabled = false;
    file.Set(kKeyboardDeviceId, {m, 0, 0, "Keyboard"});
    const std::string text = file.Format();
    CHECK(text.find("\nkeyboard a=57 zl=-1 circle=17,31,30,32 menu=3 use=0 name=Keyboard\n") !=
          std::string::npos);
    CHECK(text.find("keyboard buttons=") == std::string::npos);
    MappingFile again;
    again.Parse(text);
    REQUIRE(again.Find(kKeyboardDeviceId, 0, 0, ""));
    CHECK(again.Find(kKeyboardDeviceId, 0, 0, "")->mapping == m);
    CHECK(again.Format() == text);

    // A bare line is the default; nonsense reads as nothing for that value.
    MappingFile bare;
    bare.Parse("keyboard name=Keyboard\nkeyboard2 b=900\n");
    REQUIRE(bare.Find(kKeyboardDeviceId, 0, 0, ""));
    CHECK(bare.Find(kKeyboardDeviceId, 0, 0, "")->mapping == DefaultKeyboardMapping());
    CHECK(bare.size() == 1);
    MappingFile odd;
    odd.Parse("keyboard b=900 c_stick=23,x modifier=0 name=Keyboard\n");
    const GamepadMapping& o = odd.Find(kKeyboardDeviceId, 0, 0, "")->mapping;
    CHECK(o.buttons[NB::B] == -1);
    CHECK(o.StickKeys(1) == std::array<int8_t, 4>{Key::I, -1, Key::J, Key::L});
    CHECK(o.modifier == -1);
}

TEST_CASE("Keys: keyboards are found as the game's keyboard finds them", "[frame_xr]") {
    const std::vector<KeyboardInfo> all = ClassifyKeyboards(kDevices);
    REQUIRE(all.size() == 2); // not Valve's
    CHECK(all[0].name == "MX Master 3 Keyboard");
    CHECK(all[0].needs_key); // a mouse's keyboard node: no LEDs
    CHECK(all[1].name == "Keychron Q1 Pro Keyboard");
    CHECK(all[1].event == "event7");
    CHECK_FALSE(all[1].needs_key);
    const auto usable = UsableKeyboards(all, {});
    REQUIRE(usable.size() == 1);
    CHECK(usable[0].name == "Keychron Q1 Pro Keyboard");
    CHECK(UsableKeyboards(all, {all[0].group}).size() == 2);

    // The Keychron's mouse interface stays the mouse's (a companion, used once it moves).
    bool companion = false;
    for (const MouseCandidate& c : ClassifyInputDevices(kDevices)) {
        if (c.info.name == "Keychron Q1 Pro Mouse") {
            companion = c.verdict == MouseVerdict::Companion;
        }
    }
    CHECK(companion);
}

TEST_CASE("Keys: the hub lists the keyboard and keeps its mapping", "[frame_xr]") {
    KeyRig r;
    std::vector<DeviceInfo> list = r.hub.Devices(0);
    REQUIRE(list.size() == 3); // Frame Controllers, Mouse (the MX Master), Keyboard
    CHECK(list[1].kind == DeviceKind::Mouse);
    CHECK(list[2].kind == DeviceKind::Keyboard);
    CHECK(list[2].id == kKeyboardDeviceId);
    CHECK(list[2].name == "Keyboard");
    CHECK_FALSE(list[2].custom);
    CHECK(r.hub.Mapping(kKeyboardDeviceId, {}) == DefaultKeyboardMapping());
    CHECK(r.hub.DefaultMapping(kKeyboardDeviceId) == DefaultKeyboardMapping());

    GamepadMapping m = DefaultKeyboardMapping();
    m.buttons[NB::A] = Key::Space;
    REQUIRE(r.hub.SetMapping(kKeyboardDeviceId, m));
    CHECK(r.saved.find("keyboard a=57 name=Keyboard") != std::string::npos);
    CHECK(r.hub.Mapping(kKeyboardDeviceId, {}) == m);
    CHECK(r.hub.Devices(0)[2].custom);
    REQUIRE(r.hub.ResetMapping(kKeyboardDeviceId));
    CHECK(r.saved.find("keyboard") == r.saved.find("keyboard:")); // the header's only
    CHECK(r.hub.Mapping(kKeyboardDeviceId, {}) == DefaultKeyboardMapping());
    // Setting the default removes the line too.
    REQUIRE(r.hub.SetMapping(kKeyboardDeviceId, DefaultKeyboardMapping()));
    CHECK(r.saved.find("\nkeyboard ") == std::string::npos);

    // Unplugged (the list read again 2 s later): no Keyboard.
    r.src.devices.clear();
    list = r.hub.Devices(3'000'000'000);
    CHECK(list.size() == 1);
}

TEST_CASE("Keys: listening takes the next key, never the game's", "[frame_xr]") {
    using State = GamepadHub::ListenResult::State;
    KeyRig r;
    r.Game();
    r.src.Press(Key::Q); // held as listening starts
    REQUIRE(r.hub.BeginListen(kKeyboardDeviceId));
    CHECK(r.hub.PollListen().state == State::Listening);
    r.src.Let(Key::Q);
    CHECK(r.hub.PollListen().state == State::Listening);
    r.src.Press(Key::Esc); // the Open Menu key, bound instead of opening the menu
    CHECK_FALSE(r.hub.PollKeyboard(r.now += 11'000'000, false).open_menu);
    const auto heard = r.hub.PollListen();
    CHECK(heard.state == State::Pressed);
    CHECK(heard.input == Key::Esc);
    CHECK_FALSE(r.hub.PollKeyboard(r.now += 11'000'000, false).open_menu);
    r.src.Let(Key::Esc);
    r.Game();

    // Volume Up and the like are never an answer; a keyboard gone is.
    REQUIRE(r.hub.BeginListen(kKeyboardDeviceId));
    r.src.Press(115);
    CHECK(r.hub.PollListen().state == State::Listening);
    r.src.raw.present = false;
    CHECK(r.hub.PollListen().state == State::Gone);
    CHECK_FALSE(r.hub.BeginListen(kKeyboardDeviceId));
}

TEST_CASE("Keys: in a game the keys play, ORed with the others; typing keeps them", "[frame_xr]") {
    KeyRig r;
    r.Game();
    r.src.Press(Key::A); // the 3DS's A, on the Frame's B control by the profile
    r.src.Press(Key::Up); // the Circle Pad up
    ControlState c = r.Game();
    CHECK(c.Get(Control::B));
    CHECK(c.GetStick(AnalogControl::StickL).y == 1.0f);
    // The stick pushed further wins (the merge's rule for every device): here the keys.
    ControlState pushed{};
    pushed.SetStick(AnalogControl::StickL, {0.0f, -0.2f});
    r.hub.PollKeyboard(r.now += 11'000'000, false);
    r.hub.Merge(r.frame, r.routes, pushed, true);
    CHECK(pushed.GetStick(AnalogControl::StickL).y == 1.0f);

    // The menu open: nothing.
    CHECK_FALSE(r.Game(false, false).Get(Control::B));

    // The game's keyboard open: the keys type, none plays; A still held after it closes
    // stays away until let go.
    CHECK_FALSE(r.Game(true).Get(Control::B));
    r.src.Press(Key::Esc); // its cancel there, not Open Menu
    CHECK_FALSE(r.hub.PollKeyboard(r.now += 11'000'000, true).open_menu);
    r.src.Let(Key::Esc);
    CHECK_FALSE(r.Game().Get(Control::B));
    r.src.Let(Key::A);
    r.Game();
    r.src.Press(Key::A);
    CHECK(r.Game().Get(Control::B));

    // The menu opened and closed (InputBridge::Release): held keys stay away until let go.
    r.hub.Release();
    CHECK_FALSE(r.Game().Get(Control::B));
    r.src.Let(Key::A);
    r.Game();
    r.src.Press(Key::A);
    CHECK(r.Game().Get(Control::B));

    // Esc opens the menu, once.
    r.src.Press(Key::Esc);
    r.now += 11'000'000;
    CHECK(r.hub.PollKeyboard(r.now, false).open_menu);
    CHECK_FALSE(r.hub.PollKeyboard(r.now += 11'000'000, false).open_menu);
    r.src.Let(Key::Esc);

    // Use This Device off: nothing, and no menu.
    GamepadMapping off = DefaultKeyboardMapping();
    off.enabled = false;
    REQUIRE(r.hub.SetMapping(kKeyboardDeviceId, off));
    CHECK(r.saved.find("keyboard use=0 name=Keyboard") != std::string::npos);
    CHECK_FALSE(r.Game().Get(Control::B));
    CHECK(r.hub.LastKeyboard().disabled);
    r.src.Press(Key::Esc);
    CHECK_FALSE(r.hub.PollKeyboard(r.now += 11'000'000, false).open_menu);
}
