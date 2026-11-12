// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emfallback: the unsupported-recipe result, the sandbox's command line, the launcher's
// output lines, the remembered choice per release version, the output checks, and (where bubblewrap
// works) what the sandbox can and cannot reach. No network, no ROM, no Emerald files.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include "core/homebrew/emerald3ds_build_internal.h"
#include "core/homebrew/emerald3ds_fallback.h"

namespace B = Homebrew::Emerald3ds::Build;
namespace F = Homebrew::Emerald3ds::Fallback;
namespace fs = std::filesystem;

namespace {

fs::path Scratch(const char* name) {
    const fs::path dir = fs::temp_directory_path() / (std::string("emfallback-test-") + name);
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

bool Has(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

std::size_t IndexOf(const std::vector<std::string>& v, const std::string& s, std::size_t from = 0) {
    for (std::size_t i = from; i < v.size(); ++i) {
        if (v[i] == s) {
            return i;
        }
    }
    return v.size();
}

} // namespace

TEST_CASE("Emerald fallback: a recipe of a newer schema is the unsupported kind of refusal", "[emfallback]") {
    // the metadata {"schema":2} as .xz inside a recipe container
    static const unsigned char xz[] = {0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6, 0xb4, 0x46, 0x02, 0x00, 0x21, 0x01, 0x16, 0x00, 0x00, 0x00, 0x74, 0x2f, 0xe5, 0xa3, 0x01, 0x00, 0x0b, 0x7b, 0x22, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x22, 0x3a, 0x32, 0x7d, 0x00, 0x37, 0x8d, 0x5a, 0xa8, 0x99, 0x88, 0x6f, 0x32, 0x00, 0x01, 0x24, 0x0c, 0xa6, 0x18, 0xd8, 0xd8, 0x1f, 0xb6, 0xf3, 0x7d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a};
    std::string recipe = "EM3DRCP1";
    const u32 n = sizeof xz;
    for (int i = 0; i < 4; ++i) {
        recipe.push_back(static_cast<char>((n >> (8 * i)) & 0xFF));
    }
    recipe.append(reinterpret_cast<const char*>(xz), sizeof xz);
    std::string why;
    CHECK_FALSE(B::ParseRecipe(recipe, false, &why).has_value());
    CHECK(why.find("unsupported recipe schema") != std::string::npos);
    // a damaged recipe says damaged, not unsupported
    why.clear();
    CHECK_FALSE(B::ParseRecipe(recipe.substr(0, 14), false, &why).has_value());
    CHECK(why.find("unsupported") == std::string::npos);
    // the result type carries the kind and keeps it apart from a plain failure
    B::Result plain;
    CHECK_FALSE(plain.unsupported);
    CHECK_FALSE(plain.ok);
}

TEST_CASE("Emerald fallback: the sandbox command line", "[emfallback]") {
    F::Sandbox s;
    s.launcher = "/work/launcher.py";
    s.rom = "/data/p/rom.gba";
    s.payload_zip = "/data/p/payload.zip";
    s.work = "/data/p/work/out";
    s.language = "pt";
    s.network = false;
    auto a = F::Argv(s);
    CHECK(a.front() == "/usr/bin/bwrap");
    CHECK(Has(a, "--unshare-all"));
    CHECK(Has(a, "--die-with-parent"));
    CHECK(Has(a, "--new-session"));
    CHECK(Has(a, "--clearenv"));
    CHECK_FALSE(Has(a, "--share-net"));
    CHECK(Has(a, "--no-network"));
    CHECK_FALSE(Has(a, "/etc/resolv.conf"));
    CHECK_FALSE(Has(a, "sh"));
    CHECK_FALSE(Has(a, "-c"));
    // the player's files: the ROM and the zip read only, one writable folder
    std::size_t rw = 0;
    for (std::size_t i = 0; i + 2 < a.size(); ++i) {
        if (a[i] == "--bind") {
            ++rw;
            CHECK(a[i + 1] == "/data/p/work/out");
            CHECK(a[i + 2] == "/out");
        }
        if (a[i] == "--ro-bind" && a[i + 1].rfind("/data/p", 0) == 0) {
            CHECK((a[i + 1] == "/data/p/rom.gba" || a[i + 1] == "/data/p/payload.zip"));
        }
    }
    CHECK(rw == 1);
    CHECK(Has(a, "--tmpfs"));
    const auto home = IndexOf(a, "/home");
    CHECK(a[home - 1] == "--tmpfs");
    const auto sep = IndexOf(a, "--");
    REQUIRE(sep + 3 < a.size());
    CHECK(a[sep + 1] == "/usr/bin/python3");
    CHECK(a[sep + 2] == "-I");
    CHECK(a.back() == "--no-network");
    CHECK(Has(a, "pt"));
    // the network only when asked, with the name lookup files and nothing of the home
    s.network = true;
    s.language.clear();
    a = F::Argv(s);
    CHECK(Has(a, "--share-net"));
    CHECK(Has(a, "/etc/resolv.conf"));
    CHECK_FALSE(Has(a, "--no-network"));
    CHECK_FALSE(Has(a, "--language"));
}

TEST_CASE("Emerald fallback: the launcher's lines", "[emfallback]") {
    auto p = F::ParseLine("progress 250 Extracting game data");
    CHECK(p.kind == F::Line::Kind::Progress);
    CHECK(p.permille == 250);
    CHECK(p.text == "Extracting game data");
    CHECK(F::ParseLine("progress 5000 x").permille == 1000);
    CHECK(F::ParseLine("progress abc x").kind == F::Line::Kind::Other);
    CHECK(F::ParseLine("result {\"abi\":\"4b3cb7ab\"}").kind == F::Line::Kind::Result);
    auto e = F::ParseLine("error The ROM is wrong");
    CHECK(e.kind == F::Line::Kind::Error);
    CHECK(e.text == "The ROM is wrong");
    CHECK(F::ParseLine("Traceback (most recent call last):").kind == F::Line::Kind::Other);
    CHECK(F::ParseLine("").kind == F::Line::Kind::Other);
}

TEST_CASE("Emerald fallback: the choice is remembered per release version", "[emfallback]") {
    F::Choices c;
    CHECK(c.Get("v0.6.0") == F::Choice::Unasked);
    c.Set("v0.6.0", F::Choice::Allowed);
    c.Set("v0.7.0", F::Choice::Declined);
    CHECK(c.Get("v0.6.0") == F::Choice::Allowed);
    CHECK(c.Get("v0.7.0") == F::Choice::Declined);
    CHECK(c.Get("v0.8.0") == F::Choice::Unasked); // the next version asks again
    const auto again = F::Choices::FromJson(c.ToJson());
    CHECK(again == c);
    CHECK(F::Choices::FromJson("not json").Get("v0.6.0") == F::Choice::Unasked);
    CHECK(F::Choices::FromJson("{\"fallback\":{\"v1\":\"maybe\"}}").Get("v1") == F::Choice::Unasked);
    c.Set("v0.7.0", F::Choice::Unasked);
    CHECK(c.Get("v0.7.0") == F::Choice::Unasked);
    // on disk
    const fs::path dir = Scratch("choices");
    const std::string path = (dir / "user" / "emerald3ds" / F::kChoicesFile).string();
    CHECK(F::LoadChoices(path).Get("v0.6.0") == F::Choice::Unasked);
    REQUIRE(F::SaveChoices(path, c));
    CHECK(F::LoadChoices(path) == c);
    CHECK(F::LoadChoices(path).Get("v0.6.0") == F::Choice::Allowed);
}

TEST_CASE("Emerald fallback: what the player reads", "[emfallback]") {
    CHECK(F::PromptTitle("v0.6.0") == "Emerald 3DS v0.6.0 Needs Its Own Builder");
    const std::string text = F::PromptText(false);
    CHECK(text.find("can't build this version of Emerald 3DS itself yet") != std::string::npos);
    CHECK(text.find("protected sandbox") != std::string::npos);
    CHECK(std::string(F::kButtonBuild) == "Build With Emerald 3DS's Builder");
    CHECK(std::string(F::kButtonNotNow) == "Not Now");
    CHECK(F::MissingText(true, false) == "bubblewrap");
    CHECK(F::MissingText(false, true) == "Python 3.11 or newer");
    CHECK(F::MissingText(true, true) == "bubblewrap and Python 3.11 or newer");
    CHECK(F::MissingText(false, false).empty());
    const auto t = F::Detect("/nonexistent/bwrap", "/nonexistent/python3");
    CHECK_FALSE(t.Ok());
    CHECK(t.missing == "bubblewrap and Python 3.11 or newer");
    const auto u = F::UnavailableText("v0.6.0", t.missing);
    CHECK(u.find("v0.6.0") != std::string::npos);
    CHECK(u.find("bubblewrap and Python 3.11 or newer") != std::string::npos);
    // Run says so, too, instead of trying
    F::Options o;
    o.tools = t;
    const auto r = F::Run(o);
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("needs bubblewrap and Python 3.11 or newer") != std::string::npos);
}

TEST_CASE("Emerald fallback: the output is checked before it is installed", "[emfallback]") {
    const fs::path dir = Scratch("validate");
    const std::string rom_raw(20, '\x42');
    std::string err;
    const std::string pak = B::PakBytes({{"data/a.bin", "aaaa"}, {"data/b.bin", "bb"}}, 0x4b3cb7ab, rom_raw, &err);
    REQUIRE(!pak.empty());
    const auto write = [&](const char* name, const std::string& data) {
        std::ofstream(dir / name, std::ios::binary) << data;
    };
    write("emerald3ds.pak", pak);
    write("Emerald3DS.3dsx", "3dsx");
    write("Emerald3DS.smdh", "smdh");
    const std::string hex42 = [] {
        std::string s;
        for (int i = 0; i < 20; ++i) {
            s += "42";
        }
        return s;
    }();
    CHECK(F::ValidateOutput(dir.string(), "4b3cb7ab", hex42, "").empty());
    CHECK(F::ValidateOutput(dir.string(), "", "", "").empty());
    CHECK_FALSE(F::ValidateOutput(dir.string(), "00000000", "", "").empty());                // wrong ABI
    CHECK_FALSE(F::ValidateOutput(dir.string(), "4b3cb7ab", std::string(40, '0'), "").empty()); // other ROM
    CHECK_FALSE(F::ValidateOutput(dir.string(), "", "", std::string(64, '0')).empty());      // other game file
    std::string damaged = pak;
    damaged[160] ^= 0x55; // the first payload starts after the 64 byte header and two 40 byte index entries
    write("emerald3ds.pak", damaged);
    CHECK_FALSE(F::ValidateOutput(dir.string(), "", "", "").empty());
    write("emerald3ds.pak", pak);
    std::filesystem::remove(dir / "Emerald3DS.smdh");
    CHECK_FALSE(F::ValidateOutput(dir.string(), "", "", "").empty());
    std::filesystem::remove(dir / "emerald3ds.pak");
    CHECK_FALSE(F::ValidateOutput(dir.string(), "", "", "").empty());
}

TEST_CASE("Emerald fallback: follower files seed the sandbox's cache", "[emfallback]") {
    const fs::path from = Scratch("seed-from"), to = Scratch("seed-to");
    const std::string name(64, 'a');
    std::ofstream(from / name) << "x";
    std::ofstream(from / "not-a-hash") << "y";
    CHECK(F::SeedCache(from.string(), (to / "cache").string()) == 1);
    CHECK(fs::exists(to / "cache" / name));
    CHECK_FALSE(fs::exists(to / "cache" / "not-a-hash"));
    CHECK(F::SeedCache(from.string(), (to / "cache").string()) == 0); // nothing new
}

TEST_CASE("Emerald fallback: the sandbox cannot see the player's files", "[emfallback][sandbox]") {
    const F::Tools tools = F::Detect();
    if (!tools.Ok()) {
        SKIP("bubblewrap or Python 3.11 is not available here");
    }
    const fs::path home = Scratch("home");
    fs::create_directories(home / "Documents");
    std::ofstream(home / "canary") << "secret";
    std::ofstream(home / "Documents" / "canary2") << "secret";
    std::ofstream(home / "rom.gba") << "ROM";
    std::ofstream(home / "payload.zip") << "ZIP";
    fs::create_directories(home / "work");
    std::ofstream(home / "launcher.py") << "";
    F::Sandbox s;
    s.launcher = (home / "launcher.py").string();
    s.rom = (home / "rom.gba").string();
    s.payload_zip = (home / "payload.zip").string();
    s.work = (home / "work").string();
    auto argv = F::Argv(s);
    const auto sep = IndexOf(argv, "--");
    argv.resize(sep + 1);
    const std::string canary = (home / "canary").string();
    const std::string canary2 = (home / "Documents" / "canary2").string();
    const std::string host_home = home.string();
    const std::string code =
        "import os, socket\n"
        "def tryread(p):\n"
        "    try:\n"
        "        open(p).read(); return 'READ'\n"
        "    except OSError: return 'no'\n"
        "def trywrite(p):\n"
        "    try:\n"
        "        open(p, 'w').write('x'); return 'WROTE'\n"
        "    except OSError: return 'no'\n"
        "print('canary', tryread('" + canary + "'), trywrite('" + canary + "'))\n"
        "print('canary2', tryread('" + canary2 + "'), trywrite('" + canary2 + "'))\n"
        "print('hostdir', os.path.exists('" + host_home + "'))\n"
        "print('rom', tryread('/in/rom.gba'), trywrite('/in/rom.gba'))\n"
        "print('zip', trywrite('/in/payload.zip'))\n"
        "print('usr', trywrite('/usr/bin/x'))\n"
        "print('etc', os.path.exists('/etc/passwd'))\n"
        "print('out', trywrite('/out/made'))\n"
        "print('env', sorted(os.environ))\n"
        "try:\n"
        "    socket.create_connection(('1.1.1.1', 53), 2); print('net yes')\n"
        "except OSError: print('net no')\n";
    argv.insert(argv.end(), {s.python, "-I", "-c", code});
    std::string cmd;
    for (const std::string& a : argv) {
        std::string q = "'";
        for (const char c : a) {
            q += c == '\'' ? std::string("'\\''") : std::string(1, c);
        }
        cmd += q + "' ";
    }
    cmd += "2>&1";
    std::string out;
    if (FILE* pipe = ::popen(cmd.c_str(), "r")) {
        char buf[512];
        while (std::fgets(buf, sizeof buf, pipe)) {
            out += buf;
        }
        ::pclose(pipe);
    }
    if (out.find("canary") == std::string::npos) {
        SKIP("the sandbox could not start here: " + out.substr(0, 200));
    }
    CHECK(out.find("canary no no") != std::string::npos);
    CHECK(out.find("canary2 no no") != std::string::npos);
    CHECK(out.find("hostdir False") != std::string::npos);
    CHECK(out.find("rom READ no") != std::string::npos);
    CHECK(out.find("zip no") != std::string::npos);
    CHECK(out.find("usr no") != std::string::npos);
    CHECK(out.find("etc False") != std::string::npos);
    CHECK(out.find("out WROTE") != std::string::npos);
    CHECK(out.find("net no") != std::string::npos);
    CHECK(out.find("['HOME', 'LANG', 'PATH'") != std::string::npos);
    // and on the host: the canary is untouched, the work folder has what the sandbox wrote
    std::ifstream in(canary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(text == "secret");
    CHECK(fs::exists(home / "work" / "made"));
}

// With the owner's ROM and the release's payload (read only; outputs in a scratch folder): the same pack
// from the release's own builder in the sandbox and from the built-in builder, and a cancel that kills it.
// AZAHARFRAME_TEST_EMERALD_ROM and AZAHARFRAME_TEST_EMERALD_PAYLOAD name the two files.
TEST_CASE("Emerald fallback: same pack as the built-in builder, and Cancel stops the sandbox",
          "[emfallback][e2e]") {
    const char* rom = std::getenv("AZAHARFRAME_TEST_EMERALD_ROM");
    const char* zip = std::getenv("AZAHARFRAME_TEST_EMERALD_PAYLOAD");
    if (!rom || !zip) {
        SKIP("AZAHARFRAME_TEST_EMERALD_ROM and AZAHARFRAME_TEST_EMERALD_PAYLOAD are not set");
    }
    const F::Tools tools = F::Detect();
    if (!tools.Ok()) {
        SKIP("bubblewrap or Python 3.11 is not available here");
    }
    const fs::path dir = Scratch("e2e");
    B::Options built;
    built.payload_zip = zip;
    built.rom = rom;
    built.no_network = true;
    built.out_dir = (dir / "builtin").string();
    const B::Result a = B::BuildPack(built);
    REQUIRE(a.ok);
    CHECK_FALSE(a.unsupported);

    F::Options fo;
    fo.build = built;
    fo.build.out_dir = (dir / "fallback" / "out").string();
    fo.expected_abi = a.abi;
    int last = -1;
    fo.build.progress = [&](int permille, const std::string&) { last = permille; };
    const B::Result b = F::Run(fo);
    REQUIRE(b.ok);
    CHECK(last == 1000);
    CHECK(b.abi == a.abi);
    CHECK(b.entries == a.entries);
    const auto slurp = [](const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };
    for (const char* name : {"emerald3ds.pak", "Emerald3DS.3dsx", "Emerald3DS.smdh"}) {
        INFO(name);
        CHECK(slurp(dir / "builtin" / name) == slurp(dir / "fallback" / "out" / name));
    }

    // Cancel: stopped, and nothing of the sandbox is left running
    std::atomic<bool> cancel{false};
    F::Options co;
    co.build = built;
    co.build.out_dir = (dir / "cancelled" / "out").string();
    co.build.cancel = &cancel;
    std::thread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        cancel = true;
    });
    const auto t0 = std::chrono::steady_clock::now();
    const B::Result c = F::Run(co);
    stopper.join();
    CHECK(c.stopped);
    CHECK_FALSE(c.ok);
    CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(4));
    std::string left;
    for (int attempt = 0; attempt < 20; ++attempt) { // the kernel takes a moment to reap the sandbox
        left.clear();
        if (FILE* pipe = ::popen("pgrep -f '[e]merald3ds-fallback-launcher|[/]in/launcher.py' 2>&1", "r")) {
            char buf[128];
            while (std::fgets(buf, sizeof buf, pipe)) {
                left += buf;
            }
            ::pclose(pipe);
        }
        if (left.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    CHECK(left.empty());
}
