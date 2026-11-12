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

std::string Backup(const std::string& save_dir, const std::string& root, const std::string& stamp) {
    std::error_code ec;
    if (!fs::is_directory(save_dir, ec)) {
        return {};
    }
    fs::create_directories(root, ec);
    fs::path dst = fs::path(root) / stamp;
    for (int n = 2; fs::exists(dst, ec); n++) {
        dst = fs::path(root) / fmt::format("{}-{}", stamp, n);
    }
    const fs::path tmp = dst.string() + ".part";
    fs::remove_all(tmp, ec);
    fs::copy(save_dir, tmp, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
    if (ec) {
        fs::remove_all(tmp, ec);
        return {};
    }
    fs::rename(tmp, dst, ec);
    if (ec) {
        fs::remove_all(tmp, ec);
        return {};
    }
    Rotate(root, kKeep);
    return dst.string();
}

bool Restore(const std::string& backup, const std::string& save_dir) {
    std::error_code ec;
    if (!fs::is_directory(backup, ec) || save_dir.empty()) {
        return false;
    }
    std::string dir = save_dir;
    while (dir.size() > 1 && dir.back() == '/') {
        dir.pop_back();
    }
    const fs::path fresh = dir + ".restore";
    const fs::path old = dir + ".old";
    fs::remove_all(fresh, ec);
    fs::remove_all(old, ec);
    fs::create_directories(fs::path(dir).parent_path(), ec);
    fs::copy(backup, fresh, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
    if (ec) {
        fs::remove_all(fresh, ec);
        return false;
    }
    const bool had = fs::exists(dir, ec);
    if (had) {
        fs::rename(dir, old, ec);
        if (ec) {
            fs::remove_all(fresh, ec);
            return false;
        }
    }
    fs::rename(fresh, dir, ec);
    if (ec) {
        if (had) {
            std::error_code ec2;
            fs::rename(old, dir, ec2); // as it was
        }
        fs::remove_all(fresh, ec);
        return false;
    }
    fs::remove_all(old, ec);
    return true;
}

void Arm(const std::string& root, const std::string& backup, const std::string& save_dir) {
    std::error_code ec;
    fs::create_directories(root, ec);
    std::ofstream(fs::path(root) / kMarker, std::ios::trunc) << backup << '\n' << save_dir << '\n';
}

void Disarm(const std::string& root) {
    std::error_code ec;
    fs::remove(fs::path(root) / kMarker, ec);
}

std::optional<Pending> Armed(const std::string& root) {
    std::ifstream in(fs::path(root) / kMarker);
    Pending p;
    if (!in || !std::getline(in, p.backup) || !std::getline(in, p.save_dir) || p.backup.empty() ||
        p.save_dir.empty()) {
        return std::nullopt;
    }
    return p;
}

} // namespace Core::SaveBackup
