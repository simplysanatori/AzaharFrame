// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texlayers: see layers.h.

#include "core/custom_tex/layers.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <json.hpp>
#include "common/atomic_file.h"
#include "common/logging/log.h"
#include "core/custom_tex/download.h"
#include "core/custom_tex/optimize.h"

namespace CustomTex::Layers {

namespace fs = std::filesystem;

namespace {

std::string Canonical(const fs::path& p) {
    std::error_code ec;
    const fs::path c = fs::weakly_canonical(p, ec);
    return (ec ? p : c).generic_string();
}

bool IsDir(const std::string& p) {
    std::error_code ec;
    return !p.empty() && fs::is_directory(p, ec);
}

std::string ReadText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return {};
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string LastPart(const std::string& path) {
    const std::size_t at = path.rfind('/');
    return at == std::string::npos ? path : path.substr(at + 1);
}

/// The group key a file belongs to ("" outside every group).
std::string GroupOf(const std::map<std::string, std::string, std::less<>>& part_group,
                    const std::string& path) {
    std::size_t at = path.find('/');
    while (at != std::string::npos) {
        const auto it = part_group.find(std::string_view(path).substr(0, at));
        if (it != part_group.end()) {
            return it->second;
        }
        at = path.find('/', at + 1);
    }
    return {};
}

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& p : parts) {
        if (p.empty()) {
            continue;
        }
        out += (out.empty() ? "" : " · ") + p;
    }
    return out;
}

} // namespace

// ---- State ----

State ParseState(std::string_view text) {
    State s;
    const nlohmann::json j = nlohmann::json::parse(text, nullptr, false, true);
    if (!j.is_object()) {
        return s;
    }
    const auto entries = j.find("entries");
    if (entries == j.end() || !entries->is_array()) {
        return s;
    }
    for (const auto& e : *entries) {
        if (!e.is_object()) {
            continue;
        }
        const auto pack = e.find("pack");
        if (pack == e.end() || !pack->is_string() || pack->get<std::string>().empty()) {
            continue;
        }
        Entry entry{pack->get<std::string>(), {}};
        if (const auto key = e.find("key"); key != e.end() && key->is_string()) {
            entry.key = key->get<std::string>();
        }
        const bool dup = std::find(s.entries.begin(), s.entries.end(), entry) != s.entries.end();
        if (!dup) {
            s.entries.push_back(std::move(entry));
        }
    }
    return s;
}

std::string StateJson(const State& state) {
    nlohmann::ordered_json j;
    j["version"] = 1;
    auto entries = nlohmann::ordered_json::array();
    for (const Entry& e : state.entries) {
        nlohmann::ordered_json o;
        o["pack"] = e.pack;
        o["key"] = e.key;
        entries.push_back(std::move(o));
    }
    j["entries"] = std::move(entries);
    return j.dump(2) + "\n";
}

// ---- The list ----

bool Splits(const Parts::Layout& layout) {
    return layout.groups.size() >= kMinSplit && layout.groups.size() <= kMaxSplit;
}

std::vector<std::string> PackKeys(const Parts::Layout& layout, const Parts::State& state) {
    if (!Splits(layout)) {
        return {std::string{}};
    }
    std::vector<std::string> keys;
    for (const std::size_t i : Parts::Order(layout, state)) {
        keys.push_back(layout.groups[i].key);
    }
    return keys;
}

std::vector<std::string> Packs(const State& state) {
    std::vector<std::string> out;
    for (const Entry& e : state.entries) {
        if (std::find(out.begin(), out.end(), e.pack) == out.end()) {
            out.push_back(e.pack);
        }
    }
    return out;
}

bool Has(const State& state, std::string_view pack) {
    return std::any_of(state.entries.begin(), state.entries.end(),
                       [&](const Entry& e) { return e.pack == pack; });
}

