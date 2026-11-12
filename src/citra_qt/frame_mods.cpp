// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "citra_qt/frame_mods.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>
#include <utility>

#include <QApplication>
#include <QDesktopServices>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <fmt/format.h>

#include "common/logging/log.h"
#include "core/cheats/online.h"
#include "core/custom_tex/download.h"
#include "core/mods/library.h"
#include "frame_xr/frame_xr.h"

namespace Menu = FrameXr::Menu;

namespace {

/// `work` on a thread of its own, then `done` with its result on the GUI thread (if `self` is
/// still there).
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

/// What an install is doing, as the row's supporting line says it.
std::string PhaseText(Mods::State::Phase phase) {
    switch (phase) {
    case Mods::State::Phase::Resolving:
        return "Connecting";
    case Mods::State::Phase::Downloading:
        return "Downloading";
    case Mods::State::Phase::Extracting:
        return "Unpacking";
    case Mods::State::Phase::Installing:
        return "Installing";
    default:
        return {};
    }
}

/// Whether Steam runs: ~/.steam/steam.pid names a live process.
bool SteamRunning() {
    const char* home = std::getenv("HOME");
    if (!home) {
        return false;
    }
    std::ifstream pid_file(std::string(home) + "/.steam/steam.pid");
    long pid = 0;
    if (!(pid_file >> pid) || pid <= 0) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists("/proc/" + std::to_string(pid), ec);
}

/// The author's repository from the catalog entry ("https://github.com/owner/repo"), else from
/// what the library recorded at install; "" when it is not a GitHub repository.
std::string RepoUrl(const Mods::Catalog& catalog, const std::string& id,
                    const std::string& source = {}) {
    std::string url;
    if (const Mods::CatalogEntry* e = catalog.Find(id)) {
        const bool owner = e->source.type == Mods::Source::Type::OwnerRepoPath || e->source.repo.empty();
        url = std::string("https://github.com/") + (owner ? Mods::kOwnerRepo : e->source.repo);
    } else if (source.rfind("github.com/", 0) == 0) {
        url = "https://" + source;
    }
    return Menu::GithubLinkText(url).empty() ? std::string{} : url;
}

Menu::ModRow ToRow(const Mods::ModInfo& info, const Mods::Catalog& catalog) {
    Menu::ModRow r;
    r.id = info.id;
    r.name = info.name;
    r.verified = info.verified;
    r.managed = info.managed;
    r.active = info.state == Mods::ModState::Active;
    if (info.managed) {
        r.author = info.author;
        r.url = RepoUrl(catalog, info.id, info.source);
        r.detail = info.version;
    } else if (info.files.size() > 1) {
        r.detail = fmt::format("{} Files", info.files.size()); // your own: what is there
    }
    return r;
}

std::string PackDetail(const CustomTex::Pack& p) {
    return Menu::JoinDetail({p.author, p.version, CustomTex::FormatName(p.format),
                             p.size_bytes ? Menu::SizeText(*p.size_bytes) : std::string{}});
}

} // namespace

FrameMods::FrameMods(QObject* parent) : QObject(parent) {
    progress = new QTimer(this);
    connect(progress, &QTimer::timeout, this, [this] { PollProgress(); });
    download_poll = new QTimer(this);
    connect(download_poll, &QTimer::timeout, this, [this] { PollDownload(); });
}

FrameMods::~FrameMods() = default;

void FrameMods::Open(u64 title_id_, int run_) {
    if (title_id_ != title_id || run_ != run) {
        // Another game, or the same one started again: what waited for a restart has applied.
        title_id = title_id_;
        run = run_;
        mods = {};
        textures = {};
        packs.clear();
        failed.clear();
        pending = {};
        pack_pending = {};
        pack_failed.clear();
        pack_error.clear();
        PublishMods();
        PublishTextures();
    }
    if (title_id == 0) {
        return;
    }
    RefreshLoaded();
    RefreshPacks();
    if (!downloader) {
        downloader = std::make_shared<CustomTex::Download::Downloader>();
    }
    ListPackDownloads(); // which packs it brought (Delete), and a download that still runs
    if (mods.catalog == Menu::CatalogState::Ready) {
        RefreshAvailable();
    }
}

