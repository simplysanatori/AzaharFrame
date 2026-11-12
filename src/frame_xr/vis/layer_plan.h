// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame vis
// The order of a frame's composition layers, with never more than one projection layer.
//
// SteamVR on the Frame mishandles two projection layers in one frame: the second one shakes
// from frame to frame (Moonlight XR's room judder, adc2c77 there, confirmed on the headset).
// "Background: Passthrough" needs a projection layer with alpha for the room to show behind,
// and the controller models are a projection layer of their own (controller_models.h), so with
// both the models jittered in the hands while the screens and panels (quads) stayed still.
// The models' layer is cleared to nothing round the models and has
// XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT, just as the passthrough square, so when
// the models show it is the one projection layer and the room shows through it; the square is
// only sent on frames without the models.

#pragma once

#include <cstddef>
#include <vector>

namespace FrameXr::Vis {

/// Fills `out` with the frame's layers: the passthrough square first (only when there is no
/// models' layer), then the `quad_count` quads (`quad_at(i)`), with the models' layer before
/// quad `models_at` (after them all when `models_at` is `quad_count`). A null layer is left out.
template <typename Layer, typename QuadAt>
void AssembleLayers(std::vector<Layer>& out, Layer passthrough, Layer models,
                    std::size_t models_at, std::size_t quad_count, QuadAt&& quad_at) {
    out.clear();
    if (passthrough && !models) {
        out.push_back(passthrough);
    }
    for (std::size_t i = 0; i < quad_count; i++) {
        if (i == models_at && models) {
            out.push_back(models);
        }
        out.push_back(quad_at(i));
    }
    if (models && models_at >= quad_count) {
        out.push_back(models);
    }
}

/// Owner's call: in passthrough the room shows the real controllers, so the models are not
/// drawn (the beams and dots stay). "Show Controllers" keeps its saved value and counts again
/// as soon as the Background is Black. `passthrough_shown`: Background is Passthrough and the
/// runtime offers it.
constexpr bool ControllerModelsWanted(bool show_controllers, bool passthrough_shown) {
    return show_controllers && !passthrough_shown;
}

} // namespace FrameXr::Vis
