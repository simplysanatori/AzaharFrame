// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame zipinstall: Install from Zip. A texture pack or a mod the player has as a local archive
// (zip or 7z; rar too, as the downloads) is unpacked, recognised and installed into the same places the
// download flows use, so it shows in the menu's lists and can be removed there:
//   texture pack  <load>/texture-packs/<TITLE ID>/<name>/ with the download marker (source "zip"), like
//                 a Download Packs pack (custom_tex/download.h); not made active
//   mod           the mod library as "zip-<name>" (Library::InstallLocal), then the mods folder
// What it is, from the file names:
//   texture pack  tex1_*.png|dds|ktx files (not inside a romfs folder), or a pack.json
//   mod           a romfs, romfs_ext or exefs folder, or code.ips, code.bps, code.bin, exheader.bin
// either one under a title ID folder (16 hex digits, 0004...) or loose. Both in one archive, neither,
// or only another archive inside: refused. The title is the title ID folder's (the running game's when it is one of several),
// else the running game's; several folders and none of them the running game's: the player chooses.
// Nothing is replaced without a question: an installed pack folder or zip mod of the same name is a
// Conflict, and the install is done again with replace = true after the player said yes.
// No Qt. Blocking: run off the GUI thread.

#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Mods {
class Library;
}

namespace Mods::Zip {

enum class Kind { Unknown, TexturePack, Mod };

struct Detected {
    Kind kind = Kind::Unknown;
    std::vector<u64> title_ids; ///< the title ID folders that hold the content, sorted
    std::string reason;         ///< Unknown: why, for the menu
};

constexpr const char* kNotPackOrMod = "Not a Texture Pack or Mod";
constexpr const char* kBoth = "Texture Pack and Mod in One Zip";
constexpr const char* kNested = "Zip Inside a Zip";
constexpr const char* kEmpty = "Empty Zip";
constexpr const char* kNoGame = "Choose a Game First";

/// What an archive holds, from its file paths ('/' separated, files only).
Detected Detect(const std::vector<std::string>& files);

/// "0004000000033500" as a title ID (16 hex digits starting 0004, any case); 0 when it is not one.
u64 ParseTitleFolder(std::string_view part);

struct TitleChoice {
    u64 title_id = 0;            ///< 0: ask (choose from `candidates`, or a game when it is empty)
    std::vector<u64> candidates;
};
/// The title to install for: `found` from Detect, `current` the running (or chosen) game, 0 none.
TitleChoice ResolveTitle(const std::vector<u64>& found, u64 current);

/// The folder of `files` that belongs to `title_id`: the path of its title ID folder ("a/0004...")
/// when the archive has title ID folders, else "" (the whole archive).
std::string ContentRoot(const std::vector<std::string>& files, const Detected& detected,
                        u64 title_id);

/// The archive's name for the lists: its file name without the extension ("Ocarina 4K").
std::string NameFromArchive(const std::string& archive_path);
/// "zip-ocarina-4k": the library id of a zip mod named `name`.
std::string ModId(const std::string& name);

/// An unpacked archive, until Close.
struct Job {
    std::string archive; ///< the file chosen
    std::string name;    ///< NameFromArchive
    std::string work;    ///< the unpack folder (removed by Close)
    std::string root;    ///< the folder that holds the files
    std::vector<std::string> files;
    Detected detected;
};

/// Unpacks `archive_path` into <load_dir>/zip-install/ and recognises it. "" on success (the kind may
/// still be Unknown: `detected.reason`), else the error, with nothing left behind.
/// The archive must fit in the free space of `load_dir`'s file system, unpacked too (`free_space`
/// replaces the file system's answer, for tests); unsafe paths and links are refused (UnpackArchive).
std::string Open(const std::string& archive_path, const std::string& load_dir, Job& job,
                 const std::atomic_bool* cancel = nullptr, const std::string& tool_override = {},
                 std::optional<u64> free_space = std::nullopt);
void Close(Job& job);
/// A few words for the menu from Open's or an install's error ("Not a Zip or 7z File", "Not Enough
/// Space", "Bad Archive"); the full error goes to the log.
std::string ShortError(std::string_view error);

struct Outcome {
    enum class Result { Ok, Conflict, Blocked, Error };
    Result result = Result::Ok;
    std::string error;
    std::string installed;     ///< Ok: the pack folder's name, or the mod's id
    std::string conflict_id;   ///< Conflict: the mod it clashes with (== the new id: the same mod)
    std::string conflict_name; ///< what it replaces (a pack folder, a mod)
};

/// Moves the pack folder into <load_dir>/texture-packs/<TITLE ID>/. A folder of that name made by
/// Download Packs or Install from Zip is replaced only with `replace` (else Conflict); one of the
/// player's own is never touched (the new one is "Name (2)").
Outcome InstallTextures(Job& job, u64 title_id, const std::string& load_dir, bool replace);
/// Puts the mod into the library and the mods folder (Library::InstallLocal). `replace` answers a
/// Conflict naming the mod itself; a clash with another mod is Conflict (Library::Install(id, true)
/// replaces it, the library copy is kept) or Blocked (the player's own files).
Outcome InstallMod(Job& job, u64 title_id, Library& library, bool replace);

} // namespace Mods::Zip