void Fit(State& state, const std::string& pack, const std::vector<std::string>& keys_in) {
    if (!Has(state, pack)) {
        return;
    }
    const std::vector<std::string> keys = keys_in.empty() ? std::vector<std::string>{""} : keys_in;
    const bool whole = keys.size() == 1 && keys[0].empty();
    const auto known = [&](const std::string& k) {
        return std::find(keys.begin(), keys.end(), k) != keys.end();
    };
    std::vector<Entry> out;
    std::set<std::string> placed;
    std::size_t anchor = std::string::npos; // where its first entry was
    std::size_t last = std::string::npos;   // after its last entry placed
    for (const Entry& e : state.entries) {
        if (e.pack != pack) {
            out.push_back(e);
            continue;
        }
        if (anchor == std::string::npos) {
            anchor = out.size();
        }
        if (whole) {
            if (placed.insert("").second) {
                out.push_back({pack, ""});
                last = out.size();
            }
            continue;
        }
        if (e.key.empty()) { // the whole pack, which now splits: its groups here
            for (const std::string& k : keys) {
                if (placed.insert(k).second) {
                    out.push_back({pack, k});
                    last = out.size();
                }
            }
        } else if (known(e.key) && placed.insert(e.key).second) {
            out.push_back(e);
            last = out.size();
        }
    }
    std::vector<Entry> missing;
    for (const std::string& k : keys) {
        if (!placed.count(k)) {
            missing.push_back({pack, k});
        }
    }
    const std::size_t at = last != std::string::npos ? last : anchor;
    out.insert(out.begin() + static_cast<std::ptrdiff_t>(std::min(at, out.size())), missing.begin(),
               missing.end());
    state.entries = std::move(out);
}

void Add(State& state, const std::string& pack, const std::vector<std::string>& keys_in,
         const std::vector<std::string>& above) {
    if (pack.empty() || Has(state, pack)) {
        return;
    }
    const std::vector<std::string> keys = keys_in.empty() ? std::vector<std::string>{""} : keys_in;
    std::size_t at = 0;
    for (std::size_t n = 0; n < state.entries.size(); n++) {
        if (std::find(above.begin(), above.end(), state.entries[n].pack) != above.end()) {
            at = n + 1;
        }
    }
    std::vector<Entry> add;
    for (const std::string& k : keys) {
        add.push_back({pack, k});
    }
    state.entries.insert(state.entries.begin() + static_cast<std::ptrdiff_t>(at), add.begin(),
                         add.end());
}

void Remove(State& state, std::string_view pack) {
    std::erase_if(state.entries, [&](const Entry& e) { return e.pack == pack; });
}

void Replace(State& state, const std::string& from, const std::string& to) {
    if (from == to || !Has(state, from)) {
        return;
    }
    if (Has(state, to)) {
        Remove(state, from);
        return;
    }
    for (Entry& e : state.entries) {
        if (e.pack == from) {
            e.pack = to;
        }
    }
}

void ApplySwitch(State& state, const Switch& s) {
    if (s.from.empty() || s.to.empty() || s.from == s.to) {
        return;
    }
    if (Has(state, s.from)) {
        Replace(state, s.from, s.to);
    } else {
        Add(state, s.to, {std::string{}});
    }
}

void UndoSwitch(State& state, const Switch& s) {
    if (!s.from.empty() && !s.to.empty()) {
        Replace(state, s.to, s.from);
    }
}

std::vector<PackRow> PackRows(const std::vector<Pack>& packs, const State& list,
                              const std::map<std::string, std::string>& partners,
                              const std::set<std::string>& copies) {
    std::map<std::string, std::size_t, std::less<>> index;
    for (std::size_t i = 0; i < packs.size(); i++) {
        index.emplace(packs[i].id, i);
    }
    std::vector<PackRow> rows;
    std::vector<bool> done(packs.size(), false);
    for (std::size_t i = 0; i < packs.size(); i++) {
        if (done[i]) {
            continue;
        }
        done[i] = true;
        PackRow r;
        r.shown = i;
        r.on = Has(list, packs[i].id);
        if (const auto p = partners.find(packs[i].id); p != partners.end()) {
            if (const auto o = index.find(p->second); o != index.end() && !done[o->second]) {
                const std::size_t j = o->second;
                done[j] = true;
                const bool j_on = Has(list, packs[j].id);
                // The copy in the list; neither: the optimized copy (the one turned on)
                const bool show_j = j_on ? !r.on || copies.count(packs[j].id) != 0
                                         : !r.on && copies.count(packs[j].id) != 0;
                r.other = show_j ? i : j;
                r.shown = show_j ? j : i;
                r.on = r.on || j_on;
            }
        }
        rows.push_back(r);
    }
    return rows;
}

