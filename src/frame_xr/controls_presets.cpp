// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/controls_presets.h"

// AzaharFrame kiu: Controls presets as starting values only (controls_presets.h).
namespace FrameXr {

uint32_t ApplyControlsPreset(const ControlsPreset& preset) {
    auto& v = Settings::values;
    auto& done_setting = v.vr_controls_preset_done;
    const uint32_t done_before = done_setting.GetValue();
    uint32_t done = done_before;
    uint32_t given = 0;
    const auto offer = [&](auto& setting, auto value, uint32_t bit, const char* what) {
        if ((done & bit) != 0) {
            return; // decided before: the player's choice since then stays
        }
        if (!setting.UsingGlobal()) {
            done |= bit; // the game's own value (the player's, or given before) stays
            return;
        }
        if (setting.GetValue() != setting.GetDefault()) {
            return; // a global value the player chose
        }
        setting.SetGlobal(false);
        setting.SetValue(value);
        done |= bit;
        given |= bit;
        LOG_INFO(Frontend, "xr: {}: {} preset {}", preset.name, what, value);
    };
    if (preset.bottom_screen_reticle != 0) {
        offer(v.vr_bottom_screen_reticle, preset.bottom_screen_reticle, kPresetReticle,
              "Bottom Screen Reticle");
    }
    if (preset.motion_controls >= 0) {
        offer(v.vr_motion_controls, static_cast<u32>(preset.motion_controls),
              kPresetMotionControls, "Motion Controls");
    }
    if (preset.edge_cstick) {
        offer(v.vr_reticle_edge_cstick, true, kPresetEdgeCStick, "C-Stick at Edge");
    }
    if (done != done_before) {
        done_setting.SetGlobal(false);
        done_setting.SetValue(done);
    }
    return given;
}

} // namespace FrameXr
