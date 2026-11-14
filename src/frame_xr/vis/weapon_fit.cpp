// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "frame_xr/vis/weapon_fit.h"

// AzaharFrame kiuguns (weapon_fit.h)
namespace FrameXr::Vis {

namespace {

using V3 = std::array<float, 3>;

V3 Sub(const V3& a, const V3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
V3 Scale(const V3& a, float s) {
    return {a[0] * s, a[1] * s, a[2] * s};
}
float Dot(const V3& a, const V3& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
V3 Cross(const V3& a, const V3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
float Length(const V3& a) {
    return std::sqrt(Dot(a, a));
}
V3 Normal(const V3& a) {
    const float l = Length(a);
    return l > 1e-9f ? Scale(a, 1.0f / l) : V3{0.0f, 0.0f, -1.0f};
}

/// Row major 3x3.
using M3 = std::array<float, 9>;

M3 Mul(const M3& a, const M3& b) {
    M3 m{};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            for (int k = 0; k < 3; k++) {
                m[r * 3 + c] += a[r * 3 + k] * b[k * 3 + c];
            }
        }
    }
    return m;
}
V3 Apply(const M3& m, const V3& v) {
    return {m[0] * v[0] + m[1] * v[1] + m[2] * v[2], m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
            m[6] * v[0] + m[7] * v[1] + m[8] * v[2]};
}

/// The rotation taking the weapon's `forward` and `up` to the aim space's `to_forward` and
/// `to_up` (up made square to forward on both sides).
M3 Turn(const V3& forward, const V3& up, const V3& to_forward, const V3& to_up) {
    const auto frame = [](const V3& f0, const V3& u0) {
        const V3 f = Normal(f0);
        V3 u = Sub(u0, Scale(f, Dot(u0, f)));
        if (Length(u) < 1e-4f) {
            u = std::fabs(f[1]) < 0.9f ? V3{0, 1, 0} : V3{0, 0, 1};
            u = Sub(u, Scale(f, Dot(u, f)));
        }
        u = Normal(u);
        return std::array<V3, 3>{f, u, Cross(f, u)};
    };
    const auto a = frame(forward, up);
    const auto b = frame(to_forward, to_up);
    // R = B * A^T (columns of A and B: forward, up, side).
    M3 r{};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float v = 0.0f;
            for (int k = 0; k < 3; k++) {
                v += b[static_cast<std::size_t>(k)][static_cast<std::size_t>(i)] *
                     a[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)];
            }
            r[i * 3 + j] = v;
        }
    }
    return r;
}

M3 RotY(float deg) {
    const float a = deg * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a);
    return {c, 0, s, 0, 1, 0, -s, 0, c};
}
M3 RotX(float deg) {
    const float a = deg * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a);
    return {1, 0, 0, 0, c, -s, 0, s, c};
}
M3 RotZ(float deg) {
    const float a = deg * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a);
    return {c, -s, 0, s, c, 0, 0, 0, 1};
}

/// x_aim = at + R * (scale * (x - grip)), then mirrored across x when asked.
FitMat Compose(const V3& at, const M3& r, float scale, const V3& grip, bool mirror) {
    FitMat m{};
    const V3 t = Sub(at, Apply(r, Scale(grip, scale)));
    for (int row = 0; row < 3; row++) {
        const float flip = (mirror && row == 0) ? -1.0f : 1.0f;
        for (int col = 0; col < 3; col++) {
            m[static_cast<std::size_t>(col * 4 + row)] = flip * r[row * 3 + col] * scale;
        }
        m[static_cast<std::size_t>(12 + row)] = flip * t[static_cast<std::size_t>(row)];
    }
    m[15] = 1.0f;
    return m;
}

enum class Rule { Held, Worn, Pair };

struct ClassFit {
    Rule rule;
    float scale;  ///< metres per game unit
    V3 grip;      ///< the grip point (weapon space)
    V3 forward;   ///< without a muzzle (and for bows): along the ray
    V3 up;        ///< kept up
    bool use_muzzle;
};

