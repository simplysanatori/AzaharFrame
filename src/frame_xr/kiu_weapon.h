// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: what Kid Icarus: Uprising's memory says about the player's weapon, read on
// the emulation thread (the HID pad update hook, common/frame_emu_poll.h) and handed to the XR
// thread through atomics:
//
//   - the equipped weapon (kiu_scan.h ScanForWeapon), 4 times a second of the game's time,
//     settled for half a second before it changes (WeaponSettle); single player only (slot 0;
//     another fighter in the records: none, the controller is drawn);
//   - the shots (ScanForShots), the save's shots-fired record about once a game frame, for
//     Recoil's pulse on each real shot.
//
// Guards, every read: a Kid Icarus: Uprising title; each pattern found exactly once in the
// game's code (scanned once per boot from the code set's own image, nothing read through the
// guest); every pointer non-zero, aligned and mapped; the weapon -1 to 107 in every slot and the
// player's record of kind 1 or 2; the shots no more than the game's cap. Nothing is written.
// What is read is wanted only while Gyro Aim is on (SetWanted); otherwise nothing is read.

#pragma once

#include <cstdint>

namespace FrameXr::Kiu {

/// XR thread, each frame: what to read (installs the hook the first time). AzaharFrame kiufix:
/// `scheme`, the game's control scheme (ControlScheme).
void SetWanted(bool weapon, bool shots, bool scheme = false);

/// XR thread: the settled weapon id, or -1 (none, not known, not wanted).
int EquippedWeapon();
/// XR thread: whether the shots record is being read (else Recoil keeps its timed pulses).
bool ShotsReadable();
/// XR thread: the shots seen since the start (grows by the shots of each read).
uint64_t ShotTotal();

/// AzaharFrame kiufix: the game's own control scheme (its options, the object the shots record
/// is in: kiu_scan.h kOptionsScheme), read 4 times a second while wanted: 0 C-Stick, 1 the
/// C-Stick & Gyro mod's gyro (the only one Direct Aim and Gyro Aim work with), 3 Circle Pad; -1
/// not known (not wanted, not read yet, not readable). Aiming's hint uses it: the gyro is not
/// polled at all in another scheme, so Light Gun's own probe never runs there and cannot tell.
int ControlScheme();

} // namespace FrameXr::Kiu
