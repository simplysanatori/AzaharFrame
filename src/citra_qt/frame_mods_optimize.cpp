// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ui3: Optimize for Frame on the VR menu's pack page, FrameMods' part (frame_mods.h).
// The converter is the one the Flat game list runs (frame_texture_pack.h; core/custom_tex/convert.h,
// in the app: astcenc is linked in): worker threads of its own at the lowest CPU and disk priority
// (nice 19, idle I/O), two while a game runs, that only read the pack and write its copy
// "<pack>-astc4x4" (never the folder the game reads). It goes on when the menu closes; Stop ends it, and Optimize carries on from there. When
// it is done, the copy becomes the game's pack at the next start (ApplyPendingPack, from the
// emulator's shutdown and boot), with the Customize choices carried over (when it is chosen); Use
// Original does the same the other way. AzaharFrame texrow: a switch waits for each pair, and the
// menu shows them as done (the pack row, Enabled Textures) while the disk keeps what the game reads.

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <thread>

#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <fmt/format.h>

#include "citra_qt/frame_mods.h"
#include "citra_qt/frame_texture_pack.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/custom_tex/convert.h"
#include "core/custom_tex/layers.h" // AzaharFrame texlayers
#include "core/custom_tex/optimize.h"
#include "core/custom_tex/packs.h"
#include "core/custom_tex/download.h" // AzaharFrame fixes7
#include "core/mods/library.h" // AzaharFrame fixes7
#include "frame_xr/frame_xr.h"

namespace Menu = FrameXr::Menu;
namespace Opt = CustomTex::Optimize;
namespace Conv = CustomTex::Convert;

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
    // AzaharFrame fixes7: quitting mid-download stops it at once (the socket is shut down, nothing
    // waits for a timeout); the partial download stays and its row offers Resume next time
    if (downloader) {
        downloader->Cancel();
    }
    if (!installing.empty()) {
        Mods::Default().Cancel();
    }
    if (optimize_converter) {
        // A KTX cut off by a kill could look done to the next run: the converter stops itself
        // (the texture in hand is abandoned, the done ones stay).
        optimize_converter->Stop();
    }
    if (optimize_thread.joinable()) {
        optimize_thread.join();
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
    if (it == packs.end() || optimize_converter || !optimize_checking.empty()) {
        return;
    }
    const Opt::Pair pair = Opt::PairOf(it->path, it->format == CustomTex::Format::Png);
    if (pair.png.empty()) {
        LOG_WARNING(Frontend, "xr: optimize: {} cannot be converted (not a PNG pack)", it->path);
        optimize_states[Canon(it->path)] = {Menu::OptimizeState::Failed, {}, 0, 0, -1, {}, "Not a PNG Pack"};
        UpdateOptimizeRow();
        PublishTextures();
        return;
    }
    const std::string png = Canon(pair.png);
    optimize_checking = png;
    UpdateOptimizeRow();
    PublishTextures();
    // How much there is, from the PNG headers (a second or two for a big pack), off this thread.
    struct Estimated {
        std::optional<Conv::Estimate> estimate;
        std::string error;
    };
    OffThread(
        this,
        [png] {
            Estimated e;
            Conv::Request request;
            request.pack = png;
            e.estimate = Conv::EstimatePack(request, &e.error);
            return e;
        },
        [this, png](const Estimated& e) {
            optimize_checking.clear();
            if (!e.estimate) {
                LOG_WARNING(Frontend, "xr: optimize: the estimate of {} failed: {}", png, e.error);
                optimize_states[png] = {Menu::OptimizeState::Failed, {}, 0, 0, -1, {}, "Pack Not Readable"};
            } else {
                LOG_INFO(Frontend, "xr: optimize: {} has {} textures, about {} bytes as ASTC", png,
                         e.estimate->textures, e.estimate->total_bytes);
                pack_pending = {Pending::Kind::Optimize, png};
                textures.dialog = {Menu::ModsDialogKind::Optimize, Menu::CountWithCommas(e.estimate->textures),
                                   "About " + Menu::SizeText(e.estimate->total_bytes)};
            }
            UpdateOptimizeRow();
            PublishTextures();
        });
}

