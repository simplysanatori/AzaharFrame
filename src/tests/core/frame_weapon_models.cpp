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
#include <functional>
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
/// AzaharFrame kiuflash: an effect material's state: added light (source alpha, one) with one
/// combiner stage, the texture times the constant colour 0.
struct AddedLight {
    bool on = false;
    std::array<float, 4> constant{1, 1, 1, 1};
};

Bytes Cgfx(const std::vector<std::tuple<std::string, s32, std::array<float, 3>>>& bones,
           const std::vector<std::pair<std::string, std::string>>& materials,
           const std::vector<MeshSpec>& meshes,
           const std::vector<std::pair<std::string, bool>>& nodes = {},
           const std::string& model_name = "WpnTest", const AddedLight& added = {}) {
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
        // AzaharFrame kiuweap2: its own transform: scale 1, no turn, its offset from its parent.
        for (std::size_t k = 0; k < 3; k++) {
            const float from = parent >= 0 ? std::get<2>(bones[static_cast<std::size_t>(parent)])[k]
                                           : 0.0f;
            f.F(bo + 0x20 + 4 * k, 1.0f);
            f.F(bo + 0x38 + 4 * k, t[k] - from);
        }
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
        const std::size_t mo = f.Alloc(0x300);
        if (added.on) {
            // The constant 0 (colour 5), the blend (source alpha, one) and one stage: texture 0
            // times the constant, the texture's alpha.
            for (std::size_t k = 0; k < 4; k++) {
                f.F(mo + 0x24 + 5 * 16 + 4 * k, added.constant[k]);
            }
            f.W32(mo + 0x140, 1u << 8);
            f.W32(mo + 0x148, (6u << 16) | (1u << 20));
            const std::size_t fs = f.Alloc(0x40);
            f.W32(fs + 0x10, 0);                       // the constant: 0
            f.W32(fs + 0x14, 0x3u | (0xEu << 4) | (0x3u << 16)); // sources: tex0, const; tex0
            f.W32(fs + 0x18, (4u << 20) | 0xC0u);      // stage 0's command, four more words
            f.W32(fs + 0x1C, 0);                       // operands
            f.W32(fs + 0x20, 1u);                      // combine: modulate colour, replace alpha
            f.W32(fs + 0x28, 0);                       // scale 1
            f.Rel(mo + 0x288, fs);
        }
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
    f.Rel(model + 0xC, f.Str(model_name));
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
    const std::size_t models = f.Dict({{model_name, model}});
    const std::size_t textures = f.Dict({{"Tex", tex}});
    f.W32(0x1C, 1);
    f.Rel(0x20, models);
    f.W32(0x24, 1);
    f.Rel(0x28, textures);
    return f.b;
}

/// A claw pair (weapon 24): Pit's body, a right claw on RHandN at x -5 and a left one on
/// LHandN at x +5, a hidden mesh; the right claw's triangle is wound against its normal.
/// AzaharFrame kiuflash: with `effect` as its character/effect.bcres when given.
Bytes ClawZrc(const Bytes& effect = {}) {
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
    if (!effect.empty()) {
        return Lz11Store(Darc({{"character/fmdl.arc", Lz11Store(fmdl)},
                               {"character/json.arc", js},
                               {"character/effect.bcres", effect}}));
    }
    return Lz11Store(Darc({{"character/fmdl.arc", Lz11Store(fmdl)}, {"character/json.arc", js}}));
}

// ---- AzaharFrame kiuflash: an effect file of particle models ----

struct TexSpec {
    std::string name;
    u32 width, height;
    /// Each 8x8 tile's colour (r, g, b, a) by its column and row.
    std::function<std::array<u8, 4>(u32, u32)> tile;
};

struct SetSpec {
    std::string name, texture;
    u32 colour, sampler, type;
    float size_x, size_y;
    bool updater; ///< the size as a scale updater (else a constant scale attribute)
    float uv_x = 0.0f, uv_y = 0.0f; ///< its texture coordinate scale (attribute 6), or none
};

struct ParticleSpec {
    std::string name;
    std::vector<SetSpec> sets;
};

/// A CGFX file of particle models (as Kid Icarus: Uprising's effect.bcres keeps its flashes:
/// user data Tex, Col, Sam, Type, Order; particle sets with a scale), one mesh model that is not
/// a particle model, and RGBA8 textures of uniform tiles.
Bytes EffectCgfx(const std::vector<ParticleSpec>& particles, const std::vector<TexSpec>& textures) {
    Buf f;
    f.Alloc(0x14 + 8 + 16 * 8);
    f.Magic(0, "CGFX");
    f.W16(4, 0xFEFF);
    f.W16(6, 0x14);
    f.Magic(0x14, "DATA");
    std::vector<std::pair<std::string, std::size_t>> model_entries;
    {
        const std::size_t mesh_model = f.Alloc(0x100); // a bullet's mesh model: skipped
        f.W32(mesh_model, 0x40000012u);
        f.Magic(mesh_model + 4, "CMDL");
        f.Rel(mesh_model + 0xC, f.Str("EffTestBullet"));
        model_entries.emplace_back("EffTestBullet", mesh_model);
    }
    for (const ParticleSpec& p : particles) {
        const std::size_t mo = f.Alloc(0x100);
        f.W32(mo, 0x40000112u);
        f.Magic(mo + 4, "CMDL");
        f.Rel(mo + 0xC, f.Str(p.name));
        const std::size_t n = p.sets.size();
        const std::size_t tex = f.Alloc(0x14 + 4 * n);
        f.W32(tex, 0x10000000u);
        f.W32(tex + 8, 2);
        f.W32(tex + 0x10, static_cast<u32>(n));
        for (std::size_t i = 0; i < n; i++) {
            f.Rel(tex + 0x14 + 4 * i, f.Str(p.sets[i].texture));
        }
        const auto ints = [&](auto get) {
            const std::size_t e = f.Alloc(0x10 + 4 * n);
            f.W32(e, 0x20000000u);
            f.W32(e + 8, 1);
            f.W32(e + 0xC, static_cast<u32>(n));
            for (std::size_t i = 0; i < n; i++) {
                f.W32(e + 0x10 + 4 * i, get(p.sets[i], i));
            }
            return e;
        };
        const std::size_t col = ints([](const SetSpec& s, std::size_t) { return s.colour; });
        const std::size_t sam = ints([](const SetSpec& s, std::size_t) { return s.sampler; });
        const std::size_t type = ints([](const SetSpec& s, std::size_t) { return s.type; });
        const std::size_t order =
            ints([](const SetSpec&, std::size_t i) { return static_cast<u32>(i); });
        f.W32(mo + 0x10, 5);
        f.Rel(mo + 0x14, f.Dict({{"Tex", tex}, {"Col", col}, {"Sam", sam}, {"Type", type},
                                 {"Order", order}}));
        std::vector<std::size_t> sets;
        for (const SetSpec& s : p.sets) {
            const std::size_t so = f.Alloc(0x100);
            f.W32(so, 0x40000001u);
            f.Magic(so + 4, "CNOD");
            f.Rel(so + 0xC, f.Str(s.name));
            // The attributes, then the updaters (as the files have them).
            if (s.uv_x > 0.0f) {
                f.W32(so + 0x20, 0x40000000u);
                f.W32(so + 0x24, 6);
                f.W32(so + 0x28, 2);
                f.W32(so + 0x2C, 2);
                f.W32(so + 0x30, 4);
                f.F(so + 0x34, s.uv_x);
                f.F(so + 0x38, s.uv_y);
            }
            if (s.updater) {
                f.W32(so + 0x40, 0x00200000u);
                f.W32(so + 0x44, 0x100u);
                f.W32(so + 0x48, 1);
                f.F(so + 0x4C, s.size_x);
                f.F(so + 0x50, s.size_y);
                f.F(so + 0x54, 1.0f);
                f.F(so + 0x58, 0.5f);
            } else if (s.size_x > 0.0f) {
                f.W32(so + 0x40, 0x40000000u);
                f.W32(so + 0x44, 1);
                f.W32(so + 0x48, 2);
                f.W32(so + 0x4C, 2);
                f.W32(so + 0x50, 4);
                f.F(so + 0x54, s.size_x);
                f.F(so + 0x58, s.size_y);
            }
            sets.push_back(so);
        }
        f.PtrList(mo + 0xE0, sets);
        model_entries.emplace_back(p.name, mo);
    }
    std::vector<std::pair<std::string, std::size_t>> tex_entries;
    for (const TexSpec& t : textures) {
        const std::size_t to = f.Alloc(0x50);
        f.Magic(to + 4, "TXOB");
        f.W32(to + 0x18, t.height);
        f.W32(to + 0x1C, t.width);
        f.W32(to + 0x34, 0); // RGBA8
        const std::size_t image = f.Alloc(0x10);
        const std::size_t pixels = f.Alloc(std::size_t{t.width} * t.height * 4);
        for (u32 ty = 0; ty < t.height / 8; ty++) {
            for (u32 tx = 0; tx < t.width / 8; tx++) {
                const auto c = t.tile(tx, ty);
                const std::size_t at = pixels + (std::size_t{ty} * (t.width / 8) + tx) * 64 * 4;
                for (std::size_t k = 0; k < 64; k++) { // PICA's byte order: a, b, g, r
                    f.b[at + k * 4] = c[3];
                    f.b[at + k * 4 + 1] = c[2];
                    f.b[at + k * 4 + 2] = c[1];
                    f.b[at + k * 4 + 3] = c[0];
                }
            }
        }
        f.W32(image, t.height);
        f.W32(image + 4, t.width);
        f.W32(image + 8, t.width * t.height * 4);
        f.Rel(image + 0xC, pixels);
        f.Rel(to + 0x38, image);
        tex_entries.emplace_back(t.name, to);
    }
    const std::size_t models = f.Dict(model_entries);
    const std::size_t texs = f.Dict(tex_entries);
    f.W32(0x1C, static_cast<u32>(model_entries.size()));
    f.Rel(0x20, models);
    f.W32(0x24, static_cast<u32>(tex_entries.size()));
    f.Rel(0x28, texs);
    return f.b;
}

