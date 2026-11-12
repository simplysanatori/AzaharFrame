// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cpphelpers: see steam_cef.h.

#include "core/steam_cef.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <thread>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fmt/format.h>
#include <json.hpp>

extern char** environ;

namespace SteamCef {

namespace {

using json = nlohmann::json;

std::string Base64(const std::string& in) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < in.size(); i += 3) {
        const u32 n = static_cast<u8>(in[i]) << 16 | (i + 1 < in.size() ? static_cast<u8>(in[i + 1]) << 8 : 0) |
                      (i + 2 < in.size() ? static_cast<u8>(in[i + 2]) : 0);
        out.push_back(kAlphabet[n >> 18 & 63]);
        out.push_back(kAlphabet[n >> 12 & 63]);
        out.push_back(i + 1 < in.size() ? kAlphabet[n >> 6 & 63] : '=');
        out.push_back(i + 2 < in.size() ? kAlphabet[n & 63] : '=');
    }
    return out;
}

/// os.path.join(a, b)
std::string Join(const std::string& a, const std::string& b) {
    if (a.empty() || a.back() == '/') {
        return a + b;
    }
    return a + "/" + b;
}

std::string ReadFile(const std::string& path, bool* ok = nullptr) {
    std::ifstream in(path, std::ios::binary);
    if (ok) {
        *ok = static_cast<bool>(in);
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool IsFile(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

class Socket {
public:
    ~Socket() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
    bool Connect(int port, int timeout_s, std::string* error) {
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            *error = "no socket";
            return false;
        }
        timeval tv{timeout_s, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            *error = fmt::format("cannot connect to Steam's debugging port {}", port);
            return false;
        }
        return true;
    }
    bool SendAll(const std::string& data) {
        std::size_t done = 0;
        while (done < data.size()) {
            const ssize_t n = ::send(fd, data.data() + done, data.size() - done, MSG_NOSIGNAL);
            if (n <= 0) {
                return false;
            }
            done += static_cast<std::size_t>(n);
        }
        return true;
    }
    /// Reads up to `n` bytes (at least one); empty at end or timeout.
    std::string Recv(std::size_t n) {
        std::string buf(n, '\0');
        const ssize_t got = ::recv(fd, buf.data(), n, 0);
        buf.resize(got > 0 ? static_cast<std::size_t>(got) : 0);
        return buf;
    }
    bool RecvExact(std::size_t n, std::string& out) {
        out.clear();
        while (out.size() < n) {
            const std::string chunk = Recv(n - out.size());
            if (chunk.empty()) {
                return false;
            }
            out += chunk;
        }
        return true;
    }

private:
    int fd = -1;
};

std::string DechunkBody(const std::string& body) {
    std::string out;
    std::size_t at = 0;
    while (at < body.size()) {
        const auto eol = body.find("\r\n", at);
        if (eol == std::string::npos) {
            break;
        }
        const std::size_t size = std::strtoul(body.substr(at, eol - at).c_str(), nullptr, 16);
        if (size == 0 || eol + 2 + size > body.size()) {
            break;
        }
        out += body.substr(eol + 2, size);
        at = eol + 2 + size + 2;
    }
    return out;
}

/// GET path from Steam's debugging port; the body.
std::optional<std::string> HttpGet(int port, const std::string& path, int timeout_s,
                                   std::string* error) {
    Socket s;
    if (!s.Connect(port, timeout_s, error)) {
        return std::nullopt;
    }
    if (!s.SendAll(fmt::format("GET {} HTTP/1.1\r\nHost: 127.0.0.1:{}\r\nConnection: close\r\n\r\n",
                               path, port))) {
        *error = "request failed";
        return std::nullopt;
    }
    std::string all;
    while (true) {
        const std::string chunk = s.Recv(65536);
        if (chunk.empty()) {
            break;
        }
        all += chunk;
        if (all.size() > (16u << 20)) {
            break;
        }
    }
    const auto split = all.find("\r\n\r\n");
    if (split == std::string::npos || all.compare(0, 5, "HTTP/") != 0 ||
        all.substr(0, all.find("\r\n")).find(" 200") == std::string::npos) {
        *error = "no answer from Steam's debugging port";
        return std::nullopt;
    }
    std::string head = all.substr(0, split);
    std::transform(head.begin(), head.end(), head.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::string body = all.substr(split + 4);
    if (head.find("transfer-encoding: chunked") != std::string::npos) {
        body = DechunkBody(body);
    }
    return body;
}

} // namespace

std::string JsString(const std::string& text) {
    // ensure_ascii, as Python's json.dumps
    return json(text).dump(-1, ' ', true);
}

bool ValidAppId(const std::string& text) {
    return !text.empty() && text.size() <= 10 &&
           std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

int Port() {
    const char* env = std::getenv("AZAHARFRAME_CEF_PORT");
    return env != nullptr && *env != '\0' ? std::atoi(env) : 8080;
}

std::string WsTextFrame(const std::string& text, const unsigned char mask[4]) {
    std::string out;
    out.push_back(static_cast<char>(0x81));
    const std::size_t n = text.size();
    if (n < 126) {
        out.push_back(static_cast<char>(0x80 | n));
    } else if (n < 65536) {
        out.push_back(static_cast<char>(0x80 | 126));
        out.push_back(static_cast<char>(n >> 8));
        out.push_back(static_cast<char>(n));
    } else {
        out.push_back(static_cast<char>(0x80 | 127));
        for (int i = 7; i >= 0; --i) {
            out.push_back(static_cast<char>(static_cast<u64>(n) >> (8 * i)));
        }
    }
    out.append(reinterpret_cast<const char*>(mask), 4);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(static_cast<char>(static_cast<u8>(text[i]) ^ mask[i % 4]));
    }
    return out;
}

Reply Evaluate(const std::string& expression, int port, const std::string& title, int timeout_s) {
    Reply reply;
    std::string error;
    const auto pages_text = HttpGet(port, "/json", 5, &error);
    if (!pages_text) {
        reply.error = error;
        return reply;
    }
    const json pages = json::parse(*pages_text, nullptr, false);
    std::string ws_url;
    if (pages.is_array()) {
        for (const json& p : pages) {
            if (p.is_object() && p.value("title", std::string{}) == title) {
                ws_url = p.value("webSocketDebuggerUrl", std::string{});
                break;
            }
        }
    }
    if (ws_url.empty()) {
        reply.error = "no page titled " + title;
        return reply;
    }
    // ws://host:port/path
    static const std::regex kUrl(R"(^ws://([^/:]+):(\d+)/(.*)$)");
    std::smatch m;
    if (!std::regex_match(ws_url, m, kUrl)) {
        reply.error = "bad debugger URL";
        return reply;
    }
    const std::string host_port = m[1].str() + ":" + m[2].str();
    // Only Steam's own port on this machine
    if (m[1].str() != "127.0.0.1" && m[1].str() != "localhost") {
        reply.error = "the debugger is not on this machine";
        return reply;
    }
    Socket s;
    if (!s.Connect(std::atoi(m[2].str().c_str()), timeout_s, &error)) {
        reply.error = error;
        return reply;
    }
    unsigned char key_raw[16];
    FILE* rnd = std::fopen("/dev/urandom", "rb");
    if (rnd == nullptr || std::fread(key_raw, 1, 16, rnd) != 16) {
        reply.error = "no random numbers";
        if (rnd) {
            std::fclose(rnd);
        }
        return reply;
    }
    unsigned char mask[4];
    const bool got_mask = std::fread(mask, 1, 4, rnd) == 4;
    std::fclose(rnd);
    if (!got_mask) {
        reply.error = "no random numbers";
        return reply;
    }
    const std::string key = Base64(std::string(reinterpret_cast<char*>(key_raw), 16));
    if (!s.SendAll(fmt::format("GET /{} HTTP/1.1\r\nHost: {}\r\nUpgrade: websocket\r\nConnection: "
                               "Upgrade\r\nSec-WebSocket-Key: {}\r\nSec-WebSocket-Version: 13\r\n\r\n",
                               m[3].str(), host_port, key))) {
        reply.error = "handshake failed";
        return reply;
    }
    std::string buf;
    while (buf.find("\r\n\r\n") == std::string::npos) {
        const std::string chunk = s.Recv(4096);
        if (chunk.empty()) {
            reply.error = "handshake failed";
            return reply;
        }
        buf += chunk;
        if (buf.size() > 65536) {
            reply.error = "handshake failed";
            return reply;
        }
    }
    if (buf.substr(0, buf.find("\r\n")).find(" 101 ") == std::string::npos) {
        reply.error = "handshake rejected";
        return reply;
    }
    // Whatever followed the headers is the start of the first frame
    std::string pending = buf.substr(buf.find("\r\n\r\n") + 4);
    const json request{{"id", 1},
                       {"method", "Runtime.evaluate"},
                       {"params", {{"expression", expression}, {"awaitPromise", true}, {"returnByValue", true}}}};
    if (!s.SendAll(WsTextFrame(request.dump(-1, ' ', true), mask))) {
        reply.error = "connection closed";
        return reply;
    }
    const auto take = [&](std::size_t n, std::string& out) {
        while (pending.size() < n) {
            const std::string chunk = s.Recv(65536);
            if (chunk.empty()) {
                return false;
            }
            pending += chunk;
        }
        out = pending.substr(0, n);
        pending.erase(0, n);
        return true;
    };
    std::string message;
    while (true) {
        std::string h;
        if (!take(2, h)) {
            reply.error = "connection closed";
            return reply;
        }
        const u8 b1 = static_cast<u8>(h[0]), b2 = static_cast<u8>(h[1]);
        const int op = b1 & 0x0f;
        u64 n = b2 & 0x7f;
        std::string ext;
        if (n == 126) {
            if (!take(2, ext)) {
                reply.error = "connection closed";
                return reply;
            }
            n = static_cast<u8>(ext[0]) << 8 | static_cast<u8>(ext[1]);
        } else if (n == 127) {
            if (!take(8, ext)) {
                reply.error = "connection closed";
                return reply;
            }
            n = 0;
            for (const char c : ext) {
                n = n << 8 | static_cast<u8>(c);
            }
        }
        std::string maskkey;
        if ((b2 & 0x80) && !take(4, maskkey)) {
            reply.error = "connection closed";
            return reply;
        }
        if (n > (64u << 20)) {
            reply.error = "message too large";
            return reply;
        }
        std::string payload;
        if (!take(static_cast<std::size_t>(n), payload)) {
            reply.error = "connection closed";
            return reply;
        }
        if (!maskkey.empty()) {
            for (std::size_t i = 0; i < payload.size(); ++i) {
                payload[i] = static_cast<char>(static_cast<u8>(payload[i]) ^ static_cast<u8>(maskkey[i % 4]));
            }
        }
        if (op == 8) {
            reply.error = "closed";
            return reply;
        }
        if (op == 9) { // ping
            const std::string pong = std::string{static_cast<char>(0x8A), static_cast<char>(0x80)} +
                                     std::string(reinterpret_cast<const char*>(mask), 4);
            s.SendAll(pong);
            continue;
        }
        if (op == 1 || op == 0) {
            message += payload;
            if (!(b1 & 0x80)) {
                continue;
            }
            const json msg = json::parse(message, nullptr, false);
            message.clear();
            if (!msg.is_object() || msg.value("id", 0) != 1) {
                continue;
            }
            if (msg.contains("error")) {
                reply.error = msg["error"].dump(-1, ' ', true);
                return reply;
            }
            const json r = msg.value("result", json::object());
            if (r.contains("exceptionDetails")) {
                const json& d = r["exceptionDetails"];
                reply.error = (d.contains("exception") ? d["exception"] : d).dump(-1, ' ', true).substr(0, 2000);
                return reply;
            }
            const json inner = r.value("result", json::object());
            reply.ok = true;
            reply.json = inner.contains("value") ? inner["value"].dump(1, ' ', true) : "null";
            return reply;
        }
        // binary, pong: ignored
    }
}

std::vector<ArtCall> PlanArt(const std::string& appid, const std::string& dir,
                             std::vector<std::string>* notes) {
    std::vector<ArtCall> calls;
    const auto id = [&] { return ValidAppId(appid) ? std::strtoull(appid.c_str(), nullptr, 10) : 0ull; };
    struct Kind {
        const char* name;
        int kind;
    };
    for (const Kind k : {Kind{"portrait", 0}, Kind{"hero", 1}, Kind{"logo", 2}, Kind{"wide", 3}}) {
        const std::string path = Join(dir, std::string(k.name) + ".png");
        if (!IsFile(path)) {
            if (notes) {
                notes->push_back("no " + path + ", skipped");
            }
            continue;
        }
        ArtCall c;
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        c.dry_line = fmt::format(
            "[dry run] would call Steam (CEF): SteamClient.Apps.SetCustomArtworkForApp({}, <{}, {} bytes, "
            "base64>, 'png', {})",
            id(), path, ec ? 0 : size, k.kind);
        c.js = "\x01" + path + "\x01" + std::to_string(k.kind); // read and encoded when it is sent
        calls.push_back(std::move(c));
    }
    const std::string icon = Join(dir, "icon.png");
    if (IsFile(icon)) {
        ArtCall c;
        c.dry_line = fmt::format("[dry run] would call Steam (CEF): SteamClient.Apps.SetShortcutIcon({}, {})",
                                 appid, JsString(icon));
        c.js = ValidAppId(appid) ? fmt::format("SteamClient.Apps.SetShortcutIcon({}, {})",
                                               std::strtoull(appid.c_str(), nullptr, 10), JsString(icon))
                                 : std::string{};
        calls.push_back(std::move(c));
    } else if (notes) {
        notes->push_back("no " + icon + ", the icon is not set");
    }
    return calls;
}

int Art(const std::string& appid, const std::string& dir, bool dry_run) {
    std::vector<std::string> notes;
    const auto calls = PlanArt(appid, dir, &notes);
    for (const auto& n : notes) {
        std::fprintf(stderr, "%s\n", n.c_str());
    }
    if (dry_run) {
        for (const auto& c : calls) {
            std::printf("%s\n", c.dry_line.c_str());
        }
        return 0;
    }
    if (!ValidAppId(appid)) {
        std::fprintf(stderr, "the app id must be digits\n");
        return 1;
    }
    for (const auto& c : calls) {
        std::string js = c.js;
        if (!js.empty() && js[0] == '\x01') {
            const auto second = js.find('\x01', 1);
            const std::string path = js.substr(1, second - 1);
            bool ok = false;
            const std::string data = ReadFile(path, &ok);
            if (!ok) {
                std::fprintf(stderr, "cannot read %s\n", path.c_str());
                return 1;
            }
            js = fmt::format("SteamClient.Apps.SetCustomArtworkForApp({}, '{}', 'png', {})",
                             std::strtoull(appid.c_str(), nullptr, 10), Base64(data), js.substr(second + 1));
        }
        const Reply r = Evaluate(js, Port());
        if (!r.ok) {
            std::fprintf(stderr, "%s\n", r.error.c_str());
            return 1;
        }
    }
    return 0;
}

std::string FindShortcutJs(const std::string& name) {
    return "(() => { for (const a of (appStore.allApps || [])) { if (a.display_name === " + JsString(name) +
           " && a.app_type === 1073741824) return a.appid; } return 0; })()";
}

std::string AddShortcutJs(const std::string& name, const std::string& exe, const std::string& dir) {
    // AddShortcut puts the quotes around the program and folder itself
    return "(async () => { const id = await SteamClient.Apps.AddShortcut(" + JsString(name) + ", " +
           JsString(exe) + ", " + JsString(dir) + ", ''); SteamClient.Apps.SetShortcutName(id, " +
           JsString(name) + "); return id; })()";
}

std::string UpdateShortcutJs(const std::string& appid, const std::string& exe, const std::string& dir) {
    // SetShortcutExe and SetShortcutStartDir store the text as given, so those two get the quotes
    return "(() => { SteamClient.Apps.SetShortcutExe(" + appid + ", " + JsString("\"" + exe + "\"") +
           "); SteamClient.Apps.SetShortcutStartDir(" + appid + ", " + JsString("\"" + dir + "\"") +
           "); return 1; })()";
}

std::string RemoveShortcutJs(const std::string& appid) {
    return "(() => { SteamClient.Apps.RemoveShortcut(" + appid + "); return 1; })()";
}

// ---------------------------------------------------------------------------------------------
// --screen (vrcmd)

namespace {

/// vrcmd's output (stdout), "" when it cannot run or takes over 10 s. LD_PRELOAD (Steam's overlay
/// library) is not for a SteamVR tool.
std::string Vrcmd(const std::vector<std::string>& args) {
    std::string exe;
    if (const char* env = std::getenv("AZAHARFRAME_VRCMD"); env != nullptr && *env != '\0') {
        exe = env;
    } else {
        for (const char* p : {"/opt/steamvr/bin/linuxarm64/vrcmd", "/opt/steamvr/bin/linux64/vrcmd"}) {
            if (IsFile(p)) {
                exe = p;
                break;
            }
        }
    }
    if (exe.empty()) {
        std::fprintf(stderr, "no vrcmd\n");
        return {};
    }
    int fds[2];
    if (pipe(fds) != 0) {
        return {};
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return {};
    }
    if (pid == 0) {
        dup2(fds[1], 1);
        const int null = open("/dev/null", O_WRONLY);
        if (null >= 0) {
            dup2(null, 2);
        }
        close(fds[0]);
        close(fds[1]);
        unsetenv("LD_PRELOAD");
        std::vector<char*> argv{const_cast<char*>(exe.c_str())};
        for (const auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);
        execv(exe.c_str(), argv.data());
        _exit(127);
    }
    close(fds[1]);
    std::string out;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool timed_out = false;
    while (true) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                              deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) {
            timed_out = true;
            break;
        }
        pollfd p{fds[0], POLLIN, 0};
        const int r = poll(&p, 1, static_cast<int>(left));
        if (r <= 0) {
            timed_out = r == 0;
            if (r == 0) {
                break;
            }
            continue;
        }
        char buf[4096];
        const ssize_t n = read(fds[0], buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        out.append(buf, static_cast<std::size_t>(n));
    }
    close(fds[0]);
    if (timed_out) {
        kill(pid, SIGKILL);
        out.clear();
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return out;
}

} // namespace

OverlayState ParseOverlayLine(const std::string& line) {
    OverlayState s;
    static const std::regex kSize(R"( size=(\d+)x(\d+) )");
    static const std::regex kFlags(R"( flags=0x([0-9a-fA-F]+))");
    static const std::regex kVisible(R"( (visible|not_visible) VROverlayType)");
    std::smatch m;
    if (std::regex_search(line, m, kSize)) {
        s.picture = std::atoll(m[1].str().c_str()) > 0 && std::atoll(m[2].str().c_str()) > 0;
    }
    if (std::regex_search(line, m, kFlags)) {
        s.in_dashboard = (std::strtoull(m[1].str().c_str(), nullptr, 16) & 0x8000) != 0;
    }
    if (std::regex_search(line, m, kVisible)) {
        s.visible = m[1].str() == "visible";
    }
    return s;
}

std::optional<std::string> FindOverlayLine(const std::string& listing, const std::string& key) {
    const std::string prefix = "'" + key + "' ";
    std::size_t at = 0;
    while (at <= listing.size()) {
        auto end = listing.find('\n', at);
        if (end == std::string::npos) {
            end = listing.size();
        }
        std::string line = listing.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.compare(0, prefix.size(), prefix) == 0) {
            return line;
        }
        at = end + 1;
    }
    return std::nullopt;
}

namespace {

std::optional<std::string> OverlayLine(const std::string& key) {
    return FindOverlayLine(Vrcmd({"--overlays"}), key);
}

std::string Describe(const std::optional<std::string>& line) {
    if (!line) {
        return "missing";
    }
    const OverlayState s = ParseOverlayLine(*line);
    return fmt::format("{}, {}, {}", s.picture ? "picture" : "no picture",
                       s.in_dashboard ? "in the dashboard" : "not in the dashboard",
                       s.visible ? "visible" : "not visible");
}

bool Showable(const std::optional<std::string>& line) {
    if (!line) {
        return false;
    }
    const OverlayState s = ParseOverlayLine(*line);
    return s.picture && s.in_dashboard;
}

void HideDashboard() {
    const Reply r = Evaluate("SteamClient.OpenVR.VROverlay.HideDashboard()", Port());
    if (!r.ok) {
        std::fprintf(stderr, "HideDashboard failed: %s\n", r.error.c_str());
        Vrcmd({"--hidedashboard"});
    }
}

} // namespace

int Screen(const std::string& where, const std::string& appid, double seconds, bool after_vr) {
    if (!ValidAppId(appid)) {
        std::fprintf(stderr, "the app id must be digits\n");
        return 1;
    }
    const std::string key = "valve.steam.desktopgame." + std::to_string(std::strtoull(appid.c_str(), nullptr, 10));
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(static_cast<long long>(seconds * 1000));
    auto line = OverlayLine(key);
    if (where != "theater") {
        if (!line) {
            std::printf("dashboard: no overlay %s, nothing moved\n", key.c_str());
            return 1;
        }
        Vrcmd({"--dock-overlay", "dashboard", key});
        HideDashboard();
        std::printf("dashboard: docked %s, dashboard hidden\n", key.c_str());
        return 0;
    }
    while (!Showable(line)) {
        if (std::chrono::steady_clock::now() >= deadline) {
            std::printf("theater: overlay %s %s, nothing moved\n", key.c_str(), Describe(line).c_str());
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        line = OverlayLine(key);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(750)); // the dashboard makes the frame a moment after
    // The dashboard is never opened here (it takes the controller).
    Vrcmd({"--dock-overlay", "theater", key});
    line = OverlayLine(key);
    if (after_vr && !(line && ParseOverlayLine(*line).visible)) {
        std::fprintf(stderr, "theater: overlay %s %s after the move, once more\n", key.c_str(),
                     Describe(line).c_str());
        Vrcmd({"--dock-overlay", "world", key});
        Vrcmd({"--dock-overlay", "theater", key});
        line = OverlayLine(key);
    }
    std::printf("theater: docked %s (overlay %s)\n", key.c_str(), Describe(line).c_str());
    return (!line || ParseOverlayLine(*line).visible || !after_vr) ? 0 : 2;
}

// ---------------------------------------------------------------------------------------------

int RunCli(const std::vector<std::string>& raw) {
    std::vector<std::string> args;
    bool after_vr = false;
    for (const auto& a : raw) {
        if (a == "--after-vr") {
            after_vr = true;
        } else {
            args.push_back(a);
        }
    }
    if ((args.size() == 3 || args.size() == 4) && args[0] == "--screen" &&
        (args[1] == "theater" || args[1] == "dashboard")) {
        return Screen(args[1], args[2], args.size() == 4 ? std::atof(args[3].c_str()) : 8.0, after_vr);
    }
    if ((raw.size() == 3 || raw.size() == 4) && raw[0] == "--art") {
        return Art(raw[1], raw[2], raw.size() == 4 && raw[3] == "--dry-run");
    }
    if (raw.size() == 1) {
        const Reply r = Evaluate(raw[0], Port());
        if (!r.ok) {
            std::fprintf(stderr, "%s\n", r.error.c_str());
            return 1;
        }
        std::printf("%s\n", r.json.c_str());
        return 0;
    }
    std::fprintf(stderr, "usage: --frame-cef 'JS expression' | --art APPID DIR [--dry-run] | --screen "
                         "theater|dashboard APPID [SECONDS] [--after-vr]\n");
    return 2;
}

int RunShortcutCli(const std::vector<std::string>& raw) {
    std::vector<std::string> a;
    bool dry = false;
    for (const auto& s : raw) {
        if (s == "--dry-run") {
            dry = true;
        } else {
            a.push_back(s);
        }
    }
    const auto run = [&](const std::string& js) -> std::optional<std::string> {
        if (dry) {
            std::printf("[dry run] would call Steam (CEF): %s\n", js.c_str());
            return std::string("null");
        }
        const Reply r = Evaluate(js, Port());
        if (!r.ok) {
            std::fprintf(stderr, "%s\n", r.error.c_str());
            return std::nullopt;
        }
        return r.json;
    };
    const auto bad = [] {
        std::fprintf(stderr, "usage: --frame-steam-shortcut find NAME | add NAME EXE DIR | update APPID EXE "
                             "DIR | remove APPID | remove-named NAME [--dry-run]\n");
        return 2;
    };
    if (a.empty()) {
        return bad();
    }
    if (a[0] == "find" && a.size() == 2) {
        const auto r = run(FindShortcutJs(a[1]));
        if (!r) {
            return 1;
        }
        if (!dry) {
            std::printf("%s\n", r->c_str());
        }
        return 0;
    }
    if (a[0] == "add" && a.size() == 4) {
        const auto r = run(AddShortcutJs(a[1], a[2], a[3]));
        if (!r) {
            return 1;
        }
        if (!dry) {
            std::printf("%s\n", r->c_str());
        }
        return 0;
    }
    if (a[0] == "update" && a.size() == 4) {
        if (!ValidAppId(a[1])) {
            std::fprintf(stderr, "the app id must be digits\n");
            return 1;
        }
        return run(UpdateShortcutJs(a[1], a[2], a[3])) ? 0 : 1;
    }
    if (a[0] == "remove" && a.size() == 2) {
        if (!ValidAppId(a[1])) {
            std::fprintf(stderr, "the app id must be digits\n");
            return 1;
        }
        return run(RemoveShortcutJs(a[1])) ? 0 : 1;
    }
    if (a[0] == "remove-named" && a.size() == 2) {
        const auto found = run(FindShortcutJs(a[1]));
        if (!found) {
            return 1;
        }
        const std::string id = dry ? "0" : *found;
        if (!ValidAppId(id) || id == "0") {
            std::printf("Steam has no '%s' entry\n", a[1].c_str());
            return 0;
        }
        if (!run(RemoveShortcutJs(id))) {
            return 1;
        }
        std::printf("removed the '%s' entry (appid %s)\n", a[1].c_str(), id.c_str());
        return 0;
    }
    return bad();
}

} // namespace SteamCef
