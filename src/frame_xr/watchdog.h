// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// A diagnostic for frame loop stalls (AZAHARFRAME_XR_WATCHDOG=1, off by default): the frame
// loop marks each phase it enters (Beat); a thread polling every 5 ms logs a phase that lasts
// over 50 ms, with the kernel state and wait channel (/proc/self/task/<tid>/wchan) of the frame
// loop's thread and of the emulator's busy threads. Off: Beat and Idle are one relaxed load.

#pragma once

namespace FrameXr::Watchdog {

void Start();
void Stop();
/// The frame loop's thread enters `phase` (a string literal).
void Beat(const char* phase);
/// The frame loop is not in a frame (flat, or the session not running): nothing to watch.
void Idle();

} // namespace FrameXr::Watchdog
