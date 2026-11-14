// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctgp7: see frame_ctgp7.h.

#include "citra_qt/frame_ctgp7.h"

#include <filesystem>
#include <thread>
#include <utility>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QPointer>
#include <QTimer>
#include "citra_qt/frame_test_env.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/ctgp7/cia_build.h"
#include "core/hle/service/am/am.h"
#include "core/mods/library.h"
#include "core/mods/multipart.h"
#include "frame_xr/frame_xr.h"

namespace fs = std::filesystem;
namespace Menu = FrameXr::Menu;

namespace {

// The owner approves these (handoff/reports/34/ctgp7/REPORT.md, original -> proposed).
constexpr const char* kPhaseGame = "Installing Mario Kart 7";
constexpr const char* kPhaseDownload = "Downloading CTGP-7";
constexpr const char* kPhaseUnpack = "Unpacking CTGP-7";
constexpr const char* kPhaseInstall = "Installing CTGP-7";
constexpr const char* kPhaseStart = "Starting CTGP-7";
constexpr const char* kAboutFive = "This Takes About 5 Minutes";
constexpr const char* kToastInstalling = "Installing CTGP-7: About 5 Minutes";
constexpr const char* kToastInstalled = "CTGP-7 Installed";
constexpr const char* kFailed = "Install Failed: ";
constexpr const char* kBlockUpdate = "Needs Mario Kart 7 Version 1.2";
constexpr const char* kBlockSave = "Play Mario Kart 7 Once First";
constexpr const char* kUpdateTitle = "Update Needed";
constexpr const char* kUpdateBody =
    "CTGP-7 needs Mario Kart 7 version 1.2. Dump the 1.2 update from your 3DS and install it with "
    "File > Install CIA in Azahar's window.";
constexpr const char* kSaveTitle = "Play Mario Kart 7 First";
constexpr const char* kSaveBody =
    "CTGP-7 needs Mario Kart 7's save data. Play until the main menu, then try again.";
constexpr const char* kChooseTitle = "Choose Your Mario Kart 7";
constexpr const char* kChooseBody =
    "CTGP-7 needs a decrypted .3ds, .cci or .cia of your own Mario Kart 7.";
constexpr const char* kChooseButton = "Choose";
constexpr const char* kNotSameGame = "Not the Same Mario Kart 7";
constexpr const char* kPhaseUninstall = "Uninstalling CTGP-7";
constexpr const char* kToastUninstalled = "CTGP-7 Uninstalled";
constexpr const char* kUninstallTitle = "Uninstall CTGP-7?";
constexpr const char* kUninstallBody =
    "This removes CTGP-7, its launcher and the mods installed for it. Mario Kart 7, its save data "
    "and CTGP-7's progress stay.";
constexpr const char* kUninstallBodyBoth =
    "This removes CTGP-7, its launcher and the mods installed for it. Uninstall Both also removes "
    "the Mario Kart 7 it installed. Your game file, its save data and CTGP-7's progress stay.";
constexpr const char* kUninstallButton = "Uninstall";
constexpr const char* kUninstallBoth = "Uninstall Both";
constexpr const char* kDeleteProgress = "Also Delete CTGP-7's Progress";

/// What the installer writes in all (its installinfo.txt: "S/950000000"), for the bar.
constexpr u64 kInstallerWrites = 950000000ULL;
/// How long the installer may take to show its files, and to finish.
constexpr qint64 kStartLimitMs = 120 * 1000;
constexpr qint64 kRunLimitMs = 25 * 60 * 1000;

/// Runs `work` on a thread of its own, then `done(result)` on the GUI thread while `self` lives.
template <typename Work, typename Done>
void OffThread(QObject* self, Work work, Done done) {
    QPointer<QObject> guard(self);
    std::thread([guard, work = std::move(work), done = std::move(done)]() mutable {
        Mods::Multipart::LowerThisThreadPriority(); // never against the game
        auto result = work();
        QMetaObject::invokeMethod(
            qApp,
            [guard, done = std::move(done), result = std::move(result)]() mutable {
                if (guard) {
                    done(std::move(result));
                }
            },
            Qt::QueuedConnection);
    }).detach();
}

std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

FrameCtgp7::FrameCtgp7(Hooks hooks_, QObject* parent)
    : QObject(parent), hooks(std::move(hooks_)), paths(Ctgp7::DefaultPaths()),
      progress(std::make_shared<Progress>()) {
    tick = new QTimer(this);
    tick->setInterval(250);
    connect(tick, &QTimer::timeout, this, [this] {
        if (step == Step::Installer || step == Step::Finishing) {
            SessionTick();
        }
        if (step != Step::Idle) { // the progress (an idle segment changes only on events)
            Publish();
        }
    });
    tick->start();
}

FrameCtgp7::~FrameCtgp7() {
    if (run) {
        run->stopper.Stop();
    }
}

void FrameCtgp7::Open(u64 title_id_, const QString& path) {
    if (step == Step::Idle) {
        title_id = title_id_;
        game_path = path;
    }
    running_title = title_id_;
    Publish();
}

void FrameCtgp7::Publish() {
    Menu::Ctgp7Model m;
    const bool session = step == Step::Installer || step == Step::Finishing;
    m.shown = Ctgp7::SegmentShown(running_title, session || step == Step::Preparing);
    const Ctgp7::Found found = step == Step::Idle ? Ctgp7::DetectInstall(paths) : Ctgp7::Found::None;
    m.installed = found == Ctgp7::Found::Installed;
    m.setup_unfinished =
        found == Ctgp7::Found::FinishSetup || found == Ctgp7::Found::NeedsInstaller;
    if (m.installed) {
        m.version = Ctgp7::InstalledVersion(paths);
    }
    m.busy = step != Step::Idle;
    {
        std::scoped_lock lock{progress->mutex};
        m.phase = progress->phase;
        m.detail = progress->detail;
        m.permille = progress->permille;
    }
    m.can_cancel = step != Step::Finishing && step != Step::Uninstalling;
    m.error = error;
    m.dialog = dialog;
    if (!m.installed && !m.busy && Ctgp7::IsSupportedMarioKart7(title_id)) {
        const Ctgp7::Blocker b = Ctgp7::BlockerOf(Ctgp7::CheckGame(title_id));
        m.blocker = b == Ctgp7::Blocker::Update     ? kBlockUpdate
                    : b == Ctgp7::Blocker::SaveData ? kBlockSave
                                                    : "";
    }
    FrameXr::SetInGameMenuCtgp7(m);
}

void FrameCtgp7::Ask(const std::string& title, const std::string& body) {
    dialog = {};
    dialog.kind = Menu::ModsDialogKind::Notice;
    dialog.mod = title;
    dialog.other = body;
    dialog.confirm = "OK";
    Publish();
}

void FrameCtgp7::Fail(const std::string& why) {
    LOG_WARNING(Frontend, "ctgp7: install stopped: {}", why);
    const bool session = step == Step::Installer || step == Step::Finishing;
    step = Step::Idle;
    run.reset();
    {
        std::scoped_lock lock{progress->mutex};
        progress->phase.clear();
                progress->detail.clear();
                progress->permille = -1;
    }
    if (why == "Cancelled") {
        error.clear();
    } else {
        error = kFailed + why;
        if (session && hooks.notify) {
            hooks.notify(error);
        }
    }
    Publish();
    if (!FrameTestEnv("AZAHARFRAME_TEST_CTGP7").isEmpty() && hooks.quit) {
        LOG_INFO(Frontend, "ctgp7: test run ends: {}", error.empty() ? "cancelled" : error);
        hooks.quit();
    }
}

void FrameCtgp7::Command(const FrameXr::MenuRequest& r) {
    using C = FrameXr::MenuCommand;
    switch (r.command) {
    case C::Ctgp7Install:
        Install();
        break;
    case C::Ctgp7Launch:
        Launch();
        break;
    case C::Ctgp7Cancel:
        Cancel();
        break;
    case C::Ctgp7Uninstall:
        AskUninstall();
        break;
    case C::Ctgp7Dialog: {
        const Menu::ModsDialog d = std::exchange(dialog, {});
        Publish();
        if (d.confirm == kUninstallButton && (r.value == 1 || r.value == 2)) {
            Uninstall(r.value == 2, r.text == "checked");
            break;
        }
        if (r.value == 1 && d.confirm == kChooseButton) {
            // A copy that can be used: Azahar's window, its file dialog
            QWidget* w = hooks.window ? hooks.window() : nullptr;
            QTimer::singleShot(0, this, [this, w] {
                QString file = FrameTestEnv("AZAHARFRAME_TEST_CTGP7_ROM");
                if (file.isEmpty()) {
                    file = QFileDialog::getOpenFileName(
                        w, QString::fromUtf8(kChooseTitle), QDir::homePath(),
                        tr("3DS Games (*.3ds *.cci *.cia *.cxi *.zcci *.zcia);;All Files (*)"));
                }
                if (file.isEmpty()) {
                    return;
                }
                const auto s = Ctgp7::Cia::Inspect(file.toStdString());
                if (!s.error.empty()) {
                    error = kFailed + s.error;
                } else if (s.title_id != title_id) {
                    error = std::string(kFailed) + kNotSameGame;
                } else {
                    game_path = file;
                    Install();
                    return;
                }
                Publish();
            });
        }
        break;
    }
    default:
        break;
    }
}

void FrameCtgp7::Install() {
    if (step != Step::Idle || !Ctgp7::IsSupportedMarioKart7(title_id)) {
        return;
    }
    error.clear();
    const Ctgp7::GameCheck check = Ctgp7::CheckGame(title_id);
    switch (Ctgp7::BlockerOf(check)) {
    case Ctgp7::Blocker::Update:
        LOG_INFO(Frontend, "ctgp7: {:016X} has no v1.2 update (update version {})", title_id,
                 check.update_version ? std::to_string(*check.update_version) : "none");
        Ask(kUpdateTitle, kUpdateBody);
        return;
    case Ctgp7::Blocker::SaveData:
        LOG_INFO(Frontend, "ctgp7: {:016X} has no save data yet", title_id);
        Ask(kSaveTitle, kSaveBody);
        return;
    case Ctgp7::Blocker::NotMarioKart7:
        return;
    case Ctgp7::Blocker::None:
        break;
    }
    // A copy found on the SD card with its launcher's CIA: installing that finishes it
    const Ctgp7::Found found = Ctgp7::DetectInstall(paths);
    if (found == Ctgp7::Found::Installed) {
        return;
    }
    if (found == Ctgp7::Found::FinishSetup) {
        FinishSetup();
        return;
    }
    // The game as a title: installed already, or the file it was started from
    std::string game;
    u64 game_bytes = 0;
    if (!check.installed) {
        const std::string path = game_path.toStdString();
        const Ctgp7::Cia::Source s = Ctgp7::Cia::Inspect(path);
        if (!s.error.empty() || s.title_id != title_id) {
            LOG_INFO(Frontend, "ctgp7: {} can't be installed: {}", path,
                     s.error.empty() ? "another title" : s.error);
            dialog = {};
            dialog.kind = Menu::ModsDialogKind::Notice;
            dialog.mod = kChooseTitle;
            dialog.other = (s.error.empty() ? std::string(kNotSameGame) : s.error) + ". " + kChooseBody;
            dialog.confirm = kChooseButton;
            dialog.cancel = "Cancel";
            Publish();
            return;
        }
        game = path;
        game_bytes = s.size;
    }
    // Space for all of it, before anything starts
    u64 free_bytes = 0;
    FileUtil::CreateFullPath(paths.work);
    if (Mods::Multipart::FreeBytes(paths.work, &free_bytes)) {
        const u64 need = Ctgp7::RequiredBytes(Ctgp7::kInstallerZipBytes, game_bytes);
        if (free_bytes < need) {
            error = std::string(kFailed) + "Not Enough Space: Needs " +
                    Mods::Multipart::SizeText(need) + " Free";
            Publish();
            return;
        }
    }
    LOG_INFO(Frontend, "ctgp7: install for {:016X} ({}), update {}, game {}", title_id,
             Ctgp7::RegionName(Ctgp7::RegionOf(title_id)), *check.update_version,
             game.empty() ? "installed" : game);
    Prepare(std::move(game));
}

void FrameCtgp7::Prepare(std::string game) {
    step = Step::Preparing;
    generation++;
    run = std::make_shared<Run>();
    const auto r = run;
    const auto pr = progress;
    const Ctgp7::Paths pa = paths;
    const int gen = generation;
    {
        std::scoped_lock lock{pr->mutex};
        pr->phase.clear();
        pr->detail.clear();
        pr->permille = -1;
        pr->phase = game.empty() ? kPhaseDownload : kPhaseGame;
    }
    Publish();
    const auto set = [pr](const std::string& phase, const std::string& detail, int permille) {
        std::scoped_lock lock{pr->mutex};
        pr->phase = phase;
        pr->detail = detail;
        pr->permille = permille;
    };
    struct Result {
        std::string error;
        std::string installer;
        bool game_installed = false;
    };
    OffThread(
        this,
        [r, pa, game, set]() -> Result {
            Result out;
            std::error_code ec;
            // ---- 2. The game as a title ----
            if (!game.empty()) {
                std::string cia = game;
                const auto s = Ctgp7::Cia::Inspect(game);
                if (s.kind != Ctgp7::Cia::Kind::Cia) {
                    set(kPhaseGame, "", 0);
                    out.error = Ctgp7::Cia::Build(game, pa.game_cia, &r->cancel, [&](u64 d, u64 t) {
                        set(kPhaseGame, Menu::ProgressSizeText(d, t),
                            static_cast<int>(t ? d * 600 / t : 0));
                    });
                    if (!out.error.empty()) {
                        return out;
                    }
                    cia = pa.game_cia;
                }
                if (r->cancel.load()) {
                    fs::remove(pa.game_cia, ec);
                    out.error = "Cancelled";
                    return out;
                }
                const auto status = Service::AM::InstallCIA(cia, [&](std::size_t d, std::size_t t) {
                    set(kPhaseGame, "", static_cast<int>(600 + (t ? d * 400 / t : 0)));
                });
                fs::remove(pa.game_cia, ec);
                if (status == Service::AM::InstallStatus::Success) {
                    // what Uninstall Both may take out again (the file it came from stays)
                    Ctgp7::WriteInstalledGame(pa, {s.title_id, game});
                }
                if (status != Service::AM::InstallStatus::Success) {
                    out.error = "Mario Kart 7 Couldn't Be Installed";
                    return out;
                }
                out.game_installed = true;
                LOG_INFO(Frontend, "ctgp7: Mario Kart 7 installed as a title");
            }
            // ---- 3. The installer: kept from an earlier run (its download was checked), or fetched ----
            const std::string kept = pa.unpacked + ".checked";
            if (FileUtil::Exists(kept)) {
                for (fs::recursive_directory_iterator it(pa.unpacked, ec), end; !ec && it != end;
                     it.increment(ec)) {
                    const std::string name = Lower(it->path().filename().string());
                    if (name.size() > 5 && name.compare(name.size() - 5, 5, ".3dsx") == 0) {
                        out.installer = it->path().string();
                        LOG_INFO(Frontend, "ctgp7: the installer unpacked by an earlier run");
                        return out;
                    }
                }
            }
            set(kPhaseDownload, "", 0);
            const auto fetched = Ctgp7::FetchInstaller(
                pa, Ctgp7::RealGet(&r->cancel, &r->stopper), &r->cancel, [&](u64 d, u64 t) {
                    set(kPhaseDownload, Menu::ProgressSizeText(d, t),
                        static_cast<int>(t ? d * 1000 / t : -1));
                });
            if (!fetched.error.empty()) {
                out.error = fetched.error;
                return out;
            }
            set(kPhaseUnpack, "", -1);
            std::string why;
            out.installer = Ctgp7::UnpackInstaller(pa, fetched.zip, &r->cancel, &why);
            if (out.installer.empty()) {
                out.error = why;
                return out;
            }
            FileUtil::WriteStringToFile(true, kept, "md5 checked\n");
            return out;
        },
        [this, gen](Result res) {
            if (gen != generation || step != Step::Preparing) {
                return; // cancelled meanwhile
            }
            if (res.game_installed && hooks.refresh_list) {
                hooks.refresh_list();
            }
            if (!res.error.empty()) {
                Fail(res.error);
                return;
            }
            if (FrameTestEnv("AZAHARFRAME_TEST_CTGP7_STOP") == QStringLiteral("unpacked")) {
                LOG_INFO(Frontend, "ctgp7: test run: the installer is unpacked: {}", res.installer);
                step = Step::Idle;
                Publish();
                if (hooks.quit) {
                    hooks.quit();
                }
                return;
            }
            BootInstaller(res.installer);
        });
}

void FrameCtgp7::BootInstaller(const std::string& path) {
    installer = path;
    had_dir = FileUtil::IsDirectory(paths.dir);
    // a second's slack for file systems that keep coarse times
    base_time = std::filesystem::file_time_type::clock::now() - std::chrono::seconds(2);
    base_bytes = last_bytes = 0;
    started = false;
    quiet_ticks = 0;
    step = Step::Installer;
    {
        std::scoped_lock lock{progress->mutex};
        progress->phase = kPhaseInstall;
        progress->detail = kAboutFive;
        progress->permille = 0;
    }
    LOG_INFO(Frontend, "ctgp7: starting the installer {}", path);
    if (hooks.close_menu) {
        hooks.close_menu();
    }
    if (hooks.notify) {
        hooks.notify(kToastInstalling);
    }
    booting = true;
    if (hooks.boot) {
        hooks.boot(QString::fromStdString(path));
    }
    booting = false;
    session.start();
    Publish();
}

void FrameCtgp7::SessionTick() {
    const qint64 ms = session.elapsed();
    if (step == Step::Finishing) {
        return;
    }
    // A for "Start with install?" every 4 s from 6 s on, until its files appear
    const int quarter = static_cast<int>(ms / 250);
    if (!started && ms >= 6000 && hooks.press_a && hooks.running && hooks.running()) {
        if (quarter % 16 == 0) {
            hooks.press_a(true);
        } else if (quarter % 16 == 1) {
            hooks.press_a(false);
        }
    }
    if (quarter % 8 != 0) {
        return; // the folder is read every 2 s
    }
    const u64 bytes = Ctgp7::BytesWrittenSince(paths.dir, base_time);
    if (!started && bytes > base_bytes + (1ULL << 20)) {
        started = true;
        if (hooks.press_a) {
            hooks.press_a(false);
        }
        LOG_INFO(Frontend, "ctgp7: the installer is writing (after {} s)", ms / 1000);
    }
    {
        std::scoped_lock lock{progress->mutex};
        progress->permille = static_cast<int>(
            std::min<u64>(990, (bytes > base_bytes ? bytes - base_bytes : 0) * 1000 / kInstallerWrites));
    }
    quiet_ticks = bytes == last_bytes ? quiet_ticks + 1 : 0;
    last_bytes = bytes;
    if (!started && ms > kStartLimitMs) {
        EndSession();
        Fail("The Installer Didn't Start");
        return;
    }
    if (ms > kRunLimitMs) {
        EndSession();
        Fail("The Installer Took Too Long");
        return;
    }
    // Done: its version file written (last), its plugin and the launcher, and 6 s of quiet
    if (started && quiet_ticks >= 3 && Ctgp7::ModInstalled(paths) &&
        Ctgp7::WrittenSince(paths.version_file, base_time)) {
        LOG_INFO(Frontend, "ctgp7: CTGP-7 {} installed ({} MB, {} s)", Ctgp7::InstalledVersion(paths),
                 (bytes - base_bytes) >> 20, ms / 1000);
        step = Step::Finishing;
        {
            std::scoped_lock lock{progress->mutex};
            progress->phase = kPhaseStart;
            progress->detail.clear();
            progress->permille = 1000;
        }
        if (hooks.press_a) {
            hooks.press_a(false);
        }
        booting = true;
        if (hooks.stop) {
            hooks.stop();
        }
        booting = false;
        std::error_code ec;
        fs::remove_all(paths.unpacked, ec); // about 900 MB
        if (hooks.notify) {
            hooks.notify(kToastInstalled);
        }
        if (hooks.refresh_list) {
            hooks.refresh_list();
        }
        QTimer::singleShot(1500, this, [this] {
            step = Step::Idle;
            run.reset();
            {
                std::scoped_lock lock{progress->mutex};
                progress->phase.clear();
                progress->detail.clear();
                progress->permille = -1;
            }
            Launch();
        });
    }
}

void FrameCtgp7::EndSession() {
    if (hooks.press_a) {
        hooks.press_a(false);
    }
    if (hooks.running && hooks.running() && hooks.stop) {
        booting = true;
        hooks.stop();
        booting = false;
    }
    // A first install that stopped halfway leaves nothing behind (the launcher title stays: a
    // later install replaces it)
    if (!had_dir && !Ctgp7::ModInstalled(paths)) {
        std::error_code ec;
        fs::remove_all(paths.dir, ec);
    }
}

void FrameCtgp7::Cancel() {
    if (step == Step::Idle || step == Step::Finishing) {
        return;
    }
    LOG_INFO(Frontend, "ctgp7: Cancel");
    if (run) {
        run->stopper.Stop(); // the download's socket at once
    }
    generation++;
    if (step == Step::Installer) {
        EndSession();
    }
    Fail("Cancelled");
}

void FrameCtgp7::Launch() {
    const std::string path = Ctgp7::LauncherPath();
    if (path.empty() || !Ctgp7::ModInstalled(paths)) {
        error = std::string(kFailed) + "CTGP-7 Isn't Installed";
        Publish();
        return;
    }
    LOG_INFO(Frontend, "ctgp7: Launch {}", path);
    if (hooks.close_menu) {
        hooks.close_menu();
    }
    const QString q = QString::fromStdString(path);
    QTimer::singleShot(0, this, [this, q] {
        if (hooks.boot) {
            hooks.boot(q);
        }
    });
}

void FrameCtgp7::GameBooted(u64 title_id_, const QString& path) {
    running_title = title_id_;
    if (Ctgp7::IsSupportedMarioKart7(title_id_) && step == Step::Idle) {
        title_id = title_id_;
        game_path = path;
    }
    // AzaharFrame ctgp7: the plugin loader on for the launcher's run only (and the game it starts)
    if (title_id_ == Ctgp7::kLauncherTitleId) {
        Ctgp7::RunOverride::Begin();
    }
    // The test driver: AZAHARFRAME_TEST_CTGP7_A=N presses A every 5 s, N times, from 15 s on, in
    // Mario Kart 7 (its first start: the save data) and CTGP-7's launcher (its first questions)
    const int presses = FrameTestEnvInt("AZAHARFRAME_TEST_CTGP7_A");
    if (Ctgp7::IsSupportedMarioKart7(title_id_) || title_id_ == Ctgp7::kLauncherTitleId) {
        for (int i = 0; i < presses; i++) {
            QTimer::singleShot(15000 + i * 5000, this, [this, i, title_id_] {
                if (running_title != title_id_) {
                    return;
                }
                LOG_INFO(Frontend, "ctgp7: test run: A ({})", i + 1);
                if (hooks.press_a) {
                    hooks.press_a(true);
                    QTimer::singleShot(150, this, [this] { hooks.press_a(false); });
                }
            });
        }
    }
    // The test driver: Install that many seconds after a Mario Kart 7 starts
    const QString test = FrameTestEnv("AZAHARFRAME_TEST_CTGP7");
    // ... or "uninstall@SECONDS[:both][:progress]": Uninstall (Uninstall Both, Also Delete
    // CTGP-7's Progress) as if answered in the segment
    if (!test_fired && test.startsWith(QStringLiteral("uninstall@")) &&
        Ctgp7::IsSupportedMarioKart7(title_id_)) {
        test_fired = true;
        const QStringList parts = test.mid(10).split(QLatin1Char(':'));
        const int secs = parts.value(0).toInt();
        const bool both = parts.contains(QStringLiteral("both"));
        const bool progress_too = parts.contains(QStringLiteral("progress"));
        QTimer::singleShot(secs * 1000, this, [this, both, progress_too] {
            LOG_INFO(Frontend, "ctgp7: test run: Uninstall (found: {})",
                     static_cast<int>(Ctgp7::DetectInstall(paths)));
            AskUninstall();
            LOG_INFO(Frontend, "ctgp7: test run: the segment asks \"{}\": {} [{} | {} | {}]", dialog.mod,
                     dialog.other, dialog.extra, dialog.cancel, dialog.confirm);
            dialog = {};
            Uninstall(both, progress_too);
        });
    }
    if (!test_fired && test.startsWith(QStringLiteral("install@")) &&
        Ctgp7::IsSupportedMarioKart7(title_id_)) {
        test_fired = true;
        const int secs = test.mid(8).toInt();
        LOG_INFO(Frontend, "ctgp7: test run: Install in {} s", secs);
        QTimer::singleShot(secs * 1000, this, [this] {
            LOG_INFO(Frontend, "ctgp7: test run: Install");
            Install();
            if (!dialog.mod.empty()) {
                LOG_INFO(Frontend, "ctgp7: test run: the segment asks \"{}\": {}", dialog.mod,
                         dialog.other);
                if (dialog.confirm == kChooseButton && !FrameTestEnv("AZAHARFRAME_TEST_CTGP7_ROM").isEmpty()) {
                    FrameXr::MenuRequest r;
                    r.command = FrameXr::MenuCommand::Ctgp7Dialog;
                    r.value = 1;
                    Command(r);
                } else if (hooks.quit) {
                    hooks.quit();
                }
            } else if (!error.empty()) {
                LOG_INFO(Frontend, "ctgp7: test run: {}", error);
                if (hooks.quit) {
                    hooks.quit();
                }
            }
        });
    }
    Publish();
}

void FrameCtgp7::GameStopped() {
    Ctgp7::RunOverride::End();
    running_title = 0;
    if (!booting && step == Step::Installer) {
        // The player stopped the installer (View held, Close Game)
        LOG_INFO(Frontend, "ctgp7: the installer was stopped");
        if (hooks.press_a) {
            hooks.press_a(false);
        }
        if (!had_dir && !Ctgp7::ModInstalled(paths)) {
            std::error_code ec;
            fs::remove_all(paths.dir, ec);
        }
        Fail("Installation Stopped");
        return;
    }
    Publish();
}

void FrameCtgp7::FinishSetup() {
    // The launcher's own CIA, which CTGP-7's installer left on the SD card: installed as it is;
    // the SD folder (MyStuff, CTGP-7's progress) stays untouched
    const std::string cia = Ctgp7::LauncherCia(paths);
    LOG_INFO(Frontend, "ctgp7: finishing a copy found on the SD card: installing {}", cia);
    step = Step::Preparing;
    generation++;
    run = std::make_shared<Run>();
    const int gen = generation;
    {
        std::scoped_lock lock{progress->mutex};
        progress->phase = kPhaseInstall;
        progress->detail.clear();
        progress->permille = -1;
    }
    Publish();
    OffThread(
        this, [cia] { return Service::AM::InstallCIA(cia); },
        [this, gen](Service::AM::InstallStatus status) {
            if (gen != generation || step != Step::Preparing) {
                return;
            }
            if (status != Service::AM::InstallStatus::Success) {
                Fail("CTGP-7's Launcher Couldn't Be Installed");
                return;
            }
            step = Step::Idle;
            run.reset();
            {
                std::scoped_lock lock{progress->mutex};
                progress->phase.clear();
                progress->permille = -1;
            }
            if (hooks.refresh_list) {
                hooks.refresh_list();
            }
            if (hooks.notify) {
                hooks.notify(kToastInstalled);
            }
            Publish();
            if (FrameTestEnv("AZAHARFRAME_TEST_CTGP7").startsWith(QStringLiteral("install@"))) {
                Launch();
            }
        });
}

void FrameCtgp7::AskUninstall() {
    if (step != Step::Idle || Ctgp7::DetectInstall(paths) == Ctgp7::Found::None) {
        return;
    }
    const Ctgp7::InstalledGame game = Ctgp7::ReadInstalledGame(paths);
    // Uninstall Both only for a Mario Kart 7 this app installed, still installed, whose file is
    // still there to play or install it from again
    const bool both = game.title_id != 0 && Ctgp7::TitleInstalled(game.title_id) &&
                      !game.source.empty() && FileUtil::Exists(game.source);
    error.clear();
    dialog = {};
    dialog.kind = Menu::ModsDialogKind::Notice;
    dialog.mod = kUninstallTitle;
    dialog.other = both ? kUninstallBodyBoth : kUninstallBody;
    dialog.confirm = kUninstallButton;
    dialog.cancel = "Cancel";
    dialog.extra = both ? kUninstallBoth : "";
    dialog.check = kDeleteProgress;
    Publish();
}

void FrameCtgp7::Uninstall(bool both, bool delete_progress) {
    if (step != Step::Idle) {
        return;
    }
    const Ctgp7::InstalledGame game = both ? Ctgp7::ReadInstalledGame(paths) : Ctgp7::InstalledGame{};
    LOG_INFO(Frontend, "ctgp7: uninstall (Mario Kart 7's title too: {}, CTGP-7's progress too: {})",
             game.title_id != 0, delete_progress);
    // Nothing runs from what goes: CTGP-7 (its launcher's run), or the Mario Kart 7 title going too
    if (Ctgp7::RunOverride::Active() || (game.title_id != 0 && hooks.running && running_title == game.title_id)) {
        if (hooks.close_menu) {
            hooks.close_menu();
        }
        booting = true;
        if (hooks.stop) {
            hooks.stop();
        }
        booting = false;
    }
    step = Step::Uninstalling;
    {
        std::scoped_lock lock{progress->mutex};
        progress->phase = kPhaseUninstall;
        progress->detail.clear();
        progress->permille = -1;
    }
    Publish();
    const Ctgp7::Paths pa = paths;
    const std::string titles = Ctgp7::TitleRoot(), tickets = Ctgp7::TicketFolder();
    OffThread(
        this,
        [pa, titles, tickets, game, delete_progress] {
            // the catalog's CTGP-7 mods first (their files in MyStuff and gamefs, their library copies)
            auto& library = Mods::Default();
            for (const u64 tid : {0x0004000000030800ULL, 0x0004000000030700ULL, 0x0004000000030600ULL,
                                  0x0004000000030A00ULL}) {
                for (const Mods::ModInfo& m : library.ListLoaded(tid)) {
                    if (m.managed && !m.ctgp7_files.empty()) {
                        const auto r = library.Remove(tid, m.id);
                        LOG_INFO(Frontend, "ctgp7: uninstall: mod {} removed{}", m.id,
                                 r.ok ? "" : ": " + r.error);
                    }
                }
            }
            const auto plan = Ctgp7::PlanUninstall(pa, titles, tickets, delete_progress, game.title_id);
            return Ctgp7::RunUninstall(plan, pa, titles, tickets, game.title_id);
        },
        [this](std::string err) {
            step = Step::Idle;
            {
                std::scoped_lock lock{progress->mutex};
                progress->phase.clear();
                progress->permille = -1;
            }
            error = err.empty() ? std::string{} : "Uninstall Failed: " + err;
            LOG_INFO(Frontend, "ctgp7: uninstall {}", err.empty() ? "done" : "failed: " + err);
            if (FrameTestEnv("AZAHARFRAME_TEST_CTGP7").startsWith(QStringLiteral("uninstall@")) &&
                hooks.quit) {
                hooks.quit();
            }
            if (hooks.refresh_list) {
                hooks.refresh_list();
            }
            if (err.empty() && hooks.notify) {
                hooks.notify(kToastUninstalled);
            }
            Publish();
        });
}
