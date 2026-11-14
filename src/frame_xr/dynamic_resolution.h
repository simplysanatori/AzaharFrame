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
// and down. AzaharFrame ui2: good seconds count only kHoldAfterDown seconds after the last step
// down's settle.
//
// AzaharFrame flash3 (report 70, session 573: it only ever went down, 6x to 3x in MK7):
//   - a second that is not bad ends a bad run, so a step down needs kDownAfter bad seconds truly
//     in a row (an ordinary second left the count as it was, and the step to 3x counted a bad
//     second from 24 s before);
//   - a step up is judged by its predicted cost: the GPU's time now times the pixels of the next
//     factor over this one's, ((f + 1) / f)^2, under kUpBudget of the bad line. The old rule (a
//     quarter of the budget, 2.08 ms at 120 Hz) was under the GPU's floor in MK7 (3 ms even at
//     3x), so it never went up. The failed-step blocking above is kept;
//   - the floor (FloorFactor): 1x by default, as before, so in the immersive modes it may go
//     under the Internal Resolution setting (they add two steps over it); kFloorAtSetting stops
//     it at the setting itself instead.

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
    /// AzaharFrame flash3: the factor drawn in this second (0: not known; Judge then takes the
    /// steps up to `max_drop` as the factors down to 1x).
    int factor = 0;
};

/// AzaharFrame flash3: whether Dynamic Resolution stops at the Internal Resolution setting
/// (true) or may go down to 1x (false, as in 1.1.1: in the immersive modes, which draw two steps
/// over the setting, it can go under it, e.g. 3x under a 4x setting). The owner's choice.
inline constexpr bool kFloorAtSetting = false;

/// AzaharFrame flash3: the lowest factor it draws at for the renderer's factor `base` (with the
/// immersive modes' steps) and the Internal Resolution setting `setting` (0: Auto), and how many
/// steps that leaves (MaxDrop). Internal Resolution 1x leaves none either way.
constexpr int FloorFactor(int base, int setting, bool floor_at_setting = kFloorAtSetting) {
    if (setting == 1) {
        return base; // at 1x it has nothing to do (the menu's row is locked)
    }
    const int floor = floor_at_setting && setting > 1 ? setting : 1;
    return floor < base ? floor : base;
}
constexpr int MaxDrop(int base, int setting, bool floor_at_setting = kFloorAtSetting) {
    return base > 1 ? base - FloorFactor(base, setting, floor_at_setting) : 0;
}

/// The decisions, without threads or settings (unit-tested).
class Controller {
public:
    /// The GPU's time as a share of the display period: from kBadBudget on it is the limit.
    static constexpr double kBadBudget = 0.9;
    /// AzaharFrame flash3: a second has room for a step up when the GPU's time, scaled to the
    /// next factor's pixels, is under this share of the bad line (5.6 ms at 120 Hz). AzaharFrame
    /// ui2's rule was a quarter of the budget (0.5 before it: in report 14's logs 3 of 4 steps
    /// up from 2.2-2.9 ms at 120 Hz failed within 8 s), whatever the factor; in session 573 MK7
    /// never went under 3 ms, even at 3x, so it never went up again.
    static constexpr double kUpBudget = 0.75;
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

    /// Whether a second is bad (the GPU is the limit) or good (it has room for the next factor
    /// up from `factor`, AzaharFrame flash3); neither: neutral.
    static bool Bad(const Second& s);
    static bool Good(const Second& s, int factor);
    /// AzaharFrame flash3: the GPU's time predicted at `factor` + 1 from the time at `factor`.
    static double PredictUp(double gpu_ms, int factor);

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
