// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <mutex>
#include <fmt/format.h>
#include <json.hpp>
#include "core/cheats/game_version.h"

namespace Cheats::Sharkive {

namespace {

u16 LoadLe16(const u8* p) {
    return static_cast<u16>(p[0] | (p[1] << 8));
}
u32 LoadLe32(const u8* p) {
    return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) | (static_cast<u32>(p[2]) << 16) |
           (static_cast<u32>(p[3]) << 24);
}
u32 LoadBe32(const u8* p) {
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) |
           (static_cast<u32>(p[2]) << 8) | static_cast<u32>(p[3]);
}
u16 LoadBe16(const u8* p) {
    return static_cast<u16>((p[0] << 8) | p[1]);
}
u64 LoadBe64(const u8* p) {
    return (static_cast<u64>(LoadBe32(p)) << 32) | LoadBe32(p + 4);
}
u64 AlignUp(u64 v, u64 a) {
    return (v + a - 1) / a * a;
}

constexpr std::size_t kNcsdMagicAt = 0x100;
constexpr std::size_t kNcsdTitleVersionAt = 0x310;
constexpr std::size_t kNcsdRevisionAt = 0x312;
constexpr std::size_t kTmdVersionAt = 0x9C;
constexpr std::size_t kTmdTitleIdAt = 0x4C;
constexpr std::size_t kCiaHeaderSize = 0x2020;

/// The signature's size by its type (the 3DS TMD, RSA 4096/2048 and ECDSA, SHA-1 and SHA-256).
std::size_t SignatureSize(u32 type) {
    switch (type) {
    case 0x10000:
    case 0x10003:
        return 0x200;
    case 0x10001:
    case 0x10004:
        return 0x100;
    case 0x10002:
    case 0x10005:
        return 0x3C;
    default:
        return 0;
    }
}

} // namespace

Version FromTitleVersion(u16 v) {
    return Version{v >> 10, (v >> 4) & 0x3F};
}

std::string DecodedTriple(u16 v) {
    return fmt::format("{}.{}.{}", v >> 10, (v >> 4) & 0x3F, v & 0xF);
}

std::optional<CardInfo> ParseNcsdCardInfo(std::span<const u8> header) {
    if (header.size() < kNcsdRevisionAt + 2 ||
        std::memcmp(header.data() + kNcsdMagicAt, "NCSD", 4) != 0) {
        return std::nullopt;
    }
    return CardInfo{LoadLe16(header.data() + kNcsdTitleVersionAt),
                    LoadLe16(header.data() + kNcsdRevisionAt)};
}

std::optional<TmdInfo> ParseTmd(std::span<const u8> tmd) {
    if (tmd.size() < 4) {
        return std::nullopt;
    }
    const std::size_t sig = SignatureSize(LoadBe32(tmd.data()));
    if (sig == 0) {
        return std::nullopt;
    }
    const std::size_t body = AlignUp(4 + sig, 0x40);
    if (tmd.size() < body + kTmdVersionAt + 2) {
        return std::nullopt;
    }
    return TmdInfo{LoadBe64(tmd.data() + body + kTmdTitleIdAt),
                   LoadBe16(tmd.data() + body + kTmdVersionAt)};
}

std::optional<Range> CiaTmdRange(std::span<const u8> header) {
    if (header.size() < 0x14 || LoadLe32(header.data()) != kCiaHeaderSize) {
        return std::nullopt;
    }
    const u64 cert_size = LoadLe32(header.data() + 0x08);
    const u64 tik_size = LoadLe32(header.data() + 0x0C);
    const u64 tmd_size = LoadLe32(header.data() + 0x10);
    if (tmd_size == 0 || tmd_size > 0x100000 || cert_size > 0x100000 || tik_size > 0x100000) {
        return std::nullopt;
    }
    const u64 cert = AlignUp(kCiaHeaderSize, 0x40);
    const u64 tik = AlignUp(cert + cert_size, 0x40);
    return Range{AlignUp(tik + tik_size, 0x40), tmd_size};
}

bool IsGameTitleId(u64 title_id) {
    const u32 type = static_cast<u32>(title_id >> 32);
    return type == 0x00040000 || type == 0x00040001 || type == 0x00040002 || type == 0x0004000E;
}

FileVersion ReadFileVersionFrom(std::span<const u8> prefix,
                                const std::function<bool(u64, u8*, u64)>& read_at) {
    FileVersion out;
    if (const auto card = ParseNcsdCardInfo(prefix)) {
        out.card_version = card->title_version;
        out.card_revision = card->revision;
        return out;
    }
    const auto range = CiaTmdRange(prefix);
    if (range && read_at) {
        std::vector<u8> tmd(range->size);
        if (read_at(range->offset, tmd.data(), tmd.size())) {
            const auto info = ParseTmd(tmd);
            if (info && IsGameTitleId(info->title_id)) {
                out.cia_tmd = info->version;
            }
        }
    }
    return out;
}

