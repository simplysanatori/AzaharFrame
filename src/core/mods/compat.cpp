// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame compat: see compat.h.

#include "core/mods/compat.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace Mods::Compat {

namespace {

// The regions of the catalogs' games, from the catalog's own notes (which list every region a mod
// serves). Titles sold everywhere under one ID (Pokemon X and Y) and games whose IDs the notes do
// not place are left out: they stay Unknown.
constexpr std::pair<u64, Region> kRegions[] = {
    // The Legend of Zelda: Ocarina of Time 3D
    {0x0004000000033400, Region::Japan},
    {0x0004000000033500, Region::Usa},
    {0x0004000000033600, Region::Europe},
    {0x000400000008F800, Region::Korea},
    {0x000400000008F900, Region::Taiwan},
    // The Legend of Zelda: Majora's Mask 3D
    {0x00040000000D6E00, Region::Japan},
    {0x0004000000125500, Region::Usa},
    {0x0004000000125600, Region::Europe},
    // The Legend of Zelda: A Link Between Worlds
    {0x00040000000EC300, Region::Usa},
    // Kid Icarus: Uprising
    {0x0004000000030000, Region::Japan},
    {0x0004000000030100, Region::Usa},
    {0x0004000000030200, Region::Europe},
    // Mario Kart 7
    {0x0004000000030600, Region::Japan},
    {0x0004000000030700, Region::Europe},
    {0x0004000000030800, Region::Usa},
    // Super Mario 3D Land
    {0x0004000000053F00, Region::Europe},
    {0x0004000000054000, Region::Usa},
    // Stella Glow
    {0x0004000000173700, Region::Usa},
    // Fire Emblem Fates: Special Edition, Birthright, Conquest
    {0x000400000012DC00, Region::Japan},
    {0x000400000012DD00, Region::Japan},
    {0x000400000012DE00, Region::Japan},
    {0x0004000000179400, Region::Usa},
    {0x0004000000179500, Region::Europe},
    {0x0004000000179600, Region::Usa},
    {0x0004000000179700, Region::Europe},
    {0x0004000000179800, Region::Usa},
    {0x000400000017A800, Region::Europe},
    // Fire Emblem Echoes: Shadows of Valentia
    {0x00040000001A2B00, Region::Japan},
    {0x00040000001B4000, Region::Usa},
    {0x00040000001B4100, Region::Europe},
    {0x00040000001C6800, Region::Korea},
    // Fire Emblem Awakening
    {0x0004000000072000, Region::Japan},
    {0x000400000009F100, Region::Europe},
    {0x00040000000A0500, Region::Usa},
};

bool Has(const std::vector<u64>& ids, u64 id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

/// "A", "A and B", "A, B and C".
std::string JoinAnd(const std::vector<std::string>& parts) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); i++) {
        if (i > 0) {
            out += i + 1 == parts.size() ? " and " : ", ";
        }
        out += parts[i];
    }
    return out;
}

constexpr const char* kMayNotWork = "It may not work.";
constexpr const char* kMayNotStart = "It may not work, or the game may not start.";

} // namespace

Region RegionOfTitle(u64 title_id) {
    for (const auto& [id, region] : kRegions) {
        if (id == title_id) {
            return region;
        }
    }
    return Region::Unknown;
}

std::string RegionName(Region region) {
    switch (region) {
    case Region::Usa:
        return "USA";
    case Region::Europe:
        return "Europe";
    case Region::Japan:
        return "Japan";
    case Region::Korea:
        return "Korea";
    case Region::Taiwan:
        return "Taiwan";
    case Region::China:
        return "China";
    case Region::Unknown:
        break;
    }
    return {};
}

std::string RegionAdjective(Region region) {
    switch (region) {
    case Region::Usa:
        return "USA";
    case Region::Europe:
        return "European";
    case Region::Japan:
        return "Japanese";
    case Region::Korea:
        return "Korean";
    case Region::Taiwan:
        return "Taiwanese";
    case Region::China:
        return "Chinese";
    case Region::Unknown:
        break;
    }
    return {};
}

