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
#include <optional>
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
#include <astcenc.h>  // AzaharFrame fixc: a pack's ASTC replacements decoded for the hand
#include <dds-ktx.h> // AzaharFrame fixc

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
    /// AzaharFrame kiumodels: the texture coordinator the mapper's UVs come from, and its scale.
    s32 coord = 0;
    float scale_u = 1.0f, scale_v = 1.0f;
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

// ---- AzaharFrame kiuflash: the flash cards ----

constexpr u32 kFlagFlash = 1u << 17; // frame_xr/vis/model_file.h kModelFlagFlash

/// The game's own word for a class in its effect models' names ("PtcRifle01RapidFlashA").
constexpr const char* kEffectClass[kClasses] = {"Rifle",  "Snipe",  "Nail", "Bow", "Palm",
                                                "Pillar", "Launch", "Bit",  "Arm"};

enum class CardRole : u8 { Face, Fan };

struct Card {
    FlashKind kind = FlashKind::Rapid;
    CardRole role = CardRole::Face;
    std::string texture;
    std::array<float, kFlashValues> values{};
    float width = 0.0f, length = 0.0f; ///< game units: across, and along (a face's height)
    float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f; ///< v: rows in memory order
    bool apex_at_v1 = false; ///< a fan: its point at v1 (else at v0)
    u32 wrap_u = 0, wrap_v = 0;
    int priority = 0;
};

/// How much light a texel gives in a mode (0 to 1).
float TexelLight(const FrameCgfx::Texture& t, u32 x, u32 y, FlashMode mode) {
    const u8* p = t.rgba.data() + (std::size_t{y} * t.width + x) * 4;
    switch (mode) {
    case kFlashTwoTone:
        return std::max(p[0], p[1]) / 255.0f;
    case kFlashLuminance:
        return p[0] / 255.0f;
    default:
        return (p[0] + p[1] + p[2]) / (3.0f * 255.0f) * (p[3] / 255.0f);
    }
}

/// The light of a column (axis 0) or row (axis 1) of a texture's region.
float LineLight(const FrameCgfx::Texture& t, int axis, u32 at, u32 from, u32 to, FlashMode mode) {
    float sum = 0.0f;
    for (u32 k = from; k < to; k++) {
        sum += axis == 0 ? TexelLight(t, at, k, mode) : TexelLight(t, k, at, mode);
    }
    return sum;
}

/// A sprite sheet's cell along one axis: `cells` cells (the set's texture coordinate scale), or
/// when the set gives none (0), two where the texture's middle lines are dark (a gutter), else
/// one. Returns the lightest cell's range.
std::pair<u32, u32> SheetCell(const FrameCgfx::Texture& t, int axis, FlashMode mode, u32 cells) {
    const u32 n = axis == 0 ? t.width : t.height;
    const u32 across = axis == 0 ? t.height : t.width;
    std::vector<float> light(n);
    float peak = 0.0f;
    for (u32 k = 0; k < n; k++) {
        light[k] = LineLight(t, axis, k, 0, across, mode);
        peak = std::max(peak, light[k]);
    }
    if (cells == 0) {
        cells = n >= 16 && peak > 0.0f &&
                        light[n / 2 - 1] + light[n / 2] <= 0.04f * peak ? 2 : 1;
    }
    if (cells <= 1 || cells > 8 || n % cells != 0) {
        return {0, n};
    }
    const u32 size = n / cells;
    u32 best = 0;
    float best_light = -1.0f;
    for (u32 c = 0; c < cells; c++) {
        float sum = 0.0f;
        for (u32 k = c * size; k < (c + 1) * size; k++) {
            sum += light[k];
        }
        if (sum > best_light + 1e-4f) {
            best = c;
            best_light = sum;
        }
    }
    return {best * size, (best + 1) * size};
}

/// The cards of one flash model: its sets (smoke and unlit ones left out), the strongest first,
/// at most kFlashCardsMax.
std::vector<Card> FlashCards(const FrameCgfx::ParticleModel& model, FlashKind kind,
                             const std::vector<FrameCgfx::Texture>& textures, bool neutral) {
    std::vector<Card> cards;
    for (const FrameCgfx::ParticleSet& s : model.sets) {
        std::string lower = s.name + " " + s.texture;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (lower.find("smoke") != std::string::npos || lower.find("smork") != std::string::npos ||
            lower.find("kumo") != std::string::npos) {
            continue; // smoke: dark and slow, not a flash
        }
        const auto tex = std::find_if(textures.begin(), textures.end(),
                                      [&](const FrameCgfx::Texture& t) { return t.name == s.texture; });
        if (tex == textures.end()) {
            continue;
        }
        float rgb[3] = {((s.colour >> 16) & 0xFF) / 255.0f, ((s.colour >> 8) & 0xFF) / 255.0f,
                        (s.colour & 0xFF) / 255.0f};
        const u32 colour_mode = s.type & 0xFF;
        FlashMode mode = colour_mode == 4   ? kFlashTwoTone
                         : colour_mode >= 2 ? kFlashLuminance
                                            : kFlashTexture;
        // No colour of its own (its Type's combiner byte differs, 0 or 3 for these): the
        // texture's own colours, as they are.
        if (rgb[0] + rgb[1] + rgb[2] <= 0.0f) {
            rgb[0] = rgb[1] = rgb[2] = 1.0f;
            mode = kFlashTexture;
        }
        if (neutral) {
            const float v = std::max({rgb[0], rgb[1], rgb[2], 0.6f});
            rgb[0] = rgb[1] = rgb[2] = v;
        }
        Card c;
        c.kind = kind;
        c.texture = s.texture;
        c.values[kFlashMode] = static_cast<float>(mode);
        c.values[kFlashRed] = rgb[0];
        c.values[kFlashGreen] = rgb[1];
        c.values[kFlashBlue] = rgb[2];
        const float core = ((s.colour >> 24) & 0xFF) / 255.0f;
        c.values[kFlashCore] = mode == kFlashTwoTone ? (core > 0.0f ? core : 0.5f) : 0.0f;
        const u32 mirror = (s.type >> 16) & 0xFF;
        c.wrap_u = (s.sampler >> 12) & 0xF;
        c.wrap_v = (s.sampler >> 8) & 0xF;
        c.wrap_u = c.wrap_u > 3 ? 0 : c.wrap_u;
        c.wrap_v = c.wrap_v > 3 ? 0 : c.wrap_v;
        // Mirrored repeat on an axis (the sampler's): the texture is a half or a quarter.
        const bool mirror_u = c.wrap_u == 3;
        const bool mirror_v = c.wrap_v == 3;
        c.role = mirror == 2 ? CardRole::Fan : CardRole::Face;
        const FrameCgfx::Texture& t = *tex;
        // The UVs: a mirrored axis whose light reaches an edge spans two copies about its
        // lighter edge (a quarter glow made round, half a fan made whole; a sprite centred in its
        // texture stays whole); another axis the lighter cell of a sprite sheet.
        const auto axis_range = [&](int axis, bool mirrored, float& lo, float& hi, u32& texels) {
            const u32 n = axis == 0 ? t.width : t.height;
            const u32 across = axis == 0 ? t.height : t.width;
            if (mirrored) {
                float peak = 0.0f;
                for (u32 k = 0; k < n; k++) {
                    peak = std::max(peak, LineLight(t, axis, k, 0, across, mode));
                }
                const float e0 = LineLight(t, axis, 0, 0, across, mode);
                const float e1 = LineLight(t, axis, n - 1, 0, across, mode);
                if (std::max(e0, e1) > 0.15f * peak) {
                    lo = e0 >= e1 ? -1.0f : 0.0f;
                    hi = lo + 2.0f;
                    texels = n * 2;
                    return;
                }
            }
            const float scale = s.uv_scale[static_cast<std::size_t>(axis)];
            const u32 cells = scale > 0.0f ? static_cast<u32>(std::lround(1.0f / scale)) : 0;
            const auto [a, b] = SheetCell(t, axis, mode, cells);
            lo = static_cast<float>(a) / static_cast<float>(n);
            hi = static_cast<float>(b) / static_cast<float>(n);
            texels = b - a;
        };
        u32 cell_w = 0, cell_h = 0;
        axis_range(0, mirror_u, c.u0, c.u1, cell_w);
        axis_range(1, mirror_v, c.v0, c.v1, cell_h);
        // The size: the set's own, else ours.
        float size_x = s.size[0];
        float size_y = s.size[1];
        const bool read = size_x > 0.0f && size_y > 0.0f;
        if (!read) {
            size_x = size_y = c.role == CardRole::Fan ? 12.0f : 6.0f;
        }
        const float aspect = static_cast<float>(cell_h) / static_cast<float>(std::max(cell_w, 1u));
        if (c.role == CardRole::Fan) {
            // Its point at the darker end along its length; as long as its larger size, as wide
            // as its texture's shape (made whole) says.
            const u32 rows = std::max(1u, t.height / 4);
            float first = 0.0f, last = 0.0f;
            for (u32 k = 0; k < rows; k++) {
                first += LineLight(t, 1, k, 0, t.width, mode);
                last += LineLight(t, 1, t.height - 1 - k, 0, t.width, mode);
            }
            c.apex_at_v1 = last < first;
            c.length = std::max(size_x, size_y);
            c.width = c.length / std::max(aspect, 0.25f);
            c.values[kFlashOffset] = 0.0f;
            c.priority = 1;
        } else {
            // Its size across and down; a square size takes the cell's shape. Centred half its
            // size ahead of the muzzle, so the weapon's own tip (or a cannon's barrel, its muzzle
            // inside) does not hide it.
            c.width = size_x;
            c.length = std::abs(size_x - size_y) < 1e-3f ? size_x * aspect : size_y;
            c.values[kFlashOffset] = 0.5f * std::max(c.width, c.length);
            c.priority = mirror_u && mirror_v ? 2 : mode == kFlashLuminance ? 3 : 0;
        }
        c.values[kFlashGrow] = 1.25f; // ours: a slight scale-up (the sets' own animation is not read)
        cards.push_back(std::move(c));
    }
    // At most kFlashCardsMax: the first of each kind (burst, fan, glow, light) first, then the
    // rest in that order, each in the file's order.
    std::stable_sort(cards.begin(), cards.end(),
                     [](const Card& a, const Card& b) { return a.priority < b.priority; });
    std::vector<Card> kept;
    std::vector<bool> taken(cards.size(), false);
    for (std::size_t i = 0; i < cards.size(); i++) {
        if (i == 0 || cards[i].priority != cards[i - 1].priority) {
            kept.push_back(cards[i]);
            taken[i] = true;
        }
    }
    for (std::size_t i = 0; i < cards.size(); i++) {
        if (!taken[i]) {
            kept.push_back(cards[i]);
        }
    }
    if (kept.size() > static_cast<std::size_t>(kFlashCardsMax)) {
        kept.resize(kFlashCardsMax);
    }
    return kept;
}

