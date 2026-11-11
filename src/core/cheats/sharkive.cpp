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
