// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame zipinstall: see zip_install.h.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <system_error>
#include <json.hpp>
#include <fmt/format.h>
#include "common/atomic_file.h"
#include "core/custom_tex/download.h"
#include "core/custom_tex/packs.h"
#include "core/mods/library.h"
#include "core/mods/zip_install.h"

namespace Mods::Zip {

namespace fs = std::filesystem;
namespace Dl = CustomTex::Download;

namespace {

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

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool IsJunk(const std::vector<std::string>& parts) {
    return std::any_of(parts.begin(), parts.end(), [](const std::string& p) {
        return p == "__MACOSX" || p.rfind("._", 0) == 0;
    });
}

bool IsModTop(const std::string& dir) {
    return dir == "romfs" || dir == "romfs_ext" || dir == "exefs";
}

bool IsModFile(const std::string& base) {
    return base == "code.ips" || base == "code.bps" || base == "code.bin" || base == "exheader.bin";
}

/// One file's part in the archive: texture content, mod content or neither, and the index of the
/// title ID folder it sits in (-1 none).
struct FileRole {
    Kind kind = Kind::Unknown;
    int tid_index = -1;
};

FileRole RoleOf(const std::vector<std::string>& parts) {
    FileRole r;
    if (parts.empty() || IsJunk(parts)) {
        return r;
    }
    const std::size_t dirs = parts.size() - 1;
    std::size_t top = dirs; // the first romfs / exefs folder, or the file itself
    for (std::size_t i = 0; i < dirs; i++) {
        if (IsModTop(parts[i])) {
            top = i;
            break;
        }
    }
    const std::string& base = parts.back();
    if (top < dirs || IsModFile(base)) {
        r.kind = Kind::Mod;
    } else if (Dl::IsTextureFileName(base) || Lower(base) == "pack.json") {
        r.kind = Kind::TexturePack;
    } else {
        return r;
    }
    for (std::size_t i = 0; i < top; i++) {
        if (ParseTitleFolder(parts[i]) != 0) {
            r.tid_index = static_cast<int>(i); // the closest one wins
        }
    }
    return r;
}

std::string JoinParts(const std::vector<std::string>& parts, std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count && i < parts.size(); i++) {
        out += (out.empty() ? "" : "/") + parts[i];
    }
    return out;
}

/// The files under `prefix` ("" = all), relative to it.
std::vector<std::string> Under(const std::vector<std::string>& files, const std::string& prefix) {
    std::vector<std::string> out;
    for (const auto& f : files) {
        if (prefix.empty()) {
            out.push_back(f);
        } else if (f.size() > prefix.size() + 1 && f.compare(0, prefix.size(), prefix) == 0 &&
                   f[prefix.size()] == '/') {
            out.push_back(f.substr(prefix.size() + 1));
        }
    }
    return out;
}

std::string ReadHead(const fs::path& p, std::size_t n) {
    std::string head(n, '\0');
    std::ifstream in(p, std::ios::binary);
    in.read(head.data(), static_cast<std::streamsize>(n));
    head.resize(static_cast<std::size_t>(in ? n : in.gcount()));
    return head;
}

} // namespace

u64 ParseTitleFolder(std::string_view part) {
    if (part.size() != 16 || part.substr(0, 4) != "0004") {
        return 0;
    }
    u64 v = 0;
    for (const char c : part) {
        const int d = std::isdigit(static_cast<unsigned char>(c)) ? c - '0'
                      : (c >= 'a' && c <= 'f')                    ? c - 'a' + 10
                      : (c >= 'A' && c <= 'F')                    ? c - 'A' + 10
                                                                  : -1;
        if (d < 0) {
            return 0;
        }
        v = (v << 4) | static_cast<u64>(d);
    }
    return v;
}

