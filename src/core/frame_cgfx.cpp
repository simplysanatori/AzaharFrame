// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: see frame_cgfx.h. The CGFX offsets below are the ones SPICA's object
// layout gives (SPICA/Formats/CtrGfx/**: GfxModel, GfxModelSkeletal, GfxBone, GfxMesh, GfxShape,
// GfxSubMesh, GfxFace, GfxVertexBuffer*, GfxMaterial, GfxTextureCoord, GfxTextureMapper,
// GfxTexture*), each checked against the files they are used on.

#include "core/frame_cgfx.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include "common/lz11.h"
#include "common/string_util.h"
#include "core/file_sys/romfs_reader.h"
#include "video_core/texture/texture_decode.h"

namespace FrameCgfx {

namespace {

struct Bad : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// Bounds-checked little endian reads from one buffer (throws Bad).
class Reader {
public:
    explicit Reader(std::span<const u8> data_) : data(data_) {}

    std::size_t Size() const {
        return data.size();
    }
    void Need(std::size_t off, std::size_t len) const {
        if (off > data.size() || len > data.size() - off) {
            throw Bad("an offset out of the file");
        }
    }
    u8 U8(std::size_t off) const {
        Need(off, 1);
        return data[off];
    }
    u16 U16(std::size_t off) const {
        Need(off, 2);
        return static_cast<u16>(data[off] | data[off + 1] << 8);
    }
    u32 U32(std::size_t off) const {
        Need(off, 4);
        return static_cast<u32>(data[off]) | static_cast<u32>(data[off + 1]) << 8 |
               static_cast<u32>(data[off + 2]) << 16 | static_cast<u32>(data[off + 3]) << 24;
    }
    s32 S32(std::size_t off) const {
        return static_cast<s32>(U32(off));
    }
    u64 U64(std::size_t off) const {
        return U32(off) | static_cast<u64>(U32(off + 4)) << 32;
    }
    float F32(std::size_t off) const {
        const u32 bits = U32(off);
        float f;
        std::memcpy(&f, &bits, sizeof(f));
        if (!std::isfinite(f)) {
            throw Bad("a number that is not finite");
        }
        return f;
    }
    /// A self-relative pointer: 0 for none. Added in 32 bits, as NW4C does, so one may point
    /// back too.
    std::size_t Rel(std::size_t off) const {
        const u32 v = U32(off);
        if (v == 0) {
            return 0;
        }
        const std::size_t to = static_cast<u32>(static_cast<u32>(off) + v);
        if (to >= data.size()) {
            throw Bad("a pointer out of the file");
        }
        return to;
    }
    std::size_t RelNonNull(std::size_t off) const {
        const std::size_t to = Rel(off);
        if (to == 0) {
            throw Bad("a missing pointer");
        }
        return to;
    }
    std::string CString(std::size_t off, std::size_t max = 256) const {
        Need(off, 1);
        std::size_t end = off;
        while (end < data.size() && data[end] != 0 && end - off < max) {
            end++;
        }
        return std::string(reinterpret_cast<const char*>(data.data() + off), end - off);
    }
    bool Magic(std::size_t off, const char* m) const {
        const std::size_t n = std::strlen(m);
        return off <= data.size() && n <= data.size() - off &&
               std::memcmp(data.data() + off, m, n) == 0;
    }
    std::span<const u8> Span(std::size_t off, std::size_t len) const {
        Need(off, len);
        return data.subspan(off, len);
    }

private:
    std::span<const u8> data;
};

constexpr u32 kMaxCount = 1u << 16;

void CheckCount(u32 n) {
    if (n > kMaxCount) {
        throw Bad("a count out of range");
    }
}

std::vector<std::string> SplitPath(std::string_view p) {
    std::vector<std::string> parts;
    std::size_t at = 0;
    while (at <= p.size()) {
        const std::size_t end = std::min(p.find('/', at), p.size());
        const std::string_view part = p.substr(at, end - at);
        if (!part.empty() && part != ".") {
            parts.emplace_back(part);
        }
        at = end + 1;
    }
    return parts;
}

std::string JoinPath(const std::vector<std::string>& parts) {
    std::string out;
    for (const auto& p : parts) {
        out += (out.empty() ? "" : "/") + p;
    }
    return out;
}

std::string Utf16Name(const Reader& r, std::size_t off, u32 bytes) {
    r.Need(off, bytes);
    std::u16string s;
    for (u32 i = 0; i + 1 < bytes; i += 2) {
        const char16_t c = static_cast<char16_t>(r.U16(off + i));
        if (c == 0) {
            break;
        }
        s.push_back(c);
    }
    return Common::UTF16ToUTF8(s);
}

// The RomFS level 3 tables.
constexpr std::size_t kRomFSHeaderBytes = 0x28;
constexpr u32 kNone = 0xFFFFFFFFu;

bool WalkRomFS(std::span<const u8> dirs, std::span<const u8> files, u64 data_offset,
               std::vector<RomFSIndex::File>& out, std::string& error) {
    try {
        const Reader dr(dirs), fr(files);
        struct Pending {
            u32 dir;
            std::string path;
            int depth;
        };
        std::vector<Pending> stack{{0, "", 0}};
        std::size_t visited = 0;
        while (!stack.empty()) {
            const Pending p = stack.back();
            stack.pop_back();
            if (++visited > kMaxCount || p.depth > 32) {
                throw Bad("too many directories");
            }
            const u32 child = dr.U32(p.dir + 8);
            u32 file = dr.U32(p.dir + 12);
            std::size_t files_here = 0;
            while (file != kNone) {
                if (++files_here > kMaxCount || out.size() > (std::size_t{1} << 20)) {
                    throw Bad("too many files");
                }
                const u32 name_len = fr.U32(file + 28);
                const std::string name = Utf16Name(fr, file + 32, name_len);
                RomFSIndex::File f;
                f.path = p.path.empty() ? name : p.path + "/" + name;
                f.offset = data_offset + fr.U64(file + 8);
                f.size = fr.U64(file + 16);
                out.push_back(std::move(f));
                file = fr.U32(file + 4);
            }
            u32 dir = child;
            std::size_t dirs_here = 0;
            while (dir != kNone) {
                if (++dirs_here > kMaxCount) {
                    throw Bad("too many directories");
                }
                const u32 name_len = dr.U32(dir + 20);
                const std::string name = Utf16Name(dr, dir + 24, name_len);
                stack.push_back({dir, p.path.empty() ? name : p.path + "/" + name, p.depth + 1});
                dir = dr.U32(dir + 4);
            }
        }
        return true;
    } catch (const Bad& e) {
        error = std::string{"RomFS: "} + e.what();
        return false;
    }
}

struct RomFSHeader {
    u32 dir_meta_offset = 0, dir_meta_size = 0, file_meta_offset = 0, file_meta_size = 0;
    u32 data_offset = 0;
};

bool ParseRomFSHeader(std::span<const u8> head, RomFSHeader& h, std::string& error) {
    try {
        const Reader r(head);
        if (r.U32(0) != kRomFSHeaderBytes) {
            throw Bad("not a level 3 RomFS image");
        }
        h.dir_meta_offset = r.U32(0x0C);
        h.dir_meta_size = r.U32(0x10);
        h.file_meta_offset = r.U32(0x1C);
        h.file_meta_size = r.U32(0x20);
        h.data_offset = r.U32(0x24);
        if (h.dir_meta_size > (32u << 20) || h.file_meta_size > (32u << 20)) {
            throw Bad("tables too large");
        }
        return true;
    } catch (const Bad& e) {
        error = std::string{"RomFS: "} + e.what();
        return false;
    }
}

} // namespace

// ---- RomFS ----

bool ListRomFS(std::span<const u8> image, std::vector<RomFSIndex::File>& out,
               std::string& error) {
    RomFSHeader h;
    if (!ParseRomFSHeader(image, h, error)) {
        return false;
    }
    const Reader r(image);
    try {
        return WalkRomFS(r.Span(h.dir_meta_offset, h.dir_meta_size),
                         r.Span(h.file_meta_offset, h.file_meta_size), h.data_offset, out, error);
    } catch (const Bad& e) {
        error = std::string{"RomFS: "} + e.what();
        return false;
    }
}

bool RomFSIndex::Open(std::shared_ptr<FileSys::RomFSReader> reader_, std::string& error) {
    reader = std::move(reader_);
    files.clear();
    if (!reader) {
        error = "no RomFS";
        return false;
    }
    Bytes head(kRomFSHeaderBytes);
    if (reader->ReadFile(0, head.size(), head.data()) != head.size()) {
        error = "RomFS: the header cannot be read";
        return false;
    }
    RomFSHeader h;
    if (!ParseRomFSHeader(head, h, error)) {
        return false;
    }
    Bytes dirs(h.dir_meta_size), file_table(h.file_meta_size);
    if (reader->ReadFile(h.dir_meta_offset, dirs.size(), dirs.data()) != dirs.size() ||
        reader->ReadFile(h.file_meta_offset, file_table.size(), file_table.data()) !=
            file_table.size()) {
        error = "RomFS: the tables cannot be read";
        return false;
    }
    if (!WalkRomFS(dirs, file_table, h.data_offset, files, error)) {
        return false;
    }
    const u64 size = reader->GetSize();
    std::erase_if(files, [size](const File& f) { return f.offset > size || f.size > size - f.offset; });
    return true;
}

const RomFSIndex::File* RomFSIndex::Find(std::string_view path) const {
    const std::string want = JoinPath(SplitPath(path));
    for (const File& f : files) {
        if (f.path == want) {
            return &f;
        }
    }
    return nullptr;
}

bool RomFSIndex::Read(const File& file, Bytes& out, u64 max_size) const {
    if (!reader || file.size > max_size) {
        return false;
    }
    out.resize(static_cast<std::size_t>(file.size));
    return reader->ReadFile(static_cast<std::size_t>(file.offset), out.size(), out.data()) ==
           out.size();
}

// ---- darc ----

bool ReadDarc(std::span<const u8> data, std::vector<DarcEntry>& out, std::string& error) {
    out.clear();
    try {
        const Reader r(data);
        if (!r.Magic(0, "darc") || r.U16(4) != 0xFEFF) {
            throw Bad("not a darc");
        }
        const u32 table = r.U32(0x10);
        const u32 table_size = r.U32(0x14);
        r.Need(table, table_size);
        const u32 count = r.U32(table + 8); // the root entry's end
        if (count == 0 || u64{count} * 12 > table_size) {
            throw Bad("darc: a bad table");
        }
        const std::size_t names = table + std::size_t{count} * 12;
        const auto name_at = [&](u32 off) {
            std::u16string s;
            for (std::size_t p = names + off; p + 1 < table + std::size_t{table_size}; p += 2) {
                const char16_t c = static_cast<char16_t>(r.U16(p));
                if (c == 0) {
                    break;
                }
                s.push_back(c);
            }
            return Common::UTF16ToUTF8(s);
        };
        std::vector<std::pair<u32, std::vector<std::string>>> dirs{{count, {}}};
        for (u32 i = 1; i < count; i++) {
            const std::size_t e = table + std::size_t{i} * 12;
            const u32 name = r.U32(e), offset = r.U32(e + 4), size = r.U32(e + 8);
            while (dirs.size() > 1 && i >= dirs.back().first) {
                dirs.pop_back();
            }
            std::vector<std::string> path = dirs.back().second;
            for (auto& part : SplitPath(name_at(name & 0xFFFFFF))) {
                path.push_back(std::move(part));
            }
            if (name >> 24) {
                if (size <= i || size > count) {
                    throw Bad("darc: a bad directory");
                }
                dirs.emplace_back(size, std::move(path));
                if (dirs.size() > 64) {
                    throw Bad("darc: too deep");
                }
            } else {
                r.Need(offset, size);
                out.push_back({JoinPath(path), offset, size});
            }
        }
        return true;
    } catch (const Bad& e) {
        error = e.what();
        return false;
    }
}

bool DarcMember(std::span<const u8> data, std::string_view path, Bytes& out, std::string& error) {
    Bytes cur;
    const auto take = [&](std::span<const u8> bytes) {
        if (!bytes.empty() && bytes[0] == 0x11) {
            Bytes unpacked;
            if (!Common::DecompressLZ11(bytes.data(), bytes.size(), unpacked)) {
                error = "a bad LZ11 stream";
                return false;
            }
            cur = std::move(unpacked);
        } else {
            cur.assign(bytes.begin(), bytes.end());
        }
        return true;
    };
    if (!take(data)) {
        return false;
    }
    std::size_t at = 0;
    while (at <= path.size()) {
        const std::size_t end = std::min(path.find('!', at), path.size());
        const std::string want = JoinPath(SplitPath(path.substr(at, end - at)));
        std::vector<DarcEntry> entries;
        if (!ReadDarc(cur, entries, error)) {
            return false;
        }
        const auto it = std::find_if(entries.begin(), entries.end(),
                                     [&](const DarcEntry& e) { return e.path == want; });
        if (it == entries.end()) {
            error = "no " + want + " in the archive";
            return false;
        }
        const Bytes member(cur.begin() + it->offset, cur.begin() + it->offset + it->size);
        if (!take(member)) {
            return false;
        }
        at = end + 1;
    }
    out = std::move(cur);
    return true;
}

// ---- PSLB ----

const PslbValue* PslbValue::Get(std::string_view key) const {
    if (kind != Kind::Object) {
        return nullptr;
    }
    for (std::size_t i = 0; i < keys.size() && i < array.size(); i++) {
        if (keys[i] == key) {
            return &array[i];
        }
    }
    return nullptr;
}

std::optional<float> PslbValue::Number() const {
    if (kind == Kind::Int) {
        return static_cast<float>(integer);
    }
    if (kind == Kind::Float) {
        return real;
    }
    return std::nullopt;
}

bool ReadPslb(std::span<const u8> data, PslbValue& out, std::string& error) {
    try {
        const Reader r(data);
        if (!r.Magic(0, "PSLB")) {
            throw Bad("not a PSLB");
        }
        std::size_t pos = 0x10;
        std::size_t nodes = 0;
        const auto str = [&]() {
            const u32 n = r.U32(pos);
            pos += 4;
            r.Need(pos, n);
            std::string s(reinterpret_cast<const char*>(data.data() + pos), n);
            pos += n;
            return s;
        };
        const auto value = [&](auto&& self, PslbValue& v, int depth) -> void {
            if (depth > 32 || ++nodes > kMaxCount) {
                throw Bad("PSLB: too deep or too large");
            }
            const u8 type = r.U8(pos++);
            switch (type) {
            case 1:
                v.kind = PslbValue::Kind::String;
                v.string = str();
                break;
            case 2:
                v.kind = PslbValue::Kind::Int;
                v.integer = r.S32(pos);
                pos += 4;
                break;
            case 3:
                v.kind = PslbValue::Kind::Float;
                v.real = r.F32(pos);
                pos += 4;
                break;
            case 4:
                v.kind = PslbValue::Kind::Bool;
                v.boolean = r.U8(pos++) != 0;
                break;
            case 5: {
                v.kind = PslbValue::Kind::Object;
                const u32 n = r.U32(pos);
                pos += 4;
                CheckCount(n);
                for (u32 i = 0; i < n; i++) {
                    v.keys.push_back(str());
                    PslbValue child;
                    self(self, child, depth + 1);
                    v.array.push_back(std::move(child));
                }
                break;
            }
            case 6: {
                v.kind = PslbValue::Kind::Array;
                const u32 n = r.U32(pos);
                pos += 4;
                CheckCount(n);
                for (u32 i = 0; i < n; i++) {
                    PslbValue child;
                    self(self, child, depth + 1);
                    v.array.push_back(std::move(child));
                }
                break;
            }
            default:
                throw Bad("PSLB: an unknown value type");
            }
        };
        PslbValue root;
        value(value, root, 0);
        out = std::move(root);
        return true;
    } catch (const Bad& e) {
        error = e.what();
        return false;
    }
}

// ---- Matrices ----

Mat34 Identity34() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
}

