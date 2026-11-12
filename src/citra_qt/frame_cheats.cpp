// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "citra_qt/frame_cheats.h"

#include <thread>

#include <QApplication>
#include <QInputDialog> // AzaharFrame ui2
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include "common/logging/log.h"
#include "core/cheats/gateway_cheat.h" // AzaharFrame ui2
#include "core/cheats/online.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "frame_xr/frame_xr.h"
#include "frame_xr/menu/cheat_entry.h" // AzaharFrame ui2
#include "frame_xr/swkbd.h"

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
    case C::AddCheat: // AzaharFrame ui2
        AddCheat();
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
    // AzaharFrame integ: the network part off the GUI thread, tagged with this title; the merge on
    // the GUI thread (where games start) only while that title still runs, so a download that ends
    // after another game started never lands in that game's list.
    const u64 id = title_id;
    const std::string path = game_path;
    OffThread(
        this, [id, path] { return Online::FetchCheats(id, path); },
        [this, id](const Online::Fetch& fetch) {
            const auto result = Online::ApplyFetch(system.CheatEngine(), fetch, RunningTitle());
            if (id != title_id) {
                return; // the menu shows another game now (Open reset the state)
            }
            if (result.stale) {
                download = Menu::DownloadState::Idle;
                status.clear();
            } else if (!result.error.empty()) {
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

// AzaharFrame integ
u64 FrameCheats::RunningTitle() const {
    u64 id = 0;
    if (!system.IsPoweredOn() ||
        system.GetAppLoader().ReadProgramId(id) != Loader::ResultStatus::Success) {
        return 0;
    }
    return id;
}

// ---- Add Cheat (AzaharFrame ui2) ----

void FrameCheats::AddCheat() {
    if (adding || title_id == 0) {
        return;
    }
    if (!FrameXr::VrShowing()) {
        AddFlat(title_id);
        return;
    }
    AskName(title_id);
}

void FrameCheats::AskName(u64 id) {
    FrameXr::Swkbd::Spec spec;
    spec.hint = "Cheat Name";
    spec.max_length = 60;
    spec.buttons = {{"Cancel", 0, false}, {"Next", 1, true}};
    spec.validate = [](const std::string& text) { return Menu::CheckCheatName(text); };
    spec.over_menu = true;
    QPointer<FrameCheats> guard(this);
    adding = FrameXr::OpenGameKeyboard(std::move(spec), [guard, id](const FrameXr::SwkbdOutcome& o) {
        QMetaObject::invokeMethod(
            qApp,
            [guard, id, o] {
                if (!guard) {
                    return;
                }
                guard->adding = false;
                if (!o.shown || o.button != 1 || guard->title_id != id) {
                    return; // cancelled, VR ended, or another game
                }
                guard->AskCode(id, o.text);
            },
            Qt::QueuedConnection);
    });
    if (adding) {
        LOG_INFO(Frontend, "xr: in-game menu: add cheat: name");
    }
}

void FrameCheats::AskCode(u64 id, const std::string& name) {
    FrameXr::Swkbd::Spec spec;
    spec.hint = "Code (XXXXXXXX YYYYYYYY)";
    spec.max_length = 4000;
    spec.multiline = true;
    spec.buttons = {{"Cancel", 0, false}, {"Add", 1, true}};
    spec.validate = [](const std::string& text) {
        std::string code;
        return Menu::NormalizeCheatCode(text, code);
    };
    spec.over_menu = true;
    QPointer<FrameCheats> guard(this);
    adding = FrameXr::OpenGameKeyboard(
        std::move(spec), [guard, id, name](const FrameXr::SwkbdOutcome& o) {
            QMetaObject::invokeMethod(
                qApp,
                [guard, id, name, o] {
                    if (!guard) {
                        return;
                    }
                    guard->adding = false;
                    if (!o.shown || o.button != 1 || guard->title_id != id) {
                        return;
                    }
                    std::string code;
                    if (Menu::NormalizeCheatCode(o.text, code).empty()) {
                        guard->StoreCheat(id, name, code);
                    }
                },
                Qt::QueuedConnection);
        });
}

void FrameCheats::AddFlat(u64 id) {
    QWidget* owner = qobject_cast<QWidget*>(parent());
    QString name;
    for (;;) {
        bool ok = false;
        name = QInputDialog::getText(owner, QStringLiteral("Add Cheat"),
                                     QStringLiteral("Cheat Name"), QLineEdit::Normal, name, &ok);
        if (!ok || title_id != id) {
            return;
        }
        const std::string why = Menu::CheckCheatName(name.toStdString());
        if (why.empty()) {
            break;
        }
        QMessageBox::warning(owner, QStringLiteral("Add Cheat"), QString::fromStdString(why));
    }
    QString text;
    for (;;) {
        bool ok = false;
        text = QInputDialog::getMultiLineText(owner, QStringLiteral("Add Cheat"),
                                              QStringLiteral("Code (XXXXXXXX YYYYYYYY)"), text, &ok);
        if (!ok || title_id != id) {
            return;
        }
        std::string code;
        const std::string why = Menu::NormalizeCheatCode(text.toStdString(), code);
        if (why.empty()) {
            StoreCheat(id, name.trimmed().toStdString(), code);
            return;
        }
        QMessageBox::warning(owner, QStringLiteral("Add Cheat"), QString::fromStdString(why));
    }
}

void FrameCheats::StoreCheat(u64 id, const std::string& name_in, const std::string& code) {
    if (id != title_id || id != RunningTitle()) {
        return;
    }
    std::string name = name_in;
    while (!name.empty() && (name.front() == ' ' || name.back() == ' ')) {
        name = name.front() == ' ' ? name.substr(1) : name.substr(0, name.size() - 1);
    }
    auto& engine = system.CheatEngine();
    Online::ListCheats(engine, id); // the game's file is loaded before one is added
    auto cheat = std::make_shared<Cheats::GatewayCheat>(name, code, std::string{});
    cheat->SetEnabled(true); // typed in to be used: on at once, as its switch shows
    engine.AddCheat(std::move(cheat));
    engine.SaveCheatFile(id);
    LOG_INFO(Frontend, "xr: in-game menu: cheat added ({} lines)",
             std::count(code.begin(), code.end(), '\n') + 1);
    FrameXr::ShowToastText(Menu::CheatsAddedText(1));
    Publish();
}
