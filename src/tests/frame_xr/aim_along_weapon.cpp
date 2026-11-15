// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuinteg: Angel's Aim's one ray along the weapon (the owner's option A): from the
// muzzle along the weapon's own axis as held (vis/weapon_fit.h FitAxis, input/gyro_aim.h
// AngelsAimRay); the test switch AZAHARFRAME_ANGELS_AIM_RAY=hand gives kiuaim2's ray back. The
// holds here are made up (round turns and offsets), nothing from the game.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/gyro_aim.h"
#include "frame_xr/test_switches.h"
#include "frame_xr/vis/weapon_fit.h"

using Catch::Approx;
using namespace FrameXr::Input;
namespace Vis = FrameXr::Vis;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

Quatf AxisAngle(Vec3f axis, float degrees) {
    const Vec3f n = Normalized(axis);
    const float half = degrees * kDeg * 0.5f;
    const float s = std::sin(half);
    return Quatf{n.x * s, n.y * s, n.z * s, std::cos(half)};
}

/// An orientation whose -Z is `dir`, rolled `roll` degrees about it.
Quatf Pointing(Vec3f dir, float roll) {
    const Vec3f d = Normalized(dir);
    const float yaw = std::atan2(-d.x, -d.z) / kDeg;
    const float pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f)) / kDeg;
    return Multiply(Multiply(AxisAngle({0, 1, 0}, yaw), AxisAngle({1, 0, 0}, pitch)),
                    AxisAngle({0, 0, -1}, roll));
}

/// The shortest turn taking unit `a` to unit `b`.
Quatf FromTo(Vec3f a, Vec3f b) {
    a = Normalized(a);
    b = Normalized(b);
    const Vec3f c = Cross(a, b);
    const float d = std::clamp(Dot(a, b), -1.0f, 1.0f);
    if (Length(c) < 1e-7f) {
        return d > 0.0f ? Quatf{0, 0, 0, 1} : AxisAngle({0, 1, 0}, 180.0f);
    }
    return AxisAngle(c, std::acos(d) / kDeg);
}

PanelQuad Screen(float width) {
    PanelQuad q;
    q.visible = true;
    q.width_m = width;
    q.height_m = width * 0.6f;
    q.pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
    q.pose.position = {0.0f, 1.2f, -2.0f};
    return q;
}

Vec3f V(const std::array<float, 3>& a) {
    return {a[0], a[1], a[2]};
}

/// A made-up hold: the weapon turned (degrees about x, then y, then z) into Pit's hand, its
/// origin at `at` there, the fist's centre at `fist` (hand space).
Vis::FitHold MakeHold(float rx, float ry, float rz, std::array<float, 3> at,
                      std::array<float, 3> fist, bool left = false) {
    const auto rot = [](float deg, int axis) {
        const float c = std::cos(deg * kDeg), s = std::sin(deg * kDeg);
        std::array<float, 9> m{1, 0, 0, 0, 1, 0, 0, 0, 1};
        const int i = (axis + 1) % 3, j = (axis + 2) % 3;
        m[i * 3 + i] = c;
        m[i * 3 + j] = -s;
        m[j * 3 + i] = s;
        m[j * 3 + j] = c;
        return m;
    };
    const auto mul = [](const std::array<float, 9>& a, const std::array<float, 9>& b) {
        std::array<float, 9> m{};
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                for (int k = 0; k < 3; k++) {
                    m[r * 3 + c] += a[r * 3 + k] * b[k * 3 + c];
                }
            }
        }
        return m;
    };
    const auto r = mul(rot(rz, 2), mul(rot(ry, 1), rot(rx, 0)));
    Vis::FitHold h;
    h.to_hand = {r[0], r[1], r[2], at[0], r[3], r[4], r[5], at[1], r[6], r[7], r[8], at[2]};
    h.fist = fist;
    h.left = left;
    return h;
}

