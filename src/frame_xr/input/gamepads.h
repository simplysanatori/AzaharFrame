// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "frame_xr/input/controls.h"
#include "frame_xr/input/default_bindings.h"
#include "frame_xr/input/input_frame.h"
#include "frame_xr/input/mouse.h"
#include "frame_xr/input/profile_routes.h"
#include "frame_xr/input/stick_route.h"
#include "frame_xr/input/touchpad_touch.h"
#include "input_common/main.h"

// AzaharFrame pads
// Other controllers in VR: every SDL gamepad (Bluetooth or USB: 8BitDo, DualShock, Xbox, Steam
// Controller...) plays alongside the Frame controllers, each with a 3DS mapping of its own.
// Pure logic: SDL is reached through GamepadSource (input_common in the program, a fake in the
// tests) and the mapping file through GamepadStorage. The process-wide hub and the menu's API
// are in devices.h.
//
// How a gamepad reaches the game: its raw SDL state becomes PadInputs (by position: a Nintendo
// pad that SDL reports by label is turned back), its mapping turns them into 3DS buttons and
// sticks (ApplyGamepadMapping), and those are routed into the Frame's virtual pad through the
// controls profile (ProfileRoutes: the Frame control that drives each 3DS button), ORed with the
// Frame controllers' buttons, the stick pushed further winning. So the game sees one pad, and a
// gamepad drives every 3DS button the profile gives a Frame control (with the default profile:
// A, B, X, Y, the D-pad, L, R, ZL, ZR, Start, Select, the Circle Pad and the C-Stick).

