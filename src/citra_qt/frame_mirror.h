// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: Azahar's own Qt window as the VR home panel. While no game runs, the
// main window (with its open menus, popups, tooltips and dialogs composited where
// they are on the desktop) is rendered into an image whenever it repaints (at most
// 30 times a second) and handed to frame_xr, which shows it on a panel; the
// controller ray's hover, clicks, wheel and Escape come back and are fed to Qt as
// if they were the mouse and keyboard (the way QTest does), so menus, double
// clicks, context menus and dialogs behave as on the desktop.
//
// Set AZAHARFRAME_VR_PICKER=1 for the old VR game picker instead.
// AZAHARFRAME_MIRROR_SIZE=WxH sets the window size (logical pixels, default
// 1056x704) and AZAHARFRAME_MIRROR_SCALE the image's pixels per window pixel
// (default 2.75; the image stays at most 3400 pixels wide or high).

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <vector>

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "frame_xr/frame_xr.h"

class QWidget;
struct FrameMirrorInbox;

class FrameMirror final : public QObject {
public:
    /// False when AZAHARFRAME_VR_PICKER=1 asks for the picker.
    static bool Wanted();

    /// Mirrors `main_window` as the home panel (GUI thread, after FrameXr::Start).
    explicit FrameMirror(QWidget* main_window);
    ~FrameMirror() override;

    /// A game started (no capture while it runs) or stopped (capture again). GUI thread.
    void SetGameRunning(bool running);
    /// The in-game menu opened (the window is captured over the paused game) or closed.
    void SetMenuOpen(bool open);
    bool MenuOpen() const {
        return menu_open;
    }
    /// VR (a session: capture as above) or flat (the window is on the screen itself: no capture).
    void SetVrShowing(bool showing);
    /// What B does in the in-game menu when no menu or dialog is open: close it.
    void SetCloseMenuHandler(std::function<void()> handler) {
        close_menu = std::move(handler);
    }

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void Queue(const FrameXr::HomePanelEvent& e); // XR thread
    void Drain();
    void Deliver(const FrameXr::HomePanelEvent& e);
    void MarkDirty();
    void Capture();
    void LogLayout(const QRect& canvas);
    void FitDialog(QWidget* dialog);
    /// The panel's area (global): the main window and any dialog over it.
    QRect Canvas() const;
    /// Around the main window: top-level windows here belong on the panel.
    QRect Near() const;
    qreal Scale(const QSize& canvas) const;
    /// Visible top-level windows other than the main window, bottom to top.
    std::vector<QWidget*> Stack() const;
    QWidget* WindowAt(const QPoint& global) const;
    int Timestamp() const;

    QPointer<QWidget> main;
    QSize window_size{1056, 704}; // 3:2, the picker's shape
    qreal scale = 2.75;
    bool Capturing() const {
        return vr_showing && (!game_running || menu_open);
    }
    void Shown();

    bool game_running = false;
    bool menu_open = false;
    bool vr_showing = true;
    std::function<void()> close_menu;
    bool capturing = false;
    bool dirty = true;
    bool logged = false;
    bool log_layout = false;
    std::string last_layout;
    int captures = 0;
    int published = 0;
    int last_cost_ms = 0;
    qint64 total_cost_ms = 0;
    QTimer capture_timer; ///< single shot, at most every 33 ms
    QTimer refresh_timer; ///< a capture once a second, for repaints Qt does not report
    QElapsedTimer since_capture;
    QElapsedTimer clock;
    std::shared_ptr<const FrameXr::HomePanelImage> last_image;

    std::shared_ptr<FrameMirrorInbox> inbox;
    // Input from the XR thread, delivered on the GUI thread (moves coalesced).
    std::mutex queue_mutex;
    std::vector<FrameXr::HomePanelEvent> queue;
    bool drain_queued = false;
    Qt::MouseButtons buttons;
    QPointer<QWidget> grab; ///< the window a held button was pressed in
    QPointF last_global{-1.0, -1.0};
};
