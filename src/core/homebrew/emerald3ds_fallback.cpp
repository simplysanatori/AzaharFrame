// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include "core/homebrew/emerald3ds_fallback.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fmt/format.h>
#include <json.hpp>
#include "common/logging/log.h"
#include "core/homebrew/net.h"

namespace Homebrew::Emerald3ds::Fallback {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

std::string OneLine(const std::string& text) {
    std::string out;
    bool space = false;
    for (const char c : text) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            space = !out.empty();
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        out.push_back(c);
    }
    return out;
}

bool Executable(const std::string& path) {
    return ::access(path.c_str(), X_OK) == 0;
}

/// Runs a fixed command and returns its first line of output ("" on any failure).
std::string FirstLineOf(const std::vector<std::string>& argv) {
    int fds[2];
    if (::pipe(fds) != 0) {
        return {};
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        return {};
    }
    if (pid == 0) {
        ::dup2(fds[1], 1);
        const int null = ::open("/dev/null", O_RDWR);
        ::dup2(null, 0);
        ::dup2(null, 2);
        ::close(fds[0]);
        std::vector<char*> args;
        for (const std::string& a : argv) {
            args.push_back(const_cast<char*>(a.c_str()));
        }
        args.push_back(nullptr);
        ::execv(args[0], args.data());
        ::_exit(127);
    }
    ::close(fds[1]);
    std::string out;
    char buf[256];
    ssize_t n;
    while ((n = ::read(fds[0], buf, sizeof buf)) > 0 && out.size() < 4096) {
        out.append(buf, static_cast<std::size_t>(n));
    }
    ::close(fds[0]);
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return {};
    }
    return out.substr(0, out.find('\n'));
}

u32 Le32(const unsigned char* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<u32>(p[3]) << 24);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// What the program needs

std::string MissingText(bool bwrap_missing, bool python_missing) {
    if (bwrap_missing && python_missing) {
        return "bubblewrap and Python 3.11 or newer";
    }
    if (bwrap_missing) {
        return "bubblewrap";
    }
    return python_missing ? "Python 3.11 or newer" : std::string{};
}

Tools Detect(const std::string& bwrap, const std::string& python) {
    Tools t;
    t.bwrap = bwrap;
    t.python = python;
    const bool no_bwrap = !Executable(bwrap);
    bool no_python = !Executable(python);
    if (!no_python) {
        // "3 11": the version, read by the interpreter itself (-I: nothing of the user's environment)
        const std::string v = FirstLineOf(
            {python, "-I", "-c", "import sys; print(int(sys.version_info >= (3, 11)))"});
        no_python = v != "1";
    }
    t.missing = MissingText(no_bwrap, no_python);
    return t;
}

// ---------------------------------------------------------------------------------------------
// The sandbox

std::vector<std::string> Argv(const Sandbox& s) {
    std::vector<std::string> a{s.bwrap,
                               "--unshare-all", // user, ipc, pid, net, uts, cgroup
                               "--die-with-parent",
                               "--new-session"};
    if (s.network) {
        a.emplace_back("--share-net");
    }
    // The system, read only, as the interpreter needs it; nothing else of the machine.
    for (const char* dir : {"/usr"}) {
        a.insert(a.end(), {"--ro-bind", dir, dir});
    }
    a.insert(a.end(), {"--symlink", "usr/bin", "/bin", "--symlink", "usr/bin", "/sbin", "--symlink",
                       "usr/lib", "/lib", "--symlink", "usr/lib", "/lib64"});
    a.insert(a.end(), {"--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp", "--tmpfs", "/home",
                       "--tmpfs", "/run", "--tmpfs", "/etc"});
    if (s.network) { // name lookups and certificates only
        for (const char* f : {"/etc/resolv.conf", "/etc/hosts", "/etc/nsswitch.conf", "/etc/host.conf",
                              "/etc/ssl", "/etc/ca-certificates", "/etc/pki", "/etc/ca-certificates.conf"}) {
            a.insert(a.end(), {"--ro-bind-try", f, f});
        }
        a.insert(a.end(), {"--ro-bind-try", "/run/systemd/resolve", "/run/systemd/resolve"});
    }
    // The only things of the player's: the ROM file and the release's zip (read only), one folder to write.
    a.insert(a.end(), {"--ro-bind", s.rom, "/in/rom.gba", "--ro-bind", s.payload_zip, "/in/payload.zip",
                       "--ro-bind", s.launcher, "/in/launcher.py", "--bind", s.work, "/out"});
    a.insert(a.end(), {"--chdir", "/out", "--clearenv", "--setenv", "PATH", "/usr/bin", "--setenv", "HOME",
                       "/tmp", "--setenv", "LANG", "C.UTF-8", "--setenv", "PYTHONDONTWRITEBYTECODE", "1",
                       "--setenv", "PYTHONUNBUFFERED", "1", "--"});
    a.insert(a.end(), {s.python, "-I", "-B", "/in/launcher.py", "--payload-zip", "/in/payload.zip", "--rom",
                       "/in/rom.gba", "--out", "/out", "--cache", "/out/followers-cache"});
    if (!s.language.empty()) {
        a.insert(a.end(), {"--language", s.language});
    }
    if (!s.network) {
        a.emplace_back("--no-network");
    }
    return a;
}

