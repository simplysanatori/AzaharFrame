// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: the readers (LZ11, darc, PSLB, RomFS level 3, CGFX) and the weapon
// converter on small inputs built here byte by byte (no game data), and the MXFM reader's
// weapon space.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <fmt/format.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/file_util.h"
#include "common/hash.h"
#include "common/lz11.h"
#include "core/file_sys/romfs_reader.h"
#include "core/frame_cgfx.h"
#include "core/frame_weapon_models.h"
#include "core/frontend/image_interface.h"
#include "video_core/custom_textures/custom_tex_manager.h"
#include "frame_xr/vis/model_file.h"

using Catch::Approx;

namespace {

using Bytes = std::vector<u8>;

/// A growing buffer with self-relative pointers, as NW4C files have them.
struct Buf {
    Bytes b;
    std::size_t Alloc(std::size_t n) {
        while (b.size() % 4) {
            b.push_back(0);
        }
        const std::size_t at = b.size();
        b.resize(at + n, 0);
        return at;
    }
    void W8(std::size_t at, u8 v) {
        b[at] = v;
    }
    void W16(std::size_t at, u16 v) {
        b[at] = static_cast<u8>(v);
        b[at + 1] = static_cast<u8>(v >> 8);
    }
    void W32(std::size_t at, u32 v) {
        for (int k = 0; k < 4; k++) {
            b[at + static_cast<std::size_t>(k)] = static_cast<u8>(v >> (8 * k));
        }
    }
    void F(std::size_t at, float f) {
        u32 v;
        std::memcpy(&v, &f, 4);
        W32(at, v);
    }
    void Rel(std::size_t at, std::size_t to) {
        W32(at, static_cast<u32>(to - at));
    }
    std::size_t Str(const std::string& s) {
        const std::size_t at = Alloc(s.size() + 1);
        std::memcpy(b.data() + at, s.data(), s.size());
        return at;
    }
    void Magic(std::size_t at, const char* m) {
        std::memcpy(b.data() + at, m, std::strlen(m));
    }
    /// A pointer list at `at`: count, then a pointer to a table of pointers.
    void PtrList(std::size_t at, const std::vector<std::size_t>& to) {
        W32(at, static_cast<u32>(to.size()));
        if (to.empty()) {
            return;
        }
        const std::size_t table = Alloc(4 * to.size());
        Rel(at + 4, table);
        for (std::size_t i = 0; i < to.size(); i++) {
            Rel(table + 4 * i, to[i]);
        }
    }
    std::size_t Dict(const std::vector<std::pair<std::string, std::size_t>>& entries) {
        const std::size_t d = Alloc(0x1C + 16 * entries.size());
        Magic(d, "DICT");
        W32(d + 8, static_cast<u32>(entries.size()));
        for (std::size_t i = 0; i < entries.size(); i++) {
            const std::size_t e = d + 0x1C + 16 * i;
            Rel(e + 8, Str(entries[i].first));
            Rel(e + 12, entries[i].second);
        }
        return d;
    }
};

// ---- LZ11 ----

/// LZ11 with literals only (the decoder's other half is tested by hand-made streams).
Bytes Lz11Store(const Bytes& in) {
    Bytes out{0x11, static_cast<u8>(in.size()), static_cast<u8>(in.size() >> 8),
              static_cast<u8>(in.size() >> 16)};
    for (std::size_t i = 0; i < in.size(); i += 8) {
        out.push_back(0);
        for (std::size_t k = i; k < i + 8 && k < in.size(); k++) {
            out.push_back(in[k]);
        }
    }
    return out;
}

// ---- darc ----

Bytes Darc(const std::vector<std::pair<std::string, Bytes>>& files) {
    // The tree: root, ".", then each file under its folders (one level is enough here).
    struct Entry {
        std::u16string name;
        bool dir;
        u32 a, b; // dir: parent, end; file: data index
    };
    std::vector<Entry> entries{{u"", true, 0, 0}, {u".", true, 0, 0}};
    std::map<std::string, std::vector<std::size_t>> by_dir;
    for (std::size_t i = 0; i < files.size(); i++) {
        const auto slash = files[i].first.rfind('/');
        by_dir[slash == std::string::npos ? "" : files[i].first.substr(0, slash)].push_back(i);
    }
    std::vector<std::pair<std::size_t, std::size_t>> file_entries; // entry, file index
    const auto u16 = [](const std::string& s) { return std::u16string(s.begin(), s.end()); };
    for (const auto& [dir, list] : by_dir) {
        std::size_t dir_entry = 0;
        if (!dir.empty()) {
            dir_entry = entries.size();
            entries.push_back({u16(dir), true, 1, 0});
        }
        for (const std::size_t i : list) {
            const std::string& p = files[i].first;
            file_entries.emplace_back(entries.size(), i);
            entries.push_back({u16(p.substr(p.rfind('/') == std::string::npos ? 0 : p.rfind('/') + 1)),
                               false, 0, 0});
        }
        if (!dir.empty()) {
            entries[dir_entry].b = static_cast<u32>(entries.size());
        }
    }
    entries[0].b = entries[1].b = static_cast<u32>(entries.size());
    Bytes names;
    std::vector<u32> name_off;
    for (const Entry& e : entries) {
        name_off.push_back(static_cast<u32>(names.size()));
        for (const char16_t c : e.name) {
            names.push_back(static_cast<u8>(c));
            names.push_back(static_cast<u8>(c >> 8));
        }
        names.push_back(0);
        names.push_back(0);
    }
    const std::size_t table = 0x1C;
    const std::size_t table_size = entries.size() * 12 + names.size();
    std::size_t data = (table + table_size + 31) & ~std::size_t{31};
    Buf out;
    out.Alloc(data);
    out.Magic(0, "darc");
    out.W16(4, 0xFEFF);
    out.W16(6, 0x1C);
    out.W32(0x10, static_cast<u32>(table));
    out.W32(0x14, static_cast<u32>(table_size));
    out.W32(0x18, static_cast<u32>(data));
    std::map<std::size_t, std::pair<u32, u32>> placed;
    for (const auto& [entry, i] : file_entries) {
        const std::size_t at = out.Alloc(files[i].second.size());
        std::memcpy(out.b.data() + at, files[i].second.data(), files[i].second.size());
        placed[entry] = {static_cast<u32>(at), static_cast<u32>(files[i].second.size())};
    }
    for (std::size_t i = 0; i < entries.size(); i++) {
        const std::size_t e = table + i * 12;
        const Entry& en = entries[i];
        out.W32(e, name_off[i] | (en.dir ? 0x01000000u : 0u));
        if (en.dir) {
            out.W32(e + 4, en.a);
            out.W32(e + 8, en.b);
        } else {
            out.W32(e + 4, placed[i].first);
            out.W32(e + 8, placed[i].second);
        }
    }
    std::memcpy(out.b.data() + table + entries.size() * 12, names.data(), names.size());
    out.W32(0xC, static_cast<u32>(out.b.size()));
    return out.b;
}

// ---- PSLB ----

struct Pslb {
    Bytes b{'P', 'S', 'L', 'B', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    void U32(u32 v) {
        for (int k = 0; k < 4; k++) {
            b.push_back(static_cast<u8>(v >> (8 * k)));
        }
    }
    void Raw(const std::string& s) {
        U32(static_cast<u32>(s.size()));
        b.insert(b.end(), s.begin(), s.end());
    }
    void String(const std::string& s) {
        b.push_back(1);
        Raw(s);
    }
    void Int(s32 v) {
        b.push_back(2);
        U32(static_cast<u32>(v));
    }
    void Float(float f) {
        b.push_back(3);
        u32 v;
        std::memcpy(&v, &f, 4);
        U32(v);
    }
    void Object(u32 n) {
        b.push_back(5);
        U32(n);
    }
    void Array(u32 n) {
        b.push_back(6);
        U32(n);
    }
    void Key(const std::string& k) {
        Raw(k);
    }
};

Bytes WeaponJson(const std::string& wpnid, const std::string& node, std::array<float, 3> muzzle) {
    Pslb p;
    p.Object(1);
    p.Key("weaponParams_");
    p.Array(1);
    p.Object(2);
    p.Key("weaponID_");
    p.String(wpnid);
    p.Key("params_");
    p.Array(2);
    p.Object(2);
    p.Key("muzzleNode_");
    p.String(node);
    p.Key("muzzleOffset_");
    p.Array(3);
    for (const float f : muzzle) {
        p.Float(f);
    }
    p.Object(1);
    p.Key("chargeTime_");
    p.Array(2);
    p.Int(72);
    p.Int(80);
    return p.b;
}

// ---- CGFX ----

struct Tri {
    std::array<std::array<float, 3>, 3> pos;
    std::array<float, 3> normal;
    /// AzaharFrame kiumodels: the corners' UVs (as the game's file has them).
    std::array<std::array<float, 2>, 3> uv{{{0.0f, 0.25f}, {0.5f, 0.25f}, {1.0f, 0.25f}}};
};

struct MeshSpec {
    std::string material;  ///< index by name below
    std::vector<s32> bones; ///< the submesh's bone table
    u32 skinning = 1;
    Tri tri;
    s32 node = -1;
};

/// One skeletal model: bones (name, parent, translation), materials (name, texture), meshes;
/// one 8x8 RGBA8 texture "Tex" of one colour.
Bytes Cgfx(const std::vector<std::tuple<std::string, s32, std::array<float, 3>>>& bones,
           const std::vector<std::pair<std::string, std::string>>& materials,
           const std::vector<MeshSpec>& meshes,
           const std::vector<std::pair<std::string, bool>>& nodes = {}) {
    Buf f;
    f.Alloc(0x14 + 8 + 16 * 8);
    f.Magic(0, "CGFX");
    f.W16(4, 0xFEFF);
    f.W16(6, 0x14);
    f.Magic(0x14, "DATA");
    // Bones.
    std::vector<std::pair<std::string, std::size_t>> bone_entries;
    for (std::size_t i = 0; i < bones.size(); i++) {
        const auto& [name, parent, t] = bones[i];
        const std::size_t bo = f.Alloc(0xE0);
        f.Rel(bo, f.Str(name));
        f.W32(bo + 8, static_cast<u32>(i));
        f.W32(bo + 0xC, static_cast<u32>(parent));
        // The inverse bind matrix: a translation by -t (bones are translations here).
        const float inv[12] = {1, 0, 0, -t[0], 0, 1, 0, -t[1], 0, 0, 1, -t[2]};
        for (int k = 0; k < 12; k++) {
            f.F(bo + 0xA4 + 4 * static_cast<std::size_t>(k), inv[k]);
        }
        bone_entries.emplace_back(name, bo);
    }
    const std::size_t skel = f.Alloc(0x40);
    f.Magic(skel + 4, "SOBJ");
    f.Rel(skel + 0x1C, f.Dict(bone_entries));
    // Materials: one UV coordinate set, mapper 0 the texture, wrap mirrored across, clamp down.
    std::vector<std::pair<std::string, std::size_t>> mat_entries;
    for (const auto& [name, tex] : materials) {
        const std::size_t mo = f.Alloc(0x280);
        f.W32(mo + 0x110, 2);           // cull one side
        f.W32(mo + 0x114, 0x00010040u); // register 0x40
        f.W32(mo + 0x168, 1);
        f.F(mo + 0x16C + 0x10, 1.0f);
        f.F(mo + 0x16C + 0x14, 1.0f);
        const std::size_t mp = f.Alloc(0x30);
        f.W32(mp, 0x80000000u);
        f.W32(mp + 0x1C, 0x809F0081u);
        f.W32(mp + 0x24, 0x3006u);
        const std::size_t ref = f.Alloc(0x20);
        f.W32(ref, 0x20000004u);
        f.Magic(ref + 4, "TXOB");
        f.Rel(ref + 0x18, f.Str(tex));
        f.Rel(mp + 8, ref);
        f.Rel(mo + 0x274, mp);
        mat_entries.emplace_back(name, mo);
    }
    // Shapes and meshes: one triangle each, positions in the bone's space, normals, UVs.
    std::vector<std::size_t> shapes, mesh_objs;
    for (std::size_t i = 0; i < meshes.size(); i++) {
        const MeshSpec& ms = meshes[i];
        const std::size_t so = f.Alloc(0x40);
        // The vertex buffer: position (3 floats), normal (3 floats), uv (2 floats).
        const std::size_t raw = f.Alloc(3 * 32);
        for (int v = 0; v < 3; v++) {
            const std::size_t at = raw + static_cast<std::size_t>(v) * 32;
            for (int k = 0; k < 3; k++) {
                f.F(at + 4 * static_cast<std::size_t>(k), ms.tri.pos[static_cast<std::size_t>(v)][static_cast<std::size_t>(k)]);
                f.F(at + 12 + 4 * static_cast<std::size_t>(k), ms.tri.normal[static_cast<std::size_t>(k)]);
            }
            f.F(at + 24, ms.tri.uv[static_cast<std::size_t>(v)][0]);
            f.F(at + 28, ms.tri.uv[static_cast<std::size_t>(v)][1]);
        }
        std::vector<std::size_t> attrs;
        const u32 spec[3][4] = {{0, 3, 0}, {1, 3, 12}, {4, 2, 24}};
        for (const auto& a : spec) {
            const std::size_t ao = f.Alloc(0x34);
            f.W32(ao + 4, a[0]);
            f.W32(ao + 0x24, 0x1406);
            f.W32(ao + 0x28, a[1]);
            f.F(ao + 0x2C, 1.0f);
            f.W32(ao + 0x30, a[2]);
            attrs.push_back(ao);
        }
        const std::size_t vb = f.Alloc(0x30);
        f.W32(vb, 0x40000002u);
        f.W32(vb + 0x14, 3 * 32);
        f.Rel(vb + 0x18, raw);
        f.W32(vb + 0x24, 32);
        f.PtrList(vb + 0x28, attrs);
        f.PtrList(so + 0x38, {vb});
        // The submesh: its bones, its skinning, one face of one triangle (u16 indices).
        const std::size_t sm = f.Alloc(0x14);
        f.W32(sm, static_cast<u32>(ms.bones.size()));
        if (!ms.bones.empty()) {
            const std::size_t bt = f.Alloc(4 * ms.bones.size());
            for (std::size_t k = 0; k < ms.bones.size(); k++) {
                f.W32(bt + 4 * k, static_cast<u32>(ms.bones[k]));
            }
            f.Rel(sm + 4, bt);
        }
        f.W32(sm + 8, ms.skinning);
        const std::size_t idx = f.Alloc(8);
        f.W16(idx, 0);
        f.W16(idx + 2, 1);
        f.W16(idx + 4, 2);
        const std::size_t fd = f.Alloc(0x10);
        f.W32(fd, 0x1403);
        f.W8(fd + 4, 0);
        f.W32(fd + 8, 6);
        f.Rel(fd + 0xC, idx);
        const std::size_t face = f.Alloc(8);
        f.PtrList(face, {fd});
        f.PtrList(sm + 0xC, {face});
        f.PtrList(so + 0x2C, {sm});
        shapes.push_back(so);
        const std::size_t mo = f.Alloc(0x30);
        f.Magic(mo + 4, "SOBJ");
        f.Rel(mo + 0xC, f.Str("mesh" + std::to_string(i)));
        f.W32(mo + 0x18, static_cast<u32>(i));
        s32 mat = 0;
        for (std::size_t k = 0; k < materials.size(); k++) {
            if (materials[k].first == ms.material) {
                mat = static_cast<s32>(k);
            }
        }
        f.W32(mo + 0x1C, static_cast<u32>(mat));
        f.W8(mo + 0x24, 1);
        f.W16(mo + 0x26, static_cast<u16>(static_cast<s16>(ms.node)));
        mesh_objs.push_back(mo);
    }
    // The model.
    const std::size_t model = f.Alloc(0x100);
    f.W32(model, 0x40000092u);
    f.Rel(model + 0xC, f.Str("WpnTest"));
    f.PtrList(model + 0xB4, mesh_objs);
    f.W32(model + 0xBC, static_cast<u32>(materials.size()));
    f.Rel(model + 0xC0, f.Dict(mat_entries));
    f.PtrList(model + 0xC4, shapes);
    if (!nodes.empty()) {
        std::vector<std::pair<std::string, std::size_t>> node_entries;
        for (const auto& [name, visible] : nodes) {
            const std::size_t n = f.Alloc(8);
            f.Rel(n, f.Str(name));
            f.W32(n + 4, visible ? 1 : 0);
            node_entries.emplace_back(name, n);
        }
        f.W32(model + 0xCC, static_cast<u32>(nodes.size()));
        f.Rel(model + 0xD0, f.Dict(node_entries));
    }
    f.Rel(model + 0xE0, skel);
    // The texture: 8x8 RGBA8, one colour (PICA's byte order: a, b, g, r).
    const std::size_t tex = f.Alloc(0x50);
    f.Magic(tex + 4, "TXOB");
    f.W32(tex + 0x18, 8);
    f.W32(tex + 0x1C, 8);
    f.W32(tex + 0x34, 0);
    const std::size_t image = f.Alloc(0x10);
    const std::size_t pixels = f.Alloc(8 * 8 * 4);
    for (std::size_t i = 0; i < 64; i++) {
        f.b[pixels + i * 4] = 0xFF;
        f.b[pixels + i * 4 + 1] = 0x30;
        f.b[pixels + i * 4 + 2] = 0x20;
        f.b[pixels + i * 4 + 3] = 0x10;
    }
    f.W32(image, 8);
    f.W32(image + 4, 8);
    f.W32(image + 8, 256);
    f.Rel(image + 0xC, pixels);
    f.Rel(tex + 0x38, image);
    // The sections: models, textures.
    const std::size_t models = f.Dict({{"WpnTest", model}});
    const std::size_t textures = f.Dict({{"Tex", tex}});
    f.W32(0x1C, 1);
    f.Rel(0x20, models);
    f.W32(0x24, 1);
    f.Rel(0x28, textures);
    return f.b;
}

/// A claw pair (weapon 24): Pit's body, a right claw on RHandN at x -5 and a left one on
/// LHandN at x +5, a hidden mesh; the right claw's triangle is wound against its normal.
Bytes ClawZrc() {
    const Tri right{{{{0, 0, 0}, {0, 1, 0}, {1, 0, 0}}}, {0, 0, 1}}; // winding gives -z: wrong
    const Tri left{{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}}, {0, 0, 1}};
    const Tri body{{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}}, {0, 0, 1}};
    const Bytes bcres = Cgfx({{"TopN", -1, {0, 0, 0}}, {"RHandN", 0, {-5, 12, 0}},
                              {"LHandN", 0, {5, 12, 0}}},
                             {{"Body", "Tex"}, {"Wpn", "Tex"}},
                             {{"Body", {0}, 1, body, -1},
                              {"Wpn", {1}, 1, right, 0},
                              {"Wpn", {2}, 1, left, 0},
                              {"Wpn", {1}, 1, left, 1}},
                             {{"WpnN", true}, {"WpnHiddenN", false}});
    const Bytes fmdl = Darc({{"fmdl/00.bcres", bcres}});
    const Bytes js = Darc({{"json/00.json", WeaponJson("WPNID_WPN_NAIL_00", "RHandN", {-1.3f, 0, 0})}});
    return Lz11Store(Darc({{"character/fmdl.arc", Lz11Store(fmdl)}, {"character/json.arc", js}}));
}

