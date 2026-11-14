// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: see frame_weapon_models.h.

#include "core/frame_weapon_models.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <thread>
#include <fmt/format.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "common/file_util.h"
#include "common/hash.h"
#include "common/logging/log.h"
#include "core/file_sys/romfs_reader.h"
#include "core/core.h"
#include "core/frame_cgfx.h"
#include "core/frontend/image_interface.h"
#include "core/loader/loader.h"
#include "video_core/custom_textures/custom_tex_manager.h"

namespace FrameWeapons {

namespace {

using FrameCgfx::Bytes;
using Vec3 = std::array<float, 3>;

constexpr u32 kModelVersion = 2;
constexpr u32 kFlagWeaponSpace = 2; // frame_xr/vis/model_file.h kModelFlagWeaponSpace
constexpr u32 kFlagFlatSurface = 1u << 16; // kModelFlagFlatSurface: a one-texel surface image
constexpr u32 kNameBytes = 32;
constexpr u32 kComponentBytes = 208;
constexpr u8 kRoughness = 90; // the surface image's alpha: fairly glossy, like the game's metal

struct ClassRule {
    const char* centre; ///< the anchor of a whole weapon
    const char* right;  ///< the anchors of a pair's halves ("" when the class is whole)
    const char* left;
    const char* side_right; ///< the bone that makes a triangle the right half
    const char* side_left;
};

// Ours, from the bones the weapons hang on (handoff kiuguns study, wpn-bones): blades and clubs
// have a grip bone; staffs hang on SnipeM; claws are one per hand; a bow's two halves are under
// the left hand; palms and arms are sleeves of the right arm; cannons replace the right hand;
// orbitars are a pair of their own, off the root.
constexpr ClassRule kRules[kClasses] = {
    {"GripN", "", "", "", ""},                      // blade
    {"SnipeM", "", "", "", ""},                     // staff
    {"RHandN", "RHandN", "LHandN", "RHandN", "LHandN"}, // claws
    {"LHandN", "", "", "", ""},                     // bow
    {"RHandN", "", "", "", ""},                     // palm
    {"GripN", "", "", "", ""},                      // club
    {"RArmN", "", "", "", ""},                      // cannon
    {"RBitN", "RBitN", "LBitN", "RBitN", "LBitN"},  // orbitars
    {"RHandN", "", "", "", ""},                     // arm
};

bool Paired(int cls) {
    return kRules[cls].right[0] != '\0';
}

const FrameCgfx::PslbValue* Params(const FrameCgfx::PslbValue& root, std::string& wpnid,
                                   FrameCgfx::PslbValue& merged) {
    const auto* list = root.Get("weaponParams_");
    if (!list || list->kind != FrameCgfx::PslbValue::Kind::Array || list->array.empty()) {
        return nullptr;
    }
    const auto& first = list->array[0];
    if (const auto* id = first.Get("weaponID_");
        id && id->kind == FrameCgfx::PslbValue::Kind::String) {
        wpnid = id->string;
    }
    const auto* params = first.Get("params_");
    if (!params || params->kind != FrameCgfx::PslbValue::Kind::Array) {
        return nullptr;
    }
    merged.kind = FrameCgfx::PslbValue::Kind::Object;
    for (const auto& p : params->array) {
        if (p.kind != FrameCgfx::PslbValue::Kind::Object) {
            continue;
        }
        for (std::size_t i = 0; i < p.keys.size() && i < p.array.size(); i++) {
            merged.keys.push_back(p.keys[i]);
            merged.array.push_back(p.array[i]);
        }
    }
    return &merged;
}

bool Numbers(const FrameCgfx::PslbValue* v, float* out, std::size_t n) {
    if (!v || v->kind != FrameCgfx::PslbValue::Kind::Array || v->array.size() < n) {
        return false;
    }
    for (std::size_t i = 0; i < n; i++) {
        const auto x = v->array[i].Number();
        if (!x) {
            return false;
        }
        out[i] = *x;
    }
    return true;
}

Side BoneSide(const FrameCgfx::Model& m, s32 bone, const ClassRule& rule) {
    for (int steps = 0; bone >= 0 && static_cast<std::size_t>(bone) < m.bones.size() && steps < 64;
         steps++) {
        const std::string& name = m.bones[static_cast<std::size_t>(bone)].name;
        if (name == rule.side_right) {
            return Side::Right;
        }
        if (name == rule.side_left) {
            return Side::Left;
        }
        bone = m.bones[static_cast<std::size_t>(bone)].parent;
    }
    return Side::Centre;
}

/// The bone a vertex of a rigid or unskinned submesh hangs on (-1: smooth, model space).
s32 VertexBone(const FrameCgfx::SubMesh& sub, const FrameCgfx::Vertex& v) {
    if (sub.skinning == 2) {
        return -1;
    }
    if (sub.bones.empty()) {
        return 0;
    }
    const std::size_t local = v.has_bones ? v.bones[0] : 0;
    return local < sub.bones.size() ? sub.bones[local] : sub.bones[0];
}

bool IsGlow(const std::string& material) {
    return material.find("Efe") != std::string::npos || material.rfind("WpnE", 0) == 0;
}

/// A texture name that reads as a weapon's base colour: "Rifle00", "WpnSnipe00", "snipe07",
/// "Bow02b", "Nail01_2" (optional "Wpn", letters, two digits, a letter, "_" and a digit).
bool NamesBaseColour(const std::string& n) {
    std::size_t i = n.rfind("Wpn", 0) == 0 ? 3 : 0;
    const std::size_t letters = i;
    while (i < n.size() && std::isalpha(static_cast<unsigned char>(n[i]))) {
        i++;
    }
    if (i == letters || i + 2 > n.size() || !std::isdigit(static_cast<unsigned char>(n[i])) ||
        !std::isdigit(static_cast<unsigned char>(n[i + 1]))) {
        return false;
    }
    i += 2;
    if (i < n.size() && std::islower(static_cast<unsigned char>(n[i]))) {
        i++;
    }
    if (i + 1 < n.size() && n[i] == '_' && std::isdigit(static_cast<unsigned char>(n[i + 1]))) {
        i += 2;
    }
    return i == n.size();
}

/// A texture name that reads as a mask, an environment or an effect map ("Rifle00_Mask",
/// "Pit_Ref", "Nail04_Env", "Palm00_ef", "Rifle02M", "Snipe04R", ...).
bool NamesOtherMap(const std::string& name) {
    std::string n;
    for (const char ch : name) {
        n += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    for (const char* part : {"mask", "ref", "env", "_ef", "efe", "spe", "noise", "msk"}) {
        if (n.find(part) != std::string::npos) {
            return true;
        }
    }
    const std::size_t len = n.size();
    return (len >= 3 && n.compare(len - 3, 3, "mas") == 0) ||
           (len >= 2 && std::isdigit(static_cast<unsigned char>(n[len - 2])) &&
            (n[len - 1] == 'm' || n[len - 1] == 'r'));
}

/// The material's base colour mapper, among those with a texture of this file that are not
/// environment mapped (mapper 0 is often a sphere map, often Pit's shared Pit_Ref): the one whose
/// name reads as a base colour, else one not named as a mask, environment or effect map, else
/// the first. The study's prototype rule, checked against the game's look for all 108.
int DiffuseMapper(const FrameCgfx::Material& mat, const FrameCgfx::File& file) {
    std::vector<int> candidates;
    for (int pass = 0; pass < 2 && candidates.empty(); pass++) {
        for (int i = 0; i < 3; i++) {
            const auto& mp = mat.mappers[static_cast<std::size_t>(i)];
            if (mp.texture.empty() || !file.FindTexture(mp.texture)) {
                continue;
            }
            bool uv = true;
            if (!mat.coords.empty()) {
                uv = mat.coords[std::min<std::size_t>(static_cast<std::size_t>(i),
                                                      mat.coords.size() - 1)]
                         .mapping == 0;
            }
            if (uv || pass == 1) {
                candidates.push_back(i);
            }
        }
    }
    if (candidates.empty()) {
        return -1;
    }
    for (const int i : candidates) {
        if (NamesBaseColour(mat.mappers[static_cast<std::size_t>(i)].texture)) {
            return i;
        }
    }
    for (const int i : candidates) {
        if (!NamesOtherMap(mat.mappers[static_cast<std::size_t>(i)].texture)) {
            return i;
        }
    }
    return candidates.front();
}

struct PartBuilder {
    std::string material;
    int mapper = -1;
    std::map<Side, std::vector<float>> vertices; // 12 floats each
    std::map<Side, std::vector<u32>> indices;
    std::map<Side, std::map<std::array<s32, 4>, u32>> seen; // shape, vertex, bone, mesh -> index
};

void Put32(Bytes& b, u32 v) {
    for (int k = 0; k < 4; k++) {
        b.push_back(static_cast<u8>(v >> (8 * k)));
    }
}

void PutF(Bytes& b, float f) {
    u32 v;
    std::memcpy(&v, &f, 4);
    Put32(b, v);
}

Bytes WriteModel(const PartBuilder& p, const FrameCgfx::Texture* tex, u32 wrap_u, u32 wrap_v,
                 std::vector<Side>& sides, u32& triangles) {
    std::vector<float> verts;
    std::vector<u32> idx;
    struct Comp {
        Side side;
        u32 first, count;
    };
    std::vector<Comp> comps;
    for (const auto& [side, v] : p.vertices) {
        const auto& ii = p.indices.at(side);
        if (ii.empty()) {
            continue;
        }
        const u32 base = static_cast<u32>(verts.size() / 12);
        comps.push_back({side, static_cast<u32>(idx.size()), static_cast<u32>(ii.size())});
        verts.insert(verts.end(), v.begin(), v.end());
        for (const u32 i : ii) {
            idx.push_back(base + i);
        }
        sides.push_back(side);
    }
    triangles = static_cast<u32>(idx.size() / 3);
    u32 tw = 8, th = 8;
    if (tex) {
        tw = tex->width;
        th = tex->height;
    }
    Bytes b;
    b.insert(b.end(), {'M', 'X', 'F', 'M'});
    Put32(b, kModelVersion);
    Put32(b, static_cast<u32>(verts.size() / 12));
    Put32(b, static_cast<u32>(idx.size()));
    Put32(b, static_cast<u32>(comps.size()));
    Put32(b, tw);
    Put32(b, th);
    Put32(b, kFlagWeaponSpace | (wrap_u & 7) << 8 | (wrap_v & 7) << 12 | kFlagFlatSurface);
    for (const Comp& c : comps) {
        Bytes rec(kComponentBytes, 0);
        const char* name = SideName(c.side);
        std::memcpy(rec.data(), name, std::strlen(name));
        // The material's name as the component's "path" (no input drives it).
        std::memcpy(rec.data() + kNameBytes, p.material.data(),
                    std::min<std::size_t>(p.material.size(), kNameBytes - 1));
        const auto put_at = [&](std::size_t at, u32 v) {
            for (int k = 0; k < 4; k++) {
                rec[at + k] = static_cast<u8>(v >> (8 * k));
            }
        };
        put_at(2 * kNameBytes, c.first);
        put_at(2 * kNameBytes + 4, c.count);
        put_at(2 * kNameBytes + 8, 0); // no motion
        b.insert(b.end(), rec.begin(), rec.end());
    }
    for (const float f : verts) {
        PutF(b, f);
    }
    for (const u32 i : idx) {
        Put32(b, i);
    }
    // Colour: the texture's colour, no ambient occlusion (alpha 255).
    const std::size_t px = std::size_t{tw} * th;
    for (std::size_t i = 0; i < px; i++) {
        if (tex) {
            b.insert(b.end(), {tex->rgba[i * 4], tex->rgba[i * 4 + 1], tex->rgba[i * 4 + 2], 255});
        } else {
            b.insert(b.end(), {160, 160, 160, 255});
        }
    }
    // Surface: one texel, a flat normal and one roughness (kFlagFlatSurface).
    b.insert(b.end(), {128, 128, 255, kRoughness});
    return b;
}

std::string Hex(u64 v) {
    return fmt::format("{:016X}", v);
}

bool WriteFileAtomic(const std::string& path, const void* data, std::size_t size) {
    const std::string tmp = path + ".part";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f || !f.write(static_cast<const char*>(data), static_cast<std::streamsize>(size))) {
            return false;
        }
    }
    return FileUtil::Rename(tmp, path);
}

} // namespace

