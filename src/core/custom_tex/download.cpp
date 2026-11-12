// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texdl: see download.h.

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#include <httplib.h>
#include <json.hpp>
#include <cryptopp/sha.h>
#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/cheats/online.h"
#include "core/custom_tex/download.h"
#include "core/custom_tex/packs.h"
#include "core/mods/archive.h"
#include "core/mods/catalog.h"

namespace CustomTex::Download {

namespace fs = std::filesystem;

namespace {

constexpr const char* kMarker = ".azaharframe-download.json";

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool IdOk(std::string_view id) {
    if (id.empty() || id.size() > 64 || id.front() == '.') {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    });
}

bool NameCharsOk(std::string_view s, std::string_view extra) {
    return !s.empty() && s.size() <= 100 &&
           std::all_of(s.begin(), s.end(), [&](char c) {
               return std::isalnum(static_cast<unsigned char>(c)) || extra.find(c) != std::string_view::npos;
           });
}

bool RepoOk(std::string_view repo) {
    const auto slash = repo.find('/');
    return slash != std::string_view::npos && slash > 0 && repo.find('/', slash + 1) == std::string_view::npos &&
           NameCharsOk(repo.substr(0, slash), "-_.") && NameCharsOk(repo.substr(slash + 1), "-_.");
}

bool HostNameOk(std::string_view h) {
    return NameCharsOk(h, "-.") && h.find('.') != std::string_view::npos && h.front() != '.' &&
           h.front() != '-' && std::none_of(h.begin(), h.end(), [](char c) { return std::isupper(c); });
}

std::optional<u64> ParseTitleId(const std::string& s) {
    if (s.size() != 16 || !std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isxdigit(c); })) {
        return std::nullopt;
    }
    return std::stoull(s, nullptr, 16);
}

std::string HtmlUnescape(std::string s) {
    static const std::pair<const char*, const char*> table[] = {
        {"&amp;", "&"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&#x3D;", "="}};
    for (const auto& [from, to] : table) {
        for (std::size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + 1)) {
            s.replace(p, std::strlen(from), to);
        }
    }
    return s;
}

std::string UrlEncode(std::string_view s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

// value of attribute `name` inside one tag's text, "" when absent
std::string Attr(std::string_view tag, std::string_view name) {
    const std::string needle = std::string(name) + "=";
    for (std::size_t p = tag.find(needle); p != std::string_view::npos; p = tag.find(needle, p + 1)) {
        if (p > 0 && !std::isspace(static_cast<unsigned char>(tag[p - 1]))) {
            continue;
        }
        const std::size_t q = p + needle.size();
        if (q >= tag.size() || (tag[q] != '"' && tag[q] != '\'')) {
            continue;
        }
        const auto end = tag.find(tag[q], q + 1);
        if (end == std::string_view::npos) {
            return {};
        }
        return HtmlUnescape(std::string(tag.substr(q + 1, end - q - 1)));
    }
    return {};
}

bool Contains(std::string_view hay, std::string_view needle) {
    return hay.find(needle) != std::string_view::npos;
}

bool ContainsNoCase(std::string_view hay, std::string_view needle) {
    return Lower(std::string(hay)).find(Lower(std::string(needle))) != std::string::npos;
}

std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) {
        return b;
    }
    if (b.empty()) {
        return a;
    }
    return a + "/" + b;
}

std::vector<std::string> SplitPath(const std::string& p) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= p.size()) {
        const auto slash = p.find('/', start);
        const auto part = p.substr(start, slash == std::string::npos ? slash : slash - start);
        if (!part.empty()) {
            out.push_back(part);
        }
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    return out;
}

std::string DirOf(const std::string& path) {
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? std::string{} : path.substr(0, slash);
}