FileVersion ReadFileVersion(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::vector<u8> prefix(0x400);
    file.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
    prefix.resize(static_cast<std::size_t>(std::max<std::streamsize>(file.gcount(), 0)));
    file.clear();
    return ReadFileVersionFrom(prefix, [&file](u64 offset, u8* out, u64 size) {
        file.clear();
        file.seekg(static_cast<std::streamoff>(offset));
        file.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(size));
        return file.gcount() == static_cast<std::streamsize>(size);
    });
}

// ---- The table ----

namespace {

std::optional<u32> ParseTitleLow(const std::string& text) {
    if (text.size() != 8 && text.size() != 16) {
        return std::nullopt;
    }
    if (!std::all_of(text.begin(), text.end(),
                     [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; })) {
        return std::nullopt;
    }
    return static_cast<u32>(std::stoull(text.substr(text.size() - 8), nullptr, 16));
}

std::optional<Version> ParseShown(const std::string& text) {
    int major = 0, minor = 0;
    char dot = 0, extra = 0;
    const int n = std::sscanf(text.c_str(), "%d%c%d%c", &major, &dot, &minor, &extra);
    if (n != 3 || dot != '.' || major < 0 || minor < 0 || major > 99 || minor > 99) {
        return std::nullopt;
    }
    return Version{major, minor};
}

std::mutex g_online_mutex;
std::optional<VersionTable> g_online;

} // namespace

std::optional<VersionTable> VersionTable::Parse(std::string_view json, std::string* error,
                                                std::vector<std::string>* warnings) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json);
    } catch (const std::exception& e) {
        if (error) {
            *error = std::string("Not JSON: ") + e.what();
        }
        return std::nullopt;
    }
    const auto list = root.is_object() ? root.find("versions") : root.end();
    if (!root.is_object() || list == root.end() || !list->is_array()) {
        if (error) {
            *error = "No versions section";
        }
        return std::nullopt;
    }
    VersionTable out;
    const auto warn = [&](const std::string& why) {
        if (warnings) {
            warnings->push_back(why);
        }
    };
    for (const auto& item : *list) {
        try {
            if (!item.is_object()) {
                throw std::string("not an object");
            }
            std::vector<std::string> ids;
            if (item.contains("title_ids") && item["title_ids"].is_array()) {
                for (const auto& id : item["title_ids"]) {
                    ids.push_back(id.is_string() ? id.get<std::string>() : std::string{});
                }
            } else if (item.contains("title_id") && item["title_id"].is_string()) {
                ids.push_back(item["title_id"].get<std::string>());
            }
            if (ids.empty()) {
                throw std::string("no title_ids");
            }
            if (!item.contains("raw") || !item["raw"].is_number_unsigned() ||
                item["raw"].get<u64>() > 0xFFFF) {
                throw std::string("raw is not a u16");
            }
            const u16 raw = static_cast<u16>(item["raw"].get<u64>());
            const auto shown =
                ParseShown(item.contains("shown") && item["shown"].is_string()
                               ? item["shown"].get<std::string>()
                               : std::string{});
            if (!shown) {
                throw std::string("shown is not \"major.minor\"");
            }
            std::string source;
            if (item.contains("source") && item["source"].is_string()) {
                source = item["source"].get<std::string>().substr(0, 300);
            }
            if (source.empty()) {
                throw std::string("no source note");
            }
            for (const auto& id : ids) {
                const auto low = ParseTitleLow(id);
                if (!low) {
                    throw std::string("bad title id " + id);
                }
                VersionTable one;
                one.entries_.push_back({*low, raw, *shown, source});
                out.Merge(one);
            }
        } catch (const std::string& why) {
            warn(why);
        }
    }
    return out;
}

const TableEntry* VersionTable::Find(u64 title_id, u16 raw) const {
    const u32 low = static_cast<u32>(title_id & 0xFFFFFFFFULL);
    for (const auto& e : entries_) {
        if (e.title_low == low && e.raw == raw) {
            return &e;
        }
    }
    return nullptr;
}

void VersionTable::Merge(const VersionTable& other) {
    for (const auto& e : other.entries_) {
        const auto same = std::find_if(entries_.begin(), entries_.end(), [&](const TableEntry& x) {
            return x.title_low == e.title_low && x.raw == e.raw;
        });
        if (same != entries_.end()) {
            *same = e;
        } else {
            entries_.push_back(e);
        }
    }
}

const char* DefaultVersionsJson(); // game_version_default.cpp

const VersionTable& BuiltInVersionTable() {
    static const VersionTable table = [] {
        auto parsed = VersionTable::Parse(DefaultVersionsJson());
        return parsed ? std::move(*parsed) : VersionTable{};
    }();
    return table;
}

