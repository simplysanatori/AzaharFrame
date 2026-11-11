// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame vrsize: see screen_rig.h.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <sstream>

#include "frame_xr/input/geometry.h"
#include "frame_xr/input/screen_rig.h"

namespace FrameXr::Input::ScreenRig {

namespace {
constexpr float kDegToRad = 3.14159265f / 180.0f;
constexpr float kPi = 3.14159265f;

// AzaharFrame move: heading (about +Y, 0 = -Z, positive to the left) and pitch of a direction,
// as vis/stats_place.cpp.
float WrapAngle(float a) {
    while (a > kPi) {
        a -= 2.0f * kPi;
    }
    while (a < -kPi) {
        a += 2.0f * kPi;
    }
    return a;
}

float YawOf(const Vec3f& dir) {
    return std::atan2(-dir.x, -dir.z);
}

float PitchOf(const Vec3f& dir) {
    const float len = Length(dir);
    return len > 0.0f ? std::asin(std::clamp(dir.y / len, -1.0f, 1.0f)) : 0.0f;
}

Vec3f DirectionFrom(float yaw, float pitch) {
    const float c = std::cos(pitch);
    return {-std::sin(yaw) * c, std::sin(pitch), -std::cos(yaw) * c};
}

// Where the beam meets the sphere of `radius` round `eye` (the far side: the hand is inside it),
// else the point that far along the beam.
Vec3f SpherePoint(const Vec3f& eye, const Ray& ray, float radius) {
    const Vec3f m = ray.origin - eye;
    const float b = Dot(m, ray.direction);
    const float c = Dot(m, m) - radius * radius;
    const float disc = b * b - c;
    float t = radius;
    if (disc >= 0.0f) {
        const float far = -b + std::sqrt(disc);
        if (far > 0.0f) {
            t = far;
        }
    }
    return ray.origin + ray.direction * t;
}

bool Finite(const Posef& p) {
    return std::isfinite(p.position.x) && std::isfinite(p.position.y) &&
           std::isfinite(p.position.z) && std::isfinite(p.orientation.x) &&
           std::isfinite(p.orientation.y) && std::isfinite(p.orientation.z) &&
           std::isfinite(p.orientation.w);
}
} // namespace

float ClampScale(float scale) {
    return std::isfinite(scale) ? std::clamp(scale, kMinScale, kMaxScale) : 1.0f;
}

float ScaleWithStick(float scale, float stick_x, float dt, float threshold) {
    if (!(std::abs(stick_x) > threshold)) {
        return ClampScale(scale);
    }
    return ClampScale(scale * std::exp(stick_x * kScaleRate * std::clamp(dt, 0.0f, 0.1f)));
}

float DepthScale(float scale) {
    return 1.0f / std::max(1.0f, ClampScale(scale));
}

Vec3f BottomOffset(float scale) {
    const float s = ClampScale(scale);
    // At scale 1: 0.75 m below the centre, 0.25 m below the top screen's bottom edge.
    return {0.0f, -(kTopHalfHeight * s + (kBottomBelow - kTopHalfHeight)), kBottomNearer};
}

Posef BottomFromTop(const Posef& top, float scale) {
    const Quatf pitch{std::sin(kBottomPitch / 2), 0.0f, 0.0f, std::cos(kBottomPitch / 2)};
    Posef b;
    b.position = top.position + Rotate(top.orientation, BottomOffset(scale));
    b.orientation = Normalized(Multiply(top.orientation, pitch));
    return b;
}

const char* LayoutName(uint32_t layout) {
    return layout == kLayoutSideBySide ? "Side by Side" : "Stacked";
}

std::string LayoutToast(uint32_t layout) {
    return std::string("Screen Layout: ") + LayoutName(layout);
}

uint32_t NextLayout(uint32_t layout) {
    return layout == kLayoutSideBySide ? kLayoutStacked : kLayoutSideBySide;
}

Posef SecondFromMain(const Posef& main, float scale, float main_width, float second_width,
                     uint32_t layout) {
    if (layout != kLayoutSideBySide) {
        return BottomFromTop(main, scale);
    }
    const float horizontal = std::max(std::hypot(main.position.x, main.position.z), 0.1f);
    // The arc between the two middles, on the circle at the main screen's horizontal distance.
    const float angle = (main_width * 0.5f + kSideGap + second_width * 0.5f) / horizontal;
    // Turned to the right about +Y: a negative angle (right-handed, Y up, -Z ahead).
    const Quatf turn{0.0f, std::sin(-angle / 2), 0.0f, std::cos(-angle / 2)};
    Posef s;
    s.position = Rotate(turn, main.position);
    s.orientation = Normalized(Multiply(turn, main.orientation));
    return s;
}

std::optional<Posef> NudgeTop(const Posef& top, int right_steps, int up_steps) {
    const Vec3f p = top.position;
    const float r = Length(p);
    if (r < 1e-3f || (right_steps == 0 && up_steps == 0)) {
        return std::nullopt;
    }
    const float limit = kMaxNudgeAngleDeg * kDegToRad;
    const float azimuth = std::atan2(p.x, -p.z); // positive = right
    const float elevation = std::asin(std::clamp(p.y / r, -1.0f, 1.0f));
    const float step = kNudgeStepDeg * kDegToRad;
    const float a = azimuth + static_cast<float>(right_steps) * step;
    const float e = elevation + static_cast<float>(up_steps) * step;
    // Past the limit only back towards the middle.
    if ((std::abs(a) > limit && std::abs(a) > std::abs(azimuth)) ||
        (std::abs(e) > limit && std::abs(e) > std::abs(elevation))) {
        return std::nullopt;
    }
    const Vec3f moved{r * std::cos(e) * std::sin(a), r * std::sin(e), -r * std::cos(e) * std::cos(a)};
    if (moved.z > kNearestTopZ && moved.z > p.z) {
        return std::nullopt; // as a drag: no nearer than 1 m ahead
    }
    return Posef{FaceViewer(moved, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}), moved};
}

