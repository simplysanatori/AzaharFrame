// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame crashreport: reading, classifying and writing up the last run (frame_crash_report.h).

#include "core/frame_crash_report.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fmt/format.h>

#include "common/settings.h"

#if defined(__linux__)
#include <pwd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace Core::CrashReport {

namespace {

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

u64 ParseU64(std::string_view s) {
    const std::string str(s);
    return std::strtoull(str.c_str(), nullptr, 0);
}

long long ParseLL(std::string_view s) {
    const std::string str(s);
    return std::strtoll(str.c_str(), nullptr, 10);
}

template <typename F>
void ForLines(std::string_view text, F&& f) {
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        f(text.substr(0, nl));
        if (nl == std::string_view::npos) {
            break;
        }
        text.remove_prefix(nl + 1);
    }
}

std::string Hex(u64 v) {
    return fmt::format("0x{:016x}", v);
}

std::string Basename(std::string_view path) {
    const std::size_t slash = path.rfind('/');
    return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

std::string LocalTime(long long t, const char* format) {
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), format, &tm);
    return buf;
}

// "(symbol+0x1c)" out of a backtrace_symbols line "path(symbol+0x1c) [0x...]".
std::string SymbolOf(std::string_view line) {
    const std::size_t open = line.find('(');
    const std::size_t close = line.find(')', open == std::string_view::npos ? 0 : open);
    if (open == std::string_view::npos || close == std::string_view::npos || close <= open + 1) {
        return {};
    }
    std::string sym(line.substr(open + 1, close - open - 1));
    if (!sym.empty() && sym[0] == '+') {
        return {}; // an offset only: no name
    }
    return sym;
}

} // namespace

// ---- Network addresses --------------------------------------------------------------------

namespace {
bool IsHex(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}
bool IsWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}
bool IsIpv4(std::string_view s) {
    int parts = 0;
    std::size_t at = 0;
    while (true) {
        const std::size_t dot = s.find('.', at);
        const std::string_view part = s.substr(at, dot == std::string_view::npos ? s.npos : dot - at);
        if (part.empty() || part.size() > 3 ||
            !std::all_of(part.begin(), part.end(), [](char c) { return c >= '0' && c <= '9'; }) ||
            std::atoi(std::string(part).c_str()) > 255) {
            return false;
        }
        parts++;
        if (dot == std::string_view::npos) {
            break;
        }
        at = dot + 1;
    }
    return parts == 4;
}
bool IsMac(std::string_view s) {
    if (s.size() != 17) {
        return false;
    }
    const char sep = s[2];
    if (sep != ':' && sep != '-') {
        return false;
    }
    for (std::size_t i = 0; i < 17; i++) {
        if (i % 3 == 2 ? s[i] != sep : !IsHex(s[i])) {
            return false;
        }
    }
    return true;
}
bool IsIpv6(std::string_view s) {
    const auto colons = std::count(s.begin(), s.end(), ':');
    if (colons < 2 || colons > 7 || (colons != 7 && s.find("::") == std::string_view::npos) ||
        !std::any_of(s.begin(), s.end(), IsHex)) {
        return false;
    }
    std::size_t group = 0;
    for (char c : s) {
        if (c == ':') {
            group = 0;
        } else if (!IsHex(c) || ++group > 4) {
            return false;
        }
    }
    return true;
}
} // namespace

std::string ScrubNetwork(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const auto in_run = [](char c) { return IsHex(c) || c == '.' || c == ':' || c == '-'; };
        if (!in_run(text[i]) || (i > 0 && (IsWordChar(text[i - 1]) || text[i - 1] == '.'))) {
            out.push_back(text[i++]);
            continue;
        }
        std::size_t j = i;
        while (j < text.size() && in_run(text[j])) {
            j++;
        }
        // trailing punctuation ends a sentence, not an address
        while (j > i && (text[j - 1] == '.' || text[j - 1] == ':' || text[j - 1] == '-')) {
            j--;
        }
        if (j == i || (j < text.size() && IsWordChar(text[j]))) {
            out.append(text.substr(i, std::max<std::size_t>(j - i, 1)));
            i += std::max<std::size_t>(j - i, 1);
            continue;
        }
        const std::string_view run = text.substr(i, j - i);
        if (IsMac(run)) {
            out += "<mac>";
        } else if (IsIpv4(run)) {
            out += "<ip>";
        } else if (const std::size_t colon = run.rfind(':');
                   colon != std::string_view::npos && IsIpv4(run.substr(0, colon))) {
            out += "<ip>"; // address:port
            out.append(run.substr(colon));
        } else if (IsIpv6(run)) {
            out += "<ip>";
        } else {
            out.append(run);
        }
        i = j;
    }
    return out;
}

