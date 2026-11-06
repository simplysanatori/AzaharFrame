// Copyright 2026 CitraFrame contributors
// Licensed under GPLv3 or any later version

#include "frame_xr/picker/library.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <system_error>

namespace frame_xr::picker {

namespace fs = std::filesystem;

namespace {

std::string Lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

uint64_t Fnv1a(const void* data, size_t n, uint64_t h = 14695981039346656037ull) {
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

constexpr char kCacheMagic[4] = {'C', 'F', 'I', '1'};
constexpr uint32_t kMaxTitleBytes = 4096;
constexpr uint32_t kMaxIconDim = 256;

void PutU32(std::string& out, uint32_t v) {
    for (int i = 0; i < 4; i++)
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

bool GetU32(const std::string& in, size_t& pos, uint32_t& v) {
    if (pos + 4 > in.size())
        return false;
    v = 0;
    for (int i = 0; i < 4; i++)
        v |= static_cast<uint32_t>(static_cast<uint8_t>(in[pos + i])) << (8 * i);
    pos += 4;
    return true;
}

int64_t MTimeSeconds(const fs::path& p) {
    std::error_code ec;
    const auto t = fs::last_write_time(p, ec);
    if (ec)
        return 0;
    return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
}

} // namespace

std::vector<uint8_t> Rgb565ToRgba(const std::vector<uint16_t>& pixels) {
    std::vector<uint8_t> out(pixels.size() * 4);
    for (size_t i = 0; i < pixels.size(); i++) {
        const unsigned p = pixels[i];
        const unsigned r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
        out[i * 4 + 0] = static_cast<uint8_t>((r << 3) | (r >> 2));
        out[i * 4 + 1] = static_cast<uint8_t>((g << 2) | (g >> 4));
        out[i * 4 + 2] = static_cast<uint8_t>((b << 3) | (b >> 2));
        out[i * 4 + 3] = 255;
    }
    return out;
}

const std::vector<std::string>& SupportedExtensions() {
    // Keep in sync with GameList::supported_file_extensions (citra_qt/game_list.cpp).
    static const std::vector<std::string> extensions = {
        "3dsx", "elf", "axf", "cci", "cxi", "app", "z3dsx", "zcci", "zcxi", "bcci", "3ds",
    };
    return extensions;
}

bool HasSupportedExtension(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return false;
    }
    const std::string ext = Lower(path.substr(dot + 1));
    const auto& list = SupportedExtensions();
    return std::find(list.begin(), list.end(), ext) != list.end();
}

std::string FileStem(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        name.resize(dot);
    }
    return name;
}

std::vector<std::string> FindGameFiles(const std::vector<GameDir>& dirs,
                                       const std::function<bool()>& should_cancel) {
    std::vector<std::string> found;
    std::set<std::string> seen;

    const auto add = [&](const fs::path& p) {
        std::string s = p.string();
        if (HasSupportedExtension(s) && seen.insert(s).second) {
            found.push_back(std::move(s));
        }
    };

    for (const GameDir& dir : dirs) {
        std::error_code ec;
        const fs::path root(dir.path);
        if (dir.path.empty() || !fs::is_directory(root, ec)) {
            continue;
        }
        const int max_depth = dir.Depth();
        // Plain iterative walk so depth is bounded; symlinked folders are not followed
        // (a symlink to a folder is not is_directory(symlink_status)).
        std::vector<std::pair<fs::path, int>> stack;
        stack.emplace_back(root, 0);
        while (!stack.empty()) {
            if (should_cancel && should_cancel()) {
                return found;
            }
            auto [path, depth] = std::move(stack.back());
            stack.pop_back();
            std::vector<fs::path> subdirs;
            std::vector<fs::path> files;
            fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
            if (ec)
                continue;
            for (fs::directory_iterator end; it != end; it.increment(ec)) {
                if (ec)
                    break;
                std::error_code e2;
                const fs::file_status st = it->symlink_status(e2);
                if (e2)
                    continue;
                if (fs::is_directory(st)) {
                    if (depth < max_depth)
                        subdirs.push_back(it->path());
                } else {
                    // Regular files and symlinks to files.
                    files.push_back(it->path());
                }
            }
            std::sort(files.begin(), files.end());
            for (const fs::path& f : files)
                add(f);
            // Reverse so the stack pops them in name order.
            std::sort(subdirs.begin(), subdirs.end(), std::greater<>());
            for (const fs::path& d : subdirs)
                stack.emplace_back(d, depth + 1);
        }
    }
    return found;
}

void SortEntries(std::vector<GameEntry>& entries) {
    std::stable_sort(entries.begin(), entries.end(), [](const GameEntry& a, const GameEntry& b) {
        const std::string la = Lower(a.title), lb = Lower(b.title);
        if (la != lb)
            return la < lb;
        return a.path < b.path;
    });
}

// ---------------------------------------------------------------------------------------

IconCache::IconCache(std::string dir_) : dir(std::move(dir_)) {}

std::string IconCache::KeyName(const std::string& path, int64_t mtime, uint64_t size) {
    uint64_t h = Fnv1a(path.data(), path.size());
    h = Fnv1a(&mtime, sizeof(mtime), h);
    h = Fnv1a(&size, sizeof(size), h);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx.cfi", static_cast<unsigned long long>(h));
    return buf;
}

// File: "CFI1", u32 path length, path, u32 title length, title, u32 w, u32 h, w*h*4 RGBA.
// The stored path guards against hash collisions.
std::optional<GameMeta> IconCache::Load(const std::string& path, int64_t mtime,
                                        uint64_t size) const {
    if (dir.empty())
        return std::nullopt;
    std::FILE* f = std::fopen((dir + "/" + KeyName(path, mtime, size)).c_str(), "rb");
    if (!f)
        return std::nullopt;
    std::string data;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        data.append(buf, n);
        if (data.size() > (1u << 22))
            break;
    }
    std::fclose(f);