const char* MovedToast(int right_steps, int up_steps) {
    if (right_steps != 0) {
        return right_steps > 0 ? "Moved Right" : "Moved Left";
    }
    return up_steps > 0 ? "Moved Up" : "Moved Down";
}

uint32_t StepCurve(uint32_t deg, int dir) {
    deg = std::min(deg, kMaxCurveDeg);
    if (dir > 0) {
        for (const uint32_t step : kCurveStepsDeg) {
            if (step > deg) {
                return step;
            }
        }
    } else if (dir < 0) {
        for (auto it = kCurveStepsDeg.rbegin(); it != kCurveStepsDeg.rend(); ++it) {
            if (*it < deg) {
                return *it;
            }
        }
    }
    return deg;
}

const char* CurveName(uint32_t deg) {
    static constexpr const char* kNames[] = {"Off",   "15°", "30°", "45°",
                                           "60°", "75°", "90°"};
    static_assert(std::size(kNames) == kCurveStepsDeg.size());
    std::size_t best = 0;
    for (std::size_t i = 1; i < kCurveStepsDeg.size(); i++) {
        const auto off = [deg](uint32_t step) { return deg > step ? deg - step : step - deg; };
        if (off(kCurveStepsDeg[i]) < off(kCurveStepsDeg[best])) {
            best = i;
        }
    }
    return deg == 0 ? kNames[0] : kNames[std::max<std::size_t>(best, 1)];
}

std::string CurveToast(uint32_t deg) {
    return std::string("Screen Curve: ") + CurveName(deg);
}

float CurveRadius(float width, uint32_t deg) {
    deg = std::min(deg, kMaxCurveDeg);
    if (deg == 0 || !(width > 0.0f)) {
        return 0.0f;
    }
    return width / (static_cast<float>(deg) * kDegToRad);
}

void ApplyCurve(PanelQuad& main, PanelQuad& secondary, uint32_t deg) {
    main.curve_radius = CurveRadius(main.width_m, deg);
    secondary.curve_radius = 0.0f; // the second slot's screen is always a flat quad
}