Mat34 Multiply34(const Mat34& a, const Mat34& b) {
    Mat34 m{};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            float v = c == 3 ? a[r * 4 + 3] : 0.0f;
            for (int k = 0; k < 3; k++) {
                v += a[r * 4 + k] * b[k * 4 + c];
            }
            m[r * 4 + c] = v;
        }
    }
    return m;
}

Mat34 Inverse34(const Mat34& m) {
    const double a = m[0], b = m[1], c = m[2], d = m[4], e = m[5], f = m[6], g = m[8], h = m[9],
                 i = m[10];
    const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (std::fabs(det) < 1e-12) {
        return Identity34();
    }
    const double inv[9] = {(e * i - f * h) / det, (c * h - b * i) / det, (b * f - c * e) / det,
                           (f * g - d * i) / det, (a * i - c * g) / det, (c * d - a * f) / det,
                           (d * h - e * g) / det, (b * g - a * h) / det, (a * e - b * d) / det};
    Mat34 out{};
    for (int r = 0; r < 3; r++) {
        double t = 0.0;
        for (int k = 0; k < 3; k++) {
            out[r * 4 + k] = static_cast<float>(inv[r * 3 + k]);
            t -= inv[r * 3 + k] * m[k * 4 + 3];
        }
        out[r * 4 + 3] = static_cast<float>(t);
    }
    return out;
}

