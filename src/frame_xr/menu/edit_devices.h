// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame padsui: the devices Edit Controls can edit, as the menu sees them. The host fills
// MenuModel::devices from the device registry (lane pads: GamepadHub::Devices, Frame
// Controllers first, then each gamepad, then Mouse when a mouse exists) and each device's
// mapping; the menu draws the device picker, the 3DS drawing's targets in the selected device's
// own button names and the Pointer Test from it. No SDL, OpenXR or settings here.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "frame_xr/menu/remap.h"

namespace FrameXr::Menu {

/// As lane pads' Input::DeviceKind.
enum class DeviceKind : uint8_t { Frame, Gamepad, Mouse };

/// Whose names a device's buttons have (from lane pads' PadType, NamesFor).
enum class ButtonNames : uint8_t {
    Frame,       ///< the Steam Frame controllers: A B X Y, bumpers, triggers, Menu, View
    Generic,     ///< a gamepad of no known family: A B X Y as SDL names them
    Xbox,        ///< A B X Y, LB RB LT RT, View, Menu
    DualShock,   ///< Cross Circle Square Triangle, L1 R1 L2 R2, Share, Options, L3 R3
    DualSense,   ///< as DualShock, Create for Share
    Nintendo,    ///< by label as SDL reports a Switch pad (Control::A is the right button, A)
    NintendoPos, ///< a Switch pad behind Steam Input: by position (Control::A is the bottom, B)
    Steam,       ///< Steam Controller, Steam Deck: A B X Y, LB RB LT RT, View, Menu, grips
    Mouse,       ///< AzaharFrame padsui: Left Click... Wheel Down (Input::MouseInput)
};

/// The names of lane pads' PadType (its SDL_GameControllerType numbers, Steam = 100).
ButtonNames NamesFor(int pad_type, bool steam_input);

/// No control on any 3DS button.
inline constexpr RemapTable kNoMapping = [] {
    RemapTable t{};
    t.fill(-1);
    return t;
}();

struct EditDevice {
    std::string key;  ///< lane pads' DeviceInfo::key: "frame", "mouse", a gamepad's GUID key
    std::string name; ///< "Frame Controllers", as a gamepad names itself, "Mouse"
    DeviceKind kind = DeviceKind::Frame;
    ButtonNames names = ButtonNames::Frame;
    bool has_touchpad = false;
    RemapTable mapping = kNoMapping; ///< 3DS button -> the device's Input::Control, or -1
    /// Circle Pad, C-Stick -> an encoded Input::StickRoute of the device's sticks, or -1.
    RemapSticks sticks = kDefaultSticks;
    /// The mouse: Touch Screen, Open Menu -> an Input::MouseInput, or -1.
    RemapExtras extras = kNoExtras;
    int mouse_mode = 0;   ///< the mouse: 0 Bottom Screen, 1 Pointer
    bool recentre = true; ///< the mouse and a touchpad: Recentre at Edge
    bool operator==(const EditDevice&) const = default;
};

/// Where the touchpad or the mouse touches the bottom screen now, for the Pointer Test.
struct PointerTest {
    bool shown = false; ///< on the bottom screen
    bool down = false;  ///< touching (Touchpad Touch: Click, or the mouse button)
    float x = 0.5f;     ///< 0 left to 1 right
    float y = 0.5f;     ///< 0 top to 1 bottom
    bool operator==(const PointerTest&) const = default;
};

struct EditDevices {
    std::vector<EditDevice> list; ///< Frame Controllers first
    int selected = 0;             ///< index into `list`
    PointerTest pointer;          ///< the selected device's
    bool operator==(const EditDevices&) const = default;
    const EditDevice* Selected() const {
        return selected >= 0 && selected < static_cast<int>(list.size()) ? &list[selected] : nullptr;
    }
};

/// The name of `control` (an Input::Control) on a device with `names` ("B", "Cross", "L1",
/// "Left Bumper"), or "" for -1. Frame Controllers with `face_by_position` (Face Buttons: By
/// Position): the Frame's A drives Control::B (mapper.cpp), so Control::B is "A".
std::string SourceName(ButtonNames names, int control, bool face_by_position = true);
/// "Left Stick", "Right Stick", "Left Stick Flipped" (its axes swapped or turned round), or "".
std::string StickRouteName(int8_t route);

constexpr RemapSource SourceOf(DeviceKind kind) {
    return kind == DeviceKind::Frame   ? RemapSource::Frame
           : kind == DeviceKind::Mouse ? RemapSource::Mouse
                                       : RemapSource::Pad;
}

/// The Pointer Test shows for the mouse and for gamepads with a touchpad.
inline bool HasPointer(const EditDevice& d) {
    return d.kind == DeviceKind::Mouse || d.has_touchpad;
}
/// Every device gives the 3DS buttons (AzaharFrame padsui: the mouse too).
inline bool HasButtons(const EditDevice&) {
    return true;
}
/// The device has sticks for the Circle Pad and the C-Stick (not the mouse).
inline bool HasSticks(const EditDevice& d) {
    return d.kind != DeviceKind::Mouse;
}
/// Recentre at Edge shows: the mouse in Bottom Screen mode, a touchpad.
inline bool HasRecentre(const EditDevice& d) {
    return d.kind == DeviceKind::Mouse ? d.mouse_mode == 0 : d.has_touchpad;
}

/// A gamepad's mapping by position, as lane pads' DefaultGamepadMapping by position for an
/// Xbox-style pad (the right button is the 3DS's A, the bottom B, the top X, the left Y; bumpers
/// L and R, triggers ZL and ZR, Menu Start, View Select): for the menu's previews and tests.
RemapTable PositionalPadTable();

} // namespace FrameXr::Menu
