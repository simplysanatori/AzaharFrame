// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texparts: the parts of a texture pack, turned on and off, and the variants of a pack,
// one chosen. No Qt, no emulator state, only the standard library and json.hpp (the Flat dialog's
// review harness compiles it on its own).
//
// What a part is (Build): the pack's top-level folders that hold textures (tex1_*.png|dds|ktx).
// Wrapper folders (no textures of their own, one subfolder) are skipped first. A top folder with no
// textures of its own and two or more subfolders with textures is a folder group: its subfolders
// are the parts. Folders named "[Tag] Name" that share a tag are a choice group (one of them, or
// Off); a lone tagged folder shows as "Tag: Name". Texture files outside every part always load.
// All-caps names show in Title Case (words of four letters or more: ENVIRONMENT, but UI and HUD).
//
// Which part wins a conflict (two parts with a texture of the same hash): the higher one in the
// list. Azahar keeps the first file it reads for a hash (Material::AddMapTexture), so LoadOrder
// puts the top group's files first. Default order: by name, with groups named Main, Base, Original
// or Core last (the base a pack's other parts replace pieces of).
//
// The player's choices live in the pack folder, kStateFile, next to the converter's marker. The
// texture loader (video_core custom_tex_manager.cpp GetTextures) reads it through
// load/textures/<TID>, so a change applies at the next start of the game. A download may write
// install hints there first (Hints: a catalog entry's "parts").

#pragma once

#include <atomic>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace CustomTex::Parts {

inline constexpr const char* kStateFile = ".azaharframe-parts.json";

/// A file of the pack: its path from the pack folder ('/' separated) and size.
struct File {
    std::string path;
    u64 size = 0;
};

struct Part {
    std::string key;  ///< folder path from the pack folder ("UI", "Characters/Sprites")
    std::string name; ///< as shown ("UI", "Sprites", "Original N64")
    u64 textures = 0;
    u64 bytes = 0;
    std::vector<std::string> conflicts; ///< keys of parts with a texture of the same hash (not
                                        ///< the other choices of its own group)
    std::string preview;                ///< Scan: a PNG to show for it ("" none)
};

enum class Kind {
    Single, ///< one part: a checkbox
    Folder, ///< a folder of parts: a checkbox for all, one for each
    Choice, ///< variants: one of them, or Off
};

/// A row of the top-level list.
struct Group {
    Kind kind = Kind::Single;
    std::string
        key; ///< Single, Folder: the folder's path; Choice: "choice:" + the name, lower case
    std::string name; ///< as shown
    std::vector<Part> parts;
    bool base = false; ///< named Main, Base, Original or Core
};

/// What a catalog entry says about its pack's parts (written at install, read with the state).
struct Hints {
    std::string choose_one;        ///< the top-level parts are variants: this group's name
    std::vector<std::string> keep; ///< ... except these (folder names, '*' any text)
    std::vector<std::string> off;  ///< off until the player turns them on (or not chosen)
    bool Empty() const {
        return choose_one.empty() && keep.empty() && off.empty();
    }
    bool operator==(const Hints&) const = default;
};

struct Layout {
    std::string root;          ///< the folder holding the parts ("" the pack folder)
    std::vector<Group> groups; ///< default order
    u64 loose_textures = 0;    ///< texture files outside every part (they always load)
    u64 loose_bytes = 0;       ///< their size on disk
    std::size_t PartCount() const;
    const Group* FindGroup(std::string_view key) const;
    const Part* FindPart(std::string_view key) const;
};

struct State {
    Hints hints;
    bool set = false;                          ///< the player changed something (else defaults)
    std::set<std::string> off;                 ///< part keys turned off
    std::map<std::string, std::string> choice; ///< choice group key -> part key ("" Off)
    std::vector<std::string> order;            ///< group keys, top first
    bool operator==(const State&) const = default;
};

/// The parts of a pack from its file list.
Layout Build(const std::vector<File>& files, const Hints& hints = {});

State ParseState(std::string_view json);
std::string StateJson(const State& state);

// ---- What is on, and in which order ----

