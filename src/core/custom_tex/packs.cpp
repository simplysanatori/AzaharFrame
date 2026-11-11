// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texdata: see packs.h.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <system_error>
#include <fmt/format.h>
#include <json.hpp>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/custom_tex/packs.h"

namespace fs = std::filesystem;

namespace CustomTex {

namespace {

constexpr std::size_t kMaxFormatSample = 256; // files looked at per pack for the format

std::mutex size_mutex;
std::map<std::string, u64> size_cache;

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool StartsWithNoCase(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && Lower(s.substr(0, prefix.size())) == Lower(prefix);
}

std::string Canonical(const fs::path& p) {
    std::error_code ec;
    const fs::path c = fs::weakly_canonical(p, ec);
    return (ec ? p : c).generic_string();
}

bool IsDir(const fs::path& p) {
    std::error_code ec;
    return fs::is_directory(p, ec); // follows symlinks
}

std::string ReadFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

fs::path ManagedDir(const Paths& paths, u64 title_id) {
    return fs::path(paths.load_dir) / "texture-packs" / TitleFolder(title_id);
}

fs::path LoadPath(const Paths& paths, u64 title_id) {
    return fs::path(paths.load_dir) / "textures" / TitleFolder(title_id);
}

/// Matches `name` against a pattern with at most one '*'-wildcard-bearing last component
bool Glob(const std::string& pattern, const std::string& name) {
    const auto star = pattern.find('*');
    if (star == std::string::npos) {
        return pattern == name;
    }
    const std::string head = pattern.substr(0, star);
    const std::string tail = pattern.substr(star + 1);
    return name.size() >= head.size() + tail.size() && name.compare(0, head.size(), head) == 0 &&
           name.compare(name.size() - tail.size(), tail.size(), tail) == 0;
}

std::vector<fs::path> ExpandRoot(const std::string& root) {
    std::vector<fs::path> out;
    if (root.empty()) {
        return out;
    }
    const fs::path p(root);
    if (p.filename().string().find('*') == std::string::npos) {
        out.push_back(p);
        return out;
    }
    std::error_code ec;
    const fs::path parent = p.parent_path();
    const std::string pattern = p.filename().string();
    for (fs::directory_iterator it(parent, ec), end; !ec && it != end; it.increment(ec)) {
        if (Glob(pattern, it->path().filename().string())) {
            out.push_back(it->path());
        }
    }
    return out;
}

bool IsHexTitleFolder(const std::string& name, const std::string& tid) {
    return StartsWithNoCase(name, tid) &&
           (name.size() == tid.size() || name[tid.size()] == '-' || name[tid.size()] == '_');
}

Pack MakePack(const fs::path& folder, const std::string& title_folder, bool managed) {
    Pack pack;
    pack.path = folder.generic_string();
    pack.id = Canonical(folder);
    pack.managed = managed;
    const std::string folder_name = folder.filename().string();
    const std::string parent_name = folder.parent_path().filename().string();

    const PackJson meta = ParsePackJson(ReadFile(folder / "pack.json"));
    pack.author = meta.author;
    pack.version = meta.version;
    pack.description = meta.description;
    pack.format = DetectFormat(pack.path);
    pack.resolution = !meta.resolution.empty()
                          ? meta.resolution
                          : ResolutionHint(managed ? std::string() : parent_name, folder_name);
    if (!meta.name.empty()) {
        pack.name = meta.name;
    } else {
        pack.name = managed ? folder_name
                            : DeriveName(parent_name, folder_name, title_folder);
        if (pack.format == Format::Png && Lower(pack.name).find("png") == std::string::npos) {
            pack.name += " (PNG)";
        }
    }
    pack.size_bytes = CachedSize(pack.path);
    return pack;
}

std::string Sanitize(std::string s) {
    for (char& c : s) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|' || static_cast<unsigned char>(c) < 32) {
            c = '_';
        }
    }
    while (!s.empty() && (s.back() == '.' || s.back() == ' ')) {
        s.pop_back();
    }
    while (!s.empty() && (s.front() == '.' || s.front() == ' ')) {
        s.erase(s.begin());
    }
    return s;
}

