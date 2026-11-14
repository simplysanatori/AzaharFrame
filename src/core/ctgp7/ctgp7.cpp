// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ctgp7: see ctgp7.h.

#include "core/ctgp7/ctgp7.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md5.h>
#include <fmt/format.h>
#include <json.hpp>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/cheats/game_version.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/mods/archive.h"
#include "core/mods/catalog.h"
#include "core/mods/multipart.h"

namespace fs = std::filesystem;

namespace Ctgp7 {

namespace {

struct Mk7 {
    u64 title_id;
    Region region;
};
// Mario Kart 7's releases CTGP-7 supports ("Only EUR/USA/JPN/KOR games", its launcher says). The
// Chinese (0004000000 08B400) and Taiwanese (08B500) releases are not among them.
constexpr std::array<Mk7, 4> kMk7{{
    {0x0004000000030800ULL, Region::Usa},
    {0x0004000000030700ULL, Region::Eur},
    {0x0004000000030600ULL, Region::Jpn},
    {0x0004000000030A00ULL, Region::Kor},
}};

bool NonEmptyFile(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec) && fs::file_size(path, ec) > 0;
}

bool NonEmptyDir(const std::string& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        return false;
    }
    return fs::directory_iterator(path, ec) != fs::directory_iterator();
}

std::string WithSlash(std::string s) {
    if (!s.empty() && s.back() != '/') {
        s += '/';
    }
    return s;
}

/// The host part ("https://files.gamebanana.com") and the rest of an https URL.
bool SplitUrl(const std::string& url, std::string& host, std::string& path) {
    constexpr std::string_view scheme = "https://";
    if (url.rfind(scheme, 0) != 0) {
        return false;
    }
    const auto slash = url.find('/', scheme.size());
    host = url.substr(0, slash);
    path = slash == std::string::npos ? "/" : url.substr(slash);
    return host.size() > scheme.size() && host.find('@') == std::string::npos &&
           host.find(':', scheme.size()) == std::string::npos;
}

} // namespace

Region RegionOf(u64 title_id) {
    for (const Mk7& m : kMk7) {
        if (m.title_id == title_id) {
            return m.region;
        }
    }
    return Region::None;
}

std::string_view RegionName(Region region) {
    switch (region) {
    case Region::Usa:
        return "USA";
    case Region::Eur:
        return "Europe";
    case Region::Jpn:
        return "Japan";
    case Region::Kor:
        return "Korea";
    case Region::None:
        break;
    }
    return "";
}

bool SegmentShown(u64 running_title_id, bool installing) {
    return installing || IsSupportedMarioKart7(running_title_id) ||
           running_title_id == kLauncherTitleId;
}

bool UpdateReady(std::optional<u16> update_version) {
    return update_version.has_value() && *update_version >= kUpdateV12;
}

Blocker BlockerOf(const GameCheck& check) {
    if (!IsSupportedMarioKart7(check.title_id)) {
        return Blocker::NotMarioKart7;
    }
    if (!UpdateReady(check.update_version)) {
        return Blocker::Update;
    }
    if (!check.save_data) {
        return Blocker::SaveData;
    }
    return Blocker::None;
}

GameCheck CheckGame(u64 title_id) {
    using Service::FS::MediaType;
    GameCheck c;
    c.title_id = title_id;
    if (!IsSupportedMarioKart7(title_id)) {
        return c;
    }
    const std::string tmd = Service::AM::GetTitleMetadataPath(MediaType::SDMC, UpdateTitleOf(title_id));
    if (!tmd.empty() && FileUtil::Exists(tmd)) {
        std::string data;
        if (FileUtil::ReadFileToString(true, tmd, data) > 0) {
            const auto info = Cheats::Sharkive::ParseTmd(
                std::span<const u8>(reinterpret_cast<const u8*>(data.data()), data.size()));
            if (info && info->title_id == UpdateTitleOf(title_id)) {
                c.update_version = info->version;
            }
        }
    }
    // The save data archive Azahar mounts for the game, cartridge or installed alike
    // (archive_source_sd_savedata.cpp: <sdmc title>/<high>/<low>/data/00000001/).
    c.save_data = NonEmptyDir(Service::AM::GetTitlePath(MediaType::SDMC, title_id) + "data/00000001/");
    c.installed = TitleInstalled(title_id);
    return c;
}

