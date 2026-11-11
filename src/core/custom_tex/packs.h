// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texdata: a game's texture packs, listed and switched. No Qt here. Used by the VR
// menu's Textures tab (through the Qt side) and by tests.
//
// What Azahar loads is the folder <user>/load/textures/<TITLEID>. This module treats it as a
// pointer: Activate() repoints it (a symlink) at one of the game's packs, Deactivate() removes the
// pointer. No function here deletes or rewrites pack data; a real folder found at the pointer's
// place is moved (renamed) into the managed library first.
//
// Where packs are looked for (see ListPacks):
//   managed   <load>/texture-packs/<TITLEID>/<folder>/            one pack per folder
//   extra     a root from Paths::extra_roots, holding <TITLEID> and <TITLEID>-<suffix> folders;
//             the last path part of a root may contain '*' (e.g. ~/.local/share/azahar-emu/load/
//             textures-*), so the existing textures-4k/<TID>, textures-1080p/<TID>-astc4x4 layout
//             shows up without moving anything
//   active    whatever <load>/textures/<TITLEID> points at (or is, when it is a real folder)
//
// Threading: every function is blocking file system work with no shared state except the size
// cache (mutex). Call from a worker thread and hand the result (plain copyable values) to the
// menu; none of them touches Qt, the renderer or the emulated system. Do not run two Activate /
// Deactivate calls for one title at once.
//
// Reloading a running game: the custom texture manager cannot do it (see kReloadSupported), so the
// UI should say "Restart the Game to Apply" after a change.

#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace CustomTex {

/// AzaharFrame texdata: whether a running game can pick up a switched pack. False: the manager
/// reads the pack once per title (FindCustomTextures returns early after the first run) and surfaces
/// hold material pointers, so the new pack applies on the next start of the game.
inline constexpr bool kReloadSupported = false;

enum class Format {
    Unknown, // no texture files found
    Png,
    Ktx,   // GPU-ready (ASTC from the converter)
    Dds,
    Mixed, // more than one kind
};

/// "PNG", "ASTC/KTX", "DDS", "Mixed", "" for Unknown.
std::string FormatName(Format format);

struct Pack {
    std::string id;          // the canonical path of the folder; stable, unique
    std::string name;        // pack.json "name", else derived: "4K", "1080p", "4K (PNG)"
    std::string author;      // pack.json, "" when absent
    std::string version;     // pack.json, "" when absent
    std::string description; // pack.json, "" when absent
    Format format = Format::Unknown;
    std::string resolution;  // "4K", "1080p", ... from pack.json "resolution" or the folder names; "" unknown
    std::string path;        // the folder
    bool managed = false;    // inside <load>/texture-packs/<TITLEID>/
    bool active = false;     // load/textures/<TITLEID> points here
    std::optional<u64> size_bytes; // from the size cache; nullopt until ComputeSize ran
};

struct Paths {
    std::string load_dir;                 // <user>/load (FileUtil's LoadDir)
    std::vector<std::string> extra_roots; // see the header comment
};

/// LoadDir, plus the extra roots from ReadExtraRoots(), plus (when it exists) the folders
/// ~/.local/share/azahar-emu/load/textures-* of a desktop Azahar.
Paths DefaultPaths();

/// The extra roots saved in <load>/texture-packs/roots.txt (one path per line, '#' comments) and
/// in the environment variable AZAHARFRAME_TEXTURE_ROOTS (':' separated).
std::vector<std::string> ReadExtraRoots(const std::string& load_dir);
/// Saves the list to roots.txt (the environment variable is not touched). False on a write error.
bool WriteExtraRoots(const std::string& load_dir, const std::vector<std::string>& roots);

/// The title as the folder name: 16 upper-case hex digits.
std::string TitleFolder(u64 title_id);

/// The folder Azahar reads: <load>/textures/<TITLEID>.
std::string LoadFolder(const Paths& paths, u64 title_id);

/// The title's packs, active one flagged, sorted by name (then path). Reads folder listings,
/// pack.json and a bounded sample of file names per pack; sizes come from the cache only.
std::vector<Pack> ListPacks(const Paths& paths, u64 title_id);

/// The pack the load folder resolves to, or nullopt when there is none (missing, empty folder,
/// dangling link). A real folder there is reported as a pack (managed = false).
std::optional<Pack> ActivePack(const Paths& paths, u64 title_id);

struct Result {
    bool ok = false;
    std::string error;     // set when !ok
    std::string backed_up; // set when a real folder was moved into texture-packs/<TITLEID>/
};

/// Points the load folder at `pack` (an atomic symlink swap). A real folder at the load place is
/// first renamed into <load>/texture-packs/<TITLEID>/<name> (name from its pack.json, else
/// "Original"; made unique); an empty one is removed. Nothing is deleted otherwise.
Result Activate(const Paths& paths, u64 title_id, const Pack& pack);

/// "None": removes the symlink; a real folder there is moved into the managed library as above.
Result Deactivate(const Paths& paths, u64 title_id);

/// The cached size of the pack's folder (bytes on disk, apparent file sizes), nullopt if unknown.
std::optional<u64> CachedSize(const std::string& path);

/// Walks the folder and caches the total. Blocking (seconds for a multi-GB pack): worker thread
/// only. Stops early and returns nullopt, caching nothing, when `cancel` becomes true.
std::optional<u64> ComputeSize(const std::string& path, const std::atomic_bool* cancel = nullptr);

/// Forgets cached sizes (all, or one folder).
void ForgetSizes();

// --- pure parts (tested without a game) ---

struct PackJson {
    std::string name, author, version, description, resolution;
};
/// Reads the metadata fields of a pack.json text; wrong-typed or missing fields stay "".
PackJson ParsePackJson(const std::string& text);

/// Folder-name rules: "textures-4k" + "0004000000033500-astc4x4" -> "4K"; "textures-1080p" +
/// "0004000000033500" -> "1080p" (the format tag " (PNG)" is added by ListPacks for PNG packs).
std::string DeriveName(const std::string& parent_folder, const std::string& folder,
                       const std::string& title_folder);
/// "4K", "1080p", "2K" found in the names ("" when none).
std::string ResolutionHint(const std::string& parent_folder, const std::string& folder);

/// Detects the format from the file extensions in a bounded walk of the folder.
Format DetectFormat(const std::string& folder);

// --- settings (global, or the game's own when its per-game config is loaded) ---

bool CustomTexturesEnabled();
void SetCustomTexturesEnabled(bool enabled);
bool PreloadTexturesEnabled();
void SetPreloadTexturesEnabled(bool enabled);
/// True when the setting currently holds a per-game value (SwitchableSetting not global).
bool CustomTexturesIsPerGame();
bool PreloadTexturesIsPerGame();

} // namespace CustomTex
