// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fovaxis: Render Margin per axis (frame_xr/render_margin.h): which key decides an
// axis, the rows' writes, Use Global Value, the grip gesture, and a round trip through an older
// build that knows only Render Margin.

#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/immersive_layout.h"
#include "frame_xr/render_margin.h"

namespace M = FrameXr::Immersive::Margin;
using M::Axis;

namespace {

// The three keys as they were, put back after each test.
template <typename T, bool R>
struct Keep {
    Settings::SwitchableSetting<T, R>& s;
    bool global = s.UsingGlobal();
    T global_value = s.GetValue(true);
    T game_value = s.GetValue();
    ~Keep() {
        s.SetGlobal(true);
        s.SetValue(global_value);
        s.SetGlobal(global);
        if (!global) {
            s.SetValue(game_value);
        }
    }
};

struct Fresh {
    Keep<u32, true> old{Settings::values.vr_immersive_wider_view_level};
    Keep<s32, true> x{Settings::values.vr_immersive_render_margin_x};
    Keep<s32, true> y{Settings::values.vr_immersive_render_margin_y};
    Fresh() {
        auto& v = Settings::values;
        for (auto* s : {&v.vr_immersive_render_margin_x, &v.vr_immersive_render_margin_y}) {
            s->SetGlobal(true);
            s->SetValue(-1);
        }
        v.vr_immersive_wider_view_level.SetGlobal(true);
        v.vr_immersive_wider_view_level.SetValue(4);
    }
};

void GameOld(u32 level) { // the game's own Render Margin, as any build saves it
    auto& old = Settings::values.vr_immersive_wider_view_level;
    old.SetGlobal(false);
    old.SetValue(level);
}

} // namespace

TEST_CASE("FrameXr fovaxis: which key decides an axis", "[frame_xr][fovaxis]") {
    M::Keys k;
    // Nothing of the axes' own: Render Margin, both axes (every build before).
    REQUIRE(M::Resolve(k) == 4);
    k.old_global = 2;
    REQUIRE(M::Resolve(k) == 2);
    REQUIRE(!M::GameDecides(k));
    // The axis's own global value beats the global Render Margin.
    k.axis_global = 1;
    REQUIRE(M::Resolve(k) == 1);
    REQUIRE(M::ResolveGlobal(k) == 1);
    // Scope first: the game's own Render Margin beats the axis's global value.
    k.old_game_set = true;
    k.old_game = 3;
    REQUIRE(M::Resolve(k) == 3);
    REQUIRE(M::ResolveGlobal(k) == 1);
    REQUIRE(M::GameDecides(k));
    // The game's own axis value beats everything.
    k.axis_game_set = true;
    k.axis_game = 0;
    REQUIRE(M::Resolve(k) == 0);
    // A game's axis value of -1 follows (its own Render Margin here).
    k.axis_game = -1;
    REQUIRE(M::Resolve(k) == 3);
    // Out of range: clamped.
    k.axis_game = 9;
    REQUIRE(M::Resolve(k) == 4);
    // The settings: -1 to 4, -1 by default.
    auto& v = Settings::values;
    REQUIRE(v.vr_immersive_render_margin_x.GetDefault() == -1);
    REQUIRE(v.vr_immersive_render_margin_y.GetDefault() == -1);
    REQUIRE(v.vr_immersive_wider_view_level.GetDefault() == 4u);
}

TEST_CASE("FrameXr fovaxis: the rows and Use Global Value", "[frame_xr][fovaxis]") {
    Fresh fresh;
    auto& v = Settings::values;
    // An older build saved the game's Render Margin 1.2x: both axes show it.
    GameOld(2);
    REQUIRE(M::Level(Axis::Horizontal) == 2);
    REQUIRE(M::Level(Axis::Vertical) == 2);
    REQUIRE(M::GameHasOwn(Axis::Horizontal));
    REQUIRE(M::GlobalLevel(Axis::Horizontal) == 4);
    // The row sets Horizontal for the game: Vertical still follows the old key, which is
    // untouched (an older build reading the file shows exactly what it saved).
    M::Set(Axis::Horizontal, 1, true);
    REQUIRE(M::Level(Axis::Horizontal) == 1);
    REQUIRE(M::Level(Axis::Vertical) == 2);
    REQUIRE(v.vr_immersive_wider_view_level.GetValue() == 2u);
    REQUIRE(!v.vr_immersive_wider_view_level.UsingGlobal());
    // A global write while the game decides the axis does nothing (the menu locks that row).
    M::Set(Axis::Vertical, 0, false);
    REQUIRE(M::Level(Axis::Vertical) == 2);
    REQUIRE(v.vr_immersive_render_margin_y.GetValue(true) == -1);
    // Use Global Value on Vertical: the game's Render Margin goes; Horizontal had its own.
    REQUIRE(M::UseGlobal(Axis::Vertical));
    REQUIRE(v.vr_immersive_wider_view_level.UsingGlobal());
    REQUIRE(M::Level(Axis::Vertical) == 4);
    REQUIRE(M::Level(Axis::Horizontal) == 1);
    REQUIRE(!M::GameHasOwn(Axis::Vertical));
    // A global write now reaches the game.
    M::Set(Axis::Vertical, 3, false);
    REQUIRE(M::Level(Axis::Vertical) == 3);
    REQUIRE(v.vr_immersive_render_margin_y.GetValue(true) == 3);
    REQUIRE(M::GlobalLevel(Axis::Horizontal) == 4); // Horizontal's global still follows
    // Use Global Value on Horizontal: its own value goes.
    REQUIRE(M::UseGlobal(Axis::Horizontal));
    REQUIRE(M::Level(Axis::Horizontal) == 4);
    REQUIRE(!M::UseGlobal(Axis::Horizontal)); // nothing of its own any more
}

