// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "citra_qt/frame_mods.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QWidget>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <fmt/format.h>

#include "common/logging/log.h"
#include "core/cheats/gateway_cheat.h" // AzaharFrame ui2
#include "core/cheats/online.h"
#include "core/core.h"                 // AzaharFrame ui2
#include "core/custom_tex/download.h"
#include "core/mods/library.h"
#include "core/mods/zip_install.h" // AzaharFrame zipinstall
#include "frame_xr/frame_xr.h"
#include "frame_xr/menu/cheat_entry.h" // AzaharFrame ui2
#include "frame_xr/mem_guard.h" // AzaharFrame modfix

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

/// AzaharFrame modfix: memory and swap nearly used up (see frame_xr/mem_guard.h).
bool LowMemory() {
    std::ifstream in("/proc/meminfo");
    std::stringstream text;
    text << in.rdbuf();
    return FrameXr::BrowserWouldRunOutOfMemory(text.str());
}

/// The author's repository from the catalog entry ("https://github.com/owner/repo"), else from
/// what the library recorded at install; "" when it is not a GitHub repository.
std::string RepoUrl(const Mods::Catalog& catalog, const std::string& id,
                    const std::string& source = {}) {
    std::string url;
    if (const Mods::CatalogEntry* e = catalog.Find(id)) {
        if (!e->author_url.empty()) { // AzaharFrame modcat: a GameBanana author's page
            return Menu::LinkText(e->author_url).empty() ? std::string{} : e->author_url;
        }
        const bool owner = e->source.type == Mods::Source::Type::OwnerRepoPath || e->source.repo.empty();
        url = std::string("https://github.com/") + (owner ? Mods::kOwnerRepo : e->source.repo);
    } else if (source.rfind("github.com/", 0) == 0) {
        url = "https://" + source;
    } else if (source.rfind("gamebanana.com/mods/", 0) == 0) {
        return "https://" + source; // AzaharFrame modcat: the mod's page
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

void FrameMods::SetCodeCrcSource(std::function<std::optional<u32>()> source) {
    code_crc_source = std::move(source);
}

void FrameMods::Open(u64 title_id_, int run_) {
    if (title_id_ != title_id || run_ != run) {
        // Another game, or the same one started again: what waited for a restart has applied.
        title_id = title_id_;
        run = run_;
        game_crc_read = false; // AzaharFrame modcat
        game_crc.reset();
        mods = {};
        textures = {};
        packs.clear();
        failed.clear();
        pending = {};
        pack_pending = {};
        pack_failed.clear();
        pack_error.clear();
        ZipCloseJob(); // AzaharFrame zipinstall: a question of another game's goes
        zip = {};
        zip_question = ZipQuestion::None;
        zip_packs.clear();
        PublishMods();
        PublishTextures();
        PublishZip();
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
        // AzaharFrame zipinstall: a zip's pack comes again from the zip, not a download
        textures.dialog = {Menu::ModsDialogKind::DeletePack, it->name,
                           zip_packs.count(pack_pending.id) ? "You can install it again from its zip."
                                                            : ""};
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
    // AzaharFrame zipinstall: Install from Zip
    case C::InstallFromZip:
        ZipStart(r.value);
        return;
    case C::ZipBrowse:
        if (zip.open && zip.browse && !zip.working) {
            ZipBrowse(r.text);
        }
        return;
    case C::ZipPick:
        if (zip.open && zip.browse) {
            ZipPick(r.text);
        }
        return;
    case C::ZipChoose:
        if (zip_job && r.value >= 0 && r.value < static_cast<int>(zip_choices.size()) && !zip.working) {
            const u64 tid = zip_choices[static_cast<std::size_t>(r.value)];
            zip.choices.clear();
            zip_choices.clear();
            ZipInstall(zip_job, tid, false);
        }
        return;
    case C::ZipDialog:
        ZipAnswer(r.value != 0);
        return;
    case C::ZipClose:
        if (!zip.working) {
            ZipCloseJob();
            zip.open = false;
            zip.dialog = {};
            zip.choices.clear();
            zip.error.clear();
            zip_choices.clear();
            zip_question = ZipQuestion::None;
            PublishZip();
        }
        return;
    case C::OpenUrl: {
        // Only a page the menu showed: a mod's GitHub repository or a pack author's page from
        // the catalog (the menu asked "Open <host>?" first).
        bool known = !Menu::GithubLinkText(r.text).empty();
        for (const Menu::PackOffer& o : textures.downloads) {
            known |= o.url == r.text;
        }
        for (const Menu::ModOffer& o : mods.available) { // AzaharFrame modcat: GameBanana authors
            known |= !o.url.empty() && o.url == r.text;
        }
        for (const Menu::ModRow& row : mods.loaded) {
            known |= !row.url.empty() && row.url == r.text;
        }
        if (known && !Menu::LinkText(r.text).empty()) {
            // Steam's own browser, as other apps on the Frame show pages in the headset; the
            // desktop's browser only when Steam is not running.
            const QString url = QString::fromStdString(r.text);
            // AzaharFrame modfix: the browser's start can exhaust the Frame's memory and get the
            // game killed by the kernel (exit 137); with little room left, don't open it.
            if (LowMemory()) {
                LOG_WARNING(Frontend, "xr: mods: not opening {}: little memory left", r.text);
                FrameXr::ShowToastText("Link Not Opened: Low Memory");
                return;
            }
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
    // AzaharFrame modcat: the game's code CRC32, read here on the GUI thread (once per game), marks
    // the mods made for this build
    if (!game_crc_read && code_crc_source) {
        game_crc = code_crc_source();
        game_crc_read = true;
    }
    const std::optional<u32> crc = game_crc;
    OffThread(
        this,
        [tid, crc] {
            Mods::Library& lib = Mods::Default();
            lib.RefreshCatalogOnce(); // one request a run; the built-in catalog without it
            lib.RefreshDates(tid);    // AzaharFrame modcat: the sources' own dates, once a run
            return lib.ListAvailable(tid, crc);
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
                o.updated = CustomTex::Download::UpdatedText(a.updated); // AzaharFrame modcat
                o.version = a.chip;
                o.version_tip = a.chip_tip;
                o.version_match = a.matches_game;
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
                    o.note = failed_reason.empty() ? std::string("Install Failed")
                                                   : "Install Failed: " + failed_reason; // AzaharFrame modfix
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
    failed_reason.clear();
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
                failed_reason = Mods::ShortFailure(out.error); // AzaharFrame modfix
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
    const Mods::State st = Mods::Default().GetState();
    std::string text = PhaseText(st.phase);
    if (!text.empty() && st.phase == Mods::State::Phase::Downloading && st.total > 0) {
        text += fmt::format(" {}%", std::min<u64>(100, st.done * 100 / st.total)); // AzaharFrame mega
    }
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
        this,
        [tid] {
            auto list = CustomTex::ListPacks(CustomTex::DefaultPaths(), tid);
            // AzaharFrame zipinstall: the packs Install from Zip made can be deleted too
            std::set<std::string> zipped;
            for (const CustomTex::Pack& p : list) {
                const auto id = p.managed ? CustomTex::Download::MarkerId(p.path) : std::nullopt;
                if (id && id->rfind("zip:", 0) == 0) {
                    zipped.insert(std::filesystem::path(p.path).filename().string());
                }
            }
            return std::make_pair(std::move(list), std::move(zipped));
        },
        [this, tid](const std::pair<std::vector<CustomTex::Pack>, std::set<std::string>>& result) {
            if (tid != title_id) {
                return;
            }
            const std::vector<CustomTex::Pack>& list = result.first;
            zip_packs = result.second;
            packs = list;
            textures.packs.clear();
            for (const CustomTex::Pack& p : packs) {
                const std::string folder = std::filesystem::path(p.path).filename().string();
                const bool mine =
                    p.managed && (downloaded.count(folder) != 0 || zip_packs.count(folder) != 0);
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
        o.updated = CustomTex::Download::UpdatedText(p.updated); // AzaharFrame zipinstall
        o.verified = p.verified;
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

// ---- Install from Zip (AzaharFrame zipinstall) ----

namespace {

namespace Zip = Mods::Zip;

std::string LoadDirPath() {
    return CustomTex::DefaultPaths().load_dir;
}

std::string HomeDir() {
    return QDir::homePath().toStdString();
}

/// Downloads, Documents and Home, those that exist.
std::vector<Menu::ZipChip> QuickFolders() {
    std::vector<Menu::ZipChip> chips;
    const auto add = [&chips](const char* label, const QString& path) {
        if (!path.isEmpty() && QFileInfo(path).isDir()) {
            chips.push_back({label, QDir::cleanPath(path).toStdString()});
        }
    };
    add("Downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    add("Documents", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    add("Home", QDir::homePath());
    return chips;
}

/// "~/Downloads/Packs" for a folder under the home folder.
std::string FolderText(const std::string& folder) {
    const std::string home = HomeDir();
    if (folder == home) {
        return "~";
    }
    if (!home.empty() && folder.rfind(home + "/", 0) == 0) {
        return "~" + folder.substr(home.size());
    }
    return folder;
}

bool EndsWithNoCase(const std::string& name, std::string_view e) {
    std::string l = name;
    std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return std::tolower(c); });
    return l.size() > e.size() && l.compare(l.size() - e.size(), e.size(), e) == 0;
}

bool IsArchiveName(const std::string& name) {
    return EndsWithNoCase(name, ".zip") || EndsWithNoCase(name, ".7z");
}

/// AzaharFrame ui2: a cheat file (Citra's and Azahar's are .txt).
bool IsCheatFileName(const std::string& name) {
    return EndsWithNoCase(name, ".txt");
}

struct Listing {
    bool ok = false;
    std::vector<Menu::ZipEntry> entries;
};

/// The folder's subfolders and archives (hidden ones left out), folders first, by name;
/// AzaharFrame ui2: with `cheat_files`, .txt files too.
Listing ListFolder(const std::string& folder, bool cheat_files) {
    namespace fs = std::filesystem;
    constexpr std::size_t kMax = 1000;
    Listing out;
    std::error_code ec;
    fs::directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        return out;
    }
    out.ok = true;
    std::vector<Menu::ZipEntry> folders, files;
    for (fs::directory_iterator end; !ec && it != end && folders.size() + files.size() < kMax;
         it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.empty() || name[0] == '.') {
            continue;
        }
        std::error_code e2;
        if (it->is_directory(e2)) {
            folders.push_back({name, true, {}});
        } else if (it->is_regular_file(e2) &&
                   (IsArchiveName(name) || (cheat_files && IsCheatFileName(name)))) {
            const auto size = it->file_size(e2);
            files.push_back({name, false, e2 ? std::string{} : Menu::SizeText(size)});
        }
    }
    const auto by_name = [](const Menu::ZipEntry& a, const Menu::ZipEntry& b) {
        return QString::fromStdString(a.name).compare(QString::fromStdString(b.name),
                                                      Qt::CaseInsensitive) < 0;
    };
    std::sort(folders.begin(), folders.end(), by_name);
    std::sort(files.begin(), files.end(), by_name);
    out.entries = std::move(folders);
    out.entries.insert(out.entries.end(), files.begin(), files.end());
    return out;
}

struct Opened {
    std::shared_ptr<Zip::Job> job;
    std::string error;
};

} // namespace

void FrameMods::PublishZip() {
    FrameXr::SetInGameMenuZip(zip);
}

void FrameMods::ZipCloseJob() {
    if (auto job = std::exchange(zip_job, nullptr)) {
        std::thread([job] { Zip::Close(*job); }).detach(); // a big pack's folder takes a while
    }
}

void FrameMods::ZipStart(int target) {
    if (zip.working || target < 0 || target > 2) { // AzaharFrame ui2: 2 Cheats
        return;
    }
    ZipCloseJob();
    zip.target = static_cast<Menu::ZipTarget>(target);
    zip.error.clear();
    zip.file.clear();
    zip.choices.clear();
    zip.dialog = {};
    zip_choices.clear();
    zip_question = ZipQuestion::None;
    zip.chips = QuickFolders();
    std::string start = zip_folder;
    if (start.empty() || !std::filesystem::is_directory(start)) {
        start = zip.chips.empty() ? HomeDir() : zip.chips.front().path; // Downloads
    }
    if (FrameXr::VrShowing()) {
        // The menu's own browser: the portal's chooser would open on Steam's display, out of sight.
        LOG_INFO(Frontend, "xr: zip: browser opened");
        zip.open = true;
        zip.browse = true;
        ZipBrowse(start);
        return;
    }
    // Flat: Qt's file dialog, a window of the app (FrameMirror fits it to the theater). After the
    // menu's request has returned.
    zip.browse = false;
    zip.open = false;
    PublishZip();
    QPointer<FrameMods> guard(this);
    const u64 tid = title_id;
    QTimer::singleShot(0, this, [guard, tid, start] {
        if (!guard || guard->title_id != tid) {
            return;
        }
        QWidget* owner = qobject_cast<QWidget*>(guard->parent());
        const bool cheats = guard->zip.target == Menu::ZipTarget::Cheats; // AzaharFrame ui2
        const QString path = QFileDialog::getOpenFileName(
            owner, cheats ? QStringLiteral("Install from File") : QStringLiteral("Install from Zip"),
            QString::fromStdString(start),
            cheats ? QStringLiteral("Cheat File or Zip (*.txt *.zip *.7z *.TXT *.ZIP *.7Z)")
                   : QStringLiteral("Zip or 7z (*.zip *.7z *.ZIP *.7Z)"),
            nullptr, QFileDialog::DontUseNativeDialog);
        if (!guard || guard->title_id != tid || path.isEmpty()) {
            return;
        }
        guard->zip_folder = QFileInfo(path).absolutePath().toStdString();
        guard->zip.open = true;
        guard->ZipPick(path.toStdString());
    });
}

void FrameMods::ZipBrowse(const std::string& folder_in) {
    std::string folder = QDir::cleanPath(QString::fromStdString(folder_in)).toStdString();
    if (folder.empty() || folder[0] != '/') {
        return;
    }
    zip.folder = folder;
    zip.folder_text = FolderText(folder);
    zip.can_go_up = folder != "/";
    zip.loading = true;
    zip.entries.clear();
    zip.error.clear();
    PublishZip();
    OffThread(
        this, [folder, cheats = zip.target == Menu::ZipTarget::Cheats] {
            return ListFolder(folder, cheats);
        },
        [this, folder](const Listing& listing) {
            if (zip.folder != folder) {
                return; // another folder was asked for meanwhile
            }
            zip.loading = false;
            zip.entries = listing.entries;
            if (listing.ok) {
                zip_folder = folder;
            } else {
                zip.error = "Can't Open the Folder";
            }
            PublishZip();
        });
}

void FrameMods::ZipPick(const std::string& path) {
    if (zip.working || path.empty()) {
        return;
    }
    if (zip.target == Menu::ZipTarget::Cheats) { // AzaharFrame ui2
        return ZipCheats(path);
    }
    ZipCloseJob();
    zip.working = true;
    zip.phase = "Unpacking";
    zip.file = std::filesystem::path(path).filename().string();
    zip.error.clear();
    zip.choices.clear();
    zip_choices.clear();
    PublishZip();
    LOG_INFO(Frontend, "xr: zip: unpacking {}", zip.file);
    const u64 tid = title_id;
    OffThread(
        this,
        [path] {
            Opened o;
            o.job = std::make_shared<Zip::Job>();
            o.error = Zip::Open(path, LoadDirPath(), *o.job);
            return o;
        },
        [this, tid](const Opened& o) {
            if (tid != title_id) {
                Zip::Close(*o.job);
                return;
            }
            if (!o.error.empty()) {
                LOG_WARNING(Frontend, "xr: zip: {} not unpacked: {}", zip.file, o.error);
                Zip::Close(*o.job);
                return ZipFail(Zip::ShortError(o.error));
            }
            const Zip::Detected& d = o.job->detected;
            if (d.kind == Zip::Kind::Unknown) {
                LOG_INFO(Frontend, "xr: zip: {}: {}", zip.file, d.reason);
                zip_job = o.job;
                ZipCloseJob();
                return ZipFail(d.reason);
            }
            const Zip::TitleChoice choice = Zip::ResolveTitle(d.title_ids, title_id);
            LOG_INFO(Frontend, "xr: zip: {} is a {} for {} title(s)", zip.file,
                     d.kind == Zip::Kind::Mod ? "mod" : "texture pack", d.title_ids.size());
            if (choice.title_id == 0 && choice.candidates.empty()) {
                // No title ID in the archive and no game (the menu always has one; kept safe)
                zip_job = o.job;
                ZipCloseJob();
                return ZipFail(Zip::kNoGame);
            }
            if (choice.title_id == 0) {
                // Several games, none of them this one: the player chooses.
                zip_job = o.job;
                zip.working = false;
                zip_choices = choice.candidates;
                for (const u64 c : choice.candidates) {
                    zip.choices.push_back(Mods::TitleIdText(c));
                }
                PublishZip();
                return;
            }
            ZipInstall(o.job, choice.title_id, false);
        });
}

void FrameMods::ZipInstall(std::shared_ptr<Zip::Job> job, u64 tid, bool replace) {
    zip_job = job;
    zip_title = tid;
    zip.working = true;
    zip.phase = "Installing";
    PublishZip();
    const bool texture = job->detected.kind == Zip::Kind::TexturePack;
    const u64 game = title_id;
    OffThread(
        this,
        [job, tid, replace, texture] {
            return texture ? Zip::InstallTextures(*job, tid, LoadDirPath(), replace)
                           : Zip::InstallMod(*job, tid, Mods::Default(), replace);
        },
        [this, game, tid, texture, job](const Zip::Outcome& out) {
            if (game != title_id) {
                return;
            }
            using R = Zip::Outcome::Result;
            switch (out.result) {
            case R::Ok:
                LOG_INFO(Frontend, "xr: zip: installed {} for {:016X}", out.installed, tid);
                return ZipDone(texture, tid);
            case R::Conflict:
            case R::Blocked: {
                // Never replaced silently: ask, or say what has to go first.
                LOG_INFO(Frontend, "xr: zip: {} conflicts with {}", job->name, out.conflict_name);
                zip.working = false;
                const bool same = texture || out.conflict_id == out.installed;
                if (out.result == R::Blocked) {
                    zip_question = ZipQuestion::Blocked;
                    zip_mod_id = out.installed;
                    ZipCloseJob();
                    zip.dialog = {Menu::ModsDialogKind::Blocked, job->name, out.conflict_name};
                } else if (same) {
                    zip_question = ZipQuestion::ReplaceSame; // the job is kept for the yes
                    zip.dialog = {Menu::ModsDialogKind::Replace, job->name, out.conflict_name};
                } else {
                    zip_question = ZipQuestion::ReplaceOther; // the library has it now
                    zip_mod_id = out.installed;
                    ZipCloseJob();
                    zip.dialog = {Menu::ModsDialogKind::Replace, job->name, out.conflict_name};
                }
                PublishZip();
                return;
            }
            case R::Error:
                LOG_WARNING(Frontend, "xr: zip: {} not installed: {}", job->name, out.error);
                return ZipFail(Zip::ShortError(out.error));
            }
        });
}

void FrameMods::ZipAnswer(bool yes) {
    const ZipQuestion q = std::exchange(zip_question, ZipQuestion::None);
    zip.dialog = {};
    const u64 tid = zip_title;
    if (q == ZipQuestion::ReplaceSame && yes && zip_job) {
        ZipInstall(zip_job, tid, true);
        return;
    }
    if (q == ZipQuestion::ReplaceOther && yes) {
        zip.working = true;
        zip.phase = "Installing";
        PublishZip();
        const std::string id = zip_mod_id;
        const u64 game = title_id;
        OffThread(
            this, [tid, id] { return Mods::Default().Install(tid, id, true); },
            [this, game, tid, id](const Mods::InstallOutcome& out) {
                if (game != title_id) {
                    return;
                }
                if (out.result == Mods::InstallResult::Ok ||
                    out.result == Mods::InstallResult::AlreadyInstalled) {
                    return ZipDone(false, tid);
                }
                LOG_WARNING(Frontend, "xr: zip: {} not installed: {}", id, out.error);
                ZipFail(Zip::ShortError(out.error));
            });
        return;
    }
    // Cancel or OK: nothing changes; a mod the library took for the question goes again.
    ZipCloseJob();
    if (q == ZipQuestion::ReplaceOther || q == ZipQuestion::Blocked) {
        const std::string id = zip_mod_id;
        OffThread(
            this, [tid, id] { return Mods::Default().Remove(tid, id); },
            [this](const Mods::RemoveOutcome&) { RefreshLoaded(); });
    }
    if (!zip.browse) {
        zip.open = false; // Flat: back to the page it came from
    }
    PublishZip();
}

void FrameMods::ZipDone(bool texture, u64 tid) {
    ZipCloseJob();
    zip.working = false;
    zip.open = false;
    zip.error.clear();
    zip.file.clear();
    PublishZip();
    const bool here = tid == title_id;
    FrameXr::ShowToastText(!here      ? "Installed for Another Game"
                           : texture ? "Texture Pack Installed"
                                     : "Mod Installed");
    if (texture) {
        RefreshPacks();
    } else {
        mods.needs_restart |= here;
        RefreshLoaded();
        if (mods.catalog == Menu::CatalogState::Ready) {
            RefreshAvailable();
        }
        PublishMods();
    }
}

void FrameMods::ZipFail(const std::string& error) {
    ZipCloseJob();
    zip.working = false;
    zip.error = error;
    PublishZip();
}

// AzaharFrame ui2: Install from File in the Cheats segment. A .txt is read as it is; an archive is
// unpacked as Install from Zip does and its cheat file taken (cheat_entry.h PickCheatFile). The
// cheats whose names the game's list does not have yet are added, off, and the file saved; the
// Cheats segment shows them at its next read (frame_cheats.cpp, every second while the menu is open).
void FrameMods::ZipCheats(const std::string& path) {
    struct Read {
        std::string text;
        std::string name;  ///< the cheat's name for a plain list of codes
        std::string error; ///< a few words for the page
    };
    zip.working = true;
    zip.phase = IsCheatFileName(path) ? "Reading" : "Unpacking";
    zip.file = std::filesystem::path(path).filename().string();
    zip.error.clear();
    zip.choices.clear();
    PublishZip();
    LOG_INFO(Frontend, "xr: zip: cheat file {}", zip.file);
    const u64 tid = title_id;
    OffThread(
        this,
        [path, tid] {
            Read r;
            constexpr std::uintmax_t kMaxText = 4u << 20;
            const auto read_text = [&r](const std::filesystem::path& p) {
                std::error_code ec;
                const auto size = std::filesystem::file_size(p, ec);
                if (ec || size > kMaxText) {
                    r.error = ec ? "Can't Read the File" : "File Too Large";
                    return;
                }
                std::ifstream f(p, std::ios::binary);
                std::ostringstream ss;
                ss << f.rdbuf();
                r.text = ss.str();
                r.name = p.stem().string();
            };
            if (IsCheatFileName(path)) {
                read_text(path);
                return r;
            }
            Zip::Job job;
            const std::string err = Zip::Open(path, LoadDirPath(), job);
            if (!err.empty()) {
                r.error = Zip::ShortError(err);
                return r;
            }
            const std::string pick = Menu::PickCheatFile(job.files, tid);
            if (pick.empty()) {
                r.error = "No Cheat File in the Zip";
            } else {
                read_text(std::filesystem::path(job.root) / pick);
                if (r.error.empty() && Menu::IsTitleIdName(r.name)) {
                    r.name = std::filesystem::path(path).stem().string(); // the zip's name
                }
            }
            Zip::Close(job);
            return r;
        },
        [this, tid](const Read& r) {
            if (tid != title_id) {
                return;
            }
            if (!r.error.empty()) {
                LOG_INFO(Frontend, "xr: zip: {}: {}", zip.file, r.error);
                return ZipFail(r.error);
            }
            const Menu::CheatFileText parsed = Menu::ParseCheatFile(r.text, r.name);
            if (parsed.cheats.empty()) {
                return ZipFail("No Cheats in This File");
            }
            auto& engine = Core::System::GetInstance().CheatEngine();
            std::set<std::string> names;
            for (const auto& c : Cheats::Online::ListCheats(engine, tid)) {
                names.insert(c.name);
            }
            int added = 0;
            for (const Menu::CheatText& c : parsed.cheats) {
                if (!names.insert(c.name).second) {
                    continue; // the game has it already
                }
                engine.AddCheat(std::make_shared<Cheats::GatewayCheat>(c.name, c.code, c.comments));
                added++;
            }
            if (added > 0) {
                engine.SaveCheatFile(tid);
            }
            LOG_INFO(Frontend, "xr: zip: {} cheats added from {} ({} skipped)", added, zip.file,
                     parsed.skipped);
            zip.working = false;
            zip.open = false;
            zip.error.clear();
            zip.file.clear();
            PublishZip();
            FrameXr::ShowToastText(Menu::CheatsAddedText(added));
        });
}
