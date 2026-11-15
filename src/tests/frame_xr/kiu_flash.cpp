// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuflash: Muzzle Flash: which effect models are a weapon's flashes, the flash's
// timing and fade on the game's shots (rapid or charged), where its cards go (along the aim ray,
// facing the eye) for any pose, their size limits, the live tune, the setting's rollback mirror.
// The converter's side (cards, files, the cache) is in tests/core/frame_weapon_models.cpp.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "core/frame_weapon_models.h"
#include "frame_xr/controls_presets.h"
#include "frame_xr/input/gyro_aim.h"
#include "frame_xr/vis/model_file.h"
#include "frame_xr/vis/muzzle_flash.h"
#include "frame_xr/vis/projectile.h"
#include "frame_xr/vis/weapon_fit.h"

using Catch::Approx;
namespace Vis = FrameXr::Vis;
using FrameWeapons::PickFlashModels;

namespace {

constexpr int64_t kMs = 1'000'000;
using V3 = std::array<float, 3>;

V3 Col(const std::array<float, 16>& m, int c) {
    return {m[static_cast<std::size_t>(c * 4)], m[static_cast<std::size_t>(c * 4 + 1)],
            m[static_cast<std::size_t>(c * 4 + 2)]};
}
float Dot(const V3& a, const V3& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
float Len(const V3& a) {
    return std::sqrt(Dot(a, a));
}
V3 Sub(const V3& a, const V3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
V3 Unit(const V3& a) {
    const float l = Len(a);
    return {a[0] / l, a[1] / l, a[2] / l};
}

} // namespace

TEST_CASE("Muzzle Flash: each class's flash models by name", "[kiuflash]") {
    // A blade: RapidFlashA and ShotFlashA; the dash shot and bullets are not flashes.
    auto f = PickFlashModels(1, {"EffRifle01Bullet1", "PtcRifle01Bullet1", "PtcRifle01RapidDashShot",
                                 "PtcRifle01RapidFlashA", "PtcRifle01ShotFlashA"});
    CHECK(f.rapid == "PtcRifle01RapidFlashA");
    CHECK(f.charged == "PtcRifle01ShotFlashA");
    // The first blade: ShotA, ShotB.
    f = PickFlashModels(0, {"PtcRifle00RapidDashShot", "PtcRifle00ShotA", "PtcRifle00ShotB"});
    CHECK(f.rapid == "PtcRifle00ShotA");
    CHECK(f.charged == "PtcRifle00ShotB");
    // A staff: the plain ShotFlash before ShotFlash_Line, whatever the order.
    f = PickFlashModels(12, {"PtcSnipe00RapidFlash", "PtcSnipe00ShotFlash_Line", "PtcSnipe00ShotFlash"});
    CHECK(f.rapid == "PtcSnipe00RapidFlash");
    CHECK(f.charged == "PtcSnipe00ShotFlash");
    // Claws: RapidShot.
    f = PickFlashModels(25, {"PtcNail01RapidShot", "PtcNail01ShotFlash", "PtcNail01ShotSub"});
    CHECK(f.rapid == "PtcNail01RapidShot");
    CHECK(f.charged == "PtcNail01ShotFlash");
    // A palm: RapidFlash, not RapidDashFlash.
    f = PickFlashModels(49, {"PtcPalm01RapidDashFlash", "PtcPalm01RapidFlash", "PtcPalm01ShotFlash"});
    CHECK(f.rapid == "PtcPalm01RapidFlash");
    CHECK(f.charged == "PtcPalm01ShotFlash");
    // A club: Muzzle for rapid fire, ShotFlash (not ShotFlash2) charged.
    f = PickFlashModels(61, {"PtcPillar01Muzzle", "PtcPillar01ShotFlash2", "PtcPillar01ShotFlash"});
    CHECK(f.rapid == "PtcPillar01Muzzle");
    CHECK(f.charged == "PtcPillar01ShotFlash");
    // The first orbitars: Bullet01 and Bullet02.
    f = PickFlashModels(84, {"PtcBit00Bullet01_FlashA", "PtcBit00Bullet02_FlashA"});
    CHECK(f.rapid == "PtcBit00Bullet01_FlashA");
    CHECK(f.charged == "PtcBit00Bullet02_FlashA");
    // Another weapon's names are not this one's.
    f = PickFlashModels(2, {"PtcRifle01RapidFlashA", "PtcRifle01ShotFlashA"});
    CHECK(f.rapid.empty());
    CHECK(f.charged.empty());
    // One kind only: the other takes it.
    f = PickFlashModels(97, {"PtcArm01RapidFlash"});
    CHECK(f.rapid == "PtcArm01RapidFlash");
    CHECK(f.charged == "PtcArm01RapidFlash");
    f = PickFlashModels(97, {"PtcArm01ShotFlashA"});
    CHECK(f.rapid == "PtcArm01ShotFlashA");
    // None (one claw's file has no effects): both empty; out of range too.
    CHECK(PickFlashModels(28, {}).rapid.empty());
    CHECK(PickFlashModels(108, {"PtcArm12RapidFlash"}).rapid.empty());
    CHECK(PickFlashModels(-1, {}).charged.empty());
}

TEST_CASE("Muzzle Flash: a flash per real shot, rapid or charged, and its fade", "[kiuflash]") {
    Vis::MuzzleFlash m;
    int64_t t = 1'000 * kMs;
    // The first read only takes the total: no flash for shots before it.
    m.Update(t, 500, true, true);
    CHECK_FALSE(m.Current(t).shown);
    // A shot: a rapid flash for kRapidNs (the first shot is never charged).
    t += 16 * kMs;
    m.Update(t, 501, true, true);
    auto f = m.Current(t);
    REQUIRE(f.shown);
    CHECK_FALSE(f.charged);
    CHECK(f.age == Approx(0.0f));
    CHECK(f.duration_s == Approx(0.09f)); // AzaharFrame kiuweap2: was 70 ms
    const uint32_t first = f.shot;
    f = m.Current(t + 45 * kMs);
    CHECK(f.shown);
    CHECK(f.age == Approx(0.5f));
    CHECK_FALSE(m.Current(t + Vis::MuzzleFlash::kRapidNs).shown);
    // No new shot: nothing more, however long.
    m.Update(t + 50 * kMs, 501, true, true);
    CHECK_FALSE(m.Current(t + 200 * kMs).shown);
    // Rapid fire: each read with more shots starts a new flash (several at once: one).
    t += 125 * kMs;
    m.Update(t, 503, true, true);
    f = m.Current(t + 10 * kMs);
    CHECK(f.shown);
    CHECK_FALSE(f.charged);
    CHECK(f.shot == first + 1);
    // After the charge pause (Recoil's heuristic): the charged flash, kChargedNs long.
    t += FrameXr::Input::Recoil::kChargePauseNs;
    m.Update(t, 504, true, true);
    f = m.Current(t + 80 * kMs);
    CHECK(f.shown);
    CHECK(f.charged);
    CHECK(f.duration_s == Approx(0.12f)); // AzaharFrame kiuweap2: was 100 ms
    CHECK_FALSE(m.Current(t + Vis::MuzzleFlash::kChargedNs).shown);
    // Just under the pause: rapid.
    t += FrameXr::Input::Recoil::kChargePauseNs - 50 * kMs;
    m.Update(t, 505, true, true);
    CHECK_FALSE(m.Current(t).charged);
    // Off (or the weapon not shown): the flash goes at once, shots meanwhile flash nothing.
    m.Update(t + 5 * kMs, 505, true, false);
    CHECK_FALSE(m.Current(t + 5 * kMs).shown);
    m.Update(t + 20 * kMs, 520, true, false);
    m.Update(t + 40 * kMs, 530, true, true); // back: the total is taken again
    CHECK_FALSE(m.Current(t + 40 * kMs).shown);
    m.Update(t + 56 * kMs, 531, true, true);
    CHECK(m.Current(t + 56 * kMs).shown);
    // The record unreadable (a menu, a load): nothing, and nothing for its jump when back.
    m.Update(t + 70 * kMs, 531, false, true);
    CHECK_FALSE(m.Current(t + 70 * kMs).shown);
    m.Update(t + 90 * kMs, 900, true, true);
    CHECK_FALSE(m.Current(t + 90 * kMs).shown);
    // AZAHARFRAME_KIU_FLASH's duration.
    m.SetDurationMs(300);
    m.Update(t + 110 * kMs, 901, true, true);
    CHECK(m.Current(t + 390 * kMs).shown);
    CHECK_FALSE(m.Current(t + 410 * kMs).shown);
    m.SetDurationMs(0);
    m.Reset();
    CHECK_FALSE(m.Current(t + 120 * kMs).shown);
}

TEST_CASE("Muzzle Flash: the fade and the growth", "[kiuflash]") {
    CHECK(Vis::FlashIntensity(0.0f) == Approx(1.0f));
    CHECK(Vis::FlashIntensity(0.15f) == Approx(1.0f));
    CHECK(Vis::FlashIntensity(1.0f) == 0.0f);
    CHECK(Vis::FlashIntensity(-0.1f) == 0.0f);
    CHECK(Vis::FlashIntensity(std::nanf("")) == 0.0f);
    float last = 2.0f;
    for (int i = 0; i <= 100; i++) {
        const float a = static_cast<float>(i) / 100.0f;
        const float v = Vis::FlashIntensity(a);
        CHECK(v >= 0.0f);
        CHECK(v <= last + 1e-6f); // never brighter later
        last = v;
    }
    CHECK(Vis::FlashIntensity(0.99f) < 0.01f);
    CHECK(Vis::FlashGrowth(0.0f, 1.3f) == Approx(1.0f));
    CHECK(Vis::FlashGrowth(1.0f, 1.3f) == Approx(1.3f));
    CHECK(Vis::FlashGrowth(0.5f, 1.3f) > 1.15f); // eased: most of it early
    CHECK(Vis::FlashGrowth(2.0f, 1.3f) == Approx(1.3f));
    CHECK(Vis::FlashGrowth(0.5f, 5.0f) <= 2.0f); // a slight scale-up only
    CHECK(Vis::FlashGrowth(0.5f, 0.5f) == Approx(1.0f));
}

TEST_CASE("Muzzle Flash: a bigger burst, the flare, the size setting", "[kiuflash][kiuweap2]") {
    // The burst: from kBurstFrom at the shot to kBurstTo at the end, most of it early, never
    // shrinking; times the game's growth.
    CHECK(Vis::FlashBurst(0.0f, 1.0f) == Approx(Vis::kBurstFrom));
    CHECK(Vis::FlashBurst(1.0f, 1.0f) == Approx(Vis::kBurstTo));
    CHECK(Vis::FlashBurst(1.0f, 1.3f) == Approx(Vis::kBurstTo * 1.3f));
    CHECK(Vis::FlashBurst(1.0f / 3.0f, 1.0f) >
          Vis::kBurstFrom + 0.6f * (Vis::kBurstTo - Vis::kBurstFrom));
    CHECK(Vis::FlashBurst(std::nanf(""), 1.0f) == Approx(Vis::kBurstFrom));
    float last = 0.0f;
    for (int i = 0; i <= 50; i++) {
        const float v = Vis::FlashBurst(static_cast<float>(i) / 50.0f, 1.3f);
        CHECK(v >= last - 1e-6f);
        last = v;
    }
    // The charged flash is the bigger one.
    CHECK(Vis::FlashKindScale(false) == 1.0f);
    CHECK(Vis::FlashKindScale(true) == Approx(Vis::kChargedScale));
    CHECK(Vis::kChargedScale > 1.0f);
    // Muzzle Flash Size: percent to a factor, held to 50 to 300.
    CHECK(Vis::FlashSizeFactor(100) == Approx(1.0f));
    CHECK(Vis::FlashSizeFactor(250) == Approx(2.5f));
    CHECK(Vis::FlashSizeFactor(10) == Approx(0.5f));
    CHECK(Vis::FlashSizeFactor(1000) == Approx(3.0f));
    // The flare: full at the shot, its core first to go, gone by kFlareFade; nothing outside.
    const auto f0 = Vis::FlashFlareAt(0.0f, false);
    CHECK(f0.strength == Approx(1.0f));
    CHECK(f0.core == Approx(1.0f));
    CHECK(f0.metres == Approx(Vis::kFlareRapidM * Vis::kBurstFrom));
    const auto c0 = Vis::FlashFlareAt(0.0f, true);
    CHECK(c0.metres == Approx(Vis::kFlareChargedM * Vis::kBurstFrom));
    const auto mid = Vis::FlashFlareAt(0.3f, false);
    CHECK(mid.strength > 0.0f);
    CHECK(mid.strength < 1.0f);
    CHECK(mid.core < mid.strength);
    CHECK(mid.metres > f0.metres);
    CHECK(Vis::FlashFlareAt(Vis::kFlareFade, false).strength == Approx(0.0f).margin(1e-6));
    CHECK(Vis::FlashFlareAt(0.9f, true).strength == 0.0f);
    CHECK(Vis::FlashFlareAt(1.0f, false).metres == 0.0f);
    CHECK(Vis::FlashFlareAt(-0.1f, false).strength == 0.0f);
    CHECK(Vis::FlashFlareAt(std::nanf(""), false).strength == 0.0f);
    // Larger than before (were 0.3, 0.22, 0.32, 0.05) and longer (were 70 and 100 ms).
    CHECK(Vis::kSizeScale > 0.3f);
    CHECK(Vis::kFaceMaxM > 0.22f);
    CHECK(Vis::kFanMaxM > 0.32f);
    CHECK(Vis::kCardMinM > 0.05f);
    CHECK(Vis::MuzzleFlash::kRapidNs > 70'000'000);
    CHECK(Vis::MuzzleFlash::kChargedNs > Vis::MuzzleFlash::kRapidNs);
}

TEST_CASE("Muzzle Flash: cards along the ray, facing the eye, for any pose", "[kiuflash]") {
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    const auto rand_unit = [&] {
        V3 v;
        do {
            v = {u(rng), u(rng), u(rng)};
        } while (Len(v) < 0.2f);
        return Unit(v);
    };
    for (int i = 0; i < 500; i++) {
        INFO("pose " << i);
        const V3 origin{u(rng) * 2.0f, 1.0f + u(rng), u(rng) * 2.0f};
        const V3 forward = rand_unit();
        // The eye behind the hand mostly, sometimes beside it.
        const V3 eye{origin[0] - forward[0] * 0.4f + u(rng) * 0.3f, origin[1] + 0.2f + u(rng) * 0.1f,
                     origin[2] - forward[2] * 0.4f + u(rng) * 0.3f};
        const V3 up{0, 1, 0};
        const float metres = 0.01f + 0.02f * (u(rng) + 1.0f);
        const float offset = 2.0f;
        const float spin = u(rng) * 3.0f;
        // A face: centred `offset` units ahead on the ray, its normal to the eye, square.
        auto m = Vis::FlashCardTransform(false, origin, forward, eye, up, metres, offset, spin);
        const V3 at = Col(m, 3);
        const V3 ahead = Sub(at, origin);
        CHECK(Len(ahead) == Approx(offset * metres).margin(1e-4));
        CHECK(Dot(ahead, forward) == Approx(offset * metres).margin(1e-4));
        CHECK(Len(Col(m, 0)) == Approx(metres).epsilon(1e-3));
        CHECK(Len(Col(m, 1)) == Approx(metres).epsilon(1e-3));
        CHECK(Dot(Col(m, 0), Col(m, 1)) == Approx(0.0f).margin(1e-6));
        CHECK(Dot(Unit(Col(m, 2)), Unit(Sub(eye, at))) == Approx(1.0f).margin(1e-3));
        // A fan: from the muzzle along the ray, turned about it toward the eye.
        m = Vis::FlashCardTransform(true, origin, forward, eye, up, metres, offset, spin);
        CHECK(Len(Sub(Col(m, 3), origin)) == Approx(0.0f).margin(1e-5));
        CHECK(Dot(Unit(Col(m, 1)), forward) == Approx(1.0f).margin(1e-4));
        CHECK(Dot(Col(m, 0), forward) == Approx(0.0f).margin(1e-5));
        const V3 to_eye = Sub(eye, origin);
        const V3 across{to_eye[0] - forward[0] * Dot(to_eye, forward),
                        to_eye[1] - forward[1] * Dot(to_eye, forward),
                        to_eye[2] - forward[2] * Dot(to_eye, forward)};
        if (Len(across) > 1e-3f) {
            CHECK(Dot(Unit(Col(m, 2)), Unit(across)) == Approx(1.0f).margin(1e-3));
        }
        // Right handed (no card is mirrored).
        const V3 x = Col(m, 0), y = Col(m, 1), z = Col(m, 2);
        const V3 c{x[1] * y[2] - x[2] * y[1], x[2] * y[0] - x[0] * y[2], x[0] * y[1] - x[1] * y[0]};
        CHECK(Dot(c, z) > 0.0f);
    }
    // The eye on the ray itself, and the head's up along the view: still a frame, no NaN.
    for (const bool fan : {false, true}) {
        const auto m = Vis::FlashCardTransform(fan, {0, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, 0, 1},
                                               0.02f, 1.0f, 0.0f);
        for (const float v : m) {
            CHECK(std::isfinite(v));
        }
        CHECK(Len(Col(m, 0)) == Approx(0.02f).epsilon(1e-3));
    }
}

TEST_CASE("Muzzle Flash: size, its limits, the tune, the colour", "[kiuflash]") {
    // The weapon's scale times kSizeScale, held under the role's largest.
    CHECK(Vis::FlashCardMetres(false, 4.0f, 0.075f, 1.0f) == Approx(0.075f * Vis::kSizeScale));
    CHECK(Vis::FlashCardMetres(false, 40.0f, 0.075f, 1.0f) * 40.0f == Approx(Vis::kFaceMaxM));
    CHECK(Vis::FlashCardMetres(true, 40.0f, 0.075f, 1.0f) * 40.0f == Approx(Vis::kFanMaxM));
    CHECK(Vis::FlashCardMetres(false, 4.0f, 0.075f, 2.0f) == Approx(0.15f * Vis::kSizeScale));
    CHECK(Vis::FlashCardMetres(false, 0.0f, 0.075f, 1.0f) == Approx(0.075f * Vis::kSizeScale));
    // A tiny card (a staff's 1 unit glow) still reads: at least kCardMinM.
    CHECK(Vis::FlashCardMetres(false, 1.0f, 0.065f, 1.0f) == Approx(Vis::kCardMinM));
    CHECK(Vis::FlashCardMetres(true, 0.5f, 0.065f, 2.0f) * 0.5f == Approx(Vis::kCardMinM * 2.0f));
    // AZAHARFRAME_KIU_FLASH=size,ms
    auto t = Vis::ParseFlashTune(std::string{"1.5,120"});
    CHECK(t.size == Approx(1.5f));
    CHECK(t.duration_ms == 120);
    t = Vis::ParseFlashTune(std::string{"0.5"});
    CHECK(t.size == Approx(0.5f));
    CHECK(t.duration_ms == 0);
    t = Vis::ParseFlashTune(std::string{"x,5"});
    CHECK(t.size == 1.0f);
    CHECK(t.duration_ms == 0); // 5 ms is out of range
    t = Vis::ParseFlashTune(std::nullopt);
    CHECK(t.size == 1.0f);
    CHECK(Vis::ParseFlashTune(std::string{"99"}).size == 1.0f);
    // Spins differ from shot to shot; colours to linear light.
    CHECK(std::abs(Vis::FlashSpin(1) - Vis::FlashSpin(2)) > 0.5f);
    CHECK(Vis::FlashLinear(0.0f) == 0.0f);
    CHECK(Vis::FlashLinear(1.0f) == Approx(1.0f));
    CHECK(Vis::FlashLinear(0.5f) == Approx(0.214f).margin(0.002f));
}

TEST_CASE("Muzzle Flash: out of a barrel the muzzle is deep in", "[kiuflash]") {
    // Weapon x maps to aim -z at 0.01 m a unit; a barrel along it from 0 to 10 units, 1 unit
    // round, and a fin off to the side reaching 30 units.
    std::array<float, 16> fit{};
    fit[2] = -0.01f;
    fit[4] = 0.01f;
    fit[9] = 0.01f;
    fit[15] = 1.0f;
    std::vector<float> v;
    const auto put = [&](float x, float y, float z) {
        v.insert(v.end(), {x, y, z, 0, 0, 1, 1, 0, 0, 1, 0, 0});
    };
    for (const float x : {0.0f, 10.0f}) {
        put(x, 1, 0), put(x, -1, 0), put(x, 0, 1), put(x, 0, -1);
    }
    put(30, 6, 0); // 6 cm off the ray: not in the way
    // The muzzle 3 cm short of the barrel's end: 3 cm out.
    CHECK(Vis::FlashPush(v, fit, {0, 0, -0.07f}) == Approx(0.03f));
    // At the end or past it: none.
    CHECK(Vis::FlashPush(v, fit, {0, 0, -0.10f}) == Approx(0.0f).margin(1e-6));
    CHECK(Vis::FlashPush(v, fit, {0, 0, -0.2f}) == 0.0f);
    // Off to the side of the barrel: none.
    CHECK(Vis::FlashPush(v, fit, {-0.05f, 0, -0.05f}) == 0.0f);
    // Deep in a long one: at most kPushMaxM.
    put(100, 0, 0);
    CHECK(Vis::FlashPush(v, fit, {0, 0, -0.05f}) == Approx(Vis::kPushMaxM));
    CHECK(Vis::FlashPush({}, fit, {0, 0, 0}) == 0.0f);
}

// AzaharFrame fixc: a cannon's mouth: the same rule along its barrel (BarrelPush), and where the
// flash and the shots start (ShotOriginFor): every other class as before, bit for bit.
TEST_CASE("Muzzle Flash: a cannon's mouth along its barrel", "[kiuflash][fixc]") {
    // The barrel above, turned 36 degrees about +y (the aim space's up): its line through the
    // origin along (-sin 36, 0, -cos 36).
    const float a = 36.0f * 3.14159265f / 180.0f, c = std::cos(a), sn = std::sin(a);
    std::array<float, 16> fit{};
    fit[0] = -0.01f * sn; // weapon x: along the barrel
    fit[2] = -0.01f * c;
    fit[4] = 0.01f * c; // weapon y: across it, level
    fit[6] = -0.01f * sn;
    fit[9] = 0.01f; // weapon z: up
    fit[15] = 1.0f;
    std::vector<float> v;
    const auto put = [&](float x, float y, float z) {
        v.insert(v.end(), {x, y, z, 0, 0, 1, 1, 0, 0, 1, 0, 0});
    };
    for (const float x : {0.0f, 10.0f}) {
        put(x, 1, 0), put(x, -1, 0), put(x, 0, 1), put(x, 0, -1);
    }
    const std::array<float, 3> barrel{-sn, 0.0f, -c};
    const auto on = [&](float m) { return std::array<float, 3>{-sn * m, 0.0f, -c * m}; };
    // The muzzle 3 cm short of the barrel's end: 3 cm out along it.
    CHECK(Vis::BarrelPush(v, fit, on(0.07f), barrel) == Approx(0.03f).margin(1e-5));
    CHECK(Vis::BarrelPush(v, fit, on(0.10f), barrel) == Approx(0.0f).margin(1e-5));
    CHECK(Vis::BarrelPush(v, fit, on(0.20f), barrel) == 0.0f);
    // Deep in a long one: at most kPushMaxM; nothing: none.
    put(100, 0, 0);
    CHECK(Vis::BarrelPush(v, fit, on(0.05f), barrel) == Approx(Vis::kPushMaxM));
    CHECK(Vis::BarrelPush({}, fit, {0, 0, 0}, barrel) == 0.0f);
    // With -z as the axis it is FlashPush.
    std::array<float, 16> straight{};
    straight[2] = -0.01f;
    straight[4] = 0.01f;
    straight[9] = 0.01f;
    straight[15] = 1.0f;
    for (const float z : {-0.02f, -0.05f, -0.07f, -0.1f}) {
        CHECK(Vis::BarrelPush(v, straight, {0, 0, z}, {0, 0, -1}) ==
              Approx(Vis::FlashPush(v, straight, {0, 0, z})).margin(1e-6));
    }

    // Where the flash and the shots start: every class but a cannon, `push` ahead along -z and
    // -z, bit for bit as before; a cannon, the mouth itself and its barrel.
    const std::array<float, 3> start{0.013f, -0.0444f, -0.21f};
    const Vis::ShotOrigin before = Vis::ShotOriginFor(start, 0.04f, std::nullopt);
    const std::array<float, 3> old_from{start[0], start[1], start[2] - 0.04f};
    CHECK(std::memcmp(before.from.data(), old_from.data(), sizeof(old_from)) == 0);
    CHECK(before.forward == std::array<float, 3>{0.0f, 0.0f, -1.0f});
    const Vis::ShotOrigin cannon = Vis::ShotOriginFor(start, 0.04f, barrel);
    CHECK(cannon.from == start);
    CHECK(cannon.forward == barrel);
}

TEST_CASE("Muzzle Flash: a pack's large texture halved to fit", "[kiuflash]") {
    // 1024 x 512: halved twice to 256 x 128, each texel the average of the four under it.
    uint32_t w = 1024, h = 512;
    std::vector<uint8_t> px(std::size_t{w} * h * 4);
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t* p = &px[(std::size_t{y} * w + x) * 4];
            p[0] = static_cast<uint8_t>(x < 4 && y < 4 ? 200 : 0);
            p[1] = static_cast<uint8_t>((x + y) % 2 ? 255 : 0);
            p[2] = 10;
            p[3] = static_cast<uint8_t>(y < 256 ? 255 : 0);
        }
    }
    Vis::FlashTextureFit(px, w, h);
    CHECK(w == 256);
    CHECK(h == 128);
    REQUIRE(px.size() == std::size_t{256} * 128 * 4);
    CHECK(px[0] == 200);   // the top left 4x4 block, averaged twice
    CHECK(px[4] == 0);
    CHECK(px[1] == 128);   // a checker becomes grey
    CHECK(px[2] == 10);
    CHECK(px[3] == 255);
    CHECK(px[(std::size_t{127} * 256) * 4 + 3] == 0); // the bottom half's alpha
    // Small enough already: untouched.
    uint32_t sw = 256, sh = 64;
    std::vector<uint8_t> small(std::size_t{sw} * sh * 4, 7);
    Vis::FlashTextureFit(small, sw, sh);
    CHECK(sw == 256);
    CHECK(small.size() == std::size_t{256} * 64 * 4);
}

