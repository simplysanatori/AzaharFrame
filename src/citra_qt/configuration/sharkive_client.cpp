// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <array>
#include <httplib.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/cheats/sharkive.h"
#include "sharkive_client.h"

namespace SharkiveClient {

namespace {

constexpr const char* kHost = "https://raw.githubusercontent.com";
constexpr std::size_t kMaxBody = 2 * 1024 * 1024;

// The static LibreSSL has no CA store of its own; SteamOS and the common distributions keep one of
// these.
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

} // namespace

Result Fetch(u64 title_id) {
    Result out;
    const char* ca = CaBundle();
    if (ca == nullptr) {
        out.error = "No certificate store found";
        return out;
    }
    try {
        httplib::Client client(kHost);
        client.set_connection_timeout(10);
        client.set_read_timeout(15);
        client.set_write_timeout(15);
        client.set_follow_location(false);
        client.set_ca_cert_path(ca);
        client.enable_server_certificate_verification(true);

        const std::string path = Cheats::Sharkive::UrlPath(title_id);
        const auto res = client.Get(path);
        if (!res) {
            out.error = httplib::to_string(res.error());
            LOG_WARNING(Frontend, "Sharkive: GET failed: {}", out.error);
            return out;
        }
        if (res->status == 404) {
            out.status = Status::NotFound;
            return out;
        }
        if (res->status != 200) {
            out.error = "Server returned " + std::to_string(res->status);
            return out;
        }
        if (res->body.size() > kMaxBody) {
            out.error = "Reply too large";
            return out;
        }
        out.status = Status::Ok;
        out.body = res->body;
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    return out;
}

} // namespace SharkiveClient
