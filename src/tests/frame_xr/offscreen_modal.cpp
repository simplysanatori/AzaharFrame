// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame stall: the main window's menu bar still opens after message boxes, a modal
// progress dialog and VR / flat round trips with windows kept off the screen (frame_offscreen.h).
// Runs on Qt's offscreen platform; the filter below is FrameMirror's VR rule, through the same
// functions.

#include <optional>

#include <QApplication>
#include <QInputDialog>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QShortcut>
#include <QTimer>
#include <QWindow>
#include <catch2/catch_test_macros.hpp>

#include "citra_qt/frame_offscreen.h"

void qt_handleMouseEvent(QWindow* window, const QPointF& local, const QPointF& global,
                         Qt::MouseButtons state, Qt::MouseButton button, QEvent::Type type,
                         Qt::KeyboardModifiers mods, int timestamp);
void qt_handleKeyEvent(QWindow* w, QEvent::Type t, int k, Qt::KeyboardModifiers mods,
                       const QString& text, bool autorep, ushort count);

namespace {

QApplication& App() {
    static int argc = 1;
    static char arg0[] = "frame_qt_tests";
    static char* argv[] = {arg0, nullptr};
    static QApplication* app = nullptr; // (kept to the end: Qt does not outlive static teardown)
    if (!app) { // (tests/frame_xr/open_folder.cpp may have made it)
        app = qobject_cast<QApplication*>(QApplication::instance());
    }
    if (!app) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        app = new QApplication(argc, argv);
    }
    return *app;
}

// FrameMirror's part: windows off the screen while in VR, the stale modal check always.
struct Mirror : QObject {
    QWidget* main = nullptr;
    bool vr = false;
    bool eventFilter(QObject* object, QEvent* event) override {
        auto* w = qobject_cast<QWidget*>(object);
        if (!w) {
            return false;
        }
        if (event->type() == QEvent::Hide && w != main && w->isWindow()) {
            FrameOffscreen::DropStaleModal(w);
        }
        if (event->type() == QEvent::ShowToParent && w->isWindow()) {
            FrameOffscreen::OpenPopup(w); // AzaharFrame popups
        }
        if (vr && (event->type() == QEvent::Polish || event->type() == QEvent::ShowToParent ||
                   event->type() == QEvent::WinIdChange)) {
            FrameOffscreen::Keep(w, main);
        }
        return false;
    }
    void SetVr(bool on) {
        vr = on;
        if (on) {
            for (QWidget* w : QApplication::topLevelWidgets()) {
                FrameOffscreen::Keep(w, main);
            }
        } else {
            FrameOffscreen::Release(main, this);
        }
    }
};

struct AppFirst {
    AppFirst() {
        App();
    }
};

struct Fixture {
    AppFirst app_first; // (the application before any widget)
    QMainWindow window;
    QMenu* file = nullptr;
    QListWidget* list = nullptr;
    int double_clicks = 0;
    Mirror mirror;

    Fixture() {
        file = window.menuBar()->addMenu(QStringLiteral("File"));
        file->addAction(QStringLiteral("Install"));
        list = new QListWidget;
        list->addItem(QStringLiteral("Emerald 3DS"));
        QObject::connect(list, &QListWidget::itemDoubleClicked, [this] { double_clicks++; });
        window.setCentralWidget(list);
        window.resize(916, 610);
        window.show();
        mirror.main = &window;
        qApp->installEventFilter(&mirror);
    }
    ~Fixture() {
        qApp->removeEventFilter(&mirror);
    }

