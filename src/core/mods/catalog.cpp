// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include "core/mods/catalog_schema.h"
#include <algorithm>
#include <cctype>
#include <json.hpp>
#include <boost/crc.hpp>
#include <fmt/format.h>
#include "core/custom_tex/download.h" // AzaharFrame catalog3: Drive ids and https links
#include "core/mods/archive.h"
#include "core/mods/catalog.h"
#include "core/mods/mega.h" // AzaharFrame mega

namespace Mods {

namespace {

bool IdOk(std::string_view id) {
    if (id.empty() || id.size() > 64) {
        return false;
    }
    for (const char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) {
            return false;
        }
    }
    return id.front() != '.';
}

bool HostNameOk(std::string_view h) {
    return !h.empty() && h.find('.') != std::string_view::npos && h.front() != '.' && h.front() != '-' &&
           std::all_of(h.begin(), h.end(), [](char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
           });
}

bool RepoOk(std::string_view repo) {
    const auto slash = repo.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 >= repo.size() ||
        repo.find('/', slash + 1) != std::string_view::npos) {
        return false;
    }
    for (const char c : repo) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' ||
              c == '/')) {
            return false;
        }
    }
    return repo != "." && repo.substr(0, 2) != "..";
}

bool RefOk(std::string_view s) {
    if (s.empty()) {
        return false;
    }
    for (const char c : s) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' ||
              c == '/')) {
            return false;
        }
    }
    return s.find("..") == std::string_view::npos;
}

std::optional<u64> ParseTid(const std::string& s) {
    if (s.size() != 16) {
        return std::nullopt;
    }
    u64 v = 0;
    for (const char c : s) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return std::nullopt;
        }
        v = v << 4 | static_cast<u64>(c <= '9' ? c - '0' : (std::tolower(c) - 'a' + 10));
    }
    return v;
}

std::string Str(const nlohmann::json& j, const char* key) {
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

// AzaharFrame catalog3: "YYYY-MM-DD" or "YYYY-MM" (the month the source was last updated)
bool DateOk(std::string_view d) {
    if ((d.size() != 10 && d.size() != 7) || d[4] != '-' || (d.size() == 10 && d[7] != '-')) {
        return false;
    }
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (i != 4 && i != 7 && !std::isdigit(static_cast<unsigned char>(d[i]))) {
            return false;
        }
    }
    const int month = (d[5] - '0') * 10 + (d[6] - '0');
    return month >= 1 && month <= 12;
}

// AzaharFrame catalog3: install rules (also the "textures" block's)
std::vector<Rule> ParseRules(const nlohmann::json& rules) {
    std::vector<Rule> out;
    if (!rules.is_array()) {
        return out;
    }
    for (const auto& r : rules) {
        Rule rule{Str(r, "from"), Str(r, "to")};
        if (rule.from.empty() || !SafeRelativePath(rule.from)) {
            throw std::string("bad install rule");
        }
        const auto to = SafeRelativePath(rule.to);
        if (!to) {
            throw std::string("bad install rule");
        }
        // keep the folder marker (trailing '/')
        rule.to = to->empty() ? std::string{} : (rule.to.back() == '/' ? *to + "/" : *to);
        out.push_back(std::move(rule));
    }
    return out;
}

