// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: see frame_emerald.h.

#include "citra_qt/frame_test_env.h"
#include "common/atomic_file.h"
#include "citra_qt/frame_emerald.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>
#include <optional>
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QTimer>
#include "citra_qt/frame_texture_pack.h"
#include "citra_qt/uisettings.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include <fmt/format.h>
#include "core/hle/service/am/am.h"   // AzaharFrame emhb: the installed titles' folders
#include "core/hle/service/fs/archive.h"
#include "core/homebrew/emerald3ds_build.h"
#include "core/homebrew/emerald3ds_fallback.h"
#include "core/homebrew/net.h"

namespace E = Homebrew::Emerald3ds;
namespace F = Homebrew::Emerald3ds::Fallback;

namespace {

constexpr std::size_t kSmallReply = 2 * 1024 * 1024;

s64 Now() {
    return QDateTime::currentSecsSinceEpoch();
}

QString Q(std::string_view s) {
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

/// Runs `work` on a thread of its own and `done(result)` on the GUI thread (while `self` lives).
template <typename Work, typename Done>
void OffThread(QObject* self, Work work, Done done) {
    QPointer<QObject> guard(self);
    std::thread([guard, work = std::move(work), done = std::move(done)]() mutable {
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

/// Copies `from` over `to` through a temporary file (never a half-written `to`).
bool CopyOver(const std::string& from, const std::string& to) {
    const std::string tmp = to + ".new";
    FileUtil::Delete(tmp);
    if (!FileUtil::Copy(from, tmp)) {
        return false;
    }
    if (!FileUtil::Rename(tmp, to)) {
        FileUtil::Delete(tmp);
        return false;
    }
    return true;
}

QStringList CustomGameDirs() {
    QStringList out;
    for (const auto& d : UISettings::values.game_dirs) {
        if (d.path != QStringLiteral("INSTALLED") && d.path != QStringLiteral("SYSTEM")) {
            out << d.path;
        }
    }
    return out;
}

} // namespace

/// What the download step found and fetched.
struct FrameEmerald::Prepared {
    std::string error;
    bool up_to_date = false;
    E::UpdateKind kind = E::UpdateKind::Full;
    std::string tag;
    E::Variant variant;
    std::string rom_sha1;
    std::string payload_zip; ///< Full
    std::string out_dir;     ///< the files to install (Quick: downloaded; Full: built)
};

FrameEmerald::FrameEmerald(const Homebrew::PackageInfo& info, QObject* parent)
    : HomebrewPackager(info, parent) {
    paths = E::PlanPaths(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                         FileUtil::GetUserPath(FileUtil::UserPath::UserDir));
    LoadState();
    automatic = FrameTestEnv("AZAHARFRAME_HOMEBREW_AUTO") == Q(info.id);
    Rescan(); // AzaharFrame emhb
}

/// AzaharFrame emhb: what of Emerald 3DS the SD card, the game folders, the installed titles and
/// the user folder hold (core/homebrew/emerald3ds_detect.h).
void FrameEmerald::Rescan() {
    E::DetectInput in;
    in.paths = paths;
    for (const auto& d : UISettings::values.game_dirs) {
        if (d.path != QStringLiteral("INSTALLED") && d.path != QStringLiteral("SYSTEM")) {
            in.game_dirs.emplace_back(d.path.toStdString(), d.deep_scan);
        }
    }
    in.title_roots = {Service::AM::GetMediaTitlePath(Service::FS::MediaType::SDMC),
                      Service::AM::GetMediaTitlePath(Service::FS::MediaType::NAND)};
    E::Detection d = E::Detect(in);
    const auto summary = [](const E::Detection& x) {
        return fmt::format("{} piece(s), {}, {}, version {}, play {}", x.found.size(),
                           x.app_made ? "installed by this program" : "a copy",
                           x.playable ? "playable" : x.Any() ? x.Problem() : "nothing",
                           x.version.empty() ? "unknown" : x.version,
                           QFileInfo(QString::fromStdString(x.game_path)).fileName().toStdString());
    };
    if (summary(d) != summary(detected) || d.found != detected.found) {
        LOG_INFO(Frontend, "emerald3ds: found {}", d.Any() ? summary(d) : std::string("nothing"));
    }
    detected = std::move(d);
}

void FrameEmerald::AdoptCopy() { // AzaharFrame emhb
    if (!have_state && state.rom_dir_copy.empty() && !detected.game_copy.empty() &&
        QFileInfo(QString::fromStdString(detected.game_copy)).suffix().toLower() ==
            QStringLiteral("3dsx")) {
        state.rom_dir_copy = detected.game_copy;
    }
}

void FrameEmerald::Warn(QWidget* parent, const QString& title, const QString& text) {
    LOG_WARNING(Frontend, "emerald3ds: {}", text.toStdString());
    if (!automatic) {
        QMessageBox::warning(parent, title, text);
    }
}

std::string FrameEmerald::ChooseLanguage(QWidget* parent, const E::RomProfile& rom,
                                         const std::string& current) {
    const std::vector<std::string> langs = E::LanguagesFor(rom);
    if (automatic) {
        const std::string want = qEnvironmentVariable("AZAHARFRAME_HOMEBREW_LANG").toStdString();
        return want.empty() ? langs.front() : want;
    }
    if (langs.size() == 1) {
        return langs.front();
    }
    QStringList names;
    int selected = 0;
    for (std::size_t i = 0; i < langs.size(); i++) {
        names << QString::fromStdString(E::LanguageName(langs[i]));
        if (langs[i] == current) {
            selected = static_cast<int>(i);
        }
    }
    bool ok = false;
    const QString name = QInputDialog::getItem(parent, tr("Install Pokémon Emerald 3DS"),
                                               tr("Game Language"), names, selected, false, &ok);
    const auto index = names.indexOf(name);
    return ok && index >= 0 ? langs[static_cast<std::size_t>(index)] : std::string{};
}

void FrameEmerald::LoadState() {
    std::string text;
    have_state = false;
    if (FileUtil::Exists(paths.state_file) &&
        FileUtil::ReadFileToString(true, paths.state_file, text) > 0) {
        if (auto s = E::ParseState(text)) {
            state = *s;
            have_state = true;
        }
    }
}

void FrameEmerald::SaveState() {
    FileUtil::CreateFullPath(paths.data_dir);
    Common::WriteFileAtomic(paths.state_file, E::SerializeState(state)); // AzaharFrame harden: fsynced, atomic
}

HomebrewPackager::Status FrameEmerald::GetStatus() const {
    Status s;
    // AzaharFrame emhb: from the files (Rescan), whoever installed it; a copy with no state.json
    // has the version its files name (or none until a check finds its release)
    const std::string& version = have_state ? state.tag : detected.version;
    const std::string& latest = have_state ? state.latest_tag : imported_latest;
    const std::string& language = have_state ? state.language : detected.language;
    s.installed = detected.playable;
    s.repair = detected.Broken();
    s.can_uninstall = detected.Any();
    s.has_save = !detected.saves.empty();
    s.update = s.installed && E::IsNewer(latest, version);
    s.busy = busy;
    s.progress = progress;
    s.status = status;
    if (s.installed && !language.empty()) {
        s.detail = QString::fromStdString(E::LanguageName(language));
    } else if (s.repair) {
        s.detail = Q(detected.Problem()); // "Game Data Missing"
    }
    s.checking = checking; // AzaharFrame ui3
    if (s.installed) {
        s.version = QString::fromStdString(version);
        s.checked = have_state ? state.last_check : imported_checked;
    }
    if (s.update) {
        s.latest = QString::fromStdString(latest);
    }
    s.game_path = QString::fromStdString(detected.game_path.empty() ? paths.three_dsx
                                                                    : detected.game_path);
    return s;
}

bool FrameEmerald::ApplyBootSettings(const QString& path) {
    // AzaharFrame emhb: also a copy found under another name (a .3dsx, a .cia, an installed title)
    const QString clean = QDir::cleanPath(path);
    const auto same = [&clean](const std::string& p) {
        return !p.empty() && QDir::cleanPath(QString::fromStdString(p)) == clean;
    };
    bool ours = E::IsEmeraldPath(path.toStdString()) || same(detected.game_path);
    for (const E::Found& f : detected.found) {
        ours |= (f.piece == E::Piece::SdmcGame || f.piece == E::Piece::GameCopy) && same(f.path);
    }
    if (!ours) {
        return false;
    }
    // Its sound needs Azahar's HLE DSP (the placeholder dspfirm.cdc holds no firmware). A .3dsx has
    // no program id, so it has no per-game file: set for this run only (RestoreGlobalState).
    auto& audio = Settings::values.audio_emulation;
    if (audio.GetValue() != Settings::AudioEmulation::HLE) {
        audio.SetGlobal(false);
        audio.SetValue(Settings::AudioEmulation::HLE);
        LOG_INFO(Frontend, "emerald3ds: audio emulation set to HLE for this run");
    }
    return true;
}

QString FrameEmerald::PickRom(QWidget* parent) const {
    if (automatic) {
        return FrameTestEnv("AZAHARFRAME_HOMEBREW_ROM");
    }
    return QFileDialog::getOpenFileName(parent, tr("Choose Your Pokémon Emerald ROM"),
                                        QDir::homePath(),
                                        tr("GBA ROM (*.gba *.GBA);;All Files (*)"));
}

QString FrameEmerald::ChooseRomDir(QWidget* parent) {
    if (automatic) {
        return FrameTestEnv("AZAHARFRAME_HOMEBREW_ROMDIR");
    }
    if (!state.rom_dir_copy.empty()) {
        const QString dir = QFileInfo(QString::fromStdString(state.rom_dir_copy)).absolutePath();
        if (QFileInfo(dir).isDir()) {
            return dir;
        }
    }
    const QStringList dirs = CustomGameDirs();
    switch (E::ChooseRomDir(static_cast<std::size_t>(dirs.size()))) {
    case E::RomDirChoice::UseOnly:
        return dirs.front();
    case E::RomDirChoice::Ask: {
        bool ok = false;
        const QString dir = QInputDialog::getItem(parent, tr("Install Pokémon Emerald 3DS"),
                                                  tr("Game Folder"), dirs, 0, false, &ok);
        return ok ? dir : QString{};
    }
    case E::RomDirChoice::Prompt:
        break;
    }
    // No game folder yet: one for 3DS games, ~/Documents/ROMs/3DS by default, added to the list.
    const QString def = QString::fromStdString(E::DefaultRomDir(QDir::homePath().toStdString()));
    const bool made = !QFileInfo::exists(def) && QDir().mkpath(def);
    const QString dir =
        QFileDialog::getExistingDirectory(parent, tr("Choose a Folder for 3DS Games"), def);
    if (dir.isEmpty()) {
        if (made) {
            QDir().rmdir(def);
        }
        return {};
    }
    if (made && QDir(dir) != QDir(def)) {
        QDir().rmdir(def);
    }
    UISettings::GameDir game_dir{dir, false, true};
    if (!UISettings::values.game_dirs.contains(game_dir)) {
        UISettings::values.game_dirs.append(game_dir);
    }
    return dir;
}

/// AzaharFrame cpphelpers: what is about to be downloaded and from where, so the player decides
/// knowingly.
bool FrameEmerald::ConfirmSource(QWidget* parent, const QString& title) {
    if (automatic) { // developer and test runs have no dialogs (AZAHARFRAME_HOMEBREW_AUTO)
        return true;
    }
    const std::string& latest = have_state ? state.latest_tag : imported_latest; // AzaharFrame emhb
    const QString version =
        latest.empty() ? tr("the latest release") : tr("Version %1").arg(Q(latest));
    return QMessageBox::question(
               parent, title,
               tr("Download %1 from ZallaxDev/emerald-3ds on GitHub?\n\n"
                  "Your ROM is read only. The game's data is built from it with Emerald 3DS's own "
                  "build recipe from that release.")
                   .arg(version)) == QMessageBox::Yes;
}

void FrameEmerald::Install(QWidget* parent) {
    if (busy) {
        return;
    }
    Rescan(); // AzaharFrame emhb: a copy's game folder file is the one replaced
    AdoptCopy();
    const QString title = tr("Install Pokémon Emerald 3DS");
    Job job;
    // Reinstall from the kept ROM when it is still the one installed from.
    E::RomCheck check;
    if (have_state && !automatic && FileUtil::Exists(paths.rom_copy)) {
        check = E::CheckRomFile(paths.rom_copy);
    }
    if (check.ok && check.sha1 == state.rom_sha1) {
        job.rom = paths.rom_copy;
        job.rom_is_kept = true;
    } else {
        const QString rom = PickRom(parent);
        if (rom.isEmpty()) {
            return;
        }
        check = E::CheckRomFile(rom.toStdString());
        if (!check.ok) {
            LOG_WARNING(Frontend, "emerald3ds: ROM refused (SHA-1 {})", check.sha1);
            Warn(parent, title, QString::fromStdString(check.error));
            return;
        }
        job.rom = rom.toStdString();
    }
    LOG_INFO(Frontend, "emerald3ds: ROM {} (SHA-1 {})", check.profile->code, check.sha1);
    // The game's language, when the ROM builds more than one; remembered for updates.
    job.language =
        ChooseLanguage(parent, *check.profile,
                       have_state && check.sha1 == state.rom_sha1 ? state.language : std::string{});
    if (job.language.empty()) {
        return;
    }
    const QString dir = ChooseRomDir(parent);
    if (dir.isEmpty()) {
        return;
    }
    job.rom_dir = dir.toStdString();
    if (!ConfirmSource(parent, title)) {
        return;
    }
    Start(parent, std::move(job));
}

void FrameEmerald::Update(QWidget* parent) {
    if (busy) {
        return;
    }
    Rescan(); // AzaharFrame emhb
    AdoptCopy();
    Job job;
    job.update = true;
    job.language = state.language;
    if (FileUtil::Exists(paths.rom_copy) && E::CheckRomFile(paths.rom_copy).ok) {
        job.rom = paths.rom_copy;
        job.rom_is_kept = true;
    } else {
        // The kept ROM is gone: asked again (only a full update needs it, but it is checked first).
        const QString rom = PickRom(parent);
        if (rom.isEmpty()) {
            return;
        }
        const E::RomCheck check = E::CheckRomFile(rom.toStdString());
        if (!check.ok) {
            Warn(parent, tr("Update Pokémon Emerald 3DS"), QString::fromStdString(check.error));
            return;
        }
        job.rom = rom.toStdString();
    }
    const QString dir = ChooseRomDir(parent);
    if (dir.isEmpty()) {
        return;
    }
    job.rom_dir = dir.toStdString();
    if (!ConfirmSource(parent, tr("Update Pokémon Emerald 3DS"))) {
        return;
    }
    Start(parent, std::move(job));
}

void FrameEmerald::CheckForUpdate() {
    // AzaharFrame emhb: a copy with no state.json is checked once a day too, its time in memory
    const s64 last = have_state ? state.last_check : imported_checked;
    if (busy || checking || !GetStatus().installed || !E::CheckDue(last, Now())) {
        return;
    }
    if (have_state) {
        state.last_check = Now();
        SaveState();
    } else {
        imported_checked = Now();
    }
    Fetch();
}

void FrameEmerald::CheckNow(std::function<void(Homebrew::CheckResult)> done) { // AzaharFrame ui3
    if (busy || !GetStatus().installed) {
        return;
    }
    if (checking) { // the daily check, or a press a moment ago: its answer
        waiting.push_back(std::move(done));
        return;
    }
    const s64 now = Now();
    if (!manual.Due(now)) { // pressed again within the gap: the last answer, no request
        LOG_INFO(Frontend, "emerald3ds: Check for Updates: the last result again");
        done(*manual.last_result);
        return;
    }
    manual.Asked(now);
    waiting.push_back(std::move(done));
    Fetch();
}

void FrameEmerald::Fetch() {
    checking = true;
    emit Changed();
    // AzaharFrame emhb: a copy with no state.json: is its .3dsx the latest release's (by the
    // release's checksums and manifest)? Then its release and language are known.
    struct Answer {
        std::string tag;
        std::optional<E::Variant> variant;
    };
    const std::string identify = !have_state && detected.playable ? detected.three_dsx : "";
    OffThread(
        this,
        [identify] {
            Answer a;
            const Homebrew::Reply r = Homebrew::GetText(std::string(E::kLatestUrl), kSmallReply);
            if (!r.error.empty()) {
                return a;
            }
            const auto release = E::ParseRelease(r.body);
            if (!release) {
                return a;
            }
            a.tag = release->tag;
            if (identify.empty()) {
                return a;
            }
            const auto meta = E::SelectAssets(*release, E::UpdateKind::None, {}, nullptr);
            if (!meta) {
                return a;
            }
            const Homebrew::Reply sums = Homebrew::GetText(meta->sums->url, kSmallReply);
            const Homebrew::Reply man = Homebrew::GetText(meta->manifest->url, kSmallReply);
            if (!sums.error.empty() || !man.error.empty()) {
                return a;
            }
            const std::string man_sum = E::SumFor(E::ParseSums(sums.body), E::kManifestAsset);
            if (man_sum.empty() || man_sum != Homebrew::Sha256Hex(man.body)) {
                return a;
            }
            if (const auto manifest = E::ParseManifest(man.body)) {
                if (const E::Variant* v =
                        E::FindVariantByThreeDsx(*manifest, Homebrew::FileSha256(identify))) {
                    a.variant = *v;
                }
            }
            return a;
        },
        [this](Answer a) {
            checking = false;
            const std::string& tag = a.tag;
            const std::string installed =
                have_state ? state.tag : a.variant ? a.tag : detected.version;
            const Homebrew::CheckResult result = Homebrew::ResultOf(tag, installed);
            if (!tag.empty() && !busy && !have_state) { // AzaharFrame emhb: a copy
                imported_latest = tag;
                imported_checked = Now();
                if (a.variant && detected.playable) {
                    // It is this release: from now on it is managed as one this program installed
                    E::State s;
                    s.tag = tag;
                    s.language = a.variant->language;
                    s.rom_sha1 = a.variant->rom_sha1;
                    s.data_abi = a.variant->data_abi;
                    AdoptCopy();
                    s.rom_dir_copy = state.rom_dir_copy;
                    s.last_check = Now();
                    s.latest_tag = tag;
                    state = s;
                    have_state = true;
                    SaveState();
                    Rescan();
                    LOG_INFO(Frontend, "emerald3ds: the copy found is release {} ({})", tag,
                             s.language);
                } else {
                    LOG_INFO(Frontend, "emerald3ds: latest release {}, the copy found is {}", tag,
                             detected.version.empty() ? "of another release" : detected.version);
                }
            } else if (!tag.empty() && !busy) {
                state.latest_tag = tag;
                state.last_check = Now();
                SaveState();
                LOG_INFO(Frontend, "emerald3ds: latest release {}, installed {}", tag, state.tag);
            } else if (tag.empty()) {
                LOG_WARNING(Frontend, "emerald3ds: the update check had no answer");
            }
            manual.Answered(result);
            emit Changed();
            auto done = std::move(waiting);
            waiting.clear();
            for (auto& d : done) {
                d(result);
            }
        });
}

void FrameEmerald::SetBusy(bool busy_, const QString& status_, int progress_) {
    const bool changed = busy != busy_ || status != status_ || progress / 10 != progress_ / 10;
    busy = busy_;
    status = status_;
    progress = progress_;
    if (changed) {
        emit Changed();
    }
}

void FrameEmerald::Finish(QWidget* parent, const QString& error, const QString& done_text) {
    LoadState();
    Rescan(); // AzaharFrame emhb
    SetBusy(false, error);
    FileUtil::DeleteDirRecursively(paths.work_dir);
    if (automatic) {
        LOG_INFO(Frontend, "emerald3ds: automatic run ended: {}",
                 error.isEmpty() ? done_text.toStdString() : "failed: " + error.toStdString());
        if (FrameTestEnv("AZAHARFRAME_HOMEBREW_AUTO_QUIT") == QStringLiteral("1")) {
            QTimer::singleShot(0, qApp, &QApplication::quit);
        }
    }
    if (!error.isEmpty()) {
        Warn(parent, tr("Pokémon Emerald 3DS"), error);
        return;
    }
    FrameHomebrew::RefreshList()();
    if (!done_text.isEmpty() && !automatic) {
        QMessageBox::information(parent, tr("Pokémon Emerald 3DS"), done_text);
    }
}

void FrameEmerald::Start(QWidget* parent, Job job) {
    QPointer<QWidget> window(parent);
    auto cancel = std::make_shared<std::atomic_bool>(false);
    auto shown = std::make_shared<std::atomic_int>(0); // 0 to 1000 over the whole run

    auto* dialog =
        new QProgressDialog(tr("Downloading Pokémon Emerald 3DS"), tr("Cancel"), 0, 1000, parent);
    dialog->setWindowTitle(job.update ? tr("Update Pokémon Emerald 3DS")
                                      : tr("Install Pokémon Emerald 3DS"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumDuration(0);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    dialog->setValue(0);
    auto* timer = new QTimer(dialog);
    connect(timer, &QTimer::timeout, dialog, [this, dialog, shown] {
        dialog->setValue(shown->load());
        SetBusy(true, status, shown->load());
    });
    timer->start(100);
    connect(dialog, &QProgressDialog::canceled, this, [cancel] { cancel->store(true); });
    SetBusy(true, tr("Downloading"), 0);
    FileUtil::DeleteDirRecursively(paths.work_dir);
    FileUtil::CreateFullPath(paths.work_dir + "out/");

    const auto close_dialog = [dialog, timer] {
        timer->stop(); // no progress after the end
        dialog->hide();
        dialog->deleteLater();
    };

    // ---- 3. Install: the files into place (off the GUI thread) ----
    auto install = [this, window, job, close_dialog, shown](Prepared p) {
        SetBusy(true, tr("Installing"), 950);
        shown->store(950);
        const E::State old = state;
        const E::Paths pa = paths;
        const std::string stamp =
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")).toStdString();
        OffThread(
            this,
            [p, job, old, pa, stamp]() -> std::pair<std::string, E::State> {
                FileUtil::CreateFullPath(pa.app_dir);
                // The save and its companions, before anything is replaced; never moved.
                std::vector<std::string> saves;
                for (const std::string& name : E::SaveFiles()) {
                    if (FileUtil::Exists(pa.app_dir + name)) {
                        saves.push_back(name);
                    }
                }
                if (!saves.empty()) {
                    const std::string dir =
                        E::BackupDir(pa.backup_root, stamp,
                                     [](const std::string& d) { return FileUtil::Exists(d); });
                    const std::string partial = dir.substr(0, dir.size() - 1) + ".partial/";
                    FileUtil::CreateFullPath(partial);
                    for (const std::string& name : saves) {
                        if (!FileUtil::Copy(pa.app_dir + name, partial + name)) {
                            return {"The save could not be backed up, so nothing was changed", {}};
                        }
                    }
                    if (!FileUtil::Rename(partial.substr(0, partial.size() - 1),
                                          dir.substr(0, dir.size() - 1))) {
                        return {"The save could not be backed up, so nothing was changed", {}};
                    }
                    LOG_INFO(Frontend, "emerald3ds: save backed up to {}", dir);
                }
                std::vector<std::pair<std::string, std::string>> files{
                    {p.out_dir + std::string(E::kThreeDsxName), pa.three_dsx},
                    {p.out_dir + std::string(E::kSmdhName), pa.smdh}};
                if (p.kind == E::UpdateKind::Full) {
                    files.emplace_back(p.out_dir + std::string(E::kPakName), pa.pak);
                }
                for (const auto& [from, to] : files) {
                    if (!CopyOver(from, to)) {
                        return {"The game files could not be written to the SD card folder", {}};
                    }
                }
                // Sound: Azahar's HLE DSP takes any component, so a placeholder; never over a file.
                if (!FileUtil::Exists(pa.dspfirm)) {
                    FileUtil::WriteStringToFile(false, pa.dspfirm, E::DspPlaceholder());
                }
                // The copy the game list shows (the pack stays on the SD card, where it is read).
                FileUtil::CreateFullPath(job.rom_dir + "/");
                const std::string copy = E::RomDirCopy(job.rom_dir);
                if (!CopyOver(pa.three_dsx, copy)) {
                    return {"The game could not be copied to the game folder", {}};
                }
                if (!old.rom_dir_copy.empty() && old.rom_dir_copy != copy) {
                    FileUtil::Delete(old.rom_dir_copy);
                }
                // The ROM, for updates that make the pack again.
                if (!job.rom_is_kept) {
                    FileUtil::CreateFullPath(pa.data_dir);
                    if (!CopyOver(job.rom, pa.rom_copy)) {
                        LOG_WARNING(Frontend, "emerald3ds: the ROM could not be kept");
                    }
                }
                E::State s = old;
                s.tag = p.tag;
                s.language = p.variant.language;
                s.rom_sha1 = p.rom_sha1;
                s.data_abi = p.variant.data_abi;
                s.rom_dir_copy = copy;
                s.last_check = QDateTime::currentSecsSinceEpoch();
                s.latest_tag = p.tag;
                return {std::string{}, s};
            },
            [this, window, close_dialog, p](std::pair<std::string, E::State> r) {
                close_dialog();
                if (r.first.empty()) {
                    state = r.second;
                    SaveState();
                    LOG_INFO(Frontend, "emerald3ds: {} installed ({}, {})", p.tag,
                             p.variant.language,
                             p.kind == E::UpdateKind::Quick ? "quick update" : "pack built");
                }
                Finish(window, QString::fromStdString(r.first),
                       r.first.empty() ? tr("Pokémon Emerald 3DS %1 Installed").arg(Q(p.tag))
                                       : QString{});
            });
    };

    // ---- 2b. Fallback (AzaharFrame emfallback): the release's recipe is one the C++ builder cannot
    // follow. With the player's approval (asked once per release version) the release's own builder
    // builds the pack in a bubblewrap sandbox; what it writes is checked as for the C++ builder's pack
    // and installed the same way. ----
    auto fallback = [this, window, job, close_dialog, install, cancel, dialog,
                     shown](Prepared p, std::string reason) {
        LOG_WARNING(Frontend, "emerald3ds: the built-in builder cannot build {}: {}", p.tag, reason);
        const F::Tools tools = F::Detect();
        if (!tools.Ok()) {
            close_dialog();
            Finish(window, Q(F::UnavailableText(p.tag, tools.missing)), {});
            return;
        }
        const std::string choices_path = paths.data_dir + F::kChoicesFile;
        F::Choices choices = F::LoadChoices(choices_path);
        F::Choice choice = choices.Get(p.tag);
        if (choice == F::Choice::Unasked) {
            bool yes = false;
            bool answered = false;
            const QString test = FrameTestEnv("AZAHARFRAME_TEST_EMERALD_FALLBACK_ANSWER");
            if (!test.isEmpty()) {
                yes = test == QStringLiteral("yes");
                answered = true;
            }
            if (!answered && !automatic) {
                dialog->hide();
                QMessageBox box(QMessageBox::Question, Q(F::PromptTitle(p.tag)), Q(F::PromptText(true)),
                                QMessageBox::NoButton, window);
                QPushButton* build_button =
                    box.addButton(tr(F::kButtonBuild), QMessageBox::AcceptRole);
                box.addButton(tr(F::kButtonNotNow), QMessageBox::RejectRole);
                box.setDefaultButton(build_button);
                box.exec();
                yes = box.clickedButton() == build_button;
                dialog->show();
            }
            if (yes) { // only "Build" is remembered; Not Now just cancels this time
                choice = F::Choice::Allowed;
                choices.Set(p.tag, choice);
                if (!F::SaveChoices(choices_path, choices)) {
                    LOG_WARNING(Frontend, "emerald3ds: the choice could not be saved");
                }
            }
        }
        if (choice != F::Choice::Allowed) {
            close_dialog();
            Finish(window,
                   tr("Azahar for Frame can't build Emerald 3DS %1 itself yet. Nothing was installed.")
                       .arg(Q(p.tag)),
                   {});
            return;
        }
        SetBusy(true, tr("Building"), 400);
        dialog->setLabelText(tr("Building With Emerald 3DS's Builder\nIn a sandbox that can only read "
                                "your ROM and write its own folder"));
        F::Options fo;
        fo.build.payload_zip = p.payload_zip;
        fo.build.rom = job.rom;
        fo.build.out_dir = p.out_dir;
        fo.build.cache_dir = paths.data_dir + "followers-cache";
        fo.build.language = p.variant.language;
        fo.build.cancel = cancel.get();
        fo.build.progress = [shown](int permille, const std::string&) {
            shown->store(400 + permille * 550 / 1000);
        };
        fo.expected_abi = p.variant.data_abi;
        fo.expected_3dsx_sha256 = p.variant.three_dsx_sha256;
        fo.expected_rom_sha1 = p.rom_sha1;
        fo.tools = tools;
        OffThread(
            this, [fo]() { return F::Run(fo); },
            [this, window, close_dialog, install, cancel, p](E::Build::Result r) {
                if (cancel->load() || r.stopped) {
                    close_dialog();
                    Finish(window, tr("Cancelled"), {});
                    return;
                }
                if (!r.ok) {
                    LOG_WARNING(Frontend, "emerald3ds: Emerald 3DS's own builder: {}", r.error);
                    close_dialog();
                    Finish(window,
                           r.error.empty() ? tr("The data pack could not be built")
                                           : QString::fromStdString(r.error),
                           {});
                    return;
                }
                if (r.abi != p.variant.data_abi) {
                    close_dialog();
                    Finish(window, tr("The data pack does not match this release"), {});
                    return;
                }
                LOG_INFO(Frontend, "emerald3ds: {} built by Emerald 3DS's own builder in the sandbox (not the "
                                   "built-in builder)",
                         p.tag);
                install(p);
            });
    };

    // ---- 2. Build: emerald3ds.pak from the release's recipe (AzaharFrame cpphelpers: in this
    // program, off the GUI thread; no Python and no separate process) ----
    auto build = [this, window, job, close_dialog, install, fallback, cancel, dialog,
                  shown](Prepared p) {
        SetBusy(true, tr("Building"), 400);
        dialog->setLabelText(
            tr("Building the Data Pack\nUsing Emerald 3DS's own build recipe from the release"));
        E::Build::Options options;
        options.payload_zip = p.payload_zip;
        options.rom = job.rom;
        options.out_dir = p.out_dir;
        options.cache_dir = paths.data_dir + "followers-cache";
        options.language = p.variant.language;
        options.cancel = cancel.get();
        options.progress = [shown](int permille, const std::string&) {
            shown->store(400 + permille * 550 / 1000);
        };
        OffThread(
            this,
            [options]() {
                if (FrameTestEnv("AZAHARFRAME_TEST_EMERALD_FORCE_UNSUPPORTED") ==
                    QStringLiteral("1")) { // the recipe of a release this builder cannot follow
                    E::Build::Result forced;
                    forced.unsupported = true;
                    forced.error = "forced for a test";
                    return forced;
                }
                return E::Build::BuildPack(options);
            },
            [this, window, close_dialog, install, fallback, cancel, p](E::Build::Result r) {
                if (cancel->load() || r.stopped) {
                    close_dialog();
                    Finish(window, tr("Cancelled"), {});
                    return;
                }
                if (r.unsupported) {
                    fallback(p, r.error);
                    return;
                }
                if (!r.ok) {
                    LOG_WARNING(Frontend, "emerald3ds: builder: {}", r.error);
                    close_dialog();
                    Finish(window,
                           r.error.empty() ? tr("The data pack could not be built")
                                           : QString::fromStdString(r.error),
                           {});
                    return;
                }
                if (r.abi != p.variant.data_abi) {
                    close_dialog();
                    Finish(window, tr("The data pack does not match this release"), {});
                    return;
                }
                LOG_INFO(Frontend, "emerald3ds: {} built by the built-in (C++) builder", p.tag);
                install(p);
            });
    };

    // ---- 1. Download: the release's manifest and checksums, then what this run needs ----
    const E::State old = state;
    const E::Paths pa = paths;
    OffThread(
        this,
        [job, old, pa, cancel, shown]() -> Prepared {
            Prepared p;
            const E::RomCheck rom = E::CheckRomFile(job.rom);
            if (!rom.ok) {
                p.error = rom.error;
                return p;
            }
            p.rom_sha1 = rom.sha1;
            const Homebrew::Reply rel = Homebrew::GetText(std::string(E::kLatestUrl), kSmallReply);
            if (!rel.error.empty()) {
                p.error = "Could not reach GitHub: " + rel.error;
                return p;
            }
            const auto release = E::ParseRelease(rel.body, &p.error);
            if (!release) {
                return p;
            }
            p.tag = release->tag;
            const auto meta = E::SelectAssets(*release, E::UpdateKind::None, {}, &p.error);
            if (!meta) {
                return p;
            }
            const Homebrew::Reply sums_reply = Homebrew::GetText(meta->sums->url, kSmallReply);
            const Homebrew::Reply man_reply = Homebrew::GetText(meta->manifest->url, kSmallReply);
            if (!sums_reply.error.empty() || !man_reply.error.empty()) {
                p.error = "Download failed: " +
                          (sums_reply.error.empty() ? man_reply.error : sums_reply.error);
                return p;
            }
            const auto sums = E::ParseSums(sums_reply.body);
            const std::string man_sum = E::SumFor(sums, E::kManifestAsset);
            if (man_sum.empty() || man_sum != Homebrew::Sha256Hex(man_reply.body)) {
                p.error = "The release's files do not match its checksums";
                return p;
            }
            const auto manifest = E::ParseManifest(man_reply.body, &p.error);
            if (!manifest) {
                return p;
            }
            const E::Variant* variant = E::FindVariant(*manifest, rom.sha1, job.language);
            if (variant == nullptr) {
                p.error = job.language.empty()
                              ? "This release has no game data for this ROM"
                              : "This release has no " + E::LanguageName(job.language) +
                                    " version for this ROM";
                return p;
            }
            p.variant = *variant;
            p.kind = job.update ? E::PlanUpdate(old.tag, old.data_abi, release->tag, variant)
                                : E::UpdateKind::Full;
            if (job.update && p.kind == E::UpdateKind::None) {
                p.up_to_date = true;
                return p;
            }
            const auto plan = E::SelectAssets(*release, p.kind, variant->three_dsx, &p.error);
            if (!plan) {
                return p;
            }
            p.out_dir = pa.work_dir + "out/";
            std::vector<std::pair<const E::Asset*, std::string>> wanted;
            if (p.kind == E::UpdateKind::Full) {
                p.payload_zip = pa.work_dir + std::string(E::kPayloadAsset);
                wanted.emplace_back(plan->payload, p.payload_zip);
            } else {
                wanted.emplace_back(plan->three_dsx, p.out_dir + std::string(E::kThreeDsxName));
                wanted.emplace_back(plan->smdh, p.out_dir + std::string(E::kSmdhName));
            }
            u64 total = 0, before = 0;
            for (const auto& w : wanted) {
                total += std::max<u64>(w.first->size, 1);
            }
            for (const auto& [asset, dest] : wanted) {
                const std::string err =
                    Homebrew::Download(asset->url, dest, [&, base = before](u64 done, u64) {
                        shown->store(
                            static_cast<int>((base + done) * 400 / std::max<u64>(total, 1)));
                        return !cancel->load();
                    }, asset->size != 0 ? asset->size + (4ULL << 20) : 128ULL << 20);
                if (cancel->load()) {
                    p.error = "Cancelled";
                    return p;
                }
                if (!err.empty()) {
                    p.error = "Download failed: " + err;
                    return p;
                }
                const std::string want = E::SumFor(sums, asset->name);
                if (want.empty() || Homebrew::FileSha256(dest) != want) {
                    p.error = "The download does not match the release's checksums";
                    return p;
                }
                before += std::max<u64>(asset->size, 1);
            }
            if (p.kind == E::UpdateKind::Quick &&
                Homebrew::FileSha256(p.out_dir + std::string(E::kThreeDsxName)) !=
                    variant->three_dsx_sha256) {
                p.error = "The download does not match the release's manifest";
                return p;
            }
            shown->store(400);
            return p;
        },
        [this, window, close_dialog, build, install](Prepared p) {
            if (!p.error.empty() || p.up_to_date) {
                close_dialog();
                if (p.up_to_date) {
                    state.latest_tag = state.tag;
                    SaveState();
                }
                Finish(window, QString::fromStdString(p.error),
                       p.up_to_date ? tr("Pokémon Emerald 3DS Is Up to Date") : QString{});
                return;
            }
            if (p.kind == E::UpdateKind::Full) {
                build(std::move(p));
            } else {
                install(std::move(p));
            }
        });
}

// ---- AzaharFrame emhb: Uninstall ----

QString FrameEmerald::UninstallBody(bool has_save, bool delete_save) const {
    const auto copies = static_cast<size_t>(std::count_if(
        detected.found.begin(), detected.found.end(),
        [](const E::Found& f) { return f.piece == E::Piece::GameCopy; }));
    return QString::fromStdString(E::UninstallBody(has_save, delete_save, copies));
}

void FrameEmerald::Uninstall(QWidget* parent) {
    if (busy) {
        return;
    }
    Rescan();
    const Status st = GetStatus();
    if (!st.can_uninstall) {
        return;
    }
    // Material 3's confirmation: a short title, one or two sentences, Cancel and Uninstall (and
    // the save's checkbox, off)
    QMessageBox box(QMessageBox::NoIcon, tr("Uninstall Pokémon Emerald 3DS?"),
                    UninstallBody(st.has_save, false), QMessageBox::NoButton, parent);
    QCheckBox* save_box = nullptr;
    if (st.has_save) {
        save_box = new QCheckBox(tr("Also Delete Save Data"), &box);
        save_box->setChecked(false);
        box.setCheckBox(save_box);
        connect(save_box, &QCheckBox::toggled, &box,
                [this, &box](bool on) { box.setText(UninstallBody(true, on)); });
    }
    QPushButton* cancel = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    QPushButton* go = box.addButton(tr("Uninstall"), QMessageBox::AcceptRole);
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    box.exec();
    if (box.clickedButton() != go) {
        return;
    }
    QPointer<QWidget> window(parent);
    UninstallNow(save_box != nullptr && save_box->isChecked(), [window](const QString& error) {
        if (!window) {
            return;
        }
        if (error.isEmpty()) {
            QMessageBox::information(window, tr("Pokémon Emerald 3DS"),
                                     tr("Pokémon Emerald 3DS Uninstalled"));
        } else {
            QMessageBox::warning(window, tr("Pokémon Emerald 3DS"), error);
        }
    });
}

void FrameEmerald::UninstallNow(bool delete_save, std::function<void(QString)> done) {
    if (busy) {
        return;
    }
    Rescan();
    SetBusy(true, tr("Uninstalling"));
    // the plan hashes the game folders' copies (to find identical extras): off the UI thread too
    OffThread(
        this,
        [detection = detected, paths = paths, delete_save] {
            const E::UninstallPlan plan = E::PlanUninstall(detection, paths, delete_save);
            LOG_INFO(Frontend, "emerald3ds: uninstall: {} file(s), {} folder(s); kept: {} ({})",
                     plan.files.size(), plan.dirs.size(), plan.kept.size(),
                     delete_save ? "the save is deleted" : "the save stays");
            return E::RunUninstall(plan);
        },
        [this, done = std::move(done)](E::UninstallResult r) {
            for (const std::string& f : r.failed) {
                LOG_WARNING(Frontend, "emerald3ds: uninstall: could not remove {}",
                            QFileInfo(QDir::cleanPath(QString::fromStdString(f))).fileName().toStdString());
            }
            LOG_INFO(Frontend, "emerald3ds: uninstalled: {} removed, {} not", r.removed.size(),
                     r.failed.size());
            state = {};
            LoadState(); // gone, unless it could not be removed
            imported_latest.clear();
            imported_checked = 0;
            manual = {};
            Rescan();
            const QString error = r.Ok() ? QString{} : tr("Couldn't Remove Every File");
            SetBusy(false, error);
            emit Changed();
            FrameHomebrew::RefreshList()();
            if (done) {
                done(error);
            }
        });
}
