// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/input/gyro_aim.h"
#include "frame_xr/input/motion.h"
#include "frame_xr/kiu_scan.h"
#include "frame_xr/vis/beam.h"

// AzaharFrame kiugyro: Gyro Aim (frame_xr/input/gyro_aim.h) and the mod scan (frame_xr/kiu_scan.h).
using Catch::Approx;
using namespace FrameXr::Input;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;
constexpr int64_t kStepNs = 11'111'111; // 90 Hz

Quatf AxisAngle(Vec3f axis, float degrees) {
    const Vec3f n = Normalized(axis);
    const float half = degrees * kDeg * 0.5f;
    const float s = std::sin(half);
    return Quatf{n.x * s, n.y * s, n.z * s, std::cos(half)};
}

/// A controller aimed `right` degrees to the right and `up` degrees up, rolled `roll` degrees.
HandInput Aimed(float right, float up, float roll = 0.0f) {
    HandInput hand;
    hand.active = true;
    hand.aim.orientation = Multiply(Multiply(AxisAngle({0, 1, 0}, -right), AxisAngle({1, 0, 0}, up)),
                                    AxisAngle({0, 0, -1}, roll));
    return hand;
}

/// What the C-Stick & Gyro mod (v172) does with one HID gyroscope entry: z is the horizontal
/// rate and x the vertical one, each after Azahar's s16 truncation, the 0.25 deg/s dead zone and
/// the +-360 clamp (design.md facts 2 to 4). AzaharFrame kiufix: with a real 3DS's signs (made
/// for the console): a turn to the right reads negative z and moves the reticle right; aiming
/// down reads positive x and moves it down. (The first model had z the other way, the same
/// mistake as the code, so the tests could not see it.)
struct ModRead {
    float right = 0.0f;
    float down = 0.0f;
};
float ModAxis(float dps) {
    const float raw = std::trunc(dps * 14.375f); // HID: s16(gyro * coef)
    float v = raw / 14.375f;
    if (v > 0.25f) {
        v -= 0.25f;
    } else if (v < -0.25f) {
        v += 0.25f;
    } else {
        v = 0.0f;
    }
    return std::clamp(v, -360.0f, 360.0f);
}
ModRead ModReads(const Vec3f& gyro) {
    return {ModAxis(-gyro.z), ModAxis(gyro.x)};
}

} // namespace

TEST_CASE("FrameXr Gyro Aim: the mod's axes follow the hand", "[frame_xr][kiugyro]") {
    GyroAimTracker tracker;
    const float roll = GENERATE(0.0f, 35.0f, -60.0f);
    INFO("roll " << roll);
    tracker.Update(Aimed(0, 0, roll), 0, 1.0f, false);
    SECTION("turning right moves the reticle right") {
        const auto out = tracker.Update(Aimed(1.0f, 0, roll), kStepNs, 1.0f, false);
        REQUIRE(out.right_dps == Approx(90.0f).epsilon(0.02));
        REQUIRE(out.down_dps == Approx(0.0f).margin(0.5f));
        const ModRead m = ModReads(ModGyro(out.right_dps, out.down_dps));
        REQUIRE(m.right > 85.0f);
        REQUIRE(m.down == 0.0f);
    }
    SECTION("aiming up moves it up (down negative)") {
        const auto out = tracker.Update(Aimed(0, 0.5f, roll), kStepNs, 1.0f, false);
        REQUIRE(out.down_dps == Approx(-45.0f).epsilon(0.02));
        REQUIRE(out.right_dps == Approx(0.0f).margin(0.5f));
        const ModRead m = ModReads(ModGyro(out.right_dps, out.down_dps));
        REQUIRE(m.down < -40.0f);
        REQUIRE(m.right == 0.0f);
    }
    SECTION("turning left and aiming down") {
        const auto out = tracker.Update(Aimed(-0.5f, -0.5f, roll), kStepNs, 1.0f, false);
        REQUIRE(out.right_dps < -40.0f);
        REQUIRE(out.down_dps > 40.0f);
    }
}