// AzaharFrame catalog3: one source object (an entry's "source" or each of its "parts"). Throws
// std::string with the reason.
Source ParseSource(const nlohmann::json& src_json) {
    const nlohmann::json* src = &src_json;
    Source s;
    const std::string type = Str(*src, "type");
    if (type == "owner_repo_path") {
        s.type = Source::Type::OwnerRepoPath;
        s.repo = kOwnerRepo;
        s.path = Str(*src, "path");
    } else if (type == "github_release") {
        s.type = Source::Type::GithubRelease;
        s.repo = Str(*src, "repo");
        s.tag = Str(*src, "tag").empty() ? "latest" : Str(*src, "tag");
        s.asset_pattern = Str(*src, "asset_pattern");
        if (s.asset_pattern.empty() || s.asset_pattern.find('/') != std::string::npos) {
            throw std::string("bad asset_pattern");
        }
        if (s.tag != "latest" && !RefOk(s.tag)) {
            throw std::string("bad tag");
        }
    } else if (type == "github_repo_path") {
        s.type = Source::Type::GithubRepoPath;
        s.repo = Str(*src, "repo");
        s.ref = Str(*src, "ref").empty() ? "main" : Str(*src, "ref");
        s.path = Str(*src, "path");
        if (!RefOk(s.ref)) {
            throw std::string("bad ref");
        }
    } else if (type == "gamebanana") { // AzaharFrame modcat
        s.type = Source::Type::Gamebanana;
        const auto mod = src->find("mod");
        if (mod == src->end() || !mod->is_number_unsigned() || mod->get<u64>() == 0) {
            throw std::string("bad GameBanana mod id");
        }
        s.gb_mod = mod->get<u64>();
        s.asset_pattern = Str(*src, "file_pattern").empty() ? "*.zip" : Str(*src, "file_pattern");
        if (s.asset_pattern.find('/') != std::string::npos) {
            throw std::string("bad file_pattern");
        }
    } else if (type == "mega") { // AzaharFrame mega
        s.type = Source::Type::Mega;
        s.mega_link = Str(*src, "link");
        const auto link = Mega::ParseLink(s.mega_link);
        if (!link) {
            throw std::string("bad MEGA link");
        }
        s.asset_pattern = Str(*src, "file_pattern").empty() ? "*.zip" : Str(*src, "file_pattern");
        if (link->folder && link->node.empty() && s.asset_pattern.empty()) {
            throw std::string("bad file_pattern");
        }
    } else if (type == "https") { // AzaharFrame catalog3: the texture list's source types
        s.type = Source::Type::Https;
        s.url = Str(*src, "url");
        std::string host, path;
        if (!CustomTex::Download::SplitUrl(s.url, host, path) || path.find("..") != std::string::npos ||
            !HostNameOk(host.substr(8))) {
            throw std::string("bad https source");
        }
        s.hosts.push_back(host.substr(8));
    } else if (type == "gdrive") {
        s.type = Source::Type::Gdrive;
        s.file_id = Str(*src, "file_id");
        if (!CustomTex::Download::DriveIdOk(s.file_id)) {
            throw std::string("bad Google Drive file id");
        }
    } else if (type == "gdrive_folder") {
        s.type = Source::Type::GdriveFolder;
        s.folder_id = Str(*src, "folder_id");
        s.asset_pattern = Str(*src, "file_pattern");
        if (!CustomTex::Download::DriveIdOk(s.folder_id) || s.asset_pattern.empty() ||
            s.asset_pattern.size() > 100 || s.asset_pattern.find('/') != std::string::npos) {
            throw std::string("bad gdrive_folder source");
        }
    } else {
        throw std::string("unknown source type");
    }
    if (const auto hosts = src->find("hosts"); hosts != src->end() && s.type == Source::Type::Https) {
        if (!hosts->is_array() || hosts->size() > 8) {
            throw std::string("bad hosts");
        }
        for (const auto& h : *hosts) {
            if (!h.is_string() || !HostNameOk(h.get<std::string>())) {
                throw std::string("bad hosts (exact names only)");
            }
            s.hosts.push_back(h.get<std::string>());
        }
    }
    if (const auto name = Str(*src, "name"); !name.empty()) {
        if (name.size() > 120 || name.find('/') != std::string::npos || name.find('\\') != std::string::npos ||
            name == "." || name == "..") {
            throw std::string("bad name");
        }
        s.name = name;
    }
    if (s.type == Source::Type::OwnerRepoPath || s.type == Source::Type::GithubRelease ||
        s.type == Source::Type::GithubRepoPath) {
        if (!RepoOk(s.repo)) {
            throw std::string("bad repo");
        }
    }
    if (s.type == Source::Type::GithubRepoPath || s.type == Source::Type::OwnerRepoPath) {
        const auto safe = SafeRelativePath(s.path);
        if (s.path.empty() || !safe || safe->empty()) {
            throw std::string("bad path");
        }
    }
    return s;
}

