// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texparts: see parts.h.

#include "common/atomic_file.h"
#include "core/custom_tex/parts.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <json.hpp>

namespace CustomTex::Parts {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kRankStride = 1024; // ranks of a group's parts: position * stride + index
constexpr std::size_t kLooseRank = static_cast<std::size_t>(-1) / 2;
constexpr u64 kPreviewMaxBytes = 1'500'000;
constexpr std::size_t kPreviewCandidates = 64;
constexpr const char* kKtxMarker =
    ".azaharframe-ktx.json"; // core/custom_tex/convert.h

std::string Lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::vector<std::string> Split(std::string_view path) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at <= path.size()) {
        std::size_t end = path.find('/', at);
        if (end == std::string_view::npos) {
            end = path.size();
        }
        if (end > at) {
            out.emplace_back(path.substr(at, end - at));
        }
        at = end + 1;
    }
    return out;
}

std::string Join(const std::vector<std::string>& parts, std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count && i < parts.size(); i++) {
        if (!out.empty()) {
            out += '/';
        }
        out += parts[i];
    }
    return out;
}

std::string Child(const std::string& dir, const std::string& name) {
    return dir.empty() ? name : dir + "/" + name;
}

std::string LastPart(std::string_view key) {
    const std::size_t slash = key.rfind('/');
    return std::string(slash == std::string_view::npos ? key : key.substr(slash + 1));
}

/// '*' matches any text; case does not matter.
bool Matches(std::string_view pattern, std::string_view text) {
    const std::string p = Lower(pattern), t = Lower(text);
    std::size_t pi = 0, ti = 0, star = std::string::npos, mark = 0;
    while (ti < t.size()) {
        if (pi < p.size() && p[pi] == t[ti]) {
            pi++;
            ti++;
        } else if (pi < p.size() && p[pi] == '*') {
            star = pi++;
            mark = ti;
        } else if (star != std::string::npos) {
            pi = star + 1;
            ti = ++mark;
        } else {
            return false;
        }
    }
    while (pi < p.size() && p[pi] == '*') {
        pi++;
    }
    return pi == p.size();
}

bool MatchesAny(const std::vector<std::string>& patterns, std::string_view folder,
                std::string_view name) {
    return std::any_of(patterns.begin(), patterns.end(), [&](const std::string& p) {
        return Matches(p, folder) || Matches(p, name);
    });
}

bool IsBaseName(std::string_view name) {
    const std::string l = Lower(name);
    return l == "main" || l == "base" || l == "original" || l == "core";
}

/// "[Fonts] Original N64" -> {"Fonts", "Original N64"}.
std::optional<std::pair<std::string, std::string>> Tagged(const std::string& folder) {
    if (folder.size() < 3 || folder[0] != '[') {
        return std::nullopt;
    }
    const std::size_t close = folder.find(']');
    if (close == std::string::npos || close < 2) {
        return std::nullopt;
    }
    std::string rest = folder.substr(close + 1);
    rest.erase(0, std::min(rest.size(), rest.find_first_not_of(' ')));
    if (rest.empty()) {
        return std::nullopt; // "[Main]": a name in brackets, no tag (UntagName)
    }
    return std::make_pair(folder.substr(1, close - 1), rest);
}

/// "[Main]" -> "Main".
std::string UntagName(const std::string& folder) {
    if (folder.size() > 2 && folder.front() == '[' && folder.back() == ']' &&
        folder.find(']') == folder.size() - 1) {
        return folder.substr(1, folder.size() - 2);
    }
    return folder;
}

std::vector<std::string> Words(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string w; in >> w;) {
        out.push_back(w);
    }
    return out;
}