/// AzaharFrame kiumodels: a CGFX file holding only material animations (as the class Pit files'
/// "Wpn...UV"): per animation its name, frame count and elements. An element: its target path,
/// primitive type (0 Float, 3 Vector2D, 6 RGBA), flags (per axis: bit a constant, bit axes + a
/// absent), per axis a constant or the keys (frame, value) of a curve of quantization `quant`
/// (0 Hermite128, 6 StepLinear64 linear, 7 StepLinear32 with a value scale and offset).
struct AnimElem {
    std::string path;
    u32 type = 3;
    u32 flags = 0;
    std::vector<float> constants;
    std::vector<std::vector<std::pair<float, float>>> keys;
    u32 quant = 6;
};
struct AnimSpec {
    std::string name;
    float frames;
    std::vector<AnimElem> elements;
};

Bytes AnimCgfx(const std::vector<AnimSpec>& anims, u32 revision = 0x07000000u) {
    Buf f;
    f.Alloc(0x14 + 8 + 16 * 8);
    f.Magic(0, "CGFX");
    f.W16(4, 0xFEFF);
    f.W16(6, 0x14);
    f.Magic(0x14, "DATA");
    std::vector<std::pair<std::string, std::size_t>> entries;
    for (const AnimSpec& a : anims) {
        const std::size_t ao = f.Alloc(0x28);
        f.Magic(ao, "CANM");
        f.W32(ao + 4, revision);
        f.Rel(ao + 8, f.Str(a.name));
        f.Rel(ao + 0xC, f.Str("MaterialAnimation"));
        f.W32(ao + 0x10, 1);
        f.F(ao + 0x14, a.frames);
        std::vector<std::pair<std::string, std::size_t>> elems;
        for (const AnimElem& e : a.elements) {
            const std::size_t axes = e.type == 3 ? 2 : e.type == 0 ? 1 : 4;
            const std::size_t type_at = revision <= 0x05000000u ? 0x10 : 8;
            const std::size_t eo = f.Alloc(type_at + 4 + 4 * axes);
            f.W32(eo, e.flags);
            f.Rel(eo + 4, f.Str(e.path));
            f.W32(eo + type_at, e.type);
            for (std::size_t ax = 0; ax < axes; ax++) {
                const std::size_t word = eo + type_at + 4 + 4 * ax;
                if (e.flags & (1u << (axes + ax))) {
                    continue;
                }
                if (e.flags & (1u << ax)) {
                    f.F(word, e.constants[ax]);
                    continue;
                }
                const auto& keys = e.keys[ax];
                const std::size_t key_size = e.quant == 0 ? 16 : e.quant == 6 ? 8 : 4;
                const std::size_t scaled = e.quant == 7 ? 12 : 0;
                const std::size_t c = f.Alloc(20 + scaled + key_size * keys.size());
                f.F(c, keys.front().first);
                f.F(c + 4, keys.back().first);
                f.W32(c + 8, e.quant << 5 | (e.quant >= 6 ? 4u : 8u));
                f.W32(c + 12, static_cast<u32>(keys.size()));
                f.F(c + 16, 1.0f / keys.back().first);
                std::size_t k = c + 20;
                if (e.quant == 7) { // values as hundredths from -1
                    f.F(k, 0.01f);
                    f.F(k + 4, -1.0f);
                    f.F(k + 8, 1.0f);
                    k += 12;
                }
                for (const auto& [frame, value] : keys) {
                    if (e.quant == 7) {
                        const u32 raw = static_cast<u32>(std::lround((value + 1.0f) * 100.0f));
                        f.W32(k, static_cast<u32>(frame) | raw << 12);
                    } else {
                        f.F(k, frame);
                        f.F(k + 4, value);
                    }
                    k += key_size;
                }
                const std::size_t g = f.Alloc(24);
                f.F(g + 4, keys.back().first);
                f.W32(g + 12, 4); // a quantized curve
                f.W32(g + 16, 1);
                f.Rel(g + 20, c);
                f.Rel(word, g);
            }
            elems.emplace_back(e.path, eo);
        }
        if (!elems.empty()) {
            f.W32(ao + 0x18, static_cast<u32>(elems.size()));
            f.Rel(ao + 0x1C, f.Dict(elems));
        }
        entries.emplace_back(a.name, ao);
    }
    f.W32(0x1C + 80, static_cast<u32>(entries.size())); // the 11th dictionary
    f.Rel(0x1C + 84, f.Dict(entries));
    return f.b;
}

