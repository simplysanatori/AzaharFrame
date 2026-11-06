// Copyright 2026 CitraFrame contributors
// Licensed under GPLv3 or any later version

// The application library behind the in-VR picker: finds 3DS files in a list of
// folders, reads each one's title and 48x48 icon, caches the decoded icons on
// disk and keeps a sorted list. Scanning runs on a worker thread; the caller
// only polls Generation() and takes snapshots.
//
// This header (and library.cpp) has no emulator-core dependencies: the code
// that actually opens a file is a MetadataReader callback. The Loader-backed
// reader is in library_loader.h.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace frame_xr::picker {

/// Depth Azahar's game list uses for a folder with "Scan Subfolders" ticked.
inline constexpr int kDeepScanDepth = 256;

/// One folder to scan, as the Qt side has it (UISettings::GameDir).
struct GameDir {
    std::string path;
    /// "Scan Subfolders".
    bool recursive = false;
    /// Overrides the depth derived from `recursive` when >= 0
    /// (Azahar scans its INSTALLED and SYSTEM entries 2 levels deep).
    int max_depth = -1;

    int Depth() const {
        return max_depth >= 0 ? max_depth : (recursive ? kDeepScanDepth : 0);
    }
};

inline constexpr int kIconSize = 48;

/// What a MetadataReader finds out about one file.
struct GameMeta {
    std::string title; // UTF-8; may be empty, the file name is used then
    int icon_w = 0;
    int icon_h = 0;
    std::vector<uint8_t> icon_rgba; // icon_w * icon_h * 4, straight alpha; empty = no icon
};

struct GameEntry {
    std::string path;
    std::string title;
    int icon_w = 0;
    int icon_h = 0;
    std::vector<uint8_t> icon_rgba;
};

/// Opens `path` and returns its metadata, or nullopt when it is not a bootable
/// application (the file is then left out of the list). Called on the worker thread.
using MetadataReader = std::function<std::optional<GameMeta>(const std::string& path)>;

/// RGB565 pixels (as SMDH::GetIcon returns them) to straight RGBA8, alpha 255.
std::vector<uint8_t> Rgb565ToRgba(const std::vector<uint16_t>& pixels);

/// File extensions Azahar's game list accepts (GameList::supported_file_extensions).
const std::vector<std::string>& SupportedExtensions();
/// Case-insensitive extension check on a path.
bool HasSupportedExtension(const std::string& path);

/// All files in `dirs` with a supported extension, each once, in discovery order.
/// Folders that do not exist are skipped. `should_cancel` is polled between folder
/// entries.
std::vector<std::string> FindGameFiles(const std::vector<GameDir>& dirs,
                                       const std::function<bool()>& should_cancel = {});

/// Sorts by title, case-insensitively, then by path.
void SortEntries(std::vector<GameEntry>& entries);

/// File name without folder and extension, used when a title is missing.
std::string FileStem(const std::string& path);

/// On-disk cache of decoded titles and icons, one small file per application,
/// keyed by path, modification time and size.
class IconCache {
public:
    /// An empty `dir` disables the cache.
    explicit IconCache(std::string dir);

    std::optional<GameMeta> Load(const std::string& path, int64_t mtime, uint64_t size) const;
    void Store(const std::string& path, int64_t mtime, uint64_t size, const GameMeta& meta) const;

    /// File name (without folder) the entry for these keys is stored under.
    static std::string KeyName(const std::string& path, int64_t mtime, uint64_t size);

private:
    std::string dir;
};

/// "~/.cache/citraframe/icons" (honours XDG_CACHE_HOME), or "" if no home is known.
std::string DefaultIconCacheDir();

class Library {
public:
    /// `cache_dir` empty disables the icon cache.
    Library(MetadataReader reader, std::string cache_dir);
    ~Library();
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    /// Replaces the folder list. Does not scan; call Rescan() afterwards.
    void SetDirs(std::vector<GameDir> dirs);
    bool HasDirs() const;

    /// Starts a fresh scan on the worker thread, abandoning one in progress.
    /// Never blocks. Do not call while a game is running (the file reads compete
    /// with emulation); the picker is hidden then.
    void Rescan();

    bool IsScanning() const {
        return scanning.load();
    }
    /// Increases whenever the snapshot changes or scanning starts or stops.
    uint64_t Generation() const {
        return generation.load();
    }
    /// The current sorted list; cheap (shared, immutable).
    std::shared_ptr<const std::vector<GameEntry>> Snapshot() const;

    /// Blocks until the worker is idle. For tests and tools.
    void WaitIdle();

private:
    void Worker();
    void Publish(std::vector<GameEntry> entries);

    MetadataReader reader;
    IconCache cache;

    mutable std::mutex mutex;
    std::condition_variable cv;
    std::condition_variable idle_cv;
    std::vector<GameDir> dirs;
    std::shared_ptr<const std::vector<GameEntry>> snapshot;
    bool pending = false;
    bool busy = false;
    bool quit = false;
    std::atomic<uint64_t> epoch{0};
    std::atomic<uint64_t> generation{0};
    std::atomic<bool> scanning{false};
    std::thread thread;
};

} // namespace frame_xr::picker
