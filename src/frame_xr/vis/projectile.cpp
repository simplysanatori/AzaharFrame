// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuflash: see projectile.h.

#include "frame_xr/vis/projectile.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace FrameXr::Vis {

namespace {

using V3 = std::array<float, 3>;

V3 Sub(const V3& a, const V3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
float Dot(const V3& a, const V3& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
V3 Cross(const V3& a, const V3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
V3 Scale(const V3& a, float s) {
    return {a[0] * s, a[1] * s, a[2] * s};
}
V3 Unit(const V3& v, const V3& fallback) {
    const float l = std::sqrt(Dot(v, v));
    return l > 1e-6f && std::isfinite(l) ? Scale(v, 1.0f / l) : fallback;
}

} // namespace

int64_t ShotFlightNs(float metres, bool charged, float speed) {
    const float s = std::clamp(std::isfinite(speed) ? speed : 1.0f, 0.25f, 4.0f);
    const float m = std::isfinite(metres) ? std::max(metres, 0.0f) : 0.0f;
    const double ns = m / ((charged ? kChargedSpeed : kRapidSpeed) * s) * 1e9;
    const double lo = static_cast<double>(charged ? kChargedMinNs : kRapidMinNs) / s;
    const double hi = static_cast<double>(charged ? kChargedMaxNs : kRapidMaxNs) / s;
    return static_cast<int64_t>(std::clamp(ns, lo, hi));
}

void Projectiles::Fire(const V3& from, const V3& to, bool charged, int64_t now_ns, float speed,
                       float size) {
    Shot s;
    s.from = from;
    s.to = to;
    s.start_ns = now_ns;
    const V3 d = Sub(to, from);
    s.flight_ns = ShotFlightNs(std::sqrt(Dot(d, d)), charged, speed);
    s.charged = charged;
    s.size = std::clamp(std::isfinite(size) ? size : 1.0f, 0.25f, 4.0f);
    s.serial = ++serial_;
    if (shots_.size() >= static_cast<std::size_t>(kMaxShots)) {
        shots_.erase(shots_.begin()); // the oldest goes
    }
    shots_.push_back(s);
}

std::vector<ShotDraw> Projectiles::Current(int64_t now_ns) {
    std::vector<ShotDraw> out;
    std::erase_if(shots_, [&](const Shot& s) {
        return now_ns - s.start_ns >= s.flight_ns + kEnterNs || now_ns < s.start_ns - 1'000'000'000;
    });
    for (const Shot& s : shots_) {
        const int64_t age = now_ns - s.start_ns;
        if (age < 0) {
            continue;
        }
        ShotDraw d;
        const V3 dir = Sub(s.to, s.from);
        d.forward = Unit(dir, {0, 0, -1});
        const float base = (s.charged ? kChargedSizeM : kRapidSizeM) * s.size;
        if (age < s.flight_ns) {
            const float t = static_cast<float>(age) / static_cast<float>(s.flight_ns);
            d.at = {s.from[0] + dir[0] * t, s.from[1] + dir[1] * t, s.from[2] + dir[2] * t};
            d.size = base;
            d.strength = std::min(1.0f, t / 0.1f + 0.35f); // out of the flash, quickly full
        } else {
            // Into the screen: shrinking to nothing at the point it hit.
            const float e = static_cast<float>(age - s.flight_ns) / static_cast<float>(kEnterNs);
            d.at = s.to;
            d.size = base * (1.0f - e);
            d.strength = 1.0f - e;
        }
        // A slow turn about the way it flies (the bullets' own animations are not read).
        d.spin = static_cast<float>(age) * 1e-9f * (s.charged ? 4.0f : 7.0f) +
                 static_cast<float>(s.serial) * 2.39996323f;
        d.charged = s.charged;
        d.serial = s.serial;
        out.push_back(d);
    }
    return out;
}

ShotTune ParseShotTune(const std::optional<std::string>& value) {
    ShotTune t;
    if (!value || value->empty()) {
        return t;
    }
    const char* s = value->c_str();
    char* end = nullptr;
    const float speed = std::strtof(s, &end);
    if (end != s && speed >= 0.25f && speed <= 4.0f) {
        t.speed = speed;
    }
    if (end && *end == ',') {
        const char* z = end + 1;
        char* zend = nullptr;
        const float size = std::strtof(z, &zend);
        if (zend != z && size >= 0.25f && size <= 4.0f) {
            t.size = size;
        }
    }
    return t;
}

std::array<float, 16> ShotTransform(const ShotDraw& d) {
    const V3 z = Unit(d.forward, {0, 0, -1});
    const V3 up = std::abs(z[1]) < 0.95f ? V3{0, 1, 0} : V3{1, 0, 0};
    V3 x = Unit(Cross(up, z), {1, 0, 0});
    V3 y = Cross(z, x);
    const float c = std::cos(d.spin), s = std::sin(d.spin);
    const V3 xs{x[0] * c + y[0] * s, x[1] * c + y[1] * s, x[2] * c + y[2] * s};
    const V3 ys{y[0] * c - x[0] * s, y[1] * c - x[1] * s, y[2] * c - x[2] * s};
    std::array<float, 16> m{};
    for (int r = 0; r < 3; r++) {
        m[static_cast<std::size_t>(r)] = xs[static_cast<std::size_t>(r)] * d.size;
        m[static_cast<std::size_t>(4 + r)] = ys[static_cast<std::size_t>(r)] * d.size;
        m[static_cast<std::size_t>(8 + r)] = z[static_cast<std::size_t>(r)] * d.size;
        m[static_cast<std::size_t>(12 + r)] = d.at[static_cast<std::size_t>(r)];
    }
    m[15] = 1.0f;
    return m;
}

} // namespace FrameXr::Vis