std::array<float, 3> Point(const Mat34& m, const std::array<float, 3>& p) {
    std::array<float, 3> out{};
    for (int r = 0; r < 3; r++) {
        out[r] = m[r * 4] * p[0] + m[r * 4 + 1] * p[1] + m[r * 4 + 2] * p[2] + m[r * 4 + 3];
    }
    return out;
}

std::array<float, 3> Direction(const Mat34& m, const std::array<float, 3>& d) {
    std::array<float, 3> out{};
    for (int r = 0; r < 3; r++) {
        out[r] = m[r * 4] * d[0] + m[r * 4 + 1] * d[1] + m[r * 4 + 2] * d[2];
    }
    return out;
}

// ---- CGFX ----

namespace {

struct Cgfx {
    const Reader& r;

    std::vector<std::pair<std::string, std::size_t>> Dict(std::size_t off) const {
        std::vector<std::pair<std::string, std::size_t>> out;
        if (off == 0) {
            return out;
        }
        if (!r.Magic(off, "DICT")) {
            throw Bad("CGFX: not a DICT");
        }
        const u32 n = r.U32(off + 8);
        CheckCount(n);
        for (u32 i = 0; i < n; i++) {
            const std::size_t e = off + 0x1C + std::size_t{i} * 16;
            const std::size_t name = r.Rel(e + 8);
            out.emplace_back(name ? r.CString(name) : std::string{}, r.RelNonNull(e + 12));
        }
        return out;
    }

