// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: Kid Icarus: Uprising's weapon in hand and shots: the code patterns on
// small hand-made code (no game code), the records' rules, the settling, Recoil on real shots,
// the fit to the hand, the setting's rollback mirror. Two hidden tests ([.kiuguns_real]) run the
// scans on dumped code.bin files and write the fits for the desktop renders.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "core/frame_weapon_models.h"
#include "frame_xr/controls_presets.h"
#include "frame_xr/input/gyro_aim.h"
#include "frame_xr/kiu_scan.h"
#include "frame_xr/test_switches.h"
#include "frame_xr/vis/weapon_fit.h"

using Catch::Approx;
namespace Kiu = FrameXr::Kiu;
namespace Vis = FrameXr::Vis;

namespace {

struct Code {
    std::vector<uint8_t> b;
    explicit Code(std::size_t n) : b(n, 0) {}
    void W(std::size_t at, uint32_t v) {
        for (int k = 0; k < 4; k++) {
            b[at + static_cast<std::size_t>(k)] = static_cast<uint8_t>(v >> (8 * k));
        }
    }
};

/// getWeaponId's tail at `at` with `ldr r5,=ptr` before it and the getter at `get`.
void PutWeaponTail(Code& c, std::size_t at, uint32_t ptr, std::size_t get, uint32_t field = 5) {
    const std::size_t lit = at + 0x100;
    c.W(at - 0x10, 0xE59F5000u | static_cast<uint32_t>(lit - (at - 0x10) - 8)); // ldr r5,[pc,#]
    c.W(lit, ptr);
    const uint32_t tail[] = {0xE5D4102Cu, 0xE8BD41F0u, 0xE0812181u, 0xE0621301u, 0xE0800201u};
    for (std::size_t k = 0; k < 5; k++) {
        c.W(at + 4 * k, tail[k]);
    }
    const std::size_t b = at + 20;
    c.W(b, 0xEA000000u | (static_cast<uint32_t>((static_cast<int64_t>(get) - static_cast<int64_t>(b) - 8) / 4) & 0xFFFFFFu));
    c.W(get, 0xE1D000D0u | ((field & 0xF0u) << 4) | (field & 0xFu)); // ldrsb r0,[r0,#field]
    c.W(get + 4, 0xE12FFF1Eu);                                         // bx lr
}

void PutShotTail(Code& c, std::size_t at, uint32_t cache, uint32_t cap = 999'999'999u) {
    const uint32_t words[] = {0xE59F4050u, 0xE5940000u, 0xE3500000u, 0x1A000001u, 0xEB000123u,
                              0xE5840000u, 0xE2802A16u, 0xE2822F9Eu, 0xE59F1034u, 0xE59201DCu,
                              0xE2800001u, 0xE3500001u, 0x3A000001u, 0xE1500001u, 0x9A000000u,
                              0xE1A00001u, 0xE58201DCu};
    for (std::size_t k = 0; k < 17; k++) {
        c.W(at + 4 * k, words[k]);
    }
    c.W(at + 8 + 0x50, cache);    // ldr r4's literal
    c.W(at + 32 + 8 + 0x34, cap); // ldr r1's literal
}

Kiu::FighterRecords Single(int16_t kind, int8_t weapon) {
    Kiu::FighterRecords r;
    r.kind.fill(-1);
    r.weapon.fill(-1);
    r.kind[0] = kind;
    r.weapon[0] = weapon;
    return r;
}

std::array<float, 3> Point(const Vis::FitMat& m, const std::array<float, 3>& p) {
    return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
            m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
            m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
}

float Det(const Vis::FitMat& m) {
    return m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) +
           m[8] * (m[1] * m[6] - m[5] * m[2]);
}

} // namespace

