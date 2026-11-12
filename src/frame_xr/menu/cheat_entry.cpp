// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ui2: cheats typed in or read from a file (cheat_entry.h).

#include "frame_xr/menu/cheat_entry.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace FrameXr::Menu {

namespace {

bool IsHex(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

std::string Upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string Trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) {
        return {};
    }
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> out;
    std::string line;
    for (const char c : text) {
        if (c == '\n') {
            out.push_back(line);
            line.clear();
        } else if (c != '\r' && c != '\0') {
            line += c;
        }
    }
    out.push_back(line);
    return out;
}

/// The words of one line ("" words dropped), or false if a character is neither hex nor space.
bool Words(const std::string& line, std::vector<std::string>& words) {
    std::string w;
    for (const char c : line) {
        if (c == ' ' || c == '\t') {
            if (!w.empty()) {
                words.push_back(w);
                w.clear();
            }
        } else if (IsHex(c)) {
            w += c;
        } else {
            return false;
        }
    }
    if (!w.empty()) {
        words.push_back(w);
    }
    return true;
}

/// One line's pairs into `out` ("XXXXXXXX YYYYYYYY" each); false if it is not a code line.
bool LinePairs(const std::string& line, std::vector<std::string>& out) {
    std::vector<std::string> words;
    if (!Words(line, words)) {
        return false;
    }
    std::vector<std::string> halves;
    for (const std::string& w : words) {
        if (w.size() == 8) {
            halves.push_back(w);
        } else if (w.size() == 16) {
            halves.push_back(w.substr(0, 8));
            halves.push_back(w.substr(8));
        } else {
            return false;
        }
    }
    if (halves.empty() || halves.size() % 2 != 0) {
        return false;
    }
    for (std::size_t i = 0; i < halves.size(); i += 2) {
        out.push_back(Upper(halves[i]) + " " + Upper(halves[i + 1]));
    }
    return true;
}

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string BaseName(const std::string& path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool IsTitleIdTxt(const std::string& base) {
    if (base.size() != 20 || Lower(base.substr(16)) != ".txt") {
        return false;
    }
    return std::all_of(base.begin(), base.begin() + 16, IsHex);
}

} // namespace

std::string CheckCheatName(const std::string& name) {
    const std::string n = Trim(name);
    if (n.empty()) {
        return "Enter a Name";
    }
    if (n.find_first_of("[]\n") != std::string::npos) {
        return "No [ ] or Line Breaks";
    }
    return {};
}

std::string NormalizeCheatCode(const std::string& text, std::string& code) {
    std::vector<std::string> pairs;
    int n = 0;
    for (const std::string& raw : Lines(text)) {
        n++;
        const std::string line = Trim(raw);
        if (line.empty()) {
            continue;
        }
        if (!LinePairs(line, pairs)) {
            return "Line " + std::to_string(n) + ": Not a Code";
        }
    }
    if (pairs.empty()) {
        return "Enter a Code";
    }
    std::string out;
    for (const std::string& p : pairs) {
        out += p + "\n";
    }
    out.pop_back();
    code = out;
    return {};
}

CheatFileText ParseCheatFile(const std::string& text, const std::string& fallback_name) {
    CheatFileText out;
    CheatText cur;
    std::vector<std::string> pairs;
    bool started = false; ///< a "[Name]" or a code line came
    bool bad = false;
    const auto finish = [&] {
        if (!started) {
            return;
        }
        if (!bad && !pairs.empty()) {
            std::string code;
            for (const std::string& p : pairs) {
                code += p + "\n";
            }
            code.pop_back();
            cur.code = code;
            if (cur.name.empty()) {
                cur.name = fallback_name;
            }
            out.cheats.push_back(cur);
        } else if (bad || !cur.name.empty()) {
            out.skipped++; // a line that is not a code, or a name with no code
        }
        cur = {};
        pairs.clear();
        bad = false;
        started = false;
    };
    for (const std::string& raw : Lines(text)) {
        std::string line = Trim(raw);
        if (line.empty()) {
            continue;
        }
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            finish();
            cur.name = Trim(line.substr(1, line.size() - 2));
            started = true;
        } else if (line.front() == '*') {
            if (line == "*citra_enabled") {
                cur.enabled = true;
            } else {
                cur.comments += line.substr(1) + "\n";
            }
        } else {
            started = true;
            if (!LinePairs(line, pairs)) {
                bad = true;
            }
        }
    }
    finish();
    return out;
}

std::string PickCheatFile(const std::vector<std::string>& files, uint64_t title_id) {
    char want[24];
    std::snprintf(want, sizeof(want), "%016llx.txt", static_cast<unsigned long long>(title_id));
    std::vector<std::string> txt;
    for (const std::string& f : files) {
        const std::string base = BaseName(f);
        if (base.empty() || base[0] == '.' || f.find("__MACOSX") != std::string::npos) {
            continue;
        }
        if (Lower(base) == want) {
            return f;
        }
        if (base.size() > 4 && Lower(base.substr(base.size() - 4)) == ".txt") {
            txt.push_back(f);
        }
    }
    if (txt.size() == 1) {
        return txt.front();
    }
    for (const std::string& f : txt) {
        if (IsTitleIdTxt(BaseName(f))) {
            return f;
        }
    }
    return {};
}

bool IsTitleIdName(const std::string& stem) {
    return stem.size() == 16 && std::all_of(stem.begin(), stem.end(), IsHex);
}

std::string CheatsAddedText(int added) {
    if (added <= 0) {
        return "No New Cheats";
    }
    return added == 1 ? "1 Cheat Added" : std::to_string(added) + " Cheats Added";
}

} // namespace FrameXr::Menu