    if (data.size() < 4 || std::memcmp(data.data(), kCacheMagic, 4) != 0)
        return std::nullopt;
    size_t pos = 4;
    uint32_t plen = 0, tlen = 0, w = 0, h = 0;
    if (!GetU32(data, pos, plen) || plen > kMaxTitleBytes || pos + plen > data.size())
        return std::nullopt;
    if (data.compare(pos, plen, path) != 0 || plen != path.size())
        return std::nullopt;
    pos += plen;
    if (!GetU32(data, pos, tlen) || tlen > kMaxTitleBytes || pos + tlen > data.size())
        return std::nullopt;
    GameMeta meta;
    meta.title = data.substr(pos, tlen);
    pos += tlen;
    if (!GetU32(data, pos, w) || !GetU32(data, pos, h) || w > kMaxIconDim || h > kMaxIconDim)
        return std::nullopt;
    const size_t bytes = static_cast<size_t>(w) * h * 4;
    if (pos + bytes != data.size())
        return std::nullopt;
    meta.icon_w = static_cast<int>(w);
    meta.icon_h = static_cast<int>(h);
    meta.icon_rgba.assign(data.begin() + pos, data.end());
    return meta;
}

void IconCache::Store(const std::string& path, int64_t mtime, uint64_t size,
                      const GameMeta& meta) const {
    if (dir.empty())
        return;
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec)
        return;
    std::string data(kCacheMagic, 4);
    PutU32(data, static_cast<uint32_t>(path.size()));
    data += path;
    const std::string title = meta.title.substr(0, kMaxTitleBytes);
    PutU32(data, static_cast<uint32_t>(title.size()));
    data += title;
    const bool has_icon = !meta.icon_rgba.empty() && meta.icon_w > 0 && meta.icon_h > 0 &&
                          meta.icon_rgba.size() == static_cast<size_t>(meta.icon_w) * meta.icon_h * 4;
    PutU32(data, has_icon ? static_cast<uint32_t>(meta.icon_w) : 0);
    PutU32(data, has_icon ? static_cast<uint32_t>(meta.icon_h) : 0);
    if (has_icon) {
        data.append(reinterpret_cast<const char*>(meta.icon_rgba.data()), meta.icon_rgba.size());
    }

    // Write to a temporary name and rename, so a half-written file is never read.
    const std::string final_path = dir + "/" + KeyName(path, mtime, size);
    const std::string tmp_path = final_path + ".tmp";
    std::FILE* f = std::fopen(tmp_path.c_str(), "wb");
    if (!f)
        return;
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);
    if (!ok) {
        std::remove(tmp_path.c_str());
        return;
    }
    fs::rename(tmp_path, final_path, ec);
    if (ec)
        std::remove(tmp_path.c_str());
}

