// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/logging/log.h"
#include "frame_xr/session.h"
#include "frame_xr/vis/async_overlay.h"
#include "frame_xr/xr_loader.h"

namespace FrameXr::Vis {

AsyncOverlay::~AsyncOverlay() {
    Destroy();
}

bool AsyncOverlay::Create(XrSession session, VkDevice device_, VkPhysicalDevice physical_device_,
                          uint32_t queue_family, QueueLock& queue_lock_, VkFormat format,
                          uint32_t width_, uint32_t height_) {
    Destroy();
    auto& xr = Xr();
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    ci.format = format;
    ci.sampleCount = 1;
    ci.width = width_;
    ci.height = height_;
    ci.faceCount = 1;
    ci.arraySize = 1;
    ci.mipCount = 1;
    const XrResult r =
        queue_lock_.With([&] { return xr.xrCreateSwapchain(session, &ci, &handle); });
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: panel swapchain {}x{}: {}", width_, height_, ResultName(r));
        handle = XR_NULL_HANDLE;
        return false;
    }
    uint32_t count = 0;
    xr.xrEnumerateSwapchainImages(handle, 0, &count, nullptr);
    std::vector<XrSwapchainImageVulkan2KHR> list(count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    xr.xrEnumerateSwapchainImages(handle, count, &count,
                                  reinterpret_cast<XrSwapchainImageBaseHeader*>(list.data()));
    images.clear();
    for (const auto& image : list) {
        images.push_back(image.image);
    }

    queue_lock = &queue_lock_;
    device = vk::Device{device_};
    physical_device = vk::PhysicalDevice{physical_device_};
    width = width_;
    height = height_;
    queue = device.getQueue(queue_family, 0);
    pool = device.createCommandPool({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queue_family,
    });
    cmdbuf = device
                 .allocateCommandBuffers({
                     .commandPool = pool,
                     .level = vk::CommandBufferLevel::ePrimary,
                     .commandBufferCount = 1,
                 })
                 .front();
    fence = device.createFence({});

    const VkDeviceSize size = VkDeviceSize{width} * height * 4;
    staging = device.createBuffer({
        .size = size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive,
    });
    const vk::MemoryRequirements req = device.getBufferMemoryRequirements(staging);
    const vk::PhysicalDeviceMemoryProperties props = physical_device.getMemoryProperties();
    const vk::MemoryPropertyFlags wanted =
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if ((req.memoryTypeBits & (1u << i)) &&
            (props.memoryTypes[i].propertyFlags & wanted) == wanted) {
            type = i;
            break;
        }
    }
    if (type == UINT32_MAX) {
        LOG_ERROR(Frontend, "xr: no host-visible memory for a panel");
        Destroy();
        return false;
    }
    staging_memory = device.allocateMemory({.allocationSize = req.size, .memoryTypeIndex = type});
    device.bindBufferMemory(staging, staging_memory, 0);
    staging_map = device.mapMemory(staging_memory, 0, req.size);
    pending.assign(static_cast<std::size_t>(size), 0);
    return true;
}

void AsyncOverlay::Destroy() {
    if (device && in_flight) {
        static_cast<void>(device.waitForFences(fence, true, 1'000'000'000));
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        queue_lock->With([&] { return Xr().xrReleaseSwapchainImage(handle, &ri); });
        in_flight = false;
    }
    if (handle != XR_NULL_HANDLE) {
        const auto destroy = [&] { return Xr().xrDestroySwapchain(handle); };
        queue_lock ? queue_lock->With(destroy) : destroy();
        handle = XR_NULL_HANDLE;
    }
    if (device) {
        if (staging_map) {
            device.unmapMemory(staging_memory);
            staging_map = nullptr;
        }
        if (staging) {
            device.destroyBuffer(staging);
            staging = nullptr;
        }
        if (staging_memory) {
            device.freeMemory(staging_memory);
            staging_memory = nullptr;
        }
        if (fence) {
            device.destroyFence(fence);
            fence = nullptr;
        }
        if (pool) {
            device.destroyCommandPool(pool);
            pool = nullptr;
        }
        device = nullptr;
    }
    images.clear();
    pending.clear();
    has_pending = acquired = in_flight = has_image = false;
}

void AsyncOverlay::Upload(const uint8_t* rgba) {
    if (!Valid()) {
        return;
    }
    std::memcpy(pending.data(), rgba, pending.size());
    has_pending = true;
    if (!in_flight) {
        Start();
    }
}

void AsyncOverlay::Poll() {
    if (!Valid()) {
        return;
    }
    if (in_flight && device.getFenceStatus(fence) == vk::Result::eSuccess) {
        device.resetFences(fence);
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        const XrResult r =
            queue_lock->With([&] { return Xr().xrReleaseSwapchainImage(handle, &ri); });
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: panel release: {}", ResultName(r));
        } else {
            has_image = true;
        }
        in_flight = false;
    }
    if (has_pending && !in_flight) {
        Start();
    }
}

bool AsyncOverlay::Start() {
    auto& xr = Xr();
    if (!acquired) {
        XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        const XrResult r =
            queue_lock->With([&] { return xr.xrAcquireSwapchainImage(handle, &ai, &index); });
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: panel acquire: {}", ResultName(r));
            return false;
        }
        acquired = true;
    }
    // No waiting on the XR thread: a busy image is tried again next frame.
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wi.timeout = 0;
    const XrResult w = xr.xrWaitSwapchainImage(handle, &wi);
    if (w == XR_TIMEOUT_EXPIRED) {
        return false;
    }
    if (XR_FAILED(w)) {
        LOG_ERROR(Frontend, "xr: panel wait: {}", ResultName(w));
        return false;
    }
    acquired = false;
    std::memcpy(staging_map, pending.data(), pending.size());
    has_pending = false;

    const vk::Image image{images[index]};
    const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    cmdbuf.reset();
    cmdbuf.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    const vk::ImageMemoryBarrier to_transfer{
        .srcAccessMask = {},
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined, // every texel is rewritten
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range,
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                           vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, to_transfer);
    const vk::BufferImageCopy copy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1},
    };
    cmdbuf.copyBufferToImage(staging, image, vk::ImageLayout::eTransferDstOptimal, copy);
    const vk::ImageMemoryBarrier to_runtime{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = {},
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range,
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, to_runtime);
    cmdbuf.end();
    const vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &cmdbuf};
    queue_lock->With([&] {
        queue.submit(submit, fence);
        return 0;
    });
    in_flight = true;
    return true;
}

} // namespace FrameXr::Vis