void FrameMods::Command(const FrameXr::MenuRequest& r) {
    using C = FrameXr::MenuCommand;
    if (title_id == 0) {
        return;
    }
    switch (r.command) {
    case C::SelectTexturePack:
        SelectPack(r.text);
        return;
    case C::LoadModCatalog:
        if (mods.catalog == Menu::CatalogState::Idle) {
            mods.catalog = Menu::CatalogState::Loading;
            PublishMods();
        }
        RefreshAvailable();
        return;
    case C::InstallMod:
        Install(r.text, false);
        return;
    case C::SetModActive:
        if (r.value != 0) {
            Install(r.text, false); // from the library: no download
        } else {
            Deactivate(r.text);
        }
        return;
    case C::RemoveMod: {
        const auto it = std::find_if(mods.loaded.begin(), mods.loaded.end(),
                                     [&r](const Menu::ModRow& m) { return m.id == r.text; });
        if (it == mods.loaded.end() || mods.busy) {
            return;
        }
        if (it->managed) {
            Remove(r.text); // downloaded again from its author with Install Mods
            return;
        }
        // Your own files: only you have them, so ask.
        const Mods::Library& lib = Mods::Default();
        std::string files;
        for (const Mods::ModInfo& info : lib.ListLoaded(title_id)) {
            if (info.id == r.text) {
                for (std::size_t i = 0; i < info.files.size() && i < 3; i++) {
                    files += (files.empty() ? "" : ", ") + info.files[i];
                }
                if (info.files.size() > 3) {
                    files += fmt::format(" and {} More", info.files.size() - 3);
                }
            }
        }
        pending = {Pending::Kind::Remove, r.text};
        mods.dialog = {Menu::ModsDialogKind::Remove, it->name, files};
        PublishMods();
        return;
    }
    case C::ModsDialog: {
        const Pending p = std::exchange(pending, {});
        const Menu::ModsDialogKind kind = mods.dialog.kind;
        mods.dialog = {};
        PublishMods();
        if (r.value == 0 || kind == Menu::ModsDialogKind::Blocked) {
            return;
        }
        if (p.kind == Pending::Kind::Install) {
            Install(p.id, true);
        } else if (p.kind == Pending::Kind::Remove) {
            Remove(p.id);
        }
        return;
    }
    case C::LoadPackDownloads:
        LoadPackDownloads();
        return;
    case C::DownloadPack: {
        const Menu::PackOffer* o = PackOffer(r.text);
        if (!o || textures.downloading || !downloader) {
            return;
        }
        const auto size = offer_sizes.find(r.text);
        if (size != offer_sizes.end() && size->second > 1'000'000'000) { // a big one: say how big first
            pack_pending = {Pending::Kind::Download, r.text};
            textures.dialog = {Menu::ModsDialogKind::BigDownload, o->name, o->size_text};
            PublishTextures();
            return;
        }
        StartDownload(r.text);
        return;
    }
    case C::CancelPackDownload:
        if (downloader && textures.downloading) {
            LOG_INFO(Frontend, "xr: texture packs: download cancelled");
            downloader->Cancel();
        }
        return;
    case C::DeletePack: {
        const auto it = std::find_if(packs.begin(), packs.end(),
                                     [&r](const CustomTex::Pack& p) { return p.id == r.text; });
        if (it == packs.end() || textures.busy || textures.downloading) {
            return;
        }
        pack_pending = {Pending::Kind::Delete,
                        std::filesystem::path(it->path).filename().string()};
        textures.dialog = {Menu::ModsDialogKind::DeletePack, it->name, {}};
        PublishTextures();
        return;
    }
    case C::TexturesDialog: {
        const Pending p = std::exchange(pack_pending, {});
        textures.dialog = {};
        PublishTextures();
        if (r.value == 0) {
            return;
        }
        if (p.kind == Pending::Kind::Download) {
            StartDownload(p.id);
        } else if (p.kind == Pending::Kind::Delete) {
            DeletePack(p.id);
        }
        return;
    }
    case C::OpenUrl: {
        // Only a page the menu showed: a mod's GitHub repository or a pack author's page from
        // the catalog (the menu asked "Open <host>?" first).
        bool known = !Menu::GithubLinkText(r.text).empty();
        for (const Menu::PackOffer& o : textures.downloads) {
            known |= o.url == r.text;
        }
        if (known && !Menu::LinkText(r.text).empty()) {
            // Steam's own browser, as other apps on the Frame show pages in the headset; the
            // desktop's browser only when Steam is not running.
            const QString url = QString::fromStdString(r.text);
            if (SteamRunning() &&
                QProcess::startDetached(QStringLiteral("steam"),
                                        {QStringLiteral("steam://openurl/") + url})) {
                LOG_INFO(Frontend, "xr: mods: opening {} in Steam", r.text);
            } else {
                LOG_INFO(Frontend, "xr: mods: opening {} in the browser", r.text);
                QDesktopServices::openUrl(QUrl(url));
            }
        }
        return;
    }
    default:
        return;
    }
}

