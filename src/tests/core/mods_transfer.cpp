// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame fixes7: Cancel in every state of a download (a local server that hangs: never
// answers, answers the headers and stalls, or never lets the connection in), rate limits and their
// message, Resume of texture and mod downloads, "recommended" and "aspect" in both catalogs.

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <httplib.h>
#include "core/cheats/online.h"
#include "core/custom_tex/download.h"
#include "core/mods/aspect.h"
#include "core/mods/catalog.h"
#include "core/mods/library.h"
#include "core/mods/multipart.h"
#include "core/mods/transfer.h"

namespace fs = std::filesystem;
namespace T = Mods::Transfer;
using Clock = std::chrono::steady_clock;

namespace {

constexpr u64 kTid = 0x0004000000033500ULL;

struct Temp {
    fs::path root;
    Temp() {
        static int n = 0;
        root = fs::temp_directory_path() / ("azf_fixes7_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
        fs::remove_all(root);
        fs::create_directories(root / "load");
    }
    ~Temp() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    std::string load() const { return (root / "load").string(); }
};

/// A listening socket on 127.0.0.1 that never accepts: the kernel lets the connection in, the
/// request is never answered. With `backlog` 0 and one connection already queued, a further
/// connect is not let in at all (its SYN is dropped while the queue is full).
struct Listener {
    int fd = -1;
    int port = 0;
    int filler = -1;
    explicit Listener(int backlog = 16) {
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = 0;
        REQUIRE(::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0);
        REQUIRE(::listen(fd, backlog) == 0);
        socklen_t len = sizeof(a);
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&a), &len);
        port = ntohs(a.sin_port);
    }
    void FillQueue() {
        filler = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = htons(static_cast<uint16_t>(port));
        ::connect(filler, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    }
    std::string Host() const { return "http://127.0.0.1:" + std::to_string(port); }
    ~Listener() {
        if (filler >= 0) {
            ::close(filler);
        }
        ::close(fd);
    }
};

/// Accepts one connection, reads the request, sends `head` (headers and some bytes), then holds the
/// connection open without a word until the test ends.
struct Staller {
    Listener listener;
    std::thread thread;
    std::atomic_bool done{false};
    explicit Staller(std::string head) {
        thread = std::thread([this, head = std::move(head)] {
            const int c = ::accept(listener.fd, nullptr, nullptr);
            if (c < 0) {
                return;
            }
            char buf[4096];
            ::recv(c, buf, sizeof(buf), 0);
            ::send(c, head.data(), head.size(), MSG_NOSIGNAL);
            const auto end = Clock::now() + std::chrono::seconds(20);
            while (!done && Clock::now() < end) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            ::close(c);
        });
    }
    std::string Host() const { return listener.Host(); }
    ~Staller() {
        done = true;
        ::shutdown(listener.fd, SHUT_RDWR); // an accept still waiting ends
        thread.join();
    }
};

/// Runs `work` on a thread, stops `stopper` after `after`, and gives how long the work went on
/// after the stop.
std::chrono::milliseconds StopAfter(T::Stopper& stopper, std::chrono::milliseconds after,
                                    const std::function<void()>& work) {
    std::atomic_bool finished{false};
    std::thread t([&] {
        work();
        finished = true;
    });
    std::this_thread::sleep_for(after);
    const auto stop = Clock::now();
    stopper.Stop();
    t.join();
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - stop);
}

constexpr auto kPrompt = std::chrono::milliseconds(2000); // Cancel must end the transfer within this

} // namespace

// ---- Cancel in every state ----

TEST_CASE("fixes7: Cancel ends a texture download the server never answers", "[fixes7][cancel]") {
    Temp t;
    Listener server;
    std::atomic_bool cancel{false};
    T::Stopper stopper{cancel};
    std::atomic<u64> done{0}, total{0};
    CustomTex::Download::Step step;
    const auto waited = StopAfter(stopper, std::chrono::milliseconds(300), [&] {
        CustomTex::Download::StepIo io{(t.root / "a.part").string(), 0, &cancel, &done, &total, {}, &stopper};
        step = CustomTex::Download::GetStep(server.Host(), "/file.zip", io);
    });
    CHECK(waited < kPrompt);
    CHECK(step.cancelled);
}