// ---- Reading the last run -----------------------------------------------------------------

void ParseMarker(std::string_view text, LastRun& run) {
    run.marker = true;
    ForLines(text, [&](std::string_view line) {
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            return;
        }
        const std::string_view key = line.substr(0, eq);
        const std::string_view value = line.substr(eq + 1);
        if (key == "pid") {
            run.pid = static_cast<int>(ParseLL(value));
        } else if (key == "started") {
            run.started = ParseLL(value);
        } else if (key == "version") {
            run.version = value;
        } else if (key == "release") {
            run.release = value;
        } else if (key == "mode") {
            run.vr = value != "flat";
        } else if (key == "title_id") {
            run.title_id = std::strtoull(std::string(value).c_str(), nullptr, 16);
        } else if (key == "title") {
            run.title = value;
        } else if (key == "cause") {
            run.cause = Common::FrameCrash::CauseFromKey(value);
        } else if (key == "detail") {
            run.detail = value;
        } else if (key == "exe") {
            run.exe = value;
        } else if (key == "exe_build_id") {
            run.exe_build_id = value;
        } else if (key == "setting") {
            run.settings.emplace_back(value);
        }
    });
}

void ParseCrash(std::string_view text, LastRun& run) {
    run.crash = true;
    enum class Part { Keys, Symbols, Maps } part = Part::Keys;
    std::string maps;
    ForLines(text, [&](std::string_view line) {
        if (part == Part::Symbols) {
            if (line == "symbols-end") {
                part = Part::Keys;
            } else {
                run.symbols.emplace_back(line);
            }
            return;
        }
        if (part == Part::Maps) {
            if (line == "maps-end") {
                part = Part::Keys;
            } else {
                maps.append(line).push_back('\n');
            }
            return;
        }
        if (line == "symbols-begin") {
            part = Part::Symbols;
            return;
        }
        if (line == "maps-begin") {
            part = Part::Maps;
            return;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            return;
        }
        const std::string_view key = line.substr(0, eq);
        const std::string_view value = line.substr(eq + 1);
        if (key == "signal") {
            run.signal = static_cast<int>(ParseLL(value));
        } else if (key == "code") {
            run.code = static_cast<int>(ParseLL(value));
        } else if (key == "addr") {
            run.addr = ParseU64(value);
        } else if (key == "thread") {
            run.thread = value;
        } else if (key == "exception") {
            run.exception = value;
        } else if (key == "pc") {
            run.pc = ParseU64(value);
        } else if (key == "lr") {
            run.lr = ParseU64(value);
        } else if (key == "frame") {
            run.frames.push_back(ParseU64(value));
        } else if (key == "cause") {
            // the signal handler's copy: the marker's may be older or missing
            run.cause = Common::FrameCrash::CauseFromKey(value);
        } else if (key == "detail") {
            run.detail = value;
        }
    });
    run.maps = std::move(maps);
}

bool PidIsAzahar(int pid) {
#if defined(__linux__)
    if (pid <= 0 || pid == static_cast<int>(getpid())) {
        return false;
    }
    if (kill(pid, 0) != 0) {
        return false;
    }
    std::string comm = ReadFile(fmt::format("/proc/{}/comm", pid));
    while (!comm.empty() && (comm.back() == '\n' || comm.back() == ' ')) {
        comm.pop_back();
    }
    return comm == "azahar";
#else
    return false;
#endif
}

std::optional<LastRun> TakeLastRun(const std::string& dir,
                                   const std::function<bool(int)>& alive) {
    const std::string marker_path = dir + Common::FrameCrash::kRunningFile;
    const std::string crash_path = dir + Common::FrameCrash::kCrashFile;
    std::error_code ec;
    const bool has_marker = std::filesystem::exists(marker_path, ec);
    const bool has_crash = std::filesystem::exists(crash_path, ec);
    if (!has_marker && !has_crash) {
        return std::nullopt;
    }
    LastRun run;
    if (has_marker) {
        ParseMarker(ReadFile(marker_path), run);
        if (alive && alive(run.pid)) {
            return std::nullopt; // another copy runs: its marker, not a crash
        }
    }
    if (has_crash) {
        ParseCrash(ReadFile(crash_path), run);
    }
    std::filesystem::remove(marker_path, ec);
    std::filesystem::remove(crash_path, ec);
    return run;
}

