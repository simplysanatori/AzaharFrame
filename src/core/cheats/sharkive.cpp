// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cctype>
#include <set>
#include <fmt/format.h>
#include "core/cheats/sharkive.h"

namespace Cheats::Sharkive {

namespace {

std::string_view Trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

bool IsHex8(std::string_view s) {
    return s.size() == 8 &&
           std::all_of(s.begin(), s.end(), [](char c) {
               return std::isxdigit(static_cast<unsigned char>(c)) != 0;
           });
}

bool IsCodeLine(std::string_view line) {
    return line.size() == 17 && line[8] == ' ' && IsHex8(line.substr(0, 8)) &&
           IsHex8(line.substr(9));
}

} // namespace

std::string UrlPath(u64 title_id) {
    return fmt::format("/FlagBrew/Sharkive/master/3ds/{:016X}.txt", title_id);
}

std::vector<Entry> Parse(std::string_view text) {
    std::vector<Entry> result;
    std::set<std::string> seen;

    Entry current;
    bool open = false;
    bool bad = false;

    const auto finish = [&] {
        if (open && !bad && !current.code.empty() && !current.name.empty() &&
            seen.insert(current.name).second) {
            result.push_back(std::move(current));
        }
        current = {};
        open = false;
        bad = false;
    };

    while (!text.empty()) {
        const auto eol = text.find('\n');
        std::string_view line = text.substr(0, eol);
        text = eol == std::string_view::npos ? std::string_view{} : text.substr(eol + 1);
        line = Trim(line);
        if (line.empty() || line == "---") {
            continue;
        }
        if (line.front() == '[' && line.back() == ']' && line.size() >= 3) {
            finish();
            current.name = std::string(Trim(line.substr(1, line.size() - 2)));
            open = true;
        } else if (!open) {
            continue;
        } else if (line.front() == '*') {
            if (line != "*citra_enabled") {
                current.comments += std::string(line.substr(1)) + '\n';
            }
        } else if (IsCodeLine(line)) {
            if (!current.code.empty()) {
                current.code += '\n';
            }
            current.code += std::string(line);
        } else {
            bad = true;
        }
    }
    finish();
    return result;
}

Version FromTitleVersion(u16 v) {
    return Version{v >> 10, (v >> 4) & 0x3F};
}

namespace {

// AzaharFrame echoes: the versions games show for their update TMDs, from each game's own screen
// or its publisher's release list (the TMD alone does not tell; see GameVersionOf). Keyed by the
// low half of the title ID (the game's and its update's are the same).
struct KnownRelease {
    u32 title_low;
    u16 tmd;
    Version shown;
};
constexpr KnownRelease kKnownReleases[] = {
    // Fire Emblem Echoes: Shadows of Valentia: one update, "ver 1.1" on the title screen (Europe,
    // seen in a mock run); the USA and Japan updates are the same release (the 1.1 mods' pages).
    {0x001B4100, 1024, {1, 1}}, // Europe
    {0x001B4000, 1024, {1, 1}}, // USA
    {0x001A2B00, 1024, {1, 1}}, // Japan
    // Mario Kart 7: Nintendo's updates are Ver. 1.1 (May 2012) and Ver. 1.2 (December 2022, the
    // last); the 1.2 update shows "Ver. 1.2" by the Online Multiplayer icon. The USA update
    // installed here is TMD 3072.
    {0x00030800, 3072, {1, 2}}, // USA
};

} // namespace

std::optional<Version> GameVersionOf(u64 title_id, u16 tmd) {
    const u32 low = static_cast<u32>(title_id & 0xFFFFFFFFULL);
    for (const auto& r : kKnownReleases) {
        if (r.title_low == low && r.tmd == tmd) {
            return r.shown;
        }
    }
    if ((tmd >> 10) == 0) {
        return Version{1, 0}; // the game as released
    }
    return std::nullopt;
}

std::string ToString(const Version& v) {
    return fmt::format("v{}.{}", v.major, v.minor);
}

InstalledVersion SelectInstalledVersion(u64 title_id, std::optional<u16> update_tmd,
                                        std::optional<u16> base_tmd, std::optional<u16> ncch_remaster) {
    const auto from_tmd = [title_id](u16 tmd, VersionSource source) {
        return InstalledVersion{GameVersionOf(title_id, tmd), std::nullopt, source, tmd, tmd >> 10};
    };
    if (update_tmd) {
        return from_tmd(*update_tmd, VersionSource::UpdateTmd);
    }
    if (base_tmd) {
        return from_tmd(*base_tmd, VersionSource::BaseTmd);
    }
    if (ncch_remaster && *ncch_remaster != 0 && *ncch_remaster % 2 == 0) {
        return {std::nullopt, static_cast<int>(*ncch_remaster / 2), VersionSource::Ncch};
    }
    return {};
}

namespace {

bool IsWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

bool IsDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

// Reads "d{1,2}.d{1,2}" at s[i]; not followed by another digit or '.'+digit.
std::optional<Version> ReadMajorMinor(std::string_view s, std::size_t i) {
    std::size_t j = i;
    int major = 0, minor = 0, n = 0;
    while (j < s.size() && IsDigit(s[j]) && n < 3) {
        major = major * 10 + (s[j] - '0');
        ++j, ++n;
    }
    if (n == 0 || n > 2 || j >= s.size() || s[j] != '.') {
        return std::nullopt;
    }
    ++j;
    n = 0;
    while (j < s.size() && IsDigit(s[j]) && n < 3) {
        minor = minor * 10 + (s[j] - '0');
        ++j, ++n;
    }
    if (n == 0 || n > 2 || (j < s.size() && (IsDigit(s[j]) || IsWordChar(s[j])))) {
        return std::nullopt;
    }
    if (j + 1 < s.size() && s[j] == '.' && IsDigit(s[j + 1])) {
        return std::nullopt;
    }
    return Version{major, minor};
}

std::optional<Version> TagIn(std::string_view s) {
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (std::size_t i = 0; i < lower.size(); ++i) {
        if (lower[i] != 'v' || (i > 0 && IsWordChar(lower[i - 1]))) {
            continue;
        }
        std::size_t j = i + 1;
        while (j < lower.size() && lower[j] == ' ') {
            ++j;
        }
        if (auto v = ReadMajorMinor(lower, j)) {
            return v;
        }
    }
    if (lower.find("only") != std::string::npos) {
        for (std::size_t i = 0; i < lower.size(); ++i) {
            if (IsDigit(lower[i]) && (i == 0 || (!IsWordChar(lower[i - 1]) && lower[i - 1] != '.'))) {
                if (auto v = ReadMajorMinor(lower, i)) {
                    return v;
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace

namespace {
std::optional<int> RevisionIn(std::string_view s) {
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (std::size_t i = 0; i + 3 < lower.size(); ++i) {
        if (lower.compare(i, 3, "rev") != 0 || (i > 0 && IsWordChar(lower[i - 1]))) {
            continue;
        }
        std::size_t j = i + 3;
        while (j < lower.size() && lower[j] == ' ') {
            ++j;
        }
        int n = 0, digits = 0;
        while (j < lower.size() && IsDigit(lower[j]) && digits < 3) {
            n = n * 10 + (lower[j] - '0');
            ++j, ++digits;
        }
        if (digits > 0 && digits <= 2 && (j >= lower.size() || !IsWordChar(lower[j]))) {
            return n;
        }
    }
    return std::nullopt;
}
} // namespace

std::optional<int> TagRevision(std::string_view name, std::string_view comments) {
    if (auto r = RevisionIn(name)) {
        return r;
    }
    return RevisionIn(comments);
}

int MarkOtherRevisions(std::vector<Entry>& entries, int installed_revision) {
    int count = 0;
    for (auto& entry : entries) {
        const auto tag = TagRevision(entry.name, entry.comments);
        if (!tag || *tag == installed_revision) {
            continue;
        }
        entry.comments +=
            fmt::format("Version note: for Rev{}, this cartridge is Rev{}\n", *tag, installed_revision);
        ++count;
    }
    return count;
}

std::optional<Version> TagVersion(std::string_view name, std::string_view comments) {
    if (auto v = TagIn(name)) {
        return v;
    }
    return TagIn(comments);
}

bool VersionMatches(const std::optional<Version>& tag, const std::optional<Version>& installed) {
    return !tag || !installed || *tag == *installed;
}

bool VersionMatches(const std::optional<Version>& tag, const InstalledVersion& installed) {
    if (installed.version || installed.updates == 0) {
        return VersionMatches(tag, installed.version);
    }
    return !tag || !(*tag == Version{1, 0});
}

int MarkOtherVersions(std::vector<Entry>& entries, const InstalledVersion& installed) {
    int count = 0;
    for (auto& entry : entries) {
        const auto tag = TagVersion(entry.name, entry.comments);
        if (VersionMatches(tag, installed)) {
            continue;
        }
        entry.comments += installed.version
                              ? fmt::format("Version note: for {}, installed {}\n", ToString(*tag),
                                            ToString(*installed.version))
                              : fmt::format("Version note: for {}, this game has an update\n",
                                            ToString(*tag));
        ++count;
    }
    return count;
}

int MarkOtherVersions(std::vector<Entry>& entries, const std::optional<Version>& installed) {
    InstalledVersion i;
    i.version = installed;
    return MarkOtherVersions(entries, i);
}

std::vector<Entry> NewEntries(const std::vector<Entry>& downloaded,
                              const std::vector<std::string>& existing_names) {
    std::set<std::string> have;
    for (const auto& name : existing_names) {
        have.insert(std::string(Trim(name)));
    }
    std::vector<Entry> result;
    for (const auto& entry : downloaded) {
        if (!have.count(std::string(Trim(entry.name)))) {
            result.push_back(entry);
        }
    }
    return result;
}

} // namespace Cheats::Sharkive