void FrameMods::TextureSettingChanged() {
    textures.needs_restart = true;
    PublishTextures();
}

// ---- Mods ----

void FrameMods::RefreshLoaded() {
    const u64 tid = title_id;
    OffThread(
        this, [tid] { return Mods::Default().ListLoaded(tid); },
        [this, tid](const std::vector<Mods::ModInfo>& list) {
            if (tid != title_id) {
                return;
            }
            mods.loaded.clear();
            const Mods::Catalog catalog = Mods::Default().GetCatalog();
            for (const Mods::ModInfo& info : list) {
                mods.loaded.push_back(ToRow(info, catalog));
            }
            PublishMods();
        });
}

void FrameMods::RefreshAvailable() {
    const u64 tid = title_id;
    OffThread(
        this,
        [tid] {
            Mods::Library& lib = Mods::Default();
            lib.RefreshCatalogOnce(); // one request a run; the built-in catalog without it
            return lib.ListAvailable(tid);
        },
        [this, tid](const std::vector<Mods::AvailableMod>& list) {
            if (tid != title_id) {
                return;
            }
            mods.available.clear();
            const Mods::Catalog catalog = Mods::Default().GetCatalog();
            for (const Mods::AvailableMod& a : list) {
                Menu::ModOffer o;
                o.id = a.id;
                o.name = a.name;
                o.author = a.author;
                o.url = RepoUrl(catalog, a.id);
                o.verified = a.verified;
                if (a.id == installing) {
                    o.state = Menu::OfferState::Installing;
                    o.note = PhaseText(Mods::Default().GetState().phase);
                } else if (a.active) {
                    o.state = Menu::OfferState::Installed;
                } else if (!a.installable) {
                    o.state = Menu::OfferState::Unavailable;
                    o.note = "Unsupported Archive";
                } else if (a.id == failed) {
                    o.note = "Install Failed";
                    o.error = true;
                }
                mods.available.push_back(std::move(o));
            }
            mods.catalog = Menu::CatalogState::Ready;
            PublishMods();
        });
}

Menu::ModOffer* FrameMods::Offer(const std::string& id) {
    for (Menu::ModOffer& o : mods.available) {
        if (o.id == id) {
            return &o;
        }
    }
    return nullptr;
}

