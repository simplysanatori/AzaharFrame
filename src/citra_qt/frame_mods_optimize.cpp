// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ui3: Optimize for Frame on the VR menu's pack page, FrameMods' part (frame_mods.h).
// The converter is the Flat game list's (frame_texture_pack.h, tools/convert-texture-pack.sh): it
// runs at the lowest CPU and disk priority (nice 19, ionice idle), with two encoder jobs while a game
// runs, and only reads the pack and writes its copy "<pack>-astc4x4" (never the folder the game
// reads). It goes on when the menu closes; Stop ends it, and Optimize carries on from there. When
// it is done, the copy becomes the game's pack at the next start (ApplyPendingPack, from the
// emulator's shutdown and boot), with the Customize choices carried over; Use Original does the
// same the other way.

#include <algorithm>
#include <filesystem>
#include <thread>

#include <QApplication>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <fmt/format.h>

#include "citra_qt/frame_mods.h"
#include "citra_qt/frame_texture_pack.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/custom_tex/optimize.h"
#include "core/custom_tex/packs.h"
#include "frame_xr/frame_xr.h"

namespace Menu = FrameXr::Menu;
namespace Opt = CustomTex::Optimize;

namespace {

/// Paths compare as the packs' ids do (CustomTex::Pack::id is the canonical path).
std::string Canon(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code ec;
    const auto c = std::filesystem::weakly_canonical(path, ec);
    return ec ? path : c.string();
}

/// As frame_mods.cpp's: `work` on a thread of its own, `done` on the GUI thread while `self` lives.
template <typename Work, typename Done>
void OffThread(FrameMods* self, Work work, Done done) {
    QPointer<FrameMods> guard(self);
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

std::string LoadDir() {
    std::string dir = FileUtil::GetUserPath(FileUtil::UserPath::LoadDir);
    while (dir.size() > 1 && dir.back() == '/') {
        dir.pop_back();
    }
    return dir;
}

} // namespace

FrameMods::~FrameMods() {
    if (optimize_process && optimize_process->state() != QProcess::NotRunning) {
        // A KTX cut off by a kill could look done to the next run: let the converter stop itself.
        optimize_process->disconnect();
        optimize_process->terminate();
        optimize_process->waitForFinished(5000);
    }
}

void FrameMods::SetGameRunningSource(std::function<bool()> source) {
    game_running_source = std::move(source);
}

const CustomTex::Pack* FrameMods::PackByPath(const std::string& path) const {
    const std::string c = Canon(path);
    for (const CustomTex::Pack& p : packs) {
        if (p.id == c || Canon(p.path) == c) {
            return &p;
        }
    }
    return nullptr;
}

void FrameMods::OptimizeAsk(const std::string& pack_id) {
    const auto it = std::find_if(packs.begin(), packs.end(),
                                 [&](const CustomTex::Pack& p) { return p.id == pack_id; });
    if (it == packs.end() || optimize_process || !optimize_checking.empty()) {
        return;
    }
    const Opt::Pair pair = Opt::PairOf(it->path, it->format == CustomTex::Format::Png);
    const QString script = FrameTexturePack::ToolPath(QStringLiteral("convert-texture-pack.sh"));
    if (pair.png.empty() || script.isEmpty()) {
        LOG_WARNING(Frontend, "xr: optimize: {} cannot be converted ({})", it->path,
                    script.isEmpty() ? "no converter" : "not a PNG pack");
        optimize_states[Canon(it->path)] = {Menu::OptimizeState::Failed, {}, 0, 0, -1, {},
                                            script.isEmpty() ? "Converter Missing" : "Not a PNG Pack"};
        UpdateOptimizeRow();
        PublishTextures();
        return;
    }
    const std::string png = Canon(pair.png);
    optimize_checking = png;
    UpdateOptimizeRow();
    PublishTextures();
    // How much there is, from the PNG headers (a second or two for a big pack), off this thread.
    auto* p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p, png](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
        p->deleteLater();
        optimize_checking.clear();
        const auto est = Opt::ParseEstimate(out.toStdString());
        if (code != 0 || !est) {
            LOG_WARNING(Frontend, "xr: optimize: the estimate of {} failed (exit {})", png, code);
            optimize_states[png] = {Menu::OptimizeState::Failed, {}, 0, 0, -1, {}, "Pack Not Readable"};
        } else {
            LOG_INFO(Frontend, "xr: optimize: {} has {} textures, about {} bytes as ASTC", png,
                     est->textures, est->total_bytes);
            pack_pending = {Pending::Kind::Optimize, png};
            textures.dialog = {Menu::ModsDialogKind::Optimize, Menu::CountWithCommas(est->textures),
                               "About " + Menu::SizeText(est->total_bytes)};
        }
        UpdateOptimizeRow();
        PublishTextures();
    });
    connect(p, &QProcess::errorOccurred, this, [this, p, png](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) {
            return;
        }
        p->deleteLater();
        optimize_checking.clear();
        optimize_states[png] = {Menu::OptimizeState::Failed, {}, 0, 0, -1, {}, "Converter Missing"};
        UpdateOptimizeRow();
        PublishTextures();
    });
    p->start(script, {QStringLiteral("--estimate"), QString::fromStdString(png)});
}