TEST_CASE("fixes7: Cancel ends a texture download that stalls after its headers, and it can resume",
          "[fixes7][cancel]") {
    Temp t;
    Staller server("HTTP/1.1 200 OK\r\nContent-Type: application/zip\r\nContent-Length: 100000\r\n\r\n0123456789");
    std::atomic_bool cancel{false};
    T::Stopper stopper{cancel};
    std::atomic<u64> done{0}, total{0};
    std::atomic_bool receiving{false};
    const std::string part = (t.root / "a.part").string();
    CustomTex::Download::Step step;
    const auto waited = StopAfter(stopper, std::chrono::milliseconds(500), [&] {
        CustomTex::Download::StepIo io{part, 0, &cancel, &done, &total, {}, &stopper, &receiving};
        step = CustomTex::Download::GetStep(server.Host(), "/file.zip", io);
    });
    CHECK(waited < kPrompt);
    CHECK(step.cancelled);
    CHECK(receiving);        // the file's bytes had started: the menu showed the percent
    CHECK(total == 100000);
    CHECK(done == 10);
    // the part file keeps what came, for Resume (a Range request from byte 10)
    CHECK(fs::file_size(part) == 10);
}

TEST_CASE("fixes7: Cancel ends a connect that is never let in", "[fixes7][cancel]") {
    Temp t;
    Listener server(0);
    server.FillQueue();
    std::atomic_bool cancel{false};
    T::Stopper stopper{cancel};
    std::atomic<u64> done{0}, total{0};
    CustomTex::Download::Step step;
    const auto waited = StopAfter(stopper, std::chrono::milliseconds(300), [&] {
        CustomTex::Download::StepIo io{(t.root / "a.part").string(), 0, &cancel, &done, &total, {}, &stopper};
        step = CustomTex::Download::GetStep(server.Host(), "/file.zip", io);
    });
    CHECK(waited < kPrompt);
    CHECK(step.cancelled);
}

TEST_CASE("fixes7: Cancel ends a mod download that hangs, in each request kind", "[fixes7][cancel]") {
    Temp t;
    SECTION("a part to a file, never answered") {
        Listener server;
        std::atomic_bool cancel{false};
        T::Stopper stopper{cancel};
        Mods::Multipart::FileReply reply;
        const auto waited = StopAfter(stopper, std::chrono::milliseconds(300), [&] {
            reply = Mods::Multipart::StreamGet(server.Host(), "/p", (t.root / "p").string(), &cancel, {}, 0,
                                               &stopper);
        });
        CHECK(waited < kPrompt);
        CHECK(reply.error == "Cancelled");
    }
    SECTION("a part to a file, stalled after its headers") {
        Staller server("HTTP/1.1 200 OK\r\nContent-Length: 5000\r\n\r\nabc");
        std::atomic_bool cancel{false};
        T::Stopper stopper{cancel};
        Mods::Multipart::FileReply reply;
        const auto waited = StopAfter(stopper, std::chrono::milliseconds(500), [&] {
            reply = Mods::Multipart::StreamGet(server.Host(), "/p", (t.root / "p").string(), &cancel, {}, 0,
                                               &stopper);
        });
        CHECK(waited < kPrompt);
        CHECK(reply.error == "Cancelled");
    }
    SECTION("an API request or a small mod (to memory)") {
        Staller server("HTTP/1.1 200 OK\r\nContent-Length: 5000\r\n\r\n{");
        std::atomic_bool cancel{false};
        T::Stopper stopper{cancel};
        Cheats::Online::RawReply reply;
        const auto waited = StopAfter(stopper, std::chrono::milliseconds(500), [&] {
            reply = Cheats::Online::HttpsGetRaw(server.Host(), "/api", 1 << 20, &stopper);
        });
        CHECK(waited < kPrompt);
        CHECK(reply.error == "Cancelled");
    }
}

