// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame crashreport: the start after a run that ended badly. Detect reads what the last run
// left (core/frame_crash_report.h, and memguard's out-of-memory note from the launcher) and
// starts this run's recorder (common/frame_crash.h); ShowPending then shows "Azahar Closed
// Unexpectedly" (or memguard's "Azahar Ran Out of Memory") once: the reason, the game, a "What
// Were You Doing?" box, See What's Sent, and Don't Send, Save Report and Send Report (only with a
// DSN built in). It is a Qt dialog in both modes: at start in VR no game runs, so there is no
// in-game menu, and Azahar's window (with this dialog over it) is what VR shows, with the headset
// keyboard for the text box. Nothing is sent without the Send Report tap.
//
// Report a Problem (Help in Qt; Settings, General, Help in the VR menu) is the same dialog for a
// report the player starts: "What Happened?" (needed for Send Report), Include a Screenshot (the
// game's top and bottom screens, off by default), no reason. About Azahar for Frame is beside it.

#pragma once

#include <functional>

#include <QByteArray>
#include <QString>

#include "common/common_types.h"

class QDialog;
class QWidget;

namespace Core::CrashReport {
struct Report;
}
namespace Core::AngelsReport {
struct Info;
}

namespace FrameCrashDialog {

/// At start, after the log has started (the last run's log is then azahar_log.old.txt): reads
/// and removes the last run's marker and record, and starts this run's. GUI thread, once.
void Detect();

/// Shows the dialog for what Detect found, once (nothing when the last run ended cleanly).
void ShowPending(QWidget* parent);

/// Takes the game's top and bottom screens and calls back on the GUI thread with a PNG ("" when it
/// could not).
using ScreenshotFn = std::function<void(std::function<void(QByteArray)>)>;

/// What Report a Problem knows of the game now (none: title_id 0 and title "").
struct ProblemContext {
    u64 title_id = 0;
    std::string title;
    ScreenshotFn screenshot; ///< empty when no game runs (no Include a Screenshot)
};

/// Report a Problem, over `parent` (in VR Azahar's window shows it).
void ShowReportProblem(QWidget* parent, const ProblemContext& context);
/// AzaharFrame kiumenus: Angel's Aim's Send Report (the Mods tab's segment): Bug or Idea, a short
/// text on the VR keyboard, one line of consent, then Send Report. Nothing goes without that tap.
void ShowAngelsAimReport(QWidget* parent, const Core::AngelsReport::Info& info);
QDialog* MakeAngelsAimReport(QWidget* parent, const Core::AngelsReport::Info& info);
/// About Azahar for Frame: the version and build, GitHub and Ko-fi, and the credits
/// (THIRD_PARTY_NOTICES.md next to the program). Links open in Flat; VR shows them as text.
void ShowAbout(QWidget* parent);

/// Where Save Report writes: AZAHARFRAME_REPORTS_DIR, else ~/Documents/AzaharFrame Reports.
QString ReportsDir();

/// The dialogs, for ShowPending and the pictures.
QDialog* MakeDialog(QWidget* parent, const Core::CrashReport::Report& report, bool can_send);
QDialog* MakeWhatsSent(QWidget* parent, const Core::CrashReport::Report& report, bool can_send);
QDialog* MakeSaved(QWidget* parent, const QString& shown_path);
QDialog* MakeProblem(QWidget* parent, const ProblemContext& context);
QDialog* MakeAbout(QWidget* parent, bool links_open);

/// Test driver (not in release builds): AZAHARFRAME_TEST_CRASH_SHOTS=DIR saves the dialogs of a
/// sample report as PNGs into DIR. AZAHARFRAME_TEST_CRASH_ANSWER=send|save|dont answers the real
/// dialog by itself after 2 s (with the note "Mock run: Send Report test"; Report a Problem's:
/// "Sentry test feedback <time>"). AZAHARFRAME_TEST_REPORT_PROBLEM=1 opens Report a Problem 4 s
/// after the start.
void TestShots(QWidget* parent);

} // namespace FrameCrashDialog
