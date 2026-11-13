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

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>
#include "common/xr_pacer.h"

namespace FrameXr {

class Pacing {
public:
    /// Reads AZAHARFRAME_XR_PACE_GATE, AZAHARFRAME_XR_PACE_PICTURES (0: the gate by release, not
    /// by game picture) and AZAHARFRAME_XR_PACE_LOCK (0: off). The gate needs set
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
    /// Before ReleaseFrame. `released`: the last game frame released; `game_swaps`: the game's
    /// top screen swaps when this one was drawn, which tells a new picture from the one before
    /// again (AzaharFrame pacing2). A new picture: notes when it was ready against the last latch
    /// (the phase the frame limiter's lock steers, common/xr_pacer.h), then holds its release
    /// until the picture on view has been in 2 x the game's cadence ended frames (2 at 60
    /// pictures a second, 4 at 30), so none is on view for less. At most one display period and
    /// 2 ms at 60, the frames still needed and 2 ms at 30 or 20. The same picture again goes at
    /// once. Only at 120 Hz with set releases, while the game's screens are shown.
    void WaitSlot(uint64_t released, uint64_t game_swaps);

private:
    bool GateActive(int64_t now_ns) const; ///< under `mutex`
    /// Under `mutex`; a phase sample for the lock and the log only for a new picture.
    void NotePhase(uint64_t released, uint64_t released_picture, bool is_new,
                   uint32_t cadence_now, int64_t ready_ns);
    uint64_t PictureOf(uint64_t serial) const; ///< under `mutex` (0: none, or too old)
    /// Under `mutex`: ended frames the next release still has to wait for (0: it may go).
    uint32_t Needed(uint64_t released, uint64_t released_picture, bool is_new,
                    uint32_t cadence_now) const;

    bool set_release = true;
    bool gate_switch = true;
    bool pictures_switch = true; ///< AZAHARFRAME_XR_PACE_PICTURES (0: by release, as before)
    bool trace = false; ///< a log line per game frame (testing)

    std::atomic<int64_t> latch_lock_ns{0}; ///< NoteLatchTime

    mutable std::mutex mutex;
    std::condition_variable cv;
    Common::XrPacer::Gate gate;
    // AzaharFrame pacing2: game pictures (under `mutex`). Each release's picture by its serial.
    struct PictureEntry {
        uint64_t serial = 0;
        uint64_t picture = 0;
    };
    static constexpr std::size_t kPictureRing = 256;
    std::array<PictureEntry, kPictureRing> pictures{};
    Common::XrPacer::PictureCadence cadence;
    bool have_swaps = false;
    uint64_t last_swaps = 0;
    uint64_t picture_seq = 0;
    uint32_t new_pictures = 0, repeats = 0; ///< per second
    uint64_t cadence_sum = 0;               ///< per second, over the releases
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