void FrameMods::Install(const std::string& id, bool replace) {
    if (mods.busy || id.empty()) {
        return;
    }
    mods.busy = true;
    installing = id;
    failed.clear();
    if (Menu::ModOffer* o = Offer(id)) {
        o->state = Menu::OfferState::Installing;
        o->note = PhaseText(Mods::State::Phase::Resolving);
        o->error = false;
    }
    PublishMods();
    progress->start(250);
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: mods: install {}{}", id, replace ? " (replace)" : "");
    OffThread(
        this, [tid, id, replace] { return Mods::Default().Install(tid, id, replace); },
        [this, tid, id](const Mods::InstallOutcome& out) {
            progress->stop();
            installing.clear();
            mods.busy = false;
            if (tid != title_id) {
                return;
            }
            std::string name = id;
            if (const Menu::ModOffer* o = Offer(id)) {
                name = o->name;
            } else {
                for (const Menu::ModRow& r : mods.loaded) {
                    if (r.id == id) {
                        name = r.name;
                    }
                }
            }
            switch (out.result) {
            case Mods::InstallResult::Ok:
                mods.needs_restart = true;
                break;
            case Mods::InstallResult::AlreadyInstalled:
                break;
            case Mods::InstallResult::Conflict:
                // Never replaced silently: ask, or say what has to go first.
                LOG_INFO(Frontend, "xr: mods: {} conflicts with {} ({})", id, out.conflict_id,
                         out.conflict_path);
                if (out.conflict_unmanaged) {
                    mods.dialog = {Menu::ModsDialogKind::Blocked, name, out.conflict_name};
                } else {
                    pending = {Pending::Kind::Install, id};
                    mods.dialog = {Menu::ModsDialogKind::Replace, name, out.conflict_name};
                }
                break;
            case Mods::InstallResult::NotAvailable:
            case Mods::InstallResult::Error:
                LOG_WARNING(Frontend, "xr: mods: install {} failed: {}", id, out.error);
                failed = id;
                break;
            }
            RefreshLoaded();
            RefreshAvailable();
            PublishMods();
        });
}

void FrameMods::PollProgress() {
    Menu::ModOffer* o = Offer(installing);
    if (!o) {
        return;
    }
    const std::string text = PhaseText(Mods::Default().GetState().phase);
    if (!text.empty() && text != o->note) {
        o->note = text;
        PublishMods();
    }
}

void FrameMods::Deactivate(const std::string& id) {
    if (mods.busy) {
        return;
    }
    mods.busy = true;
    PublishMods();
    const u64 tid = title_id;
    OffThread(
        this, [tid, id] { return Mods::Default().Deactivate(tid, id); },
        [this, tid, id](const Mods::RemoveOutcome& out) {
            mods.busy = false;
            if (tid != title_id) {
                return;
            }
            if (out.ok) {
                mods.needs_restart = true;
            } else {
                LOG_WARNING(Frontend, "xr: mods: turning off {} failed: {}", id, out.error);
            }
            RefreshLoaded();
            if (mods.catalog == Menu::CatalogState::Ready) {
                RefreshAvailable();
            }
            PublishMods();
        });
}

void FrameMods::Remove(const std::string& id) {
    if (mods.busy) {
        return;
    }
    mods.busy = true;
    PublishMods();
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: mods: remove {}", id);
    OffThread(
        this, [tid, id] { return Mods::Default().Remove(tid, id); },
        [this, tid, id](const Mods::RemoveOutcome& out) {
            mods.busy = false;
            if (tid != title_id) {
                return;
            }
            if (out.ok) {
                mods.needs_restart = true;
            } else {
                LOG_WARNING(Frontend, "xr: mods: remove {} failed: {}", id, out.error);
            }
            RefreshLoaded();
            if (mods.catalog == Menu::CatalogState::Ready) {
                RefreshAvailable();
            }
            PublishMods();
        });
}

void FrameMods::PublishMods() {
    FrameXr::SetInGameMenuModsList(mods);
}

// ---- Textures ----

void FrameMods::RefreshPacks() {
    const u64 tid = title_id;
    OffThread(
        this, [tid] { return CustomTex::ListPacks(CustomTex::DefaultPaths(), tid); },
        [this, tid](const std::vector<CustomTex::Pack>& list) {
            if (tid != title_id) {
                return;
            }
            packs = list;
            textures.packs.clear();
            for (const CustomTex::Pack& p : packs) {
                const bool mine =
                    p.managed &&
                    downloaded.count(std::filesystem::path(p.path).filename().string()) != 0;
                textures.packs.push_back({p.id, p.name, PackDetail(p), p.active, mine});
            }
            PublishTextures();
            ComputeSizes();
        });
}

