// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emhb: see emerald3ds_detect.h.

#include "core/homebrew/emerald3ds_detect.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <fmt/format.h>

namespace Homebrew::Emerald3ds {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kSmdhSize = 0x36C0;
constexpr std::size_t kTitleBlock = 0x200; ///< one language: short 0x80, long 0x100, publisher 0x80
constexpr int kEnglish = 1;
constexpr int kMaxDepth = 6; ///< a deep game folder's levels scanned

u32 Le32(std::span<const u8> d, std::size_t at) {
    return d.size() < at + 4 ? 0
                             : static_cast<u32>(d[at]) | (static_cast<u32>(d[at + 1]) << 8) |
                                   (static_cast<u32>(d[at + 2]) << 16) |
                                   (static_cast<u32>(d[at + 3]) << 24);
}

u16 Le16(std::span<const u8> d, std::size_t at) {
    return d.size() < at + 2 ? 0 : static_cast<u16>(d[at] | (d[at + 1] << 8));
}

u64 Le64(std::span<const u8> d, std::size_t at) {
    return static_cast<u64>(Le32(d, at)) | (static_cast<u64>(Le32(d, at + 4)) << 32);
}

/// `size` bytes at `offset` of a file (fewer at its end; empty when it cannot be read).
std::vector<u8> ReadAt(const std::string& path, u64 offset, std::size_t size) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    in.seekg(static_cast<std::streamoff>(offset));
    if (!in) {
        return {};
    }
    std::vector<u8> out(size);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    out.resize(static_cast<std::size_t>(std::max<std::streamsize>(in.gcount(), 0)));
    return out;
}

std::string Utf16(std::span<const u8> d) {
    std::string out;
    for (std::size_t i = 0; i + 1 < d.size(); i += 2) {
        u32 c = Le16(d, i);
        if (c == 0) {
            break;
        }
        if (c >= 0xD800 && c < 0xDC00 && i + 3 < d.size()) {
            const u32 lo = Le16(d, i + 2);
            if (lo >= 0xDC00 && lo < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            }
        }
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (c >> 12)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (c >> 18)));
            out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

/// Lower-case ASCII letters and digits only: "Pokémon Emerald 3DS" -> "pokmonemerald3ds".
std::string Squash(std::string_view s) {
    std::string out;
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) != 0 && c < 0x80) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

std::string LowerExt(const fs::path& p) {
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e;
}

