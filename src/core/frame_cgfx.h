// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: readers for the Nintendo files a game's own RomFS holds that the weapon
// models (frame_weapon_models.h) are made from. Read only, every offset checked against the
// buffer (a bad or hostile file is an error, never a crash). No game data is in this code.
//
//   RomFS     the level 3 image a RomFSReader gives (Azahar's loader: NCCH, installed titles,
//             LayeredFS mods), its directory and file tables
//   darc      Nintendo's archive (nested; an LZ11 member is unpacked on the way: common/lz11.h)
//   PSLB      a binary JSON (strings, int32, float, bool, objects, arrays)
//   CGFX      NW4C graphics (.bcres): the models (skeleton, meshes, shapes, vertex and index
//             buffers, materials' texture coordinates and mappers) and the textures, decoded
//             with video_core's PICA texture decoder (ETC1, ETC1A4, RGB565, ...)
//
// The CGFX and darc layouts follow SPICA (gdkchan, public domain: the Unlicense); see
// THIRD_PARTY_NOTICES.md. No code of it is used.

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace FileSys {
class RomFSReader;
}

namespace FrameCgfx {

using Bytes = std::vector<u8>;

// ---- RomFS (level 3) ----

class RomFSIndex {
public:
    struct File {
        std::string path; ///< from the root, '/' separated, no leading '/'
        u64 offset = 0;   ///< in the RomFSReader
        u64 size = 0;
    };
    /// Reads the tables (at most 32 MB of them). False with the reason.
    bool Open(std::shared_ptr<FileSys::RomFSReader> reader, std::string& error);
    const std::vector<File>& Files() const {
        return files;
    }
    const File* Find(std::string_view path) const;
    /// A file's bytes (at most `max_size`). False when it cannot be read whole.
    bool Read(const File& file, Bytes& out, u64 max_size = u64{64} << 20) const;

private:
    std::shared_ptr<FileSys::RomFSReader> reader;
    std::vector<File> files;
};

/// The same tables from a whole level 3 image in memory (tests).
bool ListRomFS(std::span<const u8> image, std::vector<RomFSIndex::File>& out, std::string& error);

// ---- darc ----

struct DarcEntry {
    std::string path; ///< '/' separated, "." and empty parts dropped
    u32 offset = 0;
    u32 size = 0;
};
bool ReadDarc(std::span<const u8> data, std::vector<DarcEntry>& out, std::string& error);

/// One member through nested archives: "character/fmdl.arc!fmdl/00.bcres" (each '!' opens the
/// member before it as a darc; an LZ11 stream, the outer file too, is unpacked first).
bool DarcMember(std::span<const u8> data, std::string_view path, Bytes& out, std::string& error);

// ---- PSLB ----

struct PslbValue {
    enum class Kind : u8 { None, String, Int, Float, Bool, Object, Array };
    Kind kind = Kind::None;
    std::string string;
    s32 integer = 0;
    float real = 0.0f;
    bool boolean = false;
    std::vector<std::string> keys; ///< an object's keys, its values in `array` (same order)
    std::vector<PslbValue> array;  ///< an array's values, or an object's

    const PslbValue* Get(std::string_view key) const;
    /// An Int or a Float as a float.
    std::optional<float> Number() const;
};
bool ReadPslb(std::span<const u8> data, PslbValue& out, std::string& error);

// ---- CGFX ----

/// Row major 3x4 (an affine transform: x' = m[0..2].p + m[3], ...).
using Mat34 = std::array<float, 12>;
Mat34 Identity34();
Mat34 Multiply34(const Mat34& a, const Mat34& b);
/// The inverse of an affine 3x4 (identity when singular).
Mat34 Inverse34(const Mat34& m);
std::array<float, 3> Point(const Mat34& m, const std::array<float, 3>& p);
std::array<float, 3> Direction(const Mat34& m, const std::array<float, 3>& d);

struct Bone {
    std::string name;
    s32 index = -1;
    s32 parent = -1;
    Mat34 bind{};    ///< model space in the bind pose (the inverse of the stored inverse)
};

struct TexCoord {
    s32 source = 0;    ///< which UV set
    u32 mapping = 0;   ///< 0 UV, 1 cube environment, 2 sphere environment, 3 projection
    float scale_u = 1.0f, scale_v = 1.0f, rotation = 0.0f, translate_u = 0.0f, translate_v = 0.0f;
};

struct Mapper {
    std::string texture; ///< the referenced texture's name ("" for none)
    u32 wrap_u = 2, wrap_v = 2; ///< PICA wrap modes (0 clamp to edge, 1 border, 2 repeat, 3 mirror)
};

struct Material {
    std::string name;
    std::vector<TexCoord> coords;  ///< the used ones (1 to 3)
    std::array<Mapper, 3> mappers; ///< empty texture: no mapper
    /// The face culling the game draws it with (PICA's GPUREG_FACECULLING_CONFIG: 0 both sides,
    /// 1 or 2 one side only).
    u32 cull = 0;
    // AzaharFrame kiuflash: what an effect material (a bullet's) is drawn with, as far as the
    // bullets need: its six combiner constants (RGBA 0 to 1), its blend (enabled; the source and
    // destination colour factors, PICA's: 0 zero, 1 one, 6 source alpha, 7 one minus it, ...) and
    // its combiner stages (PICA's TEV: sources, operands, combine, constant, scale).
    std::array<std::array<float, 4>, 6> constants{};
    bool blend = false;
    u32 blend_src = 1, blend_dst = 0;
    struct Stage {
        u32 source = 0, operand = 0, combine = 0, constant = 0, scale = 0;
    };
    std::vector<Stage> stages;
};

struct Vertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{0.0f, 0.0f, 1.0f};
    std::array<float, 2> uv0{}, uv1{};
    std::array<float, 4> colour{1.0f, 1.0f, 1.0f, 1.0f};
    std::array<u8, 4> bones{};
    bool has_bones = false;
};

