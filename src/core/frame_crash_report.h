// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame crashreport: the next start's side of common/frame_crash.h. No Qt.
//
// TakeLastRun reads (and removes) the run marker and the crash record a run left behind when it
// did not end cleanly; Classify names the reason in plain words; Build makes the report: the
// reason, the game, the version, the GPU and driver (from the run's log), the settings the game
// ran with, the crash's stack and the modules it went through, and the last lines of the log,
// all scrubbed (the home folder becomes ~, the user name <user>). What the report holds is what
// "See What's Sent" shows (FullText), what Save Report puts in the zip and what Send Report sends
// (core/frame_crash_send.h). Nothing is sent without the player's tap.

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "common/frame_crash.h"

namespace Core::CrashReport {

/// What the last run left: the marker (it did not end cleanly) and maybe a crash record.
struct LastRun {
    bool marker = false; ///< the run marker was there
    bool crash = false;  ///< a fatal signal wrote a record
    int pid = 0;
    long long started = 0; ///< seconds since the epoch
    std::string version;
    std::string release; ///< "azaharframe@1.2.0", or the commit for a development build
    bool vr = true;
    u64 title_id = 0;
    std::string title;
    std::vector<std::string> settings; ///< "Label: value"
    Common::FrameCrash::Cause cause = Common::FrameCrash::Cause::None;
    std::string detail;
    std::string exe;          ///< the program's file, as it ran
    std::string exe_build_id; ///< its build id then (hex)
    // The crash record
    int signal = 0;
    int code = 0;
    u64 addr = 0;
    std::string thread;
    std::string exception;
    u64 pc = 0;
    u64 lr = 0;
    std::vector<u64> frames;          ///< newest first
    std::vector<std::string> symbols; ///< backtrace_symbols lines, in the frames' order
    std::string maps;                 ///< /proc/self/maps at the crash
};

/// Parses a marker (key=value lines) into `run`.
void ParseMarker(std::string_view text, LastRun& run);
/// Parses a crash record into `run`.
void ParseCrash(std::string_view text, LastRun& run);

/// `dir` ends with '/'. The last run's marker and crash record, removed once read, so a run is
/// reported once. Nothing when the last run ended cleanly, or when the marker belongs to a
/// process that still runs (`alive(pid)`: another copy of Azahar; its files are left alone).
std::optional<LastRun> TakeLastRun(const std::string& dir,
                                   const std::function<bool(int)>& alive);
/// True when `pid` is a running Azahar other than this process.
bool PidIsAzahar(int pid);

enum class Kind {
    OutOfMemory, ///< the launcher saw the kernel kill it at its memory limit
    DeviceLost,  ///< Vulkan lost the device
    GameFatal,   ///< ERR:F, an unknown core error or a CPU exception, then the end
    Crashed,     ///< a fatal signal
    Unexpected,  ///< none of these: it just did not end cleanly (killed, frozen, power)
    Manual,      ///< AzaharFrame crashreport: Report a Problem (BuildManual), no crash
};

Kind Classify(const LastRun& run, bool out_of_memory);
/// "Ran Out of Memory", "Graphics Driver Stopped Responding", "The Game Hit a Fatal Error",
/// "Azahar Crashed", "Closed Unexpectedly".
std::string ReasonText(Kind kind);
/// "out-of-memory", "device-lost", "game-fatal", "crashed", "unexpected" (a tag in the report).
std::string_view KindKey(Kind kind);
/// "SIGSEGV" and the like ("signal 42" for others).
std::string SignalName(int signal);

/// What the report's paths and text are scrubbed of.
struct Scrubber {
    std::string home; ///< becomes "~"
    std::string user; ///< becomes "<user>" (not "steamos" or "deck": SteamOS's own names)
    std::string host; ///< becomes "<host>"
    std::string Apply(std::string_view text) const;
    /// From $HOME, the login name and the host name of this machine.
    static Scrubber ForThisMachine();
};

/// AzaharFrame crashreport: IPv4, IPv6 and MAC addresses become "<ip>" and "<mac>" (for the log
/// tail: a network log line can name the console's or the PC's address).
std::string ScrubNetwork(std::string_view text);

/// One mapping of /proc/self/maps that holds a file.
struct Module {
    std::string path; ///< as mapped (scrubbed in a report)
    u64 base = 0;     ///< the start of its mapping at offset 0
    u64 end = 0;      ///< the end of its last mapping
    std::string build_id; ///< hex, from the file on disk ("" when it is gone or has none)
};
/// One mapping of a module: [start, end), and the module's index in ParseMaps' list.
struct MapRange {
    u64 start = 0, end = 0;
    std::size_t module = 0;
};
/// The modules of a maps text: regular files only (no devices such as /dev/dri, whose reads
/// block), each from its mapping at offset 0. `ranges` gets every mapping of them.
std::vector<Module> ParseMaps(std::string_view maps, std::vector<MapRange>* ranges = nullptr);
/// The GNU build id of an ELF file, as hex; "" when there is none.
std::string ReadBuildId(const std::string& path);
/// Sentry's debug id for an ELF build id: its first 16 bytes as a GUID, the first three fields
/// byte-swapped (as Sentry and breakpad read them).
std::string DebugId(std::string_view build_id_hex);

struct Frame {
    u64 addr = 0;
    int module = -1; ///< into Report::modules
    u64 offset = 0;  ///< addr - the module's base
    std::string symbol; ///< "QWidget::event+0x1c" from backtrace_symbols ("" when none)
};

struct Report {
    Kind kind = Kind::Unexpected;
    std::string headline; ///< "Azahar Closed Unexpectedly", or memguard's "Azahar Ran Out of Memory"
    std::string body;     ///< supporting text (memguard's for out of memory), "" none
    std::string reason;   ///< ReasonText
    std::string detail;   ///< what was noted ("Device lost during submit"), "" none
    u64 title_id = 0;
    std::string title; ///< "" no game
    bool vr = true;
    long long started = 0;
    std::string version; ///< the run's
    std::string release; ///< the run's, for Sentry
    std::string os;
    std::string gpu, driver;
    std::vector<std::string> settings;
    std::vector<std::string> clues; ///< "Label: value" lines (mods, Load Anyway)
    std::string crash_line;         ///< "SIGSEGV (11) at 0x..., thread GPU" ("" no record)
    std::string exception;
    std::vector<Frame> frames;  ///< newest first
    std::vector<Module> modules; ///< the ones the frames are in
    std::string log_tail;
    std::size_t log_lines = 0;
    std::string note; ///< "What Were You Doing?" (a manual report: "What Happened?")
    std::string screenshot_png; ///< a manual report's top and bottom screens, "" none
};

/// What Build reads besides the run.
struct Context {
    std::string log_path; ///< the run's log (azahar_log.old.txt once this run's log started)
    std::size_t log_lines = 2000;
    std::string os;       ///< "SteamOS 3.8.0 (Linux 6.11.11)"
    /// The settings now, for a run that had no game (its marker has none).
    std::vector<std::string> settings_now;
    std::vector<std::string> clues;
    Scrubber scrub;
};

Report Build(const LastRun& run, bool out_of_memory, const Context& ctx);

/// Report a Problem: the report of this run as it is now (`now`: the game, the mode, the version;
/// no crash), with this run's log (ctx.log_path) and no reason. The screenshot is the caller's.
Report BuildManual(const LastRun& now, const Context& ctx);

/// The last `lines` lines of the file, and the GPU and driver lines found anywhere in it
/// ("VK_DEVICE: ...", "VK_DRIVER: ...").
struct LogRead {
    std::string tail;
    std::size_t lines = 0;
    std::string gpu, driver;
};
LogRead ReadLog(const std::string& path, std::size_t lines);

/// This machine's OS line, from /etc/os-release and uname.
std::string OsText();

/// The report as text without the log (report.txt), then the log after its heading
/// (FullText: what "See What's Sent" shows).
std::string ReportText(const Report& r);
std::string FullText(const Report& r);
/// The files of a saved or sent report: report.txt and azahar_log.txt, and screenshot.png when
/// the player included one.
std::vector<std::pair<std::string, std::string>> Files(const Report& r);

/// A zip of `files`, stored (no compression).
std::string MakeZip(const std::vector<std::pair<std::string, std::string>>& files,
                    long long unix_time);
/// "azaharframe-report-20261112-153012.zip" (local time).
std::string ZipName(long long unix_time);
/// Writes the report's zip into `dir` (made when missing). The zip's path, or "" and `error`.
std::string SaveZip(const Report& r, const std::string& dir, long long unix_time,
                    std::string* error);

/// The main settings, "Label: value" lines, from Settings::values now (per game once a game's
/// settings are loaded).
std::string SettingsSummary();

} // namespace Core::CrashReport
