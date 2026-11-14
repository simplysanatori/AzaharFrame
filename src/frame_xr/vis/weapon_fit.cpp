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

V3 TransformPoint(const FitMat& m, const V3& p) {
    return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
            m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
            m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
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
    {Rule::Held, 0.090f, {-0.7f, 0, 0}, {-1, 0, 0}, {0, 0, 1}, true}, // palm (RHandN; its palm)
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
    } else if (glow && cls != 4) { // a palm's flames stay with the hand
        m[1] = static_cast<float>(0.008 * std::sin(kTau * time_s / 2.4));
    }
    return m;
}

PalmMode PalmModeFrom(const std::optional<std::string>& value) {
    return value && *value == "elbow" ? PalmMode::Elbow : PalmMode::Wrist;
}

std::array<float, 3> EstimateElbow(const std::array<float, 3>& head,
                                   const std::array<float, 3>& head_forward, bool left_hand,
                                   const std::array<float, 3>& wrist) {
    constexpr float kShoulderDown = 0.17f, kShoulderSide = 0.16f;
    constexpr float kUpperArm = 0.30f, kForearm = 0.27f;
    // The head's yaw frame: its forward flattened (straight ahead when it looks up or down).
    V3 fwd{head_forward[0], 0.0f, head_forward[2]};
    if (Length(fwd) < 1e-4f) {
        fwd = {0.0f, 0.0f, -1.0f};
    }
    fwd = Normal(fwd);
    const V3 right = Cross(fwd, V3{0.0f, 1.0f, 0.0f}); // -z ahead: +x right
    const float side = left_hand ? -kShoulderSide : kShoulderSide;
    const V3 shoulder{head[0] + right[0] * side, head[1] - kShoulderDown, head[2] + right[2] * side};
    // Two bones from the shoulder to the wrist; the elbow down and a little outward.
    const V3 to_wrist = Sub(wrist, shoulder);
    float d = Length(to_wrist);
    if (d < 1e-4f) {
        return {shoulder[0], shoulder[1] - kUpperArm, shoulder[2]};
    }
    const V3 axis = Scale(to_wrist, 1.0f / d);
    d = std::min(d, kUpperArm + kForearm - 1e-4f);
    const float along = (kUpperArm * kUpperArm - kForearm * kForearm + d * d) / (2.0f * d);
    const float out = std::sqrt(std::max(0.0f, kUpperArm * kUpperArm - along * along));
    V3 bend{right[0] * side * 2.0f, -1.0f, right[2] * side * 2.0f}; // down, outward
    bend = Sub(bend, Scale(axis, Dot(bend, axis)));
    if (Length(bend) < 1e-4f) {
        bend = Sub(V3{0, 0, 1}, Scale(axis, Dot(V3{0, 0, 1}, axis)));
    }
    bend = Normal(bend);
    return {shoulder[0] + axis[0] * along + bend[0] * out,
            shoulder[1] + axis[1] * along + bend[1] * out,
            shoulder[2] + axis[2] * along + bend[2] * out};
}

std::array<float, 3> SmoothDirection(const std::array<float, 3>& previous,
                                     const std::array<float, 3>& target, float dt_s) {
    if (Length(previous) < 0.5f || !(dt_s >= 0.0f)) {
        return Normal(target);
    }
    const float k = 1.0f - std::exp(-std::min(dt_s, 1.0f) / 0.08f);
    return Normal({previous[0] + (target[0] - previous[0]) * k,
                   previous[1] + (target[1] - previous[1]) * k,
                   previous[2] + (target[2] - previous[2]) * k});
}