Detected Detect(const std::vector<std::string>& files) {
    bool tex = false, mod = false, nested = false;
    std::set<u64> tex_tids, mod_tids;
    for (const auto& f : files) {
        const auto parts = Split(f);
        const FileRole r = RoleOf(parts);
        if (r.kind == Kind::Unknown) {
            if (!parts.empty() && !IsJunk(parts)) {
                const std::string l = Lower(parts.back());
                for (const std::string_view ext : {".zip", ".7z", ".rar"}) {
                    nested |= l.size() > ext.size() && l.compare(l.size() - ext.size(), ext.size(), ext) == 0;
                }
            }
            continue;
        }
        const u64 tid = r.tid_index >= 0 ? ParseTitleFolder(parts[static_cast<std::size_t>(r.tid_index)]) : 0;
        if (r.kind == Kind::TexturePack) {
            tex = true;
            if (tid != 0) {
                tex_tids.insert(tid);
            }
        } else {
            mod = true;
            if (tid != 0) {
                mod_tids.insert(tid);
            }
        }
    }
    Detected d;
    if (tex && mod) {
        d.reason = kBoth;
    } else if (tex) {
        d.kind = Kind::TexturePack;
        d.title_ids.assign(tex_tids.begin(), tex_tids.end());
    } else if (mod) {
        d.kind = Kind::Mod;
        d.title_ids.assign(mod_tids.begin(), mod_tids.end());
    } else {
        const bool empty = std::all_of(files.begin(), files.end(),
                                       [](const std::string& f) { return IsJunk(Split(f)); });
        d.reason = empty ? kEmpty : nested ? kNested : kNotPackOrMod;
    }
    return d;
}

TitleChoice ResolveTitle(const std::vector<u64>& found, u64 current) {
    TitleChoice c;
    if (found.empty()) {
        c.title_id = current; // 0: the player picks the game
    } else if (current != 0 && std::find(found.begin(), found.end(), current) != found.end()) {
        c.title_id = current;
    } else if (found.size() == 1) {
        c.title_id = found.front();
    } else {
        c.candidates = found;
    }
    return c;
}

std::string ContentRoot(const std::vector<std::string>& files, const Detected& detected,
                        u64 title_id) {
    if (detected.title_ids.empty()) {
        return {};
    }
    for (const auto& f : files) {
        const auto parts = Split(f);
        const FileRole r = RoleOf(parts);
        if (r.kind != detected.kind || r.tid_index < 0) {
            continue;
        }
        const auto i = static_cast<std::size_t>(r.tid_index);
        if (ParseTitleFolder(parts[i]) == title_id) {
            return JoinParts(parts, i + 1);
        }
    }
    return {};
}

std::string NameFromArchive(const std::string& archive_path) {
    std::string name = fs::path(archive_path).filename().string();
    for (const char* ext : {".zip", ".7z", ".rar"}) {
        const std::string e = ext;
        if (name.size() > e.size() && Lower(name.substr(name.size() - e.size())) == e) {
            name.resize(name.size() - e.size());
            break;
        }
    }
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) {
        name.pop_back();
    }
    return name.empty() ? std::string("Mod") : name;
}

std::string ModId(const std::string& name) {
    std::string slug;
    for (const char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u) && u < 0x80) {
            slug += static_cast<char>(std::tolower(u));
        } else if (!slug.empty() && slug.back() != '-') {
            slug += '-';
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    if (slug.size() > 60) {
        slug.resize(60);
        while (!slug.empty() && slug.back() == '-') {
            slug.pop_back();
        }
    }
    return std::string(kZipPrefix) + (slug.empty() ? "mod" : slug);
}

