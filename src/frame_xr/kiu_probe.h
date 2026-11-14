// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiugyro: Light Gun's link to the game (Gyro Aim, input/gyro_aim.h). The XR thread
// publishes where the ray hits the top screen (Publish, once a display frame). AzaharFrame
// kiuaim2: Direct Aim is absolute. On the emulation thread, at each HID pad update (about 234
// times a second of the game's time) and at each VBlank (common/frame_emu_poll.h), the probe reads
// the reticle from Kid Icarus: Uprising's memory and writes the pixel the ray hits into it
// (+0xA4, +0xA8; Input::DirectAim), with the freshest sample (Input::AimSampler). The game adds
// its own moves to that pixel and clamps it, so a pixel written between two of its frames is where
// the next one starts. The mod's gyro (the HID gyroscope poll, ~101 Hz, input_common MotionState's
// override) gets zero, or the Turn Zone's push. When it cannot read the reticle nothing is
// written, the override steps aside and the gyro the XR thread set (Gyro, the fallback) goes
// through; the reason is logged once ("xr: Light Gun:").
//
// Guards, every read: one of the game's title ids; the mod's three code patterns found in the
// loaded code (kiu_scan.h); the reticle pointer mapped, 4-byte aligned, its first word (the
// object's vtable) inside the game's code and the same as when first seen; the screen size 2 to
// 4096 pixels, its centre inside it; the reticle within one screen of it. Any control scheme
// (AzaharFrame kiufix2: the mod's gyro aims in the C-Stick scheme too). The code is scanned once
// per boot of the game (the data segment and what follows: the mod's code; about 2 MB), and again
// at most every 10 s while nothing was found. Writes go only to the two floats of that object,
// only while every guard holds, and never while the game places the reticle itself (+0xD5).

#pragma once

#include <cstdint>
#include "frame_xr/input/gyro_aim.h"

namespace FrameXr::Kiu {

struct LightGunFrame {
    bool enabled = false; ///< Gyro Aim is Light Gun, a game runs and the menu is closed
    Input::LightGunTarget target;
    float zone_fraction = 0.0f;
    float turn_speed = 1.0f;
    bool clutch = false;
    /// AzaharFrame kiufix2: the C-Stick pushed (Input::CStickTakesAim): the stick steers, Direct
    /// Aim pauses as for `clutch` and eases back in after.
    bool stick = false;
};

/// XR thread: this frame's Light Gun input. Installs the override the first time it is enabled.
void Publish(const LightGunFrame& frame);
/// XR thread: Light Gun off (the game ends, the menu opens, VR stops). The override stays
/// installed and does nothing.
void Stop();
/// Whether the last emulation-thread read found the reticle (for the log and the tests).
bool Locked();
/// AzaharFrame kiuaim2: the last reticle read in this game (kept while the menu is open; reset
/// when the game changes or stops), for Check Aim's targets. Any thread.
Input::ReticleReading LastReading();

} // namespace FrameXr::Kiu
