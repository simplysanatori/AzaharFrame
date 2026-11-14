// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame autosave: see autosave.h.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <map>
#include <system_error>
#include <utility>
#include <fmt/format.h>
#include <lodepng.h>
#include "common/atomic_file.h"
#include "common/logging/log.h"
#include "common/zstd_compression.h"
#include "core/autosave.h"
#include "core/savestate.h"

namespace Core::Autosave {

namespace fs = std::filesystem;

namespace {

constexpr std::string_view kStateExt = ".cst";
constexpr std::string_view kThumbExt = ".png";
constexpr std::size_t kStampLen = 15; // "20261112-101405"

bool EndsWith(std::string_view s, std::string_view end) {
    return s.size() >= end.size() && s.substr(s.size() - end.size()) == end;
}

bool StartsWith(std::string_view s, std::string_view start) {
    return s.size() >= start.size() && s.substr(0, start.size()) == start;
}

bool IsStamp(std::string_view s) {
    if (s.size() != kStampLen || s[8] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < s.size(); i++) {
        if (i != 8 && (s[i] < '0' || s[i] > '9')) {
            return false;
        }
    }
    return true;
}

/// "<prefix><stamp><ext>": the prefix (the game's key), or "" when the name is not an autosave's.
std::string KeyOf(std::string_view name, std::string_view ext) {
    if (!EndsWith(name, ext) || name.size() < ext.size() + kStampLen + 1) {
        return {};
    }
    const std::string_view stem = name.substr(0, name.size() - ext.size());
    const std::string_view stamp = stem.substr(stem.size() - kStampLen);
    if (!IsStamp(stamp) || stem[stem.size() - kStampLen - 1] != '.') {
        return {};
    }
    return std::string(stem.substr(0, stem.size() - kStampLen));
}

void Remove(const fs::path& p) {
    std::error_code ec;
    if (fs::remove(p, ec)) {
        LOG_INFO(Core, "autosave: removed {}", p.filename().string());
    }
}

std::tm LocalTm(s64 unix_seconds) {
    const std::time_t t = static_cast<std::time_t>(unix_seconds);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

/// Days since 1970-01-01 of a civil date (H. Hinnant's days_from_civil).
s64 DaysFromCivil(s64 y, unsigned m, unsigned d) {
    y -= m <= 2;
    const s64 era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<s64>(doe) - 719468;
}

s64 LocalDay(s64 unix_seconds) {
    const std::tm tm = LocalTm(unix_seconds);
    return DaysFromCivil(tm.tm_year + 1900, static_cast<unsigned>(tm.tm_mon + 1),
                         static_cast<unsigned>(tm.tm_mday));
}

std::string Format(u64 unix_seconds, const char* f) {
    const std::tm tm = LocalTm(static_cast<s64>(unix_seconds));
    char buf[40];
    std::strftime(buf, sizeof(buf), f, &tm);
    return buf;
}

s64 Ms(std::chrono::steady_clock::duration d) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}

} // namespace

u32 ClampEvery(u32 minutes) {
    return EveryAt(EveryIndex(minutes));
}

u32 ClampKeep(u32 keep) {
    return std::clamp(keep, kKeepMin, kKeepMax);
}

int EveryIndex(u32 minutes) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(kEveryMinutes.size()); i++) {
        const auto d = [&](int j) {
            return std::abs(static_cast<int>(kEveryMinutes[static_cast<std::size_t>(j)]) -
                            static_cast<int>(minutes));
        };
        if (d(i) < d(best)) {
            best = i;
        }
    }
    return best;
}

u32 EveryAt(int index) {
    return kEveryMinutes[static_cast<std::size_t>(
        std::clamp(index, 0, static_cast<int>(kEveryMinutes.size()) - 1))];
}

const char* ReasonName(Reason reason) {
    switch (reason) {
    case Reason::Timer:
        return "timer";
    case Reason::MenuOpen:
        return "menu opened";
    case Reason::Quit:
        return "quit";
    }
    return "?";
}

// ---- Files ----------------------------------------------------------------------------------

std::string Dir(const std::string& states_dir) {
    std::string dir = states_dir;
    if (!dir.empty() && dir.back() != '/') {
        dir += '/';
    }
    return dir + "autosave/";
}

