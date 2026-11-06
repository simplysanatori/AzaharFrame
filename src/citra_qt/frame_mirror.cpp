// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: see frame_mirror.h.

#include <algorithm>
#include <cstring>

#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QImage>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QScreen>
#include <QStringList>
#include <QTreeView>
#include <QWidget>
#include <QWindow>
#include <fmt/format.h>

#include "citra_qt/frame_mirror.h"
#include "common/logging/log.h"

QT_BEGIN_NAMESPACE
// QtGui's entry points for synthetic input (exported for QtTest, which declares them
// the same way in qtestmouse.h / qtestwheel.h / qtestkeyboard.h). They go through
// QWindowSystemInterface, so Qt handles them as real input: enter/leave, popups
// grabbing the mouse, double-click detection, context menus, modal blocking.
Q_GUI_EXPORT void qt_handleMouseEvent(QWindow* window, const QPointF& local,
                                      const QPointF& global, Qt::MouseButtons state,
                                      Qt::MouseButton button, QEvent::Type type,
                                      Qt::KeyboardModifiers mods, int timestamp);
Q_GUI_EXPORT void qt_handleWheelEvent(QWindow* window, const QPointF& local,
                                      const QPointF& global, QPoint pixelDelta,
                                      QPoint angleDelta, Qt::KeyboardModifiers mods,
                                      Qt::ScrollPhase phase);
Q_GUI_EXPORT void qt_handleKeyEvent(QWindow* w, QEvent::Type t, int k,
                                    Qt::KeyboardModifiers mods, const QString& text,
                                    bool autorep, ushort count);
QT_END_NAMESPACE

namespace {

// The image of the panel stays under the size SteamVR on the Frame shows unscaled.
constexpr int kMaxPixels = 3400;
// Dialogs get a title strip drawn above them (the window manager's is not in the image).
constexpr int kTitleHeight = 28;
constexpr int kCaptureMs = 33; // at most 30 captures a second

bool IsDialogLike(const QWidget* w) {
    const Qt::WindowType type = w->windowType();
    return type == Qt::Dialog || type == Qt::Window || type == Qt::Tool || type == Qt::Sheet;
}

} // namespace

// The XR thread's handle on the mirror: outlives it, so a late event is dropped.
struct FrameMirrorInbox {
    std::mutex mutex;
    FrameMirror* owner = nullptr;
};

bool FrameMirror::Wanted() {
    return qEnvironmentVariable("AZAHARFRAME_VR_PICKER") != QStringLiteral("1");
}

FrameMirror::FrameMirror(QWidget* main_window) : QObject(main_window), main(main_window) {
    const QStringList size = qEnvironmentVariable("AZAHARFRAME_MIRROR_SIZE").split(QLatin1Char('x'));
    if (size.size() == 2 && size[0].toInt() >= 640 && size[1].toInt() >= 400) {
        window_size = QSize(size[0].toInt(), size[1].toInt());
    }
    bool ok = false;
    const double env_scale = qEnvironmentVariable("AZAHARFRAME_MIRROR_SCALE").toDouble(&ok);
    if (ok && env_scale >= 1.0 && env_scale <= 4.0) {
        scale = env_scale;
    }
    log_layout = qEnvironmentVariableIntValue("AZAHARFRAME_MIRROR_LOG_LAYOUT") != 0;
    clock.start();
    since_capture.start();
    capture_timer.setSingleShot(true);
    connect(&capture_timer, &QTimer::timeout, this, [this] { Capture(); });
    refresh_timer.setInterval(1000);
    connect(&refresh_timer, &QTimer::timeout, this, [this] { Capture(); });
    refresh_timer.start();

    // The window gets the panel's size (also on the desktop), unless maximised.
    // On its screen if it fits, so Qt does not move menus and dropdowns to stay on it.
    if (!main->isMaximized() && !main->isFullScreen()) {
        main->resize(window_size);
        if (const QScreen* screen = main->screen()) {
            const QRect avail = screen->availableGeometry();
            QRect r = main->geometry();
            if (r.width() <= avail.width() && r.height() <= avail.height() && !avail.contains(r)) {
                r.moveLeft(std::clamp(r.left(), avail.left(), avail.right() + 1 - r.width()));
                r.moveTop(std::clamp(r.top(), avail.top(), avail.bottom() + 1 - r.height()));
                main->move(r.topLeft() + (main->pos() - main->geometry().topLeft()));
            }
        }
    }
    qApp->installEventFilter(this);

    inbox = std::make_shared<FrameMirrorInbox>();
    inbox->owner = this;
    FrameXr::SetHomePanel([inbox = inbox](const FrameXr::HomePanelEvent& e) {
        std::scoped_lock lock{inbox->mutex};
        if (inbox->owner) {
            inbox->owner->Queue(e);
        }
    });
    LOG_INFO(Frontend, "xr: home panel: Azahar's window (AZAHARFRAME_VR_PICKER=1: the picker)");
    MarkDirty();
}

