// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuopacity: see see_through.h.

#include <cstdlib>

#include "frame_xr/vis/see_through.h"

namespace FrameXr::Vis {

GlowOut GlowLight(const std::array<float, 3>& texture, float gain, float fade) {
    GlowOut o;
    for (std::size_t i = 0; i < 3; i++) {
        float c = std::min(texture[i] * 2.0f, 1.0f);
        if (gain >= 0.0f) {
            c = std::min(c * std::clamp(gain, 0.0f, 2.0f), 1.0f);
        }
        o.light[i] = c * std::clamp(fade, 0.0f, 1.0f);
    }
    o.cover = CoverAlpha(o.light[0], o.light[1], o.light[2]);
    return o;
}

OpacityTune ParseOpacityTune(const std::optional<std::string>& value) {
    OpacityTune t;
    if (!value || value->empty()) {
        return t;
    }
    const auto part = [](const char* s, char** end) {
        const long v = std::strtol(s, end, 10);
        return *end != s && v >= 0 && v <= 100 ? static_cast<int>(v) : -1;
    };
    const char* s = value->c_str();
    char* end = nullptr;
    t.weapon = part(s, &end);
    if (*end == ',') {
        const char* p = end + 1;
        char* pend = nullptr;
        t.projectile = part(p, &pend);
    }
    return t;
}

} // namespace FrameXr::Vis