TEST_CASE("Muzzle Flash: a card's extent", "[kiuflash]") {
    // Two quads: a face 6 x 4 about the middle, a fan 2 wide from 0 to 10.
    std::vector<float> v;
    const auto put = [&](float x, float y) {
        v.insert(v.end(), {x, y, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0});
    };
    put(-3, -2), put(3, -2), put(3, 2), put(-3, 2);
    put(-1, 0), put(1, 0), put(1, 10), put(-1, 10);
    const std::vector<uint32_t> i{0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    auto e = Vis::FlashCardSize(v, i, 0, 6);
    CHECK(e.half_width == 3.0f);
    CHECK(e.half_length == 2.0f);
    CHECK(e.middle == 0.0f);
    CHECK(e.Units() == 6.0f);
    e = Vis::FlashCardSize(v, i, 6, 6);
    CHECK(e.half_width == 1.0f);
    CHECK(e.half_length == 5.0f);
    CHECK(e.middle == 5.0f);
    CHECK(e.Units() == 10.0f);
    e = Vis::FlashCardSize(v, i, 6, 60); // a range past the end: what there is
    CHECK(e.Units() == 10.0f);
    CHECK(Vis::FlashCardSize(v, i, 0, 0).Units() == 0.0f);
}

TEST_CASE("Muzzle Flash: a game's choice survives an older build", "[kiuflash]") {
    auto& v = Settings::values;
    CHECK(v.vr_light_gun_flash.GetValue()); // on by default
    CHECK(std::string{v.vr_light_gun_flash.GetLabel()} == "vr_light_gun_flash");
    v.vr_controls_preset_done.SetGlobal(false);
    v.vr_controls_preset_done.SetValue(FrameXr::kPresetGyroAim);
    v.vr_light_gun_weapon.SetGlobal(false);
    v.vr_light_gun_weapon.SetValue(true);
    v.vr_light_gun_flash.SetGlobal(false);
    v.vr_light_gun_flash.SetValue(false);
    FrameXr::MirrorWeaponInHand();
    uint32_t saved = v.vr_controls_preset_done.GetValue();
    CHECK((saved & FrameXr::kPresetGyroAim) != 0); // the old bits keep their meaning
    CHECK((saved & FrameXr::kWeaponMirrorMask) >> FrameXr::kWeaponMirrorShift == 2);
    CHECK((saved & FrameXr::kLaserMirrorMask) == 0);
    CHECK((saved & FrameXr::kFlashMirrorMask) >> FrameXr::kFlashMirrorShift == 1);
    // Its own bits, 28 and 29: clear of the other mirrors (8 to 13 here, 14 to 27 kiufix's).
    CHECK(FrameXr::kFlashMirrorMask == 0x30000000u);
    CHECK((saved & 0x0FFFC000u) == 0);
    // New -> old (the key dropped: the global value) -> new: the game's Off comes back.
    for (int round = 0; round < 2; round++) {
        v.vr_light_gun_flash.SetGlobal(true);
        CHECK(FrameXr::RestoreWeaponInHand());
        CHECK_FALSE(v.vr_light_gun_flash.UsingGlobal());
        CHECK_FALSE(v.vr_light_gun_flash.GetValue());
        FrameXr::MirrorWeaponInHand();
        CHECK(v.vr_controls_preset_done.GetValue() == saved);
    }
    // On for the game.
    v.vr_light_gun_flash.SetValue(true);
    FrameXr::MirrorWeaponInHand();
    saved = v.vr_controls_preset_done.GetValue();
    CHECK((saved & FrameXr::kFlashMirrorMask) >> FrameXr::kFlashMirrorShift == 2);
    v.vr_light_gun_flash.SetGlobal(true);
    CHECK(FrameXr::RestoreWeaponInHand());
    CHECK(v.vr_light_gun_flash.GetValue());
    // Back to the global value: nothing mirrored for it, nothing restored.
    v.vr_light_gun_flash.SetGlobal(true);
    FrameXr::MirrorWeaponInHand();
    CHECK((v.vr_controls_preset_done.GetValue() & FrameXr::kFlashMirrorMask) == 0);
    v.vr_light_gun_weapon.SetGlobal(true);
    FrameXr::MirrorWeaponInHand();
    CHECK_FALSE(FrameXr::RestoreWeaponInHand());
    v.vr_controls_preset_done.SetGlobal(true);
}

TEST_CASE("Muzzle Flash: the per-frame cost", "[kiuflash]") {
    // The XR thread's share each frame: the timer and four cards' transforms for two eyes.
    Vis::MuzzleFlash m;
    uint64_t total = 0;
    float sink = 0.0f;
    constexpr int kFrames = 20000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kFrames; i++) {
        const int64_t now = int64_t{i} * 11 * kMs;
        total += i % 8 == 0 ? 1 : 0;
        m.Update(now, total, true, true);
        const Vis::FlashFrame f = m.Current(now);
        for (int e = 0; e < 2; e++) {
            for (int c = 0; c < 4; c++) {
                const float metres = Vis::FlashCardMetres(c == 1, 9.0f, 0.075f, 1.0f) *
                                     Vis::FlashGrowth(f.age, 1.3f) * Vis::FlashIntensity(f.age);
                const auto card = Vis::FlashCardTransform(c == 1, {0, 1, 0}, {0, 0, -1},
                                                          {0.1f, 1.3f, 0.4f}, {0, 1, 0}, metres,
                                                          1.8f, Vis::FlashSpin(f.shot + c));
                sink += card[12] + Vis::FlashLinear(0.5f);
            }
        }
    }
    const double us = std::chrono::duration<double, std::micro>(
                          std::chrono::steady_clock::now() - t0)
                          .count() /
                      kFrames;
    UNSCOPED_INFO("muzzle flash CPU per frame: " << us << " us (sink " << sink << ")");
    WARN("muzzle flash CPU per frame: " << us << " us");
    CHECK(us < 100.0); // well under 0.1 ms, even under qemu
}

