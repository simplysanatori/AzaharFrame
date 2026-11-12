// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame doubling: which head pose a game picture was drawn with, carried from the XR
// thread (it publishes the pose, video_core/vr_immersive.h) through the emulation thread (the
// game frame that used it) and the renderer (the XR images that show that frame,
// vk_xr_target.cpp) back to the XR thread (it places the immersive screen where the head was
// when the picture was drawn, frame_xr/immersive.cpp). No OpenXR and no Vulkan types here.

#pragma once

#include <cstdint>

namespace VideoCore {

struct XrContentPose {
    bool valid = false;           ///< drawn in immersive mode 2 or 3 with a located head pose
    float orientation[4]{0, 0, 0, 1}; ///< the head in the LOCAL space: x, y, z, w
    float position[3]{0, 0, 0};
    int64_t time = 0; ///< the XrTime the pose was located for (its predicted display time)
};

/// The two eye images of the top screen in one swapchain (AZAHARFRAME_XR_PACKED): the left eye
/// in the upper half, the right eye below it, this many pixels apart (cleared to black, so the
/// compositor's filtering at a half's edge never picks up the other eye).
constexpr uint32_t kXrPackedGap = 8;

} // namespace VideoCore