Paths PlanPaths(const std::string& sdmc_dir, const std::string& user_dir) {
    Paths p;
    p.sdmc = WithSlash(sdmc_dir);
    p.dir = p.sdmc + "CTGP-7/";
    p.version_file = p.dir + "config/version.bin";
    p.plugin = p.dir + "resources/CTGP-7.3gx";
    p.work = WithSlash(user_dir) + "ctgp7/";
    p.zip = p.work + "ctgp-7_cia_offline_installer.zip";
    p.unpacked = p.work + "unpacked/";
    p.game_cia = p.work + "mario-kart-7.cia";
    return p;
}

Paths DefaultPaths() {
    return PlanPaths(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                     FileUtil::GetUserPath(FileUtil::UserPath::UserDir));
}

bool TitleInstalled(u64 title_id) {
    const std::string tmd =
        Service::AM::GetTitleMetadataPath(Service::FS::MediaType::SDMC, title_id);
    return !tmd.empty() && FileUtil::Exists(tmd);
}

std::string LauncherCia(const Paths& paths) {
    return paths.dir + "cia/CTGP-7.cia";
}

Layout ReadLayout(const Paths& paths, bool launcher_title_installed) {
    Layout l;
    l.plugin = NonEmptyFile(paths.plugin);
    l.citra_flag = FileUtil::Exists(paths.dir + "config/citra.flag");
    l.launcher_cia = NonEmptyFile(LauncherCia(paths));
    l.launcher_title = launcher_title_installed;
    return l;
}

Found Detect(const Layout& l) {
    if (!l.plugin) {
        return Found::None;
    }
    if (!l.citra_flag) {
        return Found::NeedsInstaller; // the console's files: the emulator's come from the installer
    }
    if (l.launcher_title) {
        return Found::Installed;
    }
    return l.launcher_cia ? Found::FinishSetup : Found::NeedsInstaller;
}

Found DetectInstall(const Paths& paths) {
    return Detect(ReadLayout(paths, TitleInstalled(kLauncherTitleId)));
}

bool ModInstalled(const Paths& paths) {
    return DetectInstall(paths) == Found::Installed;
}

namespace {
std::string TitleDir(const std::string& title_root, u64 tid) {
    return fmt::format("{}{:08x}/{:08x}/", WithSlash(title_root), static_cast<u32>(tid >> 32),
                       static_cast<u32>(tid));
}
bool Under(const std::string& path, const std::string& root) {
    return !root.empty() && path.size() > root.size() && path.compare(0, root.size(), root) == 0 &&
           path.find("/../") == std::string::npos;
}
} // namespace