// Ours, per class (weapon_fit.h).
constexpr ClassFit kFits[9] = {
    {Rule::Held, 0.075f, {0, 0, 0}, {0, 0, -1}, {0, 1, 0}, true},     // blade (GripN)
    {Rule::Held, 0.065f, {0, 0, 0}, {0, 0, 1}, {0, 1, 0}, true},      // staff (SnipeM)
    {Rule::Worn, 0.080f, {0, 0, 0}, {-1, 0, 0}, {0, 0, 1}, false},    // claws (each hand)
    {Rule::Held, 0.070f, {0, 0, 0}, {1, 0, 0}, {0, 0, 1}, false},     // bow (LHandN)
    {Rule::Held, 0.090f, {0, 0, 0}, {-1, 0, 0}, {0, 0, 1}, true},     // palm (RHandN)
    {Rule::Held, 0.035f, {0, 0, 0}, {0, 0, -1}, {0, 1, 0}, true},     // club (GripN)
    {Rule::Held, 0.060f, {-2.17f, 0, 0}, {-1, 0, 0}, {0, 1, 0}, true}, // cannon (RArmN)
    {Rule::Pair, 0.070f, {0, 0, 0}, {0, 0, 1}, {0, 1, 0}, false},     // orbitars (each bit)
    {Rule::Worn, 0.080f, {0, 0, 0}, {-1, 0, 0}, {0, 0, 1}, false},    // arm (RHandN)
};

/// Where a hand holds a pistol at the aim pose (Moonlight XR's light gun, make_gun.py).
constexpr V3 kPalm{0.0f, -0.0525f, 0.043f};
/// The wrist for a worn weapon: behind the palm.
constexpr V3 kWrist{0.0f, -0.045f, 0.10f};
/// The orbitars: either side of the hand, a little above and ahead of it.
constexpr V3 kOrbitar{0.17f, 0.05f, -0.08f};

} // namespace

std::array<float, 3> FitMotion(int cls, FitSide side, bool glow, double time_s) {
    constexpr double kTau = 6.283185307179586;
    std::array<float, 3> m{};
    if (cls == 7 && side != FitSide::Centre) {
        const double phase = side == FitSide::Left ? kTau / 2.0 : 0.0;
        const double a = kTau * time_s / 2.6 + phase;
        const double b = kTau * time_s / 2.1 + phase;
        m[0] = static_cast<float>(0.015 * std::cos(a));
        m[1] = static_cast<float>(0.015 * std::sin(a) + 0.010 * std::sin(b));
    } else if (glow) {
        m[1] = static_cast<float>(0.008 * std::sin(kTau * time_s / 2.4));
    }
    return m;
}

float FitClassScale(int cls) {
    return cls >= 0 && cls < 9 ? kFits[cls].scale : 0.07f;
}

std::string FitSwitchName(int cls) {
    static constexpr const char* kNames[9] = {"BLADE",  "STAFF", "CLAWS",    "BOW", "PALM",
                                              "CLUB",   "CANNON", "ORBITARS", "ARM"};
    return std::string{"AZAHARFRAME_KIU_FIT_"} + (cls >= 0 && cls < 9 ? kNames[cls] : "X");
}

FitOverride ParseFitOverride(const std::optional<std::string>& value) {
    FitOverride o;
    if (!value || value->empty()) {
        return o;
    }
    float v[7] = {0, 0, 0, 0, 0, 0, 1};
    const char* p = value->c_str();
    int n = 0;
    while (n < 7 && *p) {
        char* end = nullptr;
        const float f = std::strtof(p, &end);
        if (end == p) {
            break;
        }
        if (std::isfinite(f)) {
            v[n] = f;
        }
        n++;
        p = end;
        while (*p == ',' || *p == ' ' || *p == ';') {
            p++;
        }
    }
    if (n == 0) {
        return o;
    }
    for (int k = 0; k < 3; k++) {
        o.offset_cm[static_cast<std::size_t>(k)] = std::clamp(v[k], -100.0f, 100.0f);
    }
    o.yaw = std::clamp(v[3], -360.0f, 360.0f);
    o.pitch = std::clamp(v[4], -360.0f, 360.0f);
    o.roll = std::clamp(v[5], -360.0f, 360.0f);
    o.scale = std::clamp(v[6], 0.05f, 20.0f);
    o.set = true;
    return o;
}