struct ClassCase {
    const char* name;
    Vis::FitWeapon w;
};

/// One weapon per class, with a made-up hold (the game's hold classes and the others).
std::vector<ClassCase> Classes() {
    std::vector<ClassCase> c;
    // Blade: the hilt in the fist, the blade out along -x and -z, turned half a turn about x.
    c.push_back({"blade", {0, {-4.5f, 0.1f, -10.0f}, true,
                           MakeHold(180.0f, 7.0f, 0.0f, {-0.9f, -0.08f, 0.0f}, {-0.9f, -0.08f, 0.0f})}});
    c.push_back({"staff", {1, {0.0f, 0.0f, 9.0f}, true,
                           MakeHold(0.0f, 0.0f, 10.0f, {-0.9f, 0.0f, 0.2f}, {-0.9f, 0.0f, 0.2f})}});
    c.push_back({"claws", {2, {-1.2f, 0.0f, 0.1f}, true,
                           MakeHold(0.0f, 0.0f, 0.0f, {0, 0, 0}, {-0.6f, 0.0f, 0.0f})}});
    // Bow: no muzzle of its own (the hand), held in Pit's left hand.
    c.push_back({"bow", {3, {}, false,
                         MakeHold(0.0f, 5.0f, 0.0f, {0.6f, 0.0f, 0.1f}, {0.6f, 0.0f, 0.1f}, true)}});
    c.push_back({"palm", {4, {-1.6f, 0.0f, 0.0f}, true,
                          MakeHold(0.0f, 0.0f, 0.0f, {0, 0, 0}, {-0.6f, 0.0f, 0.0f})}});
    c.push_back({"club", {5, {-3.0f, 0.0f, -24.0f}, true,
                          MakeHold(180.0f, 4.0f, 0.0f, {-0.9f, -0.08f, 0.0f}, {-0.9f, -0.08f, 0.0f})}});
    // Cannon: on the forearm, its barrel out along -x.
    c.push_back({"cannon", {6, {-6.0f, 0.3f, 0.0f}, true,
                            MakeHold(0.0f, 0.0f, 0.0f, {0, 0, 0}, {-2.17f, 0.0f, 0.0f})}});
    c.push_back({"orbitars", {7, {0.0f, 0.0f, 1.0f}, true, std::nullopt}});
    c.push_back({"arm", {8, {-2.0f, 0.0f, 0.0f}, true,
                         MakeHold(0.0f, 0.0f, 0.0f, {0, 0, 0}, {-0.6f, 0.0f, 0.0f})}});
    return c;
}

const std::vector<Vis::FitSide> kCentre{Vis::FitSide::Centre};
const std::vector<Vis::FitSide> kHalves{Vis::FitSide::Right, Vis::FitSide::Left};

const std::vector<Vis::FitSide>& SidesFor(int cls) {
    return cls == 2 || cls == 7 ? kHalves : kCentre;
}

bool SameBits(const Ray& a, const Ray& b) {
    return std::memcmp(&a.origin, &b.origin, sizeof(a.origin)) == 0 &&
           std::memcmp(&a.direction, &b.direction, sizeof(a.direction)) == 0;
}

/// The aim pose that puts the weapon's ray (from `muzzle` along `axis`, aim space) on `at`,
/// turned `roll` degrees about that ray (the hand rolled about where it aims).
Posef AimAt(const Vec3f& hand, const Vec3f& at, const std::optional<Vec3f>& muzzle,
            const std::optional<Vec3f>& axis, float roll) {
    const Quatf to_axis = axis ? FromTo({0, 0, -1}, *axis) : Quatf{0, 0, 0, 1};
    Posef aim{Pointing(at - hand, roll), hand};
    // The muzzle turns with the hand: a few steps, each half way (a long blade's tip is far out).
    for (int i = 0; i < 200; i++) {
        const Ray ray = AimRay(aim, muzzle, axis);
        const Quatf next = Normalized(Multiply(Pointing(at - ray.origin, roll), Conjugate(to_axis)));
        const float d = Dot(Vec3f{next.x, next.y, next.z}, Vec3f{aim.orientation.x, aim.orientation.y,
                                                                  aim.orientation.z}) +
                        next.w * aim.orientation.w;
        const float s = d < 0.0f ? -1.0f : 1.0f;
        aim.orientation = Normalized(Quatf{(aim.orientation.x + s * next.x) * 0.5f,
                                           (aim.orientation.y + s * next.y) * 0.5f,
                                           (aim.orientation.z + s * next.z) * 0.5f,
                                           (aim.orientation.w + s * next.w) * 0.5f});
    }
    return aim;
}

} // namespace