std::optional<Version> ParseVersion(std::string_view label) {
    if (!label.empty() && (label.front() == 'v' || label.front() == 'V')) {
        label.remove_prefix(1);
    }
    const std::size_t dot = label.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= label.size() || dot > 2 ||
        label.size() - dot - 1 > 2) {
        return std::nullopt;
    }
    Version v;
    for (std::size_t i = 0; i < label.size(); i++) {
        if (i == dot) {
            continue;
        }
        const char c = label[i];
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        int& part = i < dot ? v.major : v.minor;
        part = part * 10 + (c - '0');
    }
    return v;
}

std::string VersionText(const Version& v) {
    return std::to_string(v.major) + "." + std::to_string(v.minor);
}

int CompareVersions(const Version& a, const Version& b) {
    if (a.major != b.major) {
        return a.major < b.major ? -1 : 1;
    }
    return a.minor == b.minor ? 0 : a.minor < b.minor ? -1 : 1;
}

std::optional<Version> GameVersion(std::optional<Version> update_tmd, std::optional<Version> base_tmd,
                                   std::optional<int> cartridge_revision, std::string* source) {
    const auto say = [source](const char* s) {
        if (source) {
            *source = s;
        }
    };
    if (update_tmd) {
        say("update");
        return update_tmd;
    }
    if (base_tmd) {
        say("installed title");
        return base_tmd;
    }
    if (cartridge_revision && *cartridge_revision > 0) {
        say(""); // a later cartridge may have an update built in: nothing to compare with
        return std::nullopt;
    }
    say("assumed");
    return Version{1, 0};
}