bool IsFile(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool IsDir(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

std::string DirSlash(const fs::path& p) {
    std::string s = p.generic_string();
    if (!s.empty() && s.back() != '/') {
        s.push_back('/');
    }
    return s;
}

std::string NoSlash(std::string s) {
    while (s.size() > 1 && s.back() == '/') {
        s.pop_back();
    }
    return s;
}

/// Same file or folder, whatever the trailing slash or "a/../b".
bool SamePath(const std::string& a, const std::string& b) {
    return fs::path(NoSlash(a)).lexically_normal() == fs::path(NoSlash(b)).lexically_normal();
}

/// An unencrypted NCCH at `base` of a file: its ExeFS "icon".
std::optional<SmdhText> NcchSmdh(const std::string& path, u64 base) {
    const std::vector<u8> head = ReadAt(path, base, 0x200);
    if (head.size() < 0x200 || std::string_view(reinterpret_cast<const char*>(&head[0x100]), 4) !=
                                   "NCCH") {
        return std::nullopt;
    }
    if ((head[0x18F] & 0x4) == 0) { // flags[7] NoCrypto: else the ExeFS is encrypted
        return std::nullopt;
    }
    const u64 exefs = base + static_cast<u64>(Le32(head, 0x1A0)) * 0x200;
    if (Le32(head, 0x1A4) == 0) {
        return std::nullopt;
    }
    const std::vector<u8> fs_head = ReadAt(path, exefs, 0x200);
    if (fs_head.size() < 0xA0) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < 10; i++) {
        const std::size_t e = i * 16;
        const std::string_view name(reinterpret_cast<const char*>(&fs_head[e]), 8);
        if (name.substr(0, name.find('\0')) == "icon") {
            const std::vector<u8> icon =
                ReadAt(path, exefs + 0x200 + Le32(fs_head, e + 8), kSmdhSize);
            return ParseSmdh(icon);
        }
    }
    return std::nullopt;
}

u64 Align64(u64 v) {
    return (v + 63) / 64 * 64;
}

std::optional<SmdhText> CiaSmdh(const std::string& path) {
    const std::vector<u8> h = ReadAt(path, 0, 0x20);
    if (h.size() < 0x20) {
        return std::nullopt;
    }
    const u64 header = Le32(h, 0), cert = Le32(h, 8), ticket = Le32(h, 0xC), tmd = Le32(h, 0x10);
    const u64 meta = Le32(h, 0x14), content = Le64(h, 0x18);
    if (header != 0x2020) {
        return std::nullopt;
    }
    const u64 content_at = Align64(Align64(Align64(Align64(header) + cert) + ticket) + tmd);
    if (meta >= 0x400 + kSmdhSize) {
        const u64 meta_at = Align64(content_at + content);
        if (auto s = ParseSmdh(ReadAt(path, meta_at + 0x400, kSmdhSize))) {
            return s;
        }
    }
    return NcchSmdh(path, content_at); // no meta: the first content's own icon
}

void Add(Detection& d, Piece piece, const std::string& path) {
    for (const Found& f : d.found) {
        if (SamePath(f.path, path)) {
            return;
        }
    }
    d.found.push_back({piece, path});
}

bool IsGameFile(const fs::path& p) {
    const std::string e = LowerExt(p);
    return e == ".3dsx" || e == ".cia";
}

/// Emerald 3DS by its name, else by what its SMDH says.
bool IsEmeraldGame(const fs::path& p) {
    if (IsEmeraldName(p.filename().string())) {
        return true;
    }
    const auto smdh = ReadGameSmdh(p.string());
    return smdh && IsEmeraldTitle(*smdh);
}

bool IsRomPath(const std::string& path, const Paths& paths) {
    return LowerExt(fs::path(path)) == ".gba" || SamePath(path, paths.rom_copy) ||
           SamePath(path, paths.data_dir) || SamePath(path, paths.dspfirm) ||
           SamePath(path, paths.app_dir);
}

} // namespace

std::optional<SmdhText> ParseSmdh(std::span<const u8> data) {
    if (data.size() < 8 + kTitleBlock * 16 ||
        std::string_view(reinterpret_cast<const char*>(data.data()), 4) != "SMDH") {
        return std::nullopt;
    }
    const auto read = [&](int lang) {
        const auto block = data.subspan(8 + kTitleBlock * static_cast<std::size_t>(lang), kTitleBlock);
        return SmdhText{Utf16(block.subspan(0, 0x80)), Utf16(block.subspan(0x80, 0x100)),
                        Utf16(block.subspan(0x180, 0x80))};
    };
    SmdhText t = read(kEnglish);
    for (int lang = 0; lang < 16 && t.short_title.empty() && t.long_title.empty(); lang++) {
        t = read(lang);
    }
    return t;
}

std::optional<SmdhText> ReadGameSmdh(const std::string& path) {
    const std::vector<u8> head = ReadAt(path, 0, 0x200);
    if (head.size() < 4) {
        return std::nullopt;
    }
    const std::string_view magic(reinterpret_cast<const char*>(head.data()), 4);
    if (magic == "SMDH") {
        return ParseSmdh(ReadAt(path, 0, kSmdhSize));
    }
    if (magic == "3DSX") {
        if (Le16(head, 4) < 0x2C) {
            return std::nullopt; // no extended header: no SMDH
        }
        const u32 at = Le32(head, 0x20), size = Le32(head, 0x24);
        if (size < kSmdhSize) {
            return std::nullopt;
        }
        return ParseSmdh(ReadAt(path, at, kSmdhSize));
    }
    if (head.size() >= 0x104 &&
        std::string_view(reinterpret_cast<const char*>(&head[0x100]), 4) == "NCCH") {
        return NcchSmdh(path, 0);
    }
    if (LowerExt(fs::path(path)) == ".cia") {
        return CiaSmdh(path);
    }
    return std::nullopt;
}