/// Variants named "Touchless Layout (Vanilla) Xbox UI", "... Dualshock UI": the words most of them
/// start with are dropped ("Xbox UI").
void StripCommonPrefix(std::vector<Part>& parts) {
    if (parts.size() < 2) {
        return;
    }
    std::map<std::string, int> first;
    for (const Part& p : parts) {
        const auto w = Words(p.name);
        if (!w.empty()) {
            first[w[0]]++;
        }
    }
    const auto best =
        std::max_element(first.begin(), first.end(),
                         [](const auto& a, const auto& b) { return a.second < b.second; });
    if (best == first.end() || best->second < 2 ||
        best->second * 2 < static_cast<int>(parts.size())) {
        return;
    }
    std::vector<std::string> prefix;
    bool started = false;
    for (const Part& p : parts) {
        const auto w = Words(p.name);
        if (w.empty() || w[0] != best->first) {
            continue;
        }
        if (!started) {
            prefix = w;
            started = true;
            continue;
        }
        std::size_t n = 0;
        while (n < prefix.size() && n < w.size() && prefix[n] == w[n]) {
            n++;
        }
        prefix.resize(n);
    }
    if (prefix.size() < 2) {
        return; // "Original N64", "Original Pizza" keep their names
    }
    for (Part& p : parts) {
        const auto w = Words(p.name);
        if (w.size() <= prefix.size() || !std::equal(prefix.begin(), prefix.end(), w.begin())) {
            continue;
        }
        std::string rest;
        for (std::size_t i = prefix.size(); i < w.size(); i++) {
            rest += (rest.empty() ? "" : " ") + w[i];
        }
        p.name = rest;
    }
}

bool DefaultOff(const Hints& hints, const Group& group, const Part& part) {
    if (MatchesAny(hints.off, LastPart(part.key), part.name)) {
        return true;
    }
    return group.kind == Kind::Folder && MatchesAny(hints.off, LastPart(group.key), group.name);
}

std::string DefaultChoice(const Hints& hints, const Group& group) {
    for (const Part& p : group.parts) {
        if (!DefaultOff(hints, group, p)) {
            return p.key;
        }
    }
    return {};
}

/// Fills in the defaults, so a change starts from what the player sees.
void Materialize(State& state, const Layout& layout) {
    if (state.set) {
        return;
    }
    state.set = true;
    state.off.clear();
    for (const Group& g : layout.groups) {
        if (g.kind == Kind::Choice) {
            if (!state.choice.count(g.key)) {
                state.choice[g.key] = DefaultChoice(state.hints, g);
            }
            continue;
        }
        for (const Part& p : g.parts) {
            if (DefaultOff(state.hints, g, p)) {
                state.off.insert(p.key);
            }
        }
    }
}

/// The part key each folder path belongs to, and that part's rank (none for parts that are off).
struct Ranks {
    std::map<std::string, std::optional<std::size_t>, std::less<>> by_key;

    /// nullopt: drop the file; else its rank.
    std::optional<std::size_t> Of(const std::string& path) const {
        std::size_t at = path.find('/');
        while (at != std::string::npos) {
            const auto it = by_key.find(std::string_view(path).substr(0, at));
            if (it != by_key.end()) {
                return it->second;
            }
            at = path.find('/', at + 1);
        }
        return kLooseRank;
    }
};

Ranks MakeRanks(const Layout& layout, const State& state) {
    Ranks r;
    const auto order = Order(layout, state);
    for (std::size_t pos = 0; pos < order.size(); pos++) {
        const Group& g = layout.groups[order[pos]];
        for (std::size_t i = 0; i < g.parts.size(); i++) {
            const Part& p = g.parts[i];
            r.by_key[p.key] = IsOn(layout, state, g, p)
                                  ? std::optional<std::size_t>(pos * kRankStride + i)
                                  : std::nullopt;
        }
    }
    return r;
}

/// The part key of a file ("" none).
std::string PartOf(const std::set<std::string, std::less<>>& keys, const std::string& path) {
    std::size_t at = path.find('/');
    while (at != std::string::npos) {
        if (keys.count(std::string_view(path).substr(0, at))) {
            return path.substr(0, at);
        }
        at = path.find('/', at + 1);
    }
    return {};
}