/// The claws' (weapon 24, Nail00) flashes: rapid fire's a two-tone burst, a fan, smoke (left
/// out) and a set with no colour of its own (the texture's); the charged shot's a round glow from
/// a quarter, a sprite sheet and four more bursts (at most kFlashCardsMax cards).
Bytes ClawEffects() {
    const auto uniform = [](std::array<u8, 4> c) {
        return [c](u32, u32) { return c; };
    };
    const std::vector<TexSpec> textures{
        {"ptc_burst", 16, 16, uniform({255, 200, 10, 255})},
        // A half fan: lit at its left column, its point at the top (row 0), wide below.
        {"ptc_fan", 16, 32,
         [](u32 tx, u32 ty) {
             return std::array<u8, 4>{
                 static_cast<u8>(tx == 0 ? 40 + ty * 60 : 0), 0, 0, 255};
         }},
        // A quarter glow: lit at its last column and row (the bottom right corner).
        {"ptc_glow", 16, 16,
         [](u32 tx, u32 ty) {
             return std::array<u8, 4>{static_cast<u8>(tx == 1 && ty == 1 ? 250 : tx + ty ? 90 : 0),
                                      0, 0, 255};
         }},
        // A sheet of two cells across with a dark gutter between: the right one brighter.
        {"ptc_sheet", 32, 16,
         [](u32 tx, u32) {
             const u8 v = tx == 0 ? 120 : tx == 3 ? 220 : 0;
             return std::array<u8, 4>{v, v, v, 0x80};
         }},
        {"ptc_smoke", 8, 8, uniform({90, 90, 90, 255})},
        {"ptc_unused", 8, 8, uniform({9, 9, 9, 255})},
    };
    const std::vector<ParticleSpec> particles{
        {"PtcNail00RapidShot",
         {{"Nail00RapidShot", "ptc_burst", 0xE561FF00u, 6, 0xFF000704u, 9, 9, true},
          {"Nail00RapidShot_smoke", "ptc_smoke", 0x00FFFFFFu, 6, 0xFF000401u, 3, 3, false},
          {"Nail00RapidShot_gokou", "ptc_fan", 0x000000FFu, 0x3006, 0xFF020402u, 2, 20, true},
          {"Nail00RapidShot_own", "ptc_burst", 0x00000000u, 6, 0xFF000302u, 4, 4, false}}},
        {"PtcNail00ShotFlash",
         {{"Nail00ShotFlash_a", "ptc_burst", 0x00102030u, 6, 0xFF000401u, 5, 5, false},
          {"Nail00ShotFlash_b", "ptc_burst", 0x00102030u, 6, 0xFF000401u, 5, 5, false},
          {"Nail00ShotFlash_glow", "ptc_glow", 0x000040FFu, 0x3306, 0xFF010402u, 6, 6, false},
          {"Nail00ShotFlash_sheet", "ptc_sheet", 0x00FF8000u, 6, 0xFF000401u, 0, 0, false, 0.5f, 1.0f},
          {"Nail00ShotFlash_c", "ptc_burst", 0x00102030u, 6, 0xFF000401u, 5, 5, false},
          {"Nail00ShotFlash_d", "ptc_burst", 0x00102030u, 6, 0xFF000401u, 5, 5, false}}},
        {"PtcNail00ShotSub", {{"Nail00ShotSub", "ptc_unused", 0x00FFFFFFu, 6, 0xFF000401u, 1, 1, false}}},
        // AzaharFrame kiuflash: the bullet's sprites (no mesh bullet in this file).
        {"PtcNail00Bullet1", {{"Nail00Bullet1", "ptc_burst", 0x0080FF00u, 6, 0xFF000402u, 3, 3, true}}},
    };
    return EffectCgfx(particles, textures);
}