bool LoadPackTexture(const std::string& path, bool flip_png, std::vector<u8>& rgba, u32& width,
                     u32& height) {
    std::ifstream f(path, std::ios::binary);
    const Bytes png((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    static Frontend::ImageInterface images; // lodepng
    u32 w = 0, h = 0;
    Bytes px;
    if (png.empty() || !images.DecodePNG(px, w, h, png) || w == 0 || h == 0 || w > 4096 ||
        h > 4096 || px.size() != std::size_t{w} * h * 4) {
        return false;
    }
    // A pack flips its PNGs (flip_png_files, the default) so their first row is the top of the
    // picture, the first row of the game's texture in memory, which is how models keep them.
    if (!flip_png) {
        for (u32 y = 0; y < h / 2; y++) {
            std::swap_ranges(px.begin() + std::size_t{y} * w * 4,
                             px.begin() + std::size_t{y + 1} * w * 4,
                             px.begin() + std::size_t{h - 1 - y} * w * 4);
        }
    }
    for (std::size_t i = 3; i < px.size(); i += 4) {
        px[i] = 255; // no ambient occlusion
    }
    rgba = std::move(px);
    width = w;
    height = h;
    return true;
}

TexturePack ReadTexturePack(u64 title_id) {
    TexturePack pack;
    const auto index =
        VideoCore::CustomTexManager::IndexPack(Core::System::GetInstance(), title_id);
    if (index.colour.empty()) {
        return pack;
    }
    if (!index.use_new_hash) {
        LOG_INFO(Frontend, "xr: weapon models: the texture pack names textures by the old hash; "
                           "the hand keeps the game's textures");
        return pack;
    }
    pack.on = true;
    pack.flip_png = index.flip_png_files;
    pack.files.insert(index.colour.begin(), index.colour.end());
    return pack;
}

u64 PackKey(const TexturePack* pack, const std::vector<u64>& hashes) {
    if (!pack || !pack->on) {
        return 0;
    }
    std::string key;
    for (const u64 h : hashes) {
        const auto it = pack->files.find(h);
        if (it == pack->files.end()) {
            continue;
        }
        std::error_code ec;
        const auto size = std::filesystem::file_size(it->second, ec);
        const auto time = std::filesystem::last_write_time(it->second, ec);
        key += fmt::format("{:016X}:{}:{}:{};", h, it->second, ec ? 0 : size,
                           ec ? 0 : time.time_since_epoch().count());
    }
    return key.empty() ? 0 : Common::ComputeHash64(key.data(), key.size());
}

const char* ClassName(int cls) {
    static constexpr const char* kNames[kClasses] = {"blade",  "staff", "claws",
                                                     "bow",    "palm",  "club",
                                                     "cannon", "orbitars", "arm"};
    return cls >= 0 && cls < kClasses ? kNames[cls] : "?";
}

std::string ZrcName(int id) {
    return fmt::format("m{:03d}.zrc", ClassOf(id) * 100 + id % kPerClass);
}

const char* SideName(Side side) {
    switch (side) {
    case Side::Right:
        return "R";
    case Side::Left:
        return "L";
    default:
        return "C";
    }
}

const Weapon* Manifest::Find(int id) const {
    for (const Weapon& w : weapons) {
        if (w.id == id) {
            return &w;
        }
    }
    return nullptr;
}

bool ConvertWeapon(int id, std::span<const u8> zrc, Converted& out, std::string& error,
                   const TexturePack* pack) {
    if (id < 0 || id >= kWeapons) {
        error = "not a weapon id";
        return false;
    }
    const int cls = ClassOf(id);
    const ClassRule& rule = kRules[cls];
    Bytes bcres, jsonb;
    if (!FrameCgfx::DarcMember(zrc, "character/fmdl.arc!fmdl/00.bcres", bcres, error)) {
        error = "the model: " + error;
        return false;
    }
    FrameCgfx::File file;
    if (!FrameCgfx::ReadCgfx(bcres, file, error)) {
        return false;
    }
    if (file.models.empty()) {
        error = "no model";
        return false;
    }
    const FrameCgfx::Model& m = file.models[0];
    Converted c;
    Weapon& w = c.info;
    w.id = id;
    w.cls = cls;
    w.source_hash = Common::ComputeHash64(zrc.data(), zrc.size());

    // The anchors: a side's own bone, else the hand, the forearm, the root.
    const auto bone_or = [&](const char* name) {
        for (const char* n : {name, "RHandN", "RArmN"}) {
            if (const s32 b = m.BoneIndex(n); b >= 0) {
                return b;
            }
        }
        return m.bones.empty() ? -1 : 0;
    };
    const auto anchor_of = [&](Side s) {
        return bone_or(s == Side::Left ? rule.left : s == Side::Right ? rule.right : rule.centre);
    };
    const auto to_anchor = [&](Side s) {
        const s32 b = anchor_of(s);
        return b >= 0 ? FrameCgfx::Inverse34(m.bones[static_cast<std::size_t>(b)].bind)
                      : FrameCgfx::Identity34();
    };
    const Side primary = Paired(cls) ? Side::Right : Side::Centre;
    {
        const s32 b = anchor_of(primary);
        w.anchor = b >= 0 ? m.bones[static_cast<std::size_t>(b)].name : std::string{};
    }
    const std::map<Side, FrameCgfx::Mat34> space{
        {Side::Centre, to_anchor(Side::Centre)},
        {Side::Right, to_anchor(Side::Right)},
        {Side::Left, to_anchor(Side::Left)}};

    // The weapon's meshes, per material and side.
    std::vector<PartBuilder> parts;
    bool any_bounds = false;
    for (std::size_t mi = 0; mi < m.meshes.size(); mi++) {
        const FrameCgfx::Mesh& mesh = m.meshes[mi];
        const FrameCgfx::Material& mat = m.materials[static_cast<std::size_t>(mesh.material)];
        if (mat.name == "Body" || mat.name == "Face" || !m.MeshShown(mesh)) {
            continue; // Pit, and what the game hides by default
        }
        auto it = std::find_if(parts.begin(), parts.end(),
                               [&](const PartBuilder& p) { return p.material == mat.name; });
        if (it == parts.end()) {
            PartBuilder p;
            p.material = mat.name;
            p.mapper = DiffuseMapper(mat, file);
            parts.push_back(std::move(p));
            it = parts.end() - 1;
        }
        PartBuilder& part = *it;
        FrameCgfx::TexCoord coord;
        if (part.mapper >= 0 && !mat.coords.empty()) {
            coord = mat.coords[std::min<std::size_t>(static_cast<std::size_t>(part.mapper),
                                                     mat.coords.size() - 1)];
        }
        const FrameCgfx::Shape& shape = m.shapes[static_cast<std::size_t>(mesh.shape)];
        for (const FrameCgfx::SubMesh& sub : shape.submeshes) {
            for (std::size_t t = 0; t + 2 < sub.indices.size(); t += 3) {
                // The triangle's side: its first corner's bone (or, smooth, its side of Pit).
                Side side = Side::Centre;
                if (Paired(cls)) {
                    const auto& v0 = shape.vertices[sub.indices[t]];
                    const s32 bone = VertexBone(sub, v0);
                    if (bone >= 0) {
                        side = BoneSide(m, bone, rule);
                    }
                    if (side == Side::Centre) {
                        const auto p = FrameCgfx::BindPosition(m, sub, v0);
                        side = p[0] <= 0.0f ? Side::Right : Side::Left; // Pit's right is -x
                    }
                }
                auto& verts = part.vertices[side];
                auto& idx = part.indices[side];
                auto& seen = part.seen[side];
                const std::size_t first = idx.size();
                for (int k = 0; k < 3; k++) {
                    const u32 vi = sub.indices[t + static_cast<std::size_t>(k)];
                    const auto& v = shape.vertices[vi];
                    const std::array<s32, 4> key{mesh.shape, static_cast<s32>(vi),
                                                 VertexBone(sub, v), static_cast<s32>(mi)};
                    if (const auto f = seen.find(key); f != seen.end()) {
                        idx.push_back(f->second);
                        continue;
                    }
                    Vec3 n;
                    const Vec3 pm = FrameCgfx::BindPosition(m, sub, v, &n);
                    const Vec3 p = FrameCgfx::Point(space.at(side), pm);
                    Vec3 nn = FrameCgfx::Direction(space.at(side), n);
                    const float len = std::sqrt(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]);
                    nn = len > 1e-6f ? Vec3{nn[0] / len, nn[1] / len, nn[2] / len}
                                     : Vec3{0.0f, 1.0f, 0.0f};
                    const auto& uv = coord.source == 1 ? v.uv1 : v.uv0;
                    const float u = uv[0] * coord.scale_u + coord.translate_u;
                    const float vv = uv[1] * coord.scale_v + coord.translate_v;
                    verts.insert(verts.end(), {p[0], p[1], p[2], nn[0], nn[1], nn[2], 1.0f, 0.0f,
                                               0.0f, 1.0f, u, 1.0f - vv});
                    const u32 out_index = static_cast<u32>(verts.size() / 12 - 1);
                    seen.emplace(key, out_index);
                    idx.push_back(out_index);
                    if (side == primary) {
                        for (int a = 0; a < 3; a++) {
                            const std::size_t ax = static_cast<std::size_t>(a);
                            if (!any_bounds) {
                                w.bounds_min[ax] = w.bounds_max[ax] = p[ax];
                            }
                            w.bounds_min[ax] = std::min(w.bounds_min[ax], p[ax]);
                            w.bounds_max[ax] = std::max(w.bounds_max[ax], p[ax]);
                        }
                        any_bounds = true;
                    }
                }
                // The winding that agrees with the normals, so a back face (drawn: nothing is
                // culled, the hand turns the weapon every way) gets the flipped normal.
                const auto at = [&](std::size_t k) { return &verts[std::size_t{idx[first + k]} * 12]; };
                const float* a = at(0);
                const float* b = at(1);
                const float* cc = at(2);
                const Vec3 e1{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
                const Vec3 e2{cc[0] - a[0], cc[1] - a[1], cc[2] - a[2]};
                const Vec3 face{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                                e1[0] * e2[1] - e1[1] * e2[0]};
                float agree = 0.0f;
                for (const float* q : {a, b, cc}) {
                    agree += face[0] * q[3] + face[1] * q[4] + face[2] * q[5];
                }
                if (agree < 0.0f) {
                    std::swap(idx[first + 1], idx[first + 2]);
                }
            }
        }
    }
    std::erase_if(parts, [](const PartBuilder& p) {
        for (const auto& [side, ii] : p.indices) {
            if (!ii.empty()) {
                return false;
            }
        }
        return true;
    });
    if (parts.empty()) {
        error = "no weapon meshes in " + m.name;
        return false;
    }
    // The main material first (its file is w<id>.model).
    std::stable_sort(parts.begin(), parts.end(), [](const PartBuilder& a, const PartBuilder& b) {
        return IsGlow(a.material) < IsGlow(b.material);
    });
    for (std::size_t i = 0; i < parts.size(); i++) {
        const PartBuilder& pb = parts[i];
        const FrameCgfx::Material* mat = nullptr;
        for (const auto& mm : m.materials) {
            if (mm.name == pb.material) {
                mat = &mm;
                break;
            }
        }
        const FrameCgfx::Texture* tex = nullptr;
        u32 wrap_u = 2, wrap_v = 2;
        if (mat && pb.mapper >= 0) {
            const auto& mp = mat->mappers[static_cast<std::size_t>(pb.mapper)];
            tex = file.FindTexture(mp.texture);
            wrap_u = mp.wrap_u;
            wrap_v = mp.wrap_v;
        }
        Part part;
        // The custom texture pack's replacement (the same hash the game's textures are looked
        // up by): named, and read from the pack when the model loads; the file keeps the game's
        // own texture for when the pack is gone.
        if (tex) {
            const u64 hash = VideoCore::CustomTexManager::NewHash(tex->raw);
            if (std::find(w.texture_hashes.begin(), w.texture_hashes.end(), hash) ==
                w.texture_hashes.end()) {
                w.texture_hashes.push_back(hash);
            }
            if (pack && pack->on) {
                if (const auto it = pack->files.find(hash); it != pack->files.end()) {
                    const auto ext = std::filesystem::path(it->second).extension().string();
                    if (ext == ".png" || ext == ".PNG") {
                        part.replaced = it->second;
                        part.replaced_flip = pack->flip_png;
                    } else {
                        LOG_INFO(Frontend, "xr: weapon models: {} is not a PNG: the game's "
                                           "texture is used",
                                 std::filesystem::path(it->second).filename().string());
                    }
                }
            }
        }
        part.file = i == 0 ? fmt::format("w{:03d}.model", id) : fmt::format("w{:03d}-{}.model", id, i);
        part.material = pb.material;
        part.wrap_u = wrap_u;
        part.wrap_v = wrap_v;
        part.glow = IsGlow(pb.material);
        part.cull = mat ? mat->cull : 0;
        Bytes bytes = WriteModel(pb, tex, wrap_u, wrap_v, part.sides, part.triangles);
        c.files.emplace_back(part.file, std::move(bytes));
        w.parts.push_back(std::move(part));
    }

    // The json: the muzzle and the fire parameters.
    if (FrameCgfx::DarcMember(zrc, "character/json.arc!json/00.json", jsonb, error)) {
        FrameCgfx::PslbValue root, merged;
        std::string why;
        if (FrameCgfx::ReadPslb(jsonb, root, why)) {
            if (const auto* p = Params(root, w.wpnid, merged)) {
                const auto* node = p->Get("muzzleNode_");
                float off[3]{};
                if (node && node->kind == FrameCgfx::PslbValue::Kind::String &&
                    Numbers(p->Get("muzzleOffset_"), off, 3)) {
                    if (const s32 b = m.BoneIndex(node->string); b >= 0) {
                        const Vec3 pm = FrameCgfx::Point(m.bones[static_cast<std::size_t>(b)].bind,
                                                         {off[0], off[1], off[2]});
                        w.muzzle = FrameCgfx::Point(space.at(primary), pm);
                        w.muzzle_valid = true;
                    }
                }
                Numbers(p->Get("chargeTime_"), w.charge_time.data(), 2);
                Numbers(p->Get("rapidFiretime_"), w.rapid_fire_time.data(), 2);
                if (const auto* rp = p->Get("rapidParams_")) {
                    if (const auto* rn = rp->Get("RAPID_NORMAL")) {
                        const char* keys[3] = {"fireTime_", "interval_", "bulletNum_"};
                        for (int k = 0; k < 3; k++) {
                            if (const auto* v = rn->Get(keys[k]); v && v->Number()) {
                                w.rapid_normal[static_cast<std::size_t>(k)] = *v->Number();
                            }
                        }
                    }
                }
            }
        }
    }
    w.pack_key = PackKey(pack, w.texture_hashes);
    error.clear();
    out = std::move(c);
    return true;
}

// ---- The manifest ----

// The manifest: plain lines (no JSON library: the binary stays small).
//   kiu-weapons <version>
//   title <title id, hex>
//   region <folder>
//   weapon <id> <WPNID> <anchor> <source hash> <pack key> <muzzle: 1 x y z, or 0>
//          <bounds: 6 numbers> <charge time: 2> <rapid fire time: 2> <rapid normal: 3>
//          <texture hashes: count, then each>
//   part <file> <material> <sides> <triangles> <wrap u> <wrap v> <glow> <cull> <flip> [pack PNG]
// A part line belongs to the weapon line before it; the PNG's path is the rest of the line.

bool WriteManifest(const std::string& path, const Manifest& m, std::string& error) {
    std::string out = "# Kid Icarus: Uprising's weapons, made by Azahar for Frame from this game's "
                      "own files on this machine. Not to be shared.\n";
    out += fmt::format("kiu-weapons {}\ntitle {}\nregion {}\n", m.version, Hex(m.title_id),
                       m.region.empty() ? "-" : m.region);
    const auto anchor = [](const std::string& a) { return a.empty() ? std::string{"-"} : a; };
    for (const Weapon& w : m.weapons) {
        out += fmt::format("weapon {} {} {} {} {} ", w.id, w.wpnid.empty() ? "-" : w.wpnid,
                           anchor(w.anchor), Hex(w.source_hash), Hex(w.pack_key));
        out += w.muzzle_valid
                   ? fmt::format("1 {} {} {}", w.muzzle[0], w.muzzle[1], w.muzzle[2])
                   : std::string{"0"};
        for (const float f : w.bounds_min) {
            out += fmt::format(" {}", f);
        }
        for (const float f : w.bounds_max) {
            out += fmt::format(" {}", f);
        }
        for (const float f : w.charge_time) {
            out += fmt::format(" {}", f);
        }
        for (const float f : w.rapid_fire_time) {
            out += fmt::format(" {}", f);
        }
        for (const float f : w.rapid_normal) {
            out += fmt::format(" {}", f);
        }
        out += fmt::format(" {}", w.texture_hashes.size());
        for (const u64 h : w.texture_hashes) {
            out += " " + Hex(h);
        }
        out += "\n";
        for (const Part& p : w.parts) {
            std::string sides;
            for (const Side side : p.sides) {
                sides += SideName(side);
            }
            out += fmt::format("part {} {} {} {} {} {} {} {} {}", p.file,
                               p.material.empty() ? "-" : p.material, sides.empty() ? "C" : sides,
                               p.triangles, p.wrap_u, p.wrap_v, p.glow ? 1 : 0, p.cull,
                               p.replaced_flip ? 1 : 0);
            if (!p.replaced.empty()) {
                out += " " + p.replaced;
            }
            out += "\n";
        }
    }
    if (!WriteFileAtomic(path, out.data(), out.size())) {
        error = "the manifest cannot be written";
        return false;
    }
    return true;
}

bool ReadManifest(const std::string& path, Manifest& out, std::string& error) {
    std::ifstream f(path);
    if (!f) {
        error = "no manifest";
        return false;
    }
    Manifest m;
    std::string line;
    Weapon* current = nullptr;
    bool header = false;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream in(line);
        std::string key;
        in >> key;
        const auto hex = [&](u64& v) {
            std::string t;
            in >> t;
            char* end = nullptr;
            v = std::strtoull(t.c_str(), &end, 16);
            return !t.empty() && end && *end == '\0';
        };
        if (key == "kiu-weapons") {
            header = static_cast<bool>(in >> m.version);
        } else if (key == "title") {
            hex(m.title_id);
        } else if (key == "region") {
            in >> m.region;
        } else if (key == "weapon") {
            Weapon w;
            int has_muzzle = 0;
            std::size_t textures = 0;
            in >> w.id >> w.wpnid >> w.anchor;
            bool ok = static_cast<bool>(in) && hex(w.source_hash) && hex(w.pack_key);
            in >> has_muzzle;
            if (has_muzzle) {
                in >> w.muzzle[0] >> w.muzzle[1] >> w.muzzle[2];
                w.muzzle_valid = true;
            }
            for (auto* arr : {&w.bounds_min, &w.bounds_max}) {
                in >> (*arr)[0] >> (*arr)[1] >> (*arr)[2];
            }
            in >> w.charge_time[0] >> w.charge_time[1] >> w.rapid_fire_time[0] >>
                w.rapid_fire_time[1] >> w.rapid_normal[0] >> w.rapid_normal[1] >>
                w.rapid_normal[2] >> textures;
            ok = ok && static_cast<bool>(in) && w.id >= 0 && w.id < kWeapons && textures <= 16;
            for (std::size_t i = 0; ok && i < textures; i++) {
                u64 h = 0;
                ok = hex(h);
                w.texture_hashes.push_back(h);
            }
            if (!ok) {
                error = "a bad weapon line";
                return false;
            }
            w.cls = ClassOf(w.id);
            for (std::string* t : {&w.wpnid, &w.anchor}) {
                if (*t == "-") {
                    t->clear();
                }
            }
            m.weapons.push_back(std::move(w));
            current = &m.weapons.back();
        } else if (key == "part" && current) {
            Part p;
            std::string sides;
            int glow = 0, flip = 1;
            in >> p.file >> p.material >> sides >> p.triangles >> p.wrap_u >> p.wrap_v >> glow >>
                p.cull >> flip;
            if (!in || p.file.find('/') != std::string::npos ||
                p.file.find("..") != std::string::npos || p.wrap_u > 3 || p.wrap_v > 3) {
                error = "a bad part line";
                return false;
            }
            std::getline(in >> std::ws, p.replaced);
            for (const char ch : sides) {
                p.sides.push_back(ch == 'R' ? Side::Right : ch == 'L' ? Side::Left : Side::Centre);
            }
            p.glow = glow != 0;
            p.replaced_flip = flip != 0;
            current->parts.push_back(std::move(p));
        }
    }
    if (!header) {
        error = "not a weapons manifest";
        return false;
    }
    out = std::move(m);
    return true;
}

std::string WeaponDir(u64 title_id) {
    std::string root;
    if (const char* env = std::getenv("AZAHARFRAME_MODELS"); env && *env) {
        root = env;
        if (root.back() != '/') {
            root += '/';
        }
    } else {
        root = FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + "frame-models/";
    }
    return root + "games/" + Hex(title_id) + "/kiu-weapons/";
}

namespace {

/// A game's weapons made one at a time into its folder, the manifest kept up to date.
class GameConverter {
public:
    bool Open(std::shared_ptr<FileSys::RomFSReader> romfs, u64 title, const std::string& folder,
              const TexturePack* texture_pack, RunResult& res) {
        title_id = title;
        dir = folder;
        pack = texture_pack;
        if (!index.Open(std::move(romfs), res.error)) {
            return false;
        }
        // The region folder: the one with player/m<class><index>.zrc files.
        for (const auto& f : index.Files()) {
            const auto slash = f.path.find('/');
            if (slash == std::string::npos) {
                continue;
            }
            const std::string rest = f.path.substr(slash);
            if (rest.size() == 16 && rest.rfind("/player/m", 0) == 0 &&
                rest.compare(12, 4, ".zrc") == 0 &&
                std::isdigit(static_cast<unsigned char>(rest[9])) &&
                std::isdigit(static_cast<unsigned char>(rest[10])) &&
                std::isdigit(static_cast<unsigned char>(rest[11]))) {
                res.region = f.path.substr(0, slash);
                break;
            }
        }
        if (res.region.empty()) {
            res.error = "no player/m<nnn>.zrc in the RomFS: not Kid Icarus: Uprising's files";
            return false;
        }
        if (!FileUtil::CreateFullPath(dir)) {
            res.error = "the folder cannot be made";
            return false;
        }
        std::string why;
        if (!ReadManifest(dir + "weapons.txt", manifest, why) ||
            manifest.version != kConverterVersion || manifest.title_id != title_id) {
            manifest = {};
        }
        manifest.version = kConverterVersion;
        manifest.title_id = title_id;
        manifest.region = res.region;
        return true;
    }

    /// One weapon current in the folder: true when it is (kept or made).
    bool Ensure(int id, RunResult& res) {
        const auto* f = index.Find(manifest.region + "/player/" + ZrcName(id));
        Bytes zrc;
        if (!f || !index.Read(*f, zrc, u64{8} << 20)) {
            LOG_WARNING(Frontend, "xr: weapon models: {} ({}) cannot be read", ZrcName(id), id);
            res.failed++;
            return false;
        }
        const u64 hash = Common::ComputeHash64(zrc.data(), zrc.size());
        auto old = std::find_if(manifest.weapons.begin(), manifest.weapons.end(),
                                [id](const Weapon& w) { return w.id == id; });
        if (old != manifest.weapons.end() && old->source_hash == hash &&
            old->pack_key == PackKey(pack, old->texture_hashes) &&
            std::all_of(old->parts.begin(), old->parts.end(),
                        [&](const Part& p) { return FileUtil::Exists(dir + p.file); })) {
            res.kept++;
            return true;
        }
        Converted c;
        std::string why;
        if (!ConvertWeapon(id, zrc, c, why, pack)) {
            LOG_WARNING(Frontend, "xr: weapon models: {} ({}): {}", ZrcName(id), id, why);
            res.failed++;
            return false;
        }
        for (const auto& [name, bytes] : c.files) {
            if (!WriteFileAtomic(dir + name, bytes.data(), bytes.size())) {
                LOG_WARNING(Frontend, "xr: weapon models: w{:03d} cannot be written", id);
                res.failed++;
                return false;
            }
        }
        if (old != manifest.weapons.end()) {
            // A part an older file had and this one has not.
            for (const Part& p : old->parts) {
                if (std::none_of(c.info.parts.begin(), c.info.parts.end(),
                                 [&](const Part& q) { return q.file == p.file; })) {
                    FileUtil::Delete(dir + p.file);
                }
            }
            manifest.weapons.erase(old);
        }
        for (const Part& p : c.info.parts) {
            res.replaced += p.replaced.empty() ? 0 : 1;
        }
        manifest.weapons.push_back(std::move(c.info));
        std::sort(manifest.weapons.begin(), manifest.weapons.end(),
                  [](const Weapon& x, const Weapon& y) { return x.id < y.id; });
        res.made++;
        dirty = true;
        return true;
    }

    bool Save(RunResult& res) {
        if (!dirty) {
            return true;
        }
        std::string why;
        if (!WriteManifest(dir + "weapons.txt", manifest, why)) {
            res.error = why;
            return false;
        }
        dirty = false;
        return true;
    }

private:
    FrameCgfx::RomFSIndex index;
    Manifest manifest;
    u64 title_id = 0;
    std::string dir;
    const TexturePack* pack = nullptr;
    bool dirty = false;
};

} // namespace

RunResult ConvertGame(std::shared_ptr<FileSys::RomFSReader> romfs, u64 title_id,
                      const std::string& dir, const std::atomic<bool>* cancel,
                      const TexturePack* pack) {
    RunResult res;
    GameConverter game;
    if (!game.Open(std::move(romfs), title_id, dir, pack, res)) {
        return res;
    }
    int good = 0;
    for (int id = 0; id < kWeapons; id++) {
        if (cancel && cancel->load()) {
            res.error = "stopped";
            break;
        }
        good += game.Ensure(id, res) ? 1 : 0;
    }
    res.ok = game.Save(res) && res.error.empty() && good > 0;
    return res;
}

int RunCli(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "--help") {
        std::fprintf(stderr, "usage: azahar --frame-weapon-models GAME [OUTDIR]\n"
                             "Makes Kid Icarus: Uprising's weapon models from the game's own "
                             "files (OUTDIR: the user folder's frame-models/games/<title id>/"
                             "kiu-weapons/).\n");
        return 2;
    }
    auto loader = Loader::GetLoader(args[0]);
    if (!loader) {
        std::fprintf(stderr, "frame-weapon-models: not a game file Azahar reads\n");
        return 1;
    }
    u64 title = 0;
    loader->ReadProgramId(title);
    std::shared_ptr<FileSys::RomFSReader> romfs;
    if (loader->ReadRomFS(romfs) != Loader::ResultStatus::Success || !romfs) {
        std::fprintf(stderr, "frame-weapon-models: the game's RomFS cannot be read\n");
        return 1;
    }
    std::vector<std::string> rest(args.begin() + 1, args.end());
    const bool textures = std::erase(rest, std::string{"--textures"}) > 0;
    std::string dir = !rest.empty() ? rest[0] : WeaponDir(title);
    TexturePack pack;
    if (textures) {
        pack = ReadTexturePack(title);
        std::printf("frame-weapon-models: texture pack %s, %zu colour textures\n",
                    pack.on ? "on" : "off", pack.files.size());
    }
    if (!dir.empty() && dir.back() != '/') {
        dir += '/';
    }
    const auto t0 = std::chrono::steady_clock::now();
    const RunResult r = ConvertGame(romfs, title, dir, nullptr, &pack);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("frame-weapon-models: title %016llX, region %s: %d made (%d textures from the "
                "pack), %d kept, %d failed in %.1f s%s%s\n",
                static_cast<unsigned long long>(title), r.region.c_str(), r.made, r.replaced,
                r.kept, r.failed, s, r.error.empty() ? "" : ": ", r.error.c_str());
    return r.ok && r.failed == 0 ? 0 : 1;
}

