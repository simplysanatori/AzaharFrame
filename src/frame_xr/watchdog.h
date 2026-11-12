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

// AzaharFrame flatfix: Flat, always on (no variable). A frame loop that has not ticked in Flat
// for 3 s is logged once with where it is (FlatPhase), the kernel state of its thread and of
// input_common's SDL thread, and how often SDL was pumped meanwhile; a loop that ticks again is
// logged too. In Flat nothing else would show it: SetVr(true) only sets a flag the loop reads.
/// The frame loop ticks in Flat (each pass, about 120 a second).
void FlatTick();
/// Where in the Flat pass it is now (a string literal).
void FlatPhase(const char* phase);
/// The frame loop leaves Flat (a session is being made, or it stops).
void FlatEnd();
/// Seconds since the frame loop last ticked in Flat; negative when it is not in Flat.
double FlatStalledSeconds();

} // namespace FrameXr::Watchdog
