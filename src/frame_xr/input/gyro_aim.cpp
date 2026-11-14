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

LightGunTarget LightGunTargetFor(const HandInput& hand, const PanelQuad& top) {
    LightGunTarget t;
    if (!hand.active || !top.visible) {
        return t;
    }
    const QuadHit hit = IntersectQuad(RayFromPose(hand.aim), top);
    if (!hit.on_plane || !std::isfinite(hit.u) || !std::isfinite(hit.v)) {
        return t;
    }
    t.valid = true;
    t.u = std::clamp(hit.u, -0.5f, 1.5f);
    t.v = std::clamp(hit.v, -0.5f, 1.5f);
    return t;
}

bool AimSchemeHint::Update(int64_t now_ns, bool aiming, int scheme) {
    if (!aiming || scheme == 1) {
        armed_ = true; // off, or the gyro scheme: the next wrong scheme is news
        return false;
    }
    if (scheme < 0 || !armed_) {
        return false;
    }
    if (shown_once_ && now_ns - shown_ns_ < kRepeatNs) {
        return false;
    }
    armed_ = false;
    shown_once_ = true;
    shown_ns_ = now_ns;
    return true;
}

LightGunTarget ApplyCalibration(const AimCalibration& c, const LightGunTarget& t) {
    if (!c.set || !t.valid) {
        return t;
    }
    LightGunTarget out = t;
    out.u = std::clamp(c.au + c.bu * t.u, -0.5f, 1.5f);
    out.v = std::clamp(c.av + c.bv * t.v, -0.5f, 1.5f);
    return out;
}

std::optional<AimCalibration> FitCalibration(const std::vector<Vec2f>& raw,
                                             const std::vector<Vec2f>& wanted) {
    if (raw.empty() || raw.size() != wanted.size()) {
        return std::nullopt;
    }
    AimCalibration c;
    c.set = true;
    const auto fit = [&](bool x, float& a, float& b) {
        const std::size_t n = raw.size();
        if (n == 1) {
            b = 1.0f;
            a = (x ? wanted[0].x - raw[0].x : wanted[0].y - raw[0].y);
            return std::isfinite(a) && std::fabs(a) <= kCalibrationMaxOffset;
        }
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (std::size_t i = 0; i < n; i++) {
            const double r = x ? raw[i].x : raw[i].y;
            const double w = x ? wanted[i].x : wanted[i].y;
            sx += r;
            sy += w;
            sxx += r * r;
            sxy += r * w;
        }
        const double den = static_cast<double>(n) * sxx - sx * sx;
        if (!(std::fabs(den) > 1e-9)) {
            return false;
        }
        b = static_cast<float>((static_cast<double>(n) * sxy - sx * sy) / den);
        a = static_cast<float>((sy - b * sx) / static_cast<double>(n));
        // The offset as seen at the centre is what is limited (a large a with a scale is fine).
        const float at_centre = a + b * 0.5f - 0.5f;
        return std::isfinite(a) && std::isfinite(b) && b >= kCalibrationMinScale &&
               b <= kCalibrationMaxScale && std::fabs(at_centre) <= kCalibrationMaxOffset;
    };
    if (!fit(true, c.au, c.bu) || !fit(false, c.av, c.bv)) {
        return std::nullopt;
    }
    c.scaled = raw.size() > 1;
    return c;
}

std::string AimCalibrationText(const AimCalibration& c) {
    if (!c.set) {
        return {};
    }
    char buf[96];
    std::snprintf(buf, sizeof(buf), "v1 %.5f %.5f %.5f %.5f %d", c.au, c.bu, c.av, c.bv,
                  c.scaled ? 1 : 0);
    return buf;
}

AimCalibration AimCalibrationFrom(const std::string& text) {
    AimCalibration c;
    float au = 0, bu = 1, av = 0, bv = 1;
    int scaled = 0;
    if (std::sscanf(text.c_str(), "v1 %f %f %f %f %d", &au, &bu, &av, &bv, &scaled) != 5) {
        return {};
    }
    const auto ok = [](float a, float b) {
        return std::isfinite(a) && std::isfinite(b) && b >= kCalibrationMinScale &&
               b <= kCalibrationMaxScale && std::fabs(a + b * 0.5f - 0.5f) <= kCalibrationMaxOffset;
    };
    if (!ok(au, bu) || !ok(av, bv)) {
        return {};
    }
    c = {true, au, bu, av, bv, scaled != 0};
    return c;
}

void AimCalibrationFlow::Start() {
    step_ = Step::Centre;
    raw_.clear();
    grip_.Reset();
    was_held_ = true;
}