/// Indices into layout.groups, top first (the state's order, then the rest in default order).
std::vector<std::size_t> Order(const Layout& layout, const State& state);
/// The part key a choice group loads ("" Off).
std::string Chosen(const Layout& layout, const State& state, const Group& group);
bool IsOn(const Layout& layout, const State& state, const Group& group, const Part& part);
/// 0 none of the group's parts on, 1 some, 2 all.
int GroupValue(const Layout& layout, const State& state, const Group& group);
/// Parts on / parts the pack has (a choice group counts once).
std::pair<int, int> CountOn(const Layout& layout, const State& state);
/// Textures and bytes that load.
std::pair<u64, u64> TotalsOn(const Layout& layout, const State& state);
/// A part of the group conflicts with a part of another group (its order matters).
bool GroupConflicts(const Layout& layout, const Group& group);
bool AnyConflicts(const Layout& layout);

// ---- Changes (each marks the state set) ----

void SetPart(State& state, const Layout& layout, std::string_view part_key, bool on);
void SetGroup(State& state, const Layout& layout, const Group& group, bool on);
void Choose(State& state, const Layout& layout, const Group& group, std::string_view part_key);
/// Up (-1) or down (+1) one row of the top-level list.
void Move(State& state, const Layout& layout, std::string_view group_key, int delta);
/// Every checkbox part on or off; choices stay.
void SetAll(State& state, const Layout& layout, bool on);
/// AzaharFrame texlayers: the groups in `keys` order (the rest after, in their order).
void SetOrder(State& state, const Layout& layout, const std::vector<std::string>& keys);

/// The texture loader's list: indices into `files`, in the order to read them (the top group's
/// first, each part's files by path), without the files of parts that are off.
std::vector<std::size_t> LoadOrder(const Layout& layout, const State& state,
                                   const std::vector<File>& files);

// ---- Names ----

/// "ENVIRONMENT" -> "Environment", "UI" stays; names with lower case letters stay.
std::string DisplayName(std::string_view folder);
/// A texture file's hash ("tex1_64x64_1E3BCF1097CE56AC_3.png" -> 1E3BCF1097CE56AC), 0 none.
u64 TextureHash(std::string_view file_name);
bool IsTextureFile(std::string_view file_name);

// ---- Texts (the VR page and the Flat dialog say the same) ----

/// "1 Texture", "1,434 Textures".
std::string CountText(u64 n, std::string_view one, std::string_view many);
/// "4.5 GB", as the in-game menu's sizes.
std::string SizeText(u64 bytes);
/// The names of the parts a part conflicts with: "Effects, Environment" ("" none).
std::string ConflictNames(const Layout& layout, const Part& part);
/// A part's supporting line: "1,434 Textures · 4.5 GB · Conflicts with Effects".
std::string PartDetail(const Layout& layout, const Part& part);
/// A folder's: its parts' totals, and the parts outside it they conflict with.
std::string GroupDetail(const Layout& layout, const Group& group);
/// The pack row's "6 of 7 Parts" ("" when every part is on, or the pack has one part).
std::string PartsOnText(const Layout& layout, const State& state);
/// A group can move (its order decides a conflict): Move Up, Move Down.
bool CanMove(const Layout& layout, const Group& group);

// ---- File system ----

/// Every file under `pack` (following the pack's own symlinks), paths relative to it.
std::vector<File> ListFiles(const std::string& pack, const std::atomic_bool* cancel = nullptr);
/// The state saved in the pack (defaults when there is none or it cannot be read).
State ReadState(const std::string& pack);
bool WriteState(const std::string& pack, const State& state);
/// Writes install hints when the pack has no state file yet.
bool WriteHints(const std::string& pack, const Hints& hints);

/// Build plus a preview for each part: the largest PNG of at most 1.5 MB among the first 64 of
/// the part (by path); for a converted (ASTC) pack, from the pack it was made from.
Layout Scan(const std::string& pack, const Hints& hints, const std::atomic_bool* cancel = nullptr);

} // namespace CustomTex::Parts
