// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame compat: see frame_crash_guard.h.

#include "citra_qt/frame_crash_guard.h"

#include <thread>
#include <utility>

#include <QApplication>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTimer>

#include "common/logging/log.h"
#include "core/custom_tex/packs.h"
#include "core/mods/library.h"

namespace CG = Mods::CrashGuard;

namespace {

constexpr int kCleanAfterS = 60;  ///< a run this long (not paused) is a clean run
constexpr int kNoticeAfterS = 2;  ///< the notice once the game has run this long

CG::Guard DefaultGuard() {
    return CG::Guard(CG::DefaultDir());
}

/// The managed mods in the mods folder and the active pack, as the guard names them.
std::vector<CG::Item> ActiveItems(u64 title_id) {
    std::vector<CG::Item> items;
    for (const Mods::ModInfo& m : Mods::Default().ListLoaded(title_id)) {
        if (m.managed && m.state == Mods::ModState::Active) {
            items.push_back({CG::Item::Kind::Mod, m.id, m.name});
        }
    }
    if (const auto pack = CustomTex::ActivePack(CustomTex::DefaultPaths(), title_id)) {
        items.push_back({CG::Item::Kind::Pack, pack->id, pack->name});
    }
    return items;
}

std::vector<std::string> Names(const std::vector<CG::Item>& items) {
    std::vector<std::string> names;
    for (const CG::Item& i : items) {
        names.push_back(i.name);
    }
    return names;
}

} // namespace

FrameCrashGuard& FrameCrashGuard::Get() {
    static QPointer<FrameCrashGuard> instance;
    if (!instance) {
        instance = new FrameCrashGuard(qApp);
    }
    return *instance;
}

FrameCrashGuard::FrameCrashGuard(QObject* parent) : QObject(parent) {
    timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, [this] { Tick(); });
}

void FrameCrashGuard::SetHooks(Hooks hooks_) {
    hooks = std::move(hooks_);
}

void FrameCrashGuard::NoteOn(u64 title_id, const CG::Item& item) {
    if (title_id != 0) {
        DefaultGuard().NoteOn(title_id, item);
    }
}

void FrameCrashGuard::NoteOff(u64 title_id, CG::Item::Kind kind, const std::string& id) {
    if (title_id != 0) {
        DefaultGuard().NoteOff(title_id, kind, id);
    }
}

void FrameCrashGuard::BeforeBoot(u64 title_id_) {
    turned_off.clear();
    turned_off_title = 0;
    notice_due = false;
    if (title_id_ == 0) {
        return;
    }
    CG::Guard guard = DefaultGuard();
    const std::vector<CG::Item> crashed = guard.TakeCrashed(title_id_);
    if (crashed.empty()) {
        return;
    }
    const auto active_pack = CustomTex::ActivePack(CustomTex::DefaultPaths(), title_id_);
    for (const CG::Item& item : crashed) {
        bool off = false;
        if (item.kind == CG::Item::Kind::Mod) {
            for (const Mods::ModInfo& m : Mods::Default().ListLoaded(title_id_)) {
                if (m.id == item.id && m.managed && m.state == Mods::ModState::Active) {
                    const auto r = Mods::Default().Deactivate(title_id_, item.id);
                    off = r.ok;
                    if (!r.ok) {
                        LOG_WARNING(Frontend, "xr: crash guard: {} not turned off: {}", item.id, r.error);
                    }
                }
            }
        } else if (active_pack && active_pack->id == item.id) {
            const auto r = CustomTex::Deactivate(CustomTex::DefaultPaths(), title_id_);
            off = r.ok;
            if (!r.ok) {
                LOG_WARNING(Frontend, "xr: crash guard: pack {} not turned off: {}", item.name, r.error);
            }
        }
        if (off) {
            LOG_INFO(Frontend, "xr: crash guard: the last run of {:016X} closed unexpectedly: {} {} "
                     "turned off", title_id_, item.kind == CG::Item::Kind::Mod ? "mod" : "pack", item.id);
            turned_off.push_back(item);
        }
    }
    if (!turned_off.empty()) {
        turned_off_title = title_id_;
        serial++;
        notice_due = true;
    }
}