// AzaharFrame kiufix: the test that would have caught the swapped left and right. Gyro Aim's 3DS
// gyroscope must be what Motion Controls sends for the same hand move (motion.h: a 3DS held like
// the controller), the axes every other gyro game reads; only roll is left out.
TEST_CASE("FrameXr Gyro Aim: the same axes and signs as Motion Controls", "[frame_xr][kiufix]") {
    const float yaw0 = GENERATE(0.0f, 30.0f, -50.0f);
    const float pitch0 = GENERATE(0.0f, 20.0f, -25.0f);
    INFO("from " << yaw0 << " right, " << pitch0 << " up");
    const float dt = static_cast<float>(kStepNs) * 1e-9f;
    const auto both = [&](float right, float up) {
        GyroAimTracker tracker;
        tracker.Update(Aimed(yaw0, pitch0), 0, 1.0f, false);
        const auto out = tracker.Update(Aimed(yaw0 + right, pitch0 + up), kStepNs, 1.0f, false);
        const Vec3f aim = ModGyro(out.right_dps, out.down_dps);
        const Vec3f motion = GyroFromOrientations(Aimed(yaw0, pitch0).aim.orientation,
                                                  Aimed(yaw0 + right, pitch0 + up).aim.orientation,
                                                  dt);
        return std::pair{aim, motion};
    };
    SECTION("a turn to the right: negative 3DS z, as Motion Controls") {
        const auto [aim, motion] = both(1.0f, 0.0f);
        CHECK(aim.z < -50.0f);
        CHECK(motion.z < -50.0f);
        CHECK(aim.z == Approx(motion.z).epsilon(0.1));
    }
    SECTION("aiming up: negative 3DS x, as Motion Controls") {
        const auto [aim, motion] = both(0.0f, 1.0f);
        CHECK(aim.x < -50.0f);
        CHECK(motion.x < -50.0f);
        CHECK(aim.x == Approx(motion.x).epsilon(0.1));
    }
}

TEST_CASE("FrameXr Gyro Aim: rolling the controller moves nothing", "[frame_xr][kiugyro]") {
    GyroAimTracker tracker;
    tracker.Update(Aimed(10, 5, 0), 0, 1.0f, false);
    const auto out = tracker.Update(Aimed(10, 5, 20), kStepNs, 1.0f, false);
    REQUIRE(out.right_dps == Approx(0.0f).margin(0.05f));
    REQUIRE(out.down_dps == Approx(0.0f).margin(0.05f));
}

TEST_CASE("FrameXr Gyro Aim: Gyro Sensitivity scales the rate", "[frame_xr][kiugyro]") {
    const float s = GENERATE(0.2f, 0.5f, 1.0f, 2.0f, 4.0f);
    GyroAimTracker one, scaled;
    one.Update(Aimed(0, 0), 0, 1.0f, false);
    scaled.Update(Aimed(0, 0), 0, s, false);
    const auto a = one.Update(Aimed(0.7f, -0.3f), kStepNs, 1.0f, false);
    const auto b = scaled.Update(Aimed(0.7f, -0.3f), kStepNs, s, false);
    REQUIRE(b.right_dps == Approx(a.right_dps * s).epsilon(1e-4));
    REQUIRE(b.down_dps == Approx(a.down_dps * s).epsilon(1e-4));
    SECTION("out of range is clamped, not a fault") {
        REQUIRE(ClampGyroSensitivity(0.0f) == kGyroSensitivityMin);
        REQUIRE(ClampGyroSensitivity(9.0f) == kGyroSensitivityMax);
        REQUIRE(ClampGyroSensitivity(std::nanf("")) == kGyroSensitivityDefault);
    }
}