bool Move(State& state, std::size_t index, int delta) {
    const std::ptrdiff_t to = static_cast<std::ptrdiff_t>(index) + delta;
    if (index >= state.entries.size() || to < 0 ||
        to >= static_cast<std::ptrdiff_t>(state.entries.size()) || delta == 0) {
        return false;
    }
    std::swap(state.entries[index], state.entries[static_cast<std::size_t>(to)]);
    return true;
}

std::vector<std::string> PackOrder(const State& state, std::string_view pack) {
    std::vector<std::string> out;
    for (const Entry& e : state.entries) {
        if (e.pack == pack) {
            out.push_back(e.key);
        }
    }
    return out;
}

void SetPackOrder(State& state, std::string_view pack, const std::vector<std::string>& order) {
    std::vector<std::size_t> places;
    std::vector<std::string> have;
    for (std::size_t n = 0; n < state.entries.size(); n++) {
        if (state.entries[n].pack == pack) {
            places.push_back(n);
            have.push_back(state.entries[n].key);
        }
    }
    std::vector<std::string> keys;
    for (const std::string& k : order) {
        if (std::find(have.begin(), have.end(), k) != have.end() &&
            std::find(keys.begin(), keys.end(), k) == keys.end()) {
            keys.push_back(k);
        }
    }
    for (const std::string& k : have) {
        if (std::find(keys.begin(), keys.end(), k) == keys.end()) {
            keys.push_back(k);
        }
    }
    for (std::size_t i = 0; i < places.size() && i < keys.size(); i++) {
        state.entries[places[i]].key = keys[i];
    }
}

PackData Read(const std::string& pack, const std::atomic_bool* cancel) {
    PackData d;
    d.pack = pack;
    d.files = Parts::ListFiles(pack, cancel);
    d.state = Parts::ReadState(pack);
    d.layout = Parts::Build(d.files, d.state.hints);
    return d;
}

std::vector<std::vector<Placed>> EntryFiles(const State& state,
                                            const std::vector<PackData>& packs) {
    std::map<std::string, std::size_t, std::less<>> index;
    for (std::size_t i = 0; i < packs.size(); i++) {
        index.emplace(packs[i].pack, i);
    }
    struct Sorted {
        bool split = false;
        std::map<std::string, std::vector<std::size_t>> groups; ///< group key ("" loose) -> files
        std::vector<std::size_t> all;                            ///< Parts::LoadOrder
        std::vector<bool> emitted;
        std::size_t last = 0; ///< its lowest entry
    };
    std::vector<Sorted> sorted(packs.size());
    for (std::size_t i = 0; i < packs.size(); i++) {
        const PackData& d = packs[i];
        Sorted& s = sorted[i];
        s.split = Splits(d.layout);
        s.all = Parts::LoadOrder(d.layout, d.state, d.files);
        s.emitted.assign(d.files.size(), false);
        if (s.split) {
            std::map<std::string, std::string, std::less<>> part_group;
            for (const Parts::Group& g : d.layout.groups) {
                for (const Parts::Part& p : g.parts) {
                    part_group[p.key] = g.key;
                }
            }
            for (const std::size_t f : s.all) {
                s.groups[GroupOf(part_group, d.files[f].path)].push_back(f);
            }
        }
    }
    for (std::size_t n = 0; n < state.entries.size(); n++) {
        if (const auto it = index.find(state.entries[n].pack); it != index.end()) {
            sorted[it->second].last = n;
        }
    }
    std::vector<std::vector<Placed>> out(state.entries.size());
    for (std::size_t n = 0; n < state.entries.size(); n++) {
        const Entry& e = state.entries[n];
        const auto it = index.find(e.pack);
        if (it == index.end()) {
            continue;
        }
        const std::size_t i = it->second;
        Sorted& s = sorted[i];
        const auto emit = [&](const std::vector<std::size_t>& files) {
            for (const std::size_t f : files) {
                if (!s.emitted[f]) {
                    s.emitted[f] = true;
                    out[n].push_back({i, f});
                }
            }
        };
        if (e.key.empty() || !s.split) {
            emit(s.all);
            continue;
        }
        if (const auto g = s.groups.find(e.key); g != s.groups.end()) {
            emit(g->second);
        }
        if (n == s.last) {
            if (const auto g = s.groups.find(std::string{}); g != s.groups.end()) {
                emit(g->second);
            }
        }
    }
    return out;
}

