// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame useglobal: the in-game menu's per-game rows against the settings
// (frame_xr/per_game.h): which ones count as set for this game (the scope control shows it), and Use Global Value.

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
    // (AzaharFrame kiu: Motion Controls is per game now.)
    for (Id id : {Id::Background, Id::WindowScale, Id::FaceButtons, Id::ShowControllers,
                  Id::FastForwardSpeed, Id::DynamicResolution}) {
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
    SECTION("switches (Async Shader Compilation, Match Game Camera, Autosave) and Accurate Image "
            "Copies") {
        Keep k1{v.async_shader_compilation};
        Keep k2{v.vr_immersive_match_camera_mode}; // AzaharFrame fovonce
        Keep k3{v.autosave_enabled};
        Keep k4{v.image_copy_accuracy}; // AzaharFrame accxfer, xfergpu
        SetGlobalValue(v.async_shader_compilation, false);
        SetGameValue(v.async_shader_compilation, true);
        SetGlobalValue(v.vr_immersive_match_camera_mode, 2u);
        SetGameValue(v.vr_immersive_match_camera_mode, 1u);
        SetGlobalValue(v.autosave_enabled, false);
        SetGameValue(v.autosave_enabled, true);
        using Settings::ImageCopyAccuracy;
        SetGlobalValue(v.image_copy_accuracy, ImageCopyAccuracy::Automatic);
        SetGameValue(v.image_copy_accuracy, ImageCopyAccuracy::On);
        CHECK(v.image_copy_accuracy.GetValue() == ImageCopyAccuracy::On);
        // the game's choice is not the global one
        CHECK(v.image_copy_accuracy.GetValue(true) == ImageCopyAccuracy::Automatic);
        for (Id id : {Id::AsyncShaders, Id::MatchCamera, Id::Autosave, Id::AccurateImageCopies}) {
            CHECK(Marked(id));
            CHECK(PerGame::UseGlobalValue(id));
            CHECK(!Marked(id));
        }
        CHECK(!v.async_shader_compilation.GetValue());
        CHECK(v.vr_immersive_match_camera_mode.GetValue() == 2u);
        CHECK(!v.autosave_enabled.GetValue());
        CHECK(v.image_copy_accuracy.GetValue() == ImageCopyAccuracy::Automatic);
        SetGlobalValue(v.autosave_enabled, true);
        CHECK(v.autosave_enabled.GetValue());
    }
    SECTION("AzaharFrame kiu: Motion Controls and C-Stick at Edge, per game now") {
        Keep k1{v.vr_motion_controls};
        Keep k2{v.vr_reticle_edge_cstick};
        SetGlobalValue(v.vr_motion_controls, 1u);
        SetGameValue(v.vr_motion_controls, 0u);
        SetGlobalValue(v.vr_reticle_edge_cstick, false);
        SetGameValue(v.vr_reticle_edge_cstick, true);
        CHECK(v.vr_motion_controls.GetValue() == 0u);
        CHECK(v.vr_motion_controls.GetValue(true) == 1u);
        for (Id id : {Id::MotionControls, Id::ReticleEdgeCStick}) {
            CHECK(Marked(id));
            CHECK(PerGame::UseGlobalValue(id));
            CHECK(!Marked(id));
        }
        CHECK(v.vr_motion_controls.GetValue() == 1u);
        CHECK(!v.vr_reticle_edge_cstick.GetValue());
    }
    SECTION("Fast Forward Audio (an enum)") {
        Keep keep{v.fast_forward_audio};
        SetGlobalValue(v.fast_forward_audio, Settings::FastForwardAudio::NormalPitch);
        SetGameValue(v.fast_forward_audio, Settings::FastForwardAudio::Muted);
        CHECK(Marked(Id::FastForwardAudio));
        CHECK(v.fast_forward_audio.GetValue() == Settings::FastForwardAudio::Muted);
        CHECK(PerGame::UseGlobalValue(Id::FastForwardAudio));
        CHECK(!Marked(Id::FastForwardAudio));
        CHECK(v.fast_forward_audio.GetValue() == Settings::FastForwardAudio::NormalPitch);
    }
    SECTION("Scale Mode (AzaharFrame fovscale): Field of View by default, Stretch per game") {
        Keep keep{v.vr_immersive_scale_mode};
        CHECK(v.vr_immersive_scale_mode.GetDefault() == 1u); // Field of View
        SetGlobalValue(v.vr_immersive_scale_mode, 1u);
        CHECK(!Marked(Id::ScaleMode));
        SetGameValue(v.vr_immersive_scale_mode, 0u); // a game switched back to Stretch
        CHECK(Marked(Id::ScaleMode));
        CHECK(v.vr_immersive_scale_mode.GetValue() == 0u);
        CHECK(v.vr_immersive_scale_mode.GetValue(true) == 1u);
        CHECK(PerGame::UseGlobalValue(Id::ScaleMode));
        CHECK(!Marked(Id::ScaleMode));
        CHECK(v.vr_immersive_scale_mode.GetValue() == 1u);
    }
    SECTION("only that row") {
        Keep k1{v.texture_filter};
        Keep k2{v.vr_immersive_scale_x};
        SetGameValue(v.texture_filter, Settings::TextureFilter::Bicubic);
        SetGameValue(v.vr_immersive_scale_x, -20);
        CHECK(PerGame::UseGlobalValue(Id::TextureFilter));
        CHECK(!Marked(Id::TextureFilter));
        CHECK(Marked(Id::ScaleX));
        CHECK(v.vr_immersive_scale_x.GetValue() == -20);
    }
}