FrameXr::Vis::ModelFile LoadFlashFile(const std::string& dir, const std::string& name,
                                      const Bytes& bytes) {
    {
        std::ofstream o(dir + name, std::ios::binary);
        o.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }
    FrameXr::Vis::ModelFile mf;
    std::string error;
    INFO(name);
    REQUIRE(FrameXr::Vis::LoadModelFile(dir + name, mf, error, FrameXr::Vis::kModelFlagWeaponSpace));
    return mf;
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

// ---- AzaharFrame kiuweap2: Pit's poses (skeletal animations) ----

/// One bone's Transform element: slots 0-9 (scale xyz, rotation xyz, w, translation xyz), each
/// a constant, a curve (its keys) or absent (not given).
struct PoseBone {
    std::string bone;
    std::map<int, float> constants;
    std::map<int, std::vector<std::pair<float, float>>> curves;
};
struct PoseAnim {
    std::string name;
    std::vector<PoseBone> bones;
};

/// A CGFX file of skeletal animations (the 10th dictionary), as the game's Pit files have them.
Bytes PoseCgfx(const std::vector<PoseAnim>& anims) {
    Buf f;
    f.Alloc(0x14 + 8 + 16 * 8);
    f.Magic(0, "CGFX");
    f.W16(4, 0xFEFF);
    f.W16(6, 0x14);
    f.Magic(0x14, "DATA");
    std::vector<std::pair<std::string, std::size_t>> entries;
    for (const PoseAnim& a : anims) {
        const std::size_t ao = f.Alloc(0x28);
        f.Magic(ao, "CANM");
        f.W32(ao + 4, 0x07000000u);
        f.Rel(ao + 8, f.Str(a.name));
        f.Rel(ao + 0xC, f.Str("SkeletalAnimation"));
        f.W32(ao + 0x10, 1);
        f.F(ao + 0x14, 60.0f);
        std::vector<std::pair<std::string, std::size_t>> elems;
        for (const PoseBone& b : a.bones) {
            const std::size_t eo = f.Alloc(12 + 4 * 10);
            u32 flags = 0;
            f.Rel(eo + 4, f.Str(b.bone));
            f.W32(eo + 8, 5); // Transform
            for (int slot = 0; slot < 10; slot++) {
                const std::size_t word = eo + 12 + 4 * static_cast<std::size_t>(slot);
                if (const auto c = b.constants.find(slot); c != b.constants.end()) {
                    flags |= 1u << (6 + slot);
                    f.F(word, c->second);
                } else if (const auto k = b.curves.find(slot); k != b.curves.end()) {
                    const auto& keys = k->second;
                    const std::size_t cv = f.Alloc(20 + 8 * keys.size());
                    f.F(cv, keys.front().first);
                    f.F(cv + 4, keys.back().first);
                    f.W32(cv + 8, 6u << 5 | 4u);
                    f.W32(cv + 12, static_cast<u32>(keys.size()));
                    f.F(cv + 16, 1.0f / std::max(keys.back().first, 1.0f));
                    for (std::size_t i = 0; i < keys.size(); i++) {
                        f.F(cv + 20 + 8 * i, keys[i].first);
                        f.F(cv + 24 + 8 * i, keys[i].second);
                    }
                    const std::size_t g = f.Alloc(24);
                    f.F(g + 4, keys.back().first);
                    f.W32(g + 12, 4);
                    f.W32(g + 16, 1);
                    f.Rel(g + 20, cv);
                    f.Rel(word, g);
                } else {
                    flags |= 1u << (16 + slot);
                }
            }
            f.W32(eo, flags);
            elems.emplace_back(b.bone, eo);
        }
        if (!elems.empty()) {
            f.W32(ao + 0x18, static_cast<u32>(elems.size()));
            f.Rel(ao + 0x1C, f.Dict(elems));
        }
        entries.emplace_back(a.name, ao);
    }
    f.W32(0x1C + 72, static_cast<u32>(entries.size())); // the 10th dictionary
    f.Rel(0x1C + 76, f.Dict(entries));
    return f.b;
}

constexpr float kPi = 3.14159265f;

/// A blade (weapon 0's class) gripped by GripN (on Pit's right hand, 0.076 above it), as the
/// game's: the blade's mesh on GripN, its muzzle on it.
Bytes BladeZrc() {
    const Tri wpn{{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}}, {0, 0, 1}};
    const Bytes bcres = Cgfx({{"TopN", -1, {0, 0, 0}},
                              {"RHandN", 0, {-5, 12, 0}},
                              {"GripN", 1, {-5, 12.076f, 0}},
                              {"LHandN", 0, {5, 12, 0}},
                              {"LHandFistM", 3, {5.899f, 12.076f, 0}}},
                             {{"Wpn", "Tex"}}, {{"Wpn", {2}, 1, wpn, -1}});
    const Bytes fmdl = Darc({{"fmdl/00.bcres", bcres}});
    const Bytes js =
        Darc({{"json/00.json", WeaponJson("WPNID_WPN_RIFLE_00", "GripN", {-4.55f, 0.1f, -9.89f})}});
    return Lz11Store(Darc({{"character/fmdl.arc", Lz11Store(fmdl)}, {"character/json.arc", js}}));
}

