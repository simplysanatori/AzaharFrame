// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "frame_xr/input/gyro_aim.h"

// AzaharFrame kiugyro (gyro_aim.h)
namespace FrameXr::Input {

namespace {
constexpr float kRadToDeg = 57.29577951308232f;
constexpr float kPi = 3.14159265358979f;
constexpr int64_t kMaxGapNs = 100'000'000;

float WrapPi(float a) {
    while (a > kPi) {
        a -= 2.0f * kPi;
    }
    while (a < -kPi) {
        a += 2.0f * kPi;
    }
    return a;
}

float Sign(float v) {
    return v < 0.0f ? -1.0f : 1.0f;
}
} // namespace

float ClampGyroSensitivity(float value) {
    if (!std::isfinite(value)) {
        return kGyroSensitivityDefault;
    }
    return std::clamp(value, kGyroSensitivityMin, kGyroSensitivityMax);
}

float ClampTurnSpeed(float value) {
    if (!std::isfinite(value)) {
        return kTurnSpeedDefault;
    }
    return std::clamp(value, kTurnSpeedMin, kTurnSpeedMax);
}

float ClampTurnSpeedVertical(float value) { // AzaharFrame fixa
    if (!std::isfinite(value)) {
        return kTurnSpeedVerticalDefault;
    }
    return std::clamp(value, kTurnSpeedMin, kTurnSpeedMax);
}

Vec3f AimDirection(const Posef& aim) {
    return Rotate(Normalized(aim.orientation), Vec3f{0.0f, 0.0f, -1.0f});
}

AimAngles AnglesOf(const Vec3f& direction) {
    const Vec3f d = Normalized(direction);
    AimAngles a;
    a.yaw = std::atan2(d.x, -d.z);
    a.pitch = -std::asin(std::clamp(d.y, -1.0f, 1.0f));
    return a;
}

float ModRateFor(float dps) {
    if (!std::isfinite(dps) || std::fabs(dps) < 0.02f) {
        return 0.0f;
    }
    const float out = dps + Sign(dps) * (kModDeadZoneDps + 0.5f * kHidGyroStepDps);
    return std::clamp(out, -kModMaxDps - kModDeadZoneDps, kModMaxDps + kModDeadZoneDps);
}

bool GripClutch::Update(float grip) {
    if (held_) {
        held_ = grip >= 0.40f;
    } else {
        held_ = grip > 0.60f;
    }
    return held_;
}

GyroAimTracker::Out GyroAimTracker::Update(const HandInput& hand, int64_t time_ns,
                                           float sensitivity, bool clutch) {
    if (!hand.active || clutch) {
        have_last_ = false; // the pose seen now is not a start: the next tracked frame is
        last_out_ = {};
        return {};
    }
    const AimAngles now = AnglesOf(AimDirection(hand.aim));
    Out out;
    if (have_last_ && time_ns == last_time_ns_) {
        return last_out_; // the same display time twice: nothing new
    }
    if (have_last_ && time_ns > last_time_ns_ && time_ns - last_time_ns_ <= kMaxGapNs) {
        const float dt = static_cast<float>(time_ns - last_time_ns_) * 1e-9f;
        const float s = ClampGyroSensitivity(sensitivity);
        // Near straight up or down the yaw is not defined: weigh it by the pitch's cosine.
        const float yaw_weight = std::cos(now.pitch);
        out.right_dps = WrapPi(now.yaw - last_.yaw) * yaw_weight / dt * kRadToDeg * s;
        out.down_dps = (now.pitch - last_.pitch) / dt * kRadToDeg * s;
    }
    have_last_ = true;
    last_ = now;
    last_time_ns_ = time_ns;
    last_out_ = out;
    return out;
}

float RecoilAmplitude(const RecoilPulse& pulse, int charged_percent, int rapid_percent) {
    if (!pulse.fire) {
        return 0.0f;
    }
    const int pct = std::clamp(pulse.charged ? charged_percent : rapid_percent, 0, 100);
    return static_cast<float>(pct) / 100.0f;
}

RecoilPulse Recoil::Update(int64_t now_ns, bool fire, bool active) {
    RecoilPulse out;
    if (!active) {
        held_ = fire; // a trigger held through a menu is no new shot when it comes back,
        last_pulse_ns_ = now_ns; // and the pace starts again from then
        return out;
    }
    if (fire && !held_) {
        // A press: a charged shot after a long enough pause (heuristic), the first one too.
        const bool charged = !ever_released_ || now_ns - released_ns_ >= kChargePauseNs;
        out = {true, charged ? kChargedPulseNs : kPulseNs, charged};
        last_pulse_ns_ = now_ns;
    } else if (fire && now_ns - last_pulse_ns_ >= kRecoilPaceNs) {
        out = {true, kPulseNs}; // held: continuous fire
        last_pulse_ns_ = now_ns;
    } else if (!fire && held_) {
        ever_released_ = true;
        released_ns_ = now_ns;
    }
    held_ = fire;
    return out;
}

RecoilPulse ShotRecoil::Update(int64_t now_ns, uint64_t total, bool readable, bool active) {
    RecoilPulse out;
    if (!readable || !active || !has_) {
        has_ = readable; // shots made while inactive (a menu) or before the first read: none
        last_ = total;
        if (!active) {
            last_shot_ns_ = now_ns;
        }
        return out;
    }
    if (total > last_) {
        const uint64_t n = total - last_;
        const bool charged = ever_ && now_ns - last_shot_ns_ >= Recoil::kChargePauseNs;
        out = {true, charged ? Recoil::kChargedPulseNs : n > 1 ? kBurstPulseNs : Recoil::kPulseNs,
               charged};
        last_shot_ns_ = now_ns;
        ever_ = true;
    }
    last_ = total;
    return out;
}

Ray AimRay(const Posef& aim, const std::optional<Vec3f>& muzzle) {
    Ray ray;
    ray.direction = AimDirection(aim);
    ray.origin = muzzle ? aim.position + Rotate(Normalized(aim.orientation), *muzzle) : aim.position;
    return ray;
}

Ray AimRay(const Posef& aim, const std::optional<Vec3f>& muzzle,
           const std::optional<Vec3f>& axis) { // AzaharFrame kiuinteg
    if (!axis || !(Length(*axis) > 1e-6f)) {
        return AimRay(aim, muzzle);
    }
    Ray ray = AimRay(aim, muzzle);
    ray.direction = Normalized(Rotate(Normalized(aim.orientation), *axis));
    return ray;
}

AimRayFrom AimRayFromSwitch(const std::optional<std::string>& value) {
    return value && *value == "hand" ? AimRayFrom::Hand : AimRayFrom::Weapon;
}

Ray AngelsAimRay(const Posef& aim, const std::optional<Vec3f>& muzzle,
                 const std::optional<Vec3f>& axis, AimRayFrom from) {
    return from == AimRayFrom::Hand ? AimRay(aim, muzzle) : AimRay(aim, muzzle, axis);
}

LightGunTarget LightGunTargetFor(const Ray& ray, const PanelQuad& top, const PanelQuad* other) {
    LightGunTarget t;
    if (!top.visible) {
        return t;
    }
    if (other && other->visible && IntersectQuad(ray, *other).in_bounds) {
        return t; // on the bottom screen: a touch, not the reticle
    }
    const QuadHit hit = IntersectQuad(ray, top);
    if (!hit.on_plane || !std::isfinite(hit.u) || !std::isfinite(hit.v)) {
        return t;
    }
    t.valid = true;
    t.u = std::clamp(hit.u, -0.5f, 1.5f);
    t.v = std::clamp(hit.v, -0.5f, 1.5f);
    t.point = hit.point_world;
    return t;
}

LightGunTarget LightGunTargetFor(const HandInput& hand, const PanelQuad& top) {
    if (!hand.active) {
        return {};
    }
    return LightGunTargetFor(RayFromPose(hand.aim), top);
}

LightGunTarget LockedTargetFor(const Ray& ray, const PanelQuad& top) {
    LightGunTarget t;
    if (!top.visible || !(top.width_m > 0.0f) || !(top.height_m > 0.0f)) {
        return t;
    }
    const QuadHit hit = IntersectQuad(ray, top);
    const bool finite = std::isfinite(hit.u) && std::isfinite(hit.v);
    t.valid = true;
    if (hit.in_bounds && finite) {
        t.u = hit.u;
        t.v = hit.v;
        t.point = hit.point_world;
        return t;
    }
    float u = 0.5f, v = 1.0f;
    if (hit.on_plane && finite) {
        u = hit.u;
        v = hit.v;
    } else {
        // Pointing away from the screen's plane (or along it): the edge in the ray's direction,
        // seen in the screen's own axes (x right, y up), from its centre.
        const Vec3f d = Rotate(Conjugate(Normalized(top.pose.orientation)), ray.direction);
        const float x = d.x / top.width_m, y = -d.y / top.height_m;
        const float m = std::max(std::fabs(x), std::fabs(y));
        if (m > 1e-6f) {
            u = 0.5f + 0.5f * x / m;
            v = 0.5f + 0.5f * y / m;
        }
        // On the edge it is past (exactly 0 or 1 there): nudged past for past_u, past_v.
        u = u <= 0.0f ? -1.0f : (u >= 1.0f ? 2.0f : u);
        v = v <= 0.0f ? -1.0f : (v >= 1.0f ? 2.0f : v);
    }
    t.edge = true;
    t.past_u = u < 0.0f ? -1 : (u > 1.0f ? 1 : 0);
    t.past_v = v < 0.0f ? -1 : (v > 1.0f ? 1 : 0);
    t.u = std::clamp(u, 0.0f, 1.0f);
    t.v = std::clamp(v, 0.0f, 1.0f);
    t.point = PointOnQuad(top, t.u, t.v);
    const Vec3f to_edge = t.point - ray.origin;
    const float len = Length(to_edge), dlen = Length(ray.direction);
    if (len > 1e-6f && dlen > 1e-6f) {
        const float c = std::clamp(Dot(to_edge, ray.direction) / (len * dlen), -1.0f, 1.0f);
        t.past_deg = std::acos(c) * 57.29578f;
    }
    t.resting = t.past_deg > kRestingPastDeg;
    return t;
}

Vec3f PointOnQuad(const PanelQuad& quad, float u, float v) {
    Vec3f local{(u - 0.5f) * quad.width_m, (0.5f - v) * quad.height_m, 0.0f};
    if (quad.curve_radius > 0.0f) {
        // As IntersectQuad reads it: the arc's length along the screen, about the line x = 0,
        // z = R.
        const float r = quad.curve_radius;
        const float a = (u - 0.5f) * quad.width_m / r;
        local.x = r * std::sin(a);
        local.z = r - r * std::cos(a);
    }
    return Transform(quad.pose, local);
}

// ---- AzaharFrame kiuaim2: Check Aim ---------------------------------------------------------

namespace {

/// Least squares y = sum(c_k x_k) for k unknowns (2 or 3); false when singular.
bool Solve(const std::vector<std::array<double, 3>>& x, const std::vector<double>& y, int k,
           double out[3]) {
    double a[3][4]{};
    for (std::size_t i = 0; i < x.size(); i++) {
        for (int r = 0; r < k; r++) {
            for (int c = 0; c < k; c++) {
                a[r][c] += x[i][r] * x[i][c];
            }
            a[r][3] += x[i][r] * y[i];
        }
    }
    for (int c = 0; c < k; c++) {
        int pivot = c;
        for (int r = c + 1; r < k; r++) {
            if (std::fabs(a[r][c]) > std::fabs(a[pivot][c])) {
                pivot = r;
            }
        }
        if (!(std::fabs(a[pivot][c]) > 1e-12)) {
            return false;
        }
        for (int j = 0; j < 4; j++) {
            std::swap(a[c][j], a[pivot][j]);
        }
        for (int r = 0; r < k; r++) {
            if (r == c) {
                continue;
            }
            const double f = a[r][c] / a[c][c];
            for (int j = c; j < 4; j++) {
                a[r][j] -= f * a[c][j];
            }
        }
    }
    for (int c = 0; c < k; c++) {
        out[c] = a[c][3] / a[c][c];
    }
    return true;
}

Vec2f Shown(const AimMap& m, Vec2f w) {
    return {m.u[0] + m.u[1] * w.x + m.u[2] * w.y, m.v[0] + m.v[1] * w.y + m.v[2] * w.x};
}

float MaxErrorPx(const AimMap& m, const std::vector<Vec2f>& written,
                 const std::vector<Vec2f>& shown, float w_px, float h_px) {
    float worst = 0.0f;
    for (std::size_t i = 0; i < written.size(); i++) {
        const Vec2f p = Shown(m, written[i]);
        worst = std::max(worst, std::hypot((p.x - shown[i].x) * w_px, (p.y - shown[i].y) * h_px));
    }
    return worst;
}

/// One axis: offset and scale (k = 2) or with the other axis' skew (k = 3).
bool FitAxis(const std::vector<Vec2f>& written, const std::vector<Vec2f>& shown, bool u_axis,
             int k, float out[3]) {
    std::vector<std::array<double, 3>> x;
    std::vector<double> y;
    for (std::size_t i = 0; i < written.size(); i++) {
        const double own = u_axis ? written[i].x : written[i].y;
        const double other = u_axis ? written[i].y : written[i].x;
        x.push_back({1.0, own, other});
        y.push_back(u_axis ? shown[i].x : shown[i].y);
    }
    double c[3]{0.0, 1.0, 0.0};
    if (!Solve(x, y, k, c)) {
        return false;
    }
    out[0] = static_cast<float>(c[0]);
    out[1] = static_cast<float>(c[1]);
    out[2] = k == 3 ? static_cast<float>(c[2]) : 0.0f;
    return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
}

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at <= text.size()) {
        const std::size_t end = text.find('\n', at);
        std::string line = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            out.push_back(line);
        }
        if (end == std::string::npos) {
            break;
        }
        at = end + 1;
    }
    return out;
}

} // namespace