void FrameMods::ComputeSizes() {
    std::vector<std::string> paths;
    for (const CustomTex::Pack& p : packs) {
        if (!p.size_bytes) {
            paths.push_back(p.path);
        }
    }
    if (paths.empty() || sizing) {
        return;
    }
    sizing = true;
    // Seconds for a pack of many GB: the list shows first, the sizes when they are known.
    OffThread(
        this,
        [paths] {
            for (const std::string& path : paths) {
                CustomTex::ComputeSize(path);
            }
            return true;
        },
        [this](bool) {
            sizing = false;
            for (std::size_t i = 0; i < packs.size() && i < textures.packs.size(); i++) {
                packs[i].size_bytes = CustomTex::CachedSize(packs[i].path);
                textures.packs[i].detail = PackDetail(packs[i]);
            }
            PublishTextures();
        });
}

void FrameMods::SelectPack(const std::string& id) {
    if (textures.busy) {
        return;
    }
    std::optional<CustomTex::Pack> pack;
    if (!id.empty()) {
        for (const CustomTex::Pack& p : packs) {
            if (p.id == id) {
                pack = p;
            }
        }
        if (!pack) {
            return;
        }
    }
    textures.busy = true;
    for (Menu::TexturePackItem& p : textures.packs) {
        p.active = p.id == id; // at once; the list is read again after
    }
    PublishTextures();
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: texture packs: {}", pack ? pack->name : "Off");
    OffThread(
        this,
        [tid, pack] {
            const CustomTex::Paths paths = CustomTex::DefaultPaths();
            return pack ? CustomTex::Activate(paths, tid, *pack) : CustomTex::Deactivate(paths, tid);
        },
        [this, tid](const CustomTex::Result& out) {
            textures.busy = false;
            if (tid != title_id) {
                return;
            }
            if (out.ok) {
                textures.needs_restart = true;
            } else {
                LOG_WARNING(Frontend, "xr: texture packs: switch failed: {}", out.error);
                FrameXr::ShowToastText("Texture Pack Not Changed");
            }
            RefreshPacks();
        });
}

void FrameMods::PublishTextures() {
    FrameXr::SetInGameMenuTextures(textures);
}

// ---- Download Packs ----

void FrameMods::LoadPackDownloads() {
    if (!downloader) {
        downloader = std::make_shared<CustomTex::Download::Downloader>();
    }
    ListPackDownloads();
    if (catalog_fetched) {
        return;
    }
    catalog_fetched = true;
    // The catalog's latest copy: one request a run, the compiled-in copy without it.
    const auto dl = downloader;
    OffThread(
        this,
        [dl] {
            namespace D = CustomTex::Download;
            const auto r = Cheats::Online::HttpsGetRaw(
                "https://raw.githubusercontent.com",
                fmt::format("/{}/main/{}", D::kOwnerRepo, D::kCatalogPath), 1024 * 1024);
            if (r.status != 200 || !r.error.empty()) {
                return false;
            }
            auto parsed = D::ParseCatalog(r.body);
            if (!parsed || parsed->packs.empty()) {
                return false;
            }
            dl->SetCatalog(std::move(*parsed));
            return true;
        },
        [this](bool updated) {
            if (updated) {
                ListPackDownloads();
            }
        });
}

void FrameMods::ListPackDownloads() {
    if (!downloader) {
        return;
    }
    const auto dl = downloader;
    const u64 tid = title_id;
    OffThread(
        this, [dl, tid] { return dl->ListForTitle(tid); },
        [this, tid](const std::vector<CustomTex::Download::PackInfo>& list) {
            if (tid == title_id) {
                SetOffers(list);
            }
        });
}

void FrameMods::SetOffers(const std::vector<CustomTex::Download::PackInfo>& list) {
    const CustomTex::Download::State st = downloader->GetState();
    const bool running = st.Busy() && download_title == title_id;
    std::vector<Menu::PackOffer> offers;
    bool new_folders = false;
    offer_sizes.clear();
    for (const CustomTex::Download::PackInfo& p : list) {
        offer_sizes[p.id] = p.size;
        for (const std::string& f : p.folders) {
            new_folders |= downloaded.insert(f).second;
        }
        Menu::PackOffer o;
        o.id = p.id;
        o.name = p.name;
        o.author = p.author;
        o.url = Menu::LinkText(p.author_url).empty() ? std::string{} : p.author_url;
        o.size_text = p.size_text;
        if (running && st.pack_id == p.id) {
            o.state = Menu::PackOfferState::Downloading;
        } else if (p.installed) {
            o.state = Menu::PackOfferState::Installed;
        } else if (p.id == pack_failed) {
            o.state = Menu::PackOfferState::Failed;
            o.note = pack_error;
        }
        offers.push_back(std::move(o));
    }
    textures.downloads = std::move(offers);
    textures.downloading = running;
    if (running) {
        PollDownload(); // the percent at once
    }
    PublishTextures();
    if (new_folders) {
        RefreshPacks(); // Delete on the packs it brought
    }
}

