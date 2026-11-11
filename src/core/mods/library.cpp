// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <json.hpp>
#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/cheats/online.h"
#include "core/mods/library.h"

namespace fs = std::filesystem;

namespace Mods {

namespace {

constexpr std::size_t kMaxApi = 4 * 1024 * 1024;
constexpr std::size_t kMaxAsset = 128u * 1024 * 1024;
constexpr std::size_t kMaxFiles = 400;
constexpr const char* kLegacyMarker = "code.ips.developer";

std::vector<std::string> Split(std::string_view path) {
    std::vector<std::string> v;
    std::size_t start = 0;
    while (start <= path.size()) {
        const auto slash = path.find('/', start);
        const auto part = path.substr(start, slash == std::string_view::npos ? slash : slash - start);
        if (!part.empty()) {
            v.emplace_back(part);
        }
        if (slash == std::string_view::npos) {
            break;
        }
        start = slash + 1;
    }
    return v;
}

std::string Join(const std::vector<std::string>& v, std::size_t from = 0) {
    std::string out;
    for (std::size_t i = from; i < v.size(); ++i) {
        if (!out.empty()) {
            out += '/';
        }
        out += v[i];
    }
    return out;
}

bool EqualsNoCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool IsHex16(std::string_view s) {
    if (s.size() != 16) {
        return false;
    }
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });
}

bool PartMatches(const std::string& pattern, const std::string& part, u64 title_id) {
    if (pattern == "*") {
        return true;
    }
    if (pattern == "{tid}") {
        return EqualsNoCase(part, TitleIdText(title_id));
    }
    return pattern == part;
}

bool IsExefsName(std::string_view n) {
    return n == "code.bin" || n == "code.ips" || n == "code.bps" || n == "icon.bin" ||
           n == "banner.bnr" || n == "logo.bcma.lz";
}

bool IsRootFile(std::string_view n) {
    return n == "exheader.bin" || IsExefsName(n);
}

bool IsTopName(std::string_view n) {
    return n == "exefs" || n == "romfs" || n == "romfs_ext" || IsRootFile(n);
}

std::string AutoMap(const std::vector<std::string>& parts, u64 title_id) {
    // .../mods/<TID>/... or .../titles/<TID>/...
    for (std::size_t i = 1; i < parts.size(); ++i) {
        if ((EqualsNoCase(parts[i - 1], "mods") || EqualsNoCase(parts[i - 1], "titles")) &&
            IsHex16(parts[i])) {
            if (!EqualsNoCase(parts[i], TitleIdText(title_id))) {
                return {}; // another title's (or region's) files
            }
            return Join(parts, i + 1);
        }
    }
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (IsTopName(parts[i])) {
            return Join(parts, i);
        }
    }
    return {};
}

std::string UrlEncodePath(std::string_view s) {
    std::string out;
    for (const char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' ||
            c == '~' || c == '/') {
            out += c;
        } else {
            out += fmt::format("%{:02X}", static_cast<unsigned char>(c));
        }
    }
    return out;
}

bool SplitUrl(const std::string& url, std::string& host, std::string& path) {
    constexpr std::string_view scheme = "https://";
    if (url.compare(0, scheme.size(), scheme) != 0) {
        return false;
    }
    const auto slash = url.find('/', scheme.size());
    host = url.substr(0, slash);
    path = slash == std::string::npos ? "/" : url.substr(slash);
    return host.size() > scheme.size();
}

bool HostAllowed(const std::string& host) {
    static const char* allowed[] = {"https://api.github.com",
                                    "https://github.com",
                                    "https://raw.githubusercontent.com",
                                    "https://codeload.github.com",
                                    "https://objects.githubusercontent.com",
                                    "https://release-assets.githubusercontent.com"};
    for (const char* a : allowed) {
        if (host == a) {
            return true;
        }
    }
    return false;
}