    std::vector<std::size_t> PtrList(std::size_t off) const {
        const u32 n = r.U32(off);
        CheckCount(n);
        std::vector<std::size_t> out;
        if (n == 0) {
            return out;
        }
        const std::size_t table = r.RelNonNull(off + 4);
        for (u32 i = 0; i < n; i++) {
            out.push_back(r.RelNonNull(table + std::size_t{i} * 4));
        }
        return out;
    }

    Mat34 Matrix(std::size_t off) const {
        Mat34 m{};
        for (int i = 0; i < 12; i++) {
            m[static_cast<std::size_t>(i)] = r.F32(off + std::size_t(i) * 4);
        }
        return m;
    }

    Material ReadMaterial(const std::string& name, std::size_t mo) const {
        Material mat;
        mat.name = name;
        // The rasterization's face culling command (GfxRasterization: value, then the header
        // of register 0x40, one word).
        if (r.U32(mo + 0x114) == 0x00010040u) {
            mat.cull = r.U32(mo + 0x110) & 3;
        }
        const u32 used = r.U32(mo + 0x168);
        if (used > 3) {
            throw Bad("CGFX: material " + name + ": a bad texture coordinate count");
        }
        for (u32 i = 0; i < used; i++) {
            const std::size_t c = mo + 0x16C + std::size_t{i} * 0x58;
            TexCoord tc;
            tc.source = r.S32(c);
            tc.mapping = r.U32(c + 4);
            tc.scale_u = r.F32(c + 0x10);
            tc.scale_v = r.F32(c + 0x14);
            tc.rotation = r.F32(c + 0x18);
            tc.translate_u = r.F32(c + 0x1C);
            tc.translate_v = r.F32(c + 0x20);
            if (tc.source < 0 || tc.source > 2 || tc.mapping > 3) {
                throw Bad("CGFX: material " + name + ": a bad texture coordinate");
            }
            mat.coords.push_back(tc);
        }
        for (std::size_t i = 0; i < 3; i++) {
            const std::size_t m = r.Rel(mo + 0x274 + i * 4);
            if (m == 0) {
                continue;
            }
            if (r.U32(m) != 0x80000000u) {
                throw Bad("CGFX: material " + name + ": not a texture mapper");
            }
            const std::size_t ref = r.Rel(m + 8);
            if (ref == 0) {
                continue;
            }
            if (r.U32(ref) != 0x20000004u || !r.Magic(ref + 4, "TXOB")) {
                throw Bad("CGFX: material " + name + ": not a texture reference");
            }
            const std::size_t path = r.Rel(ref + 0x18);
            Mapper mp;
            mp.texture = path ? r.CString(path) : std::string{};
            // The mapper's PICA commands: the border colour register (0x81, 0x91 or 0x99) and
            // the two after it written in one burst: dimensions, then the parameters with the
            // wrap modes (t: bits 8-10, s: bits 12-14).
            const u32 header = r.U32(m + 0x1C);
            const u32 reg = header & 0xFFFF;
            if ((reg == 0x81 || reg == 0x91 || reg == 0x99) && ((header >> 20) & 0xFF) >= 2) {
                const u32 param = r.U32(m + 0x24);
                mp.wrap_v = (param >> 8) & 7;
                mp.wrap_u = (param >> 12) & 7;
            }
            mat.mappers[i] = std::move(mp);
        }
        return mat;
    }

