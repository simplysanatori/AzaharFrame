// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame useglobal: see per_game.h.

#include "common/settings.h"
#include "frame_xr/per_game.h"

namespace FrameXr::PerGame {

namespace {

/// Calls `f` with the row's setting; false for a row that is not per game (the rows of
/// Menu::PerGameSetting).
template <typename F>
bool WithSetting(Menu::Id id, F&& f) {
    using Menu::Id;
    auto& v = Settings::values;
    switch (id) {
    case Id::HideBottomScreen:
        f(v.vr_hide_bottom_screen);
        return true;
    case Id::ScreenShape:
        f(v.vr_top_aspect);
        return true;
    case Id::ImmersiveMode:
        f(v.vr_immersive_mode);
        return true;
    case Id::WiderView:
        f(v.vr_immersive_wider_view_level);
        return true;
    case Id::GameFov:
        f(v.vr_immersive_game_fov);
        return true;
    case Id::MatchCamera:
        f(v.vr_immersive_match_camera);
        return true;
    case Id::NarrowScreen:
        f(v.vr_immersive_squeeze);
        return true;
    case Id::DepthFactor:
        f(v.vr_immersive_positional_game_scaler);
        return true;
    case Id::Depth3d:
        f(v.factor_3d);
        return true;
    case Id::InternalResolution:
        f(v.resolution_factor);
        return true;
    case Id::TextureFilter:
        f(v.texture_filter);
        return true;
    case Id::AsyncShaders:
        f(v.async_shader_compilation);
        return true;
    case Id::AccurateImageCopies: // AzaharFrame accxfer
        f(v.image_copy_accuracy);
        return true;
    case Id::FastForwardAudio: // AzaharFrame ffaudio
        f(v.fast_forward_audio);
        return true;
    case Id::Autosave:
        f(v.autosave_enabled);
        return true;
    case Id::AutosaveEvery:
        f(v.autosave_every);
        return true;
    case Id::AutosaveKeep:
        f(v.autosave_keep);
        return true;
    case Id::TouchpadTouch: // AzaharFrame reticle: the Controls tab's rows
        f(v.vr_touchpad_touch);
        return true;
    case Id::BottomScreenReticle:
        f(v.vr_bottom_screen_reticle);
        return true;
    case Id::ReticleSpeed:
        f(v.vr_reticle_speed);
        return true;
    case Id::ReticleAtEdge:
        f(v.vr_reticle_at_edge);
        return true;
    default:
        return false;
    }
}

} // namespace

bool UsesGlobal(Menu::Id id) {
    bool global = true;
    WithSetting(id, [&global](auto& s) { global = s.UsingGlobal(); });
    return global;
}

bool UseGlobalValue(Menu::Id id) {
    bool had = false;
    WithSetting(id, [&had](auto& s) {
        had = !s.UsingGlobal();
        s.SetGlobal(true);
    });
    return had;
}

void FillGameValues(Menu::MenuModel& model) {
    for (int i = 0; i < Menu::kIdCount; i++) {
        bool own = false;
        WithSetting(static_cast<Menu::Id>(i), [&own](auto& s) { own = !s.UsingGlobal(); });
        model.game_value[static_cast<std::size_t>(i)] = own;
    }
}

} // namespace FrameXr::PerGame
