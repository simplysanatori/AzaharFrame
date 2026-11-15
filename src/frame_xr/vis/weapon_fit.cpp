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

/// Column major a * b.
FitMat Mul4(const FitMat& a, const FitMat& b) {
    FitMat m{};
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            float v = 0.0f;
            for (int k = 0; k < 4; k++) {
                v += a[static_cast<std::size_t>(k * 4 + r)] * b[static_cast<std::size_t>(c * 4 + k)];
            }
            m[static_cast<std::size_t>(c * 4 + r)] = v;
        }
    }
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

HoldMap HoldMapFor(int cls) {
    switch (cls) {
    case 0: // blade
    case 1: // staff
    case 5: // club
        return HoldMap::Grip;
    case 3: // bow
    case 6: // cannon
        return HoldMap::Point;
    default:
        return HoldMap::None;
    }
}

HoldPick HoldPickFor(int cls, const std::optional<std::string>& value) {
    if (value && *value == "idle") {
        return HoldPick::Idle;
    }
    if (value && *value == "shot") {
        return HoldPick::Shot;
    }
    return cls == 1 ? HoldPick::Shot : HoldPick::Idle;
}

namespace {

/// Pit's hand's frame in aim space for a hold (row major: columns are where his hand's x, y, z
/// go), and where its fist's centre goes.
M3 HoldHandFrame(HoldMap map) {
    V3 x{0, 0, 1}, y{1, 0, 0}, z{0, 1, 0}; // Point: fingers (-x) ahead, thumb (+z) up
    if (map == HoldMap::Grip) {
        z = Normal(kHandleAxis);
        y = Normal(Sub(V3{1, 0, 0}, Scale(z, z[0])));
        x = Cross(y, z);
    }
    return {x[0], y[0], z[0], x[1], y[1], z[1], x[2], y[2], z[2]};
}

V3 HoldFistAt(HoldMap map) {
    return map == HoldMap::Grip ? kHandleCentre : kPalm;
}

/// The game's hold (weapon space to the holding hand's), as a right hand's: Pit's left hand's
/// (a bow's) mirrored across his x (his right is -x), with its fist.
FitMat HoldMatrix(const FitHold& h, V3& fist) {
    FitMat m{};
    const float flip = h.left ? -1.0f : 1.0f;
    for (int row = 0; row < 3; row++) {
        const float f = row == 0 ? flip : 1.0f;
        for (int col = 0; col < 4; col++) {
            m[static_cast<std::size_t>(col * 4 + row)] = f * h.to_hand[static_cast<std::size_t>(row * 4 + col)];
        }
    }
    m[15] = 1.0f;
    fist = {flip * h.fist[0], h.fist[1], h.fist[2]};
    return m;
}

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

GlowMotion AnimateGlow(int cls, int source, float frames, float du, float dv, double time_s,
                       float game_fps) {
    constexpr double kTau = 6.283185307179586;
    const auto frac = [](double x) { return x - std::floor(x); };
    GlowMotion g;
    const double t = std::isfinite(time_s) ? time_s : 0.0;
    // The slide: a sawtooth over the loop (the game's linear loop; its jump at the loop's end is
    // a whole repeat of the texture, unseen).
    if (source != 0 && frames > 0.0f && game_fps > 0.0f && std::isfinite(frames) &&
        std::isfinite(du) && std::isfinite(dv)) {
        const double phase = frac(t * static_cast<double>(game_fps) / static_cast<double>(frames));
        g.u = static_cast<float>(du * phase);
        g.v = static_cast<float>(dv * phase);
    }
    // The flicker: three slow waves, their weights adding up to 1.
    const double amount = cls == 4 ? kGlowFlicker : kGlowFlicker * 0.5;
    const double wave = 0.5 * std::sin(kTau * 1.7 * t) + 0.3 * std::sin(kTau * 3.1 * t + 1.3) +
                        0.2 * std::sin(kTau * 4.3 * t + 2.1);
    g.gain = static_cast<float>(1.0 + amount * wave);
    // The sway of a palm's flames: about three waves along them, two phases (1.3 s and 0.9 s).
    if (cls == 4) {
        g.sway = kPalmSway;
        g.wave = static_cast<float>(kTau / 3.0);
        g.phase_a = static_cast<float>(kTau * frac(t / 1.3));
        g.phase_b = static_cast<float>(kTau * frac(t / 0.9));
    }
    return g;
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

namespace {

/// The fist's centre in weapon space: the hold's (in Pit's hand's space) taken back through the
/// hold (weapon space to the hand's, a rotation and a translation).
std::optional<V3> FistInWeapon(const FitHold& h) {
    const auto& a = h.to_hand;
    const M3 r{a[0], a[1], a[2], a[4], a[5], a[6], a[8], a[9], a[10]};
    const V3 t{a[3], a[7], a[11]};
    const float det = r[0] * (r[4] * r[8] - r[5] * r[7]) - r[1] * (r[3] * r[8] - r[5] * r[6]) +
                      r[2] * (r[3] * r[7] - r[4] * r[6]);
    if (!(std::fabs(det) > 1e-9f)) {
        return std::nullopt;
    }
    const M3 inv{(r[4] * r[8] - r[5] * r[7]) / det, (r[2] * r[7] - r[1] * r[8]) / det,
                 (r[1] * r[5] - r[2] * r[4]) / det, (r[5] * r[6] - r[3] * r[8]) / det,
                 (r[0] * r[8] - r[2] * r[6]) / det, (r[2] * r[3] - r[0] * r[5]) / det,
                 (r[3] * r[7] - r[4] * r[6]) / det, (r[1] * r[6] - r[0] * r[7]) / det,
                 (r[0] * r[4] - r[1] * r[3]) / det};
    return Apply(inv, Sub(h.fist, t));
}

V3 TransformDir(const FitMat& m, const V3& d) {
    return {m[0] * d[0] + m[4] * d[1] + m[8] * d[2], m[1] * d[0] + m[5] * d[1] + m[9] * d[2],
            m[2] * d[0] + m[6] * d[1] + m[10] * d[2]};
}

} // namespace

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

    // AzaharFrame kiuweap2: the game's hold (FitHold): Pit's hand on the player's, the weapon
    // as he holds it; the tune turns about the fist and offsets it; the left hand gets the
    // right hand's mirrored.
    if (const HoldMap map = HoldMapFor(w.cls); w.hold && map != HoldMap::None) {
        V3 fist{};
        const FitMat hold = HoldMatrix(*w.hold, fist);
        const M3 r = Mul(turn, HoldHandFrame(map));
        const FitMat base = Compose(at(HoldFistAt(map)), r, scale, fist, left_hand);
        const FitSide side = has(FitSide::Centre) ? FitSide::Centre : FitSide::Right;
        out.push_back({side, Mul4(base, hold), left_hand != w.hold->left});
        return out;
    }

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
        if (p.side == FitSide::Left &&
            (kFits[w.cls].rule == Rule::Worn || kFits[w.cls].rule == Rule::Pair)) {
            // The left claw's muzzle, in its own hand's space; the left orbitar's, its own
            // (the json gives the right one's: the left is its mirror, so their middle stays on
            // the hand's line).
            mz[0] = -mz[0];
        }
        const V3 q = TransformPoint(p.transform, mz);
        sum = {sum[0] + q[0], sum[1] + q[1], sum[2] + q[2]};
    }
    return Scale(sum, 1.0f / static_cast<float>(parts.size()));
}