const char* LauncherSource() {
    return R"PY(# AzaharFrame emfallback launcher (ours, not the release's). Runs Emerald 3DS's own builder package,
# unpacked from the release's Emerald3DS-WebPayload.zip, inside the sandbox. Prints "progress PERMILLE TEXT"
# lines, then "result {json}"; "error TEXT" and exit 1 on failure. The ROM is only read.
import argparse, json, shutil, sys, zipfile
from pathlib import Path, PurePosixPath


def say(kind, text):
    sys.stdout.write("%s %s\n" % (kind, text))
    sys.stdout.flush()


def fail(text):
    say("error", " ".join(str(text).split()))
    sys.exit(1)


def unpack(zip_path, dest):
    wanted = ("python/", "payload/", "licenses/", "web-manifest.json")
    with zipfile.ZipFile(zip_path) as zf:
        for info in zf.infolist():
            name = info.filename
            parts = PurePosixPath(name).parts
            if name.startswith("/") or "\\" in name or ".." in parts or ":" in name:
                fail("The Emerald 3DS download holds an unsafe file name.")
            if not name.startswith(wanted):
                continue
            target = dest.joinpath(*parts)
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(info) as src, open(target, "wb") as out:
                shutil.copyfileobj(src, out)
    if not (dest / "python" / "emerald3ds_builder").is_dir() or not (dest / "payload").is_dir():
        fail("The Emerald 3DS download has no builder in it.")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--payload-zip", required=True, type=Path)
    ap.add_argument("--rom", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--language", default="")
    ap.add_argument("--cache", type=Path, default=None)
    ap.add_argument("--no-network", action="store_true")
    args = ap.parse_args()
    if sys.version_info < (3, 11):
        fail("Emerald 3DS's builder needs Python 3.11 or newer.")
    args.out.mkdir(parents=True, exist_ok=True)
    unpacked = args.out / "payload.unpacked"
    shutil.rmtree(unpacked, ignore_errors=True)
    unpacked.mkdir(parents=True)
    try:
        say("progress", "0 Unpacking the builder")
        unpack(args.payload_zip, unpacked)
        sys.path.insert(0, str(unpacked / "python"))
        try:
            from emerald3ds_builder import followers
            from emerald3ds_builder.build import Payload, build_pack, select_payload
            from emerald3ds_builder.errors import BuilderError
            from emerald3ds_builder.rom import load_rom
        except ImportError as exc:
            fail("Emerald 3DS's builder could not start: %s" % exc)

        def progress(fraction, message):
            say("progress", "%d %s" % (int(max(0.0, min(1.0, fraction)) * 1000), message))

        language = args.language or None
        payload = Payload(unpacked / "payload")
        try:
            rom = load_rom(args.rom)
            variant = select_payload(payload, rom.sha1, need_executables=True, language=language)
            if args.no_network:
                fetcher = False  # no follower sprites at all, as the built-in builder does
            else:
                fetcher = followers.Fetcher(followers.load_sources(), cache=args.cache)
            pak = args.out / "emerald3ds.pak"
            info = build_pack(args.rom, payload, pak, progress, runner="inprocess", rom=rom,
                              need_executables=True, language=language, follower_fetcher=fetcher)
            for exe in info["payload"].executables():
                shutil.copy2(exe, args.out / exe.name)
        except BuilderError as exc:
            fail(exc.message if not exc.hint else "%s %s" % (exc.message, exc.hint))
        except (TypeError, AttributeError, ImportError) as exc:
            fail("Emerald 3DS's builder works differently from the one this launcher knows: %s" % exc)
        except MemoryError:
            fail("Not enough memory to build the data pack.")
        say("result", json.dumps({
            "abi": "%08x" % info["abi"],
            "release": info["release"],
            "language": info["payload"].game_language,
            "variant_matches": variant.root == info["payload"].root,
            "entries": info["entries"],
            "bytes": info["bytes"],
        }))
    finally:
        shutil.rmtree(unpacked, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
)PY";
}

// ---------------------------------------------------------------------------------------------
// One output line

Line ParseLine(const std::string& line) {
    Line l;
    const auto space = line.find(' ');
    const std::string word = line.substr(0, space);
    const std::string rest = space == std::string::npos ? std::string{} : line.substr(space + 1);
    if (word == "progress") {
        const auto sp = rest.find(' ');
        char* end = nullptr;
        const long v = std::strtol(rest.substr(0, sp).c_str(), &end, 10);
        if (end == nullptr || *end != '\0' || rest.empty()) {
            return l;
        }
        l.kind = Line::Kind::Progress;
        l.permille = static_cast<int>(std::clamp<long>(v, 0, 1000));
        l.text = sp == std::string::npos ? std::string{} : rest.substr(sp + 1);
    } else if (word == "result" && !rest.empty()) {
        l.kind = Line::Kind::Result;
        l.text = rest;
    } else if (word == "error") {
        l.kind = Line::Kind::Error;
        l.text = rest;
    }
    return l;
}

// ---------------------------------------------------------------------------------------------
// Checks

int SeedCache(const std::string& from, const std::string& to) {
    std::error_code ec;
    if (from.empty() || !fs::is_directory(from, ec)) {
        return 0;
    }
    fs::create_directories(to, ec);
    int copied = 0;
    for (const auto& e : fs::directory_iterator(from, ec)) {
        const std::string name = e.path().filename().string();
        if (name.size() != 64 || !e.is_regular_file(ec) ||
            name.find_first_not_of("0123456789abcdef") != std::string::npos) {
            continue;
        }
        const fs::path dest = fs::path(to) / name;
        if (!fs::exists(dest, ec) && fs::copy_file(e.path(), dest, ec)) {
            ++copied;
        }
    }
    return copied;
}

std::string ValidateOutput(const std::string& out_dir, const std::string& expected_abi,
                           const std::string& expected_rom_sha1,
                           const std::string& expected_3dsx_sha256) {
    const fs::path dir(out_dir);
    std::ifstream in(dir / "emerald3ds.pak", std::ios::binary);
    if (!in) {
        return "Emerald 3DS's builder did not write the data pack.";
    }
    const std::string pak((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string why;
    if (Build::PakVerify(pak, &why) == 0 && !why.empty()) {
        return "The data pack failed its own check. " + why;
    }
    const auto* p = reinterpret_cast<const unsigned char*>(pak.data());
    const std::string abi = fmt::format("{:08x}", Le32(p + 12));
    if (!expected_abi.empty() && abi != expected_abi) {
        return "The data pack does not match this release";
    }
    if (!expected_rom_sha1.empty()) {
        std::string hex;
        for (int i = 0; i < 20; ++i) {
            hex += fmt::format("{:02x}", p[16 + i]);
        }
        if (hex != expected_rom_sha1) {
            return "The data pack was not made from this ROM.";
        }
    }
    std::error_code ec;
    for (const char* name : {"Emerald3DS.3dsx", "Emerald3DS.smdh"}) {
        if (!fs::is_regular_file(dir / name, ec) || fs::file_size(dir / name, ec) == 0) {
            return fmt::format("The Emerald 3DS download is damaged ({}).", name);
        }
    }
    if (!expected_3dsx_sha256.empty() &&
        Homebrew::FileSha256((dir / "Emerald3DS.3dsx").string()) != expected_3dsx_sha256) {
        return "The game file does not match this release's manifest.";
    }
    return {};
}

// ---------------------------------------------------------------------------------------------
// The run

Build::Result Run(const Options& options) {
    Build::Result res;
    const Build::Options& b = options.build;
    const auto fail = [&](const std::string& text) {
        res.ok = false;
        res.error = OneLine(text);
        return res;
    };
    Tools tools = options.tools;
    if (tools.bwrap.empty() && tools.python.empty() && tools.missing.empty()) {
        tools = Detect();
    }
    if (!tools.Ok()) {
        return fail("Emerald 3DS's own builder needs " + tools.missing + ", which this system does not have.");
    }
    std::error_code ec;
    fs::create_directories(b.out_dir, ec);
    // the launcher: a file of its own next to the work folder, bound read only
    const std::string out_abs = fs::absolute(b.out_dir, ec).lexically_normal().string();
    const std::string launcher = fs::path(out_abs).parent_path().string() + "/emerald3ds-fallback-launcher.py";
    {
        std::ofstream f(launcher, std::ios::binary | std::ios::trunc);
        f << LauncherSource();
        if (!f) {
            return fail("The builder could not be prepared (no room to write).");
        }
    }
    const std::string cache_in_work = out_abs + "/followers-cache";
    const int seeded = SeedCache(b.cache_dir, cache_in_work);

    Sandbox sb;
    sb.bwrap = tools.bwrap;
    sb.python = tools.python;
    sb.launcher = launcher;
    sb.rom = fs::absolute(b.rom, ec).string();
    sb.payload_zip = fs::absolute(b.payload_zip, ec).string();
    sb.work = out_abs;
    sb.language = b.language;
    sb.network = !b.no_network;
    const std::vector<std::string> argv = Argv(sb);
    LOG_INFO(Frontend, "emerald3ds: building with the release's own builder in a sandbox (network {}, {} cached follower files)",
             sb.network ? "on" : "off", seeded);

    int fds[2];
    if (::pipe(fds) != 0) {
        return fail("The builder could not be started.");
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        return fail("The builder could not be started.");
    }
    if (pid == 0) {
        ::setpgid(0, 0);
        ::dup2(fds[1], 1);
        ::dup2(fds[1], 2);
        const int null = ::open("/dev/null", O_RDONLY);
        ::dup2(null, 0);
        ::close(fds[0]);
        ::close(fds[1]);
        std::vector<char*> args;
        for (const std::string& a : argv) {
            args.push_back(const_cast<char*>(a.c_str()));
        }
        args.push_back(nullptr);
        ::execv(args[0], args.data());
        ::_exit(127);
    }
    ::setpgid(pid, pid);
    ::close(fds[1]);
    ::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL) | O_NONBLOCK);

    const auto kill_all = [&] {
        ::kill(-pid, SIGKILL); // the sandbox's group; bwrap's own init dies with it
        ::kill(pid, SIGKILL);
        int status = 0;
        ::waitpid(pid, &status, 0);
    };
    const auto start = std::chrono::steady_clock::now();
    std::string buffer, last_text, result_json, error_text;
    std::vector<std::string> tail;
    bool eof = false, stopped = false, timed_out = false;
    const auto handle = [&](const std::string& line) {
        const Line l = ParseLine(line);
        switch (l.kind) {
        case Line::Kind::Progress:
            if (b.progress) {
                b.progress(l.permille, l.text);
            }
            break;
        case Line::Kind::Result:
            result_json = l.text;
            break;
        case Line::Kind::Error:
            error_text = l.text;
            break;
        default:
            if (!line.empty()) {
                tail.push_back(line);
                if (tail.size() > 6) {
                    tail.erase(tail.begin());
                }
            }
        }
    };
    while (!eof) {
        pollfd pfd{fds[0], POLLIN, 0};
        ::poll(&pfd, 1, 100);
        char buf[4096];
        for (;;) {
            const ssize_t n = ::read(fds[0], buf, sizeof buf);
            if (n > 0) {
                buffer.append(buf, static_cast<std::size_t>(n));
                std::size_t nl;
                while ((nl = buffer.find('\n')) != std::string::npos) {
                    handle(buffer.substr(0, nl));
                    buffer.erase(0, nl + 1);
                }
                continue;
            }
            if (n == 0) {
                eof = true;
            }
            break;
        }
        if (b.cancel != nullptr && b.cancel->load()) {
            stopped = true;
            break;
        }
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(options.timeout_seconds)) {
            timed_out = true;
            break;
        }
    }
    ::close(fds[0]);
    int status = 0;
    if (stopped || timed_out) {
        kill_all();
    } else {
        ::waitpid(pid, &status, 0);
        ::kill(-pid, SIGKILL); // anything left in the group
    }
    if (!buffer.empty()) {
        handle(buffer);
    }
    if (stopped) {
        res.stopped = true;
        return res;
    }
    if (timed_out) {
        return fail("Emerald 3DS's builder took too long and was stopped.");
    }
    if (!error_text.empty()) {
        return fail(error_text);
    }
    json info = json::parse(result_json, nullptr, false);
    if (result_json.empty() || !info.is_object()) {
        std::string detail;
        for (const std::string& t : tail) {
            detail += (detail.empty() ? "" : " ") + t;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
            return fail("The sandbox could not be started.");
        }
        return fail("Emerald 3DS's builder stopped unexpectedly." + (detail.empty() ? "" : " " + detail));
    }
    // What came out is checked here, as for the C++ builder's pack, before anything is installed.
    const std::string bad = ValidateOutput(b.out_dir, options.expected_abi, options.expected_rom_sha1,
                                           options.expected_3dsx_sha256);
    if (!bad.empty()) {
        return fail(bad);
    }
    res.ok = true;
    res.abi = info.value("abi", std::string{});
    res.release = info.value("release", std::string{});
    res.language = info.value("language", std::string{});
    res.variant_matches = info.value("variant_matches", true);
    res.entries = info.value("entries", u64{0});
    res.bytes = info.value("bytes", u64{0});
    if (b.progress) {
        b.progress(1000, "Done");
    }
    return res;
}