TEST_CASE("FrameXr Gyro Aim: the grip pauses, and letting go does not jump", "[frame_xr][kiugyro]") {
    GyroAimTracker tracker;
    GripClutch clutch;
    int64_t t = 0;
    float yaw = 0.0f;
    float moved_while_free = 0.0f;
    // Turn right 30 degrees, hold the grip, swing back 30 degrees left, let go, turn right again.
    const auto step = [&](float d_yaw, float grip) {
        yaw += d_yaw;
        t += kStepNs;
        const bool held = clutch.Update(grip);
        const auto out = tracker.Update(Aimed(yaw, 0), t, 1.0f, held);
        return std::pair{out, held};
    };
    for (int i = 0; i < 30; i++) {
        moved_while_free += step(1.0f, 0.0f).first.right_dps * (kStepNs * 1e-9f);
    }
    REQUIRE(moved_while_free == Approx(29.0f).margin(1.1f)); // the first frame is a start
    for (int i = 0; i < 30; i++) {
        const auto [out, held] = step(-1.0f, 1.0f);
        REQUIRE(held);
        REQUIRE(out.right_dps == 0.0f);
        REQUIRE(out.down_dps == 0.0f);
    }
    // Let go: the hand is 30 degrees left of where it was, and the first frame sends nothing.
    const auto [first, held] = step(0.0f, 0.0f);
    REQUIRE(!held);
    REQUIRE(first.right_dps == 0.0f);
    // Then it carries on from the new place.
    const auto next = step(1.0f, 0.0f).first;
    REQUIRE(next.right_dps == Approx(90.0f).epsilon(0.02));
    SECTION("the grip's hysteresis") {
        GripClutch c;
        REQUIRE(!c.Update(0.55f));
        REQUIRE(c.Update(0.65f));
        REQUIRE(c.Update(0.45f));
        REQUIRE(!c.Update(0.35f));
    }
}

TEST_CASE("FrameXr Gyro Aim: ModRateFor gives the mod the rate asked for", "[frame_xr][kiugyro]") {
    for (float r = -400.0f; r <= 400.0f; r += 0.37f) {
        const float sent = ModRateFor(r);
        const float applied = ModAxis(sent);
        if (std::fabs(r) < 0.02f) {
            REQUIRE(applied == 0.0f);
        } else if (std::fabs(r) <= 359.0f) {
            REQUIRE(applied == Approx(r).margin(kHidGyroStepDps));
        } else {
            REQUIRE(std::fabs(applied) ==
                    Approx(std::min(std::fabs(r), 360.0f)).margin(kHidGyroStepDps));
        }
    }
    REQUIRE(ModRateFor(std::nanf("")) == 0.0f);
}

TEST_CASE("FrameXr Gyro Aim: Light Gun's target and Turn Zone", "[frame_xr][kiugyro]") {
    PanelQuad top;
    top.visible = true;
    top.width_m = 1.6f;
    top.height_m = 0.96f;
    top.pose.position = {0.0f, 0.0f, -2.0f};
    SECTION("the middle, the corners") {
        const LightGunTarget mid = LightGunTargetFor(Aimed(0, 0), top);
        REQUIRE(mid.valid);
        REQUIRE(mid.u == Approx(0.5f).margin(1e-4f));
        REQUIRE(mid.v == Approx(0.5f).margin(1e-4f));
        const float right = std::atan2(0.8f, 2.0f) / kDeg;
        const float up = std::atan2(0.48f, 2.0f) / kDeg;
        const LightGunTarget tr = LightGunTargetFor(Aimed(right * 0.999f, 0), top);
        REQUIRE(tr.u == Approx(1.0f).margin(2e-3f));
        HandInput h = Aimed(0, 0);
        h.aim.orientation = AxisAngle({1, 0, 0}, up);
        const LightGunTarget t = LightGunTargetFor(h, top);
        REQUIRE(t.v == Approx(0.0f).margin(2e-3f));
    }
    SECTION("not shown, not tracked, pointing away") {
        PanelQuad hidden = top;
        hidden.visible = false;
        REQUIRE(!LightGunTargetFor(Aimed(0, 0), hidden).valid);
        HandInput lost = Aimed(0, 0);
        lost.active = false;
        REQUIRE(!LightGunTargetFor(lost, top).valid);
        REQUIRE(!LightGunTargetFor(Aimed(180, 0), top).valid);
    }
    SECTION("depth in the zone") {
        const float zone = 0.1f;
        REQUIRE(TurnZoneDepth({true, 0.5f, 0.5f}, zone).x == 0.0f);
        REQUIRE(TurnZoneDepth({true, 0.95f, 0.5f}, zone).x == Approx(0.5f));
        REQUIRE(TurnZoneDepth({true, 1.2f, 0.5f}, zone).x == 1.0f);
        REQUIRE(TurnZoneDepth({true, 0.02f, 0.5f}, zone).x == Approx(-0.8f));
        REQUIRE(TurnZoneDepth({true, 0.5f, 0.0f}, zone).y == -1.0f);
        REQUIRE(TurnZoneDepth({true, 0.95f, 0.5f}, 0.0f).x == 0.0f); // Turn Zone 0%: off
        REQUIRE(TurnZoneDepth({false, 0.95f, 0.5f}, zone).x == 0.0f);
    }
}