struct SubMesh {
    std::vector<s32> bones; ///< the skeleton's bone per local index
    u32 skinning = 0;       ///< 0 none, 1 rigid, 2 smooth
    std::vector<u32> indices; ///< triangles (strips and fans made lists)
};

struct Shape {
    std::vector<Vertex> vertices; ///< with the shape's position offset added
    std::vector<SubMesh> submeshes;
};

struct Mesh {
    std::string name;
    s32 shape = -1;
    s32 material = -1;
    bool visible = true;
    s32 node = -1; ///< index into the node visibility list, or -1
};

struct Model {
    std::string name;
    std::vector<Bone> bones;
    std::vector<Material> materials;
    std::vector<Shape> shapes;
    std::vector<Mesh> meshes;
    std::vector<std::pair<std::string, bool>> node_visibility;

    s32 BoneIndex(std::string_view name) const;
    /// Whether a mesh is shown by default (its own flag and its node's).
    bool MeshShown(const Mesh& mesh) const;
};

struct Texture {
    std::string name;
    u32 width = 0, height = 0;
    u32 format = 0;  ///< PICA texture format (12 ETC1, 13 ETC1A4, ...)
    Bytes rgba;      ///< width * height * 4, rows in memory order (the first row is v = 1)
    Bytes raw;       ///< the first level's bytes as the GPU reads them (tiled; a pack's hash)
};

struct File {
    std::vector<Model> models;
    std::vector<Texture> textures;
    const Texture* FindTexture(std::string_view name) const;
};

/// Reads the models and decodes the textures. False with the reason.
bool ReadCgfx(std::span<const u8> data, File& out, std::string& error);

/// AzaharFrame kiumodels: one texture coordinator's animation in a material animation (CANM,
/// the DATA section's 11th dictionary), by the path it targets:
/// `Materials["<material>"].TextureCoordinators[<coord>].Translate` or `.Rotate`. Kept as what
/// changes over one loop (the last key's value minus the first's, per component; a constant or
/// absent component changes by 0): the game's weapon animations are linear loops (two keys,
/// start to end). `linear` is false when a curve has more keys or is not linear (the change is
/// then an approximation).
struct UvAnim {
    std::string animation; ///< the CANM's name ("WpnPalmUV")
    std::string material;
    s32 coord = 0;
    float frames = 0.0f;              ///< the loop's length (the CANM's frame count)
    bool loops = false;               ///< loop mode 1
    float translate_u = 0.0f, translate_v = 0.0f; ///< the change of Translate over the loop
    float rotate = 0.0f;              ///< the change of Rotate (radians)
    bool linear = true;
};

/// The material animations' texture coordinator tracks of a CGFX file (no models or textures
/// are read: cheap for a big file). Elements that target anything else are skipped. False with
/// the reason for a bad file.
bool ReadUvAnims(std::span<const u8> data, std::vector<UvAnim>& out, std::string& error);

/// A model space position of one vertex of a submesh (the bind pose: rigid and unskinned
/// vertices are in their bone's space, smooth ones in model space already).
std::array<float, 3> BindPosition(const Model& model, const SubMesh& sub, const Vertex& v,
                                  std::array<float, 3>* normal = nullptr);

/// AzaharFrame kiuflash: the named models of a CGFX file that may hold others it cannot read
/// (an effect file: particle models, bullets) and all its textures: each model read on its own
/// (one that fails is left out). False only when the file itself cannot be read.
bool ReadCgfxModels(std::span<const u8> data, const std::vector<std::string>& names, File& out,
                    std::string& error);

// ---- Particle models (AzaharFrame kiuflash) ----
//
// A weapon's effect file (`character/effect.bcres`) holds its shot and flash effects as particle
// models (CMDL with type bit 0x100): no meshes or materials of their own, a list of particle sets
// (emitters' sprites), and the user data the game draws them with, one value per set: "Tex" the
// texture's name, "Col" a colour (0x??RRGGBB; the top byte a mask's white core), "Sam" the
// sampler (wrap across in bits 12-15, down in bits 8-11, PICA's modes), "Type" (low byte the
// colour mode, byte 2 the UV mirror: 1 both ways, 2 across). Read as far as the flashes need: no
// emitter or updater is simulated.

struct ParticleSet {
    std::string name;    ///< the set's node name
    std::string texture; ///< "Tex"
    u32 colour = 0;      ///< "Col"
    u32 sampler = 0;     ///< "Sam"
    u32 type = 0;        ///< "Type"
    /// The sprite's size in game units (across, down): the scale its updater sets (an updater
    /// of type 0x00200000 on attribute 1), else its scale attribute (1) when constant; 0 when
    /// neither is found.
    std::array<float, 2> size{};
    /// Its texture coordinates' scale (attribute 6, constant): 0.5 across for a sheet of two
    /// cells side by side, ...; {0, 0} when not found.
    std::array<float, 2> uv_scale{};
};

struct ParticleModel {
    std::string name;
    std::vector<ParticleSet> sets;
};

/// The particle models of a CGFX file (other models skipped). False with the reason.
bool ReadParticleModels(std::span<const u8> data, std::vector<ParticleModel>& out,
                        std::string& error);
/// The textures named (others skipped, so a large file is not decoded whole). False with the
/// reason; a name the file does not hold is no error (it is just not in `out`).
bool ReadTextures(std::span<const u8> data, const std::vector<std::string>& names,
                  std::vector<Texture>& out, std::string& error);

} // namespace FrameCgfx