TEST_CASE("fixes7: the Stopper", "[fixes7][cancel]") {
    SECTION("a stop before the socket is watched shuts it down when it is") {
        int sv[2];
        REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
        std::atomic_bool flag{false};
        T::Stopper s{flag};
        s.Stop();
        s.Stop(); // again: harmless (Cancel pressed many times)
        CHECK(flag);
        s.Watch(sv[0]);
        char c;
        CHECK(::recv(sv[0], &c, 1, 0) == 0); // shut down: no wait
        s.Unwatch();
        ::close(sv[0]);
        ::close(sv[1]);
        s.Reset();
        CHECK_FALSE(flag);
    }
    SECTION("a stop wakes a read in another thread") {
        int sv[2];
        REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
        std::atomic_bool flag{false};
        T::Stopper s{flag};
        s.Watch(sv[0]);
        const auto waited = StopAfter(s, std::chrono::milliseconds(200), [&] {
            char c;
            ::recv(sv[0], &c, 1, 0);
        });
        CHECK(waited < kPrompt);
        s.Unwatch();
        ::close(sv[0]);
        ::close(sv[1]);
    }
    SECTION("waits and lookups give up when stopped") {
        std::atomic_bool flag{true};
        const auto t0 = Clock::now();
        CHECK_FALSE(T::SleepUnlessStopped(std::chrono::seconds(10), &flag));
        CHECK(Clock::now() - t0 < kPrompt);
        CHECK_FALSE(T::ResolveHost("example.com", &flag).has_value());
        std::atomic_bool off{false};
        CHECK(T::ResolveHost("127.0.0.1", &off) == "127.0.0.1"); // an address is not looked up
        CHECK(T::SleepUnlessStopped(std::chrono::milliseconds(1), &off));
    }
}

// ---- Rate limits ----

TEST_CASE("fixes7: Retry-After", "[fixes7][ratelimit]") {
    constexpr std::time_t kDate = 1445412480; // Wed, 21 Oct 2015 07:28:00 GMT
    CHECK(T::ParseRetryAfter("600", 0) == 600);
    CHECK(T::ParseRetryAfter(" 30 ", 0) == 30);
    CHECK(T::ParseRetryAfter("Wed, 21 Oct 2015 07:28:00 GMT", kDate - 120) == 120);
    CHECK(T::ParseRetryAfter("Wed, 21 Oct 2015 07:28:00 GMT", kDate + 50) == 0); // past
    CHECK_FALSE(T::ParseRetryAfter("", 0).has_value());
    CHECK_FALSE(T::ParseRetryAfter("soon", 0).has_value());
    CHECK_FALSE(T::ParseRetryAfter("-5", 0).has_value());
}

TEST_CASE("fixes7: rate limits are found and said clearly", "[fixes7][ratelimit]") {
    CHECK(T::DetectLimit(429, "", "", 0).limited);
    CHECK(T::DetectLimit(429, "", "", 0).retry_after_s == -1);
    CHECK(T::DetectLimit(429, "", "600", 0).retry_after_s == 600);
    CHECK(T::DetectLimit(403, "error code: 1015", "", 0).limited); // Cloudflare's plain page
    CHECK(T::DetectLimit(503, "<h1>Error 1015</h1> You are being rate limited", "", 0).limited);
    CHECK_FALSE(T::DetectLimit(403, "Forbidden", "", 0).limited);
    CHECK_FALSE(T::DetectLimit(404, "", "", 0).limited);
    CHECK_FALSE(T::DetectLimit(200, "", "", 0).limited);

    CHECK(T::SiteName("https://gamebanana.com") == "GameBanana");
    CHECK(T::SiteName("https://files.gamebanana.com") == "GameBanana");
    CHECK(T::SiteName("https://notgamebanana.com") == "notgamebanana.com");
    CHECK(T::SiteName("https://objects.githubusercontent.com") == "GitHub");
    CHECK(T::SiteName("https://drive.usercontent.google.com") == "Google Drive");
    CHECK(T::SiteName("https://files.example.com:8443") == "files.example.com");

    CHECK(T::TryAgainText(-1) == "Try again later.");
    CHECK(T::TryAgainText(1) == "Try again in 1 second.");
    CHECK(T::TryAgainText(30) == "Try again in 30 seconds.");
    CHECK(T::TryAgainText(60) == "Try again in 1 minute.");
    CHECK(T::TryAgainText(61) == "Try again in 2 minutes.");
    CHECK(T::TryAgainText(600) == "Try again in 10 minutes.");
    CHECK(T::TryAgainText(7200) == "Try again in 2 hours.");

    CHECK(T::LimitText("GameBanana", -1) == "GameBanana is limiting downloads from this network. Try again later.");
    CHECK(T::LimitText("GameBanana", 600) ==
          "GameBanana is limiting downloads from this network. Try again in 10 minutes.");
    CHECK(T::IsLimitError(T::LimitText("files.example.com", 5)));
    CHECK(T::IsLimitError("GitHub rate limit reached, try again later"));
    CHECK_FALSE(T::IsLimitError("Download not found (404)"));
    // the Install Mods row shows it whole
    CHECK(Mods::ShortFailure(T::LimitText("GameBanana", 600)) == T::LimitText("GameBanana", 600));
    CHECK(Mods::ShortFailure("No connection") == "No Connection");
}