bool IsEmeraldName(std::string_view file_name) {
    const auto dot = file_name.find_last_of('.');
    const std::string stem = Squash(dot == std::string_view::npos ? file_name : file_name.substr(0, dot));
    return stem.find("emerald3ds") != std::string::npos;
}

bool IsEmeraldTitle(const SmdhText& smdh) {
    const std::string titles = Squash(smdh.short_title) + "|" + Squash(smdh.long_title);
    if (titles.find("emerald3ds") != std::string::npos) {
        return true;
    }
    return titles.find("emerald") != std::string::npos &&
           Squash(smdh.publisher).find("zallax") != std::string::npos;
}

std::string VersionInText(std::string_view text) {
    for (std::size_t i = 0; i + 3 < text.size(); i++) {
        if ((text[i] != 'v' && text[i] != 'V') ||
            (i > 0 && std::isalnum(static_cast<unsigned char>(text[i - 1])) != 0)) {
            continue;
        }
        std::size_t j = i + 1, dots = 0;
        std::string out = "v";
        while (j < text.size()) {
            const char c = text[j];
            if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
                out.push_back(c);
            } else if (c == '.' && out.back() != '.' && out.back() != 'v' && j + 1 < text.size() &&
                       std::isdigit(static_cast<unsigned char>(text[j + 1])) != 0) {
                out.push_back(c);
                dots++;
            } else {
                break;
            }
            j++;
        }
        if (dots >= 1 && dots <= 2 && ParseVersion(out)) {
            return out;
        }
    }
    return {};
}

std::optional<PakHeader> ParsePakHeader(std::span<const u8> data) {
    if (data.size() < 64 || std::string_view(reinterpret_cast<const char*>(data.data()), 8) !=
                                std::string_view("EM3DPAK\0", 8)) {
        return std::nullopt;
    }
    if (Le32(data, 8) != 1) {
        return std::nullopt;
    }
    PakHeader h;
    h.abi = fmt::format("{:08x}", Le32(data, 12));
    for (std::size_t i = 16; i < 36; i++) {
        h.rom_sha1 += fmt::format("{:02x}", data[i]);
    }
    h.entries = Le32(data, 36);
    return h;
}

std::optional<PakHeader> ReadPakHeader(const std::string& path) {
    return ParsePakHeader(ReadAt(path, 0, 64));
}

// ---------------------------------------------------------------------------------------------

bool Detection::Has(Piece piece) const {
    return std::any_of(found.begin(), found.end(), [piece](const Found& f) { return f.piece == piece; });
}

std::string_view Detection::Problem() const {
    if (!Any() || playable) {
        return {};
    }
    return launchable ? "Game Data Missing" : "Game File Missing";
}

