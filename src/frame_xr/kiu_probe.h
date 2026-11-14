// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiugyro: Light Gun's link to the game (Gyro Aim, input/gyro_aim.h). The XR thread
// publishes where the ray hits the top screen (Publish); on the emulation thread, each time the
// HID service reads the gyroscope (~101 Hz; input_common MotionState's override), the probe reads
// the reticle from Kid Icarus: Uprising's memory, read only, and LightGunLoop turns the gap into
// the mod's gyro rate. When it cannot read the reticle the override steps aside and the gyro the
// XR thread set (Gyro, the fallback) goes through; the reason is logged once ("xr: Light Gun:").
//
// Guards, every read: one of the game's title ids; the mod's three code patterns found in the
// loaded code (kiu_scan.h); the reticle pointer mapped, 4-byte aligned, its first word (the
// object's vtable) inside the game's code and the same as when first seen; the screen size 2 to
// 4096 pixels, its centre inside it; the reticle within one screen of it; the game's control
// scheme the mod's gyro (otherwise the mod reads no gyroscope at all). The code is scanned once
// per boot of the game (the data segment and what follows: the mod's code; about 2 MB), and again
// at most every 10 s while nothing was found.

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
};

/// XR thread: this frame's Light Gun input. Installs the override the first time it is enabled.
void Publish(const LightGunFrame& frame);
/// XR thread: Light Gun off (the game ends, the menu opens, VR stops). The override stays
/// installed and does nothing.
void Stop();
/// Whether the last emulation-thread read found the reticle (for the log and the tests).
bool Locked();

} // namespace FrameXr::Kiu