bool AimMapValid(const AimMap& m) {
    if (!m.set) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (!std::isfinite(m.u[i]) || !std::isfinite(m.v[i])) {
            return false;
        }
    }
    const auto axis_ok = [](const float c[3]) {
        return c[1] >= kAimMapMinScale && c[1] <= kAimMapMaxScale &&
               std::fabs(c[2]) <= kAimMapMaxSkew &&
               std::fabs(c[0] + c[1] * 0.5f + c[2] * 0.5f - 0.5f) <= kAimMapMaxOffset;
    };
    return axis_ok(m.u) && axis_ok(m.v) && std::isfinite(m.error_px) && m.error_px >= 0.0f;
}

LightGunTarget ApplyAimMap(const AimMap& m, const LightGunTarget& wanted) {
    if (!wanted.valid || !AimMapValid(m)) {
        return wanted;
    }
    // shown = A written + b; written = A^-1 (shown - b).
    const float a = m.u[1], b = m.u[2], c = m.v[2], d = m.v[1];
    const float det = a * d - b * c;
    if (!(std::fabs(det) > 1e-6f)) {
        return wanted;
    }
    const float su = wanted.u - m.u[0], sv = wanted.v - m.v[0];
    LightGunTarget out = wanted;
    out.u = std::clamp((d * su - b * sv) / det, -0.5f, 1.5f);
    out.v = std::clamp((a * sv - c * su) / det, -0.5f, 1.5f);
    return out;
}

