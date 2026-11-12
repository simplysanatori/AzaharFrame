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

#pragma once

#include <QString>

class QDialog;
class QWidget;

namespace Core::CrashReport {
struct Report;
}

namespace FrameCrashDialog {

/// At start, after the log has started (the last run's log is then azahar_log.old.txt): reads
/// and removes the last run's marker and record, and starts this run's. GUI thread, once.
void Detect();

/// Shows the dialog for what Detect found, once (nothing when the last run ended cleanly).
void ShowPending(QWidget* parent);

/// Where Save Report writes: AZAHARFRAME_REPORTS_DIR, else ~/Documents/AzaharFrame Reports.
QString ReportsDir();

/// The dialogs, for ShowPending and the pictures.
QDialog* MakeDialog(QWidget* parent, const Core::CrashReport::Report& report, bool can_send);
QDialog* MakeWhatsSent(QWidget* parent, const Core::CrashReport::Report& report, bool can_send);
QDialog* MakeSaved(QWidget* parent, const QString& shown_path);

/// Test driver (not in release builds): AZAHARFRAME_TEST_CRASH_SHOTS=DIR saves the dialogs of a
/// sample report as PNGs into DIR. AZAHARFRAME_TEST_CRASH_ANSWER=send|save|dont answers the real
/// dialog by itself after 2 s (with the note "Mock run: Send Report test").
void TestShots(QWidget* parent);

} // namespace FrameCrashDialog