std::vector<Placed> LoadOrder(const State& state, const std::vector<PackData>& packs) {
    std::vector<Placed> out;
    for (const auto& files : EntryFiles(state, packs)) {
        out.insert(out.end(), files.begin(), files.end());
    }
    return out;
}

std::vector<std::vector<std::pair<std::string, u64>>> Replaces(const State& state,
                                                               const std::vector<PackData>& packs) {
    const auto files = EntryFiles(state, packs);
    std::vector<std::unordered_set<u64>> hashes(files.size());
    for (std::size_t n = 0; n < files.size(); n++) {
        for (const Placed& p : files[n]) {
            if (const u64 h = Parts::TextureHash(LastPart(packs[p.pack].files[p.file].path))) {
                hashes[n].insert(h);
            }
        }
    }
    std::vector<std::vector<std::pair<std::string, u64>>> out(files.size());
    std::map<std::string, std::unordered_set<u64>> below; // by pack: the hashes under this entry
    for (std::size_t n = files.size(); n-- > 0;) {
        const std::string& pack = state.entries[n].pack;
        for (const auto& [other, set] : below) {
            if (other == pack) {
                continue;
            }
            u64 count = 0;
            for (const u64 h : hashes[n]) {
                count += set.count(h);
            }
            if (count > 0) {
                out[n].emplace_back(other, count);
            }
        }
        std::stable_sort(out[n].begin(), out[n].end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
        below[pack].insert(hashes[n].begin(), hashes[n].end());
    }
    return out;
}

std::string ReplacesText(const std::vector<std::pair<std::string, u64>>& names) {
    if (names.empty()) {
        return {};
    }
    u64 total = 0;
    for (const auto& [name, n] : names) {
        total += n;
    }
    std::string from = names[0].first;
    if (names.size() == 2) {
        from += " and " + names[1].first;
    } else if (names.size() > 2) {
        from += " and " + std::to_string(names.size() - 1) + " More";
    }
    return "Replaces " + Parts::CountText(total, "Texture", "Textures") + " From " + from;
}

State Resolve(const std::optional<State>& saved, const std::string& linked,
              const std::function<bool(const std::string&)>& exists,
              const std::function<std::string(const std::string&)>& partner) {
    State s;
    if (linked.empty()) {
        return s; // no pack is read: textures off
    }
    if (!saved) {
        s.entries.push_back({linked, ""}); // before layers: the one pack it points at
        return s;
    }
    s = *saved;
    std::erase_if(s.entries, [&](const Entry& e) { return !exists(e.pack); });
    if (!Has(s, linked)) {
        for (const std::string& p : Packs(s)) {
            if (partner(p) == linked) {
                Replace(s, p, linked); // the pair switched behind the list's back
                break;
            }
        }
    }
    if (!Has(s, linked)) {
        s.entries = {{linked, ""}}; // switched by something else: the link wins
    }
    return s;
}

// ---- File system ----

std::string StatePath(const std::string& load_dir, u64 title_id) {
    return (fs::path(load_dir) / "texture-packs" / TitleFolder(title_id) / kStateFile).string();
}

std::optional<State> ReadSaved(const std::string& load_dir, u64 title_id) {
    const fs::path path = StatePath(load_dir, title_id);
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        return std::nullopt;
    }
    State saved = ParseState(ReadText(path));
    // AzaharFrame texrow: a folder name is a pack of this folder (Pack::id: the canonical path)
    const std::string base = Canonical(path.parent_path());
    State out;
    for (Entry e : saved.entries) {
        if (!fs::path(e.pack).is_absolute()) {
            e.pack = Canonical(fs::path(base) / e.pack);
        }
        if (std::find(out.entries.begin(), out.entries.end(), e) == out.entries.end()) {
            out.entries.push_back(std::move(e));
        }
    }
    return out;
}

bool WriteSaved(const std::string& load_dir, u64 title_id, const State& state) {
    const fs::path path = StatePath(load_dir, title_id);
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    // AzaharFrame texrow: the packs of this folder by folder name (ids are canonical paths)
    const std::string base = Canonical(path.parent_path());
    State out = state;
    for (Entry& e : out.entries) {
        const fs::path p(e.pack);
        if (p.is_absolute() && p.has_filename() && p.parent_path().generic_string() == base) {
            e.pack = p.filename().generic_string();
        }
    }
    return Common::WriteFileAtomic(path.string(), StateJson(out));
}

