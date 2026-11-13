// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame dbl: test switches without launch options. At start (main) the app reads
// $XDG_CONFIG_HOME/AzaharFrame/test-switches.txt (~/.config/AzaharFrame/test-switches.txt), one
// NAME=value a line ('#' starts a comment), and sets each AZAHARFRAME_ name it names as if it
// were in the environment; one already in the environment wins (env vars keep working and take
// priority). Every AZAHARFRAME_ switch in effect is logged once the XR session starts: `xr: test
// switch NAME=value (from the file|from the environment)`.
//
// AzaharFrame integ9: the file is LIVE. The XR thread calls Poll() once a second; when the file
// changed (its time or size), it is read again and Poll returns what changed. The switches that
// can change at runtime read their value through Value() (never getenv, which keeps the value
// from the start) and are applied at once; the others log that they need a restart
// (frame_xr/test_switches.h). Nothing calls setenv after Load. A switch set in the environment
// still wins over the file, live too.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <istream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <sys/stat.h>

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

/// A name whose value is never logged or shown (a DSN, a token, a key), whatever file it is in.
inline bool IsSecretName(const std::string& name) {
    return name.find("DSN") != std::string::npos || name.find("TOKEN") != std::string::npos ||
           name.find("KEY") != std::string::npos;
}

inline std::string Trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) {
        return {};
    }
    const auto b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

/// NAME -> value, as a file holds them.
using Values = std::map<std::string, std::string>;

/// AzaharFrame integ9: one NAME=value a line, '#' starts a comment; only AZAHARFRAME_ names; the
/// last line for a name wins. Pure.
inline Values Parse(std::istream& in) {
    Values out;
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
        out[name] = value;
    }
    return out;
}
inline Values ParseText(const std::string& text) {
    std::istringstream in(text);
    return Parse(in);
}

/// AzaharFrame integ9: one switch that changed in the file.
struct Change {
    std::string name;
    std::string value;     ///< the new value (empty when removed)
    bool removed = false;  ///< no longer in the file: back to its default
    bool env_wins = false; ///< set in the environment, which wins: nothing changes
};

/// AzaharFrame integ9: what changed from `before` to `after`, by name. Pure.
inline std::vector<Change> Diff(const Values& before, const Values& after) {
    std::vector<Change> out;
    for (const auto& [name, value] : after) {
        const auto it = before.find(name);
        if (it == before.end() || it->second != value) {
            out.push_back({name, value, false, false});
        }
    }
    for (const auto& [name, value] : before) {
        if (after.find(name) == after.end()) {
            out.push_back({name, std::string(), true, false});
        }
    }
    return out;
}

/// AzaharFrame integ9: the file's values while running (Value, Poll). One per process (Live());
/// the tests make their own with another path and environment.
class LiveFile {
public:
    using GetEnv = const char* (*)(const char*);

    explicit LiveFile(std::string path = std::string(), GetEnv get_env = nullptr)
        : path_(std::move(path)), get_env_(get_env) {}

    /// At the start (Load): the file as read then. Its names already in the environment stay the
    /// environment's (`env_names`); the rest are the file's from now on.
    void Start(std::string path, const Values& file, const std::set<std::string>& env_names) {
        std::scoped_lock lock{mutex_};
        path_ = std::move(path);
        env_names_ = env_names;
        values_.clear();
        for (const auto& [name, value] : file) {
            if (env_names_.count(name) == 0) {
                values_[name] = value;
                ever_file_.insert(name);
            }
        }
        file_seen_ = file;
        stamp_ = StampOf(path_);
    }

    /// The switch's value now: the file's (live) for a name the file set, else the environment's.
    std::optional<std::string> Value(const std::string& name) const {
        std::scoped_lock lock{mutex_};
        if (ever_file_.count(name) != 0 && env_names_.count(name) == 0) {
            const auto it = values_.find(name);
            return it == values_.end() ? std::nullopt : std::optional<std::string>(it->second);
        }
        const char* v = Env(name);
        return v ? std::optional<std::string>(v) : std::nullopt;
    }