Cylinder CylinderFor(const Posef& screen, float width, float height, float radius) {
    Cylinder c;
    c.radius = radius;
    c.pose.orientation = screen.orientation;
    c.pose.position =
        screen.position + Rotate(screen.orientation, Vec3f{0.0f, 0.0f, radius});
    c.central_angle = radius > 0.0f ? width / radius : 0.0f;
    c.aspect_ratio = height > 0.0f ? width / height : 1.0f;
    return c;
}

// ---- AzaharFrame move: Detach Screens ---------------------------------------------------------

uint32_t ClampDetach(uint32_t mode) {
    return std::min(mode, kDetachBoth);
}

const char* DetachName(uint32_t mode) {
    switch (ClampDetach(mode)) {
    case kDetachOther:
        return "Other";
    case kDetachBoth:
        return "Both";
    default:
        return "Off";
    }
}

std::string DetachToast(uint32_t mode) {
    return std::string("Detach Screens: ") + DetachName(mode);
}

bool SlotFree(uint32_t mode, bool main_slot) {
    mode = ClampDetach(mode);
    return main_slot ? mode == kDetachBoth : mode != kDetachOff;
}

void Relink(Slots& s) {
    s.second_free = false;
    s.second = {};
    s.second_scale = 1.0f;
}

bool Detach(Slots& s, uint32_t mode, const Posef& linked_second) {
    if (ClampDetach(mode) == kDetachOff || s.second_free) {
        return false;
    }
    s.second_free = true;
    s.second = linked_second;
    s.second_scale = 1.0f;
    return true;
}

Slots ForMode(Slots s, uint32_t mode) {
    mode = ClampDetach(mode);
    s.main_scale = ClampScale(s.main_scale);
    s.second_scale = ClampScale(s.second_scale);
    if (mode != kDetachBoth && !(s.main.position.z <= kNearestTopZ)) {
        s.main = Slots{}.main;
    }
    if (mode == kDetachOff) {
        Relink(s);
    }
    return s;
}

void FreeGrab::Begin(const Vec3f& eye, const Posef& slot, const Ray& ray) {
    const Vec3f to_centre = slot.position - eye;
    const Vec3f to_hit = SpherePoint(eye, ray, Length(to_centre)) - eye;
    yaw_offset_ = WrapAngle(YawOf(to_centre) - YawOf(to_hit));
    pitch_offset_ = PitchOf(to_centre) - PitchOf(to_hit);
}

Vec3f FreeGrab::Direction(const Vec3f& eye, const Ray& ray, float distance) const {
    const Vec3f to = SpherePoint(eye, ray, distance) - eye;
    const float limit = kFreeMaxPitchDeg * kDegToRad;
    const float pitch = std::clamp(PitchOf(to) + pitch_offset_, -limit, limit);
    return DirectionFrom(YawOf(to) + yaw_offset_, pitch);
}

Posef FreePose(const Vec3f& eye, const Vec3f& dir, float distance) {
    const Vec3f centre = eye + Normalized(dir) * distance;
    return {FaceViewer(centre, eye, {0.0f, 1.0f, 0.0f}), centre};
}

float FreeDistance(float distance, float delta_z) {
    const float d = std::isfinite(distance) ? distance - delta_z : kFreeMinDistance;
    return std::clamp(d, kFreeMinDistance, kFreeMaxDistance);
}

Posef NudgeFree(const Posef& slot, const Vec3f& eye, int right_steps) {
    const Vec3f to = slot.position - eye;
    // A step to the right is a negative turn about +Y.
    const float a = -static_cast<float>(right_steps) * kNudgeStepDeg * kDegToRad;
    const Vec3f turned{to.x * std::cos(a) + to.z * std::sin(a), to.y,
                       -to.x * std::sin(a) + to.z * std::cos(a)};
    const Vec3f centre = eye + turned;
    return {FaceViewer(centre, eye, {0.0f, 1.0f, 0.0f}), centre};
}

