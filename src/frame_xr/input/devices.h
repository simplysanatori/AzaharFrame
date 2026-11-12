// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "frame_xr/input/gamepads.h"

// AzaharFrame pads
// ============================================================================================
//  The device API for Edit Controls (lane padsui). Every call is safe from any thread (the XR
//  thread included) and cheap: no file or SDL work beyond reading the pads' state, the mapping
//  file is read once and written only by SetMapping / SetButton / ResetMapping.
//
//  Devices
//    List()            Frame Controllers (id "frame", always first), then each gamepad (SDL;
//                      Bluetooth or USB), connected ones first, then "mouse" when a real mouse
//                      is plugged in or paired (/proc/bus/input/devices: a mouse handler with
//                      relative X and Y on USB or Bluetooth, not Valve's virtual ones). Hot-plug
//                      shows at once for gamepads, within 2 s for the mouse. A gamepad that went
//                      away stays listed with connected = false until the program ends.
//    Revision()        changes whenever the list, a mapping or Face Buttons changes: redraw.
//
//  Mappings: the 3DS button is the key, the device input the value.
//    GamepadMapping::buttons[Settings::NativeButton::X] = the input that drives the 3DS's X:
//      - a gamepad: a PadInput (South, East... by position; PadInputName(type, value) gives the
//        pad's own word: "A", "Cross", "L1", "ZL"), -1 none. The numbers are Menu::PadControl's.
//      - "frame": an Input::Control, as the controls profile holds it (read-only here: the Frame
//        is edited through the remap lane's SetButtons, as now).
//    GamepadMapping::analogs[Settings::NativeAnalog::CirclePad] = PadStick (0 left, 1 right).
//    GetMapping(id)            the mapping in use (the pad's own, else its default).
//    GetDefaultMapping(id)     the default (positional, follows Face Buttons).
//    SetButton(id, b, input)   one 3DS button to one input (-1 clears it); saves.
//    SetMapping(id, mapping)   the whole table; a table equal to the default removes the pad's
//                              own entry. Saves. False for "frame", "mouse", unknown ids.
//    ResetMapping(id)          back to the default; saves.
//    Mappings are per model (two DualShock 4s share one), kept in user/controller-mappings.txt.
//
//  Listening (the UI picked a 3DS button and waits for the device's button):
//    BeginListen(id)   start; inputs already held count only once let go. False if the pad is
//                      not connected (or is "frame" / "mouse": the Frame has its own path).
//    PollListen()      every frame: Listening, Pressed (input = the PadInput, delivered once),
//                      Gone (the pad went away), Idle. The press is kept from the game.
//    CancelListen()    stop.
//
//  Touchpad Touch and the Pointer Test
//    SetTouchpadDevice(id)  the gamepad whose touchpad Touchpad Touch follows; "" = whichever
//                           is touched (the default).
//    GetTouchpad(id)        that pad's touchpad now (present false without one): x, y 0..1
//                           from the top left, contact, click. For "mouse" use
//                           InputCommon::FrameXr::GetMouseState().
//
//  The game: in VR, with a game running and the in-game menu closed, every connected gamepad
//  (but the Frame's own pad and Steam echoes of it) is merged into the Frame's virtual pad each
//  frame (InputBridge). While the menu is open gamepads reach nothing, and what is held as it
//  closes stays away from the game until let go. AZAHARFRAME_PADS=0 turns the merge off.
// ============================================================================================

namespace FrameXr::Input::Devices {

std::vector<DeviceInfo> List();
uint32_t Revision();

GamepadMapping GetMapping(const std::string& id);
GamepadMapping GetDefaultMapping(const std::string& id);
bool SetMapping(const std::string& id, const GamepadMapping& mapping);
/// `button` a Settings::NativeButton, `input` a PadInput or -1.
bool SetButton(const std::string& id, int button, int input);
bool ResetMapping(const std::string& id);

bool BeginListen(const std::string& id);
GamepadHub::ListenResult PollListen();
void CancelListen();

void SetTouchpadDevice(const std::string& id);
std::string TouchpadDevice();
PadSample GetTouchpad(const std::string& id);

/// The process-wide hub (InputBridge, the tests of the program glue).
GamepadHub& Hub();

} // namespace FrameXr::Input::Devices