Menu::PackOffer* FrameMods::PackOffer(const std::string& id) {
    for (Menu::PackOffer& o : textures.downloads) {
        if (o.id == id) {
            return &o;
        }
    }
    return nullptr;
}

void FrameMods::StartDownload(const std::string& id) {
    Menu::PackOffer* o = PackOffer(id);
    if (!o || !downloader || textures.downloading) {
        return;
    }
    if (!downloader->Start(id, title_id)) { // returns at once; the work runs on its thread
        LOG_WARNING(Frontend, "xr: texture packs: download {} did not start", id);
        return;
    }
    LOG_INFO(Frontend, "xr: texture packs: download {}", id);
    download_title = title_id;
    pack_failed.clear();
    pack_error.clear();
    o->state = Menu::PackOfferState::Downloading;
    o->percent = -1;
    o->note = "Connecting";
    textures.downloading = true;
    PublishTextures();
    download_poll->start(250);
}

void FrameMods::PollDownload() {
    if (!downloader) {
        download_poll->stop();
        return;
    }
    using Phase = CustomTex::Download::State::Phase;
    const CustomTex::Download::State st = downloader->GetState();
    if (st.Busy()) {
        if (download_title != title_id) {
            return; // another game's download: nothing of it shows
        }
        if (Menu::PackOffer* o = PackOffer(st.pack_id)) {
            Menu::PackOffer next = *o;
            next.state = Menu::PackOfferState::Downloading;
            next.note = st.text;
            next.percent = st.phase == Phase::Downloading && st.bytes_total > 0
                               ? static_cast<int>(std::min<u64>(
                                     st.bytes_done * 100 / st.bytes_total, 100))
                               : -1;
            if (!(next == *o)) {
                *o = std::move(next);
                PublishTextures();
            }
        }
        return;
    }
    download_poll->stop();
    textures.downloading = false;
    if (download_title != title_id) {
        return;
    }
    switch (st.phase) {
    case Phase::Done:
        LOG_INFO(Frontend, "xr: texture packs: {} downloaded", st.pack_id);
        for (const std::string& f : st.installed) {
            downloaded.insert(f);
        }
        RefreshPacks();
        break;
    case Phase::Failed:
        LOG_WARNING(Frontend, "xr: texture packs: download {} failed: {}", st.pack_id, st.error);
        pack_failed = st.pack_id;
        pack_error = st.error.empty() ? std::string("Download Failed") : st.error;
        break;
    default: // Cancelled: the partial download is kept, Download goes on from there
        break;
    }
    ListPackDownloads();
}

void FrameMods::DeletePack(const std::string& folder) {
    if (!downloader || textures.busy || textures.downloading) {
        return;
    }
    bool was_active = false;
    for (const CustomTex::Pack& p : packs) {
        was_active |= p.active && p.managed &&
                      std::filesystem::path(p.path).filename().string() == folder;
    }
    textures.busy = true;
    PublishTextures();
    const auto dl = downloader;
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: texture packs: delete {}", folder);
    OffThread(
        this, [dl, tid, folder] { return dl->Delete(tid, folder); },
        [this, tid, folder, was_active](const CustomTex::Download::Result& out) {
            textures.busy = false;
            if (tid != title_id) {
                return;
            }
            if (out.ok) {
                downloaded.erase(folder);
                textures.needs_restart |= was_active; // the game still has its textures
            } else {
                LOG_WARNING(Frontend, "xr: texture packs: delete {} failed: {}", folder, out.error);
                FrameXr::ShowToastText("Texture Pack Not Deleted");
            }
            RefreshPacks();
            ListPackDownloads();
        });
}
