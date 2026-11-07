// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// A quad swapchain whose pixels come from the CPU, uploaded without ever waiting for the GPU on
// the XR thread, so it may change while a game runs (the stats panel): the copy is submitted with
// a fence, and the image is released only on a later frame once that fence has signalled
// (Poll), the way the renderer releases the screens (vk_xr_target.cpp). Until then the layer
// shows the image released before. The queue lock is held only around vkQueueSubmit and the
// swapchain calls that may use the queue. Each chain has its own staging buffer, command buffer
// and fence. The overlay uploader (overlays.h) waits for each copy instead and is kept for what
// changes only while no game runs.

#pragma once

#include <cstdint>
#include <vector>

#include "frame_xr/xr_includes.h"

namespace FrameXr {
class QueueLock;
}

namespace FrameXr::Vis {

class AsyncOverlay {
public:
    AsyncOverlay() = default;
    ~AsyncOverlay();
    AsyncOverlay(const AsyncOverlay&) = delete;
    AsyncOverlay& operator=(const AsyncOverlay&) = delete;

    bool Create(XrSession session, VkDevice device, VkPhysicalDevice physical_device,
                uint32_t queue_family, QueueLock& queue_lock, VkFormat format, uint32_t width,
                uint32_t height);
    /// XR thread, before the session ends. Waits for a copy still running (at most 1 s).
    void Destroy();
    bool Valid() const {
        return handle != XR_NULL_HANDLE;
    }

    /// Queues `rgba` (width * height * 4 bytes, premultiplied, rows top down). A copy already in
    /// flight is let finish first; only the newest pixels queued meanwhile are copied after it.
    void Upload(const uint8_t* rgba);
    /// XR thread, once a frame: releases a finished copy, starts the next one.
    void Poll();

    /// Released at least once: may be submitted.
    bool HasImage() const {
        return has_image;
    }
    /// Pixels queued or being copied: the image shown is not yet the newest Upload.
    bool Busy() const {
        return has_pending || in_flight;
    }
    XrSwapchain Handle() const {
        return handle;
    }
    uint32_t Width() const {
        return width;
    }
    uint32_t Height() const {
        return height;
    }

private:
    bool Start();

    QueueLock* queue_lock = nullptr;
    vk::Device device;
    vk::PhysicalDevice physical_device;
    vk::Queue queue;
    vk::CommandPool pool;
    vk::CommandBuffer cmdbuf;
    vk::Fence fence;
    vk::Buffer staging;
    vk::DeviceMemory staging_memory;
    void* staging_map = nullptr;

    XrSwapchain handle = XR_NULL_HANDLE;
    std::vector<VkImage> images;
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> pending; ///< pixels queued while a copy was in flight
    bool has_pending = false;
    bool acquired = false; ///< an image acquired, not yet waited for
    uint32_t index = 0;
    bool in_flight = false; ///< a copy submitted, the image not yet released
    bool has_image = false;
};

} // namespace FrameXr::Vis
