// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/watchdog.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <fmt/format.h>
#include "common/logging/log.h"
#include "common/thread.h"
#include "input_common/main.h"

namespace FrameXr::Watchdog {

namespace {

constexpr int64_t kStuckUs = 50'000; // six display periods at 120 Hz
constexpr auto kPoll = std::chrono::milliseconds(5);
// AzaharFrame flatfix: Flat, always on.
constexpr int64_t kFlatStuckUs = 3'000'000;
constexpr auto kFlatPoll = std::chrono::milliseconds(250);

std::atomic<bool> g_enabled{false}; ///< AZAHARFRAME_XR_WATCHDOG=1: the VR phases too
std::atomic<bool> g_running{false}; ///< the thread runs (Flat is always watched)
std::atomic<int64_t> g_flat_us{0};  ///< the last Flat tick, 0: not in Flat
std::atomic<const char*> g_flat_phase{""};
std::atomic<uint64_t> g_flat_pumps{0}; ///< SDL's pump count at the last Flat tick
std::atomic<bool> g_stop{false};
std::atomic<int64_t> g_beat_us{0}; // 0: not in a frame
std::atomic<const char*> g_phase{""};
std::atomic<int> g_xr_tid{0};
std::thread g_thread;

int64_t NowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string ReadLine(const std::string& path) {
    std::ifstream f(path);
    std::string s;
    std::getline(f, s);
    return s;
}

// "name state wchan" of one thread of this process.
struct Task {
    int tid = 0;
    std::string name;
    char state = '?';
    std::string wchan;
    std::string sys; ///< the number of the system call it is in, "running", or "-"
};

Task ReadTask(int tid) {
    Task t;
    t.tid = tid;
    const std::string dir = fmt::format("/proc/self/task/{}/", tid);
    t.name = ReadLine(dir + "comm");
    const std::string stat = ReadLine(dir + "stat");
    // pid (comm) state ...: the state follows the last ')'.
    if (const auto close = stat.rfind(')'); close != std::string::npos && close + 2 < stat.size()) {
        t.state = stat[close + 2];
    }
    t.wchan = ReadLine(dir + "wchan");
    if (t.wchan.empty() || t.wchan == "0") {
        t.wchan = "-";
    }
    // "nr args... sp pc", or "running"; only the number is kept (on arm64: 29 ioctl, 98 futex,
    // 215 munmap, 222 mmap, 226 mprotect, 63 read, 73 ppoll).
    const std::string sc = ReadLine(dir + "syscall");
    t.sys = sc.empty() ? "-" : sc.substr(0, sc.find(' '));
    return t;
}

bool Interesting(const Task& t) {
    return t.state == 'D' || t.name == "EmuThread" || t.name == "VulkanWorker" ||
           t.name == "XrRelease" || t.name.rfind("Custom tex", 0) == 0;
}

void Report(int64_t stuck_us) {
    const int xr_tid = g_xr_tid.load();
    std::string others;
    if (DIR* d = opendir("/proc/self/task")) {
        while (dirent* e = readdir(d)) {
            const int tid = std::atoi(e->d_name);
            if (tid <= 0 || tid == xr_tid) {
                continue;
            }
            const Task t = ReadTask(tid);
            if (Interesting(t)) {
                others += fmt::format("; {} {} {} syscall {}", t.name, t.state, t.wchan, t.sys);
            }
        }
        closedir(d);
    }
    const Task xr = ReadTask(xr_tid);
    LOG_WARNING(Frontend,
                "xr: watchdog: the frame loop is in {} for {:.0f} ms (thread {} {} syscall {}){}",
                g_phase.load(), stuck_us / 1000.0, xr.state, xr.wchan, xr.sys, others);
}

// AzaharFrame flatfix: "comm state wchan syscall" of the first thread with this name.
std::string DescribeNamed(const char* name) {
    std::string out = fmt::format("no {} thread", name);
    if (DIR* d = opendir("/proc/self/task")) {
        while (dirent* e = readdir(d)) {
            const int tid = std::atoi(e->d_name);
            if (tid <= 0) {
                continue;
            }
            const Task t = ReadTask(tid);
            if (t.name == name) {
                out = fmt::format("{} {} {} syscall {}", t.name, t.state, t.wchan, t.sys);
                break;
            }
        }
        closedir(d);
    }
    return out;
}

// AzaharFrame flatfix: the Flat check, once per stall, and once when the loop ticks again.
void CheckFlat(int64_t& reported_tick, int64_t& stalled_since) {
    const int64_t tick = g_flat_us.load(std::memory_order_acquire);
    if (stalled_since != 0 && tick != reported_tick) {
        LOG_WARNING(Frontend, "xr: watchdog: the frame loop moves again, {:.1f} s after its "
                              "last Flat tick",
                    ((tick != 0 ? tick : NowUs()) - stalled_since) / 1e6);
        stalled_since = 0;
    }
    if (tick == 0) {
        return; // not in Flat (a stall that ended in a session was logged above)
    }
    const int64_t now = NowUs();
    if (tick == reported_tick || now - tick < kFlatStuckUs) {
        return;
    }
    reported_tick = tick;
    stalled_since = tick;
    const Task xr = ReadTask(g_xr_tid.load());
    const uint64_t pumps = InputCommon::GetSDLPumpCount() - g_flat_pumps.load();
    LOG_ERROR(Frontend,
              "xr: watchdog: the frame loop has not ticked in Flat for {:.1f} s (in {}; thread "
              "{} {} syscall {}); {}, SDL pumped {} times since; a switch to VR waits until it "
              "ticks",
              (now - tick) / 1e6, g_flat_phase.load(), xr.state, xr.wchan, xr.sys,
              DescribeNamed("SDLPump"), pumps);
}

void Loop() {
    Common::SetCurrentThreadName("FrameXRWatch");
    int64_t reported_beat = 0;
    int reports_in_window = 0;
    int64_t window_start = 0;
    int64_t reported_flat = 0;
    int64_t flat_stalled_since = 0;
    while (!g_stop.load()) {
        const bool vr_phases = g_enabled.load();
        std::this_thread::sleep_for(vr_phases ? kPoll : kFlatPoll);
        CheckFlat(reported_flat, flat_stalled_since);
        if (!vr_phases) {
            continue;
        }
        const int64_t beat = g_beat_us.load();
        if (beat == 0 || beat == reported_beat) {
            continue;
        }
        const int64_t now = NowUs();
        if (now - beat < kStuckUs) {
            continue;
        }
        reported_beat = beat; // once per stuck phase
        if (now - window_start > 10'000'000) {
            window_start = now;
            reports_in_window = 0;
        }
        if (reports_in_window++ < 5) {
            Report(now - beat);
        }
    }
}

} // Anonymous namespace

void Start() {
    if (g_running.exchange(true)) {
        return;
    }
    const char* v = std::getenv("AZAHARFRAME_XR_WATCHDOG");
    const bool vr_phases = v && std::string{v} == "1";
    g_enabled = vr_phases;
    g_flat_us = 0;
    g_stop = false;
    g_thread = std::thread(Loop); // AzaharFrame flatfix: always, for Flat
    if (vr_phases) {
        LOG_INFO(Frontend, "xr: watchdog on (AZAHARFRAME_XR_WATCHDOG=1): a frame loop phase over "
                           "50 ms is logged with the kernel state of the busy threads");
    }
}

void Stop() {
    if (!g_running.exchange(false)) {
        return;
    }
    g_enabled = false;
    g_stop = true;
    if (g_thread.joinable()) {
        g_thread.join();
    }
}

void Beat(const char* phase) {
    if (!g_enabled.load(std::memory_order_relaxed)) {
        return;
    }
    if (g_xr_tid.load(std::memory_order_relaxed) == 0) {
        g_xr_tid = static_cast<int>(syscall(SYS_gettid));
    }
    g_phase.store(phase, std::memory_order_relaxed);
    g_beat_us.store(NowUs(), std::memory_order_release);
}

void Idle() {
    if (g_enabled.load(std::memory_order_relaxed)) {
        g_beat_us.store(0, std::memory_order_release);
    }
}

void FlatTick() { // AzaharFrame flatfix
    if (g_xr_tid.load(std::memory_order_relaxed) == 0) {
        g_xr_tid = static_cast<int>(syscall(SYS_gettid));
    }
    g_flat_pumps.store(InputCommon::GetSDLPumpCount(), std::memory_order_relaxed);
    g_flat_phase.store("the start of a Flat pass", std::memory_order_relaxed);
    g_flat_us.store(NowUs(), std::memory_order_release);
}

void FlatPhase(const char* phase) {
    g_flat_phase.store(phase, std::memory_order_relaxed);
}

void FlatEnd() {
    g_flat_us.store(0, std::memory_order_release);
}

} // namespace FrameXr::Watchdog