void FrameMods::OptimizeStart(const std::string& png) {
    if (optimize_process) {
        return;
    }
    const QString script = FrameTexturePack::ToolPath(QStringLiteral("convert-texture-pack.sh"));
    if (script.isEmpty()) {
        return;
    }
    optimize_png = png;
    optimize_title = title_id;
    optimize_stopping = false;
    optimize_output.clear();
    optimize_states.erase(png);
    optimize_progress = {Menu::OptimizeState::Converting};
    UpdateOptimizeRow();
    PublishTextures();
    // The encoder is built once (build-astcenc.sh, under a minute) before the first conversion.
    if (QProcess::execute(script, {QStringLiteral("--check")}) != Opt::kExitNoEncoder) {
        OptimizeConvert(png);
        return;
    }
    const QString build = FrameTexturePack::ToolPath(QStringLiteral("build-astcenc.sh"));
    if (build.isEmpty()) {
        OptimizeDone(Opt::kExitNoEncoder, false);
        return;
    }
    LOG_INFO(Frontend, "xr: optimize: building the encoder first");
    optimize_progress.phase = "Preparing the Encoder";
    UpdateOptimizeRow();
    PublishTextures();
    optimize_process = new QProcess(this);
    optimize_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(optimize_process, &QProcess::finished, this, [this, png](int code, QProcess::ExitStatus) {
        optimize_process->deleteLater();
        optimize_process = nullptr;
        if (code != 0 || optimize_stopping) {
            LOG_WARNING(Frontend, "xr: optimize: the encoder build ended with {}", code);
            OptimizeDone(optimize_stopping ? Opt::kExitStopped : Opt::kExitNoEncoder, optimize_stopping);
            return;
        }
        optimize_progress.phase.clear();
        OptimizeConvert(png);
    });
    optimize_process->start(build, {});
}

void FrameMods::OptimizeConvert(const std::string& png) {
    const QString script = FrameTexturePack::ToolPath(QStringLiteral("convert-texture-pack.sh"));
    // While a game runs, two textures at a time (the converter's own default is one less than the
    // cores): with nice 19 and idle I/O the game keeps its frame rate.
    const bool running = game_running_source && game_running_source();
    const int jobs = running ? 2 : std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    LOG_INFO(Frontend, "xr: optimize: converting {} ({} jobs)", png, jobs);
    optimize_process = new QProcess(this);
    connect(optimize_process, &QProcess::readyReadStandardOutput, this, [this] {
        bool changed = false;
        while (optimize_process && optimize_process->canReadLine()) {
            const std::string line = QString::fromUtf8(optimize_process->readLine()).trimmed().toStdString();
            if (const auto pr = Opt::ParseProgress(line)) {
                changed |= pr->permille != optimize_progress.permille || pr->done != optimize_progress.done;
                optimize_progress.done = pr->done;
                optimize_progress.total = pr->total;
                optimize_progress.permille = pr->permille;
            } else if (!line.empty()) {
                optimize_output = line; // the last line, for the log
            }
        }
        if (changed && !optimize_stopping) {
            UpdateOptimizeRow();
            PublishTextures();
        }
    });
    connect(optimize_process, &QProcess::readyReadStandardError, this, [this] {
        const QString err = QString::fromUtf8(optimize_process->readAllStandardError()).trimmed();
        if (!err.isEmpty()) {
            optimize_output = err.toStdString();
        }
    });
    connect(optimize_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        optimize_process->deleteLater();
        optimize_process = nullptr;
        OptimizeDone(code, optimize_stopping || code == Opt::kExitStopped);
    });
    connect(optimize_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || !optimize_process) {
            return;
        }
        optimize_process->deleteLater();
        optimize_process = nullptr;
        OptimizeDone(-1, false);
    });
    optimize_process->start(script, {QStringLiteral("--machine"), QStringLiteral("--jobs"),
                                      QString::number(jobs), QString::fromStdString(png)});
}