std::optional<AimMap> FitAimMap(const std::vector<Vec2f>& written, const std::vector<Vec2f>& shown,
                                float w_px, float h_px) {
    if (written.size() < 3 || written.size() != shown.size() || !(w_px > 0.0f) ||
        !(h_px > 0.0f)) {
        return std::nullopt;
    }
    AimMap fit;
    fit.set = true;
    fit.points = static_cast<int>(written.size());
    if (!FitAxis(written, shown, true, 2, fit.u) || !FitAxis(written, shown, false, 2, fit.v)) {
        return std::nullopt;
    }
    fit.error_px = MaxErrorPx(fit, written, shown, w_px, h_px);
    // The skew, only when the points need it (a picture drawn a little sheared).
    if (written.size() >= 5 && fit.error_px > 1.5f) {
        AimMap skew = fit;
        if (FitAxis(written, shown, true, 3, skew.u) && FitAxis(written, shown, false, 3, skew.v)) {
            skew.error_px = MaxErrorPx(skew, written, shown, w_px, h_px);
            if (skew.error_px < fit.error_px * (2.0f / 3.0f) && AimMapValid(skew)) {
                fit = skew;
            }
        }
    }
    // Nothing to correct: the identity (the player's own hand is in the points too).
    AimMap identity;
    identity.set = true;
    identity.points = fit.points;
    identity.error_px = MaxErrorPx(identity, written, shown, w_px, h_px);
    if (identity.error_px <= fit.error_px + 1.0f) {
        return identity;
    }
    if (!AimMapValid(fit)) {
        return std::nullopt;
    }
    return fit;
}