/// A class Pit file (player/a<class>.zrc) around an animation file.
Bytes ClassZrc(const Bytes& bcres) {
    return Lz11Store(Darc({{"character/mdl.arc", Lz11Store(Darc({{"mdl/00.bcres", bcres}}))}}));
}

/// A palm (weapon 49's class) with a glow card ("WpnEfe") 4 units long across x, its UVs' u
/// along it (mirrored in the builder's sampler) or, `along_v`, its v (clamped).
Bytes PalmZrc(bool along_v, float side = 1.0f) {
    const Tri wpn{{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}}, {0, 0, 1}};
    Tri card{{{{side, 0, 0}, {5 * side, 0, 0}, {side, 1, 0}}}, {0, 0, 1}};
    card.uv = along_v ? std::array<std::array<float, 2>, 3>{{{0.1f, 0.0f}, {0.1f, 1.0f}, {0.6f, 0.0f}}}
                      : std::array<std::array<float, 2>, 3>{{{0.0f, 0.25f}, {1.0f, 0.25f}, {0.0f, 0.75f}}};
    const Bytes bcres = Cgfx({{"TopN", -1, {0, 0, 0}}, {"RHandN", 0, {-5, 12, 0}}},
                             {{"Wpn", "Tex"}, {"WpnEfe", "Tex"}},
                             {{"Wpn", {1}, 1, wpn, -1}, {"WpnEfe", {1}, 1, card, -1}});
    const Bytes fmdl = Darc({{"fmdl/00.bcres", bcres}});
    const Bytes js = Darc({{"json/00.json", WeaponJson("WPNID_WPN_PALM_01", "RHandN", {-2.9f, 0, 0})}});
    return Lz11Store(Darc({{"character/fmdl.arc", Lz11Store(fmdl)}, {"character/json.arc", js}}));
}

