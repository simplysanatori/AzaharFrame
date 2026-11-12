// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame autosave: save states made on their own, every few minutes of play, when the VR menu
// opens and when the game is closed (Settings::values.autosave_*). Each is a file of its own in
// <states>/autosave/, never one of the slots (0 is the quick save, 1 to 10 the others), so Quick
// Load and "Load from Newest Slot" never see them. A game keeps `autosave_keep` of them, the oldest
// go. Beside each, its top screen (400x240) as a small PNG for the VR menu's card.
//
// The emulation thread only serializes (System::RunLoop); the zstd compression and the atomic write
// (common/atomic_file.h) happen on Writer's thread, so the game's picture is held for the
// serialization alone. Loading one first saves the moment before to kUndoLoadSlot (its own file),
// so the load can be taken back (Undo Load).
//
// No Qt, no System: the files, the trigger rules (Scheduler) and the texts are pure and
// unit-tested.

#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "common/common_types.h"

namespace Core::Autosave {

/// Every's choices, in minutes (Settings::values.autosave_every).
inline constexpr std::array<u32, 5> kEveryMinutes{1, 2, 5, 10, 15};
inline constexpr u32 kKeepMin = 1;
inline constexpr u32 kKeepMax = 10;
/// At most one autosave a minute, whatever asks for it.
inline constexpr s64 kThrottleMs = 60'000;
/// None in the first minute after a game starts.
inline constexpr s64 kBootGraceMs = 60'000;
/// Loading an autosave first saves the moment before here (`<TID>.98.cst`, not a slot).
inline constexpr u32 kUndoLoadSlot = 98;

/// The nearest of kEveryMinutes to `minutes` (an edited config file may hold anything).
u32 ClampEvery(u32 minutes);
u32 ClampKeep(u32 keep);
/// The index of `minutes` in kEveryMinutes (the VR slider's stop), and back.
int EveryIndex(u32 minutes);
u32 EveryAt(int index);

enum class Reason : u8 {
    Timer,    ///< Every minutes of play
    MenuOpen, ///< the VR menu opened
    Quit,     ///< the game was closed cleanly
};
const char* ReasonName(Reason reason);

// ---- Files ----------------------------------------------------------------------------------

/// `<states>/autosave/` (`states_dir` ends with a separator; so does the result).
std::string Dir(const std::string& states_dir);
/// The name every autosave of a game starts with: "0004000000055D00." or, during a movie,
/// "0004000000055D00.movie<ID>.".
std::string Prefix(u64 program_id, u64 movie_id);
/// A new autosave's file: `<dir><prefix><YYYYMMDD-HHMMSS>.cst` (UTC, so names sort by time).
std::string PathFor(const std::string& dir, u64 program_id, u64 movie_id, s64 unix_seconds);
/// Its top screen picture: the same name with ".png".
std::string ThumbPathFor(const std::string& state_path);

struct Entry {
    std::string path;    ///< the .cst
    std::string thumb;   ///< the .png ("" when there is none)
    u64 time = 0;        ///< from the state's header (Unix seconds)
    bool other = false;  ///< made by another build (SaveStateInfo not OK)
    std::string version; ///< FrameVersionOf (only when `other`; "" another version of Azahar)
};
/// A game's autosaves, newest first (by the header's time, then the name). Files that are not a
/// state of this game are left out.
std::vector<Entry> List(const std::string& dir, u64 program_id, u64 movie_id);
/// Deletes all but the newest `keep` (and their pictures); returns how many went. Pictures with no
/// state beside them go too.
std::size_t Prune(const std::string& dir, u64 program_id, u64 movie_id, u32 keep);
/// The same for every game in `dir` (Keep was lowered).
std::size_t PruneAll(const std::string& dir, u32 keep);

// ---- When ------------------------------------------------------------------------------------

/// What must be true for any autosave (the caller reads it from the running game).
struct Gates {
    bool game = false;          ///< a game is powered on and booted
    bool playing = false;       ///< it runs (not paused)
    bool paused = false;        ///< it is paused (the VR menu, Emulation > Pause)
    bool frames_recent = false; ///< it made an emulated frame in the last second
    bool loading_state = false; ///< a state is being saved or loaded
    bool shaders_busy = false;  ///< shaders are being prepared (the game stands for them)
};

/// Why an autosave may not happen now ("" it may), for the log and the tests.
class Scheduler {
public:
    /// The throttle and the grace (kThrottleMs, kBootGraceMs; shorter only for tests).
    void SetTimes(s64 throttle_ms, s64 grace_ms) {
        throttle = throttle_ms;
        grace = grace_ms;
    }
    /// A game started (`now_ms` any steady clock).
    void Boot(s64 now_ms);
    /// The game stopped.
    void Stop();
    /// About once a second: counts the time played since the last autosave.
    void Tick(s64 now_ms, bool playing);
    /// The timer's autosave is due (played `every_ms` since the last one, or since the start).
    bool Due(s64 every_ms) const {
        return booted && play_ms >= every_ms;
    }
    /// "" when an autosave for `reason` may happen now, else why not.
    std::string Blocked(Reason reason, s64 now_ms, const Gates& g) const;
    /// One was made (or asked for) now: the throttle and the play time start again.
    void Fired(s64 now_ms);
    s64 PlayMs() const {
        return play_ms;
    }

private:
    s64 throttle = kThrottleMs;
    s64 grace = kBootGraceMs;
    bool booted = false;
    s64 boot_ms = 0;
    s64 last_fire_ms = 0;
    bool fired_any = false;
    s64 play_ms = 0;
    s64 last_tick_ms = 0;
    bool ticked = false;
};

// ---- Texts -----------------------------------------------------------------------------------

/// "Just Now", "1 Minute Ago", "12 Minutes Ago", "1 Hour Ago", "3 Hours Ago", "Yesterday", then the
/// local date and time ("2026-11-12 10:14"). `then` and `now`: Unix seconds. `then_day` and
/// `now_day`: their local calendar days (any count of days; Yesterday is one apart).
std::string Ago(s64 then, s64 now, s64 then_day, s64 now_day, const std::string& date_time);
/// The same from the clock's local time zone.
std::string AgoLocal(u64 then, u64 now);
/// Local "YYYY-MM-DD hh:mm" (the VR menu's slot format) and "hh:mm".
std::string LocalDateTime(u64 unix_seconds);
std::string LocalTime(u64 unix_seconds);
/// Local "YYYY-MM-DD hh:mm:ss": the Emulation menu's slot format.
std::string LocalDateTimeSeconds(u64 unix_seconds);
/// An Emulation > Load State entry, as its slots are written: "Slot 1 - 2026-11-12 10:14:05",
/// "Autosave 1 - 2026-11-12 10:14:05", "Undo Load - ..." (`when` "": the label alone).
std::string MenuEntry(std::string_view label, std::string_view when);

// ---- The worker -------------------------------------------------------------------------------

/// Compresses and writes autosaves (and the undo file, and the pictures) one after another on a
/// thread of its own.
class Writer {
public:
    struct Job {
        std::string path;             ///< the .cst to write (atomically)
        std::array<u8, 256> header{}; ///< CurrentStateHeader
        std::string raw;              ///< the serialized system (not compressed)
        Reason reason = Reason::Timer;
        bool autosave = true; ///< false: the undo file (nothing pruned, nothing told)
        // Pruned after the write: `dir`'s autosaves of the game, down to `keep`.
        std::string dir;
        u64 program_id = 0;
        u64 movie_id = 0;
        u32 keep = 3;
        s64 serialize_ms = 0; ///< the emulation thread's part, for the log
    };
    struct Done {
        std::string path;
        Reason reason = Reason::Timer;
        bool autosave = true;
        bool ok = false;
        std::size_t raw_bytes = 0;
        std::size_t file_bytes = 0;
        s64 serialize_ms = 0;
        s64 compress_ms = 0;
        s64 write_ms = 0;
    };