std::string AimMapText(const AimMap& m) {
    if (!AimMapValid(m)) {
        return {};
    }
    char buf[192];
    std::snprintf(buf, sizeof(buf), "v2 %.6f %.6f %.6f %.6f %.6f %.6f %.2f %d", m.u[0], m.u[1],
                  m.u[2], m.v[0], m.v[1], m.v[2], m.error_px, m.points);
    return buf;
}

AimMap AimMapFrom(const std::string& line) {
    AimMap m;
    int points = 0;
    if (std::sscanf(line.c_str(), "v2 %f %f %f %f %f %f %f %d", &m.u[0], &m.u[1], &m.u[2],
                    &m.v[0], &m.v[1], &m.v[2], &m.error_px, &points) != 8) {
        return {};
    }
    m.set = true;
    m.points = points;
    return AimMapValid(m) ? m : AimMap{};
}

bool IsOldAimCalibration(const std::string& line) {
    return line.rfind("v1 ", 0) == 0;
}

std::string AimFileText(const std::string& old_text, const AimMap& m) {
    std::string out;
    for (const std::string& line : Lines(old_text)) {
        if (IsOldAimCalibration(line)) {
            out = line + "\n"; // an older build's own line, first, as it was
            break;
        }
    }
    const std::string map = AimMapText(m);
    if (!map.empty()) {
        out += map + "\n";
    }
    return out;
}

