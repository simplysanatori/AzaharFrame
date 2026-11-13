// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <map>
#include <memory>
#include <thread>
#include <httplib.h>
#include <fmt/format.h>
#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "core/mods/transfer.h"

namespace Mods::Transfer {

// ---------------------------------------------------------------------------------------------
// Stopper

Stopper::~Stopper() {
    Unwatch();
}

void Stopper::Stop() {
    std::lock_guard lock{mutex_};
    flag_ = true;
#ifndef _WIN32
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR); // wakes a connect, a handshake or a read in the worker
    }
#endif
}

void Stopper::Reset() {
    std::lock_guard lock{mutex_};
    flag_ = false;
}

void Stopper::Watch(int fd) {
    std::lock_guard lock{mutex_};
#ifndef _WIN32
    if (fd_ >= 0) {
        ::close(fd_); // an earlier address's socket, already given up
    }
    fd_ = fd >= 0 ? ::dup(fd) : -1;
    if (fd_ >= 0 && flag_) {
        ::shutdown(fd_, SHUT_RDWR);
    }
#else
    (void)fd;
#endif
}

void Stopper::Unwatch() {
    std::lock_guard lock{mutex_};
#ifndef _WIN32
    if (fd_ >= 0) {
        ::close(fd_);
    }
#endif
    fd_ = -1;
}

// ---------------------------------------------------------------------------------------------
// Host lookup

namespace {

std::string HostOnly(std::string_view host_with_scheme) {
    std::string_view h = host_with_scheme;
    if (const auto p = h.find("://"); p != std::string_view::npos) {
        h.remove_prefix(p + 3);
    }
    if (!h.empty() && h.front() == '[') { // [::1]:8080
        const auto close = h.find(']');
        return std::string(h.substr(1, close == std::string_view::npos ? h.npos : close - 1));
    }
    h = h.substr(0, h.find_first_of(":/"));
    std::string out(h);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool IsAddress(const std::string& host) {
#ifndef _WIN32
    unsigned char buf[16];
    return inet_pton(AF_INET, host.c_str(), buf) == 1 || inet_pton(AF_INET6, host.c_str(), buf) == 1;
#else
    return false;
#endif
}

} // namespace

std::optional<std::string> ResolveHost(const std::string& host, const std::atomic_bool* cancel,
                                       std::chrono::milliseconds timeout) {
    if (cancel != nullptr && cancel->load()) {
        return std::nullopt;
    }
    if (host.empty()) {
        return std::nullopt;
    }
    if (IsAddress(host)) {
        return host;
    }
#ifndef _WIN32
    struct Lookup {
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
        std::optional<std::string> address;
    };
    auto lookup = std::make_shared<Lookup>();
    // A thread of its own that may outlive the wait: a lookup cannot be stopped, only left behind
    std::thread([lookup, host] {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        std::optional<std::string> found;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) == 0) {
            const addrinfo* v6 = nullptr;
            const addrinfo* v4 = nullptr;
            for (const addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
                if (rp->ai_family == AF_INET && v4 == nullptr) {
                    v4 = rp;
                } else if (rp->ai_family == AF_INET6 && v6 == nullptr) {
                    v6 = rp;
                }
            }
            char text[INET6_ADDRSTRLEN] = {};
            if (v4 != nullptr) {
                const auto* a = reinterpret_cast<const sockaddr_in*>(v4->ai_addr);
                if (inet_ntop(AF_INET, &a->sin_addr, text, sizeof(text)) != nullptr) {
                    found = text;
                }
            } else if (v6 != nullptr) {
                const auto* a = reinterpret_cast<const sockaddr_in6*>(v6->ai_addr);
                if (inet_ntop(AF_INET6, &a->sin6_addr, text, sizeof(text)) != nullptr) {
                    found = text;
                }
            }
            freeaddrinfo(result);
        }
        std::lock_guard lock{lookup->mutex};
        lookup->address = std::move(found);
        lookup->done = true;
        lookup->cv.notify_all();
    }).detach();
    const auto end = std::chrono::steady_clock::now() + timeout;
    std::unique_lock lock{lookup->mutex};
    while (!lookup->done) {
        if (cancel != nullptr && cancel->load()) {
            return std::nullopt;
        }
        if (std::chrono::steady_clock::now() >= end) {
            return std::nullopt;
        }
        lookup->cv.wait_for(lock, std::chrono::milliseconds(50));
    }
    return lookup->address;
#else
    return host;
#endif
}

std::string Prepare(httplib::Client& client, const std::string& host_with_scheme, Stopper* stopper) {
    client.set_connection_timeout(kConnectTimeoutS);
    client.set_read_timeout(kStallTimeoutS);
    const std::atomic_bool* flag = stopper != nullptr ? stopper->Flag() : nullptr;
    if (flag != nullptr && flag->load()) {
        return "Cancelled";
    }
    const std::string host = HostOnly(host_with_scheme);
    if (stopper != nullptr && !IsAddress(host)) {
        const auto address = ResolveHost(host, flag);
        if (flag != nullptr && flag->load()) {
            return "Cancelled";
        }
        if (!address) {
            return "No connection";
        }
        client.set_hostname_addr_map({{host, *address}}); // the name still goes in SNI and Host
    }
    if (stopper != nullptr) {
        client.set_socket_options([stopper](socket_t sock) { stopper->Watch(static_cast<int>(sock)); });
    }
    return {};
}

