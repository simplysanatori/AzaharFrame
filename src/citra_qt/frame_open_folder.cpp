// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame qtfiles: "open a folder" in Qt's file dialog in VR and Game Mode
// (frame_open_folder.h).

#include "citra_qt/frame_open_folder.h"

#include <QAction>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QShortcut>
#include <QTreeView>
#include <QUrl>

// (declared here, not through frame_xr/frame_xr.h, so frame_qt_tests can build this file)
namespace FrameXr {
bool IsStarted();
bool VrShowing();
} // namespace FrameXr

namespace FrameOpenFolder {

namespace {

// Qt's own Delete deletes unless its question returns No, so a question hidden without an answer
// (Azahar closing while it shows) deleted. Here only a Yes deletes. Qt's words, so its
// translations.
void DeleteSelected(QFileDialog* dialog) {
    auto* view = dialog->findChild<QTreeView*>(QStringLiteral("treeView"));
    auto* model = dialog->findChild<QFileSystemModel*>();
    if (!view || !model || !view->selectionModel()) {
        return;
    }
    const QModelIndexList rows = view->selectionModel()->selectedRows();
    if (rows.size() != 1 || rows.first().model() != model) {
        return;
    }
    const QPersistentModelIndex index(rows.first());
    const QString name = index.data(QFileSystemModel::FileNameRole).toString();
    const QPointer<QFileDialog> alive(dialog);
    const auto answer =
        QMessageBox::warning(dialog, QFileDialog::tr("Delete"),
                             QFileDialog::tr("Are you sure you want to delete '%1'?").arg(name),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes || !alive || !index.isValid()) {
        return;
    }
    if (!model->remove(index)) {
        QMessageBox::warning(
            dialog, dialog->windowTitle(),
            QCoreApplication::translate("FrameOpenFolder", "Could not delete '%1'.").arg(name));
    }
}

} // namespace

bool UseDialog() {
    const QString pick = qEnvironmentVariable("AZAHARFRAME_FILE_BROWSER").trimmed().toLower();
    if (pick == QStringLiteral("app") || pick == QStringLiteral("system")) {
        return pick == QStringLiteral("app");
    }
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP") == QStringLiteral("gamescope") ||
           (FrameXr::IsStarted() && FrameXr::VrShowing());
}

void Open(QWidget* parent, const QString& path, const QString& label) {
    if (!UseDialog()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        return;
    }
    // Qt's own dialog (not the desktop's, which would not show), in Directory mode without
    // ShowDirsOnly: files are listed too, and choosing one does nothing. Not read-only, so its
    // right-click menu has Rename, Delete and New Folder.
    auto* dialog = new QFileDialog(parent);
    dialog->setOption(QFileDialog::DontUseNativeDialog);
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setViewMode(QFileDialog::Detail);
    dialog->setDirectory(path);
    dialog->setWindowTitle(QCoreApplication::translate("FrameOpenFolder", "%1 Folder").arg(label));
    // One button, Close; no name or file type rows (nothing is chosen here).
    dialog->setLabelText(QFileDialog::Reject,
                         QCoreApplication::translate("FrameOpenFolder", "Close"));
    if (auto* buttons = dialog->findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"))) {
        if (QPushButton* accept = buttons->button(QDialogButtonBox::Open)) {
            accept->hide();
        }
    }
    for (const char* name : {"fileNameLabel", "fileNameEdit", "fileTypeLabel", "fileTypeCombo"}) {
        if (auto* row = dialog->findChild<QWidget*>(QLatin1String(name))) {
            row->hide();
        }
    }
    // Delete (the menu's and the Delete key) asks with DeleteSelected instead of Qt's code.
    for (auto* action : dialog->findChildren<QAction*>(QStringLiteral("qt_delete_action"))) {
        QObject::disconnect(action, &QAction::triggered, dialog, nullptr);
        QObject::connect(action, &QAction::triggered, dialog, [dialog] { DeleteSelected(dialog); });
    }
    for (auto* shortcut : dialog->findChildren<QShortcut*>()) {
        if (shortcut->key() == QKeySequence(QKeySequence::Delete)) {
            QObject::disconnect(shortcut, &QShortcut::activated, dialog, nullptr);
            QObject::connect(shortcut, &QShortcut::activated, dialog,
                             [dialog] { DeleteSelected(dialog); });
        }
    }
    // Nearly the window's size, the Look In path as wide as it can be, the Size, Type and Date
    // columns as wide as what they show.
    if (parent) {
        dialog->resize(parent->window()->size() * 0.9);
    }
    if (auto* look_in = dialog->findChild<QWidget*>(QStringLiteral("lookInCombo"))) {
        look_in->setSizePolicy(QSizePolicy::Expanding, look_in->sizePolicy().verticalPolicy());
    }
    if (auto* view = dialog->findChild<QTreeView*>(QStringLiteral("treeView"))) {
        view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int column = 1; column < view->header()->count(); ++column) {
            view->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
        }
    }
    QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    dialog->setModal(false);
    dialog->show();
}

} // namespace FrameOpenFolder
