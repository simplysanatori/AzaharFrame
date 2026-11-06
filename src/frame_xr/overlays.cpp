// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/logging/log.h"
#include "frame_xr/overlays.h"
#include "frame_xr/session.h"
#include "frame_xr/xr_loader.h"

namespace FrameXr {

OverlayUploader::~OverlayUploader() {
    Shutdown();
}

bool OverlayUploader::Init(VkDevice device_, VkPhysicalDevice physical_device_,
                           uint32_t queue_family, QueueLock& queue_lock_) {
    // Azahar's dispatcher was set up for this device when the bootstrap
    // Vulkan::Instance adopted it, and the device outlives every renderer.
    device = vk::Device{device_};
    physical_device = vk::PhysicalDevice{physical_device_};
    queue_lock = &queue_lock_;
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
    return true;
}

void OverlayUploader::Shutdown() {
    if (!device) {
        return;
    }
    if (staging_map) {
        device.unmapMemory(staging_memory);
        staging_map = nullptr;
    }
    if (staging) {
        device.destroyBuffer(staging);
        device.freeMemory(staging_memory);
        staging = nullptr;
        staging_memory = nullptr;
        staging_size = 0;
    }
    device.destroyFence(fence);
    device.destroyCommandPool(pool);
    device = nullptr;
}

bool OverlayUploader::CreateChain(XrSession session, VkFormat format, uint32_t width,
                                  uint32_t height, OverlayChain& chain) {
    auto& xr = Xr();
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    ci.format = format;
    ci.sampleCount = 1;
    ci.width = width;
    ci.height = height;
    ci.faceCount = 1;
    ci.arraySize = 1;
    ci.mipCount = 1;
    const XrResult r = xr.xrCreateSwapchain(session, &ci, &chain.handle);
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: overlay swapchain {}x{}: {}", width, height, ResultName(r));
        chain = {};
        return false;
    }
    uint32_t count = 0;
    xr.xrEnumerateSwapchainImages(chain.handle, 0, &count, nullptr);
    std::vector<XrSwapchainImageVulkan2KHR> images(count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    xr.xrEnumerateSwapchainImages(chain.handle, count, &count,
                                  reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
    chain.images.clear();
    for (const auto& image : images) {
        chain.images.push_back(image.image);
    }
    chain.width = width;
    chain.height = height;
    chain.has_image = false;
    return true;
}

void OverlayUploader::DestroyChain(OverlayChain& chain) {
    if (chain.handle) {
        Xr().xrDestroySwapchain(chain.handle);
    }
    chain = {};
}

bool OverlayUploader::EnsureStaging(VkDeviceSize size) {
    if (staging && staging_size >= size) {
        return true;
    }
    if (staging) {
        device.unmapMemory(staging_memory);
        device.destroyBuffer(staging);
        device.freeMemory(staging_memory);
        staging = nullptr;
    }
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
        LOG_ERROR(Frontend, "xr: no host-visible memory for overlay uploads");
        device.destroyBuffer(staging);
        staging = nullptr;
        return false;
    }
    staging_memory = device.allocateMemory({.allocationSize = req.size, .memoryTypeIndex = type});
    device.bindBufferMemory(staging, staging_memory, 0);
    staging_map = device.mapMemory(staging_memory, 0, req.size);
    staging_size = size;
    return true;
}

bool OverlayUploader::Upload(OverlayChain& chain, const uint8_t* rgba) {
    if (!chain.handle || !device) {
        return false;
    }
    const VkDeviceSize size = VkDeviceSize{chain.width} * chain.height * 4;
    if (!EnsureStaging(size)) {
        return false;
    }
    std::memcpy(staging_map, rgba, size);

    auto& xr = Xr();
    uint32_t index = 0;
    XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    XrResult r = queue_lock->With([&] { return xr.xrAcquireSwapchainImage(chain.handle, &ai, &index); });
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: overlay acquire: {}", ResultName(r));
        return false;
    }
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wi.timeout = 1'000'000'000;
    r = xr.xrWaitSwapchainImage(chain.handle, &wi);
    if (r != XR_SUCCESS) {
        // An acquired image that was not waited for cannot be released: give up the
        // chain (it is remade on the next upload).
        LOG_ERROR(Frontend, "xr: overlay wait: {}", ResultName(r));
        DestroyChain(chain);
        return false;
    }

    const vk::Image image{chain.images[index]};
    const vk::ImageSubresourceRange range{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
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
        .imageSubresource{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {chain.width, chain.height, 1},
    };
    cmdbuf.copyBufferToImage(staging, image, vk::ImageLayout::eTransferDstOptimal, copy);
    // Back to COLOR_ATTACHMENT_OPTIMAL for the runtime (XR_KHR_vulkan_enable2).
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
    const vk::Result wait = device.waitForFences(fence, true, 1'000'000'000);
    device.resetFences(fence);
    if (wait != vk::Result::eSuccess) {
        LOG_ERROR(Frontend, "xr: overlay upload did not finish: {}", vk::to_string(wait));
    }
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    r = queue_lock->With([&] { return xr.xrReleaseSwapchainImage(chain.handle, &ri); });
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: overlay release: {}", ResultName(r));
        return false;
    }
    chain.has_image = true;
    return true;
}

} // namespace FrameXr