Kind Classify(const LastRun& run, bool out_of_memory) {
    using Common::FrameCrash::Cause;
    if (out_of_memory) {
        return Kind::OutOfMemory;
    }
    switch (run.cause) {
    case Cause::DeviceLost:
        return Kind::DeviceLost;
    case Cause::CoreFatal:
    case Cause::ArmException:
        return Kind::GameFatal;
    case Cause::Stuck:
    case Cause::None:
        break;
    }
    if (run.crash && run.signal != 0) {
        return Kind::Crashed;
    }
    return Kind::Unexpected;
}

std::string ReasonText(Kind kind) {
    switch (kind) {
    case Kind::OutOfMemory:
        return "Ran Out of Memory";
    case Kind::DeviceLost:
        return "Graphics Driver Stopped Responding";
    case Kind::GameFatal:
        return "The Game Hit a Fatal Error";
    case Kind::Crashed:
        return "Azahar Crashed";
    case Kind::Manual:
        return {}; // the player says what happened
    case Kind::Unexpected:
        break;
    }
    return "Closed Unexpectedly";
}

std::string_view KindKey(Kind kind) {
    switch (kind) {
    case Kind::OutOfMemory:
        return "out-of-memory";
    case Kind::DeviceLost:
        return "device-lost";
    case Kind::GameFatal:
        return "game-fatal";
    case Kind::Crashed:
        return "crashed";
    case Kind::Manual:
        return "manual";
    case Kind::Unexpected:
        break;
    }
    return "unexpected";
}

std::string SignalName(int signal) {
    switch (signal) {
    case 4:
        return "SIGILL";
    case 5:
        return "SIGTRAP";
    case 6:
        return "SIGABRT";
    case 7:
        return "SIGBUS";
    case 8:
        return "SIGFPE";
    case 9:
        return "SIGKILL";
    case 11:
        return "SIGSEGV";
    case 15:
        return "SIGTERM";
    default:
        return fmt::format("signal {}", signal);
    }
}

// ---- Scrubbing ----------------------------------------------------------------------------

namespace {
bool WordChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

// Every `from` as a whole word (no letter, digit or _ either side) becomes `to`.
std::string ReplaceWord(std::string_view text, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return std::string(text);
    }
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t at = text.find(from, i);
        if (at == std::string_view::npos) {
            out.append(text.substr(i));
            break;
        }
        const bool left = at == 0 || !WordChar(text[at - 1]);
        const std::size_t after = at + from.size();
        const bool right = after >= text.size() || !WordChar(text[after]);
        out.append(text.substr(i, at - i));
        if (left && right) {
            out.append(to);
        } else {
            out.append(from);
        }
        i = after;
    }
    return out;
}

std::string ReplaceAll(std::string_view text, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return std::string(text);
    }
    std::string out;
    std::size_t i = 0;
    while (true) {
        const std::size_t at = text.find(from, i);
        if (at == std::string_view::npos) {
            out.append(text.substr(i));
            return out;
        }
        out.append(text.substr(i, at - i)).append(to);
        i = at + from.size();
    }
}
} // namespace

std::string Scrubber::Apply(std::string_view text) const {
    std::string out(text);
    std::string h = home;
    while (h.size() > 1 && h.back() == '/') {
        h.pop_back();
    }
    if (h.size() > 1) {
        // the home folder first, so "/home/player/x" is "~/x" and not "/home/<user>/x"
        out = ReplaceAll(out, h, "~");
    }
    // the host first: its name often holds the user's ("player-frame")
    if (!host.empty() && host != "steamos" && host != "localhost" && host.size() >= 2) {
        out = ReplaceWord(out, host, "<host>");
    }
    if (!user.empty() && user != "steamos" && user != "deck" && user != "root" &&
        user.size() >= 2) {
        out = ReplaceWord(out, user, "<user>");
    }
    return out;
}

Scrubber Scrubber::ForThisMachine() {
    Scrubber s;
    if (const char* h = std::getenv("HOME")) {
        s.home = h;
    }
#if defined(__linux__)
    if (const passwd* pw = getpwuid(getuid())) {
        s.user = pw->pw_name ? pw->pw_name : "";
        if (s.home.empty() && pw->pw_dir) {
            s.home = pw->pw_dir;
        }
    }
    char host[256] = {};
    if (gethostname(host, sizeof(host) - 1) == 0) {
        s.host = host;
    }
#endif
    return s;
}