    /// XR thread, once a second: reads the file again if it changed (its time or size, or it came
    /// or went) and returns what changed; a name the environment set is marked `env_wins` and
    /// keeps the environment's value. The stat and the read happen outside the lock (a Value()
    /// on another thread never waits on the file), and at most kMaxBytes are read. A secret
    /// name (IsSecretName) takes its new value but is never returned, so it is never logged or
    /// shown in a toast.
    std::vector<Change> Poll() {
        std::string path;
        Stamp before;
        {
            std::scoped_lock lock{mutex_};
            path = path_;
            before = stamp_;
        }
        const Stamp now = StampOf(path);
        if (now == before) {
            return {};
        }
        Values after;
        if (now.exists) {
            std::ifstream in(path, std::ios::binary);
            std::string text(kMaxBytes, '\0');
            in.read(text.data(), static_cast<std::streamsize>(text.size()));
            text.resize(static_cast<std::size_t>(std::max<std::streamsize>(in.gcount(), 0)));
            after = ParseText(text);
        }
        std::scoped_lock lock{mutex_};
        stamp_ = now;
        std::vector<Change> changes = Diff(file_seen_, after);
        file_seen_ = after;
        for (Change& c : changes) {
            const bool env = env_names_.count(c.name) != 0 ||
                             (ever_file_.count(c.name) == 0 && Env(c.name) != nullptr);
            if (env) {
                env_names_.insert(c.name);
                c.env_wins = true;
                continue;
            }
            ever_file_.insert(c.name);
            if (c.removed) {
                values_.erase(c.name);
            } else {
                values_[c.name] = c.value;
            }
        }
        std::erase_if(changes, [](const Change& c) { return IsSecretName(c.name); });
        return changes;
    }

    /// The most of the file Poll reads (a switch file is a few lines).
    static constexpr std::size_t kMaxBytes = 64 * 1024;

private:
    struct Stamp {
        bool exists = false;
        long long mtime_ns = 0;
        long long size = 0;
        bool operator==(const Stamp& o) const {
            return exists == o.exists && mtime_ns == o.mtime_ns && size == o.size;
        }
    };
    static Stamp StampOf(const std::string& path) {
        Stamp s;
        struct stat st {};
        if (!path.empty() && ::stat(path.c_str(), &st) == 0) {
            s.exists = true;
            s.mtime_ns = static_cast<long long>(st.st_mtim.tv_sec) * 1'000'000'000LL +
                         static_cast<long long>(st.st_mtim.tv_nsec);
            s.size = static_cast<long long>(st.st_size);
        }
        return s;
    }
    const char* Env(const std::string& name) const {
        return get_env_ ? get_env_(name.c_str()) : std::getenv(name.c_str());
    }

    mutable std::mutex mutex_;
    std::string path_;
    GetEnv get_env_;
    Values values_;    ///< the file's values now, for the names it owns
    Values file_seen_; ///< the file as last read (all its names)
    std::set<std::string> ever_file_; ///< names the file set (Value reads them from values_)
    std::set<std::string> env_names_; ///< names the environment set (it wins)
    Stamp stamp_;
};

inline LiveFile& Live() {
    static LiveFile live;
    return live;
}

/// A switch's value now (live for the file's names); nullopt when unset.
inline std::optional<std::string> Value(const std::string& name) {
    return Live().Value(name);
}

/// Call first thing in main, before any thread starts (setenv is not thread-safe).
inline void Load() {
    const std::string path = Path();
    if (path.empty()) {
        return;
    }
    std::ifstream in(path);
    const Values file = Parse(in);
    std::set<std::string> env_names;
    for (const auto& [name, value] : file) {
        if (std::getenv(name.c_str()) != nullptr) {
            env_names.insert(name); // the environment wins
            continue;
        }
        if (setenv(name.c_str(), value.c_str(), 0) == 0) {
            FromFile().push_back(name);
        }
    }
    Live().Start(path, file, env_names);
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
        if (IsSecretName(name)) {
            continue;
        }
        out.push_back(kv + (file ? " (from the file)" : " (from the environment)"));
    }
    return out;
}

} // namespace Common::FrameTestSwitches