std::string Prefix(u64 program_id, u64 movie_id) {
    return movie_id ? fmt::format("{:016X}.movie{:016X}.", program_id, movie_id)
                    : fmt::format("{:016X}.", program_id);
}

std::string PathFor(const std::string& dir, u64 program_id, u64 movie_id, s64 unix_seconds) {
    const std::time_t t = static_cast<std::time_t>(unix_seconds);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm);
    return dir + Prefix(program_id, movie_id) + stamp + std::string(kStateExt);
}

std::string MenuPathFor(const std::string& dir, u64 program_id, u64 movie_id) {
    return dir + Prefix(program_id, movie_id) + std::string(kMenuOpenName) + std::string(kStateExt);
}

std::string ThumbPathFor(const std::string& state_path) {
    if (!EndsWith(state_path, kStateExt)) {
        return state_path + std::string(kThumbExt);
    }
    return state_path.substr(0, state_path.size() - kStateExt.size()) + std::string(kThumbExt);
}

std::vector<Entry> List(const std::string& dir, u64 program_id, u64 movie_id) {
    const std::string prefix = Prefix(program_id, movie_id);
    const std::string menu_name = prefix + std::string(kMenuOpenName) + std::string(kStateExt);
    std::vector<std::pair<Entry, std::string>> found; // the entry, its name
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        const std::string name = e.path().filename().string();
        const bool menu = name == menu_name; // AzaharFrame advfix
        if ((!menu && KeyOf(name, kStateExt) != prefix) || !e.is_regular_file(ec)) {
            continue;
        }
        const std::string path = e.path().string();
        const SaveStateInfo info = GetSaveStateInfoAt(path, program_id);
        if (info.slot != 0) {
            continue; // not a state of this game
        }
        Entry entry;
        entry.path = path;
        const std::string thumb = ThumbPathFor(path);
        entry.thumb = fs::exists(thumb, ec) ? thumb : std::string{};
        entry.time = info.time;
        entry.menu = menu;
        entry.other = info.status != SaveStateInfo::ValidationStatus::OK;
        if (entry.other) {
            entry.version = FrameVersionOf(info.build_name, info.build_version);
        }
        found.emplace_back(std::move(entry), name);
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
        return a.first.time != b.first.time ? a.first.time > b.first.time : a.second > b.second;
    });
    std::vector<Entry> out;
    out.reserve(found.size());
    for (auto& [entry, name] : found) {
        out.push_back(std::move(entry));
    }
    return out;
}

std::size_t Prune(const std::string& dir, u64 program_id, u64 movie_id, u32 keep) {
    keep = ClampKeep(keep);
    std::vector<Entry> all = List(dir, program_id, movie_id);
    // AzaharFrame advfix: the menu-open autosave is not one of the rotation
    std::erase_if(all, [](const Entry& e) { return e.menu; });
    std::size_t removed = 0;
    for (std::size_t i = keep; i < all.size(); i++) {
        Remove(all[i].path);
        Remove(ThumbPathFor(all[i].path));
        removed++;
    }
    // Pictures whose state is gone, and a write cut short (".tmp").
    const std::string prefix = Prefix(program_id, movie_id);
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        const std::string name = e.path().filename().string();
        if (KeyOf(name, kThumbExt) == prefix) {
            const std::string state =
                e.path().string().substr(0, e.path().string().size() - kThumbExt.size()) +
                std::string(kStateExt);
            if (!fs::exists(state, ec)) {
                Remove(e.path());
            }
        } else if (StartsWith(name, prefix) && EndsWith(name, ".tmp")) {
            Remove(e.path());
        }
    }
    return removed;
}

std::size_t PruneAll(const std::string& dir, u32 keep) {
    keep = ClampKeep(keep);
    std::map<std::string, std::vector<std::string>> games; // key: the names (stamps sort by time)
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        const std::string name = e.path().filename().string();
        const std::string key = KeyOf(name, kStateExt);
        if (!key.empty()) {
            games[key].push_back(name);
        }
    }
    std::size_t removed = 0;
    for (auto& [key, names] : games) {
        std::sort(names.rbegin(), names.rend());
        for (std::size_t i = keep; i < names.size(); i++) {
            const std::string path = dir + names[i];
            Remove(path);
            Remove(ThumbPathFor(path));
            removed++;
        }
    }
    return removed;
}