AimMap AimMapFromFile(const std::string& text, bool* had_old, bool* had_bad) {
    AimMap found;
    for (const std::string& line : Lines(text)) {
        if (IsOldAimCalibration(line)) {
            if (had_old) {
                *had_old = true;
            }
        } else if (line.rfind("v2 ", 0) == 0) {
            found = AimMapFrom(line);
            if (!found.set && had_bad) {
                *had_bad = true;
            }
        }
    }
    return found;
}

std::array<Vec2f, kAimTargets> AimTargets(const AimRegion& r) {
    const float du = (r.u1 - r.u0) * kAimTargetInset, dv = (r.v1 - r.v0) * kAimTargetInset;
    const float l = r.u0 + du, t = r.v0 + dv, rt = r.u1 - du, b = r.v1 - dv;
    const float mu = (r.u0 + r.u1) * 0.5f, mv = (r.v0 + r.v1) * 0.5f;
    // The centre, then round the edge clockwise from the top left.
    return {Vec2f{mu, mv}, Vec2f{l, t}, Vec2f{rt, t}, Vec2f{rt, mv}, Vec2f{rt, b}, Vec2f{l, b},
            Vec2f{l, mv}};
}

void AimCheckFlow::Start(const AimRegion& region, float w_px, float h_px) {
    targets_ = AimTargets(region);
    w_ = w_px > 1.0f ? w_px : 400.0f;
    h_ = h_px > 1.0f ? h_px : 240.0f;
    written_.clear();
    shown_.clear();
    step_ = 0;
    retries_ = 0;
}

AimCheckFlow::Out AimCheckFlow::Update(bool pulled, const LightGunTarget& written) {
    Out out;
    if (!active() || !pulled) {
        return out;
    }
    const Vec2f target = targets_[static_cast<std::size_t>(step_)];
    const auto slip = [&] {
        out.retry = true;
        if (++retries_ >= 3 && !written_.empty()) {
            // Three slips at one target: an earlier point may be the bad one. From the start.
            written_.clear();
            shown_.clear();
            step_ = 0;
            retries_ = 0;
            out.retry = false;
            out.restart = true;
        }
        return out;
    };
    if (!written.valid) {
        return slip();
    }
    const Vec2f w{written.u, written.v};
    if (std::fabs((target.x - w.x) * w_) > kAimCheckMaxPx ||
        std::fabs((target.y - w.y) * h_) > kAimCheckMaxPx) {
        return slip(); // too far from the target: not on it
    }
    if (written_.size() >= 2) {
        // From the third: against the line the others give, each axis.
        AimMap line;
        line.set = true;
        if (FitAxis(written_, shown_, true, 2, line.u) && FitAxis(written_, shown_, false, 2, line.v)) {
            const Vec2f p = Shown(line, w);
            if (std::fabs((p.x - target.x) * w_) > kAimCheckOutlierPx ||
                std::fabs((p.y - target.y) * h_) > kAimCheckOutlierPx) {
                return slip();
            }
        }
    }
    written_.push_back(w);
    shown_.push_back(target);
    retries_ = 0;
    step_++;
    if (step_ < kAimTargets) {
        out.changed = true;
        return out;
    }
    step_ = -1;
    out.done = true;
    out.result = FitAimMap(written_, shown_, w_, h_);
    return out;
}

