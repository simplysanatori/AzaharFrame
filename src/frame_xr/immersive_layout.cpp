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

uint32_t SteppedNarrow(uint32_t setting, int steps) {
    const int now = static_cast<int>(NarrowPercent(setting));
    const int next = now + steps * static_cast<int>(kNarrowStepPercent);
    return static_cast<uint32_t>(std::clamp(next, 0, static_cast<int>(kMaxNarrowPercent)));
}

std::string NarrowToast(uint32_t percent, bool height) {
    const char* what = height ? "Narrow Height" : "Narrow Screen";
    percent = NarrowPercent(percent);
    char buf[40];
    if (percent == 0) {
        std::snprintf(buf, sizeof(buf), "%s: Off", what);
    } else {
        std::snprintf(buf, sizeof(buf), "%s: %u%%", what, percent);
    }
    return buf;
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
    if (level == 0) {
        return "Wider View: Off";
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Wider View: %.1fx", WiderFactor(level));
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
    const float squeeze =
        std::isfinite(options.squeeze) ? std::clamp(options.squeeze, 0.0f, 0.5f) : 0.0f;
    const float squeeze_y =
        std::isfinite(options.squeeze_y) ? std::clamp(options.squeeze_y, 0.0f, 0.5f) : 0.0f;
    HeadLocked s;
    s.distance = p.distance;
    s.pitch_rad = PlacementPitch(p, view);
    // The game draws its own frame times the Wider View factor across. Up and down it draws as
    // much more as the screen's shape needs (factor_y below).
    s.factor = std::isfinite(options.wider) ? std::clamp(options.wider, kFactorMin, kFactorMax)
                                            : kFactorMin;
    // AzaharFrame immersive2: the screen is sized with the settled camera (`game_fov_y_deg`) at
    // the world's own scale: as wide as the game draws (gain 1).
    const float t = GameTan(game_fov_y_deg);
    const float a = std::isfinite(aspect) && aspect > 0.1f ? aspect : 5.0f / 3.0f;
    // What fills the view, fitted at its middle, so moving the screen up or down never resizes it.
    const Cover cover = DefaultCover(view, ViewMiddlePitch(view), eye_shift);
    float qx = s.factor * a * t;
    // The view's shape (the Frame's is taller than 5:3), never shorter than the picture's own
    // (the game would be cut off at the top and bottom); the game draws at most kMaxFactorY times
    // its own height.
    float qy = qx * cover.tan_y / cover.tan_x;
    qy = std::clamp(qy, qx / a, kMaxFactorY * qx / (s.factor * a));
    // Never bigger than filling the view (a very wide game camera): smaller, gain above 1.
    const float fit = std::min({1.0f, cover.tan_x / qx, cover.tan_y / qy});
    qx *= fit * p.size;
    qy *= fit * p.size;
    // The game draws factor_y times its own height into the screen's height, so the picture
    // has the same scale both ways: factor x a / qx = factor_y / qy.
    s.factor_y = s.factor * a * qy / qx;
    // Narrow Screen and Narrow Height: the quad is that much narrower (lower) than the picture.
    s.tan_half_y = qy * (1.0f - squeeze_y);
    s.aspect = qx * (1.0f - squeeze) / s.tan_half_y;
    // The gain follows the camera right now (a zoom): the game's half height (tan) at that
    // factor over the screen's, 1 = the same angle; each axis over its own narrowing. The screen
    // keeps its size when a gain hits a limit (the world then drifts a little).
    const float live = std::isfinite(options.live_fov_y_deg) && options.live_fov_y_deg > 0.0f
                           ? GameTan(options.live_fov_y_deg)
                           : t;
    const float base = s.factor_y * live / qy;
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
                                  bool enabled, Placement& placement) {
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
            placement.distance *= std::exp(hand.stick.y * kDistanceRate * dts);
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

// AzaharFrame fovonce: a step-limited, ramped follow (a trapezoid in speed). Each frame the
// step toward the target is the largest of (speed up, keep, slow down) that still leaves room to
// brake at kStepChangeDeg a frame; the last step lands on the target. A target that comes back
// past the value while it moves: it slows down, turns and comes back the same way.
float FovSlew::Update(float target) {
    if (!std::isfinite(target)) {
        return value_;
    }
    if (!have_) {
        have_ = true;
        value_ = target;
        step_ = 0.0f;
        moving_ = false;
        return value_;
    }
    constexpr float a = kStepChangeDeg;
    const float e = target - value_;
    const float dist = std::abs(e);
    if (dist < 1e-4f && std::abs(step_) <= a) {
        value_ = target;
        step_ = 0.0f;
        moving_ = false;
        return value_;
    }
    const float dir = e > 0.0f ? 1.0f : -1.0f;
    const float v = step_ * dir; // the speed toward the target (negative: moving away)
    // Braking from step s at a a frame covers (s - a) + (s - 2a) + ... = s (s - a) / (2a).
    const auto brake = [](float s) { return s > a ? s * (s - a) / (2.0f * a) : 0.0f; };
    float next;
    if (v < 0.0f) {
        next = v + a; // slow down, then turn
    } else {
        next = std::max(v - a, 0.0f);
        for (const float s : {std::min(v + a, kMaxStepDeg), std::min(v, kMaxStepDeg)}) {
            if (dist - s >= brake(s)) {
                next = s;
                break;
            }
        }
        if (next <= 0.0f) {
            next = std::min(a, dist); // creep the last bit
        }
        if (next > dist && v - a <= dist) {
            next = dist; // land on it
        }
    }
    step_ = next * dir;
    value_ += step_;
    moving_ = true;
    return value_;
}

CameraAngles::Out CameraAngles::Update(unsigned mode, float screen_deg, float live_deg,
                                       int64_t now_ms) {
    if (mode == 1) {
        slew_screen_.Reset();
        slew_live_.Reset();
        return {ease_screen_.Update(screen_deg, now_ms), ease_live_.Update(live_deg, now_ms)};
    }
    if (mode == 2) {
        ease_screen_.Reset();
        ease_live_.Reset();
        return {slew_screen_.Update(screen_deg), slew_live_.Update(live_deg)};
    }
    Reset();
    return {screen_deg, live_deg};
}

} // namespace FrameXr::Immersive::Layout
