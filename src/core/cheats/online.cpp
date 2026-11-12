// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <array>
#include <map>
#include <mutex>
#include <set>
#include <httplib.h>
#include <json.hpp>
#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/cheats/online.h"
#include "core/file_sys/ncch_container.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/mods/library.h"

namespace Cheats::Online {

namespace {

constexpr const char* kApiHost = "https://api.github.com";
constexpr const char* kRawHost = "https://raw.githubusercontent.com";
constexpr const char* kRepoPath = "/simplysanatori/3ds-game-fixes/main/";
constexpr const char* kTreePath = "/repos/simplysanatori/3ds-game-fixes/git/trees/main?recursive=1";
constexpr const char* kFixMarker = "code.ips.developer";

std::string_view Trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' ||
                          s.back() == '\n')) {
        s.remove_suffix(1);
    }
    return s;
}

// Each line of the notes, without its newline.
std::vector<std::string_view> Lines(std::string_view text) {
    std::vector<std::string_view> out;
    while (!text.empty()) {
        const auto eol = text.find('\n');
        out.push_back(text.substr(0, eol));
        text = eol == std::string_view::npos ? std::string_view{} : text.substr(eol + 1);
    }
    return out;
}

std::mutex fix_mutex;
std::map<u64, bool> fix_available;

void SetFixAvailable(u64 title_id, bool value) {
    std::lock_guard lock{fix_mutex};
    fix_available[title_id] = value;
}

} // namespace

std::string_view SourceName(Source source) {
    switch (source) {
    case Source::Developer:
        return "Developer";
    case Source::Sharkive:
        return "Sharkive";
    default:
        return "Your Cheats";
    }
}

Source SourceOf(std::string_view comments) {
    for (const auto line : Lines(comments)) {
        if (line == "source: Developer") {
            return Source::Developer;
        }
        if (line == "source: Sharkive") {
            return Source::Sharkive;
        }
    }
    return Source::User;
}

std::string WithSource(std::string_view comments, Source source) {
    std::string out = fmt::format("source: {}\n", SourceName(source));
    for (const auto line : Lines(comments)) {
        if (line.substr(0, 8) == "source: ") {
            continue;
        }
        out += std::string(line) + '\n';
    }
    return out;
}

std::string VersionNote(std::string_view comments) {
    constexpr std::string_view prefix = "Version note: ";
    for (const auto line : Lines(comments)) {
        if (line.substr(0, prefix.size()) == prefix) {
            return std::string(Trim(line.substr(prefix.size())));
        }
    }
    return {};
}

std::optional<TreeFiles> ParseTree(std::string_view json, u64 title_id, std::string* error) {
    const auto fail = [&](const char* why) -> std::optional<TreeFiles> {
        if (error) {
            *error = why;
        }
        return std::nullopt;
    };
    nlohmann::json doc = nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return fail("Unreadable file list");
    }
    const auto tree = doc.find("tree");
    if (tree == doc.end() || !tree->is_array()) {
        const auto msg = doc.find("message");
        return fail(msg != doc.end() && msg->is_string() ? "GitHub: refused the request"
                                                         : "Unexpected file list");
    }
    const std::string id = fmt::format("{:016X}", title_id);
    TreeFiles files;
    for (const auto& item : *tree) {
        if (!item.is_object() || !item.contains("path") || !item["path"].is_string()) {
            continue;
        }
        if (item.contains("type") && item["type"] != "blob") {
            continue;
        }
        const std::string path = item["path"];
        // games/<slug>/cheats/<ID>.txt  and  games/<slug>/patch/<ID>/code.ips
        const auto parts = [&] {
            std::vector<std::string> v;
            std::size_t start = 0;
            while (true) {
                const auto slash = path.find('/', start);
                v.push_back(path.substr(start, slash - start));
                if (slash == std::string::npos) {
                    break;
                }
                start = slash + 1;
            }
            return v;
        }();
        if (parts.size() == 4 && parts[0] == "games" && parts[2] == "cheats" &&
            parts[3] == id + ".txt" && files.cheats.empty()) {
            files.cheats = path;
        } else if (parts.size() == 5 && parts[0] == "games" && parts[2] == "patch" &&
                   parts[3] == id && parts[4] == "code.ips" && files.patch.empty()) {
            files.patch = path;
        }
    }
    return files;
}

