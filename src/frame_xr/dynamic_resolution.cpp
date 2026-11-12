// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <mutex>

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

bool Controller::Good(const Second& s) {
    return s.period_ms > 0.0 && s.gpu_frames >= kMinGpuFrames &&
           s.gpu_ms < kGoodBudget * s.period_ms;
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
        }
    } else if (Good(s)) {
        bad = 0;
        if (++good >= kUpAfter && drop > 0 && now >= blocked_until[drop - 1]) {
            drop--;
            good = 0;
            settle = kSettle;
            last_up = now;
        }
    } else {
        good = 0; // neither: the good seconds must come in a row
    }
    return drop;
}

void Controller::Reset() {
    *this = Controller{};
}

void Controller::Resume() { // AzaharFrame ui2
    bad = good = 0;
    settle = drop > 0 ? kSettle : 0; // the step back down is a hitch of its own
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

// AzaharFrame ui2: the setting's change, under g_mutex.
void TurnOff() {
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

void ResetLocked() {
    g_controller.Reset();
    g_have_saved = false;
    Publish(0);
}

} // namespace

void Install() {
    std::scoped_lock lock{g_mutex};
    ResetLocked();
    g_installed = true;
    g_was_on = Settings::values.vr_dynamic_resolution.GetValue();
}

void Uninstall() {
    std::scoped_lock lock{g_mutex};
    ResetLocked();
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
    // steps) is the ceiling, 1x the floor; at Internal Resolution 1x nothing is taken off.
    const u32 base = VideoCore::DynamicResolution::g_base.load(std::memory_order_relaxed);
    const bool at_lowest = Settings::values.resolution_factor.GetValue() == 1;
    const int before = g_controller.Drop();
    const int drop =
        g_controller.Judge(s, base > 1 && !at_lowest ? static_cast<int>(base) - 1 : 0);
    if (drop != before) {
        Publish(drop);
        LOG_INFO(Frontend,
                 "xr: Dynamic Resolution: {}x (was {}x; {} display frames in {:.1f} s at {:.2f} "
                 "ms, GPU {:.1f} ms behind over {} of {} game frames)",
                 base > static_cast<u32>(drop) ? base - drop : 1,
                 base > static_cast<u32>(before) ? base - before : 1, s.display_frames, s.seconds,
                 s.period_ms, s.gpu_ms, s.gpu_frames, s.emu_frames);
    }
}

int Drop() {
    std::scoped_lock lock{g_mutex};
    return g_controller.Drop();
}

void Reset() {
    std::scoped_lock lock{g_mutex};
    ResetLocked();
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
