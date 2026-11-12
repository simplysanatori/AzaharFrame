// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame useglobal: the in-game menu's per-game rows against the settings
// (frame_xr/per_game.h): which ones show the "This Game" chip, and Use Global Value.

#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/per_game.h"

using namespace FrameXr;
using Menu::Id;

namespace {

/// Puts a setting back as it was when the test ends.
template <typename T, bool R>
struct Keep {
    explicit Keep(Settings::SwitchableSetting<T, R>& s)
        : setting{s}, global{s.UsingGlobal()}, global_value{s.GetValue(true)} {}
    ~Keep() {
        setting.SetGlobal(true);
        setting.SetValue(global_value);
        setting.SetGlobal(global);
    }
    Settings::SwitchableSetting<T, R>& setting;
    bool global;
    T global_value;
};

/// The game's own value, as Per-Game sets it (MenuPanel's Apply with per_game).
template <typename T, bool R>
void SetGameValue(Settings::SwitchableSetting<T, R>& s, T value) {
    s.SetGlobal(false);
    s.SetValue(value);
}

/// A change in Global (the global value; a game that follows it sees it).
template <typename T, bool R>
void SetGlobalValue(Settings::SwitchableSetting<T, R>& s, T value) {
    const bool global = s.UsingGlobal();
    s.SetGlobal(true);
    s.SetValue(value);
    s.SetGlobal(global);
}

bool Marked(Id id) {
    Menu::MenuModel m;
    PerGame::FillGameValues(m);
    return m.game_value[static_cast<std::size_t>(id)];
}

} // namespace

TEST_CASE("Use Global Value: the marker only while the game has its own value",
          "[frame_xr][useglobal]") {
    auto& v = Settings::values;
    Keep k1{v.resolution_factor};
    Keep k2{v.factor_3d};
    Keep k3{v.async_shader_compilation};
    v.resolution_factor.SetGlobal(true);
    v.factor_3d.SetGlobal(true);
    v.async_shader_compilation.SetGlobal(true);
    Menu::MenuModel m;
    PerGame::FillGameValues(m);
    for (int i = 0; i < Menu::kIdCount; i++) {
        CHECK(!m.game_value[static_cast<std::size_t>(i)]);
    }
    SetGameValue(v.resolution_factor, 4u);
    CHECK(Marked(Id::InternalResolution));
    CHECK(!Marked(Id::Depth3d));
    CHECK(!PerGame::UsesGlobal(Id::InternalResolution));
    // Global-only rows are never marked
    for (Id id : {Id::Background, Id::WindowScale, Id::FaceButtons, Id::MotionControls,
                  Id::ShowControllers, Id::FastForwardSpeed, Id::DynamicResolution}) {
        CHECK(!Marked(id));
        CHECK(PerGame::UsesGlobal(id));
        CHECK(!PerGame::UseGlobalValue(id));
    }
}

TEST_CASE("Use Global Value: the game's value goes and the game follows the global value",
          "[frame_xr][useglobal]") {
    auto& v = Settings::values;
    SECTION("a menu (Internal Resolution)") {
        Keep keep{v.resolution_factor};
        SetGlobalValue(v.resolution_factor, 2u);
        SetGameValue(v.resolution_factor, 5u);
        CHECK(v.resolution_factor.GetValue() == 5u);
        CHECK(PerGame::UseGlobalValue(Id::InternalResolution));
        CHECK(!Marked(Id::InternalResolution));
        CHECK(v.resolution_factor.GetValue() == 2u);
        CHECK(!PerGame::UseGlobalValue(Id::InternalResolution)); // nothing left to clear
        SetGlobalValue(v.resolution_factor, 3u);
        CHECK(v.resolution_factor.GetValue() == 3u);
    }
    SECTION("sliders (3D Depth, Immersive Game FOV, Every, Keep)") {
        Keep k1{v.factor_3d};
        Keep k2{v.vr_immersive_game_fov};
        Keep k3{v.autosave_every};
        Keep k4{v.autosave_keep};
        SetGlobalValue(v.factor_3d, 40u);
        SetGameValue(v.factor_3d, 90u);
        SetGlobalValue(v.vr_immersive_game_fov, s32{60});
        SetGameValue(v.vr_immersive_game_fov, s32{95});
        SetGlobalValue(v.autosave_every, 5u);
        SetGameValue(v.autosave_every, 1u);
        SetGlobalValue(v.autosave_keep, 3u);
        SetGameValue(v.autosave_keep, 8u);
        for (Id id : {Id::Depth3d, Id::GameFov, Id::AutosaveEvery, Id::AutosaveKeep}) {
            CHECK(Marked(id));
            CHECK(PerGame::UseGlobalValue(id));
            CHECK(!Marked(id));
        }
        CHECK(v.factor_3d.GetValue() == 40u);
        CHECK(v.vr_immersive_game_fov.GetValue() == 60);
        CHECK(v.autosave_every.GetValue() == 5u);
        CHECK(v.autosave_keep.GetValue() == 3u);
        SetGlobalValue(v.factor_3d, 70u);
        SetGlobalValue(v.vr_immersive_game_fov, s32{80});
        CHECK(v.factor_3d.GetValue() == 70u);
        CHECK(v.vr_immersive_game_fov.GetValue() == 80);
    }
    SECTION("switches (Async Shader Compilation, Match Game Camera, Autosave)") {
        Keep k1{v.async_shader_compilation};
        Keep k2{v.vr_immersive_match_camera};
        Keep k3{v.autosave_enabled};
        SetGlobalValue(v.async_shader_compilation, false);
        SetGameValue(v.async_shader_compilation, true);
        SetGlobalValue(v.vr_immersive_match_camera, true);
        SetGameValue(v.vr_immersive_match_camera, false);
        SetGlobalValue(v.autosave_enabled, false);
        SetGameValue(v.autosave_enabled, true);
        for (Id id : {Id::AsyncShaders, Id::MatchCamera, Id::Autosave}) {
            CHECK(Marked(id));
            CHECK(PerGame::UseGlobalValue(id));
            CHECK(!Marked(id));
        }
        CHECK(!v.async_shader_compilation.GetValue());
        CHECK(v.vr_immersive_match_camera.GetValue());
        CHECK(!v.autosave_enabled.GetValue());
        SetGlobalValue(v.autosave_enabled, true);
        CHECK(v.autosave_enabled.GetValue());
    }
    SECTION("only that row") {
        Keep k1{v.texture_filter};
        Keep k2{v.vr_immersive_squeeze};
        SetGameValue(v.texture_filter, Settings::TextureFilter::Bicubic);
        SetGameValue(v.vr_immersive_squeeze, 20u);
        CHECK(PerGame::UseGlobalValue(Id::TextureFilter));
        CHECK(!Marked(Id::TextureFilter));
        CHECK(Marked(Id::NarrowScreen));
        CHECK(v.vr_immersive_squeeze.GetValue() == 20u);
    }
}