    Shape ReadShape(std::size_t so) const {
        Shape s;
        const std::array<float, 3> offset{r.F32(so + 0x20), r.F32(so + 0x24), r.F32(so + 0x28)};
        for (const std::size_t sm : PtrList(so + 0x2C)) {
            SubMesh sub;
            const u32 nb = r.U32(sm);
            CheckCount(nb);
            if (nb) {
                const std::size_t bt = r.RelNonNull(sm + 4);
                for (u32 i = 0; i < nb; i++) {
                    sub.bones.push_back(r.S32(bt + std::size_t{i} * 4));
                }
            }
            sub.skinning = r.U32(sm + 8);
            for (const std::size_t face : PtrList(sm + 0xC)) {
                for (const std::size_t fd : PtrList(face)) {
                    const u32 format = r.U32(fd);
                    const u8 prim = r.U8(fd + 4);
                    const u32 bytes = r.U32(fd + 8);
                    const std::size_t buf = r.RelNonNull(fd + 0xC);
                    const bool shorts = format == 0x1403;
                    const u32 n = shorts ? bytes / 2 : bytes;
                    CheckCount(n);
                    r.Need(buf, bytes);
                    std::vector<u32> idx(n);
                    for (u32 i = 0; i < n; i++) {
                        idx[i] = shorts ? r.U16(buf + std::size_t{i} * 2) : r.U8(buf + i);
                    }
                    if (prim == 0) {
                        for (u32 i = 0; i + 2 < n; i += 3) {
                            sub.indices.insert(sub.indices.end(), {idx[i], idx[i + 1], idx[i + 2]});
                        }
                    } else if (prim == 1) {
                        for (u32 i = 0; i + 2 < n; i++) {
                            if (i % 2 == 0) {
                                sub.indices.insert(sub.indices.end(), {idx[i], idx[i + 1], idx[i + 2]});
                            } else {
                                sub.indices.insert(sub.indices.end(), {idx[i + 1], idx[i], idx[i + 2]});
                            }
                        }
                    } else if (prim == 2) {
                        for (u32 i = 1; i + 1 < n; i++) {
                            sub.indices.insert(sub.indices.end(), {idx[0], idx[i], idx[i + 1]});
                        }
                    } else {
                        throw Bad("CGFX: an unknown primitive");
                    }
                }
            }
            s.submeshes.push_back(std::move(sub));
        }
        // The vertex buffers: the interleaved one, and attributes fixed for every vertex.
        struct Attr {
            u32 attr, format, elements;
            float scale;
            u32 offset;
        };
        std::size_t raw = 0, length = 0, stride = 0;
        std::vector<Attr> attrs;
        std::vector<std::pair<u32, std::vector<float>>> fixed;
        for (const std::size_t vo : PtrList(so + 0x38)) {
            const u32 type = r.U32(vo);
            if (type == 0x40000002u && stride == 0) {
                length = r.U32(vo + 0x14);
                raw = r.RelNonNull(vo + 0x18);
                stride = r.U32(vo + 0x24);
                if (stride == 0 || stride > 256) {
                    throw Bad("CGFX: a bad vertex stride");
                }
                r.Need(raw, length);
                for (const std::size_t ao : PtrList(vo + 0x28)) {
                    attrs.push_back({r.U32(ao + 4), r.U32(ao + 0x24), r.U32(ao + 0x28),
                                     r.F32(ao + 0x2C), r.U32(ao + 0x30)});
                }
            } else if (type == 0x80000000u) {
                const u32 attr = r.U32(vo + 4);
                const u32 n = r.U32(vo + 0x18);
                if (n > 4) {
                    throw Bad("CGFX: a bad fixed attribute");
                }
                std::vector<float> vec;
                if (n) {
                    const std::size_t at = r.RelNonNull(vo + 0x1C);
                    for (u32 i = 0; i < n; i++) {
                        vec.push_back(r.F32(at + std::size_t{i} * 4));
                    }
                }
                fixed.emplace_back(attr, std::move(vec));
            }
        }
        if (stride == 0) {
            throw Bad("CGFX: a shape without an interleaved vertex buffer");
        }
        const std::size_t count = length / stride;
        CheckCount(static_cast<u32>(std::min<std::size_t>(count, kMaxCount + 1)));
        const auto put = [](Vertex& v, u32 attr, const float* x, u32 n, bool raw_index) {
            switch (attr) {
            case 0:
                for (u32 k = 0; k < 3 && k < n; k++) v.position[k] = x[k];
                break;
            case 1:
                for (u32 k = 0; k < 3 && k < n; k++) v.normal[k] = x[k];
                break;
            case 3:
                for (u32 k = 0; k < 4 && k < n; k++) v.colour[k] = x[k];
                break;
            case 4:
                for (u32 k = 0; k < 2 && k < n; k++) v.uv0[k] = x[k];
                break;
            case 5:
                for (u32 k = 0; k < 2 && k < n; k++) v.uv1[k] = x[k];
                break;
            case 7:
                for (u32 k = 0; k < 4 && k < n; k++) {
                    v.bones[k] = static_cast<u8>(std::clamp(x[k], 0.0f, 255.0f));
                }
                v.has_bones = n > 0;
                (void)raw_index;
                break;
            default:
                break;
            }
        };
        s.vertices.resize(count);
        for (std::size_t i = 0; i < count; i++) {
            Vertex& v = s.vertices[i];
            for (const auto& [attr, vec] : fixed) {
                put(v, attr, vec.data(), static_cast<u32>(vec.size()), true);
            }
            const std::size_t base = raw + i * stride;
            for (const Attr& a : attrs) {
                if (a.elements == 0 || a.elements > 4) {
                    throw Bad("CGFX: a bad attribute");
                }
                float x[4]{};
                for (u32 k = 0; k < a.elements; k++) {
                    float f;
                    switch (a.format) {
                    case 0x1400:
                        f = static_cast<s8>(r.U8(base + a.offset + k));
                        break;
                    case 0x1401:
                        f = r.U8(base + a.offset + k);
                        break;
                    case 0x1402:
                        f = static_cast<s16>(r.U16(base + a.offset + std::size_t{k} * 2));
                        break;
                    case 0x1406:
                        f = r.F32(base + a.offset + std::size_t{k} * 4);
                        break;
                    default:
                        throw Bad("CGFX: an unknown attribute format");
                    }
                    x[k] = a.attr == 7 ? f : f * a.scale;
                }
                put(v, a.attr, x, a.elements, false);
            }
            for (int k = 0; k < 3; k++) {
                v.position[static_cast<std::size_t>(k)] += offset[static_cast<std::size_t>(k)];
            }
        }
        for (const SubMesh& sub : s.submeshes) {
            for (const u32 i : sub.indices) {
                if (i >= count) {
                    throw Bad("CGFX: an index out of range");
                }
            }
        }
        return s;
    }

