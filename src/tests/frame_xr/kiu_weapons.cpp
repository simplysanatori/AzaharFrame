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
    REQUIRE(FrameWeapons::ReadManifest(std::string{dir} + "/weapons.txt", m, error));
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
            const auto fits = Vis::FitWeaponToHand({w.cls, w.muzzle, w.muzzle_valid}, left, sides);
            // Palm, elbow mode (AZAHARFRAME_KIU_PALM_MODE=elbow): the forearm's turn as the app
            // works it out, for a head 0.42 m above, 0.38 m behind and 0.18 m inward of the
            // controller (aim space taken as the room, the controller held level).
            const bool elbow = w.cls == 4 && fits.size() == 1 &&
                               Vis::PalmModeFrom(std::getenv(Vis::kPalmModeSwitch)
                                                     ? std::optional<std::string>{std::getenv(Vis::kPalmModeSwitch)}
                                                     : std::nullopt) == Vis::PalmMode::Elbow;
            if (elbow) {
                const auto wrist = Point(fits[0].transform, {0, 0, 0});
                const auto along = Point(fits[0].transform, {1, 0, 0});
                const std::array<float, 3> head{left ? 0.18f : -0.18f, 0.42f, 0.38f};
                const auto e = Vis::EstimateElbow(head, {0, 0, -1}, left, wrist);
                const auto turn = Vis::RotationBetween(
                    wrist, {along[0] - wrist[0], along[1] - wrist[1], along[2] - wrist[2]},
                    {e[0] - wrist[0], e[1] - wrist[1], e[2] - wrist[2]});
                Vis::FitMat m{};
                for (int c = 0; c < 4; c++) {
                    for (int r = 0; r < 4; r++) {
                        float v = 0.0f;
                        for (int k = 0; k < 4; k++) {
                            v += turn[static_cast<std::size_t>(k * 4 + r)] *
                                 fits[0].transform[static_cast<std::size_t>(c * 4 + k)];
                        }
                        m[static_cast<std::size_t>(c * 4 + r)] = v;
                    }
                }
                o << w.id << ' ' << (left ? 'L' : 'R') << " F " << (fits[0].mirrored ? 1 : 0);
                for (const float v : m) {
                    o << ' ' << v;
                }
                o << ' ' << Vis::FitClipX(w.cls, {}, std::nullopt, Vis::PalmMode::Elbow) << " 1\n";
            }
            for (const auto& f : fits) {
                o << w.id << ' ' << (left ? 'L' : 'R') << ' ' << "CRL"[static_cast<int>(f.side)] << ' '
                  << (f.mirrored ? 1 : 0);
                for (const float v : f.transform) {
                    o << ' ' << v;
                }
                // The cut: the hand alone in elbow mode (the forearm is the F line), else the stub.
                o << ' ' << (elbow ? 1e-5f : Vis::FitClipX(w.cls, {}, std::nullopt)) << '\n';
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

TEST_CASE("Laser Sight and Recoil: each switch turns its feature off", "[kiuguns]") {
    using FrameXr::Input::AimBeamFor;
    using FrameXr::Input::GyroAimMode;
    using FrameXr::Input::LightGunTarget;
    using FrameXr::Input::RecoilActive;
    LightGunTarget on_screen;
    on_screen.valid = true;
    on_screen.u = 0.5f;
    on_screen.v = 0.5f;
    // Direct Aim: Laser Sight on draws the red laser (kiufix: read reticle or not), off hides the
    // beam over the top screen; with or without the weapon.
    for (const bool weapon : {false, true}) {
        auto b = AimBeamFor(GyroAimMode::LightGun, true, weapon, on_screen);
        CHECK(b.laser);
        CHECK_FALSE(b.hide_top);
        b = AimBeamFor(GyroAimMode::LightGun, false, weapon, on_screen);
        CHECK_FALSE(b.laser);
        CHECK(b.hide_top);
    }
    // Off the screen: no laser, whatever the switch.
    CHECK_FALSE(AimBeamFor(GyroAimMode::LightGun, true, true, LightGunTarget{}).laser);
    // Gyro Aim: no laser; the beam over the top screen hidden only with the weapon in hand.
    CHECK_FALSE(AimBeamFor(GyroAimMode::Gyro, true, true, on_screen).laser);
    CHECK_FALSE(AimBeamFor(GyroAimMode::Gyro, false, false, on_screen).hide_top);
    CHECK(AimBeamFor(GyroAimMode::Gyro, false, true, on_screen).hide_top);
    // Recoil off: no pulse at all, on the game's shots or timed.
    CHECK(RecoilActive(1, true));
    CHECK_FALSE(RecoilActive(1, false));
    CHECK_FALSE(RecoilActive(-1, true));
    FrameXr::Input::ShotRecoil shots;
    FrameXr::Input::Recoil timed;
    const bool active = RecoilActive(1, false);
    for (int i = 0; i < 50; i++) {
        const int64_t t = int64_t{i} * 16'000'000;
        CHECK_FALSE(shots.Update(t, 100 + static_cast<uint64_t>(i), true, active).fire);
        CHECK_FALSE(timed.Update(t, (i % 4) < 2, active).fire);
    }
}

TEST_CASE("Weapon fit: every class's muzzle on the aim ray", "[kiuguns]") {
    // Each class's muzzle as the game's json gives it (the study's first weapon of each).
    const struct {
        int cls;
        std::array<float, 3> muzzle;
    } kSamples[] = {{0, {-4.55f, 0.10f, -9.89f}}, {1, {-0.2f, 0.0f, 12.4f}},
                    {2, {-1.3f, 0.0f, 0.0f}},     {3, {0.0f, -0.175f, 0.0f}},
                    {4, {-2.9f, 0.0f, 0.0f}},     {5, {0.03f, 0.01f, -16.49f}},
                    {6, {-8.2f, -4.7f, 1.3f}},    {7, {0.0f, 0.0f, 1.1f}},
                    {8, {-5.7f, 0.0f, 0.0f}}};
    for (const auto& s : kSamples) {
        const std::vector<Vis::FitSide> sides =
            (s.cls == 2 || s.cls == 7) ? std::vector{Vis::FitSide::Right, Vis::FitSide::Left}
                                       : std::vector{Vis::FitSide::Centre};
        for (const bool left : {false, true}) {
            const auto m = Vis::FitMuzzle({s.cls, s.muzzle, true}, left, sides);
            REQUIRE(m);
            INFO("class " << s.cls << (left ? " left" : " right"));
            if (s.cls == 3) {
                // A bow shoots from the hand: its muzzle is the grip, at the palm, under the ray.
                CHECK((*m)[1] == Approx(-0.0525f).margin(0.02f));
                continue;
            }
            if (s.cls == 7) {
                // The orbitars' middle: above the hand, on its line.
                CHECK((*m)[0] == Approx(0.0f).margin(1e-4));
                continue;
            }
            CHECK((*m)[0] == Approx(0.0f).margin(1e-3));
            CHECK((*m)[1] == Approx(0.0f).margin(1e-3));
            CHECK((*m)[2] < 0.06f);
        }
    }
}

TEST_CASE("Weapon fit: a palm's forearm cut, tunable live", "[kiuguns]") {
    // Only palms are cut: a short stub at the wrist by default, the forearm in elbow mode.
    CHECK(Vis::FitClipX(0, {}, std::nullopt) == 0.0f);
    const float def = Vis::FitClipX(4, {}, std::nullopt);
    CHECK(def * Vis::FitClassScale(4) * 100.0f == Approx(Vis::kPalmStubCm));
    CHECK(Vis::FitClipX(4, {}, std::nullopt, Vis::PalmMode::Elbow) * Vis::FitClassScale(4) * 100.0f ==
          Approx(Vis::kPalmForearmCm));
    CHECK(Vis::PalmModeFrom(std::string{"elbow"}) == Vis::PalmMode::Elbow);
    CHECK(Vis::PalmModeFrom(std::string{"wrist"}) == Vis::PalmMode::Wrist);
    CHECK(Vis::PalmModeFrom(std::nullopt) == Vis::PalmMode::Wrist);
    CHECK(FrameXr::TestSwitches::ApplyFor(Vis::kPalmModeSwitch) == FrameXr::TestSwitches::Apply::Live);
    CHECK(Vis::FitClipX(4, {}, std::string{"10"}) * Vis::FitClassScale(4) * 100.0f == Approx(10.0f));
    CHECK(Vis::FitClipX(4, {}, std::string{"x"}) == Approx(def));
    // A scaled palm keeps the same length in cm (the cut is in the weapon's units).
    const auto half = Vis::ParseFitOverride(std::string{"0,0,0,0,0,0,0.5"});
    CHECK(Vis::FitClipX(4, half, std::nullopt) == Approx(def * 2.0f));
    // Burning Palm (its json: the muzzle 2.9 units past the wrist): Pit's palm at the grip, the
    // muzzle on the ray, the wrist behind the palm and the cut forearm further back.
    const Vis::FitWeapon palm{4, {-2.9f, 0.0f, 0.0f}, true};
    const auto p = Vis::FitWeaponToHand(palm, false, {Vis::FitSide::Centre});
    REQUIRE(p.size() == 1);
    const auto at_palm = Point(p[0].transform, {-0.7f, 0, 0});
    CHECK(at_palm[1] == Approx(-0.0525f));
    CHECK(at_palm[2] == Approx(0.043f));
    const auto wrist = Point(p[0].transform, {0, 0, 0});
    const auto cut = Point(p[0].transform, {def, 0, 0});
    CHECK(wrist[2] > at_palm[2]);
    CHECK(cut[2] == Approx(wrist[2] + Vis::kPalmStubCm / 100.0f).margin(0.02f));
    // Palms stay still, their flames too.
    CHECK(Vis::FitMotion(4, Vis::FitSide::Centre, true, 0.6) == std::array<float, 3>{});
}

TEST_CASE("Palm elbow mode: the elbow estimate, the smoothing, the turn", "[kiuguns]") {
    using V = std::array<float, 3>;
    const auto len = [](const V& a, const V& b) {
        return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) +
                         (a[2] - b[2]) * (a[2] - b[2]));
    };
    const V head{0.0f, 1.6f, 0.0f};
    const V ahead{0.0f, 0.0f, -1.0f};
    const V shoulder_r{0.16f, 1.43f, 0.0f};
    // A right hand held in front, low: the elbow below the shoulder-wrist line, on the right,
    // and the arm's two bones their lengths.
    const V wrist{0.20f, 1.10f, -0.35f};
    const V e = Vis::EstimateElbow(head, ahead, false, wrist);
    CHECK(len(e, shoulder_r) == Approx(0.30f).margin(1e-3));
    CHECK(len(e, wrist) == Approx(0.27f).margin(1e-3));
    CHECK(e[0] > 0.1f);
    const float t = (e[2] - shoulder_r[2]) / (wrist[2] - shoulder_r[2]);
    CHECK(e[1] < shoulder_r[1] + (wrist[1] - shoulder_r[1]) * t); // bent downward
    // The left hand: the mirror image.
    const V el = Vis::EstimateElbow(head, ahead, true, {-wrist[0], wrist[1], wrist[2]});
    CHECK(el[0] == Approx(-e[0]).margin(1e-4));
    CHECK(el[1] == Approx(e[1]).margin(1e-4));
    // Looking down does not move the shoulder (the yaw frame); turning the head does.
    const V down = Vis::EstimateElbow(head, {0.0f, -0.9f, -0.1f}, false, wrist);
    CHECK(down[0] == Approx(e[0]).margin(1e-3));
    const V turned = Vis::EstimateElbow(head, {1.0f, 0.0f, 0.0f}, false, {0.35f, 1.10f, 0.20f});
    CHECK(turned[2] > 0.05f); // the right side is +z when facing +x
    // An arm stretched further than it reaches: straight, the elbow on the line.
    const V far{0.16f, 1.43f, -1.2f};
    const V s = Vis::EstimateElbow(head, ahead, false, far);
    CHECK(s[0] == Approx(0.16f).margin(0.01f));
    CHECK(s[1] == Approx(1.43f).margin(0.01f));
    // Smoothing: eases, stays a unit vector, gets there.
    V d = Vis::SmoothDirection({}, {0.0f, 0.0f, 1.0f}, -1.0f);
    CHECK(d[2] == Approx(1.0f));
    d = Vis::SmoothDirection(d, {1.0f, 0.0f, 0.0f}, 0.011f);
    CHECK(d[0] > 0.0f);
    CHECK(d[0] < 0.2f);
    CHECK(std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) == Approx(1.0f));
    for (int i = 0; i < 100; i++) {
        d = Vis::SmoothDirection(d, {1.0f, 0.0f, 0.0f}, 0.011f);
    }
    CHECK(d[0] == Approx(1.0f).margin(1e-3));
    // The turn about the wrist: the wrist stays, the forearm's direction becomes the target.
    const V pivot{0.2f, 1.1f, -0.35f};
    const Vis::FitMat r = Vis::RotationBetween(pivot, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f});
    const auto p0 = Point(r, pivot);
    CHECK(len(p0, pivot) < 1e-5f);
    const auto p1 = Point(r, {pivot[0], pivot[1], pivot[2] + 0.2f});
    CHECK(p1[1] == Approx(pivot[1] - 0.2f).margin(1e-5));
    CHECK(Det(r) == Approx(1.0f));
    const Vis::FitMat back = Vis::RotationBetween(pivot, {0, 0, 1}, {0, 0, -1}); // opposite
    CHECK(Point(back, {pivot[0], pivot[1], pivot[2] + 1.0f})[2] == Approx(pivot[2] - 1.0f).margin(1e-4));
}