std::string EntryId(const Entry& entry) {
    return fmt::format("{}@{}", fs::path(entry.path).filename().string(), entry.time);
}

int FindEntry(const std::vector<Entry>& entries, const std::string& id) {
    for (std::size_t i = 0; i < entries.size(); i++) {
        if (!id.empty() && EntryId(entries[i]) == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool SameState(const std::string& path, u64 program_id, u64 time) {
    const SaveStateInfo info = GetSaveStateInfoAt(path, program_id);
    return info.slot == 0 && (time == 0 || info.time == time);
}

std::vector<Entry> Shown(std::vector<Entry> all, u32 keep) {
    keep = ClampKeep(keep);
    std::vector<Entry> out;
    std::size_t rotation = 0;
    for (auto& e : all) {
        if (e.menu || rotation++ < keep) {
            out.push_back(std::move(e));
        }
    }
    return out;
}

// ---- When ------------------------------------------------------------------------------------

void Scheduler::Boot(s64 now_ms) {
    booted = true;
    boot_ms = now_ms;
    fired_any = false;
    play_ms = 0;
    ticked = false;
}

void Scheduler::Stop() {
    booted = false;
    play_ms = 0;
    ticked = false;
}

void Scheduler::Tick(s64 now_ms, bool playing) {
    if (booted && playing && ticked) {
        // A long gap (the machine slept, a hitch) counts as at most two seconds.
        play_ms += std::clamp<s64>(now_ms - last_tick_ms, 0, 2000);
    }
    last_tick_ms = now_ms;
    ticked = true;
}

std::string Scheduler::Blocked(Reason reason, s64 now_ms, const Gates& g) const {
    if (!booted || !g.game) {
        return "no game";
    }
    if (g.memory_low) { // AzaharFrame memwarn: first, so the log says why; the quit's too (Stop
                        // Game)
        return "memory is almost full";
    }
    if (now_ms - boot_ms < grace) {
        return "the game started less than a minute ago";
    }
    if (fired_any && now_ms - last_fire_ms < throttle) {
        return "one was made less than a minute ago";
    }
    if (g.loading_state) {
        return "a state is being saved or loaded";
    }
    if (g.shaders_busy) {
        return "shaders are being prepared";
    }
    switch (reason) {
    case Reason::Timer:
        if (!g.playing || g.paused) {
            return "the game is paused";
        }
        if (!g.frames_recent) {
            return "the game is not running";
        }
        break;
    case Reason::MenuOpen:
        // Asked for as the menu opens, before the game pauses.
        if (!g.frames_recent) {
            return "the game is not running";
        }
        break;
    case Reason::Quit:
        // Closed from the menu or while paused: the paused game is a sound state too.
        if (!g.paused && !g.frames_recent) {
            return "the game is not running";
        }
        break;
    }
    return {};
}

void Scheduler::Fired(s64 now_ms) {
    fired_any = true;
    last_fire_ms = now_ms;
    play_ms = 0;
}

// ---- Failures --------------------------------------------------------------------------------

const char* FailureName(Failure failure) {
    switch (failure) {
    case Failure::None:
        return "none";
    case Failure::Busy:
        return "the system was busy";
    case Failure::Serialize:
        return "not serialized";
    case Failure::Compress:
        return "not compressed";
    case Failure::NoSpace:
        return "not enough space";
    case Failure::Write:
        return "not written";
    }
    return "?";
}

std::string FailureToast(Failure failure) {
    switch (failure) {
    case Failure::None:
    case Failure::Busy:
        return {};
    case Failure::NoSpace:
        return "Couldn't Autosave: Not Enough Space";
    case Failure::Write:
        return "Couldn't Autosave: Couldn't Write the File";
    default:
        return "Couldn't Autosave";
    }
}

Failure WriteFailure(const std::string& path, std::size_t needed) {
    return WriteFailureAt(path, needed) == SaveFailure::NoSpace ? Failure::NoSpace : Failure::Write;
}

std::string FailureNotice::Show(Failure failure, s64 now_ms) {
    std::string text = FailureToast(failure);
    if (text.empty() || (shown && now_ms - last_ms < kFailureToastGapMs)) {
        return {};
    }
    shown = true;
    last_ms = now_ms;
    return text;
}

// ---- Texts -----------------------------------------------------------------------------------

std::string Ago(s64 then, s64 now, s64 then_day, s64 now_day, const std::string& date_time) {
    const s64 d = now - then;
    if (d < 60) {
        return "Just Now"; // a clock set back counts as now too
    }
    if (d < 3600) {
        const s64 m = d / 60;
        return m == 1 ? std::string("1 Minute Ago") : fmt::format("{} Minutes Ago", m);
    }
    if (then_day == now_day) {
        const s64 h = d / 3600;
        return h == 1 ? std::string("1 Hour Ago") : fmt::format("{} Hours Ago", h);
    }
    if (then_day + 1 == now_day) {
        return "Yesterday";
    }
    return date_time;
}

std::string AgoLocal(u64 then, u64 now) {
    return Ago(static_cast<s64>(then), static_cast<s64>(now), LocalDay(static_cast<s64>(then)),
               LocalDay(static_cast<s64>(now)), LocalDateTime(then));
}

std::string ScopeToast(bool on, std::string_view title) {
    // Cut by characters (UTF-8 sequences kept whole), trailing spaces dropped before the dots.
    std::string name;
    std::size_t chars = 0, i = 0;
    while (i < title.size() && chars < kScopeTitleMax) {
        const auto c = static_cast<unsigned char>(title[i]);
        const std::size_t n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        name.append(title.substr(i, n));
        i += n;
        chars++;
    }
    if (i < title.size()) {
        while (!name.empty() && name.back() == ' ') {
            name.pop_back();
        }
        name += "...";
    }
    if (name.empty()) {
        name = "This Game";
    }
    return fmt::format("Autosave {} for {}", on ? "On" : "Off", name);
}

std::string UndoText(int slot, u64 time) {
    const std::string what = slot < 0    ? std::string("Autosave")
                             : slot == 0 ? std::string("Quick Save")
                                         : fmt::format("Slot {}", slot);
    return fmt::format("{} Loaded: {}", what, LocalTime(time));
}

std::string LocalDateTime(u64 unix_seconds) {
    return Format(unix_seconds, "%Y-%m-%d %H:%M");
}

std::string LocalTime(u64 unix_seconds) {
    return Format(unix_seconds, "%H:%M");
}

std::string LocalDateTimeSeconds(u64 unix_seconds) {
    return Format(unix_seconds, "%Y-%m-%d %H:%M:%S");
}

std::string MenuEntry(std::string_view label, std::string_view when) {
    if (when.empty()) {
        return std::string(label);
    }
    return fmt::format("{} - {}", label, when);
}

// ---- The worker -------------------------------------------------------------------------------

Writer::~Writer() {
    {
        std::scoped_lock lock{mutex};
        quit = true;
    }
    cv.notify_all();
    if (thread.joinable()) {
        thread.join();
    }
}

void Writer::Enqueue(Item item) {
    {
        std::scoped_lock lock{mutex};
        queue.push_back(std::move(item));
        if (!thread.joinable()) {
            thread = std::thread([this] { Run(); });
        }
    }
    cv.notify_all();
}

void Writer::Submit(Job job) {
    Item item;
    item.job = std::move(job);
    Enqueue(std::move(item));
}

void Writer::SubmitThumb(std::string png_path, std::vector<u8> bgra, u32 w, u32 h) {
    Item item;
    item.thumb = true;
    item.png_path = std::move(png_path);
    item.bgra = std::move(bgra);
    item.w = w;
    item.h = h;
    Enqueue(std::move(item));
}

void Writer::WaitIdle() {
    std::unique_lock lock{mutex};
    idle_cv.wait(lock, [this] { return queue.empty() && !working; });
}

bool Writer::Busy() {
    std::scoped_lock lock{mutex};
    return !queue.empty() || working;
}

void Writer::SetOnDone(std::function<void(const Done&)> callback) {
    std::scoped_lock lock{mutex};
    on_done = std::move(callback);
}

Writer::Done Writer::Write(Job& job) {
    Done done;
    done.path = job.path;
    done.reason = job.reason;
    done.autosave = job.autosave;
    done.slot_save = job.slot_save;
    done.slot = job.slot;
    done.raw_bytes = job.raw.size();
    done.serialize_ms = job.serialize_ms;
    const auto t0 = std::chrono::steady_clock::now();
    std::error_code ec;
    fs::create_directories(fs::path(job.path).parent_path(), ec);
    // AzaharFrame savemem: compressed into the file as it is written (no compressed copy, no
    // zero-filled buffer of the state's size; the file as before)
    try {
        done.file_bytes = WriteStateFileFrom(
            job.path, job.header,
            std::span<const u8>{reinterpret_cast<const u8*>(job.raw.data()), job.raw.size()});
        done.ok = true;
    } catch (const std::exception& e) {
        LOG_ERROR(Core, "autosave: {}", e.what());
    }
    std::string().swap(job.raw); // the serialized system is large: let it go now
    job.hold.reset();
    done.compress_ms = Ms(std::chrono::steady_clock::now() - t0);
    if (!done.ok) {
        done.failure = WriteFailure(job.path, done.raw_bytes / 8 + job.header.size());
        LOG_ERROR(Core, "autosave: {} not written ({})", job.path, FailureName(done.failure));
        return done;
    }
    // AzaharFrame advfix: the menu-open autosave replaces its own file and prunes nothing
    if (job.autosave && job.reason != Reason::MenuOpen) {
        Prune(job.dir, job.program_id, job.movie_id, job.keep);
    }
    return done;
}

std::vector<u8> Writer::EncodeThumb(const std::vector<u8>& bgra, u32 w, u32 h) {
    if (w == 0 || h == 0 || bgra.size() < static_cast<std::size_t>(w) * h * 4) {
        return {};
    }
    std::vector<u8> rgb(static_cast<std::size_t>(w) * h * 3);
    for (std::size_t i = 0, n = static_cast<std::size_t>(w) * h; i < n; i++) {
        rgb[i * 3 + 0] = bgra[i * 4 + 2];
        rgb[i * 3 + 1] = bgra[i * 4 + 1];
        rgb[i * 3 + 2] = bgra[i * 4 + 0];
    }
    std::vector<u8> png;
    if (lodepng::encode(png, rgb, w, h, LCT_RGB, 8) != 0) {
        return {};
    }
    return png;
}

void Writer::Run() {
    for (;;) {
        Item item;
        {
            std::unique_lock lock{mutex};
            cv.wait(lock, [this] { return quit || !queue.empty(); });
            if (queue.empty()) {
                return; // quit, and nothing is left to write
            }
            item = std::move(queue.front());
            queue.pop_front();
            working = true;
        }
        if (item.thumb) {
            const std::vector<u8> png = EncodeThumb(item.bgra, item.w, item.h);
            // Only beside a state that was written (a failed one has no picture either).
            const std::string state =
                item.png_path.substr(0, item.png_path.size() - kThumbExt.size()) +
                std::string(kStateExt);
            std::error_code ec;
            if (png.empty() || !fs::exists(state, ec) ||
                !Common::WriteFileAtomic(
                    item.png_path,
                    std::string_view{reinterpret_cast<const char*>(png.data()), png.size()})) {
                LOG_WARNING(Core, "autosave: no picture for {}", item.png_path);
            }
        } else {
            const auto then = std::move(item.job.then);
            const Done done = Write(item.job);
            LOG_INFO(Core,
                     "{} ({}): {} MB serialized on the emulation thread in {} ms; compressed and "
                     "written ({} MB) in {} ms on the writer",
                     done.slot_save ? fmt::format("Save to slot {}", done.slot)
                     : done.autosave ? fmt::format("autosave: {}", ReasonName(done.reason))
                                     : std::string("autosave: undo file"),
                     fs::path(done.path).filename().string(),
                     fmt::format("{:.1f}", done.raw_bytes / 1048576.0), done.serialize_ms,
                     fmt::format("{:.1f}", done.file_bytes / 1048576.0), done.compress_ms);
            if (then) {
                then(done);
            }
            std::function<void(const Done&)> callback;
            {
                std::scoped_lock lock{mutex};
                callback = on_done;
            }
            if (callback) {
                callback(done);
            }
        }
        {
            std::scoped_lock lock{mutex};
            working = false;
        }
        idle_cv.notify_all();
    }
}

} // namespace Core::Autosave