void PickPreviews(Layout& layout, const std::string& dir, const std::vector<File>& files) {
    std::set<std::string, std::less<>> keys;
    std::map<std::string, Part*> wanted;
    for (Group& g : layout.groups) {
        for (Part& p : g.parts) {
            if (p.preview.empty()) {
                keys.insert(p.key);
                wanted[p.key] = &p;
            }
        }
    }
    if (wanted.empty()) {
        return;
    }
    std::vector<const File*> sorted;
    for (const File& f : files) {
        sorted.push_back(&f);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const File* a, const File* b) { return a->path < b->path; });
    std::map<std::string, std::pair<std::size_t, const File*>> seen; // candidates looked at, best
    for (const File* f : sorted) {
        const std::string l = Lower(f->path);
        if (l.size() < 4 || l.compare(l.size() - 4, 4, ".png") != 0 ||
            !IsTextureFile(LastPart(f->path))) {
            continue;
        }
        const std::string key = PartOf(keys, f->path);
        if (key.empty()) {
            continue;
        }
        auto& [count, best] = seen[key];
        if (count >= kPreviewCandidates) {
            continue;
        }
        count++;
        if (f->size <= kPreviewMaxBytes && (!best || f->size > best->size)) {
            best = f;
        }
    }
    for (const auto& [key, s] : seen) {
        if (s.second) {
            wanted[key]->preview = (fs::path(dir) / fs::path(s.second->path)).string();
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------------------------

std::size_t Layout::PartCount() const {
    std::size_t n = 0;
    for (const Group& g : groups) {
        n += g.parts.size();
    }
    return n;
}

const Group* Layout::FindGroup(std::string_view key) const {
    for (const Group& g : groups) {
        if (g.key == key) {
            return &g;
        }
    }
    return nullptr;
}

const Part* Layout::FindPart(std::string_view key) const {
    for (const Group& g : groups) {
        for (const Part& p : g.parts) {
            if (p.key == key) {
                return &p;
            }
        }
    }
    return nullptr;
}

bool IsTextureFile(std::string_view file_name) {
    const std::string l = Lower(file_name);
    if (l.rfind("tex1_", 0) != 0) {
        return false;
    }
    const auto dot = l.rfind('.');
    if (dot == std::string::npos) {
        return false;
    }
    const std::string ext = l.substr(dot);
    return ext == ".png" || ext == ".dds" || ext == ".ktx";
}

u64 TextureHash(std::string_view file_name) {
    // tex1_<w>x<h>_<hash>_<format>[_mip0].<ext>
    if (!IsTextureFile(file_name)) {
        return 0;
    }
    const std::size_t a = file_name.find('_', 5);
    if (a == std::string_view::npos) {
        return 0;
    }
    const std::size_t b = file_name.find('_', a + 1);
    const std::string_view hex =
        file_name.substr(a + 1, b == std::string_view::npos ? 0 : b - a - 1);
    if (hex.empty() || hex.size() > 16) {
        return 0;
    }
    u64 v = 0;
    for (const char c : hex) {
        const int d = std::isdigit(static_cast<unsigned char>(c)) ? c - '0'
                      : (std::tolower(static_cast<unsigned char>(c)) >= 'a' &&
                         std::tolower(static_cast<unsigned char>(c)) <= 'f')
                          ? std::tolower(static_cast<unsigned char>(c)) - 'a' + 10
                          : -1;
        if (d < 0) {
            return 0;
        }
        v = v * 16 + static_cast<u64>(d);
    }
    return v;
}

std::string DisplayName(std::string_view folder) {
    std::string s(folder);
    if (std::any_of(s.begin(), s.end(), [](unsigned char c) { return std::islower(c); })) {
        return s;
    }
    // Words of four letters or more: first letter kept, the rest lower case.
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && !std::isalpha(static_cast<unsigned char>(s[i]))) {
            i++;
        }
        std::size_t j = i;
        while (j < s.size() && std::isalpha(static_cast<unsigned char>(s[j]))) {
            j++;
        }
        if (j - i >= 4) {
            for (std::size_t k = i + 1; k < j; k++) {
                s[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[k])));
            }
        }
        i = j;
    }
    return s;
}