    void Click(QPoint local, int times) {
        QWindow* win = window.windowHandle();
        const QPointF global = window.mapToGlobal(QPointF(local));
        for (int i = 0; i < times; i++) {
            qt_handleMouseEvent(win, local, global, Qt::LeftButton, Qt::LeftButton,
                                QEvent::MouseButtonPress, Qt::NoModifier, 1000 + i * 40);
            qt_handleMouseEvent(win, local, global, Qt::NoButton, Qt::LeftButton,
                                QEvent::MouseButtonRelease, Qt::NoModifier, 1010 + i * 40);
        }
    }
    bool MenuOpens() {
        const QRect r = window.menuBar()->actionGeometry(file->menuAction());
        Click(window.menuBar()->mapTo(&window, r.center()), 1);
        const bool open = file->isVisible();
        file->hide();
        return open;
    }
    bool ListTakesDoubleClick() {
        const int before = double_clicks;
        Click(list->mapTo(&window, list->visualItemRect(list->item(0)).center()), 2);
        return double_clicks == before + 1;
    }
    void Settle() {
        for (int i = 0; i < 3; i++) {
            QApplication::processEvents();
        }
    }
};

// Closes the next modal window once the event loop runs it (the beam's click on OK).
void AcceptNextModal() {
    QTimer::singleShot(0, [] {
        if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            d->accept();
        }
    });
}

} // namespace

TEST_CASE("FrameOffscreen: a message box in VR leaves the menu bar usable", "[frame_xr]") {
    Fixture f;
    f.mirror.SetVr(true);
    REQUIRE(f.MenuOpens());
    AcceptNextModal();
    QMessageBox::information(&f.window, QStringLiteral("Pokémon Emerald 3DS"),
                             QStringLiteral("Installed"));
    f.Settle();
    CHECK(QApplication::activeModalWidget() == nullptr);
    CHECK(f.MenuOpens());
    CHECK(f.ListTakesDoubleClick());
    f.mirror.SetVr(false);
}

TEST_CASE("FrameOffscreen: the Homebrew flow, then VR / flat round trips", "[frame_xr]") {
    Fixture f;
    f.mirror.SetVr(true);
    // The install's dialogs as Emerald 3DS shows them: a choice, a window-modal progress dialog
    // closed by the code, a persistent message box, a static one.
    AcceptNextModal();
    QInputDialog::getItem(&f.window, QStringLiteral("Language"), QStringLiteral("Language"),
                          {QStringLiteral("English")}, 0, false);
    auto* progress = new QProgressDialog(QStringLiteral("Downloading"), QStringLiteral("Cancel"),
                                         0, 1000, &f.window);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setValue(1);
    f.Settle();
    progress->hide();
    progress->deleteLater();
    QMessageBox box(QMessageBox::Information, QStringLiteral("Pokémon Emerald 3DS"),
                    QStringLiteral("Installed"), QMessageBox::Ok, &f.window);
    AcceptNextModal();
    box.exec();
    AcceptNextModal();
    QMessageBox::warning(&f.window, QStringLiteral("Pokémon Emerald 3DS"),
                         QStringLiteral("Cancelled"));
    f.Settle();
    CHECK_FALSE(FrameOffscreen::OnModalList(&box));
    CHECK(QApplication::activeModalWidget() == nullptr);
    CHECK(f.MenuOpens());

    for (int i = 0; i < 6; i++) {
        // A message box open when the mode changes is closed in flat.
        QPointer<QMessageBox> open = new QMessageBox(
            QMessageBox::Question, QStringLiteral("Stop"), QStringLiteral("Stop the game?"),
            QMessageBox::Yes | QMessageBox::No, &f.window);
        open->setAttribute(Qt::WA_DeleteOnClose);
        open->setWindowModality(Qt::ApplicationModal);
        open->show();
        f.Settle();
        f.mirror.SetVr(false);
        f.Settle();
        CHECK((!open || !open->isVisible()));
        CHECK(QApplication::activeModalWidget() == nullptr);
        CHECK(f.MenuOpens());
        f.mirror.SetVr(true);
        f.Settle();
        AcceptNextModal();
        QMessageBox::information(&f.window, QStringLiteral("Round Trip"), QString::number(i));
        f.Settle();
        CHECK(QApplication::activeModalWidget() == nullptr);
        CHECK(f.MenuOpens());
        CHECK(f.ListTakesDoubleClick());
    }
    f.mirror.SetVr(false);
    f.Settle();
}

