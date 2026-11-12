// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame crashreport: what the next start needs to say why this run ended badly.
//
// Start writes a run marker (<dir>running: the pid, the version, the game, the settings it ran
// with, a cause noted on the way) and installs handlers for the fatal signals and for
// std::terminate. A fatal signal writes <dir>last-crash (the signal, the faulting address, the
// thread, the stack as addresses and as backtrace_symbols lines, and /proc/self/maps) with
// async-signal-safe calls only, then lets the signal take the process as it would have. A clean
// quit removes both files (CleanExit). A marker still there at the next start is a run that did
// not end cleanly; core/frame_crash_report.h reads and classifies it.
//
// The handlers chain: one installed earlier is restored before the signal is raised again, and
// dynarmic's fastmem handler (installed later, when the first JIT is made) handles its own faults
// and passes the rest on to this one. Nothing here sends anything anywhere.

#pragma once

#include <string>
#include <string_view>

#include "common/common_types.h"

namespace Common::FrameCrash {

/// Something noted before the end that says more than the signal does.
enum class Cause : u8 {
    None,
    DeviceLost,   ///< Vulkan reported VK_ERROR_DEVICE_LOST
    CoreFatal,    ///< the game called ERR:F, or the core stopped with an unknown error
    ArmException, ///< the emulated CPU raised an exception
    Stuck,        ///< the game did not stop when asked, and the app quit (startup escape)
};

/// "device-lost", "core-fatal", "arm-exception", "stuck", "" for None.
std::string_view CauseKey(Cause cause);
Cause CauseFromKey(std::string_view key);

inline constexpr const char* kRunningFile = "running";
inline constexpr const char* kCrashFile = "last-crash";

/// `dir` ends with '/' (made when missing). Writes the run marker and installs the handlers.
/// `version` is shown in the report, `release` names the build to Sentry ("azaharframe@1.2.0").
/// Once per process; later calls do nothing.
void Start(const std::string& dir, const std::string& version, const std::string& release);

/// This program's file and its build id: the next start names the crashed build by it, even when
/// the file was replaced since (an update). Rewrites the marker.
void NoteExe(const std::string& path, const std::string& build_id);

/// The game that runs now (0 and "" when none), the settings it runs with ("Label: value"
/// lines) and whether it shows in VR. Rewrites the marker.
void NoteGame(u64 title_id, const std::string& title, const std::string& settings, bool vr);

/// A cause the next start names if this run then ends badly. A later one replaces it.
void NoteCause(Cause cause, std::string_view detail);

/// A clean quit: the marker and any crash record go.
void CleanExit();

/// Test only (AZAHARFRAME_SENTRY_TEST_CRASH, not in release builds): crashes with `message` as the
/// record's exception text.
[[noreturn]] void TestCrash(const std::string& message);

/// For the tests: the marker text Start and NoteGame write.
std::string MarkerText(int pid, long long started, const std::string& version,
                       const std::string& release, u64 title_id,
                       const std::string& title, const std::string& settings, bool vr,
                       Cause cause, std::string_view detail);

} // namespace Common::FrameCrash