std::string Open(const std::string& archive_path, const std::string& load_dir, Job& job,
                 const std::atomic_bool* cancel, const std::string& tool_override,
                 std::optional<u64> free_space) {
    job = {};
    job.archive = archive_path;
    job.name = NameFromArchive(archive_path);
    const fs::path base = fs::path(load_dir) / "zip-install";
    std::error_code ec;
    static std::atomic_flag swept = ATOMIC_FLAG_INIT;
    if (!swept.test_and_set()) {
        fs::remove_all(base, ec); // what an earlier run of the program left
    }
    static std::atomic<unsigned> serial{0};
    if (!fs::is_regular_file(archive_path, ec)) {
        return "Cannot read the downloaded file";
    }
    const u64 size = fs::file_size(archive_path, ec);
    constexpr u64 kMargin = 256ull * 1024 * 1024;
    u64 max_total = 64ull << 30;
    if (!free_space) {
        free_space = Dl::FreeSpace(load_dir);
    }
    if (free_space) {
        // the unpacked copy and room to spare; an archive that unpacks to more than the room left
        // stops when it gets there
        if (std::string e = Dl::FreeSpaceError(Dl::RequiredSpace(size, size), *free_space); !e.empty()) {
            return e;
        }
        max_total = std::min(max_total, *free_space - kMargin);
    }
    {
        // An empty zip is only its end record ("PK\5\6"): not an archive to the readers.
        std::ifstream in(archive_path, std::ios::binary);
        std::string head(4, '\0');
        in.read(head.data(), 4);
        if (in && head == std::string("PK\x05\x06", 4)) {
            job.detected = Detect({});
            return {};
        }
    }
    // A folder of its own: an earlier job's folder may still be being removed.
    job.work = (base / fmt::format("work-{}", serial.fetch_add(1))).string();
    std::string error =
        Dl::UnpackArchive(archive_path, job.work, cancel, &job.root, tool_override, max_total);
    if (!error.empty()) {
        Close(job);
        if (free_space && error.find("too large") != std::string::npos &&
            max_total < (64ull << 30)) {
            return "Not enough free space to unpack";
        }
        return error;
    }
    job.files = Dl::ListFiles(job.root);
    job.detected = Detect(job.files);
    return {};
}

void Close(Job& job) {
    if (!job.work.empty()) {
        std::error_code ec;
        fs::remove_all(job.work, ec);
        fs::remove(fs::path(job.work).parent_path(), ec); // zip-install/, when empty
    }
    job.work.clear();
    job.root.clear();
    job.files.clear();
}

std::string ShortError(std::string_view e) {
    const auto has = [&](std::string_view needle) { return e.find(needle) != std::string_view::npos; };
    if (e == kNotPackOrMod || e == kBoth || e == kEmpty) {
        return std::string(e);
    }
    if (has("not a zip, 7z or rar")) {
        return "Not a Zip or 7z File";
    }
    if (has("free space") || has("Not enough")) {
        return "Not Enough Space";
    }
    if (has("unsafe") || has("links or special")) {
        return "Unsafe Archive";
    }
    if (has("too large")) {
        return "Too Large";
    }
    if (e == kNested || e == kNoGame) {
        return std::string(e);
    }
    if (has("No game")) {
        return kNoGame;
    }
    if (has("Cannot read")) {
        return "Can't Read the File";
    }
    if (has("bsdtar") || has("not supported")) {
        return "Unsupported Archive";
    }
    if (has("No textures")) {
        return "No Textures Found";
    }
    if (has("Nothing in the archive") || has("code patch") || has("damaged") || has("unpacked") ||
        has("too many files") || has("Zip") || has("zip")) {
        return "Bad Archive";
    }
    if (has("Cannot")) {
        return "Disk Error";
    }
    if (has("Another install")) {
        return "Busy";
    }
    if (has("took too long")) {
        return "Timed Out";
    }
    return "Error";
}