TEST_CASE("fixes7: a 429 from a file host is reported as a rate limit", "[fixes7][ratelimit]") {
    httplib::Server server;
    server.Get("/limited", [](const httplib::Request&, httplib::Response& res) {
        res.status = 429;
        res.set_header("Retry-After", "600");
        res.set_content("error code: 1015", "text/plain");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    std::thread thread([&] { server.listen_after_bind(); });
    server.wait_until_ready();
    const std::string host = "http://127.0.0.1:" + std::to_string(port);
    Temp t;
    std::atomic_bool cancel{false};
    T::Stopper stopper{cancel};
    const auto file = Mods::Multipart::StreamGet(host, "/limited", (t.root / "p").string(), &cancel, {}, 0, &stopper);
    CHECK(file.error == "127.0.0.1 is limiting downloads from this network. Try again in 10 minutes.");
    const auto raw = Cheats::Online::HttpsGetRaw(host, "/limited", 1 << 20, &stopper);
    CHECK(raw.error == file.error);
    std::atomic<u64> done{0}, total{0};
    CustomTex::Download::StepIo io{(t.root / "q").string(), 0, &cancel, &done, &total, {}, &stopper};
    const auto step = CustomTex::Download::GetStep(host, "/limited", io);
    CHECK(step.status == 429);
    CHECK(step.retry_after == "600");
    CHECK(T::DetectLimit(step.status, step.html, step.retry_after, 0).limited);
    server.stop();
    thread.join();
}

// ---- Resume ----

TEST_CASE("fixes7: a kept texture download is offered for Resume with its size", "[fixes7][resume]") {
    Temp t;
    auto parsed = CustomTex::Download::ParseCatalog(R"({"schema":1,"packs":[
      {"id":"big","name":"Big","title":"G","title_ids":["0004000000033500"],"author":"A","size":3000,
       "format":"zip","folder":"Big","source":{"type":"https","url":"https://example.com/big.zip"}}]})");
    REQUIRE(parsed.has_value());
    CustomTex::Download::Downloader dl(t.load(), *parsed);
    auto list = dl.ListForTitle(kTid);
    REQUIRE(list.size() == 1);
    CHECK_FALSE(list[0].partial);
    fs::create_directories(fs::path(t.load()) / "texture-downloads");
    std::ofstream(fs::path(t.load()) / "texture-downloads" / "big.part", std::ios::binary) << std::string(1200, 'x');
    list = dl.ListForTitle(kTid);
    CHECK(list[0].partial);
    CHECK(list[0].partial_bytes == 1200);
}

TEST_CASE("fixes7: a mod's kept parts are counted for Resume", "[fixes7][resume]") {
    Temp t;
    Mods::Library lib(t.load() + "/", Mods::Catalog{}, [](const std::string&, const std::string&, std::size_t) {
        Mods::HttpReply r;
        r.status = 404;
        return r;
    });
    CHECK(lib.PartialBytes("big") == 0);
    const fs::path work = fs::path(t.load()) / "mod-downloads" / "big";
    fs::create_directories(work / "out");
    std::ofstream(work / "part001", std::ios::binary) << std::string(100, 'a');
    std::ofstream(work / "part002", std::ios::binary) << std::string(40, 'b');
    std::ofstream(work / "parts.txt") << "list\n";
    std::ofstream(work / "out" / "part003") << "not a part";
    CHECK(lib.PartialBytes("big") == 140);
    CHECK(lib.PartialBytes("../x") == 0);
}

// ---- recommended, aspect ----

TEST_CASE("fixes7: aspect values", "[fixes7][aspect]") {
    CHECK(Mods::NormalizeAspect("16:9") == "16:9");
    CHECK(Mods::NormalizeAspect(" 21 : 9 ") == "21:9");
    CHECK(Mods::NormalizeAspect("16:10") == "16:10");
    CHECK(Mods::NormalizeAspect("4:3") == "4:3");
    CHECK(Mods::NormalizeAspect("5:3") == "5:3");
    CHECK(Mods::NormalizeAspect("16x9").empty());
    CHECK(Mods::NormalizeAspect("3:2").empty());
    CHECK(Mods::NormalizeAspect("").empty());
    using Catch::Matchers::WithinAbs;
    CHECK_THAT(*Mods::AspectRatio("16:9"), WithinAbs(16.0 / 9.0, 1e-5));
    CHECK_THAT(*Mods::AspectRatio("21:9"), WithinAbs(21.0 / 9.0, 1e-5));
    CHECK_FALSE(Mods::AspectRatio("wide").has_value());
    CHECK_THAT(*Mods::FirstAspect({"", "wide", "21:9", "16:9"}), WithinAbs(21.0 / 9.0, 1e-5));
    CHECK_FALSE(Mods::FirstAspect({"", "x"}).has_value());
}

