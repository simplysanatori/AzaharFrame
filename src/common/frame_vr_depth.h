// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame vrsize: the 3D slider the games see is the 3D depth setting times this (0 to 1).
// frame_xr sets it while the normal top screen is made bigger than its normal size (grip + stick
// sideways): the stereo picture's left-right offsets grow with the screen in metres, so at 3
// times the size a distant object's two images would be further apart than the eyes and could
// not be fused. 1 / scale keeps them as far apart as at the normal size. The 3D setting itself
// is left alone (never saved).

#pragma once

#include <atomic>

namespace Settings {
inline std::atomic<float> frame_vr_depth_scale{1.0f};
} // namespace Settings
