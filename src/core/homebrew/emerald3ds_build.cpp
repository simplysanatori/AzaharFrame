// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cpphelpers: see emerald3ds_build.h.

#include "core/homebrew/emerald3ds_build_internal.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/homebrew/emerald3ds.h"
#include "core/homebrew/emerald3ds_fallback.h"
#include "core/homebrew/net.h"
#include "core/mods/archive.h"

#define XZ_USE_CRC64
extern "C" {
#include "core/homebrew/xz/xz.h"
}

namespace fs = std::filesystem;

namespace Homebrew::Emerald3ds::Build {

namespace {

using json = nlohmann::json;

u32 Le32(const unsigned char* p) {
    return static_cast<u32>(p[0]) | static_cast<u32>(p[1]) << 8 | static_cast<u32>(p[2]) << 16 |
           static_cast<u32>(p[3]) << 24;
}
u16 Le16(const unsigned char* p) {
    return static_cast<u16>(p[0] | p[1] << 8);
}
void Put32(std::string& s, u32 v) {
    for (int i = 0; i < 4; ++i) {
        s.push_back(static_cast<char>(v >> (8 * i)));
    }
}
void Put64(std::string& s, u64 v) {
    for (int i = 0; i < 8; ++i) {
        s.push_back(static_cast<char>(v >> (8 * i)));
    }
}
void Put16(std::string& s, u32 v) {
    s.push_back(static_cast<char>(v));
    s.push_back(static_cast<char>(v >> 8));
}

u32 Crc(std::string_view s) {
    return Mods::Crc32(s);
}

void SetError(std::string* error, std::string text) {
    if (error != nullptr) {
        *error = std::move(text);
    }
}

/// One line: whitespace runs become single spaces (what the Python script's fail() did).
std::string OneLine(const std::string& text) {
    std::string out;
    bool space = false;
    for (const char c : text) {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
            space = !out.empty();
        } else {
            if (space) {
                out.push_back(' ');
                space = false;
            }
            out.push_back(c);
        }
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// .xz

std::optional<std::string> XzDecompress(std::string_view in, std::size_t max_size,
                                        std::string* error) {
    static std::once_flag once;
    std::call_once(once, [] {
        xz_crc32_init();
        xz_crc64_init();
    });
    xz_dec* dec = xz_dec_init(XZ_DYNALLOC, 128u << 20);
    if (dec == nullptr) {
        SetError(error, "out of memory");
        return std::nullopt;
    }
    std::string out;
    xz_buf b{};
    b.in = reinterpret_cast<const uint8_t*>(in.data());
    b.in_size = in.size();
    std::size_t chunk = std::min<std::size_t>(std::max<std::size_t>(in.size() * 4, 1 << 16),
                                              std::size_t{8} << 20);
    xz_ret ret = XZ_OK;
    while (true) {
        const std::size_t at = out.size();
        if (at >= max_size) {
            // The output is full: the stream may be exactly done
            std::string probe(16, '\0');
            xz_buf p = b;
            p.out = reinterpret_cast<uint8_t*>(probe.data());
            p.out_pos = 0;
            p.out_size = probe.size();
            ret = xz_dec_run(dec, &p);
            if (ret == XZ_STREAM_END && p.out_pos == 0) {
                break;
            }
            xz_dec_end(dec);
            SetError(error, "the data is larger than allowed");
            return std::nullopt;
        }
        out.resize(at + std::min(chunk, max_size - at));
        b.out = reinterpret_cast<uint8_t*>(out.data());
        b.out_pos = at;
        b.out_size = out.size();
        ret = xz_dec_run(dec, &b);
        out.resize(b.out_pos);
        if (ret == XZ_STREAM_END) {
            break;
        }
        if (ret != XZ_OK) {
            break;
        }
        chunk = std::min(chunk * 2, std::size_t{16} << 20);
    }
    xz_dec_end(dec);
    if (ret != XZ_STREAM_END) {
        SetError(error, fmt::format("the data cannot be decompressed (xz error {})",
                                    static_cast<int>(ret)));
        return std::nullopt;
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// Recipes

std::optional<std::string> Lz77Decompress(std::string_view src, std::size_t offset) {
    if (offset + 4 > src.size() || static_cast<unsigned char>(src[offset]) != 0x10) {
        return std::nullopt;
    }
    const auto* s = reinterpret_cast<const unsigned char*>(src.data());
    const std::size_t size = s[offset + 1] | (s[offset + 2] << 8) | (s[offset + 3] << 16);
    std::string out;
    out.reserve(size);
    std::size_t pos = offset + 4;
    while (out.size() < size) {
        if (pos >= src.size()) {
            return std::nullopt;
        }
        const unsigned flags = s[pos++];
        for (int bit = 0; bit < 8; ++bit) {
            if (out.size() >= size) {
                break;
            }
            if (flags & (0x80u >> bit)) {
                if (pos + 2 > src.size()) {
                    return std::nullopt;
                }
                const unsigned a = s[pos], b = s[pos + 1];
                pos += 2;
                const std::size_t length = (a >> 4) + 3;
                const std::size_t back = ((a & 0xF) << 8 | b) + 1;
                if (back > out.size()) {
                    return std::nullopt;
                }
                for (std::size_t i = 0; i < length; ++i) {
                    out.push_back(out[out.size() - back]);
                }
            } else {
                if (pos >= src.size()) {
                    return std::nullopt;
                }
                out.push_back(src[pos++]);
            }
        }
    }
    out.resize(size);
    return out;
}

std::optional<Recipe> ParseRecipe(std::string_view data, bool with_literals, std::string* error) {
    constexpr std::string_view kMagic = "EM3DRCP1";
    constexpr std::size_t kHeader = 12;
    const auto damaged = [&](const std::string& what) {
        SetError(error, "damaged recipe: " + what);
        return std::nullopt;
    };
    const std::string_view head = data.substr(0, kHeader);
    if (head.empty() || head.substr(0, kMagic.size()) != kMagic.substr(0, head.size())) {
        SetError(error, "not a Emerald 3DS recipe");
        return std::nullopt;
    }
    if (head.size() < kHeader) {
        return damaged(fmt::format("header truncated ({} of {} bytes)", head.size(), kHeader));
    }
    const std::size_t size = Le32(reinterpret_cast<const unsigned char*>(data.data()) + 8);
    const std::string_view blob = data.substr(kHeader, size);
    if (blob.size() != size) {
        return damaged(fmt::format("metadata truncated ({} of {} bytes)", blob.size(), size));
    }
    std::string why;
    const auto raw = XzDecompress(blob, std::size_t{256} << 20, &why);
    if (!raw) {
        return damaged("metadata cannot be decompressed (" + why + ")");
    }
    json meta = json::parse(*raw, nullptr, false);
    if (meta.is_discarded()) {
        return damaged("metadata is not valid JSON");
    }
    if (!meta.is_object()) {
        return damaged("metadata is not an object");
    }
    if (!meta.contains("schema") || !meta["schema"].is_number_integer() ||
        meta["schema"].get<int>() != 1) {
        SetError(error, "unsupported recipe schema");
        return std::nullopt;
    }
    const auto need = [&](const char* key, bool required, auto pred, const char* kind) -> bool {
        if (!meta.contains(key)) {
            if (required) {
                damaged(fmt::format("metadata has no '{}'", key));
                SetError(error, fmt::format("damaged recipe: metadata has no '{}'", key));
                return false;
            }
            return true;
        }
        if (!pred(meta[key])) {
            SetError(error, fmt::format("damaged recipe: metadata '{}' is not {}", key, kind));
            return false;
        }
        return true;
    };
    const auto is_int = [](const json& v) { return v.is_number_integer(); };
    const auto is_str = [](const json& v) { return v.is_string(); };
    const auto is_list = [](const json& v) { return v.is_array(); };
    const auto is_obj = [](const json& v) { return v.is_object(); };
    const auto list_of_obj = [](const json& v) {
        return v.is_array() && std::all_of(v.begin(), v.end(),
                                           [](const json& i) { return i.is_object(); });
    };
    if (!need("engine_abi", true, is_int, "an integer") ||
        !need("rom_sha1", true, is_str, "a string") || !need("release", true, is_str, "a string") ||
        !need("entries", true, list_of_obj, "a list") || !need("bitmaps", false, is_list, "a list") ||
        !need("generated", false, list_of_obj, "a list") || !need("inputs", false, is_list, "a list") ||
        !need("vtree", false, is_obj, "an object")) {
        return std::nullopt;
    }
    Recipe r;
    r.engine_abi = static_cast<u32>(meta["engine_abi"].get<u64>());
    r.rom_sha1 = meta["rom_sha1"].get<std::string>();
    r.release = meta["release"].get<std::string>();
    r.entries = std::move(meta["entries"]);
    if (meta.contains("bitmaps")) {
        r.bitmaps = std::move(meta["bitmaps"]);
    }
    if (meta.contains("generated")) {
        r.generated = std::move(meta["generated"]);
    }
    if (meta.contains("inputs")) {
        r.inputs = std::move(meta["inputs"]);
    }
    if (with_literals) {
        const auto lit = XzDecompress(data.substr(kHeader + size), std::size_t{1} << 30, &why);
        if (!lit) {
            return damaged("literal pool cannot be decompressed (" + why + ")");
        }
        r.literals = std::move(*lit);
    }
    return r;
}

std::string RecipeRomSha1(std::string_view data) {
    const auto r = ParseRecipe(data, false, nullptr);
    return r ? r->rom_sha1 : std::string{};
}

// ---------------------------------------------------------------------------------------------
// Programs (precompiled.py)

namespace {

constexpr int kLit = 1, kZero = 2, kCopy = 3, kPaths = 4, kSrcs = 5, kTex = 6, kColor = 7,
              kPixel = 8, kGlow = 9;

struct ProgramError {
    std::string text;
};

u16 TexelOf(u32 colour, u32 alpha) {
    return static_cast<u16>((colour & 31) << 11 | (colour >> 5 & 31) << 6 | (colour >> 10 & 31) << 1 |
                            alpha);
}

u8 GlassOf(u32 value) {
    const u32 r = (value >> 11 & 31) * 255 / 31, g = (value >> 6 & 31) * 255 / 31,
              b = (value >> 1 & 31) * 255 / 31;
    return static_cast<u8>(std::min<u32>(255, 128 + (r + g + b) / 6));
}

const std::array<u8, 1 << 15>& GlassTable() {
    static const auto table = [] {
        std::array<u8, 1 << 15> t{};
        for (u32 v = 0; v < t.size(); ++v) {
            t[v] = GlassOf(v << 1);
        }
        return t;
    }();
    return table;
}

/// For each pair of texels (32 bits) of a w x h PICA200-ordered texture, the raster index of the
/// pair (precompiled.pica_pairs).
std::vector<u32> PicaPairs(u64 w, u64 h) {
    if (w == 0 || h == 0 || w % 8 || h % 8) {
        throw ProgramError{fmt::format("a PICA texture is whole 8x8 tiles, not {}x{}", w, h)};
    }
    const u64 half = w / 2;
    std::array<u32, 32> tile{};
    for (u32 m = 0; m < 32; ++m) {
        const u32 dx = (m >> 1 & 1) | (m >> 3 & 1) << 1;
        const u32 dy = (m & 1) | (m >> 2 & 1) << 1 | (m >> 4 & 1) << 2;
        tile[m] = static_cast<u32>(dy * half + dx);
    }
    std::vector<u32> perm;
    perm.reserve(h * half);
    for (u64 ty = 0; ty < h; ty += 8) {
        for (u64 tx = 0; tx < half; tx += 4) {
            const u64 base = ty * half + tx;
            for (const u32 t : tile) {
                perm.push_back(static_cast<u32>(base + t));
            }
        }
    }
    return perm;
}

class Reader {
public:
    explicit Reader(std::string_view d) : data(d) {}
    bool Done() const {
        return pos >= data.size();
    }
    std::size_t pos = 0;
    u8 U8() {
        if (pos >= data.size()) {
            throw ProgramError{"program truncated"};
        }
        return static_cast<u8>(data[pos++]);
    }
    u64 Varint() {
        u64 value = 0;
        int shift = 0;
        std::size_t p = pos;
        while (true) {
            if (p >= data.size() || shift > 56) {
                throw ProgramError{"damaged number in the program"};
            }
            const u8 b = static_cast<u8>(data[p++]);
            value |= static_cast<u64>(b & 0x7F) << shift;
            if (b < 0x80) {
                pos = p;
                return value;
            }
            shift += 7;
        }
    }
    std::string_view Take(u64 n) {
        if (n > data.size() - pos) {
            throw ProgramError{"program truncated"};
        }
        const auto out = data.substr(pos, n);
        pos += n;
        return out;
    }

private:
    std::string_view data;
};

std::string RunProgramImpl(std::string_view program, const ProgramSources& read,
                           std::size_t limit) {
    Reader r(program);
    std::string out;
    std::vector<std::string> paths;
    std::vector<std::array<u64, 3>> srcs;
    std::map<std::tuple<u64, u32, bool, u32>, std::string> rows;
    bool have_tex = false;
    std::size_t tex_at = 0, tex_total = 0;

    const auto room = [&](u64 n) {
        if (n > limit || out.size() + n > limit) {
            throw ProgramError{"the program writes past the file's size"};
        }
    };
    const auto path_of = [&](u64 id) -> const std::string& {
        if (id >= paths.size()) {
            throw ProgramError{"damaged program (IndexError: path index out of range)"};
        }
        return paths[id];
    };
    const auto file = [&](const std::string& path) -> const std::string& {
        const std::string* data = read(path);
        if (data == nullptr) {
            throw ProgramError{"missing input " + path};
        }
        return *data;
    };
    const auto palette = [&](const std::string& path) {
        const std::string& raw = file(path);
        if (raw.size() < 32) {
            throw ProgramError{fmt::format("palette {} is {} bytes", path, raw.size())};
        }
        std::array<u32, 16> p{};
        for (int i = 0; i < 16; ++i) {
            p[i] = static_cast<u8>(raw[2 * i]) | static_cast<u8>(raw[2 * i + 1]) << 8;
        }
        return p;
    };
    const auto tile_row = [&](u64 s, u32 row, bool left, u32 alpha) -> const std::string& {
        const auto key = std::make_tuple(s, row, left, alpha);
        auto it = rows.find(key);
        if (it == rows.end()) {
            if (s >= srcs.size()) {
                throw ProgramError{"damaged program (IndexError: tile source out of range)"};
            }
            const auto [tiles_id, tile, pal_id] = std::tuple(srcs[s][0], srcs[s][1], srcs[s][2]);
            const std::string& data = file(path_of(tiles_id));
            const auto pal = palette(path_of(pal_id));
            const u64 base = tile * 32 + row * 4;
            if (base + 4 > data.size()) {
                throw ProgramError{
                    fmt::format("tile {} past the end of {}", tile, path_of(tiles_id))};
            }
            std::array<u32, 8> idx{};
            for (int i = 0; i < 4; ++i) {
                const u8 b = static_cast<u8>(data[base + i]);
                idx[2 * i] = b & 15;
                idx[2 * i + 1] = b >> 4;
            }
            if (left) {
                std::reverse(idx.begin(), idx.end());
            }
            std::string got;
            for (const u32 i : idx) {
                Put16(got, TexelOf(pal[i], alpha));
            }
            it = rows.emplace(key, std::move(got)).first;
        }
        return it->second;
    };

    while (!r.Done()) {
        const u8 op = r.U8();
        if (op == kLit) {
            const u64 n = r.Varint();
            room(n);
            out += r.Take(n);
        } else if (op == kZero) {
            const u64 n = r.Varint();
            room(n);
            out.append(n, '\0');
        } else if (op == kCopy) {
            const u64 src = r.Varint(), n = r.Varint();
            room(n);
            if (src + n > out.size()) {
                throw ProgramError{"copy past what is written"};
            }
            const std::string piece = out.substr(src, n);
            out += piece;
        } else if (op == kPaths) {
            const u64 n = r.Varint();
            for (u64 i = 0; i < n; ++i) {
                const u64 len = r.Varint();
                paths.emplace_back(r.Take(len));
            }
        } else if (op == kSrcs) {
            const u64 n = r.Varint();
            for (u64 i = 0; i < n; ++i) {
                const u64 a = r.Varint(), b = r.Varint(), c = r.Varint();
                srcs.push_back({a, b, c});
            }
        } else if (op == kTex) {
            const u64 w = r.Varint(), h = r.Varint();
            const u8 order = r.U8();
            if (w != 0 && h > (std::size_t{1} << 40) / w) {
                throw ProgramError{"the program writes past the file's size"};
            }
            const u64 total = w * h;
            room(2 * total);
            std::string block;
            while (block.size() < 2 * total) {
                const u8 k = r.U8();
                const u32 kind = k & 3;
                if (kind == 0) { // skip
                    const u64 n = r.Varint();
                    if (block.size() + 2 * n > 2 * total) {
                        break;
                    }
                    block.append(2 * n, '\0');
                } else if (kind == 1) { // literal
                    const u64 n = r.Varint();
                    if (block.size() + 2 * n > 2 * total) {
                        break;
                    }
                    block += r.Take(2 * n);
                } else if (kind == 2) { // tile row run
                    const u32 n = (k >> 2 & 7) + 1;
                    const u64 s = r.Varint();
                    const u8 b = r.U8();
                    const u32 col = b >> 3 & 7;
                    if (col + n > 8) {
                        throw ProgramError{"tile run past its row"};
                    }
                    const std::string& row = tile_row(s, b & 7, (b & 0x40) != 0, (b & 0x80) ? 0 : 1);
                    block.append(row, 2 * col, 2 * n);
                } else { // palette run
                    const u64 pal_id = r.Varint();
                    const u8 b = r.U8();
                    const u64 n = r.Varint();
                    if (block.size() + 2 * n > 2 * total) {
                        break;
                    }
                    const u16 value = TexelOf(palette(path_of(pal_id))[b & 15], (b & 0x80) ? 0 : 1);
                    std::string pair;
                    Put16(pair, value);
                    block.reserve(block.size() + 2 * n);
                    for (u64 i = 0; i < n; ++i) {
                        block += pair;
                    }
                }
            }
            if (block.size() != 2 * total) {
                throw ProgramError{"texel runs overrun their block"};
            }
            if (order == 1) {
                const std::vector<u32> perm = PicaPairs(w, h);
                std::string moved(block.size(), '\0');
                for (std::size_t i = 0; i < perm.size(); ++i) {
                    std::memcpy(moved.data() + 4 * i, block.data() + 4 * static_cast<std::size_t>(perm[i]),
                                4);
                }
                block = std::move(moved);
            } else if (order != 0) {
                throw ProgramError{fmt::format("unknown texel order {}", order)};
            }
            have_tex = true;
            tex_at = out.size();
            tex_total = total;
            out += block;
        } else if (op == kGlow) {
            if (!have_tex) {
                throw ProgramError{"glass with no texels before it"};
            }
            room(tex_total);
            std::string glow;
            while (glow.size() < tex_total) {
                const u8 k = r.U8();
                const u64 n = r.Varint();
                if (glow.size() + n > tex_total) {
                    throw ProgramError{"glass runs overrun their block"};
                }
                if (k == 0) {
                    glow.append(n, '\0');
                } else if (k == 1) {
                    glow += r.Take(n);
                } else if (k == 2) {
                    const std::size_t i = tex_at + 2 * glow.size();
                    for (u64 j = 0; j < n; ++j) {
                        const u32 v = static_cast<u8>(out[i + 2 * j]) |
                                      static_cast<u8>(out[i + 2 * j + 1]) << 8;
                        glow.push_back(static_cast<char>(GlassTable()[v >> 1]));
                    }
                } else {
                    throw ProgramError{fmt::format("unknown glass run {}", k)};
                }
            }
            out += glow;
        } else if (op == kColor) {
            const u64 pal_id = r.Varint();
            const u8 index = r.U8();
            room(2);
            Put16(out, TexelOf(palette(path_of(pal_id))[index & 15], 1));
        } else if (op == kPixel) {
            const u64 tiles_id = r.Varint(), tile = r.Varint();
            const u8 pixel = r.U8(), high = r.U8();
            room(1);
            const std::string& data = file(path_of(tiles_id));
            if (tile * 32 + 32 > data.size() || pixel > 63) {
                throw ProgramError{fmt::format("pixel {} of tile {} past {}", pixel, tile,
                                               path_of(tiles_id))};
            }
            const u8 b = static_cast<u8>(data[tile * 32 + pixel / 2]);
            out.push_back(static_cast<char>(high | ((pixel & 1) ? b >> 4 : b & 15)));
        } else {
            throw ProgramError{fmt::format("unknown operation {} at {}", op, r.pos - 1)};
        }
    }
    return out;
}

} // namespace

std::optional<std::string> RunProgram(std::string_view program, const ProgramSources& read,
                                      std::size_t limit, std::string* error) {
    try {
        return RunProgramImpl(program, read, limit);
    } catch (const ProgramError& e) {
        SetError(error, e.text);
    } catch (const std::bad_alloc&) {
        SetError(error, "out of memory");
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------------------------
// Recipe entries

namespace {

bool IsUint(const json& v) {
    return v.is_number_unsigned() || (v.is_number_integer() && v.get<s64>() >= 0);
}

} // namespace

std::optional<std::string> BuildEntry(const json& entry, std::string_view rom, const Recipe& recipe,
                                      const ProgramSources* sources, std::string* error) {
    const std::string path = entry.value("path", std::string{});
    const auto fail = [&](const std::string& what) {
        SetError(error, what);
        return std::nullopt;
    };
    if (!entry.contains("ops") || !entry["ops"].is_array() || !entry.contains("size") ||
        !IsUint(entry["size"]) || !entry.contains("crc") || !IsUint(entry["crc"])) {
        return fail(path + ": damaged entry");
    }
    std::string out;
    for (const json& op : entry["ops"]) {
        if (!op.is_array() || op.empty() || !op[0].is_string()) {
            return fail(path + ": damaged operation");
        }
        const std::string kind = op[0].get<std::string>();
        const auto arg = [&](std::size_t i) -> s64 {
            return i < op.size() && op[i].is_number_integer() ? op[i].get<s64>() : -1;
        };
        if (kind == "C") {
            const s64 off = arg(1), length = arg(2);
            if (op.size() != 3 || off < 0 || length < 0 ||
                static_cast<u64>(off) + static_cast<u64>(length) > rom.size()) {
                return fail(path + ": ROM range out of bounds");
            }
            out.append(rom.substr(static_cast<std::size_t>(off), static_cast<std::size_t>(length)));
        } else if (kind == "F") {
            const s64 byte = arg(1), length = arg(2);
            if (op.size() != 3 || byte < 0 || byte > 255 || length < 0 || length > (1 << 28)) {
                return fail(path + ": damaged operation");
            }
            out.append(static_cast<std::size_t>(length), static_cast<char>(byte));
        } else if (kind == "L") {
            const s64 off = arg(1), length = arg(2);
            if (op.size() != 3 || off < 0 || length < 0) {
                return fail(path + ": damaged operation");
            }
            if (static_cast<u64>(off) < recipe.literals.size()) {
                out.append(recipe.literals, static_cast<std::size_t>(off),
                           static_cast<std::size_t>(length));
            }
        } else if (kind == "Z") {
            const s64 off = arg(1);
            if (op.size() != 2 || off < 0) {
                return fail(path + ": damaged operation");
            }
            const auto z = Lz77Decompress(rom, static_cast<std::size_t>(off));
            if (!z) {
                return fail(fmt::format("{}: no LZ77 data at {:#x}", path, off));
            }
            out += *z;
        } else if (kind == "R") {
            const s64 off = arg(1), src_stride = arg(2), dst_stride = arg(3), count = arg(4),
                      index = arg(5);
            if (op.size() != 6 || off < 0 || src_stride < 0 || dst_stride < 0 || count < 0 ||
                index < 0 || static_cast<u64>(index) >= recipe.bitmaps.size() ||
                !recipe.bitmaps[static_cast<std::size_t>(index)].is_array() ||
                static_cast<u64>(count) * static_cast<u64>(dst_stride) > (1u << 28)) {
                return fail(path + ": damaged operation");
            }
            const json& bitmap = recipe.bitmaps[static_cast<std::size_t>(index)];
            std::vector<s64> map;
            map.reserve(bitmap.size());
            for (const json& v : bitmap) {
                if (!v.is_number_integer()) {
                    return fail(path + ": damaged bitmap");
                }
                map.push_back(v.get<s64>());
            }
            for (s64 k = 0; k < count; ++k) {
                const u64 at = static_cast<u64>(off + k * src_stride);
                if (at + static_cast<u64>(src_stride) > rom.size()) {
                    return fail(path + ": ROM range out of bounds");
                }
                const auto* src = reinterpret_cast<const unsigned char*>(rom.data()) + at;
                const std::size_t base = out.size();
                out.append(static_cast<std::size_t>(dst_stride), '\0');
                for (std::size_t i = 0; i < map.size(); ++i) {
                    const s64 s = map[i];
                    int bit;
                    if (s == -1) {
                        bit = 0;
                    } else if (s == -2) {
                        bit = 1;
                    } else if (s >= 0 && (static_cast<u64>(s) >> 3) < static_cast<u64>(src_stride)) {
                        bit = (src[s >> 3] >> (s & 7)) & 1;
                    } else {
                        return fail(path + ": damaged bitmap");
                    }
                    if ((i >> 3) >= static_cast<std::size_t>(dst_stride)) {
                        return fail(path + ": damaged bitmap");
                    }
                    if (bit) {
                        out[base + (i >> 3)] |= static_cast<char>(1 << (i & 7));
                    }
                }
            }
        } else if (kind == "V") {
            const s64 off = arg(1), length = arg(2);
            if (op.size() != 3 || off < 0 || length < 0 ||
                static_cast<u64>(off) + static_cast<u64>(length) > recipe.literals.size()) {
                return fail(fmt::format("{}: damaged program reference {}", path, op.dump()));
            }
            if (sources == nullptr) {
                return fail(path + ": a program with nothing to read");
            }
            const u64 size = entry["size"].get<u64>();
            std::string why;
            const auto piece = RunProgram(
                std::string_view(recipe.literals).substr(static_cast<std::size_t>(off),
                                                         static_cast<std::size_t>(length)),
                *sources, out.size() < size ? static_cast<std::size_t>(size - out.size()) : 0, &why);
            if (!piece) {
                return fail(path + ": " + why);
            }
            out += *piece;
        } else {
            return fail(fmt::format("{}: unknown operation '{}'", path, kind));
        }
    }
    if (entry.contains("patches") && entry["patches"].is_array()) {
        for (const json& p : entry["patches"]) {
            if (!p.is_array() || p.size() != 3 || !p[0].is_number_integer() ||
                !p[1].is_number_integer() || !p[2].is_number_integer()) {
                return fail(path + ": damaged patch");
            }
            const s64 dst = p[0].get<s64>(), off = p[1].get<s64>(), length = p[2].get<s64>();
            if (dst < 0 || off < 0 || length < 0 ||
                static_cast<u64>(dst) + static_cast<u64>(length) > out.size() ||
                static_cast<u64>(off) + static_cast<u64>(length) > recipe.literals.size()) {
                return fail(path + ": damaged patch");
            }
            out.replace(static_cast<std::size_t>(dst), static_cast<std::size_t>(length),
                        recipe.literals, static_cast<std::size_t>(off),
                        static_cast<std::size_t>(length));
        }
    }
    const u64 want_size = entry["size"].get<u64>();
    const u64 want_crc = entry["crc"].get<u64>();
    const u32 crc = Crc(out);
    if (out.size() != want_size || crc != want_crc) {
        return fail(fmt::format("{} does not match the expected result (size {}/{}, CRC {:08x}/{:08x})",
                                path, out.size(), want_size, crc, static_cast<u32>(want_crc)));
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// The pack (pak.py)

u64 PakPathId(std::string_view path) {
    u64 value = 0xcbf29ce484222325ull;
    for (const char c : path) {
        value ^= static_cast<u8>(c);
        value *= 0x100000001b3ull;
    }
    return value;
}

namespace {

u32 PakEntryType(std::string_view path) {
    static constexpr std::array<std::pair<std::string_view, u32>, 8> kTypes{{
        {"graphics/", 1}, {"data/", 1}, {"generated/", 1}, {"maps/", 2},
        {"scripts/", 3}, {"sound/", 4}, {"gamedata/", 5}, {"voxel/", 6}}};
    for (const auto& [prefix, code] : kTypes) {
        if (path.substr(0, prefix.size()) == prefix) {
            return code;
        }
    }
    return 0;
}

bool PakPathOk(const std::string& path) {
    if (path.empty() || path.front() == '/' || path.find(':') != std::string::npos ||
        path.find('\\') != std::string::npos) {
        return false;
    }
    std::size_t at = 0;
    while (at <= path.size()) {
        const auto end = path.find('/', at);
        const std::string part = path.substr(at, end == std::string::npos ? end : end - at);
        if (part == "..") {
            return false;
        }
        if (end == std::string::npos) {
            break;
        }
        at = end + 1;
    }
    return true;
}

} // namespace

u32 PakEngineAbi(const std::vector<std::pair<std::string, std::pair<u64, u32>>>& items) {
    auto sorted = items;
    std::sort(sorted.begin(), sorted.end());
    std::string text;
    for (const auto& [path, sc] : sorted) {
        text += fmt::format("{}\n{}\n{:08x}\n", path, sc.first, sc.second);
    }
    const u32 value = Crc(text);
    return value != 0 ? value : 1;
}

std::string PakBytes(const std::vector<PakFile>& files, u32 abi, std::string_view rom_sha1_raw,
                     std::string* error) {
    constexpr std::size_t kHeader = 64, kEntry = 40, kAlign = 32;
    if (rom_sha1_raw.size() != 20) {
        SetError(error, "rom_sha1 must be 20 bytes");
        return {};
    }
    struct Item {
        u64 id;
        const PakFile* file;
    };
    std::vector<Item> items;
    std::map<u64, std::string> seen;
    for (const PakFile& f : files) {
        if (!PakPathOk(f.path)) {
            SetError(error, "invalid pack path: " + f.path);
            return {};
        }
        const u64 id = PakPathId(f.path);
        if (!seen.emplace(id, f.path).second) {
            SetError(error, fmt::format("id collision between {} and {}", seen[id], f.path));
            return {};
        }
        items.push_back({id, &f});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.id < b.id; });
    const std::size_t count = items.size();
    const std::size_t data_offset = (kHeader + count * kEntry + kAlign - 1) / kAlign * kAlign;
    std::string index;
    std::size_t cursor = data_offset;
    for (const Item& it : items) {
        Put64(index, it.id);
        Put32(index, PakEntryType(it.file->path));
        Put32(index, 0);
        Put64(index, cursor);
        Put32(index, static_cast<u32>(it.file->data.size()));
        Put32(index, static_cast<u32>(it.file->data.size()));
        Put32(index, Crc(it.file->data));
        Put32(index, 0);
        cursor = (cursor + it.file->data.size() + kAlign - 1) / kAlign * kAlign;
    }
    std::string head = "EM3DPAK";
    head.push_back('\0');
    Put32(head, 1);
    Put32(head, abi);
    head.append(rom_sha1_raw);
    Put32(head, static_cast<u32>(count));
    Put64(head, kHeader);
    Put64(head, data_offset);
    Put32(head, Crc(index));
    Put32(head, 0);
    head.resize(60);
    Put32(head, Crc(head));
    std::string out = head + index;
    out.resize(data_offset, '\0');
    out.reserve(cursor);
    for (const Item& it : items) {
        out += it.file->data;
        out.resize((out.size() + kAlign - 1) / kAlign * kAlign, '\0');
    }
    return out;
}

std::size_t PakVerify(std::string_view pak, std::string* error) {
    const auto bad = [&](const char* what) -> std::size_t {
        SetError(error, what);
        return 0;
    };
    if (pak.size() < 64 || pak.substr(0, 7) != "EM3DPAK") {
        return bad("not a Emerald 3DS data pack");
    }
    const auto* p = reinterpret_cast<const unsigned char*>(pak.data());
    if (Crc(pak.substr(0, 60)) != Le32(p + 60)) {
        return bad("header CRC mismatch");
    }
    const std::size_t count = Le32(p + 36);
    const u64 index_offset = Le32(p + 40) | static_cast<u64>(Le32(p + 44)) << 32;
    if (index_offset > pak.size() || count * 40 > pak.size() - index_offset ||
        Crc(pak.substr(index_offset, count * 40)) != Le32(p + 56)) {
        return bad("index CRC mismatch");
    }
    u64 last = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const unsigned char* e = p + index_offset + i * 40;
        const u64 id = Le32(e) | static_cast<u64>(Le32(e + 4)) << 32;
        if (i > 0 && id <= last) {
            return bad("index not sorted");
        }
        last = id;
        const u64 off = Le32(e + 16) | static_cast<u64>(Le32(e + 20)) << 32;
        const u64 raw = Le32(e + 28);
        if (off > pak.size() || raw > pak.size() - off || Crc(pak.substr(off, raw)) != Le32(e + 32)) {
            return bad("payload CRC mismatch");
        }
    }
    return count;
}

// ---------------------------------------------------------------------------------------------
// Followers (followers.py)

std::optional<json> ParsePythonLiteral(std::string_view text, std::string* error) {
    std::size_t pos = 0;
    const auto fail = [&](const char* what) {
        SetError(error, fmt::format("{} at {}", what, pos));
        return std::nullopt;
    };
    const auto skip = [&] {
        while (pos < text.size()) {
            if (std::isspace(static_cast<unsigned char>(text[pos]))) {
                ++pos;
            } else if (text[pos] == '#') {
                while (pos < text.size() && text[pos] != '\n') {
                    ++pos;
                }
            } else {
                break;
            }
        }
    };
    std::function<std::optional<json>(int)> value = [&](int depth) -> std::optional<json> {
        if (depth > 32) {
            return fail("too deep");
        }
        skip();
        if (pos >= text.size()) {
            return fail("unexpected end");
        }
        const char c = text[pos];
        if (c == '{' || c == '[') {
            const char close = c == '{' ? '}' : ']';
            ++pos;
            json out = c == '{' ? json::object() : json::array();
            while (true) {
                skip();
                if (pos < text.size() && text[pos] == close) {
                    ++pos;
                    return out;
                }
                auto first = value(depth + 1);
                if (!first) {
                    return std::nullopt;
                }
                if (c == '{') {
                    skip();
                    if (pos >= text.size() || text[pos] != ':' || !first->is_string()) {
                        return fail("expected ':'");
                    }
                    ++pos;
                    auto second = value(depth + 1);
                    if (!second) {
                        return std::nullopt;
                    }
                    out[first->get<std::string>()] = std::move(*second);
                } else {
                    out.push_back(std::move(*first));
                }
                skip();
                if (pos < text.size() && text[pos] == ',') {
                    ++pos;
                } else if (pos >= text.size() || text[pos] != close) {
                    return fail("expected ',' or the end of the list");
                }
            }
        }
        if (c == '\'' || c == '"') {
            ++pos;
            std::string s;
            while (pos < text.size() && text[pos] != c) {
                if (text[pos] == '\\' && pos + 1 < text.size()) {
                    ++pos;
                    switch (text[pos]) {
                    case 'n':
                        s.push_back('\n');
                        break;
                    case 't':
                        s.push_back('\t');
                        break;
                    default:
                        s.push_back(text[pos]);
                    }
                    ++pos;
                } else {
                    s.push_back(text[pos++]);
                }
            }
            if (pos >= text.size()) {
                return fail("unterminated string");
            }
            ++pos;
            return json(s);
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            const std::size_t start = pos++;
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
                ++pos;
            }
            return json(std::strtoll(std::string(text.substr(start, pos - start)).c_str(), nullptr, 10));
        }
        for (const auto& [word, v] : {std::pair<std::string_view, json>{"True", true},
                                      {"False", false},
                                      {"None", nullptr}}) {
            if (text.substr(pos, word.size()) == word) {
                pos += word.size();
                return v;
            }
        }
        return fail("unexpected text");
    };
    const auto at = text.find("SOURCES");
    if (at == std::string_view::npos) {
        SetError(error, "no SOURCES");
        return std::nullopt;
    }
    pos = at + 7;
    skip();
    if (pos >= text.size() || text[pos] != '=') {
        return fail("expected '='");
    }
    ++pos;
    return value(0);
}

namespace {

constexpr int kFrames = 8;

struct Nsbtx {
    struct Tex {
        u32 w, h, fmt, t0;
        std::string data;
    };
    std::vector<Tex> textures;
    std::vector<std::array<u16, 16>> palettes;
};

std::optional<Nsbtx> ParseNsbtx(std::string_view buf) {
    const auto* b = reinterpret_cast<const unsigned char*>(buf.data());
    const auto has = [&](u64 off, u64 n) { return off <= buf.size() && n <= buf.size() - off; };
    if (!has(0, 0x14) || buf.substr(0, 4) != "BTX0") {
        return std::nullopt;
    }
    const u64 t = Le32(b + 0x10);
    if (!has(t, 0x3C) || buf.substr(t, 4) != "TEX0") {
        return std::nullopt;
    }
    const u64 tex_info = Le16(b + t + 0x0E);
    const u64 tex_data = Le32(b + t + 0x14);
    const u64 pal_size = static_cast<u64>(Le32(b + t + 0x30)) << 3;
    const u64 pal_info = Le32(b + t + 0x34);
    const u64 pal_data = Le32(b + t + 0x38);
    // A Nitro info dictionary: the entries' bytes
    const auto dictionary = [&](u64 off) -> std::optional<std::vector<std::string_view>> {
        if (!has(off, 4)) {
            return std::nullopt;
        }
        const u64 count = b[off + 1];
        u64 p = off + 4 + 8 + 4 * count;
        if (!has(p, 4)) {
            return std::nullopt;
        }
        const u64 size = Le16(b + p);
        p += 4;
        std::vector<std::string_view> entries;
        for (u64 i = 0; i < count; ++i) {
            if (p + i * size > buf.size()) {
                entries.emplace_back();
            } else {
                entries.push_back(buf.substr(p + i * size, size));
            }
        }
        return entries;
    };
    Nsbtx out;
    const auto texd = dictionary(t + tex_info);
    if (!texd) {
        return std::nullopt;
    }
    for (const auto e : *texd) {
        if (e.size() < 4) {
            return std::nullopt;
        }
        const u32 param = Le32(reinterpret_cast<const unsigned char*>(e.data()));
        Nsbtx::Tex tex;
        tex.w = 8u << ((param >> 20) & 7);
        tex.h = 8u << ((param >> 23) & 7);
        tex.fmt = (param >> 26) & 7;
        tex.t0 = (param >> 29) & 1;
        const u64 start = t + tex_data + (static_cast<u64>(param & 0xFFFF) << 3);
        if (start < buf.size()) {
            tex.data = std::string(buf.substr(start, static_cast<u64>(tex.w) * tex.h / 2));
        }
        out.textures.push_back(std::move(tex));
    }
    const auto pald = dictionary(t + pal_info);
    if (!pald) {
        return std::nullopt;
    }
    for (const auto e : *pald) {
        if (e.size() < 2) {
            return std::nullopt;
        }
        const u64 start = t + pal_data + (static_cast<u64>(Le16(reinterpret_cast<const unsigned char*>(e.data()))) << 3);
        const u64 end = std::min<u64>(t + pal_data + pal_size, buf.size());
        if (start > end || end - start < 32) {
            return std::nullopt;
        }
        std::array<u16, 16> pal{};
        for (int i = 0; i < 16; ++i) {
            pal[i] = Le16(b + start + 2 * i) & 0x7FFF;
        }
        out.palettes.push_back(pal);
    }
    return out;
}

} // namespace

std::string ConvertNsbtx(std::string_view nsbtx, int flags, bool* ok) {
    *ok = false;
    const auto parsed = ParseNsbtx(nsbtx);
    if (!parsed || parsed->textures.size() != kFrames || parsed->palettes.size() < 2) {
        return {};
    }
    const u32 w = parsed->textures[0].w, h = parsed->textures[0].h;
    if (w > 255 || h > 255) {
        return {};
    }
    for (const auto& tex : parsed->textures) {
        if (tex.w != w || tex.h != h || tex.fmt != 3 || tex.t0 != 1 || tex.data.size() != w * h / 2) {
            return {};
        }
    }
    std::string out = "FMN1";
    out.push_back(static_cast<char>(w));
    out.push_back(static_cast<char>(h));
    out.push_back(static_cast<char>(kFrames));
    out.push_back(static_cast<char>(flags));
    for (int p = 0; p < 2; ++p) {
        for (const u16 c : parsed->palettes[p]) {
            Put16(out, c);
        }
    }
    const u32 row = w / 2;
    for (const auto& tex : parsed->textures) {
        for (u32 ty = 0; ty < h; ty += 8) {
            for (u32 tx = 0; tx < row; tx += 4) {
                for (u32 y = ty; y < ty + 8; ++y) {
                    out.append(tex.data, y * row + tx, 4);
                }
            }
        }
    }
    *ok = true;
    return out;
}

std::vector<int> ParamFlags(std::string_view csv) {
    std::vector<int> flags;
    std::size_t at = 0;
    bool first = true;
    while (at < csv.size()) {
        std::size_t end = csv.find_first_of("\r\n", at);
        const std::string_view line =
            csv.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
        if (end == std::string_view::npos) {
            at = csv.size();
        } else {
            at = end + (csv[end] == '\r' && end + 1 < csv.size() && csv[end + 1] == '\n' ? 2 : 1);
        }
        if (first) {
            first = false;
            continue;
        }
        std::string l(line);
        while (!l.empty() && std::isspace(static_cast<unsigned char>(l.back()))) {
            l.pop_back();
        }
        std::size_t lead = 0;
        while (lead < l.size() && std::isspace(static_cast<unsigned char>(l[lead]))) {
            ++lead;
        }
        l.erase(0, lead);
        std::vector<std::string> cols;
        std::size_t s = 0;
        while (true) {
            const auto c = l.find(',', s);
            cols.push_back(l.substr(s, c == std::string::npos ? c : c - s));
            if (c == std::string::npos) {
                break;
            }
            s = c + 1;
        }
        if (cols.size() >= 5) {
            flags.push_back((cols[2] == "true") | (cols[3] == "true") << 1 | (cols[4] == "true") << 2);
        }
    }
    return flags;
}

namespace {

/// UTF-8 text as code points (a bad byte becomes U+FFFD).
std::vector<u32> Codepoints(std::string_view s) {
    std::vector<u32> out;
    for (std::size_t i = 0; i < s.size();) {
        const u8 c = static_cast<u8>(s[i]);
        u32 cp = 0xFFFD;
        std::size_t n = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c >> 5) == 6 && i + 1 < s.size()) {
            cp = (c & 0x1F) << 6 | (static_cast<u8>(s[i + 1]) & 0x3F);
            n = 2;
        } else if ((c >> 4) == 14 && i + 2 < s.size()) {
            cp = (c & 0x0F) << 12 | (static_cast<u8>(s[i + 1]) & 0x3F) << 6 |
                 (static_cast<u8>(s[i + 2]) & 0x3F);
            n = 3;
        } else if ((c >> 3) == 30 && i + 3 < s.size()) {
            cp = (c & 0x07) << 18 | (static_cast<u8>(s[i + 1]) & 0x3F) << 12 |
                 (static_cast<u8>(s[i + 2]) & 0x3F) << 6 | (static_cast<u8>(s[i + 3]) & 0x3F);
            n = 4;
        }
        out.push_back(cp);
        i += n;
    }
    return out;
}

} // namespace

std::string EncodeMessage(std::string_view utf8_text, bool* ok) {
    static const std::map<u32, u8> kChars{
        {U' ', 0x00}, {0xC7, 0x04}, {0xC8, 0x05}, {0xE0, 0x16}, {0xE1, 0x17}, {0xE7, 0x19},
        {0xE8, 0x1A}, {0xE9, 0x1B}, {0xEA, 0x1C}, {0xEE, 0x20}, {0xF2, 0x22}, {0xF3, 0x23},
        {0xF4, 0x24}, {0x153, 0x25}, {0xF9, 0x26}, {0xFA, 0x27}, {0xFB, 0x28}, {0xF1, 0x29},
        {0xBF, 0x51}, {0xA1, 0x52}, {0xE2, 0x68}, {0xED, 0x6F}, {U'!', 0xAB}, {U'?', 0xAC},
        {U'.', 0xAD}, {U'-', 0xAE}, {U'\'', 0xB4}, {0x2019, 0xB4}, {U',', 0xB8}, {U':', 0xF0},
        {0xFC, 0xF6}, {0xDF, 0x15}, {0xC4, 0xF1}, {0xD6, 0xF2}, {0xDC, 0xF3}, {0xE4, 0xF4},
        {0xF6, 0xF5}};
    *ok = true;
    const std::vector<u32> t = Codepoints(utf8_text);
    std::string out;
    const auto hex4 = [&](std::size_t i) {
        for (std::size_t k = 0; k < 4; ++k) {
            const u32 c = t[i + k];
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
                return false;
            }
        }
        return true;
    };
    for (std::size_t i = 0; i < t.size();) {
        // [VAR XXXX(...)]
        if (t[i] == '[' && i + 10 < t.size() && t[i + 1] == 'V' && t[i + 2] == 'A' &&
            t[i + 3] == 'R' && t[i + 4] == ' ' && hex4(i + 5) && t[i + 9] == '(') {
            std::size_t j = i + 10;
            while (j < t.size() && t[j] != ']') {
                ++j;
            }
            if (j + 1 <= t.size() && j > i + 10 && j < t.size() && t[j - 1] == ')') {
                std::string var;
                for (std::size_t k = 0; k < 4; ++k) {
                    var.push_back(static_cast<char>(t[i + 5 + k]));
                }
                if (var == "0101") {
                    out += "\xFD\x02";
                } else if (var == "0103") {
                    out += "\xFD\x01";
                } else if (var == "0108") {
                    out += "\xFD\x03";
                } else if (var != "0200") {
                    *ok = false;
                    return {};
                }
                i = j + 1;
                continue;
            }
        }
        if (t[i] == '\\' && i + 1 < t.size()) {
            out.push_back(t[i + 1] == 'c' ? '\xFB' : '\0');
            i += 2;
            continue;
        }
        const u32 ch = t[i++];
        if (ch >= '0' && ch <= '9') {
            out.push_back(static_cast<char>(0xA1 + ch - '0'));
        } else if (ch >= 'A' && ch <= 'Z') {
            out.push_back(static_cast<char>(0xBB + ch - 'A'));
        } else if (ch >= 'a' && ch <= 'z') {
            out.push_back(static_cast<char>(0xD5 + ch - 'a'));
        } else if (const auto it = kChars.find(ch); it != kChars.end()) {
            out.push_back(static_cast<char>(it->second));
        } else {
            out.push_back('\xAC');
        }
    }
    // No space next to a page break, nor two in a row; no space at either end.
    std::string a;
    for (std::size_t i = 0; i < out.size();) {
        if (out[i] == '\0' || out[i] == '\xFB') {
            std::size_t j = i;
            while (j < out.size() && out[j] == '\0') {
                ++j;
            }
            if (j < out.size() && out[j] == '\xFB') {
                ++j;
                while (j < out.size() && out[j] == '\0') {
                    ++j;
                }
                a.push_back('\xFB');
                i = j;
                continue;
            }
        }
        a.push_back(out[i++]);
    }
    std::string b;
    for (std::size_t i = 0; i < a.size(); ++i) {
        b.push_back(a[i]);
        if (a[i] == '\0') {
            while (i + 1 < a.size() && a[i + 1] == '\0') {
                ++i;
            }
        }
    }
    std::size_t lo = 0, hi = b.size();
    while (lo < hi && b[lo] == '\0') {
        ++lo;
    }
    while (hi > lo && b[hi - 1] == '\0') {
        --hi;
    }
    return b.substr(lo, hi - lo) + '\xFF';
}

std::string ConvertTexts(std::string_view corpus, bool* ok) {
    *ok = false;
    std::vector<std::string_view> lines;
    std::string flat;
    // "\r\n" -> "\n", then split on "\n" only
    flat.reserve(corpus.size());
    for (std::size_t i = 0; i < corpus.size(); ++i) {
        if (corpus[i] == '\r' && i + 1 < corpus.size() && corpus[i + 1] == '\n') {
            continue;
        }
        flat.push_back(corpus[i]);
    }
    for (std::size_t at = 0;;) {
        const auto end = flat.find('\n', at);
        lines.emplace_back(std::string_view(flat).substr(at, end == std::string::npos ? end : end - at));
        if (end == std::string::npos) {
            break;
        }
        at = end + 1;
    }
    constexpr std::size_t kCount = 759;
    std::size_t start = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines[i] == "Text File : 265") {
            start = i + 2;
            break;
        }
    }
    if (start >= lines.size() + 0 && start > lines.size()) {
        return {};
    }
    std::size_t end = start;
    while (end < lines.size() && lines[end].substr(0, 3) != "~~~") {
        ++end;
    }
    if (start > lines.size() || end - start != kCount) {
        return {};
    }
    std::vector<std::string> blobs;
    for (std::size_t i = start; i < end; ++i) {
        bool good = false;
        blobs.push_back(EncodeMessage(lines[i], &good));
        if (!good) {
            return {};
        }
    }
    std::string out = "FMT1";
    Put16(out, static_cast<u32>(blobs.size()));
    Put16(out, 0);
    u32 at = static_cast<u32>(8 + 4 * blobs.size());
    for (const auto& b : blobs) {
        Put32(out, at);
        at += static_cast<u32>(b.size());
    }
    for (const auto& b : blobs) {
        out += b;
    }
    *ok = true;
    return out;
}

namespace {

/// Gets each listed file from the cache or the network, keeping only what matches its pinned size
/// and SHA-256.
class Fetcher {
public:
    Fetcher(const json& sources, std::string cache, bool network, const std::atomic<bool>* cancel)
        : sources(sources), cache(std::move(cache)), network(network), cancel(cancel) {}

