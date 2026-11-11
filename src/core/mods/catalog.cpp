// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <cctype>
#include <json.hpp>
#include <fmt/format.h>
#include "core/mods/archive.h"
#include "core/mods/catalog.h"

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
    if (src == j.end() || !src->is_object()) {
        throw std::string("no source");
    }
    const std::string type = Str(*src, "type");
    Source& s = e.source;
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
    } else {
        throw std::string("unknown source type");
    }
    if (!RepoOk(s.repo)) {
        throw std::string("bad repo");
    }
    if (s.type != Source::Type::GithubRelease) {
        const auto safe = SafeRelativePath(s.path);
        if (s.path.empty() || !safe || safe->empty()) {
            throw std::string("bad path");
        }
    }
    // The maintainer's own mods only: a community entry is never marked verified.
    e.verified = j.value("verified", false) && s.type == Source::Type::OwnerRepoPath;
    const auto install = j.find("install");
    if (install != j.end() && install->is_object()) {
        const auto rules = install->find("rules");
        if (rules != install->end() && rules->is_array()) {
            for (const auto& r : *rules) {
                Rule rule{Str(r, "from"), Str(r, "to")};
                if (rule.from.empty() || !SafeRelativePath(rule.from)) {
                    throw std::string("bad install rule");
                }
                const auto to = SafeRelativePath(rule.to);
                if (!to) {
                    throw std::string("bad install rule");
                }
                // keep the folder marker (trailing '/')
                rule.to = to->empty() ? std::string{}
                                      : (rule.to.back() == '/' ? *to + "/" : *to);
                e.rules.push_back(std::move(rule));
            }
        }
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
    if (source.type != Source::Type::GithubRelease) {
        return true;
    }
    std::string p = source.asset_pattern;
    for (char& c : p) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const auto ends = [&](std::string_view suffix) {
        return p.size() >= suffix.size() && p.compare(p.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    return !(ends(".7z") || ends(".rar")) || !FindBsdtar().empty(); // those need the system's bsdtar
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
    out.schema = root.value("schema", 1);
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

std::string TitleIdText(u64 title_id) {
    return fmt::format("{:016X}", title_id);
}

} // namespace Mods