/// One texture's cards as an MXFM file (weapon space, flagged as a flash: the texture keeps its
/// alpha; one flat surface texel).
Bytes WriteFlashModel(const FrameCgfx::Texture& tex, const std::vector<const Card*>& cards) {
    std::vector<float> verts;
    std::vector<u32> idx;
    for (const Card* c : cards) {
        const u32 base = static_cast<u32>(verts.size() / 12);
        const float x0 = -c->width / 2.0f, x1 = c->width / 2.0f;
        float y0 = -c->length / 2.0f, y1 = c->length / 2.0f;
        float va = c->v0, vb = c->v1; // v at y0, y1
        if (c->role == CardRole::Fan) {
            y0 = 0.0f;
            y1 = c->length;
            if (c->apex_at_v1) {
                std::swap(va, vb);
            }
        } else {
            std::swap(va, vb); // the texture's first row at the top
        }
        const float corners[4][4] = {{x0, y0, c->u0, va}, {x1, y0, c->u1, va},
                                     {x1, y1, c->u1, vb}, {x0, y1, c->u0, vb}};
        for (const auto& q : corners) {
            verts.insert(verts.end(), {q[0], q[1], 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
                                       1.0f, q[2], q[3]});
        }
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    const Card& first = *cards.front();
    Bytes b;
    b.insert(b.end(), {'M', 'X', 'F', 'M'});
    Put32(b, kModelVersion);
    Put32(b, static_cast<u32>(verts.size() / 12));
    Put32(b, static_cast<u32>(idx.size()));
    Put32(b, static_cast<u32>(cards.size()));
    Put32(b, tex.width);
    Put32(b, tex.height);
    Put32(b, kFlagWeaponSpace | (first.wrap_u & 7) << 8 | (first.wrap_v & 7) << 12 |
                 kFlagFlatSurface | kFlagFlash);
    for (std::size_t i = 0; i < cards.size(); i++) {
        const Card& c = *cards[i];
        Bytes rec(kComponentBytes, 0);
        const char* name = c.kind == FlashKind::Charged ? "charged" : "rapid";
        const char* role = c.role == CardRole::Fan ? "fan" : "face";
        std::memcpy(rec.data(), name, std::strlen(name));
        std::memcpy(rec.data() + kNameBytes, role, std::strlen(role));
        const auto put_at = [&](std::size_t at, u32 v) {
            for (int k = 0; k < 4; k++) {
                rec[at + k] = static_cast<u8>(v >> (8 * k));
            }
        };
        put_at(2 * kNameBytes, static_cast<u32>(i * 6));
        put_at(2 * kNameBytes + 4, 6);
        put_at(2 * kNameBytes + 8, 0); // no motion: the floats are the card's values
        for (int k = 0; k < kFlashValues; k++) {
            u32 bits;
            std::memcpy(&bits, &c.values[static_cast<std::size_t>(k)], 4);
            put_at(2 * kNameBytes + 16 + std::size_t(k) * 4, bits);
        }
        b.insert(b.end(), rec.begin(), rec.end());
    }
    for (const float f : verts) {
        PutF(b, f);
    }
    for (const u32 i : idx) {
        Put32(b, i);
    }
    b.insert(b.end(), tex.rgba.begin(), tex.rgba.end()); // with its alpha
    b.insert(b.end(), {128, 128, 255, kRoughness});
    return b;
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

namespace {

/// AzaharFrame fixc: a replacement's kind, by its extension.
enum class PackFileKind { Png, Ktx, Dds, Other };
PackFileKind PackFileKindOf(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" ? PackFileKind::Png
           : ext == ".ktx" ? PackFileKind::Ktx
           : ext == ".dds" ? PackFileKind::Dds
                           : PackFileKind::Other;
}

const char* DdsKtxFormatName(ddsktx_format f) {
    switch (f) {
    case DDSKTX_FORMAT_RGBA8:
        return "RGBA8";
    case DDSKTX_FORMAT_ASTC4x4:
        return "ASTC 4x4";
    case DDSKTX_FORMAT_ASTC6x6:
        return "ASTC 6x6";
    case DDSKTX_FORMAT_ASTC8x6:
        return "ASTC 8x6";
    case DDSKTX_FORMAT_BC1:
        return "BC1";
    case DDSKTX_FORMAT_BC3:
        return "BC3";
    case DDSKTX_FORMAT_BC5:
        return "BC5";
    case DDSKTX_FORMAT_BC7:
        return "BC7";
    default:
        return "an unknown format";
    }
}

/// The block of an ASTC format; none for the others.
std::optional<std::pair<u32, u32>> AstcBlock(ddsktx_format f) {
    switch (f) {
    case DDSKTX_FORMAT_ASTC4x4:
        return std::pair<u32, u32>{4, 4};
    case DDSKTX_FORMAT_ASTC6x6:
        return std::pair<u32, u32>{6, 6};
    case DDSKTX_FORMAT_ASTC8x6:
        return std::pair<u32, u32>{8, 6};
    default:
        return std::nullopt;
    }
}

/// AzaharFrame fixc: a KTX or DDS replacement (as the texture pack loader reads them,
/// video_core/custom_textures/material.cpp LoadDDS): its largest mip level no larger than
/// kPackDecodeMaxSide a side (a bigger one is never decoded, so the hand never holds more than
/// 4 MB of it), decoded to RGBA8: ASTC through astcenc (the LDR profile the pack converter
/// encodes with), RGBA8 as it is. The rows turned over: the file holds them as the game's
/// texture is uploaded (a flipped PNG), the model keeps the picture's top first.
bool DecodePackDdsKtx(const Bytes& file, std::vector<u8>& rgba, u32& width, u32& height,
                      std::string& why) {
    ddsktx_texture_info info{};
    ddsktx_error err{};
    if (file.empty() || !ddsktx_parse(&info, file.data(), static_cast<int>(file.size()), &err)) {
        why = file.empty() ? "cannot be read" : std::string{"not a KTX or DDS file: "} + err.msg;
        return false;
    }
    if ((info.flags & (DDSKTX_TEXTURE_FLAG_CUBEMAP | DDSKTX_TEXTURE_FLAG_VOLUME)) ||
        info.num_layers > 1 || info.depth > 1 || info.width <= 0 || info.height <= 0) {
        why = "not a plain 2D texture";
        return false;
    }
    const auto block = AstcBlock(info.format);
    if (!block && info.format != DDSKTX_FORMAT_RGBA8) {
        why = fmt::format("{}: only ASTC and RGBA8 are read for the hand",
                          DdsKtxFormatName(info.format));
        return false;
    }
    const auto level_size = [&](u32 w, u32 h) -> std::size_t {
        return block ? std::size_t{(w + block->first - 1) / block->first} *
                           ((h + block->second - 1) / block->second) * 16
                     : std::size_t{w} * h * 4;
    };
    const bool is_ktx = (info.flags & DDSKTX_TEXTURE_FLAG_KTX) != 0;
    std::size_t offset = static_cast<std::size_t>(info.data_offset);
    u32 w = static_cast<u32>(info.width), h = static_cast<u32>(info.height);
    std::optional<std::size_t> at;
    u32 at_w = 0, at_h = 0;
    for (int level = 0; level < std::max(info.num_mips, 1); level++) {
        const std::size_t size = level_size(w, h);
        if (is_ktx) {
            u32 image_size = 0;
            if (offset + sizeof(image_size) > file.size()) {
                break;
            }
            std::memcpy(&image_size, file.data() + offset, sizeof(image_size));
            offset += sizeof(image_size);
            if (image_size != size) {
                break;
            }
        }
        if (offset + size > file.size()) {
            break;
        }
        if (w <= kPackDecodeMaxSide && h <= kPackDecodeMaxSide) {
            at = offset;
            at_w = w;
            at_h = h;
            break;
        }
        offset += size;
        if (is_ktx) {
            offset = (offset + 3) & ~std::size_t{3};
        }
        w = std::max(w >> 1, 1U);
        h = std::max(h >> 1, 1U);
    }
    if (!at) {
        why = fmt::format("{}x{} with no level of {} or less a side", info.width, info.height,
                          kPackDecodeMaxSide);
        return false;
    }
    std::vector<u8> px(std::size_t{at_w} * at_h * 4);
    if (block) {
        astcenc_config config{};
        astcenc_error e = astcenc_config_init(ASTCENC_PRF_LDR, block->first, block->second, 1,
                                              ASTCENC_PRE_FASTEST, ASTCENC_FLG_DECOMPRESS_ONLY,
                                              &config);
        astcenc_context* context = nullptr;
        if (e == ASTCENC_SUCCESS) {
            e = astcenc_context_alloc(&config, 1, &context, nullptr);
        }
        if (e == ASTCENC_SUCCESS) {
            void* slices[1] = {px.data()};
            astcenc_image image{at_w, at_h, 1, ASTCENC_TYPE_U8, slices};
            const astcenc_swizzle swizzle{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B,
                                          ASTCENC_SWZ_A};
            e = astcenc_decompress_image(context, file.data() + *at, level_size(at_w, at_h),
                                         &image, &swizzle, 0);
        }
        if (context) {
            astcenc_context_free(context);
        }
        if (e != ASTCENC_SUCCESS) {
            why = fmt::format("{}: {}", DdsKtxFormatName(info.format), astcenc_get_error_string(e));
            return false;
        }
    } else {
        std::memcpy(px.data(), file.data() + *at, px.size());
    }
    for (u32 y = 0; y < at_h / 2; y++) {
        std::swap_ranges(px.begin() + std::size_t{y} * at_w * 4,
                         px.begin() + std::size_t{y + 1} * at_w * 4,
                         px.begin() + std::size_t{at_h - 1 - y} * at_w * 4);
    }
    rgba = std::move(px);
    width = at_w;
    height = at_h;
    return true;
}

} // namespace

std::string PackFileFormat(const std::string& path, std::string* why) {
    switch (PackFileKindOf(path)) {
    case PackFileKind::Png:
        return "PNG";
    case PackFileKind::Ktx:
    case PackFileKind::Dds: {
        std::ifstream f(path, std::ios::binary);
        const Bytes file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        ddsktx_texture_info info{};
        ddsktx_error err{};
        const char* kind = PackFileKindOf(path) == PackFileKind::Ktx ? "KTX" : "DDS";
        if (file.empty() ||
            !ddsktx_parse(&info, file.data(), static_cast<int>(file.size()), &err)) {
            if (why) {
                *why = fmt::format("{} that cannot be read{}", kind,
                                   file.empty() ? "" : std::string{": "} + err.msg);
            }
            return {};
        }
        if (!AstcBlock(info.format) && info.format != DDSKTX_FORMAT_RGBA8) {
            if (why) {
                *why = fmt::format("{} in {}: only PNG, and ASTC or RGBA8 in KTX or DDS, are read "
                                   "for the hand",
                                   kind, DdsKtxFormatName(info.format));
            }
            return {};
        }
        return fmt::format("{} {}", kind, DdsKtxFormatName(info.format));
    }
    case PackFileKind::Other:
        break;
    }
    if (why) {
        *why = "not a PNG, KTX or DDS file";
    }
    return {};
}

bool LoadPackTexture(const std::string& path, bool flip_png, std::vector<u8>& rgba, u32& width,
                     u32& height, bool keep_alpha, std::string* why) {
    std::string reason;
    const auto fail = [&](std::string r) {
        if (why) {
            *why = std::move(r);
        }
        return false;
    };
    std::ifstream f(path, std::ios::binary);
    const Bytes png((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // AzaharFrame fixc: KTX and DDS (ASTC or RGBA8), as a pack converted for the Frame holds.
    if (const PackFileKind kind = PackFileKindOf(path); kind != PackFileKind::Png) {
        if (kind == PackFileKind::Other) {
            return fail("not a PNG, KTX or DDS file");
        }
        if (!DecodePackDdsKtx(png, rgba, width, height, reason)) {
            return fail(reason);
        }
        for (std::size_t i = 3; i < rgba.size() && !keep_alpha; i += 4) {
            rgba[i] = 255; // no ambient occlusion
        }
        return true;
    }
    static Frontend::ImageInterface images; // lodepng
    u32 w = 0, h = 0;
    Bytes px;
    if (png.empty() || !images.DecodePNG(px, w, h, png) || w == 0 || h == 0 || w > 4096 ||
        h > 4096 || px.size() != std::size_t{w} * h * 4) {
        return fail(png.empty() ? "cannot be read" : "not a PNG of 4096 or less a side");
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
    for (std::size_t i = 3; i < px.size() && !keep_alpha; i += 4) {
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

namespace {

/// AzaharFrame fixc: whether a pack's file can replace a model's texture (PackFileFormat); a
/// file that cannot is logged with its format and why, and the game's texture is used.
bool PackReplacementUsable(const std::string& path, std::string_view what) {
    std::string why;
    if (!PackFileFormat(path, &why).empty()) {
        return true;
    }
    LOG_INFO(Frontend, "xr: {}: {} is {}: the game's texture is used", what,
             std::filesystem::path(path).filename().string(), why);
    return false;
}

} // namespace

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
        // AzaharFrame fixc: KTX and DDS replacements are read now: a weapon made when they
        // were not (the game's texture) is made again, once; PNG packs keep their weapons.
        if (PackFileKindOf(it->second) == PackFileKind::Ktx ||
            PackFileKindOf(it->second) == PackFileKind::Dds) {
            key += "read;";
        }
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

// ---- AzaharFrame kiumodels: how a part's texture moves ----

std::string ClassZrcName(int cls) {
    return fmt::format("a{}.zrc", cls);
}

bool ReadClassUvAnims(std::span<const u8> class_zrc, std::vector<FrameCgfx::UvAnim>& out,
                      std::string& error) {
    Bytes bcres;
    if (!FrameCgfx::DarcMember(class_zrc, "character/mdl.arc!mdl/00.bcres", bcres, error)) {
        return false;
    }
    std::vector<FrameCgfx::UvAnim> all;
    if (!FrameCgfx::ReadUvAnims(bcres, all, error)) {
        return false;
    }
    // The weapons' ("WpnPalmUV" ...), not Pit's own (his face's "ChrPit00TalkUV00").
    std::erase_if(all, [](const FrameCgfx::UvAnim& a) { return a.animation.rfind("Wpn", 0) != 0; });
    out = std::move(all);
    return true;
}

// ---- AzaharFrame kiuweap2: how Pit holds the weapon ----

std::string PoseZrcName(int cls) {
    return fmt::format("g{}.zrc", cls);
}

namespace {
const char* GameClassName(int cls) {
    static constexpr const char* kNames[kClasses] = {"Rifle",  "Snipe", "Nail", "Bow", "Palm",
                                                     "Pillar", "Launch", "Bit", "Arm"};
    return cls >= 0 && cls < kClasses ? kNames[cls] : "?";
}
} // namespace

std::string IdleAnimation(int cls) {
    return fmt::format("ChrPit00Wait{}", GameClassName(cls));
}

std::string ShotAnimation(int cls) {
    return fmt::format("ChrPit00ShotWait{}", GameClassName(cls));
}

bool ReadClassPoses(std::span<const u8> pose_zrc, int cls, ClassPoses& out, std::string& error) {
    Bytes bcres;
    if (!FrameCgfx::DarcMember(pose_zrc, "character/mdl.arc!mdl/00.bcres", bcres, error)) {
        return false;
    }
    ClassPoses p;
    if (!FrameCgfx::ReadSkeletalPose(bcres, IdleAnimation(cls), p.idle, error)) {
        return false;
    }
    std::string why;
    if (!FrameCgfx::ReadSkeletalPose(bcres, ShotAnimation(cls), p.shot, why)) {
        p.shot = p.idle; // clubs have no aiming pose
    }
    out = std::move(p);
    return true;
}

HoldInfo MakeHold(const FrameCgfx::Model& m, int cls, s32 anchor, const ClassPoses& poses) {
    HoldInfo h;
    h.known = true;
    const s32 hand = m.BoneIndex(cls == kBow ? "LHandN" : "RHandN");
    const s32 arm = m.BoneIndex("RArmN");
    const s32 ref = hand >= 0 ? hand : (cls == kBow ? -1 : arm);
    const std::size_t n = m.bones.size();
    if (ref < 0 || anchor < 0 || static_cast<std::size_t>(anchor) >= n ||
        static_cast<std::size_t>(ref) >= n) {
        return h;
    }
    h.hand = m.bones[static_cast<std::size_t>(ref)].name;
    // The fist's centre when the weapon has no grip bone: the fist marker of Pit's left hand
    // (LHandFistM, in the bind pose), mirrored for his right (Pit's right is -x); for a cannon,
    // where his right hand would be on the forearm: the left arm's hand and fist, mirrored.
    const auto translation = [](const FrameCgfx::Mat34& t) {
        return std::array<float, 3>{t[3], t[7], t[11]};
    };
    const auto bind = [&](s32 b) { return m.bones[static_cast<std::size_t>(b)].bind; };
    const s32 fist_marker = m.BoneIndex("LHandFistM");
    const s32 left_hand = m.BoneIndex("LHandN");
    const s32 left_arm = m.BoneIndex("LArmN");
    std::array<float, 3> marker{0.9f, 0.076f, 0.0f}; // the files' value when a skeleton lacks it
    if (fist_marker >= 0 && left_hand >= 0) {
        const s32 base = (h.hand == "RArmN" && left_arm >= 0) ? left_arm : left_hand;
        marker = translation(FrameCgfx::Multiply34(FrameCgfx::Inverse34(bind(base)),
                                                   bind(fist_marker)));
    } else if (h.hand == "RArmN") {
        marker[0] += 2.177f; // Pit's hand bone's length (when a skeleton lacks the left arm)
    }
    const std::string& anchor_name = m.bones[static_cast<std::size_t>(anchor)].name;
    const bool grip_bone = anchor_name == "GripN" || anchor_name == "SnipeM";
    const auto pose = [&](const FrameCgfx::SkeletalPose& sp, HoldPose& out) {
        const std::vector<FrameCgfx::Mat34> world = FrameCgfx::PoseBones(m, sp);
        out.to_hand = FrameCgfx::Multiply34(
            FrameCgfx::Inverse34(world[static_cast<std::size_t>(ref)]),
            world[static_cast<std::size_t>(anchor)]);
        if (grip_bone) {
            out.fist = translation(out.to_hand);
        } else {
            out.fist = marker;
            if (cls != kBow) {
                out.fist[0] = -out.fist[0];
            }
        }
    };
    pose(poses.idle, h.idle);
    pose(poses.shot, h.shot);
    const auto finite = [](const HoldPose& p) {
        return std::all_of(p.to_hand.begin(), p.to_hand.end(),
                           [](float f) { return std::isfinite(f) && std::abs(f) < 1e4f; }) &&
               std::all_of(p.fist.begin(), p.fist.end(),
                           [](float f) { return std::isfinite(f) && std::abs(f) < 1e4f; });
    };
    h.valid = finite(h.idle) && finite(h.shot);
    return h;
}

UvMotion GameUvMotion(const std::vector<FrameCgfx::UvAnim>& anims, const std::string& material,
                      s32 coord, float scale_u, float scale_v) {
    UvMotion m;
    float tu = 0.0f, tv = 0.0f, rot = 0.0f;
    for (const FrameCgfx::UvAnim& a : anims) {
        if (a.material != material || a.coord != coord || !(a.frames > 0.0f)) {
            continue;
        }
        m.frames = m.frames > 0.0f ? m.frames : a.frames;
        tu += a.translate_u;
        tv += a.translate_v;
        rot += a.rotate;
    }
    if (!(m.frames > 0.0f) || (tu == 0.0f && tv == 0.0f && rot == 0.0f)) {
        return {};
    }
    // The game samples at scale * (uv - translate) (SPICA's TextureTransform, every DCC mode):
    // a translate t slides the texture by -scale * t; the files' v is 1 - v.
    m.source = UvSource::Game;
    m.du = tu != 0.0f ? -scale_u * tu : 0.0f;
    m.dv = tv != 0.0f ? scale_v * tv : 0.0f;
    m.turns = rot / 6.2831853f;
    return m;
}

namespace {

/// A glow card's flow (no game animation for it): along its length, the axis of its longest
/// side in weapon space, by the UV direction that runs that way (each triangle's gradient of the
/// position along it, weighted by its area); one repeat of the texture per loop, away from the
/// anchor bone. None when the texture is clamped that way (it would smear its edge).
UvMotion FlowMotion(const PartBuilder& pb, Side primary, u32 wrap_u, u32 wrap_v) {
    auto vit = pb.vertices.find(primary);
    auto iit = pb.indices.find(primary);
    if (vit == pb.vertices.end() || iit == pb.indices.end() || iit->second.empty()) {
        vit = pb.vertices.begin();
        iit = pb.indices.begin();
        if (vit == pb.vertices.end() || iit == pb.indices.end()) {
            return {};
        }
    }
    const std::vector<float>& v = vit->second;
    const std::vector<u32>& idx = iit->second;
    Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
    for (std::size_t i = 0; i + 11 < v.size(); i += 12) {
        for (std::size_t k = 0; k < 3; k++) {
            lo[k] = std::min(lo[k], v[i + k]);
            hi[k] = std::max(hi[k], v[i + k]);
        }
    }
    std::size_t L = 0;
    for (std::size_t k = 1; k < 3; k++) {
        if (hi[k] - lo[k] > hi[L] - lo[L]) {
            L = k;
        }
    }
    if (!(hi[L] - lo[L] > 0.0f)) {
        return {};
    }
    double along_u = 0.0, along_v = 0.0, sign_u = 0.0, sign_v = 0.0;
    for (std::size_t t = 0; t + 2 < idx.size(); t += 3) {
        const float* p0 = &v[std::size_t{idx[t]} * 12];
        const float* p1 = &v[std::size_t{idx[t + 1]} * 12];
        const float* p2 = &v[std::size_t{idx[t + 2]} * 12];
        const double du1 = p1[10] - p0[10], dv1 = p1[11] - p0[11];
        const double du2 = p2[10] - p0[10], dv2 = p2[11] - p0[11];
        const double ds1 = p1[L] - p0[L], ds2 = p2[L] - p0[L];
        const double det = du1 * dv2 - du2 * dv1;
        if (std::abs(det) < 1e-12) {
            continue;
        }
        const double a = (ds1 * dv2 - ds2 * dv1) / det; // d(position along L) / du
        const double b = (du1 * ds2 - du2 * ds1) / det; // ... / dv
        const double n2 = a * a + b * b;
        if (!(n2 > 1e-20) || !std::isfinite(n2)) {
            continue;
        }
        const Vec3 e1{p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        const Vec3 e2{p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        const Vec3 x{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                     e1[0] * e2[1] - e1[1] * e2[0]};
        const double area = std::sqrt(double{x[0]} * x[0] + double{x[1]} * x[1] + double{x[2]} * x[2]);
        along_u += area * a * a / n2;
        along_v += area * b * b / n2;
        sign_u += area * a / std::sqrt(n2);
        sign_v += area * b / std::sqrt(n2);
    }
    if (along_u == 0.0 && along_v == 0.0) {
        return {};
    }
    const bool on_u = along_u >= along_v;
    const u32 wrap = on_u ? wrap_u : wrap_v;
    const float period = wrap == 2 ? 1.0f : wrap == 3 ? 2.0f : 0.0f;
    if (period == 0.0f) {
        return {};
    }
    // The texture at uv + offset: a growing offset moves its picture toward smaller uv, so
    // toward smaller positions along L where the position grows with uv.
    const double grows = on_u ? sign_u : sign_v;
    const float centre = 0.5f * (lo[L] + hi[L]);
    const float outward = centre < 0.0f ? -1.0f : 1.0f;
    const float slide = -period * (grows < 0.0 ? -1.0f : 1.0f) * outward;
    UvMotion m;
    m.source = UvSource::Flow;
    m.frames = kFlowFrames;
    (on_u ? m.du : m.dv) = slide;
    return m;
}

} // namespace

bool ConvertWeapon(int id, std::span<const u8> zrc, Converted& out, std::string& error,
                   const TexturePack* pack, const std::vector<FrameCgfx::UvAnim>* class_anims,
                   const ClassPoses* poses) {
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
    // AzaharFrame kiuweap2: how Pit holds it (his class's poses; none: the class's own fit).
    if (poses) {
        w.hold = MakeHold(m, cls, anchor_of(primary), *poses);
    } else {
        w.hold.known = true;
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
            const std::size_t ci = std::min<std::size_t>(static_cast<std::size_t>(part.mapper),
                                                         mat.coords.size() - 1);
            coord = mat.coords[ci];
            part.coord = static_cast<s32>(ci);
            part.scale_u = coord.scale_u;
            part.scale_v = coord.scale_v;
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
                    // AzaharFrame fixc: PNG, or ASTC or RGBA8 in KTX or DDS.
                    if (PackReplacementUsable(it->second, "weapon models")) {
                        part.replaced = it->second;
                        part.replaced_flip = pack->flip_png;
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
        // AzaharFrame kiumodels: how its texture moves: the game's own animation of this
        // material's coordinator, else, for a glow card, a flow along its length.
        if (class_anims) {
            part.uv = GameUvMotion(*class_anims, pb.material, pb.coord, pb.scale_u, pb.scale_v);
        }
        if (part.uv.source == UvSource::None && part.glow) {
            part.uv = FlowMotion(pb, primary, wrap_u, wrap_v);
        }
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
    // AzaharFrame kiuflash: its flashes from its own effects (another weapon's when it has
    // none: the caller's, GameConverter::Ensure).
    ConvertFlash(id, zrc, id, false, c, pack);
    ConvertBullets(id, zrc, c, pack); // AzaharFrame kiuflash: its projectiles
    error.clear();
    out = std::move(c);
    return true;
}

FlashModels PickFlashModels(int id, const std::vector<std::string>& names) {
    FlashModels out;
    if (id < 0 || id >= kWeapons) {
        return out;
    }
    const std::string prefix =
        fmt::format("Ptc{}{:02d}", kEffectClass[ClassOf(id)], id % kPerClass);
    // The first name of the prefix whose rest starts with one of `starts` (in that order), a
    // plain one (nothing after the start but a letter: "ShotFlash", "ShotFlashA") before one
    // with more ("ShotFlash_Line", "ShotFlash2").
    const auto find = [&](std::initializer_list<const char*> starts) {
        for (const char* start : starts) {
            const std::size_t len = std::strlen(start);
            std::string first;
            for (const std::string& n : names) {
                if (n.size() < prefix.size() + len || n.rfind(prefix, 0) != 0 ||
                    n.compare(prefix.size(), len, start) != 0) {
                    continue;
                }
                const std::string rest = n.substr(prefix.size() + len);
                if (rest.empty() ||
                    (rest.size() == 1 && std::isupper(static_cast<unsigned char>(rest[0])))) {
                    return n;
                }
                if (first.empty()) {
                    first = n;
                }
            }
            if (!first.empty()) {
                return first;
            }
        }
        return std::string{};
    };
    out.rapid = find({"RapidFlash", "RapidShot", "Bullet01_Flash", "ShotA", "Muzzle"});
    out.charged = find({"ShotFlash", "Bullet02_Flash", "ShotB"});
    if (out.rapid.empty()) {
        out.rapid = out.charged;
    }
    if (out.charged.empty()) {
        out.charged = out.rapid;
    }
    return out;
}

namespace {

/// The first of `suffixes` that names a model `prefix + suffix` exactly ("" for none).
std::string ExactName(const std::string& prefix, const std::vector<std::string>& names,
                      std::initializer_list<const char*> suffixes) {
    for (const char* suffix : suffixes) {
        const std::string want = prefix + suffix;
        if (std::find(names.begin(), names.end(), want) != names.end()) {
            return want;
        }
    }
    return {};
}

FlashModels PickBullets(int id, const std::vector<std::string>& names, const char* lead) {
    FlashModels out;
    if (id < 0 || id >= kWeapons) {
        return out;
    }
    const std::string prefix =
        fmt::format("{}{}{:02d}", lead, kEffectClass[ClassOf(id)], id % kPerClass);
    out.rapid = ExactName(prefix, names, {"Bullet1", "Bullet01", "BulletA01"});
    out.charged = ExactName(prefix, names, {"Bullet3", "Bullet03"});
    if (out.rapid.empty()) {
        out.rapid = out.charged;
    }
    if (out.charged.empty()) {
        out.charged = out.rapid;
    }
    return out;
}

using C4 = std::array<float, 4>;

/// A texel (0 to 1) at the texture coordinates (wrapped), white with no texture.
C4 Texel(const FrameCgfx::Texture* t, float u, float v) {
    if (!t || t->width == 0 || t->height == 0) {
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }
    const auto wrap = [](float x, u32 n) {
        const float f = x - std::floor(x);
        return std::min(static_cast<u32>(f * static_cast<float>(n)), n - 1);
    };
    const u8* p = t->rgba.data() + (std::size_t{wrap(v, t->height)} * t->width + wrap(u, t->width)) * 4;
    return {p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f, p[3] / 255.0f};
}

/// The material's combiner stages run on one texel (PICA's TEV, as the effects study reads it:
/// the vertex colour as given, lighting as full, the secondary colour as none).
C4 RunStages(const FrameCgfx::Material& m, const std::array<C4, 3>& tex, const C4& vertex) {
    C4 prev{0.0f, 0.0f, 0.0f, 0.0f};
    if (m.stages.empty()) {
        return tex[0];
    }
    const auto constant = [&](u32 sel) -> C4 {
        const auto& c = m.constants[sel < 6 ? sel : 0];
        return {c[0], c[1], c[2], c[3]};
    };
    for (const auto& st : m.stages) {
        const C4 k = constant(st.constant);
        const auto source = [&](u32 src) -> C4 {
            switch (src) {
            case 0:
                return vertex;
            case 1:
                return {1.0f, 1.0f, 1.0f, 1.0f};
            case 2:
                return {0.0f, 0.0f, 0.0f, 0.0f};
            case 3:
                return tex[0];
            case 4:
                return tex[1];
            case 5:
            case 6:
                return tex[2];
            case 0xE:
                return k;
            default:
                return prev;
            }
        };
        std::array<std::array<float, 3>, 3> rgb{};
        std::array<float, 3> alpha{};
        for (u32 i = 0; i < 3; i++) {
            const C4 cs = source((st.source >> (4 * i)) & 0xF);
            const C4 as = source((st.source >> (16 + 4 * i)) & 0xF);
            const u32 oc = (st.operand >> (4 * i)) & 0xF;
            for (int ch = 0; ch < 3; ch++) {
                float x;
                switch (oc) {
                case 1: x = 1.0f - cs[ch]; break;
                case 2: x = cs[3]; break;
                case 3: x = 1.0f - cs[3]; break;
                case 4: x = cs[0]; break;
                case 5: x = 1.0f - cs[0]; break;
                case 8: x = cs[1]; break;
                case 9: x = 1.0f - cs[1]; break;
                case 12: x = cs[2]; break;
                case 13: x = 1.0f - cs[2]; break;
                default: x = cs[ch]; break;
                }
                rgb[i][ch] = x;
            }
            const u32 oa = (st.operand >> (12 + 4 * i)) & 7;
            const float a[8] = {as[3], 1.0f - as[3], as[0], 1.0f - as[0],
                                as[1], 1.0f - as[1], as[2], 1.0f - as[2]};
            alpha[i] = a[oa];
        }
        const auto combine = [](u32 op, float a, float b, float c) {
            switch (op) {
            case 0: return a;
            case 2: return a + b;
            case 3: return a + b - 0.5f;
            case 4: return a * c + b * (1.0f - c);
            case 5: return a - b;
            case 8: return a * b + c;
            case 9: return (a + b) * c;
            default: return a * b;
            }
        };
        const float cs = static_cast<float>(1u << (st.scale & 3));
        const float as = static_cast<float>(1u << ((st.scale >> 16) & 3));
        C4 out;
        for (int ch = 0; ch < 3; ch++) {
            out[ch] = std::clamp(combine(st.combine & 0xF, rgb[0][ch], rgb[1][ch], rgb[2][ch]) * cs,
                                 0.0f, 1.0f);
        }
        out[3] = std::clamp(combine((st.combine >> 16) & 0xF, alpha[0], alpha[1], alpha[2]) * as,
                            0.0f, 1.0f);
        prev = out;
    }
    return prev;
}

/// A bullet material's look as one of the flash colour modes and a colour, fitted (least squares)
/// to its combiner run over its first texture's texels, the light it adds (times its alpha when
/// it blends with it).
std::array<float, kBulletValues> FitBulletLook(const FrameCgfx::Material& m,
                                               const std::array<const FrameCgfx::Texture*, 3>& tex,
                                               const C4& mean_vertex) {
    std::array<float, kBulletValues> v{};
    v[kBulletAdded] = m.blend ? 1.0f : 0.0f;
    // The vertex colour times the rest (the first stage multiplies by it, or there is none): left
    // to the draw, per vertex; else its mean goes through the stages here.
    bool multiplies = m.stages.empty();
    if (!m.stages.empty()) {
        const auto& st = m.stages[0];
        multiplies = (st.combine & 0xF) == 1 &&
                     ((st.source & 0xF) == 0 || ((st.source >> 4) & 0xF) == 0) &&
                     (st.operand & 0xFF) == 0;
    }
    v[kBulletVertexColour] = multiplies ? 1.0f : 0.0f;
    const C4 vertex = multiplies ? C4{1.0f, 1.0f, 1.0f, 1.0f} : mean_vertex;
    if (!m.blend || !tex[0]) {
        v[kBulletMode] = kFlashTexture;
        v[kBulletRed] = v[kBulletGreen] = v[kBulletBlue] = 1.0f;
        return v;
    }
    const FrameCgfx::Texture& t0 = *tex[0];
    double tt[3]{}, tl[3]{}, ll[3]{}, rr = 0.0, rl[3]{};
    for (u32 y = 0; y < t0.height; y++) {
        for (u32 x = 0; x < t0.width; x++) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(t0.width);
            const float w = (static_cast<float>(y) + 0.5f) / static_cast<float>(t0.height);
            const std::array<C4, 3> texels{Texel(tex[0], u, w), Texel(tex[1], u, w),
                                           Texel(tex[2], u, w)};
            const C4 o = RunStages(m, texels, vertex);
            const float a = m.blend_src == 6 ? o[3] : m.blend_src == 7 ? 1.0f - o[3] : 1.0f;
            const C4& t = texels[0];
            for (int c = 0; c < 3; c++) {
                const double light = o[c] * a;
                const double basis = t[c] * t[3];
                tt[c] += basis * basis;
                tl[c] += basis * light;
                ll[c] += light * light;
                rl[c] += t[0] * light;
            }
            rr += t[0] * t[0];
        }
    }
    // The error of each mode: sum(L^2) - (sum(B L))^2 / sum(B^2), per channel.
    double err_tex = 0.0, err_lum = 0.0;
    std::array<float, 3> k_tex{}, k_lum{};
    for (int c = 0; c < 3; c++) {
        k_tex[c] = tt[c] > 1e-9 ? static_cast<float>(tl[c] / tt[c]) : 0.0f;
        k_lum[c] = rr > 1e-9 ? static_cast<float>(rl[c] / rr) : 0.0f;
        err_tex += ll[c] - (tt[c] > 1e-9 ? tl[c] * tl[c] / tt[c] : 0.0);
        err_lum += ll[c] - (rr > 1e-9 ? rl[c] * rl[c] / rr : 0.0);
    }
    // The texture's own colour unless its red channel alone is clearly the better fit.
    const bool lum = err_lum < err_tex * 0.8 - 1e-9;
    const auto& k = lum ? k_lum : k_tex;
    v[kBulletMode] = lum ? kFlashLuminance : kFlashTexture;
    for (int c = 0; c < 3; c++) {
        v[static_cast<std::size_t>(kBulletRed + c)] = std::clamp(k[c], 0.0f, 1.0f);
    }
    return v;
}

constexpr u32 kFlagBullet = 1u << 18; // frame_xr/vis/model_file.h kModelFlagBullet

struct BulletBuilder {
    const FrameCgfx::Material* material = nullptr;
    int mapper = -1;
    std::vector<float> verts; // 12 floats each, model space until normalised
    std::vector<u32> idx;
};

} // namespace

FlashModels PickBulletModels(int id, const std::vector<std::string>& names) {
    return PickBullets(id, names, "Eff");
}

FlashModels PickBulletSprites(int id, const std::vector<std::string>& names) {
    return PickBullets(id, names, "Ptc");
}

bool ConvertBullets(int id, std::span<const u8> zrc, Converted& out, const TexturePack* pack) {
    BulletInfo made;
    made.known = true;
    Bytes effects;
    std::string why;
    if (!FrameCgfx::DarcMember(zrc, "character/effect.bcres", effects, why)) {
        out.info.bullets = made;
        return false;
    }
    // The names: the mesh models and the particle models.
    std::vector<FrameCgfx::ParticleModel> particles;
    FrameCgfx::ReadParticleModels(effects, particles, why);
    // The mesh bullets PickBulletModels can name: only these are read.
    std::vector<std::string> names;
    for (const char* suffix : {"Bullet1", "Bullet01", "BulletA01", "Bullet3", "Bullet03"}) {
        names.push_back(fmt::format("Eff{}{:02d}{}", kEffectClass[ClassOf(id)], id % kPerClass,
                                    suffix));
    }
    FrameCgfx::File file;
    if (!FrameCgfx::ReadCgfxModels(effects, names, file, why)) {
        out.info.bullets = made;
        return false;
    }
    std::vector<std::string> mesh_names;
    for (const auto& m : file.models) {
        mesh_names.push_back(m.name);
    }
    std::vector<std::string> sprite_names;
    for (const auto& m : particles) {
        sprite_names.push_back(m.name);
    }
    const FlashModels meshes = PickBulletModels(id, mesh_names);
    const FlashModels sprites = PickBulletSprites(id, sprite_names);
    const auto model = [&](const std::string& name) -> const FrameCgfx::Model* {
        for (const auto& m : file.models) {
            if (!name.empty() && m.name == name) {
                u32 triangles = 0;
                for (const auto& mesh : m.meshes) {
                    if (mesh.shape >= 0 && static_cast<std::size_t>(mesh.shape) < m.shapes.size()) {
                        for (const auto& sub : m.shapes[static_cast<std::size_t>(mesh.shape)].submeshes) {
                            triangles += static_cast<u32>(sub.indices.size() / 3);
                        }
                    }
                }
                return triangles > 0 && triangles <= kBulletTrianglesMax ? &m : nullptr;
            }
        }
        return nullptr;
    };
    const FrameCgfx::Model* rapid = model(meshes.rapid);
    const FrameCgfx::Model* charged = model(meshes.charged);
    std::vector<std::pair<std::string, std::vector<u8>>> files;
    const auto add_part = [&](FlashPart part, const FrameCgfx::Texture* tex, Bytes bytes) {
        part.file = fmt::format("w{:03d}-b{}.model", id, made.parts.size());
        if (tex) {
            part.texture = tex->name;
            part.hash = VideoCore::CustomTexManager::NewHash(tex->raw);
            if (pack && pack->on) {
                if (const auto it = pack->files.find(part.hash); it != pack->files.end()) {
                    // AzaharFrame fixc: PNG, or ASTC or RGBA8 in KTX or DDS.
                    if (PackReplacementUsable(it->second, "projectiles")) {
                        part.replaced = it->second;
                        part.replaced_flip = pack->flip_png;
                    }
                }
            }
        }
        files.emplace_back(part.file, std::move(bytes));
        made.parts.push_back(std::move(part));
    };

    // The mesh bullets: each model once, its components the kinds it is.
    for (int pass = 0; pass < 2; pass++) {
        const FrameCgfx::Model* m = pass == 0 ? rapid : charged;
        if (!m || (pass == 1 && charged == rapid)) {
            continue; // none, or the rapid one again (made once, for both)
        }
        std::vector<const char*> kinds;
        if (m == rapid) {
            kinds.push_back("rapid");
        }
        if (m == charged) {
            kinds.push_back("charged");
        }
        std::map<s32, BulletBuilder> by_material;
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (const FrameCgfx::Mesh& mesh : m->meshes) {
            if (!m->MeshShown(mesh) || mesh.shape < 0 ||
                static_cast<std::size_t>(mesh.shape) >= m->shapes.size()) {
                continue;
            }
            BulletBuilder& b = by_material[mesh.material];
            b.material = &m->materials[static_cast<std::size_t>(mesh.material)];
            if (b.mapper < 0) {
                for (int i = 0; i < 3; i++) {
                    const auto& mp = b.material->mappers[static_cast<std::size_t>(i)];
                    if (!mp.texture.empty() && file.FindTexture(mp.texture)) {
                        b.mapper = i;
                        break;
                    }
                }
            }
            FrameCgfx::TexCoord coord;
            if (b.mapper >= 0 && !b.material->coords.empty()) {
                coord = b.material->coords[std::min<std::size_t>(static_cast<std::size_t>(b.mapper),
                                                                 b.material->coords.size() - 1)];
            }
            const FrameCgfx::Shape& shape = m->shapes[static_cast<std::size_t>(mesh.shape)];
            for (const FrameCgfx::SubMesh& sub : shape.submeshes) {
                for (const u32 vi : sub.indices) {
                    const FrameCgfx::Vertex& v = shape.vertices[vi];
                    const auto p = FrameCgfx::BindPosition(*m, sub, v);
                    for (int k = 0; k < 3; k++) {
                        lo[k] = std::min(lo[k], p[static_cast<std::size_t>(k)]);
                        hi[k] = std::max(hi[k], p[static_cast<std::size_t>(k)]);
                    }
                    const auto& uv = coord.source == 1 ? v.uv1 : v.uv0;
                    // Unlit: the normal unused; the vertex colour in the tangent's place.
                    b.verts.insert(b.verts.end(),
                                   {p[0], p[1], p[2], 0.0f, 0.0f, 1.0f, v.colour[0], v.colour[1],
                                    v.colour[2], v.colour[3],
                                    uv[0] * coord.scale_u + coord.translate_u,
                                    1.0f - (uv[1] * coord.scale_v + coord.translate_v)});
                    b.idx.push_back(static_cast<u32>(b.idx.size()));
                }
            }
        }
        if (by_material.empty() || !(hi[0] >= lo[0])) {
            continue;
        }
        // Centred, its largest side 1; its head to +z: a trail reaches farther from the bullet's
        // mass (its vertices' mean) than its head does.
        const float centre[3] = {(lo[0] + hi[0]) / 2.0f, (lo[1] + hi[1]) / 2.0f,
                                 (lo[2] + hi[2]) / 2.0f};
        const float size = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1e-6f});
        double mean_z = 0.0;
        std::size_t count = 0;
        for (const auto& [mat, b] : by_material) {
            for (std::size_t i = 0; i + 11 < b.verts.size(); i += 12) {
                mean_z += b.verts[i + 2];
                count++;
            }
        }
        mean_z /= static_cast<double>(std::max<std::size_t>(count, 1));
        const bool flip = hi[2] - mean_z > (mean_z - lo[2]) * 1.1;
        for (auto& [mat, b] : by_material) {
            for (std::size_t i = 0; i + 11 < b.verts.size(); i += 12) {
                for (int k = 0; k < 3; k++) {
                    b.verts[i + static_cast<std::size_t>(k)] =
                        (b.verts[i + static_cast<std::size_t>(k)] - centre[k]) / size;
                }
                if (flip) { // half a turn about y
                    b.verts[i] = -b.verts[i];
                    b.verts[i + 2] = -b.verts[i + 2];
                }
            }
            const FrameCgfx::Texture* tex =
                b.mapper >= 0 ? file.FindTexture(b.material->mappers[static_cast<std::size_t>(b.mapper)].texture)
                              : nullptr;
            std::array<const FrameCgfx::Texture*, 3> texs{};
            for (int i = 0; i < 3; i++) {
                const auto& mp = b.material->mappers[static_cast<std::size_t>(i)];
                texs[static_cast<std::size_t>(i)] = mp.texture.empty() ? nullptr : file.FindTexture(mp.texture);
            }
            if (b.mapper > 0) {
                std::swap(texs[0], texs[static_cast<std::size_t>(b.mapper)]);
            }
            C4 mean{0.0f, 0.0f, 0.0f, 0.0f};
            for (std::size_t i = 0; i + 11 < b.verts.size(); i += 12) {
                for (int k = 0; k < 4; k++) {
                    mean[k] += b.verts[i + 6 + static_cast<std::size_t>(k)];
                }
            }
            for (float& x : mean) {
                x /= std::max<float>(1.0f, static_cast<float>(b.verts.size() / 12));
            }
            const auto look = FitBulletLook(*b.material, texs, mean);
            const u32 wrap_u = b.mapper >= 0 ? b.material->mappers[static_cast<std::size_t>(b.mapper)].wrap_u : 2;
            const u32 wrap_v = b.mapper >= 0 ? b.material->mappers[static_cast<std::size_t>(b.mapper)].wrap_v : 2;
            // The file: the components the kinds, the look in their floats.
            const u32 tw = tex ? tex->width : 8, th = tex ? tex->height : 8;
            Bytes bytes;
            bytes.insert(bytes.end(), {'M', 'X', 'F', 'M'});
            Put32(bytes, kModelVersion);
            Put32(bytes, static_cast<u32>(b.verts.size() / 12));
            Put32(bytes, static_cast<u32>(b.idx.size()));
            Put32(bytes, static_cast<u32>(kinds.size()));
            Put32(bytes, tw);
            Put32(bytes, th);
            Put32(bytes, kFlagWeaponSpace | (wrap_u & 7) << 8 | (wrap_v & 7) << 12 |
                             kFlagFlatSurface | kFlagBullet);
            for (const char* kind : kinds) {
                Bytes rec(kComponentBytes, 0);
                std::memcpy(rec.data(), kind, std::strlen(kind));
                std::memcpy(rec.data() + kNameBytes, b.material->name.data(),
                            std::min<std::size_t>(b.material->name.size(), kNameBytes - 1));
                const auto put_at = [&](std::size_t at, u32 val) {
                    for (int k = 0; k < 4; k++) {
                        rec[at + k] = static_cast<u8>(val >> (8 * k));
                    }
                };
                put_at(2 * kNameBytes, 0);
                put_at(2 * kNameBytes + 4, static_cast<u32>(b.idx.size()));
                for (int k = 0; k < kBulletValues; k++) {
                    u32 bits;
                    std::memcpy(&bits, &look[static_cast<std::size_t>(k)], 4);
                    put_at(2 * kNameBytes + 16 + std::size_t(k) * 4, bits);
                }
                bytes.insert(bytes.end(), rec.begin(), rec.end());
            }
            for (const float f : b.verts) {
                PutF(bytes, f);
            }
            for (const u32 i : b.idx) {
                Put32(bytes, i);
            }
            for (std::size_t i = 0; i < std::size_t{tw} * th; i++) {
                if (tex) {
                    bytes.insert(bytes.end(), {tex->rgba[i * 4], tex->rgba[i * 4 + 1],
                                               tex->rgba[i * 4 + 2], tex->rgba[i * 4 + 3]});
                } else {
                    bytes.insert(bytes.end(), {255, 255, 255, 255});
                }
            }
            bytes.insert(bytes.end(), {128, 128, 255, kRoughness});
            FlashPart part;
            part.cards = static_cast<u32>(kinds.size());
            add_part(std::move(part), tex, std::move(bytes));
        }
        (m == rapid ? made.rapid : made.charged) = m->name;
        if (m == rapid && m == charged) {
            made.charged = m->name;
        }
    }
    // A kind with no mesh: its sprites, face cards as the flashes' (one file per texture).
    std::vector<Card> cards;
    for (const FlashKind kind : {FlashKind::Rapid, FlashKind::Charged}) {
        const bool is_rapid = kind == FlashKind::Rapid;
        if ((is_rapid && rapid) || (!is_rapid && charged)) {
            continue;
        }
        const std::string& name = is_rapid ? sprites.rapid : sprites.charged;
        const auto pm = std::find_if(particles.begin(), particles.end(),
                                     [&](const auto& p) { return !name.empty() && p.name == name; });
        if (pm == particles.end()) {
            continue;
        }
        std::vector<Card> more = FlashCards(*pm, kind, file.textures, false);
        for (Card& c : more) {
            c.role = CardRole::Face; // a bullet is seen from wherever it flies
            c.values[kFlashOffset] = 0.0f;
        }
        if (!more.empty()) {
            (is_rapid ? made.rapid : made.charged) = pm->name;
            (is_rapid ? made.rapid_cards : made.charged_cards) = true;
            cards.insert(cards.end(), std::make_move_iterator(more.begin()),
                         std::make_move_iterator(more.end()));
        }
    }
    std::vector<bool> done(cards.size(), false);
    for (std::size_t i = 0; i < cards.size(); i++) {
        if (done[i]) {
            continue;
        }
        std::vector<const Card*> group;
        for (std::size_t k = i; k < cards.size(); k++) {
            if (!done[k] && cards[k].texture == cards[i].texture &&
                cards[k].wrap_u == cards[i].wrap_u && cards[k].wrap_v == cards[i].wrap_v) {
                group.push_back(&cards[k]);
                done[k] = true;
            }
        }
        const FrameCgfx::Texture* tex = file.FindTexture(cards[i].texture);
        if (!tex) {
            continue;
        }
        FlashPart part;
        part.cards = static_cast<u32>(group.size());
        add_part(std::move(part), tex, WriteFlashModel(*tex, group));
    }
    std::vector<u64> hashes;
    for (const FlashPart& p : made.parts) {
        hashes.push_back(p.hash);
    }
    made.pack_key = PackKey(pack, hashes);
    out.files.insert(out.files.end(), std::make_move_iterator(files.begin()),
                     std::make_move_iterator(files.end()));
    out.info.bullets = std::move(made);
    return !out.info.bullets.parts.empty();
}

bool ConvertFlash(int id, std::span<const u8> source_zrc, int source, bool neutral,
                  Converted& out, const TexturePack* pack) {
    FlashInfo& f = out.info.flash;
    f.known = true;
    Bytes effects;
    std::string why;
    std::vector<FrameCgfx::ParticleModel> models;
    if (!FrameCgfx::DarcMember(source_zrc, "character/effect.bcres", effects, why) ||
        !FrameCgfx::ReadParticleModels(effects, models, why)) {
        return false;
    }
    std::vector<std::string> names;
    for (const auto& m : models) {
        names.push_back(m.name);
    }
    const FlashModels picked = PickFlashModels(source, names);
    const auto model = [&](const std::string& name) -> const FrameCgfx::ParticleModel* {
        for (const auto& m : models) {
            if (!name.empty() && m.name == name) {
                return &m;
            }
        }
        return nullptr;
    };
    const FrameCgfx::ParticleModel* rapid = model(picked.rapid);
    const FrameCgfx::ParticleModel* charged = model(picked.charged);
    std::vector<std::string> wanted;
    for (const auto* m : {rapid, charged}) {
        for (const auto& s : m ? m->sets : std::vector<FrameCgfx::ParticleSet>{}) {
            if (std::find(wanted.begin(), wanted.end(), s.texture) == wanted.end()) {
                wanted.push_back(s.texture);
            }
        }
    }
    std::vector<FrameCgfx::Texture> textures;
    if (wanted.empty() || !FrameCgfx::ReadTextures(effects, wanted, textures, why)) {
        return false;
    }
    std::vector<Card> cards;
    if (rapid) {
        cards = FlashCards(*rapid, FlashKind::Rapid, textures, neutral);
    }
    if (charged) {
        auto more = FlashCards(*charged, FlashKind::Charged, textures, neutral);
        cards.insert(cards.end(), std::make_move_iterator(more.begin()),
                     std::make_move_iterator(more.end()));
    }
    if (cards.empty()) {
        return false;
    }
    // One file per texture (and wrap), its cards the components.
    FlashInfo made;
    made.known = true;
    made.rapid = rapid ? rapid->name : std::string{};
    made.charged = charged ? charged->name : std::string{};
    made.source = source;
    std::vector<std::pair<std::string, std::vector<u8>>> files;
    std::vector<bool> done(cards.size(), false);
    for (std::size_t i = 0; i < cards.size(); i++) {
        if (done[i]) {
            continue;
        }
        std::vector<const Card*> group;
        for (std::size_t k = i; k < cards.size(); k++) {
            if (!done[k] && cards[k].texture == cards[i].texture &&
                cards[k].wrap_u == cards[i].wrap_u && cards[k].wrap_v == cards[i].wrap_v) {
                group.push_back(&cards[k]);
                done[k] = true;
            }
        }
        const auto tex = std::find_if(textures.begin(), textures.end(), [&](const auto& t) {
            return t.name == cards[i].texture;
        });
        FlashPart part;
        part.file = fmt::format("w{:03d}-f{}.model", id, made.parts.size());
        part.texture = tex->name;
        part.hash = VideoCore::CustomTexManager::NewHash(tex->raw);
        part.cards = static_cast<u32>(group.size());
        if (pack && pack->on) {
            if (const auto it = pack->files.find(part.hash); it != pack->files.end()) {
                // AzaharFrame fixc: PNG, or ASTC or RGBA8 in KTX or DDS.
                if (PackReplacementUsable(it->second, "muzzle flash")) {
                    part.replaced = it->second;
                    part.replaced_flip = pack->flip_png;
                }
            }
        }
        files.emplace_back(part.file, WriteFlashModel(*tex, group));
        made.parts.push_back(std::move(part));
    }
    std::vector<u64> hashes;
    for (const FlashPart& p : made.parts) {
        hashes.push_back(p.hash);
    }
    made.pack_key = PackKey(pack, hashes);
    // Another weapon's flash replaces what this one had (nothing: it found none).
    std::erase_if(out.files, [](const auto& file) {
        return file.first.find("-f") != std::string::npos;
    });
    out.files.insert(out.files.end(), std::make_move_iterator(files.begin()),
                     std::make_move_iterator(files.end()));
    f = std::move(made);
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
//   flashes <rapid model|-> <charged model|-> <source weapon|-1> <flash pack key>
//   flash <file> <texture> <texture hash> <cards> <flip> [pack PNG]
//   bullets <rapid model|-> <charged model|-> <rapid as sprites 0|1> <charged as sprites 0|1>
//           <bullet pack key>
//   bullet <file> <texture> <texture hash> <components> <flip> [pack PNG]
//   uvanim <file> <source: 1 game, 2 flow> <frames> <du> <dv> <turns>   (kiumodels, version 4)
//   hold <hand bone|-> <idle: fist x y z, anchor to hand 12 numbers (row major 3x4)>
//        <aiming: the same>                                            (kiuweap2, HoldInfo)
// A part line belongs to the weapon line before it; the PNG's path is the rest of the line. A
// uvanim line belongs to that weapon's part of the same file.
// AzaharFrame kiuflash: the flashes and flash lines too, and the bullets and bullet lines (builds
// before them skip them; a weapon line without a flashes or a bullets line is made again).
// AzaharFrame kiuweap2: the hold line the same way (no version change: builds before it keep
// their files and skip the line; this one makes a weapon without it again, once).

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
            if (p.uv.source != UvSource::None) {
                out += fmt::format("uvanim {} {} {} {} {} {}\n", p.file,
                                   static_cast<int>(p.uv.source), p.uv.frames, p.uv.du, p.uv.dv,
                                   p.uv.turns);
            }
        }
        // AzaharFrame kiuweap2: how Pit holds it (a weapon without the line is made again).
        if (w.hold.known) {
            const HoldInfo& h = w.hold;
            if (!h.valid) {
                out += "hold -\n";
            } else {
                out += "hold " + h.hand;
                for (const HoldPose* hp : {&h.idle, &h.shot}) {
                    for (const float f : hp->fist) {
                        out += fmt::format(" {}", f);
                    }
                    for (const float f : hp->to_hand) {
                        out += fmt::format(" {}", f);
                    }
                }
                out += "\n";
            }
        }
        // AzaharFrame kiuflash: for a weapon made with its flashes (one made by a build before
        // them, kept as it was, keeps none: it is made again when next asked for).
        const FlashInfo& f = w.flash;
        if (!f.known) {
            continue;
        }
        out += fmt::format("flashes {} {} {} {}\n", f.rapid.empty() ? "-" : f.rapid,
                           f.charged.empty() ? "-" : f.charged, f.source, Hex(f.pack_key));
        for (const FlashPart& p : f.parts) {
            out += fmt::format("flash {} {} {} {} {}", p.file, p.texture.empty() ? "-" : p.texture,
                               Hex(p.hash), p.cards, p.replaced_flip ? 1 : 0);
            if (!p.replaced.empty()) {
                out += " " + p.replaced;
            }
            out += "\n";
        }
        // AzaharFrame kiuflash: the projectiles, the same way.
        const BulletInfo& b = w.bullets;
        if (!b.known) {
            continue;
        }
        out += fmt::format("bullets {} {} {} {} {}\n", b.rapid.empty() ? "-" : b.rapid,
                           b.charged.empty() ? "-" : b.charged, b.rapid_cards ? 1 : 0,
                           b.charged_cards ? 1 : 0, Hex(b.pack_key));
        for (const FlashPart& p : b.parts) {
            out += fmt::format("bullet {} {} {} {} {}", p.file,
                               p.texture.empty() ? "-" : p.texture, Hex(p.hash), p.cards,
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
        } else if (key == "hold" && current) { // AzaharFrame kiuweap2
            HoldInfo h;
            h.known = true;
            in >> h.hand;
            if (in && h.hand != "-") {
                bool ok = h.hand == "RHandN" || h.hand == "LHandN" || h.hand == "RArmN";
                for (HoldPose* hp : {&h.idle, &h.shot}) {
                    for (float& f : hp->fist) {
                        ok = ok && static_cast<bool>(in >> f) && std::isfinite(f) &&
                             std::abs(f) < 1e4f;
                    }
                    for (float& f : hp->to_hand) {
                        ok = ok && static_cast<bool>(in >> f) && std::isfinite(f) &&
                             std::abs(f) < 1e4f;
                    }
                }
                if (!ok) {
                    error = "a bad hold line";
                    return false;
                }
                h.valid = true;
            } else {
                h.hand.clear();
            }
            current->hold = std::move(h);
        } else if (key == "flashes" && current) { // AzaharFrame kiuflash
            FlashInfo& f = current->flash;
            in >> f.rapid >> f.charged >> f.source;
            if (!in || !hex(f.pack_key) || f.source < -1 || f.source >= kWeapons) {
                error = "a bad flashes line";
                return false;
            }
            for (std::string* t : {&f.rapid, &f.charged}) {
                if (*t == "-") {
                    t->clear();
                }
            }
            f.known = true;
        } else if (key == "flash" && current && current->flash.known) {
            FlashPart p;
            int flip = 1;
            in >> p.file >> p.texture;
            const bool ok = static_cast<bool>(in) && hex(p.hash);
            in >> p.cards >> flip;
            if (!ok || !in || p.file.find('/') != std::string::npos ||
                p.file.find("..") != std::string::npos || p.cards == 0 ||
                p.cards > 2u * kFlashCardsMax) {
                error = "a bad flash line";
                return false;
            }
            std::getline(in >> std::ws, p.replaced);
            p.replaced_flip = flip != 0;
            current->flash.parts.push_back(std::move(p));
        } else if (key == "bullets" && current) { // AzaharFrame kiuflash
            BulletInfo& b = current->bullets;
            int rc = 0, cc = 0;
            in >> b.rapid >> b.charged >> rc >> cc;
            if (!in || !hex(b.pack_key)) {
                error = "a bad bullets line";
                return false;
            }
            for (std::string* t : {&b.rapid, &b.charged}) {
                if (*t == "-") {
                    t->clear();
                }
            }
            b.rapid_cards = rc != 0;
            b.charged_cards = cc != 0;
            b.known = true;
        } else if (key == "bullet" && current && current->bullets.known) {
            FlashPart p;
            int flip = 1;
            in >> p.file >> p.texture;
            const bool ok = static_cast<bool>(in) && hex(p.hash);
            in >> p.cards >> flip;
            if (!ok || !in || p.file.find('/') != std::string::npos ||
                p.file.find("..") != std::string::npos || p.cards == 0 ||
                p.cards > 2u * kFlashCardsMax) {
                error = "a bad bullet line";
                return false;
            }
            std::getline(in >> std::ws, p.replaced);
            p.replaced_flip = flip != 0;
            if (p.texture == "-") {
                p.texture.clear();
            }
            current->bullets.parts.push_back(std::move(p));
        } else if (key == "uvanim" && current) {
            // AzaharFrame kiumodels: a part's texture motion; a bad one is dropped (still).
            std::string file;
            int source = 0;
            UvMotion m;
            in >> file >> source >> m.frames >> m.du >> m.dv >> m.turns;
            const auto ok = [](float f, float limit) { return std::isfinite(f) && std::abs(f) <= limit; };
            if (in && (source == 1 || source == 2) && m.frames > 0.0f && ok(m.frames, 1e5f) &&
                ok(m.du, 64.0f) && ok(m.dv, 64.0f) && ok(m.turns, 1e3f)) {
                m.source = static_cast<UvSource>(source);
                for (Part& part : current->parts) {
                    if (part.file == file) {
                        part.uv = m;
                    }
                }
            }
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
                        [&](const Part& p) { return FileUtil::Exists(dir + p.file); }) &&
            FlashCurrent(old->flash) && BulletsCurrent(old->bullets) && old->hold.known) {
            res.kept++;
            return true;
        }
        Converted c;
        std::string why;
        if (!ConvertWeapon(id, zrc, c, why, pack, ClassAnims(ClassOf(id)),
                           Poses(ClassOf(id)))) {
            LOG_WARNING(Frontend, "xr: weapon models: {} ({}): {}", ZrcName(id), id, why);
            res.failed++;
            return false;
        }
        // AzaharFrame kiuflash: no flash of its own: the first of its class's that has one, its
        // colours made neutral.
        for (int k = 0; c.info.flash.parts.empty() && k < kPerClass; k++) {
            const int other = ClassOf(id) * kPerClass + k;
            const auto* of = index.Find(manifest.region + "/player/" + ZrcName(other));
            Bytes other_zrc;
            if (other != id && of && index.Read(*of, other_zrc, u64{8} << 20)) {
                ConvertFlash(id, other_zrc, other, true, c, pack);
            }
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
            for (const FlashPart& p : old->flash.parts) { // AzaharFrame kiuflash
                if (std::none_of(c.info.flash.parts.begin(), c.info.flash.parts.end(),
                                 [&](const FlashPart& q) { return q.file == p.file; })) {
                    FileUtil::Delete(dir + p.file);
                }
            }
            for (const FlashPart& p : old->bullets.parts) {
                if (std::none_of(c.info.bullets.parts.begin(), c.info.bullets.parts.end(),
                                 [&](const FlashPart& q) { return q.file == p.file; })) {
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

    /// AzaharFrame kiuflash: a weapon's flashes as made: made by a build that makes them, the
    /// texture pack's files for them unchanged, the files there.
    bool FlashCurrent(const FlashInfo& f) const {
        if (!f.known) {
            return false;
        }
        std::vector<u64> hashes;
        for (const FlashPart& p : f.parts) {
            hashes.push_back(p.hash);
            if (!FileUtil::Exists(dir + p.file)) {
                return false;
            }
        }
        return f.pack_key == PackKey(pack, hashes);
    }

    /// AzaharFrame kiuflash: a weapon's bullets as made, the same way.
    bool BulletsCurrent(const BulletInfo& b) const {
        if (!b.known) {
            return false;
        }
        std::vector<u64> hashes;
        for (const FlashPart& p : b.parts) {
            hashes.push_back(p.hash);
            if (!FileUtil::Exists(dir + p.file)) {
                return false;
            }
        }
        return b.pack_key == PackKey(pack, hashes);
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
    /// AzaharFrame kiumodels: the class's Pit file's weapon texture animations, read once per
    /// class (nullptr when the file is missing or bad: its glow cards flow instead).
    const std::vector<FrameCgfx::UvAnim>* ClassAnims(int cls) {
        auto it = class_anims.find(cls);
        if (it == class_anims.end()) {
            std::optional<std::vector<FrameCgfx::UvAnim>> anims;
            const std::string name = ClassZrcName(cls);
            const auto* f = index.Find(manifest.region + "/player/" + name);
            Bytes zrc;
            std::string why = "not in the RomFS";
            std::vector<FrameCgfx::UvAnim> read;
            if (f && index.Read(*f, zrc, u64{8} << 20) && ReadClassUvAnims(zrc, read, why)) {
                anims = std::move(read);
                LOG_INFO(Frontend, "xr: weapon models: {}: {} texture animations of the {}'s",
                         name, anims->size(), ClassName(cls));
            } else {
                LOG_WARNING(Frontend, "xr: weapon models: {}: {} (its glow cards flow)", name,
                            f ? why : std::string{"not in the RomFS"});
            }
            it = class_anims.emplace(cls, std::move(anims)).first;
        }
        return it->second ? &*it->second : nullptr;
    }

    /// AzaharFrame kiuweap2: the class's Pit poses (how he holds his weapon), read once per
    /// class (nullptr when the file is missing or bad: the class's own fit).
    const ClassPoses* Poses(int cls) {
        auto it = class_poses.find(cls);
        if (it == class_poses.end()) {
            std::optional<ClassPoses> poses;
            const std::string name = PoseZrcName(cls);
            const auto* f = index.Find(manifest.region + "/player/" + name);
            Bytes zrc;
            std::string why = "not in the RomFS";
            ClassPoses read;
            if (f && index.Read(*f, zrc, u64{8} << 20) && ReadClassPoses(zrc, cls, read, why)) {
                poses = std::move(read);
                LOG_INFO(Frontend, "xr: weapon models: {}: Pit's hold of the {} ({}, {})", name,
                         ClassName(cls), poses->idle.animation, poses->shot.animation);
            } else {
                LOG_WARNING(Frontend, "xr: weapon models: {}: {} (the {}'s own fit)", name,
                            f ? why : std::string{"not in the RomFS"}, ClassName(cls));
            }
            it = class_poses.emplace(cls, std::move(poses)).first;
        }
        return it->second ? &*it->second : nullptr;
    }

    std::map<int, std::optional<std::vector<FrameCgfx::UvAnim>>> class_anims;
    std::map<int, std::optional<ClassPoses>> class_poses;
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
