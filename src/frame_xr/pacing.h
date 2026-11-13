// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame pacing: the XR side of the VR frame pacing (common/xr_pacer.h). The frame loop
// reports its latches (the moment it takes the screens for xrEndFrame, and the game frame each
// ended frame showed); the renderer's release thread asks for a slot before it releases a game
// frame (the parity gate) and reports when it was ready (the phase the frame limiter's lock
// steers). Once a second a "xr: pacing" log line, and the user folder's xr_pace_off file is looked
// for: while it exists the gate and the lock are both off (a runtime A/B in one session).

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>
#include "common/xr_pacer.h"

namespace FrameXr {

class Pacing {
public:
    /// Reads AZAHARFRAME_XR_PACE_GATE and AZAHARFRAME_XR_PACE_LOCK (0: off). The gate needs set
    /// releases (AZAHARFRAME_XR_SET_RELEASE): with them an ended frame shows one game frame.
    void Configure(bool set_release);

    // XR thread.
    /// The frame loop took the screens for this frame's layers: a release from now on lands in
    /// the next frame.
    void NoteLatchTime();
    /// After xrEndFrame, under the screens lock: the game frame it showed (0: none), the display
    /// period, and whether the game's screens were shown at all.
    void NoteLatch(uint64_t shown_serial, int64_t period_ns, bool shown);
    /// Once a second: the xr_pace_off file and the log line.
    void Second(double seconds);

    // The renderer's release thread.
    /// Before ReleaseFrame: notes when the game frame was ready against the last latch (the phase
    /// the frame limiter's lock steers, common/xr_pacer.h), then holds its release until
    /// the one on view (`released`, the last game frame released) has been in two ended frames,
    /// so none is on view for one refresh only. At most one display period and 2 ms; only at
    /// 120 Hz with set releases, while the game's screens are shown.
    void WaitSlot(uint64_t released);

private:
    bool GateActive(int64_t now_ns) const;             ///< under `mutex`
    void NotePhase(uint64_t released, int64_t ready_ns); ///< under `mutex`

    bool set_release = true;
    bool gate_switch = true;
    bool trace = false; ///< a log line per game frame (testing)

    std::atomic<int64_t> latch_lock_ns{0}; ///< NoteLatchTime

    mutable std::mutex mutex;
    std::condition_variable cv;
    Common::XrPacer::Gate gate;
    int64_t period_ns = 0;
    int64_t last_latch_ns = 0;
    bool shown = false;
    bool file_off = false;
    // Per second (guarded by `mutex`).
    std::vector<float> to_latch_ms; ///< each game frame: ready how long before it goes up
    uint32_t gate_waits = 0, gate_timeouts = 0, holds_prevented = 0;
    double gate_wait_max_ms = 0;

    Common::XrPacer::PhaseFilter filter; ///< for the log line (the lock keeps its own)
    double mean_ms = 0;
};

} // namespace FrameXr