/// Moves the real folder at the load place into the managed library. Returns the new path.
Result BackUpRealFolder(const Paths& paths, u64 title_id, const fs::path& load_path) {
    Result r;
    std::error_code ec;
    if (fs::is_empty(load_path, ec)) {
        fs::remove(load_path, ec); // an empty folder holds nothing
        if (ec) {
            r.error = "Could not remove the empty folder: " + ec.message();
            return r;
        }
        r.ok = true;
        return r;
    }
    const PackJson meta = ParsePackJson(ReadFile(load_path / "pack.json"));
    std::string base = Sanitize(meta.name);
    if (base.empty()) {
        base = "Original";
    }
    const fs::path dir = ManagedDir(paths, title_id);
    fs::create_directories(dir, ec);
    if (ec) {
        r.error = "Could not create " + dir.generic_string() + ": " + ec.message();
        return r;
    }
    fs::path target = dir / base;
    for (int n = 2; fs::exists(fs::symlink_status(target, ec)); ++n) {
        target = dir / fmt::format("{} {}", base, n);
    }
    fs::rename(load_path, target, ec);
    if (ec) {
        r.error = "Could not move the existing folder into the library: " + ec.message();
        return r;
    }
    r.ok = true;
    r.backed_up = target.generic_string();
    LOG_INFO(Render, "texture packs: moved {} to {}", load_path.generic_string(), r.backed_up);
    return r;
}

/// Makes sure nothing but (at most) a symlink is at the load place. Real folder: moved away.
Result ClearLoadPlace(const Paths& paths, u64 title_id, const fs::path& load_path) {
    std::error_code ec;
    const fs::file_status st = fs::symlink_status(load_path, ec);
    if (!fs::exists(st)) {
        return {true, {}, {}};
    }
    if (fs::is_symlink(st)) {
        return {true, {}, {}};
    }
    if (fs::is_directory(st)) {
        return BackUpRealFolder(paths, title_id, load_path);
    }
    return {false, "The texture place is a file, not a folder: " + load_path.generic_string(), {}};
}

} // namespace

std::string FormatName(Format format) {
    switch (format) {
    case Format::Png:
        return "PNG";
    case Format::Ktx:
        return "ASTC/KTX";
    case Format::Dds:
        return "DDS";
    case Format::Mixed:
        return "Mixed";
    default:
        return {};
    }
}

std::string TitleFolder(u64 title_id) {
    return fmt::format("{:016X}", title_id);
}

std::string LoadFolder(const Paths& paths, u64 title_id) {
    return LoadPath(paths, title_id).generic_string();
}

std::vector<std::string> ReadExtraRoots(const std::string& load_dir) {
    std::vector<std::string> roots;
    std::istringstream file(ReadFile(fs::path(load_dir) / "texture-packs" / "roots.txt"));
    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }
        roots.push_back(line.substr(first));
    }
    if (const char* env = std::getenv("AZAHARFRAME_TEXTURE_ROOTS")) {
        std::istringstream list(env);
        std::string item;
        while (std::getline(list, item, ':')) {
            if (!item.empty()) {
                roots.push_back(item);
            }
        }
    }
    return roots;
}

bool WriteExtraRoots(const std::string& load_dir, const std::vector<std::string>& roots) {
    std::error_code ec;
    const fs::path dir = fs::path(load_dir) / "texture-packs";
    fs::create_directories(dir, ec);
    std::ofstream f(dir / "roots.txt", std::ios::trunc);
    if (!f) {
        return false;
    }
    f << "# Extra places texture packs are looked for, one per line. A '*' is allowed in the last part.\n";
    for (const std::string& root : roots) {
        f << root << '\n';
    }
    return static_cast<bool>(f);
}

Paths DefaultPaths() {
    Paths paths;
    paths.load_dir = FileUtil::GetUserPath(FileUtil::UserPath::LoadDir);
    while (paths.load_dir.size() > 1 && (paths.load_dir.back() == '/' || paths.load_dir.back() == '\\')) {
        paths.load_dir.pop_back();
    }
    paths.extra_roots = ReadExtraRoots(paths.load_dir);
#if defined(__unix__) || defined(__APPLE__)
    std::string data_home;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        data_home = xdg;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        data_home = std::string(home) + "/.local/share";
    }
    if (!data_home.empty()) {
        const fs::path desktop = fs::path(data_home) / "azahar-emu" / "load";
        std::error_code ec;
        if (fs::is_directory(desktop, ec) && Canonical(desktop) != Canonical(paths.load_dir)) {
            paths.extra_roots.push_back((desktop / "textures-*").generic_string());
        }
    }
#endif
    return paths;
}

PackJson ParsePackJson(const std::string& text) {
    PackJson out;
    if (text.empty()) {
        return out;
    }
    const nlohmann::json doc = nlohmann::json::parse(text, nullptr, false, true);
    if (!doc.is_object()) {
        return out;
    }
    const auto get = [&doc](const char* key, std::string& dest) {
        const auto it = doc.find(key);
        if (it == doc.end()) {
            return;
        }
        if (it->is_string()) {
            dest = it->get<std::string>();
        } else if (it->is_number()) {
            dest = it->dump();
        }
    };
    get("name", out.name);
    get("author", out.author);
    get("version", out.version);
    get("description", out.description);
    get("resolution", out.resolution);
    return out;
}