TEST_CASE("Angel's Aim along the weapon: the axis per class", "[frame_xr][kiuinteg]") {
    for (const ClassCase& c : Classes()) {
        for (const bool left : {false, true}) {
            INFO(c.name << (left ? ", left hand" : ", right hand"));
            const auto axis = Vis::FitAxis(c.w, left, SidesFor(c.w.cls));
            const bool held = Vis::HoldMapFor(c.w.cls) != Vis::HoldMap::None && c.w.hold;
            CHECK(axis.has_value() == held);
            if (!axis) {
                continue;
            }
            CHECK(Length(V(*axis)) == Approx(1.0f).margin(1e-5f));
            CHECK((*axis)[2] < 0.0f); // away from the player
            const auto muzzle = Vis::FitMuzzle(c.w, left, SidesFor(c.w.cls));
            const auto fist = Vis::FitFist(c.w, left, SidesFor(c.w.cls));
            REQUIRE(muzzle);
            REQUIRE(fist);
            if (Vis::HoldMapFor(c.w.cls) == Vis::HoldMap::Grip) {
                // From the fist to the muzzle: the blade, the shaft as held.
                const Vec3f along = Normalized(V(*muzzle) - V(*fist));
                CHECK(Length(along - V(*axis)) < 1e-4f);
            }
            // The left hand's is the right hand's mirrored.
            const auto right = Vis::FitAxis(c.w, false, SidesFor(c.w.cls));
            if (left) {
                CHECK((*axis)[0] == Approx(-(*right)[0]).margin(1e-5f));
                CHECK((*axis)[1] == Approx((*right)[1]).margin(1e-5f));
                CHECK((*axis)[2] == Approx((*right)[2]).margin(1e-5f));
            }
        }
    }
}