std::string Linked(const Paths& paths, u64 title_id) {
    const fs::path load = LoadFolder(paths, title_id);
    std::error_code ec;
    const fs::file_status st = fs::symlink_status(load, ec);
    if (!fs::exists(st) || !fs::is_directory(load, ec) || fs::is_empty(load, ec)) {
        return {};
    }
    return Canonical(load);
}

std::string Partner(const std::string& pack) {
    if (Optimize::IsOptimized(pack)) {
        const auto source = Optimize::SourceOf(pack);
        return source && IsDir(*source) ? Canonical(*source) : std::string{};
    }
    const std::string copy = Optimize::OptimizedPath(pack);
    return !copy.empty() && Optimize::IsOptimized(copy) ? Canonical(copy) : std::string{};
}

State Current(const Paths& paths, u64 title_id) {
    return Resolve(ReadSaved(paths.load_dir, title_id), Linked(paths, title_id), IsDir, Partner);
}

std::string CatalogId(const std::string& pack) {
    return Download::MarkerId(pack).value_or(std::string{});
}

std::string BaseOf(const std::string& pack) {
    const nlohmann::json j =
        nlohmann::json::parse(ReadText(fs::path(pack) / "pack.json"), nullptr, false, true);
    if (j.is_object()) {
        if (const auto it = j.find("layer_on"); it != j.end() && it->is_string()) {
            return it->get<std::string>();
        }
    }
    const std::string id = CatalogId(pack);
    if (id.empty()) {
        return {};
    }
    static const Download::Catalog catalog = Download::DefaultCatalog();
    const Download::Entry* entry = catalog.Find(id);
    return entry ? entry->layer_on : std::string{};
}

Result Apply(const Paths& paths, u64 title_id, State state) {
    if (!WriteSaved(paths.load_dir, title_id, state)) {
        return {false, "Could not save the texture pack order", {}};
    }
    if (state.entries.empty()) {
        return Deactivate(paths, title_id);
    }
    // A real folder at the load place is moved into the library by Activate: its entries follow.
    std::error_code ec;
    const fs::path load = LoadFolder(paths, title_id);
    const bool real = fs::is_directory(fs::symlink_status(load, ec));
    const std::string real_id = real ? Canonical(load) : std::string{};
    Pack top;
    top.path = state.entries[0].pack;
    Result r = Activate(paths, title_id, top);
    if (r.ok && !r.backed_up.empty() && !real_id.empty()) {
        Replace(state, real_id, Canonical(r.backed_up));
        if (!state.entries.empty() && state.entries[0].pack != top.path) {
            top.path = state.entries[0].pack;
            r = Activate(paths, title_id, top);
        }
        WriteSaved(paths.load_dir, title_id, state);
    }
    return r;
}

Result TurnOn(const Paths& paths, u64 title_id, const std::string& pack) {
    Game g = Game::Load(paths, title_id);
    if (!g.Find(pack)) {
        return {false, "The texture pack is missing: " + pack, {}};
    }
    g.SetOn(pack, true);
    return g.Save(paths, title_id);
}

Result TurnOff(const Paths& paths, u64 title_id, const std::string& pack) {
    State s = Current(paths, title_id);
    if (!Has(s, pack)) {
        return {true, {}, {}};
    }
    Remove(s, pack);
    return Apply(paths, title_id, std::move(s));
}

std::vector<Pack> ActivePacks(const Paths& paths, u64 title_id) {
    const std::vector<Pack> all = ListPacks(paths, title_id);
    std::vector<Pack> out;
    for (const std::string& id : Packs(Current(paths, title_id))) {
        for (const Pack& p : all) {
            if (p.id == id) {
                out.push_back(p);
                out.back().active = true;
            }
        }
    }
    return out;
}

