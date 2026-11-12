// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "citra_qt/frame_cheats.h"

#include <thread>

#include <QApplication>
#include <QPointer>
#include <QTimer>

#include "common/logging/log.h"
#include "core/cheats/online.h"
#include "core/core.h"
#include "frame_xr/frame_xr.h"

namespace Online = Cheats::Online;
namespace Menu = FrameXr::Menu;

namespace {

Menu::CheatSource ToMenu(Online::Source s) {
    switch (s) {
    case Online::Source::Developer:
        return Menu::CheatSource::Developer;
    case Online::Source::Sharkive:
        return Menu::CheatSource::Sharkive;
    case Online::Source::User:
        break;
    }
    return Menu::CheatSource::User;
}

/// `work` on a thread of its own, then `done` with its result on the GUI thread (if `self` is
/// still there).
template <typename Work, typename Done>
void OffThread(FrameCheats* self, Work work, Done done) {
    QPointer<FrameCheats> guard(self);
    std::thread([guard, work = std::move(work), done = std::move(done)] {
        auto result = work();
        QMetaObject::invokeMethod(
            qApp,
            [guard, done, result = std::move(result)] {
                if (guard) {
                    done(result);
                }
            },
            Qt::QueuedConnection);
    }).detach();
}

} // namespace

FrameCheats::FrameCheats(Core::System& system_, std::function<bool()> menu_open_, QObject* parent)
    : QObject(parent), system(system_), menu_open(std::move(menu_open_)) {
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (title_id != 0 && menu_open && menu_open()) {
            Publish();
        }
    });
    timer->start(1000);
}

FrameCheats::~FrameCheats() = default;

void FrameCheats::Open(u64 id, const std::string& path) {
    if (id != title_id) {
        download = Menu::DownloadState::Idle;
        status.clear();
    }
    title_id = id;
    game_path = path;
    Publish();
}

void FrameCheats::Publish() {
    Menu::CheatsModel m;
    if (title_id != 0) {
        for (const auto& c : Online::ListCheats(system.CheatEngine(), title_id)) {
            m.cheats.push_back({c.name, ToMenu(c.source), c.enabled, c.version_note});
        }
    }
    m.download = download;
    m.status = status;
    if (m == model && published) {
        return;
    }
    model = m;
    published = true;
    FrameXr::SetInGameMenuCheats(m);
}

void FrameCheats::Command(const FrameXr::MenuRequest& r) {
    using C = FrameXr::MenuCommand;
    if (title_id == 0) {
        return;
    }
    switch (r.command) {
    case C::ToggleCheat:
        if (r.value >= 0 && r.value < static_cast<int>(model.cheats.size())) {
            SetCheat(r.value, !model.cheats[static_cast<std::size_t>(r.value)].enabled);
        }
        break;
    case C::DownloadCheats:
        Download();
        break;
    default:
        break;
    }
}

void FrameCheats::SetCheat(int index, bool on) {
    if (Online::SetCheatEnabled(system.CheatEngine(), title_id, static_cast<std::size_t>(index),
                                on)) {
        LOG_INFO(Frontend, "xr: in-game menu: cheat {} {}", index, on ? "on" : "off");
    }
    Publish();
}

void FrameCheats::Download() {
    if (download == Menu::DownloadState::Downloading) {
        return;
    }
    download = Menu::DownloadState::Downloading;
    status.clear();
    Publish();
    LOG_INFO(Frontend, "xr: in-game menu: downloading cheats");
    Cheats::CheatEngine* engine = &system.CheatEngine();
    const u64 id = title_id;
    const std::string path = game_path;
    OffThread(
        this, [engine, id, path] { return Online::DownloadCheats(*engine, id, path); },
        [this, id](const Online::DownloadResult& result) {
            if (id != title_id) {
                return;
            }
            if (!result.error.empty()) {
                download = Menu::DownloadState::Failed;
                status = Menu::FailureReason(Online::StatusText(result)); // under "Download Failed"
            } else {
                download = Menu::DownloadState::Done;
                status = Online::StatusText(result);
            }
            LOG_INFO(Frontend, "xr: in-game menu: cheats download: {}", Online::StatusText(result));
            Publish();
        });
}
