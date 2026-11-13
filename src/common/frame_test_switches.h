// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame dbl: test switches without launch options. At start (main) the app reads
// $XDG_CONFIG_HOME/AzaharFrame/test-switches.txt (~/.config/AzaharFrame/test-switches.txt), one
// NAME=value a line ('#' starts a comment), and sets each AZAHARFRAME_ name it names as if it
// were in the environment; one already in the environment wins (env vars keep working and take
// priority). Every AZAHARFRAME_ switch in effect is logged once the XR session starts: `xr: test
// switch NAME=value (from the file|from the environment)`.

#pragma once

#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

extern char** environ;

namespace Common::FrameTestSwitches {

inline std::vector<std::string>& FromFile() {
    static std::vector<std::string> names;
    return names;
}

inline std::string Path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    const std::string base = xdg && *xdg ? std::string(xdg)
                             : home && *home ? std::string(home) + "/.config"
                                             : std::string();
    return base.empty() ? std::string() : base + "/AzaharFrame/test-switches.txt";
}

inline std::string Trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) {
        return {};
    }
    const auto b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

/// Call first thing in main, before any thread starts (setenv is not thread-safe).
inline void Load() {
    const std::string path = Path();
    if (path.empty()) {
        return;
    }
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line.resize(hash);
        }
        line = Trim(line);
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string name = Trim(line.substr(0, eq));
        const std::string value = Trim(line.substr(eq + 1));
        if (name.rfind("AZAHARFRAME_", 0) != 0 || name.find_first_of(" \t") != std::string::npos) {
            continue; // only the app's own switches
        }
        if (std::getenv(name.c_str()) != nullptr) {
            continue; // the environment wins
        }
        if (setenv(name.c_str(), value.c_str(), 0) == 0) {
            FromFile().push_back(name);
        }
    }
}

/// The test switches in effect (AZAHARFRAME_IMMERSIVE_, _XR_, _TEST_ and any from the file), "NAME=value (from the file|from the environment)".
inline std::vector<std::string> InEffect() {
    std::vector<std::string> out;
    for (char** e = environ; e && *e; ++e) {
        const std::string kv = *e;
        if (kv.rfind("AZAHARFRAME_", 0) != 0) {
            continue;
        }
        const std::string name = kv.substr(0, kv.find('='));
        bool file = false;
        for (const std::string& n : FromFile()) {
            file = file || n == name;
        }
        // The test switches only (never a path, a key or a port of another kind).
        const bool test = name.rfind("AZAHARFRAME_IMMERSIVE_", 0) == 0 ||
                          name.rfind("AZAHARFRAME_XR_", 0) == 0 ||
                          name.rfind("AZAHARFRAME_TEST_", 0) == 0;
        if (!file && !test) {
            continue;
        }
        if (name.find("DSN") != std::string::npos || name.find("TOKEN") != std::string::npos ||
            name.find("KEY") != std::string::npos) {
            continue;
        }
        out.push_back(kv + (file ? " (from the file)" : " (from the environment)"));
    }
    return out;
}

} // namespace Common::FrameTestSwitches