bool SetOnlineVersions(std::string_view catalog_json) {
    auto parsed = VersionTable::Parse(catalog_json);
    if (!parsed) {
        return false;
    }
    std::lock_guard lock{g_online_mutex};
    g_online = std::move(*parsed);
    return true;
}

void ClearOnlineVersions() {
    std::lock_guard lock{g_online_mutex};
    g_online.reset();
}

std::optional<TableEntry> LookupVersion(u64 title_id, u16 raw) {
    {
        std::lock_guard lock{g_online_mutex};
        if (g_online) {
            if (const auto* e = g_online->Find(title_id, raw)) {
                return *e;
            }
        }
    }
    if (const auto* e = BuiltInVersionTable().Find(title_id, raw)) {
        return *e;
    }
    return std::nullopt;
}

std::optional<Version> GameVersionOf(u64 title_id, u16 raw) {
    if (const auto e = LookupVersion(title_id, raw)) {
        return e->shown;
    }
    if (raw == 0) {
        return Version{1, 0}; // the title as released
    }
    return std::nullopt;
}

// ---- Putting it together ----

std::string InstalledVersion::Display() const {
    if (version) {
        return fmt::format("{}.{}", version->major, version->minor);
    }
    if (!unmapped) {
        return {};
    }
    if (source == VersionSource::CardInfo && revision) {
        return fmt::format("Rev {}", *revision);
    }
    if (source == VersionSource::UpdateTmd || source == VersionSource::BaseTmd ||
        source == VersionSource::CiaTmd) {
        return "Update Installed";
    }
    return revision ? fmt::format("Rev {}", *revision) : std::string{};
}

std::string InstalledVersion::State() const {
    const std::string d = version ? std::string{} : Display();
    if (d.empty()) {
        return {};
    }
    return d == "Update Installed" ? "has an update installed" : "is " + d;
}

std::string InstalledVersion::LogText() const {
    const char* from = "no version found";
    switch (source) {
    case VersionSource::UpdateTmd:
        from = "the update TMD";
        break;
    case VersionSource::BaseTmd:
        from = "the installed title's TMD";
        break;
    case VersionSource::CardInfo:
        from = "the cartridge's card info";
        break;
    case VersionSource::CiaTmd:
        from = "the CIA's TMD";
        break;
    case VersionSource::Ncch:
        from = "the ExHeader remaster";
        break;
    case VersionSource::Unknown:
        break;
    }
    std::string text;
    if (raw) {
        text = fmt::format("raw {} ({}) from {}", *raw, DecodedTriple(*raw), from);
    } else {
        text = from;
    }
    if (version) {
        text += fmt::format(", shows {}.{} ({})", version->major, version->minor, table_source);
    } else if (!Display().empty()) {
        text += ", shows " + Display() + " (no table entry)";
    }
    if (revision) {
        text += fmt::format(", rev {}", *revision);
    }
    return text;
}

InstalledVersion SelectInstalledVersion(u64 title_id, std::optional<u16> update_tmd,
                                        std::optional<u16> base_tmd, const FileVersion& file,
                                        std::optional<u16> ncch_remaster) {
    InstalledVersion out;
    // installed: the update's TMD, else the base's
    if (update_tmd) {
        out.raw = update_tmd;
        out.source = VersionSource::UpdateTmd;
    } else if (base_tmd) {
        out.raw = base_tmd;
        out.source = VersionSource::BaseTmd;
    }
    // the file's own: the cartridge's card info or the CIA's TMD; the higher one wins, and a tie
    // goes to the installed one
    if (file.cia_tmd || file.card_version) {
        const u16 value = file.cia_tmd ? *file.cia_tmd : *file.card_version;
        if (!out.raw || value > *out.raw) {
            out.raw = value;
            out.source = file.cia_tmd ? VersionSource::CiaTmd : VersionSource::CardInfo;
        }
    }
    if (file.card_revision) {
        out.revision = *file.card_revision;
    }
    if (!out.raw && ncch_remaster && *ncch_remaster != 0 && *ncch_remaster % 2 == 0) {
        // the last resort: a revision counter, not a version (Mario Kart 7 Rev 1 reads 2)
        out.revision = *ncch_remaster / 2;
        out.source = VersionSource::Ncch;
        return out;
    }
    if (!out.raw) {
        return out;
    }
    if (const auto entry = LookupVersion(title_id, *out.raw)) {
        out.version = entry->shown;
        out.table_source = entry->source;
    } else if (out.source == VersionSource::CardInfo && out.revision == 0) {
        // the first printing of a cartridge, no update installed: the game as released, whatever
        // its card info version says (Pokemon X reads 32 at revision 0)
        out.version = Version{1, 0};
        out.table_source = "cartridge revision 0 is the game as released";
    } else if (*out.raw == 0 && !(out.revision && *out.revision > 0)) {
        out.version = Version{1, 0};
        out.table_source = "raw version 0 is the game as released";
    } else {
        out.unmapped = true;
    }
    return out;
}

} // namespace Cheats::Sharkive
