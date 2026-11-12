// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame stall: the main window's menu bar still opens after message boxes, a modal
// progress dialog and VR / flat round trips with windows kept off the screen (frame_offscreen.h).
// Runs on Qt's offscreen platform; the filter below is FrameMirror's VR rule, through the same
// functions.

#include <QApplication>
#include <QInputDialog>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QTimer>
#include <QWindow>
#include <catch2/catch_test_macros.hpp>

#include "citra_qt/frame_offscreen.h"

void qt_handleMouseEvent(QWindow* window, const QPointF& local, const QPointF& global,
                         Qt::MouseButtons state, Qt::MouseButton button, QEvent::Type type,
                         Qt::KeyboardModifiers mods, int timestamp);

namespace {

QApplication& App() {
    static int argc = 1;
    static char arg0[] = "frame_qt_tests";
    static char* argv[] = {arg0, nullptr};
    static QApplication* app = nullptr; // (kept to the end: Qt does not outlive static teardown)
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