std::vector<FitPart> FitWeaponToHand(const FitWeapon& w, bool left_hand,
                                     const std::vector<FitSide>& sides, const FitOverride& tune) {
    std::vector<FitPart> out;
    if (w.cls < 0 || w.cls >= 9) {
        return out;
    }
    const ClassFit& c = kFits[w.cls];
    const float scale = c.scale * tune.scale;
    const M3 turn = Mul(RotY(tune.yaw), Mul(RotX(tune.pitch), RotZ(-tune.roll)));
    const V3 offset = Scale(tune.offset_cm, 0.01f);
    const auto has = [&](FitSide s) { return std::find(sides.begin(), sides.end(), s) != sides.end(); };
    const auto at = [&](const V3& p) { return V3{p[0] + offset[0], p[1] + offset[1], p[2] + offset[2]}; };

    switch (c.rule) {
    case Rule::Held: {
        // The grip at the palm; the muzzle on the ray when it is far enough out.
        V3 forward = c.forward;
        V3 to_forward{0.0f, 0.0f, -1.0f};
        if (c.use_muzzle && w.muzzle_valid) {
            const V3 reach = Sub(w.muzzle, c.grip);
            const float d = Length(reach) * scale;
            if (d > 0.08f) {
                forward = reach;
                const float z = kPalm[2] - std::sqrt(d * d - kPalm[1] * kPalm[1]);
                to_forward = Normal(Sub(V3{0.0f, 0.0f, z}, kPalm));
            }
        }
        const M3 r = Mul(turn, Turn(forward, c.up, to_forward, {0.0f, 1.0f, 0.0f}));
        // A whole weapon, or (a file with halves only) its right half.
        const FitSide side = has(FitSide::Centre) ? FitSide::Centre : FitSide::Right;
        out.push_back({side, Compose(at(kPalm), r, scale, c.grip, left_hand), left_hand});
        break;
    }
    case Rule::Worn: {
        // Pit's hand frame on the player's: fingers along the ray, the back of the hand
        // outward, the thumb up.
        if (has(FitSide::Right) || has(FitSide::Left)) {
            const bool left = left_hand && has(FitSide::Left);
            const M3 base = left ? Turn({1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0})
                                 : Turn({-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0});
            // A left-handed tune is the right one mirrored.
            const M3 t = left ? Mul(RotY(-tune.yaw), Mul(RotX(tune.pitch), RotZ(tune.roll))) : turn;
            const V3 off = left ? V3{-offset[0], offset[1], offset[2]} : offset;
            const V3 wrist{kWrist[0] + off[0], kWrist[1] + off[1], kWrist[2] + off[2]};
            const bool mirror = left_hand && !left;
            out.push_back({left ? FitSide::Left : FitSide::Right,
                           Compose(mirror ? at(kWrist) : wrist, Mul(t, base), scale, c.grip, mirror),
                           mirror});
        } else {
            const M3 base = Turn({-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0});
            out.push_back({FitSide::Centre, Compose(at(kWrist), Mul(turn, base), scale, c.grip, left_hand),
                           left_hand});
        }
        break;
    }
    case Rule::Pair: {
        // Both halves float by the hand, facing along the ray, whichever hand aims.
        const M3 r = Mul(turn, Turn(c.forward, c.up, {0, 0, -1}, {0, 1, 0}));
        for (const FitSide s : {FitSide::Right, FitSide::Left}) {
            if (!has(s)) {
                continue;
            }
            const float x = s == FitSide::Right ? kOrbitar[0] : -kOrbitar[0];
            out.push_back({s, Compose(at({x, kOrbitar[1], kOrbitar[2]}), r, scale, c.grip, false),
                           false});
        }
        if (out.empty() && has(FitSide::Centre)) {
            out.push_back({FitSide::Centre, Compose(at({kOrbitar[0], kOrbitar[1], kOrbitar[2]}), r,
                                                    scale, c.grip, false),
                           false});
        }
        break;
    }
    }
    return out;
}

} // namespace FrameXr::Vis