std::string NormalizeTitle(std::string_view title) {
    std::string t(title);
    // A trailing "(USA)", "(Europe Special Edition)" note goes.
    while (!t.empty() && t.back() == ' ') {
        t.pop_back();
    }
    if (!t.empty() && t.back() == ')') {
        const std::size_t open = t.rfind('(');
        if (open != std::string::npos && open > 0) {
            t.erase(open);
        }
    }
    std::string out;
    for (const char c : t) {
        if (c == ' ' && (out.empty() || out.back() == ' ')) {
            continue;
        }
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

u64 Families::Root(u64 id) const {
    auto it = parent_.find(id);
    while (it != parent_.end() && it->second != id) {
        id = it->second;
        it = parent_.find(id);
    }
    return id;
}

void Families::Join(u64 a, u64 b) {
    parent_.emplace(a, a);
    parent_.emplace(b, b);
    const u64 ra = Root(a), rb = Root(b);
    if (ra != rb) {
        parent_[std::max(ra, rb)] = std::min(ra, rb);
    }
}

void Families::Add(std::string_view title, const std::vector<u64>& title_ids) {
    if (title_ids.empty()) {
        return;
    }
    for (const u64 id : title_ids) {
        Join(title_ids.front(), id);
    }
    const std::string key = NormalizeTitle(title);
    if (key.empty()) {
        return;
    }
    const auto [it, fresh] = by_title_.emplace(key, title_ids.front());
    if (!fresh) {
        Join(it->second, title_ids.front());
    }
}

bool Families::SameGame(u64 a, u64 b) const {
    if (a == b) {
        return true;
    }
    if (!parent_.count(a) || !parent_.count(b)) {
        return false;
    }
    return Root(a) == Root(b);
}

std::string Verdict::RegionChip() const {
    if (kind != Mismatch::Region || regions.empty()) {
        return {};
    }
    if (regions.size() == 1) {
        return RegionName(regions.front()) + " Only";
    }
    if (regions.size() > 3) {
        return "Other Regions";
    }
    std::vector<std::string> names;
    for (const Region r : regions) {
        names.push_back(RegionName(r));
    }
    return JoinAnd(names);
}

namespace {

/// "the USA version", "the USA and Japanese versions".
std::string VersionsOf(const std::vector<Region>& regions) {
    std::vector<std::string> names;
    for (const Region r : regions) {
        names.push_back(RegionAdjective(r));
    }
    return "the " + JoinAnd(names) + (regions.size() > 1 ? " versions" : " version");
}

} // namespace

std::string Verdict::Reason() const {
    switch (kind) {
    case Mismatch::Build:
        return "Made for another build of the game. Yours matches " + your_build + ". " + kMayNotWork;
    case Mismatch::Version:
        return "Made for game version " + VersionText(*made_for) + ". Yours is " +
               VersionText(*your_version) + ". " + kMayNotWork;
    case Mismatch::Region:
        return "Made for " + VersionsOf(regions) + "." +
               (yours != Region::Unknown ? " Yours is the " + RegionAdjective(yours) + " version."
                                         : std::string{}) +
               " " + kMayNotWork;
    case Mismatch::Release:
        return std::string("Made for another release of this game. ") + kMayNotWork;
    case Mismatch::None:
        break;
    }
    return {};
}

std::string Verdict::Body(std::string_view what) const {
    const std::string start = "This " + std::string(what) + " was made for ";
    switch (kind) {
    case Mismatch::Build:
        return start + "another build of the game, and yours matches " + your_build + ". " +
               kMayNotStart;
    case Mismatch::Version:
        return start + "game version " + VersionText(*made_for) + ", and yours is version " +
               VersionText(*your_version) + ". " + kMayNotStart;
    case Mismatch::Region:
        return start + VersionsOf(regions) + " of the game" +
               (yours != Region::Unknown ? ", and yours is the " + RegionAdjective(yours) + " version"
                                         : std::string{}) +
               ". " + kMayNotStart;
    case Mismatch::Release:
        return start + "another release of the game. " + kMayNotStart;
    case Mismatch::None:
        break;
    }
    return {};
}

Verdict Judge(const EntryFacts& entry, const Game& game, const Families& families,
              const std::string& matched_build) {
    Verdict v;
    const bool serves = Has(entry.title_ids, game.title_id);
    bool same = serves;
    for (const u64 id : entry.title_ids) {
        same = same || families.SameGame(id, game.title_id);
    }
    v.shown = same;
    if (!same) {
        return v;
    }
    v.yours = game.region;
    if (serves) {
        if (!entry.code_crc32.empty()) {
            // Project Restoration style: the code decides, and only when another build matches.
            const bool mine = game.code_crc &&
                              std::find(entry.code_crc32.begin(), entry.code_crc32.end(),
                                        *game.code_crc) != entry.code_crc32.end();
            if (!mine && game.code_crc && !matched_build.empty()) {
                v.kind = Mismatch::Build;
                v.your_build = matched_build;
            }
            return v;
        }
        const auto made_for = ParseVersion(entry.chip);
        if (made_for && game.version && CompareVersions(*made_for, *game.version) != 0) {
            v.kind = Mismatch::Version;
            v.made_for = made_for;
            v.your_version = game.version;
        }
        return v;
    }
    static constexpr std::array<Region, 6> kOrder{Region::Usa,    Region::Europe, Region::Japan,
                                                  Region::Korea, Region::Taiwan, Region::China};
    for (const Region r : kOrder) {
        for (const u64 id : entry.title_ids) {
            if (RegionOfTitle(id) == r) {
                v.regions.push_back(r);
                break;
            }
        }
    }
    const bool covers_yours =
        game.region != Region::Unknown &&
        std::find(v.regions.begin(), v.regions.end(), game.region) != v.regions.end();
    v.kind = !v.regions.empty() && !covers_yours ? Mismatch::Region : Mismatch::Release;
    if (v.kind == Mismatch::Release) {
        v.regions.clear();
    }
    return v;
}

} // namespace Mods::Compat