    Model ReadModel(std::size_t mo) const {
        Model m;
        const u32 type = r.U32(mo);
        const std::size_t name = r.Rel(mo + 0xC);
        m.name = name ? r.CString(name) : std::string{};
        if (type & 0x80) {
            const std::size_t so = r.RelNonNull(mo + 0xE0);
            if (!r.Magic(so + 4, "SOBJ")) {
                throw Bad("CGFX: not a skeleton");
            }
            for (const auto& [bone_name, bo] : Dict(r.Rel(so + 0x1C))) {
                Bone b;
                const std::size_t n = r.Rel(bo);
                b.name = n ? r.CString(n) : bone_name;
                b.index = r.S32(bo + 8);
                b.parent = r.S32(bo + 0xC);
                b.bind = Inverse34(Matrix(bo + 0xA4));
                m.bones.push_back(std::move(b));
            }
            // In index order when the indices are a permutation (they are in the files seen).
            std::vector<Bone> sorted(m.bones.size());
            bool ok = true;
            for (const Bone& b : m.bones) {
                if (b.index < 0 || static_cast<std::size_t>(b.index) >= sorted.size() ||
                    !sorted[static_cast<std::size_t>(b.index)].name.empty()) {
                    ok = false;
                    break;
                }
                sorted[static_cast<std::size_t>(b.index)] = b;
            }
            if (ok) {
                m.bones = std::move(sorted);
            }
        }
        if (r.U32(mo + 0xBC)) {
            for (const auto& [mat_name, mat] : Dict(r.Rel(mo + 0xC0))) {
                m.materials.push_back(ReadMaterial(mat_name, mat));
            }
        }
        if (r.U32(mo + 0xCC)) {
            for (const auto& [node_name, node] : Dict(r.Rel(mo + 0xD0))) {
                m.node_visibility.emplace_back(node_name, r.U32(node + 4) != 0);
            }
        }
        for (const std::size_t so : PtrList(mo + 0xC4)) {
            m.shapes.push_back(ReadShape(so));
        }
        for (const std::size_t o : PtrList(mo + 0xB4)) {
            if (!r.Magic(o + 4, "SOBJ")) {
                throw Bad("CGFX: not a mesh");
            }
            Mesh mesh;
            const std::size_t n = r.Rel(o + 0xC);
            mesh.name = n ? r.CString(n) : std::string{};
            mesh.shape = r.S32(o + 0x18);
            mesh.material = r.S32(o + 0x1C);
            mesh.visible = r.U8(o + 0x24) != 0;
            mesh.node = static_cast<s16>(r.U16(o + 0x26));
            if (mesh.shape < 0 || static_cast<std::size_t>(mesh.shape) >= m.shapes.size() ||
                mesh.material < 0 ||
                static_cast<std::size_t>(mesh.material) >= m.materials.size()) {
                throw Bad("CGFX: a mesh's shape or material out of range");
            }
            m.meshes.push_back(std::move(mesh));
        }
        return m;
    }

