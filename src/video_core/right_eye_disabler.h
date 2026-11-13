// Copyright 2024-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include "common/common_types.h"

namespace VideoCore {
class GPU;

class RightEyeDisabler {
public:
    RightEyeDisabler(GPU& gpu) : gpu{gpu} {}

    bool ShouldAllowCmdQueueTrigger(PAddr addr, u32 size);
    bool ShouldAllowDisplayTransfer(PAddr src_address, size_t size);

    void ReportEndFrame();

    /// AzaharFrame: whether the disabler may skip the right eye this frame. It is for the flat
    /// screen only. In VR the right eye is the 3DS's own right image, so a skipped right eye
    /// shows a stale picture to one eye (a shimmer in Fire Emblem Fates' 3D scenes): while an XR
    /// session shows the game it never runs, whatever the setting says, and the saved setting
    /// is left alone for the flat screen.
    static constexpr bool WantedForFrame(bool setting, bool xr_active) {
        return setting && !xr_active;
    }

    void SetEnabled(bool enable) {
        enabled = enable;
    }

private:
    bool enabled = true;
    // Enabled by ReportEndFrame() once the user setting has been read.
    bool enable_for_frame = false;

    bool top_screen_drawn = false;
    bool top_screen_transfered = false;
    bool top_screen_blocked = false;
    bool cmd_trigger_blocked = false;
    PAddr top_screen_buf = 0;

    bool cmd_queue_trigger_happened = false;
    bool display_tranfer_happened = false;
    bool report_end_frame_pending = false;

    GPU& gpu;
};
} // namespace VideoCore