// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuopacity: Weapon Opacity and Projectile Opacity (frame_xr/vis/see_through.h):
// the stages the controllers' pass draws a see-through weapon or projectile in, what they give
// once composited, the glows' and the flash's scaling, the live tune and the settings.

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/per_game.h"
#include "frame_xr/vis/muzzle_flash.h"
#include "frame_xr/vis/see_through.h"

using Catch::Approx;
namespace Vis = FrameXr::Vis;
using Vis::DrawStage;
using Vis::Pipe;

namespace {

/// One pixel of the controllers' image (premultiplied: colour x alpha, alpha) with its depth,
/// and the four pipelines as controller_models.cpp makes them.
struct Pixel {
    std::array<float, 3> rgb{};
    float a = 0.0f;
    float depth = 1.0f; // cleared to the far plane
};

/// A part's fragment at this pixel: its lit colour (the shader's writeLinear at fade 1) or, for
/// an added part, its light, and its depth.
struct Fragment {
    std::array<float, 3> colour{};
    float depth = 0.5f;
    bool added = false;
};

void Draw(Pixel& p, Pipe pipe, const Fragment& f, float shader_fade) {
    if (!(f.depth <= p.depth)) { // every pipeline tests LEQUAL
        return;
    }
    const float z = std::clamp(shader_fade, 0.0f, 1.0f);
    switch (pipe) {
    case Pipe::Opaque: // blend off, depth written: writeLinear's (c x z, z)
        for (int i = 0; i < 3; i++) {
            p.rgb[i] = f.colour[i] * z;
        }
        p.a = z;
        p.depth = f.depth;
        break;
    case Pipe::DepthOnly: // nothing but depth
        p.depth = f.depth;
        break;
    case Pipe::Blended: { // (c x z, z) over what is there; no depth written
        for (int i = 0; i < 3; i++) {
            p.rgb[i] = f.colour[i] * z + p.rgb[i] * (1.0f - z);
        }
        p.a = z + p.a * (1.0f - z);
        break;
    }
    case Pipe::Added: { // light x fade added, its coverage added; no depth written
        std::array<float, 3> light{};
        for (int i = 0; i < 3; i++) {
            light[i] = f.colour[i] * z;
            p.rgb[i] += light[i];
        }
        p.a += Vis::CoverAlpha(light[0], light[1], light[2]);
        break;
    }
    case Pipe::AddedHeld: { // light added as Added's; coverage held: MAX, not summed
        std::array<float, 3> light{};
        for (int i = 0; i < 3; i++) {
            light[i] = f.colour[i] * z;
            p.rgb[i] += light[i];
        }
        p.a = std::max(p.a, Vis::CoverAlpha(light[0], light[1], light[2]));
        break;
    }
    }
}

/// The parts drawn as RecordWeapon draws them: per stage, each part that stage draws.
Pixel DrawWeapon(const std::vector<Fragment>& parts, float fade, float opacity,
                 std::vector<Pipe>* pipes = nullptr) {
    Pixel p;
    for (const DrawStage stage : Vis::StagesFor(opacity)) {
        for (const Fragment& f : parts) {
            if (!Vis::DrawnIn(stage, f.added)) {
                continue;
            }
            const Pipe pipe = Vis::PipeFor(stage, f.added);
            if (pipes) {
                pipes->push_back(pipe);
            }
            Draw(p, pipe, f, Vis::StageFade(stage, fade, opacity));
        }
    }
    return p;
}

/// What the compositor shows: the premultiplied pixel over `behind`.
std::array<float, 3> Over(const Pixel& p, const std::array<float, 3>& behind) {
    std::array<float, 3> out{};
    for (int i = 0; i < 3; i++) {
        out[i] = p.rgb[i] + behind[i] * (1.0f - std::min(p.a, 1.0f));
    }
    return out;
}

} // namespace