/// The blades' Pit poses (player/g0.zrc): his idle turns the grip half a turn about x (and a
/// little about y) and moves it, his aiming pose turns it a little about x (a curve) and keeps its
/// place; both turn his hand too (the hold is in the hand's space: no change).
Bytes BladePoses() {
    const PoseBone hand{"RHandN", {{3, 0.3f}, {5, -0.2f}}, {}};
    const PoseBone idle{"GripN", {{0, 1.0f}, {1, 1.0f}, {2, 1.0f}, {3, kPi}, {4, 0.117f}, {5, 0.0f},
                                  {7, -0.898f}, {8, -0.076f}, {9, 0.0f}}, {}};
    const PoseBone shot{"GripN", {}, {{3, {{0.0f, 0.5f}, {30.0f, 0.9f}}}}};
    const Bytes bcres = PoseCgfx({{"ChrPit00WaitRifle", {hand, idle}},
                                  {"ChrPit00ShotWaitRifle", {hand, shot}},
                                  {"ChrPit00WaitSnipe", {}}});
    return Lz11Store(Darc({{"character/mdl.arc", Lz11Store(Darc({{"mdl/00.bcres", bcres}}))}}));
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

// ---- AzaharFrame kiuflash: the muzzle flashes ----

TEST_CASE("Muzzle Flash: particle models read from an effect file", "[kiuflash]") {
    const Bytes effects = ClawEffects();
    std::vector<FrameCgfx::ParticleModel> models;
    std::string error;
    REQUIRE(FrameCgfx::ReadParticleModels(effects, models, error));
    REQUIRE(models.size() == 4); // the mesh model skipped (the bullet sprites' model last)
    CHECK(models[0].name == "PtcNail00RapidShot");
    REQUIRE(models[0].sets.size() == 4);
    const auto& s = models[0].sets[2];
    CHECK(s.name == "Nail00RapidShot_gokou");
    CHECK(s.texture == "ptc_fan");
    CHECK(s.colour == 0x000000FFu);
    CHECK(s.sampler == 0x3006u);
    CHECK(s.type == 0xFF020402u);
    CHECK(s.size[0] == Approx(2.0f)); // the scale updater's across and down
    CHECK(s.size[1] == Approx(20.0f));
    CHECK(s.uv_scale[0] == 0.0f);     // none given
    CHECK(models[1].sets[2].size[0] == Approx(6.0f)); // the constant scale
    CHECK(models[1].sets[3].size[0] == 0.0f);         // none
    CHECK(models[1].sets[3].uv_scale[0] == Approx(0.5f)); // two cells across, one down
    CHECK(models[1].sets[3].uv_scale[1] == Approx(1.0f));
    std::vector<FrameCgfx::Texture> textures;
    REQUIRE(FrameCgfx::ReadTextures(effects, {"ptc_glow", "ptc_sheet", "nothing"}, textures, error));
    REQUIRE(textures.size() == 2);
    CHECK(textures[1].name == "ptc_sheet");
    CHECK(textures[1].width == 32);
    CHECK(textures[1].rgba[3] == 0x80);
    // Not CGFX, cut short: errors, not crashes.
    CHECK_FALSE(FrameCgfx::ReadParticleModels(Bytes{1, 2, 3}, models, error));
    for (const std::size_t cut : {std::size_t{0x40}, effects.size() / 2}) {
        const Bytes part(effects.begin(), effects.begin() + static_cast<std::ptrdiff_t>(cut));
        std::vector<FrameCgfx::ParticleModel> m2;
        FrameCgfx::ReadParticleModels(part, m2, error); // false or partial, never a crash
    }
}

TEST_CASE("Muzzle Flash: a weapon's flash cards and their files", "[kiuflash]") {
    FrameWeapons::Converted c;
    std::string error;
    REQUIRE(FrameWeapons::ConvertWeapon(24, ClawZrc(ClawEffects()), c, error));
    const FrameWeapons::FlashInfo& f = c.info.flash;
    CHECK(f.known);
    CHECK(f.rapid == "PtcNail00RapidShot");
    CHECK(f.charged == "PtcNail00ShotFlash");
    CHECK(f.source == 24);
    // One file per texture: burst, fan, glow, sheet (smoke left out).
    REQUIRE(f.parts.size() == 4);
    CHECK(f.parts[0].file == "w024-f0.model");
    // The claws' model, the flash files, the bullet sprites' file (one texture, both kinds).
    CHECK(c.files.size() == 1 + 4 + 1);
    const std::string dir = TestDir("frame_weapon_flash_test");
    std::map<std::string, FrameXr::Vis::ModelFile> files;
    for (const auto& [name, bytes] : c.files) {
        if (name.find("-f") != std::string::npos) {
            files[name] = LoadFlashFile(dir, name, bytes);
        }
    }
    int rapid = 0, charged = 0;
    for (const auto& part : f.parts) {
        INFO(part.texture);
        const auto& mf = files.at(part.file);
        CHECK((mf.flags & FrameXr::Vis::kModelFlagFlash) != 0);
        CHECK(part.cards == mf.components.size());
        CHECK(part.hash != 0);
        for (const auto& comp : mf.components) {
            CHECK((comp.name == "rapid" || comp.name == "charged"));
            CHECK((comp.path == "face" || comp.path == "fan"));
            CHECK(comp.index_count == 6);
            CHECK(comp.motion_type == 0);
            (comp.name == "rapid" ? rapid : charged)++;
            const float* v = comp.motion.data();
            float maxy = -1e9f, miny = 1e9f, minu = 1e9f, maxu = -1e9f, minv = 1e9f, maxv = -1e9f;
            for (uint32_t k = comp.first_index; k < comp.first_index + comp.index_count; k++) {
                const float* q = &mf.vertices[mf.indices[k] * 12];
                miny = std::min(miny, q[1]);
                maxy = std::max(maxy, q[1]);
                minu = std::min(minu, q[10]);
                maxu = std::max(maxu, q[10]);
                minv = std::min(minv, q[11]);
                maxv = std::max(maxv, q[11]);
                CHECK(q[2] == 0.0f);
                CHECK(q[5] == 1.0f);
            }
            CHECK(v[FrameWeapons::kFlashGrow] > 1.0f);
            CHECK(v[FrameWeapons::kFlashGrow] <= 2.0f);
            if (part.texture == "ptc_burst" && comp.name == "rapid" &&
                v[FrameWeapons::kFlashMode] != FrameWeapons::kFlashTwoTone) {
                // No colour of its own: the texture's, in white.
                CHECK(v[FrameWeapons::kFlashMode] == FrameWeapons::kFlashTexture);
                CHECK(v[FrameWeapons::kFlashRed] == 1.0f);
                CHECK(v[FrameWeapons::kFlashBlue] == 1.0f);
                CHECK(maxy - miny == Approx(4.0f));
            } else if (part.texture == "ptc_burst" && comp.name == "rapid") {
                // The two-tone burst: its colour, its white core (the top byte), 9 units.
                CHECK(v[FrameWeapons::kFlashMode] == FrameWeapons::kFlashTwoTone);
                CHECK(v[FrameWeapons::kFlashRed] == Approx(0x61 / 255.0f));
                CHECK(v[FrameWeapons::kFlashGreen] == Approx(1.0f));
                CHECK(v[FrameWeapons::kFlashBlue] == 0.0f);
                CHECK(v[FrameWeapons::kFlashCore] == Approx(0xE5 / 255.0f));
                CHECK(comp.path == "face");
                CHECK(maxy - miny == Approx(9.0f));
                CHECK(v[FrameWeapons::kFlashOffset] == Approx(4.5f)); // half its size ahead
            }
            if (part.texture == "ptc_fan") {
                // A fan from the muzzle along the ray, 20 units long, whole from its half
                // (mirrored about its lit left edge), its point (the dim top) at the muzzle.
                CHECK(comp.path == "fan");
                CHECK(v[FrameWeapons::kFlashMode] == FrameWeapons::kFlashLuminance);
                CHECK(miny == 0.0f);
                CHECK(maxy == Approx(20.0f));
                CHECK(minu == Approx(-1.0f));
                CHECK(maxu == Approx(1.0f));
                CHECK(FrameXr::Vis::ModelWrapU(mf.flags) == 3);
                for (uint32_t k = comp.first_index; k < comp.first_index + comp.index_count; k++) {
                    const float* q = &mf.vertices[mf.indices[k] * 12];
                    if (q[1] == 0.0f) {
                        CHECK(q[11] == 0.0f); // the first row at the point
                    }
                }
            }
            if (part.texture == "ptc_glow") {
                // A round glow from a quarter: both axes mirrored about the lit corner.
                CHECK(comp.path == "face");
                CHECK(minu == 0.0f);
                CHECK(maxu == Approx(2.0f));
                CHECK(minv == 0.0f);
                CHECK(maxv == Approx(2.0f));
                CHECK(FrameXr::Vis::ModelWrapU(mf.flags) == 3);
                CHECK(FrameXr::Vis::ModelWrapV(mf.flags) == 3);
                CHECK(v[FrameWeapons::kFlashRed] == 0.0f);
                CHECK(v[FrameWeapons::kFlashBlue] == Approx(1.0f));
            }
            if (part.texture == "ptc_sheet") {
                // A sprite sheet of two cells (its UV scale): the brighter; no size of its own:
                // ours (6 units).
                CHECK(minu == Approx(0.5f));
                CHECK(maxu == Approx(1.0f));
                CHECK(v[FrameWeapons::kFlashMode] == FrameWeapons::kFlashTexture);
                CHECK(maxy - miny == Approx(6.0f));
                CHECK(mf.colour[3] == 0x80); // the texture's alpha kept
            }
        }
    }
    CHECK(rapid == 3);                         // the burst, the fan, the texture's own
    CHECK(charged == FrameWeapons::kFlashCardsMax); // six sets, four cards
    // Each kind keeps its glow (the first of each kind before more of one).
    bool glow = false;
    for (const auto& part : f.parts) {
        glow |= part.texture == "ptc_glow";
    }
    CHECK(glow);
    // The manifest keeps it all; a reader without kiuflash's lines skips them (unknown keys).
    FrameWeapons::Manifest m;
    m.version = FrameWeapons::kConverterVersion;
    m.title_id = 0x0004000000030200ull;
    m.weapons.push_back(c.info);
    REQUIRE(FrameWeapons::WriteManifest(dir + "weapons.txt", m, error));
    FrameWeapons::Manifest back;
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", back, error));
    REQUIRE(back.Find(24));
    const auto& bf = back.Find(24)->flash;
    CHECK(bf.known);
    CHECK(bf.rapid == f.rapid);
    CHECK(bf.charged == f.charged);
    CHECK(bf.source == 24);
    REQUIRE(bf.parts.size() == f.parts.size());
    CHECK(bf.parts[1].file == f.parts[1].file);
    CHECK(bf.parts[1].hash == f.parts[1].hash);
    CHECK(bf.parts[1].cards == f.parts[1].cards);
    // A weapon file with no effects: no flash, known (the game's run then borrows one).
    FrameWeapons::Converted none;
    REQUIRE(FrameWeapons::ConvertWeapon(24, ClawZrc(), none, error));
    CHECK(none.info.flash.known);
    CHECK(none.info.flash.parts.empty());
    CHECK(none.info.flash.source == -1);
    std::filesystem::remove_all(dir);
}

TEST_CASE("Muzzle Flash: the cache: with the weapon, made again once, a class's fallback",
          "[kiuflash]") {
    const std::string dir = TestDir("frame_weapon_flash_cache");
    constexpr u64 kTitle = 0x0004000000030200ull;
    // m200 has its effects; m204 (weapon 28) has none.
    auto rom = std::make_shared<MemoryRomFS>(
        RomFS("eu", {{"m200.zrc", ClawZrc(ClawEffects())}, {"m204.zrc", ClawZrc()}}));
    FrameWeapons::RunResult r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.made == 2);
    FrameWeapons::Manifest m;
    std::string error;
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    REQUIRE(m.Find(24));
    REQUIRE(m.Find(28));
    CHECK(m.Find(24)->flash.source == 24);
    // The fallback: the class's first weapon with a flash, its colours made neutral.
    const auto& fb = m.Find(28)->flash;
    CHECK(fb.source == 24);
    CHECK(fb.rapid == "PtcNail00RapidShot");
    REQUIRE(!fb.parts.empty());
    CHECK(fb.parts[0].file == "w028-f0.model");
    FrameXr::Vis::ModelFile mf;
    REQUIRE(FrameXr::Vis::LoadModelFile(dir + fb.parts[0].file, mf, error,
                                        FrameXr::Vis::kModelFlagWeaponSpace));
    for (const auto& comp : mf.components) {
        const float* v = comp.motion.data();
        CHECK(v[FrameWeapons::kFlashRed] == v[FrameWeapons::kFlashGreen]);
        CHECK(v[FrameWeapons::kFlashGreen] == v[FrameWeapons::kFlashBlue]);
        CHECK(v[FrameWeapons::kFlashRed] >= 0.6f);
    }
    // The extra cache, per weapon (the claws' test textures are tiny; the real sizes are in the
    // report).
    std::uintmax_t flash_bytes = 0, weapon_bytes = 0;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        const std::string n = e.path().filename().string();
        if (n.rfind("w024", 0) == 0) {
            (n.find("-f") != std::string::npos ? flash_bytes : weapon_bytes) += e.file_size();
        }
    }
    CHECK(flash_bytes > 0);
    CHECK(flash_bytes < 16 * 1024);
    // Kept when nothing changed.
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.kept == 2);
    CHECK(r.made == 0);
    // A manifest from before the flashes (an older build saved it: no flashes lines): each
    // weapon made again once, then kept.
    {
        std::ifstream in(dir + "weapons.txt");
        std::string text, line;
        while (std::getline(in, line)) {
            if (line.rfind("flash", 0) != 0) {
                text += line + "\n";
            }
        }
        in.close();
        std::ofstream(dir + "weapons.txt") << text;
    }
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    CHECK_FALSE(m.Find(24)->flash.known);
    // Saved again as it is (a run that made another weapon): still not known, never "no flash".
    REQUIRE(FrameWeapons::WriteManifest(dir + "weapons.txt", m, error));
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    CHECK_FALSE(m.Find(24)->flash.known);
    CHECK_FALSE(m.Find(28)->flash.known);
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.made == 2);
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.kept == 2);
    // A flash file gone: made again.
    std::filesystem::remove(dir + "w024-f1.model");
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.made == 1);
    CHECK(std::filesystem::exists(dir + "w024-f1.model"));
    // A bad flash line: the manifest is not taken (made again), never a crash.
    {
        std::ofstream(dir + "weapons.txt", std::ios::app) << "flash ../x - 0 1 1\n";
    }
    CHECK_FALSE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    std::filesystem::remove_all(dir);
}