// ---- The booted game ----

namespace {

struct Booted {
    std::mutex mutex;
    std::condition_variable wake;
    u64 title = 0;
    std::shared_ptr<FileSys::RomFSReader> romfs;
    bool custom_textures = false;
    bool started = false;
    std::thread worker;
    std::atomic<bool> cancel{false};
    std::atomic<int> state{static_cast<int>(State::None)};
    std::atomic<u32> generation{0};
    std::vector<int> queue;                ///< under mutex
    std::array<State, kWeapons> weapons{}; ///< under mutex
};

Booted& TheBooted() {
    static Booted b;
    return b;
}

void StartLocked(Booted& b) {
    if (b.started || !b.romfs || b.title == 0) {
        return;
    }
    b.started = true;
    b.cancel = false;
    b.state = static_cast<int>(State::Working);
    b.worker = std::thread([&b, romfs = b.romfs, title = b.title, textures = b.custom_textures] {
        // The lowest priority: the game and the XR thread come first.
        setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 19);
        const TexturePack pack = textures ? ReadTexturePack(title) : TexturePack{};
        GameConverter game;
        RunResult res;
        if (!game.Open(romfs, title, WeaponDir(title), &pack, res)) {
            LOG_WARNING(Frontend, "xr: weapon models: none ({})", res.error);
            b.state = static_cast<int>(State::Failed);
            b.generation++;
            return;
        }
        LOG_INFO(Frontend, "xr: weapon models: ready to make them (region {}, texture pack {}; "
                           "frame-models/games/{:016X}/kiu-weapons)",
                 res.region, pack.on ? "on" : "off", title);
        b.state = static_cast<int>(State::Ready);
        b.generation++;
        while (true) {
            int id = -1;
            {
                std::unique_lock lock{b.mutex};
                b.wake.wait(lock, [&] { return b.cancel.load() || !b.queue.empty(); });
                if (b.cancel) {
                    return;
                }
                id = b.queue.front();
                b.queue.erase(b.queue.begin());
            }
            const auto t0 = std::chrono::steady_clock::now();
            RunResult one;
            const bool ok = game.Ensure(id, one) && game.Save(one);
            const double ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                    .count();
            LOG_INFO(Frontend, "xr: weapon models: {} {} in {:.0f} ms{}", ZrcName(id),
                     !ok ? "failed" : one.made ? "made" : "current", ms,
                     one.replaced ? " (its texture from the texture pack)" : "");
            {
                std::scoped_lock lock{b.mutex};
                b.weapons[static_cast<std::size_t>(id)] = ok ? State::Ready : State::Failed;
            }
            b.generation++;
        }
    });
}

} // namespace