void FrameMods::OptimizeStop() {
    if (!optimize_process || optimize_stopping) {
        return;
    }
    LOG_INFO(Frontend, "xr: optimize: Stop");
    optimize_stopping = true;
    optimize_progress.phase = "Stopping";
    optimize_progress.permille = -1;
    UpdateOptimizeRow();
    PublishTextures();
    optimize_process->terminate(); // the converter removes its half-written file and exits 4
}

void FrameMods::OptimizeDone(int code, bool stopped) {
    const std::string png = std::exchange(optimize_png, {});
    const u64 tid = optimize_title;
    optimize_stopping = false;
    optimize_progress = {};
    if (stopped) {
        LOG_INFO(Frontend, "xr: optimize: {} stopped", png);
        optimize_states[png] = {Menu::OptimizeState::Stopped};
    } else if (code != 0) {
        LOG_WARNING(Frontend, "xr: optimize: {} failed (exit {}): {}", png, code, optimize_output);
        optimize_states[png] = {Menu::OptimizeState::Failed, {}, 0, 0, -1, {}, Opt::FailureText(code)};
    } else {
        const std::string copy = Canon(Opt::OptimizedPath(png));
        LOG_INFO(Frontend, "xr: optimize: {} done: {}", png, copy);
        CustomTex::ForgetSizes();
        if (Opt::IsOptimized(copy)) {
            ChoosePack(tid, copy, png);
        }
    }
    if (tid == title_id) {
        RefreshPacks(); // the copy shows in the list, and the row
    }
    UpdateOptimizeRow();
    PublishTextures();
}

void FrameMods::UseOriginal(const std::string& pack_id) {
    const auto it = std::find_if(packs.begin(), packs.end(),
                                 [&](const CustomTex::Pack& p) { return p.id == pack_id; });
    if (it == packs.end()) {
        return;
    }
    const Opt::Pair pair = Opt::PairOf(it->path, it->format == CustomTex::Format::Png);
    if (pair.png.empty() || pair.optimized.empty()) {
        return;
    }
    LOG_INFO(Frontend, "xr: optimize: Use Original ({})", pair.png);
    ChoosePack(title_id, Canon(pair.png), Canon(pair.optimized));
    RefreshPacks();
}

void FrameMods::ChoosePack(u64 tid, const std::string& path, const std::string& carry_from) {
    if (tid == 0) {
        return;
    }
    pending_switch = PendingSwitch{tid, Canon(path), Canon(carry_from)};
    if (game_running_source && game_running_source()) {
        // The running game reads its pack through load/textures/<TID> as it goes: switched when it stops.
        LOG_INFO(Frontend, "xr: optimize: {} from the next start", path);
        if (tid == title_id) {
            textures.needs_restart = true; // "Restart the Game to Apply"
        }
        return;
    }
    ApplyPendingPack();
}

