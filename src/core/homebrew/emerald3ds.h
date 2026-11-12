// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emerald: the Pokémon Emerald 3DS homebrew package (packages.h). Sets up the fan port
// Emerald 3DS by ZallaxDev (github.com/ZallaxDev/emerald-3ds, emerald-3ds.com) from the player's
// own Pokémon Emerald ROM. No Qt here; the dialogs are in citra_qt/frame_emerald.cpp.
//
// Nothing of Emerald 3DS is in this repository: its release (CC BY-NC-SA 4.0 from 0.5.0) is
// downloaded from GitHub when the player asks, and its own builder (Python, from the release's
// Emerald3DS-WebPayload.zip) is run as a separate program by steam-frame/tools/emerald3ds-build.py
// to make emerald3ds.pak from the ROM. No ROM is ever downloaded.
//
// What goes where (sdmc = Azahar's emulated SD card, user = Azahar's user folder):
//   sdmc/3ds/emerald3ds/Emerald3DS.3dsx, Emerald3DS.smdh, emerald3ds.pak   the game
//   sdmc/3ds/emerald3ds/emerald3ds.sav (and companions)                    the game's save, never
//   touched sdmc/3ds/emerald3ds/backup/<YYYYMMDD-HHMMSS>[-N]/                      the save, before
//   each update sdmc/3ds/dspfirm.cdc                                                   a
//   placeholder when none exists
//                                                                          (Azahar's HLE DSP takes
//                                                                          any data)
//   <a game folder>/Emerald3DS.3dsx                                        a copy, so the game list
//   shows it user/emerald3ds/state.json, rom.gba, work/                             what was
//   installed, the ROM
//                                                                          (for rebuilds),
//                                                                          downloads

#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Homebrew::Emerald3ds {

constexpr std::string_view kOwnerRepo = "ZallaxDev/emerald-3ds";
constexpr std::string_view kLatestUrl =
    "https://api.github.com/repos/ZallaxDev/emerald-3ds/releases/latest";
/// Every asset URL must start with this (checked before anything is downloaded).
constexpr std::string_view kAssetPrefix =
    "https://github.com/ZallaxDev/emerald-3ds/releases/download/";

constexpr std::string_view kThreeDsxName = "Emerald3DS.3dsx";
constexpr std::string_view kSmdhName = "Emerald3DS.smdh";
constexpr std::string_view kPakName = "emerald3ds.pak";
constexpr std::string_view kSaveName = "emerald3ds.sav";
constexpr std::string_view kPayloadAsset = "Emerald3DS-WebPayload.zip";
constexpr std::string_view kManifestAsset = "web-manifest.json";
constexpr std::string_view kSumsAsset = "SHA256SUMS.txt";

constexpr std::size_t kRomSize = 16 * 1024 * 1024;
constexpr s64 kCheckInterval = 24 * 60 * 60; // seconds between two update checks

// ---------------------------------------------------------------------------------------------
// The ROM

struct RomProfile {
    std::string_view code;     // the cartridge's game code, "BPEE"
    std::string_view language; // the game language it builds, "en"
    std::string_view sha1;     // clean dump, lower-case hex
};

/// The clean dumps Emerald 3DS builds from (its docs/INSTALLATION.md and builder rom.py).
std::span<const RomProfile> SupportedRoms();

/// The profile with this SHA-1, or nullptr.
const RomProfile* FindRom(std::string_view sha1);

/// The game languages a ROM builds, its own first: the English ROM also builds the Brazilian
/// Portuguese fan translation ("en", "pt"); the others their own language only.
std::vector<std::string> LanguagesFor(const RomProfile& rom);
/// "English", "Português (Brasil)"... (the builder's own names); the code itself when unknown.
std::string LanguageName(std::string_view language);

constexpr std::string_view kRomRefused = "Not a Clean Pokémon Emerald ROM";

struct RomCheck {
    bool ok = false;
    std::string sha1;
    const RomProfile* profile = nullptr;
    std::string error; // kRomRefused or a read error
};

/// A trimmed dump (trailing 0xFF cut off) is checked padded back to 16 MiB, as the builder does.
RomCheck CheckRom(std::span<const u8> data);
RomCheck CheckRomFile(const std::string& path);

// ---------------------------------------------------------------------------------------------
// The release

struct Asset {
    std::string name;
    std::string url; // browser_download_url
    u64 size = 0;
};

struct Release {
    std::string tag; // "v0.5.0"
    bool draft = false;
    bool prerelease = false;
    std::vector<Asset> assets;
};

/// GitHub's "releases/latest" reply.
std::optional<Release> ParseRelease(std::string_view json, std::string* error = nullptr);

/// "v0.5.0" -> {0, 5, 0}; nullopt when it is not dotted numbers.
std::optional<std::vector<int>> ParseVersion(std::string_view tag);
/// Whether `latest` is a newer release than `installed` (by version; tags that do not parse:
/// whenever they differ).
bool IsNewer(std::string_view latest, std::string_view installed);