TEST_CASE("Kid Icarus weapon: the getWeaponId pattern, once", "[kiuguns]") {
    Code c(0x1000);
    PutWeaponTail(c, 0x200, 0xD636B4, 0x800);
    auto a = Kiu::ScanForWeapon(c.b.data(), c.b.size(), 0x100000);
    REQUIRE(a.found);
    CHECK(a.ptr == 0xD636B4u);
    CHECK(a.field == 5);
    // A second copy: not sure which, none.
    PutWeaponTail(c, 0x600, 0xD6364C, 0x900);
    CHECK_FALSE(Kiu::ScanForWeapon(c.b.data(), c.b.size(), 0x100000).found);
    // A getter that is not `ldrsb r0,[r0,#5]; bx lr`: none.
    Code d(0x1000);
    PutWeaponTail(d, 0x200, 0xD636B4, 0x800, 6);
    CHECK_FALSE(Kiu::ScanForWeapon(d.b.data(), d.b.size(), 0x100000).found);
    Code e(0x1000);
    PutWeaponTail(e, 0x200, 0xD636B4, 0x800);
    e.W(0x804, 0xE1A00000u); // not bx lr
    CHECK_FALSE(Kiu::ScanForWeapon(e.b.data(), e.b.size(), 0x100000).found);
}

TEST_CASE("Kid Icarus shots: the shot function's pattern, once", "[kiuguns]") {
    Code c(0x1000);
    PutShotTail(c, 0x300, 0xEE4000);
    auto a = Kiu::ScanForShots(c.b.data(), c.b.size(), 0x100000);
    REQUIRE(a.found);
    CHECK(a.cache == 0xEE4000u);
    CHECK(a.offset == 0x16000u + 0x278u + 0x1DCu);
    PutShotTail(c, 0x700, 0xEE4000);
    CHECK_FALSE(Kiu::ScanForShots(c.b.data(), c.b.size(), 0x100000).found);
    Code d(0x1000);
    PutShotTail(d, 0x300, 0xEE4000, 12345); // not the cap
    CHECK_FALSE(Kiu::ScanForShots(d.b.data(), d.b.size(), 0x100000).found);
}

TEST_CASE("Kid Icarus weapon: single player only, values guarded", "[kiuguns]") {
    CHECK(Kiu::WeaponFromRecords(Single(1, 49)) == 49);
    CHECK(Kiu::WeaponFromRecords(Single(2, 0)) == 0);
    CHECK(Kiu::WeaponFromRecords(Single(1, -1)) == -1);
    CHECK(Kiu::WeaponFromRecords(Single(0, 49)) == -1); // not a fighter
    auto two = Single(1, 49);
    two.kind[3] = 1;
    two.weapon[3] = 12;
    CHECK(Kiu::WeaponFromRecords(two) == -1); // multiplayer: not guessed
    auto bad = Single(1, 49);
    bad.weapon[2] = 108;
    CHECK(Kiu::WeaponFromRecords(bad) == -2);
    bad.weapon[2] = -5;
    CHECK(Kiu::WeaponFromRecords(bad) == -2);
}

TEST_CASE("Kid Icarus weapon: a new id after half a second", "[kiuguns]") {
    Kiu::WeaponSettle s;
    constexpr int64_t ms = 1'000'000;
    CHECK(s.Update(0, 49) == -1);
    CHECK(s.Update(400 * ms, 49) == -1);
    CHECK(s.Update(500 * ms, 49) == 49);
    // Scrolling through weapons: none taken until one stays.
    CHECK(s.Update(600 * ms, 50) == 49);
    CHECK(s.Update(800 * ms, 51) == 49);
    CHECK(s.Update(1200 * ms, 52) == 49);
    CHECK(s.Update(1750 * ms, 52) == 52);
    CHECK(s.Update(1800 * ms, -1) == 52);
    CHECK(s.Update(2300 * ms, -1) == -1);
}