Layout Build(const std::vector<File>& files, const Hints& hints) {
    Layout layout;
    struct Tex {
        std::vector<std::string> comps;
        const File* file;
    };
    std::vector<Tex> tex;
    for (const File& f : files) {
        auto comps = Split(f.path);
        if (comps.empty() || !IsTextureFile(comps.back()) ||
            std::any_of(comps.begin(), comps.end(), [](const std::string& c) {
                return c == "__MACOSX" || c.rfind("._", 0) == 0;
            })) {
            continue;
        }
        tex.push_back({std::move(comps), &f});
    }
    // Wrapper folders: no textures of their own, one subfolder.
    std::vector<std::string> root;
    const auto under = [&root](const std::vector<std::string>& c) {
        return c.size() > root.size() && std::equal(root.begin(), root.end(), c.begin());
    };
    for (;;) {
        bool direct = false;
        std::set<std::string> kids;
        for (const Tex& t : tex) {
            if (!under(t.comps)) {
                continue;
            }
            if (t.comps.size() == root.size() + 1) {
                direct = true;
            } else {
                kids.insert(t.comps[root.size()]);
            }
        }
        if (direct || kids.size() != 1) {
            break;
        }
        root.push_back(*kids.begin());
    }
    layout.root = Join(root, root.size());
    const std::size_t d = root.size();

    struct Top {
        bool direct = false;
        std::set<std::string> subs;
    };
    std::map<std::string, Top> tops;
    for (const Tex& t : tex) {
        if (!under(t.comps)) {
            continue;
        }
        if (t.comps.size() == d + 1) {
            layout.loose_textures++;
            layout.loose_bytes += t.file->size;
            continue;
        }
        Top& top = tops[t.comps[d]];
        if (t.comps.size() == d + 2) {
            top.direct = true;
        } else {
            top.subs.insert(t.comps[d + 1]);
        }
    }
    const auto is_folder = [](const Top& t) { return !t.direct && t.subs.size() >= 2; };

    // Counts and sizes per part, and the hashes for the conflicts.
    std::map<std::string, std::pair<u64, u64>> counts;
    std::map<u64, std::set<std::string>> by_hash;
    for (const Tex& t : tex) {
        if (!under(t.comps) || t.comps.size() == d + 1) {
            continue;
        }
        const Top& top = tops[t.comps[d]];
        const std::string key = Join(t.comps, is_folder(top) ? d + 2 : d + 1);
        auto& c = counts[key];
        c.first++;
        c.second += t.file->size;
        if (const u64 h = TextureHash(t.comps.back())) {
            by_hash[h].insert(key);
        }
    }
    const auto make_part = [&](const std::string& key, const std::string& name) {
        Part p;
        p.key = key;
        p.name = name;
        p.textures = counts[key].first;
        p.bytes = counts[key].second;
        return p;
    };

    std::vector<Group> singles;
    for (const auto& [name, top] : tops) {
        const std::string key = Child(layout.root, name);
        if (is_folder(top)) {
            Group g;
            g.kind = Kind::Folder;
            g.key = key;
            g.name = DisplayName(UntagName(name));
            g.base = IsBaseName(g.name);
            for (const std::string& sub : top.subs) {
                g.parts.push_back(make_part(Child(key, sub), DisplayName(sub)));
            }
            layout.groups.push_back(std::move(g));
            continue;
        }
        Group g;
        g.kind = Kind::Single;
        g.key = key;
        g.name = DisplayName(UntagName(name));
        g.parts.push_back(make_part(key, g.name));
        singles.push_back(std::move(g));
    }

    // Tagged folders: "[Fonts] Original N64" and "[Fonts] Original Pizza" are one choice.
    std::map<std::string, std::vector<std::size_t>> tags;
    for (std::size_t i = 0; i < singles.size(); i++) {
        if (const auto t = Tagged(LastPart(singles[i].key))) {
            tags[Lower(t->first)].push_back(i);
        }
    }
    std::vector<bool> taken(singles.size(), false);
    for (const auto& [tag, members] : tags) {
        const auto first = Tagged(LastPart(singles[members[0]].key));
        if (members.size() == 1) {
            Group& g = singles[members[0]];
            g.name = DisplayName(first->first) + ": " + DisplayName(first->second);
            g.parts[0].name = g.name;
            continue;
        }
        Group c;
        c.kind = Kind::Choice;
        c.name = DisplayName(first->first);
        c.key = "choice:" + Lower(c.name);
        for (const std::size_t i : members) {
            Part p = singles[i].parts[0];
            p.name = DisplayName(Tagged(LastPart(singles[i].key))->second);
            c.parts.push_back(std::move(p));
            taken[i] = true;
        }
        layout.groups.push_back(std::move(c));
    }
    // A catalog entry may say the pack's folders are variants.
    if (!hints.choose_one.empty()) {
        Group c;
        c.kind = Kind::Choice;
        c.name = hints.choose_one;
        c.key = "choice:" + Lower(c.name);
        for (std::size_t i = 0; i < singles.size(); i++) {
            if (taken[i] || MatchesAny(hints.keep, LastPart(singles[i].key), singles[i].name)) {
                continue;
            }
            c.parts.push_back(singles[i].parts[0]);
            taken[i] = true;
        }
        if (!c.parts.empty()) {
            layout.groups.push_back(std::move(c));
        }
    }
    for (std::size_t i = 0; i < singles.size(); i++) {
        if (!taken[i]) {
            singles[i].base = IsBaseName(singles[i].name);
            layout.groups.push_back(std::move(singles[i]));
        }
    }
    for (Group& g : layout.groups) {
        if (g.kind == Kind::Choice) {
            StripCommonPrefix(g.parts);
            g.base = IsBaseName(g.name);
        }
        if (g.kind != Kind::Single) {
            std::stable_sort(g.parts.begin(), g.parts.end(), [](const Part& a, const Part& b) {
                return Lower(a.name) < Lower(b.name);
            });
        }
    }
    std::stable_sort(layout.groups.begin(), layout.groups.end(),
                     [](const Group& a, const Group& b) {
                         if (a.base != b.base) {
                             return !a.base;
                         }
                         return Lower(a.name) < Lower(b.name);
                     });

    // Conflicts: parts sharing a hash, except the choices of one group.
    std::map<std::string, std::pair<std::size_t, Part*>> owner;
    for (std::size_t gi = 0; gi < layout.groups.size(); gi++) {
        for (Part& p : layout.groups[gi].parts) {
            owner[p.key] = {gi, &p};
        }
    }
    for (const auto& [hash, keys] : by_hash) {
        if (keys.size() < 2) {
            continue;
        }
        for (const std::string& a : keys) {
            for (const std::string& b : keys) {
                if (a == b || !owner.count(a) || !owner.count(b)) {
                    continue;
                }
                const auto [ga, pa] = owner[a];
                const auto gb = owner[b].first;
                if (ga == gb && layout.groups[ga].kind == Kind::Choice) {
                    continue;
                }
                if (std::find(pa->conflicts.begin(), pa->conflicts.end(), b) ==
                    pa->conflicts.end()) {
                    pa->conflicts.push_back(b);
                }
            }
        }
    }
    for (Group& g : layout.groups) {
        for (Part& p : g.parts) {
            std::sort(p.conflicts.begin(), p.conflicts.end());
        }
    }
    return layout;
}

