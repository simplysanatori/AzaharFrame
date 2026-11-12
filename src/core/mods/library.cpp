// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <ctime>
#include <set>
#include <thread>
#include <json.hpp>
#include <fmt/format.h>
#include "common/atomic_file.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/cheats/online.h"
#include "core/cheats/game_version.h" // AzaharFrame versions
#include "core/mods/catalog_schema.h" // AzaharFrame harden
#include "core/custom_tex/download.h" // AzaharFrame catalog3 round 2: Drive pages, ids
#include "core/mods/library.h"

namespace fs = std::filesystem;

namespace Mods {

namespace {

constexpr std::size_t kMaxApi = 4 * 1024 * 1024;
// AzaharFrame harden: mods.json is about 300 KB today; a fetch that passes the cap is a failure, never a cut list
constexpr std::size_t kMaxCatalog = 16u * 1024 * 1024;
constexpr std::size_t kMaxAsset = 128u * 1024 * 1024;
constexpr std::size_t kMaxFiles = 400;
// AzaharFrame catalog3: a big download goes to disk (no memory cap); the unpacked total and file count
// have only a safety cap
constexpr u64 kMaxExtracted = 20ull << 30;
constexpr const char* kUnscanned = "GameBanana Hasn't Scanned This File Yet";
constexpr const char* kLegacyMarker = "code.ips.developer";

// AzaharFrame harden: sleeps in short steps; false when `cancel` was set meanwhile
bool SleepUnlessCancelled(std::chrono::milliseconds total, const std::atomic_bool& cancel) {
    const auto end = std::chrono::steady_clock::now() + total;
    while (std::chrono::steady_clock::now() < end) {
        if (cancel) {
            return false;
        }
        std::this_thread::sleep_for(std::min(std::chrono::milliseconds(50), total));
    }
    return !cancel;
}

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

bool HostAllowed(const std::string& host, const std::vector<std::string>& extra = {}) {
    for (const auto& e : extra) { // an https source's own host (exact names, no wildcards)
        if (host == "https://" + e) {
            return true;
        }
    }
    static const char* allowed[] = {"https://drive.google.com", "https://drive.usercontent.google.com","https://api.github.com",
                                    "https://github.com",
                                    "https://raw.githubusercontent.com",
                                    "https://codeload.github.com",
                                    "https://objects.githubusercontent.com",
                                    "https://release-assets.githubusercontent.com",
                                    // AzaharFrame modcat: GameBanana's API, its download redirect and
                                    // its file hosts (files, filecache<N>)
                                    "https://gamebanana.com", "https://files.gamebanana.com"};
    for (const char* a : allowed) {
        if (host == a) {
            return true;
        }
    }
    constexpr std::string_view cache = "https://filecache";
    constexpr std::string_view tail = ".gamebanana.com";
    if (host.size() > cache.size() + tail.size() && host.compare(0, cache.size(), cache) == 0 &&
        host.compare(host.size() - tail.size(), tail.size(), tail) == 0) {
        const std::string_view n = std::string_view(host).substr(
            cache.size(), host.size() - cache.size() - tail.size());
        return n.size() <= 4 && std::all_of(n.begin(), n.end(), [](char c) { return c >= '0' && c <= '9'; });
    }
    return false;
}

std::string BaseName(std::string_view path) {
    const auto slash = path.rfind('/');
    return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

/// AzaharFrame modcat: "2022-01-02T17:52:18Z" -> "2022-01-02" ("" when it is not a date).
std::string IsoDay(std::string_view stamp) {
    if (stamp.size() < 10 || stamp[4] != '-' || stamp[7] != '-') {
        return {};
    }
    for (const std::size_t i : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (!std::isdigit(static_cast<unsigned char>(stamp[i]))) {
            return {};
        }
    }
    return std::string(stamp.substr(0, 10));
}

/// AzaharFrame modcat: a Unix time as "YYYY-MM-DD" (UTC).
std::string DayOfUnix(long long t) {
    if (t <= 0) {
        return {};
    }
    const std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    return fmt::format("{:04}-{:02}-{:02}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
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

namespace {
// The rules' answer for one path ("" = no rule matched); `rules` is not empty.
std::string ApplyRules(const std::vector<std::string>& parts, const std::vector<Rule>& rules, u64 title_id) {
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
            return rule.to + Join(parts, pattern.size());
        }
        return rule.to.empty() ? std::string{} : (rule.to.back() == '/' ? rule.to + parts.back() : rule.to);
    }
    return {};
}
} // namespace

std::string MapPath(std::string_view archive_path, const std::vector<Rule>& rules, u64 title_id) {
    const auto parts = Split(archive_path);
    if (parts.empty() || std::find(parts.begin(), parts.end(), "..") != parts.end()) {
        return {};
    }
    const std::string mapped = rules.empty() ? AutoMap(parts, title_id) : ApplyRules(parts, rules, title_id);
    return IsModPath(mapped) ? mapped : std::string{};
}

// AzaharFrame catalog3: the "textures" block's rules; no layout detection, no rules = nothing.
std::string MapTexturePath(std::string_view archive_path, const std::vector<Rule>& rules, u64 title_id) {
    const auto parts = Split(archive_path);
    if (rules.empty() || parts.empty() || std::find(parts.begin(), parts.end(), "..") != parts.end()) {
        return {};
    }
    const std::string mapped = ApplyRules(parts, rules, title_id);
    const auto safe = SafeRelativePath(mapped);
    return safe ? *safe : std::string{};
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
    bool active = false; // the files are in mods/<TID>/ now
    std::vector<std::string> files;
    std::vector<std::string> texture_files; // AzaharFrame catalog3: relative to load/textures/<TID>/<id>/
    // AzaharFrame catalog3: a change that waits for the running game to stop ("" none): "activate"
    // (downloaded, files not in the game's folders yet), "deactivate", "remove"
    std::string pending;
    bool unscanned = false; // AzaharFrame catalog3
};

namespace {
// What the player sees: staged on, or still on and not about to go
bool EffectiveActive(bool active, const std::string& pending) {
    return pending == "activate" || (active && pending != "deactivate" && pending != "remove");
}
} // namespace

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
        m.pending = j.value("pending", "");
        m.unscanned = j.value("unscanned", false);
        m.files.clear();
        for (const auto& f : j.at("files")) {
            const auto rel = f.get<std::string>();
            if (!IsModPath(rel)) { // a damaged or edited manifest never reaches outside the mods folder
                return false;
            }
            m.files.push_back(rel);
        }
        m.texture_files.clear();
        if (j.contains("texture_files")) { // AzaharFrame catalog3
            for (const auto& f : j.at("texture_files")) {
                const auto rel = SafeRelativePath(f.get<std::string>());
                if (!rel || rel->empty()) {
                    return false;
                }
                m.texture_files.push_back(*rel);
            }
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
    if (!m.pending.empty()) {
        j["pending"] = m.pending;
    }
    if (m.unscanned) {
        j["unscanned"] = true;
    }
    j["files"] = m.files;
    if (!m.texture_files.empty()) {
        j["texture_files"] = m.texture_files;
    }
    return Common::WriteFileAtomic(dir + "manifest.json", j.dump(2) + "\n"); // AzaharFrame harden: fsynced, atomic
}

void Library::SetState(State::Phase phase, const std::string& mod_id, const std::string& text) {
    std::lock_guard lock{state_mutex_};
    state_ = {phase, mod_id, text};
}

/// AzaharFrame mega: the bytes fetched so far (the menu shows a percentage).
void Library::SetProgress(u64 done, u64 total) {
    std::lock_guard lock{state_mutex_};
    state_.done = done;
    state_.total = total;
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
    // AzaharFrame harden: every way this fails leaves the catalog as it was (the compiled-in copy or the
    // last good fetch), with a log line; a catalog from a newer format puts the compiled-in copy back and
    // is not asked for again this run.
    const auto fail = [&](const std::string& why) {
        LOG_WARNING(Frontend, "Mods: catalog kept as it was: {}", why);
        if (error) {
            *error = why;
        }
        return false;
    };
    {
        std::lock_guard lock{mutex_};
        if (!catalog_too_new_error_.empty()) {
            if (error) {
                *error = catalog_too_new_error_;
            }
            return false;
        }
    }
    HttpReply reply;
    std::string err;
    const std::string path = fmt::format("/{}/main/{}", kOwnerRepo, kCatalogPath);
    if (!Fetch("https://raw.githubusercontent.com" + path, kMaxCatalog, reply, err) || reply.status != 200) {
        return fail(err.empty() ? "Mod list not available" : err);
    }
    auto parsed = ParseCatalog(reply.body, &err);
    if (!parsed && SchemaTooNew(err)) {
        LOG_WARNING(Frontend, "Mods: {}; using the list built into this version", err);
        std::lock_guard lock{mutex_};
        catalog_ = DefaultCatalog();
        catalog_too_new_error_ = err;
        if (error) {
            *error = err;
        }
        return false;
    }
    if (!parsed || parsed->mods.empty()) {
        return fail(err.empty() ? "Mod list is empty" : err);
    }
    SetCatalog(std::move(*parsed));
    // AzaharFrame versions: the catalog's "versions" section (what each game shows for a raw
    // version) extends and overrides the table built into the program
    if (Cheats::Sharkive::SetOnlineVersions(reply.body)) {
        LOG_INFO(Frontend, "Mods: version table from the catalog");
    }
    return true;
}

bool Library::RefreshCatalogOnce() {
    {
        std::lock_guard lock{mutex_};
        if (catalog_refreshed_) {
            return false;
        }
        catalog_refreshed_ = true;
    }
    std::string error;
    const auto t0 = std::chrono::steady_clock::now(); // AzaharFrame stall
    const bool ok = RefreshCatalog(&error);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    if (!ok) {
        LOG_WARNING(Frontend, "Mods: catalog not refreshed in {} ms: {}", ms, error);
        return false;
    }
    LOG_INFO(Frontend, "Mods: catalog refreshed in {} ms", ms);
    return true;
}

// One GET with redirects followed to GitHub's own hosts only.
std::string ShortFailure(std::string_view e) {
    const auto has = [&](std::string_view needle) { return e.find(needle) != std::string_view::npos; };
    if (e.rfind("Not Enough Space", 0) == 0 || e == kUnscanned) { // AzaharFrame catalog3
        return std::string(e); // "Not Enough Space: Needs 4.6 GB Free", "GameBanana Hasn't Scanned This File Yet"
    }
    if (has("rate limit")) {
        return "Rate Limited";
    }
    if (has("Connection") || has("SSL") || has("No reply") || has("certificate") || has("Resolve")) {
        return "No Connection";
    }
    if (has("Read") || has("Write") || has("Timeout") || has("timed out")) {
        return "Timed Out";
    }
    if (has("too large") || has("too big")) {
        return "Too Large";
    }
    if (has("No release") || has("No matching") || has("not found") || has("Server returned 404")) {
        return "Not Found";
    }
    if (has("Server returned")) {
        return "Server Error";
    }
    if (has("archive") || has("zip") || has("Zip") || has("Nothing in the download") || has("code patch") ||
        has("Unsupported")) {
        return "Bad Archive";
    }
    if (has("Cannot write") || has("Cannot copy") || has("Cannot delete")) {
        return "Disk Error";
    }
    if (has("Another install")) {
        return "Busy";
    }
    return "Error";
}

bool Library::Fetch(const std::string& url, std::size_t max_bytes, HttpReply& out, std::string& error) {
    std::string current = url;
    for (int hop = 0; hop < 6; ++hop) {
        if (cancel_) { // AzaharFrame integ3: Cancel also stops GitHub and GameBanana downloads
            error = "Cancelled";
            return false;
        }
        std::string host, path;
        if (!SplitUrl(current, host, path) || !HostAllowed(host)) {
            error = "Download refused: not an allowed address";
            return false;
        }
        out = transport_(host, path, max_bytes);
        if (out.status == 0 && out.error != "Download too large") {
            // AzaharFrame modfix: one retry when the connection failed or timed out (a download
            // while the headset is busy has been seen to fail once and work the next time).
            LOG_WARNING(Frontend, "Mods: GET {}{} failed: {}; retrying", host, path,
                        out.error.empty() ? "No reply" : out.error);
            out = transport_(host, path, max_bytes);
        }
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

// AzaharFrame mega: a public MEGA link. A zip is read in place (central directory first), and only the
// entries the entry's install rules keep are fetched and decrypted; any other file is fetched whole
// (the usual size cap) and unpacked like a download from the other hosts.
bool Library::DownloadMega(const CatalogEntry& entry, u64 title_id, std::vector<ArchiveFile>& files,
                           std::string& error) {
    constexpr u64 kMaxMegaTotal = 640ull * 1024 * 1024; // what the kept entries may add up to
    const Source& s = entry.source;
    const auto link = Mega::ParseLink(s.mega_link);
    if (!link) {
        error = Mega::kNotFound;
        return false;
    }
    const auto resolved = Mega::Resolve(mega_net_, *link, s.asset_pattern, &error);
    if (!resolved) {
        return false;
    }
    SetState(State::Phase::Downloading, entry.id, fmt::format("Downloading {}", entry.name));
    Mega::RemoteFile remote(mega_net_, *resolved, &cancel_);
    std::vector<Mega::ZipEntry> entries;
    bool not_zip = false;
    if (!Mega::ListZip(remote, entries, not_zip, error)) {
        if (!not_zip) {
            return false;
        }
        error.clear();
        if (resolved->size > kMaxAsset) { // AzaharFrame catalog3: to disk
            std::string version;
            return DownloadBig(entry, title_id, version, error);
        }
        SetProgress(0, resolved->size);
        remote.on_progress = [this, total = resolved->size](u64 done) { SetProgress(done, total); };
        std::string body;
        if (!remote.Read(0, resolved->size, body, error)) {
            return false;
        }
        SetState(State::Phase::Extracting, entry.id, fmt::format("Unpacking {}", entry.name));
        const ArchiveKind kind = DetectArchive(body);
        if (kind == ArchiveKind::Unknown) {
            files.push_back({resolved->name.empty() ? "file" : resolved->name, std::move(body)});
            return true;
        }
        auto result = ExtractArchive(body, kind);
        if (!result.error.empty()) {
            error = result.error;
            return false;
        }
        files = std::move(result.files);
        return true;
    }
    std::vector<const Mega::ZipEntry*> keep;
    u64 total = 0;
    for (const auto& e : entries) {
        if (!MapPath(e.name, entry.rules, title_id).empty()) {
            keep.push_back(&e);
            total += e.comp_size;
        }
    }
    if (keep.empty()) {
        error = "Nothing in the download fits this game";
        return false;
    }
    if (total > kMaxMegaTotal) { // AzaharFrame catalog3: the whole file to disk
        std::string version;
        return DownloadBig(entry, title_id, version, error);
    }
    SetProgress(0, total);
    const u64 base = remote.BytesFetched(); // the central directory was counted already
    remote.on_progress = [this, total, base](u64 fetched) { SetProgress(fetched - base, total); };
    for (const auto* e : keep) {
        ArchiveFile f;
        f.path = e->name;
        if (!Mega::ReadZipEntry(remote, *e, f.data, error)) {
            return false;
        }
        files.push_back(std::move(f));
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// AzaharFrame catalog3: multipart downloads

std::string Library::WorkDir(const std::string& mod_id) const {
    return fmt::format("{}mod-downloads/{}/", load_dir_, mod_id);
}

// One GET into a file with redirects followed to the allowed hosts only.
bool Library::FetchToFile(const std::string& url, const std::string& dest, const Multipart::Progress& progress,
                          std::string& error, const std::vector<std::string>& extra_hosts) {
    std::string current = url;
    int confirms = 0;
    bool retried_empty = false;
    for (int hop = 0; hop < 12; ++hop) {
        std::string host, path;
        if (!SplitUrl(current, host, path) || !HostAllowed(host, extra_hosts)) {
            error = "Download refused: not an allowed address";
            return false;
        }
        auto reply = file_transport_(host, path, dest, &cancel_, progress);
        // AzaharFrame harden: a bare 200 with no body (seen on a GameBanana file host) once: ask again after a
        // short wait, then report it.
        if (reply.status == 200 && reply.error.empty() && reply.html.empty() && !retried_empty) {
            std::error_code size_ec;
            const auto size = fs::exists(dest, size_ec) ? fs::file_size(dest, size_ec) : 0;
            if (size == 0 && reply.total == 0) {
                retried_empty = true;
                LOG_WARNING(Frontend, "Mods: GET {}{} answered 200 with no body; retrying once", host, path);
                if (!SleepUnlessCancelled(empty_retry_delay_, cancel_)) {
                    error = "Cancelled";
                    return false;
                }
                reply = file_transport_(host, path, dest, &cancel_, progress);
            }
        }
        if ((reply.status == 301 || reply.status == 302 || reply.status == 303 || reply.status == 307 ||
             reply.status == 308) &&
            reply.error.empty()) {
            if (reply.location.empty()) {
                error = "Bad redirect";
                return false;
            }
            current = reply.location.front() == '/' ? host + reply.location : reply.location;
            continue;
        }
        if (reply.status == 0 || !reply.error.empty()) {
            error = reply.error.empty() ? "No reply" : reply.error;
            return false;
        }
        if (!reply.html.empty()) { // a page, not the file: Drive's "can't scan for viruses" form, or a wall
            const bool drive = host == "https://drive.google.com" || host == "https://drive.usercontent.google.com";
            if (drive) {
                const auto page = CustomTex::Download::ParseDrivePage(reply.html);
                if (page.kind == CustomTex::Download::DrivePage::Kind::Confirm && confirms < 2) {
                    ++confirms;
                    current = page.url;
                    continue;
                }
                error = page.error.empty() ? "Google Drive did not give the file" : page.error;
                return false;
            }
            error = "The link leads to a web page, not a file";
            return false;
        }
        if (reply.status != 200) {
            error = reply.status == 404 ? "Not found" : fmt::format("Server returned {}", reply.status);
            return false;
        }
        if (retried_empty) {
            std::error_code size_ec;
            if (!fs::exists(dest, size_ec) || fs::file_size(dest, size_ec) == 0) {
                error = "The host refused the download (200)";
                return false;
            }
        }
        return true;
    }
    error = "Too many redirects";
    return false;
}

namespace {
struct PartRef {
    std::string url;  // https, the file
    std::string name;
    u64 size = 0;     // 0 unknown
    bool mega = false;
    Mega::Resolved mega_file;
    std::vector<std::string> hosts; // extra hosts of an https part
};
} // namespace

// AzaharFrame catalog3: a single download too big for memory goes through the multipart path as one
// part: to disk, space checked, unpacked into a folder (big_root_, which Install maps from).
bool Library::DownloadBig(const CatalogEntry& entry, u64 title_id, std::string& version, std::string& error) {
    CatalogEntry copy = entry;
    copy.parts = {entry.source};
    big_root_.clear();
    if (!DownloadParts(copy, title_id, big_root_, version, error)) {
        big_root_.clear();
        return false;
    }
    return true;
}

bool Library::DownloadParts(const CatalogEntry& entry, u64 title_id, std::string& root, std::string& version,
                            std::string& error) {
    (void)title_id;
    const std::string work = WorkDir(entry.id);
    std::error_code ec;
    fs::remove_all(work, ec);
    fs::create_directories(work, ec);
    if (ec) {
        error = "Cannot write the download folder";
        return false;
    }
    SetState(State::Phase::Resolving, entry.id, fmt::format("Looking Up {}", entry.name));
    const std::string api = "https://api.github.com";
    const std::string raw = "https://raw.githubusercontent.com";

    // 1. Where each part is, and how big
    std::vector<PartRef> refs;
    for (const Source& s : entry.parts) {
        if (cancel_) {
            error = "Cancelled";
            return false;
        }
        PartRef ref;
        HttpReply reply;
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
            try {
                const auto j = nlohmann::json::parse(reply.body);
                if (version.empty()) {
                    version = j.value("tag_name", "");
                }
                for (const auto& a : j.at("assets")) {
                    if (GlobMatch(s.asset_pattern, a.value("name", ""))) {
                        ref.name = a.value("name", "");
                        ref.url = a.value("browser_download_url", "");
                        ref.size = a.value("size", static_cast<u64>(0));
                        break;
                    }
                }
            } catch (const std::exception&) {
                error = "Release information not readable";
                return false;
            }
            if (ref.url.empty()) {
                error = "No matching file in the release";
                return false;
            }
        } else if (s.type == Source::Type::Gamebanana) {
            if (!Fetch(fmt::format("https://gamebanana.com/apiv11/Mod/{}/ProfilePage", s.gb_mod), kMaxApi, reply,
                       error)) {
                return false;
            }
            if (reply.status != 200) {
                error = reply.status == 404 ? "Mod not found" : fmt::format("Server returned {}", reply.status);
                return false;
            }
            bool unscanned = false;
            try {
                const auto j = nlohmann::json::parse(reply.body);
                if (j.value("_bIsTrashed", false) || j.value("_bIsWithheld", false) ||
                    j.value("_bIsPrivate", false)) {
                    error = "Mod not found";
                    return false;
                }
                if (version.empty()) {
                    version = j.value("_sVersion", "");
                }
                long long newest = -1, newest_unknown = -1;
                std::string unk_name, unk_url;
                u64 unk_size = 0;
                for (const auto& f : j.at("_aFiles")) {
                    const auto name = f.value("_sFile", "");
                    const std::string av = f.value("_sAvResult", "");
                    if (f.value("_bIsArchived", false) || !GlobMatch(s.asset_pattern, name)) {
                        continue;
                    }
                    const long long added = f.value("_tsDateAdded", 0LL);
                    if (av == "unknown") { // AzaharFrame catalog3: only when the player allowed it
                        if (added > newest_unknown) {
                            newest_unknown = added;
                            unk_name = name;
                            unk_url = f.value("_sDownloadUrl", "");
                            unk_size = f.value("_nFilesize", static_cast<u64>(0));
                        }
                        continue;
                    }
                    if (av != "clean") {
                        continue;
                    }
                    if (added > newest) {
                        newest = added;
                        ref.name = name;
                        ref.url = f.value("_sDownloadUrl", "");
                        ref.size = f.value("_nFilesize", static_cast<u64>(0));
                    }
                }
                if (ref.url.empty() && !unk_url.empty()) { // a clean file is always preferred
                    hit_unscanned_ = true;
                    if (allow_unscanned_) {
                        ref.name = unk_name;
                        ref.url = unk_url;
                        ref.size = unk_size;
                    } else {
                        unscanned = true;
                    }
                }
            } catch (const std::exception&) {
                error = "Mod information not readable";
                return false;
            }
            if (ref.url.empty()) {
                error = unscanned ? kUnscanned : "No matching file in the mod";
                return false;
            }
        } else if (s.type == Source::Type::Mega) {
            const auto link = Mega::ParseLink(s.mega_link);
            const auto resolved = link ? Mega::Resolve(mega_net_, *link, s.asset_pattern, &error) : std::nullopt;
            if (!resolved) {
                if (error.empty()) {
                    error = Mega::kNotFound;
                }
                return false;
            }
            ref.mega = true;
            ref.mega_file = *resolved;
            ref.name = resolved->name;
            ref.size = resolved->size;
        } else if (s.type == Source::Type::Https) { // AzaharFrame catalog3 round 2
            ref.url = s.url;
            ref.hosts = s.hosts;
            std::string uh, up;
            SplitUrl(s.url, uh, up);
            ref.name = !s.name.empty() ? s.name : BaseName(up.substr(0, up.find('?')));
        } else if (s.type == Source::Type::Gdrive) {
            ref.url = CustomTex::Download::DriveUrl(s.file_id);
            ref.name = s.name;
        } else if (s.type == Source::Type::GdriveFolder) {
            const CustomTex::Download::DriveLister lister =
                [this](const std::string& id) -> std::optional<std::vector<CustomTex::Download::DriveItem>> {
                HttpReply r;
                std::string err;
                if (!Fetch("https://drive.google.com/embeddedfolderview?id=" + id, 2 * 1024 * 1024, r, err) ||
                    r.status != 200) {
                    return std::nullopt;
                }
                return CustomTex::Download::ParseDriveFolder(r.body);
            };
            const auto item = CustomTex::Download::PickDriveFile(lister, s.folder_id, s.asset_pattern, &error);
            if (cancel_) {
                error = "Cancelled";
                return false;
            }
            if (!item) {
                return false;
            }
            ref.url = CustomTex::Download::DriveUrl(item->id);
            ref.name = s.name.empty() ? item->name : s.name;
        } else { // a file of a repository
            if (s.path.empty() || s.path.back() == '/') {
                error = "A part must be one file";
                return false;
            }
            ref.name = BaseName(s.path);
            ref.url = fmt::format("{}/{}/{}/{}", raw, s.repo, UrlEncodePath(s.ref), UrlEncodePath(s.path));
        }
        refs.push_back(std::move(ref));
    }

    // 2. Room for the parts, the unpacked files and a margin: (parts + unpacked) x 1.1
    u64 parts_total = 0;
    for (const auto& r : refs) {
        parts_total += r.size;
    }
    if (parts_total == 0) {
        parts_total = entry.size; // Drive and https files carry no size: the catalog's
    }
    u64 free_bytes = 0;
    if (free_space_(work, &free_bytes)) {
        if (const auto problem = Multipart::SpaceError(parts_total, parts_total, free_bytes); !problem.empty()) {
            error = problem;
            return false;
        }
    }

    // 3. Download every part, each once more when it fails
    SetState(State::Phase::Downloading, entry.id, fmt::format("Downloading {}", entry.name));
    SetProgress(0, parts_total);
    std::vector<std::string> files;
    std::vector<std::string> names;
    u64 base = 0;
    for (std::size_t i = 0; i < refs.size(); ++i) {
        const PartRef& ref = refs[i];
        const std::string dest = fmt::format("{}part{:03}", work, i + 1);
        const auto progress = [this, base, parts_total](u64 done, u64) { SetProgress(base + done, parts_total); };
        bool ok = false;
        std::string why;
        for (int attempt = 0; attempt < 2 && !ok; ++attempt) {
            if (cancel_) {
                error = "Cancelled";
                return false;
            }
            fs::remove(dest, ec);
            if (ref.mega) {
                Mega::RemoteFile remote(mega_net_, ref.mega_file, &cancel_);
                remote.on_progress = [&](u64 fetched) { SetProgress(base + fetched, parts_total); };
                std::ofstream out(dest, std::ios::binary | std::ios::trunc);
                constexpr u64 kChunk = 8ull * 1024 * 1024;
                std::string chunk;
                ok = static_cast<bool>(out);
                for (u64 at = 0; ok && at < ref.mega_file.size; at += kChunk) {
                    ok = remote.Read(at, kChunk, chunk, why);
                    if (ok) {
                        out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                        ok = static_cast<bool>(out);
                        if (!ok) {
                            why = "Not Enough Space: the disk is full";
                        }
                    }
                }
            } else {
                ok = FetchToFile(ref.url, dest, progress, why, ref.hosts);
            }
            if (!ok && why == "Cancelled") {
                error = why;
                return false;
            }
            if (!ok) {
                LOG_WARNING(Frontend, "Mods: part {} of {} failed: {}{}", i + 1, entry.id, why,
                            attempt == 0 ? "; retrying" : "");
            }
        }
        if (!ok) {
            error = why;
            return false;
        }
        if (ref.size != 0 && static_cast<u64>(fs::file_size(dest, ec)) != ref.size) {
            error = "Download incomplete (the size is not the one listed)";
            return false;
        }
        base += ref.size != 0 ? ref.size : static_cast<u64>(fs::file_size(dest, ec));
        files.push_back(dest);
        names.push_back(ref.name);
    }

    // 4. Loose files (parts that are no archive: each is one file, placed by the install rules), or
    // one archive, joined and unpacked
    {
        std::string first(16, '\0');
        std::ifstream in(files.front(), std::ios::binary);
        in.read(first.data(), static_cast<std::streamsize>(first.size()));
        first.resize(static_cast<std::size_t>(in.gcount()));
        if (DetectArchive(first) == ArchiveKind::Unknown) {
            const std::string out_dir = work + "out";
            fs::create_directories(out_dir, ec);
            std::set<std::string> used;
            for (std::size_t i = 0; i < files.size(); ++i) {
                std::string name = names[i].empty() ? fmt::format("part{:03}", i + 1) : names[i];
                if (!used.insert(name).second || name.find('/') != std::string::npos) {
                    error = "Two parts have the same file name";
                    return false;
                }
                fs::rename(files[i], out_dir + "/" + name, ec);
                if (ec) {
                    error = "Cannot write the download folder";
                    return false;
                }
            }
            root = out_dir;
            return true;
        }
    }
    SetState(State::Phase::Extracting, entry.id, fmt::format("Unpacking {}", entry.name));
    const std::string joined = work + "joined.bin";
    if (const auto problem = Multipart::JoinParts(files, joined, &cancel_); !problem.empty()) {
        error = problem;
        return false;
    }
    for (const auto& f : files) {
        fs::remove(f, ec);
    }
    std::string head(16, '\0');
    {
        std::ifstream in(joined, std::ios::binary);
        in.read(head.data(), static_cast<std::streamsize>(head.size()));
        head.resize(static_cast<std::size_t>(in.gcount()));
    }
    const ArchiveKind kind = DetectArchive(head);
    if (kind == ArchiveKind::Unknown) {
        error = "The parts are not a zip, 7z or rar archive";
        return false;
    }
    const std::string out_dir = work + "out";
    const std::string problem = ExtractFileToDir(joined, kind, out_dir, kMaxExtracted, 7200, &cancel_);
    if (!problem.empty()) {
        error = problem;
        return false;
    }
    fs::remove(joined, ec);
    root = out_dir;
    return true;
}

bool Library::DownloadSource(const CatalogEntry& entry, u64 title_id, std::vector<ArchiveFile>& files,
                             std::string& version, std::string& error) {
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
        u64 asset_size = 0;
        try {
            const auto j = nlohmann::json::parse(reply.body);
            version = j.value("tag_name", "");
            for (const auto& a : j.at("assets")) {
                const auto name = a.value("name", "");
                if (GlobMatch(s.asset_pattern, name)) {
                    asset_name = name;
                    url = a.value("browser_download_url", "");
                    asset_size = a.value("size", static_cast<u64>(0));
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
        if (asset_size > kMaxAsset) { // AzaharFrame catalog3: too big for memory: to disk
            return DownloadBig(entry, title_id, version, error);
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

    if (s.type == Source::Type::Mega) { // AzaharFrame mega
        return DownloadMega(entry, title_id, files, error);
    }

    // AzaharFrame catalog3 round 2: https and Google Drive files, any size, always to disk
    if (s.type == Source::Type::Https || s.type == Source::Type::Gdrive || s.type == Source::Type::GdriveFolder) {
        std::string v;
        return DownloadBig(entry, title_id, v, error);
    }

    if (s.type == Source::Type::Gamebanana) { // AzaharFrame modcat
        if (!Fetch(fmt::format("https://gamebanana.com/apiv11/Mod/{}/ProfilePage", s.gb_mod), kMaxApi,
                   reply, error)) {
            return false;
        }
        if (reply.status != 200) {
            error = reply.status == 404 ? "Mod not found" : fmt::format("Server returned {}", reply.status);
            return false;
        }
        std::string url, asset_name;
        std::size_t size = 0;
        bool unscanned = false;
        try {
            const auto j = nlohmann::json::parse(reply.body);
            if (j.value("_bIsTrashed", false) || j.value("_bIsWithheld", false) ||
                j.value("_bIsPrivate", false)) {
                error = "Mod not found";
                return false;
            }
            version = j.value("_sVersion", "");
            long long newest = -1, newest_unknown = -1;
            std::string unk_name, unk_url, unk_version;
            std::size_t unk_size = 0;
            for (const auto& f : j.at("_aFiles")) {
                const auto name = f.value("_sFile", "");
                // The newest file of the mod that matches, that GameBanana's scan found clean
                if (f.value("_bIsArchived", false) || !GlobMatch(s.asset_pattern, name)) {
                    continue;
                }
                const long long added = f.value("_tsDateAdded", 0LL);
                if (f.value("_sAvResult", "") == "unknown") { // AzaharFrame catalog3: only when allowed
                    if (added > newest_unknown) {
                        newest_unknown = added;
                        unk_name = name;
                        unk_url = f.value("_sDownloadUrl", "");
                        unk_size = f.value("_nFilesize", static_cast<std::size_t>(0));
                        unk_version = f.value("_sVersion", "");
                    }
                    continue;
                }
                if (f.value("_sAvResult", "") != "clean") {
                    continue;
                }
                if (added > newest) {
                    newest = added;
                    asset_name = name;
                    url = f.value("_sDownloadUrl", "");
                    size = f.value("_nFilesize", static_cast<std::size_t>(0));
                    if (!f.value("_sVersion", "").empty()) {
                        version = f.value("_sVersion", "");
                    }
                }
            }
            if (url.empty() && !unk_url.empty()) { // a clean file is always preferred
                hit_unscanned_ = true;
                if (allow_unscanned_) {
                    asset_name = unk_name;
                    url = unk_url;
                    size = unk_size;
                    if (!unk_version.empty()) {
                        version = unk_version;
                    }
                } else {
                    unscanned = true;
                }
            }
        } catch (const std::exception&) {
            error = "Mod information not readable";
            return false;
        }
        if (url.empty()) {
            error = unscanned ? kUnscanned : "No matching file in the mod";
            return false;
        }
        if (size > kMaxAsset) { // AzaharFrame catalog3: too big for memory: to disk
            return DownloadBig(entry, title_id, version, error);
        }
        SetState(State::Phase::Downloading, entry.id, fmt::format("Downloading {}", entry.name));
        if (!Fetch(url, kMaxAsset, reply, error)) {
            return false;
        }
        if (reply.status != 200) {
            error = fmt::format("Server returned {}", reply.status);
            return false;
        }
        if (size != 0 && reply.body.size() != size) {
            error = "Download incomplete (the size is not the one listed)";
            return false;
        }
        SetState(State::Phase::Extracting, entry.id, fmt::format("Unpacking {}", entry.name));
        const ArchiveKind kind = DetectArchive(reply.body);
        if (kind == ArchiveKind::Unknown) {
            files.push_back({asset_name, std::move(reply.body)});
            return true;
        }
        auto result = ExtractArchive(reply.body, kind);
        if (!result.error.empty()) {
            error = result.error;
            return false;
        }
        files = std::move(result.files);
        return true;
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

Library::SourceFacts Library::LookupDate(const CatalogEntry& entry) {
    const Source& s = entry.source;
    HttpReply reply;
    std::string error;
    if (s.type == Source::Type::Mega || s.type == Source::Type::Https || s.type == Source::Type::Gdrive ||
        s.type == Source::Type::GdriveFolder) { // no date without a download
        return {};
    }
    try {
        if (s.type == Source::Type::Gamebanana) {
            if (!Fetch(fmt::format("https://gamebanana.com/apiv11/Mod/{}/ProfilePage", s.gb_mod), kMaxApi,
                       reply, error) ||
                reply.status != 200) {
                return {};
            }
            const auto j = nlohmann::json::parse(reply.body);
            // AzaharFrame ui3: and the mod's version ("1.0.0", often "")
            const auto v = j.find("_sVersion");
            return {DayOfUnix(j.value("_tsDateUpdated", j.value("_tsDateAdded", 0LL))),
                    v != j.end() && v->is_string() ? v->get<std::string>() : std::string{}};
        }
        if (s.type == Source::Type::GithubRelease) {
            if (!Fetch(fmt::format("https://api.github.com/repos/{}/releases/{}", s.repo,
                                   s.tag == "latest" ? "latest" : "tags/" + UrlEncodePath(s.tag)),
                       kMaxApi, reply, error) ||
                reply.status != 200) {
                return {};
            }
            const auto j = nlohmann::json::parse(reply.body);
            return {IsoDay(j.value("published_at", "")), j.value("tag_name", "")}; // ui3: the tag
        }
        // a repository file or folder: the date of its last commit
        if (!Fetch(fmt::format("https://api.github.com/repos/{}/commits?sha={}&path={}&per_page=1",
                               s.repo, UrlEncodePath(s.ref), UrlEncodePath(s.path)),
                   kMaxApi, reply, error) ||
            reply.status != 200) {
            return {};
        }
        const auto j = nlohmann::json::parse(reply.body);
        if (!j.is_array() || j.empty()) {
            return {};
        }
        return {IsoDay(j[0].at("commit").at("committer").value("date", "")), {}};
    } catch (const std::exception&) {
        return {};
    }
}

std::size_t Library::RefreshDates(u64 title_id, std::chrono::steady_clock::time_point deadline) {
    std::vector<CatalogEntry> todo;
    {
        std::lock_guard lock{mutex_};
        for (const auto& e : catalog_.mods) {
            if (e.ServesTitle(title_id) && !dates_.count(e.id)) {
                todo.push_back(e);
            }
        }
    }
    std::map<std::string, SourceFacts> by_source; // one request for entries sharing a release
    std::size_t asked = 0;
    for (const auto& e : todo) {
        const Source& s = e.source;
        const std::string key = fmt::format("{}|{}|{}|{}|{}|{}", static_cast<int>(s.type), s.repo,
                                            s.type == Source::Type::GithubRelease ? s.tag : s.ref,
                                            s.type == Source::Type::GithubRelease ? "" : s.path,
                                            s.gb_mod, s.mega_link);
        auto it = by_source.find(key);
        if (it == by_source.end()) {
            if (std::chrono::steady_clock::now() >= deadline) { // AzaharFrame stall
                LOG_INFO(Frontend, "Mods: dates: {} of {} entries left for a later call",
                         todo.size() - static_cast<std::size_t>(&e - todo.data()), todo.size());
                break;
            }
            it = by_source.emplace(key, LookupDate(e)).first;
            asked++;
        }
        std::lock_guard lock{mutex_};
        dates_[e.id] = it->second.date; // "" too: not asked again this run
        versions_[e.id] = it->second.version; // AzaharFrame ui3
    }
    return asked;
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
            const bool on = EffectiveActive(m.active, m.pending);
            info.state = on ? ModState::Active : ModState::Inactive;
            info.pending = m.pending; // AzaharFrame catalog3
            info.unscanned = m.unscanned;
            info.files = m.files;
            info.texture_files = m.texture_files; // AzaharFrame catalog3
            if (m.active) {
                owned.insert(m.files.begin(), m.files.end()); // still in the folder until it is taken out
            }
            (on ? active : inactive).push_back(std::move(info));
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

bool Library::Busy() const {
    std::unique_lock lock{mutex_, std::try_to_lock};
    return !lock.owns_lock();
}

std::vector<AvailableMod> Library::ListAvailable(u64 title_id, std::optional<u32> game_code_crc,
                                                 const Compat::Game* game) const {
    std::lock_guard lock{mutex_};
    std::vector<AvailableMod> out;
    // AzaharFrame compat: the game's other regions and releases, and the build its code matches
    Compat::Families families;
    std::string matched_build;
    if (game) {
        game_code_crc = game->code_crc;
        for (const auto& e : catalog_.mods) {
            families.Add(e.title, e.title_ids);
            if (game_code_crc && e.ServesTitle(title_id) &&
                std::find(e.code_crc32.begin(), e.code_crc32.end(), *game_code_crc) !=
                    e.code_crc32.end()) {
                matched_build = e.chip;
            }
        }
    }
    for (const auto& e : catalog_.mods) {
        Compat::Verdict verdict;
        if (game) {
            verdict = Compat::Judge({e.title_ids, e.chip, e.code_crc32}, *game, families, matched_build);
            if (!verdict.shown) {
                continue;
            }
        } else if (!e.ServesTitle(title_id)) {
            continue;
        }
        AvailableMod a;
        a.compat = std::move(verdict);
        a.id = e.id;
        a.name = e.name;
        a.title = e.title;
        a.author = e.author;
        a.licence = e.licence;
        a.homepage = e.homepage;
        a.notes = e.notes;
        a.author_url = e.author_url;
        a.chip = e.chip;
        a.chip_tip = e.chip_tip;
        a.matches_game = game_code_crc && std::find(e.code_crc32.begin(), e.code_crc32.end(),
                                                     *game_code_crc) != e.code_crc32.end();
        const auto date = dates_.find(e.id);
        a.updated = date != dates_.end() && !date->second.empty() ? date->second : e.updated;
        const auto version = versions_.find(e.id); // AzaharFrame ui3
        a.version = version != versions_.end() && version->second.size() <= 24 && !version->second.empty()
                        ? version->second
                        : e.version;
        a.verified = e.verified;
        a.downloads = e.downloads;                  // AzaharFrame catalog3
        a.size = e.size;
        a.has_textures = !e.texture_rules.empty();
        a.multipart = !e.parts.empty();
        a.unscanned = unscanned_ids_.count(e.id) != 0;
        a.installable = e.Installable();
        Manifest m;
        if (ReadManifest(LibraryDir(title_id) + e.id + "/", m)) {
            a.active = EffectiveActive(m.active, m.pending);
            a.in_library = !a.active && m.pending != "remove";
            a.unscanned = a.unscanned || m.unscanned;
            a.pending = m.pending == "activate"; // AzaharFrame catalog3: downloaded, in when the game stops
        }
        out.push_back(std::move(a));
    }
    // AzaharFrame compat: those for the game's own version first (with `game`; without it the
    // build the code matches first), then the most downloaded (unknown counts last), then by name
    std::stable_sort(out.begin(), out.end(), [](const AvailableMod& a, const AvailableMod& b) {
        if (a.compat.Fits() != b.compat.Fits()) {
            return a.compat.Fits();
        }
        if (a.matches_game != b.matches_game) {
            return a.matches_game;
        }
        if ((a.downloads > 0) != (b.downloads > 0)) { // 0: unknown
            return a.downloads > 0;
        }
        if (a.downloads != b.downloads) {
            return a.downloads > b.downloads;
        }
        return a.name < b.name;
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

std::string Library::TexturesDir(u64 title_id, const std::string& mod_id) const {
    return fmt::format("{}textures/{}/{}/", load_dir_, TitleIdText(title_id), mod_id);
}

// AzaharFrame catalog3: takes a mod's textures part out of load/textures/<TID>/<id>/ (only the files it
// wrote, then the empty folders).
RemoveOutcome Library::RemoveTextureFiles(u64 title_id, const std::string& mod_id,
                                          const std::vector<std::string>& files) {
    if (files.empty()) {
        return {};
    }
    const std::string dir = TexturesDir(title_id, mod_id);
    const std::string stop = fmt::format("{}textures/{}/", load_dir_, TitleIdText(title_id));
    std::error_code ec;
    for (const auto& rel : files) {
        fs::remove(dir + rel, ec);
        if (ec) {
            return {false, "Cannot delete " + rel};
        }
        PruneEmptyParents(stop, fs::path(dir + rel).parent_path());
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
    // AzaharFrame catalog3: the running game's folders are never touched: the change waits for it to stop
    if (title_id == running_title_.load() && (m.active || m.pending == "activate")) {
        m.pending = m.active ? "deactivate" : ""; // staged and never applied: just drop the stage
        return WriteManifest(dir, m) ? RemoveOutcome{} : RemoveOutcome{false, "Cannot write the manifest"};
    }
    if (m.active) {
        auto r = RemoveFilesOf(title_id, m.files);
        if (!r.ok) {
            return r;
        }
        r = RemoveTextureFiles(title_id, m.id, m.texture_files); // AzaharFrame catalog3
        if (!r.ok) {
            return r;
        }
        m.active = false;
    }
    m.pending.clear();
    if (!WriteManifest(dir, m)) {
        return {false, "Cannot write the manifest"};
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

namespace {
/// A managed mod's id as installs make it (catalog ids, "zip-..." ids): lower-case letters, digits,
/// '-', '_' and '.', at most 80 characters, no leading '.'. Never contains '/'.
bool LibraryIdOk(const std::string& id) {
    if (id.empty() || id.size() > 80 || id.front() == '.') {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    });
}
} // namespace

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
    // The id becomes a folder name below: the characters installs allow, nothing else.
    if (!LibraryIdOk(mod_id)) {
        return {false, "Mod not found"};
    }
    auto r = DeactivateLocked(title_id, mod_id);
    if (!r.ok || mod_id == kLegacyFixId) {
        return r;
    }
    {
        // AzaharFrame catalog3: files in the running game's folders go when it stops
        Manifest m;
        const std::string dir = LibraryDir(title_id) + mod_id + "/";
        if (mod_id.find('/') == std::string::npos && ReadManifest(dir, m) && m.pending == "deactivate") {
            m.pending = "remove";
            return WriteManifest(dir, m) ? RemoveOutcome{} : RemoveOutcome{false, "Cannot write the manifest"};
        }
    }
    std::error_code ec;
    fs::remove_all(LibraryDir(title_id) + mod_id, ec);
    if (ec) {
        return {false, "Cannot delete the library copy"};
    }
    return {};
}

// AzaharFrame catalog3: the whole install (download, joining, unpacking, MEGA decryption) runs on a thread
// of its own at the lowest priority, so a game that is running is not slowed by it. bsdtar inherits it.
InstallOutcome Library::Install(u64 title_id, const std::string& mod_id, bool replace, bool allow_unscanned,
                                bool other_version) {
    InstallOutcome outcome;
    std::thread worker([&] {
        Multipart::LowerThisThreadPriority();
        allow_unscanned_ = allow_unscanned;
        hit_unscanned_ = false;
        outcome = InstallImpl(title_id, mod_id, replace, other_version);
        outcome.unscanned = hit_unscanned_;
        if (hit_unscanned_) {
            std::lock_guard lock{mutex_};
            unscanned_ids_.insert(mod_id);
        }
        allow_unscanned_ = false;
    });
    worker.join();
    return outcome;
}

InstallOutcome Library::InstallImpl(u64 title_id, const std::string& mod_id, bool replace,
                                    bool other_version) {
    CatalogEntry entry;
    {
        std::lock_guard lock{mutex_};
        const CatalogEntry* found = catalog_.Find(mod_id);
        // AzaharFrame compat: another version's mod after Install Anyway, or one already kept
        // in this title's library (turned on again)
        Manifest kept;
        const bool in_library = found != nullptr && mod_id.find('/') == std::string::npos &&
                                ReadManifest(LibraryDir(title_id) + mod_id + "/", kept);
        if (found == nullptr || !(found->ServesTitle(title_id) || other_version || in_library)) {
            found = nullptr;
        } else {
            entry = *found;
        }
    }
    if (entry.id.empty()) {
        return InstallFromLibrary(title_id, mod_id, replace); // AzaharFrame zipinstall
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
        cancel_ = false; // AzaharFrame mega
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
    if (have && EffectiveActive(manifest.active, manifest.pending)) {
        SetState(State::Phase::Done, mod_id, "");
        return {InstallResult::AlreadyInstalled, {}};
    }
    if (have && manifest.active) { // AzaharFrame catalog3: it was going out when the game stops: it stays
        std::lock_guard lock{mutex_};
        manifest.pending.clear();
        WriteManifest(dir, manifest);
        SetState(State::Phase::Done, mod_id, "");
        return {InstallResult::AlreadyInstalled, {}};
    }

    if (!have) {
        // AzaharFrame catalog3: the files come from memory (one download) or from a folder on disk
        // (multipart); the same mapping puts them in the library either way.
        std::vector<ArchiveFile> archive;
        std::string version, error, root;
        std::string work;
        struct Cleanup {
            std::string path;
            ~Cleanup() {
                if (!path.empty()) {
                    std::error_code e;
                    fs::remove_all(path, e);
                }
            }
        } cleanup;
        std::vector<std::string> paths;
        big_root_.clear();
        work = WorkDir(entry.id);
        cleanup.path = work;
        if (!entry.parts.empty()) {
            if (!DownloadParts(entry, title_id, root, version, error)) {
                return fail(InstallResult::Error, error);
            }
        } else {
            if (!DownloadSource(entry, title_id, archive, version, error)) {
                return fail(InstallResult::Error, error);
            }
            root = big_root_; // a download too big for memory was unpacked to disk
        }
        if (!root.empty()) {
            for (const auto& f : ScanFiles(root)) {
                paths.push_back(f);
            }
        } else {
            for (const auto& f : archive) {
                paths.push_back(f.path);
            }
        }
        // target path -> index in `paths` (the later of two files with one target wins)
        std::map<std::string, std::size_t> mod_targets, tex_targets;
        const auto map_for = [&](u64 rules_tid) {
            mod_targets.clear();
            tex_targets.clear();
            for (std::size_t i = 0; i < paths.size(); ++i) {
                // the targets are always this title's folder; only the rules' {tid} changes
                const std::string target = MapPath(paths[i], entry.rules, rules_tid);
                if (!target.empty()) {
                    mod_targets[target] = i;
                }
                const std::string tex = MapTexturePath(paths[i], entry.texture_rules, rules_tid);
                if (!tex.empty()) {
                    tex_targets[tex] = i;
                }
            }
        };
        map_for(title_id);
        // AzaharFrame compat: Install Anyway: the files of the region it was made for
        for (std::size_t i = 0; mod_targets.empty() && tex_targets.empty() &&
                                !entry.ServesTitle(title_id) && i < entry.title_ids.size();
             i++) {
            map_for(entry.title_ids[i]);
            if (!mod_targets.empty() || !tex_targets.empty()) {
                LOG_INFO(Core, "mods: {} installed from its files for {}", entry.id,
                         TitleIdText(entry.title_ids[i]));
            }
        }
        if (mod_targets.empty() && tex_targets.empty()) {
            return fail(InstallResult::Error, "Nothing in the download fits this game");
        }
        const auto head_of = [&](std::size_t i, std::size_t n) {
            if (root.empty()) {
                return archive[i].data.substr(0, n);
            }
            std::ifstream in(fs::path(root) / paths[i], std::ios::binary);
            std::string h(n, '\0');
            in.read(h.data(), static_cast<std::streamsize>(n));
            h.resize(static_cast<std::size_t>(in.gcount()));
            return h;
        };
        for (const auto& [target, i] : mod_targets) {
            const std::string base = BaseName(target);
            if ((base == "code.ips" && head_of(i, 5) != "PATCH") || (base == "code.bps" && head_of(i, 4) != "BPS1")) {
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
        manifest.source = entry.source.type == Source::Type::Gamebanana
                              ? fmt::format("gamebanana.com/mods/{}", entry.source.gb_mod)
                          : entry.source.type == Source::Type::Mega ? std::string("mega.nz") // AzaharFrame mega
                          : entry.source.type == Source::Type::Gdrive || entry.source.type == Source::Type::GdriveFolder
                              ? std::string("drive.google.com")
                          : entry.source.type == Source::Type::Https
                              ? (entry.source.hosts.empty() ? std::string("https") : entry.source.hosts.front())
                              : "github.com/" + entry.source.repo;
        manifest.verified = entry.verified;
        manifest.unscanned = hit_unscanned_;
        manifest.active = false;
        // one file into the library: from memory, or moved from the folder (copied when both parts
        // want the same file)
        std::set<std::size_t> wanted_twice;
        for (const auto& [t, i] : mod_targets) {
            for (const auto& [t2, i2] : tex_targets) {
                if (i == i2) {
                    wanted_twice.insert(i);
                }
            }
        }
        const auto put = [&](const std::string& dest, std::size_t i) {
            if (root.empty()) {
                return WriteFile(dest, archive[i].data);
            }
            fs::create_directories(fs::path(dest).parent_path(), ec);
            if (wanted_twice.count(i) == 0) {
                fs::rename(fs::path(root) / paths[i], dest, ec);
                if (!ec) {
                    return true;
                }
                ec.clear();
            }
            fs::copy_file(fs::path(root) / paths[i], dest, fs::copy_options::overwrite_existing, ec);
            return !ec;
        };
        bool ok = true;
        for (const auto& [target, i] : mod_targets) {
            manifest.files.push_back(target);
            ok = ok && put(dir + "files/" + target, i);
        }
        for (const auto& [target, i] : tex_targets) {
            manifest.texture_files.push_back(target);
            ok = ok && put(dir + "tex/" + target, i);
        }
        if (!ok || !WriteManifest(dir, manifest)) {
            fs::remove_all(dir, ec);
            return fail(InstallResult::Error, "Cannot write the mod library");
        }
    }
    SetState(State::Phase::Installing, mod_id, fmt::format("Installing {}", entry.name));

    std::lock_guard lock{mutex_};
    return ActivateLocked(title_id, mod_id, manifest, entry.verified, replace);
}

InstallOutcome Library::ActivateLocked(u64 title_id, const std::string& mod_id, Manifest& manifest,
                                       bool verified, bool replace) {
    const auto fail = [&](InstallResult r, const std::string& why) {
        SetState(State::Phase::Failed, mod_id, "Install failed: " + why);
        InstallOutcome o;
        o.result = r;
        o.error = why;
        return o;
    };
    const std::string dir = LibraryDir(title_id) + manifest.id + "/";
    // Conflicts against everything active in the mods folder
    std::map<std::string, std::pair<const ModInfo*, std::string>> taken; // key -> owner, path
    const auto loaded = ListLoadedLocked(title_id);
    for (const auto& m : loaded) {
        if (m.state != ModState::Active || m.id == manifest.id) {
            continue;
        }
        for (const auto& f : m.files) {
            taken.emplace(ConflictKey(f), std::make_pair(&m, f));
        }
        for (const auto& f : m.texture_files) { // AzaharFrame catalog3: two texture parts, one file
            taken.emplace("tex:" + f, std::make_pair(&m, f));
        }
    }
    std::vector<const ModInfo*> clashes;
    std::vector<std::string> mine_keys; // AzaharFrame catalog3: this mod's keys, mod files then textures
    for (const auto& f : manifest.files) {
        mine_keys.push_back(ConflictKey(f));
    }
    for (const auto& f : manifest.texture_files) {
        mine_keys.push_back("tex:" + f);
    }
    for (const auto& key : mine_keys) {
        const auto it = taken.find(key);
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
        if (c->id == kLegacyFixId && verified) {
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
                if (std::find(mine_keys.begin(), mine_keys.end(), ConflictKey(f)) != mine_keys.end()) {
                    o.conflict_path = f;
                    break;
                }
            }
            for (const auto& f : report->texture_files) {
                if (o.conflict_path.empty() &&
                    std::find(mine_keys.begin(), mine_keys.end(), "tex:" + f) != mine_keys.end()) {
                    o.conflict_path = "textures/" + f;
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

    // AzaharFrame catalog3: the running game's folders are not touched: the mod waits in the library
    // (its files are already there) and goes in when the game stops or the title boots next
    if (title_id == running_title_.load()) {
        manifest.pending = "activate";
        manifest.active = false;
        if (!WriteManifest(dir, manifest)) {
            return fail(InstallResult::Error, "Cannot write the manifest");
        }
        SetState(State::Phase::Done, mod_id, fmt::format("{} Downloaded", manifest.name));
        InstallOutcome staged;
        staged.staged = true;
        staged.textures_installed = !manifest.texture_files.empty();
        return staged;
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
    // AzaharFrame catalog3: the textures part, into load/textures/<TID>/<id>/ (a hard link of the
    // library copy when the file system allows it: a texture part can be large)
    const std::string tex_dir = TexturesDir(title_id, manifest.id);
    std::vector<std::string> tex_written;
    for (const auto& rel : manifest.texture_files) {
        fs::create_directories(fs::path(tex_dir + rel).parent_path(), ec);
        fs::remove(tex_dir + rel, ec);
        ec.clear();
        fs::create_hard_link(dir + "tex/" + rel, tex_dir + rel, ec);
        if (ec) {
            ec.clear();
            fs::copy_file(dir + "tex/" + rel, tex_dir + rel, fs::copy_options::overwrite_existing, ec);
        }
        if (ec) {
            RemoveFilesOf(title_id, written);
            RemoveTextureFiles(title_id, manifest.id, tex_written);
            return fail(InstallResult::Error, "Cannot copy " + rel);
        }
        tex_written.push_back(rel);
    }
    manifest.active = true;
    manifest.pending.clear();
    if (!WriteManifest(dir, manifest)) {
        RemoveFilesOf(title_id, written);
        RemoveTextureFiles(title_id, manifest.id, tex_written);
        return fail(InstallResult::Error, "Cannot write the manifest");
    }
    SetState(State::Phase::Done, mod_id, fmt::format("{} Installed", manifest.name));
    InstallOutcome ok;
    ok.textures_installed = !manifest.texture_files.empty();
    return ok;
}

// AzaharFrame zipinstall: Install from Zip's mods (core/mods/zip_install.h)

namespace {
bool LocalIdOk(const std::string& id) {
    if (id.size() <= std::string_view(kZipPrefix).size() || id.size() > 80 ||
        id.compare(0, std::string_view(kZipPrefix).size(), kZipPrefix) != 0) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    });
}
} // namespace

InstallOutcome Library::InstallFromLibrary(u64 title_id, const std::string& mod_id, bool replace) {
    Manifest manifest;
    {
        std::lock_guard lock{mutex_};
        if (!LocalIdOk(mod_id) || !ReadManifest(LibraryDir(title_id) + mod_id + "/", manifest) ||
            manifest.source != kZipSource) {
            return {InstallResult::NotAvailable, "No such mod for this game"};
        }
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
    if (EffectiveActive(manifest.active, manifest.pending)) {
        SetState(State::Phase::Done, mod_id, "");
        return {InstallResult::AlreadyInstalled, {}};
    }
    SetState(State::Phase::Installing, mod_id, fmt::format("Installing {}", manifest.name));
    std::lock_guard lock{mutex_};
    return ActivateLocked(title_id, mod_id, manifest, false, replace);
}

InstallOutcome Library::InstallLocal(u64 title_id, const std::string& id, const std::string& name,
                                     const std::vector<LocalFile>& files, bool replace_same) {
    if (!LocalIdOk(id) || name.empty()) {
        return {InstallResult::Error, "Bad mod name"};
    }
    if (files.empty()) {
        return {InstallResult::Error, "Nothing in the archive fits this game"};
    }
    for (const LocalFile& f : files) {
        if (!IsModPath(f.target)) {
            return {InstallResult::Error, "Bad mod path " + f.target};
        }
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
        SetState(State::Phase::Failed, id, "Install failed: " + why);
        InstallOutcome o;
        o.result = r;
        o.error = why;
        return o;
    };
    SetState(State::Phase::Installing, id, fmt::format("Installing {}", name));
    std::lock_guard lock{mutex_};
    const std::string dir = LibraryDir(title_id) + id + "/";
    Manifest old;
    if (ReadManifest(dir, old)) {
        if (!replace_same) {
            // The same mod (by name) is installed: never replaced without a question.
            InstallOutcome o;
            o.result = InstallResult::Conflict;
            o.error = fmt::format("Conflicts with {}", old.name);
            o.conflict_id = id;
            o.conflict_name = old.name;
            SetState(State::Phase::Failed, id, "Install failed: " + o.error);
            return o;
        }
        if (auto r = DeactivateLocked(title_id, id); !r.ok) {
            return fail(InstallResult::Error, r.error);
        }
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
    Manifest manifest;
    manifest.id = id;
    manifest.name = name;
    manifest.source = kZipSource;
    std::map<std::string, std::string> by_target; // the later of two files with one target wins
    for (const LocalFile& f : files) {
        by_target[f.target] = f.source;
    }
    for (const auto& [target, source] : by_target) {
        fs::create_directories(fs::path(dir + "files/" + target).parent_path(), ec);
        fs::copy_file(source, dir + "files/" + target, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            fs::remove_all(dir, ec);
            return fail(InstallResult::Error, "Cannot write the mod library");
        }
        manifest.files.push_back(target);
    }
    if (!WriteManifest(dir, manifest)) {
        fs::remove_all(dir, ec);
        return fail(InstallResult::Error, "Cannot write the mod library");
    }
    return ActivateLocked(title_id, id, manifest, false, false);
}

// ---------------------------------------------------------------------------------------------

// AzaharFrame catalog3: changes that waited for a game to stop (or for its next boot): removals first,
// then the staged mods go into mods/<TID>/ and load/textures/<TID>/<id>/. Not while that title runs.
int Library::ApplyPending() {
    {
        std::lock_guard sl{state_mutex_};
        if (busy_) {
            return 0;
        }
    }
    std::lock_guard lock{mutex_};
    int applied = 0;
    std::error_code ec;
    const std::string root = load_dir_ + "mod-library/";
    struct Item {
        u64 tid;
        std::string id;
        std::string pending;
    };
    std::vector<Item> removals, activations;
    for (fs::directory_iterator t(root, ec), end; !ec && t != end; t.increment(ec)) {
        const std::string name = t->path().filename().string();
        if (name.size() != 16 || !t->is_directory(ec) ||
            !std::all_of(name.begin(), name.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); })) {
            continue;
        }
        const u64 tid = std::stoull(name, nullptr, 16);
        if (tid == running_title_.load()) {
            continue;
        }
        for (fs::directory_iterator m(t->path(), ec), mend; !ec && m != mend; m.increment(ec)) {
            Manifest manifest;
            if (!m->is_directory(ec) || !ReadManifest(m->path().string() + "/", manifest) || manifest.pending.empty()) {
                continue;
            }
            (manifest.pending == "activate" ? activations : removals).push_back({tid, manifest.id, manifest.pending});
        }
    }
    for (const auto& item : removals) {
        auto r = DeactivateLocked(item.tid, item.id);
        if (r.ok && item.pending == "remove") {
            fs::remove_all(LibraryDir(item.tid) + item.id, ec);
        }
        applied += r.ok ? 1 : 0;
    }
    for (const auto& item : activations) {
        Manifest manifest;
        const std::string dir = LibraryDir(item.tid) + item.id + "/";
        if (!ReadManifest(dir, manifest)) {
            continue;
        }
        manifest.pending.clear();
        manifest.active = false;
        WriteManifest(dir, manifest);
        // conflicts were settled when it was staged; anything still in the way is replaced
        const auto result = ActivateLocked(item.tid, item.id, manifest, manifest.verified, true);
        applied += result.result == InstallResult::Ok ? 1 : 0;
    }
    return applied;
}

void Library::SetRunningTitle(u64 title_id) {
    running_title_ = title_id;
}

Library& Default() {
    static Library library(FileUtil::GetUserPath(FileUtil::UserPath::LoadDir));
    return library;
}

} // namespace Mods
