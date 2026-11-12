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

#include <vector>

#include <QPointer>

class QAction;
class QObject;
class QShortcut;
class QPointF;
class QWidget;
class QWindow;

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

// AzaharFrame popups: Qt opens a popup (a menu, a combo box's list) in show_helper through
// QApplicationPrivate::openPopup, which puts its QWindow on QGuiApplicationPrivate::popup_list
// only when that QWindow is visible (activatePopup). A popup kept off the screen never shows its
// QWindow, so Qt did not know it was open: QApplication::activePopupWidget() stayed null, a click
// outside did not close it, Escape went to the main window, and every new menu stayed drawn on
// the panel over the others, across games. OpenPopup does what activatePopup and openPopup
// would have done (without the platform's pointer grab, which an unmapped window cannot have);
// Qt's own hide, close and destroy take it off the list again (QApplicationPrivate::closePopup).

/// Called on a top-level window's ShowToParent event (after Qt's show_helper): a popup kept off
/// the screen and not on Qt's popup list is put on it. True when it was.
bool OpenPopup(QWidget* w);

/// True when `w`'s window is on Qt's popup list.
bool OnPopupList(const QWidget* w);

/// The beam enters and leaves windows as a mouse does. Qt keeps the widget under the pointer
/// (QWidgetWindow's qt_last_mouse_receiver, which a context menu event goes to) from the Enter
/// and Leave events a window system sends when the pointer crosses windows, also when a popup
/// closes under it. Synthetic input has none: after a menu closed by Escape, Qt still took the
/// hidden menu for the widget under the pointer and the next right click's menu never opened.
/// Call before each mouse event for `target` (null: the beam left the panel); `current` is the
/// window the beam was last in.
void PointerIn(QPointer<QWindow>& current, QWindow* target, const QPointF& local,
               const QPointF& global);

/// Azahar's Exit Fullscreen hotkey is Esc in the whole application's context (so that it works
/// in the render window), and Qt's shortcut map hands it every Escape before a window that is not
/// modal sees the key: B could not close the Multiplayer windows (Create Room, Direct Connect), a
/// Cheats window and the like. While B's Escape (or the headset keyboard's) goes to a popup or a
/// window other than the main one, the enabled application-wide shortcuts and actions on Esc in
/// `main` sit out; they are enabled again when this goes.
class EscapeShortcutsPaused {
public:
    explicit EscapeShortcutsPaused(const QWidget* main);
    ~EscapeShortcutsPaused();
    EscapeShortcutsPaused(const EscapeShortcutsPaused&) = delete;
    EscapeShortcutsPaused& operator=(const EscapeShortcutsPaused&) = delete;

private:
    std::vector<QPointer<QShortcut>> shortcuts;
    std::vector<QPointer<QAction>> actions;
};

/// Closes every popup on Qt's list, newest first, and hides any other popup still visible (at
/// most a few rounds). Returns how many were closed.
int CloseAllPopups();

} // namespace FrameOffscreen