TEST_CASE("Muzzle Flash: the texture pack's replacement, with its alpha", "[kiuflash]") {
    const std::string root = TestDir("frame_weapon_flash_pack");
    const std::string before = FileUtil::GetUserPath(FileUtil::UserPath::LoadDir);
    FileUtil::UpdateUserPath(FileUtil::UserPath::LoadDir, root);
    constexpr u64 kTitle = 0x0004000000030200ull;
    const std::string pack_dir = fmt::format("{}textures/{:016X}/", root, kTitle);
    std::filesystem::create_directories(pack_dir + "effects");
    {
        std::ofstream o(pack_dir + "pack.json");
        o << R"({"author": "test", "options": {"use_new_hash": true, "flip_png_files": true}})";
    }
    FrameWeapons::Converted plain;
    std::string error;
    REQUIRE(FrameWeapons::ConvertWeapon(24, ClawZrc(ClawEffects()), plain, error));
    const FrameWeapons::FlashPart* sheet = nullptr;
    for (const auto& p : plain.info.flash.parts) {
        if (p.texture == "ptc_sheet") {
            sheet = &p;
        }
    }
    REQUIRE(sheet);
    // A 4x replacement named by the texture's new hash, in the effects folder.
    Frontend::ImageInterface images;
    Bytes px;
    for (int i = 0; i < 128 * 64; i++) {
        px.insert(px.end(), {10, 20, 30, static_cast<u8>(i % 2 ? 40 : 200)});
    }
    const std::string png =
        fmt::format("{}effects/tex1_32x16_{:016X}_0_mip0.png", pack_dir, sheet->hash);
    REQUIRE(images.EncodePNG(png, 128, 64, px));
    const FrameWeapons::TexturePack pack = FrameWeapons::ReadTexturePack(kTitle);
    REQUIRE(pack.on);
    FrameWeapons::Converted c;
    REQUIRE(FrameWeapons::ConvertWeapon(24, ClawZrc(ClawEffects()), c, error, &pack));
    const FrameWeapons::FlashPart* replaced = nullptr;
    for (const auto& p : c.info.flash.parts) {
        if (p.texture == "ptc_sheet") {
            replaced = &p;
        }
        if (p.texture != "ptc_sheet") {
            CHECK(p.replaced.empty());
        }
    }
    REQUIRE(replaced);
    CHECK(std::filesystem::equivalent(replaced->replaced, png));
    CHECK(c.info.flash.pack_key != 0);
    CHECK(c.info.pack_key == plain.info.pack_key); // the weapon's own key is its textures' only
    // Read in place when the weapon loads, alpha and all (never copied into the cache).
    std::vector<u8> rgba;
    u32 w = 0, h = 0;
    REQUIRE(FrameWeapons::LoadPackTexture(replaced->replaced, true, rgba, w, h, true));
    CHECK(w == 128);
    CHECK(rgba[3] == 200);
    CHECK(rgba[7] == 40);
    REQUIRE(FrameWeapons::LoadPackTexture(replaced->replaced, true, rgba, w, h));
    CHECK(rgba[7] == 255); // a weapon's colour image: no alpha
    FileUtil::UpdateUserPath(FileUtil::UserPath::LoadDir, before);
    std::filesystem::remove_all(root);
}

// ---- Desktop run (hidden): AZAHARFRAME_TEST_KIU_ZRC=<a folder of the game's player/m*.zrc>,
// AZAHARFRAME_TEST_KIU_FLASH_OUT=<a new folder>, AZAHARFRAME_TEST_KIU_LOAD=<a load folder with
// textures/<title id>/, optional>: every weapon made through a game's run (as in a game: its own
// flash or its class's fallback), and flash-coverage.txt: per weapon its flashes, cards and files'
// sizes. ----

TEST_CASE("Muzzle Flash: every weapon of a game's files", "[.kiuflash_real]") {
    const char* zrc_dir = std::getenv("AZAHARFRAME_TEST_KIU_ZRC");
    const char* out = std::getenv("AZAHARFRAME_TEST_KIU_FLASH_OUT");
    const char* load = std::getenv("AZAHARFRAME_TEST_KIU_LOAD");
    REQUIRE(zrc_dir);
    REQUIRE(out);
    constexpr u64 kTitle = 0x0004000000030200ull;
    std::vector<std::pair<std::string, Bytes>> files;
    for (int id = 0; id < FrameWeapons::kWeapons; id++) {
        const std::string name = FrameWeapons::ZrcName(id);
        std::ifstream f(std::string{zrc_dir} + "/" + name, std::ios::binary);
        REQUIRE(f);
        files.emplace_back(name, Bytes((std::istreambuf_iterator<char>(f)),
                                       std::istreambuf_iterator<char>()));
    }
    // AzaharFrame kiuweap2: the class files too when they are there (Pit's poses: the hold).
    for (int cls = 0; cls < FrameWeapons::kClasses; cls++) {
        const std::string name = FrameWeapons::PoseZrcName(cls);
        std::ifstream f(std::string{zrc_dir} + "/" + name, std::ios::binary);
        if (f) {
            files.emplace_back(name, Bytes((std::istreambuf_iterator<char>(f)),
                                           std::istreambuf_iterator<char>()));
        }
    }
    auto rom = std::make_shared<MemoryRomFS>(RomFS("eu", files));
    FrameWeapons::TexturePack pack;
    const std::string before = FileUtil::GetUserPath(FileUtil::UserPath::LoadDir);
    if (load) {
        FileUtil::UpdateUserPath(FileUtil::UserPath::LoadDir, std::string{load} + "/");
        pack = FrameWeapons::ReadTexturePack(kTitle);
    }
    const std::string dir = std::string{out} + "/";
    const auto t0 = std::chrono::steady_clock::now();
    const auto r = FrameWeapons::ConvertGame(rom, kTitle, dir, nullptr, load ? &pack : nullptr);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    WARN("made " << r.made << ", kept " << r.kept << ", failed " << r.failed << " in " << s
                 << " s; pack " << (pack.on ? "on" : "off") << ", " << pack.files.size()
                 << " colour textures");
    CHECK(r.made + r.kept == FrameWeapons::kWeapons);
    FrameWeapons::Manifest m;
    std::string error;
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    std::ofstream cov(dir + "flash-coverage.txt");
    cov << "# id class source(own|fallback:<id>|none) rapid charged rapid_cards charged_cards "
           "flash_files flash_bytes weapon_bytes flash_pack_pngs\n";
    int own = 0, fallback = 0, none = 0;
    for (const auto& w : m.weapons) {
        int rapid = 0, charged = 0, pngs = 0;
        std::uintmax_t flash_bytes = 0, weapon_bytes = 0;
        for (const auto& p : w.parts) {
            weapon_bytes += std::filesystem::file_size(dir + p.file);
        }
        for (const auto& p : w.flash.parts) {
            flash_bytes += std::filesystem::file_size(dir + p.file);
            pngs += p.replaced.empty() ? 0 : 1;
            FrameXr::Vis::ModelFile mf;
            REQUIRE(FrameXr::Vis::LoadModelFile(dir + p.file, mf, error,
                                                FrameXr::Vis::kModelFlagWeaponSpace));
            for (const auto& c : mf.components) {
                (c.name == "rapid" ? rapid : charged)++;
            }
        }
        const std::string source = w.flash.source < 0         ? "none"
                                   : w.flash.source == w.id ? "own"
                                                            : fmt::format("fallback:{}", w.flash.source);
        (w.flash.source < 0 ? none : w.flash.source == w.id ? own : fallback)++;
        cov << fmt::format("{} {} {} {} {} {} {} {} {} {} {}\n", w.id, FrameWeapons::ClassName(w.cls),
                           source, w.flash.rapid.empty() ? "-" : w.flash.rapid,
                           w.flash.charged.empty() ? "-" : w.flash.charged, rapid, charged,
                           w.flash.parts.size(), flash_bytes, weapon_bytes, pngs);
    }
    cov << fmt::format("# own {} fallback {} none {}\n", own, fallback, none);
    WARN("flashes: own " << own << ", fallback " << fallback << ", none " << none);
    if (load) {
        FileUtil::UpdateUserPath(FileUtil::UserPath::LoadDir, before);
    }
}

