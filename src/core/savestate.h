// Copyright 2020-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <array>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

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
/// Load Anyway first saves the moment before here (a file of its own, not one of the slots).
inline constexpr u32 kBeforeLoadAnywaySlot = 99;

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
void WriteStateFile(const std::string& path, std::span<const u8> header, std::span<const u8> body);
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