// ---- Modules ------------------------------------------------------------------------------

std::vector<Module> ParseMaps(std::string_view maps, std::vector<MapRange>* ranges) {
    std::vector<Module> mods;
    ForLines(maps, [&](std::string_view line) {
        // start-end perms offset dev inode path
        std::istringstream s{std::string(line)};
        std::string range, perms, offset, dev, inode, path;
        if (!(s >> range >> perms >> offset >> dev >> inode)) {
            return;
        }
        std::getline(s, path);
        const std::size_t first = path.find_first_not_of(' ');
        if (first == std::string::npos) {
            return;
        }
        path = path.substr(first);
        // a file replaced while it ran (an update): its name still is the module's
        constexpr std::string_view deleted = " (deleted)";
        if (path.size() > deleted.size() &&
            path.compare(path.size() - deleted.size(), deleted.size(), deleted) == 0) {
            path.resize(path.size() - deleted.size());
        }
        // files only: not anonymous memory, [stack], [vdso], a GPU buffer (/dev/dri: reading it
        // blocks) or a memfd
        if (path.empty() || path[0] != '/' || path.rfind("/dev/", 0) == 0 ||
            path.rfind("/memfd:", 0) == 0 || inode == "0") {
            return;
        }
        const std::size_t dash = range.find('-');
        if (dash == std::string::npos) {
            return;
        }
        const u64 start = std::strtoull(range.substr(0, dash).c_str(), nullptr, 16);
        const u64 end = std::strtoull(range.substr(dash + 1).c_str(), nullptr, 16);
        const u64 off = std::strtoull(offset.c_str(), nullptr, 16);
        auto it = std::find_if(mods.begin(), mods.end(),
                               [&](const Module& m) { return m.path == path; });
        if (it == mods.end()) {
            if (off != 0) {
                return; // a module's first mapping is its ELF header, at offset 0
            }
            Module m;
            m.path = path;
            m.base = start;
            m.end = end;
            mods.push_back(std::move(m));
            it = mods.end() - 1;
        } else {
            it->end = std::max(it->end, end);
        }
        if (ranges) {
            ranges->push_back({start, end, static_cast<std::size_t>(it - mods.begin())});
        }
    });
    return mods;
}

bool IsRegularFile(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

std::string ReadBuildId(const std::string& path) {
    if (!IsRegularFile(path)) {
        return {}; // never a device: a read could block
    }
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }
    unsigned char ident[64] = {};
    f.read(reinterpret_cast<char*>(ident), 64);
    if (f.gcount() < 64 || std::memcmp(ident, "\x7f" "ELF", 4) != 0 || ident[4] != 2 ||
        ident[5] != 1) {
        return {}; // ELF64, little-endian only
    }
    const auto u16_at = [&](int o) { return static_cast<u16>(ident[o] | (ident[o + 1] << 8)); };
    u64 phoff = 0;
    std::memcpy(&phoff, ident + 32, 8);
    const u16 phentsize = u16_at(54);
    const u16 phnum = u16_at(56);
    for (u16 i = 0; i < phnum && i < 128; i++) {
        unsigned char ph[56] = {};
        f.seekg(static_cast<std::streamoff>(phoff + u64{i} * phentsize));
        f.read(reinterpret_cast<char*>(ph), 56);
        if (f.gcount() < 56) {
            return {};
        }
        u32 type = 0;
        std::memcpy(&type, ph, 4);
        if (type != 4) { // PT_NOTE
            continue;
        }
        u64 off = 0, size = 0;
        std::memcpy(&off, ph + 8, 8);
        std::memcpy(&size, ph + 32, 8);
        if (size > 1 << 16) {
            continue;
        }
        std::vector<unsigned char> notes(size);
        f.seekg(static_cast<std::streamoff>(off));
        f.read(reinterpret_cast<char*>(notes.data()), static_cast<std::streamsize>(size));
        std::size_t p = 0;
        while (p + 12 <= notes.size()) {
            u32 namesz = 0, descsz = 0, ntype = 0;
            std::memcpy(&namesz, &notes[p], 4);
            std::memcpy(&descsz, &notes[p + 4], 4);
            std::memcpy(&ntype, &notes[p + 8], 4);
            const std::size_t name_at = p + 12;
            const std::size_t desc_at = name_at + ((namesz + 3) & ~3u);
            if (desc_at + descsz > notes.size()) {
                break;
            }
            if (ntype == 3 && namesz == 4 && std::memcmp(&notes[name_at], "GNU", 4) == 0) {
                std::string hex;
                for (u32 b = 0; b < descsz; b++) {
                    hex += fmt::format("{:02x}", notes[desc_at + b]);
                }
                return hex;
            }
            p = desc_at + ((descsz + 3) & ~3u);
        }
    }
    return {};
}