// AzaharFrame popups: menus kept off the screen in VR are Qt's open popups, so the beam's click
// outside or B (Escape) closes them, a click on an item runs it, a second right click does not
// stack a second menu, and none is left over once the panel hides (CloseAllPopups).
namespace {

int clock_ms = 5000;
QPointer<QWindow> pointer_in;

// The mirror's way: every mouse event goes to the open popup's window, else the main window.
void Mouse(Fixture& f, QPoint global, QEvent::Type type, Qt::MouseButton button) {
    QWidget* target = QApplication::activePopupWidget();
    if (!target) {
        target = &f.window;
    }
    const Qt::MouseButtons state =
        type == QEvent::MouseButtonPress ? Qt::MouseButtons(button) : Qt::MouseButtons();
    const QPointF local(global - target->geometry().topLeft());
    FrameOffscreen::PointerIn(pointer_in, target->windowHandle(), local, QPointF(global));
    qt_handleMouseEvent(target->windowHandle(), local, QPointF(global), state,
                        type == QEvent::MouseMove ? Qt::NoButton : button, type, Qt::NoModifier,
                        clock_ms += 300);
}

void ClickAt(Fixture& f, QPoint global, Qt::MouseButton button = Qt::LeftButton) {
    Mouse(f, global, QEvent::MouseMove, Qt::NoButton);
    Mouse(f, global, QEvent::MouseButtonPress, button);
    Mouse(f, global, QEvent::MouseButtonRelease, button);
    f.Settle();
}

void Escape() {
    QWidget* target = QApplication::activePopupWidget();
    REQUIRE(target);
    const QString esc(QChar(0x1b));
    qt_handleKeyEvent(target->windowHandle(), QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier,
                      esc, false, 1);
    qt_handleKeyEvent(target->windowHandle(), QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier,
                      esc, false, 1);
}

int VisiblePopups() {
    int n = 0;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        n += w->windowType() == Qt::Popup && w->isVisible();
    }
    return n;
}

} // namespace

TEST_CASE("FrameOffscreen: popups off the screen are Qt's open popups", "[frame_xr]") {
    Fixture f;
    f.mirror.SetVr(true);
    const QPoint list_at = f.list->viewport()->mapToGlobal(QPoint(40, 10));
    const QPoint away = f.window.mapToGlobal(QPoint(800, 500));

    QMenu context;
    int favorites = 0;
    QObject::connect(context.addAction(QStringLiteral("Favorite")), &QAction::triggered,
                     [&] { favorites++; });
    context.addAction(QStringLiteral("Properties"));

    // A right click's menu: open for Qt, off the screen.
    context.popup(list_at);
    f.Settle();
    REQUIRE(context.isVisible());
    CHECK(context.testAttribute(Qt::WA_DontShowOnScreen));
    CHECK(QApplication::activePopupWidget() == &context);
    CHECK(FrameOffscreen::OnPopupList(&context));

    // A click away closes it (and reaches nothing else).
    ClickAt(f, away);
    CHECK_FALSE(context.isVisible());
    CHECK(QApplication::activePopupWidget() == nullptr);
    CHECK_FALSE(FrameOffscreen::OnPopupList(&context));

    // B (Escape) closes it.
    context.popup(list_at);
    f.Settle();
    REQUIRE(QApplication::activePopupWidget() == &context);
    Escape();
    f.Settle();
    CHECK_FALSE(context.isVisible());
    CHECK(QApplication::activePopupWidget() == nullptr);

    // A click on an item runs it and closes the menu.
    context.popup(list_at);
    f.Settle();
    const QRect item = context.actionGeometry(context.actions().at(0));
    const QPoint on_item = context.mapToGlobal(item.center() + QPoint(30, 0));
    Mouse(f, on_item + QPoint(-20, 0), QEvent::MouseMove, Qt::NoButton);
    ClickAt(f, on_item);
    CHECK(favorites == 1);
    CHECK_FALSE(context.isVisible());
    CHECK(QApplication::activePopupWidget() == nullptr);

    // The list's own right click menu (as the game list's): it opens again after Escape closed
    // it (Qt took the closed menu for the widget under the pointer without the beam's Enter).
    int menus = 0;
    f.list->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(f.list, &QWidget::customContextMenuRequested, [&](const QPoint& at) {
        menus++;
        context.popup(f.list->viewport()->mapToGlobal(at));
    });
    ClickAt(f, list_at, Qt::RightButton);
    REQUIRE(menus == 1);
    CHECK(QApplication::activePopupWidget() == &context);
    Escape();
    f.Settle();
    CHECK(QApplication::activePopupWidget() == nullptr);
    ClickAt(f, list_at, Qt::RightButton);
    CHECK(menus == 2);
    CHECK(QApplication::activePopupWidget() == &context);
    Escape();
    f.Settle();
    f.list->setContextMenuPolicy(Qt::DefaultContextMenu);

    // A second right click while one is open closes it first: never two menus at once.
    context.popup(list_at);
    f.Settle();
    ClickAt(f, away, Qt::RightButton);
    CHECK(VisiblePopups() == 0);

    // The menu bar's menu: Qt's popup, closed by a click away, and the bar still opens it.
    CHECK(f.MenuOpens());
    const QRect file = f.window.menuBar()->actionGeometry(f.file->menuAction());
    ClickAt(f, f.window.menuBar()->mapToGlobal(file.center()));
    REQUIRE(f.file->isVisible());
    CHECK(QApplication::activePopupWidget() == f.file);
    ClickAt(f, away);
    CHECK_FALSE(f.file->isVisible());

    // Left open when the panel hides (a game starts): closed, none left over, list usable.
    context.popup(list_at);
    QMenu sub;
    sub.addAction(QStringLiteral("Open"));
    sub.popup(list_at + QPoint(100, 0));
    f.Settle();
    CHECK(VisiblePopups() == 2);
    CHECK(FrameOffscreen::CloseAllPopups() >= 2);
    f.Settle();
    CHECK(VisiblePopups() == 0);
    CHECK(QApplication::activePopupWidget() == nullptr);
    CHECK(f.MenuOpens());
    CHECK(f.ListTakesDoubleClick());
    f.mirror.SetVr(false);
    f.Settle();
}

