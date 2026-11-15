// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "common/atomic_file.h" // AzaharFrame fixd
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/dynamic_resolution.h"
#include "video_core/vr_dynamic_resolution.h"

namespace FrameXr {
uint64_t GameTitleId(); // frame_xr.h (AzaharFrame fixd: the remembered level's game)
} // namespace FrameXr

// AzaharFrame menuux
namespace FrameXr::DynamicResolution {

bool Controller::Bad(const Second& s) {
    // The GPU is the limit. Missed display frames with the GPU under budget are CPU-bound: a
    // lower resolution does nothing for them.
    return s.period_ms > 0.0 && s.gpu_frames >= kMinGpuFrames &&
           s.gpu_ms >= kBadBudget * s.period_ms;
}

double Controller::PixelRatio(int factor) { // AzaharFrame flash3
    const double f = std::max(1, factor);
    return ((f + 1.0) / f) * ((f + 1.0) / f);
}

double Controller::MeasuredUp(int factor) const { // AzaharFrame fixd
    return factor >= 1 && factor <= kMaxSteps ? up_ratio[factor] : 0.0;
}

double Controller::MeasuredDown(int factor) const { // AzaharFrame fixd
    return factor >= 1 && factor <= kMaxSteps ? down_ratio[factor] : 0.0;
}

double Controller::Cost(int factor) const { // AzaharFrame fixd
    if (factor < 1 || factor > kMaxSteps || cost_n[factor] < kCostSeconds) {
        return 0.0;
    }
    const int n = std::min(cost_n[factor], kCostKeep);
    float v[kCostKeep];
    std::copy(costs[factor], costs[factor] + n, v);
    std::nth_element(v, v + n / 2, v + n);
    return v[n / 2];
}

double Controller::CostRatio(int factor) const { // AzaharFrame fixd
    const double low = Cost(factor), high = Cost(factor + 1);
    return low > 0.0 && high > 0.0 ? high / low : 0.0;
}

double Controller::Ratio(int factor) const { // AzaharFrame fixd
    factor = std::max(1, factor);
    const double pixels = PixelRatio(factor);
    // What says the step is cheaper than its pixels: a step down's run against the seconds after
    // it, the medians of the two factors' seconds in this game.
    double ratio = pixels;
    if (MeasuredDown(factor) > 0.0) {
        ratio = std::min(ratio, MeasuredDown(factor));
    }
    if (CostRatio(factor) > 0.0) {
        ratio = std::min(ratio, std::max(kRatioMin, CostRatio(factor)));
    }
    // A step up's own measurement (the same scene a few seconds apart) is what it cost.
    return std::max(ratio, MeasuredUp(factor));
}

double Controller::PredictUp(double gpu_ms, int factor) const {
    // AzaharFrame fixd: by the ratio this game measured between the two factors (Ratio).
    return gpu_ms * Ratio(factor);
}

std::string Controller::RatioWhy(int factor) const { // AzaharFrame fixd
    factor = std::max(1, factor);
    const double r = Ratio(factor);
    if (r == PixelRatio(factor)) {
        return fmt::format("x{:.2f} by the pixels", r);
    }
    if (r == MeasuredUp(factor)) {
        return fmt::format("x{:.2f} measured at a step up", r);
    }
    if (r == MeasuredDown(factor)) {
        return fmt::format("x{:.2f} measured at a step down", r);
    }
    return fmt::format("x{:.2f} by the medians of {}x and {}x", r, factor, factor + 1);
}

bool Controller::Good(const Second& s, int factor) const {
    // AzaharFrame flash3: the next factor's cost, from this one's pixels to the next one's,
    // well under the bad line, so the step up holds. AzaharFrame fixd: or by the measured ratio.
    return s.period_ms > 0.0 && s.gpu_frames >= kMinGpuFrames &&
           PredictUp(s.gpu_ms, factor) < kUpBudget * kBadBudget * s.period_ms;
}

bool Controller::Blocked(int level) const { // AzaharFrame fixd
    return level >= 0 && level < kMaxSteps && now < blocked_until[level];
}

// AzaharFrame fixd: a step was taken from `from_factor`, whose cost was `from_cost` (the run of
// seconds that led to it): the first judged seconds after its settle measure the new factor.
void Controller::StepDone(bool down, int from_factor, double from_cost) {
    last_step = down ? 1 : -1;
    held = 0;
    run_sum = 0.0;
    run_n = 0;
    measure_from = from_cost > 0.0 ? from_factor : 0;
    measure_down = down;
    measure_cost = from_cost;
    measure_sum = 0.0;
    measure_n = 0;
}

// AzaharFrame fixd: a judged second at `factor` after a step: when kMeasureSeconds are in, the
// two factors' costs (the same scene, a few seconds apart) give their ratio.
//   - After a step up (a good run at the lower factor): the ratio as measured, up to
//     kRatioOverPixels times the pixels'. The higher one at the bad line (the step up failed) is
//     a ratio at least that large, so it never lowers what was measured before.
//   - After a step down, the higher factor's run was the GPU behind: its time is that factor's
//     cost plus the wait behind earlier work, so the ratio is at most what it measures. It only
//     ever says the step is cheaper than thought (under the pixels' ratio, or under the kept
//     one): a heavy scene that passes as it steps down cannot block the step back up.
void Controller::Measure(const Second& s, int factor) {
    if (measure_from == 0 || s.gpu_frames < kMinGpuFrames) {
        return;
    }
    if (std::abs(factor - measure_from) != 1) {
        measure_from = 0; // not the factor next to it (the ceiling moved): nothing to compare
        return;
    }
    measure_sum += s.gpu_ms;
    if (++measure_n < kMeasureSeconds) {
        return;
    }
    const double now_cost = measure_sum / measure_n;
    const int low = std::min(factor, measure_from);
    const double cost_low = factor == low ? now_cost : measure_cost;
    const double cost_high = factor == low ? measure_cost : now_cost;
    const double line = kBadBudget * s.period_ms;
    measure_from = 0;
    if (cost_low <= 0.0 || cost_low >= line || low < 1 || low > kMaxSteps) {
        return;
    }
    if (measure_down) {
        double& kept = down_ratio[low];
        const double r = std::clamp(cost_high / cost_low, kRatioMin, PixelRatio(low));
        if (r < (kept > 0.0 ? kept : PixelRatio(low))) {
            kept = kept > 0.0 ? 0.5 * (kept + r) : r;
        }
        return;
    }
    const double r = std::clamp(cost_high / cost_low, kRatioMin, kRatioOverPixels * PixelRatio(low));
    double& kept = up_ratio[low];
    if (cost_high >= line && kept >= r) {
        return;
    }
    kept = kept > 0.0 ? 0.5 * (kept + r) : r;
}


int Controller::Judge(const Second& s, int max_drop) {
    last_step = 0; // AzaharFrame fixd
    now += std::max(0.0, s.seconds);
    max_drop = std::clamp(max_drop, 0, kMaxSteps - 1);
    drop = std::min(drop, max_drop);
    if (!s.live) {
        Reset(); // no game: the next one starts from the setting (or its remembered level)
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
    held++; // AzaharFrame fixd
    const bool holding = hold > 0; // AzaharFrame ui2
    if (holding) {
        hold--;
    }
    // AzaharFrame flash3: the factor drawn now (without one, the steps down to 1x).
    const int factor = s.factor > 0 ? s.factor : max_drop + 1 - drop;
    Measure(s, factor); // AzaharFrame fixd
    if (s.gpu_frames >= kMinGpuFrames && factor >= 1 && factor <= kMaxSteps) { // AzaharFrame fixd
        costs[factor][cost_n[factor] % kCostKeep] = static_cast<float>(s.gpu_ms);
        cost_n[factor]++;
    }
    if (Bad(s)) {
        if (bad == 0) { // AzaharFrame fixd: a new run, its cost from here
            run_sum = 0.0;
            run_n = 0;
        }
        run_sum += s.gpu_ms;
        run_n++;
        good = 0;
        if (++bad >= kDownAfter && drop < max_drop) {
            const int level = drop; // the level it leaves
            if (now - last_up <= kFailWindow) {
                // The step up to this level could not be held: wait before trying it again,
                // longer each time (AzaharFrame fixd), and after kFailsForGame not again in this
                // game.
                failed[level]++;
                blocked_until[level] =
                    failed[level] >= kFailsForGame
                        ? 1e18
                        : now + kRetryWait * std::pow(double(kRetryGrowth), failed[level] - 1);
                last_up = -1e9;
            }
            const double cost = run_sum / run_n;
            drop++;
            bad = 0;
            settle = kSettle;
            hold = kHoldAfterDown; // AzaharFrame ui2
            StepDone(true, factor, cost);
        }
    } else {
        // AzaharFrame flash3: any second that is not bad ends a bad run (only a good one did).
        bad = 0;
        if (Good(s, factor) && !holding) {
            if (good == 0) { // AzaharFrame fixd: a new run
                run_sum = 0.0;
                run_n = 0;
            }
            run_sum += s.gpu_ms;
            run_n++;
            if (++good >= kUpAfter && drop > 0 && now >= blocked_until[drop - 1]) {
                const double cost = run_sum / run_n;
                drop--;
                good = 0;
                settle = kSettle;
                last_up = now;
                StepDone(false, factor, cost);
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

void Controller::Start(int steps, bool same_game) { // AzaharFrame fixd
    if (same_game) {
        Controller fresh;
        std::copy(std::begin(up_ratio), std::end(up_ratio), fresh.up_ratio);
        std::copy(std::begin(down_ratio), std::end(down_ratio), fresh.down_ratio);
        std::copy(&costs[0][0], &costs[0][0] + (kMaxSteps + 1) * kCostKeep, &fresh.costs[0][0]);
        std::copy(std::begin(cost_n), std::end(cost_n), fresh.cost_n);
        *this = fresh;
    } else {
        Reset();
    }
    drop = std::clamp(steps, 0, kMaxSteps - 1);
    if (drop > 0) {
        settle = kSettle;
        hold = kHoldAfterDown;
    }
}

void Controller::Resume() { // AzaharFrame ui2
    bad = good = 0;
    settle = drop > 0 ? kSettle : 0; // the step back down is a hitch of its own
    hold = drop > 0 ? kHoldAfterDown : 0;
    measure_from = 0; // AzaharFrame fixd: the seconds before are not this scene's
}

// AzaharFrame fixd: the remembered levels' file.
std::string FormatStore(const std::vector<std::pair<uint64_t, int>>& levels) {
    std::map<uint64_t, int> sorted(levels.begin(), levels.end());
    std::string out;
    for (const auto& [title, factor] : sorted) {
        if (title != 0 && factor >= 1) {
            char line[48];
            std::snprintf(line, sizeof(line), "%016" PRIX64 " %d\n", title, factor);
            out += line;
        }
    }
    return out;
}

std::vector<std::pair<uint64_t, int>> ParseStore(const std::string& text) {
    std::map<uint64_t, int> levels;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        char hex[17] = {};
        int factor = 0;
        char extra = 0;
        if (std::sscanf(line.c_str(), "%16[0-9A-Fa-f] %d %c", hex, &factor, &extra) != 2 ||
            std::string_view(hex).size() != 16 || factor < 1 || factor > 16) {
            continue; // not a line this build reads
        }
        const uint64_t title = std::strtoull(hex, nullptr, 16);
        if (title != 0) {
            levels[title] = factor;
        }
    }
    return {levels.begin(), levels.end()};
}

namespace {

std::mutex g_mutex;
Controller g_controller;
bool g_installed = false;
// AzaharFrame fixd: the remembered levels (read once), where they live, and a start from the
// remembered level waiting for the renderer's factor (VR started in a game).
LevelStore g_store;
bool g_store_set = false;
std::map<uint64_t, int> g_levels;
bool g_levels_loaded = false;
bool g_pending_start = false;
constexpr const char* kScreensOff = "the game's screens are off: a start, a stop or a Load";
const char* g_pending_why = "VR started in a game";

std::string StorePath() {
    return FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + kStoreFile;
}

LevelStore FileStore() {
    LevelStore s;
    s.load = [] {
        std::string text;
        FileUtil::ReadFileToString(true, StorePath(), text);
        return ParseStore(text);
    };
    s.save = [](std::vector<std::pair<uint64_t, int>> levels) {
        // Off the XR thread (an fsync), the newest list last.
        static std::mutex write_mutex;
        static std::atomic<uint64_t> issued{0};
        static uint64_t written = 0;
        const uint64_t mine = ++issued;
        std::thread([mine, levels = std::move(levels)] {
            std::scoped_lock lock{write_mutex};
            if (mine < written) {
                return;
            }
            written = mine;
            if (!Common::WriteFileAtomic(StorePath(), FormatStore(levels))) {
                LOG_WARNING(Frontend, "xr: Dynamic Resolution: could not save the remembered levels");
            }
        }).detach();
    };
    s.title = [] { return FrameXr::GameTitleId(); };
    return s;
}

LevelStore& Store() {
    if (!g_store_set) {
        g_store = FileStore();
        g_store_set = true;
    }
    return g_store;
}

void LoadLevels() {
    if (g_levels_loaded) {
        return;
    }
    g_levels_loaded = true;
    if (Store().load) {
        for (const auto& [title, factor] : Store().load()) {
            g_levels[title] = factor;
        }
    }
}

uint64_t Title() {
    return Store().title ? Store().title() : 0;
}

int RememberedLocked(uint64_t title) {
    if (title == 0) {
        return 0; // no game (or homebrew without a title ID): nothing to read
    }
    LoadLevels();
    const auto it = g_levels.find(title);
    return title != 0 && it != g_levels.end() ? it->second : 0;
}

void Remember(uint64_t title, int factor) {
    g_levels[title] = factor;
    if (Store().save) {
        Store().save({g_levels.begin(), g_levels.end()});
    }
}
// AzaharFrame ui2: the controller as it was when the setting went off in this game, and the
// setting as NoteSecond last saw it (a change from elsewhere, Azahar's window, counts too).
Controller g_saved;
uint64_t g_measured_title = 0; ///< AzaharFrame fixd: the game g_controller has measured
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
    } else { // AzaharFrame fixd: nothing kept from this game: its remembered level, if any
        g_pending_start = true;
        g_pending_why = "turned on";
    }
}

void ResetLocked(const char* why) {
    LogBack(g_controller.Drop(), why); // AzaharFrame flash3
    g_controller.Reset();
    g_have_saved = false;
    Publish(0);
}

// AzaharFrame fixd: a game's start or a Load (its screens off), or VR started in a game: from
// the level remembered for the game, else from the setting. Logged as what it is.
void StartLocked(int base, int max_drop, const char* when) {
    const int before = g_controller.Drop();
    const uint64_t title = Title();
    const int remembered = RememberedLocked(title);
    const int steps = StartDrop(base, remembered, max_drop);
    // A Load or a Restart keeps what this game measured; another game starts from nothing.
    g_controller.Start(steps, title != 0 && title == g_measured_title);
    g_measured_title = title;
    Publish(steps);
    if (steps != before && base > 0) {
        const auto at = [base](int d) { return base > d ? base - d : 1; };
        if (steps > 0) {
            LOG_INFO(Frontend, "xr: Dynamic Resolution: {}x (was {}x; back to the level remembered "
                               "for this game; {})",
                     at(steps), at(before), when);
        } else {
            LOG_INFO(Frontend, "xr: Dynamic Resolution: {}x (was {}x; reset to the setting; {})",
                     at(steps), at(before), when);
        }
    }
}

} // namespace

void SetLevelStore(LevelStore store) { // AzaharFrame fixd
    std::scoped_lock lock{g_mutex};
    g_store = std::move(store);
    g_store_set = true;
    g_levels.clear();
    g_levels_loaded = false;
}

int RememberedFactor(uint64_t title) { // AzaharFrame fixd
    std::scoped_lock lock{g_mutex};
    return RememberedLocked(title);
}

void Install() {
    std::scoped_lock lock{g_mutex};
    ResetLocked("VR started");
    g_installed = true;
    g_was_on = Settings::values.vr_dynamic_resolution.GetValue();
    g_pending_start = true; // AzaharFrame fixd: a game running already starts at its level
    g_pending_why = "VR started in a game";
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
        if (!s.live) { // AzaharFrame fixd: a game started or stopped while it was off: the copy
            g_have_saved = false; // is another game's (ui2 meant a new game to drop it)
            g_controller.Reset();
        }
        return;
    }
    // The factor the renderer works out (Internal Resolution, with the immersive modes' own
    // steps) is the ceiling; the floor is FloorFactor's (1x, or the setting with
    // kFloorAtSetting); at Internal Resolution 1x nothing is taken off.
    const u32 base = VideoCore::DynamicResolution::g_base.load(std::memory_order_relaxed);
    const int setting = static_cast<int>(Settings::values.resolution_factor.GetValue());
    const int max_drop = MaxDrop(static_cast<int>(base), setting);
    // AzaharFrame fixd: no game on view (a game starting, stopping or loaded: session 651's
    // Load Anyway at 1818 s was logged as a step up from 0 game frames): from the level
    // remembered for the game, else from the setting; VR started in a game: the same, once the
    // renderer's factor is known.
    // The renderer's factor may still be the last game's while the screens are off: the start
    // is worked out again from the first second with a picture.
    if (!s.live) {
        g_pending_start = true;
        g_pending_why = kScreensOff;
        StartLocked(static_cast<int>(base), max_drop, kScreensOff);
        return;
    }
    if (g_pending_start && base > 0) {
        g_pending_start = false;
        StartLocked(static_cast<int>(base), max_drop, g_pending_why);
    }
    const int before = g_controller.Drop();
    Second judged = s;
    judged.factor = base > static_cast<u32>(before) ? static_cast<int>(base) - before : 1;
    // AzaharFrame fixd: the prediction and the ratio it used, before the step (for the log)
    const double predicted = g_controller.PredictUp(s.gpu_ms, judged.factor);
    const std::string ratio_why = g_controller.RatioWhy(judged.factor);
    const int drop = g_controller.Judge(judged, max_drop);
    const auto at = [base](int d) { return base > static_cast<u32>(d) ? static_cast<int>(base) - d : 1; };
    if (drop != before) {
        Publish(drop);
        // AzaharFrame flash3: why (a step up gives the cost it predicted for the new factor).
        // AzaharFrame fixd: a change that is no step is the ceiling or the floor moving.
        std::string why;
        if (g_controller.LastStep() > 0) {
            why = fmt::format("{} bad seconds in a row", Controller::kDownAfter);
        } else if (g_controller.LastStep() < 0) {
            why = fmt::format("{} good seconds in a row, predicted {:.1f} ms at the new factor ({})",
                              Controller::kUpAfter, predicted, ratio_why);
        } else {
            why = "the ceiling or the floor moved (Internal Resolution or the immersive mode)";
        }
        LOG_INFO(Frontend,
                 "xr: Dynamic Resolution: {}x (was {}x; {}; {} display frames in {:.1f} s at "
                 "{:.2f} ms, GPU {:.1f} ms behind over {} of {} game frames)",
                 at(drop), at(before), why, s.display_frames, s.seconds, s.period_ms, s.gpu_ms,
                 s.gpu_frames, s.emu_frames);
    }
    // AzaharFrame fixd: the game's level from now on: one it stepped down to (the one above
    // could not be held), or one it held kRememberAfter judged seconds (a step up that held).
    const uint64_t title = Title();
    const bool stepped_down = g_controller.LastStep() > 0;
    if (title != 0 && max_drop > 0 && (stepped_down || g_controller.Held() >= kRememberAfter)) {
        const int factor = at(drop);
        if (RememberedLocked(title) != factor) {
            Remember(title, factor);
            if (stepped_down) {
                LOG_INFO(Frontend, "xr: Dynamic Resolution: {}x remembered for this game (stepped down to it)",
                         factor);
            } else {
                LOG_INFO(Frontend, "xr: Dynamic Resolution: {}x remembered for this game (held {} s)",
                         factor, g_controller.Held());
            }
        }
    }
}

int Drop() {
    std::scoped_lock lock{g_mutex};
    return g_controller.Drop();
}

void Reset() {
    std::scoped_lock lock{g_mutex};
    g_pending_start = false; // AzaharFrame fixd: the setting's change wins over the remembered level
    ResetLocked("reset to the setting: Internal Resolution changed"); // AzaharFrame fixd: was "from the factor itself again"
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
