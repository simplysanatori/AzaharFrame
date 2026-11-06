// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// The OpenXR loader, opened at run time (SteamVR ships its own, with no soname)
// and the entry points Azahar for Frame uses, resolved through xrGetInstanceProcAddr.
// Nothing links libopenxr_loader: a build without a headset runtime still starts.

#pragma once

#include "frame_xr/xr_includes.h"

namespace FrameXr {

// Entry points resolved before an instance exists.
#define FRAME_XR_GLOBAL_FUNCTIONS(X)                                                               \
    X(xrEnumerateInstanceExtensionProperties)                                                      \
    X(xrCreateInstance)

// Core entry points, resolved once the instance exists.
#define FRAME_XR_INSTANCE_FUNCTIONS(X)                                                             \
    X(xrDestroyInstance)                                                                           \
    X(xrGetInstanceProperties)                                                                     \
    X(xrResultToString)                                                                            \
    X(xrGetSystem)                                                                                 \
    X(xrGetSystemProperties)                                                                       \
    X(xrPollEvent)                                                                                 \
    X(xrCreateSession)                                                                             \
    X(xrDestroySession)                                                                            \
    X(xrBeginSession)                                                                              \
    X(xrEndSession)                                                                                \
    X(xrRequestExitSession)                                                                        \
    X(xrCreateReferenceSpace)                                                                      \
    X(xrDestroySpace)                                                                              \
    X(xrLocateSpace)                                                                               \
    X(xrLocateViews)                                                                               \
    X(xrEnumerateViewConfigurationViews)                                                           \
    X(xrEnumerateSwapchainFormats)                                                                 \
    X(xrCreateSwapchain)                                                                           \
    X(xrDestroySwapchain)                                                                          \
    X(xrEnumerateSwapchainImages)                                                                  \
    X(xrAcquireSwapchainImage)                                                                     \
    X(xrWaitSwapchainImage)                                                                        \
    X(xrReleaseSwapchainImage)                                                                     \
    X(xrWaitFrame)                                                                                 \
    X(xrBeginFrame)                                                                                \
    X(xrEndFrame)                                                                                  \
    X(xrStringToPath)                                                                              \
    X(xrPathToString)                                                                              \
    X(xrCreateActionSet)                                                                           \
    X(xrDestroyActionSet)                                                                          \
    X(xrCreateAction)                                                                              \
    X(xrDestroyAction)                                                                             \
    X(xrSuggestInteractionProfileBindings)                                                         \
    X(xrAttachSessionActionSets)                                                                   \
    X(xrSyncActions)                                                                               \
    X(xrGetActionStateBoolean)                                                                     \
    X(xrGetActionStateFloat)                                                                       \
    X(xrGetActionStateVector2f)                                                                    \
    X(xrGetActionStatePose)                                                                        \
    X(xrCreateActionSpace)                                                                         \
    X(xrApplyHapticFeedback)                                                                       \
    X(xrStopHapticFeedback)                                                                        \
    X(xrGetCurrentInteractionProfile)

// Extension entry points (null when the extension is not enabled).
#define FRAME_XR_EXTENSION_FUNCTIONS(X)                                                            \
    X(xrGetVulkanGraphicsRequirements2KHR)                                                         \
    X(xrCreateVulkanInstanceKHR)                                                                   \
    X(xrGetVulkanGraphicsDevice2KHR)                                                               \
    X(xrCreateVulkanDeviceKHR)                                                                     \
    X(xrEnumerateDisplayRefreshRatesFB)                                                            \
    X(xrRequestDisplayRefreshRateFB)                                                               \
    X(xrGetDisplayRefreshRateFB)

struct XrFunctions {
    PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr = nullptr;
#define FRAME_XR_DECLARE(name) PFN_##name name = nullptr;
    FRAME_XR_GLOBAL_FUNCTIONS(FRAME_XR_DECLARE)
    FRAME_XR_INSTANCE_FUNCTIONS(FRAME_XR_DECLARE)
    FRAME_XR_EXTENSION_FUNCTIONS(FRAME_XR_DECLARE)
#undef FRAME_XR_DECLARE
};

/// The resolved entry points (all null until LoadLoader / LoadInstanceFunctions).
XrFunctions& Xr();

/// Opens the loader: $AZAHARFRAME_OPENXR_LOADER, else SteamVR's, else the system's.
bool LoadLoader();

/// Resolves the instance and extension entry points. False if a core one is missing.
bool LoadInstanceFunctions(XrInstance instance);

/// Forgets the instance entry points (the instance was destroyed).
void UnloadInstanceFunctions();

/// "XR_ERROR_..." text of a result (needs an instance for the runtime's names).
const char* ResultName(XrResult result);

} // namespace FrameXr