namespace FrameXr::Input {

/// A gamepad's inputs, by position: SDL_GameControllerButton's order (South = SDL's A on an
/// Xbox-style pad), then the two triggers as buttons (pressed past 0.55, let go below 0.40). The
/// values of a gamepad's ButtonTable; the same numbers as the menu's Menu::PadControl.
enum class PadInput : int {
    South,
    East,
    West,
    North,
    Back,
    Guide,
    Start,
    LeftStick,
    RightStick,
    LeftShoulder,
    RightShoulder,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    Misc1,
    Paddle1,
    Paddle2,
    Paddle3,
    Paddle4,
    Touchpad,
    LeftTrigger,
    RightTrigger,
    Count,
};
constexpr int kNumPadInputs = static_cast<int>(PadInput::Count);
static_assert(kNumPadInputs == InputCommon::SDLPadState::kButtons + 2);

/// A gamepad's sticks (the values of its AnalogTable).
enum class PadStick : int { Left = 0, Right = 1, Count = 2 };
constexpr int kNumPadSticks = static_cast<int>(PadStick::Count);

/// SDL_GameControllerType, plus Steam for Valve's own gamepads (SDL2 calls them Unknown).
enum class PadType : int {
    Unknown = 0,
    Xbox360 = 1,
    XboxOne = 2,
    PS3 = 3,
    PS4 = 4,
    SwitchPro = 5,
    Virtual = 6,
    PS5 = 7,
    AmazonLuna = 8,
    GoogleStadia = 9,
    NvidiaShield = 10,
    JoyConLeft = 11,
    JoyConRight = 12,
    JoyConPair = 13,
    Steam = 100,
};
/// "Xbox One", "DualShock 4", "Switch Pro", "Steam Controller"... ("Gamepad" for Unknown).
const char* PadTypeName(PadType type);
/// Nintendo's face button labels (A on the right): Switch Pro and Joy-Con, and the pads SDL
/// takes for them (an 8BitDo in Switch mode).
bool NintendoLabels(PadType type);
/// The name of a gamepad input in the pad's own terms: "A", "Cross", "B" (a Nintendo pad's
/// bottom button), "Left Bumper", "L1", "ZL", "Share"... "" for an invalid input.
const char* PadInputName(PadType type, int input);
/// "Left Stick", "Right Stick".
const char* PadStickName(int stick);

/// One frame of a gamepad, by position. Sticks up positive, after the dead zone.
struct PadInputs {
    std::array<bool, kNumPadInputs> pressed{};
    std::array<Vec2f, kNumPadSticks> sticks{};
    bool Get(PadInput input) const {
        return pressed[static_cast<std::size_t>(input)];
    }
};

/// SDL reports a Nintendo pad's face buttons by label (SDL2's default, kept by sdl2-compat): its
/// A is the right button. A Steam Input pad is an Xbox-style virtual pad whatever is behind it.
bool ReportsByLabel(PadType type, bool steam_virtual);

/// The raw SDL state as PadInputs. `previous` gives the triggers their hysteresis; `by_label`
/// (ReportsByLabel) swaps SDL's A and B, X and Y back to positions.
PadInputs PadInputsFromRaw(const InputCommon::SDLPadState& raw, const PadInputs& previous,
                           bool by_label, float stick_deadzone = 0.15f);

/// A 3DS mapping: for each 3DS button the input that drives it, for each 3DS stick the stick.
struct GamepadMapping {
    ButtonTable buttons{};
    AnalogTable analogs{};
    /// AzaharFrame padsui: the mouse's (its buttons and analogs above hold MouseInputs): the
    /// input that touches the bottom screen (Pointer: clicks), the one that opens the menu, its
    /// Mouse Mode and speed. Recentre at Edge is the mouse's and a touchpad's.
    int8_t touch = -1;
    int8_t open_menu = -1;
    uint8_t mode = 0;  ///< MouseMode
    uint8_t speed = kCursorSpeedNormal; ///< Cursor Speed, 0 to kNumCursorSpeeds - 1
    bool recentre = true;
    /// Use This Device: off, the device reaches nothing (Edit Controls still edits it). The
    /// Frame's is always on.
    bool enabled = true;
    bool operator==(const GamepadMapping&) const = default;
};
/// An empty mapping (every entry -1).
GamepadMapping EmptyMapping();

/// The default mapping. `by_position` is the Face Buttons setting (the Frame's default: the
/// right button is the 3DS's A, the bottom one B, the top one X, the left one Y); by label the
/// button marked A is A (on a Nintendo pad the two are the same). L and R on the shoulder
/// buttons, ZL and ZR on the triggers, Start and Select on Start and Back, the left stick the
/// Circle Pad, the right stick the C-Stick, the D-pad the D-pad. Home, Debug, Gpio14, Power:
/// none.
GamepadMapping DefaultGamepadMapping(PadType type, bool by_position);

/// AzaharFrame padsui: the mouse's default: Right Click A, Back L, Forward R, Left Click the
/// touch, Middle Click Open Menu, Bottom Screen, Normal speed, Recentre at Edge on.
GamepadMapping DefaultMouseMapping();
/// The 3DS buttons a mouse's held inputs press under `mapping` (the touch and Open Menu are not
/// buttons).
ThreeDsPadState ApplyMouseMapping(const std::array<bool, kNumMouseInputs>& held,
                                  const GamepadMapping& mapping);
/// The 3DS buttons and sticks a pad's inputs press under `mapping`.
ThreeDsPadState ApplyGamepadMapping(const PadInputs& pad, const GamepadMapping& mapping);

/// Routes a gamepad's 3DS state into the Frame's virtual pad: each pressed 3DS button presses the
/// Frame control the profile binds to it (OR); each 3DS stick replaces the bound Frame stick when
/// it is pushed further.
void MergeIntoFrame(const ThreeDsPadState& pad, const ProfileRoutes& routes,
                    ControlState& controls);

/// The routes as a mapping in the Frame's terms (values: Input::Control, Input::AnalogControl).
GamepadMapping MappingOfRoutes(const ProfileRoutes& routes);

/// The menu's terms (Edit Controls, lane padsui): a gamepad's buttons as Input::Controls, SDL's
/// own numbering (Control::A is SDL's A: the bottom button, or on a pad that reports by label,
/// ReportsByLabel, the right one), Start Menu, Back View, the stick clicks, the back paddles the
/// grips (left: Paddle2 or Paddle4, right: Paddle1 or Paddle3). Guide, Misc1 and the touchpad
/// click have no Control (-1).
int ControlOfPadInput(int input, bool by_label);
/// The other way (GripL is Paddle2, GripR Paddle1); -1 for none.
int PadInputOfControl(int control, bool by_label);
/// A gamepad mapping's buttons in the menu's terms.
ButtonTable ControlTableOf(const GamepadMapping& mapping, bool by_label);
/// `mapping` with the 3DS buttons the menu changed (`table` against ControlTableOf(mapping))
/// set from `table`; the others keep their inputs, those with no Control included.
GamepadMapping WithControlTable(GamepadMapping mapping, const ButtonTable& table, bool by_label);

/// PadType of an SDL pad (its type, or Steam for Valve's gamepads).
PadType PadTypeOf(const InputCommon::SDLPadInfo& info);

/// The key a gamepad's mapping is saved under: SDL's GUID string, with the real controller's
/// vendor and product written into it for a Steam Input pad (whose own GUID is the same for all
/// of them). Two pads of one model share it, and so their mapping.
std::string GamepadKey(const InputCommon::SDLPadInfo& info);

/// Valve's vendor id, and the products of Valve's gamepads (Steam Controller, its dongle, Steam
/// Deck, Steam Controller 2026 wired and dongle).
inline constexpr uint16_t kValveVendor = 0x28de;
bool IsValveGamepad(uint16_t vendor, uint16_t product);

/// A pad that is the Frame's own controllers seen through SDL (Valve's vendor id and not one of
/// Valve's gamepads): never listed, never merged.
bool IsFrameOwnPad(const InputCommon::SDLPadInfo& info);

// ---- Pointer devices --------------------------------------------------------------------

struct MouseInfo {
    std::string name;
    std::string event; ///< AzaharFrame padsui: its event node ("event5"), "" none
    uint16_t bus = 0;
    uint16_t vendor = 0;
    uint16_t product = 0;
};
/// The real mice in /proc/bus/input/devices' text: a "mouse" handler, relative X and Y, on USB
/// or Bluetooth, not Valve's (the Frame's and Steam's virtual devices). Touchpads (absolute)
/// are not mice.
std::vector<MouseInfo> ParseInputDevices(const std::string& text);
/// AzaharFrame padsui: Frametop's virtual mouse ("frametop virtual mouse"), which its input relay
/// feeds the real mice into while it holds them (EVIOCGRAB) and Frametop is paused for a VR game.
std::vector<MouseInfo> ParseRelayMice(const std::string& text);

// ---- Telling the Frame's own pad from a real one -------------------------------------------

/// Under Steam Input the Frame controllers may also show up as an SDL pad (Flat reads them that
/// way). Such a pad presses the same buttons as the Frame controllers at the same moments. While a
/// pad is not known to be a real one, a press it makes together with the Frame's press of the
/// same input (A with A, a bumper with that bumper...) is kept from the game, and its sticks only
/// count while the Frame's matching stick rests; after three presses that all came with the
/// Frame's it is an echo and never reaches the game; after three presses on its own it is a real
/// pad. Inputs are compared raw (SDL's numbering), as Steam Input shows the Frame.
class EchoGuard {
public:
    /// A press this close to the Frame's (either side) is the same press.
    static constexpr int64_t kWindowNs = 80'000'000;
    static constexpr int kVerdictPresses = 3;