std::string DefaultIconCacheDir() {
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg) {
        return std::string(xdg) + "/citraframe/icons";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::string(home) + "/.cache/citraframe/icons";
    }
    return {};
}

// ---------------------------------------------------------------------------------------

Library::Library(MetadataReader reader_, std::string cache_dir)
    : reader(std::move(reader_)), cache(std::move(cache_dir)),
      snapshot(std::make_shared<const std::vector<GameEntry>>()) {
    thread = std::thread([this] { Worker(); });
}

Library::~Library() {
    {
        std::lock_guard lock(mutex);
        quit = true;
        epoch++;
    }
    cv.notify_all();
    thread.join();
}

void Library::SetDirs(std::vector<GameDir> new_dirs) {
    std::lock_guard lock(mutex);
    dirs = std::move(new_dirs);
}

bool Library::HasDirs() const {
    std::lock_guard lock(mutex);
    return std::any_of(dirs.begin(), dirs.end(), [](const GameDir& d) { return !d.path.empty(); });
}

void Library::Rescan() {
    {
        std::lock_guard lock(mutex);
        pending = true;
        epoch++;
        if (!scanning.exchange(true)) {
            generation++;
        }
    }
    cv.notify_all();
}

std::shared_ptr<const std::vector<GameEntry>> Library::Snapshot() const {
    std::lock_guard lock(mutex);
    return snapshot;
}

void Library::WaitIdle() {
    std::unique_lock lock(mutex);
    idle_cv.wait(lock, [this] { return (!pending && !busy) || quit; });
}

void Library::Publish(std::vector<GameEntry> entries) {
    SortEntries(entries);
    auto shared = std::make_shared<const std::vector<GameEntry>>(std::move(entries));
    std::lock_guard lock(mutex);
    snapshot = std::move(shared);
    generation++;
}

void Library::Worker() {
    for (;;) {
        std::vector<GameDir> scan_dirs;
        uint64_t my_epoch;
        {
            std::unique_lock lock(mutex);
            cv.wait(lock, [this] { return pending || quit; });
            if (quit)
                return;
            pending = false;
            busy = true;
            scan_dirs = dirs;
            my_epoch = epoch.load();
        }

        const auto cancelled = [&] { return epoch.load() != my_epoch; };

        std::vector<GameEntry> entries;
        const std::vector<std::string> files = FindGameFiles(scan_dirs, cancelled);
        auto last_publish = std::chrono::steady_clock::now();
        for (const std::string& path : files) {
            if (cancelled())
                break;
            std::error_code ec;
            const int64_t mtime = MTimeSeconds(path);
            const uint64_t size = fs::file_size(path, ec);

            std::optional<GameMeta> meta = cache.Load(path, mtime, ec ? 0 : size);
            if (!meta) {
                meta = reader ? reader(path) : std::nullopt;
                if (meta) {
                    cache.Store(path, mtime, ec ? 0 : size, *meta);
                }
            }
            if (!meta)
                continue;

            GameEntry entry;
            entry.path = path;
            entry.title = meta->title.empty() ? FileStem(path) : meta->title;
            entry.icon_w = meta->icon_w;
            entry.icon_h = meta->icon_h;
            entry.icon_rgba = std::move(meta->icon_rgba);
            entries.push_back(std::move(entry));

            // Show a partial list every quarter second on big libraries.
            const auto now = std::chrono::steady_clock::now();
            if (now - last_publish > std::chrono::milliseconds(250)) {
                last_publish = now;
                if (!cancelled())
                    Publish(entries);
            }
        }

        const bool was_cancelled = cancelled();
        if (!was_cancelled) {
            Publish(std::move(entries));
        }
        {
            std::lock_guard lock(mutex);
            busy = false;
            // A newer Rescan() keeps `scanning` on.
            if (!pending && !quit) {
                scanning = false;
                generation++;
            }
        }
        idle_cv.notify_all();
    }
}

} // namespace frame_xr::picker