TEST_CASE("Kid Icarus shots: counted from the record, drops and jumps ignored", "[kiuguns]") {
    Kiu::ShotCounter c;
    CHECK(c.Update(2004) == 0); // the first read: a start
    CHECK(c.Update(2004) == 0);
    CHECK(c.Update(2005) == 1);
    CHECK(c.Update(2007) == 2);
    CHECK(c.Update(10) == 0);   // another save: starts again
    CHECK(c.Update(11) == 1);
    CHECK(c.Update(500) == 0);  // a jump: starts again
    CHECK(c.Update(501) == 1);
}

TEST_CASE("Recoil on the game's shots: a pulse per read with shots, none without", "[kiuguns]") {
    using FrameXr::Input::Recoil;
    using FrameXr::Input::ShotRecoil;
    ShotRecoil r;
    constexpr int64_t ms = 1'000'000;
    CHECK_FALSE(r.Update(0, 100, true, true).fire); // the first read: no pulse
    CHECK_FALSE(r.Update(16 * ms, 100, true, true).fire);
    auto p = r.Update(32 * ms, 101, true, true);
    CHECK(p.fire);
    CHECK(p.duration_ns == Recoil::kPulseNs);
    p = r.Update(48 * ms, 103, true, true); // two in one read: one longer pulse
    CHECK(p.fire);
    CHECK(p.duration_ns == ShotRecoil::kBurstPulseNs);
    CHECK_FALSE(r.Update(64 * ms, 103, true, true).fire); // a press that fired nothing
    // After a charge-length pause: the longer pulse (the heuristic kept).
    p = r.Update(64 * ms + Recoil::kChargePauseNs, 104, true, true);
    CHECK(p.fire);
    CHECK(p.duration_ns == Recoil::kChargedPulseNs);
    // Shots while inactive (a menu) or unreadable: none, and none counted afterwards.
    CHECK_FALSE(r.Update(3000 * ms, 110, true, false).fire);
    CHECK_FALSE(r.Update(3016 * ms, 110, true, true).fire);
    CHECK_FALSE(r.Update(3032 * ms, 115, false, true).fire);
    CHECK_FALSE(r.Update(3048 * ms, 116, true, true).fire);
    CHECK(r.Update(3064 * ms, 117, true, true).fire);
}

TEST_CASE("Weapon fit: held weapons' grip in the palm and muzzle on the ray", "[kiuguns]") {
    // A blade: GripN at the origin, the muzzle 10 units back and out (as the First Blade's).
    const Vis::FitWeapon blade{0, {-4.55f, 0.1f, -9.89f}, true};
    const auto parts = Vis::FitWeaponToHand(blade, false, {Vis::FitSide::Centre});
    REQUIRE(parts.size() == 1);
    const auto grip = Point(parts[0].transform, {0, 0, 0});
    CHECK(grip[0] == Approx(0.0f).margin(1e-5));
    CHECK(grip[1] == Approx(-0.0525f));
    CHECK(grip[2] == Approx(0.043f));
    const auto muzzle = Point(parts[0].transform, blade.muzzle);
    CHECK(muzzle[0] == Approx(0.0f).margin(1e-4)); // on the ray
    CHECK(muzzle[1] == Approx(0.0f).margin(1e-4));
    CHECK(muzzle[2] < -0.5f);                        // ahead, a blade's length out
    CHECK(Det(parts[0].transform) > 0.0f);
    CHECK_FALSE(parts[0].mirrored);
    // The left hand: the same, mirrored.
    const auto left = Vis::FitWeaponToHand(blade, true, {Vis::FitSide::Centre});
    REQUIRE(left.size() == 1);
    CHECK(left[0].mirrored);
    CHECK(Det(left[0].transform) < 0.0f);
    const auto lm = Point(left[0].transform, blade.muzzle);
    CHECK(lm[0] == Approx(0.0f).margin(1e-4));
    CHECK(lm[2] == Approx(muzzle[2]));
    // A club is made a hand size: its head about a metre out, not 2.5.
    const Vis::FitWeapon club{5, {0.03f, 0.01f, -16.49f}, true};
    const auto c = Vis::FitWeaponToHand(club, false, {Vis::FitSide::Centre});
    const auto head = Point(c[0].transform, club.muzzle);
    CHECK(head[2] > -0.75f);
    CHECK(head[2] < -0.4f);
    // A bow: no muzzle to aim by; its arrow axis (+x) along the ray, the limbs (z) upright.
    const Vis::FitWeapon bow{3, {0.0f, -0.175f, 0.0f}, true};
    const auto b = Vis::FitWeaponToHand(bow, false, {Vis::FitSide::Centre});
    const auto ahead = Point(b[0].transform, {1, 0, 0});
    const auto o = Point(b[0].transform, {0, 0, 0});
    CHECK(ahead[2] < o[2]);
    const auto limb = Point(b[0].transform, {0, 0, 1});
    CHECK(limb[1] > o[1]);
}