void PlacementFile::Parse(const std::string& text) {
    entries_.clear();
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line.resize(hash);
        }
        std::istringstream words(line);
        std::string id, kind;
        Slot slot;
        Posef& p = slot.pose;
        if (!(words >> id >> kind >> p.position.x >> p.position.y >> p.position.z >>
              p.orientation.x >> p.orientation.y >> p.orientation.z >> p.orientation.w >>
              slot.scale)) {
            continue;
        }
        if (id.size() != 16 || id.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos ||
            (kind != "main" && kind != "second") || !Finite(p) || !std::isfinite(slot.scale)) {
            continue;
        }
        const float q = std::sqrt(p.orientation.x * p.orientation.x +
                                  p.orientation.y * p.orientation.y +
                                  p.orientation.z * p.orientation.z +
                                  p.orientation.w * p.orientation.w);
        const float r = Length(p.position);
        if (q < 0.5f || r < kFreeMinDistance * 0.5f || r > kFreeMaxDistance * 2.0f) {
            continue;
        }
        p.orientation = Normalized(p.orientation);
        slot.scale = ClampScale(slot.scale);
        Entry& e = entries_[std::strtoull(id.c_str(), nullptr, 16)];
        (kind == "main" ? e.main : e.second) = slot;
    }
}

std::string PlacementFile::Format() const {
    std::string out = "# Azahar for Frame: screen placement per game\n"
                      "# title id, main or second slot, position x y z in metres, orientation "
                      "x y z w, size\n";
    const auto line = [&out](uint64_t title, const char* kind, const Slot& s) {
        char buf[200];
        std::snprintf(buf, sizeof(buf), "%016llx %s %.3f %.3f %.3f %.4f %.4f %.4f %.4f %.3f\n",
                      static_cast<unsigned long long>(title), kind, s.pose.position.x,
                      s.pose.position.y, s.pose.position.z, s.pose.orientation.x,
                      s.pose.orientation.y, s.pose.orientation.z, s.pose.orientation.w, s.scale);
        out += buf;
    };
    for (const auto& [title, e] : entries_) {
        if (!e.main) {
            continue; // a second slot alone is of no use
        }
        line(title, "main", *e.main);
        if (e.second) {
            line(title, "second", *e.second);
        }
    }
    return out;
}

std::optional<Slots> PlacementFile::Get(uint64_t title_id) const {
    const auto it = entries_.find(title_id);
    if (it == entries_.end() || !it->second.main) {
        return std::nullopt;
    }
    Slots s;
    s.main = it->second.main->pose;
    s.main_scale = it->second.main->scale;
    if (it->second.second) {
        s.second_free = true;
        s.second = it->second.second->pose;
        s.second_scale = it->second.second->scale;
    }
    return s;
}

void PlacementFile::Set(uint64_t title_id, const Slots& slots) {
    Entry e;
    e.main = Slot{slots.main, ClampScale(slots.main_scale)};
    if (slots.second_free) {
        e.second = Slot{slots.second, ClampScale(slots.second_scale)};
    }
    entries_[title_id] = e;
}

void PlacementFile::Erase(uint64_t title_id) {
    entries_.erase(title_id);
}

DpadNudger::Steps DpadNudger::Filter(InputFrame& in, bool holding) {
    bool* const pressed[4] = {&in.dpad_right, &in.dpad_left, &in.dpad_up, &in.dpad_down};
    static constexpr int kRight[4] = {1, -1, 0, 0};
    static constexpr int kUp[4] = {0, 0, 1, -1};
    Steps steps;
    for (std::size_t i = 0; i < dirs_.size(); i++) {
        Dir& d = dirs_[i];
        const bool now = *pressed[i];
        const bool edge = now && !d.down;
        d.down = now;
        if (!now) {
            d.swallowed = false;
            continue;
        }
        if (holding) {
            d.swallowed = true;
            bool step = false;
            if (edge) {
                step = true;
                d.next_ns = in.time_ns + kRepeatDelayNs;
            } else if (in.time_ns >= d.next_ns && d.next_ns != 0) {
                step = true;
                d.next_ns = in.time_ns + kRepeatNs;
            }
            if (step) {
                steps.right += kRight[i];
                steps.up += kUp[i];
            }
            if (edge) {
                steps.up_presses += kUp[i];
            }
        } else {
            d.next_ns = 0; // a held direction repeats again only after a new press while holding
        }
        if (d.swallowed) {
            *pressed[i] = false;
        }
    }
    return steps;
}

void DpadNudger::Reset() {
    dirs_ = {};
}

} // namespace FrameXr::Input::ScreenRig
