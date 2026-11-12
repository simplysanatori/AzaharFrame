// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emerald: what the homebrew packages download with, and their hashes. Blocking: call
// the network functions off the GUI thread. HTTPS with certificate checks against the system's
// store (httplib, as core/cheats/online.cpp), redirects followed only to GitHub's hosts.

#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include "common/common_types.h"

namespace Homebrew {

struct Reply {
    int status = 0; ///< 0: no reply
    std::string body;
    std::string error; ///< "" on success
};

/// A small reply (release information, a manifest), at most `max_body` bytes.
Reply GetText(const std::string& url, std::size_t max_body);

/// A file, written to dest + ".part" and renamed when whole. `progress(done, total)` (total 0
/// when the host does not say) returns false to stop. "" on success, else why. At most `max_bytes`
/// (by Content-Length, and again as the bytes arrive); callers that know the asset's size pass it
/// plus a margin.
std::string Download(const std::string& url, const std::string& dest,
                     const std::function<bool(u64, u64)>& progress,
                     u64 max_bytes = 128ULL * 1024 * 1024);

/// Whether a host ("https://github.com") may be fetched from.
bool HostAllowed(std::string_view host);

/// Lower-case hex digests ("" when the file cannot be read).
std::string Sha1Hex(std::span<const u8> data);
std::string Sha256Hex(std::string_view data);
std::string FileSha256(const std::string& path);

} // namespace Homebrew