TEST_CASE("Weapon fit: claws on their own hand, orbitars either side", "[kiuguns]") {
    const Vis::FitWeapon claws{2, {-1.3f, 0, 0}, true};
    const std::vector<Vis::FitSide> both{Vis::FitSide::Right, Vis::FitSide::Left};
    auto r = Vis::FitWeaponToHand(claws, false, both);
    REQUIRE(r.size() == 1);
    CHECK(r[0].side == Vis::FitSide::Right);
    // Pit's fingers (-x on the right hand) along the ray, his thumb (+z) up.
    const auto o = Point(r[0].transform, {0, 0, 0});
    const auto fingers = Point(r[0].transform, {-1, 0, 0});
    const auto thumb = Point(r[0].transform, {0, 0, 1});
    CHECK(fingers[2] < o[2]);
    CHECK(thumb[1] > o[1]);
    auto l = Vis::FitWeaponToHand(claws, true, both);
    REQUIRE(l.size() == 1);
    CHECK(l[0].side == Vis::FitSide::Left);
    CHECK_FALSE(l[0].mirrored);
    const auto lf = Point(l[0].transform, {1, 0, 0}); // the left hand's fingers are +x
    CHECK(lf[2] < Point(l[0].transform, {0, 0, 0})[2]);
    const Vis::FitWeapon orbitars{7, {0, 0, 1.1f}, true};
    auto p = Vis::FitWeaponToHand(orbitars, true, both);
    REQUIRE(p.size() == 2);
    CHECK(Point(p[0].transform, {0, 0, 0})[0] > 0.05f);
    CHECK(Point(p[1].transform, {0, 0, 0})[0] < -0.05f);
    CHECK(Point(p[0].transform, {0, 0, 1})[2] < Point(p[0].transform, {0, 0, 0})[2]);
}

TEST_CASE("Weapon fit: the live tune per class", "[kiuguns]") {
    CHECK(Vis::FitSwitchName(0) == "AZAHARFRAME_KIU_FIT_BLADE");
    CHECK(Vis::FitSwitchName(7) == "AZAHARFRAME_KIU_FIT_ORBITARS");
    CHECK(FrameXr::TestSwitches::ApplyFor("AZAHARFRAME_KIU_FIT_CLUB") ==
          FrameXr::TestSwitches::Apply::Live);
    CHECK(FrameXr::TestSwitches::ApplyFor("AZAHARFRAME_KIU_WEAPON") ==
          FrameXr::TestSwitches::Apply::Live);
    CHECK_FALSE(Vis::ParseFitOverride(std::nullopt).set);
    CHECK_FALSE(Vis::ParseFitOverride(std::string{"x"}).set);
    const auto t = Vis::ParseFitOverride(std::string{"1, -2,3,90,0,0,0.5"});
    REQUIRE(t.set);
    CHECK(t.offset_cm[1] == Approx(-2.0f));
    CHECK(t.yaw == Approx(90.0f));
    CHECK(t.scale == Approx(0.5f));
    const auto partial = Vis::ParseFitOverride(std::string{"0,0,-5"});
    CHECK(partial.offset_cm[2] == Approx(-5.0f));
    CHECK(partial.scale == Approx(1.0f));
    // The offset moves the grip; the scale halves the reach.
    const Vis::FitWeapon blade{0, {-4.55f, 0.1f, -9.89f}, true};
    const auto base = Vis::FitWeaponToHand(blade, false, {Vis::FitSide::Centre});
    const auto tuned = Vis::FitWeaponToHand(blade, false, {Vis::FitSide::Centre},
                                            Vis::ParseFitOverride(std::string{"0,0,-5,0,0,0,0.5"}));
    const auto g0 = Point(base[0].transform, {0, 0, 0});
    const auto g1 = Point(tuned[0].transform, {0, 0, 0});
    CHECK(g1[2] == Approx(g0[2] - 0.05f));
    const auto m0 = Point(base[0].transform, blade.muzzle);
    const auto m1 = Point(tuned[0].transform, blade.muzzle);
    CHECK(std::fabs(m1[2] - g1[2]) == Approx(std::fabs(m0[2] - g0[2]) * 0.5f).epsilon(0.02));
}