/// A RomFS level 3 image: root / <region> / player / the files.
Bytes RomFS(const std::string& region, const std::vector<std::pair<std::string, Bytes>>& files) {
    const auto name = [](const std::string& s) {
        Bytes b;
        for (const char c : s) {
            b.push_back(static_cast<u8>(c));
            b.push_back(0);
        }
        while (b.size() % 4) {
            b.push_back(0);
        }
        return b;
    };
    Buf dirs, metas, data;
    const auto dir_entry = [&](u32 parent, u32 sibling, u32 child, u32 file, const std::string& n) {
        const Bytes nb = name(n);
        const std::size_t at = dirs.Alloc(24 + nb.size());
        dirs.W32(at, parent);
        dirs.W32(at + 4, sibling);
        dirs.W32(at + 8, child);
        dirs.W32(at + 12, file);
        dirs.W32(at + 16, 0xFFFFFFFF);
        dirs.W32(at + 20, static_cast<u32>(n.size() * 2));
        std::memcpy(dirs.b.data() + at + 24, nb.data(), nb.size());
        return static_cast<u32>(at);
    };
    const u32 root = dir_entry(0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, "");
    const u32 reg = dir_entry(root, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, region);
    const u32 player = dir_entry(reg, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, "player");
    dirs.W32(root + 8, reg);
    dirs.W32(reg + 8, player);
    std::size_t prev = SIZE_MAX;
    for (const auto& [n, bytes] : files) {
        const Bytes nb = name(n);
        const std::size_t at = metas.Alloc(32 + nb.size());
        const std::size_t d = data.Alloc(bytes.size());
        std::memcpy(data.b.data() + d, bytes.data(), bytes.size());
        metas.W32(at, player);
        metas.W32(at + 4, 0xFFFFFFFF);
        metas.W32(at + 8, static_cast<u32>(d));
        metas.W32(at + 16, static_cast<u32>(bytes.size()));
        metas.W32(at + 24, 0xFFFFFFFF);
        metas.W32(at + 28, static_cast<u32>(n.size() * 2));
        std::memcpy(metas.b.data() + at + 32, nb.data(), nb.size());
        if (prev == SIZE_MAX) {
            dirs.W32(player + 12, static_cast<u32>(at));
        } else {
            metas.W32(prev + 4, static_cast<u32>(at));
        }
        prev = at;
    }
    Buf img;
    img.Alloc(0x28);
    const std::size_t d0 = img.Alloc(dirs.b.size());
    std::memcpy(img.b.data() + d0, dirs.b.data(), dirs.b.size());
    const std::size_t f0 = img.Alloc(metas.b.size());
    std::memcpy(img.b.data() + f0, metas.b.data(), metas.b.size());
    const std::size_t data0 = img.Alloc(data.b.size());
    std::memcpy(img.b.data() + data0, data.b.data(), data.b.size());
    img.W32(0, 0x28);
    img.W32(0x0C, static_cast<u32>(d0));
    img.W32(0x10, static_cast<u32>(dirs.b.size()));
    img.W32(0x1C, static_cast<u32>(f0));
    img.W32(0x20, static_cast<u32>(metas.b.size()));
    img.W32(0x24, static_cast<u32>(data0));
    return img.b;
}

class MemoryRomFS : public FileSys::RomFSReader {
public:
    explicit MemoryRomFS(Bytes b) : bytes(std::move(b)) {}
    std::size_t GetSize() const override {
        return bytes.size();
    }
    std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer) override {
        if (offset > bytes.size()) {
            return 0;
        }
        const std::size_t n = std::min(length, bytes.size() - offset);
        std::memcpy(buffer, bytes.data() + offset, n);
        return n;
    }
    bool AllowsCachedReads() const override {
        return false;
    }
    bool CacheReady(std::size_t, std::size_t) override {
        return false;
    }

private:
    Bytes bytes;
};

std::string TestDir(const char* name) {
    const auto p = std::filesystem::current_path() / name;
    std::filesystem::remove_all(p);
    std::filesystem::create_directories(p);
    return p.string() + "/";
}

} // namespace

TEST_CASE("LZ11: literals, back references, and bad streams", "[kiuguns]") {
    // "abc" then a reference 3 back, 6 long: "abcabcabc".
    const Bytes stream{0x11, 9, 0, 0, 0x10, 'a', 'b', 'c', 0x50, 0x02};
    std::vector<u8> out;
    REQUIRE(Common::DecompressLZ11(stream.data(), stream.size(), out));
    CHECK(std::string(out.begin(), out.end()) == "abcabcabc");
    // A reference before the start, a stream cut short, not LZ11, a buffer too small.
    const Bytes before{0x11, 4, 0, 0, 0x40, 'a', 0x20, 0x05};
    CHECK_FALSE(Common::DecompressLZ11(before.data(), before.size(), out));
    for (std::size_t n = 0; n < stream.size(); n++) {
        CHECK_FALSE(Common::DecompressLZ11(stream.data(), n, out));
    }
    const Bytes other{0x10, 3, 0, 0, 0, 'a', 'b', 'c'};
    CHECK_FALSE(Common::DecompressLZ11(other.data(), other.size(), out));
    u8 small[4];
    CHECK(Common::DecompressLZ11(stream.data(), stream.size(), small, sizeof(small)) == 0);
    const Bytes data{'x', 'y', 'z', 1, 2, 3, 4, 5, 6, 7, 8, 9};
    REQUIRE(Common::DecompressLZ11(Lz11Store(data).data(), Lz11Store(data).size(), out));
    CHECK(out == data);
}

