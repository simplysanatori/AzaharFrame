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

} // namespace Common