// ---------------------------------------------------------------------------------------------

std::vector<std::size_t> Order(const Layout& layout, const State& state) {
    std::vector<std::size_t> out;
    std::vector<bool> used(layout.groups.size(), false);
    for (const std::string& key : state.order) {
        for (std::size_t i = 0; i < layout.groups.size(); i++) {
            if (!used[i] && layout.groups[i].key == key) {
                used[i] = true;
                out.push_back(i);
                break;
            }
        }
    }
    for (std::size_t i = 0; i < layout.groups.size(); i++) {
        if (!used[i]) {
            out.push_back(i);
        }
    }
    return out;
}

std::string Chosen(const Layout&, const State& state, const Group& group) {
    if (group.kind != Kind::Choice) {
        return {};
    }
    const auto it = state.choice.find(group.key);
    if (it != state.choice.end()) {
        if (it->second.empty()) {
            return {};
        }
        for (const Part& p : group.parts) {
            if (p.key == it->second) {
                return p.key;
            }
        }
    }
    return DefaultChoice(state.hints, group);
}

bool IsOn(const Layout& layout, const State& state, const Group& group, const Part& part) {
    if (group.kind == Kind::Choice) {
        return Chosen(layout, state, group) == part.key;
    }
    if (state.set) {
        return state.off.count(part.key) == 0;
    }
    return !DefaultOff(state.hints, group, part);
}

int GroupValue(const Layout& layout, const State& state, const Group& group) {
    std::size_t on = 0;
    for (const Part& p : group.parts) {
        on += IsOn(layout, state, group, p) ? 1 : 0;
    }
    return on == 0 ? 0 : on == group.parts.size() ? 2 : 1;
}