MergeResult Merge(const std::vector<Fetched>& fetched, const std::vector<std::string>& existing_names,
                  const Sharkive::InstalledVersion& installed) {
    MergeResult out;
    std::vector<std::string> taken = existing_names;
    for (const auto& source : fetched) {
        auto fresh = Sharkive::NewEntries(source.entries, taken);
        for (auto& entry : fresh) {
            taken.push_back(entry.name);
            entry.comments = WithSource(entry.comments, source.source);
        }
        if (installed.revision) {
            out.by_revision = true;
            out.mismatched += Sharkive::MarkOtherRevisions(fresh, *installed.revision);
        } else {
            out.mismatched += Sharkive::MarkOtherVersions(fresh, installed.version);
        }
        (source.source == Source::Developer ? out.developer : out.sharkive) +=
            static_cast<int>(fresh.size());
        for (auto& entry : fresh) {
            out.added.push_back(std::move(entry));
        }
    }
    return out;
}

// The static LibreSSL has no CA store of its own; SteamOS and the common distributions keep one of
// these.
static const char* CaBundle() {
    static constexpr std::array<const char*, 4> candidates{
        "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/cert.pem", "/etc/ssl/ca-bundle.pem"};
    for (const char* path : candidates) {
        if (FileUtil::Exists(path)) {
            return path;
        }
    }
    return nullptr;
}