// ---- AzaharFrame kiuflash: Projectiles ----

TEST_CASE("Projectiles: how long a shot flies", "[kiuflash]") {
    // The distance over the kind's speed: 2.8 m at 14 m/s is 200 ms.
    CHECK(Vis::ShotFlightNs(2.8f, false, 1.0f) == Approx(200'000'000).margin(1000));
    CHECK(Vis::ShotFlightNs(2.7f, true, 1.0f) == Approx(300'000'000).margin(1000));
    // Held within the kind's limits: a near screen still reads, a far one does not drag.
    CHECK(Vis::ShotFlightNs(0.3f, false, 1.0f) == Vis::kRapidMinNs);
    CHECK(Vis::ShotFlightNs(30.0f, false, 1.0f) == Vis::kRapidMaxNs);
    CHECK(Vis::ShotFlightNs(0.3f, true, 1.0f) == Vis::kChargedMinNs);
    CHECK(Vis::ShotFlightNs(30.0f, true, 1.0f) == Vis::kChargedMaxNs);
    // Projectile Speed 200 %: half the time, the limits too; 50 %: twice.
    CHECK(Vis::ShotFlightNs(2.8f, false, 2.0f) == Approx(100'000'000).margin(1000));
    CHECK(Vis::ShotFlightNs(30.0f, false, 2.0f) == Vis::kRapidMaxNs / 2);
    CHECK(Vis::ShotFlightNs(2.8f, false, 0.5f) == Approx(400'000'000).margin(1000));
    // A charged shot is slower than a rapid one over the same way.
    for (const float m : {0.5f, 1.0f, 2.0f, 3.0f, 5.0f}) {
        CHECK(Vis::ShotFlightNs(m, true, 1.0f) > Vis::ShotFlightNs(m, false, 1.0f));
    }
    // Odd input: no crash, a time within the limits.
    CHECK(Vis::ShotFlightNs(std::nanf(""), false, std::nanf("")) == Vis::kRapidMinNs);
    CHECK(Vis::ShotFlightNs(-1.0f, false, 1.0f) == Vis::kRapidMinNs);
}