// Throws std::string with the reason.
CatalogEntry ParseEntry(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::string("not an object");
    }
    CatalogEntry e;
    e.id = Str(j, "id");
    if (!IdOk(e.id)) {
        throw std::string("bad id");
    }
    e.name = Str(j, "name");
    if (e.name.empty()) {
        throw std::string("no name");
    }
    e.title = Str(j, "title");
    e.author = Str(j, "author");
    e.licence = Str(j, "licence");
    e.homepage = Str(j, "homepage");
    e.notes = Str(j, "notes");
    e.author_url = Str(j, "author_url"); // AzaharFrame modcat
    if (!e.author_url.empty() && e.author_url.rfind("https://", 0) != 0) {
        throw std::string("author_url is not https");
    }
    e.updated = Str(j, "updated");
    if (!e.updated.empty() && !DateOk(e.updated)) {
        e.updated.clear();
    }
    e.version = Str(j, "version"); // AzaharFrame ui3
    if (e.version.size() > 24) {
        e.version.clear();
    }
    const auto ids = j.find("title_ids");
    if (ids == j.end() || !ids->is_array() || ids->empty()) {
        throw std::string("no title_ids");
    }
    for (const auto& id : *ids) {
        const auto tid = id.is_string() ? ParseTid(id.get<std::string>()) : std::nullopt;
        if (!tid) {
            throw std::string("bad title ID");
        }
        e.title_ids.push_back(*tid);
    }
    const auto src = j.find("source");
    const auto first_part = j.find("parts");
    if (src != j.end() && src->is_object()) {
        e.source = ParseSource(*src);
    } else if (first_part != j.end() && first_part->is_array() && !first_part->empty() &&
               (*first_part)[0].is_object()) {
        e.source = ParseSource((*first_part)[0]); // parts only: the first part stands for the source
    } else {
        throw std::string("no source");
    }
    const Source& s = e.source;
    // Verified: the maintainer's own mods and the GameBanana mods the maintainer has played through
    // (AzaharFrame modcat); a GitHub release from a community author is never marked verified.
    e.verified = j.value("verified", false) &&
                 (s.type == Source::Type::OwnerRepoPath || s.type == Source::Type::Gamebanana);
    // (a MEGA mod is never verified here: the owner marks one by testing it, not the catalog)
    const auto chip = j.find("version_chip"); // AzaharFrame modcat
    if (chip != j.end() && chip->is_object()) {
        e.chip = Str(*chip, "label");
        e.chip_tip = Str(*chip, "tip");
        if (e.chip.size() > 16 || e.chip_tip.size() > 120) {
            throw std::string("bad version_chip");
        }
        const auto crcs = chip->find("code_crc32");
        if (crcs != chip->end() && crcs->is_array()) {
            for (const auto& c : *crcs) {
                const std::string text = c.is_string() ? c.get<std::string>() : std::string{};
                if (text.size() != 8 ||
                    !std::all_of(text.begin(), text.end(), [](char ch) { return std::isxdigit(static_cast<unsigned char>(ch)); })) {
                    throw std::string("bad code_crc32");
                }
                e.code_crc32.push_back(static_cast<u32>(std::stoul(text, nullptr, 16)));
            }
        }
    }
    const auto install = j.find("install");
    if (install != j.end() && install->is_object()) {
        const auto rules = install->find("rules");
        if (rules != install->end()) {
            e.rules = ParseRules(*rules);
        }
    }
    // AzaharFrame catalog3: textures that come with the mod, parts, lifetime downloads
    const auto textures = j.find("textures");
    if (textures != j.end() && textures->is_object()) {
        const auto rules = textures->find("rules");
        if (rules != textures->end()) {
            e.texture_rules = ParseRules(*rules);
        }
        if (e.texture_rules.empty()) {
            throw std::string("textures block without rules");
        }
    }
    const auto parts = j.find("parts");
    if (parts != j.end()) {
        if (!parts->is_array() || parts->empty() || parts->size() > 40) {
            throw std::string("bad parts");
        }
        for (const auto& part : *parts) {
            if (!part.is_object()) {
                throw std::string("bad parts");
            }
            e.parts.push_back(ParseSource(part));
        }
    }
    const auto size = j.find("size");
    if (size != j.end() && size->is_number_unsigned()) {
        e.size = size->get<u64>();
    }
    const auto downloads = j.find("downloads");
    if (downloads != j.end() && downloads->is_number_unsigned()) {
        e.downloads = downloads->get<u64>();
    }
    return e;
}

} // namespace

