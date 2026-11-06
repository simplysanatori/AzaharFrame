// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// OpenXR with the Vulkan platform types and no prototypes (the loader is opened at
// run time, frame_xr/xr_loader.h). Vulkan comes first, with Azahar's own defines.

#pragma once

#include "video_core/renderer_vulkan/vk_common.h"

#define XR_NO_PROTOTYPES 1
#define XR_USE_GRAPHICS_API_VULKAN 1
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
