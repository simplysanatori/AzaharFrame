// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: see immersive_layout.h.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <sstream>

#include "frame_xr/immersive_layout.h"
#include "frame_xr/immersive_names.h" // AzaharFrame kiu

namespace FrameXr::Immersive::Layout {

using Input::Posef;
using Input::Quatf;
using Input::Vec3f;

namespace {

// The normal bottom screen (app.cpp DefaultScreenPlacement): 0.75 m below and 1.5 m ahead of
// the eyes, 0.75 x 0.5625 m.
constexpr float kNormalBottomY = -0.75f;
constexpr float kNormalBottomZ = -1.5f;
constexpr float kNormalBottomWidth = 0.75f;
constexpr float kNormalBottomHeight = 0.5625f;
// Mode 1's bottom screen sits at this fraction of the wide screen's distance.
constexpr float kWideBottomNearer = 0.7f;

Posef Compose(const Posef& a, const Posef& b) {
    return {Input::Normalized(Input::Multiply(a.orientation, b.orientation)),
            a.position + Input::Rotate(a.orientation, b.position)};
}

bool Inside(const ViewFov& v, const Vec3f& p) {
    if (p.z >= -1e-6f) {
        return false;
    }
    const float h = std::atan2(p.x, -p.z);
    const float e = std::atan2(p.y, -p.z);
    return h >= v.left && h <= v.right && e >= v.down && e <= v.up;
}

bool QuadFits(const ViewFov& v, float aspect, float pitch_rad, float tan_half_y,
              float eye_shift) {
    const Quatf q = PitchRotation(pitch_rad);
    const float hy = tan_half_y;
    const float hx = tan_half_y * aspect;
    for (const float sx : {-1.0f, 1.0f}) {
        for (const float sy : {-1.0f, 1.0f}) {
            const Vec3f corner = Input::Rotate(q, Vec3f{sx * hx, sy * hy, -1.0f});
            for (const float shift : {-eye_shift, eye_shift}) {
                if (!Inside(v, {corner.x - shift, corner.y, corner.z})) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace

ViewFov BothEyes(const EyeFov& l, const EyeFov& r) {
    ViewFov v;
    v.left = std::max(l.left + l.yaw, r.left + r.yaw);
    v.right = std::min(l.right + l.yaw, r.right + r.yaw);
    v.up = std::min(l.up + l.pitch, r.up + r.pitch);
    v.down = std::max(l.down + l.pitch, r.down + r.pitch);
    // AzaharFrame immersive: how far either eye sees to the side.
    v.outer_left = std::min(l.left + l.yaw, r.left + r.yaw);
    v.outer_right = std::max(l.right + l.yaw, r.right + r.yaw);
    v.outer_up = std::max(l.up + l.pitch, r.up + r.pitch);
    v.outer_down = std::min(l.down + l.pitch, r.down + r.pitch);
    // Nonsense from the runtime (no overlap, or an inverted range): keep the fallback.
    if (!(v.right - v.left > 10.0f * kDegToRad) || !(v.up - v.down > 10.0f * kDegToRad) ||
        !(v.outer_left <= v.left) || !(v.outer_right >= v.right) ||
        !(v.outer_right - v.outer_left < 170.0f * kDegToRad) ||
        !(v.outer_up - v.outer_down < 170.0f * kDegToRad)) {
        return ViewFov{};
    }
    return v;
}

namespace {
// AzaharFrame jump: ViewSettle's fields, in one order.
constexpr int kViewFields = 8;
void ViewFields(const ViewFov& v, float out[kViewFields]) {
    out[0] = v.left;
    out[1] = v.right;
    out[2] = v.up;
    out[3] = v.down;
    out[4] = v.outer_left;
    out[5] = v.outer_right;
    out[6] = v.outer_up;
    out[7] = v.outer_down;
}
ViewFov FromFields(const double in[kViewFields], double n) {
    ViewFov v;
    v.left = static_cast<float>(in[0] / n);
    v.right = static_cast<float>(in[1] / n);
    v.up = static_cast<float>(in[2] / n);
    v.down = static_cast<float>(in[3] / n);
    v.outer_left = static_cast<float>(in[4] / n);
    v.outer_right = static_cast<float>(in[5] / n);
    v.outer_up = static_cast<float>(in[6] / n);
    v.outer_down = static_cast<float>(in[7] / n);
    return v;
}
} // namespace

float ViewSettle::Distance(const ViewFov& view) const {
    float a[kViewFields], b[kViewFields];
    ViewFields(view, a);
    ViewFields(view_, b);
    float d = 0.0f;
    for (int i = 0; i < kViewFields; i++) {
        d = std::max(d, std::abs(a[i] - b[i]));
    }
    return d;
}

void ViewSettle::Restart() {
    count_ = 0;
    std::fill(std::begin(sum_), std::end(sum_), 0.0);
}

bool ViewSettle::Add(const ViewFov& view, float half_ipd) {
    float f[kViewFields];
    ViewFields(view, f);
    bool finite = std::isfinite(half_ipd);
    for (const float x : f) {
        finite = finite && std::isfinite(x);
    }
    if (!finite) {
        return false;
    }
    bool changed = false;
    if (!has_) { // something to fit to from the first frame on
        has_ = true;
        view_ = view;
        half_ipd_ = half_ipd;
        Restart();
        changed = true;
    }
    for (int i = 0; i < kViewFields; i++) {
        sum_[i] += f[i];
    }
    sum_[kViewFields] += half_ipd;
    count_++;
    if (count_ < (settled_ ? kViewSettleBlockFrames : kViewSettleFirstFrames)) {
        return changed;
    }
    const ViewFov mean = FromFields(sum_, count_);
    const float mean_ipd = static_cast<float>(sum_[kViewFields] / count_);
    // The first mean replaces the first frame's view whenever it differs at all; after that only
    // a lasting change does.
    const bool first = !settled_;
    settled_ = true;
    Restart();
    const float limit = first ? 0.0f : kViewSettleChangeDeg * kDegToRad;
    const float ipd_limit = first ? 0.0f : kViewSettleIpdM;
    if (Distance(mean) > limit || std::abs(mean_ipd - half_ipd_) > ipd_limit) {
        view_ = mean;
        half_ipd_ = mean_ipd;
        changed = true;
    }
    return changed;
}

Placement Clamped(Placement p) {
    p.pitch_deg = std::isfinite(p.pitch_deg) ? std::clamp(p.pitch_deg, kMinPitchDeg, kMaxPitchDeg)
                                             : kDefaultPitchDeg;
    p.distance = std::isfinite(p.distance) ? std::clamp(p.distance, kMinDistance, kMaxDistance)
                                           : kDefaultDistance;
    p.size = std::isfinite(p.size) ? std::clamp(p.size, kMinSize, kMaxSize) : 1.0f;
    return p;
}

float ViewMiddlePitch(const ViewFov& view) {
    return (view.up + view.down) / 2.0f;
}

float FitTanHalfY(const ViewFov& view, float aspect, float pitch_rad, float margin_rad,
                  float eye_shift) {
    ViewFov v = view;
    v.left += margin_rad;
    v.right -= margin_rad;
    v.up -= margin_rad;
    v.down += margin_rad;
    float lo = 0.01f, hi = 4.0f;
    if (!QuadFits(v, aspect, pitch_rad, lo, eye_shift)) {
        return lo;
    }
    if (QuadFits(v, aspect, pitch_rad, hi, eye_shift)) {
        return hi;
    }
    for (int i = 0; i < 40; i++) {
        const float mid = (lo + hi) / 2.0f;
        (QuadFits(v, aspect, pitch_rad, mid, eye_shift) ? lo : hi) = mid;
    }
    return lo;
}

uint32_t NarrowPercent(uint32_t setting) {
    // AzaharFrame immersive2: percent on the 5 % grid. 1 to 3 are the 10 % steps of the builds
    // before 1.2.0; the grid never holds them, so they cannot be mistaken for a percentage.
    if (setting >= 1 && setting <= 3) {
        return setting * 10;
    }
    const uint32_t clamped = std::min(setting, kMaxNarrowPercent);
    return (clamped + kNarrowStepPercent / 2) / kNarrowStepPercent * kNarrowStepPercent;
}

float SqueezeFor(uint32_t setting) {
    return static_cast<float>(NarrowPercent(setting)) / 100.0f;
}

// AzaharFrame kiu: Horizontal Scale and Vertical Scale.
int ScalePercent(int setting) {
    const int c = std::clamp(setting, -kMaxScalePercent, kMaxScalePercent);
    // To the nearest step, halves away from 0 (the same either side).
    const int half = kScaleStepPercent / 2;
    return c >= 0 ? (c + half) / kScaleStepPercent * kScaleStepPercent
                  : -((-c + half) / kScaleStepPercent * kScaleStepPercent);
}

float SqueezeForScale(int setting) {
    return -static_cast<float>(ScalePercent(setting)) / 100.0f;
}

int SteppedScale(int setting, int steps) {
    return std::clamp(ScalePercent(setting) + steps * kScaleStepPercent, -kMaxScalePercent,
                      kMaxScalePercent);
}

std::string ScaleToast(int percent, bool vertical) {
    const char* what = vertical ? Names::kVerticalScale : Names::kHorizontalScale;
    percent = ScalePercent(percent);
    char buf[48];
    std::snprintf(buf, sizeof(buf), percent > 0 ? "%s: +%d%%" : "%s: %d%%", what, percent);
    return buf;
}

int ScaleFromNarrow(uint32_t old_setting) {
    return -static_cast<int>(NarrowPercent(old_setting));
}

namespace {
float GameTan(float game_fov_y_deg) {
    return std::tan(std::clamp(game_fov_y_deg, 10.0f, 150.0f) * kDegToRad / 2.0f);
}
float PlacementPitch(const Placement& p, const ViewFov& view) {
    return ViewMiddlePitch(view) + p.pitch_deg * kDegToRad;
}
} // namespace

Cover DefaultCover(const ViewFov& view, float pitch_rad, float eye_shift) {
    // AzaharFrame immersive: the corners of what either eye sees (a black edge in the corner of
    // one eye was the "porthole"), as seen from the screen turned down by `pitch_rad`. The view
    // is a rectangle of tangents, its picture on the screen's plane a quadrilateral, so its
    // corners are the farthest points.
    const float left = std::min(view.left, view.outer_left);
    const float right = std::max(view.right, view.outer_right);
    const float down = std::min(view.down, view.outer_down);
    const float up = std::max(view.up, view.outer_up);
    const Quatf to_screen = Input::Conjugate(PitchRotation(pitch_rad));
    float max_x = 0.0f, max_y = 0.0f;
    for (const float h : {left, right}) {
        for (const float e : {down, up}) {
            const Vec3f d = Input::Rotate(to_screen, Vec3f{std::tan(h), std::tan(e), -1.0f});
            if (d.z >= -1e-3f) {
                continue; // behind the screen's plane: the view is far wider than any screen
            }
            max_x = std::max(max_x, std::abs(d.x / -d.z));
            max_y = std::max(max_y, std::abs(d.y / -d.z));
        }
    }
    const auto past = [](float t) {
        const float angle = std::atan(t) + kFillMarginDeg * kDegToRad;
        return std::tan(std::min(angle, 80.0f * kDegToRad));
    };
    Cover c;
    // Each eye of mode 2's shared quad looks at it from `eye_shift` to the side.
    c.tan_x = past(max_x) + std::abs(eye_shift);
    c.tan_y = std::max(past(max_y), std::tan(kMinFillHalfHeightDeg * kDegToRad));
    return c;
}

bool RestPose::Update(bool engaged, bool valid, int64_t pose_time) {
    if (engaged && !was_engaged_ && have_) {
        Recentre(0); // resumed after a pause
    }
    was_engaged_ = engaged;
    if (!engaged || !valid || (have_ && !(recentre_ && pose_time >= after_))) {
        return false;
    }
    have_ = true;
    recentre_ = false;
    return true;
}

void RestPose::Recentre(int64_t after_time) {
    recentre_ = true;
    after_ = after_time;
}

void RestPose::Forget() {
    have_ = false;
    recentre_ = false;
}

float WiderFactor(uint32_t level) {
    return kFactorMin + 0.1f * static_cast<float>(std::min(level, kMaxWiderLevel));
}

std::string WiderToast(uint32_t level) {
    level = std::min(level, kMaxWiderLevel);
    char buf[48];
    if (level == 0) {
        std::snprintf(buf, sizeof(buf), "%s: Off", Names::kRenderMargin);
    } else {
        std::snprintf(buf, sizeof(buf), "%s: %.1fx", Names::kRenderMargin, WiderFactor(level));
    }
    return buf;
}

uint32_t LevelForOldWiderView(std::optional<float> size) {
    if (!size || !std::isfinite(*size)) {
        return kMaxWiderLevel;
    }
    const float factor = std::clamp(0.78f * *size, kFactorMin, kFactorMax);
    return static_cast<uint32_t>(std::lround((factor - kFactorMin) / 0.1f));
}

LensOffset ClampedOffset(LensOffset o) {
    const auto axis = [](float deg) {
        if (!std::isfinite(deg)) {
            return 0.0f;
        }
        const float steps = std::round(std::clamp(deg, -kMaxOffsetDeg, kMaxOffsetDeg) /
                                       kOffsetStepDeg);
        return steps == 0.0f ? 0.0f : steps * kOffsetStepDeg; // never -0
    };
    return {axis(o.right_deg), axis(o.up_deg)};
}

LensOffset NudgedOffset(LensOffset o, int right_steps, int up_steps) {
    o = ClampedOffset(o);
    o.right_deg += static_cast<float>(right_steps) * kOffsetStepDeg;
    o.up_deg += static_cast<float>(up_steps) * kOffsetStepDeg;
    return ClampedOffset(o);
}

std::string OffsetToast(const LensOffset& o_in, bool horizontal) {
    const LensOffset o = ClampedOffset(o_in);
    const float v = horizontal ? o.right_deg : o.up_deg;
    if (v == 0.0f) {
        return "Offset Centred";
    }
    const char* dir = horizontal ? (v > 0.0f ? "Right" : "Left") : (v > 0.0f ? "Up" : "Down");
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Offset %s %.1f\xC2\xB0", dir, std::abs(v));
    return buf;
}

HeadLocked SolveHeadLocked(const Placement& placement, float game_fov_y_deg, float aspect,
                           const ViewFov& view, float eye_shift, const Options& options,
                           const LensOffset& offset_in) {
    const Placement p = Clamped(placement);
    // AzaharFrame kiu: negative too (Horizontal or Vertical Scale above 0: a larger screen).
    const float squeeze =
        std::isfinite(options.squeeze) ? std::clamp(options.squeeze, -0.5f, 0.5f) : 0.0f;
    const float squeeze_y =
        std::isfinite(options.squeeze_y) ? std::clamp(options.squeeze_y, -0.5f, 0.5f) : 0.0f;
    HeadLocked s;
    s.distance = p.distance;
    s.pitch_rad = PlacementPitch(p, view);
    // The game draws its own frame times the Render Margin factor across. Up and down it draws as
    // much more as the screen's shape needs (factor_y below).
    s.factor = std::isfinite(options.wider) ? std::clamp(options.wider, kFactorMin, kFactorMax)
                                            : kFactorMin;
    // AzaharFrame immersive2: the screen is sized with the settled camera (`game_fov_y_deg`) at
    // the world's own scale: as wide as the game draws (gain 1).
    const float t = GameTan(game_fov_y_deg);
    const float a = std::isfinite(aspect) && aspect > 0.1f ? aspect : 5.0f / 3.0f;
    // What fills the view, fitted at its middle, so moving the screen up or down never resizes it.
    const Cover cover = DefaultCover(view, ViewMiddlePitch(view), eye_shift);
    // AzaharFrame kiu2: sized with `size_wider` when given (the menu rule keeps the padded size).
    const float size_factor =
        std::isfinite(options.size_wider) && options.size_wider > 0.0f
            ? std::clamp(options.size_wider, kFactorMin, kFactorMax)
            : s.factor;
    const float drawn_over_sized = s.factor / size_factor;
    float qx = size_factor * a * t;
    // AzaharFrame oot: the game's own shape. Render Margin widens the picture by the same factor
    // both ways (Off: exactly the game's frame). The screen used to take the view's shape, so
    // the game drew 1.25 times its height on the Frame even at Off (a 4:3 picture from a 5:3
    // game), and full-screen 2D (a 2D vertex at the edge keeps its place) was stretched up and
    // down against the HUD drawn on it.
    float qy = qx / a;
    // Never bigger than filling the view (a very wide game camera): smaller, gain above 1.
    const float fit = std::min({1.0f, cover.tan_x / qx, cover.tan_y / qy});
    qx *= fit * p.size;
    qy *= fit * p.size;
    // The game draws factor_y times its own height into the screen's height, so the picture
    // has the same scale both ways: factor x a / qx = factor_y / qy.
    s.factor_y = s.factor * a * qy / qx;
    // Horizontal Scale and Vertical Scale (AzaharFrame kiu): the quad is that much narrower
    // (lower) than the picture below 0 %, wider (taller) above, the padding above still even.
    s.tan_half_y = qy * (1.0f - squeeze_y);
    s.aspect = qx * (1.0f - squeeze) / s.tan_half_y;
    // The gain follows the camera right now (a zoom): the game's half height (tan) at that
    // factor over the screen's, 1 = the same angle; each axis over its own narrowing. The screen
    // keeps its size when a gain hits a limit (the world then drifts a little).
    const float live = std::isfinite(options.live_fov_y_deg) && options.live_fov_y_deg > 0.0f
                           ? GameTan(options.live_fov_y_deg)
                           : t;
    // AzaharFrame kiu2: over the height the drawn factor would have been sized with, so a screen
    // kept at a larger size (size_wider) turns the game as the smaller one did.
    const float base = s.factor_y * live / (qy * drawn_over_sized);
    s.gain = std::clamp(base / (1.0f - squeeze_y), kMinGain, kMaxGain);
    s.gain_x = std::clamp(base / (1.0f - squeeze), kMinGain, kMaxGain);
    // The Immersive Offset last: it turns the screen fitted above, never resizes it.
    const LensOffset offset = ClampedOffset(offset_in);
    s.pitch_rad += offset.up_deg * kDegToRad;
    s.yaw_rad = -offset.right_deg * kDegToRad;
    return s;
}

Quatf PitchRotation(float pitch_rad) {
    return {std::sin(pitch_rad / 2.0f), 0.0f, 0.0f, std::cos(pitch_rad / 2.0f)};
}

Quatf ScaleRotation(const Quatf& q_in, float gain) {
    if (gain == 1.0f) {
        return q_in;
    }
    Quatf q = Input::Normalized(q_in);
    if (q.w < 0.0f) {
        q = {-q.x, -q.y, -q.z, -q.w};
    }
    const float s = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
    if (s < 1e-7f) {
        return Quatf{};
    }
    const float angle = 2.0f * std::atan2(s, q.w) * gain;
    const float k = std::sin(angle / 2.0f) / s;
    return {q.x * k, q.y * k, q.z * k, std::cos(angle / 2.0f)};
}

Quatf ScaleYawPitch(const Quatf& q_in, float yaw_gain, float pitch_gain) {
    if (yaw_gain == 1.0f && pitch_gain == 1.0f) {
        return q_in;
    }
    const Quatf q = Input::Normalized(q_in);
    // q = yaw x pitch x roll: the forward axis gives yaw and pitch, what is left is the roll.
    const Vec3f f = Input::Rotate(q, Vec3f{0.0f, 0.0f, -1.0f});
    const float yaw = std::atan2(-f.x, -f.z);
    const float pitch = std::atan2(f.y, std::hypot(f.x, f.z));
    const auto about_y = [](float a) {
        return Quatf{0.0f, std::sin(a / 2.0f), 0.0f, std::cos(a / 2.0f)};
    };
    const Quatf aim = Input::Multiply(about_y(yaw), PitchRotation(pitch));
    const Quatf roll = Input::Normalized(Input::Multiply(Input::Conjugate(aim), q));
    return Input::Normalized(Input::Multiply(
        Input::Multiply(about_y(yaw * yaw_gain), PitchRotation(pitch * pitch_gain)), roll));
}

namespace {
Quatf YawPitch(float yaw_rad, float pitch_rad) {
    const Quatf yaw{0.0f, std::sin(yaw_rad / 2.0f), 0.0f, std::cos(yaw_rad / 2.0f)};
    return Input::Normalized(Input::Multiply(yaw, PitchRotation(pitch_rad)));
}
} // namespace

Quatf QuadRotation(const HeadLocked& solved) {
    return YawPitch(solved.yaw_rad, solved.pitch_rad);
}

Quatf GameCameraRotation(const Quatf& head_rel, const HeadLocked& solved) {
    return Input::Normalized(Input::Multiply(ScaleYawPitch(head_rel, solved.gain_x, solved.gain),
                                             QuadRotation(solved)));
}

Posef HeadLockedQuadPose(const HeadLocked& solved, float eye_x) {
    const Quatf q = QuadRotation(solved);
    return {q, Vec3f{eye_x, 0.0f, 0.0f} + Input::Rotate(q, Vec3f{0.0f, 0.0f, -solved.distance})};
}

float EyeX(int eye, float half_ipd) {
    return eye == 0 ? -half_ipd : half_ipd;
}

float SharedQuadEyeShift(uint32_t mode, const Placement& placement, float half_ipd) {
    return mode == 2 ? half_ipd / Clamped(placement).distance : 0.0f;
}

StereoFar StereoFarOnQuad(const HeadLocked& solved, uint32_t mode, float half_ipd,
                          float far_width, float quad_half) {
    StereoFar out;
    const float d = std::max(solved.distance, 1e-3f);
    const float factor = std::max(solved.factor, 1e-3f);
    // The game's share, on the quad; mode 3's quads are each on their eye, IPD apart already.
    const float game_m = far_width / factor * solved.width();
    const float quads_m = mode == 3 ? 2.0f * (std::isnan(quad_half) ? half_ipd : quad_half) : 0.0f;
    out.separation_m = game_m + quads_m;
    out.far_deg = 2.0f * std::atan(out.separation_m / (2.0f * d)) * kRadToDeg;
    out.allow_deg = 2.0f * std::atan(half_ipd / d) * kRadToDeg;
    out.vergence_deg = 2.0f * std::atan((half_ipd - out.separation_m / 2.0f) / d) * kRadToDeg;
    out.diverges = out.vergence_deg < -kDivergeToleranceDeg;
    if (game_m > 1e-6f) {
        out.natural_scale = (2.0f * half_ipd - quads_m) / game_m;
    }
    return out;
}

float StereoSliderLimit(const HeadLocked& solved, float half_ipd, float far_per_slider,
                        float margin_deg) {
    if (!(far_per_slider > 0.0f) || !std::isfinite(far_per_slider)) {
        return -1.0f;
    }
    const float d = std::max(solved.distance, 1e-3f);
    // StereoFarOnQuad's vergence = margin: 2 atan((h - s / 2) / d) = margin.
    const float s_max = 2.0f * (half_ipd - d * std::tan(margin_deg * kDegToRad / 2.0f));
    if (s_max <= 0.0f) {
        return 0.0f;
    }
    const float width = solved.width();
    if (!(width > 0.0f)) {
        return -1.0f;
    }
    // s = far_per_slider x slider / factor x width.
    return s_max * std::max(solved.factor, 1e-3f) / width / far_per_slider;
}

float StereoCap::Update(float user, float limit, int64_t now_ms) {
    const float dt_s =
        last_ms_ != 0 ? std::clamp(static_cast<float>(now_ms - last_ms_) / 1000.0f, 0.0f, 0.1f)
                      : 0.0f;
    last_ms_ = now_ms;
    if (!std::isfinite(user) || user < 0.0f) {
        user = 0.0f;
    }
    if (limit >= 0.0f && std::isfinite(limit)) {
        const float lim = std::max(limit, kMinSlider);
        // Lower at once (never above the limit for long), higher only by more than a step.
        if (limit_ < 0.0f || lim < limit_ || lim > limit_ + kStep) {
            limit_ = lim;
        }
    }
    if (limit_ < 0.0f) {
        limited_ = false;
        last_user_ = user;
        return user;
    }
    const float target = std::min(user, limit_);
    if (out_ < 0.0f || user != last_user_) {
        // The first limit, or the user moved the slider: as it is (up to the limit), at once.
        out_ = target;
    } else if (out_ > target) {
        out_ = std::max(target, out_ - kDownPerS * dt_s);
    } else if (out_ < target) {
        out_ = std::min(target, out_ + kUpPerS * dt_s);
    }
    last_user_ = user;
    limited_ = limit_ < user;
    return std::min(out_, user);
}

float MenuExact::Update(bool on, int64_t now_ms) {
    const float dt_ms =
        last_ms_ != 0 ? std::clamp(static_cast<float>(now_ms - last_ms_), 0.0f, 100.0f) : 0.0f;
    last_ms_ = now_ms;
    const float step = dt_ms / kEaseMs;
    progress_ = std::clamp(progress_ + (on ? step : -step), 0.0f, 1.0f);
    return Weight();
}

float MenuExact::Weight() const {
    const float p = progress_;
    return p * p * (3.0f - 2.0f * p);
}

float GamePositionScale(int multiplier, uint32_t depth_percent) {
    return std::pow(10.0f, static_cast<float>(multiplier)) *
           static_cast<float>(depth_percent / 10);
}

uint32_t NaturalDepthPercent(bool depth_own, uint32_t depth, bool factor_3d_own,
                             uint32_t factor_3d) {
    if (!depth_own && factor_3d_own && factor_3d == 0) {
        return 0;
    }
    return std::min<uint32_t>(depth, 100);
}

float NaturalLeaning(const HeadLocked& solved, float eye_per_far) {
    if (!(eye_per_far > 0.0f) || !std::isfinite(eye_per_far)) {
        return -1.0f;
    }
    const float width = solved.width();
    if (!(width > 0.0f) || !std::isfinite(width)) {
        return -1.0f;
    }
    // The picture is 1 / factor of the quad's width: the far width share x that is the far
    // scenery apart in metres, and the eyes per that is game units per metre.
    return eye_per_far * std::max(solved.factor, 1e-3f) / width;
}

float EyePerFarFor(const HeadLocked& solved, float leaning) {
    if (!(leaning > 0.0f) || !std::isfinite(leaning)) {
        return -1.0f;
    }
    return leaning * solved.width() / std::max(solved.factor, 1e-3f);
}

float FullDistance(float percent, float size, bool follow) {
    const float s = std::isfinite(size) && size > 0.0f ? size : 1.0f;
    const float d = ClampWorldScale(percent) / 100.0f * kWorldRefDistance / (follow ? 1.0f : s);
    return std::clamp(d, kFullMinDistance, kFullMaxDistance);
}

float WorldScaleForDistance(float distance, float size, bool follow) {
    const float s = std::isfinite(size) && size > 0.0f ? size : 1.0f;
    return distance / kWorldRefDistance * (follow ? 1.0f : s) * 100.0f;
}

float MigrateWorldScaleMode3(const HeadLocked& solved, float eye_per_far, float leaning,
                             float size, bool follow, bool* exact) {
    if (exact) {
        *exact = false;
    }
    if (!(leaning > 0.0f) || !std::isfinite(leaning) || !(eye_per_far > 0.0f) ||
        !std::isfinite(eye_per_far) || !(solved.distance > 0.0f)) {
        return kWorldScaleAuto;
    }
    // The picture's width per metre of distance (its angular size): the distance at which K
    // over the picture's width is `leaning`.
    const float per_m = solved.width() / std::max(solved.factor, 1e-3f) / solved.distance;
    if (!(per_m > 0.0f) || !std::isfinite(per_m)) {
        return kWorldScaleAuto;
    }
    const float raw = WorldScaleForDistance(eye_per_far / (leaning * per_m), size, follow);
    const float out = ClampWorldScale(raw);
    if (exact) {
        // Within the range and at a distance FullDistance keeps (its own limits).
        *exact = std::isfinite(raw) && out == raw &&
                 std::abs(FullDistance(out, size, follow) / (eye_per_far / (leaning * per_m)) -
                          1.0f) < 1e-4f;
    }
    return out;
}

float MigrateWorldScaleMode2(float distance, float size, bool follow) {
    if (!(distance > 0.0f) || !std::isfinite(distance)) {
        return kWorldScaleAuto;
    }
    return ClampWorldScale(WorldScaleForDistance(distance, size, follow));
}

bool WorldScaleToMigrate(bool world_scale_own, bool world_set_own_on) {
    return !world_scale_own && !world_set_own_on;
}

float RatioRamp::Update(float target, int64_t now_ms, float ceiling) {
    const float dt_s =
        last_ms_ != 0 ? std::clamp(static_cast<float>(now_ms - last_ms_) / 1000.0f, 0.0f, 0.1f)
                      : 0.0f;
    last_ms_ = now_ms;
    if (!(target > 0.0f) || !std::isfinite(target)) {
        return value_;
    }
    if (value_ <= 0.0f) {
        value_ = eased_ = target;
        return value_;
    }
    // Eased, then rate limited in ratio: never a jump, whatever the target does.
    // AzaharFrame overshoot: and no higher than the ceiling (or than it is).
    const float top =
        std::isnan(ceiling) ? std::numeric_limits<float>::infinity() : std::max(value_, ceiling);
    eased_ += (target - eased_) * (1.0f - std::exp(-dt_s / kTauS));
    eased_ = std::min(eased_, top);
    const float step = std::exp(rate_ * dt_s);
    value_ = std::clamp(eased_, value_ / step, value_ * step);
    return value_;
}

float EyePerFarInUse::Update(float measured, int samples, float saved, float fallback,
                             int64_t now_ms) {
    const auto ok = [](float k) { return k > 0.0f && std::isfinite(k); };
    if (ok(measured) && samples >= kSaveAfter) {
        target_ = measured;
        source_ = kMeasured;
    } else if (source_ == kMeasured) {
        // Measured before, not now (a menu, 3D Depth 0): the last measure holds.
    } else if (ok(saved)) {
        target_ = saved;
        source_ = kSaved;
    } else if (ok(fallback)) {
        target_ = fallback;
        source_ = kFallback;
    }
    if (!ok(target_)) {
        return ramp_.Value();
    }
    return ramp_.Update(target_, now_ms);
}

float NaturalSlider::Update(float f, float limit, int64_t now_ms, bool no_higher) {
    const float dt_s =
        last_ms_ != 0 ? std::clamp(static_cast<float>(now_ms - last_ms_) / 1000.0f, 0.0f, 0.1f)
                      : 0.0f;
    last_ms_ = now_ms;
    if (!std::isfinite(f)) {
        f = 0.0f;
    }
    f = std::clamp(f, 0.0f, 1.0f);
    bool jump = f != last_f_;
    last_f_ = f;
    if (limit >= 0.0f && std::isfinite(limit)) {
        if (limit_ < 0.0f) {
            jump = true;
            limit_ = limit;
        } else if (limit < limit_ || limit > limit_ * (1.0f + kRise)) {
            limit_ = limit;
        }
    }
    const float target = limit_ >= 0.0f ? f * limit_ : f * kProbe;
    if (jump || out_ < 0.0f || target <= out_) {
        // Lower at once: the far scenery is never past parallel while the screen grows.
        out_ = target;
    } else if (no_higher) {
        // Held where it is until the game is measured again.
    } else {
        out_ += (target - out_) * (1.0f - std::exp(-dt_s / kUpTauS));
        if (target - out_ <= kSnap * target) {
            out_ = target;
        }
    }
    clamped_ = out_ > 1.0f;
    return std::min(out_, 1.0f);
}

float FarMeasureHold::Update(float measure, uint32_t taken, int64_t now_ms) {
    // The hold's own clock runs only while the game is being measured: while the slider moves
    // (the measure waits for it, FarPerSlider) or the game shows no far scenery, nothing ages,
    // so what was seen before still counts when the measure comes back.
    const int64_t dt = last_ms_ != 0 ? std::clamp<int64_t>(now_ms - last_ms_, 0, kBucketMs) : 0;
    last_ms_ = now_ms;
    if (taken != taken_ || held_ < 0.0f) {
        taken_ = taken;
        fresh_ms_ = now_ms;
    }
    if (now_ms - fresh_ms_ > kBucketMs || !(measure > 0.0f) || !std::isfinite(measure)) {
        return held_;
    }
    clock_ms_ += dt;
    const int64_t now = clock_ms_ / kBucketMs;
    {
        Bucket& b = buckets_[static_cast<std::size_t>(now % kRing)];
        if (b.index != now) {
            b = {now, measure, measure};
        } else {
            b.lo = std::min(b.lo, measure);
            b.hi = std::max(b.hi, measure);
        }
    }
    const auto at = [&](int64_t index) -> const Bucket* {
        if (index < 0) {
            return nullptr;
        }
        const Bucket& b = buckets_[static_cast<std::size_t>(index % kRing)];
        return b.index == index ? &b : nullptr;
    };
    // The highest of the kHoldBuckets ending at `last` (below 0: none).
    const auto top_to = [&](int64_t last) {
        float top = -1.0f;
        for (int64_t i = last - kHoldBuckets + 1; i <= last; ++i) {
            if (const Bucket* b = at(i)) {
                top = std::max(top, b->hi);
            }
        }
        return top;
    };
    // How far bucket `i` went above the highest of the hold window as it was kRiseBuckets
    // before: what the measure in use missed, the game being that far ahead of it.
    const auto beyond = [&](int64_t i) {
        const Bucket* b = at(i);
        const float before = top_to(i - kRiseBuckets);
        return b && before > 0.0f ? std::max(b->hi / before - 1.0f, 0.0f) : 0.0f;
    };
    if (now != noise_at_) {
        // A bucket done: the noise is the most any went beyond, fading by kNoiseTauS.
        if (noise_at_ >= 0) {
            const float fade = std::exp(-static_cast<float>(now - noise_at_) *
                                        static_cast<float>(kBucketMs) / (1000.0f * kNoiseTauS));
            noise_ = std::max(noise_ * fade, beyond(noise_at_));
        }
        noise_at_ = now;
    }
    const float top = top_to(now);
    if (top <= 0.0f) {
        return held_; // nothing measured lately: the last in use holds
    }
    // The measure's own spread over the hold window: a lower one counts once it is lower by
    // more than half that (and than NaturalSlider::kRise).
    float low = top;
    for (int64_t i = now - kHoldBuckets + 1; i <= now; ++i) {
        if (const Bucket* b = at(i)) {
            low = std::min(low, b->lo);
        }
    }
    const float margin = std::clamp(std::max(noise_, beyond(now)), kMarginFloor, kMarginMax);
    const float want = top * (1.0f + margin);
    const float band = std::max(NaturalSlider::kRise, 0.5f * (1.0f - low / top));
    // Higher once it has eaten into half the margin in use (the other half still covers what
    // the game may be ahead), at once.
    if (held_ < 0.0f || want > held_ * (1.0f + 0.5f * margin_) || want < held_ * (1.0f - band)) {
        held_ = want;
        margin_ = margin;
    }
    return held_;
}

float ParallelDistance(const HeadLocked& solved, float half_ipd, float far_width) {
    // The far scenery apart on the quad: far_width / factor x the quad's width (2 d tan_y
    // aspect), parallel at 2 h.
    const float per_m =
        far_width / std::max(solved.factor, 1e-3f) * 2.0f * solved.tan_half_y * solved.aspect;
    if (!(per_m > 0.0f) || !std::isfinite(per_m)) {
        return std::numeric_limits<float>::infinity();
    }
    return 2.0f * half_ipd / per_m;
}

Mode3Stereo Mode3Natural(float f, float leaning, float half_ipd, float game_far_m) {
    Mode3Stereo out;
    f = std::clamp(std::isfinite(f) ? f : 0.0f, 0.0f, 1.0f);
    out.eye_scaler = f * (std::isfinite(leaning) ? std::max(leaning, 0.0f) : 0.0f);
    const float game = std::isfinite(game_far_m) ? std::max(game_far_m, 0.0f) : 0.0f;
    out.quad_half = half_ipd - game / 2.0f;
    return out;
}

float GameFarOnQuad(const HeadLocked& solved, float far_width) {
    if (!(far_width > 0.0f) || !std::isfinite(far_width)) {
        return 0.0f;
    }
    return far_width / std::max(solved.factor, 1e-3f) * solved.width();
}

Posef FullQuadInView(const HeadLocked& solved, uint32_t mode, int eye, float half_ipd,
                     float quad_half) {
    return HeadLockedQuadPose(
        solved, mode == 3 ? EyeX(eye, std::isnan(quad_half) ? half_ipd : quad_half) : 0.0f);
}

Posef AnchoredQuadPose(const Posef& picture, const Posef& now, const Posef& in_view,
                       float eye_x, bool follow_eye) {
    if (!follow_eye) {
        return Compose(picture, in_view);
    }
    // The eye now, and the quad from it as it was from the eye in the picture's head frame.
    const Vec3f eye{eye_x, 0.0f, 0.0f};
    const Vec3f eye_now = now.position + Input::Rotate(now.orientation, eye);
    return {Input::Normalized(Input::Multiply(picture.orientation, in_view.orientation)),
            eye_now + Input::Rotate(picture.orientation, in_view.position - eye)};
}

Posef AnchoredQuadPose(const Posef& picture, const Posef& now, const Posef& in_view, float eye_x,
                       float follow) {
    const Vec3f eye{eye_x, 0.0f, 0.0f};
    const Vec3f eye_then = picture.position + Input::Rotate(picture.orientation, eye);
    const Vec3f eye_now = now.position + Input::Rotate(now.orientation, eye);
    const float f = std::isfinite(follow) ? follow : 1.0f;
    return {Input::Normalized(Input::Multiply(picture.orientation, in_view.orientation)),
            eye_then + (eye_now - eye_then) * f +
                Input::Rotate(picture.orientation, in_view.position - eye)};
}

float AnchorFollow(float distance, float scaler, float depth_units, float gain) {
    if (!(distance > 0.0f) || !(std::abs(scaler) > 0.0f) || !(depth_units > 0.0f) ||
        !(gain > 0.0f) || !std::isfinite(distance * scaler * depth_units * gain)) {
        return 1.0f;
    }
    const float parallax = std::abs(scaler) / depth_units / gain;
    return std::clamp(1.0f - parallax * distance, -20.0f, 1.0f);
}

Posef AnchoredFullQuad(const HeadLocked& solved, uint32_t mode, int eye, float half_ipd,
                       const Posef& picture, const Posef& now, bool follow_eye, float quad_half) {
    return AnchoredQuadPose(picture, now, FullQuadInView(solved, mode, eye, half_ipd, quad_half),
                            EyeX(eye, half_ipd), follow_eye);
}

namespace {
// AzaharFrame heldquad: HeldQuad's sums in double, so gain 1 matches AnchoredQuadPose to well
// under a ten-thousandth of a degree.
struct D3 {
    double x = 0, y = 0, z = 0;
};
D3 operator+(const D3& a, const D3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
D3 operator-(const D3& a, const D3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
D3 operator*(const D3& a, double k) {
    return {a.x * k, a.y * k, a.z * k};
}
double Dot(const D3& a, const D3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
D3 Cross(const D3& a, const D3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double Len(const D3& a) {
    return std::sqrt(Dot(a, a));
}
struct D4 {
    double x = 0, y = 0, z = 0, w = 1;
};
D4 ToD(const Quatf& q) {
    const double n = std::sqrt(double(q.x) * q.x + double(q.y) * q.y + double(q.z) * q.z +
                               double(q.w) * q.w);
    return n > 0 ? D4{q.x / n, q.y / n, q.z / n, q.w / n} : D4{};
}
D4 Mul(const D4& a, const D4& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
D4 Conj(const D4& q) {
    return {-q.x, -q.y, -q.z, q.w};
}
// The rotation's matrix, m[row][column].
void Matrix(const D4& q, double m[3][3]) {
    const double x = q.x, y = q.y, z = q.z, w = q.w;
    m[0][0] = 1 - 2 * (y * y + z * z);
    m[0][1] = 2 * (x * y - z * w);
    m[0][2] = 2 * (x * z + y * w);
    m[1][0] = 2 * (x * y + z * w);
    m[1][1] = 1 - 2 * (x * x + z * z);
    m[1][2] = 2 * (y * z - x * w);
    m[2][0] = 2 * (x * z - y * w);
    m[2][1] = 2 * (y * z + x * w);
    m[2][2] = 1 - 2 * (x * x + y * y);
}
D3 Apply(const double m[3][3], const D3& v) {
    return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
}
D3 ApplyT(const double m[3][3], const D3& v) {
    return {m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
            m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
            m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z};
}
// The rotation whose matrix has the columns x, y, z (orthonormal, right-handed).
D4 FromAxes(const D3& x, const D3& y, const D3& z) {
    const double tr = x.x + y.y + z.z;
    D4 q;
    if (tr > 0) {
        const double s = std::sqrt(tr + 1.0) * 2;
        q = {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25 * s};
    } else if (x.x > y.y && x.x > z.z) {
        const double s = std::sqrt(1.0 + x.x - y.y - z.z) * 2;
        q = {0.25 * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s};
    } else if (y.y > z.z) {
        const double s = std::sqrt(1.0 + y.y - x.x - z.z) * 2;
        q = {(y.x + x.y) / s, 0.25 * s, (z.y + y.z) / s, (z.x - x.z) / s};
    } else {
        const double s = std::sqrt(1.0 + z.z - x.x - y.y) * 2;
        q = {(z.x + x.z) / s, (z.y + y.z) / s, 0.25 * s, (x.y - y.x) / s};
    }
    const double n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}
} // namespace

HeldQuadPose HeldQuad(const Posef& in_view, float eye_x, float distance, float eye_share,
                      const Quatf& q_cam_then, const Quatf& q_cam_now, float gain_x,
                      float gain_y) {
    HeldQuadPose out;
    out.pose = in_view;
    const double gx = std::isfinite(gain_x) && gain_x > 0.0f ? gain_x : 1.0;
    const double gy = std::isfinite(gain_y) && gain_y > 0.0f ? gain_y : 1.0;
    const double d = std::isfinite(distance) && distance > 1e-3f ? distance : 1e-3;
    const double share = std::isfinite(eye_share) ? eye_share : 1.0;
    // R, the game camera's turn since the picture, in the quad's frame; A = M R M^-1.
    double r[3][3];
    Matrix(Mul(Conj(ToD(q_cam_now)), ToD(q_cam_then)), r);
    const double m[3] = {1.0 / gx, 1.0 / gy, 1.0};
    double a[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            a[i][j] = m[i] * r[i][j] / m[j];
        }
    }
    // In the quad's own frame: the quad's axis (its centre `d` behind it), the eye, and the
    // point the picture's rays come from, `eye_share` of the way from the axis to the eye.
    const D4 qv = ToD(in_view.orientation);
    double b[3][3]; // the quad's axes in the head's frame (columns)
    Matrix(qv, b);
    const D3 eye_h{eye_x, 0.0, 0.0};
    const D3 centre_h{in_view.position.x, in_view.position.y, in_view.position.z};
    const D3 c0 = ApplyT(b, centre_h - eye_h);       // the centre from the eye
    const D3 axis_e = c0 + D3{0.0, 0.0, d};          // the axis from the eye
    const D3 from_e = axis_e * (1.0 - share);        // the rays' point from the eye
    const D3 centre_from = c0 - from_e;              // the centre from the rays' point
    // A point s of the picture (from the rays' point) shows the game ray M^-1 s; the camera
    // turned, the fresh picture shows that ray along t = A s from the rays' point, on the quad's
    // plane (`h` in front of that point) at from_e + t h / t_z from the eye. Times t_z / h (> 0):
    // G s = A s + from_e t_z / h, linear in s, so the held quad is the parallelogram G makes of
    // the picture: its centre G(centre_from), its edges G of the picture's axes.
    const double h = centre_from.z;
    if (!(h < -1e-6)) {
        return out;
    }
    const auto g = [&](const D3& v) {
        const D3 t = Apply(a, v);
        return t + from_e * (t.z / h);
    };
    const D3 g3 = g(centre_from);
    const D3 g1 = g(D3{1, 0, 0});
    const D3 g2 = g(D3{0, 1, 0});
    const double l1 = Len(g1), l2 = Len(g2), l3 = Len(g3);
    if (!(l1 > 1e-12) || !(l2 > 1e-12) || !(l3 > 1e-12) || !std::isfinite(l1 + l2 + l3)) {
        return out;
    }
    // The rectangle nearest the parallelogram: in its plane, the pair of right angles symmetric
    // about its edges (the skew split evenly between them; corners, which see both, hold best).
    const D3 n1 = g1 * (1.0 / l1), n2 = g2 * (1.0 / l2);
    D3 sum = n1 + n2, dif = n1 - n2;
    const double ls = Len(sum), ld = Len(dif);
    if (!(ls > 1e-12) || !(ld > 1e-12)) {
        return out;
    }
    sum = sum * (1.0 / ls);
    dif = dif * (1.0 / ld);
    D3 x = sum + dif, y = sum - dif;
    x = x * (1.0 / Len(x));
    y = y * (1.0 / Len(y));
    const D3 z = Cross(x, y);
    // Back to the quad's distance from the eye, about the eye.
    const double k = Len(c0) / l3;
    const D3 centre = eye_h + Apply(b, g3 * k);
    const D4 q = Mul(qv, FromAxes(x, y, z));
    out.pose.orientation = {static_cast<float>(q.x), static_cast<float>(q.y),
                            static_cast<float>(q.z), static_cast<float>(q.w)};
    out.pose.position = {static_cast<float>(centre.x), static_cast<float>(centre.y),
                         static_cast<float>(centre.z)};
    out.width_scale = static_cast<float>(Dot(g1, x) * k);
    out.height_scale = static_cast<float>(Dot(g2, y) * k);
    return out;
}

void PoseRing::Keep(uint32_t mode, const Posef& rest) {
    const auto same = [](const Posef& a, const Posef& b) {
        return a.orientation.x == b.orientation.x && a.orientation.y == b.orientation.y &&
               a.orientation.z == b.orientation.z && a.orientation.w == b.orientation.w &&
               a.position.x == b.position.x && a.position.y == b.position.y &&
               a.position.z == b.position.z;
    };
    if (kept_ && (mode != mode_ || !same(rest, rest_))) {
        Clear();
    }
    kept_ = true;
    mode_ = mode;
    rest_ = rest;
}

float EyeShareOnQuad(const HeadLocked& solved, uint32_t mode, float half_ipd, float far_width) {
    if (mode != 2) {
        return 1.0f;
    }
    if (!(far_width > 0.0f) || !(half_ipd > 0.0f)) {
        return 0.0f;
    }
    const float share = StereoFarOnQuad(solved, mode, half_ipd, far_width).separation_m /
                        (2.0f * half_ipd);
    return std::isfinite(share) ? std::clamp(share, 0.0f, 2.0f) : 0.0f;
}

HeldScreen HeldFullQuad(const HeadLocked& solved, uint32_t mode, int eye, float half_ipd,
                        const Posef& picture, const Posef& now, float follow,
                        const HeldPose* held, const Quatf& rest, float eye_share,
                        float quad_half) {
    const Posef in_view = FullQuadInView(solved, mode, eye, half_ipd, quad_half);
    const float eye_x = EyeX(eye, half_ipd);
    HeldScreen out{AnchoredQuadPose(picture, now, in_view, eye_x, follow), solved.width(),
                   solved.height()};
    if (!held) {
        return out;
    }
    const Quatf head_rel =
        Input::Normalized(Input::Multiply(Input::Conjugate(rest), now.orientation));
    const Quatf cam_now = GameCameraRotation(head_rel, held->solved);
    const HeldQuadPose h = HeldQuad(in_view, eye_x, solved.distance, eye_share, held->q_cam,
                                    cam_now, held->solved.gain_x, held->solved.gain);
    out.pose = Compose(now, h.pose);
    // The eye's move since the picture followed by `follow` (1: none of it undone).
    const Vec3f e{eye_x, 0.0f, 0.0f};
    const Vec3f eye_then = picture.position + Input::Rotate(picture.orientation, e);
    const Vec3f eye_now = now.position + Input::Rotate(now.orientation, e);
    const float f = std::isfinite(follow) ? follow : 1.0f;
    out.pose.position = out.pose.position + (eye_then - eye_now) * (1.0f - f);
    out.width = solved.width() * h.width_scale;
    out.height = solved.height() * h.height_scale;
    return out;
}

void PoseRing::Push(const HeldPose& p) {
    poses_[next_] = p;
    next_ = (next_ + 1) % kSize;
    count_ = std::min(count_ + 1, kSize);
}

const HeldPose* PoseRing::Find(int64_t time) const {
    for (std::size_t i = 0; i < count_; i++) {
        const HeldPose& p = poses_[(next_ + kSize - 1 - i) % kSize];
        if (p.time == time) {
            return &p;
        }
    }
    return nullptr;
}

const HeldPose* PoseRing::Nearest(int64_t time) const {
    const HeldPose* best = nullptr;
    int64_t best_d = 0;
    for (std::size_t i = 0; i < count_; i++) {
        const HeldPose& p = poses_[(next_ + kSize - 1 - i) % kSize];
        const int64_t d = p.time > time ? p.time - time : time - p.time;
        if (!best || d < best_d) {
            best = &p;
            best_d = d;
        }
    }
    return best;
}

Input::Vec2f FarSceneryError(const Posef& picture, const Posef& in_view, float eye_x,
                             const Posef& placed, const Vec3f& eye_now) {
    // Both directions in the frame of the quad as the picture placed it.
    const Quatf frame = Input::Normalized(Input::Multiply(picture.orientation, in_view.orientation));
    const Vec3f now = Input::Rotate(Input::Conjugate(frame), placed.position - eye_now);
    const Vec3f drawn = Input::Rotate(Input::Conjugate(in_view.orientation),
                                      in_view.position - Vec3f{eye_x, 0.0f, 0.0f});
    return {std::atan2(now.x, -now.z) - std::atan2(drawn.x, -drawn.z),
            std::atan2(now.y, -now.z) - std::atan2(drawn.y, -drawn.z)};
}

Posef WideQuadPose(const Posef& rest, float pitch_rad, float distance, float yaw_rad) {
    const Quatf q = YawPitch(yaw_rad, pitch_rad);
    return Compose(rest, {q, Input::Rotate(q, Vec3f{0.0f, 0.0f, -distance})});
}

BottomQuad WideBottomQuad(const Posef& rest, float wide_pitch_rad, float wide_distance,
                          float yaw_rad) {
    const float normal_distance = std::hypot(kNormalBottomY, kNormalBottomZ);
    const float below = std::atan2(kNormalBottomY, -kNormalBottomZ); // -26.6 degrees
    const float d = kWideBottomNearer * wide_distance;
    BottomQuad b;
    b.pose = WideQuadPose(rest, wide_pitch_rad + below, d, yaw_rad);
    b.width = kNormalBottomWidth * d / normal_distance;
    b.height = kNormalBottomHeight * d / normal_distance;
    return b;
}

float AimElevation(const Posef& head, const Posef& aim, float radius) {
    const Posef to_head = Input::Inverse(head);
    const Vec3f o = Input::Transform(to_head, aim.position);
    const Vec3f d = Input::Normalized(Input::Rotate(
        Input::Conjugate(head.orientation), Input::Rotate(aim.orientation, {0.0f, 0.0f, -1.0f})));
    Vec3f p = d;
    const float b = Input::Dot(o, d);
    const float c = Input::Dot(o, o) - radius * radius;
    const float disc = b * b - c;
    if (disc >= 0.0f) {
        const float t = -b + std::sqrt(disc);
        if (t > 0.0f) {
            p = o + d * t;
        }
    }
    return std::atan2(p.y, std::hypot(p.x, p.z));
}

ScreenDecision Decide(const ScreenState& s) {
    ScreenDecision d;
    const bool hidden_at_start = s.hide_bottom || s.mode != 0;
    d.show_bottom = s.bottom_focused || hidden_at_start == s.bottom_toggled;
    const bool head_locked = s.mode >= 2;
    d.immersive = s.mode != 0 && s.live && !s.bottom_focused &&
                  (!head_locked || (!d.show_bottom && !s.menu_open && s.view_valid));
    return d;
}

// ---- Adjuster ----

Adjuster::Result Adjuster::Update(const Input::InputFrame& in, const Posef& head, float dt,
                                  bool enabled, Placement& placement, float* world_ratio) {
    Result r;
    bool edge[2]{};
    for (int h = 0; h < 2; h++) {
        const float g = in.hand[static_cast<std::size_t>(h)].grip;
        const bool down = grip_down_[h] ? g > kGripRelease : g > kGripPress;
        edge[h] = down && !grip_down_[h];
        grip_down_[h] = down;
    }
    if (!enabled) {
        hand_ = -1;
        return r;
    }
    if (hand_ >= 0) {
        const Input::HandInput& hand = in.hand[static_cast<std::size_t>(hand_)];
        if (!grip_down_[hand_] || !hand.active) {
            hand_ = -1;
            r.released = true;
            return r;
        }
        const Placement before = placement;
        // The sphere of the distance at the grab: changing the distance does not move the screen.
        const float elevation = AimElevation(head, hand.aim, start_radius_);
        placement.pitch_deg = start_pitch_deg_ + (elevation - start_elevation_) / kDegToRad;
        const float dts = std::clamp(dt, 0.0f, 0.1f);
        if (std::abs(hand.stick.y) > kStickThreshold) {
            const float step = std::exp(hand.stick.y * kDistanceRate * dts);
            if (world_ratio) {
                *world_ratio *= step; // AzaharFrame natural: World Scale
            } else {
                placement.distance *= step;
            }
        }
        if (std::abs(hand.stick.x) > kStickThreshold) {
            placement.size *= std::exp(hand.stick.x * kSizeRate * dts);
        }
        placement = Clamped(placement);
        // The pitch keeps following the hand from where the clamp left it.
        if (placement.pitch_deg !=
            start_pitch_deg_ + (elevation - start_elevation_) / kDegToRad) {
            start_pitch_deg_ = placement.pitch_deg;
            start_elevation_ = elevation;
        }
        r.active = true;
        r.hand = hand_;
        r.changed = !(placement == before);
        return r;
    }
    for (const int h : {1, 0}) {
        const Input::HandInput& hand = in.hand[static_cast<std::size_t>(h)];
        if (edge[h] && hand.active) {
            hand_ = h;
            start_pitch_deg_ = placement.pitch_deg;
            start_radius_ = placement.distance;
            start_elevation_ = AimElevation(head, hand.aim, start_radius_);
            r.active = true;
            r.hand = h;
            return r;
        }
    }
    return r;
}

void Adjuster::Cancel() {
    hand_ = -1;
}

// ---- Store ----

namespace {
std::string Key(uint64_t title_id, bool wide) {
    // The key's kind is only an index; Format writes the kind of the line.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llX %s", static_cast<unsigned long long>(title_id),
                  wide ? "wide" : "head");
    return buf;
}
} // namespace

void Store::Parse(const std::string& text) {
    entries_.clear();
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line.resize(hash);
        }
        std::istringstream words(line);
        std::string id, kind;
        Placement p;
        if (!(words >> id >> kind >> p.pitch_deg >> p.distance >> p.size)) {
            continue;
        }
        if (id.size() != 16 ||
            id.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos ||
            (kind != "world" && kind != "screen" && kind != "view" && kind != "head" &&
             kind != "wide")) {
            continue;
        }
        const uint64_t title = std::strtoull(id.c_str(), nullptr, 16);
        // AzaharFrame immersive: "view" (1.2.0 test builds) and "head" (1.0, 1.1) sizes were
        // relative to smaller defaults; AzaharFrame immersive2: "screen" (1.2.0 test builds) to
        // the view-filling one.
        const bool old_size = kind == "head" || kind == "view" || kind == "screen";
        const std::string key = Key(title, kind == "wide");
        // A line of this build wins over an old one for the same game, whatever the order.
        if (const auto it = entries_.find(key);
            old_size && it != entries_.end() && !it->second.old_size) {
            continue;
        }
        // Old sizes were 0.6 to 1.5, inside today's range: clamping changes none of them.
        entries_[key] = Entry{Clamped(p), old_size};
    }
}

std::string Store::Format() const {
    std::string out = "# Azahar for Frame: immersive screen placement per game\n"
                      "# title id, world (modes 2 and 3) or wide (mode 1), pitch in degrees, "
                      "distance in metres, size\n";
    for (const auto& [key, e] : entries_) {
        std::string line = key;
        if (line.size() > 5 && line.compare(line.size() - 5, 5, " head") == 0) {
            // An old line not yet replaced keeps its kind ("view" for both: its size is not
            // used, so the older kind no longer matters).
            line.replace(line.size() - 4, 4, e.old_size ? "view" : "world");
        }
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s %.2f %.3f %.3f\n", line.c_str(),
                      e.placement.pitch_deg, e.placement.distance, e.placement.size);
        out += buf;
    }
    return out;
}

std::optional<Placement> Store::Get(uint64_t title_id, bool wide) const {
    const auto it = entries_.find(Key(title_id, wide));
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second.placement;
}

bool Store::OldSize(uint64_t title_id) const {
    const auto it = entries_.find(Key(title_id, false));
    return it != entries_.end() && it->second.old_size;
}

void Store::Set(uint64_t title_id, bool wide, const Placement& placement) {
    entries_[Key(title_id, wide)] = Entry{Clamped(placement), false};
}

void Store::Erase(uint64_t title_id, bool wide) {
    entries_.erase(Key(title_id, wide));
}

// AzaharFrame depth
CameraShape CameraInUse(bool match_camera, float measured_fov_deg, float measured_aspect,
                        float setting_fov_deg, float setting_aspect) {
    if (match_camera && measured_fov_deg > 0.0f && measured_aspect > 0.0f) {
        return {measured_fov_deg, measured_aspect};
    }
    return {setting_fov_deg, setting_aspect};
}

// AzaharFrame immersive
unsigned CameraFilter::Add(float tan_half_y, float aspect) {
    if (!std::isfinite(tan_half_y) || !std::isfinite(aspect) || tan_half_y <= 0.0f ||
        aspect <= 0.0f) {
        return None;
    }
    const float fov = std::clamp(2.0f * std::atan(tan_half_y) / kDegToRad, 30.0f, 110.0f);
    const auto same_fov = [](float a, float b) { return std::abs(a - b) < kSameFovDeg; };
    const auto same_aspect = [](float a, float b) { return std::abs(a - b) < kSameAspect * b; };
    unsigned change = None;

    // The live angle (the gain).
    if (live_fov_ > 0.0f && same_fov(fov, live_fov_)) {
        live_count_ = 0;
    } else {
        live_count_ = live_count_ > 0 && same_fov(fov, live_pending_) ? live_count_ + 1 : 1;
        if (live_count_ == 1) {
            live_pending_ = fov;
        }
        if (live_count_ >= 2) {
            live_fov_ = fov;
            live_count_ = 0;
            change |= Live;
        }
    }

    if (screen_fov_ <= 0.0f) {
        // The first: the screen takes it with the live angle.
        if (change & Live) {
            screen_fov_ = fov;
            screen_aspect_ = aspect;
            change |= Screen;
        }
        return change;
    }
    // The screen's angle: only one held for kSettleCount measurements.
    if (same_fov(fov, screen_fov_)) {
        settle_count_ = 0;
    } else {
        settle_count_ = settle_count_ > 0 && same_fov(fov, settle_fov_) ? settle_count_ + 1 : 1;
        if (settle_count_ == 1) {
            settle_fov_ = fov;
        }
        if (settle_count_ >= kSettleCount) {
            screen_fov_ = fov;
            settle_count_ = 0;
            change |= Screen;
        }
    }
    // The screen's shape: only at the screen's vertical angle (a zoom never reshapes it).
    if (same_fov(fov, screen_fov_) && !same_aspect(aspect, screen_aspect_)) {
        shape_count_ = shape_count_ > 0 && same_aspect(aspect, shape_pending_) ? shape_count_ + 1 : 1;
        if (shape_count_ == 1) {
            shape_pending_ = aspect;
        }
        if (shape_count_ >= 2) {
            screen_aspect_ = aspect;
            shape_count_ = 0;
            change |= Screen;
        }
    } else {
        shape_count_ = 0;
    }
    return change;
}

// AzaharFrame fovonce
void FovEstimate::Add(int64_t t_ms, float fov_deg) {
    if (state_ != State::Measuring || !std::isfinite(fov_deg)) {
        return;
    }
    // Outside the setting's range (a close-up in a cutscene): no Game FOV can hold it, and it
    // ends the run.
    if (fov_deg < kMinDeg - 0.5f || fov_deg >= kMaxDeg + 0.5f) {
        in_run_ = false;
        pending_.clear();
        return;
    }
    const bool continues = in_run_ && t_ms >= last_ms_ && t_ms - last_ms_ <= kGapMs &&
                           std::abs(fov_deg - last_deg_) <= kStepDeg;
    int64_t ms = kSampleMs;
    if (continues) {
        ms = std::clamp<int64_t>(t_ms - last_ms_, 1, kSampleMs);
    } else {
        in_run_ = true;
        run_counts_ = false;
        run_start_ms_ = t_ms;
        pending_.clear();
    }
    last_ms_ = t_ms;
    last_deg_ = fov_deg;
    if (run_counts_) {
        Count(fov_deg, ms);
    } else {
        pending_.emplace_back(fov_deg, ms);
        if (t_ms - run_start_ms_ + kSampleMs >= kHoldMs) {
            run_counts_ = true;
            for (const auto& [deg, held] : pending_) {
                Count(deg, held);
            }
            pending_.clear();
        }
    }
    Evaluate();
}

void FovEstimate::Count(float fov_deg, int64_t ms) {
    const int bin = std::clamp(static_cast<int>(std::lround(fov_deg)) - kMinDeg, 0,
                               static_cast<int>(bin_ms_.size()) - 1);
    bin_ms_[static_cast<std::size_t>(bin)] += ms;
    bin_sum_[static_cast<std::size_t>(bin)] += static_cast<double>(fov_deg) * ms;
    counted_ms_ += ms;
}

void FovEstimate::Evaluate() {
    if (counted_ms_ < kEnoughMs) {
        return;
    }
    // The 3-degree band (a whole degree and its two neighbours) that held the most time.
    const int n = static_cast<int>(bin_ms_.size());
    int best = 0;
    int64_t best_ms = -1;
    for (int i = 0; i < n; i++) {
        int64_t ms = 0;
        for (int j = std::max(i - 1, 0); j <= std::min(i + 1, n - 1); j++) {
            ms += bin_ms_[static_cast<std::size_t>(j)];
        }
        if (ms > best_ms) {
            best_ms = ms;
            best = i;
        }
    }
    share_ = static_cast<float>(best_ms) / static_cast<float>(counted_ms_);
    if (share_ < kShareEarly && !(counted_ms_ >= kPlentyMs && share_ >= kShareLate)) {
        return;
    }
    double sum = 0.0;
    int64_t ms = 0;
    for (int j = std::max(best - 1, 0); j <= std::min(best + 1, n - 1); j++) {
        sum += bin_sum_[static_cast<std::size_t>(j)];
        ms += bin_ms_[static_cast<std::size_t>(j)];
    }
    value_ = std::clamp(static_cast<int>(std::lround(sum / static_cast<double>(ms))), kMinDeg,
                        kMaxDeg);
    state_ = State::Done;
}

void FovEstimate::Tick(int64_t dt_ms) {
    measuring_ms_ += std::max<int64_t>(dt_ms, 0);
    if (state_ == State::Measuring && measuring_ms_ >= kGiveUpMs) {
        state_ = State::Failed;
    }
}

// AzaharFrame fovonce
void FovFollow::Add(int64_t t_ms, float fov_deg) {
    if (!std::isfinite(fov_deg) || fov_deg <= 0.0f) {
        return;
    }
    window_.emplace_back(t_ms, fov_deg);
    if (window_.size() > kCount) {
        window_.erase(window_.begin(), window_.end() - kCount);
    }
    std::vector<float> v;
    v.reserve(window_.size());
    for (const auto& m : window_) {
        v.push_back(m.second);
    }
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n % 2 == 1 || target_ <= 0.0f) {
        target_ = v[n / 2 - (n % 2 == 0 ? 1 : 0)];
        return;
    }
    const float a = v[n / 2 - 1];
    const float b = v[n / 2];
    target_ = std::abs(a - target_) <= std::abs(b - target_) ? a : b;
}

// AzaharFrame fovonce: a critically damped spring, solved exactly over each step (any frame
// time): the offset e and the speed v after dt are (e + (v + w e) dt) exp(-w dt) and
// (v - w (v + w e) dt) exp(-w dt). w = 4.74 / kEaseMs leaves 5 % of a step after kEaseMs.
float FovEase::Update(float target, int64_t now_ms) {
    if (!std::isfinite(target)) {
        return value_;
    }
    if (!have_) {
        have_ = true;
        value_ = target;
        speed_ = 0.0f;
        last_ms_ = now_ms;
        moving_ = false;
        return value_;
    }
    const float dt = static_cast<float>(std::max<int64_t>(now_ms - last_ms_, 0)) / 1000.0f;
    last_ms_ = now_ms;
    constexpr float w = 4.74f * 1000.0f / static_cast<float>(kEaseMs);
    const float e = value_ - target;
    const float k = std::exp(-w * dt);
    const float c = speed_ + w * e;
    const float e1 = (e + c * dt) * k;
    speed_ = (speed_ - w * c * dt) * k;
    value_ = target + e1;
    moving_ = std::abs(e1) > 0.02f || std::abs(speed_) > 0.1f; // degrees, degrees a second
    if (!moving_) {
        value_ = target;
        speed_ = 0.0f;
    }
    return value_;
}

// AzaharFrame fovonce: a speed-limited, ramped follow (a trapezoid in speed), in time
// (AzaharFrame integ8: it was 0.1 degrees per displayed frame, so it ran slower at 90 Hz than at
// 120). Each frame the speed toward the target is the largest of (speed up, keep, slow down)
// that still leaves room to brake at kAccelDegS2 with frames of this length; the last step lands
// on the target. A target that comes back past the value while it moves: it slows down, turns
// and comes back the same way.
float FovSlew::Update(float target, int64_t now_ns) {
    if (!std::isfinite(target)) {
        return value_;
    }
    if (!have_) {
        have_ = true;
        value_ = target;
        step_ = 0.0f;
        speed_ = 0.0f;
        last_ns_ = now_ns;
        moving_ = false;
        return value_;
    }
    const float dt = std::min(static_cast<float>(std::max<int64_t>(now_ns - last_ns_, 0)) / 1e9f,
                              kMaxFrameS);
    last_ns_ = std::max(last_ns_, now_ns);
    step_ = 0.0f;
    if (dt <= 0.0f) {
        return value_; // no time passed (or the clock went back): nothing moves
    }
    const float a = kAccelDegS2 * dt; // the most the speed changes this frame
    const float e = target - value_;
    const float dist = std::abs(e);
    if (dist < 1e-4f && std::abs(speed_) <= a) {
        value_ = target;
        speed_ = 0.0f;
        moving_ = false;
        return value_;
    }
    const float dir = e > 0.0f ? 1.0f : -1.0f;
    const float v = speed_ * dir; // the speed toward the target (negative: moving away)
    // Braking from speed s, a less each frame of dt, covers dt ((s - a) + (s - 2a) + ...) =
    // s (s - a) / (2 kAccelDegS2).
    const auto brake = [a](float s) { return s > a ? s * (s - a) / (2.0f * kAccelDegS2) : 0.0f; };
    float next;
    if (v < 0.0f) {
        next = v + a; // slow down, then turn
    } else {
        next = std::max(v - a, 0.0f);
        for (const float s : {std::min(v + a, kMaxSpeedDegS), std::min(v, kMaxSpeedDegS)}) {
            if (dist - s * dt >= brake(s)) {
                next = s;
                break;
            }
        }
        if (next < 0.25f * a) {
            // Creep the last bit, at a quarter of one frame's change at least: a speed left tiny
            // by braking would keep it short of the target for ever.
            next = std::min(0.25f * a, dist / dt);
        }
        if (next * dt > dist && v - a <= dist / dt) {
            next = dist / dt; // land on it
        }
    }
    speed_ = next * dir;
    step_ = std::clamp(speed_ * dt, -kMaxStepDeg, kMaxStepDeg);
    value_ += step_;
    if (std::abs(target - value_) < 1e-4f && std::abs(speed_) <= a) {
        value_ = target; // landed: exactly the target
    }
    moving_ = true;
    return value_;
}

CameraAngles::Out CameraAngles::Update(unsigned mode, float screen_deg, float live_deg,
                                       int64_t now_ms, int64_t display_ns) {
    if (mode == 1) {
        slew_screen_.Reset();
        slew_live_.Reset();
        return {ease_screen_.Update(screen_deg, now_ms), ease_live_.Update(live_deg, now_ms)};
    }
    if (mode == 2) {
        ease_screen_.Reset();
        ease_live_.Reset();
        return {slew_screen_.Update(screen_deg, display_ns),
                slew_live_.Update(live_deg, display_ns)};
    }
    Reset();
    return {screen_deg, live_deg};
}

} // namespace FrameXr::Immersive::Layout