bool CatalogEntry::ServesTitle(u64 title_id) const {
    for (const u64 id : title_ids) {
        if (id == title_id) {
            return true;
        }
    }
    return false;
}

bool CatalogEntry::Installable() const {
    if (!parts.empty()) {
        return !FindBsdtar().empty() || !FindSevenZip().empty(); // AzaharFrame catalog3: parts are joined and unpacked by bsdtar or 7zz
    }
    if (source.type != Source::Type::GithubRelease && source.type != Source::Type::Gamebanana) {
        return true;
    }
    std::string p = source.asset_pattern;
    for (char& c : p) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const auto ends = [&](std::string_view suffix) {
        return p.size() >= suffix.size() && p.compare(p.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    return !(ends(".7z") || ends(".rar")) || !FindBsdtar().empty() || !FindSevenZip().empty(); // those need bsdtar or 7zz
}

const CatalogEntry* Catalog::Find(std::string_view id) const {
    for (const auto& e : mods) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

std::optional<Catalog> ParseCatalog(std::string_view json, std::string* error,
                                    std::vector<std::string>* warnings) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json);
    } catch (const std::exception& e) {
        if (error) {
            *error = std::string("Not JSON: ") + e.what();
        }
        return std::nullopt;
    }
    const auto mods = root.is_object() ? root.find("mods") : root.end();
    if (!root.is_object() || mods == root.end() || !mods->is_array()) {
        if (error) {
            *error = "Not a mod catalog";
        }
        return std::nullopt;
    }
    Catalog out;
    if (!ReadSchema(root, out.schema, error)) { // AzaharFrame harden
        return std::nullopt;
    }
    for (const auto& item : *mods) {
        try {
            auto entry = ParseEntry(item);
            if (out.Find(entry.id) != nullptr) {
                throw std::string("duplicate id");
            }
            out.mods.push_back(std::move(entry));
        } catch (const std::string& why) {
            if (warnings) {
                warnings->push_back(fmt::format("{}: {}",
                                                item.is_object() ? Str(item, "id") : std::string("?"),
                                                why));
            }
        }
    }
    return out;
}

Catalog DefaultCatalog() {
    auto c = ParseCatalog(DefaultCatalogJson());
    return c ? std::move(*c) : Catalog{};
}

bool GlobMatch(std::string_view pattern, std::string_view text) {
    std::size_t p = 0, t = 0, star = std::string_view::npos, mark = 0;
    while (t < text.size()) {
        if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            mark = t;
        } else if (p < pattern.size() && pattern[p] == text[t]) {
            ++p;
            ++t;
        } else if (star != std::string_view::npos) {
            p = star + 1;
            t = ++mark;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

u32 Crc32(const u8* data, std::size_t size) {
    boost::crc_32_type result;
    result.process_bytes(data, size);
    return result.checksum();
}

std::string TitleIdText(u64 title_id) {
    return fmt::format("{:016X}", title_id);
}

} // namespace Mods
