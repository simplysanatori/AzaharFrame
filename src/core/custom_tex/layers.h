// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texlayers: several texture packs on at once for a game, in one order. No Qt.
//
// The list (State): entries, top first. An entry is a whole pack, or one of a pack's Customize
// groups (core/custom_tex/parts.h) when the pack is split: a pack with kMinSplit to kMaxSplit
// groups shows each group as an entry, so the groups of two packs can be interleaved; a pack with
// fewer (one folder, or loose files) or more (hundreds of folders) is a single entry, and its own
// Customize order applies inside it. The groups are the Customize groups on purpose: the list and
// the pack's Customize page never disagree, a group's checkbox is the same choice on both, and
// moving a pack's groups in either place moves them in the other (the pack's Parts::State order
// follows the list).
//
// Which texture wins when two entries have one of the same hash: the higher entry's. Azahar keeps
// the first file it reads for a hash (Material::AddMapTexture), so LoadOrder lists the top
// entry's files first. Nothing is copied: the loader reads every pack where it is.
//
// Where it lives: <load>/texture-packs/<TITLEID>/kStateFile. load/textures/<TITLEID> (packs.h)
// keeps pointing at the top pack, so a pack's pack.json options, Optimize for Frame and the Flat
// game list's actions keep working on it; the list is only read when two or more packs are on.
// The link is the truth when the two disagree (Resolve): a list naming a pack the link does not
// point at is dropped (something else switched packs), except when the link points at the other
// pack of an Optimize for Frame pair of a pack in the list, which then takes that pack's place. No
// list (before this feature): the pack the link points at is the only one on.
//
// Defaults: a pack turned on goes on top, its entries together in its own order; a pack whose
// catalog entry (or pack.json) says "layer_on": <catalog id> goes above that pack when both are on,
// and a base pack turned on goes under the packs layered on it.

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "core/custom_tex/packs.h"
#include "core/custom_tex/parts.h"