Detection Detect(const DetectInput& input) {
    Detection d;
    const Paths& p = input.paths;
    std::error_code ec;

    // What the build left in user/emerald3ds/ (rom.gba is the player's, never a piece)
    if (IsFile(p.state_file)) {
        Add(d, Piece::State, p.state_file);
        std::ifstream in(p.state_file, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (auto s = ParseState(text)) {
            d.state = std::move(*s);
            d.app_made = true;
        }
    }

    // The SD card's copy and its pack, where the game reads it
    std::optional<PakHeader> pak;
    if (IsFile(p.three_dsx)) {
        Add(d, Piece::SdmcGame, p.three_dsx);
    }
    // Copies elsewhere under sdmc/3ds/ (its top and one folder down; not the saves' backups)
    const fs::path threeds = fs::path(NoSlash(p.app_dir)).parent_path();
    for (fs::recursive_directory_iterator it(threeds, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (it.depth() >= 1 && it->is_directory(ec)) {
            it.disable_recursion_pending();
        }
        if (it->is_directory(ec) && SamePath(it->path().string(), p.backup_root)) {
            it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec) || LowerExt(it->path()) != ".3dsx" ||
            !IsEmeraldGame(it->path())) {
            continue;
        }
        Add(d, Piece::SdmcGame, it->path().generic_string());
        // A pack beside a copy outside 3ds/emerald3ds/
        const std::string beside = (it->path().parent_path() / std::string(kPakName)).generic_string();
        if (!SamePath(beside, p.pak) && IsFile(beside)) {
            Add(d, Piece::Pak, beside);
            if (!pak) {
                pak = ReadPakHeader(beside);
            }
        }
    }
    ec.clear();
    if (IsFile(p.smdh)) {
        Add(d, Piece::Smdh, p.smdh);
    }
    if (IsFile(p.pak)) {
        Add(d, Piece::Pak, p.pak);
        if (auto h = ReadPakHeader(p.pak)) {
            pak = h;
        }
    }

    // The game folders' copies
    const auto game_file = [&](const fs::path& f) {
        if (IsGameFile(f) && IsEmeraldGame(f)) {
            Add(d, Piece::GameCopy, f.generic_string());
        }
    };
    for (const auto& [dir, deep] : input.game_dirs) {
        if (dir.empty() || !IsDir(dir)) {
            continue;
        }
        if (!deep) {
            for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end; it.increment(ec)) {
                if (it->is_regular_file(ec)) {
                    game_file(it->path());
                }
            }
        } else {
            for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end; it.increment(ec)) {
                if (it.depth() >= kMaxDepth) {
                    it.disable_recursion_pending();
                }
                if (it->is_regular_file(ec)) {
                    game_file(it->path());
                }
            }
        }
        ec.clear();
    }
    if (d.state && !d.state->rom_dir_copy.empty() && IsFile(d.state->rom_dir_copy)) {
        Add(d, Piece::GameCopy, d.state->rom_dir_copy); // a folder no longer in the list
    }

    // Installed titles (applications: 00040000)
    std::string title_app;
    for (const std::string& root : input.title_roots) {
        if (root.empty()) {
            continue;
        }
        const fs::path apps = fs::path(root) / "00040000";
        for (fs::directory_iterator t(apps, fs::directory_options::skip_permission_denied, ec), end;
             !ec && t != end; t.increment(ec)) {
            if (!t->is_directory(ec)) {
                continue;
            }
            std::error_code ec2;
            for (fs::directory_iterator c(t->path() / "content", ec2), cend; !ec2 && c != cend;
                 c.increment(ec2)) {
                if (LowerExt(c->path()) != ".app" || !c->is_regular_file(ec2)) {
                    continue;
                }
                const auto smdh = ReadGameSmdh(c->path().string());
                if (smdh && IsEmeraldTitle(*smdh)) {
                    Add(d, Piece::Title, DirSlash(t->path()));
                    if (title_app.empty()) {
                        title_app = c->path().generic_string();
                    }
                    const std::string data = DirSlash(t->path() / "data");
                    if (IsDir(data)) {
                        d.saves.push_back(data);
                    }
                    break;
                }
            }
        }
        ec.clear();
    }

    if (IsDir(p.work_dir)) {
        Add(d, Piece::Work, p.work_dir);
    }
    const std::string followers = p.data_dir + "followers-cache/";
    if (IsDir(followers)) {
        Add(d, Piece::FollowersCache, followers);
    }
    const std::string choices = p.data_dir + "fallback-choices.json";
    if (IsFile(choices)) {
        Add(d, Piece::Choices, choices);
    }
    for (const std::string& f : {p.three_dsx + ".new", p.smdh + ".new", p.pak + ".new",
                                 p.state_file + ".tmp", p.state_file + ".new", choices + ".new"}) {
        if (IsFile(f)) {
            Add(d, Piece::Temp, f);
        }
    }

    // The save, kept unless asked
    for (const std::string& name : SaveFiles()) {
        if (IsFile(p.app_dir + name)) {
            d.saves.push_back(p.app_dir + name);
        }
    }
    if (IsDir(p.backup_root)) {
        d.saves.push_back(p.backup_root);
    }

    std::stable_sort(d.found.begin(), d.found.end(),
                     [](const Found& a, const Found& b) { return a.piece < b.piece; });

    // What it is
    std::string sdmc_other, copy_3dsx, copy_cia;
    for (const Found& f : d.found) {
        const bool is_3dsx = LowerExt(fs::path(f.path)) == ".3dsx";
        if (f.piece == Piece::SdmcGame && !SamePath(f.path, p.three_dsx) && sdmc_other.empty()) {
            sdmc_other = f.path;
        } else if (f.piece == Piece::GameCopy && is_3dsx && copy_3dsx.empty()) {
            copy_3dsx = f.path;
        } else if (f.piece == Piece::GameCopy && !is_3dsx && copy_cia.empty()) {
            copy_cia = f.path;
        }
    }
    const bool sdmc_game = IsFile(p.three_dsx);
    d.launchable = d.Has(Piece::SdmcGame) || d.Has(Piece::GameCopy) || d.Has(Piece::Title);
    d.has_pak = pak.has_value();
    d.playable = d.launchable && d.has_pak;
    const std::string state_copy = d.state && !d.state->rom_dir_copy.empty() &&
                                           IsFile(d.state->rom_dir_copy)
                                       ? d.state->rom_dir_copy
                                       : std::string{};
    d.game_copy = !state_copy.empty() ? state_copy : !copy_3dsx.empty() ? copy_3dsx : copy_cia;
    // Play starts the game list's copy (so its entry is the one that runs), else the SD card's
    if (!state_copy.empty()) {
        d.game_path = state_copy;
    } else if (!copy_3dsx.empty()) {
        d.game_path = copy_3dsx;
    } else if (sdmc_game) {
        d.game_path = p.three_dsx;
    } else if (!sdmc_other.empty()) {
        d.game_path = sdmc_other;
    } else if (!copy_cia.empty()) {
        d.game_path = copy_cia;
    } else {
        d.game_path = title_app;
    }
    d.three_dsx = sdmc_game ? p.three_dsx : !copy_3dsx.empty() ? copy_3dsx : sdmc_other;

    if (d.app_made) {
        d.version = d.state->tag;
        d.language = d.state->language;
        d.data_abi = d.state->data_abi;
        d.rom_sha1 = d.state->rom_sha1;
    } else {
        // The release, when its SMDH names one (the SD card's .smdh, else the game's own)
        std::optional<SmdhText> smdh;
        if (IsFile(p.smdh)) {
            smdh = ReadGameSmdh(p.smdh);
        }
        if (!smdh && !d.three_dsx.empty()) {
            smdh = ReadGameSmdh(d.three_dsx);
        }
        if (smdh) {
            d.version = VersionInText(smdh->short_title + " " + smdh->long_title + " " +
                                      smdh->publisher);
        }
    }
    if (pak) {
        if (d.data_abi.empty()) {
            d.data_abi = pak->abi;
        }
        if (d.rom_sha1.empty()) {
            d.rom_sha1 = pak->rom_sha1;
        }
    }
    if (d.language.empty() && !d.rom_sha1.empty()) {
        // The English ROM also builds Português (Brasil): only the pack's ROM's own when it is not
        if (const RomProfile* rom = FindRom(d.rom_sha1); rom && LanguagesFor(*rom).size() == 1) {
            d.language = std::string(rom->language);
        }
    }
    return d;
}