std::string AimCheckStepToast(int step) {
    return "Check Aim: Target " + std::to_string(step + 1) + " of " + std::to_string(kAimTargets);
}

std::string AimCheckedToast(float error_px) {
    const int px = std::max(1, static_cast<int>(std::ceil(error_px - 0.05f)));
    return "Aim Checked \xC2\xB7 Within " + std::to_string(px) + " px";
}

Vec2f TurnZoneDepth(const LightGunTarget& target, float zone_fraction) {
    if (!target.valid || !(zone_fraction > 0.0f)) {
        return {};
    }
    const float z = std::min(zone_fraction, 0.5f);
    const auto depth = [z](float p) {
        if (p < z) {
            return -std::clamp((z - p) / z, 0.0f, 1.0f);
        }
        if (p > 1.0f - z) {
            return std::clamp((p - (1.0f - z)) / z, 0.0f, 1.0f);
        }
        return 0.0f;
    };
    return {depth(target.u), depth(target.v)};
}

AimGrip::Out AimGrip::Update(float grip, bool other_grip, AimGripMode mode, bool suspend) {
    Out out;
    const bool held = clutch_.Update(grip);
    const bool was_free = mode_ == AimGripMode::Toggle && toggled_free_;
    if (mode != mode_) {
        mode_ = mode;
        toggled_free_ = false;
    }
    if (held && !was_held_) {
        joined_ = other_grip;
    } else if (held) {
        joined_ = joined_ || other_grip;
    }
    const bool released = was_held_ && !held;
    was_held_ = held;
    if (suspend) {
        toggled_free_ = false;
    } else if (mode == AimGripMode::Toggle && released && !joined_) {
        toggled_free_ = !toggled_free_;
    }
    if (mode == AimGripMode::Toggle) {
        out.free = toggled_free_;
        out.toggled = toggled_free_ != was_free;
    } else {
        out.free = held;
    }
    return out;
}

// ---- AzaharFrame kiuaim2: Direct Aim --------------------------------------------------------

ReticleBox GroundBox(const ReticleReading& r) {
    return {0.7f * r.cx, (1.0f + r.box_top) * r.cy, 1.3f * r.cx, (1.0f + r.box_bottom) * r.cy};
}

ReticleBox ReachableBox(const ReticleReading& r) {
    ReticleBox b = r.ground ? GroundBox(r) : ReticleBox{0.0f, 0.0f, r.w, r.h};
    b.x0 += kReachInsetPx;
    b.y0 += kReachInsetPx;
    b.x1 -= kReachInsetPx;
    b.y1 -= kReachInsetPx;
    if (b.x1 < b.x0) {
        b.x0 = b.x1 = (b.x0 + b.x1) * 0.5f;
    }
    if (b.y1 < b.y0) {
        b.y0 = b.y1 = (b.y0 + b.y1) * 0.5f;
    }
    return b;
}

Vec2f ReticlePixel(const ReticleReading& r, float u, float v) {
    return {u * r.w, v * r.h};
}

Vec2f ReticleUv(const ReticleReading& r, float x, float y) {
    return {r.w > 0.0f ? x / r.w : 0.5f, r.h > 0.0f ? y / r.h : 0.5f};
}

AimRegion ReachableRegion(const ReticleReading& r) {
    const ReticleBox b = ReachableBox(r);
    const Vec2f a = ReticleUv(r, b.x0, b.y0), c = ReticleUv(r, b.x1, b.y1);
    return {a.x, a.y, c.x, c.y};
}

Vec2f AimBoxTurnDepth(const ReticleReading& r, Vec2f px, float zone_fraction) {
    if (!(zone_fraction > 0.0f) || !(r.w > 0.0f) || !(r.h > 0.0f)) {
        return {};
    }
    const ReticleBox b = GroundBox(r);
    const auto depth = [](float p, float lo, float hi, float z) {
        if (p < lo) {
            return -std::clamp((lo - p) / z, 0.0f, 1.0f);
        }
        if (p > hi) {
            return std::clamp((p - hi) / z, 0.0f, 1.0f);
        }
        return 0.0f;
    };
    const float z = std::min(zone_fraction, 0.5f);
    return {depth(px.x, b.x0, b.x1, z * r.w), depth(px.y, b.y0, b.y1, z * r.h)};
}