TEST_CASE("Weapon in Hand and Laser Sight: a game's choice survives an older build", "[kiuguns]") {
    auto& v = Settings::values;
    v.vr_controls_preset_done.SetGlobal(false);
    v.vr_controls_preset_done.SetValue(FrameXr::kPresetGyroAim);
    v.vr_light_gun_weapon.SetGlobal(false);
    v.vr_light_gun_weapon.SetValue(false);
    v.vr_light_gun_laser.SetGlobal(false);
    v.vr_light_gun_laser.SetValue(true);
    FrameXr::MirrorWeaponInHand();
    const uint32_t saved = v.vr_controls_preset_done.GetValue();
    CHECK((saved & FrameXr::kPresetGyroAim) != 0); // the old bits keep their meaning
    CHECK((saved & FrameXr::kWeaponMirrorMask) >> FrameXr::kWeaponMirrorShift == 1);
    CHECK((saved & FrameXr::kLaserMirrorMask) >> FrameXr::kLaserMirrorShift == 2);
    CHECK((saved & FrameXr::kGyroAimMirrorMask) == 0);
    // An older build saved the file without the keys: they read back as the global values.
    v.vr_light_gun_weapon.SetGlobal(true);
    v.vr_light_gun_laser.SetGlobal(true);
    CHECK(FrameXr::RestoreWeaponInHand());
    CHECK_FALSE(v.vr_light_gun_weapon.UsingGlobal());
    CHECK_FALSE(v.vr_light_gun_weapon.GetValue());
    CHECK_FALSE(v.vr_light_gun_laser.UsingGlobal());
    CHECK(v.vr_light_gun_laser.GetValue());
    v.vr_light_gun_laser.SetGlobal(true);
    // A game on the global value: nothing mirrored, nothing restored.
    v.vr_light_gun_weapon.SetGlobal(true);
    FrameXr::MirrorWeaponInHand();
    CHECK((v.vr_controls_preset_done.GetValue() & FrameXr::kWeaponMirrorMask) == 0);
    CHECK_FALSE(FrameXr::RestoreWeaponInHand());
    v.vr_controls_preset_done.SetGlobal(true);
    v.vr_light_gun_weapon.SetGlobal(true);
}

// ---- Desktop checks (hidden): AZAHARFRAME_TEST_KIU_CODE=<folder of code.bin files> ----

TEST_CASE("Kid Icarus: the patterns in dumped code", "[.kiuguns_real]") {
    const char* dir = std::getenv("AZAHARFRAME_TEST_KIU_CODE");
    REQUIRE(dir);
    int files = 0;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() != ".bin") {
            continue;
        }
        std::ifstream f(e.path(), std::ios::binary);
        const std::vector<uint8_t> code((std::istreambuf_iterator<char>(f)),
                                        std::istreambuf_iterator<char>());
        const auto w = Kiu::ScanForWeapon(code.data(), code.size(), 0x100000);
        const auto s = Kiu::ScanForShots(code.data(), code.size(), 0x100000);
        std::printf("%-22s weapon %s [0x%08X] + slot * 0x370 + %u; shots %s [0x%08X] + 0x%X\n",
                    e.path().filename().string().c_str(), w.found ? "found" : "MISSING", w.ptr,
                    w.field, s.found ? "found" : "MISSING", s.cache, s.offset);
        CHECK(w.found);
        CHECK(s.found);
        files++;
    }
    CHECK(files > 0);
}