    Texture ReadTexture(const std::string& name, std::size_t o) const {
        if (!r.Magic(o + 4, "TXOB")) {
            throw Bad("CGFX: not a texture");
        }
        Texture t;
        t.name = name;
        t.height = r.U32(o + 0x18);
        t.width = r.U32(o + 0x1C);
        t.format = r.U32(o + 0x34);
        const std::size_t image = r.RelNonNull(o + 0x38);
        const std::size_t data = r.RelNonNull(image + 0xC);
        if (t.width == 0 || t.height == 0 || t.width > 1024 || t.height > 1024 ||
            t.width % 8 != 0 || t.height % 8 != 0 || t.format > 13) {
            throw Bad("CGFX: texture " + name + ": a size or format not read");
        }
        using Pica::TexturingRegs;
        Pica::Texture::TextureInfo info{};
        info.width = t.width;
        info.height = t.height;
        info.format = static_cast<TexturingRegs::TextureFormat>(t.format);
        info.SetDefaultStride();
        const std::size_t bytes = Pica::Texture::CalculateTileSize(info.format) *
                                  (t.width / 8) * (t.height / 8);
        const std::span<const u8> src = r.Span(data, bytes);
        t.raw.assign(src.begin(), src.end());
        t.rgba.resize(std::size_t{t.width} * t.height * 4);
        for (u32 y = 0; y < t.height; y++) {
            for (u32 x = 0; x < t.width; x++) {
                const auto c = Pica::Texture::LookupTexture(src.data(), x, y, info);
                u8* p = t.rgba.data() + (std::size_t{y} * t.width + x) * 4;
                p[0] = c.r();
                p[1] = c.g();
                p[2] = c.b();
                p[3] = c.a();
            }
        }
        return t;
    }
};

} // namespace

