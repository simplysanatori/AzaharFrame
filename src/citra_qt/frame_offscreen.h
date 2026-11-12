// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame stall: the windows kept off the gamescope screen in VR (FrameMirror's vrfocus,
// Qt::WA_DontShowOnScreen), and the modal list they can leave behind.
//
// Qt registers a modal window kept off the screen in QWidgetPrivate::show_sys
// (QGuiApplicationPrivate::showModalWindow) and takes it out in hide_sys only while the window
// still has WA_DontShowOnScreen. QMessageBox clears that attribute itself as it hides
// (QMessageBoxPrivate::setVisible sets it to "a native dialog is in use", false on Linux), and
// ReleaseOffscreen clears it on dialogs open when VR ends. hide_sys then takes the QWindow path,
// which does nothing for a QWindow that never mapped, so the window stays on Qt's modal list:
// every other window (the main window, its menu bar, the game list) stays blocked from mouse
// input after the box has gone, while keys sent straight to widgets still work.

#pragma once

class QObject;
class QWidget;

namespace FrameOffscreen {

/// True when `w` is a top-level window that must stay off the screen in VR (not `main`, not the
/// desktop, not the render window).
bool Transient(const QWidget* w, const QWidget* main);

/// Marks `w` (a hidden top-level window) as not to map; true when it was.
bool Keep(QWidget* w, const QWidget* main);

/// Back in flat: the windows kept off the screen may map again; one open while VR ended is
/// closed (it never had an X window on the screen, and mapping one under a running exec() is not
/// safe), later, from `context`'s thread.
void Release(QWidget* main, QObject* context);

/// Called on every top-level window's Hide event: a hidden window still on Qt's modal list is
/// taken off it (and the windows it blocked are unblocked). True when it was on the list.
bool DropStaleModal(QWidget* w);

/// True when `w`'s window is on Qt's modal list (for the log and the tests).
bool OnModalList(const QWidget* w);

} // namespace FrameOffscreen
