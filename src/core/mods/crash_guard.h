// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame compat: undoing a mod or texture pack after the game closed unexpectedly. No Qt.
//
// A mod or pack installed or turned on from the menu is "new" for its game (NoteOn; NoteOff when
// it is turned off or removed) until the game has had a clean run with it. At each start of the
// game the new ones that are active are written to a marker (Arm). A clean run removes the marker
// and they are no longer new (Clean: the game ran for 60 seconds, or it was stopped or quit
// normally). A marker still there at the next start of the same game means that run did not end
// cleanly: TakeCrashed hands its items over (the caller turns them off before the game boots,
// disabling, never deleting) and removes the marker. Mods that had a clean run are never touched.
// Items turned on while the game runs apply from the next start, so they are armed from then.
//
// Files, in the user directory (frame-crash-guard/): <TITLE ID>.new and <TITLE ID>.marker, a
// line per item: "mod" or "pack", a tab, the id, a tab, the name.

#pragma once

#include <string>
#include <vector>
#include "common/common_types.h"

namespace Mods::CrashGuard {

struct Item {
    enum class Kind { Mod, Pack };
    Kind kind = Kind::Mod;
    std::string id;   ///< the mod library's id; a pack's CustomTex::Pack::id
    std::string name; ///< what the message names
    bool operator==(const Item&) const = default;
};

/// <user dir>/frame-crash-guard/
std::string DefaultDir();

class Guard {
public:
    /// `dir` ends with '/'; created when first written.
    explicit Guard(std::string dir);

    /// Installed or turned on for the title: new until a clean run of the game.
    void NoteOn(u64 title_id, const Item& item);
    /// Turned off or removed: no longer new.
    void NoteOff(u64 title_id, Item::Kind kind, const std::string& id);
    std::vector<Item> New(u64 title_id) const;

    /// Before the game boots: the items of a marker the last run left (it did not end cleanly).
    /// The marker goes, and they are no longer new: the caller turns them off.
    std::vector<Item> TakeCrashed(u64 title_id);
    /// The game started with `active` on: the new ones among them are written to the marker
    /// (nothing when none is new). Returns them.
    std::vector<Item> Arm(u64 title_id, const std::vector<Item>& active);
    /// A clean run: what the marker holds is no longer new; the marker goes.
    void Clean(u64 title_id);
    bool Armed(u64 title_id) const;

private:
    std::string File(u64 title_id, const char* ext) const;
    std::vector<Item> Read(const std::string& path) const;
    void Write(const std::string& path, const std::vector<Item>& items) const;
    std::string dir_;
};

/// "Turned Off No Bloom: The game closed unexpectedly after it was installed.", several names
/// joined ("No Bloom and HD Textures", "... after they were installed.").
std::string TurnedOffText(const std::vector<std::string>& names);

} // namespace Mods::CrashGuard
