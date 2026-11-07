// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: while Azahar for Frame runs flat (its window, no VR session; frame_xr), games see
// the 3D slider at 0 so they draw one eye. The 3D setting itself is left alone (never saved).
// Its own header so that setting it does not touch common/settings.h.

#pragma once

#include <atomic>

namespace Settings {
inline std::atomic<bool> frame_flat_mono{false};
} // namespace Settings