void FrameMods::OptimizeStart(const std::string& png) {
    if (optimize_converter) {
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

    // While a game runs, two textures at a time (the default is one less than the cores): at
    // nice 19 and idle I/O the game keeps its frame rate.
    const bool running = game_running_source && game_running_source();
    const int jobs = running ? 2 : std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    LOG_INFO(Frontend, "xr: optimize: converting {} ({} jobs)", png, jobs);

    // The worker's progress, handed to the GUI thread at most once per its turn
    struct Latest {
        std::mutex mutex;
        Conv::Progress progress;
        bool queued = false;
    };
    auto latest = std::make_shared<Latest>();
    QPointer<FrameMods> guard(this);
    Conv::Request request;
    request.pack = png;
    request.jobs = jobs;
    auto converter = std::make_shared<Conv::Converter>(
        request, [guard, latest](const Conv::Progress& progress) {
            std::lock_guard lock(latest->mutex);
            latest->progress = progress;
            if (latest->queued) {
                return;
            }
            latest->queued = true;
            QMetaObject::invokeMethod(
                qApp,
                [guard, latest] {
                    Conv::Progress p;
                    {
                        std::lock_guard lock(latest->mutex);
                        p = latest->progress;
                        latest->queued = false;
                    }
                    if (!guard || !guard->optimize_converter || guard->optimize_stopping) {
                        return;
                    }
                    FrameXr::Menu::PackOptimize& row = guard->optimize_progress;
                    if (p.permille == row.permille && p.done == row.done) {
                        return;
                    }
                    row.done = p.done;
                    row.total = p.total;
                    row.permille = p.permille;
                    row.done_bytes = p.done_bytes; // AzaharFrame fixes7: "3.0 of 6.1 GB · 50%"
                    row.total_bytes = p.total_bytes;
                    guard->UpdateOptimizeRow();
                    guard->PublishTextures();
                },
                Qt::QueuedConnection);
        });
    optimize_converter = converter;
    if (optimize_thread.joinable()) {
        optimize_thread.join(); // the last run's, already finished
    }
    optimize_thread = std::thread([guard, converter] {
        Conv::LowerThreadPriority(); // the folders and links it makes too
        Conv::Result result = converter->Run();
        QMetaObject::invokeMethod(
            qApp,
            [guard, result = std::move(result)] {
                if (!guard) {
                    return;
                }
                guard->optimize_converter.reset();
                for (const auto& [rel, why] : result.kept_as_png) {
                    LOG_WARNING(Frontend, "xr: optimize: kept as PNG: {}: {}", rel, why);
                }
                guard->optimize_output = result.message;
                guard->OptimizeDone(result.code, guard->optimize_stopping || result.code == Conv::kStopped);
            },
            Qt::QueuedConnection);
    });
}

void FrameMods::OptimizeStop() {
    if (!optimize_converter || optimize_stopping) {
        return;
    }
    LOG_INFO(Frontend, "xr: optimize: Stop");
    optimize_stopping = true;
    optimize_progress.phase = "Stopping";
    optimize_progress.permille = -1;
    UpdateOptimizeRow();
    PublishTextures();
    optimize_converter->Stop(); // removes its half-written files and ends with kStopped
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

const FrameMods::PendingSwitch* FrameMods::PendingFor(u64 tid, const std::string& path) const {
    const std::string c = Canon(path);
    for (const PendingSwitch& s : pending_switches) {
        if (s.title == tid && !c.empty() && (s.path == c || s.carry_from == c)) {
            return &s;
        }
    }
    return nullptr;
}

void FrameMods::DropPending(u64 tid, const std::string& path) {
    const std::string c = Canon(path);
    std::erase_if(pending_switches, [&](const PendingSwitch& s) {
        return s.title == tid && !c.empty() && (s.path == c || s.carry_from == c);
    });
    if (layers && tid == title_id) {
        std::erase_if(layers->waiting, [&](const CustomTex::Layers::Switch& s) {
            return s.from == c || s.to == c;
        });
    }
}

void FrameMods::ChoosePack(u64 tid, const std::string& path, const std::string& carry_from) {
    if (tid == 0) {
        return;
    }
    // AzaharFrame texrow: the Customize choices go to the chosen copy now (the game reads only its
    // own copy's, at boot), so what is changed on its page while the switch waits stays
    if (!carry_from.empty() && !Opt::CarryParts(carry_from, path)) {
        LOG_WARNING(Frontend, "xr: optimize: the Customize choices of {} did not carry over",
                    carry_from);
    }
    // One switch a pair (texrow: the switches of other pairs stay)
    DropPending(tid, path);
    DropPending(tid, carry_from);
    pending_switches.push_back(PendingSwitch{tid, Canon(path), Canon(carry_from)});
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
    const std::vector<PendingSwitch> all = std::exchange(pending_switches, {});
    if (layers) {
        layers->waiting.clear(); // AzaharFrame texrow: the disk has them now; Save keeps them
    }
    const CustomTex::Paths paths = CustomTex::DefaultPaths();
    for (const PendingSwitch& s : all) { // AzaharFrame texrow: every pair that switches
        bool found = false;
        for (const CustomTex::Pack& p : CustomTex::ListPacks(paths, s.title)) {
            if (p.id != s.path && Canon(p.path) != s.path) {
                continue;
            }
            found = true;
            // AzaharFrame texlayers: in its pair's place in the game's list (on top when the pair
            // was off); the Customize choices were carried when it was chosen (ChoosePack)
            namespace L = CustomTex::Layers;
            L::State list = L::Current(paths, s.title);
            L::ApplySwitch(list, {s.carry_from, p.id});
            const CustomTex::Result r = L::Apply(paths, s.title, list);
            LOG_INFO(Frontend, "xr: optimize: {:016X} uses {}{}", s.title, p.path,
                     r.ok ? "" : " (failed: " + r.error + ")");
            break;
        }
        if (!found) {
            LOG_WARNING(Frontend, "xr: optimize: {} is gone; the pack stays as it was", s.path);
        }
    }
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
    // The one chosen: waiting for the next start, else the one of the pair that is on
    // (AzaharFrame texlayers: other packs may be on too).
    std::string chosen;
    const PendingSwitch* waiting_switch = copy.empty() ? nullptr : PendingFor(title_id, copy);
    if (!waiting_switch && !png.empty()) {
        waiting_switch = PendingFor(title_id, png);
    }
    if (waiting_switch) { // AzaharFrame texrow: this pair's
        chosen = waiting_switch->path;
    } else {
        for (const CustomTex::Pack& p : packs) {
            if (p.active && (p.id == png || p.id == copy || Canon(p.path) == png ||
                             Canon(p.path) == copy)) {
                chosen = Canon(p.path);
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
            row.can_remove = !waiting_switch && Canon(ActiveLink()) != png;
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
    const std::string png = Canon(Opt::PairOf(optimized, false).png);
    if (PendingFor(title_id, optimized) || (!png.empty() && PendingFor(title_id, png))) {
        FrameXr::ShowToastText("Original Is in Use", FrameXr::ToastTime::Long);
        return;
    }
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
