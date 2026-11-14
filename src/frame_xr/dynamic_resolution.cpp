// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <mutex>
#include <string>

#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/dynamic_resolution.h"
#include "video_core/vr_dynamic_resolution.h"

// AzaharFrame menuux
namespace FrameXr::DynamicResolution {

bool Controller::Bad(const Second& s) {
    // The GPU is the limit. Missed display frames with the GPU under budget are CPU-bound: a
    // lower resolution does nothing for them.
    return s.period_ms > 0.0 && s.gpu_frames >= kMinGpuFrames &&
           s.gpu_ms >= kBadBudget * s.period_ms;
}

double Controller::PredictUp(double gpu_ms, int factor) { // AzaharFrame flash3
    const double f = std::max(1, factor);
    return gpu_ms * ((f + 1.0) / f) * ((f + 1.0) / f);
}

bool Controller::Good(const Second& s, int factor) {
    // AzaharFrame flash3: the next factor's cost, from this one's pixels to the next one's,
    // well under the bad line, so the step up holds.
    return s.period_ms > 0.0 && s.gpu_frames >= kMinGpuFrames &&
           PredictUp(s.gpu_ms, factor) < kUpBudget * kBadBudget * s.period_ms;
}

int Controller::Judge(const Second& s, int max_drop) {
    now += std::max(0.0, s.seconds);
    max_drop = std::clamp(max_drop, 0, kMaxSteps - 1);
    drop = std::min(drop, max_drop);
    if (!s.live) {
        Reset(); // no game: the next one starts from the setting
        return drop;
    }
    if (s.emu_frames == 0 || s.seconds <= 0.0 || s.period_ms <= 0.0) {
        bad = good = 0; // paused or the menu: nothing to judge, the streaks start again
        return drop;
    }
    if (settle > 0) {
        settle--;
        return drop;
    }
    const bool holding = hold > 0; // AzaharFrame ui2
    if (holding) {
        hold--;
    }
    // AzaharFrame flash3: the factor drawn now (without one, the steps down to 1x).
    const int factor = s.factor > 0 ? s.factor : max_drop + 1 - drop;
    if (Bad(s)) {
        good = 0;
        if (++bad >= kDownAfter && drop < max_drop) {
            if (now - last_up <= kFailWindow) {
                // The step up to this level could not be held: wait before trying it again,
                // and after the second time not again in this game.
                const int level = drop;
                failed[level]++;
                blocked_until[level] = failed[level] >= 2 ? 1e18 : now + kRetryWait;
                last_up = -1e9;
            }
            drop++;
            bad = 0;
            settle = kSettle;
            hold = kHoldAfterDown; // AzaharFrame ui2
        }
    } else {
        // AzaharFrame flash3: any second that is not bad ends a bad run (only a good one did).
        bad = 0;
        if (Good(s, factor) && !holding) {
            if (++good >= kUpAfter && drop > 0 && now >= blocked_until[drop - 1]) {
                drop--;
                good = 0;
                settle = kSettle;
                last_up = now;
            }
        } else {
            good = 0; // neither (or good while it holds): the good seconds must come in a row
        }
    }
    return drop;
}

void Controller::Reset() {
    *this = Controller{};
}

void Controller::Resume() { // AzaharFrame ui2
    bad = good = 0;
    settle = drop > 0 ? kSettle : 0; // the step back down is a hitch of its own
    hold = drop > 0 ? kHoldAfterDown : 0;
}

namespace {

std::mutex g_mutex;
Controller g_controller;
bool g_installed = false;
// AzaharFrame ui2: the controller as it was when the setting went off in this game, and the
// setting as NoteSecond last saw it (a change from elsewhere, Azahar's window, counts too).
Controller g_saved;
bool g_have_saved = false;
bool g_was_on = false;

void Publish(int drop) {
    VideoCore::DynamicResolution::g_drop.store(static_cast<u32>(std::max(0, drop)),
                                              std::memory_order_relaxed);
}

// AzaharFrame flash3: every change of the level is logged ("xr: Dynamic Resolution: "), also the
// ones that are not a judged step (turned off, a new game or setting with steps taken off).
void LogBack(int before, const char* why) {
    if (before == 0) {
        return;
    }
    const u32 base = VideoCore::DynamicResolution::g_base.load(std::memory_order_relaxed);
    if (base > 1) {
        LOG_INFO(Frontend, "xr: Dynamic Resolution: {}x (was {}x; {})", base,
                 base > static_cast<u32>(before) ? base - before : 1, why);
    } else {
        LOG_INFO(Frontend, "xr: Dynamic Resolution: the full factor (was {} steps under it; {})",
                 before, why);
    }
}

// AzaharFrame ui2: the setting's change, under g_mutex.
void TurnOff() {
    LogBack(g_controller.Drop(), "turned off"); // AzaharFrame flash3
    g_saved = g_controller;
    g_have_saved = true;
    g_controller.Reset();
    Publish(0);
}

void TurnOn() {
    if (g_have_saved) {
        const int before = g_controller.Drop();
        g_controller = g_saved;
        g_controller.Resume();
        g_have_saved = false;
        Publish(g_controller.Drop());
        if (g_controller.Drop() != before) {
            LOG_INFO(Frontend, "xr: Dynamic Resolution: on again, {} steps under the setting as before",
                     g_controller.Drop());
        }
    }
}

void ResetLocked(const char* why) {
    LogBack(g_controller.Drop(), why); // AzaharFrame flash3
    g_controller.Reset();
    g_have_saved = false;
    Publish(0);
}

} // namespace

void Install() {
    std::scoped_lock lock{g_mutex};
    ResetLocked("VR started");
    g_installed = true;
    g_was_on = Settings::values.vr_dynamic_resolution.GetValue();
}

void Uninstall() {
    std::scoped_lock lock{g_mutex};
    ResetLocked("VR stopped");
    g_installed = false;
}

void NoteSecond(const Second& s) {
    std::scoped_lock lock{g_mutex};
    if (!g_installed) {
        return;
    }
    const bool on = Settings::values.vr_dynamic_resolution.GetValue();
    if (on != g_was_on) { // AzaharFrame ui2: changed where SetEnabled did not see it
        g_was_on = on;
        if (on) {
            TurnOn();
        } else if (!g_have_saved) {
            TurnOff();
        }
    }
    if (!on) {
        return;
    }
    // The factor the renderer works out (Internal Resolution, with the immersive modes' own
    // steps) is the ceiling; the floor is FloorFactor's (1x, or the setting with
    // kFloorAtSetting); at Internal Resolution 1x nothing is taken off.
    const u32 base = VideoCore::DynamicResolution::g_base.load(std::memory_order_relaxed);
    const int setting = static_cast<int>(Settings::values.resolution_factor.GetValue());
    const int before = g_controller.Drop();
    Second judged = s;
    judged.factor = base > static_cast<u32>(before) ? static_cast<int>(base) - before : 1;
    const int drop = g_controller.Judge(judged, MaxDrop(static_cast<int>(base), setting));
    if (drop != before) {
        Publish(drop);
        // AzaharFrame flash3: why (a step up gives the cost it predicted for the new factor).
        const std::string why =
            drop > before
                ? fmt::format("{} bad seconds in a row", Controller::kDownAfter)
                : fmt::format("{} good seconds in a row, predicted {:.1f} ms at the new factor",
                              Controller::kUpAfter,
                              Controller::PredictUp(s.gpu_ms, judged.factor));
        LOG_INFO(Frontend,
                 "xr: Dynamic Resolution: {}x (was {}x; {}; {} display frames in {:.1f} s at "
                 "{:.2f} ms, GPU {:.1f} ms behind over {} of {} game frames)",
                 base > static_cast<u32>(drop) ? base - drop : 1,
                 base > static_cast<u32>(before) ? base - before : 1, why, s.display_frames,
                 s.seconds, s.period_ms, s.gpu_ms, s.gpu_frames, s.emu_frames);
    }
}

int Drop() {
    std::scoped_lock lock{g_mutex};
    return g_controller.Drop();
}

void Reset() {
    std::scoped_lock lock{g_mutex};
    ResetLocked("from the factor itself again");
}

void SetEnabled(bool on) {
    std::scoped_lock lock{g_mutex};
    if (on == g_was_on) {
        return;
    }
    g_was_on = on;
    if (on) {
        TurnOn();
    } else {
        TurnOff();
    }
}

} // namespace FrameXr::DynamicResolution