Vec2f TurnDepthFor(const ReticleReading& r, const LightGunTarget& target, float zone_fraction,
                   TurnZoneFrom from) {
    if (!r.ground || !target.valid || target.resting || !(zone_fraction > 0.0f)) {
        return {};
    }
    Vec2f d = from == TurnZoneFrom::ScreenEdge
                  ? TurnZoneDepth(target, zone_fraction)
                  : AimBoxTurnDepth(r, ReticlePixel(r, target.u, target.v), zone_fraction);
    if (target.edge) {
        // Past the screen's edge: all the way, as the C-Stick held to its rim.
        if (target.past_u != 0) {
            d.x = static_cast<float>(target.past_u);
        }
        if (target.past_v != 0) {
            d.y = static_cast<float>(target.past_v);
        }
    }
    return d;
}

DirectAim::Out DirectAim::Update(int64_t time_ns, const ReticleReading& r,
                                 const LightGunTarget& target, const Turn& turn, bool pause) {
    Out out;
    if (!r.valid || !(r.w > 1.0f) || !(r.h > 1.0f)) {
        Reset();
        return out;
    }
    if (pause || r.game_places) {
        paused_ = true; // the stick (or the game) has the reticle: hands off
        easing_ = false;
        return out;
    }
    if (paused_) {
        paused_ = false;
        easing_ = true;
        resume_ns_ = time_ns;
        from_x_ = r.x;
        from_y_ = r.y;
    }
    float ease = 1.0f;
    if (easing_) {
        const float t = static_cast<float>(time_ns - resume_ns_) / static_cast<float>(kEaseInNs);
        if (t >= 1.0f || time_ns < resume_ns_) {
            easing_ = false;
        } else {
            const float k = std::clamp(t, 0.0f, 1.0f);
            ease = k * k * (3.0f - 2.0f * k);
        }
    }
    if (!target.valid) {
        return out; // off the top screen: the reticle stays
    }
    const ReticleBox box = ReachableBox(r);
    const Vec2f px = ReticlePixel(r, target.u, target.v);
    const float tx = std::clamp(px.x, box.x0, box.x1);
    const float ty = std::clamp(px.y, box.y0, box.y1);
    out.write = true;
    out.easing = easing_;
    out.x = easing_ ? from_x_ + (tx - from_x_) * ease : tx;
    out.y = easing_ ? from_y_ + (ty - from_y_) * ease : ty;
    out.resting = target.resting;
    if (r.ground) {
        const Vec2f depth = TurnDepthFor(r, target, turn.zone_fraction, turn.from);
        if (depth.x != 0.0f || depth.y != 0.0f) {
            // AzaharFrame fixa: each axis at its own Turn Speed.
            const float sx = ClampTurnSpeed(turn.speed) * kTurnPushDps * ease;
            const float sy = (turn.speed_vertical < 0.0f
                                  ? ClampTurnSpeed(turn.speed)
                                  : ClampTurnSpeedVertical(turn.speed_vertical)) *
                             kTurnPushDps * ease;
            out.right_dps = std::clamp(depth.x * sx, -kModMaxDps, kModMaxDps);
            out.down_dps = std::clamp(depth.y * sy, -kModMaxDps, kModMaxDps);
            out.turning = true;
        }
    }
    return out;
}

void AimSampler::Push(int64_t host_ns, const LightGunTarget& target) {
    if (have_last_ && host_ns <= last_ns_) {
        last_ = target; // the same moment again: the newer value
        return;
    }
    before_ = last_;
    before_ns_ = last_ns_;
    have_before_ = have_last_;
    last_ = target;
    last_ns_ = host_ns;
    have_last_ = true;
}

LightGunTarget AimSampler::At(int64_t host_ns) const {
    if (!have_last_) {
        return {};
    }
    LightGunTarget t = last_;
    const int64_t gap = last_ns_ - before_ns_;
    // AzaharFrame kiuaim3: an edge point (Keep Reticle on Screen) is not carried along.
    if (!t.valid || !have_before_ || !before_.valid || t.edge || before_.edge || gap <= 0 ||
        gap > kMaxGapNs || host_ns <= last_ns_) {
        return t;
    }
    const float lead = static_cast<float>(std::min<int64_t>(host_ns - last_ns_, kMaxLeadNs)) /
                       static_cast<float>(gap);
    t.u = std::clamp(t.u + (last_.u - before_.u) * lead, -0.5f, 1.5f);
    t.v = std::clamp(t.v + (last_.v - before_.v) * lead, -0.5f, 1.5f);
    return t;
}

} // namespace FrameXr::Input
