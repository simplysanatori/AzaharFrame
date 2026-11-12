// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame crashreport: the run marker and the fatal signal handler (frame_crash.h).

#include "common/frame_crash.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <typeinfo>

#include <fmt/format.h>

#if defined(__linux__)
#include <csignal>
#include <execinfo.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <ucontext.h>
#include <pthread.h>
#include <unistd.h>
#define FRAME_CRASH_POSIX 1
#endif

namespace Common::FrameCrash {

std::string_view CauseKey(Cause cause) {
    switch (cause) {
    case Cause::DeviceLost:
        return "device-lost";
    case Cause::CoreFatal:
        return "core-fatal";
    case Cause::ArmException:
        return "arm-exception";
    case Cause::Stuck:
        return "stuck";
    case Cause::None:
        break;
    }
    return "";
}

Cause CauseFromKey(std::string_view key) {
    for (const Cause c : {Cause::DeviceLost, Cause::CoreFatal, Cause::ArmException, Cause::Stuck}) {
        if (key == CauseKey(c)) {
            return c;
        }
    }
    return Cause::None;
}

namespace {

// One line per value: newlines in a title or detail would start a new key.
std::string OneLine(std::string_view s) {
    std::string out(s.substr(0, 400));
    for (char& c : out) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return out;
}

std::mutex g_mutex;
std::string g_dir;
std::string g_version;
std::string g_release;
long long g_started = 0;
u64 g_title_id = 0;
std::string g_title;
std::string g_settings;
bool g_vr = true;
Cause g_cause_noted = Cause::None;
std::string g_detail;
std::string g_exe; // AzaharFrame crashreport: "exe=...\nexe_build_id=...\n" (NoteExe)

#ifdef FRAME_CRASH_POSIX
// Read by the signal handler without a lock: written before the handlers go in (paths) or as one
// atomic store (the cause).
char g_crash_path[4096];
char g_maps_path[] = "/proc/self/maps";
std::atomic<int> g_cause{0};
char g_cause_detail[512];
char g_exception[512];
std::atomic<bool> g_in_handler{false};
constexpr int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};
struct sigaction g_old[std::size(kSignals)];
std::terminate_handler g_old_terminate = nullptr;
#endif

void WriteMarkerLocked() {
    if (g_dir.empty()) {
        return;
    }
    const std::string path = g_dir + kRunningFile;
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f) {
            return;
        }
#ifdef FRAME_CRASH_POSIX
        const int pid = static_cast<int>(getpid());
#else
        const int pid = 0;
#endif
        f << MarkerText(pid, g_started, g_version, g_release, g_title_id, g_title, g_settings, g_vr,
                        g_cause_noted, g_detail)
          << g_exe;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
}

#ifdef FRAME_CRASH_POSIX
// ---- async-signal-safe output -----------------------------------------------------------

void PutRaw(int fd, const char* s, std::size_t n) {
    while (n > 0) {
        const ssize_t w = write(fd, s, n);
        if (w <= 0) {
            return;
        }
        s += w;
        n -= static_cast<std::size_t>(w);
    }
}

void Put(int fd, const char* s) {
    PutRaw(fd, s, std::strlen(s));
}

void PutHex(int fd, unsigned long long v) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        const unsigned d = static_cast<unsigned>((v >> (60 - i * 4)) & 0xF);
        buf[2 + i] = static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10);
    }
    PutRaw(fd, buf, 18);
}

void PutDec(int fd, long long v) {
    char buf[24];
    int i = 23;
    const bool neg = v < 0;
    unsigned long long u = neg ? static_cast<unsigned long long>(-(v + 1)) + 1
                               : static_cast<unsigned long long>(v);
    buf[i] = 0;
    do {
        buf[--i] = static_cast<char>('0' + u % 10);
        u /= 10;
    } while (u != 0 && i > 1);
    if (neg) {
        buf[--i] = '-';
    }
    Put(fd, buf + i);
}

// A value written into one line: newlines become spaces.
void PutLine(int fd, const char* key, const char* value) {
    Put(fd, key);
    Put(fd, "=");
    char buf[512];
    std::size_t n = 0;
    for (const char* p = value; *p && n < sizeof(buf); p++) {
        buf[n++] = (*p == '\n' || *p == '\r') ? ' ' : *p;
    }
    PutRaw(fd, buf, n);
    Put(fd, "\n");
}