s32 Model::BoneIndex(std::string_view bone) const {
    for (std::size_t i = 0; i < bones.size(); i++) {
        if (bones[i].name == bone) {
            return static_cast<s32>(i);
        }
    }
    return -1;
}

bool Model::MeshShown(const Mesh& mesh) const {
    // As the game's default: a mesh whose node is hidden is not drawn (the node visibility list,
    // e.g. a palm's idle sleeve and its glowing one).
    if (mesh.node >= 0 && static_cast<std::size_t>(mesh.node) < node_visibility.size()) {
        return node_visibility[static_cast<std::size_t>(mesh.node)].second;
    }
    return true;
}

const Texture* File::FindTexture(std::string_view name) const {
    for (const Texture& t : textures) {
        if (t.name == name) {
            return &t;
        }
    }
    return nullptr;
}

bool ReadCgfx(std::span<const u8> data, File& out, std::string& error) {
    try {
        const Reader r(data);
        if (!r.Magic(0, "CGFX") || r.U16(4) != 0xFEFF) {
            throw Bad("not a CGFX file");
        }
        const u16 header = r.U16(6);
        if (!r.Magic(header, "DATA")) {
            throw Bad("CGFX: no DATA");
        }
        const Cgfx c{r};
        File f;
        const auto section = [&](int i) -> std::size_t {
            const std::size_t p = header + 8 + std::size_t(i) * 8;
            return r.U32(p) ? r.RelNonNull(p + 4) : 0;
        };
        for (const auto& [name, mo] : c.Dict(section(0))) {
            f.models.push_back(c.ReadModel(mo));
        }
        for (const auto& [name, to] : c.Dict(section(1))) {
            f.textures.push_back(c.ReadTexture(name, to));
        }
        out = std::move(f);
        return true;
    } catch (const Bad& e) {
        error = e.what();
        return false;
    }
}

std::array<float, 3> BindPosition(const Model& model, const SubMesh& sub, const Vertex& v,
                                  std::array<float, 3>* normal) {
    std::array<float, 3> p = v.position;
    std::array<float, 3> n = v.normal;
    if (sub.skinning != 2 && !model.bones.empty()) {
        s32 bone = 0;
        if (!sub.bones.empty()) {
            const std::size_t local = v.has_bones ? v.bones[0] : 0;
            bone = local < sub.bones.size() ? sub.bones[local] : sub.bones[0];
        }
        if (bone >= 0 && static_cast<std::size_t>(bone) < model.bones.size()) {
            const Mat34& m = model.bones[static_cast<std::size_t>(bone)].bind;
            p = Point(m, p);
            n = Direction(m, n);
        }
    }
    if (normal) {
        *normal = n;
    }
    return p;
}

} // namespace FrameCgfx