std::pair<int, int> CountOn(const Layout& layout, const State& state) {
    int on = 0, all = 0;
    for (const Group& g : layout.groups) {
        if (g.kind == Kind::Choice) {
            all++;
            on += Chosen(layout, state, g).empty() ? 0 : 1;
            continue;
        }
        for (const Part& p : g.parts) {
            all++;
            on += IsOn(layout, state, g, p) ? 1 : 0;
        }
    }
    return {on, all};
}

std::pair<u64, u64> TotalsOn(const Layout& layout, const State& state) {
    u64 n = layout.loose_textures, bytes = layout.loose_bytes;
    for (const Group& g : layout.groups) {
        for (const Part& p : g.parts) {
            if (IsOn(layout, state, g, p)) {
                n += p.textures;
                bytes += p.bytes;
            }
        }
    }
    return {n, bytes};
}

bool GroupConflicts(const Layout& layout, const Group& group) {
    for (const Part& p : group.parts) {
        for (const std::string& other : p.conflicts) {
            const bool own = std::any_of(group.parts.begin(), group.parts.end(),
                                         [&](const Part& q) { return q.key == other; });
            if (!own) {
                return true;
            }
        }
    }
    (void)layout;
    return false;
}

bool AnyConflicts(const Layout& layout) {
    for (const Group& g : layout.groups) {
        for (const Part& p : g.parts) {
            if (!p.conflicts.empty()) {
                return true;
            }
        }
    }
    return false;
}

void SetPart(State& state, const Layout& layout, std::string_view part_key, bool on) {
    Materialize(state, layout);
    for (const Group& g : layout.groups) {
        for (const Part& p : g.parts) {
            if (p.key != part_key) {
                continue;
            }
            if (g.kind == Kind::Choice) {
                state.choice[g.key] = on ? p.key : std::string{};
            } else if (on) {
                state.off.erase(p.key);
            } else {
                state.off.insert(p.key);
            }
            return;
        }
    }
}

void SetGroup(State& state, const Layout& layout, const Group& group, bool on) {
    Materialize(state, layout);
    if (group.kind == Kind::Choice) {
        state.choice[group.key] = on ? DefaultChoice(state.hints, group) : std::string{};
        return;
    }
    for (const Part& p : group.parts) {
        if (on) {
            state.off.erase(p.key);
        } else {
            state.off.insert(p.key);
        }
    }
}

void Choose(State& state, const Layout& layout, const Group& group, std::string_view part_key) {
    Materialize(state, layout);
    state.choice[group.key] = std::string(part_key);
}

void Move(State& state, const Layout& layout, std::string_view group_key, int delta) {
    Materialize(state, layout);
    const auto order = Order(layout, state);
    std::vector<std::string> keys;
    for (const std::size_t i : order) {
        keys.push_back(layout.groups[i].key);
    }
    const auto it = std::find(keys.begin(), keys.end(), group_key);
    if (it == keys.end()) {
        return;
    }
    const std::ptrdiff_t at = it - keys.begin();
    const std::ptrdiff_t to = at + delta;
    if (to < 0 || to >= static_cast<std::ptrdiff_t>(keys.size())) {
        return;
    }
    std::swap(keys[static_cast<std::size_t>(at)], keys[static_cast<std::size_t>(to)]);
    state.order = std::move(keys);
}

void SetAll(State& state, const Layout& layout, bool on) {
    Materialize(state, layout);
    for (const Group& g : layout.groups) {
        if (g.kind != Kind::Choice) {
            SetGroup(state, layout, g, on);
        }
    }
}

void SetOrder(State& state, const Layout& layout, const std::vector<std::string>& keys) {
    Materialize(state, layout);
    state.order = keys;
}

std::vector<std::size_t> LoadOrder(const Layout& layout, const State& state,
                                   const std::vector<File>& files) {
    const Ranks ranks = MakeRanks(layout, state);
    std::vector<std::pair<std::size_t, std::size_t>> keep; // rank, index
    keep.reserve(files.size());
    for (std::size_t i = 0; i < files.size(); i++) {
        const auto r = ranks.Of(files[i].path);
        if (r) {
            keep.emplace_back(*r, i);
        }
    }
    std::stable_sort(keep.begin(), keep.end(), [&files](const auto& a, const auto& b) {
        if (a.first != b.first) {
            return a.first < b.first;
        }
        return files[a.second].path < files[b.second].path;
    });
    std::vector<std::size_t> out;
    out.reserve(keep.size());
    for (const auto& k : keep) {
        out.push_back(k.second);
    }
    return out;
}

