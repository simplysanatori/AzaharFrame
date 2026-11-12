// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <memory>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include "citra_qt/frame_preload_confirm.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "video_core/custom_textures/custom_tex_manager.h"

namespace PB = VideoCore::PreloadBudget;

PB::ConfirmText FramePreloadConfirmText() {
    std::optional<u64> need;
    Core::System& system = Core::System::GetInstance();
    if (system.IsPoweredOn() && Settings::values.custom_textures.GetValue()) {
        if (const u64 bytes = system.CustomTexManager().PreloadEstimate(); bytes > 0) {
            need = bytes;
        }
    }
    const PB::Room room = PB::ReadRoom();
    const PB::Summary s = PB::Summarize(need, room, PB::ReserveFromEnv());
    PB::ConfirmText t = PB::MakeConfirmText(s);
    std::string line;
    for (const auto& [label, value] : t.rows) {
        line += (line.empty() ? "" : ", ") + label + " " + value;
    }
    LOG_INFO(Frontend, "custom textures: Preload Textures? asked ({}{}{})", line,
             t.status_text.empty() ? "" : "; ", t.status_text);
    return t;
}

QDialog* FrameMakePreloadDialog(QWidget* parent, const PB::ConfirmText& t) {
    // A Material-style dialog in Qt widgets: the headline, the supporting text, the numbers
    // between two lines, the status row, then Cancel and Turn On at the end.
    auto* d = new QDialog(parent);
    QDialog& dialog = *d;
    dialog.setWindowTitle(QString::fromStdString(t.title));
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 16);
    layout->setSpacing(12);
    auto* headline = new QLabel(QString::fromStdString(t.title));
    QFont hf = headline->font();
    hf.setPointSizeF(hf.pointSizeF() * 1.5);
    headline->setFont(hf);
    layout->addWidget(headline);
    auto* body = new QLabel(QString::fromStdString(t.body));
    body->setWordWrap(true);
    layout->addWidget(body);
    const auto line = [&] {
        auto* f = new QFrame;
        f->setFrameShape(QFrame::HLine);
        f->setFrameShadow(QFrame::Plain);
        f->setStyleSheet(QStringLiteral("color: palette(mid);"));
        layout->addWidget(f);
    };
    line();
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(10);
    int row = 0;
    for (const auto& [label, value] : t.rows) {
        auto* l = new QLabel(QString::fromStdString(label));
        auto* v = new QLabel(QString::fromStdString(value));
        QFont vf = v->font();
        vf.setBold(true);
        v->setFont(vf);
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(l, row, 0);
        grid->addWidget(v, row, 1);
        row++;
    }
    layout->addLayout(grid);
    line();
    if (t.status != PB::Status::None) {
        auto* status = new QHBoxLayout;
        auto* icon = new QLabel;
        const bool fits = t.status == PB::Status::Fits;
        icon->setPixmap(
            dialog.style()
                ->standardIcon(fits ? QStyle::SP_DialogApplyButton : QStyle::SP_MessageBoxWarning)
                .pixmap(20, 20));
        auto* text = new QLabel(QString::fromStdString(t.status_text));
        QFont sf = text->font();
        sf.setBold(true);
        text->setFont(sf);
        status->addWidget(icon);
        status->addWidget(text, 1);
        layout->addLayout(status);
    }
    // Text buttons at the end, the confirming one last (Material), with no icons.
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* cancel = new QPushButton(QObject::tr("Cancel"));
    auto* on = new QPushButton(QString::fromStdString(t.confirm));
    for (QPushButton* b : {cancel, on}) {
        b->setFlat(true);
        b->setAutoDefault(false);
        buttons->addWidget(b);
    }
    cancel->setDefault(true);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(on, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addLayout(buttons);
    dialog.setMinimumWidth(460);
    return d;
}

bool FrameAskPreload(QWidget* parent) {
    const PB::ConfirmText t = FramePreloadConfirmText();
    std::unique_ptr<QDialog> dialog{FrameMakePreloadDialog(parent, t)};
    const bool yes = dialog->exec() == QDialog::Accepted;
    LOG_INFO(Frontend, "custom textures: Preload Custom Textures: {}", yes ? "Turn On" : "Cancel");
    return yes;
}