std::optional<LoaderList> LoaderFiles(const std::string& load_dir, u64 title_id) {
    Paths paths;
    paths.load_dir = load_dir;
    State state = Current(paths, title_id);
    const std::vector<std::string> ids = Packs(state);
    if (ids.size() < 2) {
        return std::nullopt;
    }
    std::vector<PackData> data;
    for (const std::string& id : ids) {
        data.push_back(Read(id));
        Fit(state, id, PackKeys(data.back().layout, data.back().state));
    }
    LoaderList out;
    out.packs = ids;
    for (const Placed& p : LoadOrder(state, data)) {
        const Parts::File& f = data[p.pack].files[p.file];
        // the loader's own formats only (pack.json, markers and notes stay out)
        const std::size_t dot = f.path.rfind('.');
        std::string ext = dot == std::string::npos ? std::string{} : f.path.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == "png" || ext == "dds" || ext == "ktx") {
            out.files.emplace_back(data[p.pack].pack + "/" + f.path, f.size);
        }
    }
    std::string order;
    for (const Entry& e : state.entries) {
        order += (order.empty() ? "" : ", ") + LastPart(e.pack) + (e.key.empty() ? "" : "/" + e.key);
    }
    LOG_INFO(Render, "custom textures: {} packs on, {} files, top first: {}", ids.size(),
             out.files.size(), order);
    return out;
}

// ---- Game ----

Game Game::Load(const Paths& paths, u64 title_id, const std::atomic_bool* cancel) {
    Game g;
    g.packs = ListPacks(paths, title_id);
    for (const Pack& p : g.packs) {
        g.data[p.id] = Read(p.path, cancel);
        g.data[p.id].pack = p.id;
        g.catalog_ids[p.id] = CatalogId(p.path);
        g.bases[p.id] = BaseOf(p.path);
    }
    for (const Pack& p : g.packs) { // AzaharFrame texrow: the Optimize for Frame pairs
        if (Optimize::IsOptimized(p.path)) {
            g.copies.insert(p.id);
        }
        const std::string other = Partner(p.path);
        if (!other.empty() && other != p.id && g.data.count(other) != 0) {
            g.partners[p.id] = other;
            g.partners[other] = p.id;
        }
    }
    g.list = Current(paths, title_id);
    for (const std::string& id : Packs(g.list)) {
        const auto it = g.data.find(id);
        if (it == g.data.end()) {
            Remove(g.list, id);
            continue;
        }
        Fit(g.list, id, PackKeys(it->second.layout, it->second.state));
    }
    for (Pack& p : g.packs) {
        p.active = Has(g.list, p.id);
    }
    return g;
}

bool Game::IsOn(std::string_view pack) const {
    return Has(list, pack);
}

const Pack* Game::Find(std::string_view pack) const {
    for (const Pack& p : packs) {
        if (p.id == pack) {
            return &p;
        }
    }
    return nullptr;
}

void Game::SetOn(const std::string& pack, bool on) {
    const auto d = data.find(pack);
    if (on && d != data.end() && !IsOn(pack)) {
        const std::vector<std::string> keys = PackKeys(d->second.layout, d->second.state);
        const std::string partner = Partner(pack);
        if (!partner.empty() && IsOn(partner)) {
            Replace(list, partner, pack); // one of a pair: the other goes, in its place
            Fit(list, pack, keys);
            // AzaharFrame texrow: chosen now; a switch that waited the other way goes
            std::erase_if(waiting, [&](const Switch& s) { return s.from == pack || s.to == partner; });
        } else {
            std::vector<std::string> above;
            const std::string id = catalog_ids[pack];
            for (const std::string& p : Packs(list)) {
                if (!id.empty() && bases[p] == id) {
                    above.push_back(p);
                }
            }
            Add(list, pack, keys, above);
        }
    } else if (!on) {
        Remove(list, pack);
        // AzaharFrame texrow: the pair is one row: both copies off, and no switch waits for it
        const auto it = partners.find(pack);
        const std::string other = it != partners.end() ? it->second : Partner(pack);
        if (!other.empty()) {
            Remove(list, other);
        }
        std::erase_if(waiting, [&](const Switch& s) {
            return s.from == pack || s.to == pack || (!other.empty() && (s.from == other || s.to == other));
        });
    }
    for (Pack& p : packs) {
        p.active = Has(list, p.id);
    }
}

void Game::Wait(const Switch& s) {
    if (data.find(s.to) == data.end()) {
        return; // the copy is gone
    }
    std::erase_if(waiting, [&](const Switch& w) {
        return w.from == s.from || w.to == s.from || w.from == s.to || w.to == s.to;
    });
    waiting.push_back(s);
    ApplySwitch(list, s);
    const PackData& d = data.at(s.to);
    Fit(list, s.to, PackKeys(d.layout, d.state));
    for (Pack& p : packs) {
        p.active = Has(list, p.id);
    }
}

std::vector<PackRow> Game::PackRows() const {
    return Layers::PackRows(packs, list, partners, copies);
}

