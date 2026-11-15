// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiufix: Angel's Aim's fixes of 2026-11-14: Check Aim's targets, Recoil's strengths, the
// new keys' rollback mirror, "in a battle" for Weapon in Hand (the menu's part:
// angels_aim_menu.cpp, in frame_xr_picker_tests).

#include <cmath>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/controls_presets.h"
#include "frame_xr/input/gyro_aim.h"
#include "frame_xr/kiu_scan.h"
#include "frame_xr/menu/angels_aim_tab.h"

using Catch::Approx;
using namespace FrameXr;
using namespace FrameXr::Input;

namespace {
Posef Facing() {
    return {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
}
} // namespace

// AzaharFrame kiuaim2: Calibrate Aim's fit and steps are gone (Check Aim: direct_aim.cpp).

TEST_CASE("Check Aim: the target on the screen where (u, v) is", "[kiufix]") {
    PanelQuad flat{Facing(), 1.6f, 0.96f, true};
    flat.pose.position = {0.0f, 1.0f, -2.0f};
    for (const float curve : {0.0f, 1.5f}) {
        PanelQuad q = flat;
        q.curve_radius = curve;
        for (const auto& [u, v] : std::vector<std::pair<float, float>>{{0.5f, 0.5f}, {0.1f, 0.2f}, {0.9f, 0.85f}}) {
            const Vec3f p = PointOnQuad(q, u, v);
            // A ray from the viewer through it reads (u, v) back.
            const Vec3f eye{0.0f, 1.0f, 0.0f};
            const Ray ray{eye, Normalized(p - eye)};
            const QuadHit hit = IntersectQuad(ray, q);
            REQUIRE(hit.on_plane);
            CHECK(hit.u == Approx(u).margin(1e-3));
            CHECK(hit.v == Approx(v).margin(1e-3));
        }
    }
}

TEST_CASE("Recoil's strengths: charged and rapid, per game", "[kiufix]") {
    Recoil r;
    const int64_t s = 1'000'000'000;
    const RecoilPulse first = r.Update(0, true, true); // the first press: charged (heuristic)
    CHECK(first.charged);
    CHECK(RecoilAmplitude(first, 100, 50) == 1.0f);
    const RecoilPulse held = r.Update(Recoil::kRecoilPaceNs + 1, true, true);
    REQUIRE(held.fire);
    CHECK_FALSE(held.charged);
    CHECK(RecoilAmplitude(held, 100, 50) == Approx(0.5f));
    CHECK(RecoilAmplitude(held, 100, 0) == 0.0f); // Rapid Fire 0 %: no pulse
    r.Update(s, false, true);
    const RecoilPulse after_pause = r.Update(3 * s, true, true);
    CHECK(after_pause.charged);
    CHECK(RecoilAmplitude(after_pause, 70, 50) == Approx(0.7f));
    CHECK(RecoilAmplitude({}, 100, 100) == 0.0f);
    // The game's own shots the same way.
    ShotRecoil shots;
    shots.Update(0, 10, true, true);
    const RecoilPulse one = shots.Update(s / 10, 11, true, true);
    CHECK_FALSE(one.charged);
    const RecoilPulse later = shots.Update(4 * s, 12, true, true);
    CHECK(later.charged);
}

TEST_CASE("Recoil's strengths and Weapon Size: a game's values survive an older build",
          "[kiufix]") {
    auto& v = Settings::values;
    v.vr_controls_preset_done.SetGlobal(false);
    v.vr_controls_preset_done.SetValue(kPresetGyroAim | (2u << kLaserMirrorShift));
    v.vr_light_gun_recoil_charged.SetGlobal(false);
    v.vr_light_gun_recoil_charged.SetValue(80);
    v.vr_light_gun_recoil_rapid.SetGlobal(false);
    v.vr_light_gun_recoil_rapid.SetValue(0);
    v.vr_light_gun_weapon_size.SetGlobal(false);
    v.vr_light_gun_weapon_size.SetValue(1);
    v.vr_light_gun_weapon_scale.SetGlobal(false);
    v.vr_light_gun_weapon_scale.SetValue(130);
    MirrorAimExtras();
    const uint32_t saved = v.vr_controls_preset_done.GetValue();
    CHECK((saved & kPresetGyroAim) != 0); // the old bits keep their meaning
    CHECK(((saved & kLaserMirrorMask) >> kLaserMirrorShift) == 2);
    // new -> old (the keys dropped) -> new: the values come back from the mirror
    for (int round = 0; round < 2; round++) {
        v.vr_light_gun_recoil_charged.SetGlobal(true);
        v.vr_light_gun_recoil_rapid.SetGlobal(true);
        v.vr_light_gun_weapon_size.SetGlobal(true);
        v.vr_light_gun_weapon_scale.SetGlobal(true);
        CHECK(RestoreAimExtras());
        CHECK(v.vr_light_gun_recoil_charged.GetValue() == 80);
        CHECK(v.vr_light_gun_recoil_rapid.GetValue() == 0);
        CHECK(v.vr_light_gun_weapon_size.GetValue() == 1);
        CHECK(v.vr_light_gun_weapon_scale.GetValue() == 130);
        CHECK_FALSE(v.vr_light_gun_weapon_scale.UsingGlobal());
        MirrorAimExtras();
        CHECK(v.vr_controls_preset_done.GetValue() == saved);
    }
    // On the global values: nothing mirrored, nothing restored.
    v.vr_light_gun_recoil_charged.SetGlobal(true);
    v.vr_light_gun_recoil_rapid.SetGlobal(true);
    v.vr_light_gun_weapon_size.SetGlobal(true);
    v.vr_light_gun_weapon_scale.SetGlobal(true);
    MirrorAimExtras();
    CHECK((v.vr_controls_preset_done.GetValue() & kAimExtrasMirrorMask) == 0);
    CHECK_FALSE(RestoreAimExtras());
    v.vr_controls_preset_done.SetGlobal(true);
}

TEST_CASE("Weapon in Hand: only in a 3D battle", "[kiufix]") {
    using Kiu::InBattle;
    constexpr uint32_t lo = 0x100000, hi = 0xD32000;
    CHECK(InBattle(false, false, 0, 0, lo, hi));                 // no mod: the records decide
    CHECK(InBattle(true, true, 0x141ef5d8, 0xCF1C34, lo, hi));    // the player's reticle set
    CHECK_FALSE(InBattle(true, true, 0, 0, lo, hi));              // none: a hub, a menu
    CHECK_FALSE(InBattle(true, false, 0x141ef5d8, 0, lo, hi));    // not readable
    CHECK_FALSE(InBattle(true, true, 0x141ef5d8, 0x14000000, lo, hi)); // not an object
    CHECK_FALSE(InBattle(true, true, 0x141ef5d9, 0xCF1C34, lo, hi));   // unaligned
}

TEST_CASE("Angel's Aim: the segment's mods are the C-Stick mods of the Controls preset",
          "[kiufix]") {
    REQUIRE(std::size(Menu::kAngelsAimModIds) == std::size(kCStickModIds));
    for (std::size_t i = 0; i < std::size(kCStickModIds); i++) {
        CHECK(std::string(Menu::kAngelsAimModIds[i]) == kCStickModIds[i]);
    }
}
