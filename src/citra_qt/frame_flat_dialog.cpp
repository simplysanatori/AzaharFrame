// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flatdialog: see frame_flat_dialog.h.

#include <algorithm>

#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QLayout>
#include <QMessageBox>
#include <QProgressDialog>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include "citra_qt/frame_flat_dialog.h"
#include "common/logging/log.h"
#include "frame_xr/flat_fit.h"

namespace FrameFlatDialog {

namespace {

constexpr char kScrolled[] = "azaharframe_flat_scroll";

FrameXr::WindowSize ToSize(const QSize& s) {
    return {s.width(), s.height()};
}

QSize MinimumOf(const QWidget* window) {
    return window->minimumSize().expandedTo(window->minimumSizeHint());
}

} // namespace

bool AddScrollArea(QDialog* dialog) {
    if (!dialog || dialog->property(kScrolled).toBool()) {
        return false;
    }
    // Qt's own dialogs lay themselves out again (and a message box fixes its size).
    if (qobject_cast<QMessageBox*>(dialog) || qobject_cast<QFileDialog*>(dialog) ||
        qobject_cast<QInputDialog*>(dialog) || qobject_cast<QProgressDialog*>(dialog) ||
        qobject_cast<QColorDialog*>(dialog) || qobject_cast<QFontDialog*>(dialog) ||
        dialog->inherits("QWizard")) {
        return false;
    }
    QLayout* const top = dialog->layout();
    if (!top || (top->sizeConstraint() != QLayout::SetDefaultConstraint &&
                 top->sizeConstraint() != QLayout::SetMinimumSize)) {
        return false;
    }
    QDialogButtonBox* buttons = nullptr;
    for (int i = top->count() - 1; i >= 0 && !buttons; --i) {
        buttons = qobject_cast<QDialogButtonBox*>(top->itemAt(i)->widget());
    }
    const QMargins margins = top->contentsMargins();
    if (buttons) {
        top->removeWidget(buttons); // (still the dialog's child, under the scroll area below)
    }
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("azaharframe_flat_content"));
    content->setLayout(top); // Qt takes the layout, with its widgets, from the dialog
    auto* scroll = new QScrollArea(dialog);
    scroll->setObjectName(QStringLiteral("azaharframe_flat_scroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);
    dialog->setMinimumSize(0, 0); // the old layout's minimum, kept on the dialog
    auto* outer = new QVBoxLayout(dialog);
    outer->setContentsMargins(0, 0, 0, buttons ? margins.bottom() : 0);
    outer->setSpacing(0);
    outer->addWidget(scroll, 1);
    if (buttons) {
        buttons->setContentsMargins(margins.left(), std::max(0, top->spacing()), margins.right(),
                                    0);
        outer->addWidget(buttons);
    }
    scroll->show();
    outer->activate();
    dialog->setProperty(kScrolled, true);
    return true;
}

void Fit(QWidget* window, const QWidget* main, bool first) {
    if (!window || !main) {
        return;
    }
    QSize room = main->size();
    if (const QScreen* screen = main->screen()) {
        room = room.boundedTo(screen->availableGeometry().size());
    }
    const QSize asked = window->size();
    const QSize asked_min = MinimumOf(window);
    FrameXr::FlatDialogFit fit = FrameXr::FitFlatDialog(
        ToSize(room), ToSize(asked), ToSize(asked_min), ToSize(window->maximumSize()));
    bool scrolled = false;
    if (fit.too_small && first) {
        scrolled = AddScrollArea(qobject_cast<QDialog*>(window));
        if (scrolled) {
            fit = FrameXr::FitFlatDialog(ToSize(room), ToSize(asked), ToSize(MinimumOf(window)),
                                         ToSize(window->maximumSize()));
        }
    }
    const QSize size(fit.size.width, fit.size.height);
    if (size != window->size()) {
        window->resize(size);
    }
    QRect target(QPoint(), window->size());
    target.moveCenter(main->geometry().center());
    if (window->geometry().topLeft() != target.topLeft()) {
        window->move(target.topLeft() + (window->pos() - window->geometry().topLeft()));
    }
    if (first) {
        LOG_INFO(Frontend, "xr: flat: {} {}x{} (minimum {}x{}) shown at {}x{}{}",
                 window->metaObject()->className(), asked.width(), asked.height(),
                 asked_min.width(), asked_min.height(), window->width(), window->height(),
                 scrolled         ? ", its content in a scroll area"
                 : fit.too_small ? ", LARGER than the area"
                                 : "");
    }
}

} // namespace FrameFlatDialog