bool Game::Move(std::size_t index, int delta) {
    if (!Layers::Move(list, index, delta)) {
        return false;
    }
    // Two entries of one pack swapped: its Customize order follows.
    const std::string& pack = list.entries[static_cast<std::size_t>(
                                               static_cast<std::ptrdiff_t>(index) + delta)]
                                  .pack;
    const auto d = data.find(pack);
    if (d != data.end() && Splits(d->second.layout)) {
        const std::vector<std::string> order = PackOrder(list, pack);
        if (order != PackKeys(d->second.layout, d->second.state)) {
            Parts::SetOrder(d->second.state, d->second.layout, order);
            dirty.insert(pack);
        }
    }
    return true;
}

bool Game::Toggle(std::size_t index) {
    if (index >= list.entries.size()) {
        return false;
    }
    const Entry& e = list.entries[index];
    const auto d = data.find(e.pack);
    if (e.key.empty() || d == data.end()) {
        return false;
    }
    PackData& pd = d->second;
    const Parts::Group* g = pd.layout.FindGroup(e.key);
    if (!g || g->kind == Parts::Kind::Choice || g->parts.empty()) {
        return false;
    }
    if (g->kind == Parts::Kind::Single) {
        const Parts::Part& p = g->parts[0];
        Parts::SetPart(pd.state, pd.layout, p.key, !Parts::IsOn(pd.layout, pd.state, *g, p));
    } else {
        Parts::SetGroup(pd.state, pd.layout, *g, Parts::GroupValue(pd.layout, pd.state, *g) != 2);
    }
    dirty.insert(e.pack);
    return true;
}

void Game::PackChanged(const std::string& pack, const Parts::State& state) {
    const auto d = data.find(pack);
    if (d == data.end()) {
        return;
    }
    d->second.state = state;
    if (IsOn(pack)) {
        const std::vector<std::string> keys = PackKeys(d->second.layout, state);
        Fit(list, pack, keys);
        SetPackOrder(list, pack, keys);
    }
}

std::vector<Row> Game::Rows() const {
    std::vector<PackData> on;
    std::map<std::string, std::string> names;
    for (const std::string& id : Packs(list)) {
        if (const auto d = data.find(id); d != data.end()) {
            on.push_back(d->second);
        }
        const Pack* p = Find(id);
        names[id] = p ? p->name : LastPart(id);
    }
    const auto replaces = Replaces(list, on);
    std::vector<Row> rows;
    for (std::size_t n = 0; n < list.entries.size(); n++) {
        const Entry& e = list.entries[n];
        Row r;
        r.pack = e.pack;
        r.key = e.key;
        std::vector<std::pair<std::string, u64>> named;
        for (const auto& [pack, count] : replaces[n]) {
            named.emplace_back(names[pack], count);
        }
        const std::string hint = ReplacesText(named);
        const auto d = data.find(e.pack);
        const Parts::Group* g = d != data.end() && !e.key.empty() ? d->second.layout.FindGroup(e.key)
                                                                  : nullptr;
        if (g) {
            r.name = g->name;
            r.detail = Join({names[e.pack], hint});
            if (g->kind != Parts::Kind::Choice) {
                r.value = Parts::GroupValue(d->second.layout, d->second.state, *g);
            }
        } else {
            r.name = names[e.pack];
            const u64 textures =
                d != data.end() ? Parts::TotalsOn(d->second.layout, d->second.state).first : 0;
            r.detail = Join({Parts::CountText(textures, "Texture", "Textures"), hint});
        }
        r.up = n > 0;
        r.down = n + 1 < list.entries.size();
        rows.push_back(std::move(r));
    }
    return rows;
}

Result Game::Save(const Paths& paths, u64 title_id) {
    for (const std::string& pack : dirty) {
        if (const auto d = data.find(pack); d != data.end()) {
            if (!Parts::WriteState(pack, d->second.state)) {
                LOG_WARNING(Frontend, "texture layers: cannot save the parts of {}", pack);
            }
        }
    }
    dirty.clear();
    State now = list; // AzaharFrame texrow: the game reads `from` until the next start
    for (auto it = waiting.rbegin(); it != waiting.rend(); ++it) {
        UndoSwitch(now, *it);
    }
    return Apply(paths, title_id, std::move(now));
}

} // namespace CustomTex::Layers