/// One game language of a release (web-manifest.json: the top level is English, `variants` the
/// rest).
struct Variant {
    std::string language;  // "en", "pt", "de"...
    std::string rom_sha1;  // the ROM it is built from
    std::string data_abi;  // the pack's data ABI, 8 hex digits: a new one needs a new pack
    std::string three_dsx; // its release asset, "Emerald3DS.3dsx" or "Emerald3DS-de.3dsx"
    std::string three_dsx_sha256;
};

struct Manifest {
    std::string release_tag;
    int api_version = 0;
    std::vector<Variant> variants; // English first
};

std::optional<Manifest> ParseManifest(std::string_view json, std::string* error = nullptr);

/// The variant made from this ROM: in `language` when given ("pt" from the English ROM), else the
/// ROM's own language (the first match, English before the subfolders).
const Variant* FindVariant(const Manifest& manifest, std::string_view rom_sha1,
                           std::string_view language = {});

/// SHA256SUMS.txt: "hash  name" lines -> name to lower-case hash.
std::vector<std::pair<std::string, std::string>> ParseSums(std::string_view text);
std::string SumFor(const std::vector<std::pair<std::string, std::string>>& sums,
                   std::string_view name);

enum class UpdateKind { None, Quick, Full };

/// What an update needs. Quick: the new release keeps this language's data ABI, so only the .3dsx
/// and .smdh are replaced. Full: the pack is made again from the kept ROM (also when the installed
/// ABI is unknown or the release has no manifest).
UpdateKind PlanUpdate(std::string_view installed_tag, std::string_view installed_abi,
                      std::string_view latest_tag, const Variant* latest_variant);

struct AssetPlan {
    const Asset* manifest = nullptr;  // always
    const Asset* sums = nullptr;      // always
    const Asset* payload = nullptr;   // Full: the builder and every language's files
    const Asset* three_dsx = nullptr; // Quick
    const Asset* smdh = nullptr;      // Quick
};

/// The assets to download. `three_dsx_asset` is the variant's (Quick only). Refuses a missing
/// asset or one whose URL is not under kAssetPrefix + the release's tag.
std::optional<AssetPlan> SelectAssets(const Release& release, UpdateKind kind,
                                      std::string_view three_dsx_asset, std::string* error);

// ---------------------------------------------------------------------------------------------
// Where things go

struct Paths {
    std::string app_dir; // sdmc/3ds/emerald3ds/
    std::string three_dsx;
    std::string smdh;
    std::string pak;
    std::string save;
    std::string backup_root; // sdmc/3ds/emerald3ds/backup/
    std::string dspfirm;     // sdmc/3ds/dspfirm.cdc
    std::string data_dir;    // user/emerald3ds/
    std::string state_file;
    std::string rom_copy;
    std::string work_dir;
};

/// `sdmc_dir` and `user_dir` as FileUtil gives them (with or without the trailing slash).
Paths PlanPaths(std::string_view sdmc_dir, std::string_view user_dir);

/// <rom_dir>/Emerald3DS.3dsx
std::string RomDirCopy(std::string_view rom_dir);

/// Whether a booted file is Emerald 3DS (the sdmc copy or the game folder copy).
bool IsEmeraldPath(std::string_view path);

/// The save and the companions the game can recover progress from (as the builder backs them up).
std::vector<std::string> SaveFiles();

/// backup_root/<stamp>[-N]/ that `exists` says is free ("20261111-221500", then "-2", "-3"...).
std::string BackupDir(std::string_view backup_root, std::string_view stamp,
                      const std::function<bool(const std::string&)>& exists);

enum class RomDirChoice { Prompt, UseOnly, Ask };
/// None: ask for a folder (default DefaultRomDir), one: use it, several: ask which.
RomDirChoice ChooseRomDir(std::size_t game_dirs);
std::string DefaultRomDir(std::string_view home); // home/Documents/ROMs/3DS

/// The placeholder for sdmc/3ds/dspfirm.cdc (no firmware in it; written only when no file exists).
std::string DspPlaceholder();

// ---------------------------------------------------------------------------------------------
// What is installed (user/emerald3ds/state.json)

struct State {
    std::string tag;      // installed release
    std::string language; // its game language
    std::string rom_sha1;
    std::string data_abi;
    std::string rom_dir_copy; // the game folder's copy of the .3dsx
    s64 last_check = 0;       // unix seconds of the last update check
    std::string latest_tag;   // what that check found
};

std::string SerializeState(const State& state);
std::optional<State> ParseState(std::string_view json);

/// Whether the once-a-day check is due (also when the clock went back).
bool CheckDue(s64 last_check, s64 now);

/// Whether the last check saw a newer release than the installed one.
bool UpdateAvailable(const State& state);

} // namespace Homebrew::Emerald3ds
