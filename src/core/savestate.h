// Copyright 2020-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <array>
#include <functional>
#include <span>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"
#include "common/transient_memory.h"

namespace Core {

struct SaveStateInfo {
    u32 slot;
    u64 time;
    enum class ValidationStatus {
        OK,
        RevisionMismatch,
        BuildMismatch,
    } status;
    std::string build_name;
    std::string build_version;
};

constexpr u32 SaveStateSlotCount = 11; // Maximum count of savestate slots

std::vector<SaveStateInfo> ListSaveStates(u64 program_id, u64 movie_id);

SaveStateInfo GetSaveStateInfo(u64 program_id, u64 movie_id, u32 slot);

/// AzaharFrame savestate: ListSaveStates / GetSaveStateInfo in a given states folder (with its
/// trailing separator). Every call reads each file's header again: nothing is cached.
std::vector<SaveStateInfo> ListSaveStatesIn(const std::string& states_dir, u64 program_id,
                                            u64 movie_id);
SaveStateInfo GetSaveStateInfoIn(const std::string& states_dir, u64 program_id, u64 movie_id,
                                 u32 slot);
/// AzaharFrame autosave: the same for any state file; `slot` 0 when it is a state of this game,
/// else std::numeric_limits<u32>::max().
SaveStateInfo GetSaveStateInfoAt(const std::string& path, u64 program_id);
/// A slot's file in `states_dir` ("<dir>0004000000055D00.03.cst").
std::string StatePathIn(const std::string& states_dir, u64 program_id, u64 movie_id, u32 slot);

// AzaharFrame savestate: a state loads only in the exact build that made it (its revision is
// checked), so every Azahar for Frame update makes the older ones unloadable unless the player
// chooses Load Anyway (System::LoadState with allow_mismatch, after a warning, a backup of the
// game's save data and a state of the moment before). What the player is told, in VR and Flat.

/// Written to a new state's header build_version: "AZF " and the build's version ("AZF 1.2.0").
inline constexpr std::string_view kFrameBuildTag = "AZF ";
// AzaharFrame advfix: Load Anyway's moment before is no longer slot 99: every load saves it to
// Autosave::kUndoLoadSlot (98), which Undo Load reads.

/// Shown once, the first time a state is saved or loaded.
inline constexpr const char* kSaveStatesAboutTitle = "About Save States";
inline constexpr const char* kSaveStatesAboutBody =
    "Save states are NOT a replacement for saving in the game. They only load in the version of "
    "Azahar for Frame that made them, so after an update older ones may not load. Save in the "
    "game before you update.";
inline constexpr const char* kSaveStatesAboutButton = "Got It";
/// A state from another build: Cancel, Load Anyway.
inline constexpr const char* kLoadAnywayButton = "Load Anyway";
/// A load that failed: the state was not read, or (SaveStateHalfLoaded) the game stops.
inline constexpr const char* kSaveStateNotLoaded = "This Save State Couldn't Be Loaded";
/// AzaharFrame advfix: its body when nothing was restored (the game goes on as it was).
inline constexpr const char* kSaveStateNotLoadedBody =
    "The game and its save data weren't changed.";
/// AzaharFrame advfix: a save state (a slot or Quick Save) that could not be written.
inline constexpr const char* kSaveFailedTitle = "Couldn't Save State";
inline constexpr const char* kSaveFailedBody =
    "The game is still running. Try another slot, or free up space on the Frame.";
/// Why a state file could not be written, when known.
enum class SaveFailure : u8 { Unknown, NoSpace, Write };
/// The notice's body: the reason on a line of its own ("Not Enough Space", "Couldn't Write the
/// File"), then kSaveFailedBody; kSaveFailedBody alone when the reason is not known.
std::string SaveFailedBody(SaveFailure failure);
/// NoSpace when the folder of `path` (or the nearest one that exists) has less free space than
/// `needed` bytes, else Write.
SaveFailure WriteFailureAt(const std::string& path, std::size_t needed);
/// System::SaveState's write failed (what() the details for the log).
struct SaveStateWriteFailed : std::runtime_error {
    SaveStateWriteFailed(SaveFailure f, const std::string& what)
        : std::runtime_error(what), failure(f) {}
    SaveFailure failure;
};

/// The Azahar for Frame version that made a state, from its header: build_version "AZF 1.2.0"
/// (states from 1.2.0 on), else a build_name that is one of our releases ("1.1.1": every release
/// so far is 1.x; upstream Azahar's are "2122.1"); "" for anything else (upstream Azahar, a
/// development build named by its commit, nothing written).
std::string FrameVersionOf(std::string_view build_name, std::string_view build_version);
/// "This Save State Is From an Older Version" / "a Newer Version" (`version` against `ours`,
/// both "x.y.z"), else "This Save State Is From Another Version" (not comparable: unknown, or a
/// development build of the same version).
std::string SaveStateMismatchTitle(std::string_view version, std::string_view ours);
/// "It was made with Azahar for Frame 1.1.1. It may not load, and it could corrupt this game's
/// progress. Save states are NOT a replacement for saving in the game." ("" version: another
/// version of Azahar).
std::string SaveStateMismatchBody(std::string_view version);
/// In a slot list, in place of the date: "From 1.1.1", "From Another Version".
std::string SaveStateFromText(std::string_view version);

/// A state's 256-byte header as System::SaveState writes it.
std::array<u8, 256> MakeStateHeader(u64 program_id, std::string_view revision_hex,
                                    std::string_view build_name, std::string_view build_version,
                                    u64 time);
/// The header System::SaveState writes now: this build's revision and version ("AZF 1.2.0").
std::array<u8, 256> CurrentStateHeader(u64 program_id, u64 time);
/// Writes (replacing) a state file: header, then the compressed state. Throws on failure.
/// AzaharFrame advfix: atomic (a temporary file renamed over it): a failure leaves the old file.
void WriteStateFile(const std::string& path, std::span<const u8> header, std::span<const u8> body);
/// AzaharFrame savemem: writes (replacing, atomically as WriteStateFile) a state file from the
/// serialized system: the header, then `raw` compressed into it as it is written
/// (Common::Compression::CompressDataZSTDTo, the same zstd frame), so no compressed copy of the
/// state is held. Returns the file's size; throws on failure.
std::size_t WriteStateFileFrom(const std::string& path, std::span<const u8> header,
                               std::span<const u8> raw);
/// AzaharFrame savemem: what System::SerializeState reserves: the last state's size and 32 MiB,
/// at least all the guest memory a state can hold and 96 MiB (only address space until written).
std::size_t ReserveForState(std::size_t last_size);
/// AzaharFrame savemem: the stream buffer System::SerializeState writes the archive into: one
/// string reserved up front (`reserve`, ReserveForState), appended to, never copied; what it holds
/// counts as transient memory (common/transient_memory.h) as it grows.
class StateWriteBuf : public std::streambuf {
public:
    explicit StateWriteBuf(std::size_t reserve);
    std::string out;

protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override;
    int_type overflow(int_type c) override;

private:
    Common::TransientMemory::Hold hold;
    std::size_t counted = 0;
};
/// AzaharFrame savemem: a read-only stream buffer over a decompressed state, so the archive reads
/// it in place (an istringstream held a second copy of it).
class StateReadBuf : public std::streambuf {
public:
    explicit StateReadBuf(std::span<u8> bytes);
};
/// What a header allows: a state of this game from this build (`revision_hex`), or with
/// `allow_mismatch` from any build (the program id and the file type are always checked).
enum class HeaderCheck { Ok, NotAState, OtherGame, OtherBuild };
HeaderCheck CheckStateHeader(std::span<const u8> data, u64 program_id,
                             std::string_view revision_hex, bool allow_mismatch);

/// Reading the archive into the system failed partway: the emulated system may be half-restored,
/// so the game must stop (System::ResultStatus::ErrorSavestateHalfLoaded). what(): kSaveStateNotLoaded.
struct SaveStateHalfLoaded : std::runtime_error {
    SaveStateHalfLoaded() : std::runtime_error(kSaveStateNotLoaded) {}
};
/// Runs `restore` (the archive read into the system); any exception, boost's archive ones and
/// non-standard ones too, becomes SaveStateHalfLoaded.
void RestoreGuarded(const std::function<void()>& restore);

} // namespace Core