UninstallPlan PlanUninstall(const Paths& paths, const std::string& title_root,
                            const std::string& tickets, bool delete_progress, u64 game) {
    UninstallPlan plan;
    std::error_code ec;
    const auto add = [&](const std::string& p) {
        if (fs::exists(fs::path(p.back() == '/' ? p.substr(0, p.size() - 1) : p), ec)) {
            plan.remove.push_back(p);
        }
    };
    add(paths.dir);                    // sdmc/CTGP-7/: gamefs, MyStuff, resources, config, cia
    add(paths.sdmc + "CTGP-7tmp/");    // an installer's leftovers
    add(paths.sdmc + "3ds/CTGP-7/");   // the launcher's .3dsx, the console installer puts there
    if (delete_progress) {
        add(paths.sdmc + "CTGP-7savebak/");
        add(paths.sdmc + "CTGP-7savebakCitra/");
    } else {
        for (const char* p : {"savefs", "savefsCitra"}) {
            if (fs::is_directory(paths.dir + p, ec)) {
                plan.keep.push_back(paths.dir + p + "/");
            }
        }
    }
    add(TitleDir(title_root, kLauncherTitleId)); // the launcher title (its own data with it)
    if (game != 0 && IsSupportedMarioKart7(game)) {
        add(TitleDir(title_root, game) + "content/"); // never its data/ (the save)
    }
    // the tickets of those titles
    for (fs::directory_iterator it(tickets, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        const auto is = [&name](u64 tid) {
            return name.rfind(fmt::format("{:016X}.", tid), 0) == 0 && name.size() > 4 &&
                   name.compare(name.size() - 4, 4, ".tik") == 0;
        };
        if (is(kLauncherTitleId) || (game != 0 && IsSupportedMarioKart7(game) && is(game))) {
            plan.remove.push_back(it->path().string());
        }
    }
    add(paths.work); // the app's downloads and notes
    return plan;
}

std::string RunUninstall(const UninstallPlan& plan, const Paths& paths, const std::string& title_root,
                         const std::string& tickets, u64 game) {
    const std::string launcher = TitleDir(title_root, kLauncherTitleId);
    const std::string game_content =
        game != 0 && IsSupportedMarioKart7(game) ? TitleDir(title_root, game) + "content/" : "";
    const auto allowed = [&](const std::string& p) {
        if (p == paths.dir || p == launcher || (!game_content.empty() && p == game_content) ||
            p == paths.work) {
            return true;
        }
        for (const char* root : {"CTGP-7tmp/", "3ds/CTGP-7/", "CTGP-7savebak/", "CTGP-7savebakCitra/"}) {
            if (p == paths.sdmc + root) {
                return true;
            }
        }
        const std::string t = WithSlash(tickets);
        return Under(p, t) && p.find('/', t.size()) == std::string::npos &&
               (p.compare(t.size(), 17, fmt::format("{:016X}.", kLauncherTitleId)) == 0 ||
                (game != 0 && p.compare(t.size(), 17, fmt::format("{:016X}.", game)) == 0));
    };
    std::string error;
    std::error_code ec;
    for (const std::string& p : plan.remove) {
        if (!allowed(p)) {
            LOG_ERROR(Frontend, "ctgp7: uninstall: not removing {} (not one of CTGP-7's)", p);
            continue;
        }
        if (p == paths.dir && !plan.keep.empty()) {
            // everything in sdmc/CTGP-7/ but the progress folders
            for (fs::directory_iterator it(p, ec), end; !ec && it != end; it.increment(ec)) {
                const std::string child = it->path().string() + (it->is_directory() ? "/" : "");
                if (std::find(plan.keep.begin(), plan.keep.end(), child) != plan.keep.end()) {
                    continue;
                }
                std::error_code e2;
                fs::remove_all(it->path(), e2);
                if (e2) {
                    error = "Couldn't Delete " + it->path().filename().string();
                }
            }
            continue;
        }
        std::error_code e2;
        fs::remove_all(fs::path(p.back() == '/' ? p.substr(0, p.size() - 1) : p), e2);
        if (e2) {
            error = "Couldn't Delete " + p;
        }
    }
    LOG_INFO(Frontend, "ctgp7: uninstalled ({} paths, {} kept){}", plan.remove.size(), plan.keep.size(),
             error.empty() ? "" : ": " + error);
    return error;
}

std::string TitleRoot() {
    return Service::AM::GetMediaTitlePath(Service::FS::MediaType::SDMC);
}

std::string TicketFolder() {
    const std::string any = Service::AM::GetTicketPath(0, 0); // <folder>/0000....0000.tik
    return any.substr(0, any.find_last_of('/') + 1);
}

InstalledGame ReadInstalledGame(const Paths& paths) {
    std::string text;
    InstalledGame g;
    if (FileUtil::ReadFileToString(true, paths.work + "installed-game.txt", text) == 0) {
        return g;
    }
    const auto nl = text.find('\n');
    g.title_id = std::strtoull(text.substr(0, nl).c_str(), nullptr, 16);
    if (nl != std::string::npos) {
        g.source = text.substr(nl + 1);
        while (!g.source.empty() && (g.source.back() == '\n' || g.source.back() == '\r')) {
            g.source.pop_back();
        }
    }
    if (!IsSupportedMarioKart7(g.title_id)) {
        g = {};
    }
    return g;
}

void WriteInstalledGame(const Paths& paths, const InstalledGame& game) {
    FileUtil::CreateFullPath(paths.work);
    FileUtil::WriteStringToFile(true, paths.work + "installed-game.txt",
                                fmt::format("{:016X}\n{}\n", game.title_id, game.source));
}

std::string InstalledVersion(const Paths& paths) {
    std::ifstream in(paths.version_file, std::ios::binary);
    std::string v(32, '\0');
    in.read(v.data(), static_cast<std::streamsize>(v.size()));
    v.resize(static_cast<std::size_t>(std::max<std::streamsize>(in.gcount(), 0)));
    // "1.6.30" and nothing else: digits and dots
    std::string out;
    for (const char c : v) {
        if ((c >= '0' && c <= '9') || c == '.') {
            out += c;
        } else {
            break;
        }
    }
    return out;
}

std::string LauncherPath() {
    if (!TitleInstalled(kLauncherTitleId)) {
        return {};
    }
    return Service::AM::GetTitleContentPath(Service::FS::MediaType::SDMC, kLauncherTitleId);
}

u64 FolderBytes(const std::string& dir) {
    std::error_code ec;
    u64 total = 0;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code e2;
        if (it->is_regular_file(e2)) {
            total += it->file_size(e2);
        }
    }
    return total;
}