void Handler(int sig, siginfo_t* info, void* raw_context) {
    std::size_t index = 0;
    for (; index < std::size(kSignals); index++) {
        if (kSignals[index] == sig) {
            break;
        }
    }
    if (!g_in_handler.exchange(true)) {
        // AzaharFrame crashreport: a watchdog. backtrace() is not async-signal-safe (it unwinds
        // through libgcc, which can take a lock the crashed thread held), so the handler could
        // hang and leave a frozen process. SIGALRM at its default action ends the process 5 s
        // from now whatever the handler is doing; what was written by then stays (the record is
        // written in order of safety: the plain fields and the memory map with write() only,
        // then the stack). sigaction and alarm are async-signal-safe.
        struct sigaction alarm_action {};
        alarm_action.sa_handler = SIG_DFL;
        sigemptyset(&alarm_action.sa_mask);
        sigaction(SIGALRM, &alarm_action, nullptr);
        sigset_t alarm_set;
        sigemptyset(&alarm_set);
        sigaddset(&alarm_set, SIGALRM);
        pthread_sigmask(SIG_UNBLOCK, &alarm_set, nullptr);
        alarm(5);
        const int fd = open(g_crash_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (fd >= 0) {
            Put(fd, "signal=");
            PutDec(fd, sig);
            Put(fd, "\ncode=");
            PutDec(fd, info ? info->si_code : 0);
            Put(fd, "\naddr=");
            PutHex(fd, info ? reinterpret_cast<unsigned long long>(info->si_addr) : 0);
            Put(fd, "\n");
            char name[17] = {};
            prctl(PR_GET_NAME, name, 0, 0, 0);
            PutLine(fd, "thread", name);
            const int cause = g_cause.load();
            if (cause != 0) {
                Put(fd, "cause=");
                const std::string_view key = CauseKey(static_cast<Cause>(cause));
                PutRaw(fd, key.data(), key.size());
                Put(fd, "\n");
                PutLine(fd, "detail", g_cause_detail);
            }
            if (g_exception[0] != 0) {
                PutLine(fd, "exception", g_exception);
            }
            const auto* uc = static_cast<const ucontext_t*>(raw_context);
#if defined(__aarch64__)
            if (uc) {
                Put(fd, "pc=");
                PutHex(fd, uc->uc_mcontext.pc);
                Put(fd, "\nlr=");
                PutHex(fd, uc->uc_mcontext.regs[30]);
                Put(fd, "\n");
            }
#elif defined(__x86_64__)
            if (uc) {
                Put(fd, "pc=");
                PutHex(fd, static_cast<unsigned long long>(uc->uc_mcontext.gregs[REG_RIP]));
                Put(fd, "\n");
            }
#endif
            Put(fd, "maps-begin\n");
            const int maps = open(g_maps_path, O_RDONLY | O_CLOEXEC);
            if (maps >= 0) {
                char buf[4096];
                ssize_t r;
                while ((r = read(maps, buf, sizeof(buf))) > 0) {
                    PutRaw(fd, buf, static_cast<std::size_t>(r));
                }
                close(maps);
            }
            Put(fd, "maps-end\n");
            fsync(fd); // the safe part is on disk before the unwinder runs
            // the stack last: the one call that can hang (the watchdog above covers it)
            void* frames[64];
            const int n = backtrace(frames, 64);
            for (int i = 0; i < n; i++) {
                Put(fd, "frame=");
                PutHex(fd, reinterpret_cast<unsigned long long>(frames[i]));
                Put(fd, "\n");
            }
            Put(fd, "symbols-begin\n");
            backtrace_symbols_fd(frames, n, fd);
            Put(fd, "symbols-end\n");
            fsync(fd);
            close(fd);
        }
        alarm(0); // done in time: the watchdog is off
    }
    // Hand the signal on: the handler that was there before, else the default action.
    if (index < std::size(kSignals)) {
        sigaction(sig, &g_old[index], nullptr);
    } else {
        signal(sig, SIG_DFL);
    }
    // A fault the CPU raised comes back when the instruction runs again; one sent (abort, kill)
    // is raised again and taken once this handler returns.
    if (!info || info->si_code <= 0 || sig == SIGABRT) {
        raise(sig);
    }
}

void OnTerminate() {
    if (const std::exception_ptr e = std::current_exception()) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& ex) {
            std::snprintf(g_exception, sizeof(g_exception), "%s: %s", typeid(ex).name(),
                          ex.what());
        } catch (...) {
            std::snprintf(g_exception, sizeof(g_exception), "an exception of unknown type");
        }
    } else {
        std::snprintf(g_exception, sizeof(g_exception), "std::terminate called");
    }
    if (g_old_terminate) {
        g_old_terminate();
    }
    std::abort();
}
#endif

} // namespace