// AzaharFrame kiuaim2: the relative loop's simulations (LightGunLoop) are gone with it; Direct
// Aim's own tests are in direct_aim.cpp.

// AzaharFrame kiufix2: "The C-Stick is still nice to use." With the game's controls on C-Stick
// the mod moves the reticle with the stick and the gyro together; Direct Aim used to pull the
// reticle back under the pointer every poll, undoing the stick's moves and fighting its turns.
TEST_CASE("FrameXr Light Gun: the C-Stick wins while pushed", "[frame_xr][kiufix2]") {
    CHECK_FALSE(CStickTakesAim({0.0f, 0.0f}));      // centred (inside the dead zone: 0)
    CHECK_FALSE(CStickTakesAim({0.005f, -0.005f})); // float noise
    CHECK(CStickTakesAim({0.05f, 0.0f}));           // just past the dead zone
    CHECK(CStickTakesAim({0.0f, -1.0f}));
    CHECK(CStickTakesAim({-0.6f, 0.6f}));
}

// ---- The mod scan (kiu_scan.h) -------------------------------------------------------------

namespace {
void Put(std::vector<uint8_t>& b, std::size_t at, std::initializer_list<uint32_t> words) {
    for (uint32_t w : words) {
        std::memcpy(b.data() + at, &w, 4);
        at += 4;
    }
}
/// A data segment with the three v172 patterns at their places (base 0xD32000), and the
/// literals they load.
std::vector<uint8_t> FakeModCode() {
    constexpr uint32_t base = 0xD32000;
    std::vector<uint8_t> b(0x22D000, 0);
    // Options check at 0xD33978: ldr ip,[pc,#0x60] -> literal 0xD339E0 = 0xD636C4.
    Put(b, 0xD33978 - base, {0xE59FC060, 0xE59CC000, 0xE35C0000, 0x0A000006, 0xE28CCA16,
                             0xE59CC3E8, 0xE20CC60E, 0xE35C0602});
    Put(b, 0xD339E0 - base, {0x00D636C4});
    // State hook at 0xF45398: push; ldr r4,[pc,#0xAF4] -> 0xF45E98 = 0xF35D40; sub; ldr; cmp.
    Put(b, 0xF45398 - base, {0xE92D4070, 0xE59F4AF4, 0xE2405C01, 0xE594601C, 0xE1510006});
    Put(b, 0xF45E98 - base, {0x00F35D40});
    // The box hook's reads at +0xA4 and +0xA8 (0xF453E8).
    Put(b, 0xF453E8 - base, {0xE1A04000, 0xE1A05001, 0xE1A06002, 0xE1A07003, 0xE59D8020,
                             0xE59DC024, 0xE59490A4, 0xE594A0A8});
    return b;
}
} // namespace

