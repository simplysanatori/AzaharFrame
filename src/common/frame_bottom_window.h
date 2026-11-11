// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame window: the bottom screen in its own desktop window during VR
// (Settings::values.vr_bottom_screen_window, config file only). citra_qt sets the flag while the
// second render window is shown in VR; the second window then lays out the bottom screen alone
// (core/frontend/emu_window.cpp) and the Vulkan renderer presents it next to the XR swapchains
// (renderer_vulkan.cpp). Its own header so that common/settings.h stays as it is.

#pragma once

#include <atomic>

namespace Settings {
inline std::atomic<bool> frame_vr_bottom_window{false};
} // namespace Settings