std::string MarkerText(int pid, long long started, const std::string& version,
                       const std::string& release, u64 title_id, const std::string& title,
                       const std::string& settings, bool vr, Cause cause,
                       std::string_view detail) {
    std::string s =
        fmt::format("pid={}\nstarted={}\nversion={}\nrelease={}\nmode={}\n", pid, started,
                    OneLine(version), OneLine(release), vr ? "vr" : "flat");
    if (title_id != 0 || !title.empty()) {
        s += fmt::format("title_id={:016X}\ntitle={}\n", title_id, OneLine(title));
    }
    if (cause != Cause::None) {
        s += fmt::format("cause={}\ndetail={}\n", CauseKey(cause), OneLine(detail));
    }
    std::string_view rest = settings;
    while (!rest.empty()) {
        const std::size_t nl = rest.find('\n');
        const std::string_view line = rest.substr(0, nl);
        if (!line.empty()) {
            s += fmt::format("setting={}\n", line);
        }
        if (nl == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(nl + 1);
    }
    return s;
}

void Start(const std::string& dir, const std::string& version, const std::string& release) {
    std::scoped_lock lock{g_mutex};
    if (!g_dir.empty() || dir.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    g_dir = dir;
    g_version = version;
    g_release = release;
    g_started = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
    // A crash record of an earlier run was read before this (or is stale): it goes.
    std::filesystem::remove(dir + kCrashFile, ec);
    WriteMarkerLocked();
#ifdef FRAME_CRASH_POSIX
    std::snprintf(g_crash_path, sizeof(g_crash_path), "%s%s", dir.c_str(), kCrashFile);
    // backtrace() loads libgcc's unwinder on its first call: done now, not in the handler.
    void* warm[4];
    backtrace(warm, 4);
    struct sigaction sa{};
    sa.sa_sigaction = &Handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (std::size_t i = 0; i < std::size(kSignals); i++) {
        sigaction(kSignals[i], &sa, &g_old[i]);
    }
    g_old_terminate = std::set_terminate(&OnTerminate);
#endif
}

void NoteExe(const std::string& path, const std::string& build_id) {
    std::scoped_lock lock{g_mutex};
    g_exe.clear();
    if (path.find('\n') == std::string::npos && build_id.find('\n') == std::string::npos) {
        g_exe = "exe=" + path + "\nexe_build_id=" + build_id + "\n";
    }
    WriteMarkerLocked();
}

void NoteGame(u64 title_id, const std::string& title, const std::string& settings, bool vr) {
    std::scoped_lock lock{g_mutex};
    g_title_id = title_id;
    g_title = title;
    g_settings = settings;
    g_vr = vr;
    if (title_id == 0 && title.empty()) {
        // No game: a cause noted for the last one no longer applies.
        g_cause_noted = Cause::None;
        g_detail.clear();
#ifdef FRAME_CRASH_POSIX
        g_cause.store(0);
#endif
    }
    WriteMarkerLocked();
}

void NoteCause(Cause cause, std::string_view detail) {
    std::scoped_lock lock{g_mutex};
    g_cause_noted = cause;
    g_detail = OneLine(detail);
#ifdef FRAME_CRASH_POSIX
    std::snprintf(g_cause_detail, sizeof(g_cause_detail), "%s", g_detail.c_str());
    g_cause.store(static_cast<int>(cause));
#endif
    WriteMarkerLocked();
}

void CleanExit() {
    std::scoped_lock lock{g_mutex};
    if (g_dir.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(g_dir + kRunningFile, ec);
    std::filesystem::remove(g_dir + kCrashFile, ec);
}

void TestCrash(const std::string& message) {
#ifdef FRAME_CRASH_POSIX
    std::snprintf(g_exception, sizeof(g_exception), "%s", message.c_str());
#endif
    volatile int* volatile nowhere = nullptr;
    *nowhere = 1; // SIGSEGV, through the handler like any crash
    std::abort();
}

} // namespace Common::FrameCrash