TEST_CASE("See-through: opacity 100 % takes the old path", "[kiuopacity]") {
    // The setting's default and the tune's absence: 1, not see-through.
    CHECK(Vis::OpacityOf(100, -1) == 1.0f);
    CHECK_FALSE(Vis::SeeThrough(Vis::OpacityOf(100, -1)));
    const Vis::DrawStages one = Vis::StagesFor(1.0f);
    REQUIRE(one.count == 1);
    CHECK(one.list[0] == DrawStage::Whole);
    // Whole draws every part in the pipeline it always had, with the fade and flags unchanged.
    CHECK(Vis::DrawnIn(DrawStage::Whole, false));
    CHECK(Vis::DrawnIn(DrawStage::Whole, true));
    CHECK(Vis::PipeFor(DrawStage::Whole, false) == Pipe::Opaque);
    CHECK(Vis::PipeFor(DrawStage::Whole, true) == Pipe::Added);
    CHECK(Vis::FlashPipe(1.0f) == Pipe::Added);
    std::mt19937 rng(62);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    for (int i = 0; i < 1000; i++) {
        const float fade = u(rng), flags = std::floor(u(rng) * 32.0f);
        CHECK(Vis::StageFade(DrawStage::Whole, fade, 1.0f) == fade); // bit for bit
        CHECK(Vis::StageFlags(DrawStage::Whole, flags) == flags);
        // The flash and the projectiles are handed fade x 1: the same float.
        CHECK(fade * 1.0f == fade);
    }
    // The hands keep their order.
    CHECK(Vis::HandOrder(false, false) == std::array<int, 2>{0, 1});
    // And the image is what the opaque pass always gave: the nearest surface, solid.
    const std::vector<Fragment> parts{{{0.2f, 0.4f, 0.6f}, 0.3f, false},
                                      {{0.9f, 0.1f, 0.1f}, 0.6f, false}};
    std::vector<Pipe> pipes;
    const Pixel p = DrawWeapon(parts, 1.0f, 1.0f, &pipes);
    CHECK(pipes == std::vector<Pipe>{Pipe::Opaque, Pipe::Opaque});
    CHECK(p.a == 1.0f);
    CHECK(p.rgb[0] == Approx(0.2f));
}

TEST_CASE("See-through: under 100 % a depth stage first, the nearest surface only",
          "[kiuopacity]") {
    for (const int percent : {20, 30, 50, 60, 90}) {
        const float o = Vis::OpacityFactor(percent);
        INFO(percent);
        REQUIRE(Vis::SeeThrough(o));
        const Vis::DrawStages s = Vis::StagesFor(o);
        REQUIRE(s.count == 3);
        CHECK(s.list[0] == DrawStage::Depth);
        CHECK(s.list[1] == DrawStage::Colour);
        CHECK(s.list[2] == DrawStage::Added);
        // The lit parts twice (depth, then colour), the glows once, last.
        CHECK(Vis::DrawnIn(DrawStage::Depth, false));
        CHECK_FALSE(Vis::DrawnIn(DrawStage::Depth, true));
        CHECK(Vis::DrawnIn(DrawStage::Colour, false));
        CHECK_FALSE(Vis::DrawnIn(DrawStage::Colour, true));
        CHECK_FALSE(Vis::DrawnIn(DrawStage::Added, false));
        CHECK(Vis::DrawnIn(DrawStage::Added, true));
        CHECK(Vis::PipeFor(DrawStage::Depth, false) == Pipe::DepthOnly);
        CHECK(Vis::PipeFor(DrawStage::Colour, false) == Pipe::Blended);
        CHECK(Vis::PipeFor(DrawStage::Added, true) == Pipe::AddedHeld);
        CHECK(Vis::FlashPipe(o) == Pipe::AddedHeld);
        CHECK(Vis::StageFade(DrawStage::Colour, 0.8f, o) == Approx(0.8f * o));
        CHECK(Vis::StageFade(DrawStage::Added, 1.0f, o) == Approx(o));
        // The depth flag (+32) leaves the other bits as the shader tests them: mirrored (+1),
        // a forearm (+2), never a glow, moving glow or flash card.
        for (const float flags : {0.0f, 1.0f, 2.0f, 3.0f}) {
            const float w = Vis::StageFlags(DrawStage::Depth, flags);
            const int bits = static_cast<int>(w + 0.5f);
            CHECK((bits & 32) != 0);
            CHECK(std::fmod(w, 4.0f) == flags);
            CHECK((bits & (4 | 8 | 16)) == 0);
            CHECK(Vis::StageFlags(DrawStage::Colour, flags) == flags);
        }
    }
    // A faded weapon's hand is drawn after the other one.
    CHECK(Vis::HandOrder(true, false) == std::array<int, 2>{1, 0});
    CHECK(Vis::HandOrder(false, true) == std::array<int, 2>{0, 1});
    CHECK(Vis::HandOrder(true, true) == std::array<int, 2>{0, 1});

    // Composited: the front surface alone at the opacity, whatever the order the parts come in
    // (a back face first or last), and the screen behind shows by 1 - opacity.
    const std::array<float, 3> front{0.2f, 0.5f, 0.8f}, back{1.0f, 0.0f, 0.0f},
        inner{0.0f, 1.0f, 0.0f}, screen{0.3f, 0.3f, 0.9f};
    std::mt19937 rng(7);
    for (const float o : {0.2f, 0.3f, 0.6f, 0.9f}) {
        std::vector<Fragment> parts{{front, 0.40f, false}, {inner, 0.45f, false},
                                    {back, 0.50f, false}};
        for (int k = 0; k < 6; k++) {
            std::shuffle(parts.begin(), parts.end(), rng);
            std::vector<Pipe> pipes;
            const Pixel p = DrawWeapon(parts, 1.0f, o, &pipes);
            // Three depth draws, then three colour draws: one extra pass.
            CHECK(pipes == std::vector<Pipe>{Pipe::DepthOnly, Pipe::DepthOnly, Pipe::DepthOnly,
                                             Pipe::Blended, Pipe::Blended, Pipe::Blended});
            CHECK(p.a == Approx(o));
            const auto seen = Over(p, screen);
            for (int i = 0; i < 3; i++) {
                CHECK(seen[static_cast<std::size_t>(i)] ==
                      Approx(front[static_cast<std::size_t>(i)] * o +
                             screen[static_cast<std::size_t>(i)] * (1.0f - o))
                          .margin(1e-5));
            }
        }
    }
    // A hand fading out (fade 0.5) at 60 %: the two multiply.
    const Pixel half = DrawWeapon({{front, 0.4f, false}}, 0.5f, 0.6f);
    CHECK(half.a == Approx(0.3f));
}

