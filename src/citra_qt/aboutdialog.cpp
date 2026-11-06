// Copyright 2017-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <QIcon>
#include <QPixmap>
#include "aboutdialog.h"
#include "citra_qt/frame_branding.h"
#include "common/scm_rev.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(std::make_unique<Ui::AboutDialog>()) {
    ui->setupUi(this);
    ui->labelBuildInfo->setText(ui->labelBuildInfo->text().arg(
        QString::fromUtf8(Common::g_build_fullname), QString::fromUtf8(Common::g_scm_branch),
        QString::fromUtf8(Common::g_scm_desc), QString::fromUtf8(Common::g_build_date).left(10),
        QString::fromUtf8(Common::g_build_variant)));
#ifdef ENABLE_FRAME_XR // AzaharFrame: the Azahar for Frame icon instead of Azahar's
    const QString icon_path = FrameBranding::ArtFile(QStringLiteral("icon.png"));
    if (!icon_path.isEmpty()) {
        const QPixmap icon(icon_path);
        ui->labelLogo->setPixmap(icon.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
#endif
}

AboutDialog::~AboutDialog() = default;
