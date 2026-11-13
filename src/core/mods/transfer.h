// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame fixes7: what every download of mods and texture packs shares so Cancel (and quitting
// the app) works at once in any state, and a site that limits downloads says so.
//
// Stopper: Cancel sets the flag the transfer loops check, and shuts down the socket in use (a copy
// of its descriptor is kept while the request runs), so a connect that hangs, a TLS handshake or a
// read that stalls ends at once instead of after its timeout. A host name is looked up on a thread
// of its own (ResolveHost), so a dead DNS server after a network change does not hold Cancel either.
//
// Rate limits: HTTP 429, or Cloudflare's error 1015 page, with the Retry-After header when the site
// sends one: "GameBanana is limiting downloads from this network. Try again in 10 minutes."
// Nothing retries on its own against a limit.

#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace httplib {
class Client;
}

namespace Mods::Transfer {

class Stopper {
public:
    explicit Stopper(std::atomic_bool& flag) : flag_(flag) {}
    ~Stopper();
    Stopper(const Stopper&) = delete;
    Stopper& operator=(const Stopper&) = delete;

    /// Sets the flag and shuts down the socket a request uses now (any thread; again is harmless).
    void Stop();
    /// Clears the flag for the next run (the worker is not running).
    void Reset();
    bool Stopped() const {
        return flag_.load();
    }
    const std::atomic_bool* Flag() const {
        return &flag_;
    }

    /// The transfer's side: the socket being connected or read (a copy of the descriptor is kept,
    /// so a stop never touches a number the process reuses). Shut down at once when already stopped.
    void Watch(int fd);
    /// The request is over: the copy is closed.
    void Unwatch();

private:
    std::atomic_bool& flag_;
    std::mutex mutex_;
    int fd_ = -1;
};

/// Timeouts of every download request: connecting, and a read that gets nothing at all (a stall).
constexpr int kConnectTimeoutS = 10;
constexpr int kStallTimeoutS = 30;

/// Sets up `client` for a download: the timeouts above, the socket watched by `stopper` (nullptr:
/// none) and the host's address looked up first (ResolveHost) so the lookup can be stopped too.
/// "" when it may go ahead, else why not ("Cancelled", "Resolve").
std::string Prepare(httplib::Client& client, const std::string& host_with_scheme, Stopper* stopper);
/// Ends what Prepare watched (call after the request, before the client goes).
void Finish(Stopper* stopper);

/// The address of `host` ("gamebanana.com"), IPv4 first, looked up on a thread of its own; waits
/// at most `timeout` and returns early (nullopt) when `cancel` is set. An address stays as it is.
std::optional<std::string> ResolveHost(const std::string& host, const std::atomic_bool* cancel,
                                       std::chrono::milliseconds timeout = std::chrono::seconds(15));

/// Sleeps `total` in short steps; false when `cancel` was set meanwhile.
bool SleepUnlessStopped(std::chrono::milliseconds total, const std::atomic_bool* cancel);

// ---- Rate limits ----

struct Limit {
    bool limited = false;
    int retry_after_s = -1; ///< from Retry-After, -1 not given
};
/// HTTP 429, or a Cloudflare "error code: 1015" page (sent with 429, sometimes 403 or 503).
/// `retry_after` is the header's value ("600" or an HTTP date, read against `now`).
Limit DetectLimit(int status, std::string_view body, std::string_view retry_after, std::time_t now);
/// Retry-After as seconds from `now`: a number, or an HTTP date ("Wed, 21 Oct 2015 07:28:00 GMT");
/// nullopt for anything else. A date already past is 0.
std::optional<int> ParseRetryAfter(std::string_view value, std::time_t now);
/// The site's name for a message: "GameBanana", "GitHub", "Google Drive", "MEGA", else the bare
/// host ("files.example.com").
std::string SiteName(std::string_view host_with_scheme);
/// "Try again in 10 minutes." (rounded up: seconds under a minute, minutes under two hours, then
/// hours); "Try again later." when the time is not known (< 0).
std::string TryAgainText(int seconds);
/// "<Site> is limiting downloads from this network. Try again in 10 minutes."
std::string LimitText(std::string_view site, int retry_after_s);
/// The error a download failed with is a rate limit (LimitText's, or GitHub's own): never retried.
bool IsLimitError(std::string_view error);

} // namespace Mods::Transfer
