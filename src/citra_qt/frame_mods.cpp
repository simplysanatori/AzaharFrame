// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "citra_qt/frame_mods.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <chrono> // AzaharFrame stall
#include <thread>
#include <utility>

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage> // AzaharFrame texparts
#include <QPainter>
#include <QPainterPath>
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
#include "core/custom_tex/layers.h" // AzaharFrame texlayers
#include "core/custom_tex/optimize.h" // AzaharFrame ui3
#include "core/mods/transfer.h" // AzaharFrame fixes7
#include "core/mods/library.h"
#include "core/mods/zip_install.h" // AzaharFrame zipinstall
#include "citra_qt/frame_crash_guard.h" // AzaharFrame compat
#include "citra_qt/frame_preload_confirm.h" // AzaharFrame memguard
#include <cmath>
#include "common/scm_rev.h"
#include "core/frame_crash_send.h"
#include "common/settings.h"
#include "frame_xr/frame_xr.h"
#include "frame_xr/controls_presets.h" // AzaharFrame kiufix: CStickModInstalled
#include "frame_xr/kiu_scan.h"         // AzaharFrame kiufix: IsKidIcarus
#include "frame_xr/menu/angels_aim_tab.h"
#include "frame_xr/menu/cheat_entry.h" // AzaharFrame ui2
#include "frame_xr/menu/ctgp7_tab.h"   // AzaharFrame ctgp7
#include "frame_xr/mem_guard.h" // AzaharFrame modfix

namespace Menu = FrameXr::Menu;

