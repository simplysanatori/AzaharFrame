// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emerald: see emerald3ds.h.

#include "core/homebrew/emerald3ds.h"
#include "core/homebrew/net.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <cryptopp/sha.h>
#include <fmt/format.h>
#include <json.hpp>
#include "common/file_util.h"
#include "common/logging/log.h"

namespace Homebrew::Emerald3ds {

namespace {

constexpr std::array<RomProfile, 6> kRoms{{
    {"BPEE", "en", "f3ae088181bf583e55daf962a92bb46f4f1d07b7"}, // USA, Europe
    {"BPES", "es", "fe1558a3dcb0360ab558969e09b690888b846dd9"}, // Spain
    {"BPEF", "fr", "ca666651374d89ca439007bed54d839eb7bd14d0"}, // France
    {"BPEI", "it", "1692db322400c3141c5de2db38469913ceb1f4d4"}, // Italy
    {"BPED", "de", "61c2eb2b380b1a75f0c94b767a2d4c26cd7ce4e3"}, // Germany
    {"BPEJ", "ja", "d7cf8f156ba9c455d164e1ea780a6bf1945465c2"}, // Japan
}};

std::string Lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string WithSlash(std::string_view dir) {
    std::string out(dir);
    if (!out.empty() && out.back() != '/') {
        out.push_back('/');
    }
    return out;
}

bool IsHex(std::string_view s, std::size_t len) {
    return s.size() == len && std::all_of(s.begin(), s.end(), [](char c) {
               return std::isxdigit(static_cast<unsigned char>(c)) != 0;
           });
}

std::string Str(const nlohmann::json& o, const char* key) {
    const auto it = o.find(key);
    return it != o.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

template <typename Hash>
std::string HexDigest(Hash& hash) {
    std::array<CryptoPP::byte, Hash::DIGESTSIZE> digest{};
    hash.Final(digest.data());
    std::string out;
    out.reserve(digest.size() * 2);
    for (const auto b : digest) {
        out += fmt::format("{:02x}", b);
    }
    return out;
}

} // namespace

std::span<const RomProfile> SupportedRoms() {
    return kRoms;
}

const RomProfile* FindRom(std::string_view sha1) {
    const std::string want = Lower(sha1);
    for (const RomProfile& rom : kRoms) {
        if (rom.sha1 == want) {
            return &rom;
        }
    }
    return nullptr;
}

std::vector<std::string> LanguagesFor(const RomProfile& rom) {
    std::vector<std::string> out{std::string(rom.language)};
    if (rom.code == "BPEE") {
        out.emplace_back("pt"); // docs/PORTUGUESE.md: built from the English ROM
    }
    return out;
}

std::string LanguageName(std::string_view language) {
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 7> kNames{{
        {"en", "English"},
        {"es", "Español"},
        {"fr", "Français"},
        {"it", "Italiano"},
        {"de", "Deutsch"},
        {"ja", "日本語"},
        {"pt", "Português (Brasil)"},
    }};
    for (const auto& [code, name] : kNames) {
        if (code == language) {
            return std::string(name);
        }
    }
    return std::string(language);
}

RomCheck CheckRom(std::span<const u8> data) {
    RomCheck out;
    if (data.empty() || data.size() > kRomSize) {
        out.error = std::string(kRomRefused);
        return out;
    }
    if (data.size() == kRomSize) {
        out.sha1 = Sha1Hex(data);
    } else {
        // A trimmed dump: the cartridge's trailing 0xFF fill cut off.
        CryptoPP::SHA1 hash;
        hash.Update(data.data(), data.size());
        const std::vector<u8> fill(kRomSize - data.size(), 0xFF);
        hash.Update(fill.data(), fill.size());
        out.sha1 = HexDigest(hash);
    }
    out.profile = FindRom(out.sha1);
    out.ok = out.profile != nullptr;
    if (!out.ok) {
        out.error = std::string(kRomRefused);
    }
    return out;
}

RomCheck CheckRomFile(const std::string& path) {
    RomCheck out;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        out.error = "The file could not be read";
        return out;
    }
    const auto size = static_cast<std::size_t>(in.tellg());
    if (size == 0 || size > kRomSize) {
        out.error = std::string(kRomRefused);
        return out;
    }
    std::vector<u8> data(size);
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) {
        out.error = "The file could not be read";
        return out;
    }
    return CheckRom(data);
}

