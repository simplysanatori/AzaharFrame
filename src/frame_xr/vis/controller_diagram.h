// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctrldiag
// Line drawings of the Steam Frame controllers for the controls help, made on the device from
// the same model files the in-hand controllers use (model_file.h: SteamVR's own models,
// converted at install time and never shipped). A small software rasteriser draws a model from
// a fixed angle (orthographic, supersampled) and keeps, for each pixel, the part it shows, its
// depth and its facing; the outlines are where those change (silhouette, part edges, creases),
// in the style of CitraVR's controller pages. Each labelled part's visible pixels give the point
// its leader line starts from. No OpenXR, Vulkan or Qt here (unit tested).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "frame_xr/vis/model_file.h"

namespace FrameXr::Vis {

/// The parts of a Frame controller the help points at. A hand has the ones its model has (the
/// left one has the D-pad and View, the right one the face buttons and Menu).
enum class CtrlPart : int {
    Stick,
    Dpad,
    View,
    Menu,
    System,
    A,
    B,
    X,
    Y,
    Bumper,
    Trigger,
    Grip,
    Count,
};
constexpr std::size_t kCtrlParts = static_cast<std::size_t>(CtrlPart::Count);

/// The part a model component is (by its input path), if the help uses it.
std::optional<CtrlPart> PartOfComponent(const ModelComponent& component);

/// The angle a hand is drawn from: the model rolled by `roll` about its forward axis (positive
/// lifts its right side), then turned by `pitch` about its right axis (degrees). Pitch 90 looks
/// straight down at the face buttons with the front of the controller at the top; more than 90
/// comes round to the front, so the bumper and the trigger show above the face.
struct DiagramAngle {
    float roll = 0.0f;
    float pitch = 90.0f;
};

struct DiagramPoint {
    float x = 0.0f, y = 0.0f;
};

/// One hand's drawing: premultiplied RGBA8, rows top down, and where each part shows.
struct DiagramImage {
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;
    /// Image pixels of the middle of each part's visible pixels, if any are visible.
    std::array<std::optional<DiagramPoint>, kCtrlParts> anchor{};
    /// The visible bounds of each part (x0, y0, x1, y1), for the pointer's end.
    std::array<std::array<float, 4>, kCtrlParts> bounds{};
};

struct DiagramStyle {
    uint32_t line = 0xFFE6E6EAu;      ///< outlines, 0xAARRGGBB
    uint32_t body = 0xFF26262Au;      ///< the controller's fill
    uint32_t part = 0xFF323238u;      ///< the fill of the parts the help points at
    float line_width = 2.2f;          ///< outline width, output pixels
    float crease_degrees = 38.0f;     ///< a fold sharper than this is drawn
};

/// The scale that fits every model in `models` drawn at `angle` into width x height (pixels per
/// metre), so the two hands come out the same size.
float DiagramScale(const std::vector<const ModelFile*>& models, const DiagramAngle& angle,
                   int width, int height);

/// Draws `model` at `angle`, `scale` pixels a metre, centred in width x height.
DiagramImage DrawDiagram(const ModelFile& model, const DiagramAngle& angle, float scale, int width,
                         int height, const DiagramStyle& style = {});

/// The rows [y0, y1) of `image`, the last `fade` of them fading out (a drawing cut short).
DiagramImage CropDiagram(const DiagramImage& image, int y0, int y1, int fade);

} // namespace FrameXr::Vis