// ---- AzaharFrame kiuflash: the projectiles' bullets ----

namespace {

/// Weapon 24's file with a mesh bullet "EffNail00Bullet1" as its effect file: one triangle 2 x 1 x
/// 4 units, its head (2 across) at z 0 and its trail's point at z 4, an added-light material whose
/// texture (0x10, 0x20, 0x30) is coloured by the constant (1, 0.5, 0.25).
Bytes ClawWithMeshBullet() {
    const Tri bullet{{{{-1, 0, 0}, {1, 0, 0}, {0, 1, 4}}}, {0, 1, 0}};
    AddedLight added;
    added.on = true;
    added.constant = {1.0f, 0.5f, 0.25f, 1.0f};
    const Bytes effect = Cgfx({{"TopN", -1, {0, 0, 0}}}, {{"BulletMat", "Tex"}},
                              {{"BulletMat", {0}, 1, bullet, -1}}, {}, "EffNail00Bullet1", added);
    return ClawZrc(effect);
}

} // namespace

TEST_CASE("Projectiles: each class's bullets by name", "[kiuflash]") {
    using FrameWeapons::PickBulletModels;
    using FrameWeapons::PickBulletSprites;
    // A blade: Bullet1 and Bullet3; the slashes (Bullet02, BulletA02) are no shots.
    auto b = PickBulletModels(1, {"EffRifle01Bullet02", "EffRifle01Bullet1", "EffRifle01Bullet3",
                                  "EffRifle01BulletA02"});
    CHECK(b.rapid == "EffRifle01Bullet1");
    CHECK(b.charged == "EffRifle01Bullet3");
    // Claws and orbitars: Bullet01 and Bullet03.
    b = PickBulletModels(24, {"EffNail00Blow", "EffNail00Bullet01", "EffNail00Bullet02",
                              "EffNail00Bullet03"});
    CHECK(b.rapid == "EffNail00Bullet01");
    CHECK(b.charged == "EffNail00Bullet03");
    b = PickBulletModels(85, {"EffBit01Bullet01", "EffBit01Bullet03"});
    CHECK(b.rapid == "EffBit01Bullet01");
    CHECK(b.charged == "EffBit01Bullet03");
    // A palm: one bullet for both.
    b = PickBulletModels(49, {"EffPalm01Bullet1"});
    CHECK(b.rapid == "EffPalm01Bullet1");
    CHECK(b.charged == "EffPalm01Bullet1");
    // None by these names (an arm with Bullet2 and Bullet5 only); another weapon's names.
    CHECK(PickBulletModels(105, {"EffArm09Bullet2", "EffArm09Bullet5"}).rapid.empty());
    CHECK(PickBulletModels(2, {"EffRifle01Bullet1"}).rapid.empty());
    // Sprites the same way.
    b = PickBulletSprites(1, {"PtcRifle01Bullet1", "PtcRifle01Bullet3", "PtcRifle01RapidFlashA"});
    CHECK(b.rapid == "PtcRifle01Bullet1");
    CHECK(b.charged == "PtcRifle01Bullet3");
}

TEST_CASE("Projectiles: a mesh bullet, centred, its look fitted to its material", "[kiuflash]") {
    FrameWeapons::Converted c;
    std::string error;
    {
        // The effect file read as the converter reads it: the bullet model, its texture.
        Bytes effects;
        REQUIRE(FrameCgfx::DarcMember(ClawWithMeshBullet(), "character/effect.bcres", effects,
                                      error));
        FrameCgfx::File f;
        REQUIRE(FrameCgfx::ReadCgfxModels(effects, {"EffNail00Bullet1"}, f, error));
        INFO(error);
        REQUIRE(f.models.size() == 1);
        REQUIRE(f.models[0].materials.size() == 1);
        const auto& mat = f.models[0].materials[0];
        CHECK(mat.blend);
        CHECK(mat.blend_src == 6);
        CHECK(mat.blend_dst == 1);
        REQUIRE(mat.stages.size() == 1);
        CHECK(mat.constants[0][1] == Approx(0.5f));
        CHECK(f.FindTexture("Tex"));
    }
    REQUIRE(FrameWeapons::ConvertWeapon(24, ClawWithMeshBullet(), c, error));
    const FrameWeapons::BulletInfo& b = c.info.bullets;
    CHECK(b.known);
    CHECK(b.rapid == "EffNail00Bullet1");
    CHECK(b.charged == "EffNail00Bullet1"); // no Bullet3: the same for both
    CHECK_FALSE(b.rapid_cards);
    REQUIRE(b.parts.size() == 1);
    CHECK(b.parts[0].file == "w024-b0.model");
    CHECK(b.parts[0].texture == "Tex");
    CHECK(b.parts[0].hash != 0);
    const std::string dir = TestDir("frame_weapon_bullet_test");
    Bytes bytes;
    for (const auto& [name, data] : c.files) {
        if (name == b.parts[0].file) {
            bytes = data;
        }
    }
    REQUIRE(!bytes.empty());
    const FrameXr::Vis::ModelFile mf = LoadFlashFile(dir, b.parts[0].file, bytes);
    CHECK((mf.flags & FrameXr::Vis::kModelFlagBullet) != 0);
    REQUIRE(mf.components.size() == 2);
    CHECK(mf.components[0].name == "rapid");
    CHECK(mf.components[1].name == "charged");
    CHECK(mf.components[0].path == "BulletMat");
    for (const auto& comp : mf.components) {
        CHECK(comp.first_index == 0);
        CHECK(comp.index_count == 3);
        const float* v = comp.motion.data();
        CHECK(v[FrameWeapons::kBulletAdded] == 1.0f);
        CHECK(v[FrameWeapons::kBulletMode] == FrameWeapons::kFlashTexture);
        CHECK(v[FrameWeapons::kBulletRed] == Approx(1.0f).margin(0.01));
        CHECK(v[FrameWeapons::kBulletGreen] == Approx(0.5f).margin(0.01));
        CHECK(v[FrameWeapons::kBulletBlue] == Approx(0.25f).margin(0.01));
    }
    // Centred, its largest side (z, 4 units) 1, its wide end (z 0 in the file) to +z.
    float lo[3] = {9, 9, 9}, hi[3] = {-9, -9, -9};
    float wide_z = 0.0f;
    for (std::size_t i = 0; i < mf.vertices.size(); i += 12) {
        for (int k = 0; k < 3; k++) {
            lo[k] = std::min(lo[k], mf.vertices[i + k]);
            hi[k] = std::max(hi[k], mf.vertices[i + k]);
        }
        if (std::abs(mf.vertices[i]) > 0.1f) {
            wide_z = mf.vertices[i + 2]; // the two corners 2 apart across
        }
        CHECK(mf.vertices[i + 6] == 1.0f); // the vertex colour (white) in the tangent's place
        CHECK(mf.vertices[i + 9] == 1.0f);
    }
    CHECK(hi[2] - lo[2] == Approx(1.0f));
    CHECK(hi[0] - lo[0] == Approx(0.5f));
    CHECK(hi[2] + lo[2] == Approx(0.0f).margin(1e-5));
    CHECK(wide_z == Approx(0.5f)); // the head ahead, the trail behind
    CHECK(mf.colour[3] == 0xFF);
    // The manifest keeps it all.
    FrameWeapons::Manifest m;
    m.version = FrameWeapons::kConverterVersion;
    m.weapons.push_back(c.info);
    REQUIRE(FrameWeapons::WriteManifest(dir + "weapons.txt", m, error));
    FrameWeapons::Manifest back;
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", back, error));
    const auto& bb = back.Find(24)->bullets;
    CHECK(bb.known);
    CHECK(bb.rapid == b.rapid);
    CHECK(bb.charged == b.charged);
    CHECK_FALSE(bb.charged_cards);
    REQUIRE(bb.parts.size() == 1);
    CHECK(bb.parts[0].file == b.parts[0].file);
    CHECK(bb.parts[0].hash == b.parts[0].hash);
    CHECK(bb.parts[0].cards == 2);
    std::filesystem::remove_all(dir);
}