u64 BytesWrittenSince(const std::string& dir, fs::file_time_type since) {
    std::error_code ec;
    u64 total = 0;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code e2;
        if (it->is_regular_file(e2) && it->last_write_time(e2) >= since && !e2) {
            total += it->file_size(e2);
        }
    }
    return total;
}

bool WrittenSince(const std::string& path, fs::file_time_type since) {
    std::error_code ec;
    const auto t = fs::last_write_time(path, ec);
    return !ec && t >= since;
}

std::optional<InstallerFile> PickInstaller(std::string_view profile_json, std::string* error) {
    const auto fail = [error](const char* why) -> std::optional<InstallerFile> {
        if (error) {
            *error = why;
        }
        return std::nullopt;
    };
    try {
        const auto j = nlohmann::json::parse(profile_json);
        if (j.value("_bIsTrashed", false) || j.value("_bIsWithheld", false) ||
            j.value("_bIsPrivate", false)) {
            return fail("The CTGP-7 page isn't available on GameBanana");
        }
        std::optional<InstallerFile> best;
        long long newest = -1;
        for (const auto& f : j.at("_aFiles")) {
            const std::string name = f.value("_sFile", "");
            if (f.value("_bIsArchived", false) || !Mods::GlobMatch(kInstallerPattern, name) ||
                f.value("_sAvResult", "") != "clean") {
                continue;
            }
            const long long added = f.value("_tsDateAdded", 0LL);
            if (added <= newest) {
                continue;
            }
            InstallerFile file;
            file.name = name;
            file.url = f.value("_sDownloadUrl", "");
            file.md5 = f.value("_sMd5Checksum", "");
            std::transform(file.md5.begin(), file.md5.end(), file.md5.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            file.size = f.value("_nFilesize", u64{0});
            std::string host, path;
            if (!SplitUrl(file.url, host, path) || !DownloadHostAllowed(host)) {
                continue;
            }
            newest = added;
            best = file;
        }
        if (!best) {
            return fail("The CTGP-7 installer isn't on its GameBanana page");
        }
        return best;
    } catch (const std::exception&) {
        return fail("GameBanana's answer couldn't be read");
    }
}

bool DownloadHostAllowed(std::string_view host) {
    if (host == "https://gamebanana.com" || host == "https://files.gamebanana.com") {
        return true;
    }
    constexpr std::string_view cache = "https://filecache";
    constexpr std::string_view tail = ".gamebanana.com";
    if (host.size() > cache.size() + tail.size() && host.substr(0, cache.size()) == cache &&
        host.substr(host.size() - tail.size()) == tail) {
        const std::string_view n = host.substr(cache.size(), host.size() - cache.size() - tail.size());
        return n.size() <= 4 &&
               std::all_of(n.begin(), n.end(), [](char c) { return c >= '0' && c <= '9'; });
    }
    return false;
}

GetToFile RealGet(const std::atomic_bool* cancel, Mods::Transfer::Stopper* stopper) {
    return [cancel, stopper](const std::string& host, const std::string& path,
                             const std::string& dest, u64 resume_from,
                             const std::function<void(u64, u64)>& progress) {
        const auto r = Mods::Multipart::StreamGet(host, path, dest, cancel, progress, resume_from,
                                                  stopper);
        GetReply out;
        out.status = r.status;
        out.location = r.location;
        out.error = r.error;
        out.total = r.total;
        out.start = r.start;
        return out;
    };
}

std::string FileMd5(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    CryptoPP::Weak::MD5 hash;
    std::vector<char> buf(1 << 20);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const auto n = in.gcount();
        if (n > 0) {
            hash.Update(reinterpret_cast<const CryptoPP::byte*>(buf.data()),
                        static_cast<std::size_t>(n));
        }
    }
    std::array<CryptoPP::byte, CryptoPP::Weak::MD5::DIGESTSIZE> digest{};
    hash.Final(digest.data());
    std::string out;
    for (const auto b : digest) {
        out += fmt::format("{:02x}", b);
    }
    return out;
}