TEST_CASE("darc: nested members, LZ11 on the way, missing ones", "[kiuguns]") {
    const Bytes inner = Darc({{"json/00.json", {'P', 'S'}}, {"json/01.json", {'Q'}}});
    const Bytes outer = Lz11Store(Darc({{"character/json.arc", Lz11Store(inner)},
                                        {"character/00.snd", {1, 2, 3}}}));
    std::vector<FrameCgfx::DarcEntry> entries;
    std::string error;
    REQUIRE(FrameCgfx::ReadDarc(inner, entries, error));
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].path == "json/00.json");
    Bytes member;
    REQUIRE(FrameCgfx::DarcMember(outer, "character/json.arc!json/01.json", member, error));
    CHECK(member == Bytes{'Q'});
    REQUIRE(FrameCgfx::DarcMember(outer, "./character/00.snd", member, error));
    CHECK(member == Bytes{1, 2, 3});
    CHECK_FALSE(FrameCgfx::DarcMember(outer, "character/json.arc!json/02.json", member, error));
    // Cut short anywhere: an error, never a crash.
    const Bytes plain = Darc({{"a/b", {1, 2, 3, 4}}});
    for (std::size_t n = 0; n < plain.size(); n += 3) {
        Bytes cut(plain.begin(), plain.begin() + static_cast<std::ptrdiff_t>(n));
        FrameCgfx::DarcMember(cut, "a/b", member, error);
    }
}

TEST_CASE("PSLB: objects, arrays, numbers", "[kiuguns]") {
    const Bytes js = WeaponJson("WPNID_WPN_RIFLE_00", "BodyN", {-4.5f, 0.0f, -7.8f});
    FrameCgfx::PslbValue root;
    std::string error;
    REQUIRE(FrameCgfx::ReadPslb(js, root, error));
    const auto* list = root.Get("weaponParams_");
    REQUIRE(list);
    REQUIRE(list->array.size() == 1);
    CHECK(list->array[0].Get("weaponID_")->string == "WPNID_WPN_RIFLE_00");
    const auto& params = list->array[0].Get("params_")->array;
    REQUIRE(params.size() == 2);
    CHECK(*params[0].Get("muzzleOffset_")->array[2].Number() == Approx(-7.8f));
    CHECK(*params[1].Get("chargeTime_")->array[0].Number() == Approx(72.0f));
    for (std::size_t n = 0; n < js.size(); n++) {
        Bytes cut(js.begin(), js.begin() + static_cast<std::ptrdiff_t>(n));
        CHECK_FALSE(FrameCgfx::ReadPslb(cut, root, error));
    }
}

TEST_CASE("RomFS level 3: the tree and the file offsets", "[kiuguns]") {
    const Bytes img = RomFS("eu", {{"m000.zrc", {1, 2, 3}}, {"m001.zrc", {4, 5}}});
    std::vector<FrameCgfx::RomFSIndex::File> files;
    std::string error;
    REQUIRE(FrameCgfx::ListRomFS(img, files, error));
    REQUIRE(files.size() == 2);
    CHECK(files[0].path == "eu/player/m000.zrc");
    CHECK(files[1].path == "eu/player/m001.zrc");
    CHECK(files[1].size == 2);
    CHECK(img[files[1].offset] == 4);
    FrameCgfx::RomFSIndex index;
    REQUIRE(index.Open(std::make_shared<MemoryRomFS>(img), error));
    Bytes out;
    REQUIRE(index.Find("/eu/player/m001.zrc"));
    REQUIRE(index.Read(*index.Find("eu/player/m001.zrc"), out));
    CHECK(out == Bytes{4, 5});
    CHECK_FALSE(FrameCgfx::ListRomFS(Bytes(0x10, 0), files, error));
}

TEST_CASE("CGFX: bones, materials, meshes, textures", "[kiuguns]") {
    Bytes bcres;
    std::string error;
    REQUIRE(FrameCgfx::DarcMember(ClawZrc(), "character/fmdl.arc!fmdl/00.bcres", bcres, error));
    FrameCgfx::File file;
    const bool read = FrameCgfx::ReadCgfx(bcres, file, error);
    INFO(error);
    REQUIRE(read);
    REQUIRE(file.models.size() == 1);
    const auto& m = file.models[0];
    CHECK(m.name == "WpnTest");
    REQUIRE(m.bones.size() == 3);
    CHECK(m.bones[1].name == "RHandN");
    CHECK(m.bones[1].bind[3] == Approx(-5.0f)); // the inverse's inverse: the bone at x -5
    REQUIRE(m.materials.size() == 2);
    CHECK(m.materials[1].mappers[0].texture == "Tex");
    CHECK(m.materials[1].mappers[0].wrap_u == 3); // mirrored across
    CHECK(m.materials[1].mappers[0].wrap_v == 0); // clamped down
    CHECK(m.materials[1].cull == 2);
    REQUIRE(m.meshes.size() == 4);
    CHECK(m.MeshShown(m.meshes[1]));
    CHECK_FALSE(m.MeshShown(m.meshes[3]));
    REQUIRE(file.textures.size() == 1);
    CHECK(file.textures[0].rgba[0] == 0x10);
    CHECK(file.textures[0].rgba[1] == 0x20);
    CHECK(file.textures[0].rgba[2] == 0x30);
    // Every cut of the file: an error, never a crash.
    for (std::size_t n = 0; n + 8 < bcres.size(); n += 7) {
        Bytes cut(bcres.begin(), bcres.begin() + static_cast<std::ptrdiff_t>(n));
        CHECK_FALSE(FrameCgfx::ReadCgfx(cut, file, error));
    }
}

TEST_CASE("Weapon models: only the weapon, each claw in its own hand's space", "[kiuguns]") {
    FrameWeapons::Converted c;
    std::string error;
    REQUIRE(FrameWeapons::ConvertWeapon(24, ClawZrc(), c, error));
    CHECK(c.info.cls == FrameWeapons::kClaws);
    CHECK(c.info.wpnid == "WPNID_WPN_NAIL_00");
    CHECK(c.info.anchor == "RHandN");
    REQUIRE(c.info.muzzle_valid);
    CHECK(c.info.muzzle[0] == Approx(-1.3f));
    CHECK(c.info.charge_time[1] == Approx(80.0f));
    REQUIRE(c.files.size() == 1); // Body and the hidden mesh left out
    REQUIRE(c.info.parts.size() == 1);
    const auto& part = c.info.parts[0];
    CHECK(part.file == "w024.model");
    CHECK(part.material == "Wpn");
    CHECK(part.triangles == 2);
    CHECK(part.cull == 2);
    REQUIRE(part.sides.size() == 2);

    const std::string dir = TestDir("frame_weapon_models_test");
    {
        std::ofstream o(dir + part.file, std::ios::binary);
        o.write(reinterpret_cast<const char*>(c.files[0].second.data()),
                static_cast<std::streamsize>(c.files[0].second.size()));
    }
    FrameXr::Vis::ModelFile mf;
    CHECK_FALSE(FrameXr::Vis::LoadModelFile(dir + part.file, mf, error)); // not a controller
    REQUIRE(FrameXr::Vis::LoadModelFile(dir + part.file, mf, error,
                                        FrameXr::Vis::kModelFlagWeaponSpace));
    CHECK(FrameXr::Vis::ModelWrapU(mf.flags) == 3);
    CHECK(FrameXr::Vis::ModelWrapV(mf.flags) == 0);
    REQUIRE(mf.components.size() == 2);
    // Each half in its own hand's space: the bone-local positions as given.
    for (const auto& comp : mf.components) {
        CHECK((comp.name == "R" || comp.name == "L"));
        CHECK(comp.index_count == 3);
        float maxx = -1e9f;
        for (uint32_t i = comp.first_index; i < comp.first_index + comp.index_count; i++) {
            maxx = std::max(maxx, mf.vertices[mf.indices[i] * 12]);
        }
        CHECK(maxx == Approx(1.0f));
        // Wound to agree with the normal (+z), the right half's turned around.
        const float* a = &mf.vertices[mf.indices[comp.first_index] * 12];
        const float* b = &mf.vertices[mf.indices[comp.first_index + 1] * 12];
        const float* d = &mf.vertices[mf.indices[comp.first_index + 2] * 12];
        const float z = (b[0] - a[0]) * (d[1] - a[1]) - (b[1] - a[1]) * (d[0] - a[0]);
        CHECK(z > 0.0f);
        CHECK(a[5] == Approx(1.0f));
    }
    // UVs: v flipped for the GPU (1 - v).
    CHECK(mf.vertices[11] == Approx(0.75f));
    CHECK(mf.colour[0] == 0x10);
    CHECK(mf.colour[3] == 255);
    // One surface texel (kModelFlagFlatSurface), not a constant image the colour's size.
    CHECK((mf.flags & FrameXr::Vis::kModelFlagFlatSurface) != 0);
    CHECK(mf.surface.size() == 4);
    CHECK(mf.SurfaceWidth() == 1);
    std::filesystem::remove_all(dir);
}

