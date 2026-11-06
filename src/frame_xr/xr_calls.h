// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// For sources written against the plain OpenXR API (xrSyncActions(...)): the
// calls go through the entry points frame_xr resolved at run time
// (xr_loader.h), since nothing links libopenxr_loader. Include last.

#pragma once

#include "frame_xr/xr_loader.h"

#define FRAME_XR_CALL(name) ::FrameXr::Xr().name
#define xrStringToPath FRAME_XR_CALL(xrStringToPath)
#define xrPathToString FRAME_XR_CALL(xrPathToString)
#define xrCreateActionSet FRAME_XR_CALL(xrCreateActionSet)
#define xrDestroyActionSet FRAME_XR_CALL(xrDestroyActionSet)
#define xrCreateAction FRAME_XR_CALL(xrCreateAction)
#define xrDestroyAction FRAME_XR_CALL(xrDestroyAction)
#define xrSuggestInteractionProfileBindings FRAME_XR_CALL(xrSuggestInteractionProfileBindings)
#define xrAttachSessionActionSets FRAME_XR_CALL(xrAttachSessionActionSets)
#define xrSyncActions FRAME_XR_CALL(xrSyncActions)
#define xrGetActionStateBoolean FRAME_XR_CALL(xrGetActionStateBoolean)
#define xrGetActionStateFloat FRAME_XR_CALL(xrGetActionStateFloat)
#define xrGetActionStateVector2f FRAME_XR_CALL(xrGetActionStateVector2f)
#define xrGetActionStatePose FRAME_XR_CALL(xrGetActionStatePose)
#define xrCreateActionSpace FRAME_XR_CALL(xrCreateActionSpace)
#define xrDestroySpace FRAME_XR_CALL(xrDestroySpace)
#define xrLocateSpace FRAME_XR_CALL(xrLocateSpace)
#define xrApplyHapticFeedback FRAME_XR_CALL(xrApplyHapticFeedback)
#define xrStopHapticFeedback FRAME_XR_CALL(xrStopHapticFeedback)
#define xrGetCurrentInteractionProfile FRAME_XR_CALL(xrGetCurrentInteractionProfile)