TEST_CASE("Angel's Aim along the weapon: the ray from the tip hits where the reticle goes, "
          "every class, every roll",
          "[frame_xr][kiuinteg]") {
    const PanelQuad top = Screen(1.6f);
    const Vec3f hand{0.2f, 1.0f, -0.3f};
    for (const ClassCase& c : Classes()) {
        for (const bool left : {false, true}) {
            const auto mz = Vis::FitMuzzle(c.w, left, SidesFor(c.w.cls));
            const auto ax = Vis::FitAxis(c.w, left, SidesFor(c.w.cls));
            const std::optional<Vec3f> muzzle = mz ? std::optional<Vec3f>{V(*mz)} : std::nullopt;
            const std::optional<Vec3f> axis = ax ? std::optional<Vec3f>{V(*ax)} : std::nullopt;
            for (const Vec2f& p : std::vector<Vec2f>{{0.5f, 0.5f}, {0.1f, 0.15f}, {0.9f, 0.85f}}) {
                const Vec3f at = PointOnQuad(top, p.x, p.y);
                for (const float roll : {0.0f, 30.0f, 90.0f, 180.0f, 270.0f}) {
                    INFO(c.name << (left ? " L" : " R") << " at " << p.x << ", " << p.y
                                << " roll " << roll);
                    const Posef aim = AimAt(hand, at, muzzle, axis, roll);
                    const Ray ray = AngelsAimRay(aim, muzzle, axis, AimRayFrom::Weapon);
                    // It starts at the weapon's tip and runs along its axis as held.
                    if (muzzle) {
                        CHECK(Length(ray.origin - (hand + Rotate(aim.orientation, *muzzle))) <
                              1e-5f);
                    }
                    const Vec3f along = axis ? Rotate(aim.orientation, *axis) : AimDirection(aim);
                    CHECK(Length(ray.direction - Normalized(along)) < 1e-5f);
                    // The reticle goes where that ray meets the top screen: the point aimed at,
                    // and the beam drawn from the tip ends there (one ray).
                    const LightGunTarget t = LightGunTargetFor(ray, top);
                    REQUIRE(t.valid);
                    CHECK(t.u == Approx(p.x).margin(2e-3f));
                    CHECK(t.v == Approx(p.y).margin(2e-3f));
                    CHECK(Length(PointOnQuad(top, t.u, t.v) - t.point) < 1e-4f);
                    const Vec3f beam = t.point - ray.origin;
                    CHECK(Length(Cross(Normalized(beam), ray.direction)) < 1e-4f);
                    CHECK(Dot(beam, ray.direction) > 0.0f);
                    // Keep Reticle on Screen takes the same ray: on the screen, the same point.
                    const LightGunTarget locked = LockedTargetFor(ray, top);
                    CHECK_FALSE(locked.edge);
                    CHECK(Length(locked.point - t.point) < 1e-5f);
                }
            }
        }
    }
}

TEST_CASE("Angel's Aim along the weapon: a blade's tilt, and rolling the wrist moves the aim",
          "[frame_xr][kiuinteg]") {
    const ClassCase blade = Classes()[0];
    const auto ax = Vis::FitAxis(blade.w, false, kCentre);
    const auto mz = Vis::FitMuzzle(blade.w, false, kCentre);
    REQUIRE(ax);
    REQUIRE(mz);
    const Vec3f axis = V(*ax), muzzle = V(*mz);
    // The weapon's line is not the controller's: the ray turns by the same angle at any roll.
    const float tilt = std::acos(std::clamp(-axis.z, -1.0f, 1.0f)) / kDeg;
    REQUIRE(tilt > 3.0f);
    const PanelQuad top = Screen(1.6f);
    const Vec3f hand{0.0f, 1.0f, -0.3f};
    const Vec3f at = PointOnQuad(top, 0.5f, 0.5f);
    std::vector<Vec3f> hits;
    for (const float roll : {0.0f, 90.0f, 180.0f, 270.0f}) {
        const Posef aim{Pointing(at - hand, roll), hand};
        const Ray ray = AngelsAimRay(aim, muzzle, axis, AimRayFrom::Weapon);
        CHECK(std::acos(std::clamp(Dot(ray.direction, AimDirection(aim)), -1.0f, 1.0f)) / kDeg ==
              Approx(tilt).margin(0.01f));
        const LightGunTarget t = LockedTargetFor(ray, top);
        REQUIRE(t.valid);
        hits.push_back(t.point);
        // The beam still ends on the reticle.
        CHECK(Length(PointOnQuad(top, t.u, t.v) - t.point) < 1e-4f);
    }
    // The accepted trade-off: the tilt is fixed in the hand, so a rolled wrist aims elsewhere.
    CHECK(Length(hits[0] - hits[2]) > 0.1f);
}

