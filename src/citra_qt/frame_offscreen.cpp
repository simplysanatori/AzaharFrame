// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <QApplication>
#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#if __has_include(<QtGui/private/qguiapplication_p.h>)
#include <QtGui/private/qguiapplication_p.h>
#define FRAME_OFFSCREEN_QT_PRIVATE 1
#endif

#include "citra_qt/frame_offscreen.h"
#include "common/logging/log.h"

namespace FrameOffscreen {

bool Transient(const QWidget* w, const QWidget* main) {
    return w && w != main && w->isWindow() && w->windowType() != Qt::Desktop &&
           !w->inherits("GRenderWindow");
}

bool Keep(QWidget* w, const QWidget* main) {
    if (!Transient(w, main) || w->isVisible()) {
        return false; // (a window already on the screen cannot be taken off before it shows)
    }
    w->setAttribute(Qt::WA_DontShowOnScreen, true);
    return true;
}

void Release(QWidget* main, QObject* context) {
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (!w->testAttribute(Qt::WA_DontShowOnScreen) || !Transient(w, main)) {
            continue;
        }
        if (!w->isVisible()) {
            w->setAttribute(Qt::WA_DontShowOnScreen, false);
            continue;
        }
        // Open while VR ended: closed (a dialog is rejected), and only then allowed on the
        // screen, so that its hide still takes it off Qt's modal list.
        LOG_INFO(Frontend, "xr: closing {} (it was open when VR ended)",
                 w->metaObject()->className());
        QPointer<QWidget> window = w;
        QTimer::singleShot(0, context, [window] {
            if (!window) {
                return;
            }
            window->close();
            if (window && !window->isVisible()) {
                window->setAttribute(Qt::WA_DontShowOnScreen, false);
                DropStaleModal(window); // (a message box clears the attribute itself)
            }
        });
    }
}

bool OnModalList(const QWidget* w) {
    QWindow* win = w ? w->windowHandle() : nullptr;
    if (!win) {
        return false;
    }
#ifdef FRAME_OFFSCREEN_QT_PRIVATE
    return QGuiApplicationPrivate::instance()->modalWindowList.contains(win);
#else
    return QGuiApplication::modalWindow() == win;
#endif
}

bool DropStaleModal(QWidget* w) {
    if (!w || !w->isWindow() || w->isVisible() || !OnModalList(w)) {
        return false;
    }
    LOG_INFO(Frontend, "xr: {} hidden but still modal (it was off the screen): released",
             w->metaObject()->className());
#ifdef FRAME_OFFSCREEN_QT_PRIVATE
    QGuiApplicationPrivate::hideModalWindow(w->windowHandle());
#else
    // Without Qt's private API the window stays listed until it is destroyed (a message box
    // from QMessageBox's static functions is, right after); the others are checked again then.
    QTimer::singleShot(0, qApp, [] {
        for (QWindow* t : QGuiApplication::topLevelWindows()) {
            t->setTransientParent(t->transientParent()); // recomputes its blocked state
        }
    });
#endif
    return true;
}

} // namespace FrameOffscreen
