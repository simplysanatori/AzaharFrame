// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame harden: small state files (manifests, .azaharframe-*.json, pack.json, the crash-undo marker,
// saved settings) written so that a freeze and a reboot mid-write leave the old file or the new one, never
// an empty or half-written one: write a temporary file beside it, fsync it, rename it over the target,
// fsync the folder so the rename itself is on disk. Not for large files (one fsync per call).
#pragma once

#include <cerrno>
#include <cstddef>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#include <filesystem>

namespace Common {

#ifndef _WIN32
/// fsync of an open folder (makes a rename or a new name in it durable). False on failure.
inline bool SyncDirectory(const std::string& dir) {
    const int fd = ::open(dir.empty() ? "." : dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    const bool ok = ::fsync(fd) == 0;
    ::close(fd);
    return ok;
}

/// Writes all of `data` to `path` (the file is created, or emptied) and fsyncs it.
inline bool WriteAndSyncFile(const std::string& path, std::string_view data) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        return false;
    }
    std::size_t done = 0;
    while (done < data.size()) {
        const ssize_t n = ::write(fd, data.data() + done, data.size() - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            ::close(fd);
            return false;
        }
        done += static_cast<std::size_t>(n);
    }
    const bool ok = ::fsync(fd) == 0;
    return ::close(fd) == 0 && ok;
}
#else
inline bool SyncDirectory(const std::string&) {
    return true;
}
inline bool WriteAndSyncFile(const std::string& path, std::string_view data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.flush();
    return static_cast<bool>(out);
}
#endif

/// Replaces `path` with `data` atomically and durably: `path`.tmp is written and fsynced, renamed over `path`,
/// then the folder is fsynced. On failure the temporary file is removed and `path` is untouched. The folder
/// must exist.
inline bool WriteFileAtomic(const std::string& path, std::string_view data) {
    namespace fs = std::filesystem;
    const std::string tmp = path + ".tmp";
    std::error_code ec;
    if (!WriteAndSyncFile(tmp, data)) {
        fs::remove(tmp, ec);
        return false;
    }
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    const std::string dir = fs::path(path).parent_path().string();
    SyncDirectory(dir); // the file is in place either way; a failed folder sync is not an error
    return true;
}

/// AzaharFrame savemem: WriteFileAtomic for data that comes in pieces (a save state compressed as
/// it is written, with no copy of the whole file in memory): `path`.tmp is created at once, each
/// Write appends to it, and Commit fsyncs it, renames it over `path` and fsyncs the folder. Not
/// committed (a failed Write, an exception, no Commit), the temporary file is removed and `path`
/// is untouched. The folder must exist.
class AtomicFileWriter {
public:
    explicit AtomicFileWriter(std::string path_) : path(std::move(path_)), tmp(path + ".tmp") {
#ifndef _WIN32
        fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        ok = fd >= 0;
#else
        out.open(tmp, std::ios::binary | std::ios::trunc);
        ok = static_cast<bool>(out);
#endif
    }
    ~AtomicFileWriter() {
        if (!committed) {
            Close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
        }
    }
    AtomicFileWriter(const AtomicFileWriter&) = delete;
    AtomicFileWriter& operator=(const AtomicFileWriter&) = delete;

    /// Opened, and every Write so far succeeded.
    [[nodiscard]] bool Ok() const {
        return ok;
    }
    [[nodiscard]] std::size_t Written() const {
        return written;
    }
    /// Appends `data`; false (and every later call too) on failure.
    bool Write(std::string_view data) {
        if (!ok) {
            return false;
        }
#ifndef _WIN32
        std::size_t done = 0;
        while (done < data.size()) {
            const ssize_t n = ::write(fd, data.data() + done, data.size() - done);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                ok = false;
                return false;
            }
            done += static_cast<std::size_t>(n);
        }
#else
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        ok = static_cast<bool>(out);
        if (!ok) {
            return false;
        }
#endif
        written += data.size();
        return true;
    }
    /// Puts the file in place (see the class). False when anything failed: `path` is untouched.
    bool Commit() {
        if (!ok || committed) {
            return ok && committed;
        }
#ifndef _WIN32
        const bool synced = ::fsync(fd) == 0;
        const bool closed = Close();
        if (!synced || !closed) {
            ok = false;
            return false;
        }
#else
        out.flush();
        ok = static_cast<bool>(out);
        Close();
        if (!ok) {
            return false;
        }
#endif
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            ok = false;
            return false;
        }
        committed = true;
        SyncDirectory(std::filesystem::path(path).parent_path().string());
        return true;
    }

private:
    bool Close() {
#ifndef _WIN32
        if (fd < 0) {
            return true;
        }
        const bool closed = ::close(fd) == 0;
        fd = -1;
        return closed;
#else
        if (out.is_open()) {
            out.close();
        }
        return true;
#endif
    }

    std::string path;
    std::string tmp;
#ifndef _WIN32
    int fd = -1;
#else
    std::ofstream out;
#endif
    bool ok = false;
    bool committed = false;
    std::size_t written = 0;
};

} // namespace Common