TEST_CASE("See-through: the glows, flames and flash dim with the weapon", "[kiuopacity]") {
    std::mt19937 rng(49);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    for (int i = 0; i < 2000; i++) {
        const std::array<float, 3> tex{u(rng), u(rng), u(rng)};
        const float gain = i % 2 ? -1.0f : 0.8f + 0.4f * u(rng); // still, or flickering
        const float fade = 0.5f + 0.5f * u(rng);
        const float o = Vis::OpacityFactor(20 + (i % 9) * 10);
        const Vis::GlowOut full = Vis::GlowLight(tex, gain, fade);
        const Vis::GlowOut dim = Vis::GlowLight(tex, gain, fade * o);
        // At 100 % the light is the old order's (fade before the flicker's hold at 1) wherever
        // that hold did not bite, and the same at fade 1.
        const Vis::GlowOut one = Vis::GlowLight(tex, gain, 1.0f);
        for (std::size_t c = 0; c < 3; c++) {
            float old = std::min(tex[c] * 2.0f, 1.0f);
            if (gain >= 0.0f) {
                old = std::min(old * gain, 1.0f);
            }
            CHECK(one.light[c] == Approx(old));
            // Light and coverage both times the opacity.
            CHECK(dim.light[c] == Approx(full.light[c] * o).margin(1e-6));
        }
        // The coverage too, down to its floor (lit pixels are never at alpha 0).
        const bool any = std::max(dim.light[0], std::max(dim.light[1], dim.light[2])) > 0.0f;
        CHECK(dim.cover ==
              Approx(any ? std::max(full.cover * o, Vis::kCoverFloor) : 0.0f).margin(1e-6));
        CHECK(dim.cover <= 0.6f);
        // Lit means covered: SteamVR drops light written at alpha 0.
        const float lit = std::max(dim.light[0], std::max(dim.light[1], dim.light[2]));
        if (lit > 0.0f) {
            CHECK(dim.cover > 0.0f);
        }
    }
    // The flash: its strength (FlashIntensity x fade) times the opacity, so its light and its
    // coverage (taken from that light) scale the same.
    for (const float age : {0.0f, 0.1f, 0.5f, 0.9f}) {
        const float s = Vis::FlashIntensity(age);
        const float light = 0.7f * s, dimmed = 0.7f * s * 0.3f;
        CHECK(Vis::CoverAlpha(dimmed, 0.0f, 0.0f) ==
              Approx(std::max(Vis::CoverAlpha(light, 0.0f, 0.0f) * 0.3f,
                              light > 0.0f ? Vis::kCoverFloor : 0.0f))
                  .margin(1e-6));
    }
    // Composited: a glow over a faded surface adds its dimmed light on top.
    const Pixel p = DrawWeapon({{{0.2f, 0.2f, 0.2f}, 0.4f, false}, {{0.5f, 0.3f, 0.0f}, 0.38f, true}},
                               1.0f, 0.5f);
    CHECK(p.rgb[0] == Approx(0.2f * 0.5f + 0.5f * 0.5f));
    CHECK(p.a == Approx(0.5f)); // the glow's coverage (0.15) held under the surface's
    // Overlapping glow cards (a glowing weapon's): at 100 % their coverage sums, as it always
    // did; see-through it never goes past the opacity, so the screen still shows through.
    {
        std::vector<Fragment> parts{{{0.2f, 0.2f, 0.2f}, 0.4f, false}};
        for (int k = 0; k < 8; k++) {
            parts.push_back({{1.0f, 1.0f, 1.0f}, 0.30f + 0.01f * static_cast<float>(k), true});
        }
        const std::vector<Fragment> cards(parts.begin() + 1, parts.end());
        CHECK(DrawWeapon(cards, 1.0f, 1.0f).a >= 1.0f);
        for (const float o : {0.2f, 0.3f, 0.6f, 0.9f}) {
            const Pixel held = DrawWeapon(parts, 1.0f, o);
            CHECK(held.a == Approx(o));
            // Beside the body (glow cards alone): one card's coverage.
            CHECK(DrawWeapon(cards, 1.0f, o).a == Approx(Vis::CoverAlpha(o, o, o)));
        }
    }
    // A glow behind the surface stays hidden, as with the solid weapon.
    const Pixel hidden =
        DrawWeapon({{{0.2f, 0.2f, 0.2f}, 0.4f, false}, {{0.5f, 0.3f, 0.0f}, 0.45f, true}}, 1.0f,
                   0.5f);
    CHECK(hidden.rgb[0] == Approx(0.1f));
}

