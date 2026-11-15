// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame crashreport: classifying the last run, scrubbing, showing it once, the zip, the VR
// keyboard's note, and Send Report against a server on this machine (never a real endpoint).

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include "common/frame_crash.h"
#include "core/frame_crash_report.h"
#include "core/frame_crash_send.h"
#include "common/settings.h" // AzaharFrame projdef
#include "frame_xr/menu/crash_note.h"
#include "frame_xr/menu/swkbd_model.h"

namespace CR = Core::CrashReport;
using Common::FrameCrash::Cause;

namespace {

std::string TempDir(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("azaharframe-crash-test-" + std::string(name) + "-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    return dir.string() + "/";
}

void Write(const std::string& path, const std::string& text) {
    std::ofstream(path, std::ios::trunc) << text;
}

bool Exists(const std::string& path) {
    return std::filesystem::exists(path);
}

CR::Scrubber Player() {
    CR::Scrubber s;
    s.home = "/home/player";
    s.user = "player";
    s.host = "player-frame";
    return s;
}

const char* kCrashRecord = "signal=11\ncode=1\naddr=0x0000000000000010\nthread=GPU\n"
                           "pc=0x0000aaaa00001234\nlr=0x0000aaaa00001000\n"
                           "frame=0x0000ffff00000100\nframe=0x0000ffff00000200\n"
                           "frame=0x0000aaaa00001234\nframe=0x0000bbbb00002000\n"
                           "symbols-begin\n"
                           "/home/player/Games/AzaharFrame/app/azahar(+0x100) [0xffff00000100]\n"
                           "linux-vdso.so.1(__kernel_rt_sigreturn+0x0) [0xffff00000200]\n"
                           "/home/player/Games/AzaharFrame/app/azahar(+0x1234) [0xaaaa00001234]\n"
                           "/usr/lib/libQt6Core.so.6(_ZN7QObject5eventEP6QEvent+0x1c) [0xbbbb00002000]\n"
                           "symbols-end\n"
                           "maps-begin\n"
                           "aaaa00000000-aaaa00001000 r--p 00000000 00:1a 77 /home/player/Games/AzaharFrame/app/azahar\n"
                           "aaaa00001000-aaaa00003000 r-xp 00001000 00:1a 77 /home/player/Games/AzaharFrame/app/azahar\n"
                           "ffff00000000-ffff00001000 r-xp 00000000 00:00 0 [vdso]\n"
                           "maps-end\n";

} // namespace

TEST_CASE("Crash report: classification", "[frame_xr]") {
    CR::LastRun run;
    run.marker = true;
    REQUIRE(CR::Classify(run, false) == CR::Kind::Unexpected);
    REQUIRE(CR::Classify(run, true) == CR::Kind::OutOfMemory);
    run.crash = true;
    run.signal = 11;
    REQUIRE(CR::Classify(run, false) == CR::Kind::Crashed);
    REQUIRE(CR::Classify(run, true) == CR::Kind::OutOfMemory); // the launcher's word comes first
    run.cause = Cause::DeviceLost;
    REQUIRE(CR::Classify(run, false) == CR::Kind::DeviceLost);
    run.cause = Cause::CoreFatal;
    REQUIRE(CR::Classify(run, false) == CR::Kind::GameFatal);
    run.cause = Cause::ArmException;
    REQUIRE(CR::Classify(run, false) == CR::Kind::GameFatal);
    run.cause = Cause::Stuck;
    run.crash = false;
    run.signal = 0;
    REQUIRE(CR::Classify(run, false) == CR::Kind::Unexpected);
    REQUIRE(CR::ReasonText(CR::Kind::DeviceLost) == "Graphics Driver Stopped Responding");
    REQUIRE(CR::ReasonText(CR::Kind::GameFatal) == "The Game Hit a Fatal Error");
    REQUIRE(CR::ReasonText(CR::Kind::Crashed) == "Azahar Crashed");
    REQUIRE(CR::ReasonText(CR::Kind::OutOfMemory) == "Ran Out of Memory");
    REQUIRE(CR::ReasonText(CR::Kind::Unexpected) == "Closed Unexpectedly");
}

TEST_CASE("Crash report: the marker round trip", "[frame_xr]") {
    const std::string text = Common::FrameCrash::MarkerText(
        4242, 1731400000, "1.2.0 (HEAD abc)", "azaharframe@1.2.0", 0x0004000000054000,
        "Super Mario 3D Land\nsecond line", "Graphics API: Vulkan\nInternal Resolution: 3x\n",
        false, Cause::DeviceLost, "Device lost during submit");
    CR::LastRun run;
    CR::ParseMarker(text, run);
    REQUIRE(run.pid == 4242);
    REQUIRE(run.started == 1731400000);
    REQUIRE(run.release == "azaharframe@1.2.0");
    REQUIRE(run.title_id == 0x0004000000054000);
    REQUIRE(run.title == "Super Mario 3D Land second line"); // one line
    REQUIRE_FALSE(run.vr);
    REQUIRE(run.cause == Cause::DeviceLost);
    REQUIRE(run.settings.size() == 2);
    REQUIRE(run.settings[1] == "Internal Resolution: 3x");
}

TEST_CASE("Crash report: shown once, another copy's marker left alone", "[frame_xr]") {
    const std::string dir = TempDir("once");
    REQUIRE_FALSE(CR::TakeLastRun(dir, {}).has_value()); // a clean last run
    Write(dir + "running", "pid=999999\nversion=x\n");
    Write(dir + "last-crash", kCrashRecord);
    // the pid still runs (another Azahar): not this run's to report
    REQUIRE_FALSE(CR::TakeLastRun(dir, [](int) { return true; }).has_value());
    REQUIRE(Exists(dir + "running"));
    const auto run = CR::TakeLastRun(dir, [](int) { return false; });
    REQUIRE(run.has_value());
    REQUIRE(run->marker);
    REQUIRE(run->crash);
    REQUIRE(run->signal == 11);
    REQUIRE(run->frames.size() == 4);
    REQUIRE_FALSE(Exists(dir + "running"));
    REQUIRE_FALSE(Exists(dir + "last-crash"));
    REQUIRE_FALSE(CR::TakeLastRun(dir, [](int) { return false; }).has_value()); // once
    std::filesystem::remove_all(dir);
}

TEST_CASE("Crash report: scrubbing paths and user names only", "[frame_xr]") {
    const CR::Scrubber s = Player();
    REQUIRE(s.Apply("/home/player/Documents/Roms/Zelda.3ds") == "~/Documents/Roms/Zelda.3ds");
    REQUIRE(s.Apply("user player on player-frame") == "user <user> on <host>");
    // only whole words: other text stays as it was
    REQUIRE(s.Apply("Players playerbase Mario Kart 7, SteamOS 3.8") ==
            "Players playerbase Mario Kart 7, SteamOS 3.8");
    CR::Scrubber deck;
    deck.home = "/home/steamos";
    deck.user = "steamos"; // SteamOS's own user name is not personal: left as it is
    REQUIRE(deck.Apply("/home/steamos/x steamos-manager") == "~/x steamos-manager");
}

TEST_CASE("Crash report: the report from a crash record", "[frame_xr]") {
    CR::LastRun run;
    CR::ParseMarker("pid=1\nversion=1.2.0\nrelease=azaharframe@1.2.0\ntitle_id=0004000000054000\n"
                    "title=Super Mario 3D Land\nsetting=Graphics API: Vulkan\n"
                    "exe=/home/player/Games/AzaharFrame/app/azahar\n"
                    "exe_build_id=ed30ca89b9992ba9b8a018add351181b6f0e1b19\n",
                    run);
    CR::ParseCrash(kCrashRecord, run);
    const std::string dir = TempDir("build");
    Write(dir + "azahar_log.old.txt",
          "[0.1] Render.Vulkan <Info> VK_DRIVER: turnip Mesa driver 26.2.99\n"
          "[0.2] Render.Vulkan <Info> VK_DEVICE: Turnip Adreno (TM) 750\n"
          "[0.3] Frontend <Info> Booting /home/player/Documents/Roms/sm3dl.3ds\n"
          "[0.4] last line\n");
    CR::Context ctx;
    ctx.log_path = dir + "azahar_log.old.txt";
    ctx.log_lines = 2;
    ctx.scrub = Player();
    const CR::Report r = CR::Build(run, false, ctx);
    REQUIRE(r.kind == CR::Kind::Crashed);
    REQUIRE(r.gpu == "Turnip Adreno (TM) 750"); // found before the tail
    REQUIRE(r.driver == "turnip Mesa driver 26.2.99");
    REQUIRE(r.log_lines == 2);
    REQUIRE(r.log_tail == "[0.3] Frontend <Info> Booting ~/Documents/Roms/sm3dl.3ds\n[0.4] last line\n");
    // the handler's frame and the trampoline go: the stack starts at the faulting pc
    REQUIRE(r.frames.size() == 2);
    REQUIRE(r.frames[0].addr == 0x0000aaaa00001234);
    REQUIRE(r.frames[0].module == 0);
    REQUIRE(r.frames[0].offset == 0x1234);
    REQUIRE(r.frames[1].module == -1); // Qt's mapping is not in this record
    REQUIRE(r.frames[1].symbol == "_ZN7QObject5eventEP6QEvent+0x1c");
    REQUIRE(r.modules.size() == 1);
    REQUIRE(r.modules[0].path == "~/Games/AzaharFrame/app/azahar");
    // the build id from when it ran (the file may be gone or replaced by an update since)
    REQUIRE(r.modules[0].build_id == "ed30ca89b9992ba9b8a018add351181b6f0e1b19");
    const std::string text = CR::FullText(r);
    REQUIRE(text.find("/home/player") == std::string::npos);
    REQUIRE(text.find("Reason: Azahar Crashed") != std::string::npos);
    REQUIRE(text.find("Title ID: 0004000000054000") != std::string::npos);
    REQUIRE(text.find("SIGSEGV (11)") != std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("Crash report: the VR keyboard's note reaches the report, scrubbed", "[frame_xr]") {
    using namespace FrameXr::Swkbd;
    Model m(FrameXr::Menu::CrashNoteSpec("Opened /home/player/Roms"));
    REQUIRE(m.TextUtf8() == "Opened /home/player/Roms"); // what the box had
    REQUIRE(m.GetSpec().multiline);
    m.InsertText(" and player's pack");
    REQUIRE(m.NewLine());
    m.InsertText("then it froze");
    REQUIRE(m.Backspace());
    m.InsertText("e");
    // Clear empties the field and keeps the keyboard open
    {
        Model c(FrameXr::Menu::CrashNoteSpec("something"));
        REQUIRE_FALSE(c.PressButton(1).has_value());
        REQUIRE(c.TextUtf8().empty());
    }
    // Done (the third button) gives the text back
    const auto done = m.PressButton(2);
    REQUIRE(done.has_value());
    REQUIRE(done->button == 2);
    REQUIRE(done->text == "Opened /home/player/Roms and player's pack\nthen it froze");
    CR::Report r;
    r.reason = "Azahar Crashed";
    r.note = Player().Apply(done->text);
    REQUIRE(r.note == "Opened ~/Roms and <user>'s pack\nthen it froze");
    const std::string text = CR::ReportText(r);
    REQUIRE(text.find("What Were You Doing?\n  Opened ~/Roms and <user>'s pack\n  then it froze\n") !=
            std::string::npos);
    // about 500 characters at most
    Model cap(FrameXr::Menu::CrashNoteSpec(std::string(600, 'a')));
    REQUIRE(cap.Length() == FrameXr::Menu::kCrashNoteMax);
    REQUIRE_FALSE(cap.Insert(U'b'));
}

TEST_CASE("Crash report: the zip", "[frame_xr]") {
    const std::string zip = CR::MakeZip({{"report.txt", "hello"}, {"azahar_log.txt", ""}}, 1731400000);
    REQUIRE(zip.substr(0, 4) == std::string("PK\x03\x04", 4));
    REQUIRE(zip.find("report.txt") != std::string::npos);
    // the end record: 2 entries
    const std::size_t end = zip.size() - 22;
    REQUIRE(zip.substr(end, 4) == std::string("PK\x05\x06", 4));
    REQUIRE(static_cast<unsigned char>(zip[end + 10]) == 2);
    // CRC-32 of "hello" in the local header
    const unsigned crc = static_cast<unsigned char>(zip[14]) |
                         (static_cast<unsigned char>(zip[15]) << 8) |
                         (static_cast<unsigned char>(zip[16]) << 16) |
                         (static_cast<unsigned>(static_cast<unsigned char>(zip[17])) << 24);
    REQUIRE(crc == 0x3610a686u);
    const std::string dir = TempDir("zip");
    std::string error;
    const std::string path = CR::SaveZip(CR::Report{}, dir + "Reports", 1731400000, &error);
    REQUIRE_FALSE(path.empty());
    REQUIRE(Exists(path));
    REQUIRE(path.find("azaharframe-report-") != std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("Crash report: debug ids", "[frame_xr]") {
    REQUIRE(CR::DebugId("00112233445566778899aabbccddeeff00112233") ==
            "33221100-5544-7766-8899-aabbccddeeff");
}

TEST_CASE("Crash report: modules are files, never devices", "[frame_xr]") {
    // A GPU buffer maps /dev/dri with a huge fake offset; reading it for a build id blocks.
    const char* maps =
        "aaaa00000000-aaaa00001000 r--p 00000000 00:1a 77 /usr/bin/azahar\n"
        "aaaa00001000-aaaa00003000 r-xp 00001000 00:1a 77 /usr/bin/azahar\n"
        "ffff10000000-ffff10100000 rw-s 1a0000000 00:06 512 /dev/dri/renderD128\n"
        "ffff20000000-ffff20001000 rw-s 00000000 00:01 9 /memfd:shader (deleted)\n"
        "ffff30000000-ffff30001000 r--p 00000000 00:1a 0 \n";
    std::vector<CR::MapRange> ranges;
    const auto mods = CR::ParseMaps(maps, &ranges);
    REQUIRE(mods.size() == 1);
    REQUIRE(mods[0].path == "/usr/bin/azahar");
    REQUIRE(mods[0].base == 0xaaaa00000000ULL);
    REQUIRE(mods[0].end == 0xaaaa00003000ULL);
    REQUIRE(ranges.size() == 2);
    REQUIRE(CR::ReadBuildId("/dev/null").empty());
    REQUIRE(CR::ReadBuildId("/dev/zero").empty()); // returns at once
}

TEST_CASE("Crash report: Send Report to a server on this machine", "[frame_xr]") {
    CR::Report r;
    r.kind = CR::Kind::Crashed;
    r.headline = "Azahar Closed Unexpectedly";
    r.reason = "Azahar Crashed";
    r.crash_line = "SIGSEGV (11), code 1, address 0x10";
    r.exception = "Sentry test error (unit test)";
    r.frames = {{0x1234, 0, 0x234, ""}, {0x1100, 0, 0x100, "QObject::event+0x1c"}};
    r.modules = {{"~/app/azahar", 0x1000, 0x9000, "00112233445566778899aabbccddeeff"}};
    r.note = "It froze in the menu";
    r.log_tail = "[1.0] Frontend <Info> last line\n";
    r.release = "azaharframe@test";

    httplib::Server server;
    std::string body, auth, path;
    std::atomic<int> posts{0};
    server.Post(R"(/api/(\d+)/envelope/)", [&](const httplib::Request& req, httplib::Response& res) {
        body = req.body;
        auth = req.get_header_value("X-Sentry-Auth");
        path = req.path;
        posts++;
        res.set_content(R"({"id":"0123"})", "application/json");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    std::thread t([&] { server.listen_after_bind(); });
    const std::string dir = TempDir("send");
    const CR::SendResult result =
        CR::Send(r, "http://publickey@127.0.0.1:" + std::to_string(port) + "/42", dir);
    server.stop();
    t.join();
    if (result.error == "Sending Isn't Built In") {
        SKIP("Sentry is not built in");
    }
    REQUIRE(result.ok);
    REQUIRE(posts == 1);
    REQUIRE(path == "/api/42/envelope/");
    REQUIRE(auth.find("sentry_key=publickey") != std::string::npos);
    REQUIRE(body.find("Sentry test error (unit test)") != std::string::npos);
    REQUIRE(body.find("It froze in the menu") != std::string::npos);  // the note
    REQUIRE(body.find("\"filename\":\"report.txt\"") != std::string::npos);
    REQUIRE(body.find("\"filename\":\"azahar_log.txt\"") != std::string::npos);
    REQUIRE(body.find("33221100-5544-7766-8899-aabbccddeeff") != std::string::npos);
    REQUIRE(body.find("azaharframe@test") != std::string::npos);
    REQUIRE(body.find("\"infer_ip\":\"never\"") != std::string::npos); // no IP, no place
    REQUIRE(body.find("\"function\":\"QObject::event\"") != std::string::npos);
    REQUIRE(body.find("\"package\":\"~/app/azahar\"") != std::string::npos);
    // nothing of Sentry's own about this machine or person
    REQUIRE(body.find("ip_address") == std::string::npos);
    REQUIRE(body.find("kernel_version") == std::string::npos);
    REQUIRE(body.find("\"user\":{\"geo\":{}}") != std::string::npos); // an empty place, no more
    REQUIRE(body.find("server_name") == std::string::npos);
    REQUIRE_FALSE(Exists(dir + "/sentry-db")); // its database went with it
    std::filesystem::remove_all(dir);
}

// AzaharFrame crashreport: Report a Problem

TEST_CASE("Problem report: no crash fields, no reason", "[frame_xr]") {
    CR::LastRun now;
    now.marker = true;
    now.version = "1.2.0 (steam-frame abc)";
    now.release = "azaharframe@1.2.0";
    now.title_id = 0x0004000000054000;
    now.title = "Super Mario 3D Land";
    now.vr = true;
    // a crash record's fields left in by mistake are not reported
    CR::ParseCrash(kCrashRecord, now);
    now.cause = Cause::DeviceLost;
    now.detail = "Device lost";
    const std::string dir = TempDir("manual");
    Write(dir + "azahar_log.txt", "[0.1] Render.Vulkan <Info> VK_DEVICE: Turnip Adreno (TM) 750\n"
                                  "[0.2] Frontend <Info> Booting /home/player/Documents/Roms/sm3dl.3ds\n");
    CR::Context ctx;
    ctx.log_path = dir + "azahar_log.txt";
    ctx.settings_now = {"Graphics API: Vulkan"};
    ctx.scrub = Player();
    CR::Report r = CR::BuildManual(now, ctx);
    REQUIRE(r.kind == CR::Kind::Manual);
    REQUIRE(r.headline == "Report a Problem");
    REQUIRE(r.reason.empty());
    REQUIRE(r.detail.empty());
    REQUIRE(r.crash_line.empty());
    REQUIRE(r.exception.empty());
    REQUIRE(r.frames.empty());
    REQUIRE(r.modules.empty());
    REQUIRE(r.title == "Super Mario 3D Land");
    REQUIRE(r.gpu == "Turnip Adreno (TM) 750");
    REQUIRE(r.settings == std::vector<std::string>{"Graphics API: Vulkan"});
    REQUIRE(CR::KindKey(r.kind) == "manual");
    r.note = "Flickers on the map";
    const std::string text = CR::FullText(r);
    REQUIRE(text.rfind("Azahar for Frame Problem Report", 0) == 0);
    REQUIRE(text.find("Reason:") == std::string::npos);
    REQUIRE(text.find("\nCrash\n") == std::string::npos);
    REQUIRE(text.find("What Happened?\n  Flickers on the map") != std::string::npos);
    REQUIRE(text.find("What Were You Doing?") == std::string::npos);
    REQUIRE(text.find("~/Documents/Roms/sm3dl.3ds") != std::string::npos); // the log, scrubbed
    REQUIRE(text.find("/home/player") == std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("Problem report: the screenshot only when included", "[frame_xr]") {
    CR::Report r;
    r.kind = CR::Kind::Manual;
    r.headline = "Report a Problem";
    r.note = "It froze";
    auto files = CR::Files(r);
    REQUIRE(files.size() == 2);
    REQUIRE(CR::ReportText(r).find("Screenshot") == std::string::npos);
    r.screenshot_png = std::string("\x89PNG\r\n\x1a\n\0\0binary", 16);
    files = CR::Files(r);
    REQUIRE(files.size() == 3);
    REQUIRE(files[2].first == "screenshot.png");
    REQUIRE(files[2].second == r.screenshot_png); // byte for byte, NULs included
    REQUIRE(CR::ReportText(r).find("Screenshot: Included (screenshot.png)") != std::string::npos);
    const std::string zip = CR::MakeZip(files, 1731400000);
    REQUIRE(zip.find("screenshot.png") != std::string::npos);
    REQUIRE(zip.find(r.screenshot_png) != std::string::npos); // stored, not compressed
}

TEST_CASE("Problem report: what the player typed, scrubbed and titled", "[frame_xr]") {
    const CR::Scrubber sc = Player();
    // only paths and user names change; the rest is the player's own words
    REQUIRE(sc.Apply("My save in /home/player/saves broke, player here, on player-frame. Help!") ==
            "My save in ~/saves broke, <user> here, on <host>. Help!");
    REQUIRE(CR::ManualTitle("Sentry test feedback 2026-11-12T17:00:00Z\nmore") ==
            "Sentry test feedback 2026-11-12T17:00:00Z");
    REQUIRE(CR::ManualTitle("") == "Problem Report");
    REQUIRE(CR::ManualTitle("   ") == "Problem Report");
    const std::string longer(200, 'a');
    REQUIRE(CR::ManualTitle(longer).size() == 120);
    // a cut never splits a UTF-8 character
    std::string accents;
    for (int i = 0; i < 80; i++) {
        accents += "\xc3\xa9"; // é
    }
    const std::string t = CR::ManualTitle(accents);
    REQUIRE(t.size() <= 120);
    REQUIRE((t.size() - 3) % 2 == 0); // whole two-byte characters only
    REQUIRE(t.substr(t.size() - 3) == "...");
}

TEST_CASE("Problem report: Send Report as a warning, tagged manual", "[frame_xr]") {
    CR::Report r;
    r.kind = CR::Kind::Manual;
    r.headline = "Report a Problem";
    r.note = "Sentry test feedback (unit test)\nThe map flickers";
    r.title = "Super Mario 3D Land";
    r.settings = {"Graphics API: Vulkan", "Internal Resolution: 3x"};
    r.log_tail = "[1.0] Frontend <Info> last line\n";
    r.screenshot_png = "\x89PNG fake";
    r.release = "azaharframe@test";
    httplib::Server server;
    std::string body;
    server.Post(R"(/api/(\d+)/envelope/)", [&](const httplib::Request& req, httplib::Response& res) {
        body = req.body;
        res.set_content(R"({"id":"0123"})", "application/json");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    std::thread t([&] { server.listen_after_bind(); });
    const std::string dir = TempDir("send-manual");
    const CR::SendResult result =
        CR::Send(r, "http://publickey@127.0.0.1:" + std::to_string(port) + "/42", dir);
    server.stop();
    t.join();
    if (result.error == "Sending Isn't Built In") {
        SKIP("Sentry is not built in");
    }
    REQUIRE(result.ok);
    REQUIRE(body.find("\"level\":\"warning\"") != std::string::npos);
    REQUIRE(body.find("\"report\":\"manual\"") != std::string::npos);
    REQUIRE(body.find("\"formatted\":\"Sentry test feedback (unit test)\"") != std::string::npos);
    REQUIRE(body.find("\"exception\"") == std::string::npos); // nothing crashed
    REQUIRE(body.find("Internal Resolution: 3x") != std::string::npos);
    REQUIRE(body.find("\"filename\":\"screenshot.png\"") != std::string::npos);
    REQUIRE(body.find("\"filename\":\"azahar_log.txt\"") != std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("Crash report: IP and MAC addresses out of the log", "[frame_xr]") {
    REQUIRE(CR::ScrubNetwork("Connected to 192.168.0.12, room 10.0.0.1:24872.") ==
            "Connected to <ip>, room <ip>:24872.");
    REQUIRE(CR::ScrubNetwork("wlan0 a4:5e:60:c2:11:9f up, peer A4-5E-60-C2-11-9F") ==
            "wlan0 <mac> up, peer <mac>");
    REQUIRE(CR::ScrubNetwork("via fe80::1c2b:3aff:fe4d:5e6f and 2001:db8:0:0:0:0:2:1 and ::1") ==
            "via <ip> and <ip> and <ip>");
    // left alone: times, versions, hashes, C++ names, timestamps, file names
    const std::string keep = "[  33.459260] 12:30:45 Mesa 26.2.99 v1.2.0 0xdeadbeef "
                             "Core::System::Run cafe:babe 2026-11-12 Pushmo.cci 999.1.1.1";
    REQUIRE(CR::ScrubNetwork(keep) == keep);
}

// AzaharFrame kiumenus: Angel's Aim's Send Report payload
#include "core/frame_angels_report.h"

TEST_CASE("Angel's Aim report: the payload has the fields and nothing of the machine",
          "[crashreport][kiumenus]") {
    using namespace Core::AngelsReport;
    Info info;
    info.app_version = "1.1.1-524 test";
    info.title_id = 0x0004000000030600;
    info.title = "Kid Icarus: Uprising";
    info.region = "Europe";
    info.game_version = "1.0";
    info.mod_installed = true;
    info.aiming = 2;
    info.weapon_in_hand = true;
    info.laser_sight = true;
    info.recoil = true;
    info.recoil_charged = 100;
    info.recoil_rapid = 50;
    info.immersive = true;
    Core::CrashReport::Scrubber scrub;
    scrub.home = "/home/someone";
    scrub.user = "someone";
    scrub.host = "somebox";
    const auto f = BuildPayload(Type::Bug,
                                "The laser is off at /home/someone/Games/x on somebox, by someone\nmore",
                                info, scrub);
    const auto tag = [&](const std::string& k) -> std::string {
        for (const auto& [key, v] : f.tags) {
            if (key == k) {
                return v;
            }
        }
        return "<none>";
    };
    CHECK(tag("area") == "angels-aim");
    CHECK(tag("type") == "bug");
    CHECK(tag("title_id") == "0004000000030600");
    CHECK(tag("game_region") == "Europe");
    CHECK(tag("cstick_gyro_mod") == "installed");
    CHECK(tag("aiming") == "Direct Aim");
    CHECK(tag("immersive") == "On");
    CHECK(f.message.rfind("Angel's Aim Bug: ", 0) == 0);
    std::string all = f.message + f.comments;
    for (const auto& [k, v] : f.tags) {
        all += k + v;
    }
    bool settings = false, version = false;
    for (const auto& [k, v] : f.extra) {
        all += k + v;
        settings |= k == "settings" && v.find("Laser Sight: On") != std::string::npos &&
                    v.find("Immersive Mode: On") != std::string::npos &&
                    v.find("Weapon in Hand: On") != std::string::npos;
        version |= k == "app_version" && v == "1.1.1-524 test";
    }
    CHECK(settings);
    CHECK(version);
    CHECK(all.find("/home/someone") == std::string::npos);
    CHECK(all.find("somebox") == std::string::npos);
    CHECK(all.find("someone") == std::string::npos);
    CHECK(f.comments.find("more") != std::string::npos);
    // an idea with no words: a title only, no feedback text
    const auto idea = BuildPayload(Type::Idea, "  \n", info, scrub);
    CHECK(idea.message == "Angel's Aim Idea");
    CHECK(idea.comments.empty());
    CHECK(std::string(TypeKey(Type::Idea)) == "idea");
    // long text is cut
    CHECK(BuildPayload(Type::Bug, std::string(5000, 'a'), info, scrub).comments.size() == 2000);
}

// AzaharFrame projdef: Send Report's settings say whether Motion Clarity is on in the Full modes.
TEST_CASE("Crash report: the settings list Motion Clarity in Immersive Modes", "[frame_xr][projdef]") {
    auto& s = Settings::values.vr_motion_clarity_immersive;
    const bool was = s.GetValue();
    s.SetValue(false);
    std::string text = CR::SettingsSummary();
    CHECK(text.find("Motion Clarity: ") != std::string::npos);
    CHECK(text.find("Motion Clarity in Immersive Modes: Off\n") != std::string::npos);
    s.SetValue(true);
    text = CR::SettingsSummary();
    CHECK(text.find("Motion Clarity in Immersive Modes: On\n") != std::string::npos);
    s.SetValue(was);
}
