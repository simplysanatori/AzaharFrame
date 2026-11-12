// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <QAction>
#include <QApplication>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QPointer>
#include <QShortcut>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <qpa/qwindowsysteminterface.h>

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

bool OnPopupList(const QWidget* w) {
    QWindow* win = w ? w->windowHandle() : nullptr;
    if (!win) {
        return false;
    }
#ifdef FRAME_OFFSCREEN_QT_PRIVATE
    return QGuiApplicationPrivate::popup_list.contains(win);
#else
    return QApplication::activePopupWidget() == w;
#endif
}

bool OpenPopup(QWidget* w) {
    if (!w || !w->isWindow() || w->windowType() != Qt::Popup || !w->isVisible() ||
        !w->testAttribute(Qt::WA_DontShowOnScreen) || !w->windowHandle() || OnPopupList(w)) {
        return false;
    }
#ifdef FRAME_OFFSCREEN_QT_PRIVATE
    // QGuiApplicationPrivate::activatePopup without its isVisible() check.
    QGuiApplicationPrivate::popup_list.append(w->windowHandle());
    // QApplicationPrivate::openPopup's focus: the popup's own focus widget, else the widget that
    // had the keys loses them (the popup takes the keys; QApplicationPrivate::closePopup gives
    // them back).
    if (QWidget* fw = w->focusWidget()) {
        fw->setFocus(Qt::PopupFocusReason);
    } else if (QGuiApplicationPrivate::popupCount() == 1) {
        if (QWidget* had = QApplication::focusWidget()) {
            QFocusEvent out(QEvent::FocusOut, Qt::PopupFocusReason);
            QCoreApplication::sendEvent(had, &out);
        }
    }
    LOG_INFO(Frontend, "xr: popup {} open (off the screen; {} open)", w->metaObject()->className(),
             QGuiApplicationPrivate::popupCount());
    return true;
#else
    return false;
#endif
}

void PointerIn(QPointer<QWindow>& current, QWindow* target, const QPointF& local,
               const QPointF& global) {
    if (current == target) {
        return;
    }
    using Sync = QWindowSystemInterface::SynchronousDelivery;
    if (current) {
        QWindowSystemInterface::handleLeaveEvent<Sync>(current);
    }
    current = target;
    if (target) {
        QWindowSystemInterface::handleEnterEvent<Sync>(target, local, global);
    }
}

EscapeShortcutsPaused::EscapeShortcutsPaused(const QWidget* main) {
    if (!main) {
        return;
    }
    const QKeySequence esc(Qt::Key_Escape);
    for (QShortcut* sc : main->findChildren<QShortcut*>()) {
        if (sc->isEnabled() && sc->context() == Qt::ApplicationShortcut &&
            sc->keys().contains(esc)) {
            sc->setEnabled(false);
            shortcuts.emplace_back(sc);
        }
    }
    for (QAction* a : main->findChildren<QAction*>()) {
        if (a->isEnabled() && a->shortcutContext() == Qt::ApplicationShortcut &&
            a->shortcuts().contains(esc)) {
            a->setEnabled(false);
            actions.emplace_back(a);
        }
    }
}

EscapeShortcutsPaused::~EscapeShortcutsPaused() {
    for (const auto& sc : shortcuts) {
        if (sc) {
            sc->setEnabled(true);
        }
    }
    for (const auto& a : actions) {
        if (a) {
            a->setEnabled(true);
        }
    }
}

int CloseAllPopups() {
    int closed = 0;
    // Closing a menu can close its parents too (hideUpToMenuBar), or open nothing new: a few
    // rounds at most.
    for (int round = 0; round < 64; round++) {
        QWidget* popup = QApplication::activePopupWidget();
        if (!popup) {
            for (QWidget* w : QApplication::topLevelWidgets()) {
                if (w->windowType() == Qt::Popup && w->isVisible()) {
                    popup = w;
                    break;
                }
            }
        }
        if (!popup) {
            break;
        }
        LOG_INFO(Frontend, "xr: closing popup {}", popup->metaObject()->className());
        popup->close();
        if (popup->isVisible()) {
            popup->hide(); // (a popup that refuses its close event)
        }
        closed++;
    }
    return closed;
}

} // namespace FrameOffscreen