namespace {

std::vector<std::string> Tokens(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '-' || c == '_' || c == ' ' || c == '.') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

bool IsResolutionToken(const std::string& t) {
    const std::string l = Lower(t);
    if (l.size() >= 2 && l.back() == 'k' &&
        std::all_of(l.begin(), l.end() - 1, [](unsigned char c) { return std::isdigit(c); })) {
        return true;
    }
    if (l.size() >= 2 && l.back() == 'p' &&
        std::all_of(l.begin(), l.end() - 1, [](unsigned char c) { return std::isdigit(c); })) {
        return true;
    }
    return false;
}

std::string Pretty(const std::string& t) {
    std::string l = Lower(t);
    if (IsResolutionToken(t)) {
        if (l.back() == 'k') {
            l.back() = 'K';
        }
        return l; // "4K", "1080p"
    }
    if (std::all_of(t.begin(), t.end(), [](unsigned char c) { return std::islower(c) || std::isdigit(c); })) {
        std::string r = t;
        r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
        return r;
    }
    return t;
}

bool IsFormatToken(const std::string& t) {
    const std::string l = Lower(t);
    return l.rfind("astc", 0) == 0 || l.rfind("ktx", 0) == 0 || l == "dds" || l == "gpu";
}

} // namespace

std::string DeriveName(const std::string& parent_folder, const std::string& folder,
                       const std::string& title_folder) {
    std::vector<std::string> parts;
    // The parent "textures-4k" / "textures_1080p" says the variant
    std::vector<std::string> ptoks = Tokens(parent_folder);
    if (!ptoks.empty() && Lower(ptoks.front()) == "textures") {
        ptoks.erase(ptoks.begin());
        for (const auto& t : ptoks) {
            parts.push_back(Pretty(t));
        }
    }
    std::string rest = folder;
    if (IsHexTitleFolder(rest, title_folder)) {
        rest = rest.substr(title_folder.size());
    }
    for (const auto& t : Tokens(rest)) {
        if (!IsFormatToken(t)) {
            parts.push_back(Pretty(t));
        }
    }
    std::string name;
    for (const auto& p : parts) {
        name += name.empty() ? p : " " + p;
    }
    return name.empty() ? "Default" : name;
}

std::string ResolutionHint(const std::string& parent_folder, const std::string& folder) {
    for (const std::string& s : {folder, parent_folder}) {
        for (const auto& t : Tokens(s)) {
            if (IsResolutionToken(t)) {
                return Pretty(t);
            }
        }
    }
    return {};
}

Format DetectFormat(const std::string& folder) {
    std::size_t png = 0, ktx = 0, dds = 0, seen = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(folder, fs::directory_options::follow_directory_symlink,
                                             ec),
         end;
         !ec && it != end && seen < kMaxFormatSample; it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        const std::string ext = Lower(it->path().extension().string());
        if (ext == ".png") {
            ++png;
        } else if (ext == ".ktx") {
            ++ktx;
        } else if (ext == ".dds") {
            ++dds;
        } else {
            continue;
        }
        ++seen;
    }
    const int kinds = (png > 0) + (ktx > 0) + (dds > 0);
    if (kinds == 0) {
        return Format::Unknown;
    }
    if (kinds > 1) {
        return Format::Mixed;
    }
    return png ? Format::Png : ktx ? Format::Ktx : Format::Dds;
}

std::optional<u64> CachedSize(const std::string& path) {
    std::lock_guard lock{size_mutex};
    const auto it = size_cache.find(Canonical(path));
    if (it == size_cache.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<u64> ComputeSize(const std::string& path, const std::atomic_bool* cancel) {
    u64 total = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end; it.increment(ec)) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            return std::nullopt;
        }
        std::error_code e2;
        if (it->is_regular_file(e2)) {
            const auto size = it->file_size(e2);
            if (!e2) {
                total += size;
            }
        }
    }
    std::lock_guard lock{size_mutex};
    size_cache[Canonical(path)] = total;
    return total;
}

void ForgetSizes() {
    std::lock_guard lock{size_mutex};
    size_cache.clear();
}

std::optional<Pack> ActivePack(const Paths& paths, u64 title_id) {
    const fs::path load = LoadPath(paths, title_id);
    std::error_code ec;
    const fs::file_status st = fs::symlink_status(load, ec);
    if (!fs::exists(st) || !IsDir(load)) {
        return std::nullopt; // missing, or a dangling link
    }
    if (fs::is_empty(load, ec)) {
        return std::nullopt;
    }
    const std::string tid = TitleFolder(title_id);
    if (fs::is_symlink(st)) {
        const fs::path target = fs::canonical(load, ec);
        if (ec) {
            return std::nullopt;
        }
        const bool managed = target.parent_path() == fs::weakly_canonical(ManagedDir(paths, title_id), ec);
        Pack pack = MakePack(target, tid, managed);
        pack.active = true;
        return pack;
    }
    Pack pack = MakePack(load, tid, false);
    pack.active = true;
    return pack;
}

