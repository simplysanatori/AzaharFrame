// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame qtfiles: "open a folder" in Qt's file dialog in VR and Game Mode
// (frame_open_folder.h).

#include "citra_qt/frame_open_folder.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QPushButton>
#include <QUrl>

#include "frame_xr/frame_xr.h"

namespace FrameOpenFolder {

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
    QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    dialog->setModal(false);
    dialog->show();
}

} // namespace FrameOpenFolder