// ---------------------------------------------------------------------------------------------
// Choices

Choice Choices::Get(const std::string& tag) const {
    const auto it = map.find(tag);
    return it == map.end() ? Choice::Unasked : it->second;
}

void Choices::Set(const std::string& tag, Choice choice) {
    if (tag.empty()) {
        return;
    }
    if (choice == Choice::Unasked) {
        map.erase(tag);
    } else {
        map[tag] = choice;
    }
}

std::string Choices::ToJson() const {
    json j = json::object();
    for (const auto& [tag, c] : map) {
        j[tag] = c == Choice::Allowed ? "build" : "not-now";
    }
    return json{{"fallback", j}}.dump(2) + "\n";
}

Choices Choices::FromJson(const std::string& text) {
    Choices c;
    const json j = json::parse(text, nullptr, false);
    if (!j.is_object() || !j.contains("fallback") || !j["fallback"].is_object()) {
        return c;
    }
    for (const auto& [tag, v] : j["fallback"].items()) {
        if (v == "build") {
            c.map[tag] = Choice::Allowed;
        } else if (v == "not-now") {
            c.map[tag] = Choice::Declined;
        }
    }
    return c;
}

Choices LoadChoices(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return Choices::FromJson(std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
}

bool SaveChoices(const std::string& path, const Choices& choices) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    const std::string tmp = path + ".new";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << choices.ToJson();
        if (!out) {
            return false;
        }
    }
    fs::rename(tmp, path, ec);
    return !ec;
}

// ---------------------------------------------------------------------------------------------
// Words

std::string PromptTitle(const std::string& tag) {
    return fmt::format("Emerald 3DS {} Needs Its Own Builder", tag.empty() ? "Update" : tag);
}

std::string PromptText(bool network_used) {
    return std::string("Azahar for Frame can't build this version of Emerald 3DS itself yet. "
                       "Emerald 3DS's own build program can, in a protected sandbox that can only read your "
                       "ROM and write its own folder.") +
           (network_used ? " It also downloads the optional follower sprites." : "");
}

std::string UnavailableText(const std::string& tag, const std::string& missing) {
    return fmt::format("Azahar for Frame can't build Emerald 3DS {} itself yet. Emerald 3DS's own build "
                       "program needs {}, which this system does not have. Update Azahar for Frame to build "
                       "this version.",
                       tag.empty() ? "this version" : tag, missing);
}

} // namespace Homebrew::Emerald3ds::Fallback