TEST_CASE("Projectiles: sprites when there is no mesh bullet, and the cache", "[kiuflash]") {
    FrameWeapons::Converted c;
    std::string error;
    REQUIRE(FrameWeapons::ConvertWeapon(24, ClawZrc(ClawEffects()), c, error));
    const FrameWeapons::BulletInfo& b = c.info.bullets;
    CHECK(b.rapid == "PtcNail00Bullet1");
    CHECK(b.charged == "PtcNail00Bullet1");
    CHECK(b.rapid_cards);
    CHECK(b.charged_cards);
    REQUIRE(!b.parts.empty());
    const std::string dir = TestDir("frame_weapon_bullet_cards");
    int rapid = 0, charged = 0;
    for (const auto& [name, data] : c.files) {
        if (name.find("-b") == std::string::npos) {
            continue;
        }
        const FrameXr::Vis::ModelFile mf = LoadFlashFile(dir, name, data);
        CHECK((mf.flags & FrameXr::Vis::kModelFlagFlash) != 0); // cards, as the flashes'
        for (const auto& comp : mf.components) {
            CHECK(comp.path == "face");
            (comp.name == "rapid" ? rapid : charged)++;
        }
    }
    CHECK(rapid == 1);
    CHECK(charged == 1);
    CHECK(b.parts.size() == 1); // both kinds' cards in one file
    // With no effect file: known, nothing.
    FrameWeapons::Converted none;
    REQUIRE(FrameWeapons::ConvertWeapon(24, ClawZrc(), none, error));
    CHECK(none.info.bullets.known);
    CHECK(none.info.bullets.parts.empty());
    // A game's run: made, kept, made again once for a manifest from before the projectiles (no
    // bullets lines), made again when a bullet file is gone.
    constexpr u64 kTitle = 0x0004000000030200ull;
    auto rom = std::make_shared<MemoryRomFS>(RomFS("eu", {{"m200.zrc", ClawWithMeshBullet()}}));
    auto r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.made == 1);
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.kept == 1);
    {
        std::ifstream in(dir + "weapons.txt");
        std::string text, line;
        while (std::getline(in, line)) {
            if (line.rfind("bullet", 0) != 0) {
                text += line + "\n";
            }
        }
        in.close();
        std::ofstream(dir + "weapons.txt") << text;
    }
    FrameWeapons::Manifest m;
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    CHECK_FALSE(m.Find(24)->bullets.known);
    CHECK(m.Find(24)->flash.known); // the flashes stay as they were
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.made == 1);
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    REQUIRE(!m.Find(24)->bullets.parts.empty());
    std::filesystem::remove(dir + m.Find(24)->bullets.parts[0].file);
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.made == 1);
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

// ---- AzaharFrame kiuweap2: how Pit holds the weapon ----

namespace {
std::array<float, 3> At(const FrameCgfx::Mat34& m, const std::array<float, 3>& p) {
    std::array<float, 3> r{};
    for (std::size_t i = 0; i < 3; i++) {
        r[i] = m[i * 4] * p[0] + m[i * 4 + 1] * p[1] + m[i * 4 + 2] * p[2] + m[i * 4 + 3];
    }
    return r;
}
void CheckMat(const FrameCgfx::Mat34& a, const FrameCgfx::Mat34& b) {
    for (std::size_t i = 0; i < 12; i++) {
        INFO("element " << i);
        CHECK(a[i] == Approx(b[i]).margin(1e-4));
    }
}
FrameCgfx::Bone MakeBone(const std::string& name, s32 index, s32 parent,
                         const std::array<float, 3>& at) {
    FrameCgfx::Bone b;
    b.name = name;
    b.index = index;
    b.parent = parent;
    b.bind = {1, 0, 0, at[0], 0, 1, 0, at[1], 0, 0, 1, at[2]};
    return b;
}
} // namespace

TEST_CASE("CGFX: a bone's transform, a pose of the skeleton", "[kiuweap2]") {
    // Rz * Ry * Rx * S, then the translation.
    const auto id = FrameCgfx::LocalTransform({1, 1, 1}, {0, 0, 0}, {1, 2, 3});
    CheckMat(id, {1, 0, 0, 1, 0, 1, 0, 2, 0, 0, 1, 3});
    const auto rx = FrameCgfx::LocalTransform({1, 1, 1}, {kPi / 2, 0, 0}, {0, 0, 0});
    const auto y = At(rx, {0, 1, 0});
    CHECK(y[2] == Approx(1.0f).margin(1e-5)); // about x: y to z
    const auto xz = FrameCgfx::LocalTransform({2, 1, 1}, {kPi / 2, 0, kPi / 2}, {0, 0, 0});
    const auto x = At(xz, {1, 0, 0}); // scaled, then x's turn (none), then about z: x to y
    CHECK(x[0] == Approx(0.0f).margin(1e-5));
    CHECK(x[1] == Approx(2.0f));
    const auto zz = At(xz, {0, 0, 1}); // about x: z to -y, then about z: -y to x
    CHECK(zz[0] == Approx(1.0f).margin(1e-5));
    // A pose: a bone it holds takes its keys (the rest of its parts its own), one it does not
    // keeps its bind place against its parent.
    FrameCgfx::Model m;
    m.bones = {MakeBone("TopN", 0, -1, {0, 0, 0}), MakeBone("ArmN", 1, 0, {0, 10, 0}),
               MakeBone("HandN", 2, 1, {3, 10, 0})};
    m.bones[1].translation = {0, 10, 0};
    m.bones[2].translation = {3, 0, 0};
    FrameCgfx::SkeletalPose pose;
    FrameCgfx::BoneKey arm;
    arm.bone = "ArmN";
    arm.srt[5] = kPi / 2; // turned a quarter about z; its place (absent) its own
    pose.bones.push_back(arm);
    REQUIRE(pose.Find("ArmN"));
    CHECK_FALSE(pose.Find("HandN"));
    const auto world = FrameCgfx::PoseBones(m, pose);
    REQUIRE(world.size() == 3);
    const auto hand = At(world[2], {0, 0, 0});
    CHECK(hand[0] == Approx(0.0f).margin(1e-5));
    CHECK(hand[1] == Approx(13.0f)); // the hand, 3 along the arm's x, now up
    // A loop in a bad file: an answer, never a hang.
    m.bones[0].parent = 2;
    CHECK(FrameCgfx::PoseBones(m, pose).size() == 3);
}