// AZAHARFRAME_TEST_KIU_MODELS=<a weapons folder> AZAHARFRAME_TEST_KIU_FITS=<out file>: each
// weapon's fit for both hands, for the desktop renders (handoff tools).
TEST_CASE("Kid Icarus: the fits of converted weapons", "[.kiuguns_real]") {
    const char* dir = std::getenv("AZAHARFRAME_TEST_KIU_MODELS");
    const char* out = std::getenv("AZAHARFRAME_TEST_KIU_FITS");
    REQUIRE(dir);
    REQUIRE(out);
    FrameWeapons::Manifest m;
    std::string error;
    REQUIRE(FrameWeapons::ReadManifest(std::string{dir} + "/weapons.json", m, error));
    std::ofstream o(out);
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
        for (const bool left : {false, true}) {
            for (const auto& f : Vis::FitWeaponToHand({w.cls, w.muzzle, w.muzzle_valid}, left, sides)) {
                o << w.id << ' ' << (left ? 'L' : 'R') << ' ' << "CRL"[static_cast<int>(f.side)] << ' '
                  << (f.mirrored ? 1 : 0);
                for (const float v : f.transform) {
                    o << ' ' << v;
                }
                o << '\n';
            }
        }
    }
    CHECK(m.weapons.size() > 0);
}

TEST_CASE("Weapon fit: the orbitars float, small and across the ray", "[kiuguns]") {
    // Deterministic: the same time, the same place; never along the ray.
    const auto a = Vis::FitMotion(7, Vis::FitSide::Right, false, 1.25);
    const auto b = Vis::FitMotion(7, Vis::FitSide::Right, false, 1.25);
    CHECK(a == b);
    float most = 0.0f, least = 1.0f;
    for (int i = 0; i < 2000; i++) {
        const double t = i * 0.01;
        for (const auto side : {Vis::FitSide::Right, Vis::FitSide::Left}) {
            const auto m = Vis::FitMotion(7, side, false, t);
            CHECK(m[2] == 0.0f);
            const float r = std::sqrt(m[0] * m[0] + m[1] * m[1]);
            most = std::max(most, r);
            least = std::min(least, r);
        }
    }
    CHECK(most <= 0.026f); // a few cm at most
    CHECK(most >= 0.015f); // and it moves
    // Periodic: after 2.6 s the circle is where it was (the bob aside).
    const auto p0 = Vis::FitMotion(7, Vis::FitSide::Right, false, 0.0);
    const auto p1 = Vis::FitMotion(7, Vis::FitSide::Right, false, 2.6 * 2.1 * 10);
    CHECK(p1[0] == Approx(p0[0]).margin(1e-4));
    CHECK(p1[1] == Approx(p0[1]).margin(1e-4));
    // The halves opposite each other.
    const auto r = Vis::FitMotion(7, Vis::FitSide::Right, false, 0.3);
    const auto l = Vis::FitMotion(7, Vis::FitSide::Left, false, 0.3);
    CHECK(r[0] == Approx(-l[0]).margin(1e-6));
    // Nothing else moves but a detached part's gentle bob.
    CHECK(Vis::FitMotion(0, Vis::FitSide::Centre, false, 0.7) == std::array<float, 3>{});
    const auto bob = Vis::FitMotion(0, Vis::FitSide::Centre, true, 0.6);
    CHECK(bob[0] == 0.0f);
    CHECK(std::fabs(bob[1]) <= 0.008f);
    CHECK(bob[1] != 0.0f);
}