TEST_CASE("Weapon models: a game's folder, kept until the file changes", "[kiuguns]") {
    const std::string dir = TestDir("frame_weapon_game_test");
    const Bytes zrc = ClawZrc();
    auto rom = std::make_shared<MemoryRomFS>(RomFS("us", {{"m200.zrc", zrc}}));
    FrameWeapons::RunResult r = FrameWeapons::ConvertGame(rom, 0x0004000000030100ull, dir);
    CHECK(r.ok);
    CHECK(r.region == "us");
    CHECK(r.made == 1);
    CHECK(r.failed == FrameWeapons::kWeapons - 1);
    FrameWeapons::Manifest m;
    std::string error;
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    CHECK(m.title_id == 0x0004000000030100ull);
    CHECK(m.version == FrameWeapons::kConverterVersion);
    REQUIRE(m.Find(24));
    CHECK(m.Find(24)->parts[0].sides.size() == 2);
    CHECK(m.Find(24)->muzzle_valid);
    // Again: kept. A changed file (a mod): made again.
    r = FrameWeapons::ConvertGame(rom, 0x0004000000030100ull, dir);
    CHECK(r.kept == 1);
    CHECK(r.made == 0);
    Bytes modded = zrc;
    modded.push_back(0);
    rom = std::make_shared<MemoryRomFS>(RomFS("us", {{"m200.zrc", modded}}));
    r = FrameWeapons::ConvertGame(rom, 0x0004000000030100ull, dir);
    CHECK(r.made == 1);
    // Not the game's files.
    rom = std::make_shared<MemoryRomFS>(RomFS("us", {{"other.bin", {1}}}));
    r = FrameWeapons::ConvertGame(rom, 0x0004000000030100ull, dir);
    CHECK_FALSE(r.ok);
    CHECK(FrameWeapons::ZrcName(24) == "m200.zrc");
    CHECK(FrameWeapons::ZrcName(107) == "m811.zrc");
    std::filesystem::remove_all(dir);
}

TEST_CASE("Weapon models: the texture pack's replacement, found by the game's own hash",
          "[kiuguns]") {
    // The hash is CityHash64 of the raw bytes (the empty input: CityHash64's own constant).
    CHECK(VideoCore::CustomTexManager::NewHash({}) == 0x9AE16A3B2F90404Full);
    // The claws' texture: 8x8 RGBA8, every texel the bytes ff 30 20 10 as the GPU reads them.
    Bytes raw;
    for (int i = 0; i < 64; i++) {
        raw.insert(raw.end(), {0xFF, 0x30, 0x20, 0x10});
    }
    const u64 hash = VideoCore::CustomTexManager::NewHash(raw);
    CHECK(hash == Common::ComputeHash64<Common::HashAlgo64::CityHash>(raw.data(), raw.size()));

    // A pack in a load folder of its own: a 16x16 replacement, named as Azahar dumps it.
    const std::string root = TestDir("frame_weapon_pack_test");
    const std::string before = FileUtil::GetUserPath(FileUtil::UserPath::LoadDir);
    FileUtil::UpdateUserPath(FileUtil::UserPath::LoadDir, root);
    constexpr u64 kTitle = 0x0004000000030200ull;
    const std::string pack_dir = fmt::format("{}textures/{:016X}/", root, kTitle);
    std::filesystem::create_directories(pack_dir + "weapons");
    {
        std::ofstream o(pack_dir + "pack.json");
        o << R"({"author": "test", "options": {"use_new_hash": true, "flip_png_files": true}})";
    }
    Frontend::ImageInterface images;
    const auto write_png = [&](u8 green) {
        Bytes px;
        for (int i = 0; i < 16 * 16; i++) {
            px.insert(px.end(), {0, green, 0, 255});
        }
        px[1] = 7; // the first row's first texel: the top of the picture
        REQUIRE(images.EncodePNG(fmt::format("{}weapons/tex1_8x8_{:016X}_0_mip0.png", pack_dir, hash),
                                 16, 16, px));
    };
    write_png(200);
    FrameWeapons::TexturePack pack = FrameWeapons::ReadTexturePack(kTitle);
    REQUIRE(pack.on);
    REQUIRE(pack.files.count(hash) == 1);

    FrameWeapons::Converted c;
    std::string error;
    REQUIRE(FrameWeapons::ConvertWeapon(24, ClawZrc(), c, error, &pack));
    REQUIRE(c.info.texture_hashes.size() == 1);
    CHECK(c.info.texture_hashes[0] == hash);
    // The pack's file is named, not copied: the model keeps the game's 8x8 texture.
    const std::string png = fmt::format("{}weapons/tex1_8x8_{:016X}_0_mip0.png", pack_dir, hash);
    CHECK(std::filesystem::equivalent(c.info.parts[0].replaced, png));
    CHECK(c.info.parts[0].replaced_flip);
    CHECK(c.info.pack_key != 0);
    const std::string dir = TestDir("frame_weapon_pack_models");
    {
        std::ofstream o(dir + "w.model", std::ios::binary);
        o.write(reinterpret_cast<const char*>(c.files[0].second.data()),
                static_cast<std::streamsize>(c.files[0].second.size()));
    }
    FrameXr::Vis::ModelFile mf;
    REQUIRE(FrameXr::Vis::LoadModelFile(dir + "w.model", mf, error,
                                        FrameXr::Vis::kModelFlagWeaponSpace));
    CHECK(mf.tex_width == 8);
    CHECK(mf.colour[0] == 0x10);
    // Read from the pack when the model loads, at its own size.
    std::vector<u8> rgba;
    u32 w = 0, h = 0;
    REQUIRE(FrameWeapons::LoadPackTexture(c.info.parts[0].replaced, true, rgba, w, h));
    CHECK(w == 16);
    CHECK(h == 16);
    CHECK(rgba[1] == 7);       // flip_png_files: the PNG's first row is the texture's first
    CHECK(rgba[4 + 1] == 200);
    CHECK(rgba[3] == 255);
    REQUIRE(FrameWeapons::LoadPackTexture(c.info.parts[0].replaced, false, rgba, w, h));
    CHECK(rgba[1] == 200); // not flipped by the pack: turned over here
    CHECK(rgba[15 * 16 * 4 + 1] == 7);

    // A game's run keeps it while the pack's file is the same, and makes it again once not.
    auto rom = std::make_shared<MemoryRomFS>(RomFS("eu", {{"m200.zrc", ClawZrc()}}));
    FrameWeapons::RunResult r = FrameWeapons::ConvertGame(rom, kTitle, dir, nullptr, &pack);
    CHECK(r.made == 1);
    CHECK(r.replaced == 1);
    r = FrameWeapons::ConvertGame(rom, kTitle, dir, nullptr, &pack);
    CHECK(r.kept == 1);
    // Nothing of the pack in the folder: the models hold the game's small textures only.
    std::uintmax_t bytes = 0;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        bytes += e.file_size();
    }
    CHECK(bytes < 8 * 1024);
    std::filesystem::remove(fmt::format("{}weapons/tex1_8x8_{:016X}_0_mip0.png", pack_dir, hash));
    write_png(100);
    std::filesystem::last_write_time(
        fmt::format("{}weapons/tex1_8x8_{:016X}_0_mip0.png", pack_dir, hash),
        std::filesystem::file_time_type::clock::now() + std::chrono::seconds(5));
    pack = FrameWeapons::ReadTexturePack(kTitle);
    r = FrameWeapons::ConvertGame(rom, kTitle, dir, nullptr, &pack);
    CHECK(r.made == 1);
    // No pack: made again with the game's own texture.
    r = FrameWeapons::ConvertGame(rom, kTitle, dir, nullptr, nullptr);
    CHECK(r.made == 1);
    CHECK(r.replaced == 0);

    FileUtil::UpdateUserPath(FileUtil::UserPath::LoadDir, before);
    std::filesystem::remove_all(root);
    std::filesystem::remove_all(dir);
}