std::string DebugId(std::string_view build_id_hex) {
    std::array<unsigned char, 16> b{};
    for (std::size_t i = 0; i < 16 && i * 2 + 1 < build_id_hex.size(); i++) {
        b[i] = static_cast<unsigned char>(
            std::strtoul(std::string(build_id_hex.substr(i * 2, 2)).c_str(), nullptr, 16));
    }
    std::swap(b[0], b[3]);
    std::swap(b[1], b[2]);
    std::swap(b[4], b[5]);
    std::swap(b[6], b[7]);
    std::string s;
    for (std::size_t i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            s += '-';
        }
        s += fmt::format("{:02x}", b[i]);
    }
    return s;
}

// ---- The log ------------------------------------------------------------------------------

LogRead ReadLog(const std::string& path, std::size_t lines) {
    LogRead out;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return out;
    }
    std::deque<std::string> tail;
    std::string line;
    const auto after = [](const std::string& l, std::string_view key) -> std::string {
        const std::size_t at = l.find(key);
        if (at == std::string::npos) {
            return {};
        }
        std::string v = l.substr(at + key.size());
        const std::size_t first = v.find_first_not_of(' ');
        return first == std::string::npos ? std::string() : v.substr(first);
    };
    while (std::getline(f, line)) {
        if (out.gpu.empty()) {
            out.gpu = after(line, "VK_DEVICE:");
        }
        if (out.driver.empty()) {
            out.driver = after(line, "VK_DRIVER:");
        }
        tail.push_back(std::move(line));
        if (tail.size() > lines) {
            tail.pop_front();
        }
    }
    out.lines = tail.size();
    for (const std::string& l : tail) {
        out.tail.append(l).push_back('\n');
    }
    return out;
}

std::string OsText() {
    std::string name, version, build;
    ForLines(ReadFile("/etc/os-release"), [&](std::string_view line) {
        const auto value = [&](std::string_view key) -> std::string {
            std::string v(line.substr(key.size()));
            v.erase(std::remove(v.begin(), v.end(), '"'), v.end());
            return v;
        };
        if (line.rfind("NAME=", 0) == 0) {
            name = value("NAME=");
        } else if (line.rfind("VERSION_ID=", 0) == 0) {
            version = value("VERSION_ID=");
        } else if (line.rfind("BUILD_ID=", 0) == 0) {
            build = value("BUILD_ID=");
        }
    });
    std::string s = name.empty() ? std::string("Linux") : name;
    if (!version.empty()) {
        s += " " + version;
    }
    if (!build.empty()) {
        s += " build " + build; // SteamOS: "SteamOS 0.5.8 build 20261106.6144341"
    }
#if defined(__linux__)
    utsname u{};
    if (uname(&u) == 0) {
        s += fmt::format(" (Linux {}, {})", u.release, u.machine);
    }
#endif
    return s;
}

// ---- The report ---------------------------------------------------------------------------

