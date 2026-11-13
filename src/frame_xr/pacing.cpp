// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <utility>
#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "frame_xr/pacing.h"

namespace FrameXr {

namespace {

namespace P = Common::XrPacer;

int64_t NowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool SwitchOn(const char* name) {
    const char* v = std::getenv(name);
    return !v || !*v || v[0] != '0';
}

constexpr const char* kOffFile = "xr_pace_off";

} // namespace

void Pacing::Configure(bool set_release_) {
    set_release = set_release_;
    gate_switch = SwitchOn("AZAHARFRAME_XR_PACE_GATE");
    const char* t = std::getenv("AZAHARFRAME_XR_PACE_TRACE"); // =1: a log line per frame
    trace = t && t[0] == '1';
    pictures_switch = SwitchOn("AZAHARFRAME_XR_PACE_PICTURES"); // pacing2 (0: by release)
    const bool lock = SwitchOn("AZAHARFRAME_XR_PACE_LOCK");
    P::Get().lock_switch.store(lock);
    LOG_INFO(Frontend,
             "xr: pacing at 120 Hz: gate {} ({}), lock {} (AZAHARFRAME_XR_PACE_GATE, "
             "AZAHARFRAME_XR_PACE_PICTURES, AZAHARFRAME_XR_PACE_LOCK; a user/{} file turns the "
             "gate and the lock off while it exists)",
             !gate_switch   ? "off"
             : !set_release ? "off (needs AZAHARFRAME_XR_SET_RELEASE)"
                            : "on",
             pictures_switch ? "by game picture" : "by release", lock ? "on" : "off", kOffFile);
}

void Pacing::NoteLatchTime() {
    latch_lock_ns.store(NowNs(), std::memory_order_relaxed);
}

void Pacing::NoteLatch(uint64_t shown_serial, int64_t period, bool shown_now) {
    const int64_t latch = latch_lock_ns.load(std::memory_order_relaxed);
    {
        std::scoped_lock lock{mutex};
        gate.Latch(shown_now ? shown_serial : 0, PictureOf(shown_serial));
        if (trace) {
            LOG_INFO(Frontend,
                     "xr: pacing trace: latch at {:.2f} ms: frame {} ({} ended frames), picture "
                     "{} ({} ended frames)",
                     (latch % 1'000'000'000) / 1e6, gate.shown, gate.count, gate.shown_picture,
                     gate.picture_count);
        }
        period_ns = period;
        last_latch_ns = latch;
        shown = shown_now;
    }
    cv.notify_all();
    auto& s = P::Get();
    s.period_ns.store(period, std::memory_order_relaxed);
    s.latch_ns.store(latch, std::memory_order_relaxed);
    s.session.store(shown_now, std::memory_order_relaxed);
}

bool Pacing::GateActive(int64_t now_ns) const {
    return gate_switch && set_release && !file_off && shown && P::EvenCadencePeriod(period_ns) &&
           now_ns - last_latch_ns < 3 * period_ns;
}

uint64_t Pacing::PictureOf(uint64_t serial) const {
    if (serial == 0) {
        return 0;
    }
    const PictureEntry& e = pictures[serial % kPictureRing];
    return e.serial == serial ? e.picture : 0;
}

uint32_t Pacing::Needed(uint64_t released, uint64_t released_picture, bool is_new,
                        uint32_t cadence_now) const {
    if (pictures_switch) {
        return gate.PictureNeeded(released_picture, is_new, cadence_now);
    }
    // By release (AZAHARFRAME_XR_PACE_PICTURES=0, the gate before pacing2): every release waits
    // for two ended frames of the one before.
    if (gate.Open(released)) {
        return 0;
    }
    return released == gate.shown ? 2 - std::min<uint32_t>(gate.count, 2) : 2;
}

void Pacing::WaitSlot(uint64_t released, uint64_t game_swaps) {
    const int64_t now = NowNs();
    std::unique_lock lock{mutex};
    // AzaharFrame pacing2: a new game picture, or the one before again (no top screen swap since
    // the frame before: a game at 30 or 20 pictures a second, common/xr_pacer.h Gate).
    const bool first = !have_swaps;
    const bool swapped = !first && game_swaps != last_swaps;
    have_swaps = true;
    last_swaps = game_swaps;
    cadence.Note(swapped);
    const bool is_new = first || swapped || !cadence.Swapping() || picture_seq == 0;
    if (is_new) {
        picture_seq++;
        new_pictures++;
    } else {
        repeats++;
    }
    const uint64_t next = released + 1; // ReleaseFrame's serial for this one
    pictures[next % kPictureRing] = PictureEntry{next, picture_seq};
    const uint64_t released_picture = PictureOf(released);
    const uint32_t c = cadence.Cadence();
    cadence_sum += c;
    NotePhase(released, released_picture, is_new, c, now);
    if (!GateActive(now)) {
        return;
    }
    const uint32_t needed = Needed(released, released_picture, is_new, c);
    if (needed == 0) {
        return;
    }
    // At 60 pictures a second at most one period and 2 ms, as before; at 30 or 20 until the
    // picture on view has had its 4 or 6 frames (each latch notifies), and 2 ms.
    const int64_t hold_ns =
        (pictures_switch && c > 1 ? static_cast<int64_t>(needed) : 1) * period_ns + 2'000'000;
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::nanoseconds(hold_ns);
    const uint32_t count_before = gate.picture_count;
    const bool on_view = gate.shown_picture == released_picture;
    const bool open = cv.wait_until(lock, deadline, [&] {
        return Needed(released, released_picture, is_new, c) == 0 || !GateActive(NowNs());
    });
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
            .count();
    if (trace) {
        LOG_INFO(Frontend,
                 "xr: pacing trace: frame {} held {:.2f} ms (picture {} {}, {} ended frames, "
                 "cadence {})",
                 released, ms, released_picture, on_view ? "on view" : "not yet on view",
                 count_before, c);
    }
    gate_waits++;
    gate_wait_max_ms = std::max(gate_wait_max_ms, ms);
    if (!open) {
        gate_timeouts++;
    } else if (Needed(released, released_picture, is_new, c) == 0) {
        holds_prevented++;
    }
}

void Pacing::NotePhase(uint64_t released, uint64_t released_picture, bool is_new,
                       uint32_t cadence_now, int64_t ready) {
    if (!shown || period_ns <= 0 || last_latch_ns == 0 || ready - last_latch_ns >= 3 * period_ns) {
        return; // no frame loop showing the game at a steady rate
    }
    const double period_us = period_ns / 1000.0;
    const double since_us = (ready - last_latch_ns) / 1000.0;
    if (trace) {
        LOG_INFO(Frontend,
                 "xr: pacing trace: ready at {:.2f} ms after frame {} (picture {}, {}): {:.2f} ms "
                 "after the last latch, {:.2f} ms after the limiter woke; {} (cadence {})",
                 (ready % 1'000'000'000) / 1e6, released, released_picture,
                 released_picture == gate.shown_picture ? "on view" : "not yet on view",
                 since_us / 1000.0, (ready - P::Get().wake_ns.load()) / 1e6,
                 is_new ? "new picture" : "the same picture again", cadence_now);
    }
    if (!is_new && pictures_switch) {
        return; // the same picture again: it never waits, and when it goes up does not matter
    }
    // Held by the gate: the game runs ahead of the cadence the gate keeps (SampleErrorUs). The
    // picture before goes up at the next latch (not on view yet) and may make way after 2 x
    // cadence.
    const bool gated = GateActive(ready);
    const uint32_t needed = gated ? Needed(released, released_picture, true, cadence_now) : 0;
    const double error_us = P::SampleErrorUs(since_us, needed, period_us);
    auto& s = P::Get();
    s.phase_error_ns.store(static_cast<int64_t>(error_us * 1000.0), std::memory_order_relaxed);
    s.phase_gated.store(gated, std::memory_order_relaxed);
    s.phase_seq.fetch_add(1, std::memory_order_release);
    filter.Add(error_us, period_us); // for the log line (the lock keeps its own)
    mean_ms = filter.MeanUs() / 1000.0;
    if (to_latch_ms.size() < 1000) {
        const double to_latch = period_us / 2 - P::PhaseErrorUs(since_us, period_us);
        to_latch_ms.push_back(static_cast<float>(to_latch / 1000.0));
    }
}

void Pacing::Second(double seconds) {
    auto& s = P::Get();
    const bool off_file =
        FileUtil::Exists(FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + kOffFile);
    if (off_file != s.file_off.load()) {
        LOG_INFO(Frontend, "xr: pacing: {} {}: gate and lock {}", kOffFile,
                 off_file ? "found" : "gone", off_file ? "off" : "back on");
    }
    s.file_off.store(off_file);

    std::vector<float> phases;
    uint32_t runs[4];
    uint32_t picture_runs[6];
    uint32_t news, again;
    double cadence_avg;
    uint32_t waits, timeouts, prevented;
    double wait_max;
    bool gate_on, was_shown;
    double mean, concentration;
    int64_t period;
    {
        std::scoped_lock lock{mutex};
        file_off = off_file;
        phases.swap(to_latch_ms);
        gate.TakeRuns(runs);
        gate.TakePictureRuns(picture_runs);
        news = std::exchange(new_pictures, 0);
        again = std::exchange(repeats, 0);
        cadence_avg = news + again ? static_cast<double>(std::exchange(cadence_sum, 0)) /
                                         (news + again)
                                   : 1.0;
        waits = std::exchange(gate_waits, 0);
        timeouts = std::exchange(gate_timeouts, 0);
        prevented = std::exchange(holds_prevented, 0);
        wait_max = std::exchange(gate_wait_max_ms, 0.0);
        gate_on = GateActive(NowNs());
        mean = mean_ms;
        concentration = filter.Concentration();
        was_shown = shown;
        period = period_ns;
    }
    const uint32_t lock_frames = s.lock_frames.exchange(0);
    const int64_t corr_sum = s.correction_sum_us.exchange(0);
    const uint32_t corr_max = s.correction_max_us.exchange(0);
    const uint32_t no_sleep = s.no_sleep_frames.exchange(0);
    const uint32_t drops = s.audio_drops.exchange(0);
    const uint32_t dropped = s.audio_dropped_frames.exchange(0);
    if (!was_shown) {
        return; // no game on view: nothing to pace
    }
    const auto lock_off = static_cast<P::Off>(s.lock_off.load());
    const char* gate_why = !gate_switch                   ? "AZAHARFRAME_XR_PACE_GATE=0"
                           : !set_release                 ? "no set releases"
                           : off_file                     ? kOffFile
                           : !P::EvenCadencePeriod(period) ? "not 120 Hz"
                                                          : "no session";
    const std::string mode =
        fmt::format("gate {}, lock {}", gate_on ? "on" : fmt::format("off ({})", gate_why),
                    lock_off == P::Off::None ? "on" : fmt::format("off ({})", P::OffText(lock_off)));
    std::string phase = "-";
    if (!phases.empty()) {
        std::sort(phases.begin(), phases.end());
        const auto at = [&](double q) {
            return phases[std::min(phases.size() - 1,
                                   static_cast<std::size_t>(q * (phases.size() - 1) + 0.5))];
        };
        phase = fmt::format("{:.1f}/{:.1f}/{:.1f}/{:.1f} ms over {}", at(0.05), at(0.5), at(0.95),
                            phases.back(), phases.size());
    }
    // AzaharFrame pacing2: by game picture (a repeat is the picture before again): a clean second
    // is all 2s at cadence 1, all 4s at cadence 2; releases on view as before.
    LOG_INFO(Frontend,
             "xr: pacing {:.1f}s: {} at {:.0f} Hz; new picture ready to the next latch "
             "p5/p50/p95/max {} (smoothed {:+.2f} ms from the lock's target, concentration "
             "{:.2f}); game pictures {} and {} repeats (cadence {:.1f}): on view 1/2/3/4/5/6+ "
             "frames {}/{}/{}/{}/{}/{}; releases on view 1/2/3/4+ frames {}/{}/{}/{}; gate waits "
             "{} (max {:.1f} ms, timeouts {}), 1-frame holds prevented {}; speed x{:.5f}, "
             "correction {:+.0f} us avg (max {}) over {} frames; limiter could not sleep {} "
             "frames; audio backlog drops {} ({} frames)",
             seconds, mode, period > 0 ? 1e9 / period : 0.0, phase,
             lock_off == P::Off::None ? s.phase_mean_ns.load() / 1e6 : mean, concentration, news,
             again, cadence_avg, picture_runs[0], picture_runs[1], picture_runs[2],
             picture_runs[3], picture_runs[4], picture_runs[5], runs[0], runs[1], runs[2], runs[3],
             waits, wait_max, timeouts, prevented, s.audio_ratio.load(),
             lock_frames ? static_cast<double>(corr_sum) / lock_frames : 0.0, corr_max,
             lock_frames, no_sleep, drops, dropped);
}

} // namespace FrameXr