TEST_CASE("See-through: the live tune", "[kiuopacity]") {
    auto t = Vis::ParseOpacityTune(std::string{"60,30"});
    CHECK(t.weapon == 60);
    CHECK(t.projectile == 30);
    t = Vis::ParseOpacityTune(std::string{"45"});
    CHECK(t.weapon == 45);
    CHECK(t.projectile == -1);
    t = Vis::ParseOpacityTune(std::string{",25"});
    CHECK(t.weapon == -1);
    CHECK(t.projectile == 25);
    t = Vis::ParseOpacityTune(std::string{"150,x"});
    CHECK(t.weapon == -1);
    CHECK(t.projectile == -1);
    CHECK(Vis::ParseOpacityTune(std::nullopt).weapon == -1);
    // The tune wins over the setting; both held at 20 to 100 %.
    CHECK(Vis::OpacityOf(80, 60) == Approx(0.6f));
    CHECK(Vis::OpacityOf(80, -1) == Approx(0.8f));
    CHECK(Vis::OpacityOf(80, 5) == Approx(0.2f));
    CHECK(Vis::OpacityOf(0, -1) == Approx(0.2f));
    CHECK(Vis::OpacityOf(400, -1) == 1.0f);
}

TEST_CASE("See-through: the settings, their defaults and a game's own values",
          "[kiuopacity]") {
    using FrameXr::Menu::Id;
    auto& v = Settings::values;
    for (auto* s : {&v.vr_light_gun_weapon_opacity, &v.vr_light_gun_projectile_opacity}) {
        CHECK(s->GetValue() == 100);
        CHECK(s->GetDefault() == 100);
        CHECK(s->UsingGlobal());
    }
    CHECK(v.vr_light_gun_weapon_opacity.GetLabel() == "vr_light_gun_weapon_opacity");
    CHECK(v.vr_light_gun_projectile_opacity.GetLabel() == "vr_light_gun_projectile_opacity");
    // Held at 20 to 100.
    v.vr_light_gun_weapon_opacity.SetValue(5);
    CHECK(v.vr_light_gun_weapon_opacity.GetValue() == 20);
    v.vr_light_gun_weapon_opacity.SetValue(250);
    CHECK(v.vr_light_gun_weapon_opacity.GetValue() == 100);
    // Per game, each its own: set for the game, the global value stays; Use Global Value goes
    // back to it; and the game's values are what the scope control shows.
    v.vr_light_gun_weapon_opacity.SetValue(90); // the global value
    v.vr_light_gun_weapon_opacity.SetGlobal(false);
    v.vr_light_gun_weapon_opacity.SetValue(40);
    v.vr_light_gun_projectile_opacity.SetGlobal(false);
    v.vr_light_gun_projectile_opacity.SetValue(70);
    CHECK(v.vr_light_gun_weapon_opacity.GetValue() == 40);
    CHECK(v.vr_light_gun_projectile_opacity.GetValue() == 70);
    CHECK_FALSE(FrameXr::PerGame::UsesGlobal(Id::WeaponOpacity));
    CHECK_FALSE(FrameXr::PerGame::UsesGlobal(Id::ProjectileOpacity));
    FrameXr::Menu::MenuModel m;
    FrameXr::PerGame::FillGameValues(m);
    CHECK(m.game_value[static_cast<std::size_t>(Id::WeaponOpacity)]);
    CHECK(m.game_value[static_cast<std::size_t>(Id::ProjectileOpacity)]);
    CHECK(FrameXr::PerGame::UseGlobalValue(Id::WeaponOpacity));
    CHECK(v.vr_light_gun_weapon_opacity.UsingGlobal());
    CHECK(v.vr_light_gun_weapon_opacity.GetValue() == 90);
    CHECK(v.vr_light_gun_projectile_opacity.GetValue() == 70); // the other untouched
    CHECK(FrameXr::PerGame::UseGlobalValue(Id::ProjectileOpacity));
    CHECK(v.vr_light_gun_projectile_opacity.GetValue() == 100);
    v.vr_light_gun_weapon_opacity.SetValue(100);
    CHECK(v.vr_light_gun_weapon_opacity.GetValue() == 100);
    // (Both are per-game rows of the Controls tab: the picker tests' "Weapon Opacity and
    // Projectile Opacity".)
}