void FrameCrashGuard::Started(u64 title_id_) {
    title_id = title_id_;
    core_error = false;
    run_ticks = 0;
    armed = false;
    if (title_id != 0) {
        const auto marked = DefaultGuard().Arm(title_id, ActiveItems(title_id));
        armed = !marked.empty();
        if (armed) {
            LOG_INFO(Frontend, "xr: crash guard: watching {} new mod(s) or pack(s) for {} s",
                     marked.size(), kCleanAfterS);
        }
    }
    if (armed || notice_due) {
        timer->start();
    }
}

void FrameCrashGuard::Stopped() {
    timer->stop();
    if (armed && !core_error) {
        DefaultGuard().Clean(title_id); // a normal Stop or Quit
        LOG_INFO(Frontend, "xr: crash guard: clean stop, the new mods stay on");
    } else if (armed) {
        LOG_INFO(Frontend, "xr: crash guard: the game stopped after an error; the mark stays");
    }
    armed = false;
    notice_due = false;
}

void FrameCrashGuard::CoreError() {
    core_error = true;
}

void FrameCrashGuard::Tick() {
    if (hooks.running && hooks.running()) {
        run_ticks++;
    }
    if (notice_due && run_ticks >= kNoticeAfterS) {
        notice_due = false;
        ShowNotice();
    }
    if (armed && run_ticks >= kCleanAfterS) {
        armed = false;
        DefaultGuard().Clean(title_id);
        LOG_INFO(Frontend, "xr: crash guard: {} s run, the new mods stay on", kCleanAfterS);
    }
    if (!armed && !notice_due) {
        timer->stop();
    }
}

void FrameCrashGuard::ShowNotice() {
    if (turned_off.empty()) {
        return;
    }
    if (hooks.vr && hooks.vr()) {
        // The in-game menu opens on the Mods tab with the snackbar (frame_mods.cpp)
        if (hooks.open_menu) {
            hooks.open_menu();
        }
        return;
    }
    // Flat: a message box with the same words
    QWidget* parent = hooks.window ? hooks.window() : nullptr;
    auto* box = new QMessageBox(QMessageBox::Information, QStringLiteral("Game Closed Unexpectedly"),
                                QString::fromStdString(CG::TurnedOffText(Names(turned_off))),
                                QMessageBox::NoButton, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton* on = box->addButton(QStringLiteral("Turn Back On"), QMessageBox::AcceptRole);
    box->addButton(QStringLiteral("OK"), QMessageBox::RejectRole);
    box->setDefaultButton(on);
    connect(box, &QMessageBox::finished, this, [this, box, on](int) {
        if (box->clickedButton() == on) {
            TurnBackOn([](bool) {});
        } else {
            Dismiss();
        }
    });
    box->open();
}

void FrameCrashGuard::Dismiss() {
    turned_off.clear();
    turned_off_title = 0;
}

void FrameCrashGuard::TurnBackOn(std::function<void(bool)> done) {
    if (turned_off.empty()) {
        done(false);
        return;
    }
    const std::vector<CG::Item> items = std::exchange(turned_off, {});
    const u64 tid = std::exchange(turned_off_title, 0);
    QPointer<FrameCrashGuard> self(this);
    std::thread([self, items, tid, done] {
        bool ok = true;
        for (const CG::Item& item : items) {
            bool on = false;
            if (item.kind == CG::Item::Kind::Mod) {
                const auto r = Mods::Default().Install(tid, item.id, false, false, true); // from the library
                on = r.result == Mods::InstallResult::Ok ||
                     r.result == Mods::InstallResult::AlreadyInstalled;
                if (!on) {
                    LOG_WARNING(Frontend, "xr: crash guard: {} not turned back on: {}", item.id, r.error);
                }
            } else {
                for (const CustomTex::Pack& p : CustomTex::ListPacks(CustomTex::DefaultPaths(), tid)) {
                    if (p.id == item.id) {
                        on = CustomTex::Activate(CustomTex::DefaultPaths(), tid, p).ok;
                    }
                }
            }
            if (on) {
                CG::Guard(CG::DefaultDir()).NoteOn(tid, item); // new again: watched at the next start
                LOG_INFO(Frontend, "xr: crash guard: {} turned back on", item.id);
            }
            ok = ok && on;
        }
        QMetaObject::invokeMethod(
            qApp, [self, done, ok] {
                if (self) {
                    done(ok);
                }
            },
            Qt::QueuedConnection);
    }).detach();
}