    enum class Verdict { Unknown, Real, Echo };

    /// One frame: `frame` and `pad` are raw (FrameAsPad, and the pad's SDL buttons with the
    /// triggers). Returns which of the pad's inputs may reach the game now.
    std::array<bool, kNumPadInputs> Filter(const std::array<bool, kNumPadInputs>& frame,
                                           const std::array<bool, kNumPadInputs>& pad,
                                           int64_t now_ns);
    Verdict verdict() const {
        return verdict_;
    }
    void SetVerdict(Verdict v) {
        verdict_ = v;
    }

private:
    Verdict verdict_ = Verdict::Unknown;
    int coincident_ = 0;
    int independent_ = 0;
    std::array<bool, kNumPadInputs> pad_prev_{};
    std::array<bool, kNumPadInputs> frame_prev_{};
    std::array<int64_t, kNumPadInputs> frame_down_ns_{}; ///< the Frame's last press, 0 none
    std::array<int64_t, kNumPadInputs> pending_ns_{};    ///< a pad press waiting, 0 none
    std::array<bool, kNumPadInputs> passing_{};          ///< a pad press that reaches the game
};

/// The Frame controllers' raw state as Steam Input's pad would show it (A as SDL's A...).
std::array<bool, kNumPadInputs> FrameAsPad(const InputFrame& frame);

// ---- Mapping file -----------------------------------------------------------------------

/// user/controller-mappings.txt: one line per gamepad model with a mapping of its own,
///   <key> buttons=<18 values> analogs=<2 values> vendor=<hex> product=<hex> name=<name>
/// values comma separated (PadInput / PadStick numbers), -1 for none; the name runs to the end
/// of the line. Lines starting with # are comments. AzaharFrame padsui: the mouse is the line
/// "mouse" (MouseInputs), with touch=, menu=, mode=, speed= (Cursor Speed, 0 to 4); recentre=0
/// turns Recentre at Edge off, use=0 Use This Device. Each is written only when it is not the
/// plain value (touch and menu -1, mode 0, speed 2, recentre and use 1), and a line without them
/// reads as those, so older files read the same.
struct MappingEntry {
    GamepadMapping mapping;
    uint16_t vendor = 0;
    uint16_t product = 0;
    std::string name;
    bool operator==(const MappingEntry&) const = default;
};
class MappingFile {
public:
    void Parse(const std::string& text);
    std::string Format() const;
    /// The entry of `key`, else one with the same vendor, product and name (the same model on
    /// another bus, or with Steam Input on or off).
    const MappingEntry* Find(const std::string& key, uint16_t vendor, uint16_t product,
                             const std::string& name) const;
    void Set(const std::string& key, const MappingEntry& entry);
    /// Removes `key` and every entry Find would match for it. True when something went.
    bool Remove(const std::string& key, uint16_t vendor, uint16_t product,
                const std::string& name);
    std::size_t size() const {
        return entries_.size();
    }

private:
    std::map<std::string, MappingEntry> entries_;
};

// ---- The hub ------------------------------------------------------------------------------

/// Where the hub reads SDL and the system's pointer devices (input_common; a fake in tests).
class GamepadSource {
public:
    virtual ~GamepadSource() = default;
    virtual uint64_t Generation() = 0;
    virtual void Infos(std::vector<InputCommon::SDLPadInfo>& out) = 0;
    virtual void States(std::vector<InputCommon::SDLPadState>& out) = 0;
    /// /proc/bus/input/devices' text, or empty.
    virtual std::string InputDevices() = 0;
    /// AzaharFrame padsui: the mice's running totals (all real mice, and Frametop's relay mouse
    /// while it feeds them), present when a real mouse is connected.
    virtual MouseRaw Mouse() {
        return {};
    }
};

/// Where the mapping file lives (user/controller-mappings.txt in the program).
struct GamepadStorage {
    std::function<std::string()> load;
    std::function<bool(const std::string&)> save;
};

enum class DeviceKind : int { Frame = 0, Gamepad = 1, Mouse = 2 };

/// One entry of the menu's device list.
struct DeviceInfo {
    DeviceKind kind = DeviceKind::Gamepad;
    /// Unique in the list and stable while the program runs: "frame", "mouse", or a gamepad's
    /// GamepadKey (with "#2", "#3"... for a second pad of the same model).
    std::string id;
    /// "Frame Controllers", the gamepad's name ("8BitDo Pro 2"), the mouse's name.
    std::string name;
    PadType type = PadType::Unknown;
    uint16_t vendor = 0;
    uint16_t product = 0;
    bool has_touchpad = false;
    /// Gamepads seen earlier in this run stay listed, not connected.
    bool connected = true;
    /// Behind Steam Input (a Steam virtual pad).
    bool steam_input = false;
    /// Its own mapping is saved (else the default applies).
    bool custom = false;
    bool operator==(const DeviceInfo&) const = default;
};

inline constexpr const char* kFrameDeviceId = "frame";
inline constexpr const char* kMouseDeviceId = "mouse";

/// What the hub gives the touch for a frame.
struct TouchpadPick {
    PadSample pad;
    std::string id; ///< the device it came from, "" none
    bool recentre = true; ///< AzaharFrame padsui: its Recentre at Edge
};

/// The registry and the merge. Thread-safe: every call locks.
class GamepadHub {
public:
    GamepadHub(GamepadSource& source, GamepadStorage storage);