// AzaharFrame popups: B (Escape) closes a window that is not modal although an application-wide
// shortcut on Esc (Azahar's Exit Fullscreen) would take the key, and the shortcut works again after.
TEST_CASE("FrameOffscreen: Escape reaches a non-modal window past an Esc hotkey", "[frame_xr]") {
    Fixture f;
    f.mirror.SetVr(true);
    int exits = 0;
    auto* exit_fullscreen = new QShortcut(QKeySequence(Qt::Key_Escape), &f.window, nullptr,
                                          nullptr, Qt::ApplicationShortcut);
    QObject::connect(exit_fullscreen, &QShortcut::activated, [&] { exits++; });

    QDialog room(&f.window, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    room.setWindowTitle(QStringLiteral("Create Room"));
    room.resize(400, 200);
    const auto escape = [&](bool paused) {
        std::optional<FrameOffscreen::EscapeShortcutsPaused> pause;
        if (paused) {
            pause.emplace(&f.window);
        }
        const QString esc(QChar(0x1b));
        qt_handleKeyEvent(room.windowHandle(), QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier,
                          esc, false, 1);
        qt_handleKeyEvent(room.windowHandle(), QEvent::KeyRelease, Qt::Key_Escape,
                          Qt::NoModifier, esc, false, 1);
    };

    f.window.activateWindow(); // (Qt matches shortcuts only with an active window)
    f.Settle();
    room.show();
    f.Settle();
    REQUIRE(room.isVisible());
    REQUIRE(QApplication::activeWindow() == &f.window);
    CHECK(room.testAttribute(Qt::WA_DontShowOnScreen));
    escape(false); // the way B went before: the hotkey takes it, the window stays
    f.Settle();
    CHECK(exits == 1);
    CHECK(room.isVisible());

    escape(true);
    f.Settle();
    CHECK(exits == 1);
    CHECK_FALSE(room.isVisible());
    CHECK(exit_fullscreen->isEnabled());
    f.mirror.SetVr(false);
    f.Settle();
}