TEST_CASE("FrameXr Light Gun: the mod's addresses are found by their code", "[frame_xr][kiugyro]") {
    using namespace FrameXr::Kiu;
    std::vector<uint8_t> b = FakeModCode();
    SECTION("found") {
        const ModAddresses a = ScanForMod(b.data(), b.size(), 0xD32000);
        REQUIRE(a.found);
        REQUIRE(a.state == 0xF35D40u);
        REQUIRE(a.options_ptr == 0xD636C4u);
    }
    SECTION("the game without the mod, or a build that changed: not found") {
        std::vector<uint8_t> none(b.size(), 0);
        REQUIRE(!ScanForMod(none.data(), none.size(), 0xD32000).found);
        b[0xF453E8 - 0xD32000 + 24] = 0xA0; // +0xA0 instead of +0xA4: another layout
        REQUIRE(!ScanForMod(b.data(), b.size(), 0xD32000).found);
    }
    SECTION("twice is ambiguous: not found") {
        std::memcpy(b.data() + 0x100, b.data() + (0xF45398 - 0xD32000), 20);
        REQUIRE(!ScanForMod(b.data(), b.size(), 0xD32000).found);
    }
    SECTION("the scheme bits and the title ids") {
        REQUIRE(SchemeOf(0x00200000u) == kSchemeGyro);
        REQUIRE(SchemeOf(0x00600000u) == 3u);
        REQUIRE(SchemeOf(0x001FFFFFu) == 0u);
        REQUIRE(IsKidIcarus(0x0004000000030200ull));
        REQUIRE(!IsKidIcarus(0x0004000000030600ull));
    }
}

// ---- Light Gun's laser sight (vis/beam.h) --------------------------------------------------

TEST_CASE("FrameXr Light Gun: the laser sight shows with Direct Aim on the top screen",
          "[frame_xr][kiugyro]") {
    const LightGunTarget on{true, 0.4f, 0.6f};
    SECTION("on the top screen, in a game: red") {
        CHECK(LaserSight(GyroAimMode::LightGun, true, on));
        CHECK(LaserSight(GyroAimMode::LightGun, true, {true, 0.0f, 1.0f})); // its edge
    }
    SECTION("off the top screen, or pointing away: the usual beam") {
        CHECK(!LaserSight(GyroAimMode::LightGun, true, {true, 1.05f, 0.5f}));
        CHECK(!LaserSight(GyroAimMode::LightGun, true, {true, 0.5f, -0.1f}));
        CHECK(!LaserSight(GyroAimMode::LightGun, true, {false, 0.5f, 0.5f}));
    }
    SECTION("menus, Gyro, Off: the usual beam") {
        CHECK(!LaserSight(GyroAimMode::LightGun, false, on));
        CHECK(!LaserSight(GyroAimMode::Gyro, true, on));
        CHECK(!LaserSight(GyroAimMode::Off, true, on));
    }
}

// AzaharFrame kiufix: the owner's session: Direct Aim had not read the reticle yet and Laser
// Sight, on, drew nothing at all.
TEST_CASE("FrameXr Light Gun: Laser Sight does not wait for the reticle's reading",
          "[frame_xr][kiufix]") {
    const LightGunTarget on{true, 0.5f, 0.5f};
    for (const bool weapon : {false, true}) {
        const AimBeam b = AimBeamFor(GyroAimMode::LightGun, true, weapon, on);
        CHECK(b.laser);
        CHECK_FALSE(b.hide_top);
    }
}