// ---------------------------------------------------------------------------------------------

State ParseState(std::string_view text) {
    State s;
    const nlohmann::json j = nlohmann::json::parse(text, nullptr, false, true);
    if (!j.is_object()) {
        return s;
    }
    const auto strings = [](const nlohmann::json& a) {
        std::vector<std::string> out;
        if (a.is_array()) {
            for (const auto& v : a) {
                if (v.is_string()) {
                    out.push_back(v.get<std::string>());
                }
            }
        }
        return out;
    };
    if (const auto h = j.find("hints"); h != j.end() && h->is_object()) {
        if (const auto c = h->find("choose_one"); c != h->end() && c->is_string()) {
            s.hints.choose_one = c->get<std::string>();
        }
        if (h->contains("keep")) {
            s.hints.keep = strings((*h)["keep"]);
        }
        if (h->contains("off")) {
            s.hints.off = strings((*h)["off"]);
        }
    }
    if (const auto set = j.find("set"); set != j.end() && set->is_boolean()) {
        s.set = set->get<bool>();
    }
    if (!s.set) {
        return s;
    }
    if (j.contains("off")) {
        for (std::string& k : strings(j["off"])) {
            s.off.insert(std::move(k));
        }
    }
    if (const auto c = j.find("choice"); c != j.end() && c->is_object()) {
        for (const auto& [k, v] : c->items()) {
            if (v.is_string()) {
                s.choice[k] = v.get<std::string>();
            }
        }
    }
    if (j.contains("order")) {
        s.order = strings(j["order"]);
    }
    return s;
}

std::string StateJson(const State& s) {
    nlohmann::ordered_json j;
    j["version"] = 1;
    if (!s.hints.Empty()) {
        nlohmann::ordered_json h;
        if (!s.hints.choose_one.empty()) {
            h["choose_one"] = s.hints.choose_one;
        }
        if (!s.hints.keep.empty()) {
            h["keep"] = s.hints.keep;
        }
        if (!s.hints.off.empty()) {
            h["off"] = s.hints.off;
        }
        j["hints"] = h;
    }
    j["set"] = s.set;
    if (s.set) {
        j["off"] = std::vector<std::string>(s.off.begin(), s.off.end());
        j["choice"] = s.choice;
        j["order"] = s.order;
    }
    return j.dump(2) + "\n";
}

// ---------------------------------------------------------------------------------------------

std::string CountText(u64 n, std::string_view one, std::string_view many) {
    const std::string digits = std::to_string(n);
    std::string out;
    for (std::size_t i = 0; i < digits.size(); i++) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            out += ',';
        }
        out += digits[i];
    }
    out += ' ';
    out += n == 1 ? one : many;
    return out;
}

std::string SizeText(u64 bytes) {
    static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes);
    int unit = 0;
    while (v >= 1024.0 && unit < 4) { // AzaharFrame texrow: 1024s, as the VR menu's sizes
        v /= 1024.0;
        unit++;
    }
    char buf[32];
    if (unit == 0) {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    } else if (v < 10.0) {
        std::snprintf(buf, sizeof(buf), "%.1f %s", v, kUnits[unit]);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f %s", v, kUnits[unit]);
    }
    return buf;
}

namespace {

std::string Joined(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (const std::string& p : parts) {
        if (p.empty()) {
            continue;
        }
        if (!out.empty()) {
            out += sep;
        }
        out += p;
    }
    return out;
}

void AddConflictNames(const Layout& layout, const std::vector<std::string>& keys, const Group* skip,
                      std::vector<std::string>& names) {
    for (const std::string& key : keys) {
        if (skip && std::any_of(skip->parts.begin(), skip->parts.end(),
                                [&](const Part& q) { return q.key == key; })) {
            continue;
        }
        const Part* other = layout.FindPart(key);
        const std::string name = other ? other->name : LastPart(key);
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    }
}

std::string Detail(u64 textures, u64 bytes, const std::vector<std::string>& conflicts) {
    return Joined({CountText(textures, "Texture", "Textures"), SizeText(bytes),
                   conflicts.empty() ? std::string{} : "Conflicts with " + Joined(conflicts, ", ")},
                  " · ");
}

} // namespace

