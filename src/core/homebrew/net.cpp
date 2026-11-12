// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emerald: see net.h.

#include "core/homebrew/net.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <cryptopp/sha.h>
#include <fmt/format.h>
#include <httplib.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/request_path.h"

namespace Homebrew {

namespace {

constexpr std::string_view kApiHost = "https://api.github.com";

template <typename Hash>
std::string HexDigest(Hash& hash) {
    std::array<CryptoPP::byte, Hash::DIGESTSIZE> digest{};
    hash.Final(digest.data());
    std::string out;
    out.reserve(digest.size() * 2);
    for (const auto b : digest) {
        out += fmt::format("{:02x}", b);
    }
    return out;
}

const char* CaBundle() {
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

bool SplitUrl(const std::string& url, std::string& host, std::string& path) {
    constexpr std::string_view scheme = "https://";
    if (url.rfind(scheme, 0) != 0) {
        return false;
    }
    const auto slash = url.find('/', scheme.size());
    host = url.substr(0, slash);
    path = slash == std::string::npos ? "/" : url.substr(slash);
    return host.size() > scheme.size() && host.find('@') == std::string::npos &&
           host.find(':', scheme.size()) == std::string::npos;
}

} // namespace

bool HostAllowed(std::string_view host) {
    static constexpr std::array<std::string_view, 4> allowed{
        "https://api.github.com", "https://github.com", "https://objects.githubusercontent.com",
        "https://release-assets.githubusercontent.com"};
    return std::find(allowed.begin(), allowed.end(), host) != allowed.end();
}

bool PinnedHostAllowed(std::string_view host) {
    static constexpr std::array<std::string_view, 2> allowed{"https://raw.githubusercontent.com",
                                                             "https://cdn.jsdelivr.net"};
    return std::find(allowed.begin(), allowed.end(), host) != allowed.end();
}

namespace {

/// "sp,sv,sig" for "?sp=r&sv=1&sig=x": the names only.
std::string QueryNames(const std::string& path) {
    const auto q = path.find('?');
    if (q == std::string::npos) {
        return "none";
    }
    std::string names;
    std::size_t at = q + 1;
    while (at <= path.size()) {
        auto end = path.find('&', at);
        if (end == std::string::npos) {
            end = path.size();
        }
        const std::string pair = path.substr(at, end - at);
        if (!names.empty()) {
            names += ',';
        }
        names += pair.substr(0, pair.find('='));
        at = end + 1;
    }
    return names;
}

std::string HeaderText(const httplib::Headers& headers) {
    std::string out;
    for (const auto& [name, value] : headers) {
        out += fmt::format("{}{}: {}", out.empty() ? "" : ", ", name, value);
    }
    return out;
}

std::string HostOf(const std::string& url) {
    const auto start = url.find("://");
    const auto slash = url.find('/', start == std::string::npos ? 0 : start + 3);
    return url.substr(0, slash);
}

struct Hop {
    int status = 0;
    std::string location;
    std::string error;
    std::string error_body; // an error answer's first bytes, for the log
    bool done = false; // the body went to `sink`
};

/// One GET, no redirect followed. A 200 body goes to `sink` (false stops it).
Hop GetOnce(const std::string& host, const std::string& path,
            const std::function<bool(u64 total)>& start,
            const std::function<bool(const char*, std::size_t)>& sink) {
    Hop hop;
    const char* ca = CaBundle();
    if (ca == nullptr) {
        hop.error = "No certificate store found";
        return hop;
    }
    try {
        httplib::Client client(host);
        client.set_connection_timeout(15);
        client.set_read_timeout(60);
        client.set_write_timeout(30);
        client.set_follow_location(false);
        client.set_ca_cert_path(ca);
        client.enable_server_certificate_verification(true);
        client.set_url_encode(false); // AzaharFrame emerald: common/request_path.h
        httplib::Headers headers{{"Accept-Encoding", "identity"}};
        if (host == kApiHost) {
            headers.emplace("Accept", "application/vnd.github+json");
        }
        // AzaharFrame emerald: every hop in the log, without the query's values (a signed URL's
        // query is a credential): host, path, the query's names, our headers, the answer.
        LOG_INFO(Frontend, "homebrew: GET {}{} (query: {}; headers: {})", host,
                 path.substr(0, path.find('?')), QueryNames(path), HeaderText(headers));
        bool stopped = false;
        const auto res = client.Get(
            Common::RequestPath(path), headers,
            [&](const httplib::Response& r) {
                hop.status = r.status;
                hop.location = r.get_header_value("Location");
                if (r.status == 200) {
                    u64 total = 0;
                    if (const auto cl = r.get_header_value("Content-Length"); !cl.empty()) {
                        total = std::strtoull(cl.c_str(), nullptr, 10);
                    }
                    hop.done = true;
                    if (!start(total)) {
                        stopped = true;
                        return false;
                    }
                }
                return true;
            },
            [&](const char* data, std::size_t len) {
                if (!hop.done) {
                    if (hop.status >= 400 && hop.error_body.size() < 1000) {
                        hop.error_body.append(data, std::min<std::size_t>(len, 1000));
                    }
                    return true; // a redirect's or an error's body
                }
                if (!sink(data, len)) {
                    stopped = true;
                    return false;
                }
                return true;
            });
        if (res) {
            const auto type = res->get_header_value("Content-Type");
            LOG_INFO(Frontend, "homebrew: {} from {}{}{}", hop.status, host,
                     hop.location.empty() ? "" : ", redirect to ",
                     hop.location.empty() ? "" : HostOf(hop.location));
            if (hop.status >= 400) {
                LOG_WARNING(Frontend, "homebrew: {} answered {} ({}): {}", host, hop.status, type,
                            hop.error_body.substr(0, 1000));
            }
        }
        if (!res && !stopped) {
            hop.error = httplib::to_string(res.error());
        } else if (stopped && hop.error.empty()) {
            hop.error = "Stopped";
        }
    } catch (const std::exception& e) {
        hop.error = e.what();
    }
    return hop;
}

/// Follows redirects among the allowed hosts. "" on success.
std::string Fetch(std::string url, const std::function<bool(u64 total)>& start,
                  const std::function<bool(const char*, std::size_t)>& sink, int* status,
                  bool pinned_hosts = false) {
    for (int hops = 0; hops < 8; ++hops) {
        std::string host, path;
        if (!SplitUrl(url, host, path) ||
            !(HostAllowed(host) || (pinned_hosts && PinnedHostAllowed(host)))) {
            return "Download refused: not a GitHub address";
        }
        const Hop hop = GetOnce(host, path, start, sink);
        *status = hop.status;
        if (!hop.error.empty()) {
            LOG_WARNING(Frontend, "homebrew: GET {}{} failed: {}", host, path, hop.error);
            return hop.error;
        }
        if (hop.done) {
            return {};
        }
        if (hop.status >= 300 && hop.status < 400 && !hop.location.empty()) {
            url = hop.location.front() == '/' ? host + hop.location : hop.location;
            continue;
        }
        if ((hop.status == 403 || hop.status == 429) && host == kApiHost) {
            return "GitHub rate limit reached, try again later";
        }
        if (hop.status == 404) {
            return "Not found on GitHub (404)";
        }
        return hop.status == 0 ? "No connection" : fmt::format("Server returned {}", hop.status);
    }
    return "Too many redirects";
}

} // namespace

Reply GetText(const std::string& url, std::size_t max_body) {
    Reply out;
    bool too_large = false;
    out.error = Fetch(
        url, [&](u64 total) { return !(too_large = total > max_body); },
        [&](const char* data, std::size_t len) {
            if (out.body.size() + len > max_body) {
                too_large = true;
                return false;
            }
            out.body.append(data, len);
            return true;
        },
        &out.status);
    if (too_large) {
        out.error = "Download too large";
    }
    if (!out.error.empty()) {
        out.body.clear();
    }
    return out;
}

Reply GetPinned(const std::string& url, std::size_t max_body) {
    Reply out;
    bool too_large = false;
    out.error = Fetch(
        url, [&](u64 total) { return !(too_large = total > max_body); },
        [&](const char* data, std::size_t len) {
            if (out.body.size() + len > max_body) {
                too_large = true;
                return false;
            }
            out.body.append(data, len);
            return true;
        },
        &out.status, true);
    if (too_large) {
        out.error = "Download too large";
    }
    if (!out.error.empty()) {
        out.body.clear();
    }
    return out;
}

std::string Download(const std::string& url, const std::string& dest,
                     const std::function<bool(u64, u64)>& progress) {
    const std::string part = dest + ".part";
    std::FILE* file = nullptr;
    u64 total = 0, done = 0;
    bool write_failed = false;
    int status = 0;
    std::string error = Fetch(
        url,
        [&](u64 t) {
            total = t;
            done = 0;
            if (file != nullptr) {
                std::fclose(file);
            }
            file = std::fopen(part.c_str(), "wb");
            return file != nullptr && progress(0, total);
        },
        [&](const char* data, std::size_t len) {
            if (std::fwrite(data, 1, len, file) != len) {
                write_failed = true;
                return false;
            }
            done += len;
            return progress(done, total);
        },
        &status);
    if (file != nullptr) {
        if (std::fclose(file) != 0) {
            write_failed = true;
        }
    }
    if (write_failed) {
        error = "Not enough free space (or the disk is read-only)";
    } else if (error.empty() && total != 0 && done != total) {
        error = "The download was cut short";
    }
    if (!error.empty()) {
        FileUtil::Delete(part);
        return error;
    }
    FileUtil::Delete(dest);
    if (!FileUtil::Rename(part, dest)) {
        FileUtil::Delete(part);
        return "The download could not be saved";
    }
    return {};
}

std::string FileSha256(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    CryptoPP::SHA256 hash;
    std::vector<char> buf(1 << 20);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const auto n = in.gcount();
        if (n > 0) {
            hash.Update(reinterpret_cast<const CryptoPP::byte*>(buf.data()),
                        static_cast<std::size_t>(n));
        }
    }
    return HexDigest(hash);
}

std::string Sha256Hex(std::string_view data) {
    CryptoPP::SHA256 hash;
    hash.Update(reinterpret_cast<const CryptoPP::byte*>(data.data()), data.size());
    return HexDigest(hash);
}

std::string Sha1Hex(std::span<const u8> data) {
    CryptoPP::SHA1 hash;
    hash.Update(data.data(), data.size());
    return HexDigest(hash);
}

} // namespace Homebrew