TEST_CASE("CGFX: Pit's poses read from a skeletal animation", "[kiuweap2]") {
    Bytes bcres;
    std::string error;
    REQUIRE(FrameCgfx::DarcMember(BladePoses(), "character/mdl.arc!mdl/00.bcres", bcres, error));
    FrameCgfx::SkeletalPose p;
    const bool read = FrameCgfx::ReadSkeletalPose(bcres, "ChrPit00WaitRifle", p, error);
    INFO(error);
    REQUIRE(read);
    CHECK(p.animation == "ChrPit00WaitRifle");
    CHECK(p.frames == 60.0f);
    const auto* g = p.Find("GripN");
    REQUIRE(g);
    CHECK(*g->srt[3] == Approx(kPi));
    CHECK(*g->srt[6] == Approx(-0.898f)); // translation x (the slot after w)
    CHECK(*g->srt[7] == Approx(-0.076f));
    const auto* h = p.Find("RHandN");
    REQUIRE(h);
    CHECK_FALSE(h->srt[0]); // absent: the bone's own
    CHECK(*h->srt[3] == Approx(0.3f));
    // A curve: its first key.
    REQUIRE(FrameCgfx::ReadSkeletalPose(bcres, "ChrPit00ShotWaitRifle", p, error));
    REQUIRE(p.Find("GripN"));
    CHECK(*p.Find("GripN")->srt[3] == Approx(0.5f));
    CHECK_FALSE(p.Find("GripN")->srt[6]);
    // No such animation; not a CGFX file; every cut: an error, never a crash.
    CHECK_FALSE(FrameCgfx::ReadSkeletalPose(bcres, "ChrPit00WaitBow", p, error));
    CHECK_FALSE(FrameCgfx::ReadSkeletalPose(Bytes(8, 0), "ChrPit00WaitRifle", p, error));
    for (std::size_t n = 0; n + 4 < bcres.size(); n += 5) {
        Bytes cut(bcres.begin(), bcres.begin() + static_cast<std::ptrdiff_t>(n));
        FrameCgfx::ReadSkeletalPose(cut, "ChrPit00ShotWaitRifle", p, error);
    }
    // A class's both (an aiming pose that is missing is the idle).
    CHECK(FrameWeapons::PoseZrcName(6) == "g6.zrc");
    CHECK(FrameWeapons::IdleAnimation(1) == "ChrPit00WaitSnipe");
    CHECK(FrameWeapons::ShotAnimation(8) == "ChrPit00ShotWaitArm");
    FrameWeapons::ClassPoses cp;
    REQUIRE(FrameWeapons::ReadClassPoses(BladePoses(), 1, cp, error));
    CHECK(cp.shot.animation == "ChrPit00WaitSnipe");
    CHECK_FALSE(FrameWeapons::ReadClassPoses(BladePoses(), 3, cp, error));
}

TEST_CASE("Weapon models: the hold from Pit's poses, by class", "[kiuweap2]") {
    // A blade: its grip bone in the right hand, as his idle and his aiming pose put it.
    Bytes bcres;
    std::string error;
    REQUIRE(FrameCgfx::DarcMember(BladeZrc(), "character/fmdl.arc!fmdl/00.bcres", bcres, error));
    FrameCgfx::File file;
    REQUIRE(FrameCgfx::ReadCgfx(bcres, file, error));
    const auto& m = file.models[0];
    CHECK(m.bones[2].translation[1] == Approx(0.076f)); // the bone's own, read
    FrameWeapons::ClassPoses poses;
    REQUIRE(FrameWeapons::ReadClassPoses(BladePoses(), 0, poses, error));
    const auto h = FrameWeapons::MakeHold(m, FrameWeapons::kBlade, m.BoneIndex("GripN"), poses);
    CHECK(h.known);
    REQUIRE(h.valid);
    CHECK(h.hand == "RHandN");
    CheckMat(h.idle.to_hand, FrameCgfx::LocalTransform({1, 1, 1}, {kPi, 0.117f, 0},
                                                       {-0.898f, -0.076f, 0}));
    CHECK(h.idle.fist[0] == Approx(-0.898f)); // the grip bone's origin
    CHECK(h.idle.fist[1] == Approx(-0.076f));
    CheckMat(h.shot.to_hand, FrameCgfx::LocalTransform({1, 1, 1}, {0.5f, 0, 0}, {0, 0.076f, 0}));
    // Without a grip bone (claws, as the bind pose has them): the hand itself, the fist the left
    // fist marker mirrored to the right.
    const auto c = FrameWeapons::MakeHold(m, FrameWeapons::kClaws, m.BoneIndex("RHandN"), poses);
    REQUIRE(c.valid);
    CheckMat(c.idle.to_hand, FrameCgfx::Identity34());
    CHECK(c.idle.fist[0] == Approx(-0.899f));
    CHECK(c.idle.fist[1] == Approx(0.076f));
    // A bow: in Pit's left hand, the marker as it is.
    const auto b = FrameWeapons::MakeHold(m, FrameWeapons::kBow, m.BoneIndex("LHandN"), poses);
    REQUIRE(b.valid);
    CHECK(b.hand == "LHandN");
    CHECK(b.idle.fist[0] == Approx(0.899f));
    // A cannon: no right hand, the forearm holds it; the fist where the hand would be (the left
    // arm's, mirrored).
    FrameCgfx::Model cannon;
    cannon.bones = {MakeBone("TopN", 0, -1, {0, 0, 0}), MakeBone("RArmN", 1, 0, {-3, 12, 0}),
                    MakeBone("LArmN", 2, 0, {3, 12, 0}), MakeBone("LHandN", 3, 2, {5.177f, 12, 0}),
                    MakeBone("LHandFistM", 4, 3, {6.076f, 12.076f, 0})};
    const auto k = FrameWeapons::MakeHold(cannon, FrameWeapons::kCannon, 1, {});
    REQUIRE(k.valid);
    CHECK(k.hand == "RArmN");
    CHECK(k.idle.fist[0] == Approx(-3.076f));
    CHECK(k.idle.fist[1] == Approx(0.076f));
    // No hand: not valid (the class's own fit), though known.
    FrameCgfx::Model bare;
    bare.bones = {MakeBone("TopN", 0, -1, {0, 0, 0})};
    const auto none = FrameWeapons::MakeHold(bare, FrameWeapons::kBlade, 0, poses);
    CHECK(none.known);
    CHECK_FALSE(none.valid);
}

TEST_CASE("Weapon models: the hold in the cache, made again once", "[kiuweap2]") {
    const std::string dir = TestDir("frame_weapon_hold_cache");
    constexpr u64 kTitle = 0x0004000000030300ull;
    auto rom = std::make_shared<MemoryRomFS>(
        RomFS("us", {{"m000.zrc", BladeZrc()}, {"g0.zrc", BladePoses()}, {"m200.zrc", ClawZrc()}}));
    FrameWeapons::RunResult r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.made == 2);
    FrameWeapons::Manifest m;
    std::string error;
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    REQUIRE(m.Find(0));
    REQUIRE(m.Find(24));
    const auto& h = m.Find(0)->hold;
    CHECK(h.known);
    REQUIRE(h.valid);
    CHECK(h.hand == "RHandN");
    CheckMat(h.idle.to_hand, FrameCgfx::LocalTransform({1, 1, 1}, {kPi, 0.117f, 0},
                                                       {-0.898f, -0.076f, 0}));
    CHECK(h.idle.fist[0] == Approx(-0.898f));
    CheckMat(h.shot.to_hand, FrameCgfx::LocalTransform({1, 1, 1}, {0.5f, 0, 0}, {0, 0.076f, 0}));
    // The claws' class has no poses in these files: known, not valid ("hold -").
    CHECK(m.Find(24)->hold.known);
    CHECK_FALSE(m.Find(24)->hold.valid);
    // Kept when nothing changed.
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.kept == 2);
    // A manifest from before the hold (no hold lines): each made again once, then kept; saved
    // again as it is meanwhile: still not known.
    {
        std::ifstream in(dir + "weapons.txt");
        std::string text, line;
        while (std::getline(in, line)) {
            if (line.rfind("hold", 0) != 0) {
                text += line + "\n";
            }
        }
        in.close();
        std::ofstream(dir + "weapons.txt") << text;
    }
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    CHECK_FALSE(m.Find(0)->hold.known);
    CHECK(m.version == FrameWeapons::kConverterVersion); // no version change
    REQUIRE(FrameWeapons::WriteManifest(dir + "weapons.txt", m, error));
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    CHECK_FALSE(m.Find(0)->hold.known);
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.made == 2);
    r = FrameWeapons::ConvertGame(rom, kTitle, dir);
    CHECK(r.kept == 2);
    REQUIRE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    CHECK(m.Find(0)->hold.valid);
    // A bad hold line: the manifest is not taken (made again), never a crash.
    {
        std::ofstream(dir + "weapons.txt", std::ios::app) << "hold RHandN 1 2 nan\n";
    }
    CHECK_FALSE(FrameWeapons::ReadManifest(dir + "weapons.txt", m, error));
    std::filesystem::remove_all(dir);
}