std::vector<Pack> ListPacks(const Paths& paths, u64 title_id) {
    const std::string tid = TitleFolder(title_id);
    std::vector<Pack> packs;
    std::set<std::string> seen;
    const auto add = [&](Pack pack) {
        if (seen.insert(pack.id).second) {
            packs.push_back(std::move(pack));
        }
    };
    std::error_code ec;

    // managed
    const fs::path managed = ManagedDir(paths, title_id);
    for (fs::directory_iterator it(managed, ec), end; !ec && it != end; it.increment(ec)) {
        if (IsDir(it->path()) && !it->path().filename().string().starts_with('.')) {
            add(MakePack(it->path(), tid, true));
        }
    }
    // extra roots
    for (const std::string& root : paths.extra_roots) {
        for (const fs::path& dir : ExpandRoot(root)) {
            std::error_code e2;
            for (fs::directory_iterator it(dir, e2), end; !e2 && it != end; it.increment(e2)) {
                if (IsDir(it->path()) && IsHexTitleFolder(it->path().filename().string(), tid)) {
                    add(MakePack(it->path(), tid, false));
                }
            }
        }
    }
    // the active one (a real folder at the load place, or a link out of every library)
    const std::optional<Pack> active = ActivePack(paths, title_id);
    if (active) {
        add(*active);
    }
    // the load place itself is not a library entry when it is a link; when it is a real folder it
    // was added above under its own id.
    for (Pack& pack : packs) {
        pack.active = active && pack.id == active->id;
    }
    std::sort(packs.begin(), packs.end(), [](const Pack& a, const Pack& b) {
        const std::string la = Lower(a.name), lb = Lower(b.name);
        return la != lb ? la < lb : a.path < b.path;
    });
    return packs;
}

Result Activate(const Paths& paths, u64 title_id, const Pack& pack) {
    Result r;
    std::error_code ec;
    const fs::path load = LoadPath(paths, title_id);
    if (pack.path.empty() || !IsDir(pack.path)) {
        r.error = "The texture pack folder is missing: " + pack.path;
        return r;
    }
    const std::string target = Canonical(pack.path);
    // Already active?
    if (const auto active = ActivePack(paths, title_id); active && active->id == target) {
        r.ok = true;
        return r;
    }
    fs::create_directories(load.parent_path(), ec);
    if (ec) {
        r.error = "Could not create " + load.parent_path().generic_string() + ": " + ec.message();
        return r;
    }
    // The pack must not be the real folder we are about to move away
    Result clear = ClearLoadPlace(paths, title_id, load);
    if (!clear.ok) {
        return clear;
    }
    r.backed_up = clear.backed_up;
    // A moved pack changed place: re-resolve it
    std::string link_target = target;
    if (!clear.backed_up.empty() && Canonical(load) == target) {
        link_target = Canonical(clear.backed_up);
    }
    const fs::path tmp = load.parent_path() / ("." + TitleFolder(title_id) + ".tmp");
    fs::remove(tmp, ec);
    ec.clear();
    fs::create_directory_symlink(link_target, tmp, ec);
    if (ec) {
        r.error = "Could not create the link: " + ec.message();
        return r;
    }
    fs::rename(tmp, load, ec); // atomic: replaces an existing link
    if (ec) {
        std::error_code ignore;
        fs::remove(tmp, ignore);
        r.error = "Could not switch the pack: " + ec.message();
        return r;
    }
    r.ok = true;
    return r;
}

Result Deactivate(const Paths& paths, u64 title_id) {
    const fs::path load = LoadPath(paths, title_id);
    std::error_code ec;
    const fs::file_status st = fs::symlink_status(load, ec);
    if (!fs::exists(st)) {
        return {true, {}, {}};
    }
    if (fs::is_symlink(st)) {
        fs::remove(load, ec); // the link only, never what it points at
        if (ec) {
            return {false, "Could not remove the link: " + ec.message(), {}};
        }
        return {true, {}, {}};
    }
    return ClearLoadPlace(paths, title_id, load);
}

bool CustomTexturesEnabled() {
    return Settings::values.custom_textures.GetValue();
}
void SetCustomTexturesEnabled(bool enabled) {
    Settings::values.custom_textures.SetValue(enabled);
}
bool PreloadTexturesEnabled() {
    return Settings::values.preload_textures.GetValue();
}
void SetPreloadTexturesEnabled(bool enabled) {
    Settings::values.preload_textures.SetValue(enabled);
}
bool CustomTexturesIsPerGame() {
    return !Settings::values.custom_textures.UsingGlobal();
}
bool PreloadTexturesIsPerGame() {
    return !Settings::values.preload_textures.UsingGlobal();
}

} // namespace CustomTex
