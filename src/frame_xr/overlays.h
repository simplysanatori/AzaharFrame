// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Internal: quad swapchains whose pixels come from the CPU (the picker panel,
// the pointer dot), uploaded on the XR thread with a copy on the process's
// Vulkan device. Only while no game runs (the picker) or once (the dot): no GPU
// work on the XR thread during a game.

#pragma once

#include <cstdint>
#include <vector>

#include "frame_xr/xr_includes.h"

namespace FrameXr {

class QueueLock;

struct OverlayChain {
    XrSwapchain handle = XR_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<VkImage> images;
    bool has_image = false; ///< released once: may be shown
};

class OverlayUploader {
public:
    OverlayUploader() = default;
    ~OverlayUploader();

    bool Init(VkDevice device, VkPhysicalDevice physical_device, uint32_t queue_family,
              QueueLock& queue_lock);
    void Shutdown();

    bool CreateChain(XrSession session, VkFormat format, uint32_t width, uint32_t height,
                     OverlayChain& chain);
    void DestroyChain(OverlayChain& chain);

    /// Copies `rgba` (width*height*4 bytes, rows top down) into the chain's next image
    /// and releases it once the copy finished.
    bool Upload(OverlayChain& chain, const uint8_t* rgba);

private:
    bool EnsureStaging(VkDeviceSize size);

    QueueLock* queue_lock = nullptr;
    vk::Device device;
    vk::PhysicalDevice physical_device;
    vk::Queue queue;
    vk::CommandPool pool;
    vk::CommandBuffer cmdbuf;
    vk::Fence fence;
    vk::Buffer staging;
    vk::DeviceMemory staging_memory;
    VkDeviceSize staging_size = 0;
    void* staging_map = nullptr;
};

} // namespace FrameXr