// ---- AzaharFrame kiumodels: the texture's motion ----

TEST_CASE("CGFX: the material animations' texture coordinator tracks", "[kiumodels]") {
    const std::string efe0 = "Materials[\"WpnEfe\"].TextureCoordinators[0].Translate";
    AnimElem slide{efe0, 3, 0x4, {}, {{}, {{0.0f, 0.0f}, {160.0f, -2.0f}}}};      // u absent
    AnimElem turn{"Materials[\"WpnR90\"].TextureCoordinators[1].Rotate", 0, 0, {},
                  {{{0.0f, 0.0f}, {40.0f, 3.0f}, {90.0f, 6.2831855f}}}, 0};        // Hermite, 3 keys
    AnimElem held{"Materials[\"Wpn\"].TextureCoordinators[2].Translate", 3, 0x3, {0.5f, 0.25f}};
    AnimElem packed{"Materials[\"WpnT\"].TextureCoordinators[0].Translate", 3, 0x8,
                    {}, {{{0.0f, 0.0f}, {30.0f, 1.0f}}, {}}, 7};                    // v absent
    AnimElem colour{"Materials[\"WpnEfe\"].MaterialColor.Emission", 6, 0xF0, {}, {}};
    AnimElem other{"Materials[\"WpnEfe\"].TextureCoordinators[0].Scale", 3, 0x3, {1.0f, 2.0f}};
    const Bytes bcres = AnimCgfx({{"WpnPalmUV", 160.0f, {slide, turn, held, colour, other}},
                                  {"WpnLaunchUV", 30.0f, {packed}}});
    std::vector<FrameCgfx::UvAnim> anims;
    std::string error;
    const bool read = FrameCgfx::ReadUvAnims(bcres, anims, error);
    INFO(error);
    REQUIRE(read);
    REQUIRE(anims.size() == 4); // the colour and the scale left out
    const auto find = [&](const std::string& material) -> const FrameCgfx::UvAnim& {
        for (const auto& a : anims) {
            if (a.material == material) {
                return a;
            }
        }
        FAIL("no " << material);
        return anims[0];
    };
    const auto& efe = find("WpnEfe");
    CHECK(efe.animation == "WpnPalmUV");
    CHECK(efe.coord == 0);
    CHECK(efe.frames == 160.0f);
    CHECK(efe.loops);
    CHECK(efe.translate_u == 0.0f);
    CHECK(efe.translate_v == Approx(-2.0f));
    CHECK(efe.linear);
    const auto& r90 = find("WpnR90");
    CHECK(r90.coord == 1);
    CHECK(r90.rotate == Approx(6.2831855f));
    CHECK_FALSE(r90.linear); // three Hermite keys: the change is all that is kept
    const auto& wpn = find("Wpn");
    CHECK(wpn.coord == 2);
    CHECK(wpn.translate_u == 0.0f);
    CHECK(wpn.translate_v == 0.0f);
    const auto& t = find("WpnT");
    CHECK(t.animation == "WpnLaunchUV");
    CHECK(t.frames == 30.0f);
    CHECK(t.translate_u == Approx(1.0f).margin(1e-4)); // hundredths from -1: 0 to 1
    CHECK(t.translate_v == 0.0f);
    // An older file (a second name and a word before each element's type).
    const Bytes old = AnimCgfx({{"WpnPalmUV", 160.0f, {slide}}}, 0x05000000u);
    REQUIRE(FrameCgfx::ReadUvAnims(old, anims, error));
    REQUIRE(anims.size() == 1);
    CHECK(anims[0].translate_v == Approx(-2.0f));
    // None at all: an empty list. Every cut of the file: an error, never a crash.
    REQUIRE(FrameCgfx::ReadUvAnims(AnimCgfx({}), anims, error));
    CHECK(anims.empty());
    for (std::size_t n = 0; n + 4 < bcres.size(); n += 5) {
        Bytes cut(bcres.begin(), bcres.begin() + static_cast<std::ptrdiff_t>(n));
        FrameCgfx::ReadUvAnims(cut, anims, error);
    }
    CHECK_FALSE(FrameCgfx::ReadUvAnims(Bytes(8, 0), anims, error));
}

TEST_CASE("Weapon models: a glow's texture moves as the game's, or flows along it", "[kiumodels]") {
    // The game's: the class file's animation of the part's material and coordinator; the game
    // samples at scale * (uv - translate), the files' v is 1 - v.
    std::vector<FrameCgfx::UvAnim> anims(3);
    anims[0].animation = "WpnPalmUV";
    anims[0].material = "WpnEfe";
    anims[0].frames = 160.0f;
    anims[0].translate_v = -2.0f;
    anims[1] = anims[0];
    anims[1].coord = 1; // another coordinator: not this part's
    anims[1].translate_u = 2.0f;
    anims[2] = anims[0];
    anims[2].material = "Wpn";
    anims[2].translate_v = 0.0f;
    anims[2].translate_u = 0.5f;
    FrameWeapons::UvMotion m = FrameWeapons::GameUvMotion(anims, "WpnEfe", 0, 1.0f, 2.0f);
    CHECK(m.source == FrameWeapons::UvSource::Game);
    CHECK(m.frames == 160.0f);
    CHECK(m.du == 0.0f);
    CHECK(m.dv == Approx(-4.0f));
    m = FrameWeapons::GameUvMotion(anims, "Wpn", 0, 1.0f, 1.0f);
    CHECK(m.du == Approx(-0.5f));
    CHECK(FrameWeapons::GameUvMotion(anims, "WpnT", 0, 1.0f, 1.0f).source ==
          FrameWeapons::UvSource::None);

    FrameWeapons::Converted c;
    std::string error;
    REQUIRE(FrameWeapons::ConvertWeapon(49, PalmZrc(false), c, error, nullptr, &anims));
    REQUIRE(c.info.parts.size() == 2);
    CHECK_FALSE(c.info.parts[0].glow);
    CHECK(c.info.parts[0].uv.source == FrameWeapons::UvSource::Game); // Wpn's, kept (not drawn)
    const FrameWeapons::Part& efe = c.info.parts[1];
    REQUIRE(efe.glow);
    CHECK(efe.material == "WpnEfe");
    CHECK(efe.uv.source == FrameWeapons::UvSource::Game);
    CHECK(efe.uv.dv == Approx(-2.0f));

    // No game animation: the card flows along its length (x), on u (mirrored: two units a
    // loop), away from the hand: its picture moves toward +x, so u's offset falls.
    REQUIRE(FrameWeapons::ConvertWeapon(49, PalmZrc(false), c, error));
    CHECK(c.info.parts[0].uv.source == FrameWeapons::UvSource::None); // not a glow: still
    CHECK(c.info.parts[1].uv.source == FrameWeapons::UvSource::Flow);
    CHECK(c.info.parts[1].uv.frames == FrameWeapons::kFlowFrames);
    CHECK(c.info.parts[1].uv.du == -2.0f);
    CHECK(c.info.parts[1].uv.dv == 0.0f);
    // The card on the other side of the hand (-x), its u growing outward: still outward.
    REQUIRE(FrameWeapons::ConvertWeapon(49, PalmZrc(false, -1.0f), c, error));
    CHECK(c.info.parts[1].uv.du == -2.0f);
    // Its length along v, which is clamped: no flow (it would smear the edge).
    REQUIRE(FrameWeapons::ConvertWeapon(49, PalmZrc(true), c, error));
    CHECK(c.info.parts[1].uv.source == FrameWeapons::UvSource::None);
}