FrameMirror::~FrameMirror() {
    {
        std::scoped_lock lock{inbox->mutex};
        inbox->owner = nullptr;
    }
    FrameXr::SetHomePanel({});
    qApp->removeEventFilter(this);
}

void FrameMirror::SetGameRunning(bool running) {
    game_running = running;
    if (running) {
        capture_timer.stop();
        refresh_timer.stop();
        LOG_INFO(Frontend, "xr: home panel hidden ({} captures, {} images sent, {} ms per capture)",
                 captures, published, captures ? total_cost_ms / captures : 0);
    } else {
        refresh_timer.start();
        MarkDirty();
    }
}

int FrameMirror::Timestamp() const {
    return static_cast<int>(clock.elapsed());
}

QRect FrameMirror::Canvas() const {
    return main ? main->geometry() : QRect{};
}

qreal FrameMirror::Scale(const QSize& canvas) const {
    qreal s = scale;
    s = std::min(s, static_cast<qreal>(kMaxPixels) / std::max(1, canvas.width()));
    s = std::min(s, static_cast<qreal>(kMaxPixels) / std::max(1, canvas.height()));
    return s;
}

std::vector<QWidget*> FrameMirror::Stack() const {
    std::vector<QWidget*> windows, popups, tips;
    QWidget* const modal = QApplication::activeModalWidget();
    QWidget* const popup = QApplication::activePopupWidget();
    const QRect canvas = Canvas();
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w == main || !w->isVisible() || w->windowType() == Qt::Desktop ||
            w->inherits("GRenderWindow") || !w->geometry().intersects(canvas)) {
            continue;
        }
        switch (w->windowType()) {
        case Qt::ToolTip:
            tips.push_back(w);
            break;
        case Qt::Popup:
            popups.push_back(w);
            break;
        default:
            windows.push_back(w);
            break;
        }
    }
    // The modal dialog and the newest popup on top.
    std::stable_partition(windows.begin(), windows.end(), [&](QWidget* w) { return w != modal; });
    std::stable_partition(popups.begin(), popups.end(), [&](QWidget* w) { return w != popup; });
    windows.insert(windows.end(), popups.begin(), popups.end());
    windows.insert(windows.end(), tips.begin(), tips.end());
    return windows;
}

QWidget* FrameMirror::WindowAt(const QPoint& global) const {
    // An open popup has the mouse (as with a pointer grab); Qt closes it on a click outside.
    if (QWidget* popup = QApplication::activePopupWidget()) {
        return popup;
    }
    const auto stack = Stack();
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        if ((*it)->windowType() != Qt::ToolTip && (*it)->geometry().contains(global)) {
            return *it;
        }
    }
    return main;
}

bool FrameMirror::eventFilter(QObject* object, QEvent* event) {
    if (game_running || capturing) {
        return false;
    }
    switch (event->type()) {
    case QEvent::UpdateRequest:
    case QEvent::Paint:
    case QEvent::Hide:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::WindowStateChange:
        MarkDirty();
        break;
    case QEvent::Show:
        MarkDirty();
        if (auto* w = qobject_cast<QWidget*>(object);
            w && w != main && w->isWindow() && IsDialogLike(w) && !w->inherits("GRenderWindow")) {
            QPointer<QWidget> dialog{w};
            QTimer::singleShot(0, this, [this, dialog] {
                if (dialog) {
                    FitDialog(dialog);
                }
            });
        }
        break;
    default:
        break;
    }
    return false;
}