namespace CustomTex::Layers {

inline constexpr const char* kStateFile = ".azaharframe-layers.json";
/// A pack with this many Customize groups shows them as entries (fewer or more: one entry).
inline constexpr std::size_t kMinSplit = 2;
inline constexpr std::size_t kMaxSplit = 10;

struct Entry {
    std::string pack; ///< Pack::id (the canonical folder)
    std::string key;  ///< a Parts group key; "" the whole pack
    bool operator==(const Entry&) const = default;
};

struct State {
    std::vector<Entry> entries; ///< top first
    bool operator==(const State&) const = default;
};

State ParseState(std::string_view json);
std::string StateJson(const State& state);

// ---- The list (pure) ----

/// The pack shows its groups as entries.
bool Splits(const Parts::Layout& layout);
/// The pack's entry keys in its own order: its group keys (Parts::Order) when it splits, else {""}.
std::vector<std::string> PackKeys(const Parts::Layout& layout, const Parts::State& state);
/// The packs on, in the order of their first entries.
std::vector<std::string> Packs(const State& state);
bool Has(const State& state, std::string_view pack);
/// Makes the pack's entries match `keys` (PackKeys): unknown keys go, missing ones are added
/// after its last entry, a whole-pack entry of a pack that now splits becomes its groups there
/// (and the other way). A pack not in the list is left out.
void Fit(State& state, const std::string& pack, const std::vector<std::string>& keys);
/// Turns the pack on: its entries together at the top, or right under the lowest entry of the
/// packs in `above` (the packs layered on it) when any is on. Already on: nothing changes.
void Add(State& state, const std::string& pack, const std::vector<std::string>& keys,
         const std::vector<std::string>& above = {});
void Remove(State& state, std::string_view pack);
/// The pack `from` becomes `to` where it is (an Optimize for Frame pair); `to` already in the
/// list: `from` just goes.
void Replace(State& state, const std::string& from, const std::string& to);
/// AzaharFrame texrow: an Optimize for Frame switch that waits for the next start (the running
/// game reads `from` until then; FrameMods applies it when the game stops).
struct Switch {
    std::string from, to;
    bool operator==(const Switch&) const = default;
};
/// The list from the next start: `to` in `from`'s place (on top, whole, when `from` is off).
void ApplySwitch(State& state, const Switch& s);
/// The list as it is until then (what goes to disk while the switch waits): `to` back to `from`.
void UndoSwitch(State& state, const Switch& s);

/// AzaharFrame texrow: a row of the menu's pack list. An Optimize for Frame pair (a PNG pack and its
/// optimized copy) is one row: the copy the game uses, or will use from the next start, or (the
/// pair off) the optimized copy, which is the one turned on.
struct PackRow {
    std::size_t shown = 0;            ///< index into the packs
    std::optional<std::size_t> other; ///< the pair's other copy, when it is there
    bool on = false;
    bool operator==(const PackRow&) const = default;
};
/// One row a pack (a pair: one row, where its first copy is). `list` with the waiting switches
/// applied; `partners`: the pairs, both ways; `copies`: the optimized copies.
std::vector<PackRow> PackRows(const std::vector<Pack>& packs, const State& list,
                              const std::map<std::string, std::string>& partners,
                              const std::set<std::string>& copies);

/// Up (-1) or down (+1) one entry. False when it cannot.
bool Move(State& state, std::size_t index, int delta);
/// The pack's keys in the list's order.
std::vector<std::string> PackOrder(const State& state, std::string_view pack);
/// Puts the pack's entries in `order` in the places they hold (Customize's Move Up / Move Down).
void SetPackOrder(State& state, std::string_view pack, const std::vector<std::string>& order);

/// A pack as the loader and the menu read it.
struct PackData {
    std::string pack; ///< its folder (Pack::id)
    std::vector<Parts::File> files;
    Parts::Layout layout;
    Parts::State state;
};
/// Lists the pack's files, reads its Customize state and builds its layout. Blocking.
PackData Read(const std::string& pack, const std::atomic_bool* cancel = nullptr);

struct Placed {
    std::size_t pack = 0; ///< index into the PackData list
    std::size_t file = 0; ///< index into its files
    bool operator==(const Placed&) const = default;
};
/// Each entry's files, in the order to read them (a pack's files outside every group go with its
/// lowest entry; parts turned off are left out).
std::vector<std::vector<Placed>> EntryFiles(const State& state, const std::vector<PackData>& packs);
/// The loader's list: every entry's files, top entry first.
std::vector<Placed> LoadOrder(const State& state, const std::vector<PackData>& packs);
/// For each entry: the textures it replaces in other packs lower in the list, by pack, most first.
std::vector<std::vector<std::pair<std::string, u64>>> Replaces(const State& state,
                                                               const std::vector<PackData>& packs);
/// "Replaces 120 Textures From Modern Pokemon Sprites" ("" none); `names` as Replaces, the pack
/// names in place of the folders.
std::string ReplacesText(const std::vector<std::pair<std::string, u64>>& names);

/// What the link and the saved list say (pure; the file system's answers passed in).
/// `linked`: the pack load/textures/<TID> points at ("" none). See the header comment.
State Resolve(const std::optional<State>& saved, const std::string& linked,
              const std::function<bool(const std::string&)>& exists,
              const std::function<std::string(const std::string&)>& partner);

// ---- File system ----

std::string StatePath(const std::string& load_dir, u64 title_id);
/// AzaharFrame texrow: packs in the title's texture-packs folder are saved by folder name (moving
/// the user folder keeps the list); absolute paths (other places, older lists) are read too.
std::optional<State> ReadSaved(const std::string& load_dir, u64 title_id);
bool WriteSaved(const std::string& load_dir, u64 title_id, const State& state);
/// The canonical folder load/textures/<TID> resolves to, "" when there is none.
std::string Linked(const Paths& paths, u64 title_id);
/// The other pack of the Optimize for Frame pair `pack` belongs to, when it is there ("" none).
std::string Partner(const std::string& pack);
/// The game's list now (Resolve with the disk). Whole-pack entries until Fit.
State Current(const Paths& paths, u64 title_id);
/// The catalog id of the pack (its download marker), "" none.
std::string CatalogId(const std::string& pack);
/// The catalog id of the pack it is made to go on top of: pack.json "layer_on", else its catalog
/// entry's "layer_on" ("" none).
std::string BaseOf(const std::string& pack);
/// Saves the list and points load/textures/<TID> at its top pack (no pack: removes the link).
Result Apply(const Paths& paths, u64 title_id, State state);
/// Turns one pack on or off with its default place (the crash guard, Delete). Blocking.
Result TurnOn(const Paths& paths, u64 title_id, const std::string& pack);
Result TurnOff(const Paths& paths, u64 title_id, const std::string& pack);
/// The packs on now (ListPacks entries), top first.
std::vector<Pack> ActivePacks(const Paths& paths, u64 title_id);

struct LoaderList {
    std::vector<std::pair<std::string, u64>> files; ///< absolute path, size; in the order to read
    std::vector<std::string> packs;                 ///< the packs on, top first
};
/// The loader's files, or nullopt when fewer than two packs are on (load/textures/<TID> is read as
/// before).
std::optional<LoaderList> LoaderFiles(const std::string& load_dir, u64 title_id);

// ---- A game's packs for a menu (VR and Flat) ----

/// A row of Enabled Textures.
struct Row {
    std::string pack, key; ///< the entry
    std::string name;      ///< the group's name, or the pack's for a whole pack
    std::string detail;    ///< the pack's name (or its textures), then what it replaces
    int value = -1;        ///< a checkbox: 0 off, 1 some, 2 on; -1 none (a whole pack, variants)
    bool up = false, down = false;
};

/// Read once (Load, blocking), changed in memory, written by Save.
struct Game {
    std::vector<Pack> packs;              ///< ListPacks; `active` = on in the list
    std::map<std::string, PackData> data; ///< by pack id
    std::map<std::string, std::string> catalog_ids, bases; ///< by pack id ("" none)
    State list;                           ///< fitted; with the waiting switches applied
    std::set<std::string> dirty;          ///< packs whose Customize state changed
    /// AzaharFrame texrow: the Optimize for Frame pairs, both ways, and the optimized copies (Load)
    std::map<std::string, std::string> partners;
    std::set<std::string> copies;
    std::vector<Switch> waiting; ///< switches for the next start: in `list`, not on disk yet

    static Game Load(const Paths& paths, u64 title_id, const std::atomic_bool* cancel = nullptr);
    bool IsOn(std::string_view pack) const;
    const Pack* Find(std::string_view pack) const;
    /// On: at its default place, its pair's other pack off. Off: its entries go, and its pair's
    /// other pack's (AzaharFrame texrow: a pair is one row; its waiting switch goes too).
    void SetOn(const std::string& pack, bool on);
    /// AzaharFrame texrow: a switch that waits for the next start shows in the list (ApplySwitch).
    void Wait(const Switch& s);
    /// AzaharFrame texrow: the pack list's rows (Layers::PackRows).
    std::vector<PackRow> PackRows() const;
    bool Move(std::size_t index, int delta);
    /// The entry's checkbox (a group's parts on or off).
    bool Toggle(std::size_t index);
    /// The pack's Customize state changed (its page): the list follows its order.
    void PackChanged(const std::string& pack, const Parts::State& state);
    std::vector<Row> Rows() const;
    /// Writes the changed Customize states and the list (the waiting switches undone), and points
    /// the link at the top pack.
    Result Save(const Paths& paths, u64 title_id);
};

} // namespace CustomTex::Layers
