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
// Window Scale (Settings::values.vr_window_scale, 100 to 200 %) makes the window look larger on
// the panel by showing less of it: the window gets 1/scale of the logical size and the panel
// grows a little (kPanelGrowth); the image is rendered at more pixels per logical pixel, so
// the text stays sharp. Flat mode keeps the 100 % size.
// AZAHARFRAME_MIRROR_SIZE=WxH sets the window size at 100 % (logical pixels, default
// 1056x704) and AZAHARFRAME_MIRROR_SCALE the image's pixels per window pixel (default: the
// image is about 2900 to 3400 pixels wide; it stays at most 3400 pixels wide or high).

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
#include "frame_xr/keyboard/keyboard.h" // AzaharFrame keyboard

class QPushButton;
class QToolBar;
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
    /// AzaharFrame vrqol: a button in the row at the top of the window that shows only while the
    /// in-game menu is open in VR (never on the desktop or in flat). New buttons go to the right
    /// of the ones before. `callback` runs on the GUI thread when the button is clicked (a
    /// checkable button is already toggled). Null when the window has no room for the row. The
    /// stock buttons (Quick Save, Quick Load, Passthrough) are added by citra_qt.cpp; the controls
    /// help and the stats panel add theirs here.
    QPushButton* AddVrMenuButton(const QString& label, std::function<void()> callback);
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
    qreal Scale(const QSize& canvas, float panel_scale) const;
    /// Window Scale as a factor (1 to 2), from the setting.
    qreal WindowScale() const;
    /// The window's logical size at a Window Scale (the panel's whole width at 100 %).
    QSize WindowSizeFor(qreal window_scale) const;
    /// How much wider than its placed width the panel is: the growth for the Window Scale, and
    /// more for a dialog that does not fit the window, up to a limit.
    float PanelScale(const QSize& canvas) const;
    /// Resizes the window to the size for the Window Scale (VR) or 100 % (flat).
    /// True when the size changed.
    bool ApplyWindowSize();
    /// Visible top-level windows other than the main window, bottom to top.
    std::vector<QWidget*> Stack() const;
    QWidget* WindowAt(const QPoint& global) const;
    int Timestamp() const;

    QPointer<QWidget> main;
    QSize window_size{1056, 704}; // at 100 %; 3:2, the picker's shape
    qreal scale = 0.0;            // pixels per window pixel; 0: from the panel's size
    QSize requested_size;         ///< what ApplyWindowSize last asked the window for
    bool Capturing() const {
        return vr_showing && (!game_running || menu_open);
    }
    void Shown();

    void UpdateVrBar();
    QPointer<QToolBar> vr_bar; ///< AzaharFrame vrqol: hidden unless the in-game menu shows in VR

    bool game_running = false;
    bool menu_open = false;
    bool vr_showing = true;
    std::function<void()> close_menu;
    bool capturing = false;
    bool dirty = true;
    bool logged = false;
    QSize logged_size;
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

    // AzaharFrame keyboard: the headset keyboard's keys typed into the focused widget, and the
    // keyboard opened by a click into a text field and closed when no text field has the keys.
    void StartKeyboard();
    void TypeKey(const FrameXr::KeyboardEvent& e);
    /// The window keys go to: the open popup, the modal dialog, the topmost dialog, the main one.
    QWidget* KeyTarget() const;
    /// `clicked`: a left click was just let go at last_global.
    void CheckTextFocus(bool clicked);
    QTimer focus_timer; ///< notices a text field losing the keys without a click
    bool text_focus = false;
};
