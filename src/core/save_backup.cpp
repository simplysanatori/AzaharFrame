// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame savestate: see save_backup.h.

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <fmt/format.h>
#include "core/save_backup.h"

namespace Core::SaveBackup {

namespace fs = std::filesystem;

namespace {

constexpr const char* kMarker = "load-anyway.pending";

bool IsBackupName(const std::string& name) {
    // "20261112-101500", maybe "-2" after it (two in one second)
    return name.size() >= 15 && name[8] == '-' &&
           std::all_of(name.begin(), name.begin() + 8, [](char c) { return c >= '0' && c <= '9'; });
}

} // namespace

std::string Root(const std::string& user_dir, u64 title_id) {
    std::string dir = user_dir;
    if (!dir.empty() && dir.back() != '/') {
        dir += '/';
    }
    return fmt::format("{}save-backups/{:016X}/", dir, title_id);
}

std::string Stamp(s64 unix_seconds) {
    const std::time_t t = static_cast<std::time_t>(unix_seconds);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    return buf;
}

std::vector<std::string> List(const std::string& root) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(root, ec)) {
        const std::string name = e.path().filename().string();
        if (e.is_directory(ec) && !e.is_symlink(ec) && IsBackupName(name)) {
            names.push_back(name);
        }
    }
    // Newest first: the stamps sort by time; "-2" after its own second.
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        return a.substr(0, 15) != b.substr(0, 15) ? a.substr(0, 15) > b.substr(0, 15)
               : a.size() != b.size()             ? a.size() > b.size()
                                                  : a > b;
    });
    std::vector<std::string> out;
    for (const std::string& n : names) {
        out.push_back((fs::path(root) / n).string());
    }
    return out;
}

void Rotate(const std::string& root, std::size_t keep) {
    const auto all = List(root);
    std::error_code ec;
    for (std::size_t i = keep; i < all.size(); i++) {
        fs::remove_all(all[i], ec);
    }
}

namespace {

/// Each part's folder that exists copied to root/<name>/<part>/ ("-2", "-3" after <name> when
/// taken), whole or not at all. The copy's path, or "" when no part's folder exists or a copy
/// failed.
std::string CopyParts(const std::vector<Part>& parts, const std::string& root,
                      const std::string& name) {
    std::error_code ec;
    const bool any = std::any_of(parts.begin(), parts.end(), [](const Part& p) {
        std::error_code e;
        return !p.dir.empty() && fs::is_directory(p.dir, e);
    });
    if (!any) {
        return {};
    }
    fs::create_directories(root, ec);
    fs::path dst = fs::path(root) / name;
    for (int n = 2; fs::exists(dst, ec); n++) {
        dst = fs::path(root) / fmt::format("{}-{}", name, n);
    }
    const fs::path tmp = dst.string() + ".part";
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    for (const Part& p : parts) {
        if (ec || p.dir.empty() || !fs::is_directory(p.dir, ec)) {
            ec.clear();
            continue;
        }
        fs::copy(p.dir, tmp / p.name, fs::copy_options::recursive | fs::copy_options::copy_symlinks,
                 ec);
        if (ec) {
            fs::remove_all(tmp, ec);
            return {};
        }
    }
    fs::rename(tmp, dst, ec);
    if (ec) {
        fs::remove_all(tmp, ec);
        return {};
    }
    return dst.string();
}

std::string NoSlash(std::string dir) {
    while (dir.size() > 1 && dir.back() == '/') {
        dir.pop_back();
    }
    return dir;
}

} // namespace

std::string Backup(const std::vector<Part>& parts, const std::string& root,
                   const std::string& stamp) {
    const std::string dst = CopyParts(parts, root, stamp);
    if (!dst.empty()) {
        Rotate(root, kKeep);
    }
    return dst;
}

