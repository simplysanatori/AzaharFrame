// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: see frame_homebrew.h.

#include "citra_qt/frame_homebrew.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
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
    Refresh();
}

std::function<void()> FrameHomebrew::RefreshList() {
    return g_refresh_list ? g_refresh_list : [] {};
}

void FrameHomebrew::Refresh() {
    FrameXr::Menu::HomebrewTabModel model;
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
        item.installed = s.installed;
        item.update = s.update;
        item.busy = s.busy;
        item.progress = s.progress;
        item.status = s.status.toStdString();
        model.items.push_back(std::move(item));
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