TEST_CASE("Projectiles: from the muzzle into the screen, several at once", "[kiuflash]") {
    Vis::Projectiles p;
    const V3 from{0.1f, 1.2f, -0.5f}, to{0.1f, 1.2f, -3.3f}; // 2.8 m ahead: 200 ms
    int64_t t = 5'000 * kMs;
    p.Fire(from, to, false, t, 1.0f, 1.0f);
    REQUIRE(p.InFlight() == 1);
    auto d = p.Current(t);
    REQUIRE(d.size() == 1);
    CHECK(Len(Sub(d[0].at, from)) == Approx(0.0f).margin(1e-5));
    CHECK(d[0].forward[2] == Approx(-1.0f));
    CHECK(d[0].size == Approx(Vis::kRapidSizeM));
    CHECK(d[0].strength > 0.0f);
    CHECK_FALSE(d[0].charged);
    // Halfway at half the time, along the straight line; full strength by then.
    d = p.Current(t + 100 * kMs);
    REQUIRE(d.size() == 1);
    CHECK(d[0].at[2] == Approx(-1.9f).margin(1e-4));
    CHECK(d[0].at[1] == Approx(1.2f));
    CHECK(d[0].strength == Approx(1.0f));
    // At the screen, then into it: shrinking to nothing at the point, then gone.
    const int64_t flight = Vis::ShotFlightNs(2.8f, false, 1.0f);
    d = p.Current(t + flight + Vis::kEnterNs / 2);
    REQUIRE(d.size() == 1);
    CHECK(Len(Sub(d[0].at, to)) == Approx(0.0f).margin(1e-5));
    CHECK(d[0].size == Approx(Vis::kRapidSizeM * 0.5f));
    CHECK(d[0].strength == Approx(0.5f));
    CHECK(p.Current(t + flight + Vis::kEnterNs).empty());
    CHECK(p.InFlight() == 0);
    // Rapid fire: each shot its own, at most kMaxShots (the oldest go first).
    for (int i = 0; i < 10; i++) {
        p.Fire(from, to, false, t + i * 20 * kMs, 1.0f, 1.0f);
    }
    CHECK(p.InFlight() == static_cast<std::size_t>(Vis::kMaxShots));
    d = p.Current(t + 180 * kMs);
    REQUIRE(!d.empty());
    CHECK(d.front().serial == 4); // 1 and 2 of the ten (serials 2 and 3 here) went first
    for (std::size_t i = 1; i < d.size(); i++) {
        CHECK(d[i].serial == d[i - 1].serial + 1);
        CHECK(d[i].at[2] >= d[i - 1].at[2] - 1e-5f); // the later, the nearer the muzzle
        CHECK(d[i].spin != d[i - 1].spin);           // each its own turn
    }
    // A charged one: its own size and kind; Projectile Size scales it.
    p.Clear();
    p.Fire(from, to, true, t, 1.0f, 1.5f);
    d = p.Current(t + 10 * kMs);
    REQUIRE(d.size() == 1);
    CHECK(d[0].charged);
    CHECK(d[0].size == Approx(Vis::kChargedSizeM * 1.5f));
    // A clock that went back (a load): an old shot is let go.
    CHECK(p.Current(t - 2'000 * kMs).empty());
}

// AzaharFrame fixc: the 5 s summary: seen, fired, drawn, skipped by reason; quiet without shots.
TEST_CASE("Projectiles: the 5 s summary", "[kiuflash][fixc]") {
    Vis::ShotLog log;
    constexpr int64_t s = 1'000'000'000;
    CHECK_FALSE(log.Take(0)); // starts the count
    CHECK_FALSE(log.Take(6 * s)); // nothing happened: nothing logged
    log.Seen(4);
    log.Fired(140'000'000, 1.62f, 0.07f);
    log.SetOpacity(0.6f);
    log.Fired(150'000'000, 1.70f, 0.07f);
    log.Skipped(Vis::ShotSkip::NoTarget);
    log.Skipped(Vis::ShotSkip::SameFrame, 1);
    log.Drawn(1);
    log.Drawn(1); // the same projectile again (the other eye): once
    log.Drawn(2);
    CHECK_FALSE(log.Take(10 * s)); // not 5 s yet
    const auto line = log.Take(11 * s + 1);
    REQUIRE(line);
    CHECK(*line == "xr: projectiles (5 s): 4 shots seen, 2 fired, 2 drawn; skipped: 1 no point "
                   "on the Primary Screen, 1 several in one refresh; last 150 ms over 1.70 m, "
                   "7 cm across; Projectile Opacity 60%");
    // Cleared; the drawn serials go on (an old one is not counted again).
    CHECK_FALSE(log.Take(17 * s));
    log.Seen(2);
    log.Drawn(2);
    const auto quiet = log.Take(23 * s);
    REQUIRE(quiet);
    CHECK(*quiet == "xr: projectiles (6 s): 2 shots seen, 0 fired, 0 drawn; skipped: 2 while the "
                    "models' pass was not ready");
    log.Seen(1);
    log.Fired(110'000'000, 1.0f, 0.12f);
    log.Drawn(3);
    const auto all = log.Take(29 * s);
    REQUIRE(all);
    CHECK(all->find("1 shots seen, 1 fired, 1 drawn; none skipped") != std::string::npos);
    // Projectiles::Fire gives the flight it chose.
    Vis::Projectiles p;
    CHECK(p.Fire({0, 0, 0}, {0, 0, -1.4f}, false, 0, 1.0f, 1.0f) ==
          Vis::ShotFlightNs(1.4f, false, 1.0f));
}

TEST_CASE("Projectiles: a bullet turned the way it flies", "[kiuflash]") {
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (int i = 0; i < 300; i++) {
        Vis::ShotDraw d;
        d.at = {u(rng), 1.0f + u(rng), u(rng) - 2.0f};
        V3 f{u(rng), u(rng) * 0.5f, -1.0f + u(rng) * 0.3f};
        d.forward = Unit(f);
        d.size = 0.05f + 0.1f * (u(rng) + 1.0f);
        d.spin = u(rng) * 6.0f;
        const auto m = Vis::ShotTransform(d);
        // Its +z (its head) along the way it flies, every axis `size` long and square to the
        // others, right handed, at the shot.
        CHECK(Dot(Unit(Col(m, 2)), d.forward) == Approx(1.0f).margin(1e-4));
        for (int c = 0; c < 3; c++) {
            CHECK(Len(Col(m, c)) == Approx(d.size).epsilon(1e-3));
        }
        CHECK(Dot(Col(m, 0), Col(m, 1)) == Approx(0.0f).margin(1e-5));
        const V3 x = Col(m, 0), y = Col(m, 1), z = Col(m, 2);
        const V3 c{x[1] * y[2] - x[2] * y[1], x[2] * y[0] - x[0] * y[2], x[0] * y[1] - x[1] * y[0]};
        CHECK(Dot(c, z) > 0.0f);
        CHECK(Len(Sub(Col(m, 3), d.at)) == Approx(0.0f).margin(1e-6));
    }
    // Straight up or down: still a frame.
    Vis::ShotDraw d;
    d.forward = {0, 1, 0};
    d.size = 0.1f;
    for (const float v : Vis::ShotTransform(d)) {
        CHECK(std::isfinite(v));
    }
}

TEST_CASE("Projectiles: the live tune", "[kiuflash]") {
    auto t = Vis::ParseShotTune(std::string{"1.5,2"});
    CHECK(t.speed == Approx(1.5f));
    CHECK(t.size == Approx(2.0f));
    t = Vis::ParseShotTune(std::string{"0.5"});
    CHECK(t.speed == Approx(0.5f));
    CHECK(t.size == 1.0f);
    t = Vis::ParseShotTune(std::string{"9,0.1"});
    CHECK(t.speed == 1.0f);
    CHECK(t.size == 1.0f);
    CHECK(Vis::ParseShotTune(std::nullopt).speed == 1.0f);
}

TEST_CASE("Projectiles: a game's choice survives an older build", "[kiuflash]") {
    auto& v = Settings::values;
    CHECK(v.vr_light_gun_projectile.GetValue()); // on by default
    CHECK(v.vr_light_gun_projectile_size.GetValue() == 100);
    CHECK(v.vr_light_gun_projectile_speed.GetValue() == 100);
    v.vr_controls_preset_done.SetGlobal(false);
    v.vr_controls_preset_done.SetValue(FrameXr::kPresetGyroAim);
    v.vr_light_gun_projectile.SetGlobal(false);
    v.vr_light_gun_projectile.SetValue(false);
    FrameXr::MirrorWeaponInHand();
    const uint32_t saved = v.vr_controls_preset_done.GetValue();
    CHECK(FrameXr::kProjectileMirrorMask == 0xC0000000u);
    CHECK((saved & FrameXr::kProjectileMirrorMask) >> FrameXr::kProjectileMirrorShift == 1);
    CHECK((saved & 0x3FFFC000u) == 0); // kiufix's bits and Muzzle Flash's untouched
    for (int round = 0; round < 2; round++) {
        v.vr_light_gun_projectile.SetGlobal(true); // the key dropped by an older build
        CHECK(FrameXr::RestoreWeaponInHand());
        CHECK_FALSE(v.vr_light_gun_projectile.UsingGlobal());
        CHECK_FALSE(v.vr_light_gun_projectile.GetValue());
        FrameXr::MirrorWeaponInHand();
        CHECK(v.vr_controls_preset_done.GetValue() == saved);
    }
    v.vr_light_gun_projectile.SetValue(true);
    FrameXr::MirrorWeaponInHand();
    CHECK((v.vr_controls_preset_done.GetValue() & FrameXr::kProjectileMirrorMask) ==
          (2u << FrameXr::kProjectileMirrorShift));
    v.vr_light_gun_projectile.SetGlobal(true);
    FrameXr::MirrorWeaponInHand();
    CHECK((v.vr_controls_preset_done.GetValue() & FrameXr::kProjectileMirrorMask) == 0);
    CHECK_FALSE(FrameXr::RestoreWeaponInHand());
    v.vr_controls_preset_done.SetGlobal(true);
}

TEST_CASE("Projectiles: the per-frame cost", "[kiuflash]") {
    // The XR thread's share each frame with eight in flight: the list and two eyes' transforms.
    Vis::Projectiles p;
    float sink = 0.0f;
    constexpr int kFrames = 20000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kFrames; i++) {
        const int64_t now = int64_t{i} * 11 * kMs;
        if (i % 3 == 0) {
            p.Fire({0, 1, 0}, {0.2f, 1.3f, -3.0f}, i % 30 == 0, now, 1.0f, 1.0f);
        }
        for (const auto& d : p.Current(now)) {
            for (int e = 0; e < 2; e++) {
                sink += Vis::ShotTransform(d)[12];
            }
        }
    }
    const double us = std::chrono::duration<double, std::micro>(
                          std::chrono::steady_clock::now() - t0)
                          .count() /
                      kFrames;
    WARN("projectiles CPU per frame: " << us << " us (sink " << sink << ")");
    CHECK(us < 100.0);
}

// ---- Desktop proofs (hidden): AZAHARFRAME_TEST_KIU_MODELS=<a folder the converter made>,
// AZAHARFRAME_TEST_KIU_FLASH_CARDS=<out file>: each weapon's fit in the right hand, its muzzle and
// its flash cards placed as the app places them, 10 % into the flash, the head level, for two
// eyes: the player's (0.22 m above and 0.42 m behind the controller) and one 0.6 m to the right
// of the muzzle (as a player looking from the side would see it), for the renders. ----

TEST_CASE("Muzzle Flash: the cards of converted weapons", "[.kiuflash_real]") {
    const char* dir = std::getenv("AZAHARFRAME_TEST_KIU_MODELS");
    const char* out = std::getenv("AZAHARFRAME_TEST_KIU_FLASH_CARDS");
    REQUIRE(dir);
    REQUIRE(out);
    FrameWeapons::Manifest m;
    std::string error;
    REQUIRE(FrameWeapons::ReadManifest(std::string{dir} + "/weapons.txt", m, error));
    std::ofstream o(out);
    const V3 up{0, 1, 0};
    const float age = 0.1f;
    for (const auto& w : m.weapons) {
        std::vector<Vis::FitSide> sides;
        for (const auto& p : w.parts) {
            for (const auto s : p.sides) {
                const auto fs = static_cast<Vis::FitSide>(static_cast<int>(s));
                if (std::find(sides.begin(), sides.end(), fs) == sides.end()) {
                    sides.push_back(fs);
                }
            }
        }
        const Vis::FitWeapon fw{w.cls, w.muzzle, w.muzzle_valid};
        const auto muzzle = Vis::FitMuzzle(fw, false, sides);
        REQUIRE(muzzle);
        const float scale = Vis::FitClassScale(w.cls);
        const auto fits = Vis::FitWeaponToHand(fw, false, sides);
        REQUIRE(!fits.empty());
        float push = 0.0f; // as the app's loader works it out
        for (const auto& p : w.parts) {
            Vis::ModelFile pf;
            REQUIRE(Vis::LoadModelFile(std::string{dir} + "/" + p.file, pf, error,
                                       Vis::kModelFlagWeaponSpace));
            if (!p.glow) {
                push = std::max(push, Vis::FlashPush(pf.vertices, fits[0].transform, *muzzle));
            }
        }
        o << "muzzle " << w.id << ' ' << (*muzzle)[0] << ' ' << (*muzzle)[1] << ' ' << (*muzzle)[2]
          << ' ' << scale << ' ' << w.flash.source << ' ' << push << '\n';
        const V3 from{(*muzzle)[0], (*muzzle)[1], (*muzzle)[2] - push};
        for (const char* view : {"player", "side"}) {
        const V3 eye = view[0] == 'p' ? V3{0.0f, 0.22f, 0.42f}
                                      : V3{(*muzzle)[0] + 0.6f, (*muzzle)[1] + 0.05f, (*muzzle)[2]};
        uint32_t n = 0;
        for (const auto& part : w.flash.parts) {
            Vis::ModelFile mf;
            REQUIRE(Vis::LoadModelFile(std::string{dir} + "/" + part.file, mf, error,
                                       Vis::kModelFlagWeaponSpace));
            for (std::size_t k = 0; k < mf.components.size(); k++) {
                const auto& c = mf.components[k];
                const bool fan = c.path == "fan";
                const float* v = c.motion.data();
                const auto extent =
                    Vis::FlashCardSize(mf.vertices, mf.indices, c.first_index, c.index_count);
                const float units = extent.Units();
                const float metres = Vis::FlashCardMetres(fan, units, scale, 1.0f) *
                                     Vis::FlashGrowth(age, v[FrameWeapons::kFlashGrow]);
                const auto t = Vis::FlashCardTransform(fan, from, {0, 0, -1}, eye, up, metres,
                                                       v[FrameWeapons::kFlashOffset],
                                                       Vis::FlashSpin(7u + n++));
                o << "card " << view << ' ' << w.id << ' ' << c.name << ' ' << part.file << ' '
                  << k << ' ' << c.path;
                for (const float x : t) {
                    o << ' ' << x;
                }
                o << ' ' << Vis::FlashIntensity(age) << ' ' << v[FrameWeapons::kFlashMode] << ' '
                  << Vis::FlashLinear(v[FrameWeapons::kFlashRed]) << ' '
                  << Vis::FlashLinear(v[FrameWeapons::kFlashGreen]) << ' '
                  << Vis::FlashLinear(v[FrameWeapons::kFlashBlue]) << ' '
                  << v[FrameWeapons::kFlashCore] << ' ' << units * metres << ' '
                  << extent.half_width << ' ' << extent.half_length << ' ' << extent.middle
                  << '\n';
            }
        }
        }
    }
    CHECK(m.weapons.size() > 0);
}

// AZAHARFRAME_TEST_KIU_MODELS=<a folder the converter made>, AZAHARFRAME_TEST_KIU_SHOTS=<out
// file>: each weapon's bullets placed as the app places them, a shot fired from its muzzle (as the
// flash's, out of the barrel) to a screen 2.5 m ahead and 0.1 m up, at 15, 50 and 85 % of its
// flight and halfway into the screen, the eye 0.22 m above and 0.42 m behind the controller.
TEST_CASE("Projectiles: the bullets of converted weapons", "[.kiuflash_real]") {
    const char* dir = std::getenv("AZAHARFRAME_TEST_KIU_MODELS");
    const char* out = std::getenv("AZAHARFRAME_TEST_KIU_SHOTS");
    REQUIRE(dir);
    REQUIRE(out);
    FrameWeapons::Manifest m;
    std::string error;
    REQUIRE(FrameWeapons::ReadManifest(std::string{dir} + "/weapons.txt", m, error));
    std::ofstream o(out);
    const V3 eye{0.0f, 0.22f, 0.42f}, up{0, 1, 0};
    for (const auto& w : m.weapons) {
        std::vector<Vis::FitSide> sides;
        for (const auto& p : w.parts) {
            for (const auto s : p.sides) {
                const auto fs = static_cast<Vis::FitSide>(static_cast<int>(s));
                if (std::find(sides.begin(), sides.end(), fs) == sides.end()) {
                    sides.push_back(fs);
                }
            }
        }
        const Vis::FitWeapon fw{w.cls, w.muzzle, w.muzzle_valid};
        const auto muzzle = Vis::FitMuzzle(fw, false, sides);
        const auto fits = Vis::FitWeaponToHand(fw, false, sides);
        REQUIRE(muzzle);
        REQUIRE(!fits.empty());
        float push = 0.0f;
        for (const auto& p : w.parts) {
            Vis::ModelFile pf;
            REQUIRE(Vis::LoadModelFile(std::string{dir} + "/" + p.file, pf, error,
                                       Vis::kModelFlagWeaponSpace));
            if (!p.glow) {
                push = std::max(push, Vis::FlashPush(pf.vertices, fits[0].transform, *muzzle));
            }
        }
        const V3 from{(*muzzle)[0], (*muzzle)[1], (*muzzle)[2] - push};
        const V3 to{0.0f, 0.1f, -2.5f};
        o << "path " << w.id << ' ' << from[0] << ' ' << from[1] << ' ' << from[2] << ' ' << to[0]
          << ' ' << to[1] << ' ' << to[2] << '\n';
        for (const bool charged : {false, true}) {
            Vis::Projectiles shots;
            shots.Fire(from, to, charged, 0, 1.0f, 1.0f);
            const int64_t flight = Vis::ShotFlightNs(Len(Sub(to, from)), charged, 1.0f);
            for (const float f : {0.15f, 0.5f, 0.85f, 1.0f + 0.5f * Vis::kEnterNs / flight}) {
                const auto draws = shots.Current(static_cast<int64_t>(f * flight));
                REQUIRE(draws.size() == 1);
                const Vis::ShotDraw& d = draws[0];
                for (const auto& part : w.bullets.parts) {
                    Vis::ModelFile mf;
                    REQUIRE(Vis::LoadModelFile(std::string{dir} + "/" + part.file, mf, error,
                                               Vis::kModelFlagWeaponSpace));
                    const bool cards = (mf.flags & Vis::kModelFlagFlash) != 0;
                    for (std::size_t k = 0; k < mf.components.size(); k++) {
                        const auto& c = mf.components[k];
                        if (c.name != (charged ? "charged" : "rapid")) {
                            continue;
                        }
                        const float* v = c.motion.data();
                        const auto ext =
                            Vis::FlashCardSize(mf.vertices, mf.indices, c.first_index, c.index_count);
                        std::array<float, 16> t;
                        float mode, r, g, b, core = 0.0f, added = 1.0f, vcol = 0.0f;
                        if (cards) {
                            t = Vis::FlashCardTransform(false, d.at, d.forward, eye, up,
                                                        d.size / std::max(ext.Units(), 1e-4f),
                                                        0.0f, d.spin);
                            mode = v[FrameWeapons::kFlashMode];
                            r = v[FrameWeapons::kFlashRed];
                            g = v[FrameWeapons::kFlashGreen];
                            b = v[FrameWeapons::kFlashBlue];
                            core = v[FrameWeapons::kFlashCore];
                        } else {
                            t = Vis::ShotTransform(d);
                            mode = v[FrameWeapons::kBulletMode];
                            r = v[FrameWeapons::kBulletRed];
                            g = v[FrameWeapons::kBulletGreen];
                            b = v[FrameWeapons::kBulletBlue];
                            added = v[FrameWeapons::kBulletAdded];
                            vcol = v[FrameWeapons::kBulletVertexColour];
                        }
                        o << "bullet " << w.id << ' ' << (charged ? "charged" : "rapid") << ' '
                          << f << ' ' << part.file << ' ' << k << ' ' << (cards ? "card" : "mesh");
                        for (const float x : t) {
                            o << ' ' << x;
                        }
                        o << ' ' << d.strength << ' ' << mode << ' ' << Vis::FlashLinear(r) << ' '
                          << Vis::FlashLinear(g) << ' ' << Vis::FlashLinear(b) << ' ' << core
                          << ' ' << added << ' ' << ext.half_width << ' ' << ext.half_length
                          << ' ' << ext.middle << ' ' << vcol << '\n';
                    }
                }
            }
        }
    }
    CHECK(m.weapons.size() > 0);
}
