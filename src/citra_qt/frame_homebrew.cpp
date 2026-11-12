// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: see frame_homebrew.h.

#include "citra_qt/frame_homebrew.h"

#include <QAction>
#include <QDateTime>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include "citra_qt/frame_emerald.h"
#include "frame_xr/frame_xr.h"
#include "frame_xr/menu/homebrew_tab.h"

namespace {
FrameHomebrew* g_instance = nullptr;
std::function<void()> g_refresh_list;
} // namespace

FrameHomebrew::FrameHomebrew(QWidget* window_, QMenuBar* bar, QMenu* before,
                             std::function<void()> refresh_list)
    : QObject(window_), window(window_) {
    g_instance = this;
    g_refresh_list = std::move(refresh_list);
    menu = new QMenu(tr("Homebrew"), bar);
    bar->insertMenu(before->menuAction(), menu);

    for (const Homebrew::PackageInfo& info : Homebrew::Packages()) {
        HomebrewPackager* p = nullptr;
        if (info.id == "emerald3ds") {
            p = new FrameEmerald(info, this);
        }
        if (p == nullptr) {
            continue;
        }
        packagers.push_back(p);
        QAction* action = menu->addAction(QString{});
        actions.push_back(action);
        connect(action, &QAction::triggered, this, [this, p] {
            if (p->GetStatus().update) {
                p->Update(window);
            } else {
                p->Install(window);
            }
        });
        connect(p, &HomebrewPackager::Changed, this, &FrameHomebrew::Refresh);
    }
    // AzaharFrame ui3: Check for Updates, the result in a message box (as Install's)
    menu->addSeparator();
    check_action = menu->addAction(tr("Check for Updates"));
    connect(check_action, &QAction::triggered, this, [this] { CheckNow(QString{}, false); });
    // "Checked 5 Minutes Ago" stays true on the VR row (the model only goes out when it changes)
    auto* minute = new QTimer(this);
    connect(minute, &QTimer::timeout, this, &FrameHomebrew::Refresh);
    minute->start(60 * 1000);
    Refresh();
    QTimer::singleShot(3000, this, &FrameHomebrew::StartAutomatic);
}

void FrameHomebrew::StartAutomatic() {
    for (HomebrewPackager* p : packagers) {
        if (p->Automatic() && !p->GetStatus().busy) {
            p->Install(window);
        }
    }
}

std::function<void()> FrameHomebrew::RefreshList() {
    return g_refresh_list ? g_refresh_list : [] {};
}

void FrameHomebrew::Refresh() {
    FrameXr::Menu::HomebrewTabModel model;
    const s64 now = QDateTime::currentSecsSinceEpoch();
    bool can_check = false;
    for (std::size_t i = 0; i < packagers.size(); i++) {
        const HomebrewPackager::Status s = packagers[i]->GetStatus();
        const Homebrew::PackageInfo& info = packagers[i]->Info();
        const std::string title(info.title);
        // "Install Pokémon Emerald 3DS", "Update ...", "Reinstall ..."
        const char* verb = !s.installed ? "Install " : s.update ? "Update " : "Reinstall ";
        actions[i]->setText(QString::fromStdString(verb + title));
        actions[i]->setEnabled(!s.busy);

        FrameXr::Menu::HomebrewItem item;
        item.id = std::string(info.id);
        item.title = title;
        item.author = std::string(info.author);
        if (!s.detail.isEmpty()) {
            item.author += " · " + s.detail.toStdString(); // "ZallaxDev · English"
        }
        item.installed = s.installed;
        item.update = s.update;
        item.busy = s.busy;
        item.progress = s.progress;
        item.status = s.status.toStdString();
        item.checking = s.checking; // AzaharFrame ui3
        item.version = s.version.toStdString();
        item.latest = s.latest.toStdString();
        item.checked = Homebrew::CheckedText(s.checked, now);
        can_check |= s.installed && !s.busy && !s.checking;
        model.items.push_back(std::move(item));
    }
    if (check_action != nullptr) {
        check_action->setEnabled(can_check);
    }
    FrameXr::SetInGameMenuHomebrew(model);
}

void FrameHomebrew::CheckForUpdates() {
    for (HomebrewPackager* p : packagers) {
        p->CheckForUpdate();
    }
}

void FrameHomebrew::Run(const QString& package, bool update) {
    for (HomebrewPackager* p : packagers) {
        if (QString::fromUtf8(p->Info().id.data(), static_cast<qsizetype>(p->Info().id.size())) !=
                package ||
            p->GetStatus().busy) {
            continue;
        }
        if (update && p->GetStatus().update) {
            p->Update(window);
        } else {
            p->Install(window);
        }
    }
}

void FrameHomebrew::CheckNow(const QString& package, bool vr) { // AzaharFrame ui3
    for (HomebrewPackager* p : packagers) {
        const QString id =
            QString::fromUtf8(p->Info().id.data(), static_cast<qsizetype>(p->Info().id.size()));
        const HomebrewPackager::Status s = p->GetStatus();
        if ((!package.isEmpty() && id != package) || !s.installed || s.busy) {
            continue;
        }
        const QString title = QString::fromUtf8(p->Info().title.data(),
                                                static_cast<qsizetype>(p->Info().title.size()));
        QPointer<QWidget> parent(window);
        p->CheckNow([vr, title, parent](Homebrew::CheckResult result) {
            const std::string_view text = Homebrew::ResultText(result);
            if (vr) {
                FrameXr::ShowToastText(std::string(text), FrameXr::ToastTime::Long);
                return;
            }
            if (!parent) {
                return;
            }
            const QString body = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
            if (result == Homebrew::CheckResult::Failed) {
                QMessageBox::warning(parent, title, body);
            } else if (result == Homebrew::CheckResult::UpdateAvailable) {
                QMessageBox::information(parent, title,
                                         body + QStringLiteral(". ") +
                                             tr("Choose Update %1 in the Homebrew menu.").arg(title));
            } else {
                QMessageBox::information(parent, title, body);
            }
        });
    }
}

QString FrameHomebrew::GamePath(const QString& package) const {
    for (HomebrewPackager* p : packagers) {
        if (QString::fromUtf8(p->Info().id.data(), static_cast<qsizetype>(p->Info().id.size())) ==
            package) {
            const auto s = p->GetStatus();
            return s.installed ? s.game_path : QString{};
        }
    }
    return {};
}

void FrameHomebrew::ApplyBootSettings(const QString& path) {
    if (g_instance == nullptr) {
        return;
    }
    for (HomebrewPackager* p : g_instance->packagers) {
        if (p->ApplyBootSettings(path)) {
            return;
        }
    }
}
