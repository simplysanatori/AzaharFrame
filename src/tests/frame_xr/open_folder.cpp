// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame qtfiles: the "open a folder" dialog's Delete deletes only on an explicit Yes
// (citra_qt/frame_open_folder.h), on Qt's offscreen platform.

#include <functional>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeView>
#include <catch2/catch_test_macros.hpp>

#include "citra_qt/frame_open_folder.h"

// frame_open_folder.cpp asks whether VR shows; here it never does (AZAHARFRAME_FILE_BROWSER=app).
namespace FrameXr {
bool IsStarted() {
    return false;
}
bool VrShowing() {
    return false;
}
} // namespace FrameXr

namespace {

QApplication& TheApp() {
    static int argc = 1;
    static char arg0[] = "frame_qt_tests";
    static char* argv[] = {arg0, nullptr};
    if (auto* app = qobject_cast<QApplication*>(QApplication::instance())) {
        return *app;
    }
    qputenv("QT_QPA_PLATFORM", "offscreen");
    return *new QApplication(argc, argv); // (kept to the end, as in offscreen_modal.cpp)
}

void Wait(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
    }
}

void MakeFolder(const QString& root, const QString& name) {
    REQUIRE(QDir(root).mkpath(name + QStringLiteral("/sub")));
    QFile file(root + QLatin1Char('/') + name + QStringLiteral("/sub/a.txt"));
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write("x");
}

bool Select(QTreeView* view, const QString& name) {
    for (int i = 0; i < 100; ++i) { // the model lists the folder in the background
        const QModelIndex root = view->rootIndex();
        for (int row = 0; row < view->model()->rowCount(root); ++row) {
            const QModelIndex index = view->model()->index(row, 0, root);
            if (index.data().toString() == name) {
                view->setCurrentIndex(index);
                view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect |
                                                          QItemSelectionModel::Rows);
                return true;
            }
        }
        Wait(20);
    }
    return false;
}

// Delete on `name` (the menu's action, or the Delete key), the question answered by `answer`.
bool DeleteAndAnswer(QFileDialog* dialog, const QString& name,
                     std::function<void(QMessageBox*)> answer, bool key = false) {
    auto* view = dialog->findChild<QTreeView*>(QStringLiteral("treeView"));
    REQUIRE(Select(view, name));
    bool asked = false;
    QTimer::singleShot(100, dialog, [&] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            asked = true;
            answer(box);
        }
    });
    if (key) {
        for (auto* shortcut : view->findChildren<QShortcut*>()) {
            if (shortcut->key() == QKeySequence(QKeySequence::Delete)) {
                emit shortcut->activated();
            }
        }
    } else {
        auto* action = dialog->findChild<QAction*>(QStringLiteral("qt_delete_action"));
        REQUIRE(action);
        action->setEnabled(true); // (as the right-click menu does)
        action->trigger();
    }
    Wait(100);
    return asked;
}

} // namespace

TEST_CASE("Open folder dialog: Delete only on Yes", "[frame_xr][qtfiles]") {
    TheApp();
    qputenv("AZAHARFRAME_FILE_BROWSER", "app");
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString root = tmp.path();
    for (const char* name : {"hidden", "closed", "no", "key", "yes"}) {
        MakeFolder(root, QString::fromLatin1(name));
    }
    QWidget main;
    main.resize(916, 610);
    main.show();
    FrameOpenFolder::Open(&main, root, QStringLiteral("Mods"));
    QFileDialog* dialog = main.findChild<QFileDialog*>();
    REQUIRE(dialog);
    CHECK(dialog->isVisible());
    CHECK_FALSE(dialog->isModal());
    CHECK(dialog->windowTitle() == QStringLiteral("Mods Folder"));
    CHECK(dialog->width() > 800);
    const auto exists = [&](const char* name) {
        return QDir(root + QLatin1Char('/') + QLatin1String(name)).exists();
    };

    // A question hidden without an answer (Azahar closing while it shows): kept. Qt's own Delete
    // deleted here.
    CHECK(DeleteAndAnswer(dialog, QStringLiteral("hidden"), [](QMessageBox* box) { box->hide(); }));
    CHECK(exists("hidden"));
    CHECK(
        DeleteAndAnswer(dialog, QStringLiteral("closed"), [](QMessageBox* box) { box->close(); }));
    CHECK(exists("closed"));
    CHECK(DeleteAndAnswer(dialog, QStringLiteral("no"),
                          [](QMessageBox* box) { box->button(QMessageBox::No)->click(); }));
    CHECK(exists("no"));
    CHECK(DeleteAndAnswer(
        dialog, QStringLiteral("key"), [](QMessageBox* box) { box->hide(); }, true));
    CHECK(exists("key"));
    // Yes deletes, a folder with files in it too.
    CHECK(DeleteAndAnswer(dialog, QStringLiteral("yes"),
                          [](QMessageBox* box) { box->button(QMessageBox::Yes)->click(); }));
    CHECK_FALSE(exists("yes"));

    dialog->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(main.findChild<QFileDialog*>() == nullptr); // deleted when closed
}