std::string ConflictNames(const Layout& layout, const Part& part) {
    std::vector<std::string> names;
    AddConflictNames(layout, part.conflicts, nullptr, names);
    return Joined(names, ", ");
}

std::string PartDetail(const Layout& layout, const Part& part) {
    std::vector<std::string> names;
    AddConflictNames(layout, part.conflicts, nullptr, names);
    return Detail(part.textures, part.bytes, names);
}

std::string GroupDetail(const Layout& layout, const Group& group) {
    u64 n = 0, bytes = 0;
    std::vector<std::string> names;
    for (const Part& p : group.parts) {
        n += p.textures;
        bytes += p.bytes;
        AddConflictNames(layout, p.conflicts, &group, names);
    }
    return Detail(n, bytes, names);
}

std::string PartsOnText(const Layout& layout, const State& state) {
    const auto [on, all] = CountOn(layout, state);
    if (layout.PartCount() < 2 || on == all) {
        return {};
    }
    return std::to_string(on) + " of " + CountText(static_cast<u64>(all), "Part", "Parts");
}

bool CanMove(const Layout& layout, const Group& group) {
    return GroupConflicts(layout, group);
}

std::vector<File> ListFiles(const std::string& pack, const std::atomic_bool* cancel) {
    std::vector<File> out;
    std::error_code ec;
    const fs::path base = fs::path(pack);
    fs::recursive_directory_iterator it(base,
                                        fs::directory_options::follow_directory_symlink |
                                            fs::directory_options::skip_permission_denied,
                                        ec);
    for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (cancel && cancel->load()) {
            return {};
        }
        if (it.depth() > 64) {
            it.disable_recursion_pending();
            continue;
        }
        std::error_code e2;
        if (!it->is_regular_file(e2)) {
            continue;
        }
        File f;
        f.path = it->path().lexically_relative(base).generic_string();
        f.size = it->file_size(e2);
        out.push_back(std::move(f));
    }
    return out;
}

State ReadState(const std::string& pack) {
    std::ifstream in(fs::path(pack) / kStateFile, std::ios::binary);
    if (!in) {
        return {};
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ParseState(ss.str());
}

bool WriteState(const std::string& pack, const State& state) {
    // AzaharFrame harden: fsynced temp file, rename, fsynced folder
    return Common::WriteFileAtomic((fs::path(pack) / kStateFile).string(), StateJson(state));
}

bool WriteHints(const std::string& pack, const Hints& hints) {
    std::error_code ec;
    if (hints.Empty() || fs::exists(fs::path(pack) / kStateFile, ec)) {
        return true;
    }
    State s;
    s.hints = hints;
    return WriteState(pack, s);
}

Layout Scan(const std::string& pack, const Hints& hints, const std::atomic_bool* cancel) {
    const std::vector<File> files = ListFiles(pack, cancel);
    Layout layout = Build(files, hints);
    PickPreviews(layout, pack, files);
    // A converted pack has KTX files only: the previews come from the pack it was made from.
    const bool missing =
        std::any_of(layout.groups.begin(), layout.groups.end(), [](const Group& g) {
            return std::any_of(g.parts.begin(), g.parts.end(),
                               [](const Part& p) { return p.preview.empty(); });
        });
    if (missing && !(cancel && cancel->load())) {
        std::ifstream in(fs::path(pack) / kKtxMarker, std::ios::binary);
        if (in) {
            std::stringstream ss;
            ss << in.rdbuf();
            const nlohmann::json j = nlohmann::json::parse(ss.str(), nullptr, false);
            if (j.is_object() && j.contains("source") && j["source"].is_string()) {
                const fs::path src =
                    (fs::path(pack) / fs::path(j["source"].get<std::string>())).lexically_normal();
                std::error_code ec;
                if (fs::is_directory(src, ec)) {
                    PickPreviews(layout, src.string(), ListFiles(src.string(), cancel));
                }
            }
        }
    }
    return layout;
}

} // namespace CustomTex::Parts