Report Build(const LastRun& run, bool out_of_memory, const Context& ctx) {
    const Scrubber& sc = ctx.scrub;
    Report r;
    r.kind = Classify(run, out_of_memory);
    r.reason = ReasonText(r.kind);
    r.headline = "Azahar Closed Unexpectedly";
    r.detail = sc.Apply(run.detail);
    if (run.cause == Common::FrameCrash::Cause::Stuck && r.detail.empty()) {
        r.detail = "The game didn't stop when asked";
    }
    r.title_id = run.title_id;
    r.title = sc.Apply(run.title);
    r.vr = run.vr;
    r.started = run.started;
    r.version = run.version;
    r.release = run.release;
    r.os = ctx.os;
    r.settings = run.settings.empty() ? ctx.settings_now : run.settings;
    for (std::string& s : r.settings) {
        s = sc.Apply(s);
    }
    r.clues = ctx.clues;
    for (std::string& s : r.clues) {
        s = sc.Apply(s);
    }
    const LogRead log = ReadLog(ctx.log_path, ctx.log_lines);
    r.log_tail = ScrubNetwork(sc.Apply(log.tail));
    r.log_lines = log.lines;
    r.gpu = log.gpu;
    r.driver = log.driver;
    if (run.crash && run.signal != 0) {
        r.crash_line = fmt::format("{} ({}), code {}, address {}", SignalName(run.signal),
                                   run.signal, run.code, Hex(run.addr));
        if (!run.thread.empty()) {
            r.crash_line += ", thread " + sc.Apply(run.thread);
        }
        r.exception = sc.Apply(run.exception);
        std::vector<MapRange> ranges;
        std::vector<Module> all = ParseMaps(run.maps, &ranges);
        std::vector<u64> addrs = run.frames;
        std::vector<std::string> symbols = run.symbols;
        if (addrs.empty() && run.pc != 0) {
            addrs.push_back(run.pc);
        }
        // The handler's own frames and the signal trampoline go: the stack starts where the
        // signal came (the faulting pc, else the frame after __kernel_rt_sigreturn).
        std::size_t first = 0;
        bool found = false;
        for (std::size_t i = 0; i < addrs.size() && run.pc != 0; i++) {
            if (addrs[i] == run.pc) {
                first = i;
                found = true;
                break;
            }
        }
        for (std::size_t i = 0; i < run.symbols.size() && !found && i < addrs.size(); i++) {
            if (run.symbols[i].find("__kernel_rt_sigreturn") != std::string::npos) {
                first = i + 1;
                found = true;
            }
        }
        if (found && run.pc != 0 && (first >= addrs.size() || addrs[first] != run.pc)) {
            addrs.insert(addrs.begin() + static_cast<std::ptrdiff_t>(first), run.pc);
            std::vector<std::string> syms = run.symbols;
            syms.insert(syms.begin() + static_cast<std::ptrdiff_t>(std::min(first, syms.size())),
                        std::string());
            symbols = std::move(syms);
        }
        for (std::size_t i = first; i < addrs.size(); i++) {
            Frame fr;
            fr.addr = addrs[i];
            if (i < symbols.size()) {
                fr.symbol = sc.Apply(SymbolOf(symbols[i]));
            }
            for (const MapRange& range : ranges) {
                const std::size_t m = range.module;
                if (fr.addr >= range.start && fr.addr < range.end) {
                    auto it = std::find_if(r.modules.begin(), r.modules.end(),
                                           [&](const Module& x) { return x.path == all[m].path; });
                    if (it == r.modules.end()) {
                        Module mod = all[m];
                        // the program's id from when it ran: its file may have been replaced
                        mod.build_id = !run.exe_build_id.empty() && mod.path == run.exe
                                           ? run.exe_build_id
                                           : ReadBuildId(mod.path);
                        r.modules.push_back(std::move(mod));
                        it = r.modules.end() - 1;
                    }
                    fr.module = static_cast<int>(it - r.modules.begin());
                    fr.offset = fr.addr - it->base;
                    break;
                }
            }
            r.frames.push_back(std::move(fr));
        }
        for (Module& m : r.modules) {
            m.path = sc.Apply(m.path);
        }
    }
    return r;
}

Report BuildManual(const LastRun& now, const Context& ctx) {
    LastRun run = now;
    run.crash = false; // no stack, no modules: nothing crashed
    run.cause = Common::FrameCrash::Cause::None;
    run.detail.clear();
    Report r = Build(run, false, ctx);
    r.kind = Kind::Manual;
    r.headline = "Report a Problem";
    r.reason.clear();
    r.detail.clear();
    return r;
}