HttpResult HttpsGet(const std::string& host, const std::string& path) {
    constexpr std::size_t max_body = 8 * 1024 * 1024;
    HttpResult out;
    const char* ca = CaBundle();
    if (ca == nullptr) {
        out.error = "No certificate store found";
        return out;
    }
    try {
        httplib::Client client(host);
        client.set_connection_timeout(10);
        client.set_read_timeout(15);
        client.set_write_timeout(15);
        client.set_follow_location(false);
        client.set_ca_cert_path(ca);
        client.enable_server_certificate_verification(true);
        const auto res = client.Get(path);
        if (!res) {
            out.error = httplib::to_string(res.error());
            LOG_WARNING(Frontend, "Cheats: GET {}{} failed: {}", host, path, out.error);
            return out;
        }
        out.status = res->status;
        if ((res->status == 403 || res->status == 429) && host == kApiHost) {
            out.error = "GitHub rate limit reached, try again later";
        } else if (res->status != 200 && res->status != 404) {
            out.error = "Server returned " + std::to_string(res->status);
        } else if (res->body.size() > max_body) {
            out.error = "Reply too large";
        } else {
            out.body = res->body;
        }
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    return out;
}

RawReply HttpsGetRaw(const std::string& host, const std::string& path, std::size_t max_body) {
    RawReply out;
    const char* ca = CaBundle();
    if (ca == nullptr) {
        out.error = "No certificate store found";
        return out;
    }
    try {
        httplib::Client client(host);
        client.set_connection_timeout(10);
        client.set_read_timeout(60);
        client.set_write_timeout(15);
        client.set_follow_location(false);
        client.set_ca_cert_path(ca);
        client.enable_server_certificate_verification(true);
        httplib::Headers headers;
        if (host == kApiHost) {
            headers.emplace("Accept", "application/vnd.github+json");
        }
        bool too_large = false;
        std::string body;
        const auto res = client.Get(
            path, headers, [](const httplib::Response&) { return true; },
            [&](const char* data, std::size_t len) {
                if (body.size() + len > max_body) {
                    too_large = true;
                    return false;
                }
                body.append(data, len);
                return true;
            });
        if (too_large) {
            out.error = "Download too large";
            return out;
        }
        if (!res) {
            out.error = httplib::to_string(res.error());
            LOG_WARNING(Frontend, "Mods: GET {}{} failed: {}", host, path, out.error);
            return out;
        }
        out.status = res->status;
        if ((res->status == 403 || res->status == 429) && host == kApiHost) {
            out.error = "GitHub rate limit reached, try again later";
        } else if (res->status >= 300 && res->status < 400) {
            out.location = res->get_header_value("Location");
        } else if (res->status != 200 && res->status != 404) {
            out.error = "Server returned " + std::to_string(res->status);
        } else {
            out.body = std::move(body);
        }
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    return out;
}

std::string StatusText(const DownloadResult& r) {
    if (!r.error.empty()) {
        return fmt::format("Download failed: {}", r.error);
    }
    if (!r.found_any) {
        return "No cheats for this game";
    }
    const int added = r.developer_added + r.sharkive_added;
    std::string text = added == 0   ? "No new cheats"
                       : added == 1 ? "1 cheat added"
                                    : fmt::format("{} cheats added", added);
    if (r.mismatched > 0) {
        text += fmt::format(", {} for another {}", r.mismatched,
                            r.by_revision ? "revision" : "version");
        if (!r.update_version.empty()) {
            text += fmt::format(" (update {} installed)", r.update_version);
        }
    }
    if (!r.warning.empty()) {
        text += " (" + r.warning + ")";
    }
    return text;
}

Sharkive::InstalledVersion QueryInstalledVersion(u64 title_id, const std::string& game_path) {
    using Service::FS::MediaType;
    const u64 low = title_id & 0xFFFFFFFFULL;
    const auto tmd_version = [](u64 tid) -> std::optional<u16> {
        for (const MediaType media : {MediaType::SDMC, MediaType::NAND}) {
            const auto tmd = Service::AM::GetTitleMetadata(media, tid);
            if (tmd.LoadResult() == Loader::ResultStatus::Success) {
                return tmd.GetTitleVersion();
            }
        }
        return std::nullopt;
    };
    const auto update = tmd_version((0x0004000EULL << 32) | low);
    const auto base = tmd_version((0x00040000ULL << 32) | low);
    std::optional<u16> remaster;
    if (!update && !base && !game_path.empty()) {
        FileSys::NCCHContainer ncch(game_path);
        if (ncch.Load() == Loader::ResultStatus::Success) {
            const auto& r = ncch.exheader_header.codeset_info.flags.remaster_version;
            remaster = static_cast<u16>(r[0] | (r[1] << 8));
        }
    }
    return Sharkive::SelectInstalledVersion(update, base, remaster);
}

DownloadResult DownloadCheatsWith(const HttpGet& get, CheatEngine& engine, u64 title_id,
                                  const std::string& game_path) {
    DownloadResult out;
    std::vector<Fetched> fetched;
    std::vector<std::string> errors;

    // Developer: one tree request, then the files
    std::string tree_error;
    const auto tree_reply = get(kApiHost, kTreePath);
    std::optional<TreeFiles> files;
    if (tree_reply.status == 200) {
        files = ParseTree(tree_reply.body, title_id, &tree_error);
        if (!files) {
            errors.push_back(tree_error);
        }
    } else {
        errors.push_back(tree_reply.error.empty() ? "No file list" : tree_reply.error);
    }
    if (files) {
        out.fix_available = !files->patch.empty();
        SetFixAvailable(title_id, out.fix_available);
        if (!files->cheats.empty()) {
            const auto reply = get(kRawHost, std::string(kRepoPath) + files->cheats);
            if (reply.status == 200) {
                out.found_any = true;
                fetched.push_back({Source::Developer, Sharkive::Parse(reply.body)});
            } else {
                errors.push_back(reply.error.empty() ? "Developer cheats not readable" : reply.error);
            }
        }
        out.found_any = out.found_any || out.fix_available;
    }

    // Sharkive
    const auto reply = get(kRawHost, Sharkive::UrlPath(title_id));
    bool sharkive_ok = false;
    if (reply.status == 200) {
        auto entries = Sharkive::Parse(reply.body);
        if (!entries.empty()) {
            out.found_any = true;
            fetched.push_back({Source::Sharkive, std::move(entries)});
        }
        sharkive_ok = true;
    } else if (reply.status == 404) {
        sharkive_ok = true;
    } else {
        errors.push_back(reply.error.empty() ? "Sharkive not readable" : reply.error);
    }

    if (!files && !sharkive_ok) {
        out.error = errors.empty() ? "No reply" : errors.front();
        return out;
    }
    if (!errors.empty()) {
        out.warning = errors.front();
    }

    engine.LoadCheatFile(title_id);
    std::vector<std::string> names;
    for (const auto& cheat : engine.GetCheats()) {
        names.push_back(cheat->GetName());
    }
    const auto installed = QueryInstalledVersion(title_id, game_path);
    const auto merged = Merge(fetched, names, installed);
    for (const auto& entry : merged.added) {
        auto cheat = std::make_shared<GatewayCheat>(entry.name, entry.code, entry.comments);
        cheat->SetEnabled(false);
        engine.AddCheat(std::move(cheat));
    }
    if (!merged.added.empty()) {
        engine.SaveCheatFile(title_id);
    }
    out.developer_added = merged.developer;
    out.sharkive_added = merged.sharkive;
    out.mismatched = merged.mismatched;
    out.by_revision = merged.by_revision;
    if (installed.source == Sharkive::VersionSource::UpdateTmd && installed.version) {
        out.update_version = Sharkive::ToString(*installed.version);
    }
    return out;
}

DownloadResult DownloadCheats(CheatEngine& engine, u64 title_id, const std::string& game_path) {
    return DownloadCheatsWith(HttpsGet, engine, title_id, game_path);
}

std::vector<CheatInfo> ListCheats(CheatEngine& engine, u64 title_id) {
    engine.LoadCheatFile(title_id);
    std::vector<CheatInfo> out;
    for (const auto& cheat : engine.GetCheats()) {
        const std::string comments = cheat->GetComments();
        out.push_back({cheat->GetName(), SourceOf(comments), cheat->IsEnabled(),
                       VersionNote(comments)});
    }
    return out;
}

bool SetCheatEnabled(CheatEngine& engine, u64 title_id, std::size_t index, bool enabled) {
    engine.LoadCheatFile(title_id);
    const auto cheats = engine.GetCheats();
    if (index >= cheats.size()) {
        return false;
    }
    cheats[index]->SetEnabled(enabled);
    engine.SaveCheatFile(title_id);
    return true;
}

// ---------------------------------------------------------------------------------------------

std::string ModsDir(u64 title_id) {
    return fmt::format("{}mods/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                       title_id);
}

FixState GetFixStateIn(const std::string& dir) {
    const bool ips = FileUtil::Exists(dir + "code.ips");
    const bool ours = FileUtil::Exists(dir + kFixMarker);
    if (ips && ours) {
        return FixState::Installed;
    }
    if (ips || FileUtil::Exists(dir + "exefs/code.ips")) {
        return FixState::OtherPatch;
    }
    return FixState::NotInstalled;
}

// The maintainer's fix of the catalog for the title (the verified entry), nullptr when there is none.
static const Mods::CatalogEntry* OwnerFix(const Mods::Catalog& catalog, u64 title_id) {
    for (const auto& e : catalog.mods) {
        if (e.verified && e.ServesTitle(title_id)) {
            return &e;
        }
    }
    return nullptr;
}

FixState GetFixState(u64 title_id) {
    for (const auto& m : Mods::Default().ListLoaded(title_id)) {
        if (m.verified && m.state == Mods::ModState::Active) {
            return FixState::Installed;
        }
    }
    return GetFixStateIn(ModsDir(title_id)) == FixState::NotInstalled ? FixState::NotInstalled
                                                                      : FixState::OtherPatch;
}

bool FixAvailable(u64 title_id) {
    {
        std::lock_guard lock{fix_mutex};
        const auto it = fix_available.find(title_id);
        if (it != fix_available.end() && it->second) {
            return true;
        }
    }
    return OwnerFix(Mods::Default().GetCatalog(), title_id) != nullptr;
}

FixOutcome InstallFixTo(const std::string& dir, std::string_view ips) {
    if (ips.size() < 8 || ips.substr(0, 5) != "PATCH") {
        return {FixResult::Error, "Not an IPS patch"};
    }
    switch (GetFixStateIn(dir)) {
    case FixState::OtherPatch:
        return {FixResult::Conflict, "The Game has its own code patch"};
    case FixState::Installed: {
        std::string have(FileUtil::GetSize(dir + "code.ips"), '\0');
        FileUtil::IOFile in(dir + "code.ips", "rb");
        if (in.IsOpen() && in.ReadBytes(have.data(), have.size()) == have.size() && have == ips) {
            return {FixResult::AlreadyInstalled, {}};
        }
        break; // ours, an older copy: replace it
    }
    case FixState::NotInstalled:
        break;
    }
    if (!FileUtil::CreateFullPath(dir + "x")) {
        return {FixResult::Error, "Cannot create the mods folder"};
    }
    {
        FileUtil::IOFile out(dir + "code.ips", "wb");
        if (!out.IsOpen() || out.WriteBytes(ips.data(), ips.size()) != ips.size()) {
            return {FixResult::Error, "Cannot write code.ips"};
        }
    }
    FileUtil::IOFile marker(dir + kFixMarker, "wb");
    const std::string_view text = "installed by the Developer source\n";
    if (!marker.IsOpen() || marker.WriteBytes(text.data(), text.size()) != text.size()) {
        FileUtil::Delete(dir + "code.ips");
        return {FixResult::Error, "Cannot write the marker"};
    }
    return {FixResult::Ok, {}};
}

FixOutcome RemoveFixFrom(const std::string& dir) {
    switch (GetFixStateIn(dir)) {
    case FixState::NotInstalled:
        return {FixResult::NotAvailable, "No fix installed"};
    case FixState::OtherPatch:
        return {FixResult::Conflict, "The code patch is not the Developer's"};
    case FixState::Installed:
        break;
    }
    FileUtil::Delete(dir + "code.ips");
    FileUtil::Delete(dir + kFixMarker);
    return {FixResult::Ok, {}};
}

FixOutcome InstallFixWith(const HttpGet& get, u64 title_id, const std::string& dir) {
    // Conflicts are found before any download
    if (GetFixStateIn(dir) == FixState::OtherPatch) {
        return {FixResult::Conflict, "The Game has its own code patch"};
    }
    const auto tree = get(kApiHost, kTreePath);
    if (tree.status != 200) {
        return {FixResult::Error, tree.error.empty() ? "No file list" : tree.error};
    }
    std::string error;
    const auto files = ParseTree(tree.body, title_id, &error);
    if (!files) {
        return {FixResult::Error, error};
    }
    SetFixAvailable(title_id, !files->patch.empty());
    if (files->patch.empty()) {
        return {FixResult::NotAvailable, "No fix for this game"};
    }
    const auto ips = get(kRawHost, std::string(kRepoPath) + files->patch);
    if (ips.status != 200) {
        return {FixResult::Error, ips.error.empty() ? "Fix not readable" : ips.error};
    }
    return InstallFixTo(dir, ips.body);
}

FixOutcome InstallFix(u64 title_id) {
    auto& library = Mods::Default();
    library.RefreshCatalogOnce();
    const auto* fix = OwnerFix(library.GetCatalog(), title_id);
    if (fix == nullptr) {
        return {FixResult::NotAvailable, "No fix for this game"};
    }
    SetFixAvailable(title_id, true);
    const auto outcome = library.Install(title_id, fix->id, false);
    switch (outcome.result) {
    case Mods::InstallResult::Ok:
        return {FixResult::Ok, {}};
    case Mods::InstallResult::AlreadyInstalled:
        return {FixResult::AlreadyInstalled, {}};
    case Mods::InstallResult::Conflict:
        return {FixResult::Conflict, outcome.conflict_unmanaged
                                         ? "The Game has its own code patch"
                                         : "Another Mod is using the code patch"};
    case Mods::InstallResult::NotAvailable:
        return {FixResult::NotAvailable, outcome.error};
    default:
        return {FixResult::Error, outcome.error};
    }
}

FixOutcome RemoveFix(u64 title_id) {
    auto& library = Mods::Default();
    for (const auto& m : library.ListLoaded(title_id)) {
        if (m.verified && m.managed) {
            const auto r = library.Remove(title_id, m.id);
            return r.ok ? FixOutcome{FixResult::Ok, {}} : FixOutcome{FixResult::Error, r.error};
        }
    }
    return RemoveFixFrom(ModsDir(title_id)); // NotAvailable, or Conflict for a code.ips that is not ours
}

} // namespace Cheats::Online