// ---------------------------------------------------------------------------------------------

std::optional<Release> ParseRelease(std::string_view json, std::string* error) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (const std::exception&) {
        if (error) {
            *error = "The release information could not be read";
        }
        return std::nullopt;
    }
    if (!j.is_object() || Str(j, "tag_name").empty()) {
        if (error) {
            *error = Str(j, "message").empty() ? "No release found" : Str(j, "message");
        }
        return std::nullopt;
    }
    Release r;
    r.tag = Str(j, "tag_name");
    r.draft = j.value("draft", false);
    r.prerelease = j.value("prerelease", false);
    if (const auto it = j.find("assets"); it != j.end() && it->is_array()) {
        for (const auto& a : *it) {
            if (!a.is_object()) {
                continue;
            }
            Asset asset;
            asset.name = Str(a, "name");
            asset.url = Str(a, "browser_download_url");
            if (const auto s = a.find("size"); s != a.end() && s->is_number_unsigned()) {
                asset.size = s->get<u64>();
            }
            if (!asset.name.empty() && !asset.url.empty()) {
                r.assets.push_back(std::move(asset));
            }
        }
    }
    return r;
}

std::optional<std::vector<int>> ParseVersion(std::string_view tag) {
    if (!tag.empty() && (tag.front() == 'v' || tag.front() == 'V')) {
        tag.remove_prefix(1);
    }
    std::vector<int> parts;
    while (!tag.empty()) {
        const auto dot = tag.find('.');
        const std::string_view part = tag.substr(0, dot);
        if (part.empty() || part.size() > 6 || !std::all_of(part.begin(), part.end(), [](char c) {
                return std::isdigit(static_cast<unsigned char>(c)) != 0;
            })) {
            return std::nullopt;
        }
        parts.push_back(std::atoi(std::string(part).c_str()));
        if (dot == std::string_view::npos) {
            break;
        }
        tag.remove_prefix(dot + 1);
        if (tag.empty()) {
            return std::nullopt;
        }
    }
    if (parts.empty()) {
        return std::nullopt;
    }
    return parts;
}

bool IsNewer(std::string_view latest, std::string_view installed) {
    if (latest.empty()) {
        return false;
    }
    const auto a = ParseVersion(latest);
    const auto b = ParseVersion(installed);
    if (!a || !b) {
        return latest != installed;
    }
    std::vector<int> x = *a, y = *b;
    const std::size_t n = std::max(x.size(), y.size());
    x.resize(n, 0);
    y.resize(n, 0);
    return x > y;
}

std::optional<Manifest> ParseManifest(std::string_view json, std::string* error) {
    const auto fail = [error](const char* why) -> std::optional<Manifest> {
        if (error) {
            *error = why;
        }
        return std::nullopt;
    };
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (const std::exception&) {
        return fail("The release manifest could not be read");
    }
    if (!j.is_object() || Str(j, "kind") != "emerald3ds-web-payload") {
        return fail("The release manifest is not an Emerald 3DS manifest");
    }
    Manifest m;
    m.release_tag = Str(j, "releaseTag");
    if (const auto e = j.find("entrypoint"); e != j.end() && e->is_object()) {
        m.api_version = e->value("apiVersion", 0);
    }
    const auto read_variant = [](const nlohmann::json& rom, const nlohmann::json& assets,
                                 const std::string& abi) -> std::optional<Variant> {
        if (!rom.is_object() || !assets.is_object()) {
            return std::nullopt;
        }
        Variant v;
        v.language = Str(rom, "language");
        v.rom_sha1 = Lower(Str(rom, "sha1"));
        v.data_abi = Lower(abi);
        if (const auto t = assets.find("threeDsx"); t != assets.end() && t->is_object()) {
            v.three_dsx = Str(*t, "releaseAsset");
            v.three_dsx_sha256 = Lower(Str(*t, "sha256"));
        }
        if (v.language.empty() || !IsHex(v.rom_sha1, 40) || !IsHex(v.data_abi, 8)) {
            return std::nullopt;
        }
        return v;
    };
    // The top level is the default variant (English), built from supportedRoms[0].
    const auto roms = j.find("supportedRoms");
    const auto assets = j.find("assets");
    if (roms == j.end() || !roms->is_array() || roms->empty() || assets == j.end()) {
        return fail("The release manifest has no game data");
    }
    if (auto v = read_variant((*roms)[0], *assets, Str(j, "dataAbi"))) {
        m.variants.push_back(std::move(*v));
    } else {
        return fail("The release manifest has no game data");
    }
    if (const auto vs = j.find("variants"); vs != j.end() && vs->is_array()) {
        for (const auto& entry : *vs) {
            if (!entry.is_object() || !entry.contains("rom") || !entry.contains("assets")) {
                continue;
            }
            if (auto v = read_variant(entry["rom"], entry["assets"], Str(entry, "dataAbi"))) {
                m.variants.push_back(std::move(*v));
            }
        }
    }
    return m;
}

