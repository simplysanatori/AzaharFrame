// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emhb: what of Pokémon Emerald 3DS is on this machine, from the files themselves, and
// what Uninstall removes. An install this program made has user/emerald3ds/state.json; one copied
// from another computer or SD card has only the game's files, so the files decide:
//
//   sdmc/3ds/emerald3ds/Emerald3DS.3dsx, .smdh, emerald3ds.pak   the game and its data pack
//   sdmc/3ds/**/<Emerald 3DS>.3dsx (two levels)                  a copy put elsewhere on the SD card
//   <a game folder>/<Emerald 3DS>.3dsx or .cia                   what the game list shows
//   <title root>/00040000/<id>/content/*.app                     an installed title (NAND or SD)
//   user/emerald3ds/state.json, work/, followers-cache/,         what the build left
//   fallback-choices.json
//
// A file is Emerald 3DS by its name ("Emerald3DS*.3dsx") or by the title in its SMDH (a renamed
// .3dsx, a .cia, an installed title). Never part of an install, so never removed by Uninstall:
// the player's ROM (any .gba, and user/emerald3ds/rom.gba, the copy kept for rebuilds), and the
// shared sdmc/3ds/dspfirm.cdc. The save (emerald3ds.sav and its companions, backup/, an installed
// title's data/) is removed only when the player asks for it. The .3dsx and .cia copies in the game
// folders are the player's files: Uninstall keeps them, except extra copies that are byte for byte
// the same as one it keeps (IdenticalExtras). No Qt here.

#pragma once

#include <optional>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "core/homebrew/emerald3ds.h"

namespace Homebrew::Emerald3ds {

// ---------------------------------------------------------------------------------------------
// What a file says it is

/// An SMDH's English title (or the first one filled in).
struct SmdhText {
    std::string short_title;
    std::string long_title;
    std::string publisher;
};

/// An SMDH (0x36C0 bytes, "SMDH").
std::optional<SmdhText> ParseSmdh(std::span<const u8> data);
/// The SMDH of a game file: a .smdh, a .3dsx (its extended header), a .cia (its meta section) or
/// an installed title's .app / a .cxi (an unencrypted NCCH's ExeFS icon). nullopt when it has none.
std::optional<SmdhText> ReadGameSmdh(const std::string& path);

/// "Emerald3DS.3dsx", "Emerald3DS-de.3dsx", "emerald 3ds.cia": the name says Emerald 3DS.
bool IsEmeraldName(std::string_view file_name);
/// The SMDH says Emerald 3DS (its title, or "Emerald" by ZallaxDev).
bool IsEmeraldTitle(const SmdhText& smdh);
/// The first "v1.2" / "v1.2.3" in `text` ("" none): the release an SMDH names, if it does.
std::string VersionInText(std::string_view text);

/// emerald3ds.pak's header ("EM3DPAK", version 1).
struct PakHeader {
    std::string abi;      ///< 8 hex digits
    std::string rom_sha1; ///< the ROM it was built from, 40 hex digits
    u32 entries = 0;
};
std::optional<PakHeader> ParsePakHeader(std::span<const u8> data);
std::optional<PakHeader> ReadPakHeader(const std::string& path);

// ---------------------------------------------------------------------------------------------
// What is installed

enum class Piece : u8 {
    SdmcGame,       ///< a .3dsx on the SD card (Paths::three_dsx, or a copy elsewhere under 3ds/)
    GameCopy,       ///< a .3dsx or .cia in a game folder
    Title,          ///< an installed title's folder (<title root>/00040000/<id>/)
    Smdh,           ///< Emerald3DS.smdh
    Pak,            ///< emerald3ds.pak
    State,          ///< user/emerald3ds/state.json
    Work,           ///< user/emerald3ds/work/ (a build's downloads and output)
    FollowersCache, ///< user/emerald3ds/followers-cache/
    Choices,        ///< user/emerald3ds/fallback-choices.json
    Temp,           ///< a half-written "*.new" or "*.tmp" of the above
};

struct Found {
    Piece piece;
    std::string path; ///< a folder ends in '/'
    bool operator==(const Found&) const = default;
};

struct DetectInput {
    Paths paths;
    /// The game list's folders: the path and whether its subfolders are scanned too.
    std::vector<std::pair<std::string, bool>> game_dirs;
    /// "<NAND or SD>/title/" (Service::AM::GetMediaTitlePath); "" skipped.
    std::vector<std::string> title_roots;
};

struct Detection {
    std::vector<Found> found;       ///< every piece there, in the order above
    std::vector<std::string> saves; ///< the save files and folders there (kept unless asked)
    bool app_made = false;          ///< state.json read (this program installed it)
    std::optional<State> state;
    bool launchable = false; ///< a .3dsx, .cia or installed title
    bool has_pak = false;    ///< emerald3ds.pak with a valid header
    bool playable = false;   ///< both: shown as installed
    std::string game_path;   ///< what Play starts ("" none)
    std::string game_copy;   ///< the game folder's copy ("" none): updates replace it
    std::string three_dsx;   ///< the .3dsx whose SHA-256 can name the release ("" none)
    std::string version;     ///< "v0.5.0": state.json's, else the SMDH's, else ""
    std::string language;    ///< "en"... when known
    std::string data_abi;    ///< state.json's, else the pack's
    std::string rom_sha1;