    std::optional<std::string> Get(const FollowerFile& entry) {
        if (!cache.empty()) {
            std::ifstream in(cache + "/" + entry.sha256, std::ios::binary);
            if (in) {
                std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                if (Check(entry, data)) {
                    return data;
                }
            }
        }
        if (!network) {
            return std::nullopt;
        }
        const json& src = sources["sources"][entry.source];
        const std::string repo = src.value("repo", std::string{}),
                          commit = src.value("commit", std::string{});
        const std::array<std::string, 2> urls{
            fmt::format("https://raw.githubusercontent.com/{}/{}/{}", repo, commit, entry.path),
            fmt::format("https://cdn.jsdelivr.net/gh/{}@{}/{}", repo, commit, entry.path)};
        for (const std::string& url : urls) {
            if (cancel != nullptr && cancel->load()) {
                return std::nullopt;
            }
            const Reply r = GetPinned(url, entry.size + 1);
            if (!r.error.empty() || r.status != 200 || !Check(entry, r.body)) {
                continue;
            }
            downloaded.fetch_add(1);
            if (!cache.empty()) {
                std::error_code ec;
                fs::create_directories(cache, ec);
                const std::string tmp = cache + "/" + entry.sha256 + ".tmp" +
                                        std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
                {
                    std::ofstream out(tmp, std::ios::binary);
                    out.write(r.body.data(), static_cast<std::streamsize>(r.body.size()));
                }
                fs::rename(tmp, cache + "/" + entry.sha256, ec);
                if (ec) {
                    fs::remove(tmp, ec);
                }
            }
            return r.body;
        }
        return std::nullopt;
    }
    std::atomic<int> downloaded{0};

private:
    static bool Check(const FollowerFile& e, const std::string& data) {
        return data.size() == e.size && Sha256Hex(data) == e.sha256;
    }
    const json& sources;
    std::string cache;
    bool network;
    const std::atomic<bool>* cancel;
};

FollowerFile FileOf(const json& j) {
    FollowerFile f;
    f.source = j.value("source", std::string{});
    f.path = j.value("path", std::string{});
    f.sha256 = j.value("sha256", std::string{});
    f.language = j.value("language", std::string{});
    f.key = j.value("key", std::string{});
    f.size = j.value("size", u64{0});
    f.follower_index = j.value("follower_index", 0);
    return f;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// The whole build

std::string Result::ToJson() const {
    return json{{"abi", abi},
                {"release", release},
                {"language", language},
                {"variant_matches", variant_matches},
                {"entries", entries},
                {"bytes", bytes}}
        .dump();
}

namespace {

constexpr std::string_view kRecipeName = "emerald3ds.recipe";
constexpr std::array<std::string_view, 2> kExecutables{"Emerald3DS.3dsx", "Emerald3DS.smdh"};

std::string LanguageCode(std::string language) {
    for (char& c : language) {
        c = c == '-' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const auto first = language.find_first_not_of(" \t");
    language = first == std::string::npos ? std::string{} : language.substr(first);
    return language.substr(0, language.find('_'));
}

std::string LanguageNameEn(const std::string& code) {
    static const std::map<std::string, std::string> names{
        {"en", "English"}, {"es", "Spanish"}, {"fr", "French"}, {"it", "Italian"},
        {"de", "German"},  {"ja", "Japanese"}, {"pt", "Brazilian Portuguese"}};
    const auto it = names.find(code);
    return it == names.end() ? "'" + code + "'" : it->second;
}

/// The wanted files of the payload zip, by name.
class PayloadZip {
public:
    bool Open(const std::string& path, std::string* error) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            SetError(error, "The Emerald 3DS download could not be read.");
            return false;
        }
        in.seekg(0, std::ios::end);
        const auto size = static_cast<std::size_t>(in.tellg());
        in.seekg(0);
        data.resize(size);
        if (!in.read(data.data(), static_cast<std::streamsize>(size))) {
            SetError(error, "The Emerald 3DS download could not be read.");
            return false;
        }
        // End of central directory
        const auto* p = reinterpret_cast<const unsigned char*>(data.data());
        std::size_t eocd = std::string::npos;
        for (std::size_t i = size >= 22 ? size - 22 : 0, n = 0; n < 70000 && i != std::string::npos; ++n) {
            if (i + 22 <= size && Le32(p + i) == 0x06054b50) {
                eocd = i;
                break;
            }
            if (i == 0) {
                break;
            }
            --i;
        }
        if (eocd == std::string::npos) {
            SetError(error, "The Emerald 3DS download could not be read.");
            return false;
        }
        const std::size_t count = Le16(p + eocd + 10);
        std::size_t at = Le32(p + eocd + 16);
        for (std::size_t i = 0; i < count; ++i) {
            if (at + 46 > size || Le32(p + at) != 0x02014b50) {
                SetError(error, "The Emerald 3DS download could not be read.");
                return false;
            }
            Entry e;
            e.method = Le16(p + at + 10);
            e.crc = Le32(p + at + 16);
            e.csize = Le32(p + at + 20);
            e.usize = Le32(p + at + 24);
            const std::size_t nlen = Le16(p + at + 28), xlen = Le16(p + at + 30),
                              clen = Le16(p + at + 32);
            e.local = Le32(p + at + 42);
            if (at + 46 + nlen > size) {
                SetError(error, "The Emerald 3DS download could not be read.");
                return false;
            }
            e.name = data.substr(at + 46, nlen);
            at += 46 + nlen + xlen + clen;
            // Names that leave the folder are refused, wanted or not.
            if (UnsafeName(e.name)) {
                SetError(error, "The Emerald 3DS download holds an unsafe file name.");
                return false;
            }
            if (!e.name.empty() && e.name.back() != '/') {
                entries[e.name] = e;
            }
        }
        return true;
    }
    bool Has(const std::string& name) const {
        return entries.count(name) != 0;
    }
    std::vector<std::string> Names() const {
        std::vector<std::string> out;
        for (const auto& [name, e] : entries) {
            out.push_back(name);
        }
        return out;
    }
    std::optional<std::string> Read(const std::string& name) const {
        const auto it = entries.find(name);
        if (it == entries.end()) {
            return std::nullopt;
        }
        const Entry& e = it->second;
        const auto* p = reinterpret_cast<const unsigned char*>(data.data());
        if (e.local + 30 > data.size() || Le32(p + e.local) != 0x04034b50) {
            return std::nullopt;
        }
        const std::size_t start = e.local + 30 + Le16(p + e.local + 26) + Le16(p + e.local + 28);
        if (start > data.size() || e.csize > data.size() - start) {
            return std::nullopt;
        }
        const std::string_view raw = std::string_view(data).substr(start, e.csize);
        std::optional<std::string> out;
        if (e.method == 0) {
            out = std::string(raw);
        } else if (e.method == 8) {
            out = Mods::Inflate(raw, e.usize);
        }
        if (!out || out->size() != e.usize || Crc(*out) != e.crc) {
            return std::nullopt;
        }
        return out;
    }

private:
    struct Entry {
        u32 method = 0, crc = 0, csize = 0, usize = 0, local = 0;
        std::string name;
    };
    static bool UnsafeName(const std::string& name) {
        if (name.empty() || name.front() == '/' || name.find('\\') != std::string::npos ||
            name.find(':') != std::string::npos) {
            return true;
        }
        std::size_t at = 0;
        while (at <= name.size()) {
            const auto end = name.find('/', at);
            if (name.substr(at, end == std::string::npos ? end : end - at) == "..") {
                return true;
            }
            if (end == std::string::npos) {
                break;
            }
            at = end + 1;
        }
        return false;
    }
    std::string data;
    std::map<std::string, Entry> entries;
};

struct Variant {
    std::string dir;       ///< "" = payload/ itself, else "es"
    std::string rom_sha1;  ///< what its recipe is made from
    std::string Prefix() const {
        return dir.empty() ? "payload/" : "payload/" + dir + "/";
    }
    std::string Language() const {
        return dir.empty() ? "en" : LanguageCode(dir);
    }
};

class Progress {
public:
    explicit Progress(const Options& o) : options(o) {}
    void operator()(double fraction, const std::string& message) const {
        if (options.progress) {
            options.progress(static_cast<int>(std::clamp(fraction, 0.0, 1.0) * 1000), message);
        }
    }
    bool Stopped() const {
        return options.cancel != nullptr && options.cancel->load();
    }

private:
    const Options& options;
};

struct Stop {};

} // namespace

Result BuildPack(const Options& options) {
    Result res;
    const Progress report(options);
    const auto fail = [&](const std::string& text) {
        res.ok = false;
        res.error = OneLine(text);
        return res;
    };
    const auto check_stop = [&] {
        if (report.Stopped()) {
            throw Stop{};
        }
    };
    // A recipe this builder cannot follow: a result of its own, with the reason (AzaharFrame emfallback).
    const auto unsupported = [&](const std::string& text) {
        res.unsupported = true;
        return fail(text);
    };
    const auto is_unknown_op = [](const std::string& why) {
        return why.find("unknown operation") != std::string::npos;
    };
    try {
        report(0.0, "Unpacking the builder");
        std::error_code ec;
        fs::create_directories(options.out_dir, ec);
        PayloadZip zip;
        std::string why;
        if (!zip.Open(options.payload_zip, &why)) {
            return fail(why);
        }
        const auto names = zip.Names();
        const auto has_prefix = [&](const std::string& prefix) {
            return std::any_of(names.begin(), names.end(), [&](const std::string& n) {
                return n.compare(0, prefix.size(), prefix) == 0;
            });
        };
        if (!has_prefix("payload/")) {
            return fail("The Emerald 3DS download has no game data in it.");
        }

        // The ROM
        std::string rom;
        {
            std::ifstream in(options.rom, std::ios::binary);
            if (!in) {
                return fail("The ROM file was not found: " +
                            fs::path(options.rom).filename().string());
            }
            rom.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        }
        if (rom.size() > kRomSize) {
            return fail("This file is larger than a GBA cartridge; it is not the supported ROM.");
        }
        if (rom.size() < kRomSize) {
            rom.resize(kRomSize, '\xFF'); // a trimmed dump: the cartridge's trailing 0xFF fill
        }
        const RomCheck check = CheckRom(std::span<const u8>(reinterpret_cast<const u8*>(rom.data()), rom.size()));
        if (!check.ok) {
            return fail("This file is not the supported ROM.");
        }
        const std::string rom_sha1 = check.sha1;
        std::string rom_title(rom.substr(0xA0, 12));
        rom_title = rom_title.substr(0, rom_title.find('\0'));
        const std::string rom_code = rom.substr(0xAC, 4);

        // select_payload: the variants whose recipe was made from this ROM
        std::vector<Variant> variants;
        if (zip.Has("payload/" + std::string(kRecipeName))) {
            variants.push_back({"", ""});
        } else {
            variants.push_back({"", ""}); // the default always counts; it is checked below
        }
        {
            std::set<std::string> dirs;
            for (const std::string& n : names) {
                if (n.compare(0, 8, "payload/") != 0) {
                    continue;
                }
                const auto slash = n.find('/', 8);
                if (slash != std::string::npos && n.substr(slash + 1) == kRecipeName) {
                    dirs.insert(n.substr(8, slash - 8));
                }
            }
            for (const auto& d : dirs) {
                variants.push_back({d, ""});
            }
        }
        std::vector<Variant> matching;
        for (Variant& v : variants) {
            const auto bytes = zip.Read(v.Prefix() + std::string(kRecipeName));
            if (!bytes) {
                continue;
            }
            v.rom_sha1 = RecipeRomSha1(*bytes);
            if (v.rom_sha1 == rom_sha1) {
                matching.push_back(v);
            }
        }
        std::string language = options.language;
        if (!language.empty() && !matching.empty()) {
            const std::string want = LanguageCode(language);
            if (std::none_of(matching.begin(), matching.end(),
                             [&](const Variant& v) { return v.Language() == want; })) {
                std::string builds;
                for (const Variant& v : matching) {
                    builds += (builds.empty() ? "" : " and ") + LanguageNameEn(v.Language());
                }
                return fail(fmt::format(
                    "This release has no {} version made from this ROM. It builds {} from it.",
                    LanguageNameEn(want), builds));
            }
        }
        Variant variant = variants.front();
        {
            std::vector<Variant> pick = matching;
            if (!language.empty()) {
                const std::string want = LanguageCode(language);
                pick.erase(std::remove_if(pick.begin(), pick.end(),
                                          [&](const Variant& v) { return v.Language() != want; }),
                           pick.end());
            }
            if (!pick.empty()) {
                variant = pick.front();
            }
        }
        // The variant must hold its recipe and executables
        {
            std::vector<std::string> missing;
            const std::string rel = variant.dir.empty() ? "" : variant.dir + "/";
            if (!zip.Has(variant.Prefix() + std::string(kRecipeName))) {
                missing.push_back(rel + std::string(kRecipeName));
            }
            if (!variant.rom_sha1.empty() && variant.rom_sha1 == rom_sha1) {
                for (const auto name : kExecutables) {
                    if (!zip.Has(variant.Prefix() + std::string(name))) {
                        missing.push_back(rel + std::string(name));
                    }
                }
            }
            if (!missing.empty()) {
                std::string list;
                for (const auto& m : missing) {
                    list += (list.empty() ? "" : ", ") + m;
                }
                if (variant.dir.empty()) {
                    return fail("This release is incomplete (" + list +
                                " missing next to the builder). Extract the whole ZIP again and run "
                                "the builder from there.");
                }
                return fail("This release is incomplete for the '" + variant.dir +
                            "' language of your ROM (" + list +
                            " missing from payload next to the builder). Extract the whole ZIP "
                            "again, including the payload/" + variant.dir +
                            " folder, and run the builder from there.");
            }
        }

        // build_pack
        report(0.0, "Checking the ROM");
        check_stop();
        const auto recipe_bytes = zip.Read(variant.Prefix() + std::string(kRecipeName));
        std::optional<Recipe> recipe;
        if (recipe_bytes) {
            recipe = ParseRecipe(*recipe_bytes, true, &why);
        } else {
            why = "the recipe could not be read from the download";
        }
        if (!recipe) {
            if (why.find("unsupported recipe schema") != std::string::npos) {
                return unsupported("This release's recipe uses a newer format (" + why + ").");
            }
            return fail("The release's recipe could not be read. " + why);
        }
        if (recipe->rom_sha1 != rom_sha1) {
            return fail(fmt::format(
                "This release has no game data for {} ({}). Each language needs its own executable "
                "and recipe in the release; this one does not include them for that ROM.",
                rom_title, rom_code));
        }

        std::map<std::string, std::string> files;
        const std::size_t total = recipe->entries.size();
        for (std::size_t i = 0; i < total; ++i) {
            const json& entry = recipe->entries[i];
            std::string why2;
            auto data = BuildEntry(entry, rom, *recipe, nullptr, &why2);
            if (!data) {
                if (is_unknown_op(why2)) {
                    return unsupported("This release's recipe has a step this builder does not know (" +
                                       why2 + ").");
                }
                return fail("A game data file could not be rebuilt from the ROM. " + why2);
            }
            files[entry["path"].get<std::string>()] = std::move(*data);
            if (i % 100 == 0) {
                report(0.05 + 0.45 * static_cast<double>(i) / static_cast<double>(std::max<std::size_t>(total, 1)),
                       "Extracting game data");
                check_stop();
            }
        }

        // The voxel data, from the release's programs
        std::vector<const json*> programs;
        for (const json& g : recipe->generated) {
            if (g.contains("ops") && g["ops"].is_array() && !g["ops"].empty()) {
                programs.push_back(&g);
            } else {
                return unsupported(
                    "This release needs its 3D scenery generators, which this builder does not run.");
            }
        }
        if (!recipe->generated.empty()) {
            report(0.5, "Preparing the 3D scenery inputs");
        }
        {
            std::map<std::string, const json*> by_path;
            for (const json& in : recipe->inputs) {
                if (in.is_object() && in.contains("path")) {
                    by_path[in["path"].get<std::string>()] = &in;
                }
            }
            std::map<std::string, std::string> cache;
            std::string input_error;
            const ProgramSources read = [&](const std::string& path) -> const std::string* {
                if (const auto it = cache.find(path); it != cache.end()) {
                    return &it->second;
                }
                const auto it = by_path.find(path);
                if (it == by_path.end()) {
                    return nullptr;
                }
                auto data = BuildEntry(*it->second, rom, *recipe, nullptr, &input_error);
                if (!data) {
                    return nullptr;
                }
                return &cache.emplace(path, std::move(*data)).first->second;
            };
            // in the generators' order, labelled as they are
            static const std::vector<std::pair<std::string, std::string>> kSteps{
                {"voxel/regions.bin", "gen_voxel_regions.py"},
                {"voxel/signposts.bin", "gen_voxel_sign_masks.py"},
                {"voxel/buildings.bin", "gen_voxel_buildings.py"},
                {"voxel/relief.bin", "gen_voxel_relief.py"},
                {"stage/leaves.bin", "gen_intro_margins.py"}};
            static const std::map<std::string, std::string> kLabels{
                {"gen_voxel_regions.py", "map regions"},     {"gen_voxel_sign_masks.py", "signposts"},
                {"gen_voxel_relief.py", "terrain relief"},   {"gen_voxel_buildings.py", "buildings"},
                {"gen_intro_margins.py", "intro scene"}};
            const auto order_of = [&](const std::string& path) {
                for (std::size_t i = 0; i < kSteps.size(); ++i) {
                    if (kSteps[i].first == path) {
                        return i;
                    }
                }
                return kSteps.size();
            };
            std::stable_sort(programs.begin(), programs.end(), [&](const json* a, const json* b) {
                return order_of((*a)["path"].get<std::string>()) < order_of((*b)["path"].get<std::string>());
            });
            for (std::size_t i = 0; i < programs.size(); ++i) {
                const json& entry = *programs[i];
                const std::string path = entry["path"].get<std::string>();
                const std::size_t o = order_of(path);
                const std::string script = o < kSteps.size() ? kSteps[o].second : path;
                const auto label_it = kLabels.find(script);
                const std::string label = fmt::format("{} ({}/{})",
                                                      label_it != kLabels.end() ? label_it->second : path,
                                                      i + 1, programs.size());
                const double n = static_cast<double>(programs.size());
                report(0.6 + 0.3 * static_cast<double>(i) / n, "Generating the 3D scenery: " + label);
                check_stop();
                const auto t0 = std::chrono::steady_clock::now();
                std::string why2;
                auto data = BuildEntry(entry, rom, *recipe, &read, &why2);
                if (!data) {
                    const std::string full = input_error.empty() ? why2 : input_error + " " + why2;
                    if (is_unknown_op(full)) {
                        return unsupported("This release's 3D scenery programs have a step this builder "
                                           "does not know (" + full + ").");
                    }
                    return fail("A voxel data file could not be rebuilt from the ROM. " + full);
                }
                files[path] = std::move(*data);
                report(0.6 + 0.3 * static_cast<double>(i + 1) / n,
                       fmt::format("Generating the 3D scenery: {}, done in {:.1f} s", label,
                                   std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count()));
            }
        }

        std::vector<std::pair<std::string, std::pair<u64, u32>>> items;
        for (const auto& [path, data] : files) {
            items.push_back({path, {data.size(), Crc(data)}});
        }
        const u32 abi = PakEngineAbi(items);
        if (abi != recipe->engine_abi) {
            return unsupported(fmt::format(
                "The data this builder made does not match this release (ABI {:08x}, expected {:08x}).",
                abi, recipe->engine_abi));
        }

        // The optional follower sprites and texts
        std::map<std::string, std::string> optional;
        if (!options.no_network || !options.cache_dir.empty()) {
            // (--no-network: no follower section at all, as the script did)
        }
        if (!options.no_network) {
            report(0.9, "Fetching the follower sprites");
            check_stop();
            std::string cache = options.cache_dir.empty() ? options.out_dir + "/followers-cache" : options.cache_dir;
            std::string src_text, src_error;
            const auto src_zip = zip.Read("python/emerald3ds_builder/followers_sources.py");
            json sources;
            bool have_sources = false;
            if (src_zip) {
                if (auto parsed = ParsePythonLiteral(*src_zip, &src_error); parsed && parsed->is_object() &&
                    parsed->contains("files") && parsed->contains("sources")) {
                    sources = std::move(*parsed);
                    have_sources = true;
                }
            }
            if (!have_sources) {
                LOG_WARNING(Frontend, "emerald3ds: no follower sources in the download ({})", src_error);
            } else {
                Fetcher fetcher(sources, cache, true, options.cancel);
                std::vector<FollowerFile> sprites, texts;
                static const std::map<char, std::string> kRomLanguages{
                    {'E', "en"}, {'S', "es"}, {'F', "fr"}, {'I', "it"}, {'D', "de"}};
                const auto lang_it = rom_code.size() == 4 ? kRomLanguages.find(rom_code[3]) : kRomLanguages.end();
                const std::string text_language = lang_it == kRomLanguages.end() ? "en" : lang_it->second;
                std::optional<FollowerFile> params;
                for (const json& f : sources["files"]) {
                    const FollowerFile ff = FileOf(f);
                    if (!ff.key.empty()) {
                        sprites.push_back(ff);
                    }
                    if (ff.language == text_language) {
                        texts.push_back(ff);
                    }
                    if (!params && ff.path.size() >= 12 &&
                        ff.path.compare(ff.path.size() - 12, 12, "tp_param.csv") == 0) {
                        params = ff;
                    }
                }
                std::map<std::string, std::string> section;
                std::size_t missing = 0, text_count = 0;
                if (params) {
                    std::vector<int> flags;
                    if (const auto csv = fetcher.Get(*params)) {
                        flags = ParamFlags(*csv);
                    }
                    std::vector<std::optional<std::string>> converted(sprites.size());
                    std::atomic<std::size_t> next{0}, done{0};
                    const auto worker = [&] {
                        while (true) {
                            const std::size_t i = next.fetch_add(1);
                            if (i >= sprites.size() || report.Stopped()) {
                                return;
                            }
                            const auto data = fetcher.Get(sprites[i]);
                            if (data) {
                                bool ok = false;
                                const std::size_t index = static_cast<std::size_t>(sprites[i].follower_index);
                                std::string bin = ConvertNsbtx(*data, index < flags.size() ? flags[index] : 0, &ok);
                                if (ok) {
                                    converted[i] = std::move(bin);
                                }
                            }
                            done.fetch_add(1);
                        }
                    };
                    std::vector<std::thread> pool;
                    for (int i = 0; i < 8; ++i) {
                        pool.emplace_back(worker);
                    }
                    std::size_t shown = 0;
                    while (done.load() < sprites.size() && !report.Stopped()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(40));
                        if (done.load() / 20 != shown) {
                            shown = done.load() / 20;
                            report(0.9 + 0.02 * static_cast<double>(done.load()) / static_cast<double>(sprites.size()),
                                   "Fetching the follower sprites: " + std::to_string(done.load()) + "/" +
                                       std::to_string(sprites.size()));
                        }
                    }
                    for (auto& t : pool) {
                        t.join();
                    }
                    check_stop();
                    for (std::size_t i = 0; i < sprites.size(); ++i) {
                        if (converted[i]) {
                            section["followers/mon/" + sprites[i].key + ".bin"] = std::move(*converted[i]);
                        } else {
                            ++missing;
                        }
                    }
                    for (const FollowerFile& t : texts) {
                        const auto data = fetcher.Get(t);
                        bool ok = false;
                        std::string bin;
                        if (data) {
                            bin = ConvertTexts(*data, &ok);
                        }
                        if (ok) {
                            section["followers/text/" + t.language + ".bin"] = std::move(bin);
                            ++text_count;
                        } else {
                            ++missing;
                        }
                    }
                    if (!section.empty()) {
                        std::string stamp;
                        for (const auto& [name, s] : sources["sources"].items()) {
                            stamp += fmt::format("{} {} {}\n", name, s.value("repo", std::string{}),
                                                 s.value("commit", std::string{}));
                        }
                        section["followers/sources.txt"] = stamp;
                    }
                }
                LOG_INFO(Frontend, "emerald3ds: followers: {} files, {} texts, {} missing, {} downloaded",
                         section.size(), text_count, missing, fetcher.downloaded.load());
                optional = std::move(section);
            }
        }
        check_stop();

        report(0.92, "Writing the data pack");
        std::vector<PakFile> pak_files;
        for (auto& [path, data] : files) {
            pak_files.push_back({path, std::move(data)});
        }
        for (auto& [path, data] : optional) {
            // {**files, **optional}: the optional file replaces one of the same path
            const auto it = std::find_if(pak_files.begin(), pak_files.end(),
                                         [&, p = path](const PakFile& f) { return f.path == p; });
            if (it != pak_files.end()) {
                it->data = std::move(data);
            } else {
                pak_files.push_back({path, std::move(data)});
            }
        }
        std::string pak_error;
        const std::string pak = PakBytes(pak_files, abi, [&] {
            std::string raw;
            for (std::size_t i = 0; i + 1 < rom_sha1.size(); i += 2) {
                raw.push_back(static_cast<char>(std::stoi(rom_sha1.substr(i, 2), nullptr, 16)));
            }
            return raw;
        }(), &pak_error);
        if (pak.empty()) {
            return fail("The data pack could not be written. " + pak_error);
        }
        const std::string out_pak = options.out_dir + "/emerald3ds.pak";
        {
            const std::string tmp = out_pak + ".tmp";
            {
                std::ofstream out(tmp, std::ios::binary);
                out.write(pak.data(), static_cast<std::streamsize>(pak.size()));
                if (!out) {
                    std::remove(tmp.c_str());
                    return fail("Not enough free space (or the disk is read-only).");
                }
            }
            fs::rename(tmp, out_pak, ec);
            if (ec) {
                return fail("The data pack could not be written.");
            }
        }
        report(0.96, "Verifying the data pack");
        const std::size_t verified = PakVerify(pak, &pak_error);
        if (verified != pak_files.size()) {
            return fail("The data pack failed its own check. " + pak_error);
        }
        for (const auto name : kExecutables) {
            const auto data = zip.Read(variant.Prefix() + std::string(name));
            if (!data) {
                return fail("The Emerald 3DS download is damaged (" + std::string(name) + ").");
            }
            std::ofstream out(options.out_dir + "/" + std::string(name), std::ios::binary);
            out.write(data->data(), static_cast<std::streamsize>(data->size()));
            if (!out) {
                return fail("Not enough free space (or the disk is read-only).");
            }
        }
        report(1.0, "Done");
        res.ok = true;
        res.abi = fmt::format("{:08x}", abi);
        res.release = recipe->release;
        res.language = variant.Language();
        res.entries = pak_files.size();
        res.bytes = pak.size();
        return res;
    } catch (const Stop&) {
        res.ok = false;
        res.stopped = true;
        return res;
    } catch (const std::bad_alloc&) {
        return fail("Not enough memory to build the data pack.");
    }
}

int RunCli(const std::vector<std::string>& args) {
    Options o;
    bool force_fallback = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto value = [&]() -> std::string {
            return i + 1 < args.size() ? args[++i] : std::string{};
        };
        if (args[i] == "--payload-zip") {
            o.payload_zip = value();
        } else if (args[i] == "--rom") {
            o.rom = value();
        } else if (args[i] == "--out") {
            o.out_dir = value();
        } else if (args[i] == "--language") {
            o.language = value();
        } else if (args[i] == "--cache") {
            o.cache_dir = value();
        } else if (args[i] == "--no-network") {
            o.no_network = true;
        } else if (args[i] == "--force-fallback") {
            force_fallback = true; // test hook, honoured only in AZAHARFRAME_TEST_HOOKS builds
        }
    }
    if (o.payload_zip.empty() || o.rom.empty() || o.out_dir.empty()) {
        std::puts("error usage: --frame-emerald-build --payload-zip ZIP --rom ROM --out DIR "
                  "[--language LANG] [--cache DIR] [--no-network]");
        return 1;
    }
    o.progress = [](int permille, const std::string& text) {
        std::printf("progress %d %s\n", permille, text.c_str());
        std::fflush(stdout);
    };
#ifdef AZAHARFRAME_TEST_HOOKS
    // AZAHARFRAME_TEST_HOOKS: the release's own builder in the sandbox, as when the recipe is unsupported
    const Result r = force_fallback ? Fallback::Run({.build = o}) : BuildPack(o);
#else
    (void)force_fallback;
    const Result r = BuildPack(o);
#endif
    if (r.stopped) {
        return 4;
    }
    if (r.unsupported) {
        std::printf("unsupported %s\n", r.error.c_str());
        return 5;
    }
    if (!r.ok) {
        std::printf("error %s\n", r.error.c_str());
        return 1;
    }
    std::printf("result %s\n", r.ToJson().c_str());
    return 0;
}

} // namespace Homebrew::Emerald3ds::Build