const Variant* FindVariant(const Manifest& manifest, std::string_view rom_sha1,
                           std::string_view language) {
    const std::string sha = Lower(rom_sha1);
    for (const Variant& v : manifest.variants) {
        if (v.rom_sha1 == sha && (language.empty() || v.language == language)) {
            return &v;
        }
    }
    return nullptr;
}

std::vector<std::pair<std::string, std::string>> ParseSums(std::string_view text) {
    std::vector<std::pair<std::string, std::string>> out;
    while (!text.empty()) {
        const auto nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.remove_suffix(1);
        }
        const auto sp = line.find(' ');
        if (sp == std::string_view::npos || !IsHex(line.substr(0, sp), 64)) {
            continue;
        }
        std::string_view name = line.substr(sp);
        while (!name.empty() && (name.front() == ' ' || name.front() == '*')) {
            name.remove_prefix(1);
        }
        if (!name.empty()) {
            out.emplace_back(std::string(name), Lower(line.substr(0, sp)));
        }
    }
    return out;
}

std::string SumFor(const std::vector<std::pair<std::string, std::string>>& sums,
                   std::string_view name) {
    for (const auto& [n, hash] : sums) {
        if (n == name) {
            return hash;
        }
    }
    return {};
}

UpdateKind PlanUpdate(std::string_view installed_tag, std::string_view installed_abi,
                      std::string_view latest_tag, const Variant* latest_variant) {
    if (!IsNewer(latest_tag, installed_tag)) {
        return UpdateKind::None;
    }
    if (latest_variant != nullptr && !installed_abi.empty() &&
        Lower(installed_abi) == latest_variant->data_abi && !latest_variant->three_dsx.empty()) {
        return UpdateKind::Quick;
    }
    return UpdateKind::Full;
}

std::optional<AssetPlan> SelectAssets(const Release& release, UpdateKind kind,
                                      std::string_view three_dsx_asset, std::string* error) {
    const std::string prefix = fmt::format("{}{}/", kAssetPrefix, release.tag);
    std::string missing;
    const auto find = [&](std::string_view name) -> const Asset* {
        for (const Asset& a : release.assets) {
            if (a.name == name) {
                if (a.url != prefix + a.name) {
                    missing = fmt::format("{} is not on the release's download page", name);
                    return nullptr;
                }
                return &a;
            }
        }
        if (missing.empty()) {
            missing = fmt::format("The release has no {}", name);
        }
        return nullptr;
    };
    AssetPlan plan;
    plan.manifest = find(kManifestAsset);
    plan.sums = find(kSumsAsset);
    if (kind == UpdateKind::Full) {
        plan.payload = find(kPayloadAsset);
    } else if (kind == UpdateKind::Quick) {
        plan.three_dsx = three_dsx_asset.empty() ? nullptr : find(three_dsx_asset);
        plan.smdh = find(kSmdhName);
        if (three_dsx_asset.empty() && missing.empty()) {
            missing = "The release has no game file for this language";
        }
    }
    if (!missing.empty() || release.tag.empty()) {
        if (error) {
            *error = missing.empty() ? "The release has no tag" : missing;
        }
        return std::nullopt;
    }
    return plan;
}

// ---------------------------------------------------------------------------------------------