FitMat RotationBetween(const std::array<float, 3>& pivot, const std::array<float, 3>& from,
                       const std::array<float, 3>& to) {
    const V3 a = Normal(from), b = Normal(to);
    const V3 axis = Cross(a, b);
    const float s = Length(axis), c = Dot(a, b);
    M3 r{1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (s > 1e-6f) {
        const V3 k = Scale(axis, 1.0f / s);
        const float t = 1.0f - c;
        r = {c + k[0] * k[0] * t,        k[0] * k[1] * t - k[2] * s, k[0] * k[2] * t + k[1] * s,
             k[1] * k[0] * t + k[2] * s, c + k[1] * k[1] * t,        k[1] * k[2] * t - k[0] * s,
             k[2] * k[0] * t - k[1] * s, k[2] * k[1] * t + k[0] * s, c + k[2] * k[2] * t};
    } else if (c < 0.0f) {
        // Opposite: half a turn about any axis square to them.
        V3 k = Normal(Cross(a, std::fabs(a[1]) < 0.9f ? V3{0, 1, 0} : V3{1, 0, 0}));
        r = {2 * k[0] * k[0] - 1, 2 * k[0] * k[1],     2 * k[0] * k[2],
             2 * k[1] * k[0],     2 * k[1] * k[1] - 1, 2 * k[1] * k[2],
             2 * k[2] * k[0],     2 * k[2] * k[1],     2 * k[2] * k[2] - 1};
    }
    return Compose(pivot, r, 1.0f, pivot, false);
}

float FitClipX(int cls, const FitOverride& tune, const std::optional<std::string>& forearm_cm,
               PalmMode mode) {
    if (cls != 4) {
        return 0.0f;
    }
    float cm = mode == PalmMode::Elbow ? kPalmForearmCm : kPalmStubCm;
    if (forearm_cm && !forearm_cm->empty()) {
        char* end = nullptr;
        const float v = std::strtof(forearm_cm->c_str(), &end);
        if (end != forearm_cm->c_str() && std::isfinite(v)) {
            cm = std::clamp(v, 1.0f, 200.0f);
        }
    }
    return cm / 100.0f / (kFits[cls].scale * tune.scale);
}

float FitClassScale(int cls) {
    return cls >= 0 && cls < 9 ? kFits[cls].scale : 0.07f;
}

float HandSizeCapM(int cls) {
    //                                blade  staff  claws  bow    palm   club   cannon orbit  arm
    static constexpr float kCap[9] = {0.45f, 0.45f, 0.35f, 0.45f, 0.45f, 0.45f, 0.40f, 0.35f, 0.40f};
    return cls >= 0 && cls < 9 ? kCap[cls] : 0.40f;
}

float WeaponSizeFactor(int cls, const std::array<float, 3>& bounds_min,
                       const std::array<float, 3>& bounds_max, float fit_scale, WeaponSize size,
                       int scale_percent) {
    float f = 1.0f;
    if (size == WeaponSize::Hand) {
        float longest = 0.0f;
        for (int k = 0; k < 3; k++) {
            const float e = bounds_max[static_cast<std::size_t>(k)] -
                            bounds_min[static_cast<std::size_t>(k)];
            if (std::isfinite(e)) {
                longest = std::max(longest, e);
            }
        }
        const float metres = longest * fit_scale;
        const float cap = HandSizeCapM(cls);
        if (metres > cap && metres > 0.0f) {
            f = cap / metres;
        }
    }
    const int pct = std::clamp(scale_percent, 50, 150);
    return f * static_cast<float>(pct) / 100.0f;
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
            FitMat m = Compose(mirror ? at(kWrist) : wrist, Mul(t, base), scale, c.grip, mirror);
            // The hand frame raised onto the ray at its muzzle (the left claw's is its own: +x),
            // the tune's offset kept on top.
            if (w.muzzle_valid) {
                const FitMat plain = Compose(kWrist, Mul(t, base), scale, c.grip, mirror);
                const V3 mz = left ? V3{-w.muzzle[0], w.muzzle[1], w.muzzle[2]} : w.muzzle;
                const V3 p = TransformPoint(plain, mz);
                m[12] -= p[0];
                m[13] -= p[1];
            }
            out.push_back({left ? FitSide::Left : FitSide::Right, m, mirror});
        } else {
            const M3 base = Turn({-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0});
            FitMat m = Compose(at(kWrist), Mul(turn, base), scale, c.grip, left_hand);
            if (w.muzzle_valid) {
                const FitMat plain = Compose(kWrist, Mul(turn, base), scale, c.grip, left_hand);
                const V3 p = TransformPoint(plain, w.muzzle);
                m[12] -= p[0];
                m[13] -= p[1];
            }
            out.push_back({FitSide::Centre, m, left_hand});
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

std::optional<std::array<float, 3>> FitMuzzle(const FitWeapon& w, bool left_hand,
                                              const std::vector<FitSide>& sides,
                                              const FitOverride& tune) {
    const auto parts = FitWeaponToHand(w, left_hand, sides, tune);
    if (parts.empty() || w.cls < 0 || w.cls >= 9) {
        return std::nullopt;
    }
    V3 sum{};
    for (const FitPart& p : parts) {
        V3 mz = w.muzzle_valid ? w.muzzle : kFits[w.cls].grip;
        if (p.side == FitSide::Left && kFits[w.cls].rule == Rule::Worn) {
            mz[0] = -mz[0]; // the left claw's muzzle, in its own hand's space
        }
        const V3 q = TransformPoint(p.transform, mz);
        sum = {sum[0] + q[0], sum[1] + q[1], sum[2] + q[2]};
    }
    return Scale(sum, 1.0f / static_cast<float>(parts.size()));
}

} // namespace FrameXr::Vis