std::string BaseName(std::string_view path) {
    const auto slash = path.rfind('/');
    return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

bool ReadFile(const fs::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool WriteFile(const fs::path& p, std::string_view data) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

// Every regular file under `root`, relative with '/', sorted.
std::vector<std::string> ScanFiles(const std::string& root) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        return out;
    }
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        std::error_code e2;
        if (it->is_regular_file(e2)) {
            out.push_back(it->path().lexically_relative(root).generic_string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

void PruneEmptyParents(const std::string& root, fs::path dir) {
    std::error_code ec;
    const fs::path stop = fs::path(root).lexically_normal();
    while (!dir.empty() && dir.lexically_normal() != stop && dir.string().size() > root.size()) {
        if (!fs::is_empty(dir, ec) || ec) {
            break;
        }
        fs::remove(dir, ec);
        dir = dir.parent_path();
    }
}

// The item an unmanaged file belongs to: its id suffix.
std::string UnmanagedItem(const std::string& rel) {
    const auto parts = Split(rel);
    if (parts.size() == 1) {
        return rel;
    }
    if (parts[0] == "exefs" && parts.size() == 2) {
        return rel;
    }
    return parts[0]; // romfs, romfs_ext, or an unknown folder
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Layout

std::string MapPath(std::string_view archive_path, const std::vector<Rule>& rules, u64 title_id) {
    const auto parts = Split(archive_path);
    if (parts.empty() || std::find(parts.begin(), parts.end(), "..") != parts.end()) {
        return {};
    }
    std::string mapped;
    if (rules.empty()) {
        mapped = AutoMap(parts, title_id);
    } else {
        for (const Rule& rule : rules) {
            const bool is_dir = rule.from.back() == '/';
            const auto pattern = Split(rule.from);
            if (is_dir ? parts.size() <= pattern.size() : parts.size() != pattern.size()) {
                continue;
            }
            bool match = true;
            for (std::size_t i = 0; i < pattern.size() && match; ++i) {
                match = PartMatches(pattern[i], parts[i], title_id);
            }
            if (!match) {
                continue;
            }
            if (is_dir) {
                mapped = rule.to + Join(parts, pattern.size());
            } else {
                mapped = rule.to.empty() ? std::string{}
                                         : (rule.to.back() == '/' ? rule.to + parts.back() : rule.to);
            }
            break;
        }
    }
    return IsModPath(mapped) ? mapped : std::string{};
}

bool IsModPath(std::string_view rel) {
    const auto parts = Split(rel);
    if (parts.empty() || parts.size() != static_cast<std::size_t>(std::count(rel.begin(), rel.end(), '/')) + 1) {
        return false; // empty parts: "a//b", leading or trailing '/'
    }
    for (const auto& p : parts) {
        if (p == "." || p == ".." || p.find('\\') != std::string::npos) {
            return false;
        }
    }
    if (parts.size() == 1) {
        return IsRootFile(parts[0]);
    }
    if (parts[0] == "exefs") {
        return parts.size() == 2 && IsExefsName(parts[1]);
    }
    return parts[0] == "romfs" || parts[0] == "romfs_ext";
}

std::string ConflictKey(std::string_view rel) {
    const auto parts = Split(rel);
    if (parts.size() == 1 || (parts.size() == 2 && parts[0] == "exefs")) {
        const std::string& name = parts.back();
        if (name == "code.ips" || name == "code.bps") {
            return "code-patch";
        }
        if (IsExefsName(name)) {
            return "exefs:" + name;
        }
    }
    if (parts.size() >= 2 && parts[0] == "romfs") {
        return "romfs:" + Join(parts, 1);
    }
    if (parts.size() >= 2 && parts[0] == "romfs_ext") {
        std::string p = Join(parts, 1);
        for (const std::string_view ext : {".ips", ".bps", ".stub"}) {
            if (p.size() > ext.size() && p.compare(p.size() - ext.size(), ext.size(), ext) == 0) {
                p.resize(p.size() - ext.size());
                break;
            }
        }
        return "romfs:" + p;
    }
    return std::string(rel);
}

MapResult MapArchive(const std::vector<ArchiveFile>& archive, const std::vector<Rule>& rules,
                     u64 title_id) {
    MapResult out;
    std::map<std::string, std::string> by_path;
    for (const auto& file : archive) {
        const std::string target = MapPath(file.path, rules, title_id);
        if (target.empty()) {
            ++out.skipped;
            continue;
        }
        by_path[target] = file.data;
    }
    for (auto& [path, data] : by_path) {
        out.files.push_back({path, std::move(data)});
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// Library

struct Library::Manifest {
    std::string id, name, version, author, licence, source;
    bool verified = false;
    bool active = false;
    std::vector<std::string> files;
};

Library::Library(std::string load_dir, Catalog catalog, Transport transport)
    : load_dir_(std::move(load_dir)), transport_(std::move(transport)), catalog_(std::move(catalog)) {
    if (!load_dir_.empty() && load_dir_.back() != '/') {
        load_dir_ += '/';
    }
    if (!transport_) {
        transport_ = [](const std::string& host, const std::string& path, std::size_t max) {
            const auto r = Cheats::Online::HttpsGetRaw(host, path, max);
            return HttpReply{r.status, r.body, r.location, r.error};
        };
    }
}

std::string Library::ModsDir(u64 title_id) const {
    return fmt::format("{}mods/{}/", load_dir_, TitleIdText(title_id));
}

std::string Library::LibraryDir(u64 title_id) const {
    return fmt::format("{}mod-library/{}/", load_dir_, TitleIdText(title_id));
}

bool Library::ReadManifest(const std::string& dir, Manifest& m) const {
    std::string text;
    if (!ReadFile(dir + "manifest.json", text)) {
        return false;
    }
    try {
        const auto j = nlohmann::json::parse(text);
        m.id = j.value("id", "");
        m.name = j.value("name", "");
        m.version = j.value("version", "");
        m.author = j.value("author", "");
        m.licence = j.value("licence", "");
        m.source = j.value("source", "");
        m.verified = j.value("verified", false);
        m.active = j.value("active", false);
        m.files.clear();
        for (const auto& f : j.at("files")) {
            const auto rel = f.get<std::string>();
            if (!IsModPath(rel)) { // a damaged or edited manifest never reaches outside the mods folder
                return false;
            }
            m.files.push_back(rel);
        }
        return !m.id.empty() && !m.name.empty();
    } catch (const std::exception&) {
        return false;
    }
}

bool Library::WriteManifest(const std::string& dir, const Manifest& m) const {
    nlohmann::json j;
    j["schema"] = 1;
    j["id"] = m.id;
    j["name"] = m.name;
    j["version"] = m.version;
    j["author"] = m.author;
    j["licence"] = m.licence;
    j["source"] = m.source;
    j["verified"] = m.verified;
    j["active"] = m.active;
    j["files"] = m.files;
    return WriteFile(dir + "manifest.json", j.dump(2) + "\n");
}

void Library::SetState(State::Phase phase, const std::string& mod_id, const std::string& text) {
    std::lock_guard lock{state_mutex_};
    state_ = {phase, mod_id, text};
}

State Library::GetState() const {
    std::lock_guard lock{state_mutex_};
    return state_;
}

Catalog Library::GetCatalog() const {
    std::lock_guard lock{mutex_};
    return catalog_;
}

void Library::SetCatalog(Catalog catalog) {
    std::lock_guard lock{mutex_};
    catalog_ = std::move(catalog);
}

bool Library::RefreshCatalog(std::string* error) {
    HttpReply reply;
    std::string err;
    const std::string path = fmt::format("/{}/main/{}", kOwnerRepo, kCatalogPath);
    if (!Fetch("https://raw.githubusercontent.com" + path, kMaxApi, reply, err) || reply.status != 200) {
        if (error) {
            *error = err.empty() ? "Mod list not available" : err;
        }
        return false;
    }
    auto parsed = ParseCatalog(reply.body, &err);
    if (!parsed || parsed->mods.empty()) {
        if (error) {
            *error = err.empty() ? "Mod list is empty" : err;
        }
        return false;
    }
    SetCatalog(std::move(*parsed));
    return true;
}

void Library::RefreshCatalogOnce() {
    {
        std::lock_guard lock{mutex_};
        if (catalog_refreshed_) {
            return;
        }
        catalog_refreshed_ = true;
    }
    std::string error;
    if (!RefreshCatalog(&error)) {
        LOG_WARNING(Frontend, "Mods: catalog not refreshed: {}", error);
    }
}

// One GET with redirects followed to GitHub's own hosts only.
bool Library::Fetch(const std::string& url, std::size_t max_bytes, HttpReply& out, std::string& error) {
    std::string current = url;
    for (int hop = 0; hop < 6; ++hop) {
        std::string host, path;
        if (!SplitUrl(current, host, path) || !HostAllowed(host)) {
            error = "Download refused: not a GitHub address";
            return false;
        }
        out = transport_(host, path, max_bytes);
        if (out.status == 301 || out.status == 302 || out.status == 303 || out.status == 307 ||
            out.status == 308) {
            if (out.location.empty()) {
                error = "Bad redirect";
                return false;
            }
            current = out.location.front() == '/' ? host + out.location : out.location;
            continue;
        }
        if (out.status == 0 || !out.error.empty()) {
            error = out.error.empty() ? "No reply" : out.error;
            return false;
        }
        return true;
    }
    error = "Too many redirects";
    return false;
}

bool Library::DownloadSource(const CatalogEntry& entry, u64 title_id, std::vector<ArchiveFile>& files,
                             std::string& version, std::string& error) {
    (void)title_id;
    const Source& s = entry.source;
    HttpReply reply;
    const std::string api = "https://api.github.com";
    const std::string raw = "https://raw.githubusercontent.com";
    SetState(State::Phase::Resolving, entry.id, fmt::format("Looking Up {}", entry.name));

    if (s.type == Source::Type::GithubRelease) {
        const std::string path = fmt::format("{}/repos/{}/releases/{}", api, s.repo,
                                             s.tag == "latest" ? "latest" : "tags/" + UrlEncodePath(s.tag));
        if (!Fetch(path, kMaxApi, reply, error)) {
            return false;
        }
        if (reply.status != 200) {
            error = reply.status == 404 ? "No release found" : fmt::format("Server returned {}", reply.status);
            return false;
        }
        std::string url, asset_name;
        try {
            const auto j = nlohmann::json::parse(reply.body);
            version = j.value("tag_name", "");
            for (const auto& a : j.at("assets")) {
                const auto name = a.value("name", "");
                if (GlobMatch(s.asset_pattern, name)) {
                    asset_name = name;
                    url = a.value("browser_download_url", "");
                    break;
                }
            }
        } catch (const std::exception&) {
            error = "Release information not readable";
            return false;
        }
        if (url.empty()) {
            error = "No matching file in the release";
            return false;
        }
        SetState(State::Phase::Downloading, entry.id, fmt::format("Downloading {}", entry.name));
        if (!Fetch(url, kMaxAsset, reply, error)) {
            return false;
        }
        if (reply.status != 200) {
            error = fmt::format("Server returned {}", reply.status);
            return false;
        }
        SetState(State::Phase::Extracting, entry.id, fmt::format("Unpacking {}", entry.name));
        const ArchiveKind kind = DetectArchive(reply.body);
        if (kind == ArchiveKind::Unknown) {
            files.push_back({asset_name, std::move(reply.body)}); // a single file asset
            return true;
        }
        auto result = ExtractArchive(reply.body, kind);
        if (!result.error.empty()) {
            error = result.error;
            return false;
        }
        files = std::move(result.files);
        return true;
        return false;
    }

    // Repository files
    const bool dir = s.path.back() == '/';
    if (!dir) {
        SetState(State::Phase::Downloading, entry.id, fmt::format("Downloading {}", entry.name));
        const std::string url = fmt::format("{}/{}/{}/{}", raw, s.repo, UrlEncodePath(s.ref), UrlEncodePath(s.path));
        if (!Fetch(url, kMaxAsset, reply, error)) {
            return false;
        }
        if (reply.status != 200) {
            error = reply.status == 404 ? "File not found" : fmt::format("Server returned {}", reply.status);
            return false;
        }
        files.push_back({BaseName(s.path), std::move(reply.body)});
        return true;
    }
    if (!Fetch(fmt::format("{}/repos/{}/git/trees/{}?recursive=1", api, s.repo, UrlEncodePath(s.ref)),
               kMaxApi, reply, error)) {
        return false;
    }
    if (reply.status != 200) {
        error = fmt::format("Server returned {}", reply.status);
        return false;
    }
    std::vector<std::string> paths;
    try {
        const auto j = nlohmann::json::parse(reply.body);
        if (j.value("truncated", false)) {
            error = "Folder listing too large";
            return false;
        }
        for (const auto& t : j.at("tree")) {
            const auto p = t.value("path", "");
            if (t.value("type", "") == "blob" && p.compare(0, s.path.size(), s.path) == 0) {
                paths.push_back(p);
            }
        }
    } catch (const std::exception&) {
        error = "Folder listing not readable";
        return false;
    }
    if (paths.empty() || paths.size() > kMaxFiles) {
        error = paths.empty() ? "Nothing in that folder" : "Too many files in the folder";
        return false;
    }
    SetState(State::Phase::Downloading, entry.id, fmt::format("Downloading {}", entry.name));
    for (const auto& p : paths) {
        if (!SafeRelativePath(p)) {
            error = "The folder holds an unsafe file path";
            return false;
        }
        if (!Fetch(fmt::format("{}/{}/{}/{}", raw, s.repo, UrlEncodePath(s.ref), UrlEncodePath(p)),
                   kMaxAsset, reply, error)) {
            return false;
        }
        if (reply.status != 200) {
            error = fmt::format("Server returned {}", reply.status);
            return false;
        }
        files.push_back({p.substr(s.path.size()), std::move(reply.body)});
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Listing

std::vector<ModInfo> Library::ListLoaded(u64 title_id) const {
    std::lock_guard lock{mutex_};
    return ListLoadedLocked(title_id);
}

std::vector<ModInfo> Library::ListLoadedLocked(u64 title_id) const {
    std::vector<ModInfo> active, inactive, unmanaged;
    std::set<std::string> owned;
    const std::string lib = LibraryDir(title_id);
    std::error_code ec;
    if (fs::is_directory(lib, ec)) {
        for (const auto& item : fs::directory_iterator(lib, ec)) {
            Manifest m;
            if (!item.is_directory() || !ReadManifest(item.path().string() + "/", m)) {
                continue;
            }
            ModInfo info;
            info.id = m.id;
            info.name = m.name;
            info.version = m.version;
            info.author = m.author;
            info.licence = m.licence;
            info.source = m.source;
            info.verified = m.verified;
            info.managed = true;
            info.state = m.active ? ModState::Active : ModState::Inactive;
            info.files = m.files;
            if (m.active) {
                owned.insert(m.files.begin(), m.files.end());
            }
            (m.active ? active : inactive).push_back(std::move(info));
        }
    }
    const std::string mods = ModsDir(title_id);
    // The Cheats tab of an earlier version: code.ips with a marker file beside it
    if (fs::exists(mods + kLegacyMarker, ec) && fs::exists(mods + "code.ips", ec) &&
        !owned.count("code.ips")) {
        ModInfo info;
        info.id = kLegacyFixId;
        info.name = "Fix";
        for (const auto& e : catalog_.mods) {
            if (e.verified && e.ServesTitle(title_id)) {
                info.name = e.name;
                info.author = e.author;
                info.licence = e.licence;
                info.source = "github.com/" + e.source.repo;
                break;
            }
        }
        info.verified = true;
        info.managed = true;
        info.files = {"code.ips", kLegacyMarker};
        owned.insert("code.ips");
        owned.insert(kLegacyMarker);
        active.push_back(std::move(info));
    }
    std::map<std::string, ModInfo> groups; // id -> info, files appended
    for (const auto& rel : ScanFiles(mods)) {
        if (owned.count(rel)) {
            continue;
        }
        const std::string item = UnmanagedItem(rel);
        auto& g = groups[std::string(kUnmanagedPrefix) + item];
        if (g.id.empty()) {
            g.id = std::string(kUnmanagedPrefix) + item;
            g.name = item;
            g.managed = false;
            g.state = ModState::Active;
        }
        g.files.push_back(rel);
    }
    for (auto& [id, info] : groups) {
        unmanaged.push_back(std::move(info));
    }
    const auto by_name = [](const ModInfo& a, const ModInfo& b) { return a.name < b.name; };
    std::sort(active.begin(), active.end(), by_name);
    std::sort(inactive.begin(), inactive.end(), by_name);
    std::vector<ModInfo> out = std::move(active);
    for (auto& v : {&inactive, &unmanaged}) {
        for (auto& m : *v) {
            out.push_back(std::move(m));
        }
    }
    return out;
}

std::vector<AvailableMod> Library::ListAvailable(u64 title_id) const {
    std::lock_guard lock{mutex_};
    std::vector<AvailableMod> out;
    for (const auto& e : catalog_.mods) {
        if (!e.ServesTitle(title_id)) {
            continue;
        }
        AvailableMod a;
        a.id = e.id;
        a.name = e.name;
        a.title = e.title;
        a.author = e.author;
        a.licence = e.licence;
        a.homepage = e.homepage;
        a.notes = e.notes;
        a.verified = e.verified;
        a.installable = e.Installable();
        Manifest m;
        if (ReadManifest(LibraryDir(title_id) + e.id + "/", m)) {
            a.active = m.active;
            a.in_library = !m.active;
        }
        out.push_back(std::move(a));
    }
    // The maintainer's first, then by name
    std::stable_sort(out.begin(), out.end(), [](const AvailableMod& a, const AvailableMod& b) {
        return a.verified != b.verified ? a.verified : a.name < b.name;
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Install / Remove

RemoveOutcome Library::RemoveFilesOf(u64 title_id, const std::vector<std::string>& files) {
    const std::string mods = ModsDir(title_id);
    std::error_code ec;
    for (const auto& rel : files) {
        if (!IsModPath(rel) && rel != kLegacyMarker) {
            continue;
        }
        fs::remove(mods + rel, ec);
        if (ec) {
            return {false, "Cannot delete " + rel};
        }
        PruneEmptyParents(mods, fs::path(mods + rel).parent_path());
    }
    return {};
}

RemoveOutcome Library::DeactivateLocked(u64 title_id, const std::string& mod_id) {
    if (mod_id == kLegacyFixId) {
        return RemoveFilesOf(title_id, {"code.ips", kLegacyMarker});
    }
    const std::string dir = LibraryDir(title_id) + mod_id + "/";
    Manifest m;
    if (mod_id.find('/') != std::string::npos || !ReadManifest(dir, m)) {
        return {false, "Mod not found"};
    }
    if (m.active) {
        auto r = RemoveFilesOf(title_id, m.files);
        if (!r.ok) {
            return r;
        }
        m.active = false;
        if (!WriteManifest(dir, m)) {
            return {false, "Cannot write the manifest"};
        }
    }
    return {};
}

RemoveOutcome Library::Deactivate(u64 title_id, const std::string& mod_id) {
    {
        std::lock_guard sl{state_mutex_};
        if (busy_) {
            return {false, "Another install is running"};
        }
    }
    std::lock_guard lock{mutex_};
    return DeactivateLocked(title_id, mod_id);
}

RemoveOutcome Library::Remove(u64 title_id, const std::string& mod_id) {
    {
        std::lock_guard sl{state_mutex_};
        if (busy_) {
            return {false, "Another install is running"};
        }
    }
    std::lock_guard lock{mutex_};
    if (mod_id.compare(0, std::string_view(kUnmanagedPrefix).size(), kUnmanagedPrefix) == 0) {
        for (const auto& m : ListLoadedLocked(title_id)) {
            if (m.id == mod_id && !m.managed) {
                return RemoveFilesOf(title_id, m.files);
            }
        }
        return {false, "Mod not found"};
    }
    auto r = DeactivateLocked(title_id, mod_id);
    if (!r.ok || mod_id == kLegacyFixId) {
        return r;
    }
    std::error_code ec;
    fs::remove_all(LibraryDir(title_id) + mod_id, ec);
    if (ec) {
        return {false, "Cannot delete the library copy"};
    }
    return {};
}

InstallOutcome Library::Install(u64 title_id, const std::string& mod_id, bool replace) {
    CatalogEntry entry;
    {
        std::lock_guard lock{mutex_};
        const CatalogEntry* found = catalog_.Find(mod_id);
        if (found == nullptr || !found->ServesTitle(title_id)) {
            return {InstallResult::NotAvailable, "No such mod for this game"};
        }
        entry = *found;
    }
    if (!entry.Installable()) {
        return {InstallResult::NotAvailable, "This mod needs bsdtar (for 7z and rar), which wasn't found"};
    }
    {
        std::lock_guard sl{state_mutex_};
        if (busy_) {
            return {InstallResult::Error, "Another install is running"};
        }
        busy_ = true;
    }
    struct Done {
        Library& self;
        ~Done() {
            std::lock_guard sl{self.state_mutex_};
            self.busy_ = false;
        }
    } done{*this};
    const auto fail = [&](InstallResult r, const std::string& why) {
        SetState(State::Phase::Failed, mod_id, "Install failed: " + why);
        InstallOutcome o;
        o.result = r;
        o.error = why;
        return o;
    };

    const std::string dir = LibraryDir(title_id) + entry.id + "/";
    Manifest manifest;
    bool have = false;
    {
        std::lock_guard lock{mutex_};
        have = ReadManifest(dir, manifest);
    }
    if (have && manifest.active) {
        SetState(State::Phase::Done, mod_id, "");
        return {InstallResult::AlreadyInstalled, {}};
    }

    if (!have) {
        std::vector<ArchiveFile> archive;
        std::string version, error;
        if (!DownloadSource(entry, title_id, archive, version, error)) {
            return fail(InstallResult::Error, error);
        }
        auto mapped = MapArchive(archive, entry.rules, title_id);
        archive.clear();
        if (mapped.files.empty()) {
            return fail(InstallResult::Error, "Nothing in the download fits this game");
        }
        for (const auto& f : mapped.files) {
            const std::string base = BaseName(f.path);
            if ((base == "code.ips" && f.data.compare(0, 5, "PATCH") != 0) ||
                (base == "code.bps" && f.data.compare(0, 4, "BPS1") != 0)) {
                return fail(InstallResult::Error, "Not a valid code patch");
            }
        }
        SetState(State::Phase::Installing, mod_id, fmt::format("Installing {}", entry.name));
        std::lock_guard lock{mutex_};
        std::error_code ec;
        fs::remove_all(dir, ec);
        manifest = {};
        manifest.id = entry.id;
        manifest.name = entry.name;
        manifest.version = version;
        manifest.author = entry.author;
        manifest.licence = entry.licence;
        manifest.source = "github.com/" + entry.source.repo;
        manifest.verified = entry.verified;
        manifest.active = false;
        bool ok = true;
        for (const auto& f : mapped.files) {
            manifest.files.push_back(f.path);
            ok = ok && WriteFile(dir + "files/" + f.path, f.data);
        }
        if (!ok || !WriteManifest(dir, manifest)) {
            fs::remove_all(dir, ec);
            return fail(InstallResult::Error, "Cannot write the mod library");
        }
    }
    SetState(State::Phase::Installing, mod_id, fmt::format("Installing {}", entry.name));

    std::lock_guard lock{mutex_};
    // Conflicts against everything active in the mods folder
    std::map<std::string, std::pair<const ModInfo*, std::string>> taken; // key -> owner, path
    const auto loaded = ListLoadedLocked(title_id);
    for (const auto& m : loaded) {
        if (m.state != ModState::Active || m.id == entry.id) {
            continue;
        }
        for (const auto& f : m.files) {
            taken.emplace(ConflictKey(f), std::make_pair(&m, f));
        }
    }
    std::vector<const ModInfo*> clashes;
    for (const auto& f : manifest.files) {
        const auto it = taken.find(ConflictKey(f));
        if (it == taken.end()) {
            continue;
        }
        const ModInfo* owner = it->second.first;
        if (std::find(clashes.begin(), clashes.end(), owner) == clashes.end()) {
            clashes.push_back(owner);
        }
    }
    bool legacy_ours = false;
    for (const ModInfo* c : clashes) {
        if (c->id == kLegacyFixId && entry.verified) {
            legacy_ours = true; // our own earlier install of the same fix: upgrade it silently
        }
    }
    if (!clashes.empty()) {
        // an unmanaged clash is reported first: replace cannot help with it
        const ModInfo* report = clashes.front();
        for (const ModInfo* c : clashes) {
            if (!c->managed) {
                report = c;
                break;
            }
        }
        const bool unmanaged = !report->managed;
        if (unmanaged || (!replace && !(legacy_ours && clashes.size() == 1))) {
            InstallOutcome o;
            o.result = InstallResult::Conflict;
            o.error = fmt::format("Conflicts with {}", report->name);
            o.conflict_id = report->id;
            o.conflict_name = report->name;
            o.conflict_unmanaged = unmanaged;
            for (const auto& f : report->files) {
                for (const auto& mine : manifest.files) {
                    if (ConflictKey(f) == ConflictKey(mine)) {
                        o.conflict_path = f;
                        break;
                    }
                }
                if (!o.conflict_path.empty()) {
                    break;
                }
            }
            SetState(State::Phase::Failed, mod_id, "Install failed: " + o.error);
            return o;
        }
        for (const ModInfo* c : clashes) {
            auto r = DeactivateLocked(title_id, c->id);
            if (!r.ok) {
                return fail(InstallResult::Error, r.error);
            }
        }
    }

    // Compose: copy the library files into the mods folder
    const std::string mods = ModsDir(title_id);
    std::vector<std::string> written;
    std::error_code ec;
    for (const auto& rel : manifest.files) {
        fs::create_directories(fs::path(mods + rel).parent_path(), ec);
        fs::copy_file(dir + "files/" + rel, mods + rel, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            RemoveFilesOf(title_id, written);
            return fail(InstallResult::Error, "Cannot copy " + rel);
        }
        written.push_back(rel);
    }
    manifest.active = true;
    if (!WriteManifest(dir, manifest)) {
        RemoveFilesOf(title_id, written);
        return fail(InstallResult::Error, "Cannot write the manifest");
    }
    SetState(State::Phase::Done, mod_id, fmt::format("{} Installed", entry.name));
    return {};
}

// ---------------------------------------------------------------------------------------------

Library& Default() {
    static Library library(FileUtil::GetUserPath(FileUtil::UserPath::LoadDir));
    return library;
}

} // namespace Mods
