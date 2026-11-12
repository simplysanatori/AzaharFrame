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
    // Nonsense from the runtime (no overlap, or an inverted range): keep the fallback.
    if (!(v.right - v.left > 10.0f * kDegToRad) || !(v.up - v.down > 10.0f * kDegToRad)) {
        return ViewFov{};
    }
    return v;
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

float SqueezeFor(uint32_t setting) {
    return setting < std::size(kSqueezeSteps) ? kSqueezeSteps[setting] : 0.0f;
}

namespace {
float GameTan(float game_fov_y_deg) {
    return std::tan(std::clamp(game_fov_y_deg, 10.0f, 150.0f) * kDegToRad / 2.0f);
}
float PlacementPitch(const Placement& p, const ViewFov& view) {
    return ViewMiddlePitch(view) + p.pitch_deg * kDegToRad;
}
} // namespace

float DefaultTanHalfY(float game_fov_y_deg, float aspect, float pitch_rad, const ViewFov& view,
                      float eye_shift) {
    // The whole quad inside the view, and its middle row (where the log measures its width)
    // no wider than +-kComfortHalfWidthDeg. A lower screen's bottom corners reach a little
    // farther out, its top ones a little less.
    const float fit =
        std::min(FitTanHalfY(view, aspect, pitch_rad, kFitMarginDeg * kDegToRad, eye_shift),
                 std::tan(kComfortHalfWidthDeg * kDegToRad) / aspect);
    // Never larger than the game's own angle: no magnifying gain by default, even when the view
    // has room for more. Wider View then grows the screen with the factor (SizeForWider).
    return std::min(fit, GameTan(game_fov_y_deg));
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

float SizeForWider(float size, float from, float to) {
    if (!(from > 0.0f) || !(to > 0.0f) || !std::isfinite(size)) {
        return std::clamp(std::isfinite(size) ? size : 1.0f, kMinSize, kMaxSize);
    }
    return std::clamp(size * to / from, kMinSize, kMaxSize);
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
    const float squeeze = std::clamp(options.squeeze, 0.0f, 0.5f);
    HeadLocked s;
    s.aspect = aspect * (1.0f - squeeze);
    s.distance = p.distance;
    s.pitch_rad = PlacementPitch(p, view);
    const float t = GameTan(game_fov_y_deg);
    // The height comes from the picture's own shape; the squeeze only narrows it.
    const float wanted =
        DefaultTanHalfY(game_fov_y_deg, aspect, s.pitch_rad, view, eye_shift) * p.size;
    // The game draws its own frame times the Wider View factor, whatever the size: a bigger
    // screen magnifies it (gain below 1) and never shows past it, where a game's culling and
    // scenery run out. A Wider View step scales the size with the factor (SizeForWider).
    s.factor = std::isfinite(options.wider) ? std::clamp(options.wider, kFactorMin, kFactorMax)
                                            : kFactorMin;
    // The game's half height (tan) at that factor over the screen's: 1 = the same angle. The
    // screen keeps its size when the gain hits a limit (the world then drifts a little).
    s.tan_half_y = wanted;
    s.gain = std::clamp(s.factor * t / wanted, kMinGain, kMaxGain);
    s.gain_x = std::clamp(s.gain / (1.0f - squeeze), kMinGain, kMaxGain);
    // The Immersive Offset last: it turns the screen fitted above, never resizes it.
    const LensOffset offset = ClampedOffset(offset_in);
    s.pitch_rad += offset.up_deg * kDegToRad;
    s.yaw_rad = -offset.right_deg * kDegToRad;
    return s;
}

float SizeFromOldDefault(float old_size, const Placement& placement, float game_fov_y_deg,
                         float aspect, const ViewFov& view, float eye_shift) {
    const Placement p = Clamped(placement);
    const float pitch = PlacementPitch(p, view);
    // Earlier builds: the whole view both eyes see, at most 1.4 times the game's angle.
    const float old_default =
        std::min(FitTanHalfY(view, aspect, pitch, kFitMarginDeg * kDegToRad, eye_shift),
                 kFactorMax * GameTan(game_fov_y_deg));
    const float now = DefaultTanHalfY(game_fov_y_deg, aspect, pitch, view, eye_shift);
    const float size = std::isfinite(old_size) ? old_size : 1.0f;
    return std::clamp(size * old_default / now, kMinSize, kMaxSize);
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
        if (id.size() != 16 || id.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos ||
            (kind != "view" && kind != "head" && kind != "wide")) {
            continue;
        }
        const uint64_t title = std::strtoull(id.c_str(), nullptr, 16);
        const bool old_size = kind == "head";
        const std::string key = Key(title, kind == "wide");
        // A converted line wins over an old one for the same game, whatever the order.
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
                      "# title id, view (modes 2 and 3) or wide (mode 1), pitch in degrees, "
                      "distance in metres, size\n";
    for (const auto& [key, e] : entries_) {
        std::string line = key;
        if (line.size() > 5 && line.compare(line.size() - 5, 5, " head") == 0) {
            line.replace(line.size() - 4, 4, e.old_size ? "head" : "view");
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

} // namespace FrameXr::Immersive::Layout
