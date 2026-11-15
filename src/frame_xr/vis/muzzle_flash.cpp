// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuflash: see muzzle_flash.h.

#include "frame_xr/vis/muzzle_flash.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "frame_xr/input/gyro_aim.h"

namespace FrameXr::Vis {

namespace {

using V3 = std::array<float, 3>;

V3 Sub(const V3& a, const V3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
V3 Add(const V3& a, const V3& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
V3 Mul(const V3& a, float s) {
    return {a[0] * s, a[1] * s, a[2] * s};
}
float Dot(const V3& a, const V3& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
V3 Cross(const V3& a, const V3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
/// Unit length, or `fallback` when too short.
V3 Unit(const V3& v, const V3& fallback) {
    const float l = std::sqrt(Dot(v, v));
    return l > 1e-6f ? Mul(v, 1.0f / l) : fallback;
}
/// Any unit vector at right angles to unit `v`.
V3 Perpendicular(const V3& v) {
    const V3 a = std::abs(v[0]) < 0.9f ? V3{1, 0, 0} : V3{0, 1, 0};
    return Unit(Cross(v, a), {0, 0, 1});
}

} // namespace

void MuzzleFlash::Update(int64_t now_ns, uint64_t total, bool readable, bool active) {
    if (!readable || !active || !has_) {
        has_ = readable && active; // shots before the first read, or while inactive: none
        last_ = total;
        if (!active || !readable) {
            start_ns_ = -1;
            last_shot_ns_ = now_ns;
        }
        return;
    }
    if (total > last_) {
        charged_ = ever_ && now_ns - last_shot_ns_ >= Input::Recoil::kChargePauseNs;
        start_ns_ = now_ns;
        last_shot_ns_ = now_ns;
        shot_++;
        ever_ = true;
    }
    last_ = total;
}

FlashFrame MuzzleFlash::Current(int64_t now_ns) const {
    FlashFrame f;
    if (start_ns_ < 0) {
        return f;
    }
    const int64_t duration =
        duration_override_ns > 0 ? duration_override_ns : charged_ ? kChargedNs : kRapidNs;
    const int64_t age = now_ns - start_ns_;
    if (age < 0 || age >= duration) {
        return f;
    }
    f.shown = true;
    f.charged = charged_;
    f.age = static_cast<float>(age) / static_cast<float>(duration);
    f.shot = shot_;
    f.duration_s = static_cast<float>(duration) * 1e-9f;
    return f;
}

float FlashIntensity(float age) {
    if (!(age >= 0.0f) || age >= 1.0f) {
        return 0.0f;
    }
    if (age <= 0.15f) {
        return 1.0f;
    }
    const float t = (age - 0.15f) / 0.85f;
    return (1.0f - t) * (1.0f - t);
}

float FlashGrowth(float age, float grow) {
    const float t = std::clamp(age, 0.0f, 1.0f);
    const float g = std::clamp(grow, 1.0f, 2.0f);
    return 1.0f + (g - 1.0f) * (1.0f - (1.0f - t) * (1.0f - t));
}

float FlashBurst(float age, float grow) {
    const float t = std::clamp(std::isfinite(age) ? age : 0.0f, 0.0f, 1.0f);
    const float out = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); // ease out (cubic)
    return (kBurstFrom + (kBurstTo - kBurstFrom) * out) * FlashGrowth(t, grow);
}

float FlashSizeFactor(int percent) {
    return static_cast<float>(std::clamp(percent, kFlashSizeMin, kFlashSizeMax)) / 100.0f;
}

FlashFlare FlashFlareAt(float age, bool charged) {
    FlashFlare f;
    if (!(age >= 0.0f) || age >= 1.0f) {
        return f;
    }
    f.metres = (charged ? kFlareChargedM : kFlareRapidM) * FlashBurst(age, 1.0f);
    const float t = std::clamp(age / kFlareFade, 0.0f, 1.0f);
    f.strength = (1.0f - t) * (1.0f - t);
    f.core = f.strength * f.strength; // the white core goes first: the halo's colour lingers
    return f;
}

FlashTune ParseFlashTune(const std::optional<std::string>& value) {
    FlashTune t;
    if (!value || value->empty()) {
        return t;
    }
    const char* s = value->c_str();
    char* end = nullptr;
    const float size = std::strtof(s, &end);
    if (end != s && size >= 0.05f && size <= 10.0f) {
        t.size = size;
    }
    if (end && *end == ',') {
        const char* d = end + 1;
        char* dend = nullptr;
        const long ms = std::strtol(d, &dend, 10);
        if (dend != d && ms >= 10 && ms <= 2000) {
            t.duration_ms = static_cast<int>(ms);
        }
    }
    return t;
}

std::array<float, 16> FlashCardTransform(bool fan, const V3& origin, const V3& forward,
                                         const V3& eye, const V3& up, float metres, float offset,
                                         float spin) {
    const V3 f = Unit(forward, {0, 0, -1});
    V3 x, y, z, at;
    if (fan) {
        // Along the ray, turned about it toward the eye.
        at = origin;
        y = f;
        x = Unit(Cross(y, Sub(eye, origin)), Perpendicular(y));
        z = Cross(x, y);
    } else {
        at = Add(origin, Mul(f, offset * metres));
        z = Unit(Sub(eye, at), Mul(f, -1.0f));
        x = Unit(Cross(up, z), Perpendicular(z));
        y = Cross(z, x);
        const float c = std::cos(spin), s = std::sin(spin);
        const V3 xs = Add(Mul(x, c), Mul(y, s));
        const V3 ys = Sub(Mul(y, c), Mul(x, s));
        x = xs;
        y = ys;
    }
    std::array<float, 16> m{};
    for (int r = 0; r < 3; r++) {
        m[static_cast<std::size_t>(0 + r)] = x[static_cast<std::size_t>(r)] * metres;
        m[static_cast<std::size_t>(4 + r)] = y[static_cast<std::size_t>(r)] * metres;
        m[static_cast<std::size_t>(8 + r)] = z[static_cast<std::size_t>(r)] * metres;
        m[static_cast<std::size_t>(12 + r)] = at[static_cast<std::size_t>(r)];
    }
    m[15] = 1.0f;
    return m;
}

float FlashCardMetres(bool fan, float card_units, float fit_scale, float tune_size) {
    const float metres = fit_scale * kSizeScale * tune_size;
    const float max_m = (fan ? kFanMaxM : kFaceMaxM) * tune_size;
    const float min_m = kCardMinM * tune_size;
    if (card_units > 0.0f && card_units * metres > max_m) {
        return max_m / card_units;
    }
    if (card_units > 0.0f && card_units * metres < min_m) {
        return min_m / card_units;
    }
    return metres;
}

FlashCardExtent FlashCardSize(const std::vector<float>& vertices,
                              const std::vector<uint32_t>& indices, uint32_t first,
                              uint32_t count) {
    FlashCardExtent e;
    float y0 = 1e30f, y1 = -1e30f;
    for (uint64_t k = first; k < uint64_t{first} + count && k < indices.size(); k++) {
        const std::size_t at = std::size_t{indices[k]} * 12;
        if (at + 1 < vertices.size()) {
            e.half_width = std::max(e.half_width, std::abs(vertices[at]));
            y0 = std::min(y0, vertices[at + 1]);
            y1 = std::max(y1, vertices[at + 1]);
        }
    }
    if (y1 >= y0) {
        e.half_length = (y1 - y0) / 2.0f;
        e.middle = (y0 + y1) / 2.0f;
    }
    return e;
}

float FlashPush(const std::vector<float>& vertices, const std::array<float, 16>& fit,
                const V3& muzzle) {
    float push = 0.0f;
    for (std::size_t at = 0; at + 2 < vertices.size(); at += 12) {
        const float x = vertices[at], y = vertices[at + 1], z = vertices[at + 2];
        const V3 p{fit[0] * x + fit[4] * y + fit[8] * z + fit[12],
                   fit[1] * x + fit[5] * y + fit[9] * z + fit[13],
                   fit[2] * x + fit[6] * y + fit[10] * z + fit[14]};
        const float dx = p[0] - muzzle[0], dy = p[1] - muzzle[1];
        if (dx * dx + dy * dy <= kPushRadiusM * kPushRadiusM && std::isfinite(p[2])) {
            push = std::max(push, muzzle[2] - p[2]); // ahead: smaller z
        }
    }
    return std::min(push, kPushMaxM);
}

float BarrelPush(const std::vector<float>& vertices, const std::array<float, 16>& fit,
                 const V3& muzzle, const V3& axis) { // AzaharFrame fixc
    float push = 0.0f;
    for (std::size_t at = 0; at + 2 < vertices.size(); at += 12) {
        const float x = vertices[at], y = vertices[at + 1], z = vertices[at + 2];
        const V3 d{fit[0] * x + fit[4] * y + fit[8] * z + fit[12] - muzzle[0],
                   fit[1] * x + fit[5] * y + fit[9] * z + fit[13] - muzzle[1],
                   fit[2] * x + fit[6] * y + fit[10] * z + fit[14] - muzzle[2]};
        const float along = d[0] * axis[0] + d[1] * axis[1] + d[2] * axis[2];
        const float across2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] - along * along;
        if (across2 <= kPushRadiusM * kPushRadiusM && std::isfinite(along)) {
            push = std::max(push, along);
        }
    }
    return std::min(push, kPushMaxM);
}

ShotOrigin ShotOriginFor(const V3& start, float push,
                         const std::optional<V3>& barrel) { // AzaharFrame fixc
    ShotOrigin o;
    if (barrel) {
        o.from = start;
        o.forward = *barrel;
    } else {
        o.from = {start[0], start[1], start[2] - push};
    }
    return o;
}

void FlashTextureFit(std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height) {
    while ((width > kTextureMaxSide || height > kTextureMaxSide) && width >= 2 && height >= 2 &&
           rgba.size() == std::size_t{width} * height * 4) {
        const uint32_t w = width / 2, h = height / 2;
        std::vector<uint8_t> out(std::size_t{w} * h * 4);
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                for (int c = 0; c < 4; c++) {
                    const auto at = [&](uint32_t xx, uint32_t yy) {
                        return uint32_t{rgba[(std::size_t{yy} * width + xx) * 4 + c]};
                    };
                    const uint32_t sum = at(2 * x, 2 * y) + at(2 * x + 1, 2 * y) +
                                         at(2 * x, 2 * y + 1) + at(2 * x + 1, 2 * y + 1);
                    out[(std::size_t{y} * w + x) * 4 + c] = static_cast<uint8_t>((sum + 2) / 4);
                }
            }
        }
        rgba = std::move(out);
        width = w;
        height = h;
    }
}

float FlashSpin(uint32_t shot) {
    // The golden angle: each shot well apart from the one before.
    return std::fmod(static_cast<float>(shot) * 2.39996323f, 6.28318531f);
}

float FlashLinear(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

} // namespace FrameXr::Vis