TEST_CASE("FrameXr fovaxis: Use Global Value keeps the other axis", "[frame_xr][fovaxis]") {
    Fresh fresh;
    auto& v = Settings::values;
    GameOld(1); // both axes 1.1x from the game's Render Margin
    REQUIRE(M::UseGlobal(Axis::Horizontal));
    // Horizontal follows the global 1.4x; Vertical keeps the 1.1x it showed, as its own.
    REQUIRE(M::Level(Axis::Horizontal) == 4);
    REQUIRE(M::Level(Axis::Vertical) == 1);
    REQUIRE(!v.vr_immersive_render_margin_y.UsingGlobal());
    REQUIRE(v.vr_immersive_render_margin_y.GetValue() == 1);
    REQUIRE(v.vr_immersive_wider_view_level.UsingGlobal());
}

TEST_CASE("FrameXr fovaxis: the grip gesture steps both axes", "[frame_xr][fovaxis]") {
    Fresh fresh;
    auto& v = Settings::values;
    // Both follow Render Margin: the old key steps, per game, exactly as before; the axes'
    // keys are untouched.
    M::Stepped s = M::StepBoth(-1);
    REQUIRE(s.changed);
    REQUIRE(s.x_before == 4);
    REQUIRE(s.x == 3);
    REQUIRE(s.y == 3);
    REQUIRE(!v.vr_immersive_wider_view_level.UsingGlobal());
    REQUIRE(v.vr_immersive_wider_view_level.GetValue() == 3u);
    REQUIRE(v.vr_immersive_wider_view_level.GetValue(true) == 4u);
    REQUIRE(v.vr_immersive_render_margin_x.UsingGlobal());
    REQUIRE(v.vr_immersive_render_margin_y.UsingGlobal());
    // At the top: nothing changes.
    s = M::StepBoth(1);
    REQUIRE(s.x == 4);
    s = M::StepBoth(1);
    REQUIRE(!s.changed);
    REQUIRE(s.x == 4);
    // Horizontal of its own (1.1x), Vertical following (1.4x): each steps.
    M::Set(Axis::Horizontal, 1, true);
    s = M::StepBoth(-1);
    REQUIRE(s.changed);
    REQUIRE(s.x == 0);
    REQUIRE(s.y == 3);
    REQUIRE(M::Level(Axis::Horizontal) == 0);
    REQUIRE(M::Level(Axis::Vertical) == 3);
    // At Off across, down again moves only Vertical.
    s = M::StepBoth(-1);
    REQUIRE(s.changed);
    REQUIRE(s.x == 0);
    REQUIRE(s.y == 2);
    // The toast says both.
    REQUIRE(FrameXr::Immersive::Layout::MarginToast(s.x, s.y) == "Render Margin: H Off, V 1.2x");
    s = M::StepBoth(0);
    REQUIRE(!s.changed);
}

// Rollback: an older build reads and writes only Render Margin. Whatever it does to that key,
// an axis that follows it shows the change here; an axis this build gave its own value keeps it.
TEST_CASE("FrameXr fovaxis: a round trip through an older build", "[frame_xr][fovaxis]") {
    Fresh fresh;
    auto& v = Settings::values;
    // This build: the game's own Horizontal 1.2x; Vertical untouched.
    M::Set(Axis::Horizontal, 2, true);
    // The older build reads the file: its Render Margin is the old key (the global 1.4x here),
    // the per-axis keys mean nothing to it. It steps the game's Render Margin to 1.1x and saves.
    REQUIRE(v.vr_immersive_wider_view_level.GetValue() == 4u);
    GameOld(1);
    // This build reads it again: Vertical follows the older build's 1.1x, Horizontal keeps 1.2x.
    REQUIRE(M::Level(Axis::Horizontal) == 2);
    REQUIRE(M::Level(Axis::Vertical) == 1);
    // A file no build of this lane touched: both follow the old key, as every build before.
    v.vr_immersive_render_margin_x.SetGlobal(true);
    REQUIRE(M::Level(Axis::Horizontal) == 1);
    REQUIRE(M::Level(Axis::Vertical) == 1);
}