    /// Frame Controllers first, then the gamepads (connected first, in the order they came),
    /// then Mouse when a mouse exists (read again at most every 2 s, `now_ns`).
    std::vector<DeviceInfo> Devices(int64_t now_ns);
    /// Counts changes of the list and of the mappings.
    uint32_t Revision();

    /// The mapping in use for a device (a gamepad's own, else its default; the Frame's from
    /// `routes`, in the Frame's terms). Empty for the mouse and unknown ids.
    GamepadMapping Mapping(const std::string& id, const ProfileRoutes& routes);
    GamepadMapping DefaultMapping(const std::string& id);
    /// Saves a gamepad's own mapping (a mapping equal to its default removes it). False for the
    /// Frame (its mapping is the controls profile), the mouse, an unknown id, or a failed save.
    bool SetMapping(const std::string& id, const GamepadMapping& mapping);
    /// Back to the default. False when it had none of its own (or the id is not a gamepad's).
    bool ResetMapping(const std::string& id);

    /// Listening for the next input of one gamepad (the menu's "press a button"): inputs held
    /// when it starts count only once let go. Poll returns the first new press, once.
    bool BeginListen(const std::string& id);
    void CancelListen();
    struct ListenResult {
        enum class State { Idle, Listening, Pressed, Gone } state = State::Idle;
        int input = -1; ///< Pressed: a PadInput (the mouse: a MouseInput)
        /// AzaharFrame padsui: Listening: where the pad's sticks are (a 3DS stick listening).
        std::array<Vec2f, kNumPadSticks> sticks{};
    };
    ListenResult PollListen();