void FrameMods::ApplyPendingPack() {
    if (!pending_switch) {
        return;
    }
    const PendingSwitch s = *std::exchange(pending_switch, std::nullopt);
    const CustomTex::Paths paths = CustomTex::DefaultPaths();
    for (const CustomTex::Pack& p : CustomTex::ListPacks(paths, s.title)) {
        if (p.id != s.path && Canon(p.path) != s.path) {
            continue;
        }
        if (!s.carry_from.empty() && !Opt::CarryParts(s.carry_from, p.path)) {
            LOG_WARNING(Frontend, "xr: optimize: the Customize choices of {} did not carry over",
                        s.carry_from);
        }
        const CustomTex::Result r = CustomTex::Activate(paths, s.title, p);
        LOG_INFO(Frontend, "xr: optimize: {:016X} uses {}{}", s.title, p.path,
                 r.ok ? "" : " (failed: " + r.error + ")");
        return;
    }
    LOG_WARNING(Frontend, "xr: optimize: {} is gone; the pack stays as it was", s.path);
}

void FrameMods::UpdateOptimizeRow() {
    Menu::PackOptimize& row = textures.parts.optimize;
    row = {};
    const CustomTex::Pack* page = nullptr;
    for (const CustomTex::Pack& p : packs) {
        if (p.id == textures.parts.pack_id) {
            page = &p;
        }
    }
    if (page == nullptr) {
        return;
    }
    const Opt::Pair pair = Opt::PairOf(page->path, page->format == CustomTex::Format::Png);
    const std::string png = Canon(pair.png), copy = Canon(pair.optimized);
    if (png.empty() && copy.empty()) {
        return;
    }
    // The one chosen: waiting for the next start, else the one load/textures/<TID> points at.
    std::string chosen;
    if (pending_switch && pending_switch->title == title_id) {
        chosen = pending_switch->path;
    } else {
        for (const CustomTex::Pack& p : packs) {
            if (p.active) {
                chosen = p.id;
            }
        }
    }
    const auto size_of = [this](const std::string& path) { return PackSizeText(path); };
    const bool page_is_copy = !copy.empty() && Canon(page->path) == copy;
    if (!png.empty() && optimize_png == png) {
        row = optimize_progress;
        row.state = Menu::OptimizeState::Converting;
    } else if (!png.empty() && optimize_checking == png) {
        row.state = Menu::OptimizeState::Checking;
    } else if (!copy.empty() && (chosen == copy || (page_is_copy && chosen != png))) {
        row.state = Menu::OptimizeState::Optimized;
        row.size_text = size_of(copy);
        // AzaharFrame ui3: Remove Original, unless the game still reads it (a switch waits for
        // the next start)
        row.original_removed = png.empty();
        if (!png.empty()) {
            row.original_size = size_of(png);
            const bool waiting = pending_switch && pending_switch->title == title_id;
            row.can_remove = !waiting && Canon(ActiveLink()) != png;
        }
        return;
    } else if (const auto it = optimize_states.find(png); !png.empty() && it != optimize_states.end()) {
        row = it->second;
    } else if (!png.empty()) {
        row.state = Menu::OptimizeState::Png;
    } else {
        return; // an optimized copy whose original is gone, not chosen: nothing to switch to
    }
    row.size_text = size_of(png);
}

std::string FrameMods::PackSizeText(const std::string& path) const {
    const CustomTex::Pack* p = PackByPath(path);
    const auto bytes = p && p->size_bytes ? p->size_bytes : CustomTex::CachedSize(path);
    return bytes ? Menu::SizeText(*bytes) : std::string{};
}

std::string FrameMods::ActiveLink() const {
    return (std::filesystem::path(LoadDir()) / "textures" / CustomTex::TitleFolder(title_id)).string();
}

void FrameMods::RemoveOriginalAsk(const std::string& pack_id) { // AzaharFrame ui3
    const auto it = std::find_if(packs.begin(), packs.end(),
                                 [&](const CustomTex::Pack& p) { return p.id == pack_id; });
    if (it == packs.end() || textures.busy) {
        return;
    }
    const Opt::Pair pair = Opt::PairOf(it->path, it->format == CustomTex::Format::Png);
    if (pair.png.empty() || pair.optimized.empty()) {
        return;
    }
    const Opt::RemoveCheck check =
        Opt::CheckRemoveOriginal(pair.optimized, Opt::ManagedRoots(LoadDir()), ActiveLink());
    if (!check.Ok()) {
        LOG_WARNING(Frontend, "xr: optimize: Remove Original refused for {}: {}", pair.png, check.error);
        FrameXr::ShowToastText(check.error, FrameXr::ToastTime::Long);
        return;
    }
    pack_pending = {Pending::Kind::RemoveOriginal, Canon(pair.optimized)};
    textures.dialog = {Menu::ModsDialogKind::RemoveOriginal,
                       Menu::RemoveOriginalBody(PackSizeText(pair.png), Opt::FromCatalog(check.original)),
                       ""};
    PublishTextures();
}