void FrameMirror::FitDialog(QWidget* dialog) {
    if (!main || !dialog->isVisible()) {
        return;
    }
    // Centred over the main window and no larger than it, title strip included.
    const QRect canvas = Canvas();
    const QSize room(canvas.width() - 16, canvas.height() - kTitleHeight - 16);
    const QSize size = dialog->size().boundedTo(room).expandedTo(dialog->minimumSize());
    if (size != dialog->size()) {
        dialog->resize(size);
    }
    const int top = canvas.top() + (canvas.height() - size.height() - kTitleHeight) / 2;
    const QPoint pos(canvas.left() + (canvas.width() - size.width()) / 2,
                     std::max(canvas.top() + kTitleHeight, top + kTitleHeight));
    if (dialog->geometry().topLeft() != pos) {
        dialog->move(pos + (dialog->pos() - dialog->geometry().topLeft())); // frame offset
    }
    MarkDirty();
}

void FrameMirror::MarkDirty() {
    if (game_running || capturing) {
        return;
    }
    dirty = true;
    if (!capture_timer.isActive()) {
        // At most 30 a second, and the GUI thread keeps at least half of its time.
        const qint64 gap = std::max(kCaptureMs, 2 * last_cost_ms);
        capture_timer.start(static_cast<int>(std::max<qint64>(0, gap - since_capture.elapsed())));
    }
}