FetchResult FetchInstaller(const Paths& paths, const GetToFile& get, const std::atomic_bool* cancel,
                           const std::function<void(u64, u64)>& progress) {
    FetchResult out;
    const auto cancelled = [cancel] { return cancel != nullptr && cancel->load(); };
    std::error_code ec;
    fs::create_directories(paths.work, ec);
    // The page's file list (a small JSON answer, through the same transport)
    const std::string page = paths.work + "profile.json";
    std::string host, path;
    SplitUrl(fmt::format("https://gamebanana.com/apiv11/Mod/{}/ProfilePage", kGameBananaMod), host,
             path);
    const GetReply pr = get(host, path, page, 0, {});
    if (cancelled()) {
        out.error = "Cancelled";
        return out;
    }
    if (!pr.error.empty() || pr.status != 200) {
        fs::remove(page, ec);
        out.error = !pr.error.empty() ? pr.error
                    : pr.status == 0  ? std::string("No Internet Connection")
                                      : fmt::format("GameBanana Answered {}", pr.status);
        return out;
    }
    std::string json;
    FileUtil::ReadFileToString(true, page, json);
    fs::remove(page, ec);
    std::string why;
    const auto file = PickInstaller(json, &why);
    if (!file) {
        out.error = why;
        return out;
    }
    LOG_INFO(Frontend, "ctgp7: installer {} ({} bytes) from GameBanana mod {}", file->name, file->size,
             kGameBananaMod);
    // A partial download of the same file is resumed (the name and the size say which).
    const std::string dest = paths.work + file->name;
    const std::string part = dest + ".part";
    const std::string part_of = part + ".of";
    std::string kept_of;
    FileUtil::ReadFileToString(true, part_of, kept_of);
    u64 resume = 0;
    if (kept_of == fmt::format("{} {}", file->md5, file->size) && fs::exists(part, ec)) {
        resume = fs::file_size(part, ec);
        if (ec || resume >= file->size) {
            resume = 0;
        }
    }
    if (resume == 0) {
        fs::remove(part, ec);
        FileUtil::WriteStringToFile(true, part_of, fmt::format("{} {}", file->md5, file->size));
    }
    std::string url = file->url;
    for (int hops = 0;; hops++) {
        if (hops >= 8) {
            out.error = "Too Many Redirects";
            return out;
        }
        if (!SplitUrl(url, host, path) || !DownloadHostAllowed(host)) {
            LOG_WARNING(Frontend, "ctgp7: refused a redirect to {}", url.substr(0, url.find('?')));
            out.error = "The Download Left GameBanana";
            return out;
        }
        const GetReply r = get(host, path, part, resume, progress);
        if (cancelled()) {
            out.error = "Cancelled";
            return out;
        }
        if (r.status >= 300 && r.status < 400 && !r.location.empty()) {
            url = r.location.front() == '/' ? host + r.location : r.location;
            continue;
        }
        if (!r.error.empty()) {
            out.error = r.error;
            return out;
        }
        if (r.status != 200 && r.status != 206) {
            out.error = r.status == 0 ? std::string("No Internet Connection")
                                      : fmt::format("GameBanana Answered {}", r.status);
            return out;
        }
        break;
    }
    if (file->size != 0 && fs::file_size(part, ec) != file->size) {
        fs::remove(part, ec);
        out.error = "The Download Was Cut Short";
        return out;
    }
    if (!file->md5.empty() && FileMd5(part) != file->md5) {
        fs::remove(part, ec);
        fs::remove(part_of, ec);
        out.error = "The Download Is Damaged";
        return out;
    }
    fs::remove(dest, ec);
    fs::rename(part, dest, ec);
    fs::remove(part_of, ec);
    if (ec) {
        out.error = "The Download Couldn't Be Saved";
        return out;
    }
    out.zip = dest;
    return out;
}