void OnGameBoot(u64 title_id, std::shared_ptr<FileSys::RomFSReader> romfs, bool start,
                bool custom_textures) {
    OnGameStop();
    Booted& b = TheBooted();
    std::scoped_lock lock{b.mutex};
    b.title = title_id;
    b.romfs = std::move(romfs);
    b.custom_textures = custom_textures;
    b.started = false;
    b.state = static_cast<int>(State::None);
    b.queue.clear();
    b.weapons.fill(State::None);
    if (start) {
        StartLocked(b);
    }
}

void OnGameStop() {
    Booted& b = TheBooted();
    std::thread worker;
    {
        std::scoped_lock lock{b.mutex};
        b.cancel = true;
        worker = std::move(b.worker);
        b.romfs.reset();
        b.title = 0;
        b.started = false;
        b.state = static_cast<int>(State::None);
        b.queue.clear();
        b.weapons.fill(State::None);
    }
    b.wake.notify_all();
    if (worker.joinable()) {
        worker.join(); // between two weapons: a few milliseconds at most
    }
}

void Request() {
    Booted& b = TheBooted();
    std::scoped_lock lock{b.mutex};
    StartLocked(b);
}

void Request(int id) {
    if (id < 0 || id >= kWeapons) {
        return;
    }
    Booted& b = TheBooted();
    {
        std::scoped_lock lock{b.mutex};
        StartLocked(b);
        if (!b.started || b.weapons[static_cast<std::size_t>(id)] != State::None) {
            return; // asked for already this boot
        }
        b.weapons[static_cast<std::size_t>(id)] = State::Working;
        b.queue.push_back(id);
    }
    b.wake.notify_all();
}

State CurrentState() {
    return static_cast<State>(TheBooted().state.load());
}

State WeaponState(int id) {
    if (id < 0 || id >= kWeapons) {
        return State::Failed;
    }
    Booted& b = TheBooted();
    std::scoped_lock lock{b.mutex};
    return b.weapons[static_cast<std::size_t>(id)];
}

u64 CurrentTitle() {
    Booted& b = TheBooted();
    std::scoped_lock lock{b.mutex};
    return b.title;
}

u32 Generation() {
    return TheBooted().generation.load();
}

} // namespace FrameWeapons