// AzaharFrame kiufix: Weapon Size. Hand Size brings a big weapon down to its class's hand-held
// length; Weapon Scale goes on top; scaled about the grip, so the grip stays in the palm and the
// muzzle on the aim ray, pointing ahead.
TEST_CASE("Weapon Size: big weapons to a hand size, about the grip", "[kiufix]") {
    // The Ball Cannon as converted (weapon 73: RArmN, muzzle and bounds from its manifest).
    const Vis::FitWeapon cannon{6, {-6.8f, -2.4000006f, 1.4999999f}, true};
    const std::array<float, 3> lo{-9.30427f, -4.9350095f, -3.92186f};
    const std::array<float, 3> hi{2.7301297f, 7.699711f, 3.54042f};
    const float s = Vis::FitClassScale(6);
    CHECK(Vis::WeaponSizeFactor(6, lo, hi, s, Vis::WeaponSize::Game, 100) == 1.0f);
    const float hand = Vis::WeaponSizeFactor(6, lo, hi, s, Vis::WeaponSize::Hand, 100);
    CHECK(hand < 1.0f);
    // Its longest side (12.6 units, 76 cm at the class scale) becomes the cap.
    CHECK((hi[1] - lo[1]) * s * hand == Approx(Vis::HandSizeCapM(6)).epsilon(0.01));
    CHECK(Vis::WeaponSizeFactor(6, lo, hi, s, Vis::WeaponSize::Hand, 150) ==
          Approx(hand * 1.5f));
    CHECK(Vis::WeaponSizeFactor(6, lo, hi, s, Vis::WeaponSize::Game, 10) == Approx(0.5f));
    // A small weapon stays as it is.
    const std::array<float, 3> slo{-1, -1, -1}, shi{1, 1, 1};
    CHECK(Vis::WeaponSizeFactor(0, slo, shi, Vis::FitClassScale(0), Vis::WeaponSize::Hand, 100) ==
          1.0f);
    for (const float f : {1.0f, hand, hand * 0.5f, 1.5f}) {
        INFO("factor " << f);
        Vis::FitOverride tune;
        tune.scale = f;
        const auto parts = Vis::FitWeaponToHand(cannon, false, {Vis::FitSide::Centre}, tune);
        REQUIRE(parts.size() == 1);
        // The grip (where Pit's hand would be, 2.17 units out along the forearm) in the palm.
        const auto grip = Point(parts[0].transform, {-2.17f, 0, 0});
        CHECK(grip[0] == Approx(0.0f).margin(1e-5));
        CHECK(grip[1] == Approx(-0.0525f).margin(1e-5));
        CHECK(grip[2] == Approx(0.043f).margin(1e-5));
        // The muzzle on the ray, ahead.
        const auto muzzle = Point(parts[0].transform, cannon.muzzle);
        CHECK(muzzle[0] == Approx(0.0f).margin(1e-4));
        CHECK(muzzle[1] == Approx(0.0f).margin(1e-4));
        CHECK(muzzle[2] < 0.0f);
        const auto mz = Vis::FitMuzzle(cannon, false, {Vis::FitSide::Centre}, tune);
        REQUIRE(mz.has_value());
        CHECK((*mz)[0] == Approx(0.0f).margin(1e-4));
        CHECK((*mz)[1] == Approx(0.0f).margin(1e-4));
    }
}