    /// The pad whose touchpad Touchpad Touch follows; empty: whichever is touched.
    void SetTouchpadDevice(const std::string& id);
    std::string TouchpadDevice();
    /// The touchpad of one device now (the Pointer Test), present false without one.
    PadSample Touchpad(const std::string& id);

    void SetFaceButtonsByPosition(bool by_position);
    /// Turns the merge off altogether (AZAHARFRAME_PADS=0).
    void SetMergeEnabled(bool enabled);

    struct MergeResult {
        TouchpadPick touchpad;
        /// Gamepads whose input reached `controls` this frame.
        int merged = 0;
    };
    /// One frame. Reads every gamepad; with `merge_buttons` (VR, a game running, no menu) their
    /// 3DS state goes into `controls` through `routes`. The touchpad is picked either way.
    MergeResult Merge(const InputFrame& frame, const ProfileRoutes& routes,
                      ControlState& controls, bool merge_buttons);
    /// Everything held now stays away from the game until let go (the menu closed, a new game).
    void Release();

    /// AzaharFrame padsui: the mouse, once a display frame (the XR thread): its frame, with
    /// its mapping's mode, speed, Recentre at Edge and touch input. The chord switching the
    /// mode saves the new mode. Merge then gives the game its buttons.
    MouseFrame PollMouse(int64_t now_ns);
    /// The mouse frame of the last PollMouse.
    MouseFrame LastMouse();

private:
    struct Pad {
        InputCommon::SDLPadInfo info;
        std::string key; ///< GamepadKey
        std::string id;  ///< DeviceInfo::id
        PadType type = PadType::Unknown;
        bool frame_own = false;
        bool by_label = false;
        bool connected = false;
        uint64_t order = 0;
        PadInputs inputs;
        std::array<bool, kNumPadInputs> raw{};
        std::array<bool, kNumPadInputs> swallowed{};
        EchoGuard echo;
        EchoGuard::Verdict logged = EchoGuard::Verdict::Unknown;
        InputCommon::TouchpadSample touch;
    };

    void RefreshLocked();
    void ReadStatesLocked();
    void LoadLocked();
    bool SaveLocked();
    Pad* FindLocked(const std::string& id);
    bool ListedLocked(const Pad& pad) const;
    GamepadMapping MappingLocked(const Pad& pad);
    GamepadMapping DefaultLocked(const Pad& pad) const;
    TouchpadPick PickTouchpadLocked();
    GamepadMapping MouseMappingLocked();
    bool SetMouseMappingLocked(const GamepadMapping& mapping);

    std::mutex mutex_;
    GamepadSource& source_;
    GamepadStorage storage_;
    bool loaded_ = false;
    MappingFile file_;
    uint64_t generation_ = 0;
    bool generation_seen_ = false;
    std::vector<Pad> pads_;
    uint64_t next_order_ = 0;
    uint32_t revision_ = 1;
    bool by_position_ = true;
    bool merge_enabled_ = true;
    bool release_pending_ = false;
    std::string touchpad_id_;
    std::string touchpad_last_id_;
    std::vector<InputCommon::SDLPadInfo> infos_;
    std::vector<InputCommon::SDLPadState> states_;
    std::vector<MouseInfo> mice_;
    int64_t mice_read_ns_ = 0;
    bool mice_read_ = false;
    // AzaharFrame padsui: the mouse.
    MouseLogic mouse_logic_;
    MouseFrame mouse_frame_;
    MouseRaw listen_mouse_;
    bool listen_is_mouse_ = false;
    // Listening.
    std::string listen_id_;
    bool listening_ = false;
    std::array<bool, kNumPadInputs> listen_held_{};
    ListenResult listen_result_;
};

} // namespace FrameXr::Input