TEST_CASE("Weapon models: the class file's animations in a game's run, the manifest's lines",
          "[kiumodels]") {
    const std::string dir = TestDir("frame_weapon_uv_test");
    // The claws' class file: the weapon's "Wpn" slides (v, 1 a loop of 30 frames); Pit's own
    // animation of the same material is not the weapon's.
    AnimElem wpn{"Materials[\"Wpn\"].TextureCoordinators[0].Translate", 3, 0x4, {},
                 {{}, {{0.0f, 0.0f}, {30.0f, 1.0f}}}};
    AnimElem talk{"Materials[\"Wpn\"].TextureCoordinators[0].Translate", 3, 0x8, {},
                  {{{0.0f, 0.0f}, {10.0f, 5.0f}}, {}}};
    const Bytes a2 = ClassZrc(AnimCgfx({{"WpnNailUV", 30.0f, {wpn}}, {"ChrPit00TalkUV00", 10.0f, {talk}}}));
    std::vector<FrameCgfx::UvAnim> anims;
    std::string error;
    REQUIRE(FrameWeapons::ReadClassUvAnims(a2, anims, error));
    REQUIRE(anims.size() == 1);
    CHECK(FrameWeapons::ClassZrcName(2) == "a2.zrc");
    auto rom = std::make_shared<MemoryRomFS>(
        RomFS("eu", {{"m200.zrc", ClawZrc()}, {"a2.zrc", a2}, {"m401.zrc", PalmZrc(false)}}));
    FrameWeapons::RunResult r = FrameWeapons::ConvertGame(rom, 0x0004000000030200ull, dir);
    CHECK(r.made == 2);
    FrameWeapons::Manifest m;
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    CHECK(m.version == 4);
    REQUIRE(m.Find(24));
    const FrameWeapons::UvMotion& claw = m.Find(24)->parts[0].uv;
    CHECK(claw.source == FrameWeapons::UvSource::Game);
    CHECK(claw.frames == 30.0f);
    CHECK(claw.du == 0.0f);
    CHECK(claw.dv == Approx(1.0f));
    // The palm's class file is missing: its glow card flows.
    REQUIRE(m.Find(49));
    CHECK(m.Find(49)->parts[1].uv.source == FrameWeapons::UvSource::Flow);
    CHECK(m.Find(49)->parts[1].uv.du == -2.0f);
    {
        std::ifstream in(dir + "weapons.txt");
        const std::string text{std::istreambuf_iterator<char>(in), {}};
        CHECK(text.find("\nuvanim w024.model 1 30 0 1 0\n") != std::string::npos);
        CHECK(text.find("\nuvanim w049-1.model 2 75 -2 0 0\n") != std::string::npos);
    }
    // A bad uvanim line is dropped (the part stays still); an unknown file's is ignored.
    {
        std::ifstream in(dir + "weapons.txt");
        std::string text{std::istreambuf_iterator<char>(in), {}};
        const auto at = text.find("uvanim w024.model 1 30 0 1 0");
        text.replace(at, std::strlen("uvanim w024.model 1 30 0 1 0"), "uvanim w024.model 1 30 nan 1 0");
        text += "uvanim w999.model 1 30 0 1 0\n";
        std::ofstream(dir + "bad.txt") << text;
    }
    REQUIRE(FrameWeapons::ReadManifest(dir + "bad.txt", m, error));
    CHECK(m.Find(24)->parts[0].uv.source == FrameWeapons::UvSource::None);
    CHECK(m.Find(49)->parts[1].uv.source == FrameWeapons::UvSource::Flow);
    // A version 3 folder (before the motion): every weapon made again, once.
    {
        std::ifstream in(dir + "weapons.txt");
        std::string text{std::istreambuf_iterator<char>(in), {}};
        text.replace(text.find("kiu-weapons 4"), 13, "kiu-weapons 3");
        std::ofstream(dir + "weapons.txt") << text;
    }
    r = FrameWeapons::ConvertGame(rom, 0x0004000000030200ull, dir);
    CHECK(r.made == 2);
    r = FrameWeapons::ConvertGame(rom, 0x0004000000030200ull, dir);
    CHECK(r.made == 0);
    CHECK(r.kept == 2);
    std::filesystem::remove_all(dir);
}

// The game's own files (the study's copy: AZAHARFRAME_TEST_KIU_ZRC, a folder of player/*.zrc)
// made into AZAHARFRAME_TEST_KIU_OUT, every weapon, and each glow part's motion printed.
TEST_CASE("Weapon models: every weapon of a game's files, the glows' motion", "[.kiumodels_real]") {
    const char* zrc = std::getenv("AZAHARFRAME_TEST_KIU_ZRC");
    const char* out = std::getenv("AZAHARFRAME_TEST_KIU_OUT");
    if (!zrc || !out) {
        SKIP("AZAHARFRAME_TEST_KIU_ZRC and AZAHARFRAME_TEST_KIU_OUT");
    }
    std::vector<std::pair<std::string, Bytes>> files;
    for (const auto& e : std::filesystem::directory_iterator(zrc)) {
        const std::string name = e.path().filename().string();
        // The weapons' m<class><index>.zrc and the classes' Pit files a<class>.zrc.
        if ((name.size() == 8 && name[0] == 'm') || (name.size() == 6 && name[0] == 'a')) {
            std::ifstream in(e.path(), std::ios::binary);
            files.emplace_back(name, Bytes{std::istreambuf_iterator<char>(in), {}});
        }
    }
    std::sort(files.begin(), files.end());
    auto rom = std::make_shared<MemoryRomFS>(RomFS("eu", files));
    std::string dir = out;
    if (dir.back() != '/') {
        dir += '/';
    }
    const auto r = FrameWeapons::ConvertGame(rom, 0x0004000000030200ull, dir);
    CHECK(r.made + r.kept == FrameWeapons::kWeapons);
    FrameWeapons::Manifest m;
    std::string error;
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    for (const auto& w : m.weapons) {
        for (const auto& p : w.parts) {
            if (p.uv.source != FrameWeapons::UvSource::None) {
                std::printf("weapon %d %s %s glow %d: %s, %g frames, du %g dv %g turns %g\n", w.id,
                            p.file.c_str(), p.material.c_str(), p.glow ? 1 : 0,
                            p.uv.source == FrameWeapons::UvSource::Game ? "game" : "flow",
                            p.uv.frames, p.uv.du, p.uv.dv, p.uv.turns);
            } else if (p.glow) {
                std::printf("weapon %d %s %s glow 1: still (wrap %u %u)\n", w.id, p.file.c_str(),
                            p.material.c_str(), p.wrap_u, p.wrap_v);
            }
        }
    }
}
