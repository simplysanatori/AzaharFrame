// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: Dynamic Resolution (Settings::values.vr_dynamic_resolution, Off by
// default).
//
// The rule: Internal Resolution is the ceiling and 1x the floor. Dynamic Resolution only ever
// draws below the Internal Resolution setting, one step at a time, never under 1x; at 1x it has
// nothing to do (the menu shows its row disabled). It lowers the resolution only when the GPU is
// the limit: the GPU's time on the game's frames is near or over the headset's frame budget (its
// display period). Frames missed while the GPU has room are the emulation thread's (CPU-bound):
// a lower resolution would not help them, so they never step it down.
//
// What it measures: each second the XR thread's summary (Runtime::LogSummary) gives the display
// period and how long the GPU took to finish each game frame's last work after it was queued
// (video_core/vr_dynamic_resolution.h). A GPU that keeps up finishes it in a few ms; one that
// cannot is still on earlier work, close to or past a whole display period. Whether the game
// runs at 30 or 60 fps does not count; seconds without game frames (paused, the menu open) are
// not judged, and a started or stopped game starts it again from the setting (AzaharFrame ui2:
// Restart too). Turned off and on again in a game, it carries on at the level it had.
//
// Not jarring: a step needs kDownAfter bad (or kUpAfter good) seconds in a row, one step at a
// time; the seconds after a step are not judged (the step's own hitch: the rasterizer cache
// starts again at the new factor); and a step up the GPU cannot hold blocks that level for
// kRetryWait, and for the rest of the game the second time, so it settles instead of going up
// and down. AzaharFrame ui2: a step up needs the GPU under a quarter of the budget, and good
// seconds count only kHoldAfterDown seconds after the last step down's settle.

#pragma once

#include <cstdint>

namespace FrameXr {

namespace DynamicResolution {

/// One second (about) of the XR thread's frames.
struct Second {
    double seconds = 1.0;
    uint32_t display_frames = 0;
    double period_ms = 0.0;  ///< the runtime's display period
    uint32_t emu_frames = 0; ///< game frames (0: paused, the menu, no game)
    uint32_t gpu_frames = 0; ///< game frames whose GPU finish was timed
    double gpu_ms = 0.0;     ///< their average time from the last submit to the fence
    bool live = true;        ///< a game is running (its screens are live)
};

/// The decisions, without threads or settings (unit-tested).
class Controller {
public:
    /// The GPU's time as a share of the display period: from kBadBudget on it is the limit,
    /// under kGoodBudget it has room for a step up.
    static constexpr double kBadBudget = 0.9;
    /// AzaharFrame ui2: 0.5 was too eager: in report 14's logs 3 of 4 steps up from 2.2-2.9 ms
    /// at 120 Hz (a third of the budget) failed within 8 s; the GPU's time jumps from a few ms
    /// to over a frame at the next level.
    static constexpr double kGoodBudget = 0.25;
    static constexpr uint32_t kMinGpuFrames = 5; ///< fewer timed frames say nothing
    static constexpr int kDownAfter = 3;        ///< bad seconds in a row for a step down
    static constexpr int kUpAfter = 10;         ///< good seconds in a row for a step up
    static constexpr int kSettle = 5;           ///< seconds not judged after a step
    /// AzaharFrame ui2: after a step down (and kSettle), seconds whose good ones do not count
    /// yet, so it does not bounce straight back up; bad ones still do.
    static constexpr int kHoldAfterDown = 5;
    static constexpr int kFailWindow = 20;      ///< a step down this soon after a step up fails
    static constexpr int kRetryWait = 120;      ///< seconds before that level is tried again
    static constexpr int kMaxSteps = 32;

    /// Judges a second; `max_drop` is how far it may go (the factor less 1). Returns the steps
    /// under the factor to draw at now.
    int Judge(const Second& s, int max_drop);
    int Drop() const {
        return drop;
    }
    /// From the factor itself again (the setting changed, a game started or stopped).
    void Reset();
    /// AzaharFrame ui2: carries on from a copy taken when it was turned off (the same game):
    /// the streaks start again and, at a lower level, the seconds after are not judged.
    void Resume();

    /// Whether a second is bad (the GPU is the limit) or good (it has room); neither: neutral.
    static bool Bad(const Second& s);
    static bool Good(const Second& s);

private:
    int drop = 0;
    int bad = 0;
    int good = 0;
    int settle = 0;
    int hold = 0; ///< AzaharFrame ui2: kHoldAfterDown left
    double now = 0.0; ///< seconds since the start
    double last_up = -1e9;
    /// Per drop level (the level a step up went to): failed step ups and until when it waits.
    int failed[kMaxSteps] = {};
    double blocked_until[kMaxSteps] = {};
};

/// Frame loop side: the controller for the running session, fed by Runtime::LogSummary.
void Install();
void Uninstall();
/// XR thread, once a second: judges it when the setting is on (else nothing is taken off).
void NoteSecond(const Second& s);
/// The steps taken off now.
int Drop();
/// Any thread: from the factor itself again.
void Reset();
/// AzaharFrame ui2: the setting was turned off or on in this game. Off: the full factor at once,
/// and the controller is kept aside; on again: it carries on where it was (its level and what it
/// learnt), so turning it off and on is not a start from the full factor that needs bad seconds
/// all over again. A new game or a new Internal Resolution drops the copy (Reset).
void SetEnabled(bool on);

} // namespace DynamicResolution

} // namespace FrameXr
