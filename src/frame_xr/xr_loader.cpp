// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

#include "common/logging/log.h"
#include "frame_xr/xr_loader.h"

namespace FrameXr {

namespace {
XrFunctions g_functions;
void* g_loader = nullptr;
XrInstance g_instance = XR_NULL_HANDLE;

constexpr const char* kSteamVrLoader = "/opt/steamvr/bin/linuxarm64/libopenxr_loader.so";
} // namespace

XrFunctions& Xr() {
    return g_functions;
}

bool LoadLoader() {
    if (g_loader) {
        return true;
    }
    const char* candidates[] = {std::getenv("AZAHARFRAME_OPENXR_LOADER"), kSteamVrLoader,
                                "libopenxr_loader.so.1", "libopenxr_loader.so"};
    for (const char* path : candidates) {
        if (!path || !*path) {
            continue;
        }
        g_loader = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (g_loader) {
            LOG_INFO(Frontend, "xr: OpenXR loader {}", path);
            break;
        }
        LOG_INFO(Frontend, "xr: no OpenXR loader at {} ({})", path, dlerror());
    }
    if (!g_loader) {
        LOG_ERROR(Frontend, "xr: no OpenXR loader found");
        return false;
    }
    g_functions.xrGetInstanceProcAddr =
        reinterpret_cast<PFN_xrGetInstanceProcAddr>(dlsym(g_loader, "xrGetInstanceProcAddr"));
    if (!g_functions.xrGetInstanceProcAddr) {
        LOG_ERROR(Frontend, "xr: the OpenXR loader has no xrGetInstanceProcAddr");
        return false;
    }
#define FRAME_XR_LOAD(name)                                                                        \
    if (XR_FAILED(g_functions.xrGetInstanceProcAddr(                                               \
            XR_NULL_HANDLE, #name, reinterpret_cast<PFN_xrVoidFunction*>(&g_functions.name)))) {   \
        LOG_ERROR(Frontend, "xr: the OpenXR loader has no " #name);                                \
        return false;                                                                              \
    }
    FRAME_XR_GLOBAL_FUNCTIONS(FRAME_XR_LOAD)
#undef FRAME_XR_LOAD
    return true;
}

bool LoadInstanceFunctions(XrInstance instance) {
    g_instance = instance;
    bool ok = true;
#define FRAME_XR_LOAD(name)                                                                        \
    if (XR_FAILED(g_functions.xrGetInstanceProcAddr(                                               \
            instance, #name, reinterpret_cast<PFN_xrVoidFunction*>(&g_functions.name))) ||         \
        !g_functions.name) {                                                                       \
        LOG_ERROR(Frontend, "xr: the runtime has no " #name);                                      \
        g_functions.name = nullptr;                                                                \
        ok = false;                                                                                \
    }
    FRAME_XR_INSTANCE_FUNCTIONS(FRAME_XR_LOAD)
#undef FRAME_XR_LOAD
#define FRAME_XR_LOAD_OPTIONAL(name)                                                               \
    if (XR_FAILED(g_functions.xrGetInstanceProcAddr(                                               \
            instance, #name, reinterpret_cast<PFN_xrVoidFunction*>(&g_functions.name)))) {         \
        g_functions.name = nullptr;                                                                \
    }
    FRAME_XR_EXTENSION_FUNCTIONS(FRAME_XR_LOAD_OPTIONAL)
#undef FRAME_XR_LOAD_OPTIONAL
    return ok;
}

void UnloadInstanceFunctions() {
    g_instance = XR_NULL_HANDLE;
#define FRAME_XR_CLEAR(name) g_functions.name = nullptr;
    FRAME_XR_INSTANCE_FUNCTIONS(FRAME_XR_CLEAR)
    FRAME_XR_EXTENSION_FUNCTIONS(FRAME_XR_CLEAR)
#undef FRAME_XR_CLEAR
}

const char* ResultName(XrResult result) {
    thread_local char buffer[XR_MAX_RESULT_STRING_SIZE];
    if (g_instance && g_functions.xrResultToString &&
        XR_SUCCEEDED(g_functions.xrResultToString(g_instance, result, buffer))) {
        return buffer;
    }
    std::snprintf(buffer, sizeof(buffer), "XrResult(%d)", static_cast<int>(result));
    return buffer;
}

} // namespace FrameXr