void Finish(Stopper* stopper) {
    if (stopper != nullptr) {
        stopper->Unwatch();
    }
}

bool SleepUnlessStopped(std::chrono::milliseconds total, const std::atomic_bool* cancel) {
    const auto end = std::chrono::steady_clock::now() + total;
    while (std::chrono::steady_clock::now() < end) {
        if (cancel != nullptr && cancel->load()) {
            return false;
        }
        std::this_thread::sleep_for(std::min(std::chrono::milliseconds(50), total));
    }
    return cancel == nullptr || !cancel->load();
}

// ---------------------------------------------------------------------------------------------
// Rate limits

std::optional<int> ParseRetryAfter(std::string_view value, std::time_t now) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    if (value.empty()) {
        return std::nullopt;
    }
    if (std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        if (value.size() > 9) {
            return std::nullopt;
        }
        return std::stoi(std::string(value));
    }
    // An HTTP date: "Wed, 21 Oct 2015 07:28:00 GMT"
    static constexpr const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char wday[4] = {}, mon[4] = {}, zone[4] = {};
    int day = 0, year = 0, h = 0, m = 0, s = 0;
    const std::string text(value);
    if (std::sscanf(text.c_str(), "%3s, %d %3s %d %d:%d:%d %3s", wday, &day, mon, &year, &h, &m, &s, zone) != 8 ||
        std::string_view(zone) != "GMT") {
        return std::nullopt;
    }
    int month = -1;
    for (int i = 0; i < 12; ++i) {
        if (std::string_view(mon) == kMonths[i]) {
            month = i;
        }
    }
    if (month < 0 || day < 1 || day > 31 || year < 1970 || h > 23 || m > 59 || s > 60) {
        return std::nullopt;
    }
    // Days from the civil date (no timegm needed)
    const int y = year - (month < 2 ? 1 : 0);
    const int era = y / 400;
    const int yoe = y - era * 400;
    const int mp = (month + 10) % 12;
    const int doy = (153 * mp + 2) / 5 + day - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long long days = static_cast<long long>(era) * 146097 + doe - 719468;
    const long long when = days * 86400 + h * 3600 + m * 60 + s;
    const long long wait = when - static_cast<long long>(now);
    return static_cast<int>(std::clamp<long long>(wait, 0, 7LL * 86400));
}

Limit DetectLimit(int status, std::string_view body, std::string_view retry_after, std::time_t now) {
    Limit out;
    const bool cloudflare_1015 = body.find("error code: 1015") != std::string_view::npos ||
                                 body.find("Error 1015") != std::string_view::npos ||
                                 body.find("error 1015") != std::string_view::npos;
    out.limited = status == 429 || (cloudflare_1015 && (status == 403 || status == 503 || status == 200));
    if (out.limited) {
        out.retry_after_s = ParseRetryAfter(retry_after, now).value_or(-1);
    }
    return out;
}

std::string SiteName(std::string_view host_with_scheme) {
    const std::string host = HostOnly(host_with_scheme);
    const auto is = [&](std::string_view domain) {
        return host == domain || (host.size() > domain.size() && host.ends_with(domain) &&
                                  host[host.size() - domain.size() - 1] == '.');
    };
    if (is("gamebanana.com")) {
        return "GameBanana";
    }
    if (is("github.com") || is("githubusercontent.com")) {
        return "GitHub";
    }
    if (host == "drive.google.com" || host == "drive.usercontent.google.com" || is("googleusercontent.com")) {
        return "Google Drive";
    }
    if (is("mega.nz") || is("mega.co.nz") || is("mega.io")) {
        return "MEGA";
    }
    return host;
}

std::string TryAgainText(int seconds) {
    if (seconds < 0) {
        return "Try again later.";
    }
    const auto unit = [](int n, const char* one) {
        return fmt::format("Try again in {} {}{}.", n, one, n == 1 ? "" : "s");
    };
    if (seconds < 60) {
        return unit(std::max(1, seconds), "second");
    }
    if (seconds < 2 * 3600) {
        return unit((seconds + 59) / 60, "minute");
    }
    return unit((seconds + 3599) / 3600, "hour");
}

std::string LimitText(std::string_view site, int retry_after_s) {
    return fmt::format("{} is limiting downloads from this network. {}", site.empty() ? "The site" : site,
                       TryAgainText(retry_after_s));
}

bool IsLimitError(std::string_view error) {
    return error.find("is limiting downloads") != std::string_view::npos ||
           error.find("rate limit") != std::string_view::npos;
}

} // namespace Mods::Transfer
