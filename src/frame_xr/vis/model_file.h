// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// The Steam Frame controller models: the files steam-frame/tools/frame-controller-models.py makes
// at install time from SteamVR's own render models (never shipped: Valve's), and the moving parts
// posed from the controls. The format is Moonlight XR Native's MXFM (same author; the reader and
// the posing follow its rooms/room_gpu.c), version 2, with flags 1: the vertices are in the
// controller's OpenXR aim space (the converter's --space aim), so a model is drawn at the aim
// pose this port already locates. No OpenXR or Vulkan here (unit tested).
//
//   header      'MXFM', version, vertexCount, indexCount, componentCount, texWidth, texHeight, flags
//   components  208 bytes each: name[32], input path[32], firstIndex, indexCount, motion type
//               (0 none, 1 translate, 2 rotate, 3 joystick, 4 d-pad), 0, 32 floats of motion
//   vertices    12 floats each: position 3, normal 3, tangent 4, uv 2
//   indices     uint32 triangles
//   textures    texWidth x texHeight RGBA8 twice: colour (sRGB rgb, AO a), surface (normal rgb,
//               OpenGL green, roughness a)

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace FrameXr::Vis {

/// What drives a moving part (Moonlight XR's MLXR_IN_*).
enum ModelInput : int {
    kInTrigger,
    kInGrip,
    kInStickX,
    kInStickY,
    kInStickClick,
    kInA,
    kInB,
    kInX,
    kInY,
    kInMenu,
    kInView,
    kInSystem,
    kInBumper,
    kInDpadUp,
    kInDpadDown,
    kInDpadLeft,
    kInDpadRight,
    kModelInputs,
};
using ModelInputs = std::array<float, kModelInputs>;

constexpr uint32_t kModelVertexFloats = 12;
constexpr uint32_t kModelFlagAimSpace = 1;

struct ModelComponent {
    std::string name;
    std::string path;
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    uint32_t motion_type = 0;
    int input = -1; ///< ModelInput, or -1
    std::array<float, 32> motion{};
};

struct ModelFile {
    uint32_t version = 0;
    uint32_t flags = 0;
    std::vector<float> vertices; ///< kModelVertexFloats per vertex
    std::vector<uint32_t> indices;
    std::vector<ModelComponent> components;
    uint32_t tex_width = 0, tex_height = 0;
    std::vector<uint8_t> colour;  ///< tex_width * tex_height * 4
    std::vector<uint8_t> surface; ///< the same size
};

/// Reads and checks one hand's file. False with the reason in `error`.
bool LoadModelFile(const std::string& path, ModelFile& out, std::string& error);

/// One draw of a model: an index range and its transform (column major 4x4, in model space).
struct ModelPart {
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    std::array<float, 16> transform{};
};

/// The parts posed by the controls (SteamVR's motion rules), the ones at rest merged into as few
/// draws as their index ranges allow.
std::vector<ModelPart> PoseModel(const ModelFile& model, const ModelInputs& in);

} // namespace FrameXr::Vis