std::string UnpackInstaller(const Paths& paths, const std::string& zip, const std::atomic_bool* cancel,
                            std::string* error) {
    std::error_code ec;
    fs::remove_all(paths.unpacked, ec);
    fs::create_directories(paths.unpacked, ec);
    // 2 GB at most (the installer is about 900 MB unpacked); bsdtar on disk
    const std::string err = Mods::ExtractFileToDir(zip, Mods::ArchiveKind::Zip, paths.unpacked,
                                                   2ULL << 30, 1800, cancel);
    fs::remove(zip, ec);
    if (!err.empty()) {
        fs::remove_all(paths.unpacked, ec);
        if (error) {
            *error = err == "Cancelled" ? err : "The Installer Couldn't Be Unpacked";
        }
        LOG_WARNING(Frontend, "ctgp7: unpacking failed: {}", err);
        return {};
    }
    for (fs::recursive_directory_iterator it(paths.unpacked, ec), end; !ec && it != end;
         it.increment(ec)) {
        std::string name = it->path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (it->is_regular_file() && name.size() > 5 &&
            name.compare(name.size() - 5, 5, ".3dsx") == 0) {
            return it->path().string();
        }
    }
    fs::remove_all(paths.unpacked, ec);
    if (error) {
        *error = "The Download Has No Installer";
    }
    return {};
}

u64 RequiredBytes(u64 zip_bytes, u64 game_bytes) {
    // the zip and the unpacked installer at the same time (the zip goes once unpacked), then the
    // installer and what it writes (about as much again); the converted game twice (the CIA and the
    // installed copy) before any of it. 10 % margin.
    const u64 installer = zip_bytes + zip_bytes / 8;
    const u64 peak = std::max(zip_bytes + installer, installer * 2);
    return (std::max(peak, game_bytes * 2) + game_bytes) / 10 * 11;
}

// ---- RunOverride ----

namespace {
std::atomic_bool g_active{false};
bool g_loader = false;
bool g_allow = true;
} // namespace

void RunOverride::Begin() {
    if (g_active.load()) {
        return;
    }
    g_loader = Settings::values.plugin_loader_enabled.GetValue();
    g_allow = Settings::values.allow_plugin_loader.GetValue();
    g_active = true;
    Settings::values.plugin_loader_enabled.SetValue(true);
    Settings::values.allow_plugin_loader.SetValue(true);
    LOG_INFO(Frontend, "ctgp7: plugin loader on for this run (the setting stays {}, allow {})", g_loader,
             g_allow);
}

void RunOverride::End() {
    if (!g_active.exchange(false)) {
        return;
    }
    Settings::values.plugin_loader_enabled.SetValue(g_loader);
    Settings::values.allow_plugin_loader.SetValue(g_allow);
    LOG_INFO(Frontend, "ctgp7: plugin loader back to {} (allow {})", g_loader, g_allow);
}

bool RunOverride::Active() {
    return g_active.load();
}

bool RunOverride::LoaderToSave(bool current) {
    return g_active.load() ? g_loader : current;
}

bool RunOverride::AllowToSave(bool current) {
    return g_active.load() ? g_allow : current;
}

} // namespace Ctgp7
