// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame compat: see crash_guard.h.

#include "core/mods/crash_guard.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <fmt/format.h>
#include "common/file_util.h"

namespace fs = std::filesystem;

namespace Mods::CrashGuard {

namespace {

std::string OneLine(std::string s) {
    for (char& c : s) {
        if (c == '\t' || c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return s;
}

bool Same(const Item& a, Item::Kind kind, const std::string& id) {
    return a.kind == kind && a.id == id;
}

} // namespace

std::string DefaultDir() {
    return FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + "frame-crash-guard/";
}

Guard::Guard(std::string dir) : dir_(std::move(dir)) {
    if (!dir_.empty() && dir_.back() != '/') {
        dir_ += '/';
    }
}

std::string Guard::File(u64 title_id, const char* ext) const {
    return fmt::format("{}{:016X}.{}", dir_, title_id, ext);
}

std::vector<Item> Guard::Read(const std::string& path) const {
    std::vector<Item> items;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t a = line.find('\t');
        const std::size_t b = a == std::string::npos ? a : line.find('\t', a + 1);
        if (b == std::string::npos) {
            continue;
        }
        const std::string kind = line.substr(0, a);
        Item item;
        if (kind == "mod") {
            item.kind = Item::Kind::Mod;
        } else if (kind == "pack") {
            item.kind = Item::Kind::Pack;
        } else {
            continue;
        }
        item.id = line.substr(a + 1, b - a - 1);
        item.name = line.substr(b + 1);
        if (!item.id.empty()) {
            items.push_back(std::move(item));
        }
    }
    return items;
}

void Guard::Write(const std::string& path, const std::vector<Item>& items) const {
    std::error_code ec;
    if (items.empty()) {
        fs::remove(path, ec);
        return;
    }
    fs::create_directories(dir_, ec);
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        for (const Item& i : items) {
            out << (i.kind == Item::Kind::Mod ? "mod" : "pack") << '\t' << OneLine(i.id) << '\t'
                << OneLine(i.name) << '\n';
        }
        out.flush();
        if (!out) {
            fs::remove(tmp, ec);
            return;
        }
    }
    fs::rename(tmp, path, ec);
}

void Guard::NoteOn(u64 title_id, const Item& item) {
    auto items = Read(File(title_id, "new"));
    const auto it = std::find_if(items.begin(), items.end(),
                                 [&](const Item& i) { return Same(i, item.kind, item.id); });
    if (it != items.end()) {
        it->name = item.name;
    } else {
        items.push_back(item);
    }
    Write(File(title_id, "new"), items);
}

void Guard::NoteOff(u64 title_id, Item::Kind kind, const std::string& id) {
    auto items = Read(File(title_id, "new"));
    std::erase_if(items, [&](const Item& i) { return Same(i, kind, id); });
    Write(File(title_id, "new"), items);
}

std::vector<Item> Guard::New(u64 title_id) const {
    return Read(File(title_id, "new"));
}

std::vector<Item> Guard::TakeCrashed(u64 title_id) {
    const std::vector<Item> crashed = Read(File(title_id, "marker"));
    std::error_code ec;
    fs::remove(File(title_id, "marker"), ec);
    for (const Item& i : crashed) {
        NoteOff(title_id, i.kind, i.id);
    }
    return crashed;
}

std::vector<Item> Guard::Arm(u64 title_id, const std::vector<Item>& active) {
    std::vector<Item> armed;
    for (const Item& n : New(title_id)) {
        const auto on = std::find_if(active.begin(), active.end(),
                                     [&](const Item& a) { return Same(a, n.kind, n.id); });
        if (on != active.end()) {
            armed.push_back(n);
        }
    }
    Write(File(title_id, "marker"), armed); // none: no marker
    return armed;
}

void Guard::Clean(u64 title_id) {
    const std::vector<Item> armed = Read(File(title_id, "marker"));
    std::error_code ec;
    fs::remove(File(title_id, "marker"), ec);
    for (const Item& i : armed) {
        NoteOff(title_id, i.kind, i.id);
    }
}

bool Guard::Armed(u64 title_id) const {
    std::error_code ec;
    return fs::exists(File(title_id, "marker"), ec);
}

std::string TurnedOffText(const std::vector<std::string>& names) {
    std::string joined;
    for (std::size_t i = 0; i < names.size(); i++) {
        if (i > 0) {
            joined += i + 1 == names.size() ? " and " : ", ";
        }
        joined += names[i];
    }
    return fmt::format("Turned Off {}: The game closed unexpectedly after {} installed.", joined,
                       names.size() > 1 ? "they were" : "it was");
}

} // namespace Mods::CrashGuard