void FrameMods::RemoveOriginalDo(const std::string& optimized) {
    if (textures.busy) {
        return;
    }
    // The game reads the original until the waiting switch happens: never then.
    if (pending_switch && pending_switch->title == title_id) {
        FrameXr::ShowToastText("Original Is in Use", FrameXr::ToastTime::Long);
        return;
    }
    const std::string png = Canon(Opt::PairOf(optimized, false).png);
    textures.busy = true;
    PublishTextures();
    const std::vector<std::string> roots = Opt::ManagedRoots(LoadDir());
    const std::string active = ActiveLink();
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: optimize: Remove Original of {}", optimized);
    OffThread(
        this,
        [optimized, roots, active] {
            std::string error;
            return Opt::RemoveOriginal(optimized, roots, active, &error) ? std::string{} : error;
        },
        [this, tid, optimized, png](const std::string& error) {
            textures.busy = false;
            if (!error.empty()) {
                LOG_WARNING(Frontend, "xr: optimize: Remove Original failed: {}", error);
                FrameXr::ShowToastText(error, FrameXr::ToastTime::Long);
            }
            if (tid != title_id) {
                return;
            }
            RefreshPacks();
            ListPackDownloads(); // a catalog pack shows Download again
            if (error.empty() && textures.parts.pack_id == png) {
                OpenParts(optimized); // the page was the original's: the copy's now
            } else {
                UpdateOptimizeRow();
                PublishTextures();
            }
        });
}

void FrameMods::NudgeOptimizeActive(u64 tid) {
    nudge_active_title = tid;
    if (tid != 0 && tid == title_id && !packs.empty()) {
        CheckNudge();
        PublishTextures();
    }
}

void FrameMods::CheckNudge() {
    if (!textures.nudge_pack.empty()) {
        return;
    }
    // AzaharFrame integ3: too large to preload: the active pack, if it is PNG with no optimized copy
    if (nudge_active_title != 0 && nudge_active_title == title_id && !packs.empty()) {
        nudge_active_title = 0;
        for (const CustomTex::Pack& p : packs) {
            if (!p.active || p.format != CustomTex::Format::Png ||
                !Opt::PairOf(p.path, true).optimized.empty()) {
                continue;
            }
            LOG_INFO(Frontend, "xr: optimize: asking about {} (too large to preload)", p.path);
            textures.nudge_pack = p.id;
            textures.nudge_name = p.name;
            return;
        }
    }
    if (nudge_candidates.empty()) {
        return;
    }
    const std::string load = LoadDir();
    for (const CustomTex::Pack& p : packs) {
        const std::string folder = "folder:" + std::filesystem::path(p.path).filename().string();
        const bool by_id = nudge_candidates.count(p.id) != 0;
        if (!by_id && nudge_candidates.count(folder) == 0) {
            continue;
        }
        if (p.format != CustomTex::Format::Png) {
            nudge_candidates.erase(p.id);
            nudge_candidates.erase(folder);
            continue;
        }
        if (!p.size_bytes) {
            continue; // asked again once ComputeSizes knows it
        }
        nudge_candidates.erase(p.id);
        nudge_candidates.erase(folder);
        if (*p.size_bytes < Opt::kNudgeBytes || Opt::AskedBefore(load, p.id) ||
            !Opt::PairOf(p.path, true).optimized.empty()) {
            continue;
        }
        Opt::MarkAsked(load, p.id);
        LOG_INFO(Frontend, "xr: optimize: asking about {} ({} bytes)", p.path, *p.size_bytes);
        textures.nudge_pack = p.id;
        textures.nudge_name = p.name;
        return;
    }
}
