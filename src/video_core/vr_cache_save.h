// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash3: the driver pipeline cache (the Vulkan driver's built pipelines, the
// .bin of each game) was written only at a clean stop or a title switch, so a crash or SteamVR
// ending the app lost every pipeline built since the game started (report 70, session 573). The
// Vulkan pipeline cache now also saves it while the game runs: once its cached pipelines are
// built after the start, and when frame_xr asks (the VR menu opens). This is frame_xr's way to
// ask: video_core does not link frame_xr, and frame_xr does not reach the renderer's pipeline
// cache. Header only; nothing happens without a Vulkan renderer.

#pragma once

#include <functional>
#include <mutex>

namespace VideoCore::PipelineCacheSave {

inline std::mutex g_mutex;
inline std::function<void(const char*)> g_queue; ///< set while a Vulkan pipeline cache lives
inline const void* g_owner = nullptr;

/// Any thread: a save of the driver pipeline cache, queued off the emulation thread (the
/// pipeline cache's workers) and rate-limited there; `why` (a string literal) is logged.
inline void Request(const char* why) {
    std::scoped_lock lock{g_mutex};
    if (g_queue) {
        g_queue(why);
    }
}

/// The pipeline cache: registers itself (`queue`), or goes (an empty `queue`, only its own).
inline void Register(const void* owner, std::function<void(const char*)> queue) {
    std::scoped_lock lock{g_mutex};
    if (queue) {
        g_queue = std::move(queue);
        g_owner = owner;
    } else if (g_owner == owner) {
        g_queue = nullptr;
        g_owner = nullptr;
    }
}

} // namespace VideoCore::PipelineCacheSave