TEST_CASE("Angel's Aim along the weapon: Keep Reticle on Screen's resting guard measures the "
          "weapon's ray",
          "[frame_xr][kiuinteg]") {
    const ClassCase blade = Classes()[0];
    const Vec3f axis = V(*Vis::FitAxis(blade.w, false, kCentre));
    const Vec3f muzzle = V(*Vis::FitMuzzle(blade.w, false, kCentre));
    const float tilt = std::acos(std::clamp(-axis.z, -1.0f, 1.0f)) / kDeg;
    REQUIRE(axis.y > 0.05f); // this blade rises from the hand
    const PanelQuad top = Screen(1.6f);
    const Vec3f hand{0.0f, 1.0f, -0.3f};
    // The controller's own line above the top edge, short of the guard, where the blade's
    // line, higher by its tilt, is past it.
    const Vec3f edge = PointOnQuad(top, 0.5f, 0.0f);
    const Vec3f to_edge = Normalized(edge - hand);
    const Vec3f side = Normalized(Cross(to_edge, {0, 1, 0}));
    std::optional<Posef> found;
    for (float beta = 0.0f; beta < 40.0f && !found; beta += 0.25f) {
        const Posef p{Pointing(Rotate(AxisAngle(side, beta), to_edge), 0.0f), hand};
        const LightGunTarget h = LockedTargetFor(AngelsAimRay(p, muzzle, axis, AimRayFrom::Hand), top);
        const LightGunTarget w = LockedTargetFor(AngelsAimRay(p, muzzle, axis, AimRayFrom::Weapon), top);
        if (h.valid && w.valid && !h.resting && w.resting) {
            found = p;
        }
    }
    REQUIRE(found);
    const Posef aim = *found;
    const auto check = [&](const Ray& ray) {
        const LightGunTarget t = LockedTargetFor(ray, top);
        REQUIRE(t.valid);
        CHECK(t.edge);
        // Its angle is the one between this ray and the edge point, from this ray's start.
        const float measured =
            std::acos(std::clamp(Dot(ray.direction, Normalized(t.point - ray.origin)), -1.0f, 1.0f)) /
            kDeg;
        CHECK(t.past_deg == Approx(measured).margin(0.05f));
        CHECK(t.resting == (t.past_deg > kRestingPastDeg));
        return t;
    };
    const LightGunTarget by_hand = check(AngelsAimRay(aim, muzzle, axis, AimRayFrom::Hand));
    const LightGunTarget by_weapon = check(AngelsAimRay(aim, muzzle, axis, AimRayFrom::Weapon));
    CHECK_FALSE(by_hand.resting);
    CHECK(by_weapon.resting);
    CHECK(by_weapon.past_deg > by_hand.past_deg + 0.25f * tilt);
}