namespace {

/// `work` on a thread of its own, then `done` with its result on the GUI thread (if `self` is
/// still there).
template <typename Work, typename Done>
void OffThread(FrameMods* self, Work work, Done done) {
    QPointer<FrameMods> guard(self);
    std::thread([guard, work = std::move(work), done = std::move(done)] {
        // AzaharFrame jsoncrash: an exception out of the job (a bad file, a JSON type error) was
        // an uncaught exception on a thread of its own: std::terminate, the whole app gone.
        // The job's result type is default-constructible: its "empty" answer is the failure.
        decltype(work()) result{};
        try {
            result = work();
        } catch (const std::exception& e) {
            LOG_ERROR(Frontend, "xr: mods: a background job failed: {}", e.what());
        } catch (...) {
            LOG_ERROR(Frontend, "xr: mods: a background job failed");
        }
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

/// AzaharFrame jsoncrash: a job's answer; a default one (the job threw) is a failure.
struct AvailableResult {
    bool same = false;
    bool failed = true;
    std::vector<Mods::AvailableMod> list;
};

/// What an install is doing, as the row's supporting line says it.
std::string PhaseText(Mods::State::Phase phase) {
    switch (phase) {
    case Mods::State::Phase::Resolving:
        return "Preparing"; // AzaharFrame fixes7: as Download Packs (CustomTex::Download::kPreparing)
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
    r.recommended = info.recommended; // AzaharFrame fixes7: the badge
    r.managed = info.managed;
    r.active = info.state == Mods::ModState::Active;
    if (info.managed) {
        r.author = info.author;
        r.url = RepoUrl(catalog, info.id, info.source);
        r.detail = Menu::ModVersionText(info.version); // AzaharFrame ui3: "v1.5.9"
    } else if (info.files.size() > 1) {
        r.detail = fmt::format("{} Files", info.files.size()); // your own: what is there
    }
    return r;
}

/// AzaharFrame texparts: a part's preview for the menu: the PNG's middle square, `px` across,
/// corners rounded as the page's 8 dp of 40 (RGBA, straight alpha).
std::shared_ptr<const std::vector<uint8_t>> PartPreview(const std::string& path, int px) {
    QImage src(QString::fromStdString(path));
    if (src.isNull()) {
        return {};
    }
    const int side = std::min(src.width(), src.height());
    const QImage sq = src.copy((src.width() - side) / 2, (src.height() - side) / 2, side, side)
                          .scaled(px, px, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                          .convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    QImage out(px, px, QImage::Format_RGBA8888_Premultiplied);
    out.fill(Qt::transparent);
    {
        QPainter painter(&out);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        const qreal r = px * 8.0 / 40.0;
        clip.addRoundedRect(QRectF(0, 0, px, px), r, r);
        painter.setClipPath(clip);
        painter.drawImage(0, 0, sq);
    }
    out = out.convertToFormat(QImage::Format_RGBA8888);
    auto rgba = std::make_shared<std::vector<uint8_t>>(static_cast<std::size_t>(px) * px * 4);
    for (int y = 0; y < px; y++) {
        std::memcpy(rgba->data() + static_cast<std::size_t>(y) * px * 4, out.constScanLine(y),
                    static_cast<std::size_t>(px) * 4);
    }
    return rgba;
}

constexpr int kPreviewPx = 96; ///< the page's 40 dp previews

std::string PackDetail(const CustomTex::Pack& p) {
    // AzaharFrame texrow: "ASTC", as the Customize page's "Optimized for Frame · ASTC" says it
    const std::string format =
        p.format == CustomTex::Format::Ktx ? std::string("ASTC") : CustomTex::FormatName(p.format);
    return Menu::JoinDetail({p.author, p.version, format,
                             p.size_bytes ? Menu::SizeText(*p.size_bytes) : std::string{}});
}

} // namespace

FrameMods::FrameMods(QObject* parent) : QObject(parent) {
    progress = new QTimer(this);
    connect(progress, &QTimer::timeout, this, [this] { PollProgress(); });
    download_poll = new QTimer(this);
    connect(download_poll, &QTimer::timeout, this, [this] { PollDownload(); });
}

// AzaharFrame ui3: the destructor is in frame_mods_optimize.cpp (a conversion is stopped cleanly).

void FrameMods::SetCodeCrcSource(std::function<std::optional<u32>()> source) {
    code_crc_source = std::move(source);
}

void FrameMods::SetGameInfoSource(std::function<Mods::Compat::Game()> source) {
    game_info_source = std::move(source);
}

const Mods::Compat::Game& FrameMods::GameInfo() {
    if (!game_crc_read && code_crc_source) {
        game_crc = code_crc_source();
        game_crc_read = true;
    }
    if (!game_info) {
        Mods::Compat::Game g = game_info_source ? game_info_source() : Mods::Compat::Game{};
        g.title_id = title_id;
        g.code_crc = game_crc;
        LOG_INFO(Frontend, "xr: mods: the game is {:016X}, region {}, version {} ({}), code {}",
                 g.title_id, g.region == Mods::Compat::Region::Unknown ? "unknown" : Mods::Compat::RegionName(g.region),
                 g.version ? Mods::Compat::VersionText(*g.version)
                           : (g.version_state.empty() ? std::string("unknown") : "not known, it " + g.version_state),
                 g.version_source,
                 g.code_crc ? fmt::format("{:08x}", *g.code_crc) : "unknown");
        game_info = std::move(g);
    }
    game_info->code_crc = game_crc;
    return *game_info;
}

void FrameMods::Open(u64 title_id_, int run_) {
    if (title_id_ != title_id || run_ != run) {
        // Another game, or the same one started again: what waited for a restart has applied.
        title_id = title_id_;
        run = run_;
        game_crc_read = false; // AzaharFrame modcat
        game_crc.reset();
        game_info.reset(); // AzaharFrame compat
        anyway.clear();
        unscanned_ok.clear();
        mods = {};
        textures = {};
        packs.clear();
        layers.reset(); // AzaharFrame texlayers
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
        angels_aim_report_url.clear(); // AzaharFrame kiufix: the segment goes with its game
        FrameXr::SetInGameMenuAngelsAim({});
    }
    if (title_id == 0) {
        return;
    }
    // AzaharFrame compat: what was turned off before this start (the snackbar)
    {
        const FrameCrashGuard& guard = FrameCrashGuard::Get();
        std::vector<std::string> names;
        if (guard.TurnedOffTitle() == title_id) {
            for (const auto& item : guard.TurnedOff()) {
                names.push_back(item.name);
            }
        }
        if (names != mods.turned_off || guard.Serial() != mods.turned_off_serial) {
            mods.turned_off = std::move(names);
            mods.turned_off_serial = guard.Serial();
            PublishMods();
        }
    }
    RefreshLoaded();
    RefreshPacks();
    RefreshAngelsAim(); // AzaharFrame kiufix
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
        // AzaharFrame stall: a game with no title id (homebrew) has no catalog mods; Install Mods
        // says so at once (it waited for an answer that never came, its ring standing still).
        if (r.command == C::LoadModCatalog && mods.catalog != Menu::CatalogState::Ready) { // jsoncrash: Failed too
            LOG_INFO(Frontend, "xr: mods: Install Mods: no title id, no catalog mods");
            mods.available.clear();
            mods.catalog = Menu::CatalogState::Ready;
            PublishMods();
        }
        return;
    }
    switch (r.command) {
    case C::SelectTexturePack:
        SelectPack(r.text, r.value != 0);
        return;
    case C::TextureLayer: // AzaharFrame texlayers
        ToggleLayer(r.value);
        return;
    case C::MoveTextureLayer:
        MoveLayer(r.value, r.text == "up" ? -1 : 1);
        return;
    case C::LoadModCatalog:
        LOG_INFO(Frontend, "xr: mods: Install Mods opened ({:016X}, list {})", title_id,
                 mods.catalog == Menu::CatalogState::Ready ? "shown" : "being made");
        if (mods.catalog == Menu::CatalogState::Idle ||
            mods.catalog == Menu::CatalogState::Failed) { // AzaharFrame jsoncrash: Retry
            mods.catalog = Menu::CatalogState::Loading;
            PublishMods();
        }
        RefreshAvailable();
        return;
    case C::InstallMod:
        AllowAnyway(r.text, r.value); // AzaharFrame compat: Install Anyway

        Install(r.text, false);
        return;
    case C::CancelModInstall: // AzaharFrame integ3: the install fails with "Cancelled"
        if (mods.busy && !installing.empty()) {
            LOG_INFO(Frontend, "xr: mods: install {} cancelled", installing);
            Mods::Default().Cancel();
        }
        return;
    case C::TurnBackOn: // AzaharFrame compat
        TurnBackOn();
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
        } else if (p.kind == Pending::Kind::BigInstall) { // AzaharFrame catalog3
            Install(p.id, false, true);
        } else if (p.kind == Pending::Kind::Unscanned) { // AzaharFrame compat: Download Anyway
            unscanned_ok.insert(p.id);
            Install(p.id, false, true);
        } else if (p.kind == Pending::Kind::Remove) {
            Remove(p.id);
        }
        return;
    }
    case C::LoadPackDownloads:
        LoadPackDownloads();
        return;
    // AzaharFrame texparts: a pack's Customize page
    case C::OpenTextureParts:
        OpenParts(r.text);
        return;
    case C::OptimizePack: // AzaharFrame ui3
        OptimizeAsk(r.text);
        return;
    case C::OptimizeStop:
        OptimizeStop();
        return;
    case C::UseOriginalPack:
        UseOriginal(r.text);
        return;
    case C::RemoveOriginalPack:
        RemoveOriginalAsk(r.text);
        return;
    case C::TexturePart:
        PartClicked(r.value);
        return;
    case C::MoveTexturePart:
        MovePart(r.value, r.text == "up" ? -1 : 1);
        return;
    case C::TexturePartsAll:
        AllParts(r.value != 0);
        return;
    case C::DownloadPack: {
        const Menu::PackOffer* o = PackOffer(r.text);
        if (!o || textures.downloading || !downloader) {
            return;
        }
        AllowAnyway(r.text, r.value); // AzaharFrame compat: Download Anyway
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
        const std::string folder = std::filesystem::path(it->path).filename().string();
        // AzaharFrame texrow: the row is the pair: its other copy goes too, when it can
        std::string other;
        if (layers) {
            if (const auto o = layers->partners.find(it->id); o != layers->partners.end()) {
                const CustomTex::Pack* op = layers->Find(o->second);
                const std::string of =
                    op ? std::filesystem::path(op->path).filename().string() : std::string{};
                if (op && op->managed && (downloaded.count(of) != 0 || zip_packs.count(of) != 0)) {
                    other = of;
                }
            }
        }
        pack_pending = {Pending::Kind::Delete, folder, other};
        // AzaharFrame zipinstall: a zip's pack comes again from the zip, not a download
        textures.dialog = {Menu::ModsDialogKind::DeletePack, it->name,
                           Menu::DeletePackBody(!other.empty(), zip_packs.count(folder) != 0)};
        PublishTextures();
        return;
    }
    case C::AskPreload: { // AzaharFrame memguard: this moment's numbers (frame_preload_confirm.h)
        if (textures.dialog.kind != Menu::ModsDialogKind::None) {
            return; // another question is open
        }
        const VideoCore::PreloadBudget::ConfirmText t = FramePreloadConfirmText();
        pack_pending = {Pending::Kind::Preload, {}};
        Menu::ModsDialog& d = textures.dialog;
        d = {Menu::ModsDialogKind::Preload, t.body, t.status_text};
        d.rows = t.rows;
        d.status = static_cast<int>(t.status);
        d.title = t.title;
        // AzaharFrame integ3: too large: Optimize for Frame is offered on the Packs page
        if (t.status == VideoCore::PreloadBudget::Status::TooLarge) {
            NudgeOptimizeActive(title_id);
        }
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
        if (p.kind == Pending::Kind::Unscanned) { // AzaharFrame compat: Download Anyway
            unscanned_ok.insert(p.id);
            StartDownload(p.id);
        } else if (p.kind == Pending::Kind::Download) {
            StartDownload(p.id);
        } else if (p.kind == Pending::Kind::Delete) {
            DeletePack(p.id, p.other);
        } else if (p.kind == Pending::Kind::Optimize) { // AzaharFrame ui3
            OptimizeStart(p.id);
        } else if (p.kind == Pending::Kind::RemoveOriginal) {
            RemoveOriginalDo(p.id);
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
        known |= r.text == Menu::kCtgp7Website; // AzaharFrame ctgp7: Official Website
        // AzaharFrame kiufix: Angel's Aim's Report an Issue (the page this side made)
        known |= !angels_aim_report_url.empty() && r.text == angels_aim_report_url;
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
                FrameXr::ShowToastText("Link Not Opened: Low Memory", FrameXr::ToastTime::Long);
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
            RefreshAngelsAim(); // AzaharFrame kiufix: the mod may have come or gone
        });
}

// AzaharFrame kiufix: the Angel's Aim segment.
namespace {
/// A folder named `name` with a PNG in it, under `dir` (at most three levels down): the Kid
/// Icarus: Uprising texture packs install as <title>/weapons/ and <title>/effects/.
bool HasPackFolder(const std::filesystem::path& dir, const std::string& name) {
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(
        dir, std::filesystem::directory_options::skip_permission_denied, ec);
    for (; !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (it.depth() > 3) {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_directory(ec) && it->path().filename() == name) {
            std::error_code ec2;
            for (const auto& f : std::filesystem::recursive_directory_iterator(it->path(), ec2)) {
                if (f.path().extension() == ".png") {
                    return true;
                }
            }
        }
    }
    return false;
}

std::string AppVersion() {
    std::error_code ec;
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
    std::ifstream f(exe.parent_path() / "VERSION");
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}
} // namespace

void FrameMods::RefreshAngelsAim() {
    const u64 tid = title_id;
    if (!FrameXr::Kiu::IsKidIcarus(tid)) {
        angels_aim_report_url.clear();
        FrameXr::SetInGameMenuAngelsAim({});
        return;
    }
    const Mods::Compat::Game& g = GameInfo();
    std::string game = g.region == Mods::Compat::Region::Unknown
                           ? std::string("region unknown")
                           : Mods::Compat::RegionName(g.region);
    game += g.version ? " " + Mods::Compat::VersionText(*g.version) : std::string(" version unknown");
    OffThread(
        this,
        [tid, game] {
            Menu::AngelsAimModel m;
            m.shown = true;
            m.mod_installed = FrameXr::CStickModInstalled(tid);
            const std::filesystem::path dir =
                std::filesystem::path(CustomTex::DefaultPaths().load_dir) / "textures" /
                fmt::format("{:016X}", tid);
            m.weapons_pack = HasPackFolder(dir, "weapons");
            m.effects_pack = HasPackFolder(dir, "effects");
            m.report_url = Menu::AngelsAimReportUrl(AppVersion(), game, m.mod_installed);
            return m;
        },
        [this, tid](const Menu::AngelsAimModel& m) {
            if (tid != title_id) {
                return;
            }
            angels_aim_report_url = m.report_url;
            LOG_INFO(Frontend, "xr: Angel's Aim: the mod {}, weapon textures {}, effect textures {}",
                     m.mod_installed ? "installed" : "not installed",
                     m.weapons_pack ? "installed" : "not installed",
                     m.effects_pack ? "installed" : "not installed");
            FrameXr::SetInGameMenuAngelsAim(m);
        });
}

Core::AngelsReport::Info FrameMods::AngelsAimInfo(const std::string& game_title) {
    Core::AngelsReport::Info i;
    const auto& v = Settings::values;
    i.app_version = Common::g_build_fullname;
    i.release = Core::CrashReport::ReleaseName();
    i.title_id = title_id;
    i.title = game_title;
    const Mods::Compat::Game& g = GameInfo();
    if (g.region != Mods::Compat::Region::Unknown) {
        i.region = Mods::Compat::RegionName(g.region);
    }
    if (g.version) {
        i.game_version = Mods::Compat::VersionText(*g.version);
    }
    i.mod_installed = FrameXr::CStickModInstalled(title_id);
    i.aiming = static_cast<int>(v.vr_gyro_aim.GetValue());
    i.weapon_in_hand = v.vr_light_gun_weapon.GetValue();
    i.laser_sight = v.vr_light_gun_laser.GetValue();
    i.recoil = v.vr_light_gun_recoil.GetValue();
    i.recoil_charged = static_cast<int>(v.vr_light_gun_recoil_charged.GetValue());
    i.recoil_rapid = static_cast<int>(v.vr_light_gun_recoil_rapid.GetValue());
    i.weapon_size = static_cast<int>(v.vr_light_gun_weapon_size.GetValue());
    i.weapon_scale = static_cast<int>(v.vr_light_gun_weapon_scale.GetValue());
    i.weapon_opacity = static_cast<int>(v.vr_light_gun_weapon_opacity.GetValue()); // kiuopacity
    i.projectile_opacity = static_cast<int>(v.vr_light_gun_projectile_opacity.GetValue());
    i.flash_size = static_cast<int>(v.vr_light_gun_muzzle_flash_size.GetValue()); // kiuweap2
    i.gyro_sensitivity = static_cast<int>(std::lround(v.vr_gyro_sensitivity.GetValue() * 10.0f));
    i.turn_zone = static_cast<int>(v.vr_light_gun_turn_zone.GetValue());
    i.turn_speed = static_cast<int>(std::lround(v.vr_light_gun_turn_speed.GetValue() * 10.0f));
    i.turn_zone_from = static_cast<int>(v.vr_light_gun_turn_zone_from.GetValue()); // kiuaim3
    i.keep_on_screen = v.vr_light_gun_lock.GetValue();
    i.grip_toggle = v.vr_light_gun_grip.GetValue() == 1;
    i.immersive = v.vr_immersive_mode.GetValue() != 0;
    return i;
}

void FrameMods::RefreshMods() { // AzaharFrame ctgp7b
    if (title_id == 0) {
        return;
    }
    RefreshLoaded();
    if (mods.catalog == Menu::CatalogState::Ready) {
        RefreshAvailable();
    }
}

void FrameMods::RefreshAvailable() {
    // AzaharFrame stall: the list at once from the catalog in hand (the compiled-in copy, or the
    // one fetched earlier this run), filtered to this game; then, once per game a run, the
    // catalog's latest copy and the sources' dates in the background, and the list again. A
    // download or install never holds it back (each job is a thread of its own).
    ListAvailable(false);
    if (mods_network_title != title_id && !mods_network_running) {
        ListAvailable(true);
    }
}

void FrameMods::ListAvailable(bool network) {
    const u64 tid = title_id;
    // AzaharFrame modcat: the game's code CRC32, read here on the GUI thread (once per game), marks
    // the mods made for this build
    const Mods::Compat::Game game = GameInfo(); // AzaharFrame compat: with the CRC
    if (network) {
        mods_network_running = true;
        mods_network_title = tid;
    }
    OffThread(
        this,
        [tid, game, network] {
            using Clock = std::chrono::steady_clock;
            const auto t0 = Clock::now();
            Mods::Library& lib = Mods::Default();
            AvailableResult out;
            out.failed = false;
            Mods::Compat::Game game_in = game; // AzaharFrame versions: may learn its version below
            bool changed = false;
            if (network) {
                changed |= lib.RefreshCatalogOnce(); // one request a run
                Mods::Compat::UpdateVersion(game_in); // AzaharFrame versions: the online table may name it
                // AzaharFrame modcat: the sources' own dates, once a run; no new request after 8 s
                changed |= lib.RefreshDates(tid, Clock::now() + std::chrono::seconds(8)) > 0;
            }
            // AzaharFrame jsoncrash: an install holds the library for as long as it copies files;
            // the list used to wait behind it with the ring turning. It waits 20 s at most, then
            // says so (Couldn't Load Mods, Retry).
            const auto give_up = Clock::now() + std::chrono::seconds(20);
            while (lib.Busy() && Clock::now() < give_up) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (lib.Busy()) {
                LOG_WARNING(Frontend, "xr: mods: the library stayed busy for 20 s, no list");
                out.failed = true;
                return out;
            }
            out.list = lib.ListAvailable(tid, game_in.code_crc, &game_in);
            const auto ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
            LOG_INFO(Frontend, "xr: mods: {} catalog mods for {:016X} in {} ms ({})",
                     out.list.size(), tid, ms,
                     !network ? "the catalog in hand"
                              : changed ? "after the requests" : "the requests changed nothing");
            out.same = network && !changed;
            return out;
        },
        [this, tid, network](const AvailableResult& r) {
            if (network) {
                mods_network_running = false;
            }
            if (tid != title_id) {
                return; // another game now
            }
            if (r.failed) {
                if (mods.catalog != Menu::CatalogState::Ready) {
                    mods.catalog = Menu::CatalogState::Failed;
                    PublishMods();
                }
                return;
            }
            if (r.same) {
                return; // the same list
            }
            SetAvailable(r.list);
        });
}

void FrameMods::SetAvailable(const std::vector<Mods::AvailableMod>& list) {
    mods.available.clear();
    mod_sizes.clear();
    const Mods::Catalog catalog = Mods::Default().GetCatalog();
    for (const Mods::AvailableMod& a : list) {
        mod_sizes[a.id] = a.active || a.in_library ? 0 : a.size; // AzaharFrame catalog3
        Menu::ModOffer o;
        o.id = a.id;
        o.name = a.name;
        o.author = a.author;
        o.url = RepoUrl(catalog, a.id);
        o.updated = CustomTex::Download::UpdatedText(a.updated); // AzaharFrame modcat
        o.notes = a.notes; // AzaharFrame modcat2
        o.version = a.chip;
        o.version_tip = a.chip_tip;
        o.version_match = a.matches_game;
        o.recommended = a.recommended; // AzaharFrame fixes7: the badge
        o.downloads = a.downloads > 0 ? Menu::DownloadsText(a.downloads) : std::string{}; // AzaharFrame compat
        o.unscanned = a.unscanned;
        o.mod_version = a.version; // AzaharFrame ui3
        o.requires_ctgp7 = a.requires_pack == Mods::kRequiresCtgp7; // AzaharFrame ctgp7
        if (!a.compat.Fits()) { // AzaharFrame compat
            o.mismatch = true;
            o.region_chip = a.compat.RegionChip();
            o.mismatch_reason = a.compat.Reason();
            o.mismatch_body = a.compat.Body("mod");
        }
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
                     // AzaharFrame fixes7: a site's rate limit says it all
                     : Mods::Transfer::IsLimitError(failed_reason) ? failed_reason
                                                                   : "Install Failed: " + failed_reason; // AzaharFrame modfix
            o.error = true;
        }
        if (o.state == Menu::OfferState::Install && a.partial_bytes > 0) {
            o.resume = Menu::ResumeText(a.partial_bytes, a.size); // AzaharFrame fixes7: Resume
        }
        mods.available.push_back(std::move(o));
    }
    mods.catalog = Menu::CatalogState::Ready;
    PublishMods();
}

Menu::ModOffer* FrameMods::Offer(const std::string& id) {
    for (Menu::ModOffer& o : mods.available) {
        if (o.id == id) {
            return &o;
        }
    }
    return nullptr;
}

void FrameMods::SetTexturesEnabler(std::function<bool()> enabler) {
    textures_enabler = std::move(enabler);
}

void FrameMods::SetNotifier(std::function<void(const std::string&)> n) {
    notifier = std::move(n);
}

void FrameMods::Install(const std::string& id, bool replace, bool confirmed) {
    if (mods.busy || id.empty()) {
        return;
    }
    // AzaharFrame catalog3: a download over 1 GB says how big first
    if (!confirmed && !replace) {
        const auto big = mod_sizes.find(id);
        const Menu::ModOffer* offer = Offer(id);
        if (big != mod_sizes.end() && big->second > 1'000'000'000 && offer != nullptr) {
            pending = {Pending::Kind::BigInstall, id};
            mods.dialog = {Menu::ModsDialogKind::BigDownload, offer->name,
                           CustomTex::Download::SizeText(big->second)};
            PublishMods();
            return;
        }
    }
    mods.notice.clear();
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
    const bool other = anyway.count(id) != 0;             // AzaharFrame compat
    const bool unscanned = unscanned_ok.count(id) != 0;   // AzaharFrame compat
    OffThread(
        this,
        [tid, id, replace, unscanned, other] {
            return Mods::Default().Install(tid, id, replace, unscanned, other);
        },
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
                // AzaharFrame catalog3: a textures part needs Custom Textures on for this game
                if (out.textures_installed && textures_enabler && textures_enabler()) {
                    mods.notice = "Installed. Custom Textures turned on for this game.";
                }
                // the running game's folders are not touched: it is downloaded and goes in at the next start
                if (out.staged && notifier) {
                    notifier(name + " Downloaded. Restart to Enable.");
                }
                // AzaharFrame compat: new until the game has a clean run with it
                FrameCrashGuard::NoteOn(tid, {Mods::CrashGuard::Item::Kind::Mod, id, name});
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
                if (out.error == "Cancelled") { // AzaharFrame integ3: back to Install
                    LOG_INFO(Frontend, "xr: mods: install {} cancelled", id);
                    break;
                }
                // AzaharFrame compat: GameBanana has not scanned the file yet: ask (only that)
                if (out.unscanned && unscanned_ok.count(id) == 0) {
                    LOG_INFO(Frontend, "xr: mods: {} is not scanned yet: asking", id);
                    pending = {Pending::Kind::Unscanned, id};
                    mods.dialog = {Menu::ModsDialogKind::Unscanned, name, "Download Anyway"};
                    break;
                }
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
        // AzaharFrame fixes7: the size too, so a slow start still shows it moving
        text += " \u00b7 " + Menu::ProgressSizeText(st.done, st.total);
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
                FrameCrashGuard::NoteOff(tid, Mods::CrashGuard::Item::Kind::Mod, id); // AzaharFrame compat
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
                FrameCrashGuard::NoteOff(tid, Mods::CrashGuard::Item::Kind::Mod, id); // AzaharFrame compat
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

// AzaharFrame compat: the "Turned Off" snackbar's Turn Back On
void FrameMods::TurnBackOn() {
    if (mods.turned_off.empty()) {
        return;
    }
    const u64 tid = title_id;
    mods.turned_off.clear();
    PublishMods();
    FrameCrashGuard::Get().TurnBackOn([this, tid](bool ok) {
        if (tid != title_id) {
            return;
        }
        if (!ok) {
            FrameXr::ShowToastText("Not Turned Back On", FrameXr::ToastTime::Long);
        }
        mods.needs_restart = true;
        textures.needs_restart = true;
        RefreshLoaded();
        RefreshPacks();
        if (mods.catalog == Menu::CatalogState::Ready) {
            RefreshAvailable();
        }
        PublishMods();
        PublishTextures();
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
            // AzaharFrame texlayers: the packs, their parts and the game's list, read once
            auto game = std::make_shared<CustomTex::Layers::Game>(
                CustomTex::Layers::Game::Load(CustomTex::DefaultPaths(), tid));
            // AzaharFrame zipinstall: the packs Install from Zip made can be deleted too
            std::set<std::string> zipped;
            // AzaharFrame texparts: which packs have Customize, and "6 of 7 Parts"
            std::map<std::string, PackParts> parts;
            for (const CustomTex::Pack& p : game->packs) {
                const auto id = p.managed ? CustomTex::Download::MarkerId(p.path) : std::nullopt;
                if (id && id->rfind("zip:", 0) == 0) {
                    zipped.insert(std::filesystem::path(p.path).filename().string());
                }
                namespace P = CustomTex::Parts;
                const CustomTex::Layers::PackData& d = game->data[p.id];
                parts[p.id] = {d.layout.PartCount(), P::PartsOnText(d.layout, d.state),
                               // AzaharFrame ui3: Optimize for Frame is on the pack's page
                               p.format == CustomTex::Format::Png ||
                                   CustomTex::Optimize::IsOptimized(p.path)};
            }
            return std::make_tuple(std::move(game), std::move(zipped), std::move(parts));
        },
        [this, tid](const std::tuple<std::shared_ptr<CustomTex::Layers::Game>, std::set<std::string>,
                                     std::map<std::string, PackParts>>& result) {
            if (tid != title_id) {
                return;
            }
            layers = std::get<0>(result);
            zip_packs = std::get<1>(result);
            pack_parts = std::get<2>(result);
            // AzaharFrame ui3, texrow: the switches waiting for the next start show as chosen, in
            // the pack rows and in Enabled Textures (the disk keeps what the game reads now)
            for (const PendingSwitch& s : pending_switches) {
                if (s.title == tid) {
                    layers->Wait({s.carry_from, s.path});
                }
            }
            packs = layers->packs;
            FillPackRows();
            BuildLayerRows();
            // AzaharFrame texrow: an open Customize page of a pair follows its row (Optimize, Use
            // Original): its header, sizes and parts are the copy the row shows
            if (const auto o = layers->partners.find(textures.parts.pack_id);
                o != layers->partners.end()) {
                for (const Menu::TexturePackItem& item : textures.packs) {
                    if (item.id == o->second) {
                        OpenParts(item.id);
                        break;
                    }
                }
            }
            UpdateOptimizeRow(); // AzaharFrame ui3
            CheckNudge();
            PublishTextures();
            ComputeSizes();
        });
}

// AzaharFrame texlayers: Enabled Textures, when two or more packs are on.
void FrameMods::BuildLayerRows() {
    textures.layers.clear();
    if (!layers || CustomTex::Layers::Packs(layers->list).size() < 2) {
        return;
    }
    for (const CustomTex::Layers::Row& r : layers->Rows()) {
        textures.layers.push_back({r.name, r.detail, r.value, r.up, r.down});
    }
}

// AzaharFrame texrow: one row a pack, a pair one row (CustomTex::Layers::PackRows)
void FrameMods::FillPackRows() {
    textures.packs.clear();
    if (!layers) {
        return;
    }
    for (const CustomTex::Layers::PackRow& r :
         CustomTex::Layers::PackRows(packs, layers->list, layers->partners, layers->copies)) {
        const CustomTex::Pack& p = packs[r.shown];
        const std::string folder = std::filesystem::path(p.path).filename().string();
        const bool mine =
            p.managed && (downloaded.count(folder) != 0 || zip_packs.count(folder) != 0);
        Menu::TexturePackItem item{p.id, p.name, PackDetail(p), r.on, mine};
        if (const auto it = pack_parts.find(p.id); it != pack_parts.end()) {
            // AzaharFrame texparts; ui3: a PNG or optimized pack has its page too
            item.customize = it->second.count >= 2 || it->second.optimizable;
            item.detail = Menu::JoinDetail({item.detail, it->second.on_text});
        }
        textures.packs.push_back(std::move(item));
    }
}

/// The pack rows' "6 of 7 Parts" after the list changed a pack's parts.
void FrameMods::LayersChanged(const std::string& pack) {
    namespace P = CustomTex::Parts;
    if (const auto d = layers->data.find(pack); d != layers->data.end()) {
        pack_parts[pack].on_text = P::PartsOnText(d->second.layout, d->second.state);
        FillPackRows();
    }
    const CustomTex::Result r = layers->Save(CustomTex::DefaultPaths(), title_id);
    if (!r.ok) {
        LOG_WARNING(Frontend, "xr: texture layers: cannot save: {}", r.error);
        FrameXr::ShowToastText("Texture Pack Not Changed", FrameXr::ToastTime::Long);
    }
    textures.needs_restart = true;
    BuildLayerRows();
    PublishTextures();
}

void FrameMods::ToggleLayer(int row) {
    if (!layers || textures.busy || row < 0 ||
        row >= static_cast<int>(layers->list.entries.size())) {
        return;
    }
    const std::string pack = layers->list.entries[static_cast<std::size_t>(row)].pack;
    if (layers->Toggle(static_cast<std::size_t>(row))) {
        LOG_INFO(Frontend, "xr: texture layers: {} on/off in {}",
                 layers->list.entries[static_cast<std::size_t>(row)].key, pack);
        LayersChanged(pack);
    }
}

void FrameMods::MoveLayer(int row, int delta) {
    if (!layers || textures.busy || row < 0 ||
        row >= static_cast<int>(layers->list.entries.size())) {
        return;
    }
    const std::string pack = layers->list.entries[static_cast<std::size_t>(row)].pack;
    if (layers->Move(static_cast<std::size_t>(row), delta)) {
        LOG_INFO(Frontend, "xr: texture layers: row {} moved {}", row, delta < 0 ? "up" : "down");
        LayersChanged(pack);
    }
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
            for (CustomTex::Pack& p : packs) {
                p.size_bytes = CustomTex::CachedSize(p.path);
            }
            FillPackRows(); // AzaharFrame texrow
            UpdateOptimizeRow(); // AzaharFrame ui3: the sizes
            CheckNudge();
            PublishTextures();
        });
}

void FrameMods::SelectPack(const std::string& id, bool on) {
    // AzaharFrame texlayers: a pack's checkbox: on (at its place in the list) or off
    if (textures.busy || !layers) {
        return;
    }
    const CustomTex::Pack* pack = layers->Find(id);
    if (!pack) {
        return;
    }
    const std::string name = pack->name;
    // AzaharFrame ui3: this choice wins over its pair's waiting switch (texrow: only that pair's;
    // the switches of other packs stay)
    if (const PendingSwitch* s = PendingFor(title_id, id); s && (!on || s->path != id)) {
        LOG_INFO(Frontend, "xr: texture packs: the waiting switch to {} is dropped", s->path);
        DropPending(title_id, id);
    }
    textures.nudge_pack.clear(); // AzaharFrame ui3
    const std::vector<std::string> before = CustomTex::Layers::Packs(layers->list);
    layers->SetOn(id, on);
    const std::vector<std::string> after = CustomTex::Layers::Packs(layers->list);
    for (CustomTex::Pack& p : packs) {
        p.active = layers->IsOn(p.id);
    }
    FillPackRows(); // at once; the list is read again after
    BuildLayerRows();
    textures.busy = true;
    PublishTextures();
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: texture packs: {} {}", name, on ? "on" : "off");
    const auto game = std::make_shared<CustomTex::Layers::Game>(*layers);
    OffThread(
        this, [tid, game] { return game->Save(CustomTex::DefaultPaths(), tid); },
        [this, tid, before, after, name](const CustomTex::Result& out) {
            textures.busy = false;
            if (tid != title_id) {
                return;
            }
            if (out.ok) {
                textures.needs_restart = true;
                // AzaharFrame compat: a pack turned on is new until a clean run
                using Kind = Mods::CrashGuard::Item::Kind;
                for (const std::string& p : before) {
                    if (std::find(after.begin(), after.end(), p) == after.end()) {
                        FrameCrashGuard::NoteOff(tid, Kind::Pack, p);
                    }
                }
                for (const std::string& p : after) {
                    if (std::find(before.begin(), before.end(), p) == before.end()) {
                        const CustomTex::Pack* added = layers ? layers->Find(p) : nullptr;
                        FrameCrashGuard::NoteOn(tid, {Kind::Pack, p, added ? added->name : name});
                        nudge_candidates.insert(p); // AzaharFrame ui3
                    }
                }
            } else {
                LOG_WARNING(Frontend, "xr: texture packs: switch failed: {}", out.error);
                FrameXr::ShowToastText("Texture Pack Not Changed", FrameXr::ToastTime::Long);
            }
            RefreshPacks();
        });
}

void FrameMods::PublishTextures() {
    FrameXr::SetInGameMenuTextures(textures);
}

// ---- AzaharFrame texparts: a pack's Customize page ----

void FrameMods::OpenParts(const std::string& pack_id) {
    const auto pack = std::find_if(packs.begin(), packs.end(),
                                   [&](const CustomTex::Pack& p) { return p.id == pack_id; });
    if (pack == packs.end()) {
        return;
    }
    const int serial = ++parts_serial;
    parts_path = pack->path;
    parts_layout = {};
    parts_state = {};
    part_actions.clear();
    part_previews.clear();
    textures.parts = {};
    textures.parts.pack_id = pack_id;
    textures.parts.title = pack->name;
    textures.parts.author = pack->author;
    textures.parts.rest = pack->version;
    textures.parts.loading = true;
    if (textures.nudge_pack == pack_id) { // AzaharFrame ui3: answered
        textures.nudge_pack.clear();
    }
    UpdateOptimizeRow(); // AzaharFrame ui3
    PublishTextures();
    const std::string path = pack->path;
    OffThread(
        this,
        [path] {
            namespace P = CustomTex::Parts;
            P::State state = P::ReadState(path);
            P::Layout layout = P::Scan(path, state.hints);
            std::map<std::string, std::shared_ptr<const std::vector<uint8_t>>> previews;
            for (const P::Group& g : layout.groups) {
                for (const P::Part& p : g.parts) {
                    if (!p.preview.empty()) {
                        if (auto px = PartPreview(p.preview, kPreviewPx)) {
                            previews[p.key] = std::move(px);
                        }
                    }
                }
            }
            return std::make_tuple(std::move(layout), std::move(state), std::move(previews));
        },
        [this, serial, pack_id](const std::tuple<CustomTex::Parts::Layout, CustomTex::Parts::State,
                                                 decltype(part_previews)>& result) {
            if (serial != parts_serial || textures.parts.pack_id != pack_id) {
                return;
            }
            parts_layout = std::get<0>(result);
            parts_state = std::get<1>(result);
            part_previews = std::get<2>(result);
            LOG_INFO(Frontend, "xr: texture parts: {} has {} parts in {} groups", parts_path,
                     parts_layout.PartCount(), parts_layout.groups.size());
            BuildPartRows();
            PublishTextures();
        });
}

void FrameMods::BuildPartRows() {
    namespace P = CustomTex::Parts;
    const P::Layout& l = parts_layout;
    const P::State& st = parts_state;
    Menu::TexturePartsModel& m = textures.parts;
    m.loading = false;
    m.rows.clear();
    part_actions.clear();
    m.note = P::AnyConflicts(l);
    m.all_buttons = std::any_of(l.groups.begin(), l.groups.end(),
                                [](const P::Group& g) { return g.kind != P::Kind::Choice; });
    const auto [on_textures, on_bytes] = P::TotalsOn(l, st);
    const auto pack = std::find_if(packs.begin(), packs.end(), [&](const CustomTex::Pack& p) {
        return p.id == m.pack_id;
    });
    m.rest = Menu::JoinDetail({pack != packs.end() ? pack->version : std::string{},
                               P::CountText(on_textures, "Texture On", "Textures On"),
                               P::SizeText(on_bytes)});
    const auto preview = [this](Menu::TexturePartRow& row, const std::string& key) {
        if (const auto it = part_previews.find(key); it != part_previews.end()) {
            row.preview = it->second;
            row.preview_px = kPreviewPx;
        }
    };
    const std::vector<std::size_t> order = P::Order(l, st);
    for (std::size_t pos = 0; pos < order.size(); pos++) {
        const std::size_t gi = order[pos];
        const P::Group& g = l.groups[gi];
        const bool arrows = P::CanMove(l, g);
        const auto with_arrows = [&](Menu::TexturePartRow& row) {
            row.arrows = arrows;
            row.up = arrows && pos > 0;
            row.down = arrows && pos + 1 < order.size();
        };
        Menu::TexturePartRow row;
        switch (g.kind) {
        case P::Kind::Single:
            row.kind = Menu::PartRowKind::Check;
            row.name = g.name;
            row.detail = P::PartDetail(l, g.parts[0]);
            row.value = P::IsOn(l, st, g, g.parts[0]) ? 2 : 0;
            preview(row, g.parts[0].key);
            with_arrows(row);
            m.rows.push_back(std::move(row));
            part_actions.push_back({PartAction::Kind::Part, gi, g.parts[0].key});
            break;
        case P::Kind::Folder:
            row.kind = Menu::PartRowKind::Folder;
            row.name = g.name;
            row.detail = P::GroupDetail(l, g);
            row.value = P::GroupValue(l, st, g);
            with_arrows(row);
            m.rows.push_back(std::move(row));
            part_actions.push_back({PartAction::Kind::Folder, gi, {}});
            for (const P::Part& p : g.parts) {
                Menu::TexturePartRow child;
                child.kind = Menu::PartRowKind::Check;
                child.name = p.name;
                child.detail = P::PartDetail(l, p);
                child.value = P::IsOn(l, st, g, p) ? 2 : 0;
                child.child = true;
                preview(child, p.key);
                m.rows.push_back(std::move(child));
                part_actions.push_back({PartAction::Kind::Part, gi, p.key});
            }
            break;
        case P::Kind::Choice: {
            row.kind = Menu::PartRowKind::ChoiceTitle;
            row.name = g.name;
            with_arrows(row);
            m.rows.push_back(std::move(row));
            part_actions.push_back({});
            const std::string chosen = P::Chosen(l, st, g);
            Menu::TexturePartRow off;
            off.kind = Menu::PartRowKind::ChoiceOff;
            off.name = "Off";
            off.value = chosen.empty() ? 2 : 0;
            m.rows.push_back(std::move(off));
            part_actions.push_back({PartAction::Kind::Choose, gi, {}});
            for (const P::Part& p : g.parts) {
                Menu::TexturePartRow choice;
                choice.kind = Menu::PartRowKind::Choice;
                choice.name = p.name;
                choice.detail = P::PartDetail(l, p);
                choice.value = p.key == chosen ? 2 : 0;
                preview(choice, p.key);
                m.rows.push_back(std::move(choice));
                part_actions.push_back({PartAction::Kind::Choose, gi, p.key});
            }
            break;
        }
        }
    }
}

void FrameMods::PartClicked(int row) {
    if (row < 0 || row >= static_cast<int>(part_actions.size()) || textures.parts.loading) {
        return;
    }
    namespace P = CustomTex::Parts;
    const PartAction a = part_actions[static_cast<std::size_t>(row)];
    if (a.group >= parts_layout.groups.size()) {
        return;
    }
    const P::Group& g = parts_layout.groups[a.group];
    switch (a.kind) {
    case PartAction::Kind::Part: {
        const P::Part* p = parts_layout.FindPart(a.key);
        if (!p) {
            return;
        }
        P::SetPart(parts_state, parts_layout, a.key, !P::IsOn(parts_layout, parts_state, g, *p));
        break;
    }
    case PartAction::Kind::Folder:
        P::SetGroup(parts_state, parts_layout, g, P::GroupValue(parts_layout, parts_state, g) != 2);
        break;
    case PartAction::Kind::Choose:
        P::Choose(parts_state, parts_layout, g, a.key);
        break;
    case PartAction::Kind::None:
        return;
    }
    PartsChanged();
}

void FrameMods::MovePart(int row, int delta) {
    if (row < 0 || row >= static_cast<int>(part_actions.size()) || textures.parts.loading) {
        return;
    }
    // The row's group: a choice's title row has no action of its own, the next row does.
    std::size_t at = static_cast<std::size_t>(row);
    while (at < part_actions.size() && part_actions[at].kind == PartAction::Kind::None) {
        at++;
    }
    if (at >= part_actions.size() || part_actions[at].group >= parts_layout.groups.size()) {
        return;
    }
    CustomTex::Parts::Move(parts_state, parts_layout,
                           parts_layout.groups[part_actions[at].group].key, delta);
    PartsChanged();
}

void FrameMods::AllParts(bool on) {
    if (textures.parts.loading || parts_layout.groups.empty()) {
        return;
    }
    CustomTex::Parts::SetAll(parts_state, parts_layout, on);
    PartsChanged();
}

void FrameMods::PartsChanged() {
    namespace P = CustomTex::Parts;
    if (!P::WriteState(parts_path, parts_state)) {
        LOG_WARNING(Frontend, "xr: texture parts: cannot save {}", parts_path);
        FrameXr::ShowToastText("Texture Pack Not Changed", FrameXr::ToastTime::Long);
        return;
    }
    const auto [on, all] = P::CountOn(parts_layout, parts_state);
    LOG_INFO(Frontend, "xr: texture parts: {} of {} parts on in {}", on, all, parts_path);
    // AzaharFrame texlayers: the game's list follows the pack's order (and its checkboxes)
    if (layers && layers->IsOn(textures.parts.pack_id)) {
        layers->PackChanged(textures.parts.pack_id, parts_state);
        if (const CustomTex::Result r = layers->Save(CustomTex::DefaultPaths(), title_id); !r.ok) {
            LOG_WARNING(Frontend, "xr: texture layers: cannot save: {}", r.error);
        }
        BuildLayerRows();
    } else if (layers) {
        layers->PackChanged(textures.parts.pack_id, parts_state);
    }
    BuildPartRows();
    for (const CustomTex::Pack& p : packs) {
        if (p.id != textures.parts.pack_id) {
            continue;
        }
        pack_parts[p.id].on_text = P::PartsOnText(parts_layout, parts_state);
        if (p.active) {
            textures.needs_restart = true; // the loader reads the parts at the next start
        }
    }
    FillPackRows(); // AzaharFrame texrow
    PublishTextures();
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
    const Mods::Compat::Game game = GameInfo(); // AzaharFrame compat: other versions' packs too
    OffThread(
        this, [dl, tid, game] { return dl->ListForTitle(tid, &game); },
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
        o.recommended = p.recommended; // AzaharFrame fixes7: the badge
        o.notes = p.notes; // AzaharFrame modcat2
        o.downloads = p.downloads > 0 ? Menu::DownloadsText(p.downloads) : std::string{}; // AzaharFrame compat
        o.unscanned = p.unscanned;
        if (!p.compat.Fits()) { // AzaharFrame compat
            o.mismatch = true;
            o.region_chip = p.compat.RegionChip();
            o.mismatch_reason = p.compat.Reason();
            o.mismatch_body = p.compat.Body("texture pack");
        }
        if (running && st.pack_id == p.id) {
            o.state = Menu::PackOfferState::Downloading;
        } else if (p.installed) {
            o.state = Menu::PackOfferState::Installed;
        } else if (p.id == pack_failed) {
            o.state = Menu::PackOfferState::Failed;
            o.note = pack_error;
        }
        if (o.state == Menu::PackOfferState::Download && p.partial) {
            // AzaharFrame fixes7: a cancelled download (or one the app was closed in) goes on: Resume
            o.resume = Menu::ResumeText(p.partial_bytes, p.size);
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

// AzaharFrame compat: what the player allowed with Install Anyway / Download Anyway (the bits of
// Menu::kAnywayOtherVersion and Menu::kAnywayUnscanned).
void FrameMods::AllowAnyway(const std::string& id, int value) {
    if (value & Menu::kAnywayOtherVersion) {
        anyway.insert(id);
    }
    if (value & Menu::kAnywayUnscanned) {
        unscanned_ok.insert(id);
    }
}

void FrameMods::StartDownload(const std::string& id) {
    Menu::PackOffer* o = PackOffer(id);
    if (!o || !downloader || textures.downloading) {
        return;
    }
    if (!downloader->Start(id, title_id, unscanned_ok.count(id) != 0,
                           anyway.count(id) != 0)) { // returns at once; the work runs on its thread
        LOG_WARNING(Frontend, "xr: texture packs: download {} did not start", id);
        return;
    }
    LOG_INFO(Frontend, "xr: texture packs: download {}", id);
    download_title = title_id;
    pack_failed.clear();
    pack_error.clear();
    o->state = Menu::PackOfferState::Downloading;
    o->percent = -1;
    o->note = CustomTex::Download::kPreparing; // AzaharFrame fixes7: until the file's bytes start
    o->resume.clear();
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
            next.resume.clear();
            next.note = st.text;
            next.percent = st.phase == Phase::Downloading && st.bytes_total > 0
                               ? static_cast<int>(std::min<u64>(
                                     st.bytes_done * 100 / st.bytes_total, 100))
                               : -1;
            if (st.phase == Phase::Downloading) {
                // AzaharFrame fixes7: "120 MB of 3.4 GB": a big pack on a slow link moves here long
                // before the percent leaves 0
                next.note = Menu::ProgressSizeText(st.bytes_done, st.bytes_total);
            }
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
            nudge_candidates.insert("folder:" + f); // AzaharFrame ui3
        }
        RefreshPacks();
        break;
    case Phase::Failed:
        // AzaharFrame compat: GameBanana has not scanned the file yet: ask (only that)
        if (st.unscanned && unscanned_ok.count(st.pack_id) == 0) {
            LOG_INFO(Frontend, "xr: texture packs: {} is not scanned yet: asking", st.pack_id);
            const Menu::PackOffer* o = PackOffer(st.pack_id);
            pack_pending = {Pending::Kind::Unscanned, st.pack_id};
            textures.dialog = {Menu::ModsDialogKind::Unscanned, o ? o->name : st.pack_id,
                               "Download Anyway"};
            break;
        }
        LOG_WARNING(Frontend, "xr: texture packs: download {} failed: {}", st.pack_id, st.error);
        pack_failed = st.pack_id;
        pack_error = st.error.empty() ? std::string("Download Failed") : st.error;
        break;
    default: // Cancelled: the partial download is kept, Download goes on from there
        break;
    }
    ListPackDownloads();
}

void FrameMods::DeletePack(const std::string& folder, const std::string& other) {
    if (!downloader || textures.busy || textures.downloading) {
        return;
    }
    bool was_active = false;
    for (const CustomTex::Pack& p : packs) {
        const std::string f = std::filesystem::path(p.path).filename().string();
        if (p.managed && (f == folder || (!other.empty() && f == other))) {
            // texrow: what the game reads now (a waiting switch's `from` too)
            was_active |= p.active || PendingFor(title_id, p.id) != nullptr;
            DropPending(title_id, p.id); // AzaharFrame texrow: the pair goes, and its switch
        }
    }
    textures.busy = true;
    PublishTextures();
    const auto dl = downloader;
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: texture packs: delete {}{}", folder, other.empty() ? "" : " and " + other);
    OffThread(
        this,
        [dl, tid, folder, other] {
            // AzaharFrame texrow: the pair's other copy first, then the one the row showed
            if (!other.empty()) {
                if (CustomTex::Download::Result r = dl->Delete(tid, other); !r.ok) {
                    return r;
                }
            }
            return dl->Delete(tid, folder);
        },
        [this, tid, folder, other, was_active](const CustomTex::Download::Result& out) {
            textures.busy = false;
            if (tid != title_id) {
                return;
            }
            if (out.ok) {
                downloaded.erase(folder);
                downloaded.erase(other);
                textures.needs_restart |= was_active; // the game still has its textures
            } else {
                LOG_WARNING(Frontend, "xr: texture packs: delete {} failed: {}", folder, out.error);
                FrameXr::ShowToastText("Texture Pack Not Deleted", FrameXr::ToastTime::Long);
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
                return ZipDone(texture, tid, texture ? std::string{} : out.installed);
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
                    return ZipDone(false, tid, id);
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

void FrameMods::ZipDone(bool texture, u64 tid, const std::string& mod_id) {
    if (!mod_id.empty()) { // AzaharFrame compat: new until a clean run
        FrameCrashGuard::NoteOn(tid, {Mods::CrashGuard::Item::Kind::Mod, mod_id,
                                      zip_job ? zip_job->name : mod_id});
    }
    ZipCloseJob();
    zip.working = false;
    zip.open = false;
    zip.error.clear();
    zip.file.clear();
    PublishZip();
    const bool here = tid == title_id;
    FrameXr::ShowToastText(!here      ? "Installed for Another Game"
                           : texture ? "Texture Pack Installed"
                                     : "Mod Installed",
                           FrameXr::ToastTime::Long);
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
            FrameXr::ShowToastText(Menu::CheatsAddedText(added), FrameXr::ToastTime::Long);
        });
}