std::string ReportText(const Report& r) {
    const bool manual = r.kind == Kind::Manual;
    std::string s = manual ? "Azahar for Frame Problem Report\n\n" : "Azahar for Frame Crash Report\n\n";
    const auto row = [&](std::string_view label, std::string_view value) {
        if (!value.empty()) {
            s += fmt::format("{}: {}\n", label, value);
        }
    };
    row("Reason", r.reason);
    row("Detail", r.detail);
    row("Game", r.title.empty() && r.title_id == 0 ? std::string("None") : r.title);
    if (r.title_id != 0) {
        row("Title ID", fmt::format("{:016X}", r.title_id));
    }
    row("Mode", r.vr ? "VR" : "Flat");
    if (r.started > 0) {
        row("Started", LocalTime(r.started, "%Y-%m-%d %H:%M"));
    }
    row("Version", r.version.empty() ? std::string("Unknown") : r.version);
    row("System", r.os);
    row("GPU", r.gpu.empty() ? std::string("Unknown") : r.gpu);
    row("Driver", r.driver.empty() ? std::string("Unknown") : r.driver);
    if (!r.screenshot_png.empty()) {
        row("Screenshot", "Included (screenshot.png)");
    }
    if (!r.note.empty()) {
        s += manual ? "\nWhat Happened?\n" : "\nWhat Were You Doing?\n";
        ForLines(r.note, [&](std::string_view l) { s += fmt::format("  {}\n", l); });
    }
    if (!r.settings.empty()) {
        s += "\nSettings\n";
        for (const std::string& l : r.settings) {
            s += "  " + l + "\n";
        }
    }
    if (!r.clues.empty()) {
        s += "\nMore\n";
        for (const std::string& l : r.clues) {
            s += "  " + l + "\n";
        }
    }
    if (!r.crash_line.empty()) {
        s += "\nCrash\n  " + r.crash_line + "\n";
        if (!r.exception.empty()) {
            s += "  Exception: " + r.exception + "\n";
        }
        for (std::size_t i = 0; i < r.frames.size(); i++) {
            const Frame& f = r.frames[i];
            s += fmt::format("  #{:<2} {}", i, Hex(f.addr));
            if (f.module >= 0) {
                s += fmt::format(" {} + 0x{:x}",
                                 Basename(r.modules[static_cast<std::size_t>(f.module)].path),
                                 f.offset);
            }
            if (!f.symbol.empty()) {
                s += " (" + f.symbol + ")";
            }
            s += "\n";
        }
        if (!r.modules.empty()) {
            s += "\nModules\n";
            for (const Module& m : r.modules) {
                s += fmt::format("  {} {}-{} build id {}\n", m.path, Hex(m.base), Hex(m.end),
                                 m.build_id.empty() ? std::string("none") : m.build_id);
            }
        }
    }
    return s;
}

std::string FullText(const Report& r) {
    return ReportText(r) +
           fmt::format("\nLog (Last {} Lines)\n", r.log_lines) + r.log_tail;
}

std::vector<std::pair<std::string, std::string>> Files(const Report& r) {
    std::vector<std::pair<std::string, std::string>> files{{"report.txt", ReportText(r)},
                                                           {"azahar_log.txt", r.log_tail}};
    if (!r.screenshot_png.empty()) {
        files.emplace_back("screenshot.png", r.screenshot_png);
    }
    return files;
}

// ---- The zip ------------------------------------------------------------------------------