TEST_CASE("Angel's Aim along the weapon: AZAHARFRAME_ANGELS_AIM_RAY=hand is kiuaim2's ray, bit "
          "for bit",
          "[frame_xr][kiuinteg][test_switches]") {
    CHECK(std::string{kAimRaySwitch} == "AZAHARFRAME_ANGELS_AIM_RAY");
    CHECK(AimRayFromSwitch(std::string{"hand"}) == AimRayFrom::Hand);
    CHECK(AimRayFromSwitch(std::nullopt) == AimRayFrom::Weapon);
    CHECK(AimRayFromSwitch(std::string{"weapon"}) == AimRayFrom::Weapon);
    CHECK(AimRayFromSwitch(std::string{""}) == AimRayFrom::Weapon);
    CHECK(AimRayFromSwitch(std::string{"Hand"}) == AimRayFrom::Weapon);
    CHECK(FrameXr::TestSwitches::ApplyFor(kAimRaySwitch) == FrameXr::TestSwitches::Apply::Live);
    const Vec3f hand{0.13f, 1.07f, -0.29f};
    for (const ClassCase& c : Classes()) {
        const auto mz = Vis::FitMuzzle(c.w, false, SidesFor(c.w.cls));
        const auto ax = Vis::FitAxis(c.w, false, SidesFor(c.w.cls));
        const std::optional<Vec3f> muzzle = mz ? std::optional<Vec3f>{V(*mz)} : std::nullopt;
        const std::optional<Vec3f> axis = ax ? std::optional<Vec3f>{V(*ax)} : std::nullopt;
        for (const float roll : {0.0f, 33.0f, 90.0f, 181.0f}) {
            for (const float yaw : {-20.0f, 0.0f, 12.5f}) {
                INFO(c.name << " roll " << roll << " yaw " << yaw);
                const Posef aim{Multiply(AxisAngle({0, 1, 0}, yaw), Pointing({0.1f, 0.2f, -1.0f}, roll)),
                                hand};
                // kiuaim2's ray, as it was worked out (AimRay with the muzzle alone).
                Ray old;
                old.direction = Rotate(Normalized(aim.orientation), Vec3f{0.0f, 0.0f, -1.0f});
                old.origin = muzzle ? aim.position + Rotate(Normalized(aim.orientation), *muzzle)
                                    : aim.position;
                CHECK(SameBits(AimRay(aim, muzzle), old));
                CHECK(SameBits(AngelsAimRay(aim, muzzle, axis, AimRayFrom::Hand), old));
                // No axis (a class fitted along the ray, or no weapon): the same ray either way.
                CHECK(SameBits(AngelsAimRay(aim, muzzle, std::nullopt, AimRayFrom::Weapon), old));
                if (axis) {
                    // Along the weapon: the same start, its own direction (a bow's or a
                    // cannon's held along the hand's line: the controller's here).
                    const Ray along = AngelsAimRay(aim, muzzle, axis, AimRayFrom::Weapon);
                    CHECK(std::memcmp(&along.origin, &old.origin, sizeof(old.origin)) == 0);
                    const float off = std::acos(std::clamp(-axis->z, -1.0f, 1.0f)) / kDeg;
                    const float turned =
                        std::acos(std::clamp(Dot(along.direction, old.direction), -1.0f, 1.0f)) /
                        kDeg;
                    CHECK(turned == Approx(off).margin(0.05f));
                }
            }
        }
    }
}

TEST_CASE("Angel's Aim along the weapon: Check Aim with the weapon's ray, at any roll",
          "[frame_xr][kiuinteg]") {
    const PanelQuad top = Screen(1.6f);
    const Vec3f hand{0.1f, 1.0f, -0.2f};
    for (const ClassCase& c : Classes()) {
        const auto mz = Vis::FitMuzzle(c.w, false, SidesFor(c.w.cls));
        const auto ax = Vis::FitAxis(c.w, false, SidesFor(c.w.cls));
        if (!ax) {
            continue;
        }
        const Vec3f muzzle = V(*mz), axis = V(*ax);
        std::optional<AimMap> first;
        for (const float roll : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
            INFO(c.name << " roll " << roll);
            AimCheckFlow flow;
            flow.Start({0.0f, 0.0f, 1.0f, 1.0f}, 400.0f, 240.0f);
            AimCheckFlow::Out out;
            int n = 0;
            while (flow.active()) {
                // The game shows the reticle 2 px right of where it is written: the player puts
                // the shown reticle on the target with the weapon's own ray.
                const Vec2f target = flow.targets()[static_cast<std::size_t>(flow.step())];
                const Vec2f want{target.x - 2.0f / 400.0f, target.y};
                const Posef aim =
                    AimAt(hand, PointOnQuad(top, want.x, want.y), muzzle, axis, roll);
                const Ray ray = AngelsAimRay(aim, muzzle, axis, AimRayFrom::Weapon);
                out = flow.Update(true, LightGunTargetFor(ray, top));
                REQUIRE(++n < 40);
            }
            REQUIRE(out.done);
            REQUIRE(out.result);
            if (!first) {
                first = out.result;
            }
            CHECK(out.result->u[0] * 400.0f == Approx(2.0f).margin(0.1f));
            CHECK(out.result->u[1] == Approx(1.0f).margin(1e-3f));
            CHECK(out.result->v[0] == Approx(0.0f).margin(1e-3f));
            CHECK(out.result->u[0] == Approx(first->u[0]).margin(1e-4f));
            CHECK(out.result->error_px < 1.0f);
        }
    }
}