bool Restore(const std::string& backup, const std::vector<Part>& parts, const std::string& root,
             const std::string& stamp, std::string* kept) {
    std::error_code ec;
    if (kept) {
        kept->clear();
    }
    if (!fs::is_directory(backup, ec) || parts.empty()) {
        return false;
    }
    // The parts the backup holds (a part it does not hold is left as it is)
    struct Swap {
        std::string dir, fresh, old;
        bool had = false, swapped = false;
    };
    std::vector<Swap> swaps;
    for (const Part& p : parts) {
        if (!p.dir.empty() && fs::is_directory(fs::path(backup) / p.name, ec)) {
            const std::string dir = NoSlash(p.dir);
            swaps.push_back({dir, dir + ".restore", dir + ".old"});
        }
    }
    if (swaps.empty()) {
        return false;
    }
    // AzaharFrame advfix: what is being replaced may hold newer progress: it is kept first (not
    // rotated yet, so the backup being restored cannot go before it is copied)
    const bool any_now = std::any_of(parts.begin(), parts.end(), [](const Part& p) {
        std::error_code e;
        return !p.dir.empty() && fs::exists(p.dir, e);
    });
    if (any_now) {
        const std::string before =
            CopyParts(parts, root, stamp + std::string(kBeforeRestoreSuffix));
        if (before.empty()) {
            return false;
        }
        if (kept) {
            *kept = before;
        }
    }
    const auto undo = [&] {
        std::error_code e;
        for (Swap& w : swaps) {
            if (w.swapped) {
                fs::remove_all(w.dir, e);
            }
            if (w.had && !fs::exists(w.dir, e)) {
                fs::rename(w.old, w.dir, e); // as it was
            }
            fs::remove_all(w.fresh, e);
        }
    };
    // Each copied beside its folder first...
    std::size_t i = 0;
    for (const Part& p : parts) {
        if (p.dir.empty() || !fs::is_directory(fs::path(backup) / p.name, ec)) {
            continue;
        }
        Swap& w = swaps[i++];
        fs::remove_all(w.fresh, ec);
        fs::remove_all(w.old, ec);
        fs::create_directories(fs::path(w.dir).parent_path(), ec);
        fs::copy(fs::path(backup) / p.name, w.fresh,
                 fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
        if (ec) {
            undo();
            return false;
        }
    }
    // ...then all swapped in, or none
    for (Swap& w : swaps) {
        w.had = fs::exists(w.dir, ec);
        if (w.had) {
            fs::rename(w.dir, w.old, ec);
            if (ec) {
                w.had = false; // still in place
                undo();
                return false;
            }
        }
        fs::rename(w.fresh, w.dir, ec);
        if (ec) {
            undo();
            return false;
        }
        w.swapped = true;
    }
    for (const Swap& w : swaps) {
        fs::remove_all(w.old, ec);
    }
    Rotate(root, kKeep);
    return true;
}

std::string TimeText(const std::string& backup, s64 now_unix) {
    const std::string name = fs::path(backup).filename().string();
    if (!IsBackupName(name) || !std::all_of(name.begin() + 9, name.begin() + 15,
                                            [](char c) { return c >= '0' && c <= '9'; })) {
        return {};
    }
    const std::string hm = name.substr(9, 2) + ":" + name.substr(11, 2);
    if (name.compare(0, 8, Stamp(now_unix), 0, 8) == 0) {
        return hm;
    }
    return fmt::format("{}-{}-{} {}", name.substr(0, 4), name.substr(4, 2), name.substr(6, 2), hm);
}

std::string RestoreQuestion(const std::string& time) {
    return time.empty() ? std::string("Restore Save Data?")
                        : fmt::format("Restore Save Data From {}?", time);
}

void Arm(const std::string& root, const std::string& backup, const std::vector<Part>& parts) {
    std::error_code ec;
    fs::create_directories(root, ec);
    std::ofstream out(fs::path(root) / kMarker, std::ios::trunc);
    out << backup << '\n';
    for (const Part& p : parts) { // AzaharFrame advfix: "<name>\t<folder>" per part
        if (!p.dir.empty()) {
            out << p.name << '\t' << p.dir << '\n';
        }
    }
}

void Disarm(const std::string& root) {
    std::error_code ec;
    fs::remove(fs::path(root) / kMarker, ec);
}

std::optional<Pending> Armed(const std::string& root) {
    std::ifstream in(fs::path(root) / kMarker);
    Pending p;
    if (!in || !std::getline(in, p.backup) || p.backup.empty()) {
        return std::nullopt;
    }
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t tab = line.find('\t');
        if (tab != std::string::npos && tab > 0 && tab + 1 < line.size()) {
            p.parts.push_back({line.substr(0, tab), line.substr(tab + 1)});
        }
    }
    if (p.parts.empty()) {
        return std::nullopt;
    }
    return p;
}

} // namespace Core::SaveBackup