std::optional<std::array<float, 3>> FitFist(const FitWeapon& w, bool left_hand,
                                            const std::vector<FitSide>& sides,
                                            const FitOverride& tune) {
    if (!w.hold || w.cls < 0 || w.cls >= 9) {
        return std::nullopt;
    }
    const auto fist = FistInWeapon(*w.hold);
    const auto parts = FitWeaponToHand(w, left_hand, sides, tune);
    if (!fist || parts.empty()) {
        return std::nullopt;
    }
    V3 f = *fist;
    if (parts.front().side == FitSide::Left && kFits[w.cls].rule == Rule::Worn) {
        f[0] = -f[0];
    }
    return TransformPoint(parts.front().transform, f);
}

std::optional<std::array<float, 3>> FitAxis(const FitWeapon& w, bool left_hand,
                                            const std::vector<FitSide>& sides,
                                            const FitOverride& tune) {
    if (!w.hold || w.cls < 0 || w.cls >= 9 || HoldMapFor(w.cls) == HoldMap::None) {
        return std::nullopt;
    }
    const auto fist = FistInWeapon(*w.hold);
    const auto parts = FitWeaponToHand(w, left_hand, sides, tune);
    if (!fist || parts.empty()) {
        return std::nullopt;
    }
    const FitMat& m = parts.front().transform;
    V3 axis{};
    if (HoldMapFor(w.cls) == HoldMap::Grip && w.muzzle_valid) {
        // A hilt or a shaft in the fist: from the fist to the muzzle (the blade, the staff, the
        // club as his hand holds it).
        axis = Sub(TransformPoint(m, w.muzzle), TransformPoint(m, *fist));
        if (axis[2] >= 0.0f) {
            // Held pointing back (a staff in Pit's idle grip, AZAHARFRAME_KIU_HOLD=idle): no
            // aim along it; the controller's line.
            return std::nullopt;
        }
    }
    if (Length(axis) < 0.03f) {
        // Held by the hand's line (a bow's arrow, a cannon's forearm: their muzzles sit beside
        // it), or a muzzle in the fist: the class's forward as held, away from the player.
        axis = TransformDir(m, kFits[w.cls].forward);
        if (axis[2] > 0.0f) {
            axis = Scale(axis, -1.0f);
        }
    }
    if (!(Length(axis) > 1e-6f)) {
        return std::nullopt;
    }
    return Normal(axis);
}

} // namespace FrameXr::Vis