// AzaharFrame kiuweap2: Muzzle Flash Size (per game, as the opacities) and the prompt's plain key.
TEST_CASE("Muzzle Flash Size and the prompt's key: defaults, limits, a game's own value",
          "[kiuweap2]") {
    using FrameXr::Menu::Id;
    auto& v = Settings::values;
    auto& s = v.vr_light_gun_muzzle_flash_size;
    CHECK(s.GetValue() == 100);
    CHECK(s.GetDefault() == 100);
    CHECK(s.UsingGlobal());
    CHECK(s.GetLabel() == "vr_light_gun_muzzle_flash_size");
    s.SetValue(10);
    CHECK(s.GetValue() == 50);
    s.SetValue(1000);
    CHECK(s.GetValue() == 300);
    s.SetValue(120);
    s.SetGlobal(false);
    s.SetValue(250);
    CHECK(s.GetValue() == 250);
    CHECK(Vis::FlashSizeFactor(static_cast<int>(s.GetValue())) == Approx(2.5f));
    CHECK_FALSE(FrameXr::PerGame::UsesGlobal(Id::MuzzleFlashSize));
    FrameXr::Menu::MenuModel m;
    FrameXr::PerGame::FillGameValues(m);
    CHECK(m.game_value[static_cast<std::size_t>(Id::MuzzleFlashSize)]);
    CHECK(FrameXr::PerGame::UseGlobalValue(Id::MuzzleFlashSize));
    CHECK(s.UsingGlobal());
    CHECK(s.GetValue() == 120);
    s.SetValue(100);
    // The prompt's key: a new plain one, off until shown (no older key changes its meaning).
    CHECK_FALSE(v.vr_angels_aim_intro_shown.GetValue());
    CHECK_FALSE(v.vr_angels_aim_intro_shown.GetDefault());
    CHECK(v.vr_angels_aim_intro_shown.GetLabel() == "vr_angels_aim_intro_shown");
}