    Writer() = default;
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    /// Queues the state; the thread starts on the first job.
    void Submit(Job job);
    /// Queues a picture: `bgra` 400x240 (or `w` x `h`) from the renderer's screenshot, rows top
    /// down.
    void SubmitThumb(std::string png_path, std::vector<u8> bgra, u32 w, u32 h);
    /// Waits until every queued job is done (a load of the undo file, the end of a run).
    void WaitIdle();
    bool Busy();
    /// Called on the writer's thread after each state (not the pictures).
    void SetOnDone(std::function<void(const Done&)> callback);

    /// The work of a job, on the calling thread (the tests; the thread uses it too).
    static Done Write(Job& job);
    /// A picture as PNG bytes (BGRA in, RGB out); empty on failure.
    static std::vector<u8> EncodeThumb(const std::vector<u8>& bgra, u32 w, u32 h);

private:
    struct Item {
        bool thumb = false;
        Job job;
        std::string png_path;
        std::vector<u8> bgra;
        u32 w = 0, h = 0;
    };
    void Run();
    void Enqueue(Item item);

    std::mutex mutex;
    std::condition_variable cv;
    std::condition_variable idle_cv;
    std::deque<Item> queue;
    bool working = false;
    bool quit = false;
    std::function<void(const Done&)> on_done;
    std::thread thread;
};

} // namespace Core::Autosave