namespace {
u32 Crc32(std::string_view data) {
    static const auto table = [] {
        std::array<u32, 256> t{};
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            t[i] = c;
        }
        return t;
    }();
    u32 c = 0xFFFFFFFFu;
    for (const char ch : data) {
        c = table[(c ^ static_cast<unsigned char>(ch)) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

void Put16(std::string& s, u32 v) {
    s.push_back(static_cast<char>(v & 0xFF));
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
}
void Put32(std::string& s, u32 v) {
    Put16(s, v & 0xFFFF);
    Put16(s, v >> 16);
}
} // namespace

std::string MakeZip(const std::vector<std::pair<std::string, std::string>>& files,
                    long long unix_time) {
    std::time_t tt = static_cast<std::time_t>(unix_time);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    const u32 dos_time = static_cast<u32>((tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2));
    const u32 dos_date =
        static_cast<u32>((std::max(tm.tm_year - 80, 0) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday);
    std::string out, central;
    for (const auto& [name, data] : files) {
        const u32 crc = Crc32(data);
        const u32 offset = static_cast<u32>(out.size());
        const auto header = [&](std::string& s, bool central_entry) {
            Put32(s, central_entry ? 0x02014b50u : 0x04034b50u);
            if (central_entry) {
                Put16(s, 20); // made by
            }
            Put16(s, 20); // needed
            Put16(s, 0x0800); // UTF-8 names
            Put16(s, 0); // stored
            Put16(s, dos_time);
            Put16(s, dos_date);
            Put32(s, crc);
            Put32(s, static_cast<u32>(data.size()));
            Put32(s, static_cast<u32>(data.size()));
            Put16(s, static_cast<u32>(name.size()));
            Put16(s, 0); // extra
            if (central_entry) {
                Put16(s, 0); // comment
                Put16(s, 0); // disk
                Put16(s, 0); // internal attributes
                Put32(s, 0100644u << 16); // external: a regular file, rw-r--r--
                Put32(s, offset);
            }
            s += name;
        };
        header(out, false);
        out += data;
        header(central, true);
    }
    const u32 central_at = static_cast<u32>(out.size());
    out += central;
    Put32(out, 0x06054b50u);
    Put16(out, 0);
    Put16(out, 0);
    Put16(out, static_cast<u32>(files.size()));
    Put16(out, static_cast<u32>(files.size()));
    Put32(out, static_cast<u32>(central.size()));
    Put32(out, central_at);
    Put16(out, 0);
    return out;
}

std::string ZipName(long long unix_time) {
    return "azaharframe-report-" + LocalTime(unix_time, "%Y%m%d-%H%M%S") + ".zip";
}

std::string SaveZip(const Report& r, const std::string& dir, long long unix_time,
                    std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::string path = dir;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    path += ZipName(unix_time);
    const std::string zip = MakeZip(Files(r), unix_time);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (f) {
        f.write(zip.data(), static_cast<std::streamsize>(zip.size()));
        f.close();
    }
    if (!f) {
        if (error) {
            *error = ec ? ec.message() : std::string("Couldn't write the file");
        }
        return {};
    }
    return path;
}

// ---- Settings -----------------------------------------------------------------------------

std::string SettingsSummary() {
    const auto& v = Settings::values;
    const auto on = [](bool b) { return b ? "On" : "Off"; };
    const auto api = [](Settings::GraphicsAPI a) {
        switch (a) {
        case Settings::GraphicsAPI::Software:
            return "Software";
        case Settings::GraphicsAPI::OpenGL:
            return "OpenGL";
        case Settings::GraphicsAPI::Vulkan:
            return "Vulkan";
        }
        return "Unknown";
    };
    const auto filter = [](Settings::TextureFilter f) {
        switch (f) {
        case Settings::TextureFilter::NoFilter:
            return "None";
        case Settings::TextureFilter::Anime4K:
            return "Anime4K";
        case Settings::TextureFilter::Bicubic:
            return "Bicubic";
        case Settings::TextureFilter::ScaleForce:
            return "ScaleForce";
        case Settings::TextureFilter::xBRZ:
            return "xBRZ";
        case Settings::TextureFilter::MMPX:
            return "MMPX";
        }
        return "Unknown";
    };
    const u32 res = v.resolution_factor.GetValue();
    std::string s;
    s += fmt::format("Graphics API: {}\n", api(v.graphics_api.GetValue()));
    s += fmt::format("Internal Resolution: {}\n", res == 0 ? std::string("Auto")
                                                             : fmt::format("{}x", res));
    s += fmt::format("Dynamic Resolution: {}\n", on(v.vr_dynamic_resolution.GetValue()));
    s += fmt::format("Texture Filter: {}\n", filter(v.texture_filter.GetValue()));
    s += fmt::format("Async Shaders: {}\n", on(v.async_shader_compilation.GetValue()));
    s += fmt::format("Accurate Image Copies: {}\n",
                     Settings::GetImageCopyAccuracyName(v.image_copy_accuracy.GetValue()));
    s += fmt::format("Disk Shader Cache: {}\n", on(v.use_disk_shader_cache.GetValue()));
    s += fmt::format("Custom Textures: {}\n", on(v.custom_textures.GetValue()));
    s += fmt::format("Preload Textures: {}\n", on(v.preload_textures.GetValue()));
    s += fmt::format("CPU Clock: {} %\n", v.cpu_clock_percentage.GetValue());
    s += fmt::format("New 3DS: {}\n", on(v.is_new_3ds.GetValue()));
    s += fmt::format("Immersive Mode: {}\n", v.vr_immersive_mode.GetValue());
    s += fmt::format("Wider View: {}\n", v.vr_immersive_wider_view_level.GetValue());
    s += fmt::format("Screen Layout: {}\n", v.vr_screen_layout.GetValue());
    s += fmt::format("Screen Curve: {}\n", v.vr_screen_curve.GetValue());
    s += fmt::format("Window Scale: {} %\n", v.vr_window_scale.GetValue());
    s += fmt::format("Background: {}\n",
                     v.vr_background.GetValue() == 1 ? "Passthrough" : "Black");
    s += fmt::format("Fast Forward Speed: {} %\n", static_cast<int>(v.turbo_limit.GetValue()));
    return s;
}

} // namespace Core::CrashReport