    /// Anything of an install is there (saves alone are not).
    bool Any() const {
        return !found.empty();
    }
    /// Something is there but it cannot be played: Repair (install again; the save stays).
    bool Broken() const {
        return Any() && !playable;
    }
    bool Has(Piece piece) const;
    /// For the row: "Game Data Missing" (no pack), "Game File Missing" (no .3dsx, .cia or title),
    /// "" when it plays.
    std::string_view Problem() const;
};

Detection Detect(const DetectInput& input);

/// The release variant whose .3dsx has this SHA-256 (an imported copy's release and language).
const Variant* FindVariantByThreeDsx(const Manifest& manifest, std::string_view sha256);

// ---------------------------------------------------------------------------------------------
// Uninstall

struct UninstallPlan {
    std::vector<std::string> files;
    std::vector<std::string> dirs;           ///< removed with everything in them
    std::vector<std::string> remove_if_empty; ///< afterwards, deepest first
    std::vector<std::string> kept; ///< the save, the ROM's copy, the game folders' copies: never in files or dirs
                                   ///< (but an identical extra copy, see IdenticalExtras)
};

/// Of the game folders' copies (in this order), the ones that are byte for byte the same as an
/// earlier one (same size and SHA-256, then the bytes themselves): the extras Uninstall may remove.
/// The first of each kind of content is never among them, nor a copy that differs in any way, nor
/// the same file listed twice.
std::vector<std::string> IdenticalExtras(const std::vector<std::string>& copies);

/// What Uninstall removes: every piece Detect found, except the game folders' .3dsx and .cia copies
/// (the player's files: kept, bar identical extras; an installed title's content/, all of it with
/// `delete_save`), and with `delete_save` the save files, backup/ and a title's data/. Never a ROM
/// (a .gba, Paths::rom_copy) or dspfirm.cdc, whatever is asked.
UninstallPlan PlanUninstall(const Detection& detection, const Paths& paths, bool delete_save);

struct UninstallResult {
    std::vector<std::string> removed;
    std::vector<std::string> failed;
    bool Ok() const {
        return failed.empty();
    }
};
UninstallResult RunUninstall(const UninstallPlan& plan);

// ---------------------------------------------------------------------------------------------
// Uninstall's dialog (the VR menu's and Azahar's window's): "Uninstall Pokémon Emerald 3DS?", this
// text, the checkbox "Also Delete Save Data" (only with a save), Cancel and Uninstall.

/// `game_copies`: how many .3dsx / .cia copies the game folders hold.
/// "Removes the installed game and its build files. Your ROM, save data and game files are kept."
/// (no game files: "Your ROM and save data are kept."; no save: "...Your ROM and game files..."),
/// ticked: "Removes the installed game, its build files and your save data. Your ROM and game
/// files are kept."; two or more copies add " Any extra identical copies are removed."
std::string UninstallBody(bool has_save, bool delete_save, size_t game_copies = 0);

} // namespace Homebrew::Emerald3ds
