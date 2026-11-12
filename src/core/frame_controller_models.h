// Copyright Azahar for Frame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cpphelpers: converts SteamVR's Steam Frame controller render models into the files
// Azahar for Frame draws in VR (the C++ port of steam-frame/tools/frame-controller-models.py, which
// was Moonlight XR Native's converter, GPLv3, by the same author). Valve's models are only read from
// the player's own SteamVR install, never redistributed.
//
//   --frame-controller-models OUTDIR [SteamVR rendermodels dir]
//
// writes OUTDIR/frame_controller_left.model and frame_controller_right.model (format: see
// frame_xr/vis/model_file.cpp). The vertices are in the controller's OpenXR aim space. Textures are
// scaled to 1024 x 1024 (AZAHARFRAME_MODEL_TEXTURE=0 keeps SteamVR's size). No Qt: the PNGs are
// decoded by the caller's `ImageLoader`.

#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace FrameControllerModels {

constexpr const char* kRenderModels = "/opt/steamvr/drivers/frame_controller/resources/rendermodels";

/// A decoded image: tight RGB rows from the top.
struct Image {
    int width = 0, height = 0;
    std::vector<u8> rgb;
};
/// Decodes a PNG file (any channels; the result is RGB). false when it cannot.
using ImageLoader = std::function<bool(const std::string& path, Image* out)>;

/// `src` scaled to w x h by area averaging.
Image Scale(const Image& src, int w, int h);

struct Model {
    std::string bytes; ///< the whole .model file
    int triangles = 0, vertices = 0, components = 0, tex_w = 0, tex_h = 0;
};
/// One hand ("left"/"right"); false with `error` on a missing file or a bad model.
bool Convert(const std::string& hand, const std::string& rendermodels, int texture,
             const ImageLoader& load, Model* out, std::string* error);

/// Both hands into `out_dir` (both or neither). Prints the per-hand summary line.
int Run(const std::vector<std::string>& args, const ImageLoader& load);

} // namespace FrameControllerModels