TEST_CASE("FrameXr Light Gun: the laser sight's art", "[frame_xr][kiugyro]") {
    using namespace FrameXr::Vis;
    SECTION("the red beam is the white one's shape, in red") {
        const std::vector<uint8_t> white = BeamArt();
        const std::vector<uint8_t> red = BeamArt(kLaserRgb);
        REQUIRE(white.size() == red.size());
        bool some = false;
        for (std::size_t i = 0; i < red.size(); i += 4) {
            REQUIRE(red[i + 3] == white[i + 3]); // the same alpha everywhere
            REQUIRE(red[i] <= red[i + 3]);        // premultiplied
            REQUIRE(red[i + 1] <= red[i]);        // red over green and blue
            REQUIRE(red[i + 2] <= red[i]);
            some = some || red[i + 3] > 200;
        }
        REQUIRE(some);
    }
    SECTION("the dot: opaque red in the middle, clear at the corners, small") {
        const std::vector<uint8_t> dot = LaserDotArt();
        REQUIRE(dot.size() == static_cast<std::size_t>(kLaserDotPixels * kLaserDotPixels * 4));
        const auto at = [&](int x, int y) { return &dot[(y * kLaserDotPixels + x) * 4]; };
        const uint8_t* mid = at(kLaserDotPixels / 2, kLaserDotPixels / 2);
        CHECK(mid[3] == 255);
        CHECK(mid[0] == 255);
        const uint8_t* rim = at(kLaserDotPixels / 2, kLaserDotPixels / 2 - 13);
        CHECK(rim[0] > 2 * rim[1]); // red toward the rim
        CHECK(at(0, 0)[3] == 0);
        CHECK(at(kLaserDotPixels - 1, kLaserDotPixels - 1)[3] == 0);
        // Half the pointer dot's size: about 1 cm at 3 m, a few game pixels on a 1.6 m screen.
        CHECK(LaserDotSize(3.0f) == Approx(0.0095f));
        CHECK(LaserDotSize(1.0f) < 0.01f + 0.003f * 1.0f);
    }
}

// ---- Recoil (gyro_aim.h) -------------------------------------------------------------------

TEST_CASE("FrameXr Gyro Aim: Recoil's pulses", "[frame_xr][kiugyro]") {
    Recoil r;
    constexpr int64_t ms = 1'000'000;
    int64_t t = 1000 * ms;
    const auto step = [&](int64_t dt_ms, bool fire, bool active = true) {
        t += dt_ms * ms;
        return r.Update(t, fire, active);
    };
    SECTION("a press: one pulse; the first is the charged one (heuristic)") {
        RecoilPulse p = step(11, true);
        REQUIRE(p.fire);
        CHECK(p.duration_ns == Recoil::kChargedPulseNs);
        CHECK(!step(11, true).fire); // still held, before the pace
        CHECK(!step(11, false).fire);
        p = step(300, true); // a quick second press: a plain shot
        REQUIRE(p.fire);
        CHECK(p.duration_ns == Recoil::kPulseNs);
    }
    SECTION("held: a pulse at the pace, short ones") {
        step(11, true);
        int pulses = 0;
        for (int i = 0; i < 90; i++) { // ~1 s more at 90 Hz
            const RecoilPulse p = step(11, true);
            if (p.fire) {
                pulses++;
                CHECK(p.duration_ns == Recoil::kPulseNs);
            }
        }
        CHECK(pulses == 7); // 990 ms / 125 ms, the press's own pulse aside
    }
    SECTION("a press after a charge-length pause: the longer pulse") {
        step(11, true);
        step(11, false);
        step(Recoil::kChargePauseNs / ms - 50, false);
        CHECK(step(11, true).duration_ns == Recoil::kPulseNs); // not long enough
        step(11, false);
        step(Recoil::kChargePauseNs / ms + 10, false);
        CHECK(step(11, true).duration_ns == Recoil::kChargedPulseNs);
    }
    SECTION("never while inactive (menus, Gyro Aim off, Recoil off)") {
        for (int i = 0; i < 50; i++) {
            CHECK(!step(11, i % 7 < 4, false).fire);
        }
        // A trigger held through it is no new shot when it comes back, only the pace.
        step(11, true, false);
        CHECK(!step(11, true).fire);
        bool later = false;
        for (int i = 0; i < 20; i++) {
            later = later || step(11, true).fire;
        }
        CHECK(later);
    }
}