AimCalibrationFlow::Out AimCalibrationFlow::Update(float grip, const LightGunTarget& raw) {
    Out out;
    if (step_ == Step::Off) {
        return out;
    }
    const bool held = grip_.Update(grip);
    const bool press = held && !was_held_;
    was_held_ = held;
    if (!press) {
        return out;
    }
    if (!raw.valid || raw.u < -0.25f || raw.u > 1.25f || raw.v < -0.25f || raw.v > 1.25f) {
        out.retry = true; // not at the top screen
        return out;
    }
    static const Vec2f kWanted[3] = {{0.5f, 0.5f}, {0.0f, 0.0f}, {1.0f, 1.0f}};
    raw_.push_back({raw.u, raw.v});
    const std::vector<Vec2f> wanted(kWanted, kWanted + raw_.size());
    const std::optional<AimCalibration> c = FitCalibration(raw_, wanted);
    if (!c) {
        raw_.pop_back();
        out.retry = true;
        return out;
    }
    out.result = c;
    out.changed = true;
    step_ = step_ == Step::Centre    ? Step::TopLeft
            : step_ == Step::TopLeft ? Step::BottomRight
                                     : Step::Off;
    return out;
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

const char* AimCalibrationPrompt(AimCalibrationFlow::Step step) {
    switch (step) {
    case AimCalibrationFlow::Step::Centre:
        return "Calibrate Aim: Point at the Top Screen's Centre, Press Grip";
    case AimCalibrationFlow::Step::TopLeft:
        return "Calibrate Aim: Top-Left Corner, Press Grip";
    case AimCalibrationFlow::Step::BottomRight:
        return "Calibrate Aim: Bottom-Right Corner, Press Grip";
    case AimCalibrationFlow::Step::Off:
        break;
    }
    return kAimCalibratedToast;
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

ReticleBox GroundBox(const ReticleReading& r) {
    return {0.7f * r.cx, 0.4f * r.cy, 1.3f * r.cx, 1.25f * r.cy};
}

void LightGunLoop::Reset() {
    // What was learnt about the game (its gain, its frame) stays; the rest starts again.
    LightGunLoop fresh;
    fresh.gain_[0] = gain_[0];
    fresh.gain_[1] = gain_[1];
    fresh.frame_s_ = frame_s_;
    *this = fresh;
}

float LightGunLoop::SentSince(int axis, int64_t from_ns) const {
    float sum = 0.0f;
    for (std::size_t i = 0; i < sent_count_; i++) {
        const Sent& s = sent_[(sent_head_ + kHistory - 1 - i) % kHistory];
        if (s.time_ns <= from_ns) {
            break; // older entries are older still
        }
        sum += s.deg[axis];
    }
    return sum;
}

void LightGunLoop::Learn(int64_t time_ns, const ReticleReading& r, bool pushing) {
    // Is the reticle free to move here (not held by the box or the screen's edge)?
    float x0 = 0.0f, y0 = 0.0f, x1 = r.w, y1 = r.h;
    if (r.ground) {
        const ReticleBox b = GroundBox(r);
        x0 = b.x0, y0 = b.y0, x1 = b.x1, y1 = b.y1;
    }
    constexpr float kEdgePx = 2.0f;
    const bool free = r.x > x0 + kEdgePx && r.x < x1 - kEdgePx && r.y > y0 + kEdgePx &&
                      r.y < y1 - kEdgePx;
    // A move from rest to rest says exactly how far the degrees sent went, however late the
    // game applied them and whichever frames it sampled: so each axis learns between two quiet
    // moments (nothing sent for a frame more than the latency).
    const int64_t quiet_ns = static_cast<int64_t>((kLatencyFrames + 1.5f) * frame_s_ * 1e9f);
    const float pos[2]{r.x, r.y};
    // Running away (the reticle goes the other way than sent: the game's invert option, or a
    // build with the other sign): flip at once rather than wait for a rest that never comes.
    const int64_t lag_ns = static_cast<int64_t>(kLatencyFrames * frame_s_ * 1e9f);
    for (int a = 0; a < 2; a++) {
        if (mark_ns_[a] == 0 || pushing) {
            mark_ns_[a] = time_ns;
            mark_pos_[a] = a == 0 ? r.x : r.y;
            continue;
        }
        if (time_ns - mark_ns_[a] < static_cast<int64_t>(6.0f * frame_s_ * 1e9f)) {
            continue;
        }
        const float p = a == 0 ? r.x : r.y;
        const float moved = p - mark_pos_[a];
        const float deg = SentSince(a, mark_ns_[a] - lag_ns) - SentSince(a, time_ns - lag_ns);
        // Or held against an edge while what is sent should take it away from that edge.
        const float lo = a == 0 ? x0 : y0, hi = a == 0 ? x1 : y1;
        const float meant = deg * gain_[a]; // where the sent degrees should have taken it
        const bool pinned = std::fabs(moved) < 0.5f && std::fabs(deg) > 0.3f &&
                            ((p <= lo + 1.0f && meant > 0.0f) || (p >= hi - 1.0f && meant < 0.0f));
        const bool away = pinned || (std::fabs(moved) > 6.0f && std::fabs(deg) > 0.3f &&
                                     (moved / deg < 0.0f) != (gain_[a] < 0.0f));
        runaway_votes_[a] = away ? runaway_votes_[a] + 1 : 0;
        if (runaway_votes_[a] >= 2) { // twice in a row: not a late overshoot
            // The other sign; and how far it went says how much, when it could move.
            gain_[a] = pinned ? -gain_[a]
                              : Sign(-gain_[a]) * std::clamp(std::fabs(moved / deg), kMinGain,
                                                             kMaxGain);
            sign_votes_[a] = 0;
            runaway_votes_[a] = 0;
            window_[a] = {};
        }
        mark_ns_[a] = time_ns;
        mark_pos_[a] = a == 0 ? r.x : r.y;
    }
    for (int a = 0; a < 2; a++) {
        Window& w = window_[a];
        // (A crawl under 0.02 degrees in that time counts as rest: its error is a few percent
        // of a window's 0.3 degrees at most.)
        const bool quiet = std::fabs(SentSince(a, time_ns - quiet_ns)) < 0.02f;
        if (!quiet) {
            if (w.start_ns == 0) {
                continue; // a window opens only at a quiet moment
            }
            w.clean = w.clean && free && !pushing;
            continue;
        }
        // A window older than the history cannot be summed.
        const bool whole = sent_count_ < kHistory ||
                           sent_[sent_head_ % kHistory].time_ns < w.start_ns;
        if (w.start_ns != 0 && w.clean && whole && free && !pushing) {
            const float deg = SentSince(a, w.start_ns);
            const float moved = pos[a] - w.pos0;
            if (std::fabs(deg) >= 0.3f && std::fabs(moved) >= 3.0f) {
                const float seen = moved / deg;
                if ((seen < 0.0f) != (gain_[a] < 0.0f)) {
                    // The other way round (the game's invert option): only after it is seen
                    // twice in a row.
                    if (++sign_votes_[a] >= 2) {
                        gain_[a] = std::clamp(seen, -kMaxGain, kMaxGain);
                        if (std::fabs(gain_[a]) < kMinGain) {
                            gain_[a] = Sign(gain_[a]) * kMinGain;
                        }
                        sign_votes_[a] = 0;
                    }
                } else {
                    sign_votes_[a] = 0;
                    // Mostly the new measure: it is exact up to the sub-pixel rest.
                    const float mag = std::exp(0.25f * std::log(std::fabs(gain_[a])) +
                                               0.75f * std::log(std::fabs(seen)));
                    gain_[a] = Sign(gain_[a]) * std::clamp(mag, kMinGain, kMaxGain);
                }
            }
        }
        w.start_ns = time_ns;
        w.pos0 = pos[a];
        w.clean = free && !pushing;
    }
}

LightGunLoop::Out LightGunLoop::Update(int64_t time_ns, const ReticleReading& r,
                                       const LightGunTarget& target, float zone_fraction,
                                       float turn_speed, bool clutch) {
    Out out;
    if (!r.valid || !(r.w > 1.0f) || !(r.h > 1.0f)) {
        Reset();
        return out;
    }
    // What was sent since the last call (the loop's part only: the push is stopped by the box).
    if (have_last_ && time_ns > last_time_ns_) {
        const int64_t dt_ns = std::min<int64_t>(time_ns - last_time_ns_, kMaxGapNs);
        const float dt = static_cast<float>(dt_ns) * 1e-9f;
        Sent s;
        s.time_ns = time_ns;
        s.deg[0] = last_rate_[0] * dt;
        s.deg[1] = last_rate_[1] * dt;
        sent_[sent_head_] = s;
        sent_head_ = (sent_head_ + 1) % kHistory;
        sent_count_ = std::min(sent_count_ + 1, kHistory);
    } else if (have_last_ && time_ns <= last_time_ns_) {
        out.right_dps = last_rate_[0];
        out.down_dps = last_rate_[1];
        return out;
    }
    // The game frame's length: while a rate is being sent the reticle moves every game frame,
    // so the changes seen over that time count the frames (the polls, ~101 Hz, are faster than
    // the game's 60 or 30 frames a second, so none is missed).
    const bool moved = have_last_ && (r.x != last_x_ || r.y != last_y_);
    if (have_last_ && (last_rate_[0] != 0.0f || last_rate_[1] != 0.0f)) {
        count_time_s_ += static_cast<float>(std::min<int64_t>(time_ns - last_time_ns_, kMaxGapNs)) *
                         1e-9f;
        count_changes_ += moved ? 1 : 0;
        if (count_time_s_ >= 0.25f) {
            if (count_changes_ >= 3) {
                const float seen = count_time_s_ / static_cast<float>(count_changes_);
                frame_s_ = std::clamp(frame_s_ + 0.5f * (seen - frame_s_), 1.0f / 120.0f,
                                      1.0f / 15.0f);
            }
            count_time_s_ = 0.0f;
            count_changes_ = 0;
        }
    }
    last_x_ = r.x;
    last_y_ = r.y;
    have_last_ = true;
    last_time_ns_ = time_ns;

    const Vec2f depth = TurnZoneDepth(target, zone_fraction);
    const bool push = r.ground && !clutch && target.valid && (depth.x != 0.0f || depth.y != 0.0f);
    Learn(time_ns, r, push || clutch);
    if (clutch || !target.valid) {
        last_rate_[0] = last_rate_[1] = 0.0f;
        return out;
    }

    // The target in the game's pixels, kept where the reticle can go.
    float tx = target.u * r.w;
    float ty = target.v * r.h;
    float x0 = 0.0f, y0 = 0.0f, x1 = r.w, y1 = r.h;
    if (r.ground) {
        const ReticleBox b = GroundBox(r);
        x0 = b.x0, y0 = b.y0, x1 = b.x1, y1 = b.y1;
    }
    tx = std::clamp(tx, x0, x1);
    ty = std::clamp(ty, y0, y1);

    const int64_t lag_ns = static_cast<int64_t>(kLatencyFrames * frame_s_ * 1e9f);
    const float tau = kTauFrames * frame_s_;
    const float target_px[2]{tx, ty};
    const float now_px[2]{r.x, r.y};
    for (int a = 0; a < 2; a++) {
        if (std::fabs(target_px[a] - last_target_[a]) > 2.0f) {
            flips_[a] = 0; // the pointer moves: a change of side is the hand's, not overshoot
        }
        last_target_[a] = target_px[a];
    }
    float rate[2]{};
    for (int a = 0; a < 2; a++) {
        const float predicted = now_px[a] + gain_[a] * SentSince(a, time_ns - lag_ns);
        const float error = target_px[a] - predicted;
        // Overshooting back and forth: the gain is higher than thought. Halve what is sent.
        const float seen_error = target_px[a] - now_px[a];
        if (std::fabs(seen_error) > 3.0f) {
            const int sign = seen_error > 0.0f ? 1 : -1;
            if (error_sign_[a] != 0 && sign != error_sign_[a]) {
                flips_[a]++;
            }
            error_sign_[a] = sign;
        }
        if (time_ns - flips_since_ns_[a] > static_cast<int64_t>(10.0f * frame_s_ * 1e9f)) {
            flips_since_ns_[a] = time_ns;
            flips_[a] = 0;
        }
        if (flips_[a] >= 3) {
            gain_[a] = std::clamp(gain_[a] * 2.0f, -kMaxGain, kMaxGain);
            flips_[a] = 0;
            flips_since_ns_[a] = time_ns;
        }
        if (std::fabs(error) < kHoldPx) {
            continue;
        }
        rate[a] = error / tau / gain_[a];
        rate[a] = std::clamp(rate[a], -kModMaxDps, kModMaxDps);
    }
    last_rate_[0] = rate[0];
    last_rate_[1] = rate[1];
    out.right_dps = rate[0];
    out.down_dps = rate[1];
    if (push) {
        // One screen width (height) a second at the edge, x Turn Speed, past the box.
        const float s = ClampTurnSpeed(turn_speed);
        out.right_dps += depth.x * s * r.w / gain_[0];
        out.down_dps += depth.y * s * r.h / gain_[1];
        out.right_dps = std::clamp(out.right_dps, -kModMaxDps, kModMaxDps);
        out.down_dps = std::clamp(out.down_dps, -kModMaxDps, kModMaxDps);
        out.turning = true;
    }
    return out;
}

} // namespace FrameXr::Input