void FrameMirror::Capture() {
    if (!main || game_running || capturing) {
        return;
    }
    dirty = false;
    since_capture.restart();
    QElapsedTimer cost;
    cost.start();
    const auto account = [&] {
        last_cost_ms = static_cast<int>(cost.elapsed());
        total_cost_ms += last_cost_ms;
    };
    const QRect canvas = Canvas();
    if (canvas.isEmpty()) {
        return;
    }
    const qreal s = Scale(canvas.size());
    const QSize px(qRound(canvas.width() * s), qRound(canvas.height() * s));
    QImage img(px, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(s);
    img.fill(main->palette().color(QPalette::Window));
    capturing = true;
    {
        QPainter p(&img);
        main->render(&p, QPoint(), QRegion(), QWidget::DrawWindowBackground | QWidget::DrawChildren);
        bool dimmed = false;
        QWidget* const modal = QApplication::activeModalWidget();
        for (QWidget* w : Stack()) {
            const QRect r = w->geometry().translated(-canvas.topLeft());
            const bool dialog = w->windowType() != Qt::Popup && w->windowType() != Qt::ToolTip;
            if (dialog) {
                // The main window behind a modal dialog is dimmed, as it cannot be used.
                if (!dimmed && (w == modal || w->isModal())) {
                    p.fillRect(QRect(QPoint(), canvas.size()), QColor(0, 0, 0, 110));
                    dimmed = true;
                }
                const QRect title(r.left() - 1, r.top() - kTitleHeight, r.width() + 2, kTitleHeight);
                p.fillRect(title, w->palette().color(QPalette::Highlight).darker(130));
                QFont font = w->font();
                font.setBold(true);
                p.setFont(font);
                p.setPen(Qt::white);
                p.drawText(title.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                           w->windowTitle());
            }
            w->render(&p, r.topLeft(), QRegion(),
                      QWidget::DrawWindowBackground | QWidget::DrawChildren);
            if (dialog) {
                p.setPen(QColor(0, 0, 0, 170));
                p.setBrush(Qt::NoBrush);
                p.drawRect(r.adjusted(-1, -kTitleHeight, 0, 0));
            }
        }
    }
    capturing = false;
    captures++;
    if (log_layout) {
        LogLayout(canvas);
    }

    const QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
    const std::size_t bytes = static_cast<std::size_t>(px.width()) * px.height() * 4;
    if (rgba.bytesPerLine() != px.width() * 4) {
        account();
        return; // cannot happen for 4-byte pixels
    }
    const uchar* bits = rgba.constBits();
    if (last_image && last_image->width == static_cast<uint32_t>(px.width()) &&
        last_image->height == static_cast<uint32_t>(px.height()) &&
        std::memcmp(last_image->rgba.data(), bits, bytes) == 0) {
        account();
        return; // unchanged
    }
    auto out = std::make_shared<FrameXr::HomePanelImage>();
    out->width = static_cast<uint32_t>(px.width());
    out->height = static_cast<uint32_t>(px.height());
    out->rgba.assign(bits, bits + bytes);
    last_image = out;
    published++;
    FrameXr::SetHomePanelImage(std::move(out));
    account();
    if (!logged) {
        logged = true;
        LOG_INFO(Frontend, "xr: home panel: window {}x{} at {:.2f} pixels per pixel -> {}x{}",
                 canvas.width(), canvas.height(), s, px.width(), px.height());
    }
}

void FrameMirror::Queue(const FrameXr::HomePanelEvent& e) {
    std::scoped_lock lock{queue_mutex};
    if (e.type == FrameXr::HomePanelEvent::Type::Move && !queue.empty() &&
        queue.back().type == FrameXr::HomePanelEvent::Type::Move) {
        queue.back() = e; // only the newest hover position matters
    } else {
        queue.push_back(e);
    }
    if (!drain_queued) {
        drain_queued = true;
        QMetaObject::invokeMethod(this, [this] { Drain(); }, Qt::QueuedConnection);
    }
}

void FrameMirror::Drain() {
    std::vector<FrameXr::HomePanelEvent> events;
    {
        std::scoped_lock lock{queue_mutex};
        events.swap(queue);
        drain_queued = false;
    }
    for (const auto& e : events) {
        Deliver(e);
    }
}

void FrameMirror::Deliver(const FrameXr::HomePanelEvent& e) {
    using Type = FrameXr::HomePanelEvent::Type;
    if (!main) {
        return;
    }
    if (e.type == Type::Leave) {
        // TODO(AzaharFrame): Qt has no public way to send a leave; the last hover stays.
        return;
    }
    if (e.type == Type::Escape) {
        QWidget* target = QApplication::activePopupWidget();
        if (!target) {
            target = QApplication::activeModalWidget();
        }
        if (!target) {
            const auto stack = Stack();
            for (auto it = stack.rbegin(); it != stack.rend() && !target; ++it) {
                if (IsDialogLike(*it)) {
                    target = *it;
                }
            }
        }
        if (!target) {
            target = main;
        }
        if (QWindow* win = target->windowHandle()) {
            LOG_INFO(Frontend, "xr: home panel: Escape to {}",
                     target->metaObject()->className());
            const QString esc(QChar(0x1b));
            qt_handleKeyEvent(win, QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier, esc, false, 1);
            qt_handleKeyEvent(win, QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier, esc, false,
                              1);
        }
        MarkDirty();
        return;
    }

    const QRect canvas = Canvas();
    const QPointF global(canvas.x() + e.u * canvas.width(), canvas.y() + e.v * canvas.height());
    QWidget* target = QApplication::activePopupWidget();
    if (!target) {
        target = grab ? grab.data() : WindowAt(global.toPoint());
    }
    QWindow* win = target ? target->windowHandle() : nullptr;
    if (!win) {
        return;
    }
    const QPointF local = global - QPointF(win->geometry().topLeft());
    const Qt::MouseButton button =
        e.button == FrameXr::HomePanelEvent::Button::Right ? Qt::RightButton : Qt::LeftButton;
    const auto move = [&] {
        if (global != last_global) {
            qt_handleMouseEvent(win, local, global, buttons, Qt::NoButton, QEvent::MouseMove,
                                Qt::NoModifier, Timestamp());
            last_global = global;
        }
    };
    switch (e.type) {
    case Type::Move:
        move();
        break;
    case Type::Press:
        move();
        buttons |= button;
        grab = target;
        LOG_INFO(Frontend, "xr: home panel: {} press at {:.0f},{:.0f} in {}",
                 button == Qt::RightButton ? "right" : "left", global.x() - canvas.x(),
                 global.y() - canvas.y(), target->metaObject()->className());
        qt_handleMouseEvent(win, local, global, buttons, button, QEvent::MouseButtonPress,
                            Qt::NoModifier, Timestamp());
        break;
    case Type::Release:
        move();
        buttons &= ~button;
        if (!buttons) {
            grab = nullptr;
        }
        qt_handleMouseEvent(win, local, global, buttons, button, QEvent::MouseButtonRelease,
                            Qt::NoModifier, Timestamp());
        break;
    case Type::Wheel:
        move();
        qt_handleWheelEvent(win, local, global, QPoint(), QPoint(0, qRound(e.wheel * 120.0f)),
                            Qt::NoModifier, Qt::NoScrollPhase);
        break;
    default:
        break;
    }
    // A hover that changes something repaints (and so is captured); a plain move must not
    // cost a capture.
    if (e.type != Type::Move) {
        MarkDirty();
    }
}

// Test aid (AZAHARFRAME_MIRROR_LOG_LAYOUT=1): where the menu entries, game list rows and
// dialog buttons are on the panel (window pixels, and u,v), logged when they change,
// so scripted controller input (tools/xrmock) can aim at them.
void FrameMirror::LogLayout(const QRect& canvas) {
    std::vector<std::string> lines;
    const auto add = [&](const char* kind, const QString& text, const QRect& global_rect,
                         bool enabled) {
        const QRect r = global_rect.translated(-canvas.topLeft());
        const QPoint c = r.center();
        lines.push_back(fmt::format("xr layout: {} \"{}\"{} at {},{} {}x{} centre {},{} (u {:.4f} v {:.4f})",
                                    kind, text.toStdString(), enabled ? "" : " (disabled)",
                                    r.x(), r.y(), r.width(), r.height(), c.x(), c.y(),
                                    (c.x() + 0.5) / canvas.width(),
                                    (c.y() + 0.5) / canvas.height()));
    };
    if (auto* bar = main->findChild<QMenuBar*>(); bar && bar->isVisible()) {
        for (QAction* a : bar->actions()) {
            const QRect g = bar->actionGeometry(a);
            if (!g.isEmpty()) {
                add("menubar", a->text(), QRect(bar->mapToGlobal(g.topLeft()), g.size()),
                    a->isEnabled());
            }
        }
    }
    if (auto* tree = main->findChild<QTreeView*>(); tree && tree->isVisible() && tree->model()) {
        const auto* model = tree->model();
        for (int r = 0; r < model->rowCount(); r++) {
            const QModelIndex parent = model->index(r, 0);
            const auto row = [&](const QModelIndex& index) {
                const QRect v = tree->visualRect(index);
                if (!v.isEmpty()) {
                    add("game list", index.data().toString().simplified(),
                        QRect(tree->viewport()->mapToGlobal(v.topLeft()), v.size()), true);
                }
            };
            row(parent);
            for (int c = 0; c < model->rowCount(parent); c++) {
                row(model->index(c, 0, parent));
            }
        }
    }
    for (QWidget* w : Stack()) {
        if (auto* menu = qobject_cast<QMenu*>(w)) {
            for (QAction* a : menu->actions()) {
                const QRect g = menu->actionGeometry(a);
                if (!g.isEmpty() && !a->isSeparator()) {
                    add("menu item", a->text(), QRect(menu->mapToGlobal(g.topLeft()), g.size()),
                        a->isEnabled());
                }
            }
        } else if (IsDialogLike(w)) {
            add("dialog", w->windowTitle(), w->geometry(), true);
            for (QAbstractButton* b : w->findChildren<QAbstractButton*>()) {
                if (b->isVisible() && !b->text().isEmpty()) {
                    add("button", b->text(), QRect(b->mapToGlobal(QPoint()), b->size()),
                        b->isEnabled());
                }
            }
        }
    }
    std::string all;
    for (const auto& l : lines) {
        all += l;
        all += '\n';
    }
    if (all == last_layout) {
        return;
    }
    last_layout = all;
    LOG_INFO(Frontend, "xr layout: canvas {}x{} at {},{}", canvas.width(), canvas.height(),
             canvas.x(), canvas.y());
    for (const auto& l : lines) {
        LOG_INFO(Frontend, "{}", l);
    }
}