Paths PlanPaths(std::string_view sdmc_dir, std::string_view user_dir) {
    Paths p;
    const std::string sdmc = WithSlash(sdmc_dir);
    const std::string user = WithSlash(user_dir);
    p.app_dir = sdmc + "3ds/emerald3ds/";
    p.three_dsx = p.app_dir + std::string(kThreeDsxName);
    p.smdh = p.app_dir + std::string(kSmdhName);
    p.pak = p.app_dir + std::string(kPakName);
    p.save = p.app_dir + std::string(kSaveName);
    p.backup_root = p.app_dir + "backup/";
    p.dspfirm = sdmc + "3ds/dspfirm.cdc";
    p.data_dir = user + "emerald3ds/";
    p.state_file = p.data_dir + "state.json";
    p.rom_copy = p.data_dir + "rom.gba";
    p.work_dir = p.data_dir + "work/";
    return p;
}

std::string RomDirCopy(std::string_view rom_dir) {
    return WithSlash(rom_dir) + std::string(kThreeDsxName);
}

bool IsEmeraldPath(std::string_view path) {
    const auto slash = path.find_last_of("/\\");
    const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
    return Lower(name) == Lower(kThreeDsxName);
}

std::vector<std::string> SaveFiles() {
    return {"emerald3ds.sav",         "emerald3ds.clock",    "emerald3ds.clock.tmp",
            "emerald3ds.clock.bak",   "emerald3ds.nuzlocke", "emerald3ds.nuzlocke.tmp",
            "emerald3ds.nuzlocke.bak"};
}

std::string BackupDir(std::string_view backup_root, std::string_view stamp,
                      const std::function<bool(const std::string&)>& exists) {
    const std::string root = WithSlash(backup_root);
    std::string dir = root + std::string(stamp) + "/";
    for (int n = 2; exists(dir) || exists(dir.substr(0, dir.size() - 1) + ".partial"); ++n) {
        dir = fmt::format("{}{}-{}/", root, stamp, n);
    }
    return dir;
}

RomDirChoice ChooseRomDir(std::size_t game_dirs) {
    if (game_dirs == 0) {
        return RomDirChoice::Prompt;
    }
    return game_dirs == 1 ? RomDirChoice::UseOnly : RomDirChoice::Ask;
}

std::string DefaultRomDir(std::string_view home) {
    return WithSlash(home) + "Documents/ROMs/3DS";
}

std::string DspPlaceholder() {
    std::string text =
        "Azahar for Frame: placeholder for 3ds/dspfirm.cdc.\n"
        "It holds no DSP firmware. Azahar's HLE audio does not run the firmware a\n"
        "homebrew game loads, so any file here lets games such as Emerald 3DS start\n"
        "their sound. Replace it with your own console's dump if you have one.\n";
    // Past 0x37C bytes, where HLE logs a structures hash; padded so the size is not tiny.
    text.resize(0x400, '\n');
    return text;
}

// ---------------------------------------------------------------------------------------------

std::string SerializeState(const State& s) {
    nlohmann::json j;
    j["tag"] = s.tag;
    j["language"] = s.language;
    j["rom_sha1"] = s.rom_sha1;
    j["data_abi"] = s.data_abi;
    j["rom_dir_copy"] = s.rom_dir_copy;
    j["last_check"] = s.last_check;
    j["latest_tag"] = s.latest_tag;
    return j.dump(2) + "\n";
}

std::optional<State> ParseState(std::string_view json) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (!j.is_object() || Str(j, "tag").empty()) {
        return std::nullopt;
    }
    State s;
    s.tag = Str(j, "tag");
    s.language = Str(j, "language");
    s.rom_sha1 = Str(j, "rom_sha1");
    s.data_abi = Str(j, "data_abi");
    s.rom_dir_copy = Str(j, "rom_dir_copy");
    if (const auto it = j.find("last_check"); it != j.end() && it->is_number_integer()) {
        s.last_check = it->get<s64>();
    }
    s.latest_tag = Str(j, "latest_tag");
    return s;
}

bool CheckDue(s64 last_check, s64 now) {
    return last_check <= 0 || now < last_check || now - last_check >= kCheckInterval;
}

bool UpdateAvailable(const State& state) {
    return IsNewer(state.latest_tag, state.tag);
}

} // namespace Homebrew::Emerald3ds