const Variant* FindVariantByThreeDsx(const Manifest& manifest, std::string_view sha256) {
    if (sha256.size() != 64) {
        return nullptr;
    }
    std::string want(sha256);
    std::transform(want.begin(), want.end(), want.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const Variant& v : manifest.variants) {
        if (v.three_dsx_sha256 == want) {
            return &v;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------------

UninstallPlan PlanUninstall(const Detection& d, const Paths& paths, bool delete_save) {
    UninstallPlan plan;
    const auto add_if_empty = [&plan](const std::string& dir) {
        if (!dir.empty() &&
            std::none_of(plan.remove_if_empty.begin(), plan.remove_if_empty.end(),
                         [&](const std::string& x) { return SamePath(x, dir); })) {
            plan.remove_if_empty.push_back(dir);
        }
    };
    const fs::path threeds = fs::path(NoSlash(paths.app_dir)).parent_path();
    for (const Found& f : d.found) {
        if (f.path.empty() || IsRomPath(f.path, paths)) {
            continue; // never the ROM, its copy, the data folder or dspfirm.cdc as a whole
        }
        switch (f.piece) {
        case Piece::Title:
            if (delete_save) {
                plan.dirs.push_back(f.path);
            } else {
                plan.dirs.push_back(f.path + "content/");
                add_if_empty(f.path); // gone unless data/ (its save) stays
            }
            break;
        case Piece::Work:
        case Piece::FollowersCache:
            plan.dirs.push_back(f.path);
            break;
        case Piece::SdmcGame: {
            plan.files.push_back(f.path);
            const fs::path parent = fs::path(f.path).parent_path();
            if (parent.lexically_normal() != threeds.lexically_normal()) {
                add_if_empty(DirSlash(parent)); // sdmc/3ds/Emerald3DS/, as it came
            }
            break;
        }
        default:
            plan.files.push_back(f.path);
            break;
        }
    }
    for (const std::string& s : d.saves) {
        if (!delete_save) {
            plan.kept.push_back(s);
        } else if (!s.empty() && s.back() == '/') {
            plan.dirs.push_back(s);
        } else {
            plan.files.push_back(s);
        }
    }
    if (IsFile(paths.rom_copy)) {
        plan.kept.push_back(paths.rom_copy);
    }
    add_if_empty(paths.app_dir);
    add_if_empty(paths.data_dir);
    std::stable_sort(plan.remove_if_empty.begin(), plan.remove_if_empty.end(),
                     [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
    return plan;
}

UninstallResult RunUninstall(const UninstallPlan& plan) {
    UninstallResult r;
    const auto safe = [](const std::string& path) {
        const fs::path n = fs::path(NoSlash(path)).lexically_normal();
        return !path.empty() && n.has_parent_path() && n != n.root_path() && n.filename() != "..";
    };
    for (const std::string& f : plan.files) {
        std::error_code ec;
        if (!safe(f) || !fs::exists(f, ec)) {
            continue;
        }
        if (fs::remove(f, ec) && !ec) {
            r.removed.push_back(f);
        } else {
            r.failed.push_back(f);
        }
    }
    for (const std::string& dir : plan.dirs) {
        std::error_code ec;
        if (!safe(dir) || !fs::exists(NoSlash(dir), ec)) {
            continue;
        }
        fs::remove_all(NoSlash(dir), ec);
        if (ec || fs::exists(NoSlash(dir), ec)) {
            r.failed.push_back(dir);
        } else {
            r.removed.push_back(dir);
        }
    }
    for (const std::string& dir : plan.remove_if_empty) {
        std::error_code ec;
        if (safe(dir) && fs::is_directory(NoSlash(dir), ec) && fs::is_empty(NoSlash(dir), ec)) {
            fs::remove(NoSlash(dir), ec);
        }
    }
    return r;
}

std::string UninstallBody(bool has_save, bool delete_save) {
    if (has_save && delete_save) {
        return "Removes the game, its build files and your save data. Your ROM is kept.";
    }
    return has_save ? "Removes the game and its build files. Your ROM and save data are kept."
                    : "Removes the game and its build files. Your ROM is kept.";
}

} // namespace Homebrew::Emerald3ds