Outcome InstallTextures(Job& job, u64 title_id, const std::string& load_dir, bool replace) {
    Outcome out;
    const auto error = [&out](std::string why) {
        out.result = Outcome::Result::Error;
        out.error = std::move(why);
        return out;
    };
    if (title_id == 0) {
        return error("No game chosen");
    }
    if (job.root.empty() || job.detected.kind != Kind::TexturePack) {
        return error("Nothing to install");
    }
    const std::string prefix = ContentRoot(job.files, job.detected, title_id);
    const auto folders = Dl::FindPackFolders(Under(job.files, prefix), Dl::Layout::Single);
    if (folders.empty()) {
        return error("No textures found in the archive");
    }
    fs::path src = job.root;
    if (!prefix.empty()) {
        src /= prefix;
    }
    if (!folders.front().dir.empty()) {
        src /= folders.front().dir;
    }
    std::string name = job.name;
    try {
        std::ifstream in(src / "pack.json");
        if (in) {
            const auto j = nlohmann::json::parse(in);
            if (j.contains("name") && j["name"].is_string() && !j["name"].get<std::string>().empty()) {
                name = j["name"].get<std::string>();
            }
        }
    } catch (const std::exception&) {
        // a pack.json that does not parse: the archive's name
    }
    std::string folder = Dl::SanitizeFolderName(name);
    if (folder.empty()) {
        folder = "Texture Pack";
    }
    const fs::path root = fs::path(load_dir) / "texture-packs" / CustomTex::TitleFolder(title_id);
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) {
        return error("Cannot make the pack library folder");
    }
    fs::path dst = root / folder;
    if (fs::exists(dst, ec) || fs::is_symlink(dst, ec)) {
        if (Dl::MarkerId(dst.string()) && !fs::is_symlink(dst, ec)) {
            // one the menu installed (Download Packs or a zip): only after a yes
            if (!replace) {
                out.result = Outcome::Result::Conflict;
                out.conflict_name = folder;
                out.error = "Conflicts with " + folder;
                return out;
            }
            fs::remove_all(dst, ec);
            if (ec) {
                return error("Cannot remove the old pack");
            }
        } else {
            // the player's own folder: never touched
            for (int n = 2; (fs::exists(dst, ec) || fs::is_symlink(dst, ec)) && n < 100; ++n) {
                dst = root / fmt::format("{} ({})", folder, n);
            }
        }
    }
    fs::rename(src, dst, ec);
    if (ec) {
        return error("Cannot move the pack into the library");
    }
    Dl::WriteMarker(dst.string(), "zip:" + dst.filename().string(), "zip");
    if (!fs::exists(dst / "pack.json", ec)) {
        nlohmann::json pj;
        pj["name"] = name;
        Common::WriteFileAtomic((dst / "pack.json").string(), pj.dump(2)); // AzaharFrame harden
    }
    out.installed = dst.filename().string();
    return out;
}

Outcome InstallMod(Job& job, u64 title_id, Library& library, bool replace) {
    Outcome out;
    const auto error = [&out](std::string why) {
        out.result = Outcome::Result::Error;
        out.error = std::move(why);
        return out;
    };
    if (title_id == 0) {
        return error("No game chosen");
    }
    if (job.root.empty() || job.detected.kind != Kind::Mod) {
        return error("Nothing to install");
    }
    const std::string prefix = ContentRoot(job.files, job.detected, title_id);
    fs::path base = job.root;
    if (!prefix.empty()) {
        base /= prefix;
    }
    std::vector<LocalFile> files;
    for (const auto& rel : Under(job.files, prefix)) {
        const auto parts = Split(rel);
        if (IsJunk(parts)) {
            continue;
        }
        const std::string target = MapPath(rel, {}, title_id);
        if (target.empty()) {
            continue;
        }
        const fs::path src = base / rel;
        const std::string name = parts.back();
        if ((name == "code.ips" && ReadHead(src, 5) != "PATCH") ||
            (name == "code.bps" && ReadHead(src, 4) != "BPS1")) {
            return error("Not a valid code patch");
        }
        files.push_back({src.string(), target});
    }
    if (files.empty()) {
        return error("Nothing in the archive fits this game");
    }
    const std::string id = ModId(job.name);
    const InstallOutcome o = library.InstallLocal(title_id, id, job.name, files, replace);
    switch (o.result) {
    case InstallResult::Ok:
    case InstallResult::AlreadyInstalled:
        out.installed = id;
        return out;
    case InstallResult::Conflict:
        out.result = o.conflict_unmanaged ? Outcome::Result::Blocked : Outcome::Result::Conflict;
        out.error = o.error;
        out.installed = id;
        out.conflict_id = o.conflict_id;
        out.conflict_name = o.conflict_name;
        return out;
    default:
        return error(o.error);
    }
}

} // namespace Mods::Zip