TEST_CASE("fixes7: recommended and aspect in the mod catalog", "[fixes7][recommended]") {
    const auto c = Mods::ParseCatalog(R"({"schema":1,"mods":[
 {"id":"a","name":"Zeta","title_ids":["0004000000033500"],"author":"x","downloads":50,
  "source":{"type":"github_release","repo":"o/a","asset_pattern":"*.zip"}},
 {"id":"b","name":"Alpha","title_ids":["0004000000033500"],"author":"x","downloads":10,"recommended":true,
  "aspect":"16:9","source":{"type":"github_release","repo":"o/b","asset_pattern":"*.zip"}},
 {"id":"c","name":"Beta","title_ids":["0004000000033500"],"author":"x","recommended":"yes","aspect":"wide",
  "source":{"type":"github_release","repo":"o/c","asset_pattern":"*.zip"}}
]})");
    REQUIRE(c.has_value());
    CHECK_FALSE(c->Find("a")->recommended);
    CHECK(c->Find("b")->recommended); // any source (verified is for the maintainer's own only)
    CHECK_FALSE(c->Find("b")->verified);
    CHECK(c->Find("b")->aspect == "16:9");
    CHECK_FALSE(c->Find("c")->recommended); // not a boolean
    CHECK(c->Find("c")->aspect.empty());    // not a shape this build knows: left out, entry kept

    Temp t;
    Mods::Library lib(t.load() + "/", *c, [](const std::string&, const std::string&, std::size_t) {
        Mods::HttpReply r;
        r.status = 404;
        return r;
    });
    // the recommended one first; the rest keep their order (most downloads, then by name)
    const auto list = lib.ListAvailable(kTid);
    REQUIRE(list.size() == 3);
    CHECK(list[0].id == "b");
    CHECK(list[0].recommended);
    CHECK(list[1].id == "a");
    CHECK(list[2].id == "c");
}

TEST_CASE("fixes7: recommended and aspect in the texture catalog", "[fixes7][recommended]") {
    std::vector<std::string> warnings;
    const auto c = CustomTex::Download::ParseCatalog(R"({"schema":1,"packs":[
      {"id":"a","name":"A","title":"G","title_ids":["0004000000033500"],"author":"x","downloads":900,
       "source":{"type":"https","url":"https://example.com/a.zip"}},
      {"id":"b","name":"B","title":"G","title_ids":["0004000000033500"],"author":"x","downloads":5,
       "recommended":true,"aspect":"21:9","source":{"type":"https","url":"https://example.com/b.zip"}},
      {"id":"c","name":"C","title":"G","title_ids":["0004000000033500"],"author":"x","downloads":800,
       "verified":true,"aspect":"16x9","source":{"type":"https","url":"https://example.com/c.zip"}}]})",
                                                     nullptr, &warnings);
    REQUIRE(c.has_value());
    REQUIRE(c->packs.size() == 3);
    CHECK(c->Find("b")->recommended);
    CHECK(c->Find("b")->aspect == "21:9");
    CHECK_FALSE(c->Find("c")->recommended); // verified is its own flag (AzaharFrame fixd: the chip)
    CHECK(c->Find("c")->aspect.empty());
    CHECK(warnings.size() == 1); // the bad aspect
    Temp t;
    CustomTex::Download::Downloader dl(t.load(), *c);
    const auto list = dl.ListForTitle(kTid);
    REQUIRE(list.size() == 3);
    // AzaharFrame fixd: verified first, then recommended, then by downloads as before
    CHECK(list[0].id == "c");
    CHECK(list[0].verified);
    CHECK(list[1].id == "b");
    CHECK(list[1].recommended);
    CHECK(list[2].id == "a");
    // the marker keeps the shape of an installed pack
    const fs::path folder = t.root / "pack";
    fs::create_directories(folder);
    REQUIRE(CustomTex::Download::WriteMarker(folder.string(), "b", "https", "", "21:9"));
    CHECK(CustomTex::Download::MarkerAspect(folder.string()) == "21:9");
    REQUIRE(CustomTex::Download::WriteMarker(folder.string(), "a", "https"));
    CHECK(CustomTex::Download::MarkerAspect(folder.string()).empty());
}
