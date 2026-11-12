// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame crashreport: Send Report. Only on the player's tap, Sentry (sentry-native) is
// started for that one report, the report is sent, and Sentry is closed again: no telemetry, no
// session, no crash handler of Sentry's. What goes is the report as "See What's Sent" shows it:
// an event with the reason, the game, the version, the GPU and the crash's stack (addresses and
// the build ids of the modules, so Sentry can name the functions), and report.txt and the log
// tail as attachments. Sentry's own extras (the OS context, the user, breadcrumbs, the modules
// of this process) are taken out, and send-default-PII is off. Sentry still sees the address the
// request comes from (the project is set not to store it); the event asks it not to attach it
// (sdk.settings.infer_ip: never) and to derive no place from it (an empty user.geo).

#pragma once

#include <string>

#include "core/frame_crash_report.h"

namespace Core::CrashReport {

/// The DSN built in (AZAHARFRAME_SENTRY_DSN at configure time); "" when none.
std::string BuiltInDsn();
/// Send Report can be offered: Sentry is built in and there is a DSN.
bool CanSend();
/// "azaharframe@1.2.0" for a release, "azaharframe@<commit>" for a development build.
std::string ReleaseName();
/// "production" for release builds, else "development".
std::string Environment();

struct SendResult {
    bool ok = false;
    int status = 0;       ///< the HTTP status (0: no answer)
    std::string error;    ///< what went wrong, for the player ("Couldn't Connect")
    std::string event_id; ///< Sentry's id for the report
};

/// A manual report's title in Sentry: the first line of what the player wrote (120 characters at
/// most), or "Problem Report".
std::string ManualTitle(const std::string& note);

/// Sends `r` to `dsn` (a test passes its own; the app BuiltInDsn()). `work_dir` holds Sentry's
/// database while it runs, removed after. Blocks (a few seconds at most): call it off the GUI
/// thread. One send at a time.
SendResult Send(const Report& r, const std::string& dsn, const std::string& work_dir);

} // namespace Core::CrashReport