std::string BaseOf(const std::string& path) {
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool IsTextureFile(const std::string& base) {
    const std::string l = Lower(base);
    if (l.rfind("tex1_", 0) != 0) {
        return false;
    }
    const auto dot = l.rfind('.');
    if (dot == std::string::npos) {
        return false;
    }
    const auto ext = l.substr(dot);
    return ext == ".png" || ext == ".dds" || ext == ".ktx";
}

bool IsAncestorOrSelf(const std::string& ancestor, const std::string& dir) {
    return ancestor.empty() || dir == ancestor || dir.rfind(ancestor + "/", 0) == 0;
}

const char* CaBundle() {
    static constexpr std::array<const char*, 4> candidates{
        "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/cert.pem",
        "/etc/ssl/ca-bundle.pem"};
    for (const char* path : candidates) {
        if (FileUtil::Exists(path)) {
            return path;
        }
    }
    return nullptr;
}

std::string JStr(const nlohmann::json& o, const char* key) {
    return o.contains(key) && o[key].is_string() ? o[key].get<std::string>() : std::string{};
}

// One "source" object; nullopt with the reason in `why`.
std::optional<Source> ParseSource(const nlohmann::json& s, std::string* why) {
    Source src;
    const std::string type = JStr(s, "type");
    if (type == "github_release") {
        src.type = Source::Type::GithubRelease;
        src.repo = JStr(s, "repo");
        src.tag = JStr(s, "tag").empty() ? "latest" : JStr(s, "tag");
        src.asset_pattern = JStr(s, "asset_pattern");
        if (!RepoOk(src.repo) || src.asset_pattern.empty() || src.asset_pattern.find('/') != std::string::npos ||
            (src.tag != "latest" && !NameCharsOk(src.tag, "-_.+"))) {
            *why = "bad github_release source";
            return std::nullopt;
        }
    } else if (type == "https") {
        src.type = Source::Type::Https;
        src.url = JStr(s, "url");
        std::string host, path;
        if (!SplitUrl(src.url, host, path) || path.find("..") != std::string::npos || !HostNameOk(host.substr(8))) {
            *why = "bad https source";
            return std::nullopt;
        }
    } else if (type == "gdrive") {
        src.type = Source::Type::Gdrive;
        src.file_id = JStr(s, "file_id");
        if (!DriveIdOk(src.file_id)) {
            *why = "bad Google Drive file id (use gdrive_folder for a folder)";
            return std::nullopt;
        }
    } else if (type == "gdrive_folder") {
        src.type = Source::Type::GdriveFolder;
        src.folder_id = JStr(s, "folder_id");
        src.file_pattern = JStr(s, "file_pattern");
        if (!DriveIdOk(src.folder_id) || src.file_pattern.empty() || src.file_pattern.size() > 100 ||
            src.file_pattern.find('/') != std::string::npos) {
            *why = "bad gdrive_folder source (folder_id and a file_pattern are needed)";
            return std::nullopt;
        }
    } else {
        *why = "unknown source type";
        return std::nullopt;
    }
    if (s.contains("hosts")) {
        if (!s["hosts"].is_array()) {
            *why = "bad hosts (exact names only, no wildcards)";
            return std::nullopt;
        }
        for (const auto& h : s["hosts"]) {
            if (!h.is_string() || !HostNameOk(h.get<std::string>())) {
                *why = "bad hosts (exact names only, no wildcards)";
                return std::nullopt;
            }
            src.hosts.push_back(h.get<std::string>());
        }
    }
    return src;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Catalog

bool Entry::ServesTitle(u64 title_id) const {
    return std::find(title_ids.begin(), title_ids.end(), title_id) != title_ids.end();
}

const Entry* Catalog::Find(std::string_view id) const {
    for (const auto& p : packs) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

std::string SanitizeFolderName(std::string_view name) {
    std::string out;
    for (const unsigned char c : name) {
        if (c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|') {
            out += '_';
        } else {
            out += static_cast<char>(c);
        }
    }
    while (!out.empty() && (out.front() == ' ' || out.front() == '.')) {
        out.erase(out.begin());
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) {
        out.pop_back();
    }
    if (out.size() > 80) {
        out.resize(80);
    }
    return out.empty() ? "Pack" : out;
}

std::optional<Catalog> ParseCatalog(std::string_view json, std::string* error, std::vector<std::string>* warnings) {
    const auto warn = [&](const std::string& id, const std::string& why) {
        if (warnings != nullptr) {
            warnings->push_back(fmt::format("{}: {}", id.empty() ? "?" : id, why));
        }
    };
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (const std::exception& e) {
        if (error != nullptr) {
            *error = e.what();
        }
        return std::nullopt;
    }
    if (!j.is_object() || !j.contains("packs") || !j["packs"].is_array()) {
        if (error != nullptr) {
            *error = "Not a texture catalog";
        }
        return std::nullopt;
    }
    Catalog catalog;
    catalog.schema = j.value("schema", 1);
    std::set<std::string> seen;
    for (const auto& p : j["packs"]) {
        Entry e;
        try {
            const auto str = [&](const nlohmann::json& o, const char* key) {
                return o.contains(key) && o[key].is_string() ? o[key].get<std::string>() : std::string{};
            };
            e.id = str(p, "id");
            if (!IdOk(e.id) || !seen.insert(e.id).second) {
                warn(e.id, "bad or repeated id");
                continue;
            }
            e.name = str(p, "name");
            if (e.name.empty()) {
                warn(e.id, "no name");
                continue;
            }
            e.title = str(p, "title");
            e.author = str(p, "author");
            e.author_url = str(p, "author_url");
            if (!e.author_url.empty() && e.author_url.rfind("https://", 0) != 0) {
                warn(e.id, "author_url is not https");
                continue;
            }
            e.licence = str(p, "licence");
            e.notes = str(p, "notes");
            bool ids_ok = p.contains("title_ids") && p["title_ids"].is_array() && !p["title_ids"].empty();
            if (ids_ok) {
                for (const auto& t : p["title_ids"]) {
                    const auto id = t.is_string() ? ParseTitleId(t.get<std::string>()) : std::nullopt;
                    if (!id) {
                        ids_ok = false;
                        break;
                    }
                    e.title_ids.push_back(*id);
                }
            }
            if (!ids_ok) {
                warn(e.id, "bad title_ids");
                continue;
            }
            e.size = p.value("size", 0ull);
            e.sha256 = Lower(str(p, "sha256"));
            if (!e.sha256.empty() &&
                (e.sha256.size() != 64 || !std::all_of(e.sha256.begin(), e.sha256.end(),
                                                       [](unsigned char c) { return std::isxdigit(c); }))) {
                warn(e.id, "bad sha256");
                continue;
            }
            e.format = str(p, "format").empty() ? "auto" : str(p, "format");
            if (e.format != "auto" && e.format != "zip" && e.format != "7z" && e.format != "rar") {
                warn(e.id, "unknown format");
                continue;
            }
            const std::string layout = str(p, "layout").empty() ? "single" : str(p, "layout");
            if (layout != "single" && layout != "variants") {
                warn(e.id, "unknown layout");
                continue;
            }
            e.layout = layout == "variants" ? Layout::Variants : Layout::Single;
            e.folder = SanitizeFolderName(str(p, "folder").empty() ? e.name : str(p, "folder"));

            if (!p.contains("source") || !p["source"].is_object()) {
                warn(e.id, "no source");
                continue;
            }
            std::string why;
            auto primary = ParseSource(p["source"], &why);
            if (!primary) {
                warn(e.id, why);
                continue;
            }
            e.source = std::move(*primary);
            bool alternates_ok = true;
            if (p.contains("alternates")) {
                alternates_ok = p["alternates"].is_array() && p["alternates"].size() <= 4;
                for (const auto& alt : alternates_ok ? p["alternates"] : nlohmann::json::array()) {
                    auto parsed = alt.is_object() ? ParseSource(alt, &why) : std::nullopt;
                    if (!parsed) {
                        alternates_ok = false;
                        break;
                    }
                    e.alternates.push_back(std::move(*parsed));
                }
            }
            if (!alternates_ok) {
                warn(e.id, "bad alternates: " + why);
                continue;
            }
        } catch (const std::exception&) {
            warn(e.id, "unreadable entry");
            continue;
        }
        catalog.packs.push_back(std::move(e));
    }
    return catalog;
}

Catalog DefaultCatalog() {
    return ParseCatalog(DefaultCatalogJson()).value_or(Catalog{});
}

// ---------------------------------------------------------------------------------------------
// Pure parts

bool SplitUrl(const std::string& url, std::string& host, std::string& path) {
    constexpr std::string_view scheme = "https://";
    if (url.compare(0, scheme.size(), scheme) != 0) {
        return false;
    }
    const auto slash = url.find('/', scheme.size());
    host = url.substr(0, slash);
    path = slash == std::string::npos ? "/" : url.substr(slash);
    return host.size() > scheme.size() && host.find('@') == std::string::npos &&
           host.find(':', scheme.size()) == std::string::npos;
}

bool HostAllowed(const std::string& host, const std::vector<std::string>& extra) {
    static const char* allowed[] = {"https://github.com",
                                    "https://objects.githubusercontent.com",
                                    "https://release-assets.githubusercontent.com",
                                    "https://codeload.github.com",
                                    "https://drive.google.com",
                                    "https://drive.usercontent.google.com"};
    for (const char* a : allowed) {
        if (host == a) {
            return true;
        }
    }
    for (const auto& e : extra) {
        if (host == "https://" + e) {
            return true;
        }
    }
    return false;
}

bool DriveIdOk(std::string_view id) {
    return id.size() >= 10 && id.size() <= 80 &&
           std::all_of(id.begin(), id.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; });
}

std::string DriveUrl(std::string_view file_id) {
    return fmt::format("https://drive.usercontent.google.com/download?id={}&export=download&confirm=t", file_id);
}

DrivePage ParseDrivePage(std::string_view html) {
    DrivePage page;
    // 1. The virus-scan form: <form id="download-form" action="https://drive.usercontent.google.com/download">
    const auto form = html.find("<form");
    for (auto f = form; f != std::string_view::npos; f = html.find("<form", f + 1)) {
        const auto tag_end = html.find('>', f);
        if (tag_end == std::string_view::npos) {
            break;
        }
        const std::string action = Attr(html.substr(f, tag_end - f), "action");
        const auto form_end = html.find("</form>", tag_end);
        if (action.empty() || !Contains(action, "/download")) {
            continue;
        }
        std::string query;
        const auto body = html.substr(tag_end, form_end == std::string_view::npos ? std::string_view::npos : form_end - tag_end);
        for (auto i = body.find("<input"); i != std::string_view::npos; i = body.find("<input", i + 1)) {
            const auto ie = body.find('>', i);
            if (ie == std::string_view::npos) {
                break;
            }
            const auto input = body.substr(i, ie - i);
            const std::string name = Attr(input, "name");
            if (name.empty() || (Attr(input, "type") != "hidden" && !Attr(input, "type").empty())) {
                continue;
            }
            query += (query.empty() ? "" : "&") + UrlEncode(name) + "=" + UrlEncode(Attr(input, "value"));
        }
        std::string url = action;
        if (url.front() == '/') {
            url = "https://drive.usercontent.google.com" + url;
        }
        std::string host, path;
        if (!SplitUrl(url, host, path) || !HostAllowed(host) || query.empty() || url.find('?') != std::string::npos) {
            continue;
        }
        page.kind = DrivePage::Kind::Confirm;
        page.url = url + "?" + query;
        return page;
    }
    // 2. Quota, sign-in wall, missing file.
    if (ContainsNoCase(html, "too many users have viewed or downloaded") || ContainsNoCase(html, "download quota") ||
        ContainsNoCase(html, "quota exceeded") || ContainsNoCase(html, "can't view or download this file at this time") ||
        ContainsNoCase(html, "can&#39;t view or download this file at this time")) {
        page.kind = DrivePage::Kind::Quota;
        page.error = "Google Drive: too many downloads of this file today, try again later";
        return page;
    }
    if (ContainsNoCase(html, "you need access") || ContainsNoCase(html, "request access") ||
        ContainsNoCase(html, "accounts.google.com/servicelogin") || ContainsNoCase(html, "sign in to continue")) {
        page.kind = DrivePage::Kind::NoAccess;
        page.error = "Google Drive: the file is not shared publicly";
        return page;
    }
    if (ContainsNoCase(html, "does not exist") || ContainsNoCase(html, "file not found") ||
        ContainsNoCase(html, "was not found")) {
        page.kind = DrivePage::Kind::NotFound;
        page.error = "Google Drive: file not found";
        return page;
    }
    page.error = "Google Drive sent a web page instead of the file";
    return page;
}

std::vector<DriveItem> ParseDriveFolder(std::string_view html) {
    std::vector<DriveItem> out;
    constexpr std::string_view open = "id=\"entry-";
    for (auto p = html.find(open); p != std::string_view::npos;) {
        const auto next = html.find(open, p + open.size());
        const auto chunk = html.substr(p, next == std::string_view::npos ? std::string_view::npos : next - p);
        p = next;
        DriveItem item;
        const auto id_end = chunk.find('"', open.size());
        if (id_end == std::string_view::npos) {
            continue;
        }
        item.id = std::string(chunk.substr(open.size(), id_end - open.size()));
        const auto href = chunk.find("href=\"");
        if (href != std::string_view::npos) {
            const auto end = chunk.find('"', href + 6);
            const auto url = chunk.substr(href + 6, end == std::string_view::npos ? 0 : end - href - 6);
            item.is_folder = Contains(url, "/folders/");
        }
        constexpr std::string_view title = "class=\"flip-entry-title\">";
        const auto t = chunk.find(title);
        if (t == std::string_view::npos) {
            continue;
        }
        const auto t_end = chunk.find("</div>", t);
        item.name = HtmlUnescape(std::string(chunk.substr(t + title.size(), t_end == std::string_view::npos ? 0 : t_end - t - title.size())));
        if (DriveIdOk(item.id) && !item.name.empty()) {
            out.push_back(std::move(item));
        }
    }
    return out;
}

std::optional<DriveItem> PickDriveFile(const DriveLister& list, const std::string& folder_id,
                                       const std::string& pattern, std::string* error, int max_depth,
                                       int max_listings) {
    std::vector<std::pair<std::string, int>> queue{{folder_id, 0}};
    int listings = 0;
    bool root_read = false;
    for (std::size_t i = 0; i < queue.size() && listings < max_listings; ++i) {
        const auto [id, depth] = queue[i];
        ++listings;
        auto items = list(id);
        if (!items) {
            continue;
        }
        root_read = true;
        std::sort(items->begin(), items->end(), [](const DriveItem& a, const DriveItem& b) { return a.name < b.name; });
        for (const auto& item : *items) {
            if (!item.is_folder && Mods::GlobMatch(pattern, item.name)) {
                return item;
            }
        }
        if (depth < max_depth) {
            for (const auto& item : *items) {
                if (item.is_folder) {
                    queue.emplace_back(item.id, depth + 1);
                }
            }
        }
    }
    if (error != nullptr) {
        *error = root_read ? fmt::format("Google Drive: no file named \"{}\" in the folder", pattern)
                           : "Google Drive: the folder could not be read (is it shared publicly?)";
    }
    return std::nullopt;
}

std::vector<PackFolder> FindPackFolders(const std::vector<std::string>& files, Layout layout) {
    std::set<std::string> tex_dirs;
    std::set<std::string> json_dirs;
    for (const auto& f : files) {
        const auto parts = SplitPath(f);
        if (std::any_of(parts.begin(), parts.end(), [](const std::string& p) {
                return p == "__MACOSX" || p.rfind("._", 0) == 0;
            })) {
            continue;
        }
        const std::string base = BaseOf(f);
        if (IsTextureFile(base)) {
            tex_dirs.insert(DirOf(f));
        } else if (Lower(base) == "pack.json") {
            json_dirs.insert(DirOf(f));
        }
    }
    if (tex_dirs.empty()) {
        return {};
    }
    // lowest common parent of the texture folders
    std::vector<std::string> common = SplitPath(*tex_dirs.begin());
    for (const auto& d : tex_dirs) {
        const auto parts = SplitPath(d);
        std::size_t n = 0;
        while (n < common.size() && n < parts.size() && common[n] == parts[n]) {
            ++n;
        }
        common.resize(n);
    }
    std::string lca;
    for (const auto& c : common) {
        lca = JoinPath(lca, c);
    }
    if (layout == Layout::Variants) {
        std::set<std::string> children;
        bool direct = false;
        const std::size_t depth = common.size();
        for (const auto& d : tex_dirs) {
            const auto parts = SplitPath(d);
            if (parts.size() <= depth) {
                direct = true;
            } else {
                children.insert(parts[depth]);
            }
        }
        if (!direct && children.size() > 1) {
            std::vector<PackFolder> out;
            for (const auto& c : children) {
                out.push_back({JoinPath(lca, c), c});
            }
            return out;
        }
    }
    std::string root = lca;
    std::size_t best = std::string::npos;
    for (const auto& d : json_dirs) {
        const bool covers = std::all_of(tex_dirs.begin(), tex_dirs.end(),
                                        [&](const std::string& t) { return IsAncestorOrSelf(d, t); });
        if (covers && (best == std::string::npos || d.size() > best)) {
            best = d.size();
            root = d;
        }
    }
    return {{root, ""}};
}

u64 RequiredSpace(u64 archive_size, u64 already_downloaded) {
    constexpr u64 margin = 256ull * 1024 * 1024;
    const u64 left = archive_size > already_downloaded ? archive_size - already_downloaded : 0;
    return left + archive_size + margin; // the rest of the download, plus the unpacked copy, plus room
}

std::string SizeText(u64 bytes) {
    if (bytes == 0) {
        return {};
    }
    constexpr double mb = 1024.0 * 1024.0;
    if (bytes < 1024ull * 1024 * 1024) {
        return fmt::format("{} MB", std::max<u64>(1, static_cast<u64>(bytes / mb + 0.5)));
    }
    return fmt::format("{:.1f} GB", static_cast<double>(bytes) / (1024.0 * mb));
}

std::string FreeSpaceError(u64 required, u64 available) {
    if (required <= available) {
        return {};
    }
    return fmt::format("Not enough free space: {} needed, {} free", SizeText(required),
                       available == 0 ? "0 MB" : SizeText(available));
}

std::optional<u64> FreeSpace(const std::string& dir) {
    std::error_code ec;
    fs::path p = dir;
    while (!p.empty() && !fs::exists(p, ec)) {
        p = p.parent_path();
    }
    if (p.empty()) {
        return std::nullopt;
    }
    const auto info = fs::space(p, ec);
    if (ec) {
        return std::nullopt;
    }
    return static_cast<u64>(info.available);
}

// ---------------------------------------------------------------------------------------------
// Installing a finished archive

namespace {

std::string PacksRoot(const std::string& load_dir, u64 title_id) {
    return (fs::path(load_dir) / "texture-packs" / CustomTex::TitleFolder(title_id)).string();
}

bool HasMarker(const fs::path& folder, std::string* id = nullptr) {
    std::ifstream in(folder / kMarker);
    if (!in) {
        return false;
    }
    try {
        const auto j = nlohmann::json::parse(in);
        if (id != nullptr) {
            *id = j.value("id", "");
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

std::string InstallArchive(const std::string& archive_path, const Entry& entry, u64 title_id,
                           const std::string& load_dir, const std::atomic_bool* cancel,
                           std::vector<std::string>* installed, const std::string& tool_override) {
    std::string head(16, '\0');
    {
        std::ifstream in(archive_path, std::ios::binary);
        if (!in) {
            return "Cannot read the downloaded file";
        }
        in.read(head.data(), static_cast<std::streamsize>(head.size()));
        head.resize(static_cast<std::size_t>(in.gcount()));
    }
    const auto kind = Mods::DetectArchive(head);
    if (kind == Mods::ArchiveKind::Unknown) {
        return "The download is not a zip, 7z or rar archive";
    }
    const fs::path work = fs::path(load_dir) / "texture-downloads" / (entry.id + ".x");
    std::error_code ec;
    fs::remove_all(work, ec);
    fs::create_directories(work, ec);
    if (ec) {
        return "Cannot make a work folder";
    }
    struct Cleanup {
        fs::path p;
        ~Cleanup() {
            std::error_code e;
            fs::remove_all(p, e);
        }
    } cleanup{work};

    const std::uint64_t max_total = 64ull << 30;
    std::string error;
    const std::string tool = tool_override.empty() ? Mods::FindBsdtar() : tool_override;
    const auto archive_size = fs::file_size(archive_path, ec);
    if (kind == Mods::ArchiveKind::Zip && tool.empty() && !ec && archive_size <= 256ull * 1024 * 1024) {
        // No bsdtar: the built-in reader takes a small zip.
        std::ifstream in(archive_path, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const auto r = Mods::ExtractZip(bytes);
        if (!r.error.empty()) {
            return r.error;
        }
        for (const auto& f : r.files) {
            const fs::path dst = work / f.path;
            fs::create_directories(dst.parent_path(), ec);
            std::ofstream out(dst, std::ios::binary | std::ios::trunc);
            out.write(f.data.data(), static_cast<std::streamsize>(f.data.size()));
            if (!out) {
                return "Not enough free space to unpack";
            }
        }
    } else {
        error = Mods::ExtractFileToDir(archive_path, kind, (work / "out").string(), max_total, 7200, cancel,
                                       tool_override);
        if (!error.empty()) {
            return error;
        }
    }
    const fs::path out_root = fs::exists(work / "out") ? work / "out" : work;

    std::vector<std::string> files;
    for (fs::recursive_directory_iterator it(out_root, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            files.push_back(it->path().lexically_relative(out_root).generic_string());
        }
    }
    const auto folders = FindPackFolders(files, entry.layout);
    if (folders.empty()) {
        return "No textures found in the download";
    }
    const fs::path root = PacksRoot(load_dir, title_id);
    fs::create_directories(root, ec);
    if (ec) {
        return "Cannot make the pack library folder";
    }
    std::vector<std::string> made;
    for (const auto& pf : folders) {
        const std::string base = pf.name.empty() ? entry.folder : entry.folder + " - " + pf.name;
        const std::string name = SanitizeFolderName(base);
        fs::path dst = root / name;
        std::string other_id;
        if (fs::exists(dst, ec)) {
            if (HasMarker(dst, &other_id) && other_id == entry.id) {
                fs::remove_all(dst, ec); // downloaded again: replace our own copy
            } else {
                for (int n = 2; fs::exists(dst, ec) && n < 100; ++n) {
                    dst = root / fmt::format("{} ({})", name, n);
                }
            }
        }
        const fs::path src = pf.dir.empty() ? out_root : out_root / pf.dir;
        fs::rename(src, dst, ec);
        if (ec) {
            return "Cannot move the pack into the library";
        }
        {
            nlohmann::json m;
            m["id"] = entry.id;
            m["source"] = entry.source.type == Source::Type::GithubRelease ? "github"
                          : entry.source.type == Source::Type::Gdrive      ? "gdrive"
                                                                           : "https";
            std::ofstream(dst / kMarker) << m.dump();
        }
        if (!fs::exists(dst / "pack.json", ec)) {
            nlohmann::json pj;
            pj["name"] = pf.name.empty() ? entry.name : entry.name + " - " + pf.name;
            pj["author"] = entry.author;
            std::ofstream(dst / "pack.json") << pj.dump(2);
        }
        made.push_back(dst.filename().string());
        if (pf.dir.empty()) {
            break; // the whole extraction folder moved
        }
    }
    if (installed != nullptr) {
        *installed = made;
    }
    return {};
}

// ---------------------------------------------------------------------------------------------
// Transfer

namespace {

struct Step {
    int status = 0; // 0: no reply
    std::string location;
    std::string error;
    std::string html;        // a text/html body (capped)
    bool is_file = false;    // body was written to the part file
    bool cancelled = false;
    u64 start = 0;           // first byte of the file this reply began at (resume point or 0)
    std::optional<u64> total; // whole file size when the host said
};

struct StepIo {
    std::string part_path;
    u64 resume_from = 0;
    const std::atomic_bool* cancel = nullptr;
    std::atomic<u64>* done = nullptr;
    std::atomic<u64>* total = nullptr;
    std::function<std::string(u64 total, u64 resume)> check_space; // "" = fine
};

Step GetStep(const std::string& host, const std::string& path, const StepIo& io) {
    Step step;
    const char* ca = CaBundle();
    if (ca == nullptr) {
        step.error = "No certificate store found";
        return step;
    }
    FILE* file = nullptr;
    try {
        httplib::Client client(host);
        client.set_connection_timeout(15);
        client.set_read_timeout(60);
        client.set_write_timeout(30);
        client.set_follow_location(false);
        client.set_ca_cert_path(ca);
        client.enable_server_certificate_verification(true);
        httplib::Headers headers{{"Accept-Encoding", "identity"}};
        if (io.resume_from > 0) {
            headers.emplace("Range", fmt::format("bytes={}-", io.resume_from));
        }
        bool refused = false;
        const auto res = client.Get(
            path, headers,
            [&](const httplib::Response& r) {
                step.status = r.status;
                step.location = r.get_header_value("Location");
                const std::string type = Lower(r.get_header_value("Content-Type"));
                const bool html = type.rfind("text/html", 0) == 0;
                if ((r.status == 200 || r.status == 206) && !html) {
                    u64 start = 0;
                    std::optional<u64> total;
                    if (r.status == 206) {
                        // Content-Range: bytes a-b/total
                        unsigned long long a = 0, b = 0, t = 0;
                        const std::string cr = r.get_header_value("Content-Range");
                        if (std::sscanf(cr.c_str(), "bytes %llu-%llu/%llu", &a, &b, &t) >= 2) {
                            start = a;
                            if (std::sscanf(cr.c_str(), "bytes %llu-%llu/%llu", &a, &b, &t) == 3) {
                                total = t;
                            }
                        } else {
                            start = io.resume_from;
                        }
                        if (start != io.resume_from) {
                            step.error = "The host answered a different range";
                            return false;
                        }
                    } else if (const auto cl = r.get_header_value("Content-Length"); !cl.empty()) {
                        total = std::strtoull(cl.c_str(), nullptr, 10);
                    }
                    step.start = start;
                    step.total = total;
                    if (total) {
                        io.total->store(*total);
                        if (io.check_space) {
                            const auto problem = io.check_space(*total, start);
                            if (!problem.empty()) {
                                step.error = problem;
                                refused = true;
                                return false;
                            }
                        }
                    }
                    file = std::fopen(io.part_path.c_str(), start > 0 ? "ab" : "wb");
                    if (file == nullptr) {
                        step.error = "Cannot write the download file";
                        return false;
                    }
                    step.is_file = true;
                    io.done->store(start);
                }
                return true;
            },
            [&](const char* data, std::size_t len) {
                if (io.cancel != nullptr && io.cancel->load()) {
                    step.cancelled = true;
                    return false;
                }
                if (step.is_file) {
                    if (std::fwrite(data, 1, len, file) != len) {
                        step.error = "Not enough free space (or the disk is read-only)";
                        return false;
                    }
                    io.done->fetch_add(len);
                } else if (step.html.size() < 512 * 1024) {
                    step.html.append(data, std::min<std::size_t>(len, 512 * 1024 - step.html.size()));
                }
                return true;
            });
        if (file != nullptr) {
            std::fclose(file);
            file = nullptr;
        }
        if (!res && step.error.empty() && !step.cancelled) {
            step.error = httplib::to_string(res.error());
        } else if (!res && refused) {
            // step.error already set
        }
    } catch (const std::exception& e) {
        if (file != nullptr) {
            std::fclose(file);
        }
        step.error = e.what();
    }
    return step;
}

std::string HexSha256(const std::string& path, const std::atomic_bool* cancel, bool* cancelled) {
    std::ifstream in(path, std::ios::binary);
    CryptoPP::SHA256 hash;
    std::vector<char> buf(1 << 20);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const auto n = in.gcount();
        if (n > 0) {
            hash.Update(reinterpret_cast<const CryptoPP::byte*>(buf.data()), static_cast<std::size_t>(n));
        }
        if (cancel != nullptr && cancel->load()) {
            *cancelled = true;
            return {};
        }
    }
    CryptoPP::byte digest[CryptoPP::SHA256::DIGESTSIZE];
    hash.Final(digest);
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (const auto b : digest) {
        out += hex[b >> 4];
        out += hex[b & 15];
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Downloader

Downloader::Downloader(std::string load_dir, Catalog catalog) : load_dir_(std::move(load_dir)), catalog_(std::move(catalog)) {
    if (load_dir_.empty()) {
        load_dir_ = FileUtil::GetUserPath(FileUtil::UserPath::LoadDir);
    }
}

Downloader::~Downloader() {
    cancel_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }
    if (refresher_.joinable()) {
        refresher_.join();
    }
}

void Downloader::SetCatalog(Catalog catalog) {
    std::lock_guard lock{mutex_};
    catalog_ = std::move(catalog);
}

void Downloader::RefreshCatalogAsync() {
    std::lock_guard lock{mutex_};
    if (refreshing_) {
        return;
    }
    refreshing_ = true;
    if (refresher_.joinable()) {
        refresher_.join();
    }
    refresher_ = std::thread([this] {
        const auto r = Cheats::Online::HttpsGetRaw(
            "https://raw.githubusercontent.com", fmt::format("/{}/main/{}", kOwnerRepo, kCatalogPath), 1024 * 1024);
        if (r.status == 200 && r.error.empty()) {
            std::string error;
            if (auto parsed = ParseCatalog(r.body, &error); parsed && !parsed->packs.empty()) {
                SetCatalog(std::move(*parsed));
            } else {
                LOG_WARNING(Frontend, "Textures: catalog not usable: {}", error);
            }
        }
        std::lock_guard lock{mutex_};
        refreshing_ = false;
    });
}

std::vector<PackInfo> Downloader::ListForTitle(u64 title_id) const {
    std::vector<Entry> entries;
    {
        std::lock_guard lock{mutex_};
        for (const auto& e : catalog_.packs) {
            if (e.ServesTitle(title_id)) {
                entries.push_back(e);
            }
        }
    }
    std::vector<PackInfo> out;
    std::error_code ec;
    for (const auto& e : entries) {
        PackInfo info;
        info.id = e.id;
        info.name = e.name;
        info.author = e.author;
        info.author_url = e.author_url;
        info.licence = e.licence;
        info.notes = e.notes;
        info.size = e.size;
        info.size_text = SizeText(e.size);
        info.partial = fs::exists(fs::path(load_dir_) / "texture-downloads" / (e.id + ".part"), ec);
        for (fs::directory_iterator it(PacksRoot(load_dir_, title_id), ec), end; !ec && it != end; it.increment(ec)) {
            std::string id;
            if (it->is_directory(ec) && HasMarker(it->path(), &id) && id == e.id) {
                info.installed = true;
                info.folders.push_back(it->path().filename().string());
            }
        }
        std::sort(info.folders.begin(), info.folders.end());
        out.push_back(std::move(info));
    }
    return out;
}

State Downloader::GetState() const {
    std::lock_guard lock{mutex_};
    State s = state_;
    if (s.Busy()) {
        s.bytes_done = done_.load();
        s.bytes_total = total_.load();
    }
    return s;
}

void Downloader::SetState(State::Phase phase, const std::string& text) {
    std::lock_guard lock{mutex_};
    state_.phase = phase;
    state_.text = text;
}

void Downloader::Fail(const std::string& error) {
    std::lock_guard lock{mutex_};
    if (cancel_.load() || error == "Cancelled") {
        state_.phase = State::Phase::Cancelled;
        state_.text = "Cancelled";
        state_.error.clear();
    } else {
        state_.phase = State::Phase::Failed;
        state_.text = "Failed";
        state_.error = error;
    }
}

void Downloader::Cancel() {
    cancel_ = true;
}

void Downloader::DiscardPartial(const std::string& pack_id) {
    if (!IdOk(pack_id) || GetState().Busy()) {
        return;
    }
    std::error_code ec;
    fs::remove(fs::path(load_dir_) / "texture-downloads" / (pack_id + ".part"), ec);
}

bool Downloader::Start(const std::string& pack_id, u64 title_id) {
    Entry entry;
    {
        std::lock_guard lock{mutex_};
        if (state_.Busy()) {
            return false;
        }
        const Entry* e = catalog_.Find(pack_id);
        if (e == nullptr || !e->ServesTitle(title_id)) {
            return false;
        }
        entry = *e;
        state_ = State{};
        state_.phase = State::Phase::Resolving;
        state_.pack_id = pack_id;
        state_.text = "Looking Up";
        state_.bytes_total = entry.size;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    cancel_ = false;
    done_ = 0;
    total_ = entry.size;
    worker_ = std::thread([this, entry = std::move(entry), title_id] { Run(entry, title_id); });
    return true;
}


namespace {

bool IsDriveHost(const std::string& host) {
    return host == "https://drive.google.com" || host == "https://drive.usercontent.google.com";
}

} // namespace

namespace {

// Errors another source of the same file might get around.
bool Retryable(const std::string& error) {
    for (const char* needle : {"too many downloads", "not shared", "not found", "(404)", "web page", "no matching file",
                               "no file named", "could not be read", "refused the download", "not found"}) {
        if (error.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::optional<std::vector<DriveItem>> ListDriveFolder(const std::string& folder_id) {
    const auto r = Cheats::Online::HttpsGetRaw("https://drive.google.com", "/embeddedfolderview?id=" + folder_id,
                                               2 * 1024 * 1024);
    if (r.status != 200 || !r.error.empty()) {
        return std::nullopt;
    }
    return ParseDriveFolder(r.body);
}

} // namespace

// One source: where the file is, the free space check, the transfer into `part`. "" on success.
std::string Downloader::FetchSource(const Entry& entry, const Source& source, const std::string& part) {
    std::error_code ec;
    std::vector<std::string> extra = source.hosts;
    std::string url;
    u64 expected = entry.size;

    switch (source.type) {
    case Source::Type::GithubRelease: {
        const std::string api_path = fmt::format("/repos/{}/releases/{}", source.repo,
                                                 source.tag == "latest" ? "latest" : "tags/" + source.tag);
        const auto r = Cheats::Online::HttpsGetRaw("https://api.github.com", api_path, 4 * 1024 * 1024);
        if (cancel_) {
            return "Cancelled";
        }
        if (!r.error.empty() || r.status == 0) {
            return r.error.empty() ? "No reply from GitHub" : r.error;
        }
        if (r.status != 200) {
            return r.status == 404 ? "Release not found" : fmt::format("Server returned {}", r.status);
        }
        try {
            const auto j = nlohmann::json::parse(r.body);
            for (const auto& a : j.at("assets")) {
                if (Mods::GlobMatch(source.asset_pattern, a.value("name", ""))) {
                    url = a.value("browser_download_url", "");
                    expected = a.value("size", expected);
                    break;
                }
            }
        } catch (const std::exception&) {
            return "Release information not readable";
        }
        if (url.empty()) {
            return "The release has no matching file";
        }
        break;
    }
    case Source::Type::Https: {
        url = source.url;
        std::string host, path;
        if (SplitUrl(url, host, path)) {
            extra.push_back(host.substr(8));
        }
        break;
    }
    case Source::Type::Gdrive:
        url = DriveUrl(source.file_id);
        break;
    case Source::Type::GdriveFolder: {
        SetState(State::Phase::Resolving, "Looking Up");
        std::string error;
        const auto item = PickDriveFile(ListDriveFolder, source.folder_id, source.file_pattern, &error);
        if (cancel_) {
            return "Cancelled";
        }
        if (!item) {
            return error;
        }
        url = DriveUrl(item->id);
        break;
    }
    }

    const u64 existing = fs::exists(part, ec) ? static_cast<u64>(fs::file_size(part, ec)) : 0;
    const auto check_space = [this](u64 total, u64 resume) {
        const auto free = FreeSpace(load_dir_);
        return free ? FreeSpaceError(RequiredSpace(total, resume), *free) : std::string{};
    };
    if (expected > 0) {
        if (const auto problem = check_space(expected, existing); !problem.empty()) {
            return problem;
        }
    }
    SetState(State::Phase::Downloading, "Downloading");
    total_ = expected;
    done_ = existing;
    u64 resume = existing;
    bool retried_range = false;
    int confirms = 0;
    for (int hop = 0; hop < 12; ++hop) {
        std::string host, path;
        if (!SplitUrl(url, host, path) || !HostAllowed(host, extra)) {
            return "Download refused: not an allowed host";
        }
        StepIo io{part, resume, &cancel_, &done_, &total_, check_space};
        const Step step = GetStep(host, path, io);
        if (step.cancelled || cancel_) {
            return "Cancelled";
        }
        if (step.is_file) {
            if (!step.error.empty()) {
                return step.error;
            }
            const u64 size = static_cast<u64>(fs::file_size(part, ec));
            if (step.total && size != *step.total) {
                return "The download was cut short, start it again to resume";
            }
            return {};
        }
        if (step.status >= 300 && step.status < 400 && !step.location.empty()) {
            url = step.location.front() == '/' ? host + step.location : step.location;
            continue;
        }
        if (step.status == 416 && resume > 0 && !retried_range) {
            fs::remove(part, ec);
            resume = 0;
            done_ = 0;
            retried_range = true;
            continue;
        }
        if (IsDriveHost(host) && !step.html.empty()) {
            const DrivePage page = ParseDrivePage(step.html);
            if (page.kind == DrivePage::Kind::Confirm && confirms < 2) {
                ++confirms;
                url = page.url;
                continue;
            }
            return page.error.empty() ? "Google Drive did not give the file" : page.error;
        }
        if (step.status == 0) {
            return step.error.empty() ? "No connection" : step.error;
        }
        if (step.status == 404) {
            return "Download not found (404)";
        }
        if (step.status == 200 && !step.html.empty()) {
            return "The link leads to a web page, not a file";
        }
        return fmt::format("The host refused the download ({})", step.status);
    }
    return "Too many redirects";
}

void Downloader::Run(Entry entry, u64 title_id) {
    std::error_code ec;
    const fs::path dl_dir = fs::path(load_dir_) / "texture-downloads";
    fs::create_directories(dl_dir, ec);
    if (ec) {
        return Fail("Cannot make the download folder");
    }
    const std::string part = (dl_dir / (entry.id + ".part")).string();

    // 1. The file, from the first source that gives it.
    std::vector<Source> sources{entry.source};
    sources.insert(sources.end(), entry.alternates.begin(), entry.alternates.end());
    std::string error;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        error = FetchSource(entry, sources[i], part);
        if (error.empty() || cancel_ || !Retryable(error)) {
            break;
        }
        if (i + 1 < sources.size()) {
            LOG_WARNING(Frontend, "Textures: {} failed ({}), trying the next source", entry.id, error);
            fs::remove(part, ec); // a partial file belongs to the source that wrote it
        }
    }
    if (!error.empty()) {
        return Fail(error);
    }

    // 2. Checksum, unpack.
    if (!entry.sha256.empty()) {
        SetState(State::Phase::Verifying, "Checking");
        bool cancelled = false;
        const std::string sum = HexSha256(part, &cancel_, &cancelled);
        if (cancelled) {
            return Fail("Cancelled");
        }
        if (sum != entry.sha256) {
            fs::remove(part, ec);
            return Fail("The download does not match the catalog checksum");
        }
    }
    SetState(State::Phase::Extracting, "Unpacking");
    std::vector<std::string> installed;
    error = InstallArchive(part, entry, title_id, load_dir_, &cancel_, &installed);
    if (!error.empty()) {
        if (error != "Cancelled") {
            fs::remove(part, ec);
        }
        return Fail(error);
    }
    fs::remove(part, ec);
    std::lock_guard lock{mutex_};
    state_.phase = State::Phase::Done;
    state_.text = "Installed";
    state_.installed = installed;
}

Result Downloader::Delete(u64 title_id, const std::string& folder) {
    Result result;
    if (folder.empty() || folder.find('/') != std::string::npos || folder.find('\\') != std::string::npos ||
        folder == "." || folder == "..") {
        result.error = "Bad folder name";
        return result;
    }
    if (GetState().Busy()) {
        result.error = "A download is running";
        return result;
    }
    const fs::path dir = fs::path(PacksRoot(load_dir_, title_id)) / folder;
    std::error_code ec;
    if (!fs::is_directory(dir, ec) || fs::is_symlink(dir, ec) || !HasMarker(dir)) {
        result.error = "Not a downloaded pack";
        return result;
    }
    CustomTex::Paths paths;
    paths.load_dir = load_dir_;
    if (const auto active = CustomTex::ActivePack(paths, title_id);
        active && fs::equivalent(fs::path(active->path), dir, ec)) {
        if (const auto r = CustomTex::Deactivate(paths, title_id); !r.ok) {
            result.error = r.error;
            return result;
        }
    }
    fs::remove_all(dir, ec);
    if (ec) {
        result.error = "Cannot remove the folder";
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace CustomTex::Download
